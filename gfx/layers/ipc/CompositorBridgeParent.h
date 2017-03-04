/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorBridgeParent_h
#define mozilla_layers_CompositorBridgeParent_h

// Enable this pref to turn on compositor performance warning.
// This will print warnings if the compositor isn't meeting
// its responsiveness objectives:
//    1) Compose a frame within 15ms of receiving a ScheduleCompositeCall
//    2) Unless a frame was composited within the throttle threshold in
//       which the deadline will be 15ms + throttle threshold
//#define COMPOSITOR_PERFORMANCE_WARNING

#include <stdint.h>                     // for uint64_t
#include "Layers.h"                     // for Layer
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for override
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"            // for Monitor
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/layers/CompositorController.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/CompositorVsyncSchedulerOwner.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/ISurfaceAllocator.h" // for ShmemAllocator
#include "mozilla/layers/LayersMessages.h"  // for TargetConfig
#include "mozilla/layers/MetricsSharingController.h"
#include "mozilla/layers/PCompositorBridgeParent.h"
#include "mozilla/layers/APZTestData.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/widget/CompositorWidget.h"
#include "nsISupportsImpl.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

class MessageLoop;
class nsIWidget;

namespace mozilla {

class CancelableRunnable;

namespace gfx {
class DrawTarget;
class GPUProcessManager;
class GPUParent;
} // namespace gfx

namespace ipc {
class Shmem;
} // namespace ipc

namespace layers {

class APZCTreeManager;
class APZCTreeManagerParent;
class AsyncCompositionManager;
class Compositor;
class CompositorBridgeParent;
class CompositorVsyncScheduler;
class HostLayerManager;
class LayerTransactionParent;
class PAPZParent;
class CrossProcessCompositorBridgeParent;
class CompositorThreadHolder;
class InProcessCompositorSession;
class WebRenderBridgeParent;

struct ScopedLayerTreeRegistration
{
  ScopedLayerTreeRegistration(APZCTreeManager* aApzctm,
                              uint64_t aLayersId,
                              Layer* aRoot,
                              GeckoContentController* aController);
  ~ScopedLayerTreeRegistration();

private:
  uint64_t mLayersId;
};

class CompositorBridgeParentBase : public PCompositorBridgeParent,
                                   public HostIPCAllocator,
                                   public ShmemAllocator,
                                   public MetricsSharingController
{
public:
  virtual void ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                   const TransactionInfo& aInfo,
                                   bool aHitTestUpdate) = 0;

  virtual AsyncCompositionManager* GetCompositionManager(LayerTransactionParent* aLayerTree) { return nullptr; }

  virtual void NotifyClearCachedResources(LayerTransactionParent* aLayerTree) { }

  virtual void ForceComposite(LayerTransactionParent* aLayerTree) { }
  virtual bool SetTestSampleTime(LayerTransactionParent* aLayerTree,
                                 const TimeStamp& aTime) { return true; }
  virtual void LeaveTestMode(LayerTransactionParent* aLayerTree) { }
  virtual void ApplyAsyncProperties(LayerTransactionParent* aLayerTree) = 0;
  virtual void FlushApzRepaints(const LayerTransactionParent* aLayerTree) = 0;
  virtual void GetAPZTestData(const LayerTransactionParent* aLayerTree,
                              APZTestData* aOutData) { }
  virtual void SetConfirmedTargetAPZC(const LayerTransactionParent* aLayerTree,
                                      const uint64_t& aInputBlockId,
                                      const nsTArray<ScrollableLayerGuid>& aTargets) = 0;
  virtual void UpdatePaintTime(LayerTransactionParent* aLayerTree, const TimeDuration& aPaintTime) {}

  virtual ShmemAllocator* AsShmemAllocator() override { return this; }

  virtual mozilla::ipc::IPCResult RecvSyncWithCompositor() override { return IPC_OK(); }

  virtual void ObserveLayerUpdate(uint64_t aLayersId, uint64_t aEpoch, bool aActive) = 0;

  virtual void NotifyDidCompositeToPipeline(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, TimeStamp& aCompositeStart, TimeStamp& aCompositeEnd) {}

