/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_MANAGER_H
#define GFX_VR_MANAGER_H

#include "nsIObserver.h"
#include "nsTArray.h"
#include "mozilla/Atomics.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/TimeStamp.h"
#include "gfxVR.h"

class nsITimer;
namespace mozilla {
namespace gfx {
class VRLayerParent;
class VRManagerParent;
class VRServiceHost;
class VRThread;
class VRShMem;

enum class VRManagerState : uint32_t {
  Disabled,  // All VRManager activity is stopped
  Idle,  // No VR hardware has been activated, but background tasks are running
  Enumeration,  // Waiting for enumeration and startup of VR hardware
  Active        // VR hardware is active
};

class VRManager : nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void ManagerInit();
  static VRManager* Get();

  void AddVRManagerParent(VRManagerParent* aVRManagerParent);
  void RemoveVRManagerParent(VRManagerParent* aVRManagerParent);

  void NotifyVsync(const TimeStamp& aVsyncTimestamp);

  void RefreshVRDisplays(bool aMustDispatch = false);
  void StopAllHaptics();

  void VibrateHaptic(uint32_t aControllerIdx, uint32_t aHapticIndex,
                     double aIntensity, double aDuration,
                     const VRManagerPromise& aPromise);
  void StopVibrateHaptic(uint32_t aControllerIdx);
  void NotifyVibrateHapticCompleted(const VRManagerPromise& aPromise);
  void StartVRNavigation(const uint32_t& aDisplayID);
  void StopVRNavigation(const uint32_t& aDisplayID,
                        const TimeDuration& aTimeout);
  void Shutdown();
#if !defined(MOZ_WIDGET_ANDROID)
  bool RunPuppet(const nsTArray<uint64_t>& aBuffer,
                 VRManagerParent* aManagerParent);
  void ResetPuppet(VRManagerParent* aManagerParent);
#endif
  void AddLayer(VRLayerParent* aLayer);
  void RemoveLayer(VRLayerParent* aLayer);
  void SetGroupMask(uint32_t aGroupMask);
  void SubmitFrame(VRLayerParent* aLayer,
                   const layers::SurfaceDescriptor& aTexture, uint64_t aFrameId,
                   const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect);
  bool IsPresenting();

 private:
  VRManager();
  virtual ~VRManager();
  void Destroy();
  void StartTasks();
  void StopTasks();
  static void TaskTimerCallback(nsITimer* aTimer, void* aClosure);
  void RunTasks();
  void Run1msTasks(double aDeltaTime);
  void Run10msTasks();
  void Run100msTasks();
  uint32_t GetOptimalTaskInterval();
  void PullState(const std::function<bool()>& aWaitCondition = nullptr);
  void PushState(const bool aNotifyCond = false);
  static uint32_t AllocateDisplayID();

  void DispatchVRDisplayInfoUpdate();
  void UpdateRequestedDevices();
  void EnumerateVRDisplays();
  void CheckForInactiveTimeout();
#if !defined(MOZ_WIDGET_ANDROID)
  void CheckForPuppetCompletion();
#endif
  void CheckForShutdown();
  void CheckWatchDog();
  void ExpireNavigationTransition();
  void OpenShmem();
  void CloseShmem();
  void UpdateHaptics(double aDeltaTime);
  void ClearHapticSlot(size_t aSlot);
  void StartFrame();
  void ShutdownSubmitThread();
  void StartPresentation();
  void StopPresentation();
  void CancelCurrentSubmitTask();

  void SubmitFrameInternal(const layers::SurfaceDescriptor& aTexture,
                           uint64_t aFrameId, const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect);
  bool SubmitFrame(const layers::SurfaceDescriptor& aTexture, uint64_t aFrameId,
                   const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect);

  Atomic<VRManagerState> mState;
  typedef nsTHashtable<nsRefPtrHashKey<VRManagerParent>> VRManagerParentSet;
  VRManagerParentSet mVRManagerParents;
#if !defined(MOZ_WIDGET_ANDROID)
  VRManagerParentSet mManagerParentsWaitingForPuppetReset;
  RefPtr<VRManagerParent> mManagerParentRunningPuppet;
#endif
  // Weak reference to mLayers entries are cleared in
  // VRLayerParent destructor
  nsTArray<VRLayerParent*> mLayers;

  TimeStamp mLastDisplayEnumerationTime;
  TimeStamp mLastActiveTime;
  TimeStamp mLastTickTime;
  TimeStamp mEarliestRestartTime;
  TimeStamp mVRNavigationTransitionEnd;
  TimeStamp mLastFrameStart[kVRMaxLatencyFrames];
  double mAccumulator100ms;
  bool mVRDisplaysRequested;
  bool mVRDisplaysRequestedNonFocus;
  bool mVRControllersRequested;
  bool mFrameStarted;
  uint32_t mTaskInterval;
  RefPtr<nsITimer> mTaskTimer;
  mozilla::Monitor mCurrentSubmitTaskMonitor;
  RefPtr<CancelableRunnable> mCurrentSubmitTask;
  uint64_t mLastSubmittedFrameId;
  uint64_t mLastStartedFrame;
  bool mEnumerationCompleted;
  bool mAppPaused;

  // Note: mShmem doesn't support RefPtr; thus, do not share this private
  // pointer so that its lifetime can still be controlled by VRManager
  VRShMem* mShmem;
  bool mVRProcessEnabled;

#if !defined(MOZ_WIDGET_ANDROID)
  RefPtr<VRServiceHost> mServiceHost;
#endif

  static Atomic<uint32_t> sDisplayBase;
  RefPtr<VRThread> mSubmitThread;
  VRTelemetry mTelemetry;
  nsTArray<UniquePtr<VRManagerPromise>> mHapticPromises;
  // Duration of haptic pulse time remaining (milliseconds)
  double mHapticPulseRemaining[kVRHapticsMaxCount];

  VRDisplayInfo mDisplayInfo;
  VRDisplayInfo mLastUpdateDisplayInfo;
  VRBrowserState mBrowserState;
  VRHMDSensorState mLastSensorState;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_MANAGER_H
