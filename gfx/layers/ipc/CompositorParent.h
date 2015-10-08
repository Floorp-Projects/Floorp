/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorParent_h
#define mozilla_layers_CompositorParent_h

// Enable this pref to turn on compositor performance warning.
// This will print warnings if the compositor isn't meeting
// its responsiveness objectives:
//    1) Compose a frame within 15ms of receiving a ScheduleCompositeCall
//    2) Unless a frame was composited within the throttle threshold in
//       which the deadline will be 15ms + throttle threshold
//#define COMPOSITOR_PERFORMANCE_WARNING

#include <stdint.h>                     // for uint64_t
#include "Layers.h"                     // for Layer
#include "base/basictypes.h"            // for DISALLOW_EVIL_CONSTRUCTORS
#include "base/platform_thread.h"       // for PlatformThreadId
#include "base/thread.h"                // for Thread
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for override
#include "mozilla/Monitor.h"            // for Monitor
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/LayersMessages.h"  // for TargetConfig
#include "mozilla/layers/PCompositorParent.h"
#include "mozilla/layers/ShadowLayersManager.h" // for ShadowLayersManager
#include "mozilla/layers/APZTestData.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "mozilla/VsyncDispatcher.h"

class CancelableTask;
class MessageLoop;
class nsIWidget;

namespace mozilla {
namespace gfx {
class DrawTarget;
} // namespace gfx

namespace layers {

class APZCTreeManager;
class AsyncCompositionManager;
class Compositor;
class CompositorParent;
class LayerManagerComposite;
class LayerTransactionParent;

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

class CompositorThreadHolder final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(CompositorThreadHolder)

public:
  CompositorThreadHolder();

  base::Thread* GetCompositorThread() const {
    return mCompositorThread;
  }

private:
  ~CompositorThreadHolder();

  base::Thread* const mCompositorThread;

  static base::Thread* CreateCompositorThread();
  static void DestroyCompositorThread(base::Thread* aCompositorThread);

  friend class CompositorParent;
};

/**
 * Manages the vsync (de)registration and tracking on behalf of the
 * compositor when it need to paint.
 * Turns vsync notifications into scheduled composites.
 **/
class CompositorVsyncScheduler
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorVsyncScheduler)

public:
  explicit CompositorVsyncScheduler(CompositorParent* aCompositorParent, nsIWidget* aWidget);
  bool NotifyVsync(TimeStamp aVsyncTimestamp);
  void SetNeedsComposite(bool aSchedule);
  void OnForceComposeToTarget();

  void ScheduleTask(CancelableTask*, int);
  void ResumeComposition();
  void ComposeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect = nullptr);
  void PostCompositeTask(TimeStamp aCompositeTimestamp);
  void Destroy();
  void ScheduleComposition();
  void CancelCurrentCompositeTask();
  bool NeedsComposite();
  void Composite(TimeStamp aVsyncTimestamp);
  void ForceComposeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect);

  const TimeStamp& GetLastComposeTime()
  {
    return mLastCompose;
  }

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  const TimeStamp& GetExpectedComposeStartTime()
  {
    return mExpectedComposeStartTime;
  }
#endif
 
private:
  virtual ~CompositorVsyncScheduler();

  void NotifyCompositeTaskExecuted();
  void ObserveVsync();
  void UnobserveVsync();
  void DispatchTouchEvents(TimeStamp aVsyncTimestamp);
  void CancelCurrentSetNeedsCompositeTask();

  class Observer final : public VsyncObserver
  {
  public:
    explicit Observer(CompositorVsyncScheduler* aOwner);
    virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) override;
    void Destroy();
  private:
    virtual ~Observer();

    Mutex mMutex;
    // Hold raw pointer to avoid mutual reference.
    CompositorVsyncScheduler* mOwner;
  };

  CompositorParent* mCompositorParent;
  TimeStamp mLastCompose;
  CancelableTask* mCurrentCompositeTask;

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeStamp mExpectedComposeStartTime;
#endif

  bool mAsapScheduling;
  bool mNeedsComposite;
  bool mIsObservingVsync;
  int32_t mVsyncNotificationsSkipped;
  nsRefPtr<CompositorVsyncDispatcher> mCompositorVsyncDispatcher;
  nsRefPtr<CompositorVsyncScheduler::Observer> mVsyncObserver;

  mozilla::Monitor mCurrentCompositeTaskMonitor;

  mozilla::Monitor mSetNeedsCompositeMonitor;
  CancelableTask* mSetNeedsCompositeTask;
};

class CompositorUpdateObserver
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorUpdateObserver);

  virtual void ObserveUpdate(uint64_t aLayersId, bool aActive) = 0;

protected:
  virtual ~CompositorUpdateObserver() {}
};

class CompositorParent final : public PCompositorParent,
                               public ShadowLayersManager
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(CompositorParent)
  friend class CompositorVsyncScheduler;

