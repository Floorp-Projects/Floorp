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
#include "mozilla/UniquePtr.h"

#include "gfxVR.h"
#include "VRDisplayHost.h"

#if defined(XP_MACOSX)
class MacIOSurface;
#endif
namespace mozilla {
namespace gfx {
namespace impl {

class VRDisplayExternal : public VRDisplayHost {
 public:
  void ZeroSensor() override;

 protected:
  VRHMDSensorState& GetSensorState() override;
  void StartPresentation() override;
  void StopPresentation() override;
  void StartVRNavigation() override;
  void StopVRNavigation(const TimeDuration& aTimeout) override;

  bool SubmitFrame(const layers::SurfaceDescriptor& aTexture, uint64_t aFrameId,
                   const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect) override;

 public:
  explicit VRDisplayExternal(const VRDisplayState& aDisplayState);
  void Refresh();
  const VRControllerState& GetLastControllerState(uint32_t aStateIndex) const;
  void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                     double aIntensity, double aDuration,
                     const VRManagerPromise& aPromise);
  void StopVibrateHaptic(uint32_t aControllerIdx);
  void StopAllHaptics();
  void Run1msTasks(double aDeltaTime) override;
  void Run10msTasks() override;

 protected:
  virtual ~VRDisplayExternal();
  void Destroy();

 private:
  bool PopulateLayerTexture(const layers::SurfaceDescriptor& aTexture,
                            VRLayerTextureType* aTextureType,
                            VRLayerTextureHandle* aTextureHandle);
  void PushState(bool aNotifyCond = false);
#if defined(MOZ_WIDGET_ANDROID)
  bool PullState(const std::function<bool()>& aWaitCondition = nullptr);
#else
  bool PullState();
#endif
  void ClearHapticSlot(size_t aSlot);
  void ExpireNavigationTransition();
  void UpdateHaptics(double aDeltaTime);
  nsTArray<UniquePtr<VRManagerPromise>> mHapticPromises;
  // Duration of haptic pulse time remaining (milliseconds)
  double mHapticPulseRemaining[kVRHapticsMaxCount];
  VRTelemetry mTelemetry;
  TimeStamp mVRNavigationTransitionEnd;
  VRBrowserState mBrowserState;
  VRHMDSensorState mLastSensorState;
};

}  // namespace impl

class VRSystemManagerExternal : public VRSystemManager {
 public:
  static already_AddRefed<VRSystemManagerExternal> Create(
      VRExternalShmem* aAPIShmem = nullptr);

  void Destroy() override;
  void Shutdown() override;
  void Run100msTasks() override;
  void Enumerate() override;
  bool ShouldInhibitEnumeration() override;
  void GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult) override;
  bool GetIsPresenting() override;
  void HandleInput() override;
  void GetControllers(
      nsTArray<RefPtr<VRControllerHost>>& aControllerResult) override;
  void ScanForControllers() override;
  void RemoveControllers() override;
  void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                     double aIntensity, double aDuration,
                     const VRManagerPromise& aPromise) override;
  void StopVibrateHaptic(uint32_t aControllerIdx) override;
#if defined(MOZ_WIDGET_ANDROID)
  bool PullState(VRDisplayState* aDisplayState,
                 VRHMDSensorState* aSensorState = nullptr,
                 VRControllerState* aControllerState = nullptr,
                 const std::function<bool()>& aWaitCondition = nullptr);
#else
  bool PullState(VRDisplayState* aDisplayState,
                 VRHMDSensorState* aSensorState = nullptr,
                 VRControllerState* aControllerState = nullptr);
#endif
  void PushState(VRBrowserState* aBrowserState, const bool aNotifyCond = false);

 protected:
  explicit VRSystemManagerExternal(VRExternalShmem* aAPIShmem = nullptr);
  virtual ~VRSystemManagerExternal();

 private:
  // there can only be one
  RefPtr<impl::VRDisplayExternal> mDisplay;
#if defined(XP_MACOSX)
  int mShmemFD;
#elif defined(XP_WIN)
  base::ProcessHandle mShmemFile;
#elif defined(MOZ_WIDGET_ANDROID)

  bool mExternalStructFailed;
  bool mEnumerationCompleted;
#endif
  bool mDoShutdown;

  volatile VRExternalShmem* mExternalShmem;

#if defined(XP_WIN)
  HANDLE mMutex;
#endif
#if !defined(MOZ_WIDGET_ANDROID)
  bool mSameProcess;
#endif
  TimeStamp mEarliestRestartTime;

  void OpenShmem();
  void CloseShmem();
  void CheckForShutdown();
};

}  // namespace gfx
}  // namespace mozilla

#endif /* GFX_VR_EXTERNAL_H */
