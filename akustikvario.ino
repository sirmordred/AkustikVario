//**********************************
//*   Akustik vario ver2.0 Design Oguzhan/Osman    *
//*********************************


#include <Wire.h>

const byte led = 13;
unsigned int calibrationData[7];
unsigned long time = 0;
float toneFreq, toneFreqLowpass, pressure, lowpassFast, lowpassSlow;
float p0; // this will be used to store the airfield elevation pressure
float total = 0;
float t1 = 0;
float alt1;
int altitude;
int ch1; // Here's where we'll keep our channel values
int ddsAcc;
float climbRate = 0;
int lastaltitude;
int firstaltitude;
int firstseq;
int variation = 0;
unsigned long clock;
double sec;
double factor = 1000000;
int climbSpeed;


void setup() {
  Wire.begin();
  Serial.begin(9600); // in case you want to use the included serial.print debugging commands that are currently commented out
  delay(200);
  setupSensor();
  for (int a = 1; a <= 60; a++) {
    pressure = getPressure(); // warming up the sensor by reading it 60 times for ground level setting
  }
  for (int z = 0; z < 40; z++) {
    t1 += getPressure();
  }
  pressure = t1 / 40f;
  p0 = pressure; // Setting the ground level pressure
  lowpassFast = lowpassSlow = pressure;
  pinMode(3, INPUT); // Set our input pins as such for altitude command input from receiver via pin D3
  firstseq = 1;
}


void loop() {
  clock = micros();
  for(int i = 0; i < 10; i++) { //Read the value by 10 times
    // read and add the value to the total:
    total += getPressure();
  }
  pressure = total / 10f; //Divide total to the number of readings(10)
  alt1 = (float)44330 * (1 - pow(((float) pressure/p0), 0.190295));
  altitude = round(alt1);
  if (firstseq) {
    firstaltitude = altitude;
    firstseq = 0;
  } else {
    lastaltitude = altitude;
    variation = lastaltitude - firstaltitude;
    firstaltitude = lastaltitude;
  }
  
  lowpassFast = lowpassFast + (pressure - lowpassFast) * 0.1;
  lowpassSlow = lowpassSlow + (pressure - lowpassSlow) * 0.05;
  toneFreq = (lowpassSlow - lowpassFast) * 50;
  toneFreqLowpass = toneFreqLowpass + (toneFreq - toneFreqLowpass) * 0.1;
  toneFreq = constrain(toneFreqLowpass, -500, 500);
  ddsAcc += toneFreq * 100 + 2000;
  
  if (toneFreq < -40 || (ddsAcc > 0 && toneFreq > 40)) {
    tone(2, toneFreq + 510);
    digitalWrite(led,1);  // the Arduino led will blink if the Vario plays a tone, so you can test without having audio connected
  } else {
    noTone(2);
    digitalWrite(led,0);
  }
  
  int ones = altitude%10;
  int tens = (altitude/10)%10;
  int hundreds = (altitude/100)%10;
  int thousands = (altitude/1000)%10;
 
  while (millis() < time);        //loop frequency timer
  time += 20;

  ch1 = pulseIn(3, HIGH, 25000); // Read the pulse width of servo signal connected to pin D3

  if((map(ch1, 1000,2000,-500,500)) > 0) { // interpret the servo channel pulse, if the Vario should beep altitude or send vario sound 
    noTone(2); // create 750 ms of silence, or you won't hear the first altitude beep
    digitalWrite(led,0);
    delay(750);
    if(hundreds == 0) {
      tone(2,900);                //long duration tone if the number is zero
      digitalWrite(led,1);
      delay(600);
      noTone(2);
      digitalWrite(led,0);
    } else {
      for(int h = 1; h <= hundreds; h++) {        //this loop makes a beep for each hundred meters altitude
        tone(2,900); // 900 Hz tone frequency for the hundreds
        digitalWrite(led,1);
        delay(200);
        noTone(2);
        digitalWrite(led,0);
        delay(200);
      }
    delay(750);                            //longer delay between hundreds and tens
    }

    if(tens == 0) {
      tone(2,1100);                //long pulse if the number is zero
      digitalWrite(led,1);
      delay(600);
      noTone(2);
      digitalWrite(led,0);
    } else {
      for(int g = 1; g <= tens; g++) {          //this loop makes a beep for each ten meters altitude
        tone(2,1100); //1100 Hz tone frequency for the tens
        digitalWrite(led,1);
        delay(200);
        noTone(2);
        digitalWrite(led,0);
        delay(200);
      }
    }

    for (int s = 1; s <= 30; s++) {
      pressure = getPressure(); // warming up the sensor again, by reading it 30 times
    } 
  }
  total = 0;
  clock = micros() - clock;
  sec = clock/factor;
  climbRate = float(variation) / sec;
  climbSpeed = round(climbRate);
  Serial.println("Climb speed (m/sn) = ");
  Serial.print(climbSpeed);
}


long getPressure() {
  long D1, D2, dT, P;
  float TEMP;
  int64_t OFF, SENS;

  D1 = getData(0x48, 10);
  D2 = getData(0x50, 1);

  dT = D2 - ((long)calibrationData[5] << 8);
  TEMP = (2000 + (((int64_t)dT * (int64_t)calibrationData[6]) >> 23)) / (float)100;
  OFF = ((unsigned long)calibrationData[2] << 16) + (((int64_t)calibrationData[4] * dT) >> 7);
  SENS = ((unsigned long)calibrationData[1] << 15) + (((int64_t)calibrationData[3] * dT) >> 8);
  P = (((D1 * SENS) >> 21) - OFF) >> 15;

  return P;
}


long getData(byte command, byte del) {
  long result = 0;
  twiSendCommand(0x77, command);
  delay(del);
  twiSendCommand(0x77, 0x00);
  Wire.requestFrom(0x77, 3);
  if(Wire.available()!=3) Serial.println("Error: raw data not available");
  for (int i = 0; i <= 2; i++) {
    result = (result<<8) | Wire.read(); 
  }
  return result;
}


void setupSensor() {
  twiSendCommand(0x77, 0x1e);
  delay(100);

  for (byte i = 1; i <= 6; i++) {
    unsigned int low, high;
    twiSendCommand(0x77, 0xa0 + i * 2);
    Wire.requestFrom(0x77, 2);
    if(Wire.available()!=2) Serial.println("Error: calibration data not available");
    high = Wire.read();
    low = Wire.read();
    calibrationData[i] = high<<8 | low;
  }
}


void twiSendCommand(byte address, byte command) {
  Wire.beginTransmission(address);
  if (!Wire.write(command)) Serial.println("Error: write()");
  if (Wire.endTransmission()) {
    Serial.print("Error when sending command: ");
    Serial.println(command, HEX);
  }
}
