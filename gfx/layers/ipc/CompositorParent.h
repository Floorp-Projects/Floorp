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
#include "ShadowLayersManager.h"        // for ShadowLayersManager
#include "base/basictypes.h"            // for DISALLOW_EVIL_CONSTRUCTORS
#include "base/platform_thread.h"       // for PlatformThreadId
#include "base/thread.h"                // for Thread
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/Monitor.h"            // for Monitor
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/LayersMessages.h"  // for TargetConfig
#include "mozilla/layers/PCompositorParent.h"
#include "mozilla/layers/APZTestData.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"
#include "nsSize.h"                     // for nsIntSize
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

class CancelableTask;
class MessageLoop;
class gfxContext;
class nsIWidget;

namespace mozilla {
namespace gfx {
class DrawTarget;
}

namespace layers {

class APZCTreeManager;
class AsyncCompositionManager;
class Compositor;
class LayerManagerComposite;
class LayerTransactionParent;

struct ScopedLayerTreeRegistration
{
  ScopedLayerTreeRegistration(uint64_t aLayersId,
                              Layer* aRoot,
                              GeckoContentController* aController);
  ~ScopedLayerTreeRegistration();

private:
  uint64_t mLayersId;
};

class CompositorThreadHolder MOZ_FINAL
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

class CompositorParent : public PCompositorParent,
                         public ShadowLayersManager
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(CompositorParent)

public:
  CompositorParent(nsIWidget* aWidget,
                   bool aUseExternalSurfaceSize = false,
                   int aSurfaceWidth = -1, int aSurfaceHeight = -1);

  // IToplevelProtocol::CloneToplevel()
  virtual IToplevelProtocol*
  CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                base::ProcessHandle aPeerProcess,
                mozilla::ipc::ProtocolCloneContext* aCtx) MOZ_OVERRIDE;

  virtual bool RecvRequestOverfill() MOZ_OVERRIDE;
  virtual bool RecvWillStop() MOZ_OVERRIDE;
  virtual bool RecvStop() MOZ_OVERRIDE;
  virtual bool RecvPause() MOZ_OVERRIDE;
  virtual bool RecvResume() MOZ_OVERRIDE;
  virtual bool RecvNotifyChildCreated(const uint64_t& child) MOZ_OVERRIDE;
  virtual bool RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                const nsIntRect& aRect) MOZ_OVERRIDE;
  virtual bool RecvFlushRendering() MOZ_OVERRIDE;

  virtual bool RecvNotifyRegionInvalidated(const nsIntRegion& aRegion) MOZ_OVERRIDE;
  virtual bool RecvStartFrameTimeRecording(const int32_t& aBufferSize, uint32_t* aOutStartIndex) MOZ_OVERRIDE;
  virtual bool RecvStopFrameTimeRecording(const uint32_t& aStartIndex, InfallibleTArray<float>* intervals) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  virtual void ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                   const uint64_t& aTransactionId,
                                   const TargetConfig& aTargetConfig,
                                   bool aIsFirstPaint,
                                   bool aScheduleComposite,
                                   uint32_t aPaintSequenceNumber) MOZ_OVERRIDE;
  virtual void ForceComposite(LayerTransactionParent* aLayerTree) MOZ_OVERRIDE;
  virtual bool SetTestSampleTime(LayerTransactionParent* aLayerTree,
                                 const TimeStamp& aTime) MOZ_OVERRIDE;
  virtual void LeaveTestMode(LayerTransactionParent* aLayerTree) MOZ_OVERRIDE;
  virtual void GetAPZTestData(const LayerTransactionParent* aLayerTree,
                              APZTestData* aOutData) MOZ_OVERRIDE;
  virtual AsyncCompositionManager* GetCompositionManager(LayerTransactionParent* aLayerTree) MOZ_OVERRIDE { return mCompositionManager; }

  /**
   * This forces the is-first-paint flag to true. This is intended to
   * be called by the widget code when it loses its viewport information
   * (or for whatever reason wants to refresh the viewport information).
   * The information refresh happens because the compositor will call
   * SetFirstPaintViewport on the next frame of composition.
   */
  void ForceIsFirstPaint();
  void Destroy();

  void NotifyChildCreated(const uint64_t& aChild);

  void AsyncRender();

  // Can be called from any thread
  void ScheduleRenderOnCompositorThread();
  void SchedulePauseOnCompositorThread();
  /**
   * Returns true if a surface was obtained and the resume succeeded; false
   * otherwise.
   */
  bool ScheduleResumeOnCompositorThread(int width, int height);

  virtual void ScheduleComposition();
  void NotifyShadowTreeTransaction(uint64_t aId, bool aIsFirstPaint,
      bool aScheduleComposite, uint32_t aPaintSequenceNumber);

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
  };

  /**
   * Lookup the indirect shadow tree for |aId| and return it if it
   * exists.  Otherwise null is returned.  This must only be called on
   * the compositor thread.
   */
  static LayerTreeState* GetIndirectShadowTree(uint64_t aId);

  float ComputeRenderIntegrity();

  /**
   * Returns true if the calling thread is the compositor thread.
   */
  static bool IsInCompositorThread();

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~CompositorParent();

  void DeferredDestroy();

  virtual PLayerTransactionParent*
    AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                 const uint64_t& aId,
                                 TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                 bool* aSuccess) MOZ_OVERRIDE;
  virtual bool DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers) MOZ_OVERRIDE;
  virtual void ScheduleTask(CancelableTask*, int);
  void CompositeCallback();
  void CompositeToTarget(gfx::DrawTarget* aTarget, const nsIntRect* aRect = nullptr);
  void ForceComposeToTarget(gfx::DrawTarget* aTarget, const nsIntRect* aRect = nullptr);

  void SetEGLSurfaceSize(int width, int height);

  void InitializeLayerManager(const nsTArray<LayersBackend>& aBackendHints);
  void PauseComposition();
  void ResumeComposition();
  void ResumeCompositionAndResize(int width, int height);
  void ForceComposition();
  void CancelCurrentCompositeTask();

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

  void DidComposite();

  nsRefPtr<LayerManagerComposite> mLayerManager;
  nsRefPtr<Compositor> mCompositor;
  RefPtr<AsyncCompositionManager> mCompositionManager;
  nsIWidget* mWidget;
  CancelableTask *mCurrentCompositeTask;
  TimeStamp mLastCompose;
  TimeStamp mTestTime;
  bool mIsTesting;
#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeStamp mExpectedComposeStartTime;
#endif

  uint64_t mPendingTransaction;

  bool mPaused;

  bool mUseExternalSurfaceSize;
  nsIntSize mEGLSurfaceSize;

  mozilla::Monitor mPauseCompositionMonitor;
  mozilla::Monitor mResumeCompositionMonitor;

  uint64_t mCompositorID;
  uint64_t mRootLayerTreeID;

  bool mOverrideComposeReadiness;
  CancelableTask* mForceCompositionTask;

  nsRefPtr<APZCTreeManager> mApzcTreeManager;

  nsRefPtr<CompositorThreadHolder> mCompositorThreadHolder;

  DISALLOW_EVIL_CONSTRUCTORS(CompositorParent);
};

} // layers
} // mozilla

#endif // mozilla_layers_CompositorParent_h
