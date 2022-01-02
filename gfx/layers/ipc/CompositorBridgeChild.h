/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorBridgeChild_h
#define mozilla_layers_CompositorBridgeChild_h

#include "base/basictypes.h"     // for DISALLOW_EVIL_CONSTRUCTORS
#include "mozilla/Assertions.h"  // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"  // for override
#include "mozilla/Monitor.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/PCompositorBridgeChild.h"
#include "mozilla/layers/TextureForwarder.h"  // for TextureForwarder
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsClassHashtable.h"  // for nsClassHashtable
#include "nsCOMPtr.h"          // for nsCOMPtr
#include "nsHashKeys.h"        // for nsUint64HashKey
#include "nsISupportsImpl.h"   // for NS_INLINE_DECL_REFCOUNTING
#include "nsIWeakReferenceUtils.h"

#include <unordered_map>

namespace mozilla {

namespace dom {
class BrowserChild;
}  // namespace dom

namespace webgpu {
class PWebGPUChild;
class WebGPUChild;
}  // namespace webgpu

namespace widget {
class CompositorWidget;
}  // namespace widget

namespace layers {

using mozilla::dom::BrowserChild;

class IAPZCTreeManager;
class APZCTreeManagerChild;
class CanvasChild;
class CompositorBridgeParent;
class CompositorManagerChild;
class CompositorOptions;
class WebRenderLayerManager;
class TextureClient;
class TextureClientPool;
struct FrameMetrics;

class CompositorBridgeChild final : public PCompositorBridgeChild,
                                    public TextureForwarder {
  typedef nsTArray<AsyncParentMessageData> AsyncParentMessageArray;

  friend class PCompositorBridgeChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorBridgeChild, override);

  explicit CompositorBridgeChild(CompositorManagerChild* aManager);

  /**
   * Initialize the singleton compositor bridge for a content process.
   */
  void InitForContent(uint32_t aNamespace);

  void InitForWidget(uint64_t aProcessToken,
                     WebRenderLayerManager* aLayerManager, uint32_t aNamespace);

  void Destroy();

  static CompositorBridgeChild* Get();

  // Returns whether the compositor is in the GPU process (false if in the UI
  // process). This may only be called on the main thread.
  static bool CompositorIsInGPUProcess();

  mozilla::ipc::IPCResult RecvDidComposite(
      const LayersId& aId, const nsTArray<TransactionId>& aTransactionIds,
      const TimeStamp& aCompositeStart, const TimeStamp& aCompositeEnd);

  mozilla::ipc::IPCResult RecvNotifyFrameStats(
      nsTArray<FrameStats>&& aFrameStats);

  mozilla::ipc::IPCResult RecvInvalidateLayers(const LayersId& aLayersId);

  mozilla::ipc::IPCResult RecvNotifyJankedAnimations(
      const LayersId& aLayersId, nsTArray<uint64_t>&& aJankedAnimations);

  PTextureChild* AllocPTextureChild(
      const SurfaceDescriptor& aSharedData, ReadLockDescriptor& aReadLock,
      const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
      const LayersId& aId, const uint64_t& aSerial,
      const wr::MaybeExternalImageId& aExternalImageId);

  bool DeallocPTextureChild(PTextureChild* actor);

  mozilla::ipc::IPCResult RecvParentAsyncMessages(
      nsTArray<AsyncParentMessageData>&& aMessages);
  PTextureChild* CreateTexture(const SurfaceDescriptor& aSharedData,
                               ReadLockDescriptor&& aReadLock,
                               LayersBackend aLayersBackend,
                               TextureFlags aFlags, uint64_t aSerial,
                               wr::MaybeExternalImageId& aExternalImageId,
                               nsISerialEventTarget* aTarget) override;

  already_AddRefed<CanvasChild> GetCanvasChild() final;

  void EndCanvasTransaction();

  RefPtr<webgpu::WebGPUChild> GetWebGPUChild();

  // Beware that these methods don't override their super-class equivalent
  // (which are not virtual), they just overload them. All of these Send*
  // methods just add a sanity check (that it is not too late send a message)
  // and forward the call to the super-class's equivalent method. This means
  // that it is correct to call directly the super-class methods, but you won't
  // get the extra safety provided here.
  bool SendWillClose();
  bool SendPause();
  bool SendResume();
  bool SendResumeAsync();
  bool SendAdoptChild(const LayersId& id);
  bool SendFlushRendering(const wr::RenderReasons& aReasons);
  bool SendStartFrameTimeRecording(const int32_t& bufferSize,
                                   uint32_t* startIndex);
  bool SendStopFrameTimeRecording(const uint32_t& startIndex,
                                  nsTArray<float>* intervals);
  bool IsSameProcess() const override;

  bool IPCOpen() const override { return mCanSend; }

  static void ShutDown();

  void UpdateFwdTransactionId() { ++mFwdTransactionId; }
  uint64_t GetFwdTransactionId() { return mFwdTransactionId; }

  /**
   * Hold TextureClient ref until end of usage on host side if
   * TextureFlags::RECYCLE is set. Host side's usage is checked via
   * CompositableRef.
   */
  void HoldUntilCompositableRefReleasedIfNecessary(TextureClient* aClient);

