/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ASYNCCOMPOSITIONMANAGER_H
#define GFX_ASYNCCOMPOSITIONMANAGER_H

#include "Units.h"                      // for ScreenPoint, etc
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerManagerComposite
#include "mozilla/Attributes.h"         // for MOZ_DELETE, MOZ_FINAL, etc
#include "mozilla/RefPtr.h"             // for RefCounted
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/dom/ScreenOrientation.h"  // for ScreenOrientation
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/layers/LayersMessages.h"  // for TargetConfig
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"            // for LayerManager::AddRef, etc

namespace mozilla {
namespace layers {

class AsyncPanZoomController;
class Layer;
class LayerManagerComposite;
class AutoResolveRefLayers;

// Represents (affine) transforms that are calculated from a content view.
struct ViewTransform {
  explicit ViewTransform(ParentLayerToScreenScale aScale = ParentLayerToScreenScale(),
                         ScreenPoint aTranslation = ScreenPoint())
    : mScale(aScale)
    , mTranslation(aTranslation)
  {}

  operator gfx::Matrix4x4() const
  {
    return
      gfx::Matrix4x4::Scaling(mScale.scale, mScale.scale, 1)
                      .PostTranslate(mTranslation.x, mTranslation.y, 0);
  }

  // For convenience, to avoid writing the cumbersome
  // "gfx::Matrix4x4(a) * gfx::Matrix4x4(b)".
  friend gfx::Matrix4x4 operator*(const ViewTransform& a, const ViewTransform& b) {
    return gfx::Matrix4x4(a) * gfx::Matrix4x4(b);
  }

  bool operator==(const ViewTransform& rhs) const {
    return mTranslation == rhs.mTranslation && mScale == rhs.mScale;
  }

  bool operator!=(const ViewTransform& rhs) const {
    return !(*this == rhs);
  }

  ParentLayerToScreenScale mScale;
  ScreenPoint mTranslation;
};

/**
 * Manage async composition effects. This class is only used with OMTC and only
 * lives on the compositor thread. It is a layer on top of the layer manager
 * (LayerManagerComposite) which deals with elements of composition which are
 * usually dealt with by dom or layout when main thread rendering, but which can
 * short circuit that stuff to directly affect layers as they are composited,
 * for example, off-main thread animation, async video, async pan/zoom.
 */
class AsyncCompositionManager MOZ_FINAL
{
  friend class AutoResolveRefLayers;
  ~AsyncCompositionManager()
  {
  }
public:
  NS_INLINE_DECL_REFCOUNTING(AsyncCompositionManager)

  explicit AsyncCompositionManager(LayerManagerComposite* aManager)
    : mLayerManager(aManager)
    , mIsFirstPaint(false)
    , mLayersUpdated(false)
    , mReadyForCompose(true)
  {
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
  void TransformScrollableLayer(Layer* aLayer);
  // Return true if an AsyncPanZoomController content transform was
  // applied for |aLayer|.
  bool ApplyAsyncContentTransformToTree(Layer* aLayer);
  /**
   * Update the shadow transform for aLayer assuming that is a scrollbar,
   * so that it stays in sync with the content that is being scrolled by APZ.
   */
  void ApplyAsyncTransformToScrollbar(Layer* aLayer);

  void SetFirstPaintViewport(const LayerIntPoint& aOffset,
                             const CSSToLayerScale& aZoom,
                             const CSSRect& aCssPageRect);
  void SetPageRect(const CSSRect& aCssPageRect);
  void SyncViewportInfo(const LayerIntRect& aDisplayPort,
                        const CSSToLayerScale& aDisplayResolution,
                        bool aLayersUpdated,
                        ScreenPoint& aScrollOffset,
                        CSSToScreenScale& aScale,
                        LayerMargin& aFixedLayerMargins,
                        ScreenPoint& aOffset);
  void SyncFrameMetrics(const ScreenPoint& aScrollOffset,
                        float aZoom,
                        const CSSRect& aCssPageRect,
                        bool aLayersUpdated,
                        const CSSRect& aDisplayPort,
                        const CSSToLayerScale& aDisplayResolution,
                        bool aIsFirstPaint,
                        LayerMargin& aFixedLayerMargins,
                        ScreenPoint& aOffset);

  /**
   * Adds a translation to the transform of any fixed position (whose parent
   * layer is not fixed) or sticky position layer descendant of
   * aTransformedSubtreeRoot. The translation is chosen so that the layer's
   * anchor point relative to aTransformedSubtreeRoot's parent layer is the same
   * as it was when aTransformedSubtreeRoot's GetLocalTransform() was
   * aPreviousTransformForRoot. aCurrentTransformForRoot is
   * aTransformedSubtreeRoot's current GetLocalTransform() modulo any
   * overscroll-related transform, which we don't want to adjust for.
   * For sticky position layers, the translation is further intersected with
   * the layer's sticky scroll ranges.
   * This function will also adjust layers so that the given content document
   * fixed position margins will be respected during asynchronous panning and
   * zooming.
   */
  void AlignFixedAndStickyLayers(Layer* aLayer, Layer* aTransformedSubtreeRoot,
                                 FrameMetrics::ViewID aTransformScrollId,
                                 const gfx::Matrix4x4& aPreviousTransformForRoot,
                                 const gfx::Matrix4x4& aCurrentTransformForRoot,
                                 const LayerMargin& aFixedLayerMargins);

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

  gfx::Matrix mWorldTransform;
};

class MOZ_STACK_CLASS AutoResolveRefLayers {
public:
  explicit AutoResolveRefLayers(AsyncCompositionManager* aManager) : mManager(aManager)
  {
    if (mManager) {
      mManager->ResolveRefLayers();
    }
  }

  ~AutoResolveRefLayers()
  {
    if (mManager) {
      mManager->DetachRefLayers();
    }
  }

private:
  AsyncCompositionManager* mManager;

  AutoResolveRefLayers(const AutoResolveRefLayers&) MOZ_DELETE;
  AutoResolveRefLayers& operator=(const AutoResolveRefLayers&) MOZ_DELETE;
};

} // layers
} // mozilla

#endif
