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

#include "mozilla/DebugOnly.h"

#include "base/basictypes.h"
#include "base/thread.h"

#include "Hal.h"
#include "HalSensor.h"
#include "hardware/sensors.h"
#include "nsThreadUtils.h"

#undef LOG

#include <android/log.h>

using namespace mozilla::hal;

#define LOGE(args...)  __android_log_print(ANDROID_LOG_ERROR, "GonkSensor" , ## args)
#define LOGW(args...)  __android_log_print(ANDROID_LOG_WARN, "GonkSensor" , ## args)

namespace mozilla {

// The value from SensorDevice.h (Android)
#define DEFAULT_DEVICE_POLL_RATE 200000000 /*200ms*/
// ProcessOrientation.cpp needs smaller poll rate to detect delay between
// different orientation angles
#define ACCELEROMETER_POLL_RATE 66667000 /*66.667ms*/

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
  SensorRunnable(const sensors_event_t& data, const sensor_t* sensors, ssize_t size)
  {
    mSensorData.sensor() = HardwareSensorToHalSensor(data.type);
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

static DebugOnly<int> sSensorRefCount[NUM_SENSOR_TYPE];
static base::Thread* sPollingThread;
static sensors_poll_device_t* sSensorDevice;
static sensors_module_t* sSensorModule;

static void
PollSensors()
{
  const size_t numEventMax = 16;
  sensors_event_t buffer[numEventMax];
  const sensor_t* sensors;
  int size = sSensorModule->get_sensors_list(sSensorModule, &sensors);

  do {
    // didn't check sSensorDevice because already be done on creating pollingThread.
    int n = sSensorDevice->poll(sSensorDevice, buffer, numEventMax);
    if (n < 0) {
      LOGE("Error polling for sensor data (err=%d)", n);
      break;
    }

    for (int i = 0; i < n; ++i) {
      // FIXME: bug 802004, add proper support for the magnetic field sensor.
      if (buffer[i].type == SENSOR_TYPE_MAGNETIC_FIELD)
        continue;

      // Bug 938035, transfer HAL data for orientation sensor to meet w3c spec
      // ex: HAL report alpha=90 means East but alpha=90 means West in w3c spec
      if (buffer[i].type == SENSOR_TYPE_ORIENTATION) {
        buffer[i].orientation.azimuth = 360 - buffer[i].orientation.azimuth;
        buffer[i].orientation.pitch = -buffer[i].orientation.pitch;
        buffer[i].orientation.roll = -buffer[i].orientation.roll;
      }

      if (HardwareSensorToHalSensor(buffer[i].type) == SENSOR_UNKNOWN) {
        // Emulator is broken and gives us events without types set
        int index;
        for (index = 0; index < size; index++) {
          if (sensors[index].handle == buffer[i].sensor) {
            break;
          }
        }
        if (index < size &&
            HardwareSensorToHalSensor(sensors[index].type) != SENSOR_UNKNOWN) {
          buffer[i].type = sensors[index].type;
        } else {
          LOGW("Could not determine sensor type of event");
          continue;
        }
      }

      NS_DispatchToMainThread(new SensorRunnable(buffer[i], sensors, size));
    }
  } while (true);
}

static void
SwitchSensor(bool aActivate, sensor_t aSensor, pthread_t aThreadId)
{
  int index = HardwareSensorToHalSensor(aSensor.type);

  MOZ_ASSERT(sSensorRefCount[index] || aActivate);

  sSensorDevice->activate(sSensorDevice, aSensor.handle, aActivate);

  if (aActivate) {
    if (aSensor.type == SENSOR_TYPE_ACCELEROMETER) {
      sSensorDevice->setDelay(sSensorDevice, aSensor.handle,
                   ACCELEROMETER_POLL_RATE);
    } else {
      sSensorDevice->setDelay(sSensorDevice, aSensor.handle,
                   DEFAULT_DEVICE_POLL_RATE);
    }
  }

  if (aActivate) {
    sSensorRefCount[index]++;
  } else {
    sSensorRefCount[index]--;
  }
}

static void
SetSensorState(SensorType aSensor, bool activate)
{
  int type = HalSensorToHardwareSensor(aSensor);
  const sensor_t* sensors = nullptr;

  int size = sSensorModule->get_sensors_list(sSensorModule, &sensors);
  for (ssize_t i = 0; i < size; i++) {
    if (sensors[i].type == type) {
      SwitchSensor(activate, sensors[i], pthread_self());
      break;
    }
  }
}

void
EnableSensorNotifications(SensorType aSensor)
{
  if (!sSensorModule) {
    hw_get_module(SENSORS_HARDWARE_MODULE_ID,
                       (hw_module_t const**)&sSensorModule);
    if (!sSensorModule) {
      LOGE("Can't get sensor HAL module\n");
      return;
    }

    sensors_open(&sSensorModule->common, &sSensorDevice);
    if (!sSensorDevice) {
      sSensorModule = nullptr;
      LOGE("Can't get sensor poll device from module \n");
      return;
    }

    sensor_t const* sensors;
    int count = sSensorModule->get_sensors_list(sSensorModule, &sensors);
    for (size_t i=0 ; i<size_t(count) ; i++) {
      sSensorDevice->activate(sSensorDevice, sensors[i].handle, 0);
    }
  }

  if (!sPollingThread) {
    sPollingThread = new base::Thread("GonkSensors");
    MOZ_ASSERT(sPollingThread);
    // sPollingThread never terminates because poll may never return
    sPollingThread->Start();
    sPollingThread->message_loop()->PostTask(FROM_HERE,
                                     NewRunnableFunction(PollSensors));
  }

  SetSensorState(aSensor, true);
}

void
DisableSensorNotifications(SensorType aSensor)
{
  if (!sSensorModule) {
    return;
  }
  SetSensorState(aSensor, false);
}

} // hal_impl
} // mozilla
