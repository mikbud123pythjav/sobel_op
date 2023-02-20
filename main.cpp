#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <string>

using namespace std;

struct bmp_header
{
	char Type[2];
	int Size;
	short int r1;
	short int r2;
	int OffBits;
};

struct bmp_info
{
	int Size;
	int Width;
	int Height;
	short int Planes;
	short int Bitcount;
	int Compression;
	int SizeImage;
	int XpelsPerMeter;
	int YpelsPerMeter;
	int ClrUses;
	int ClrImportant;
};

struct pixel
{
	unsigned char B;
	unsigned char G;
	unsigned char R;
};

struct pixel_int
{
	int B;
	int G;
	int R;
};

struct filter
{
	int count;
	int size;
	float threshold;
	int*** mask;
};

void print_menu(int ch)
{
	switch (ch) {
	case 1: {
		cout << endl << endl;
		cout << "   Wybierz tryb odczytu danych" << endl;
		cout << "-------------------------------" << endl;
		cout << "1. Wczytanie calosci pliku bmp" << endl;
		cout << "2. Wczytywanie po tyle rzedow ile wynosi rozmiar maski";
		break;
	}
	case 2: {
		cout << endl << endl;
		cout << "   Wybierz filtr do detekcji krawedzi" << endl;
		cout << "--------------------------------------" << endl;
		cout << "1. Filtr Sobela w 8 kierunkach i o rozmiarze 3x3" << endl;
		cout << "2. Filtr Sobela w 4 kierunkach i o rozmiarze 3x3" << endl;
		cout << "3. Filtr Sobela w 8 kierunkach i o rozmiarze 5x5" << endl;
		cout << "h. Pomoc";
		break;
	}
	case 3: {
		cout << endl << endl;
		cout << "   Wybierz dodatkowe filtry (mozliwosc wyboru kilku filtrow)" << endl;
		cout << "-------------------------------------------------------------" << endl;
		cout << "1. Wyrownanie histogramu (dziala wylacznie w 1 trybie odczytu)" << endl;
		cout << "2. Odcienie szarosci" << endl;
		cout << "3. Usuwanie szumu" << endl;
		cout << "4. Brak dodatkowych filtrow" << endl;
		cout << "h. Pomoc";
	}
	}
}

class bmp
{
public:

	bmp_header file_head;
	bmp_info info_head;
	pixel** input;
	pixel** temp;
	string filename;

	int open()
	{
		bool open;
		int mode;

		// wczytanie nazwy plik do odczytu
		do {
			cout << "Podaj nazwe pliku do otwarcia: ";
			cin >> filename;

			ifstream ibmp(filename);

			if (!ibmp) {
				cout << "Blad odczytu pliku." << endl << endl;
				open = false;
			}
			else {
				open = true;
				// przy b³êdnym wprowadzeniu wyczyszczenie flagi b³êdu i strumienia
				cin.clear();
				cin.ignore(numeric_limits<streamsize>::max(), '\n');
			}
			ibmp.close();
		} while (!open);

		// wybranie trybu odczytu
		print_menu(1);
		do {
			cout << endl << " > ";
			cin >> mode;

			if (mode == 1 || mode == 2) break;
			else {
				cout << endl << "Nie ma takiego trybu";
				// przy b³êdnym wprowadzeniu wyczyszczenie flagi b³êdu i strumienia
				cin.clear();
				cin.ignore(numeric_limits<streamsize>::max(), '\n');
			}
		} while (true);

		return mode;
	}
	void read_header()
	{
		// otwarcie pliku bmp
		ifstream ibmp(filename, ios::binary);

		if (!ibmp) {
			cout << "Blad odczytu pliku" << endl;
			exit(1);
		}

		// wczytanie po koleji danych BITMAPFILEHEADER do struktury bmp_header
		ibmp.read(reinterpret_cast<char*>(&file_head.Type), 2);
		ibmp.read(reinterpret_cast<char*>(&file_head.Size), 4);
		ibmp.read(reinterpret_cast<char*>(&file_head.r1), 2);
		ibmp.read(reinterpret_cast<char*>(&file_head.r2), 2);
		ibmp.read(reinterpret_cast<char*>(&file_head.OffBits), 4);

		// wczytanie w ca³oœci BITMAPINFOHEADER do struktury bmp_info
		ibmp.read(reinterpret_cast<char*>(&info_head), sizeof(info_head));

		ibmp.close();
	}
};