public:
  explicit CompositorParent(nsIWidget* aWidget,
                            bool aUseExternalSurfaceSize = false,
                            int aSurfaceWidth = -1, int aSurfaceHeight = -1);

  // IToplevelProtocol::CloneToplevel()
  virtual IToplevelProtocol*
  CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                base::ProcessHandle aPeerProcess,
                mozilla::ipc::ProtocolCloneContext* aCtx) override;

  virtual bool RecvGetFrameUniformity(FrameUniformityData* aOutData) override;
  virtual bool RecvRequestOverfill() override;
  virtual bool RecvWillStop() override;
  virtual bool RecvStop() override;
  virtual bool RecvPause() override;
  virtual bool RecvResume() override;
  virtual bool RecvNotifyHidden(const uint64_t& id) override { return true; }
  virtual bool RecvNotifyVisible(const uint64_t& id) override { return true; }
  virtual bool RecvNotifyChildCreated(const uint64_t& child) override;
  virtual bool RecvAdoptChild(const uint64_t& child) override;
  virtual bool RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                const gfx::IntRect& aRect) override;
  virtual bool RecvMakeWidgetSnapshot(const SurfaceDescriptor& aInSnapshot) override;
  virtual bool RecvFlushRendering() override;

  virtual bool RecvGetTileSize(int32_t* aWidth, int32_t* aHeight) override;

  virtual bool RecvNotifyRegionInvalidated(const nsIntRegion& aRegion) override;
  virtual bool RecvStartFrameTimeRecording(const int32_t& aBufferSize, uint32_t* aOutStartIndex) override;
  virtual bool RecvStopFrameTimeRecording(const uint32_t& aStartIndex, InfallibleTArray<float>* intervals) override;

  // Unused for chrome <-> compositor communication (which this class does).
  // @see CrossProcessCompositorParent::RecvRequestNotifyAfterRemotePaint
  virtual bool RecvRequestNotifyAfterRemotePaint() override { return true; };

  virtual void ActorDestroy(ActorDestroyReason why) override;

  virtual void ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                   const uint64_t& aTransactionId,
                                   const TargetConfig& aTargetConfig,
                                   const InfallibleTArray<PluginWindowData>& aPlugins,
                                   bool aIsFirstPaint,
                                   bool aScheduleComposite,
                                   uint32_t aPaintSequenceNumber,
                                   bool aIsRepeatTransaction,
                                   int32_t aPaintSyncId) override;
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

  /**
   * This forces the is-first-paint flag to true. This is intended to
   * be called by the widget code when it loses its viewport information
   * (or for whatever reason wants to refresh the viewport information).
   * The information refresh happens because the compositor will call
   * SetFirstPaintViewport on the next frame of composition.
   */
  void ForceIsFirstPaint();
  void Destroy();

  static void SetShadowProperties(Layer* aLayer);

  void NotifyChildCreated(const uint64_t& aChild);

  void AsyncRender();

  // Can be called from any thread
  void ScheduleRenderOnCompositorThread();
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
      bool aIsRepeatTransaction);

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
   * Returns a pointer to the compositor corresponding to the given ID.
   */
  static CompositorParent* GetCompositor(uint64_t id);

  /**
   * Returns the compositor thread's message loop.
   *
   * This message loop is used by CompositorParent and ImageBridgeParent.
   */
  static MessageLoop* CompositorLoop();

  /**
   * Creates the compositor thread and the global compositor map.
   */
  static void StartUp();

  /**
   * Waits for all [CrossProcess]CompositorParent's to be gone,
   * and destroys the compositor thread and global compositor map.
   *
   * Does not return until all of that has completed.
   */
  static void ShutDown();

  /**
   * Allocate an ID that can be used to refer to a layer tree and
   * associated resources that live only on the compositor thread.
   *
   * Must run on the content main thread.
   */
  static uint64_t AllocateLayerTreeId();
  /**
   * Release compositor-thread resources referred to by |aID|.
   *
   * Must run on the content main thread.
   */
  static void DeallocateLayerTreeId(uint64_t aId);

  /**
   * Set aController as the pan/zoom callback for the subtree referred
   * to by aLayersId.
   *
   * Must run on content main thread.
   */
  static void SetControllerForLayerTree(uint64_t aLayersId,
                                        GeckoContentController* aController);

  /**
   * This returns a reference to the APZCTreeManager to which
   * pan/zoom-related events can be sent.
   */
  static APZCTreeManager* GetAPZCTreeManager(uint64_t aLayersId);

  /**
   * A new child process has been configured to push transactions
   * directly to us.  Transport is to its thread context.
   */
  static PCompositorParent*
  Create(Transport* aTransport, ProcessId aOtherProcess);

  struct LayerTreeState {
    LayerTreeState();
    ~LayerTreeState();
    nsRefPtr<Layer> mRoot;
    nsRefPtr<GeckoContentController> mController;
    CompositorParent* mParent;
    LayerManagerComposite* mLayerManager;
    // Pointer to the CrossProcessCompositorParent. Used by APZCs to share
    // their FrameMetrics with the corresponding child process that holds
    // the PCompositorChild
    PCompositorParent* mCrossProcessParent;
    TargetConfig mTargetConfig;
    APZTestData mApzTestData;
    LayerTransactionParent* mLayerTree;
    nsTArray<PluginWindowData> mPluginData;
    bool mUpdatedPluginDataAvailable;
    nsRefPtr<CompositorUpdateObserver> mLayerTreeReadyObserver;
    nsRefPtr<CompositorUpdateObserver> mLayerTreeClearedObserver;
  };

  /**
   * Lookup the indirect shadow tree for |aId| and return it if it
   * exists.  Otherwise null is returned.  This must only be called on
   * the compositor thread.
   */
  static LayerTreeState* GetIndirectShadowTree(uint64_t aId);

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  /**
   * Calculates and requests the main thread update plugin positioning, clip,
   * and visibility via ipc.
   */
  bool UpdatePluginWindowState(uint64_t aId);
