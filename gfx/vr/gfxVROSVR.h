/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_OSVR_H
#define GFX_VR_OSVR_H

#include "nsTArray.h"
#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/EnumeratedArray.h"

#include "gfxVR.h"

#include <osvr/ClientKit/ClientKitC.h>
#include <osvr/ClientKit/DisplayC.h>

namespace mozilla {
namespace gfx {
namespace impl {

class HMDInfoOSVR : public VRHMDInfo, public VRHMDRenderingSupport
{
public:
  explicit HMDInfoOSVR(OSVR_ClientContext* context, OSVR_ClientInterface* iface,
                       OSVR_DisplayConfig* display);

  bool SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
              double zNear, double zFar) override;

  VRHMDSensorState GetSensorState() override;
  VRHMDSensorState GetImmediateSensorState() override;
  bool KeepSensorTracking() override;
  void ZeroSensor() override;
  void NotifyVsync(const TimeStamp& aVsyncTimestamp) override;

  void FillDistortionConstants(uint32_t whichEye, const IntSize& textureSize,
                               const IntRect& eyeViewport,
                               const Size& destViewport, const Rect& destRect,
                               VRDistortionConstants& values) override;

  VRHMDRenderingSupport* GetRenderingSupport() override { return this; }

  void Destroy();

  /* VRHMDRenderingSupport */
  already_AddRefed<RenderTargetSet> CreateRenderTargetSet(
    layers::Compositor* aCompositor, const IntSize& aSize) override;
  void DestroyRenderTargetSet(RenderTargetSet* aRTSet) override;
  void SubmitFrame(RenderTargetSet* aRTSet, int32_t aInputFrameID) override;

protected:
  virtual ~HMDInfoOSVR()
  {
    Destroy();
    MOZ_COUNT_DTOR_INHERITED(HMDInfoOSVR, VRHMDInfo);
  }

  // must match the size of VRDistortionVertex
  struct DistortionVertex
  {
    float pos[2];
    float texR[2];
    float texG[2];
    float texB[2];
    float genericAttribs[4];
  };

  OSVR_ClientContext* m_ctx;
  OSVR_ClientInterface* m_iface;
  OSVR_DisplayConfig* m_display;
};

} // namespace impl

class VRHMDManagerOSVR : public VRHMDManager
{
public:
  static already_AddRefed<VRHMDManagerOSVR> Create();
  virtual bool Init() override;
  virtual void Destroy() override;
  virtual void GetHMDs(nsTArray<RefPtr<VRHMDInfo>>& aHMDResult) override;

protected:
  VRHMDManagerOSVR()
    : mOSVRInitialized(false)
    , mClientContextInitialized(false)
    , mDisplayConfigInitialized(false)
    , mInterfaceInitialized(false)
    , m_ctx(nullptr)
    , m_iface(nullptr)
    , m_display(nullptr)
  {
  }

  RefPtr<impl::HMDInfoOSVR> mHMDInfo;
  bool mOSVRInitialized;
  bool mClientContextInitialized;
  bool mDisplayConfigInitialized;
  bool mInterfaceInitialized;
  RefPtr<nsIThread> mOSVRThread;

  OSVR_ClientContext m_ctx;
  OSVR_ClientInterface m_iface;
  OSVR_DisplayConfig m_display;

private:
  // check if all components are initialized
  // and if not, it will try to initialize them
  void CheckOSVRStatus();
  void InitializeClientContext();
  void InitializeDisplay();
  void InitializeInterface();
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VR_OSVR_H */