void save_filters()
{
	int sobel4[4][3][3] = {
		{ {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1} },
		{ {0, 1, 2}, {-1, 0, 1}, {-2, -1, 0} },
		{ {1, 2, 1}, {0, 0, 0}, {-1, -2, -1} },
		{ {2, 1, 0}, {1, 0, -1}, {0, -1, -2} }
	};
	int sobel2[2][3][3] = {
		{ {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1} },
		{ {1, 2, 1}, {0, 0, 0}, {-1, -2, -1} }
	};
	int sobel45[4][5][5] = {
		{ {-1, 0, 0, 0, 1}, {-2, -1, 0, 1, 2}, {-3, -2, 0, 2, 3}, {-2, -1, 0, 1, 2}, {-1, 0, 0, 0, 1} },
		{ {1, 2, 3, 2, 1}, {0, 1, 2, 1, 0}, {0, 0, 0, 0, 0}, {0, -1, -2, -1, 0}, {-1, -2, -3, -2, -1} },
		{ {-3, -2, -1, 0, 0}, {-2, -2, -1, 0, 0}, {-1, -1, 0, 1, 1}, {0, 0, 1, 2, 2}, {0, 0, 1, 2, 3} },
		{ {0, 0, 3, 2, 1}, {0, 0, 2, 2, 1}, {-1, -1, 0, 1, 1}, {-2, -2, -1, 0, 0}, {-3, -2, -1, 0, 0} }
	};
	int gauss[1][3][3] = {
		{ {1, 2, 1}, {2, 4, 2}, {1, 2, 1} }
	};

	fstream fs;
	// zapisanie operatora sobela dla 4 kierunków 3x3
	fs.open("sobel4.msk", fstream::out);
	fs << 4 << ' ' << 3 << endl;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 3; j++) {
			for (int k = 0; k < 3; k++) {
				fs << sobel4[i][j][k] << ' ';
			}
		}
		fs << endl;
	}
	fs.close();

	// zapisanie operatora sobela dla 2 kierunków 3x3
	fs.open("sobel2.msk", fstream::out);
	fs << 2 << ' ' << 3 << endl;
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 3; j++) {
			for (int k = 0; k < 3; k++) {
				fs << sobel2[i][j][k] << ' ';
			}
		}
		fs << endl;
	}
	fs.close();

	// zapisanie operatora sobela dla 4 kierunków 5x5
	fs.open("sobel45.msk", fstream::out);
	fs << 4 << ' ' << 5 << endl;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 5; j++) {
			for (int k = 0; k < 5; k++) {
				fs << sobel45[i][j][k] << ' ';
			}
		}
		fs << endl;
	}
	fs.close();

	// zapisanie filtra gaussa 3x3
	fs.open("gauss.msk", fstream::out);
	fs << 1 << ' ' << 3 << endl;
	for (int i = 0; i < 1; i++) {
		for (int j = 0; j < 3; j++) {
			for (int k = 0; k < 3; k++) {
				fs << gauss[i][j][k] << ' ';
			}
		}
		fs << endl;
	}
	fs.close();
}

void bmp_header_read(char plik[], bmp_header& header)
{
	// otwarcie pliku bmp
	ifstream ibmp(plik, ios::binary);

	if (!ibmp) {
		cout << "Blad odczytu pliku" << endl;
		exit(1);
	}

	// wczytanie po koleji danych BITMAPFILEHEADER do struktury bmp_header
	ibmp.read(reinterpret_cast<char*>(&header.Type), 2);
	ibmp.read(reinterpret_cast<char*>(&header.Size), 4);
	ibmp.read(reinterpret_cast<char*>(&header.r1), 2);
	ibmp.read(reinterpret_cast<char*>(&header.r2), 2);
	ibmp.read(reinterpret_cast<char*>(&header.OffBits), 4);

	ibmp.close();
}