#endif

  /**
   * Main thread response for a plugin visibility request made by the
   * compositor thread.
   */
  virtual bool RecvRemotePluginsReady() override;

  /**
   * Used by the profiler to denote when a vsync occured
   */
  static void PostInsertVsyncProfilerMarker(mozilla::TimeStamp aVsyncTimestamp);

  static void RequestNotifyLayerTreeReady(uint64_t aLayersId, CompositorUpdateObserver* aObserver);
  static void RequestNotifyLayerTreeCleared(uint64_t aLayersId, CompositorUpdateObserver* aObserver);
  static void SwapLayerTreeObservers(uint64_t aLayer, uint64_t aOtherLayer);

  float ComputeRenderIntegrity();

  /**
   * Returns true if the calling thread is the compositor thread.
   */
  static bool IsInCompositorThread();

  nsIWidget* GetWidget() { return mWidget; }

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~CompositorParent();

  void DeferredDestroy();

  virtual PLayerTransactionParent*
    AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                 const uint64_t& aId,
                                 TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                 bool* aSuccess) override;
  virtual bool DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers) override;
  virtual void ScheduleTask(CancelableTask*, int);
  void CompositeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect = nullptr);
  void ForceComposeToTarget(gfx::DrawTarget* aTarget, const gfx::IntRect* aRect = nullptr);

  void SetEGLSurfaceSize(int width, int height);

  void InitializeLayerManager(const nsTArray<LayersBackend>& aBackendHints);
  void PauseComposition();
  void ResumeComposition();
  void ResumeCompositionAndResize(int width, int height);
  void ForceComposition();
  void CancelCurrentCompositeTask();
  void Invalidate();

  /**
   * Add a compositor to the global compositor map.
   */
  static void AddCompositor(CompositorParent* compositor, uint64_t* id);
  /**
   * Remove a compositor from the global compositor map.
   */
  static CompositorParent* RemoveCompositor(uint64_t id);

   /**
   * Return true if current state allows compositing, that is
   * finishing a layers transaction.
   */
  bool CanComposite();

  void DidComposite(TimeStamp& aCompositeStart, TimeStamp& aCompositeEnd);

  nsRefPtr<LayerManagerComposite> mLayerManager;
  nsRefPtr<Compositor> mCompositor;
  RefPtr<AsyncCompositionManager> mCompositionManager;
  nsIWidget* mWidget;
  TimeStamp mTestTime;
  bool mIsTesting;

  uint64_t mPendingTransaction;

  bool mPaused;

  bool mUseExternalSurfaceSize;
  gfx::IntSize mEGLSurfaceSize;

  mozilla::Monitor mPauseCompositionMonitor;
  mozilla::Monitor mResumeCompositionMonitor;

  uint64_t mCompositorID;
  const uint64_t mRootLayerTreeID;

  bool mOverrideComposeReadiness;
  CancelableTask* mForceCompositionTask;

  nsRefPtr<APZCTreeManager> mApzcTreeManager;

  nsRefPtr<CompositorThreadHolder> mCompositorThreadHolder;
  nsRefPtr<CompositorVsyncScheduler> mCompositorScheduler;

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  // cached plugin data used to reduce the number of updates we request.
  uint64_t mLastPluginUpdateLayerTreeId;
  nsIntPoint mPluginsLayerOffset;
  nsIntRegion mPluginsLayerVisibleRegion;
  nsTArray<PluginWindowData> mCachedPluginData;
#endif
#if defined(XP_WIN)
  // indicates if we are currently waiting on a plugin update confirmation.
  // When this is true, composition is currently on hold.
  bool mPluginUpdateResponsePending;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(CompositorParent);
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CompositorParent_h
