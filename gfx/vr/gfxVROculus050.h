/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_OCULUS_050_H
#define GFX_VR_OCULUS_050_H

#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"

#include "gfxVR.h"
#include "ovr_capi_dynamic050.h"

namespace mozilla {
namespace gfx {
namespace impl {

class HMDInfoOculus050 : public VRHMDInfo {
public:
  explicit HMDInfoOculus050(ovr050::ovrHmd aHMD, bool aDebug, int aDeviceID);

  bool SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
              double zNear, double zFar) override;

  VRHMDSensorState GetSensorState(double timeOffset) override;
  void ZeroSensor() override;
  bool KeepSensorTracking() override;
  void NotifyVsync(const TimeStamp& aVsyncTimestamp) override;

  void FillDistortionConstants(uint32_t whichEye,
                               const IntSize& textureSize, const IntRect& eyeViewport,
                               const Size& destViewport, const Rect& destRect,
                               VRDistortionConstants& values) override;

  void Destroy();
  bool GetIsDebug() const;
  int GetDeviceID() const;

protected:
  virtual ~HMDInfoOculus050() {
      Destroy();
      MOZ_COUNT_DTOR_INHERITED(HMDInfoOculus050, VRHMDInfo);
  }

  bool StartSensorTracking();
  void StopSensorTracking();

  // must match the size of VRDistortionVertex
  struct DistortionVertex {
    float pos[2];
    float texR[2];
    float texG[2];
    float texB[2];
    float genericAttribs[4];
  };

  ovr050::ovrHmd mHMD;
  ovr050::ovrFovPort mFOVPort[2];
  uint32_t mTracking;
  bool mDebug; // True if this is a debug HMD
  int mDeviceID; // ID of device passed to ovrHmd_Create

  uint32_t mSensorTrackingFramesRemaining;
};

} // namespace impl

class VRHMDManagerOculus050 : public VRHMDManager
{
public:
  static already_AddRefed<VRHMDManagerOculus050> Create();
  virtual bool Init() override;
  virtual void Destroy() override;
  virtual void GetHMDs(nsTArray<RefPtr<VRHMDInfo> >& aHMDResult) override;
protected:
  VRHMDManagerOculus050()
    : mOculusInitialized(false)
  { }

  nsTArray<RefPtr<impl::HMDInfoOculus050> > mOculusHMDs;
  bool mOculusInitialized;
  RefPtr<nsIThread> mOculusThread;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_OCULUS_050_H */