void bmp_info_read(char plik[], bmp_info& info)
{
	//otwarcie pliku bmp
	ifstream ibmp(plik, ios::binary);

	if (!ibmp) {
		cout << "Blad odczytu pliku" << endl;
		exit(1);
	}

	// przesuniêcie wskaŸnika do pocz¹tku BITMAPINFOHEADER
	ibmp.seekg(14, ios::beg);

	// wczytanie w ca³oœci BITMAPINFOHEADER do struktury bmp_info
	ibmp.read(reinterpret_cast<char*>(&info), sizeof(info));

	ibmp.close();
}

void choose_read_filters(filter& filtr)
{
	string name;
	char ch;

	// menu wyboru filtra
	print_menu(2);
	do {
		cout << endl << " > ";
		cin >> ch;
		if (ch == '1' || ch == '2' || ch == '3') break;
		else if (ch == 'h') {
			cout << endl;
			cout << "Filtry zostaly zapisane do plikow z roszerzeniem .msk," << endl;
			cout << "gdzie pierwsze dwie wartosci to iloœæ wszystkich masek oraz ich rozmiar," << endl;
			cout << "w nastêpnych linijkach znaduja sia juz konkretne wartosci masek, po linijce na kazda";
			cout << "4 kierunki filtra Sobela oznaczaja po dwa kierunki poziome i po dwa pionowe," << endl;
			cout << "natomiast 8 kierunkow, to dodatkowe 4 kierunki na ukos." << endl;
			system("pause");
			system("cls");
			print_menu(2);
		}
		else {
			cout << endl << "Brak takiego filtra";
			// przy b³êdnym wprowadzeniu wyczyszczenie flagi b³êdu i strumienia
			cin.clear();
			cin.ignore(numeric_limits<streamsize>::max(), '\n');
		}
	} while (true);

	// podanie progu filtra
	cout << "Prog (1 to wartosc domyslna maski)" << endl;
	cout << " > ";
	cin >> filtr.threshold;

	// prze³o¿enie wybór filtra do detekcji krawêdzi na odpowiedni¹ nazwê pliku
	switch (ch) {
	case '1': name = "sobel4.msk"; break;
	case '2': name = "sobel2.msk"; break;
	case '3': name = "sobel45.msk"; break;
	}

	// otwarcie pliku z wybran¹ mask¹
	fstream fs;
	fs.open(name, fstream::in);

	//wczytanie podstawowych wielkoœci filtra

	fs >> filtr.count;
	fs >> filtr.size;

	// przygotowanie tablic
	filtr.mask = new int** [filtr.count];
	for (int i = 0; i < filtr.count; i++) {

		filtr.mask[i] = new int* [filtr.size];
		for (int j = 0; j < filtr.size; j++) {

			filtr.mask[i][j] = new int[filtr.size];
			for (int k = 0; k < filtr.size; k++) {

				fs >> filtr.mask[i][j][k];
			}
		}
	}
}

pixel** bmp_data_read(string plik, bmp_header& head, bmp_info& info, int& rows, int& c)
{
	int skok = 0;

	// otworzenie pliku bmp
	ifstream ibmp(plik, ios::binary);

	if (!ibmp) {
		cout << "Blad odczytu pliku" << endl;
		exit(1);
	}

	// utworzenie tablicy struktury pikseli
	pixel** piksele = new pixel*[rows];
	for (int i = 0; i < rows; i++) {
		piksele[i] = new pixel[info.Width];
		for (int j = 0; j < info.Width; j++) {
			piksele[i][j].B = '\0';
			piksele[i][j].G = '\0';
			piksele[i][j].R = '\0';
		}
	}

	int N = (head.Size - head.OffBits) / info.Height;   // iloœæ danych w rzêdzie ³¹cznie z zerami
	char* img_data_row = new char[N];   // rz¹d danych o pikselach

	

	// przesuniêcie wskaŸnika na pocz¹tek tablicy pikseli w pliku bmp
	ibmp.seekg(head.OffBits + (c * N), ios::beg);

	// co jeden rz¹d pikseli, wczytanie danych i zapisanie ich do tablicy pikseli juz z pominiêceim zer
	for (int i = 0; i < rows; i++) {

		ibmp.read(img_data_row, N);

		for (int j = 0, k = 0; j < info.Width; j++, k += 3) {

			piksele[i][j].B = img_data_row[k];
			piksele[i][j].G = img_data_row[k + 1];
			piksele[i][j].R = img_data_row[k + 2];
		}
	}

	ibmp.close();

	return piksele;
}

