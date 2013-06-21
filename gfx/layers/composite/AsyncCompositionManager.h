/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ASYNCCOMPOSITIONMANAGER_H
#define GFX_ASYNCCOMPOSITIONMANAGER_H

#include "gfxPoint.h"
#include "gfx3DMatrix.h"
#include "nsAutoPtr.h"
#include "nsRect.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/layers/LayerTransaction.h" // for TargetConfig

namespace mozilla {
namespace layers {

class AsyncPanZoomController;
class Layer;
class LayerManagerComposite;
class AutoResolveRefLayers;

// Represents (affine) transforms that are calculated from a content view.
struct ViewTransform {
  ViewTransform(LayerPoint aTranslation = LayerPoint(),
                CSSToScreenScale aScale = CSSToScreenScale())
    : mTranslation(aTranslation)
    , mScale(aScale)
  {}

  operator gfx3DMatrix() const
  {
    return
      gfx3DMatrix::Translation(mTranslation.x, mTranslation.y, 0) *
      gfx3DMatrix::ScalingMatrix(mScale.scale, mScale.scale, 1);
  }

  LayerPoint mTranslation;
  CSSToScreenScale mScale;
};

/**
 * Manage async composition effects. This class is only used with OMTC and only
 * lives on the compositor thread. It is a layer on top of the layer manager
 * (LayerManagerComposite) which deals with elements of composition which are
 * usually dealt with by dom or layout when main thread rendering, but which can
 * short circuit that stuff to directly affect layers as they are composited,
 * for example, off-main thread animation, async video, async pan/zoom.
 */
class AsyncCompositionManager MOZ_FINAL : public RefCounted<AsyncCompositionManager>
{
  friend class AutoResolveRefLayers;
public:
  AsyncCompositionManager(LayerManagerComposite* aManager)
    : mLayerManager(aManager)
    , mIsFirstPaint(false)
    , mLayersUpdated(false)
    , mReadyForCompose(true)
  {
    MOZ_COUNT_CTOR(AsyncCompositionManager);
  }
  ~AsyncCompositionManager()
  {
    MOZ_COUNT_DTOR(AsyncCompositionManager);
  }

  /**
   * This forces the is-first-paint flag to true. This is intended to
   * be called by the widget code when it loses its viewport information
   * (or for whatever reason wants to refresh the viewport information).
   * The information refresh happens because the compositor will call
   * SetFirstPaintViewport on the next frame of composition.
   */
  void ForceIsFirstPaint() { mIsFirstPaint = true; }

  // Sample transforms for layer trees.  Return true to request
  // another animation frame.
  bool TransformShadowTree(TimeStamp aCurrentFrame);

  // Calculates the correct rotation and applies the transform to
  // our layer manager
  void ComputeRotation();

  // Call after updating our layer tree.
  void Updated(bool isFirstPaint, const TargetConfig& aTargetConfig)
  {
    mIsFirstPaint |= isFirstPaint;
    mLayersUpdated = true;
    mTargetConfig = aTargetConfig;
  }

  bool RequiresReorientation(mozilla::dom::ScreenOrientation aOrientation)
  {
    return mTargetConfig.orientation() != aOrientation;
  }

  // True if the underlying layer tree is ready to be composited.
  bool ReadyForCompose() { return mReadyForCompose; }

  // Returns true if the next composition will be the first for a
  // particular document.
  bool IsFirstPaint() { return mIsFirstPaint; }

private:
  void TransformScrollableLayer(Layer* aLayer, const gfx3DMatrix& aRootTransform);
  // Return true if an AsyncPanZoomController content transform was
  // applied for |aLayer|.  *aWantNextFrame is set to true if the
  // controller wants another animation frame.
  bool ApplyAsyncContentTransformToTree(TimeStamp aCurrentFrame, Layer* aLayer,
                                        bool* aWantNextFrame);

  void SetFirstPaintViewport(const LayerIntPoint& aOffset,
                             const CSSToLayerScale& aZoom,
                             const CSSRect& aCssPageRect);
  void SetPageRect(const CSSRect& aCssPageRect);
  void SyncViewportInfo(const LayerIntRect& aDisplayPort,
                        const CSSToLayerScale& aDisplayResolution,
                        bool aLayersUpdated,
                        ScreenPoint& aScrollOffset,
                        CSSToScreenScale& aScale,
                        gfx::Margin& aFixedLayerMargins,
                        ScreenPoint& aOffset);
  void SyncFrameMetrics(const ScreenPoint& aScrollOffset,
                        float aZoom,
                        const CSSRect& aCssPageRect,
                        bool aLayersUpdated,
                        const CSSRect& aDisplayPort,
                        const CSSToLayerScale& aDisplayResolution,
                        bool aIsFirstPaint,
                        gfx::Margin& aFixedLayerMargins,
                        ScreenPoint& aOffset);

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

  /**
   * DRAWING PHASE ONLY
   *
   * For reach RefLayer in our layer tree, look up its referent and connect it
   * to the layer tree, if found.
   */
  void ResolveRefLayers();
  /**
   * Detaches all referents resolved by ResolveRefLayers.
   * Assumes that mLayerManager->GetRoot() and mTargetConfig have not changed
   * since ResolveRefLayers was called.
   */
  void DetachRefLayers();

  TargetConfig mTargetConfig;
  CSSRect mContentRect;

  nsRefPtr<LayerManagerComposite> mLayerManager;
  // When this flag is set, the next composition will be the first for a
  // particular document (i.e. the document displayed on the screen will change).
  // This happens when loading a new page or switching tabs. We notify the
  // front-end (e.g. Java on Android) about this so that it take the new page
  // size and zoom into account when providing us with the next view transform.
  bool mIsFirstPaint;

  // This flag is set during a layers update, so that the first composition
  // after a layers update has it set. It is cleared after that first composition.
  bool mLayersUpdated;

  bool mReadyForCompose;
};

class MOZ_STACK_CLASS AutoResolveRefLayers {
public:
  AutoResolveRefLayers(AsyncCompositionManager* aManager) : mManager(aManager)
  { mManager->ResolveRefLayers(); }

  ~AutoResolveRefLayers()
  { mManager->DetachRefLayers(); }

private:
  AsyncCompositionManager* mManager;

  AutoResolveRefLayers(const AutoResolveRefLayers&) MOZ_DELETE;
  AutoResolveRefLayers& operator=(const AutoResolveRefLayers&) MOZ_DELETE;
};

} // layers
} // mozilla

#endif
