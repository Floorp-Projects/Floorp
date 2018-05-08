/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_OSVR_H
#define GFX_VR_OSVR_H

#include "nsTArray.h"
#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/EnumeratedArray.h"

#include "VRDisplayLocal.h"

#include <osvr/ClientKit/ClientKitC.h>
#include <osvr/ClientKit/DisplayC.h>

#if defined(XP_MACOSX)
class MacIOSurface;
#endif
namespace mozilla {
namespace gfx {
namespace impl {

class VRDisplayOSVR : public VRDisplayLocal
{
public:
  void ZeroSensor() override;

protected:
  VRHMDSensorState GetSensorState() override;
  virtual void StartPresentation() override;
  virtual void StopPresentation() override;

#if defined(XP_WIN)
  virtual bool SubmitFrame(ID3D11Texture2D* aSource,
                           const IntSize& aSize,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) override;
#elif defined(XP_MACOSX)
  virtual bool SubmitFrame(MacIOSurface* aMacIOSurface,
                           const IntSize& aSize,
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
    MOZ_COUNT_DTOR_INHERITED(VRDisplayOSVR, VRDisplayLocal);
  }
  void Destroy();

  OSVR_ClientContext* m_ctx;
  OSVR_ClientInterface* m_iface;
  OSVR_DisplayConfig* m_display;

  gfx::Matrix4x4 mHeadToEye[2];
};

} // namespace impl

class VRSystemManagerOSVR : public VRSystemManager
{
public:
  static already_AddRefed<VRSystemManagerOSVR> Create();
  virtual void Destroy() override;
  virtual void Shutdown() override;
  virtual void NotifyVSync() override;
  virtual void Enumerate() override;
  virtual bool ShouldInhibitEnumeration() override;
  virtual void GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult) override;
  virtual bool GetIsPresenting() override;
  virtual void HandleInput() override;
  virtual void GetControllers(nsTArray<RefPtr<VRControllerHost>>&
                              aControllerResult) override;
  virtual void ScanForControllers() override;
  virtual void RemoveControllers() override;
  virtual void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                             double aIntensity, double aDuration,
                             const VRManagerPromise& aPromise) override;
  virtual void StopVibrateHaptic(uint32_t aControllerIdx) override;

protected:
  VRSystemManagerOSVR()
    : mOSVRInitialized(false)
    , mClientContextInitialized(false)
    , mDisplayConfigInitialized(false)
    , mInterfaceInitialized(false)
    , m_ctx(nullptr)
    , m_iface(nullptr)
    , m_display(nullptr)
  {
  }

  bool Init();

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
