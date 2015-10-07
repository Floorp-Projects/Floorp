/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_CARDBOARD_H
#define GFX_VR_CARDBOARD_H

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Quaternion.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/HalSensor.h"
#include "mozilla/HalScreenConfiguration.h"

#include "gfxVR.h"

namespace mozilla {
namespace gfx {
namespace impl {

class HMDInfoCardboard :
    public VRHMDInfo,
    public hal::ISensorObserver,
    public hal::ScreenConfigurationObserver
{
public:
  explicit HMDInfoCardboard();

  bool SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
              double zNear, double zFar) override;

  bool StartSensorTracking() override;
  VRHMDSensorState GetSensorState(double timeOffset) override;
  void StopSensorTracking() override;
  void ZeroSensor() override;

  void FillDistortionConstants(uint32_t whichEye,
                               const IntSize& textureSize, const IntRect& eyeViewport,
                               const Size& destViewport, const Rect& destRect,
                               VRDistortionConstants& values) override;

  void Destroy();

  // ISensorObserver interface
  void Notify(const hal::SensorData& SensorData) override;
  // ScreenConfigurationObserver interface
  void Notify(const hal::ScreenConfiguration& ScreenConfiguration) override;

protected:
  // must match the size of VRDistortionVertex
  struct DistortionVertex {
    float pos[2];
    float texR[2];
    float texG[2];
    float texB[2];
    float padding[4];
  };

  virtual ~HMDInfoCardboard() {
    Destroy();
  }

  void ComputeStateFromLastSensor();

  uint32_t mStartCount;
  VRHMDSensorState mLastSensorState;
  uint32_t mOrient;
  Quaternion mScreenTransform;
  Quaternion mSensorZeroInverse;

  Quaternion mSavedLastSensor;
  double mSavedLastSensorTime;
  bool mNeedsSensorCompute;    // if we need to compute the state from mSavedLastSensor
};

} // namespace impl

class VRHMDManagerCardboard : public VRHMDManager
{
public:
  VRHMDManagerCardboard()
    : mCardboardInitialized(false)
  { }

  virtual bool PlatformInit() override;
  virtual bool Init() override;
  virtual void Destroy() override;
  virtual void GetHMDs(nsTArray<nsRefPtr<VRHMDInfo> >& aHMDResult) override;
protected:
  nsTArray<nsRefPtr<impl::HMDInfoCardboard>> mCardboardHMDs;
  bool mCardboardInitialized;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_CARDBOARD_H */