  // HostIPCAllocator
  virtual base::ProcessId GetChildProcessId() override;
  virtual void NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId) override;
  virtual void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) override;

  // ShmemAllocator
  virtual bool AllocShmem(size_t aSize,
                          mozilla::ipc::SharedMemory::SharedMemoryType aType,
                          mozilla::ipc::Shmem* aShmem) override;
  virtual bool AllocUnsafeShmem(size_t aSize,
                                mozilla::ipc::SharedMemory::SharedMemoryType aType,
                                mozilla::ipc::Shmem* aShmem) override;
  virtual void DeallocShmem(mozilla::ipc::Shmem& aShmem) override;

  // MetricsSharingController
  NS_IMETHOD_(MozExternalRefCountType) AddRef() override { return HostIPCAllocator::AddRef(); }
  NS_IMETHOD_(MozExternalRefCountType) Release() override { return HostIPCAllocator::Release(); }
  base::ProcessId RemotePid() override;
  bool StartSharingMetrics(mozilla::ipc::SharedMemoryBasic::Handle aHandle,
                           CrossProcessMutexHandle aMutexHandle,
                           uint64_t aLayersId,
                           uint32_t aApzcId) override;
  bool StopSharingMetrics(FrameMetrics::ViewID aScrollId,
                          uint32_t aApzcId) override;
};

