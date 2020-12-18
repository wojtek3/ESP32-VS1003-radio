/* 16 - xdcs
 * 17 - xcs
 * 5 - dreq
 * 18 - sck
 * 19 - miso
 * 21 - sda
 * 22 - scl
 * 23 - mosi
 * next key 15
 * prev key 0
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <WiFi.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <EEPROM.h> 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define PRESET_NUM 6

#define NEXT_KEY 15
#define PREV_KEY 0

const unsigned char image [] PROGMEM = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x3f, 0xfc, 0x00, 0x01, 0xff, 0xff, 0x80, 0x07, 0xff, 0xff, 0xe0, 0x0f, 0xfc, 0x3f, 0xf0,
0x1f, 0xc0, 0x03, 0xf8, 0x3f, 0x00, 0x00, 0xfc, 0x7c, 0x0f, 0xf0, 0x3e, 0x78, 0x7f, 0xfe, 0x1e,
0x30, 0xff, 0xff, 0x0c, 0x03, 0xff, 0xff, 0xc0, 0x07, 0xf0, 0x0f, 0xe0, 0x07, 0xc0, 0x03, 0xe0,
0x07, 0x87, 0xe1, 0xe0, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x7f, 0xfe, 0x00,
0x00, 0x78, 0x1e, 0x00, 0x00, 0x30, 0x0c, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x03, 0xc0, 0x00,
0x00, 0x03, 0xc0, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
char* ssid     = "XXXXX";
const char* password = "XXXXX";
int lastPlayed;
int playing;
bool lastNext = 1;
bool lastPrev = 1;
unsigned long timeLast = 0;
unsigned long decodeLast = 0;
unsigned long decodeTime = 0;

String title[PRESET_NUM] =     {"Weekend\nFM",
                       "ESKA\nBydgoszcz",
                       "RMF FM",
                       "Plus\nBydgoszcz",
                       "Zlote\nprzeboje",
                       "Radio ZET"};
                   
const char *host[PRESET_NUM] = {"stream.weekendfm.pl",
                       "waw01-01.ic.smcdn.pl",
                       "217.74.72.10",
                       "plu-bdg-01.cdn.eurozet.pl",
                       "stream10.radioagora.pl",
                       "zet-net-01.cdn.eurozet.pl"};
                       
const char *path[PRESET_NUM] = {"/weekendfm_najlepsza.mp3",
                       "/t054-1.mp3",
                       "/rmf_fm",
                       "/",
                       "/zloteprzeboje.mp3",
                       "/"};
                       
int httpPort[PRESET_NUM] =     {8000,
                       8000,
                       80,
                       8302,
                       8000,
                       8400};


#define VS1053_RESET   -1     // VS1053 reset pin (do resetu ESP32)
#define VS1053_CS      17     
#define VS1053_DCS     16     
#define VS1053_DREQ     5     // VS1053 Data request, najlepiej INT

Adafruit_VS1053 musicPlayer =  Adafruit_VS1053(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ);

WiFiClient client;
  
void setup() {
  pinMode(NEXT_KEY,INPUT_PULLUP);
  pinMode(PREV_KEY,INPUT_PULLUP);
  EEPROM.begin(512);
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  lastPlayed = EEPROM.read(16);                     //Dwie zmienne bo jedna przechowuje ostatnią stację, a druga aktualną
  playing = EEPROM.read(16);                        //Wystarczyłaby jedna ale EEPROM by padł od ilości zapisu
  Serial.println(lastPlayed);                       //No a żeby zrobić pojedynczy zapis bez tej lastPlayed to i tak bym musial jakiegos boola zainicjowac

  if (! musicPlayer.begin()) {                      //Debug z zatrzaskiem
      display.clearDisplay();
      display.setTextSize(1); 
      display.setTextColor(WHITE);
      display.setCursor(0,0);             
      display.println("Blad VS1003");
      display.display();
      while (1) delay(10);
  }
  
  musicPlayer.setVolume(10, 10);

  WiFi.begin(ssid, password);                       //Łączenia z bajerancką grafiką na oledzie
  while (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setTextSize(1); 
    display.setTextColor(WHITE);
    display.setCursor(0,0);             
    display.println("Laczenie z WiFi");
    display.drawBitmap(48, 24, image, 32, 32, WHITE);
    display.display();
    delay(500);
  }
  
  display.clearDisplay();
  display.setTextSize(1);             
  display.setTextColor(WHITE);
  display.setFont(&FreeSansBold12pt7b);
  
  Connect(lastPlayed);
}
uint8_t mp3buff[32];   // bufor 32bajty

void loop() {
  if(millis() - timeLast > 2000){                 //Taki niby watchdog
    decodeTime = musicPlayer.decodeTime();        //W razie gdyby przestał lecieć dźwięk z jakiejkolwiek przyczyny
    if(decodeTime == decodeLast){                 //Rejestr zliczający czas dekodowana zatrzyma się
      musicPlayer.softReset();                    //I jeśli przez 2 sekundy nie ruszy to zrestartuje dekoder mp3 i zrobi reconnect
      Connect(lastPlayed);
    }
    decodeLast = decodeTime;
    timeLast = millis();
  }
  
  if(digitalRead(NEXT_KEY) == 0 && lastNext == 1){  //wiadomo guziczki
    playing++;
    if(playing >= PRESET_NUM) playing = 0;
    Connect(playing);
    lastNext = 0;
    delay(50);
  }
  if(digitalRead(NEXT_KEY) == 1){
    lastNext = 1;
  }

  if(digitalRead(PREV_KEY) == 0 && lastPrev == 1){
    playing--;
    if(playing < 0) playing = (PRESET_NUM - 1);
    Connect(playing);
    lastPrev = 0;
    delay(50);
  }
  if(digitalRead(PREV_KEY) == 1){
    lastPrev = 1;
  }
  
  if (musicPlayer.readyForData()) {                   //Przesyłanie danych do dekodera
    if (client.available() > 0) {
      uint8_t bytesread = client.read(mp3buff, 32);
      musicPlayer.playData(mp3buff, bytesread);
    }
  }
}

void Connect(int i){               //Procedura łączenia ze stacją, wyświetlanie na oled i zapisywanie numerka stacji w EEPROMie
  if(lastPlayed != i ){
    EEPROM.write(16, i); 
    EEPROM.commit(); 
    lastPlayed = i; 
  }

  display.clearDisplay();
  display.setTextSize(1); 
  display.setCursor(0,20);             
  display.println(title[i]);
  display.display();

  client.stop();
  client.connect(host[i], httpPort[i]);
  client.print(String("GET ") + path[i] + " HTTP/1.1\r\n" +
               "Host: " + host[i] + "\r\n" + 
               "Connection: close\r\n\r\n");
}
