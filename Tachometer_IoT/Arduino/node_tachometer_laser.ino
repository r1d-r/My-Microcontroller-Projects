#include <ThingSpeak.h>
#include <ESP8266WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define channelID 2035500
#define limitsChannelID 2052931
#define readAPIKey "LZKLO7Q4MNCCDQTD"
#define apiKey "XUY2ZYY6MVJGGEIB"

// WiFi Details
#define ssid "Supreme"
#define password "FateFather"
#define server "api.thingspeak.com"

WiFiClient client;

//Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

//Pin declaration
const uint8_t sensor = D4;
const uint8_t alarm = D3;
const uint8_t relay = D7;

//Creating Button with LATCH
struct Button {
  const uint8_t PIN;
  bool pressed;
};

//Relay Reset Button
Button interbut = { D6, false };

unsigned long pulseDuration = 0;
unsigned long pulseStarted = 0;
volatile unsigned long pulseDuration_old = 0;
volatile unsigned long pulseStarted_old = 0;

unsigned long int RPM_max;
long sum = 0;
long re_ad[100];
long sto_re[100];
long rpm = 0;
byte max_iter = 0;
byte n = 0;
byte i = 0;

volatile bool timed_out = 1;
volatile bool pulse = 0;

//Limits
int lowerLimit, upperLimit;
const int dataFieldTwo = 1;
const int dataFieldThree = 2;

int counting;  //speed exceed count

void IRAM_ATTR ISR_sensor() {  //EVERY SENSOR RISE , INVOKED

  pulseDuration_old = micros() - pulseStarted_old;
  pulseStarted_old = micros();
  timed_out = 0;
  pulse = 1;
}

void IRAM_ATTR relay_reset() {  //RELAY RESET
  if (digitalRead(relay) == LOW) {
    digitalWrite(relay, HIGH);
    interbut.pressed = true;
  } else {
    interbut.pressed = false;
  }
}

void setup() {

  // INITIALIZE LCD
  lcd.init();
  lcd.home();
  lcd.backlight();

  pinMode(alarm, OUTPUT);
  pinMode(relay, OUTPUT);
  pinMode(interbut.PIN, INPUT_PULLUP);
  digitalWrite(relay, HIGH);

  //INTERRUPTS
  attachInterrupt(sensor, ISR_sensor, RISING);
  attachInterrupt(interbut.PIN, relay_reset, FALLING);

  //   STARTUP TEXT
  lcd.print("TACHOMETER");
  lcd.setCursor(0, 1);
  lcd.print("- W. NOEL");
  delay(2000);
  lcd.clear();
  IoTCom();
}

void loop() {

  noInterrupts();
  pulseStarted = pulseStarted_old;
  pulseDuration = pulseDuration_old;
  interrupts();

  if (((micros() - pulseStarted) > 5000000) && timed_out == 0 && pulse == 0) {

    timed_out = 1;
    rpm = 0;
    n = 0;
    i = 0;
  };

  if (timed_out == 0) {

    if (pulse) {

      re_ad[i] = (60000000 / pulseDuration);
      if (re_ad[i] <= 1.5 * re_ad[i - 1] && re_ad[i] > 30) {
        sto_re[n] = re_ad[i];
        max_iter = constrain(map(sto_re[n], 60, 100000, 0, 100), 0, 100);
        n++;
      };
      i++;
      pulse = 0;

      /*fetch values until array size becomes greater than n_max then average
      n_max depends on value of rpm_reading*/
      if (n > max_iter) {

        for (byte a = 0; a <= max_iter; a++) {

          sum = sum + sto_re[a];
        };

        rpm = sum / (max_iter + 1);
        sum = 0;
        n = 0;
        i = 0;
      }
    }

    if (rpm > RPM_max) { RPM_max = rpm; }
    updatedisplay();
    RelayTrigger(rpm);

  } else {
    Sleeping();
  };
}

void IoTCom() {  //WIFI ESTABLISHING AND FETCHING LIMITS
  // WiFi COnnecting
  lcd.print("WIFI CONNECTIN...");
  WiFi.hostname("Tachometer");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  lcd.clear();
  lcd.print("WiFi CONNECTED");
  delay(1000);
  lcd.clear();

  // Reading Limits from Cloud
  ThingSpeak.begin(client);
  lowerLimit = ThingSpeak.readFloatField(limitsChannelID, dataFieldTwo, readAPIKey);
  upperLimit = ThingSpeak.readFloatField(limitsChannelID, dataFieldThree, readAPIKey);
}

void updatedisplay() {

  lcd.clear();

  if (rpm < 100000) {

    lcd.print("ROTATING SPEED");

    lcd.setCursor(0, 1);
    lcd.print(rpm, DEC);

    lcd.setCursor(6, 1);
    lcd.print("RPM");

    ThingSpeak.writeField(channelID, 1, rpm, apiKey);

  } else {
    lcd.print("LIMIT EXCEEDED");
  }
  delay(2000);
}

void Sleeping() {  //Function for when not monitoring
  lcd.clear();
  lcd.print("MAX READING");
  lcd.setCursor(0, 1);
  lcd.print(RPM_max, DEC);
  lcd.print("   RPM");
  delay(2000);
  lcd.clear();
  lcd.print("IDLE STATE");
  lcd.setCursor(0, 1);
  lcd.print("READY TO MEASURE");
  delay(2000);
}

void RelayTrigger(int z) {  //TRIGGER PROTECTION RELAY
  if ((z < lowerLimit || z > upperLimit) && z > 60 && (z < 1.5 * upperLimit) && digitalRead(relay)) {
    counting++;
    if (counting >= 3) {
      counting = 0;
      digitalWrite(relay, LOW);
    }
  };
  
  if (~digitalRead(relay)) { //blinking at 10Hz
    digitalWrite(alarm, LOW);
    delay(1000);

    digitalWrite(alarm, HIGH);
  }else{digitalWrite(alarm, LOW);}
}