/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_VR_VRMANAGERCHILD_H
#define MOZILLA_GFX_VR_VRMANAGERCHILD_H

#include "mozilla/Attributes.h"
#include "mozilla/dom/WindowBinding.h"  // For FrameRequestCallback
#include "mozilla/gfx/PVRManagerChild.h"
#include "mozilla/ipc/SharedMemory.h"  // for SharedMemory, etc
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/LayersTypes.h"        // for LayersBackend
#include "mozilla/layers/TextureForwarder.h"

namespace mozilla {
namespace dom {
class Promise;
class GamepadManager;
class Navigator;
class VRDisplay;
}  // namespace dom
namespace layers {
class SyncObjectClient;
}
namespace gfx {
class VRLayerChild;
class VRDisplayClient;

class VRManagerEventObserver {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
  virtual void NotifyVRDisplayMounted(uint32_t aDisplayID) = 0;
  virtual void NotifyVRDisplayUnmounted(uint32_t aDisplayID) = 0;
  virtual void NotifyVRDisplayConnect(uint32_t aDisplayID) = 0;
  virtual void NotifyVRDisplayDisconnect(uint32_t aDisplayID) = 0;
  virtual void NotifyVRDisplayPresentChange(uint32_t aDisplayID) = 0;
  virtual void NotifyPresentationGenerationChanged(uint32_t aDisplayID) = 0;
  virtual bool GetStopActivityStatus() const = 0;
  virtual void NotifyEnumerationCompleted() = 0;

 protected:
  virtual ~VRManagerEventObserver() = default;
};

class VRManagerChild : public PVRManagerChild {
  friend class PVRManagerChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRManagerChild);

  static VRManagerChild* Get();

  // Indicate that an observer wants to receive VR events.
  void AddListener(VRManagerEventObserver* aObserver);
  // Indicate that an observer should no longer receive VR events.
  void RemoveListener(VRManagerEventObserver* aObserver);
  void StartActivity();
  void StopActivity();

  void GetVRDisplays(nsTArray<RefPtr<VRDisplayClient>>& aDisplays);
  bool RefreshVRDisplaysWithCallback(uint64_t aWindowId);
  bool EnumerateVRDisplays();
  void AddPromise(const uint32_t& aID, dom::Promise* aPromise);

  static void InitSameProcess();
  static void InitWithGPUProcess(Endpoint<PVRManagerChild>&& aEndpoint);
  static bool InitForContent(Endpoint<PVRManagerChild>&& aEndpoint);
  static bool ReinitForContent(Endpoint<PVRManagerChild>&& aEndpoint);
  static void ShutDown();

  static bool IsCreated();

  PVRLayerChild* CreateVRLayer(uint32_t aDisplayID, nsIEventTarget* aTarget,
                               uint32_t aGroup);

  static void IdentifyTextureHost(
      const layers::TextureFactoryIdentifier& aIdentifier);
  layers::LayersBackend GetBackendType() const;
  layers::SyncObjectClient* GetSyncObject() { return mSyncObject; }

  nsresult ScheduleFrameRequestCallback(
      mozilla::dom::FrameRequestCallback& aCallback, int32_t* aHandle);
  void CancelFrameRequestCallback(int32_t aHandle);
  MOZ_CAN_RUN_SCRIPT
  void RunFrameRequestCallbacks();
  void NotifyPresentationGenerationChanged(uint32_t aDisplayID);

  MOZ_CAN_RUN_SCRIPT
  void UpdateDisplayInfo(nsTArray<VRDisplayInfo>& aDisplayUpdates);
  void FireDOMVRDisplayMountedEvent(uint32_t aDisplayID);
  void FireDOMVRDisplayUnmountedEvent(uint32_t aDisplayID);
  void FireDOMVRDisplayConnectEvent(uint32_t aDisplayID);
  void FireDOMVRDisplayDisconnectEvent(uint32_t aDisplayID);
  void FireDOMVRDisplayPresentChangeEvent(uint32_t aDisplayID);
  void FireDOMVRDisplayConnectEventsForLoad(VRManagerEventObserver* aObserver);

  virtual void HandleFatalError(const char* aMsg) const override;

  void RunPuppet(const nsTArray<uint64_t>& aBuffer, dom::Promise* aPromise,
                 ErrorResult& aRv);
  void ResetPuppet(dom::Promise* aPromise, ErrorResult& aRv);

 protected:
  explicit VRManagerChild();
  ~VRManagerChild();
  void Destroy();
  static void DeferredDestroy(RefPtr<VRManagerChild> aVRManagerChild);

  PVRLayerChild* AllocPVRLayerChild(const uint32_t& aDisplayID,
                                    const uint32_t& aGroup);
  bool DeallocPVRLayerChild(PVRLayerChild* actor);

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until we can mark ipdl-generated things as
  // MOZ_CAN_RUN_SCRIPT.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvUpdateDisplayInfo(
      nsTArray<VRDisplayInfo>&& aDisplayUpdates);
  mozilla::ipc::IPCResult RecvReplyGamepadVibrateHaptic(
      const uint32_t& aPromiseID);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvNotifyPuppetCommandBufferCompleted(bool aSuccess);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  mozilla::ipc::IPCResult RecvNotifyPuppetResetComplete();

  bool IsSameProcess() const { return OtherPid() == base::GetCurrentProcId(); }

 private:
  void FireDOMVRDisplayMountedEventInternal(uint32_t aDisplayID);
  void FireDOMVRDisplayUnmountedEventInternal(uint32_t aDisplayID);
  void FireDOMVRDisplayConnectEventInternal(uint32_t aDisplayID);
  void FireDOMVRDisplayDisconnectEventInternal(uint32_t aDisplayID);
  void FireDOMVRDisplayPresentChangeEventInternal(uint32_t aDisplayID);
  void FireDOMVRDisplayConnectEventsForLoadInternal(
      uint32_t aDisplayID, VRManagerEventObserver* aObserver);
  void NotifyPresentationGenerationChangedInternal(uint32_t aDisplayID);
  void NotifyEnumerationCompletedInternal();

  nsTArray<RefPtr<VRDisplayClient>> mDisplays;
  bool mDisplaysInitialized;
  nsTArray<uint64_t> mNavigatorCallbacks;

  MessageLoop* mMessageLoop;

  struct FrameRequest;

  nsTArray<FrameRequest> mFrameRequestCallbacks;
  /**
   * The current frame request callback handle
   */
  int32_t mFrameRequestCallbackCounter;
  mozilla::TimeStamp mStartTimeStamp;

  nsTArray<RefPtr<VRManagerEventObserver>> mListeners;
  bool mWaitingForEnumeration;

  layers::LayersBackend mBackend;
  RefPtr<layers::SyncObjectClient> mSyncObject;
  nsRefPtrHashtable<nsUint32HashKey, dom::Promise> mGamepadPromiseList;
  RefPtr<dom::Promise> mRunPuppetPromise;
  nsTArray<RefPtr<dom::Promise>> mResetPuppetPromises;

  DISALLOW_COPY_AND_ASSIGN(VRManagerChild);
};

}  // namespace gfx
}  // namespace mozilla

#endif  // MOZILLA_GFX_VR_VRMANAGERCHILD_H
