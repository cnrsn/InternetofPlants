//------------ Genel Kütüphanaler ------------ 
#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>//esp kartta mevcut yüklemeye gerek yok

//------------  Blynk ------------ 
#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp8266.h>
BlynkTimer timer; //senkronu sağlamak için bir sayaç nesnesi oluşturuluyor.

//------------ WiFi Manager ------------ 
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "ESP8266TimerInterrupt.h" //interrupt kullanımı için gerekli kütüphaneyi yarattık.https://github.com/khoih-prog/ESP8266TimerInterrupt
bool res; //wifimanager oto bağlantı durumu takibi için
int buttonSayac=0;
boolean button=false;
boolean resetle=false;

// ------------ SICAKLIK SENSÖRÜ ------------ 
#include <Adafruit_AHTX0.h>
Adafruit_AHTX0 aht;

// ------------ IŞIK SENSÖRÜ ------------ 
#include <BH1750.h>       // adds BH1750 library file 
BH1750 lightMeter(0x23); 

// ------------ Değişkenler ------------ 
long now = millis();
long lastMeasure = 0;
const int analogInPin = A0;  // ESP8266 Analog Pin ADC0 = A0
int SoilSensor = 0;  // value read from the pot
float h,t,lux;

//////////Bir adet wifimanager nesnesi oluşturduk.////////////////////////////////
WiFiManager wm;


char blynk_token[] = "blynk_token_buraya_girilecek"; //kodda birşey girmeyin, wifi'ye bağlanırken girilecek

//verilerin kaydedilmesi için kontrol değişkeni
bool shouldSaveConfig = false;

//ayarların kaydedilmesini hatırlatmak için
void saveConfigCallback () {
  Serial.println("Ayarlar Kaydedilmeli");
  shouldSaveConfig = true;
}


// ------------ Sensör veri ve Bildirim ayarları ------------ 
void sendSensor(){    
  // Sensör değer oku
  SoilSensor = analogRead(analogInPin); // Toprak nem sensörü
  //Serial.println("toprak: "+ String(SoilSensor));
  SoilSensor = map(SoilSensor, 327, 753, 100, 0);  // Toprak nem sensörü değer ölçeklendirmesi (0-100)
  if(SoilSensor<3){SoilSensor=0;} 
  if(SoilSensor>100){SoilSensor=100;} 
  //float h = dht.readHumidity(); // Ortam nem sensör
  //float t = dht.readTemperature(); // Ortam Sıcaklığı (default: Celcius) 

  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  t=temp.temperature;
  h=humidity.relative_humidity;
  float lux = lightMeter.readLightLevel(); // Ortam ışığı
  
  // BLYNK veri gönder. Saniyede 10 defadan fazla veri göndermeyin.
  Blynk.virtualWrite(V5, t); 
  Blynk.virtualWrite(V6, h); 
  Blynk.virtualWrite(V7, SoilSensor);
  Blynk.virtualWrite(V8, lux);

  //Serial com send text
  Serial.println("Sicaklik: "+ String(t) +"*C // "+ "Nem: "+ String(h)+ "%" + "//" + "Light: " + String(lux) + "lx" + " // " + "Soil:" + String(SoilSensor)); 

  // Bildirim kuralları
  /*if(t > 30){
    //Blynk.email("isimsoyisim@email.com", "HomePlantProject EsP8266 Alert", "Temperature over 30C!");
    Blynk.notify("HomePlantProject EsP8266 Alert - Temperature over 30C!");
  }
  if(t < 0){
    //Blynk.email("isimsoyisim@email.com", "HomePlantProject EsP8266 Alert", "Temperature below 0C!");
    Blynk.notify("HomePlantProject EsP8266 Alert - Temperature below 0C!");
  }
*/
}



//wifi bilgilerini eproma yazıyoruz
void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
   EEPROM.commit();
   delay(500);
}

//epromdan okuyoruz
String readStringFromEEPROM(int addrOffset)
{
int newStrLen = EEPROM.read(addrOffset);
char data[newStrLen + 1];
for (int i = 0; i < newStrLen; i++)
{
data[i] = EEPROM.read(addrOffset + 1 + i);
}
data[newStrLen] = '\0';
return String(data);
}