class CompositorBridgeParent final : public CompositorBridgeParentBase
                                   , public CompositorController
                                   , public CompositorVsyncSchedulerOwner
{
  friend class CompositorThreadHolder;
  friend class InProcessCompositorSession;
  friend class gfx::GPUProcessManager;
  friend class gfx::GPUParent;

public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef() override { return CompositorBridgeParentBase::AddRef(); }
  NS_IMETHOD_(MozExternalRefCountType) Release() override { return CompositorBridgeParentBase::Release(); }

  explicit CompositorBridgeParent(CSSToLayoutDeviceScale aScale,
                                  const TimeDuration& aVsyncRate,
                                  const CompositorOptions& aOptions,
                                  bool aUseExternalSurfaceSize,
                                  const gfx::IntSize& aSurfaceSize);

  // Must only be called by CompositorBridgeChild. After invoking this, the
  // IPC channel is active and RecvWillStop/ActorDestroy must be called to
  // free the compositor.
  void InitSameProcess(widget::CompositorWidget* aWidget,
                       const uint64_t& aLayerTreeId);

  // Must only be called by GPUParent. After invoking this, the IPC channel
  // is active and RecvWillStop/ActorDestroy must be called to free the
  // compositor.
  bool Bind(Endpoint<PCompositorBridgeParent>&& aEndpoint);

  virtual mozilla::ipc::IPCResult RecvInitialize(const uint64_t& aRootLayerTreeId) override;
  virtual mozilla::ipc::IPCResult RecvReset(nsTArray<LayersBackend>&& aBackendHints,
                         const uint64_t& aSeqNo,
                         bool* aResult,
                         TextureFactoryIdentifier* aOutIdentifier) override;
  virtual mozilla::ipc::IPCResult RecvGetFrameUniformity(FrameUniformityData* aOutData) override;
  virtual mozilla::ipc::IPCResult RecvWillClose() override;
  virtual mozilla::ipc::IPCResult RecvPause() override;
  virtual mozilla::ipc::IPCResult RecvResume() override;
  virtual mozilla::ipc::IPCResult RecvNotifyChildCreated(const uint64_t& child) override;
  virtual mozilla::ipc::IPCResult RecvNotifyChildRecreated(const uint64_t& child) override;
  virtual mozilla::ipc::IPCResult RecvAdoptChild(const uint64_t& child) override;
  virtual mozilla::ipc::IPCResult RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                const gfx::IntRect& aRect) override;
  virtual mozilla::ipc::IPCResult RecvFlushRendering() override;
  virtual mozilla::ipc::IPCResult RecvForcePresent() override;

  virtual mozilla::ipc::IPCResult RecvAcknowledgeCompositorUpdate(const uint64_t&, const uint64_t&) override {
    MOZ_ASSERT_UNREACHABLE("This message is only sent cross-process");
    return IPC_OK();
  }

  virtual mozilla::ipc::IPCResult RecvNotifyRegionInvalidated(const nsIntRegion& aRegion) override;
  virtual mozilla::ipc::IPCResult RecvStartFrameTimeRecording(const int32_t& aBufferSize, uint32_t* aOutStartIndex) override;
  virtual mozilla::ipc::IPCResult RecvStopFrameTimeRecording(const uint32_t& aStartIndex, InfallibleTArray<float>* intervals) override;

  // Unused for chrome <-> compositor communication (which this class does).
  // @see CrossProcessCompositorBridgeParent::RecvRequestNotifyAfterRemotePaint
  virtual mozilla::ipc::IPCResult RecvRequestNotifyAfterRemotePaint() override { return IPC_OK(); };

  virtual mozilla::ipc::IPCResult RecvClearApproximatelyVisibleRegions(const uint64_t& aLayersId,
                                                                       const uint32_t& aPresShellId) override;
  void ClearApproximatelyVisibleRegions(const uint64_t& aLayersId,
                                        const Maybe<uint32_t>& aPresShellId);
  virtual mozilla::ipc::IPCResult RecvNotifyApproximatelyVisibleRegion(const ScrollableLayerGuid& aGuid,
                                                                       const CSSIntRegion& aRegion) override;

  virtual mozilla::ipc::IPCResult RecvAllPluginsCaptured() override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

  virtual void ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                   const TransactionInfo& aInfo,
                                   bool aHitTestUpdate) override;
  virtual void ForceComposite(LayerTransactionParent* aLayerTree) override;
  virtual bool SetTestSampleTime(LayerTransactionParent* aLayerTree,
                                 const TimeStamp& aTime) override;
  virtual void LeaveTestMode(LayerTransactionParent* aLayerTree) override;
  virtual void ApplyAsyncProperties(LayerTransactionParent* aLayerTree)
               override;
  virtual void FlushApzRepaints(const LayerTransactionParent* aLayerTree) override;
  virtual void GetAPZTestData(const LayerTransactionParent* aLayerTree,
                              APZTestData* aOutData) override;
  virtual void SetConfirmedTargetAPZC(const LayerTransactionParent* aLayerTree,
                                      const uint64_t& aInputBlockId,
                                      const nsTArray<ScrollableLayerGuid>& aTargets) override;
  virtual AsyncCompositionManager* GetCompositionManager(LayerTransactionParent* aLayerTree) override { return mCompositionManager; }

  virtual PTextureParent* AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                              const LayersBackend& aLayersBackend,
                                              const TextureFlags& aFlags,
                                              const uint64_t& aId,
                                              const uint64_t& aSerial) override;
  virtual bool DeallocPTextureParent(PTextureParent* actor) override;

  virtual bool IsSameProcess() const override;


  PCompositorWidgetParent* AllocPCompositorWidgetParent(const CompositorWidgetInitData& aInitData) override;
  bool DeallocPCompositorWidgetParent(PCompositorWidgetParent* aActor) override;

  void ObserveLayerUpdate(uint64_t aLayersId, uint64_t aEpoch, bool aActive) override { }

  /**
   * Request that the compositor be recreated due to a shared device reset.
   * This must be called on the main thread, and blocks until a task posted
   * to the compositor thread has completed.
   *
   * Note that this posts a task directly, rather than using synchronous
   * IPDL, and waits on a monitor notification from the compositor thread.
   * We do this as a best-effort attempt to jump any IPDL messages that
   * have not yet been posted (and are sitting around in the IO pipe), to
   * minimize the amount of time the main thread is blocked.
   */
  bool ResetCompositor(const nsTArray<LayersBackend>& aBackendHints,
                       uint64_t aSeqNo,
                       TextureFactoryIdentifier* aOutIdentifier);

  /**
   * This forces the is-first-paint flag to true. This is intended to
   * be called by the widget code when it loses its viewport information
   * (or for whatever reason wants to refresh the viewport information).
   * The information refresh happens because the compositor will call
   * SetFirstPaintViewport on the next frame of composition.
   */
  mozilla::ipc::IPCResult RecvForceIsFirstPaint() override;

  static void SetShadowProperties(Layer* aLayer);

  void NotifyChildCreated(uint64_t aChild);

  void AsyncRender();

  // Can be called from any thread
  void ScheduleRenderOnCompositorThread() override;
  void SchedulePauseOnCompositorThread();
  void InvalidateOnCompositorThread();
  /**
   * Returns true if a surface was obtained and the resume succeeded; false
   * otherwise.
   */
  bool ScheduleResumeOnCompositorThread();
  bool ScheduleResumeOnCompositorThread(int width, int height);

  virtual void ScheduleComposition();
  void NotifyShadowTreeTransaction(uint64_t aId, bool aIsFirstPaint,
      bool aScheduleComposite, uint32_t aPaintSequenceNumber,
      bool aIsRepeatTransaction, bool aHitTestUpdate);

  void UpdatePaintTime(LayerTransactionParent* aLayerTree,
                       const TimeDuration& aPaintTime) override;

  /**
   * Check rotation info and schedule a rendering task if needed.
   * Only can be called from compositor thread.
   */
  void ScheduleRotationOnCompositorThread(const TargetConfig& aTargetConfig, bool aIsFirstPaint);

  /**
   * Returns the unique layer tree identifier that corresponds to the root
   * tree of this compositor.
   */
  uint64_t RootLayerTreeId();

  /**
   * Notify local and remote layer trees connected to this compositor that
   * the compositor's local device is being reset. All layers must be
   * invalidated to clear any cached TextureSources.
   *
   * This must be called on the compositor thread.
   */
  void InvalidateRemoteLayers();

  /**
   * Returns a pointer to the CompositorBridgeParent corresponding to the given ID.
   */
  static CompositorBridgeParent* GetCompositorBridgeParent(uint64_t id);

  /**
   * Notify the compositor for the given layer tree that vsync has occurred.
   */
  static void NotifyVsync(const TimeStamp& aTimeStamp, const uint64_t& aLayersId);

  /**
   * Set aController as the pan/zoom callback for the subtree referred
   * to by aLayersId.
   *
   * Must run on content main thread.
   */
  static void SetControllerForLayerTree(uint64_t aLayersId,
                                        GeckoContentController* aController);

  /**
   * A new child process has been configured to push transactions
   * directly to us.  Transport is to its thread context.
   */
  static bool
  CreateForContent(Endpoint<PCompositorBridgeParent>&& aEndpoint);

  struct LayerTreeState {
    LayerTreeState();
    ~LayerTreeState();
    RefPtr<Layer> mRoot;
    RefPtr<GeckoContentController> mController;
    APZCTreeManagerParent* mApzcTreeManagerParent;
    RefPtr<CompositorBridgeParent> mParent;
    HostLayerManager* mLayerManager;
    RefPtr<WebRenderBridgeParent> mWrBridge;
    // Pointer to the CrossProcessCompositorBridgeParent. Used by APZCs to share
    // their FrameMetrics with the corresponding child process that holds
    // the PCompositorBridgeChild
    CrossProcessCompositorBridgeParent* mCrossProcessParent;
    TargetConfig mTargetConfig;
    APZTestData mApzTestData;
    LayerTransactionParent* mLayerTree;
    nsTArray<PluginWindowData> mPluginData;
    bool mUpdatedPluginDataAvailable;

    // Most recent device reset sequence number that has not been acknowledged;
    // this is needed in case a device reset occurs in between allocating a
    // RefLayer id on the parent, and allocating a PLayerTransaction on the
    // child.
    Maybe<uint64_t> mPendingCompositorUpdate;

    CompositorController* GetCompositorController() const;
    MetricsSharingController* CrossProcessSharingController() const;
    MetricsSharingController* InProcessSharingController() const;
  };

  /**
   * Lookup the indirect shadow tree for |aId| and return it if it
   * exists.  Otherwise null is returned.  This must only be called on
   * the compositor thread.
   */
  static LayerTreeState* GetIndirectShadowTree(uint64_t aId);

  /**
   * Given the layers id for a content process, get the APZCTreeManagerParent
   * for the corresponding *root* layers id. That is, the APZCTreeManagerParent,
   * if one is found, will always be connected to the parent process rather
   * than a content process. Note that unless the compositor process is
   * separated this is expected to return null, because if the compositor is
   * living in the gecko parent process then there is no APZCTreeManagerParent
   * for the parent process.
   */
  static APZCTreeManagerParent* GetApzcTreeManagerParentForRoot(
        uint64_t aContentLayersId);
  /**
   * Same as the GetApzcTreeManagerParentForRoot function, but returns
   * the GeckoContentController for the parent process.
   */
  static GeckoContentController* GetGeckoContentControllerForRoot(
        uint64_t aContentLayersId);

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  /**
   * Calculates and requests the main thread update plugin positioning, clip,
   * and visibility via ipc.
   */
  bool UpdatePluginWindowState(uint64_t aId);

  /**
   * Plugin visibility helpers for the apz (main thread) and compositor
   * thread.
   */
  void ScheduleShowAllPluginWindows() override;
  void ScheduleHideAllPluginWindows() override;
  void ShowAllPluginWindows();
  void HideAllPluginWindows();
