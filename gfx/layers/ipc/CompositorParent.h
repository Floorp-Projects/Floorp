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
#define COMPOSITOR_PERFORMANCE_WARNING

#include "mozilla/layers/PCompositorParent.h"
#include "mozilla/layers/PLayersParent.h"
#include "base/thread.h"
#include "mozilla/Monitor.h"
#include "ShadowLayersManager.h"

class nsIWidget;

namespace mozilla {
namespace layers {

class LayerManager;

// Represents (affine) transforms that are calculated from a content view.
struct ViewTransform {
  ViewTransform(nsIntPoint aTranslation = nsIntPoint(0, 0), float aXScale = 1, float aYScale = 1)
    : mTranslation(aTranslation)
    , mXScale(aXScale)
    , mYScale(aYScale)
  {}

  operator gfx3DMatrix() const
  {
    return
      gfx3DMatrix::ScalingMatrix(mXScale, mYScale, 1) *
      gfx3DMatrix::Translation(mTranslation.x, mTranslation.y, 0);
  }

  nsIntPoint mTranslation;
  float mXScale;
  float mYScale;
};

class CompositorParent : public PCompositorParent,
                         public ShadowLayersManager
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorParent)
public:
  CompositorParent(nsIWidget* aWidget, MessageLoop* aMsgLoop,
                   PlatformThreadId aThreadID, bool aRenderToEGLSurface = false,
                   int aSurfaceWidth = -1, int aSurfaceHeight = -1);

  virtual ~CompositorParent();

  virtual bool RecvWillStop() MOZ_OVERRIDE;
  virtual bool RecvStop() MOZ_OVERRIDE;
  virtual bool RecvPause() MOZ_OVERRIDE;
  virtual bool RecvResume() MOZ_OVERRIDE;

  virtual void ShadowLayersUpdated(bool isFirstPaint) MOZ_OVERRIDE;
  void Destroy();

  LayerManager* GetLayerManager() { return mLayerManager; }

  void SetTransformation(float aScale, nsIntPoint aScrollOffset);
  void AsyncRender();

  // Can be called from any thread
  void ScheduleRenderOnCompositorThread();
  void SchedulePauseOnCompositorThread();
  void ScheduleResumeOnCompositorThread(int width, int height);

protected:
  virtual PLayersParent* AllocPLayers(const LayersBackend& aBackendType, int* aMaxTextureSize);
  virtual bool DeallocPLayers(PLayersParent* aLayers);
  virtual void ScheduleTask(CancelableTask*, int);
  virtual void Composite();
  virtual void ScheduleComposition();
  virtual void SetFirstPaintViewport(float aOffsetX, float aOffsetY, float aZoom, float aPageWidth, float aPageHeight,
                                     float aCssPageWidth, float aCssPageHeight);
  virtual void SetPageSize(float aZoom, float aPageWidth, float aPageHeight, float aCssPageWidth, float aCssPageHeight);
  virtual void SyncViewportInfo(const nsIntRect& aDisplayPort, float aDisplayResolution, bool aLayersUpdated,
                                nsIntPoint& aScrollOffset, float& aScaleX, float& aScaleY);
  void SetEGLSurfaceSize(int width, int height);

private:
  void PauseComposition();
  void ResumeComposition();
  void ResumeCompositionAndResize(int width, int height);

  void TransformShadowTree();

  inline MessageLoop* CompositorLoop();
  inline PlatformThreadId CompositorThreadID();

  // Platform specific functions
  /**
   * Does a breadth-first search to find the first layer in the tree with a
   * displayport set.
   */
  Layer* GetPrimaryScrollableLayer();

  nsRefPtr<LayerManager> mLayerManager;
  nsIWidget* mWidget;
  CancelableTask *mCurrentCompositeTask;
  TimeStamp mLastCompose;
#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeStamp mExpectedComposeTime;
#endif

  bool mPaused;
  float mXScale;
  float mYScale;
  nsIntPoint mScrollOffset;
  nsIntSize mContentSize;

  // When this flag is set, the next composition will be the first for a
  // particular document (i.e. the document displayed on the screen will change).
  // This happens when loading a new page or switching tabs. We notify the
  // front-end (e.g. Java on Android) about this so that it take the new page
  // size and zoom into account when providing us with the next view transform.
  bool mIsFirstPaint;

  // This flag is set during a layers update, so that the first composition
  // after a layers update has it set. It is cleared after that first composition.
  bool mLayersUpdated;

  MessageLoop* mCompositorLoop;
  PlatformThreadId mThreadID;
  bool mRenderToEGLSurface;
  nsIntSize mEGLSurfaceSize;

  mozilla::Monitor mPauseCompositionMonitor;

  DISALLOW_EVIL_CONSTRUCTORS(CompositorParent);
};

} // layers
} // mozilla

#endif // mozilla_layers_CompositorParent_h
