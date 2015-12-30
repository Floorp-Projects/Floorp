/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_OCULUS_H
#define GFX_VR_OCULUS_H

#include "nsTArray.h"
#include "mozilla/RefPtr.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/EnumeratedArray.h"

#include "gfxVR.h"
//#include <OVR_CAPI.h>
//#include <OVR_CAPI_D3D.h>
#include "ovr_capi_dynamic.h"

namespace mozilla {
namespace gfx {
namespace impl {

class HMDInfoOculus : public VRHMDInfo, public VRHMDRenderingSupport {
public:
  explicit HMDInfoOculus(ovrHmd aHMD, bool aDebug, int aDeviceID);

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

  VRHMDRenderingSupport* GetRenderingSupport() override { return this; }
  
  void Destroy();

  /* VRHMDRenderingSupport */
  already_AddRefed<RenderTargetSet> CreateRenderTargetSet(layers::Compositor *aCompositor, const IntSize& aSize) override;
  void DestroyRenderTargetSet(RenderTargetSet *aRTSet) override;
  void SubmitFrame(RenderTargetSet *aRTSet) override;

  ovrHmd GetOculusHMD() const { return mHMD; }
  bool GetIsDebug() const;
  int GetDeviceID() const;

protected:
  virtual ~HMDInfoOculus() {
      Destroy();
      MOZ_COUNT_DTOR_INHERITED(HMDInfoOculus, VRHMDInfo);
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

  ovrHmd mHMD;
  ovrFovPort mFOVPort[2];
  bool mTracking;
  ovrTrackingState mLastTrackingState;

  bool mDebug; // True if this is a debug HMD
  int mDeviceID; // Index of device passed to ovrHmd_Create

  uint32_t mSensorTrackingFramesRemaining;
};

} // namespace impl

class VRHMDManagerOculus : public VRHMDManager
{
public:
  static already_AddRefed<VRHMDManagerOculus> Create();
  virtual bool Init() override;
  virtual void Destroy() override;
  virtual void GetHMDs(nsTArray<RefPtr<VRHMDInfo> >& aHMDResult) override;
protected:
  VRHMDManagerOculus()
    : mOculusInitialized(false)
  { }

  nsTArray<RefPtr<impl::HMDInfoOculus>> mOculusHMDs;
  bool mOculusInitialized;
  RefPtr<nsIThread> mOculusThread;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_OCULUS_H */
