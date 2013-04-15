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

#include "mozilla/layers/PCompositorParent.h"
#include "mozilla/layers/PLayersParent.h"
#include "base/thread.h"
#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "ShadowLayersManager.h"
#include "LayerManagerComposite.h"
class nsIWidget;

namespace base {
class Thread;
}

namespace mozilla {
namespace layers {

class AsyncPanZoomController;
class Layer;
class LayerManager;
struct TextureFactoryIdentifier;

// Represents (affine) transforms that are calculated from a content view.
struct ViewTransform {
  ViewTransform(gfxPoint aTranslation = gfxPoint(),
                gfxSize aScale = gfxSize(1, 1))
    : mTranslation(aTranslation)
    , mScale(aScale)
  {}

  operator gfx3DMatrix() const
  {
    return
      gfx3DMatrix::ScalingMatrix(mScale.width, mScale.height, 1) *
      gfx3DMatrix::Translation(mTranslation.x, mTranslation.y, 0);
  }

  gfxPoint mTranslation;
  gfxSize mScale;
};

class CompositorParent : public PCompositorParent,
                         public ShadowLayersManager
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorParent)

public:
  CompositorParent(nsIWidget* aWidget,
                   bool aRenderToEGLSurface = false,
                   int aSurfaceWidth = -1, int aSurfaceHeight = -1);

  virtual ~CompositorParent();

  virtual bool RecvWillStop() MOZ_OVERRIDE;
  virtual bool RecvStop() MOZ_OVERRIDE;
  virtual bool RecvPause() MOZ_OVERRIDE;
  virtual bool RecvResume() MOZ_OVERRIDE;
  virtual bool RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                SurfaceDescriptor* aOutSnapshot);

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  virtual void ShadowLayersUpdated(ShadowLayersParent* aLayerTree,
                                   const TargetConfig& aTargetConfig,
                                   bool isFirstPaint) MOZ_OVERRIDE;
  /**
   * This forces the is-first-paint flag to true. This is intended to
   * be called by the widget code when it loses its viewport information
   * (or for whatever reason wants to refresh the viewport information).
   * The information refresh happens because the compositor will call
   * SetFirstPaintViewport on the next frame of composition.
   */
  void ForceIsFirstPaint() { mIsFirstPaint = true; }
  void Destroy();

  LayerManagerComposite* GetLayerManager() { return mLayerManager; }

  void SetTransformation(float aScale, nsIntPoint aScrollOffset);
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
  void NotifyShadowTreeTransaction();

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
   * Set aController as the pan/zoom controller for the tree referred
   * to by aLayersId.
   *
   * Must run on content main thread.
   */
  static void SetPanZoomControllerForLayerTree(uint64_t aLayersId,
                                               AsyncPanZoomController* aController);

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

protected:
  virtual PLayersParent* AllocPLayers(const LayersBackend& aBackendHint,
                                      const uint64_t& aId,
                                      TextureFactoryIdentifier* aTextureFactoryIdentifier);
  virtual bool DeallocPLayers(PLayersParent* aLayers);
  virtual void ScheduleTask(CancelableTask*, int);
  virtual void Composite();
  virtual void ComposeToTarget(gfxContext* aTarget);
  virtual void SetFirstPaintViewport(const nsIntPoint& aOffset, float aZoom, const nsIntRect& aPageRect, const gfx::Rect& aCssPageRect);
  virtual void SetPageRect(const gfx::Rect& aCssPageRect);
  virtual void SyncViewportInfo(const nsIntRect& aDisplayPort, float aDisplayResolution, bool aLayersUpdated,
                                nsIntPoint& aScrollOffset, float& aScaleX, float& aScaleY,
                                gfx::Margin& aFixedLayerMargins);
  void SetEGLSurfaceSize(int width, int height);
  // If SetPanZoomControllerForLayerTree is not set, Compositor will use
  // derived class AsyncPanZoomController transformations.
  // Compositor will not own AsyncPanZoomController here.
  virtual AsyncPanZoomController* GetDefaultPanZoomController() { return nullptr; }

private:
  void PauseComposition();
  void ResumeComposition();
  void ResumeCompositionAndResize(int width, int height);
  void ForceComposition();

  // Sample transforms for layer trees.  Return true to request
  // another animation frame.
  bool TransformShadowTree(TimeStamp aCurrentFrame);
  void TransformScrollableLayer(Layer* aLayer, const gfx3DMatrix& aRootTransform);
  // Return true if an AsyncPanZoomController content transform was
  // applied for |aLayer|.  *aWantNextFrame is set to true if the
  // controller wants another animation frame.
  bool ApplyAsyncContentTransformToTree(TimeStamp aCurrentFrame, Layer* aLayer,
                                        bool* aWantNextFrame);

  inline PlatformThreadId CompositorThreadID();

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

  // Platform specific functions
  /**
   * Recursively applies the given translation to all top-level fixed position
   * layers that are descendants of the given layer.
   * aScaleDiff is considered to be the scale transformation applied when
   * displaying the layers, and is used to make sure the anchor points of
   * fixed position layers remain in the same position.
   */
  void TransformFixedLayers(Layer* aLayer,
                            const gfxPoint& aTranslation,
                            const gfxSize& aScaleDiff,
                            const gfx::Margin& aFixedLayerMargins);

  nsRefPtr<LayerManagerComposite> mLayerManager;
  nsIWidget* mWidget;
  TargetConfig mTargetConfig;
  CancelableTask *mCurrentCompositeTask;
  TimeStamp mLastCompose;
#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeStamp mExpectedComposeTime;
#endif

  bool mPaused;
  float mXScale;
  float mYScale;
  nsIntPoint mScrollOffset;
  nsIntRect mContentRect;

  // When this flag is set, the next composition will be the first for a
  // particular document (i.e. the document displayed on the screen will change).
  // This happens when loading a new page or switching tabs. We notify the
  // front-end (e.g. Java on Android) about this so that it take the new page
  // size and zoom into account when providing us with the next view transform.
  bool mIsFirstPaint;

  // This flag is set during a layers update, so that the first composition
  // after a layers update has it set. It is cleared after that first composition.
  bool mLayersUpdated;

  bool mRenderToEGLSurface;
  nsIntSize mEGLSurfaceSize;

  mozilla::Monitor mPauseCompositionMonitor;
  mozilla::Monitor mResumeCompositionMonitor;

  uint64_t mCompositorID;

  bool mOverrideComposeReadiness;
  CancelableTask* mForceCompositionTask;

  DISALLOW_EVIL_CONSTRUCTORS(CompositorParent);
};

} // layers
} // mozilla

#endif // mozilla_layers_CompositorParent_h
