#include <NTPClient.h>
#include <WiFiUdp.h> //timeclient için

#include "FirebaseESP8266.h"
#include <ESP8266WiFi.h>

#define ANALOG_INPUT A0
#define MUX_A D6
#define MUX_B D7
#define MUX_C D8
#define MOTOR_A_EN D0
#define MOTOR_A_IN1 D1
#define MOTOR_A_IN2 D2
#define LED_TEST_PIN D5

#define MOTOR_DONUS_SURESI 1200 // milisaniye cinsinden sulamayı başlatana kadar motorun dönme süresi
#define SULAMA_SURESI 15000 //milisaniye cinsinden bitkiyi sulama süresi

#define SIFIR_NEM 1024  //pratik ve teorik hiç ıslak olmama durumunda sensördne okunan değer
#define MAX_NEM 130     //pratikteki maksimum ıslaklık değeri
#define VARSAYILAN_SULAMA_SINIRI 1//% ıslaklığın altına düşünce sulama yapılacak. 


#define WIFI_SSID "TurkTelekom_TB9E8"
#define WIFI_PASSWORD "KeS7GEfJ"


#define FIREBASE_HOST "iot-project-36625.firebaseio.com"
#define FIREBASE_AUTH "ZdzMNK1b0IdlrFkmbIvSHhAuUba5Lnoge56ZT78e"

FirebaseData firebaseData;

//10800 sn offset, yani saati 3 saat ileri alır
//havuzdan okunan saat 3 saat geri old. böyle dengelendi.
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "tr.pool.ntp.org", 10800, 60000);

float yuzdeIslaklik;
float okunanAnalogDeger, sicaklik, value ;
String saat = ""; int SULAMA_SINIRI = VARSAYILAN_SULAMA_SINIRI;
String cihazismi = "saunodemcu";


void changeMux(int c, int b, int a);
void sula();
void testLED();
void nemKontrol();
void uygSulamaKontrol();
void printResult(FirebaseData &data);

void muxOkmuaTest();
float yuzdeNemHesapla(int val);
float sicaklikHesapla(int analogDeger);


void setup()
{
  Serial.begin(9600);

  pinMode(MUX_A, OUTPUT);
  pinMode(MUX_B, OUTPUT);
  pinMode(MUX_C, OUTPUT);
  pinMode(MOTOR_A_EN, OUTPUT);
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(LED_TEST_PIN, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();


  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  Firebase.setReadTimeout(firebaseData, 1000 * 60); //okuma timeoutu 1 dakika ayaranıyor
  Firebase.setwriteSizeLimit(firebaseData, "tiny");

  timeClient.begin();

  //alanlar başlatılıyor:
  Firebase.set(firebaseData, "/" + cihazismi + "/led/acikmi" , "false" );
  Firebase.set(firebaseData, "/" + cihazismi + "/motor/acikmi" , "false" );
  Firebase.set(firebaseData, "/" + cihazismi + "/sulamaSiniri" , VARSAYILAN_SULAMA_SINIRI );

}

//minimalistik gönderim: if (Firebase.set(firebaseData, "/bepsi/"+saat , value)){}
//else Serial.println("Bir hata meydana geldi: "+ firebaseData.errorReason() );

//if (Firebase.get(firebaseData, "/Test/float/Data3")) printResult(firebaseData);
//else Serial.println("Bir hata meydana geldi: "+firebaseData.errorReason());


void loop()
{
  //muxTraverse();

  timeClient.update();
  saat = timeClient.getFormattedTime();
  //

  uygSulamaKontrol();
  testLED();
  //muxOkmuaTest();
  muxTraverse();




}

void sula() {
  Firebase.set(firebaseData, "/" + cihazismi + "/motor/acikmi" , "true" );
  digitalWrite(MOTOR_A_EN, HIGH);

  digitalWrite(MOTOR_A_IN1, HIGH);
  delay(MOTOR_DONUS_SURESI);
  digitalWrite(MOTOR_A_IN1, LOW);

  delay(SULAMA_SURESI);

  digitalWrite(MOTOR_A_IN2, HIGH);
  delay(MOTOR_DONUS_SURESI);
  digitalWrite(MOTOR_A_IN2, LOW);

  digitalWrite(MOTOR_A_EN, LOW);
  Firebase.set(firebaseData, "/" + cihazismi + "/motor/acikmi" , "false" );


}

//uygulamadaki sulama sınırını kontrol et:
void uygSulamaKontrol() {
  //uygulamadan değişen sulama sınırını güncelle;
  if (Firebase.getInt(firebaseData, "/" + cihazismi + "/sulamaSiniri")) {
    SULAMA_SINIRI = firebaseData.intData();
    Serial.print("Sulama sınırı: "); Serial.print(SULAMA_SINIRI); Serial.println();
  }
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason());

  //uygulamadan sulama yapılıyor mı, motor çalıştırılıyor mu, kontrol:
  if (Firebase.get(firebaseData, "/" + cihazismi + "/motor/acikmi")) {
    Serial.print("motor ");
    Serial.println(firebaseData.boolData() == 1 ? "Çalışıyor" : "Kapalı");
    //SULAMA İŞLEMİ, MOTOR ÇALIŞTIRILIYOR.
    if (firebaseData.boolData() == 1 ) {
      sula();

    }
  }
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason());

}