void bmp_write_header(string plik, bmp_header& header, bmp_info& info)
{
	// otawrcie pliku do zapisu
	ofstream ibmp(plik, ios::binary);

	if (!ibmp) {
		cout << "Blad zapisu pliku" << endl;
		exit(1);
	}

	// zapisanie nag³ówków BITMAPFILEHEADER i BITMAPINFOHEADER
	ibmp.write(reinterpret_cast<char*>(header.Type), 2);
	ibmp.write(reinterpret_cast<char*>(&header.Size), 4);
	ibmp.write(reinterpret_cast<char*>(&header.r1), 2);
	ibmp.write(reinterpret_cast<char*>(&header.r2), 2);
	ibmp.write(reinterpret_cast<char*>(&header.OffBits), 4);

	ibmp.write(reinterpret_cast<char*>(&info), sizeof(info));

	ibmp.close();
}

int bmp_write_data(string plik, bmp_header& header, bmp_info& info, pixel** dane, int& rows, int& c)
{
	// iloœæ danych o pikselach w jednym rzêdzie
	int N_row = info.Width * sizeof(pixel);
	// sprawdzenie czy iloœæ miejsca na dane jest podzielna przez 4
	// jeœli nie to dodanie odpowiedniej iloœci miejsca na zera (padding)
	if (N_row % 4 != 0) {
		N_row += 4 - (N_row % 4);
	}
	unsigned char* dane_row = new unsigned char[N_row];  // tablica danych o rzêdzie pikseli

	for (int i = 0; i < N_row; i++) dane_row[i] = 0;  // wyzerowanie rzêdu danych o pikselach

	// otwarcie pliku do zapisu
	ofstream ibmp(plik, ios::binary | ios::app);

	if (!ibmp) {
		cout << "Blad zapisu pliku" << endl;
		exit(1);
	}

	if (rows == info.Height) {   // w przypadku wczytania ca³oœci obrazu
		// co jeden rz¹d zapisanie danych z tablicy pikseli do pliku bmp
		for (int i = 0; i < info.Height; i++) {

			for (int j = 0, k = 0; j < info.Width; j++, k += 3) {

				dane_row[k] = dane[i][j].B;
				dane_row[k + 1] = dane[i][j].G;
				dane_row[k + 2] = dane[i][j].R;
			}

			ibmp.write(reinterpret_cast<char*>(dane_row), N_row);
		}
	}
	else {
		// w przypadku pierwszego kawa³ka, zapisywana jest równie¿ dolna krawêdŸ obrazu
		if (c == 0) {
			for (int i = 0; i < (rows - 1) / 2; i++) {

				// zapisanie czarnego paska jako ramka obrazu
				for (int j = 0, k = 0; j < info.Width; j++, k += 3) {
					dane_row[k] = dane[i][0].B;
					dane_row[k + 1] = dane[i][0].G;
					dane_row[k + 2] = dane[i][0].R;
				}
				ibmp.write(reinterpret_cast<char*>(dane_row), N_row);

				// zapisanie obliczonego, œrodkowego rzêdu
				for (int j = 0, k = 0; j < info.Width; j++, k += 3) {

					dane_row[k] = dane[i][j].B;
					dane_row[k + 1] = dane[i][j].G;
					dane_row[k + 2] = dane[i][j].R;
				}
				ibmp.write(reinterpret_cast<char*>(dane_row), N_row);
			}
		}
		// w przypadku ostatniego kawa³ka, zapisywana jest równie¿ górna krawêdŸ obrazu
		else if (c == info.Height - rows) {
			for (int i = 0; i < (rows - 1) / 2; i++) {

				// zapisanie obliczonego, œrodkowego rzêdu
				for (int j = 0, k = 0; j < info.Width; j++, k += 3) {

					dane_row[k] = dane[i][j].B;
					dane_row[k + 1] = dane[i][j].G;
					dane_row[k + 2] = dane[i][j].R;
				}
				ibmp.write(reinterpret_cast<char*>(dane_row), N_row);

				// zapisanie czarnego paska jako ramka obrazu
				for (int j = 0, k = 0; j < info.Width; j++, k += 3) {
					dane_row[k] = dane[i][0].B;
					dane_row[k + 1] = dane[i][0].G;
					dane_row[k + 2] = dane[i][0].R;
				}
				ibmp.write(reinterpret_cast<char*>(dane_row), N_row);
			}
		}
		// w reszcie przypadków zapisywany jest tylko obliczony, œrodkowy rz¹d
		else {
			int i = (rows - 1) / 2;

			for (int j = 0, k = 0; j < info.Width; j++, k += 3) {

				dane_row[k] = dane[i][j].B;
				dane_row[k + 1] = dane[i][j].G;
				dane_row[k + 2] = dane[i][j].R;
			}
			ibmp.write(reinterpret_cast<char*>(dane_row), N_row);
		}
	}

	delete[] dane_row;

	ibmp.close();

	return 0;
}