//Interrupt özellikli reset butonu fonksiyonu
ICACHE_RAM_ATTR void resetlefonk(){
 
    if (!button){
      if (digitalRead(12)==HIGH){
        buttonSayac=millis();
        button=true;
      }
    }
  else{
    if (digitalRead(12)==LOW){
        button=false;
        if ((millis()-buttonSayac)>=50){
          if ((millis()-buttonSayac)>=3000){
            Serial.println(">3 sn basıldı resetliyor");
              wm.resetSettings();
              ESP.restart();
             resetle=true;
          }
          else{            
            Serial.println("<3 sn");
          }
        }
    }
  }
  
}

// ------------ Esp8266 Başlayınca 1 kere çalışacak ayarlar ------------ 
void setup()
{
  EEPROM.begin(512);
  Serial.begin(9600);
  delay(2000);
  aht.begin();
  Wire.begin(); // SDA SCL için eklendi
  lightMeter.begin(); 
  uint8_t Relay_Pin = D7;       // declare RELAY pin
  uint8_t Reset_Pin = D6;       // declare EEPROM WIFI DATA RESET pin
  pinMode(Relay_Pin, OUTPUT);
  pinMode(Reset_Pin, INPUT);
  digitalWrite(Relay_Pin, HIGH);
  
  
  
  /////////////WIFI MANAGER AYAR BLOĞU//////////////////////////////////////////////////
 
  //wm.resetSettings(); //bu satırı açarsak hafızadaki wifi ayarlarını temizler.
  wm.setSaveConfigCallback(saveConfigCallback);
  
  WiFi.mode(WIFI_STA); // Özellikle modu station'a ayarlıyoruz.
  
  WiFiManagerParameter custom_blynk_token("Blynk", "blynk token", blynk_token, 34);
  wm.addParameter(&custom_blynk_token);

  res = wm.autoConnect("IOPlants", "12345678"); // Wifimanager bağlanma satırı. Ağ adı olarak görünmesini istediğimiz
  // ismi ve belirleyeceğimiz şifreyi tanımladık. İstersek şifre de girmeyebiliriz. Korumasız ağ olarak görünür.
  
  if (!res) {
    Serial.println("Bağlantı Sağlanamadı");
    ESP.restart();
  }
  else {
    //buraya gelmişse WiFi'ya bağlanmış demektir.
    Serial.println("Ağ Bağlantısı Kuruldu");
  }

 strcpy(blynk_token, custom_blynk_token.getValue());
 Serial.println("blink token için ilk değer:");
 Serial.println(blynk_token);
 //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving to eeprom:");
    String yazilacak=String(blynk_token);
    writeStringToEEPROM(100, yazilacak);
    Serial.print("eproma yazdı:");
    Serial.println(yazilacak);
   //end save
    shouldSaveConfig = false;
  }

  int index=EEPROM.read(100);
  Serial.print("epromdan okunan index :");
  Serial.println(index);
  
  String token =readStringFromEEPROM(100);
  token.toCharArray(blynk_token,34); 
   
  Serial.print("epromdan okudu:");
  Serial.println(token);
  Serial.println(blynk_token);
  
  
  Blynk.config(blynk_token);
  delay(100);
  //Blynk.config(blynk_token);
  bool result = Blynk.connect();

  if (result != true)
    {
      Serial.println("BLYNK Connection Fail");
      wm.resetSettings();
      ESP.reset();
      delay (5000);
    }
    else
    {
     Serial.println("BLYNK Connected");
    }


/////////////////////////////////////////////////////////////////////////////////////////



// ------------ ağ bilgilerini resetlemek için D6(GPIO12) pininde kullandığımız buton için kesme tanımlıyoruz. ------------
attachInterrupt(digitalPinToInterrupt(12), resetlefonk, CHANGE); 
}

void loop()
{
  Blynk.run(); 
  timer.run(); 
  //resetlefonk(); //eprom resetleme
  
  //resetleme butonunu takip ediyor
  if (resetle){
      resetle=false;
      wm.resetSettings();//wifi manager ayarlarını silip esp'yi yeniden başlatıyoruz.
      ESP.restart();
    }
  now = millis();
  // Yeni sıcaklık ve nem değerlerini her 1 saniyede bir publish eder
  if (now - lastMeasure > 5000) {
    lastMeasure = now;
    sendSensor();
  }
}    
