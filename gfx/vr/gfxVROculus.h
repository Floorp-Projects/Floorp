/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_OCULUS_H
#define GFX_VR_OCULUS_H

#include "nsTArray.h"
#include "nsIScreen.h"
#include "nsCOMPtr.h"
#include "mozilla/nsRefPtr.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/EnumeratedArray.h"

#include "gfxVR.h"
#include "ovr_capi_dynamic.h"

namespace mozilla {
namespace gfx {
namespace impl {

class HMDInfoOculus : public VRHMDInfo {
public:
  explicit HMDInfoOculus(ovrHmd aHMD);

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

protected:
  // must match the size of VRDistortionVertex
  struct DistortionVertex {
    float pos[2];
    float texR[2];
    float texG[2];
    float texB[2];
    float genericAttribs[4];
  };

  virtual ~HMDInfoOculus() {
      Destroy();
      MOZ_COUNT_DTOR_INHERITED(HMDInfoOculus, VRHMDInfo);
  }

  ovrHmd mHMD;
  ovrFovPort mFOVPort[2];
  uint32_t mStartCount;
};

} // namespace impl

class VRHMDManagerOculus : public VRHMDManager
{
public:
  VRHMDManagerOculus()
    : mOculusInitialized(false), mOculusPlatformInitialized(false)
  { }

  virtual bool PlatformInit() override;
  virtual bool Init() override;
  virtual void Destroy() override;
  virtual void GetHMDs(nsTArray<nsRefPtr<VRHMDInfo> >& aHMDResult) override;
protected:
  nsTArray<nsRefPtr<impl::HMDInfoOculus>> mOculusHMDs;
  bool mOculusInitialized;
  bool mOculusPlatformInitialized;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_OCULUS_H */