void print_img_data(string plik, bmp_info& info, bmp_header& head, pixel** data)
{
	// wyœwietlenie nag³ówków
	cout << endl << "--BITMAPFILEHEADER--" << endl;
	cout << "Type: " << head.Type[0] << head.Type[1] << "; Size: " << head.Size << endl;
	cout << "Reserved1: " << head.r1 << "; Reserved2: " << head.r2 << "; Off Bits: " << endl;
	cout << endl << "--BITMAPINFOHEADER--" << endl;
	cout << "Size: " << info.Size << "; Width: " << info.Width << "; Height: " << info.Height << endl;
	cout << "Planes: " << info.Planes << "; Bit Count: " << info.Bitcount << "; Compression: " << info.Compression << endl;
	cout << "Size Image: " << info.SizeImage << "; Xpels Per Meter: " << info.XpelsPerMeter << "; Ypels Per Meter: " << info.YpelsPerMeter << endl;
	cout << "Crl Uses: " << info.ClrUses << "; Crl Important: " << info.ClrImportant << endl << endl;
}

pixel** img_normalize(pixel** data, bmp_info& info)
{
	const int MAX_PX = 256;
	pixel_int min;   // minimalna niezerowa wartoœæ dystrybuanty
	min.B = 0; min.G = 0; min.R = 0;
	pixel_int hist[MAX_PX];   // tablica do zliczenia wyst¹pieñ pikseli i dystrybuanty rozk³adu
	float temp;

	// utworzenie wyjœæiowej tablicy pikseli
	pixel** out = new pixel * [info.Height];
	for (int i = 0; i < info.Height; i++) {
		out[i] = new pixel[info.Width];
		for (int j = 0; j < info.Width; j++) {
			out[i][j].B = '\0';
			out[i][j].G = '\0';
			out[i][j].R = '\0';
		}
	}
	int max = 0;
	// zliczenie wyst¹pieñ pikseli
	for (int p = 0; p < MAX_PX; p++) {

		hist[p].B = 0;
		hist[p].G = 0;
		hist[p].R = 0;
		for (int i = 0; i < info.Height; i++) {
			for (int j = 0; j < info.Width; j++) {
				
				if (data[i][j].B == p) hist[p].B++;
				if (data[i][j].G == p) hist[p].G++;
				if (data[i][j].R == p) hist[p].R++;
				if (max < data[i][j].B) max = data[i][j].B;
			}
		}
	}

	for (int p = 1; p < MAX_PX; p++) {

		// przetworzenie histogramu na dystrybuantê
		hist[p].B += hist[p - 1].B;
		hist[p].G += hist[p - 1].G;
		hist[p].R += hist[p - 1].R;


		// i wyszukanie wartoœci dystrybuanty najmniejszego elementu niezerowego
		if (hist[p].B != 0 && min.B == 0) min.B = hist[p].B;
		if (hist[p].G != 0 && min.G == 0) min.G = hist[p].G;
		if (hist[p].R != 0 && min.R == 0) min.R = hist[p].R;
	}
	
	// wyrównanie histogramu
	for (int i = 0; i < info.Height; i++) {
		for (int j = 0; j < info.Width; j++) {

			temp = (float(hist[data[i][j].B].B - min.B) / float(info.Width * info.Height - min.B)) * (MAX_PX - 1);
			out[i][j].B = round(temp);
			if (max < out[i][j].B) max = out[i][j].B;

			temp = (float(hist[data[i][j].G].G - min.G) / float(info.Width * info.Height - min.G)) * (MAX_PX - 1);
			out[i][j].G = round(temp);

			temp = (float(hist[data[i][j].R].R - min.R) / float(info.Width * info.Height - min.R)) * (MAX_PX - 1);
			out[i][j].R = round(temp);
		}
	}

	return out;
}