float yuzdeNemHesapla(int val) {
  yuzdeIslaklik  = SIFIR_NEM - val; int temp; temp = SIFIR_NEM - MAX_NEM;
  yuzdeIslaklik = yuzdeIslaklik / temp;
  yuzdeIslaklik = yuzdeIslaklik * 100;
  return yuzdeIslaklik;
}
//sulama yapılmalı mı kontrol et(nem düşükse sula):
void nemKontrol() {
  //yuzdeIslaklik=( (SIFIR_NEM - A0) / (SIFIR_NEM - MAX_NEM) )  *100; şeklinde hesaplanamıyor. bu yüzden ayrı yazıldı
  value = analogRead(ANALOG_INPUT);

  Serial.print("x0(nem): %"); Serial.print(yuzdeNemHesapla(value)); Serial.print("\n");

  if (yuzdeIslaklik < SULAMA_SINIRI) {
    sula();
  }

}

void testLED() {
  if (Firebase.get(firebaseData, "/" + cihazismi + "/led/acikmi")) {
    Serial.println(firebaseData.boolData() == 1 ? "led açık" : "led kapalı");
    if (firebaseData.boolData() == 1) {
      digitalWrite(D5, HIGH);
    }
    else {
      digitalWrite(D5, LOW);
    }
  }
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason());

}

void muxTraverse() {

  //X0:
  changeMux(LOW, LOW, LOW);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 0 pin of Mux
  if (Firebase.set(firebaseData, "/" + cihazismi + "/sensorler/topraknem/" + saat , yuzdeNemHesapla(value))) {}
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason() );
  if (Firebase.set(firebaseData, "/" + cihazismi + "/anlikdegerler/topraknem/" , yuzdeNemHesapla(value))) {}
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason() );
  nemKontrol(); // nem sensörünün olduğu yerde

  //X1ışık1:
  changeMux(LOW, LOW, HIGH);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 1 pin of Mux
  Serial.print("x1(ışık): "); Serial.print(value); Serial.print("\n");
  if (Firebase.set(firebaseData, "/" + cihazismi + "/sensorler/isik1/" + saat , value)) {}
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason() );
  if (Firebase.set(firebaseData, "/" + cihazismi + "/anlikdegerler/isik1/" , value)) {}
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason() );


  //X2, ışık2:
  changeMux(LOW, HIGH, LOW);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 2 pin of Mux
  Serial.print("x2(ışık): "); Serial.print(value); Serial.print("\n");
  if (Firebase.set(firebaseData, "/" + cihazismi + "/sensorler/isik2/" + saat , value)) {}
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason() );
  if (Firebase.set(firebaseData, "/" + cihazismi + "/anlikdegerler/isik2/" , value)) {}
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason() );

  //X3: sıcaklık
  changeMux(LOW, HIGH, HIGH);
  value = sicaklikHesapla(analogRead(ANALOG_INPUT));
  Serial.print("x3(sıcaklık): "); Serial.print(value); Serial.print("\n");
  if (Firebase.set(firebaseData, "/" + cihazismi + "/sensorler/sicaklik/" + saat , value)) {}
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason() );
  if (Firebase.set(firebaseData, "/" + cihazismi + "/anlikdegerler/sicaklik/" , value)) {}
  else Serial.println("Bir hata meydana geldi: " + firebaseData.errorReason() );


}

void muxOkmuaTest() {

  //X0:
  changeMux(LOW, LOW, LOW);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 0 pin of Mux
  Serial.print("x0: "); Serial.print(value); Serial.print("\n");

  //X1:
  changeMux(LOW, LOW, HIGH);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 1 pin of Mux
  Serial.print("x1: "); Serial.print(value); Serial.print("\n");


  //X2:
  changeMux(LOW, HIGH, LOW);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 2 pin of Mux
  Serial.print("x2: "); Serial.print(value); Serial.print("\n");

  //X3
  changeMux(LOW, HIGH, HIGH);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 3 pin of Mux
  Serial.print("x3: "); Serial.print(value); Serial.print("\n");


}