#else
  void ScheduleShowAllPluginWindows() override {}
  void ScheduleHideAllPluginWindows() override {}
#endif

  /**
   * Main thread response for a plugin visibility request made by the
   * compositor thread.
   */
  virtual mozilla::ipc::IPCResult RecvRemotePluginsReady() override;

  /**
   * Used by the profiler to denote when a vsync occured
   */
  static void PostInsertVsyncProfilerMarker(mozilla::TimeStamp aVsyncTimestamp);

  widget::CompositorWidget* GetWidget() { return mWidget; }

  void ForceComposeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect = nullptr);

  PAPZCTreeManagerParent* AllocPAPZCTreeManagerParent(const uint64_t& aLayersId) override;
  bool DeallocPAPZCTreeManagerParent(PAPZCTreeManagerParent* aActor) override;

  PAPZParent* AllocPAPZParent(const uint64_t& aLayersId) override;
  bool DeallocPAPZParent(PAPZParent* aActor) override;

  mozilla::ipc::IPCResult RecvGetCompositorOptions(const uint64_t& aLayersId,
                                                   CompositorOptions* aOptions) override;

  RefPtr<APZCTreeManager> GetAPZCTreeManager();

  CompositorOptions GetOptions() const {
    return mOptions;
  }

  PWebRenderBridgeParent* AllocPWebRenderBridgeParent(const wr::PipelineId& aPipelineId,
                                                      TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                                      uint32_t* aIdNamespace) override;
  bool DeallocPWebRenderBridgeParent(PWebRenderBridgeParent* aActor) override;
  static void SetWebRenderProfilerEnabled(bool aEnabled);

  static CompositorBridgeParent* GetCompositorBridgeParentFromLayersId(const uint64_t& aLayersId);
