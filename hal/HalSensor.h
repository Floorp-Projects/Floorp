/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: sw=2 ts=8 et ft=cpp : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sinker Li <thinker@codemud.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __HAL_SENSOR_H_
#define __HAL_SENSOR_H_

#include "mozilla/Observer.h"

namespace mozilla {
namespace hal {

/**
 * Enumeration of sensor types.  They are used to specify type while
 * register or unregister an observer for a sensor of given type.
 */
enum SensorType {
  SENSOR_UNKNOWN = -1,
  SENSOR_ORIENTATION,
  SENSOR_ACCELERATION,
  SENSOR_PROXIMITY,
  SENSOR_LINEAR_ACCELERATION,
  SENSOR_GYROSCOPE,
  NUM_SENSOR_TYPE
};

class SensorData;

typedef Observer<SensorData> ISensorObserver;

/**
 * Enumeration of sensor accuracy types.
 */
enum SensorAccuracyType {
  SENSOR_ACCURACY_UNKNOWN = -1,
  SENSOR_ACCURACY_UNRELIABLE,
  SENSOR_ACCURACY_LOW,
  SENSOR_ACCURACY_MED,
  SENSOR_ACCURACY_HIGH,
  NUM_SENSOR_ACCURACY_TYPE
};

class SensorAccuracy;

typedef Observer<SensorAccuracy> ISensorAccuracyObserver;

}
}

#include "IPC/IPCMessageUtils.h"

namespace IPC {
  /**
   * Serializer for SensorType
   */
  template <>
  struct ParamTraits<mozilla::hal::SensorType>:
    public EnumSerializer<mozilla::hal::SensorType,
                          mozilla::hal::SENSOR_UNKNOWN,
                          mozilla::hal::NUM_SENSOR_TYPE> {
  };

  template <>
  struct ParamTraits<mozilla::hal::SensorAccuracyType>:
    public EnumSerializer<mozilla::hal::SensorAccuracyType,
                          mozilla::hal::SENSOR_ACCURACY_UNKNOWN,
                          mozilla::hal::NUM_SENSOR_ACCURACY_TYPE> {

  };
} // namespace IPC

#endif /* __HAL_SENSOR_H_ */