pixel** detect_edge(pixel** data, bmp_info& info, filter fl, int& rows)
{
	// utworzenie wyjœæiowej tablicy pikseli
	pixel** out = new pixel* [rows];
	for (int i = 0; i < rows; i++) {
		out[i] = new pixel[info.Width];
		for (int j = 0; j < info.Width; j++) {
			out[i][j].B = '\0';
			out[i][j].G = '\0';
			out[i][j].R = '\0';
		}
	}

	// utworzenie tablic filtrów dla ka¿dego kana³u koloru
	pixel_int*** filter = new pixel_int ** [fl.count];
	for (int h = 0; h < fl.count; h++) {

		filter[h] = new pixel_int * [rows];
		for (int i = 0; i < rows; i++) {

			filter[h][i] = new pixel_int[info.Width];
			for (int j = 0; j < info.Width; j++) {

				filter[h][i][j].B = 0;
				filter[h][i][j].G = 0;
				filter[h][i][j].R = 0;
			}
		}
	}
	// tymczasowe wartoœci pikseli / suma wartoœci pikseli ze wszystkich masek
	pixel_int temp, temp_minus;

	int edge_buf = (fl.size - 1) / 2;   // bufor dla pikseli granicznych które bêd¹ czarne
	// operacja konwolucji dla 8 masek
	// pêtle do przesuwania siê po tablicy pikseli z pominiêciem brzegów obrazu
	for (int m = 0; m < fl.count; m++) {

		for (int i = edge_buf; i < rows - edge_buf; i++) {
			for (int j = edge_buf; j < info.Width - edge_buf; j++) {

				temp.B = 0; temp_minus.B = 0;
				temp.G = 0; temp_minus.G = 0;
				temp.R = 0; temp_minus.R = 0;

				// pêtle do poruszania siê po masce
				for (int w = 0; w < fl.size; w++) {
					for (int k = 0; k < fl.size; k++) {

						temp.B += fl.threshold * fl.mask[m][w][k] * data[i + w - edge_buf][j + k - edge_buf].B;
						temp_minus.B += fl.threshold * -fl.mask[m][w][k] * data[i + w - edge_buf][j + k - edge_buf].B;

						temp.G += fl.threshold * fl.mask[m][w][k] * data[i + w - edge_buf][j + k - edge_buf].G;
						temp_minus.G += fl.threshold * -fl.mask[m][w][k] * data[i + w - edge_buf][j + k - edge_buf].G;

						temp.R += fl.threshold * fl.mask[m][w][k] * data[i + w - edge_buf][j + k - edge_buf].R;
						temp_minus.R += fl.threshold * -fl.mask[m][w][k] * data[i + w - edge_buf][j + k - edge_buf].R;

					}
				}
				// przepisanie tymczasowych wartoœci maski do filtra
				filter[m][i][j].B = abs(temp.B) + abs(temp_minus.B);
				filter[m][i][j].G = abs(temp.G) + abs(temp_minus.G);
				filter[m][i][j].R = abs(temp.R) + abs(temp_minus.R);
			}
		}
	}

	// wyliczenie koñcowych wartoœci pikseli do wyjœciowej tablicy pikseli
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < info.Width; j++) {

			temp.B = 0;
			temp.G = 0;
			temp.R = 0;

			for (int m = 0; m < fl.count; m++) {

				temp.B += filter[m][i][j].B;
				temp.G += filter[m][i][j].G;
				temp.R += filter[m][i][j].R;
				// obciêcie skrajnych wartoœci pikseli wychodz¹cych poza zakres <0; 255>
				if (temp.B / (fl.count * 2) > 255)
					temp.B = 255 * (fl.count * 2);
				if (temp.G / (fl.count * 2) > 255)
					temp.G = 255 * (fl.count * 2);
				if (temp.R / (fl.count * 2) > 255)
					temp.R = 255 * (fl.count * 2);
			}

			out[i][j].B = temp.B / (fl.count * 2);
			out[i][j].G = temp.G / (fl.count * 2);
			out[i][j].R = temp.R / (fl.count * 2);
		}
	}

	return out;
}

