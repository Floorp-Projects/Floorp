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

#include "VRDisplayHost.h"

#include <osvr/ClientKit/ClientKitC.h>
#include <osvr/ClientKit/DisplayC.h>

namespace mozilla {
namespace gfx {
namespace impl {

class VRDisplayOSVR : public VRDisplayHost
{
public:
  VRHMDSensorState GetSensorState() override;
  VRHMDSensorState GetImmediateSensorState() override;
  void ZeroSensor() override;

protected:
  virtual void StartPresentation() override;
  virtual void StopPresentation() override;

#if defined(XP_WIN)
  virtual void SubmitFrame(TextureSourceD3D11* aSource,
    const IntSize& aSize,
    const VRHMDSensorState& aSensorState,
    const gfx::Rect& aLeftEyeRect,
    const gfx::Rect& aRightEyeRect) override;
#endif

public:
  explicit VRDisplayOSVR(OSVR_ClientContext* context,
                         OSVR_ClientInterface* iface,
                         OSVR_DisplayConfig* display);

protected:
  virtual ~VRDisplayOSVR()
  {
    Destroy();
    MOZ_COUNT_DTOR_INHERITED(VRDisplayOSVR, VRDisplayHost);
  }
  void Destroy();

  OSVR_ClientContext* m_ctx;
  OSVR_ClientInterface* m_iface;
  OSVR_DisplayConfig* m_display;
};

} // namespace impl

class VRDisplayManagerOSVR : public VRDisplayManager
{
public:
  static already_AddRefed<VRDisplayManagerOSVR> Create();
  virtual bool Init() override;
  virtual void Destroy() override;
  virtual void GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult) override;

protected:
  VRDisplayManagerOSVR()
    : mOSVRInitialized(false)
    , mClientContextInitialized(false)
    , mDisplayConfigInitialized(false)
    , mInterfaceInitialized(false)
    , m_ctx(nullptr)
    , m_iface(nullptr)
    , m_display(nullptr)
  {
  }

  RefPtr<impl::VRDisplayOSVR> mHMDInfo;
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