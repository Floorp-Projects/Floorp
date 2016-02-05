/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef hal_gonk_GonkSensorsHelpers_h
#define hal_gonk_GonkSensorsHelpers_h

#include <mozilla/ipc/DaemonSocketPDU.h>
#include <mozilla/ipc/DaemonSocketPDUHelpers.h>
#include "SensorsTypes.h"

namespace mozilla {
namespace hal {

using mozilla::ipc::DaemonSocketPDU;
using mozilla::ipc::DaemonSocketPDUHeader;
using mozilla::ipc::DaemonSocketPDUHelpers::Convert;
using mozilla::ipc::DaemonSocketPDUHelpers::PackPDU;
using mozilla::ipc::DaemonSocketPDUHelpers::UnpackPDU;

using namespace mozilla::ipc::DaemonSocketPDUHelpers;

//
// Conversion
//
// The functions below convert the input value to the output value's
// type and perform extension tests on the validity of the result. On
// success the output value will be returned in |aOut|. The functions
// return NS_OK on success, or an XPCOM error code otherwise.
//
// See the documentation of the HAL IPC framework for more information
// on conversion functions.
//

nsresult
Convert(int32_t aIn, SensorsStatus& aOut)
{
  static const uint8_t sStatus[] = {
    [0] = SENSORS_STATUS_NO_CONTACT, // '-1'
    [1] = SENSORS_STATUS_UNRELIABLE, // '0'
    [2] = SENSORS_STATUS_ACCURACY_LOW, // '1'
    [3] = SENSORS_STATUS_ACCURACY_MEDIUM, // '2'
    [4] = SENSORS_STATUS_ACCURACY_HIGH // '3'
  };
  static const int8_t sOffset = -1; // '-1' is the lower bound of the status

  if (MOZ_HAL_IPC_CONVERT_WARN_IF(aIn < sOffset, int32_t, SensorsStatus) ||
      MOZ_HAL_IPC_CONVERT_WARN_IF(
        aIn >= (static_cast<ssize_t>(MOZ_ARRAY_LENGTH(sStatus)) + sOffset),
        int32_t, SensorsStatus)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<SensorsStatus>(sStatus[aIn - sOffset]);
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, SensorsDeliveryMode& aOut)
{
  static const uint8_t sMode[] = {
    [0x00] = SENSORS_DELIVERY_MODE_BEST_EFFORT,
    [0x01] = SENSORS_DELIVERY_MODE_IMMEDIATE
  };
  if (MOZ_HAL_IPC_CONVERT_WARN_IF(
        aIn >= MOZ_ARRAY_LENGTH(sMode), uint8_t, SensorsDeliveryMode)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<SensorsDeliveryMode>(sMode[aIn]);
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, SensorsError& aOut)
{
  static const uint8_t sError[] = {
    [0x00] = SENSORS_ERROR_NONE,
    [0x01] = SENSORS_ERROR_FAIL,
    [0x02] = SENSORS_ERROR_NOT_READY,
    [0x03] = SENSORS_ERROR_NOMEM,
    [0x04] = SENSORS_ERROR_BUSY,
    [0x05] = SENSORS_ERROR_DONE,
    [0x06] = SENSORS_ERROR_UNSUPPORTED,
    [0x07] = SENSORS_ERROR_PARM_INVALID
  };
  if (MOZ_HAL_IPC_CONVERT_WARN_IF(
        aIn >= MOZ_ARRAY_LENGTH(sError), uint8_t, SensorsError)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<SensorsError>(sError[aIn]);
  return NS_OK;
}

nsresult
Convert(uint8_t aIn, SensorsTriggerMode& aOut)
{
  static const uint8_t sMode[] = {
    [0x00] = SENSORS_TRIGGER_MODE_CONTINUOUS,
    [0x01] = SENSORS_TRIGGER_MODE_ON_CHANGE,
    [0x02] = SENSORS_TRIGGER_MODE_ONE_SHOT,
    [0x03] = SENSORS_TRIGGER_MODE_SPECIAL
  };
  if (MOZ_HAL_IPC_CONVERT_WARN_IF(
        aIn >= MOZ_ARRAY_LENGTH(sMode), uint8_t, SensorsTriggerMode)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<SensorsTriggerMode>(sMode[aIn]);
  return NS_OK;
}

nsresult
Convert(uint32_t aIn, SensorsType& aOut)
{
  static const uint8_t sType[] = {
    [0x00] = 0, // invalid, required by gcc
    [0x01] = SENSORS_TYPE_ACCELEROMETER,
    [0x02] = SENSORS_TYPE_GEOMAGNETIC_FIELD,
    [0x03] = SENSORS_TYPE_ORIENTATION,
    [0x04] = SENSORS_TYPE_GYROSCOPE,
    [0x05] = SENSORS_TYPE_LIGHT,
    [0x06] = SENSORS_TYPE_PRESSURE,
    [0x07] = SENSORS_TYPE_TEMPERATURE,
    [0x08] = SENSORS_TYPE_PROXIMITY,
    [0x09] = SENSORS_TYPE_GRAVITY,
    [0x0a] = SENSORS_TYPE_LINEAR_ACCELERATION,
    [0x0b] = SENSORS_TYPE_ROTATION_VECTOR,
    [0x0c] = SENSORS_TYPE_RELATIVE_HUMIDITY,
    [0x0d] = SENSORS_TYPE_AMBIENT_TEMPERATURE,
    [0x0e] = SENSORS_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
    [0x0f] = SENSORS_TYPE_GAME_ROTATION_VECTOR,
    [0x10] = SENSORS_TYPE_GYROSCOPE_UNCALIBRATED,
    [0x11] = SENSORS_TYPE_SIGNIFICANT_MOTION,
    [0x12] = SENSORS_TYPE_STEP_DETECTED,
    [0x13] = SENSORS_TYPE_STEP_COUNTER,
    [0x14] = SENSORS_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
    [0x15] = SENSORS_TYPE_HEART_RATE,
    [0x16] = SENSORS_TYPE_TILT_DETECTOR,
    [0x17] = SENSORS_TYPE_WAKE_GESTURE,
    [0x18] = SENSORS_TYPE_GLANCE_GESTURE,
    [0x19] = SENSORS_TYPE_PICK_UP_GESTURE,
    [0x1a] = SENSORS_TYPE_WRIST_TILT_GESTURE
  };
  if (MOZ_HAL_IPC_CONVERT_WARN_IF(
        !aIn, uint32_t, SensorsType) ||
      MOZ_HAL_IPC_CONVERT_WARN_IF(
        aIn >= MOZ_ARRAY_LENGTH(sType), uint32_t, SensorsType)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aOut = static_cast<SensorsType>(sType[aIn]);
  return NS_OK;
}

nsresult
Convert(nsresult aIn, SensorsError& aOut)
{
  if (NS_SUCCEEDED(aIn)) {
    aOut = SENSORS_ERROR_NONE;
  } else if (aIn == NS_ERROR_OUT_OF_MEMORY) {
    aOut = SENSORS_ERROR_NOMEM;
  } else if (aIn == NS_ERROR_ILLEGAL_VALUE) {
    aOut = SENSORS_ERROR_PARM_INVALID;
  } else {
    aOut = SENSORS_ERROR_FAIL;
  }
  return NS_OK;
}

//
// Packing
//
// Pack functions store a value in PDU. See the documentation of the
// HAL IPC framework for more information.
//
// There are currently no sensor-specific pack functions necessary. If
// you add one, put it below.
//

//
// Unpacking
//
// Unpack function retrieve a value from a PDU. The functions return
// NS_OK on success, or an XPCOM error code otherwise. On sucess, the
// returned value is stored in the second argument |aOut|.
//
// See the documentation of the HAL IPC framework for more information
// on unpack functions.
//

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, SensorsDeliveryMode& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, SensorsDeliveryMode>(aOut));
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, SensorsError& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, SensorsError>(aOut));
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, SensorsEvent& aOut);

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, SensorsStatus& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<int32_t, SensorsStatus>(aOut));
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, SensorsTriggerMode& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint8_t, SensorsTriggerMode>(aOut));
}

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, SensorsType& aOut)
{
  return UnpackPDU(aPDU, UnpackConversion<uint32_t, SensorsType>(aOut));
}

} // namespace hal
} // namespace mozilla

#endif // hal_gonk_GonkSensorsHelpers_h
