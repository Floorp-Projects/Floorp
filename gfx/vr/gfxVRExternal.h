/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_EXTERNAL_H
#define GFX_VR_EXTERNAL_H

#include "nsTArray.h"
#include "nsIScreen.h"
#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/EnumeratedArray.h"

#include "gfxVR.h"
#include "VRDisplayHost.h"

#if defined(XP_MACOSX)
class MacIOSurface;
#endif
namespace mozilla {
namespace gfx {
class VRThread;

namespace impl {

class VRDisplayExternal : public VRDisplayHost
{
public:
  void ZeroSensor() override;
  static int sPushIndex;

protected:
  VRHMDSensorState GetSensorState() override;
  void StartPresentation() override;
  void StopPresentation() override;

  bool SubmitFrame(const layers::SurfaceDescriptor& aTexture,
                   uint64_t aFrameId,
                   const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect) override;

public:
  explicit VRDisplayExternal(const VRDisplayState& aDisplayState);
  void Refresh();
protected:
  virtual ~VRDisplayExternal();
  void Destroy();

private:
  bool PopulateLayerTexture(const layers::SurfaceDescriptor& aTexture,
                            VRLayerTextureType* aTextureType,
                            VRLayerTextureHandle* aTextureHandle);

  VRTelemetry mTelemetry;
  bool mIsPresenting;
  VRHMDSensorState mLastSensorState;
};

class VRControllerExternal : public VRControllerHost
{
public:
  explicit VRControllerExternal(dom::GamepadHand aHand, uint32_t aDisplayID, uint32_t aNumButtons,
                              uint32_t aNumTriggers, uint32_t aNumAxes,
                              const nsCString& aId);

protected:
  virtual ~VRControllerExternal();
};

} // namespace impl

class VRSystemManagerExternal : public VRSystemManager
{
public:
  static already_AddRefed<VRSystemManagerExternal> Create(VRExternalShmem* aAPIShmem = nullptr);

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
  virtual void VibrateHaptic(uint32_t aControllerIdx,
                             uint32_t aHapticIndex,
                             double aIntensity,
                             double aDuration,
                             const VRManagerPromise& aPromise) override;
  virtual void StopVibrateHaptic(uint32_t aControllerIdx) override;
  bool PullState(VRDisplayState* aDisplayState, VRHMDSensorState* aSensorState = nullptr);
  void PushState(VRBrowserState* aBrowserState, const bool aNotifyCond = false);

protected:
  explicit VRSystemManagerExternal(VRExternalShmem* aAPIShmem = nullptr);
  virtual ~VRSystemManagerExternal();

private:
  // there can only be one
  RefPtr<impl::VRDisplayExternal> mDisplay;
  nsTArray<RefPtr<impl::VRControllerExternal>> mExternalController;
#if defined(XP_MACOSX)
  int mShmemFD;
#elif defined(XP_WIN)
  HANDLE mShmemFile;
#elif defined(MOZ_WIDGET_ANDROID)
  bool mDoShutdown;
  bool mExternalStructFailed;
#endif

  volatile VRExternalShmem* mExternalShmem;
#if !defined(MOZ_WIDGET_ANDROID)
  bool mSameProcess;
#endif

  void OpenShmem();
  void CloseShmem();
  void CheckForShutdown();
};

} // namespace gfx
} // namespace mozilla


#endif /* GFX_VR_EXTERNAL_H */
