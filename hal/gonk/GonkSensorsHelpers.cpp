/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkSensorsHelpers.h"

namespace mozilla {
namespace hal {

//
// Unpacking
//

nsresult
UnpackPDU(DaemonSocketPDU& aPDU, SensorsEvent& aOut)
{
  nsresult rv = UnpackPDU(aPDU, aOut.mType);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = UnpackPDU(aPDU, aOut.mTimestamp);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = UnpackPDU(aPDU, aOut.mStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }

  size_t i = 0;

  switch (aOut.mType) {
    case SENSORS_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
    case SENSORS_TYPE_GYROSCOPE_UNCALIBRATED:
      /* 6 data values */
      rv = UnpackPDU(aPDU, aOut.mData.mFloat[i++]);
      if (NS_FAILED(rv)) {
        return rv;
      }
      /* fall through */
    case SENSORS_TYPE_ROTATION_VECTOR:
    case SENSORS_TYPE_GAME_ROTATION_VECTOR:
    case SENSORS_TYPE_GEOMAGNETIC_ROTATION_VECTOR:
      /* 5 data values */
      rv = UnpackPDU(aPDU, aOut.mData.mFloat[i++]);
      if (NS_FAILED(rv)) {
        return rv;
      }
      rv = UnpackPDU(aPDU, aOut.mData.mFloat[i++]);
      if (NS_FAILED(rv)) {
        return rv;
      }
      /* fall through */
    case SENSORS_TYPE_ACCELEROMETER:
    case SENSORS_TYPE_GEOMAGNETIC_FIELD:
    case SENSORS_TYPE_ORIENTATION:
    case SENSORS_TYPE_GYROSCOPE:
    case SENSORS_TYPE_GRAVITY:
    case SENSORS_TYPE_LINEAR_ACCELERATION:
      /* 3 data values */
      rv = UnpackPDU(aPDU, aOut.mData.mFloat[i++]);
      if (NS_FAILED(rv)) {
        return rv;
      }
      rv = UnpackPDU(aPDU, aOut.mData.mFloat[i++]);
      if (NS_FAILED(rv)) {
        return rv;
      }
      /* fall through */
    case SENSORS_TYPE_LIGHT:
    case SENSORS_TYPE_PRESSURE:
    case SENSORS_TYPE_TEMPERATURE:
    case SENSORS_TYPE_PROXIMITY:
    case SENSORS_TYPE_RELATIVE_HUMIDITY:
    case SENSORS_TYPE_AMBIENT_TEMPERATURE:
    case SENSORS_TYPE_HEART_RATE:
    case SENSORS_TYPE_TILT_DETECTOR:
    case SENSORS_TYPE_WAKE_GESTURE:
    case SENSORS_TYPE_GLANCE_GESTURE:
    case SENSORS_TYPE_PICK_UP_GESTURE:
    case SENSORS_TYPE_WRIST_TILT_GESTURE:
    case SENSORS_TYPE_SIGNIFICANT_MOTION:
    case SENSORS_TYPE_STEP_DETECTED:
      /* 1 data value */
      rv = UnpackPDU(aPDU, aOut.mData.mFloat[i++]);
      if (NS_FAILED(rv)) {
        return rv;
      }
      break;
    case SENSORS_TYPE_STEP_COUNTER:
      /* 1 data value */
      rv = UnpackPDU(aPDU, aOut.mData.mUint[0]);
      if (NS_FAILED(rv)) {
        return rv;
      }
      break;
    default:
      if (MOZ_HAL_IPC_UNPACK_WARN_IF(true, SensorsEvent)) {
        return NS_ERROR_ILLEGAL_VALUE;
      }
  }
  rv = UnpackPDU(aPDU, aOut.mDeliveryMode);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

} // namespace hal
} // namespace mozilla
