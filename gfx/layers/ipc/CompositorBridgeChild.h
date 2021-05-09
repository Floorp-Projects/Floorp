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
#include "mozilla/layers/PaintThread.h"       // for PaintThread
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
class ClientLayerManager;
class CompositorBridgeParent;
class CompositorManagerChild;
class CompositorOptions;
class LayerManager;
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

  void InitForWidget(uint64_t aProcessToken, LayerManager* aLayerManager,
                     uint32_t aNamespace);

  void Destroy();

  /**
   * Lookup the FrameMetrics shared by the compositor process with the
   * associated ScrollableLayerGuid::ViewID. The returned FrameMetrics is used
   * in progressive paint calculations.
   */
  bool LookupCompositorFrameMetrics(const ScrollableLayerGuid::ViewID aId,
                                    FrameMetrics&);

  static CompositorBridgeChild* Get();

  static bool ChildProcessHasCompositorBridge();

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
      const SurfaceDescriptor& aSharedData, const ReadLockDescriptor& aReadLock,
      const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
      const LayersId& aId, const uint64_t& aSerial,
      const wr::MaybeExternalImageId& aExternalImageId);

  bool DeallocPTextureChild(PTextureChild* actor);

  mozilla::ipc::IPCResult RecvParentAsyncMessages(
      nsTArray<AsyncParentMessageData>&& aMessages);
  PTextureChild* CreateTexture(const SurfaceDescriptor& aSharedData,
                               const ReadLockDescriptor& aReadLock,
                               LayersBackend aLayersBackend,
                               TextureFlags aFlags, uint64_t aSerial,
                               wr::MaybeExternalImageId& aExternalImageId,
                               nsISerialEventTarget* aTarget) override;

  already_AddRefed<CanvasChild> GetCanvasChild() final;

  void EndCanvasTransaction();

  RefPtr<webgpu::WebGPUChild> GetWebGPUChild();

  /**
   * Request that the parent tell us when graphics are ready on GPU.
   * When we get that message, we bounce it to the BrowserParent via
   * the BrowserChild
   * @param browserChild The object to bounce the note to.  Non-NULL.
   */
  void RequestNotifyAfterRemotePaint(BrowserChild* aBrowserChild);

  void CancelNotifyAfterRemotePaint(BrowserChild* aBrowserChild);

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
  bool SendNotifyChildCreated(const LayersId& id, CompositorOptions* aOptions);
  bool SendAdoptChild(const LayersId& id);
  bool SendMakeSnapshot(const SurfaceDescriptor& inSnapshot,
                        const gfx::IntRect& dirtyRect);
  bool SendFlushRendering();
  bool SendGetTileSize(int32_t* tileWidth, int32_t* tileHeight);
  bool SendStartFrameTimeRecording(const int32_t& bufferSize,
                                   uint32_t* startIndex);
  bool SendStopFrameTimeRecording(const uint32_t& startIndex,
                                  nsTArray<float>* intervals);
  bool SendNotifyRegionInvalidated(const nsIntRegion& region);
  bool SendRequestNotifyAfterRemotePaint();
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

  TextureClientPool* GetTexturePool(KnowsCompositor* aAllocator,
                                    gfx::SurfaceFormat aFormat,
                                    TextureFlags aFlags);
  void ClearTexturePool();

  FixedSizeSmallShmemSectionAllocator* GetTileLockAllocator() override;

  void HandleMemoryPressure();

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

  void WillEndTransaction();

  PWebRenderBridgeChild* AllocPWebRenderBridgeChild(
      const wr::PipelineId& aPipelineId, const LayoutDeviceIntSize&,
      const WindowKind&);
  bool DeallocPWebRenderBridgeChild(PWebRenderBridgeChild* aActor);

  webgpu::PWebGPUChild* AllocPWebGPUChild();
  bool DeallocPWebGPUChild(webgpu::PWebGPUChild* aActor);

  wr::MaybeExternalImageId GetNextExternalImageId() override;

  wr::PipelineId GetNextPipelineId();

  // Must only be called from the main thread. Ensures that any paints from
  // previous frames have been flushed. The main thread blocks until the
  // operation completes.
  void FlushAsyncPaints();

  // Must only be called from the main thread. Notifies the CompositorBridge
  // that the paint thread is going to begin painting asynchronously.
  void NotifyBeginAsyncPaint(PaintTask* aTask);

  // Must only be called from the paint thread. Notifies the CompositorBridge
  // that the paint thread has finished an asynchronous paint request.
  bool NotifyFinishedAsyncWorkerPaint(PaintTask* aTask);

  // Must only be called from the main thread. Notifies the CompositorBridge
  // that all work has been submitted to the paint thread or paint worker
  // threads, and returns whether all paints are completed. If this returns
  // true, then an AsyncEndLayerTransaction must be queued, otherwise once
  // NotifyFinishedAsyncWorkerPaint returns true, an AsyncEndLayerTransaction
  // must be executed.
  bool NotifyBeginAsyncEndLayerTransaction(SyncObjectClient* aSyncObject);

  // Must only be called from the paint thread. Notifies the CompositorBridge
  // that the paint thread has finished all async paints and and may do the
  // requested texture sync and resume sending messages.
  void NotifyFinishedAsyncEndLayerTransaction();

  // Must only be called from the main thread. Notifies the CompoistorBridge
  // that a transaction is about to be sent, and if the paint thread is
  // currently painting, to begin delaying IPC messages.
  void PostponeMessagesIfAsyncPainting();

 private:
  // Private destructor, to discourage deletion outside of Release():
  virtual ~CompositorBridgeChild();

  // Must only be called from the paint thread. If the main thread is delaying
  // IPC messages, this forwards all such delayed IPC messages to the I/O thread
  // and resumes IPC.
  void ResumeIPCAfterAsyncPaint();

  void PrepareFinalDestroy();
  void AfterDestroy();

  PLayerTransactionChild* AllocPLayerTransactionChild(
      const nsTArray<LayersBackend>& aBackendHints, const LayersId& aId);

  bool DeallocPLayerTransactionChild(PLayerTransactionChild* aChild);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvSharedCompositorFrameMetrics(
      const mozilla::ipc::SharedMemoryBasic::Handle& metrics,
      const CrossProcessMutexHandle& handle, const LayersId& aLayersId,
      const uint32_t& aAPZCId);

  mozilla::ipc::IPCResult RecvReleaseSharedCompositorFrameMetrics(
      const ViewID& aId, const uint32_t& aAPZCId);

  mozilla::ipc::IPCResult RecvRemotePaintIsReady();

  mozilla::ipc::IPCResult RecvObserveLayersUpdate(
      const LayersId& aLayersId, const LayersObserverEpoch& aEpoch,
      const bool& aActive);

  mozilla::ipc::IPCResult RecvCompositorOptionsChanged(
      const LayersId& aLayersId, const CompositorOptions& aNewOptions);

  uint64_t GetNextResourceId();

  void ClearSharedFrameMetricsData(LayersId aLayersId);

  // Class used to store the shared FrameMetrics, mutex, and APZCId  in a hash
  // table
  class SharedFrameMetricsData final {
   public:
    SharedFrameMetricsData(
        const mozilla::ipc::SharedMemoryBasic::Handle& metrics,
        const CrossProcessMutexHandle& handle, const LayersId& aLayersId,
        const uint32_t& aAPZCId);

    ~SharedFrameMetricsData();

    void CopyFrameMetrics(FrameMetrics* aFrame);
    ScrollableLayerGuid::ViewID GetViewID();
    LayersId GetLayersId() const;
    uint32_t GetAPZCId();

   private:
    // Pointer to the class that allows access to the shared memory that
    // contains the shared FrameMetrics
    RefPtr<mozilla::ipc::SharedMemoryBasic> mBuffer;
    CrossProcessMutex* mMutex;
    LayersId mLayersId;
    // Unique ID of the APZC that is sharing the FrameMetrics
    uint32_t mAPZCId;
  };

  RefPtr<CompositorManagerChild> mCompositorManager;

  RefPtr<LayerManager> mLayerManager;

  uint32_t mIdNamespace;
  uint32_t mResourceId;

  // When not multi-process, hold a reference to the CompositorBridgeParent to
  // keep it alive. This reference should be null in multi-process.
  RefPtr<CompositorBridgeParent> mCompositorBridgeParent;

  // The ViewID of the FrameMetrics is used as the key for this hash table.
  // While this should be safe to use since the ViewID is unique
  nsClassHashtable<nsUint64HashKey, SharedFrameMetricsData> mFrameMetricsTable;

  // Weakly hold the BrowserChild that made a request to be alerted when
  // the transaction has been received.
  nsWeakPtr mWeakBrowserChild;  // type is BrowserChild

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

  // Contains the number of outstanding asynchronous paints tied to a
  // PLayerTransaction on this bridge. This is R/W on both the main and paint
  // threads, and must be accessed within the paint lock.
  size_t mOutstandingAsyncPaints;

  // Whether we are waiting for an async paint end transaction
  bool mOutstandingAsyncEndTransaction;
  RefPtr<SyncObjectClient> mOutstandingAsyncSyncObject;

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