float sicaklikHesapla(int analogDeger) {
  float sicaklik = analogDeger;
  sicaklik = sicaklik / 1024;
  sicaklik = sicaklik * 3300;
  sicaklik = sicaklik / 10, 0;
  sicaklik = sicaklik - 10;
  // sicaklik = (sicaklik / 1024) *1000;
  //sicaklik = sicaklik * 3,2;

  return sicaklik;

}


void printResult(FirebaseData &data)
{

  if (data.dataType() == "int")
    Serial.println(data.intData());
  else if (data.dataType() == "float")
    Serial.println(data.floatData(), 5);
  else if (data.dataType() == "double")
    printf("%.9lf\n", data.doubleData());
  else if (data.dataType() == "boolean")
    Serial.println(data.boolData() == 1 ? "true" : "false");
  else if (data.dataType() == "string")
    Serial.println(data.stringData());
  else if (data.dataType() == "json")
  {
    Serial.println();
    FirebaseJson &json = data.jsonObject();
    //Print all object data
    Serial.println("Pretty printed JSON data:");
    String jsonStr;
    json.toString(jsonStr, true);
    Serial.println(jsonStr);
    Serial.println();
    Serial.println("Iterate JSON data:");
    Serial.println();
    size_t len = json.iteratorBegin();
    String key, value = "";
    int type = 0;
    for (size_t i = 0; i < len; i++)
    {
      json.iteratorGet(i, type, key, value);
      Serial.print(i);
      Serial.print(", ");
      Serial.print("Type: ");
      Serial.print(type == JSON_OBJECT ? "object" : "array");
      if (type == JSON_OBJECT)
      {
        Serial.print(", Key: ");
        Serial.print(key);
      }
      Serial.print(", Value: ");
      Serial.println(value);
    }
    json.iteratorEnd();
  }
  else if (data.dataType() == "array")
  {
    Serial.println();
    //get array data from FirebaseData using FirebaseJsonArray object
    FirebaseJsonArray &arr = data.jsonArray();
    //Print all array values
    Serial.println("Pretty printed Array:");
    String arrStr;
    arr.toString(arrStr, true);
    Serial.println(arrStr);
    Serial.println();
    Serial.println("Iterate array values:");
    Serial.println();
    for (size_t i = 0; i < arr.size(); i++)
    {
      Serial.print(i);
      Serial.print(", Value: ");

      FirebaseJsonData &jsonData = data.jsonData();
      //Get the result data from FirebaseJsonArray object
      arr.get(jsonData, i);
      if (jsonData.typeNum == JSON_BOOL)
        Serial.println(jsonData.boolValue ? "true" : "false");
      else if (jsonData.typeNum == JSON_INT)
        Serial.println(jsonData.intValue);
      else if (jsonData.typeNum == JSON_DOUBLE)
        printf("%.9lf\n", jsonData.doubleValue);
      else if (jsonData.typeNum == JSON_STRING ||
               jsonData.typeNum == JSON_NULL ||
               jsonData.typeNum == JSON_OBJECT ||
               jsonData.typeNum == JSON_ARRAY)
        Serial.println(jsonData.stringValue);
    }
  }
}

void changeMux(int c, int b, int a) {
  digitalWrite(MUX_A, a);
  digitalWrite(MUX_B, b);
  digitalWrite(MUX_C, c);
}

/*
  changeMux(HIGH, LOW, LOW);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 4 pin of Mux
  Serial.print("x4: ");Serial.print(value);Serial.print("\n");Blynk.virtualWrite(V4, value);

  changeMux(HIGH, LOW, HIGH);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 5 pin of Mux
  Serial.print("x5: ");Serial.print(value);Serial.print("\n");Blynk.virtualWrite(V5, value);

  changeMux(HIGH, HIGH, LOW);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 6 pin of Mux
  Serial.print("x6: ");Serial.print(value);Serial.print("\n");Blynk.virtualWrite(V6, value);

  changeMux(HIGH, HIGH, HIGH);
  value = analogRead(ANALOG_INPUT); //Value of the sensor connected Option 7 pin of Mux
  Serial.print("x7: ");Serial.print(value);Serial.print("\n");Blynk.virtualWrite(V7, value);
  Serial.print("\n");
*/
