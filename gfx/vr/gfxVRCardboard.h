/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_CARDBOARD_H
#define GFX_VR_CARDBOARD_H

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Quaternion.h"
#include "mozilla/EnumeratedArray.h"

#include "gfxVR.h"

namespace mozilla {
namespace gfx {
namespace impl {

class HMDInfoCardboard :
    public VRHMDInfo
{
public:
  explicit HMDInfoCardboard();

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

protected:
  virtual ~HMDInfoCardboard() {
    MOZ_COUNT_DTOR_INHERITED(HMDInfoCardboard, VRHMDInfo);
    Destroy();
  }

  // must match the size of VRDistortionVertex
  struct DistortionVertex {
    float pos[2];
    float texR[2];
    float texG[2];
    float texB[2];
    float padding[4];
  };

};

} // namespace impl

class VRHMDManagerCardboard : public VRHMDManager
{
public:
  static already_AddRefed<VRHMDManagerCardboard> Create();
  virtual bool Init() override;
  virtual void Destroy() override;
  virtual void GetHMDs(nsTArray<RefPtr<VRHMDInfo> >& aHMDResult) override;
protected:
  VRHMDManagerCardboard()
    : mCardboardInitialized(false)
  { }
  nsTArray<RefPtr<impl::HMDInfoCardboard>> mCardboardHMDs;
  bool mCardboardInitialized;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_CARDBOARD_H */
