/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_PROXY_ORIENTATION_FALLBACK_H
#define GFX_VR_PROXY_ORIENTATION_FALLBACK_H

#include "VRDeviceProxy.h"

#include "mozilla/HalSensor.h"
#include "mozilla/HalScreenConfiguration.h"

namespace mozilla {
namespace gfx {

class VRDeviceProxyOrientationFallBack : public VRDeviceProxy
                                       , public hal::ISensorObserver
                                       , public hal::ScreenConfigurationObserver
{
public:

  explicit VRDeviceProxyOrientationFallBack(const VRDeviceUpdate& aDeviceUpdate);

  virtual void ZeroSensor() override;
  virtual VRHMDSensorState GetSensorState(double timeOffset = 0.0) override;

  // ISensorObserver interface
  void Notify(const hal::SensorData& SensorData) override;
  // ScreenConfigurationObserver interface
  void Notify(const hal::ScreenConfiguration& ScreenConfiguration) override;


protected:
  virtual ~VRDeviceProxyOrientationFallBack();

  void StartSensorTracking();
  void StopSensorTracking();
  void ComputeStateFromLastSensor();

  uint32_t mOrient;
  Quaternion mScreenTransform;
  Quaternion mSensorZeroInverse;
  Quaternion mSavedLastSensor;
  double mSavedLastSensorTime;
  bool mNeedsSensorCompute;    // if we need to compute the state from mSavedLastSensor

  bool mTracking;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_PROXY_ORIENTATION_FALLBACK_H */