pixel** rgb2grey(bmp_info& info, pixel** dane, int& rows)
{
	// utworzenie tablicy struktury pikseli
	pixel** grey = new pixel * [rows];
	for (int i = 0; i < rows; i++) {
		grey[i] = new pixel[info.Width];
		for (int j = 0; j < info.Width; j++) {
			grey[i][j].B = '\0';
			grey[i][j].G = '\0';
			grey[i][j].R = '\0';
		}
	}

	int mean;
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < info.Width; j++) {
			// obliczenie œredniej z wartoœci pikseli kana³ów RGB
			mean = (dane[i][j].R + dane[i][j].G + dane[i][j].B) / sizeof(dane[i][j]);
			// przepisanie wartoœci œrednije na piksele RGB
			grey[i][j].R = mean;
			grey[i][j].G = mean;
			grey[i][j].B = mean;
		}
	}

	return grey;
}

string add_filters()
{
	print_menu(3);
	string ch;

	do {
		cout << endl << " > ";
		cin >> ch;

		if (ch.find("h") != ch.npos) {
			system("cls");
			cout << "Wyrownanie histogramu \"rozciaga\" wartosci pixeli na caly zakres [0, 255]." << endl;
			cout << "Odcienie szarosci oblicza srednia z kanalow RGB ktora staje sie pozniej ich wartoscia." << endl;
			cout << "Usuwanie szumu przy pomocy filtru gaussa" << endl;
			system("pause");
			system("cls");
			ch.erase();
			print_menu(3);
		}
		else break;
	} while (true);

	return ch;
}

pixel** smooth(pixel** data, bmp_info& info, int& rows)
{
	filter fl;
	fl.threshold = 1 / 16;

	// otwarcie pliku z filtrem gaussa
	fstream fs;
	fs.open("gauss.msk", fstream::in);

	//wczytanie podstawowych wielkoœci filtra

	fs >> fl.count;
	fs >> fl.size;

	// przygotowanie tablicy maski
	fl.mask = new int** [fl.count];
	for (int i = 0; i < fl.count; i++) {

		fl.mask[i] = new int* [fl.size];
		for (int j = 0; j < fl.size; j++) {

			fl.mask[i][j] = new int[fl.size];
			for (int k = 0; k < fl.size; k++) {

				fs >> fl.mask[i][j][k];
			}
		}
	}

	// utworzenie wyjœæiowej tablicy pikseli
	pixel** out = new pixel * [rows];
	for (int i = 0; i < rows; i++) {
		out[i] = new pixel[info.Width];
		for (int j = 0; j < info.Width; j++) {
			out[i][j].B = '\0';
			out[i][j].G = '\0';
			out[i][j].R = '\0';
		}
	}

	// tymczasowe wartoœci pikseli / suma wartoœci pikseli ze wszystkich masek
	pixel_int temp;

	int edge_buf = (fl.size - 1) / 2;   // bufor dla pikseli granicznych które bêd¹ czarne
	// operacja konwolucji dla 8 masek
	// pêtle do przesuwania siê po tablicy pikseli z pominiêciem brzegów obrazu
	for (int m = 0; m < fl.count; m++) {

		for (int i = 0; i < rows; i++) {
			for (int j = 0; j < info.Width; j++) {

				temp.B = 0;
				temp.G = 0;
				temp.R = 0;
				fl.threshold = 0;

				// przepisanie danych z krawêdzi obrazu
				if (i == 0 || i == rows - 1 || j == 0 || j == info.Width - 1) {
					out[i][j].B = data[i][j].B;
					out[i][j].G = data[i][j].G;
					out[i][j].R = data[i][j].R;
				}
				// tam gdzie maska nie wyjdzie poza obraz
				else {
					// pêtle do poruszania siê po masce
					for (int w = 0; w < fl.size; w++) {
						for (int k = 0; k < fl.size; k++) {

							temp.B += fl.mask[m][w][k] * data[i + w - edge_buf][j + k - edge_buf].B;

							temp.G += fl.mask[m][w][k] * data[i + w - edge_buf][j + k - edge_buf].G;

							temp.R += fl.mask[m][w][k] * data[i + w - edge_buf][j + k - edge_buf].R;

							fl.threshold += fl.mask[m][w][k];
						}
					}
					// przepisanie wyliczonych pikseli
					out[i][j].B = temp.B / fl.threshold;
					out[i][j].G = temp.G / fl.threshold;
					out[i][j].R = temp.R / fl.threshold;
				}
			}
		}
	}

	return out;
}

