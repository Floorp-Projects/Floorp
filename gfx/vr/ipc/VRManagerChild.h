/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_VR_VRMANAGERCHILD_H
#define MOZILLA_GFX_VR_VRMANAGERCHILD_H

#include "mozilla/gfx/PVRManagerChild.h"
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend
#include "mozilla/layers/TextureForwarder.h"

namespace mozilla {
namespace dom {
class GamepadManager;
class Navigator;
class VRDisplay;
class VREventObserver;
} // namespace dom
namespace layers {
class PCompositableChild;
class TextureClient;
}
namespace gfx {
class VRLayerChild;
class VRDisplayClient;

class VRManagerChild : public PVRManagerChild
                     , public layers::TextureForwarder
                     , public layers::KnowsCompositor
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRManagerChild, override);

  TextureForwarder* GetTextureForwarder() override { return this; }
  LayersIPCActor* GetLayersIPCActor() override { return this; }

  static VRManagerChild* Get();

  // Indicate that an observer wants to receive VR events.
  void AddListener(dom::VREventObserver* aObserver);
  // Indicate that an observer should no longer receive VR events.
  void RemoveListener(dom::VREventObserver* aObserver);

  int GetInputFrameID();
  bool GetVRDisplays(nsTArray<RefPtr<VRDisplayClient> >& aDisplays);
  bool RefreshVRDisplaysWithCallback(uint64_t aWindowId);

  static void InitSameProcess();
  static void InitWithGPUProcess(Endpoint<PVRManagerChild>&& aEndpoint);
  static bool InitForContent(Endpoint<PVRManagerChild>&& aEndpoint);
  static bool ReinitForContent(Endpoint<PVRManagerChild>&& aEndpoint);
  static void ShutDown();

  static bool IsCreated();

  virtual PTextureChild* CreateTexture(const SurfaceDescriptor& aSharedData,
                                       layers::LayersBackend aLayersBackend,
                                       TextureFlags aFlags,
                                       uint64_t aSerial) override;
  virtual void CancelWaitForRecycle(uint64_t aTextureId) override;

  PVRLayerChild* CreateVRLayer(uint32_t aDisplayID, const Rect& aLeftEyeRect, const Rect& aRightEyeRect);

  static void IdentifyTextureHost(const layers::TextureFactoryIdentifier& aIdentifier);
  layers::LayersBackend GetBackendType() const;
  layers::SyncObject* GetSyncObject() { return mSyncObject; }

  virtual MessageLoop* GetMessageLoop() const override { return mMessageLoop; }
  virtual base::ProcessId GetParentPid() const override { return OtherPid(); }

  nsresult ScheduleFrameRequestCallback(mozilla::dom::FrameRequestCallback& aCallback,
    int32_t *aHandle);
  void CancelFrameRequestCallback(int32_t aHandle);
  void RunFrameRequestCallbacks();
  // GamepadManager has to be set by the content side to make sure we are using
  // the same singleton GamepadManager from the same process.
  void SetGamepadManager(dom::GamepadManager* aGamepadManager);

  void UpdateDisplayInfo(nsTArray<VRDisplayInfo>& aDisplayUpdates);
  void FireDOMVRDisplayConnectEvent();
  void FireDOMVRDisplayDisconnectEvent();
  void FireDOMVRDisplayPresentChangeEvent();

  virtual void HandleFatalError(const char* aName, const char* aMsg) const override;

protected:
  explicit VRManagerChild();
  ~VRManagerChild();
  void Destroy();
  static void DeferredDestroy(RefPtr<VRManagerChild> aVRManagerChild);

  virtual PTextureChild* AllocPTextureChild(const SurfaceDescriptor& aSharedData,
                                            const layers::LayersBackend& aLayersBackend,
                                            const TextureFlags& aFlags,
                                            const uint64_t& aSerial) override;
  virtual bool DeallocPTextureChild(PTextureChild* actor) override;

  virtual PVRLayerChild* AllocPVRLayerChild(const uint32_t& aDisplayID,
                                            const float& aLeftEyeX,
                                            const float& aLeftEyeY,
                                            const float& aLeftEyeWidth,
                                            const float& aLeftEyeHeight,
                                            const float& aRightEyeX,
                                            const float& aRightEyeY,
                                            const float& aRightEyeWidth,
                                            const float& aRightEyeHeight) override;
  virtual bool DeallocPVRLayerChild(PVRLayerChild* actor) override;

  virtual mozilla::ipc::IPCResult RecvUpdateDisplayInfo(nsTArray<VRDisplayInfo>&& aDisplayUpdates) override;

  virtual mozilla::ipc::IPCResult RecvParentAsyncMessages(InfallibleTArray<AsyncParentMessageData>&& aMessages) override;

  virtual mozilla::ipc::IPCResult RecvNotifyVSync() override;
  virtual mozilla::ipc::IPCResult RecvNotifyVRVSync(const uint32_t& aDisplayID) override;
  virtual mozilla::ipc::IPCResult RecvGamepadUpdate(const GamepadChangeEvent& aGamepadEvent) override;

  // ShmemAllocator

  virtual bool AllocShmem(size_t aSize,
                          ipc::SharedMemory::SharedMemoryType aType,
                          ipc::Shmem* aShmem) override;

  virtual bool AllocUnsafeShmem(size_t aSize,
                                ipc::SharedMemory::SharedMemoryType aType,
                                ipc::Shmem* aShmem) override;

  virtual bool DeallocShmem(ipc::Shmem& aShmem) override;

  virtual bool IsSameProcess() const override
  {
    return OtherPid() == base::GetCurrentProcId();
  }

  friend class layers::CompositorBridgeChild;

private:

  void FireDOMVRDisplayConnectEventInternal();
  void FireDOMVRDisplayDisconnectEventInternal();
  void FireDOMVRDisplayPresentChangeEventInternal();
  /**
  * Notify id of Texture When host side end its use. Transaction id is used to
  * make sure if there is no newer usage.
  */
  void NotifyNotUsed(uint64_t aTextureId, uint64_t aFwdTransactionId);

  nsTArray<RefPtr<VRDisplayClient> > mDisplays;
  bool mDisplaysInitialized;
  nsTArray<uint64_t> mNavigatorCallbacks;
  dom::GamepadManager* mGamepadManager;

  int32_t mInputFrameID;

  MessageLoop* mMessageLoop;

  struct FrameRequest;

  nsTArray<FrameRequest> mFrameRequestCallbacks;
  /**
  * The current frame request callback handle
  */
  int32_t mFrameRequestCallbackCounter;
  mozilla::TimeStamp mStartTimeStamp;

  // Array of Weak pointers, instance is owned by nsGlobalWindow::mVREventObserver.
  nsTArray<dom::VREventObserver*> mListeners;

  /**
  * Hold TextureClients refs until end of their usages on host side.
  * It defer calling of TextureClient recycle callback.
  */
  nsDataHashtable<nsUint64HashKey, RefPtr<layers::TextureClient> > mTexturesWaitingRecycled;

  layers::LayersBackend mBackend;
  RefPtr<layers::SyncObject> mSyncObject;

  DISALLOW_COPY_AND_ASSIGN(VRManagerChild);
};

} // namespace mozilla
} // namespace gfx

#endif // MOZILLA_GFX_VR_VRMANAGERCHILD_H