private:

  void Initialize();

  /**
   * Called during destruction in order to release resources as early as possible.
   */
  void StopAndClearResources();

  /**
   * This returns a reference to the APZCTreeManager to which
   * pan/zoom-related events can be sent.
   */
  static already_AddRefed<APZCTreeManager> GetAPZCTreeManager(uint64_t aLayersId);

  /**
   * Release compositor-thread resources referred to by |aID|.
   *
   * Must run on the content main thread.
   */
  static void DeallocateLayerTreeId(uint64_t aId);

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~CompositorBridgeParent();

  void DeferredDestroy();

  virtual PLayerTransactionParent*
    AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                 const uint64_t& aId,
                                 TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                 bool* aSuccess) override;
  virtual bool DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers) override;
  virtual void ScheduleTask(already_AddRefed<CancelableRunnable>, int);

  void SetEGLSurfaceSize(int width, int height);

  void InitializeLayerManager(const nsTArray<LayersBackend>& aBackendHints);

public:
  void PauseComposition();
  void ResumeComposition();
  void ResumeCompositionAndResize(int width, int height);
  void Invalidate();

protected:
  void ForceComposition();
  void CancelCurrentCompositeTask();

  // CompositorVsyncSchedulerOwner
  bool IsPendingComposite() override;
  void FinishPendingComposite() override;
  void CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect = nullptr) override;

  RefPtr<Compositor> NewCompositor(const nsTArray<LayersBackend>& aBackendHints);
  void ResetCompositorTask(const nsTArray<LayersBackend>& aBackendHints,
                           uint64_t aSeqNo,
                           Maybe<TextureFactoryIdentifier>* aOutNewIdentifier);
  Maybe<TextureFactoryIdentifier> ResetCompositorImpl(const nsTArray<LayersBackend>& aBackendHints);

  /**
   * Add a compositor to the global compositor map.
   */
  static void AddCompositor(CompositorBridgeParent* compositor, uint64_t* id);
  /**
   * Remove a compositor from the global compositor map.
   */
  static CompositorBridgeParent* RemoveCompositor(uint64_t id);

  /**
   * Creates the global compositor map.
   */
  static void Setup();

  /**
   * Destroys the compositor thread and global compositor map.
   */
  static void Shutdown();

  /**
   * Finish the shutdown operation on the compositor thread.
   */
  static void FinishShutdown();

  /**
   * Return true if current state allows compositing, that is
   * finishing a layers transaction.
   */
  bool CanComposite();

  void DidComposite(TimeStamp& aCompositeStart, TimeStamp& aCompositeEnd);

  virtual void NotifyDidCompositeToPipeline(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, TimeStamp& aCompositeStart, TimeStamp& aCompositeEnd) override;

  void NotifyDidComposite(uint64_t aTransactionId, TimeStamp& aCompositeStart, TimeStamp& aCompositeEnd);

  // The indirect layer tree lock must be held before calling this function.
  // Callback should take (LayerTreeState* aState, const uint64_t& aLayersId)
  template <typename Lambda>
  inline void ForEachIndirectLayerTree(const Lambda& aCallback);

  RefPtr<HostLayerManager> mLayerManager;
  RefPtr<Compositor> mCompositor;
  RefPtr<AsyncCompositionManager> mCompositionManager;
  RefPtr<WebRenderBridgeParent> mWrBridge;
  widget::CompositorWidget* mWidget;
  TimeStamp mTestTime;
  CSSToLayoutDeviceScale mScale;
  TimeDuration mVsyncRate;
  bool mIsTesting;

  uint64_t mPendingTransaction;

  bool mPaused;

  bool mUseExternalSurfaceSize;
  gfx::IntSize mEGLSurfaceSize;

  CompositorOptions mOptions;

  mozilla::Monitor mPauseCompositionMonitor;
  mozilla::Monitor mResumeCompositionMonitor;
  mozilla::Monitor mResetCompositorMonitor;

  uint64_t mCompositorID;
  uint64_t mRootLayerTreeID;

  bool mOverrideComposeReadiness;
  RefPtr<CancelableRunnable> mForceCompositionTask;

  RefPtr<APZCTreeManager> mApzcTreeManager;

  RefPtr<CompositorThreadHolder> mCompositorThreadHolder;
  RefPtr<CompositorVsyncScheduler> mCompositorScheduler;
  // This makes sure the compositorParent is not destroyed before receiving
  // confirmation that the channel is closed.
  // mSelfRef is cleared in DeferredDestroy which is scheduled by ActorDestroy.
  RefPtr<CompositorBridgeParent> mSelfRef;

  TimeDuration mPaintTime;

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  // cached plugin data used to reduce the number of updates we request.
  uint64_t mLastPluginUpdateLayerTreeId;
  nsIntPoint mPluginsLayerOffset;
  nsIntRegion mPluginsLayerVisibleRegion;
  nsTArray<PluginWindowData> mCachedPluginData;
  // Time until which we will block composition to wait for plugin updates.
  TimeStamp mWaitForPluginsUntil;
  // Indicates that we have actually blocked a composition waiting for plugins.
  bool mHaveBlockedForPlugins = false;
  // indicates if plugin window visibility and metric updates are currently
  // being defered due to a scroll operation.
  bool mDeferPluginWindows;
  // indicates if the plugin windows were hidden, and need to be made
  // visible again even if their geometry has not changed.
  bool mPluginWindowsHidden;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(CompositorBridgeParent);
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CompositorBridgeParent_h
