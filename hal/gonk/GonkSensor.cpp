/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <pthread.h>
#include <stdio.h>

#include "Hal.h"
#include "HalSensor.h"
#include "hardware/sensors.h"
#include "mozilla/Util.h"
#include "SensorDevice.h"

using namespace mozilla::hal;
using namespace android;

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
    default:
      return SENSOR_UNKNOWN;
  }
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
    default:
      return -1;
  }
}

static bool
SensorseventToSensorData(const sensors_event_t& data, SensorData* aSensorData)
{
  aSensorData->sensor() = HardwareSensorToHalSensor(data.type);

  if (aSensorData->sensor() == SENSOR_UNKNOWN)
    return false;

  aSensorData->timestamp() = data.timestamp;
  aSensorData->values()[0] = data.data[0];
  aSensorData->values()[1] = data.data[1];
  aSensorData->values()[2] = data.data[2];
  return true;
}

static void
onSensorChanged(const sensors_event_t& data, SensorData* aSensorData)
{
  mozilla::DebugOnly<bool> convertedData = SensorseventToSensorData(data, aSensorData);
  MOZ_ASSERT(convertedData);
  NotifySensorChange(*aSensorData);
}

namespace mozilla {
namespace hal_impl {

static pthread_t sThread;
static bool sInitialized = false;
static bool sContinue = false;
static int sActivatedSensors = 0;
static SensorData sSensordata[NUM_SENSOR_TYPE];

static void*
UpdateSensorData(void* /*unused*/)
{
  SensorDevice &device = SensorDevice::getInstance();
  const size_t numEventMax = 16;
  sensors_event_t buffer[numEventMax];
  int count = 0;

  while (sContinue) {
    count = device.poll(buffer, numEventMax);
    if (count < 0) {
      continue;
    }

    for (int i=0; i<count; i++) {
      onSensorChanged(buffer[i], &sSensordata[HardwareSensorToHalSensor(buffer[i].type)]);
    }
  }

  return NULL;
}

static void 
InitialResources()
{
  pthread_create(&sThread, NULL, &UpdateSensorData, NULL);
  sInitialized = true;
  sContinue = true;
}

static void 
ReleaseResources()
{
  sContinue = false;
  pthread_join(sThread, NULL);
  sInitialized = false;
}

static void
SensorSwitch(SensorType aSensor, bool activate)
{
  int type = HalSensorToHardwareSensor(aSensor);
  const sensor_t* sensors = NULL;
  SensorDevice& device = SensorDevice::getInstance();
  size_t size = device.getSensorList(&sensors);

  for (size_t i=0; i<size; i++) {
    if (sensors[i].type == type) {
      device.activate((void*)pthread_self(), sensors[i].handle, activate);
      break;
    }
  }
}

void
EnableSensorNotifications(SensorType aSensor) 
{
  if (!sInitialized) {
    InitialResources();
  }
  
  SensorSwitch(aSensor, true);
  sActivatedSensors++;
}

void
DisableSensorNotifications(SensorType aSensor) 
{
  if (!sInitialized) {
    NS_WARNING("Disable sensors without initializing first");
    return;
  }
  
  SensorSwitch(aSensor, false);  
  sActivatedSensors--;

  if (!sActivatedSensors) {
    ReleaseResources();  
  }
}

void 
GetCurrentSensorDataInformation(SensorType aSensor, SensorData* aSensorData) {
  *aSensorData = sSensordata[aSensor];
}

} // hal_impl
} // mozilla