  /**
   * Notify id of Texture When host side end its use. Transaction id is used to
   * make sure if there is no newer usage.
   */
  void NotifyNotUsed(uint64_t aTextureId, uint64_t aFwdTransactionId);

  void CancelWaitForNotifyNotUsed(uint64_t aTextureId) override;

  FixedSizeSmallShmemSectionAllocator* GetTileLockAllocator() override;

  nsISerialEventTarget* GetThread() const override { return mThread; }

  base::ProcessId GetParentPid() const override { return OtherPid(); }

  bool AllocUnsafeShmem(size_t aSize,
                        mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                        mozilla::ipc::Shmem* aShmem) override;
  bool AllocShmem(size_t aSize,
                  mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                  mozilla::ipc::Shmem* aShmem) override;
  bool DeallocShmem(mozilla::ipc::Shmem& aShmem) override;

  PCompositorWidgetChild* AllocPCompositorWidgetChild(
      const CompositorWidgetInitData& aInitData);
  bool DeallocPCompositorWidgetChild(PCompositorWidgetChild* aActor);

  PAPZCTreeManagerChild* AllocPAPZCTreeManagerChild(const LayersId& aLayersId);
  bool DeallocPAPZCTreeManagerChild(PAPZCTreeManagerChild* aActor);

  PAPZChild* AllocPAPZChild(const LayersId& aLayersId);
  bool DeallocPAPZChild(PAPZChild* aActor);

  PWebRenderBridgeChild* AllocPWebRenderBridgeChild(
      const wr::PipelineId& aPipelineId, const LayoutDeviceIntSize&,
      const WindowKind&);
  bool DeallocPWebRenderBridgeChild(PWebRenderBridgeChild* aActor);

  webgpu::PWebGPUChild* AllocPWebGPUChild();
  bool DeallocPWebGPUChild(webgpu::PWebGPUChild* aActor);

  wr::MaybeExternalImageId GetNextExternalImageId() override;

  wr::PipelineId GetNextPipelineId();

 private:
  // Private destructor, to discourage deletion outside of Release():
  virtual ~CompositorBridgeChild();

  // Must only be called from the paint thread. If the main thread is delaying
  // IPC messages, this forwards all such delayed IPC messages to the I/O thread
  // and resumes IPC.
  void ResumeIPCAfterAsyncPaint();

  void PrepareFinalDestroy();
  void AfterDestroy();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvObserveLayersUpdate(
      const LayersId& aLayersId, const LayersObserverEpoch& aEpoch,
      const bool& aActive);

  mozilla::ipc::IPCResult RecvCompositorOptionsChanged(
      const LayersId& aLayersId, const CompositorOptions& aNewOptions);

  uint64_t GetNextResourceId();

  RefPtr<CompositorManagerChild> mCompositorManager;

  RefPtr<WebRenderLayerManager> mLayerManager;

  uint32_t mIdNamespace;
  uint32_t mResourceId;

  // When not multi-process, hold a reference to the CompositorBridgeParent to
  // keep it alive. This reference should be null in multi-process.
  RefPtr<CompositorBridgeParent> mCompositorBridgeParent;

  DISALLOW_EVIL_CONSTRUCTORS(CompositorBridgeChild);

  // True until the beginning of the two-step shutdown sequence of this actor.
  bool mCanSend;

  // False until the actor is destroyed.
  bool mActorDestroyed;

  /**
   * Transaction id of ShadowLayerForwarder.
   * It is incrementaed by UpdateFwdTransactionId() in each BeginTransaction()
   * call.
   */
  uint64_t mFwdTransactionId;

  /**
   * Hold TextureClients refs until end of their usages on host side.
   * It defer calling of TextureClient recycle callback.
   */
  std::unordered_map<uint64_t, RefPtr<TextureClient>>
      mTexturesWaitingNotifyNotUsed;

  nsCOMPtr<nsISerialEventTarget> mThread;

  AutoTArray<RefPtr<TextureClientPool>, 2> mTexturePools;

  uint64_t mProcessToken;

  FixedSizeSmallShmemSectionAllocator* mSectionAllocator;

  // TextureClients that must be kept alive during async painting. This
  // is only accessed on the main thread.
  nsTArray<RefPtr<TextureClient>> mTextureClientsForAsyncPaint;

  // Off-Main-Thread Painting state. This covers access to the OMTP-related
  // state below.
  Monitor mPaintLock;

  // Contains the number of asynchronous paints that were queued since the
  // beginning of the last async transaction, and the time stamp of when
  // that was
  size_t mTotalAsyncPaints;
  TimeStamp mAsyncTransactionBegin;

  // True if this CompositorBridge is currently delaying its messages until the
  // paint thread completes. This is R/W on both the main and paint threads, and
  // must be accessed within the paint lock.
  bool mIsDelayingForAsyncPaints;

  uintptr_t mSlowFlushCount;
  uintptr_t mTotalFlushCount;

  RefPtr<CanvasChild> mCanvasChild;

  RefPtr<webgpu::WebGPUChild> mWebGPUChild;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CompositorBrigedChild_h
