/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pthread.h>
#include <stdio.h>

#include "base/basictypes.h"
#include "base/thread.h"

#include "Hal.h"
#include "HalSensor.h"
#include "hardware/sensors.h"
#include "mozilla/Util.h"
#include "SensorDevice.h"
#undef LOG

#include <android/log.h>

using namespace android;
using namespace mozilla::hal;

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GonkSensor" , ## args)

namespace mozilla {

#define DEFAULT_DEVICE_POLL_RATE 100000000 /*100ms*/

double radToDeg(double a) {
  return a * (180.0 / M_PI);
}

static SensorType
HardwareSensorToHalSensor(int type)
{     
  switch(type) {
    case SENSOR_TYPE_ORIENTATION:
      return SENSOR_ORIENTATION;
    case SENSOR_TYPE_ACCELEROMETER:
      return SENSOR_ACCELERATION;
    case SENSOR_TYPE_PROXIMITY:
      return SENSOR_PROXIMITY;
    case SENSOR_TYPE_LIGHT:
      return SENSOR_LIGHT;
    case SENSOR_TYPE_GYROSCOPE:
      return SENSOR_GYROSCOPE;
    case SENSOR_TYPE_LINEAR_ACCELERATION:
      return SENSOR_LINEAR_ACCELERATION;
    default:
      return SENSOR_UNKNOWN;
  }
}

static SensorAccuracyType
HardwareStatusToHalAccuracy(int status) {
  return static_cast<SensorAccuracyType>(status);
}

static int
HalSensorToHardwareSensor(SensorType type)
{
  switch(type) {
    case SENSOR_ORIENTATION:
      return SENSOR_TYPE_ORIENTATION;
    case SENSOR_ACCELERATION:
      return SENSOR_TYPE_ACCELEROMETER;
    case SENSOR_PROXIMITY:
      return SENSOR_TYPE_PROXIMITY;
    case SENSOR_LIGHT:
      return SENSOR_TYPE_LIGHT;
    case SENSOR_GYROSCOPE:
      return SENSOR_TYPE_GYROSCOPE;
    case SENSOR_LINEAR_ACCELERATION:
      return SENSOR_TYPE_LINEAR_ACCELERATION;
    default:
      return -1;
  }
}

static int
SensorseventStatus(const sensors_event_t& data)
{
  int type = data.type;
  switch(type) {
    case SENSOR_ORIENTATION:
      return data.orientation.status;
    case SENSOR_LINEAR_ACCELERATION:
    case SENSOR_ACCELERATION:
      return data.acceleration.status;
    case SENSOR_GYROSCOPE:
      return data.gyro.status;
  }

  return SENSOR_STATUS_UNRELIABLE;
}

class SensorRunnable : public nsRunnable
{
public:
  SensorRunnable(const sensors_event_t& data)
  {
    mSensorData.sensor() = HardwareSensorToHalSensor(data.type);
    if (mSensorData.sensor() == SENSOR_UNKNOWN) {
      // Emulator is broken and gives us events without types set
      const sensor_t* sensors = NULL;
      SensorDevice& device = SensorDevice::getInstance();
      ssize_t size = device.getSensorList(&sensors);
      if (data.sensor < size)
        mSensorData.sensor() = HardwareSensorToHalSensor(sensors[data.sensor].type);
    }
    mSensorData.accuracy() = HardwareStatusToHalAccuracy(SensorseventStatus(data));
    mSensorData.timestamp() = data.timestamp;
    if (mSensorData.sensor() == SENSOR_GYROSCOPE) {
      // libhardware returns gyro as rad.  convert.
      mSensorValues.AppendElement(radToDeg(data.data[0]));
      mSensorValues.AppendElement(radToDeg(data.data[1]));
      mSensorValues.AppendElement(radToDeg(data.data[2]));
    } else if (mSensorData.sensor() == SENSOR_PROXIMITY) {
      mSensorValues.AppendElement(data.data[0]);
      mSensorValues.AppendElement(0);     

      // Determine the maxRange for this sensor.
      const sensor_t* sensors = NULL;
      ssize_t size = SensorDevice::getInstance().getSensorList(&sensors);
      for (ssize_t i = 0; i < size; i++) {
        if (sensors[i].type == SENSOR_TYPE_PROXIMITY) {
          mSensorValues.AppendElement(sensors[i].maxRange);     
        }
      }
    } else if (mSensorData.sensor() == SENSOR_LIGHT) {
      mSensorValues.AppendElement(data.data[0]);
    } else {
      mSensorValues.AppendElement(data.data[0]);
      mSensorValues.AppendElement(data.data[1]);
      mSensorValues.AppendElement(data.data[2]);
    }
    mSensorData.values() = mSensorValues;
  }

  ~SensorRunnable() {}

  NS_IMETHOD Run()
  {
    NotifySensorChange(mSensorData);
    return NS_OK;
  }

private:
  SensorData mSensorData;
  InfallibleTArray<float> mSensorValues;
};

namespace hal_impl {

static int sActivatedSensors = 0;
static DebugOnly<int> sSensorRefCount[NUM_SENSOR_TYPE];
static base::Thread* sSwitchThread;

static void
PollSensorsOnce()
{
  if (!sActivatedSensors) {
    return;
  }

  SensorDevice &device = SensorDevice::getInstance();
  const size_t numEventMax = 16;
  sensors_event_t buffer[numEventMax];

  int n = device.poll(buffer, numEventMax);
  if (n < 0) {
    LOG("Error polling for sensor data (err=%d)", n);
    return;
  }

  for (int i = 0; i < n; ++i) {
    NS_DispatchToMainThread(new SensorRunnable(buffer[i]));
  }

  if (sActivatedSensors) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     NewRunnableFunction(PollSensorsOnce));
  }
}

static void
SwitchSensor(bool aActivate, sensor_t aSensor, pthread_t aThreadId)
{
  int index = HardwareSensorToHalSensor(aSensor.type);

  MOZ_ASSERT(sSensorRefCount[index] || aActivate);

  SensorDevice& device = SensorDevice::getInstance();

  device.activate((void*)aThreadId, aSensor.handle, aActivate);
  device.setDelay((void*)aThreadId, aSensor.handle, DEFAULT_DEVICE_POLL_RATE);

  if (aActivate) {
    if (++sActivatedSensors == 1) {
      MessageLoop::current()->PostTask(FROM_HERE,
                                       NewRunnableFunction(PollSensorsOnce));
    }
    sSensorRefCount[index]++;
  } else {
    sSensorRefCount[index]--;
    --sActivatedSensors;
  }
}

static void
SetSensorState(SensorType aSensor, bool activate)
{
  int type = HalSensorToHardwareSensor(aSensor);
  const sensor_t* sensors = NULL;
  SensorDevice& device = SensorDevice::getInstance();
  ssize_t size = device.getSensorList(&sensors);
  for (ssize_t i = 0; i < size; i++) {
    if (sensors[i].type == type) {
      // Post an event to the sensor thread
      sSwitchThread->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(SwitchSensor,
                            activate, sensors[i], pthread_self()));
      break;
    }
  }
}

void
EnableSensorNotifications(SensorType aSensor) 
{
  if (!sSwitchThread) {
    sSwitchThread = new base::Thread("GonkSensors");
    sSwitchThread->Start();
  }
  
  SetSensorState(aSensor, true);
}

void
DisableSensorNotifications(SensorType aSensor) 
{
  SetSensorState(aSensor, false);
}

} // hal_impl
} // mozilla
