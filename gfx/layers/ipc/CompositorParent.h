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
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/Monitor.h"            // for Monitor
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/LayersMessages.h"  // for TargetConfig
#include "mozilla/layers/PCompositorParent.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"
#include "nsSize.h"                     // for nsIntSize

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

class CompositorParent : public PCompositorParent,
                         public ShadowLayersManager
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorParent)

public:
  CompositorParent(nsIWidget* aWidget,
                   bool aUseExternalSurfaceSize = false,
                   int aSurfaceWidth = -1, int aSurfaceHeight = -1);

  virtual ~CompositorParent();

  // IToplevelProtocol::CloneToplevel()
  virtual IToplevelProtocol*
  CloneToplevel(const InfallibleTArray<mozilla::ipc::ProtocolFdMapping>& aFds,
                base::ProcessHandle aPeerProcess,
                mozilla::ipc::ProtocolCloneContext* aCtx) MOZ_OVERRIDE;

  virtual bool RecvWillStop() MOZ_OVERRIDE;
  virtual bool RecvStop() MOZ_OVERRIDE;
  virtual bool RecvPause() MOZ_OVERRIDE;
  virtual bool RecvResume() MOZ_OVERRIDE;
  virtual bool RecvNotifyChildCreated(const uint64_t& child) MOZ_OVERRIDE;
  virtual bool RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                SurfaceDescriptor* aOutSnapshot);
  virtual bool RecvFlushRendering() MOZ_OVERRIDE;

  virtual bool RecvNotifyRegionInvalidated(const nsIntRegion& aRegion) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  virtual void ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                   const TargetConfig& aTargetConfig,
                                   bool isFirstPaint) MOZ_OVERRIDE;
  /**
   * This forces the is-first-paint flag to true. This is intended to
   * be called by the widget code when it loses its viewport information
   * (or for whatever reason wants to refresh the viewport information).
   * The information refresh happens because the compositor will call
   * SetFirstPaintViewport on the next frame of composition.
   */
  void ForceIsFirstPaint();
  void Destroy();

  LayerManagerComposite* GetLayerManager() { return mLayerManager; }

  void NotifyChildCreated(uint64_t aChild);

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
  void NotifyShadowTreeTransaction(uint64_t aId, bool aIsFirstPaint);

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
   * Destroys the compositor thread and the global compositor map.
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

  /**
   * Setup external message loop and thread ID for Compositor.
   * Should be used when CompositorParent should work in existing thread/MessageLoop,
   * for example moving Compositor into native toolkit main thread will allow to avoid
   * extra synchronization and call ::Composite() right from toolkit::Paint event
   */
  static void StartUpWithExistingThread(MessageLoop* aMsgLoop,
                                        PlatformThreadId aThreadID);

  struct LayerTreeState {
    LayerTreeState();
    nsRefPtr<Layer> mRoot;
    nsRefPtr<GeckoContentController> mController;
    CompositorParent* mParent;
    TargetConfig mTargetConfig;
  };

  /**
   * Lookup the indirect shadow tree for |aId| and return it if it
   * exists.  Otherwise null is returned.  This must only be called on
   * the compositor thread.
   */
  static const LayerTreeState* GetIndirectShadowTree(uint64_t aId);

  /**
   * Tell all CompositorParents to update their last refresh to aTime and sample
   * animations at this time stamp.  If aIsTesting is true, the
   * CompositorParents will become "paused" and continue sampling animations at
   * this time stamp until this function is called again with aIsTesting set to
   * false.
   */
  static void SetTimeAndSampleAnimations(TimeStamp aTime, bool aIsTesting);

  /**
   * Returns true if the calling thread is the compositor thread.
   */
  static bool IsInCompositorThread();
protected:
  virtual PLayerTransactionParent*
    AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                 const uint64_t& aId,
                                 TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                 bool* aSuccess);
  virtual bool DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers);
  virtual void ScheduleTask(CancelableTask*, int);
  void Composite();
  void CompositeInTransaction();
  virtual void ComposeToTarget(gfx::DrawTarget* aTarget);

  void SetEGLSurfaceSize(int width, int height);

private:
  void InitializeLayerManager(const nsTArray<LayersBackend>& aBackendHints);
  void PauseComposition();
  void ResumeComposition();
  void ResumeCompositionAndResize(int width, int height);
  void ForceComposition();

  inline static PlatformThreadId CompositorThreadID();

  /**
   * Creates a global map referencing each compositor by ID.
   *
   * This map is used by the ImageBridge protocol to trigger
   * compositions without having to keep references to the
   * compositor
   */
  static void CreateCompositorMap();
  static void DestroyCompositorMap();

  /**
   * Creates the compositor thread.
   *
   * All compositors live on the same thread.
   * The thread is not lazily created on first access to avoid dealing with
   * thread safety. Therefore it's best to create and destroy the thread when
   * we know we areb't using it (So creating/destroying along with gfxPlatform
   * looks like a good place).
   */
  static bool CreateThread();

  /**
   * Destroys the compositor thread.
   *
   * It is safe to call this fucntion more than once, although the second call
   * will have no effect.
   * This function is not thread-safe.
   */
  static void DestroyThread();

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

  nsRefPtr<LayerManagerComposite> mLayerManager;
  RefPtr<AsyncCompositionManager> mCompositionManager;
  nsIWidget* mWidget;
  CancelableTask *mCurrentCompositeTask;
  TimeStamp mLastCompose;
  TimeStamp mTestTime;
  bool mIsTesting;
#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeStamp mExpectedComposeTime;
#endif

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

  DISALLOW_EVIL_CONSTRUCTORS(CompositorParent);
};

} // layers
} // mozilla

#endif // mozilla_layers_CompositorParent_h