int main()
{
	save_filters(); // zapisanie filtrów w plikach .msk

	bmp img;
	filter filtr;
	int mode;
	string plik_out;
	string command;
	int rows;
	int chunks;

	// podanie nazwy pliku do otwarcia i wybór trybu odczytu
	mode = img.open();

	do {
		// wybór filtrów do detekcji krawêdzi
		choose_read_filters(filtr);
		// wybór dodatkowego filtra
		command = add_filters();

		system("cls");

		// wyœwietlenie informacji o pliku bmp
		cout << "Plik: " << img.filename << endl;
		print_img_data(img.filename, img.info_head, img.file_head, img.input);

		//zapisanie pliku bmp
		cout << endl;
		cout << "Podaj nazwe pliku wyjsciowego (bez rozszerzenia): ";
		cin >> plik_out;
		// dodanie rozszerzenia pliku
		plik_out.append(".bmp");

		cout << endl << "Przetwarzanie..." << endl;

		// wczytanie nag³ówka pliku bmp
		img.read_header();

		// wyznaczenie iloœci wczytanych rzêdów pikseli oraz kawa³ków
		// wczytywanego obrazu wzglêdem wybranego trybu odczytu
		if (mode == 1) {
			rows = img.info_head.Height;
			chunks = 1;
		}
		else {
			rows = filtr.size;
			chunks = img.info_head.Height - (rows - 1);
		}

		for (int c = 0; c < chunks; c++)
		{
			// wczytanie danych o samym obrazie
			img.input = bmp_data_read(img.filename, img.file_head, img.info_head, rows, c);
			img.temp = img.input;
			// zastosowanie dodatkowych filtrów jeœli wybrane
			if (command.find("1") != command.npos && mode == 1) {
				img.temp = img_normalize(img.temp, img.info_head);
			}
			if (command.find("2") != command.npos) {
				img.temp = rgb2grey(img.info_head, img.temp, rows);
			}
			if (command.find("3") != command.npos) {
				img.temp = smooth(img.temp, img.info_head, rows);
			}

			// detekcja krawêdzi
			img.temp = detect_edge(img.temp, img.info_head, filtr, rows);

			// zapisanie pliku bmp
			if (c == 0) bmp_write_header(plik_out, img.file_head, img.info_head);
			bmp_write_data(plik_out, img.file_head, img.info_head, img.temp, rows, c);
		}

		cout << endl << "Gotowe!!!" << endl << endl;
		cout << "Czy chcesz ponownie przetworzyc plik bmp? t/n" << endl;
		cout << " > ";
		do {
			command.erase();
			cin >> command;
			if (tolower(command[0]) != 't' && tolower(command[0]) != 'n') {
				cout << "Niepoprawna komenda" << endl;
				cout << " > ";
				cin >> command;
			}
			else break;
		} while (true);

		system("cls");

	} while (tolower(command[0]) != 'n');

	return 0;
}