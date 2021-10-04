/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_H
#define GFX_LAYERS_H

#include <cstdint>        // for uint32_t, uint64_t, int32_t, uint8_t
#include <cstring>        // for memcpy, size_t
#include <iosfwd>         // for stringstream
#include <new>            // for operator new
#include <unordered_set>  // for unordered_set
#include <utility>        // for forward, move
#include "FrameMetrics.h"  // for ScrollMetadata, FrameMetrics::ViewID, FrameMetrics
#include "Units.h"  // for LayerIntRegion, ParentLayerIntRect, LayerIntSize, Lay...
#include "gfxPoint.h"           // for gfxPoint
#include "gfxRect.h"            // for gfxRect
#include "mozilla/Maybe.h"      // for Maybe, Nothing (ptr only)
#include "mozilla/Poison.h"     // for CorruptionCanary
#include "mozilla/RefPtr.h"     // for RefPtr, operator!=
#include "mozilla/TimeStamp.h"  // for TimeStamp
#include "mozilla/UniquePtr.h"  // for UniquePtr, MakeUnique
#include "mozilla/gfx/BasePoint.h"  // for BasePoint<>::(anonymous union)::(anonymous), BasePoin...
#include "mozilla/gfx/BaseSize.h"     // for BaseSize
#include "mozilla/gfx/Matrix.h"       // for Matrix4x4, Matrix, Matrix4x4Typed
#include "mozilla/gfx/Point.h"        // for Point, PointTyped
#include "mozilla/gfx/Polygon.h"      // for Polygon
#include "mozilla/gfx/Rect.h"         // for IntRectTyped, IntRect
#include "mozilla/gfx/TiledRegion.h"  // for TiledIntRegion
#include "mozilla/gfx/Types.h"  // for CompositionOp, DeviceColor, SamplingFilter, SideBits
#include "mozilla/gfx/UserData.h"  // for UserData, UserDataKey (ptr only)
#include "mozilla/layers/AnimationInfo.h"  // for AnimationInfo
#include "mozilla/layers/LayerAttributes.h"  // for SimpleLayerAttributes, ScrollbarData (ptr only)
#include "mozilla/layers/LayerManager.h"  // for LayerManager
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid, ScrollableLayerGuid::ViewID
#include "mozilla/layers/BSPTree.h"
#include "nsISupports.h"    // for NS_INLINE_DECL_REFCOUNTING
#include "nsPoint.h"        // for nsIntPoint
#include "nsRect.h"         // for nsIntRect
#include "nsRegion.h"       // for nsIntRegion
#include "nsStringFlags.h"  // for operator&
#include "nsStringFwd.h"    // for nsCString, nsACString
#include "nsTArray.h"  // for nsTArray, nsTArray_Impl, nsTArray_Impl<>::elem_type

// XXX These includes could be avoided by moving function implementations to the
// cpp file
#include "gfx2DGlue.h"           // for ThebesPoint
#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT, MOZ_A...
#include "mozilla/DebugOnly.h"  // for DebugOnly
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG_IF_SHADOWABLE, LayersId, EventRegionsO...
#include "nsDebug.h"  // for NS_ASSERTION, NS_WARNING

namespace mozilla {

namespace gfx {
class DataSourceSurface;
class DrawTarget;
class Path;
}  // namespace gfx

namespace layers {

class Animation;
class AsyncPanZoomController;
class ContainerLayer;
class CompositorAnimations;
class SpecificLayerAttributes;
class Compositor;
class TransformData;
struct PropertyAnimationGroup;

#define MOZ_LAYER_DECL_NAME(n, e)                  \
  const char* Name() const override { return n; }  \
  LayerType GetType() const override { return e; } \
  static LayerType Type() { return e; }

/*
 * Motivation: For truly smooth animation and video playback, we need to
 * be able to compose frames and render them on a dedicated thread (i.e.
 * off the main thread where DOM manipulation, script execution and layout
 * induce difficult-to-bound latency). This requires Gecko to construct
 * some kind of persistent scene structure (graph or tree) that can be
 * safely transmitted across threads. We have other scenarios (e.g. mobile
 * browsing) where retaining some rendered data between paints is desired
 * for performance, so again we need a retained scene structure.
 *
 * Our retained scene structure is a layer tree. Each layer represents
 * content which can be composited onto a destination surface; the root
 * layer is usually composited into a window, and non-root layers are
 * composited into their parent layers. Layers have attributes (e.g.
 * opacity and clipping) that influence their compositing.
 *
 * We want to support a variety of layer implementations, including
 * a simple "immediate mode" implementation that doesn't retain any
 * rendered data between paints (i.e. uses cairo in just the way that
 * Gecko used it before layers were introduced). But we also don't want
 * to have bifurcated "layers"/"non-layers" rendering paths in Gecko.
 * Therefore the layers API is carefully designed to permit maximally
 * efficient implementation in an "immediate mode" style. See the
 * BasicLayerManager for such an implementation.
 */

/**
 * A Layer represents anything that can be rendered onto a destination
 * surface.
 */
class Layer {
  NS_INLINE_DECL_REFCOUNTING(Layer)

  using AnimationArray = nsTArray<layers::Animation>;

 public:
  // Keep these in alphabetical order
  enum LayerType {
    TYPE_CANVAS,
    TYPE_COLOR,
    TYPE_CONTAINER,
    TYPE_DISPLAYITEM,
    TYPE_IMAGE,
    TYPE_SHADOW,
    TYPE_PAINTED
  };

  /**
   * Returns the LayerManager this Layer belongs to. Note that the layer
   * manager might be in a destroyed state, at which point it's only
   * valid to set/get user data from it.
   */
  LayerManager* Manager() { return mManager; }

  enum {
    /**
     * If this is set, the caller is promising that by the end of this
     * transaction the entire visible region (as specified by
     * SetVisibleRegion) will be filled with opaque content.
     */
    CONTENT_OPAQUE = 0x01,
    /**
     * If this is set, the caller is notifying that the contents of this layer
     * require per-component alpha for optimal fidelity. However, there is no
     * guarantee that component alpha will be supported for this layer at
     * paint time.
     * This should never be set at the same time as CONTENT_OPAQUE.
     */
    CONTENT_COMPONENT_ALPHA = 0x02,

    /**
     * If this is set then this layer is part of a preserve-3d group, and should
     * be sorted with sibling layers that are also part of the same group.
     */
    CONTENT_EXTEND_3D_CONTEXT = 0x08,

    /**
     * Disable subpixel AA for this layer. This is used if the display isn't
     * suited for subpixel AA like hidpi or rotated content.
     */
    CONTENT_DISABLE_SUBPIXEL_AA = 0x20,

    /**
     * If this is set then the layer contains content that may look
     * objectionable if not handled as an active layer (such as text with an
     * animated transform). This is for internal layout/FrameLayerBuilder usage
     * only until flattening code is obsoleted. See bug 633097
     */
    CONTENT_DISABLE_FLATTENING = 0x40,

    /**
     * This layer is hidden if the backface of the layer is visible
     * to user.
     */
    CONTENT_BACKFACE_HIDDEN = 0x80,

    /**
     * This layer should be snapped to the pixel grid.
     */
    CONTENT_SNAP_TO_GRID = 0x100
  };
  /**
   * CONSTRUCTION PHASE ONLY
   * This lets layout make some promises about what will be drawn into the
   * visible region of the PaintedLayer. This enables internal quality
   * and performance optimizations.
   */
  void SetContentFlags(uint32_t aFlags) {
    NS_ASSERTION((aFlags & (CONTENT_OPAQUE | CONTENT_COMPONENT_ALPHA)) !=
                     (CONTENT_OPAQUE | CONTENT_COMPONENT_ALPHA),
                 "Can't be opaque and require component alpha");
    if (mSimpleAttrs.SetContentFlags(aFlags)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this,
                                   ("Layer::Mutated(%p) ContentFlags", this));
      MutatedSimple();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Tell this layer which region will be visible. The visible region
   * is a region which contains all the contents of the layer that can
   * actually affect the rendering of the window. It can exclude areas
   * that are covered by opaque contents of other layers, and it can
   * exclude areas where this layer simply contains no content at all.
   * (This can be an overapproximation to the "true" visible region.)
   *
   * There is no general guarantee that drawing outside the bounds of the
   * visible region will be ignored. So if a layer draws outside the bounds
   * of its visible region, it needs to ensure that what it draws is valid.
   */
  virtual void SetVisibleRegion(const LayerIntRegion& aRegion) {
    // IsEmpty is required otherwise we get invalidation glitches.
    // See bug 1288464 for investigating why.
    if (!mVisibleRegion.IsEqual(aRegion) || aRegion.IsEmpty()) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(
          this, ("Layer::Mutated(%p) VisibleRegion was %s is %s", this,
                 mVisibleRegion.ToString().get(), aRegion.ToString().get()));
      mVisibleRegion = aRegion;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the (sub)document metrics used to render the Layer subtree
   * rooted at this. Note that a layer may have multiple FrameMetrics
   * objects; calling this function will remove all of them and replace
   * them with the provided FrameMetrics. See the documentation for
   * SetFrameMetrics(const nsTArray<FrameMetrics>&) for more details.
   */
  void SetScrollMetadata(const ScrollMetadata& aScrollMetadata) {
    Manager()->ClearPendingScrollInfoUpdate();
    if (mScrollMetadata.Length() != 1 ||
        mScrollMetadata[0] != aScrollMetadata) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this,
                                   ("Layer::Mutated(%p) ScrollMetadata", this));
      mScrollMetadata.ReplaceElementsAt(0, mScrollMetadata.Length(),
                                        aScrollMetadata);
      ScrollMetadataChanged();
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the (sub)document metrics used to render the Layer subtree
   * rooted at this. There might be multiple metrics on this layer
   * because the layer may, for example, be contained inside multiple
   * nested scrolling subdocuments. In general a Layer having multiple
   * ScrollMetadata objects is conceptually equivalent to having a stack
   * of ContainerLayers that have been flattened into this Layer.
   * See the documentation in LayerMetricsWrapper.h for a more detailed
   * explanation of this conceptual equivalence.
   *
   * Note also that there is actually a many-to-many relationship between
   * Layers and ScrollMetadata, because multiple Layers may have identical
   * ScrollMetadata objects. This happens when those layers belong to the
   * same scrolling subdocument and therefore end up with the same async
   * transform when they are scrolled by the APZ code.
   */
  void SetScrollMetadata(const nsTArray<ScrollMetadata>& aMetadataArray) {
    Manager()->ClearPendingScrollInfoUpdate();
    if (mScrollMetadata != aMetadataArray) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this,
                                   ("Layer::Mutated(%p) ScrollMetadata", this));
      mScrollMetadata = aMetadataArray.Clone();
      ScrollMetadataChanged();
      Mutated();
    }
  }

  /*
   * Compositor event handling
   * =========================
   * When a touch-start event (or similar) is sent to the
   * AsyncPanZoomController, it needs to decide whether the event should be sent
   * to the main thread. Each layer has a list of event handling regions. When
   * the compositor needs to determine how to handle a touch event, it scans the
   * layer tree from top to bottom in z-order (traversing children before their
   * parents). Points outside the clip region for a layer cause that layer (and
   * its subtree) to be ignored. If a layer has a mask layer, and that mask
   * layer's alpha value is zero at the event point, then the layer and its
   * subtree should be ignored. For each layer, if the point is outside its hit
   * region, we ignore the layer and move onto the next. If the point is inside
   * its hit region but outside the dispatch-to-content region, we can initiate
   * a gesture without consulting the content thread. Otherwise we must dispatch
   * the event to content. Note that if a layer or any ancestor layer has a
   * ForceEmptyHitRegion override in GetEventRegionsOverride() then the
   * hit-region must be treated as empty. Similarly, if there is a
   * ForceDispatchToContent override then the dispatch-to-content region must be
   * treated as encompassing the entire hit region, and therefore we must
   * consult the content thread before initiating a gesture. (If both flags are
   * set, ForceEmptyHitRegion takes priority.)
   */
  /**
   * CONSTRUCTION PHASE ONLY
   * Set the event handling region.
   */
  void SetEventRegions(const EventRegions& aRegions);

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the opacity which will be applied to this layer as it
   * is composited to the destination.
   */
  void SetOpacity(float aOpacity) {
    if (mSimpleAttrs.SetOpacity(aOpacity)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) Opacity", this));
      MutatedSimple();
    }
  }

  void SetMixBlendMode(gfx::CompositionOp aMixBlendMode) {
    if (mSimpleAttrs.SetMixBlendMode(aMixBlendMode)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this,
                                   ("Layer::Mutated(%p) MixBlendMode", this));
      MutatedSimple();
    }
  }

  void SetForceIsolatedGroup(bool aForceIsolatedGroup) {
    if (mSimpleAttrs.SetForceIsolatedGroup(aForceIsolatedGroup)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(
          this, ("Layer::Mutated(%p) ForceIsolatedGroup", this));
      MutatedSimple();
    }
  }

  bool GetForceIsolatedGroup() const {
    return mSimpleAttrs.GetForceIsolatedGroup();
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set a clip rect which will be applied to this layer as it is
   * composited to the destination. The coordinates are relative to
   * the parent layer (i.e. the contents of this layer
   * are transformed before this clip rect is applied).
   * For the root layer, the coordinates are relative to the widget,
   * in device pixels.
   * If aRect is null no clipping will be performed.
   */
  void SetClipRect(const Maybe<ParentLayerIntRect>& aRect) {
    if (mClipRect) {
      if (!aRect) {
        MOZ_LAYERS_LOG_IF_SHADOWABLE(
            this, ("Layer::Mutated(%p) ClipRect was %d,%d,%d,%d is <none>",
                   this, mClipRect->X(), mClipRect->Y(), mClipRect->Width(),
                   mClipRect->Height()));
        mClipRect.reset();
        Mutated();
      } else {
        if (!aRect->IsEqualEdges(*mClipRect)) {
          MOZ_LAYERS_LOG_IF_SHADOWABLE(
              this,
              ("Layer::Mutated(%p) ClipRect was %d,%d,%d,%d is %d,%d,%d,%d",
               this, mClipRect->X(), mClipRect->Y(), mClipRect->Width(),
               mClipRect->Height(), aRect->X(), aRect->Y(), aRect->Width(),
               aRect->Height()));
          mClipRect = aRect;
          Mutated();
        }
      }
    } else {
      if (aRect) {
        MOZ_LAYERS_LOG_IF_SHADOWABLE(
            this,
            ("Layer::Mutated(%p) ClipRect was <none> is %d,%d,%d,%d", this,
             aRect->X(), aRect->Y(), aRect->Width(), aRect->Height()));
        mClipRect = aRect;
        Mutated();
      }
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set a layer to mask this layer.
   *
   * The mask layer should be applied using its effective transform (after it
   * is calculated by ComputeEffectiveTransformForMaskLayer), this should use
   * this layer's parent's transform and the mask layer's transform, but not
   * this layer's. That is, the mask layer is specified relative to this layer's
   * position in it's parent layer's coord space.
   * Currently, only 2D translations are supported for the mask layer transform.
   *
   * Ownership of aMaskLayer passes to this.
   * Typical use would be an ImageLayer with an alpha image used for masking.
   * See also ContainerState::BuildMaskLayer in FrameLayerBuilder.cpp.
   */
  void SetMaskLayer(Layer* aMaskLayer) {
#ifdef DEBUG
    if (aMaskLayer) {
      bool maskIs2D = aMaskLayer->GetTransform().CanDraw2D();
      NS_ASSERTION(maskIs2D, "Mask layer has invalid transform.");
    }
#endif

    if (mMaskLayer != aMaskLayer) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this,
                                   ("Layer::Mutated(%p) MaskLayer", this));
      mMaskLayer = aMaskLayer;
      Mutated();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Tell this layer what its transform should be. The transformation
   * is applied when compositing the layer into its parent container.
   */
  void SetBaseTransform(const gfx::Matrix4x4& aMatrix) {
    NS_ASSERTION(!aMatrix.IsSingular(),
                 "Shouldn't be trying to draw with a singular matrix!");
    mPendingTransform = nullptr;
    if (!mSimpleAttrs.SetTransform(aMatrix)) {
      return;
    }
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this,
                                 ("Layer::Mutated(%p) BaseTransform", this));
    MutatedSimple();
  }

  /**
   * Can be called at any time.
   *
   * Like SetBaseTransform(), but can be called before the next
   * transform (i.e. outside an open transaction).  Semantically, this
   * method enqueues a new transform value to be set immediately after
   * the next transaction is opened.
   */
  void SetBaseTransformForNextTransaction(const gfx::Matrix4x4& aMatrix) {
    mPendingTransform = mozilla::MakeUnique<gfx::Matrix4x4>(aMatrix);
  }

  void SetPostScale(float aXScale, float aYScale) {
    if (!mSimpleAttrs.SetPostScale(aXScale, aYScale)) {
      return;
    }
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) PostScale", this));
    MutatedSimple();
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * A layer is "fixed position" when it draws content from a content
   * (not chrome) document, the topmost content document has a root scrollframe
   * with a displayport, but the layer does not move when that displayport
   * scrolls.
   */
  void SetIsFixedPosition(bool aFixedPosition) {
    if (mSimpleAttrs.SetIsFixedPosition(aFixedPosition)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(
          this, ("Layer::Mutated(%p) IsFixedPosition", this));
      MutatedSimple();
    }
  }

  void SetAsyncZoomContainerId(const Maybe<FrameMetrics::ViewID>& aViewId) {
    if (mSimpleAttrs.SetAsyncZoomContainerId(aViewId)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(
          this, ("Layer::Mutated(%p) AsyncZoomContainerId", this));
      MutatedSimple();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * This flag is true when the transform on the layer is a perspective
   * transform. The compositor treats perspective transforms specially
   * for async scrolling purposes.
   */
  void SetTransformIsPerspective(bool aTransformIsPerspective) {
    if (mSimpleAttrs.SetTransformIsPerspective(aTransformIsPerspective)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(
          this, ("Layer::Mutated(%p) TransformIsPerspective", this));
      MutatedSimple();
    }
  }
  // This is only called when the layer tree is updated. Do not call this from
  // layout code.  To add an animation to this layer, use AddAnimation.
  void SetCompositorAnimations(
      const LayersId& aLayersId,
      const CompositorAnimations& aCompositorAnimations);
  // Go through all animations in this layer and its children and, for
  // any animations with a null start time, update their start time such
  // that at |aReadyTime| the animation's current time corresponds to its
  // 'initial current time' value.
  void StartPendingAnimations(const TimeStamp& aReadyTime);

  void ClearCompositorAnimations();

  /**
   * CONSTRUCTION PHASE ONLY
   * If a layer represents a fixed position element, this data is stored on the
   * layer for use by the compositor.
   *
   *   - |aScrollId| identifies the scroll frame that this element is fixed
   *     with respect to.
   *
   *   - |aAnchor| is the point on the layer that is considered the "anchor"
   *     point, that is, the point which remains in the same position when
   *     compositing the layer tree with a transformation (such as when
   *     asynchronously scrolling and zooming).
   *
   *   - |aSides| is the set of sides to which the element is fixed relative to.
   *     This is used if the viewport size is changed in the compositor and
   *     fixed position items need to shift accordingly. This value is made up
   *     combining appropriate values from mozilla::SideBits.
   */
  void SetFixedPositionData(ScrollableLayerGuid::ViewID aScrollId,
                            const LayerPoint& aAnchor, SideBits aSides) {
    if (mSimpleAttrs.SetFixedPositionData(aScrollId, aAnchor, aSides)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(
          this, ("Layer::Mutated(%p) FixedPositionData", this));
      MutatedSimple();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * If a layer is "sticky position", |aScrollId| holds the scroll identifier
   * of the scrollable content that contains it. The difference between the two
   * rectangles |aOuter| and |aInner| is treated as two intervals in each
   * dimension, with the current scroll position at the origin. For each
   * dimension, while that component of the scroll position lies within either
   * interval, the layer should not move relative to its scrolling container.
   */
  void SetStickyPositionData(ScrollableLayerGuid::ViewID aScrollId,
                             LayerRectAbsolute aOuter,
                             LayerRectAbsolute aInner) {
    if (mSimpleAttrs.SetStickyPositionData(aScrollId, aOuter, aInner)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(
          this, ("Layer::Mutated(%p) StickyPositionData", this));
      MutatedSimple();
    }
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * If a layer is a scroll thumb container layer or a scrollbar container
   * layer, set the scroll identifier of the scroll frame scrolled by
   * the scrollbar, and other data related to the scrollbar.
   */
  void SetScrollbarData(const ScrollbarData& aThumbData) {
    if (mSimpleAttrs.SetScrollbarData(aThumbData)) {
      MOZ_LAYERS_LOG_IF_SHADOWABLE(this,
                                   ("Layer::Mutated(%p) ScrollbarData", this));
      MutatedSimple();
    }
  }

  // Used when forwarding transactions. Do not use at any other time.
  void SetSimpleAttributes(const SimpleLayerAttributes& aAttrs) {
    mSimpleAttrs = aAttrs;
  }
  const SimpleLayerAttributes& GetSimpleAttributes() const {
    return mSimpleAttrs;
  }

  // These getters can be used anytime.
  float GetOpacity() { return mSimpleAttrs.GetOpacity(); }
  gfx::CompositionOp GetMixBlendMode() const {
    return mSimpleAttrs.GetMixBlendMode();
  }
  const Maybe<ParentLayerIntRect>& GetClipRect() const { return mClipRect; }
  uint32_t GetContentFlags() { return mSimpleAttrs.GetContentFlags(); }
  const LayerIntRegion& GetVisibleRegion() const { return mVisibleRegion; }
  const ScrollMetadata& GetScrollMetadata(uint32_t aIndex) const;
  const FrameMetrics& GetFrameMetrics(uint32_t aIndex) const;
  uint32_t GetScrollMetadataCount() const { return mScrollMetadata.Length(); }
  const nsTArray<ScrollMetadata>& GetAllScrollMetadata() {
    return mScrollMetadata;
  }
  bool HasScrollableFrameMetrics() const;
  bool IsScrollableWithoutContent() const;
  const EventRegions& GetEventRegions() const { return mEventRegions; }
  ContainerLayer* GetParent() const { return mParent; }
  Layer* GetNextSibling() {
    if (mNextSibling) {
      mNextSibling->CheckCanary();
    }
    return mNextSibling;
  }
  const Layer* GetNextSibling() const {
    if (mNextSibling) {
      mNextSibling->CheckCanary();
    }
    return mNextSibling;
  }
  Layer* GetPrevSibling() { return mPrevSibling; }
  const Layer* GetPrevSibling() const { return mPrevSibling; }
  virtual Layer* GetFirstChild() const { return nullptr; }
  virtual Layer* GetLastChild() const { return nullptr; }
  gfx::Matrix4x4 GetTransform() const;
  // Same as GetTransform(), but returns the transform as a strongly-typed
  // matrix. Eventually this will replace GetTransform().
  const CSSTransformMatrix GetTransformTyped() const;
  const gfx::Matrix4x4& GetBaseTransform() const {
    return mSimpleAttrs.GetTransform();
  }
  // Note: these are virtual because ContainerLayerComposite overrides them.
  virtual float GetPostXScale() const { return mSimpleAttrs.GetPostXScale(); }
  virtual float GetPostYScale() const { return mSimpleAttrs.GetPostYScale(); }
  bool GetIsFixedPosition() { return mSimpleAttrs.IsFixedPosition(); }
  Maybe<FrameMetrics::ViewID> GetAsyncZoomContainerId() {
    return mSimpleAttrs.GetAsyncZoomContainerId();
  }
  bool GetTransformIsPerspective() const {
    return mSimpleAttrs.GetTransformIsPerspective();
  }
  bool GetIsStickyPosition() { return mSimpleAttrs.IsStickyPosition(); }
  ScrollableLayerGuid::ViewID GetFixedPositionScrollContainerId() {
    return mSimpleAttrs.GetFixedPositionScrollContainerId();
  }
  LayerPoint GetFixedPositionAnchor() {
    return mSimpleAttrs.GetFixedPositionAnchor();
  }
  SideBits GetFixedPositionSides() {
    return mSimpleAttrs.GetFixedPositionSides();
  }
  ScrollableLayerGuid::ViewID GetStickyScrollContainerId() {
    return mSimpleAttrs.GetStickyScrollContainerId();
  }
  const LayerRectAbsolute& GetStickyScrollRangeOuter() {
    return mSimpleAttrs.GetStickyScrollRangeOuter();
  }
  const LayerRectAbsolute& GetStickyScrollRangeInner() {
    return mSimpleAttrs.GetStickyScrollRangeInner();
  }
  const ScrollbarData& GetScrollbarData() const {
    return mSimpleAttrs.GetScrollbarData();
  }
  bool IsScrollbarContainer() const;
  Layer* GetMaskLayer() const { return mMaskLayer; }
  bool HasPendingTransform() const { return !!mPendingTransform; }

  void CheckCanary() const { mCanary.Check(); }

  /**
   * Retrieve the root level visible region for |this| taking into account
   * clipping applied to parent layers of |this| as well as subtracting
   * visible regions of higher siblings of this layer and each ancestor.
   *
   * Note translation values for offsets of visible regions and accumulated
   * aLayerOffset are integer rounded using IntPoint::Round.
   *
   * @param aResult - the resulting visible region of this layer.
   * @param aLayerOffset - this layer's total offset from the root layer.
   * @return - false if during layer tree traversal a parent or sibling
   *  transform is found to be non-translational. This method returns early
   *  in this case, results will not be valid. Returns true on successful
   *  traversal.
   */
  bool GetVisibleRegionRelativeToRootLayer(nsIntRegion& aResult,
                                           nsIntPoint* aLayerOffset);

  // Note that all lengths in animation data are either in CSS pixels or app
  // units and must be converted to device pixels by the compositor.
  // Besides, this should only be called on the compositor thread.
  AnimationArray& GetAnimations() { return mAnimationInfo.GetAnimations(); }
  uint64_t GetCompositorAnimationsId() {
    return mAnimationInfo.GetCompositorAnimationsId();
  }
  nsTArray<PropertyAnimationGroup>& GetPropertyAnimationGroups() {
    return mAnimationInfo.GetPropertyAnimationGroups();
  }
  const Maybe<TransformData>& GetTransformData() const {
    return mAnimationInfo.GetTransformData();
  }
  const LayersId& GetAnimationLayersId() const {
    return mAnimationInfo.GetLayersId();
  }

  Maybe<uint64_t> GetAnimationGeneration() const {
    return mAnimationInfo.GetAnimationGeneration();
  }

  gfx::Path* CachedMotionPath() { return mAnimationInfo.CachedMotionPath(); }

  bool HasTransformAnimation() const;

  /**
   * Returns the local transform for this layer: either mTransform or,
   * for shadow layers, GetShadowBaseTransform(), in either case with the
   * pre- and post-scales applied.
   */
  gfx::Matrix4x4 GetLocalTransform();

  /**
   * Same as GetLocalTransform(), but returns a strongly-typed matrix.
   * Eventually, this will replace GetLocalTransform().
   */
  const LayerToParentLayerMatrix4x4 GetLocalTransformTyped();

  /**
   * Returns the local opacity for this layer: either mOpacity or,
   * for shadow layers, GetShadowOpacity()
   */
  float GetLocalOpacity();

  /**
   * DRAWING PHASE ONLY
   *
   * Apply pending changes to layers before drawing them, if those
   * pending changes haven't been overridden by later changes.
   *
   * Returns a list of scroll ids which had pending updates.
   */
  std::unordered_set<ScrollableLayerGuid::ViewID>
  ApplyPendingUpdatesToSubtree();

  // Returns true if it's OK to save the contents of aLayer in an
  // opaque surface (a surface without an alpha channel).
  // If we can use a surface without an alpha channel, we should, because
  // it will often make painting of antialiased text faster and higher
  // quality.
  bool CanUseOpaqueSurface();

  SurfaceMode GetSurfaceMode() {
    if (CanUseOpaqueSurface()) return SurfaceMode::SURFACE_OPAQUE;
    if (GetContentFlags() & CONTENT_COMPONENT_ALPHA)
      return SurfaceMode::SURFACE_COMPONENT_ALPHA;
    return SurfaceMode::SURFACE_SINGLE_CHANNEL_ALPHA;
  }

  // Returns true if this layer can be treated as opaque for visibility
  // computation. A layer may be non-opaque for visibility even if it
  // is not transparent, for example, if it has a mix-blend-mode.
  bool IsOpaqueForVisibility();

  /**
   * This setter can be used anytime. The user data for all keys is
   * initially null. Ownership pases to the layer manager.
   */
  void SetUserData(
      void* aKey, LayerUserData* aData,
      void (*aDestroy)(void*) = LayerManager::LayerUserDataDestroy) {
    mUserData.Add(static_cast<gfx::UserDataKey*>(aKey), aData, aDestroy);
  }
  /**
   * This can be used anytime. Ownership passes to the caller!
   */
  UniquePtr<LayerUserData> RemoveUserData(void* aKey);
  /**
   * This getter can be used anytime.
   */
  bool HasUserData(void* aKey) {
    return mUserData.Has(static_cast<gfx::UserDataKey*>(aKey));
  }
  /**
   * This getter can be used anytime. Ownership is retained by the layer
   * manager.
   */
  LayerUserData* GetUserData(void* aKey) const {
    return static_cast<LayerUserData*>(
        mUserData.Get(static_cast<gfx::UserDataKey*>(aKey)));
  }

  /**
   * |Disconnect()| is used by layers hooked up over IPC.  It may be
   * called at any time, and may not be called at all.  Using an
   * IPC-enabled layer after Destroy() (drawing etc.) results in a
   * safe no-op; no crashy or uaf etc.
   *
   * XXX: this interface is essentially LayerManager::Destroy, but at
   * Layer granularity.  It might be beneficial to unify them.
   */
  virtual void Disconnect() {}

  /**
   * Dynamic cast to a ContainerLayer. Returns null if this is not
   * a ContainerLayer.
   */
  virtual ContainerLayer* AsContainerLayer() { return nullptr; }
  virtual const ContainerLayer* AsContainerLayer() const { return nullptr; }

  // These getters can be used anytime.  They return the effective
  // values that should be used when drawing this layer to screen,
  // accounting for this layer possibly being a shadow.
  const Maybe<ParentLayerIntRect>& GetLocalClipRect();
  const LayerIntRegion& GetLocalVisibleRegion();

  bool Extend3DContext() {
    return GetContentFlags() & CONTENT_EXTEND_3D_CONTEXT;
  }
  bool Combines3DTransformWithAncestors() {
    return GetParent() &&
           reinterpret_cast<Layer*>(GetParent())->Extend3DContext();
  }
  bool Is3DContextLeaf() {
    return !Extend3DContext() && Combines3DTransformWithAncestors();
  }
  /**
   * It is true if the user can see the back of the layer and the
   * backface is hidden.  The compositor should skip the layer if the
   * result is true.
   */
  bool IsBackfaceHidden();
  bool IsVisible() {
    // For containers extending 3D context, visible region
    // is meaningless, since they are just intermediate result of
    // content.
    return !GetLocalVisibleRegion().IsEmpty() || Extend3DContext();
  }

  /**
   * Return true if current layer content is opaque.
   * It does not guarantee that layer content is always opaque.
   */
  virtual bool IsOpaque() { return GetContentFlags() & CONTENT_OPAQUE; }

  /**
   * Returns the product of the opacities of this layer and all ancestors up
   * to and excluding the nearest ancestor that has UseIntermediateSurface()
   * set.
   */
  float GetEffectiveOpacity();

  /**
   * Returns the blendmode of this layer.
   */
  gfx::CompositionOp GetEffectiveMixBlendMode();

  /**
   * This returns the effective transform computed by
   * ComputeEffectiveTransforms. Typically this is a transform that transforms
   * this layer all the way to some intermediate surface or destination
   * surface. For non-BasicLayers this will be a transform to the nearest
   * ancestor with UseIntermediateSurface() (or to the root, if there is no
   * such ancestor), but for BasicLayers it's different.
   */
  const gfx::Matrix4x4& GetEffectiveTransform() const {
    return mEffectiveTransform;
  }

  /**
   * This returns the effective transform for Layer's buffer computed by
   * ComputeEffectiveTransforms. Typically this is a transform that transforms
   * this layer's buffer all the way to some intermediate surface or destination
   * surface. For non-BasicLayers this will be a transform to the nearest
   * ancestor with UseIntermediateSurface() (or to the root, if there is no
   * such ancestor), but for BasicLayers it's different.
   *
   * By default, its value is same to GetEffectiveTransform().
   * When ImageLayer is rendered with ScaleMode::STRETCH,
   * it becomes different from GetEffectiveTransform().
   */
  virtual const gfx::Matrix4x4& GetEffectiveTransformForBuffer() const {
    return mEffectiveTransform;
  }

  /**
   * If the current layers participates in a preserve-3d
   * context (returns true for Combines3DTransformWithAncestors),
   * returns the combined transform up to the preserve-3d (nearest
   * ancestor that doesn't Extend3DContext()). Otherwise returns
   * the local transform.
   */
  gfx::Matrix4x4 ComputeTransformToPreserve3DRoot();

  /**
   * @param aTransformToSurface the composition of the transforms
   * from the parent layer (if any) to the destination pixel grid.
   *
   * Computes mEffectiveTransform for this layer and all its descendants.
   * mEffectiveTransform transforms this layer up to the destination
   * pixel grid (whatever aTransformToSurface is relative to).
   *
   * We promise that when this is called on a layer, all ancestor layers
   * have already had ComputeEffectiveTransforms called.
   */
  virtual void ComputeEffectiveTransforms(
      const gfx::Matrix4x4& aTransformToSurface) = 0;

  /**
   * Computes the effective transform for mask layers, if this layer has any.
   */
  void ComputeEffectiveTransformForMaskLayers(
      const gfx::Matrix4x4& aTransformToSurface);
  static void ComputeEffectiveTransformForMaskLayer(
      Layer* aMaskLayer, const gfx::Matrix4x4& aTransformToSurface);

  /**
   * Calculate the scissor rect required when rendering this layer.
   * Returns a rectangle relative to the intermediate surface belonging to the
   * nearest ancestor that has an intermediate surface, or relative to the root
   * viewport if no ancestor has an intermediate surface, corresponding to the
   * clip rect for this layer intersected with aCurrentScissorRect.
   */
  RenderTargetIntRect CalculateScissorRect(
      const RenderTargetIntRect& aCurrentScissorRect);

  virtual const char* Name() const = 0;
  virtual LayerType GetType() const = 0;

  /**
   * Only the implementation should call this. This is per-implementation
   * private data. Normally, all layers with a given layer manager
   * use the same type of ImplData.
   */
  void* ImplData() { return mImplData; }

  /**
   * Only the implementation should use these methods.
   */
  void SetParent(ContainerLayer* aParent) { mParent = aParent; }
  void SetNextSibling(Layer* aSibling) { mNextSibling = aSibling; }
  void SetPrevSibling(Layer* aSibling) { mPrevSibling = aSibling; }

  /**
   * Log information about this layer manager and its managed tree to
   * the NSPR log (if enabled for "Layers").
   */
  void Log(const char* aPrefix = "");
  /**
   * Log information about just this layer manager itself to the NSPR
   * log (if enabled for "Layers").
   */
  void LogSelf(const char* aPrefix = "");

  // Print interesting information about this into aStream. Internally
  // used to implement Dump*() and Log*(). If subclasses have
  // additional interesting properties, they should override this with
  // an implementation that first calls the base implementation then
  // appends additional info to aTo.
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  /**
   * Store display list log.
   */
  void SetDisplayListLog(const char* log);

  /**
   * Return display list log.
   */
  void GetDisplayListLog(nsCString& log);

  static bool IsLogEnabled() { return LayerManager::IsLogEnabled(); }

  /**
   * Returns the current area of the layer (in layer-space coordinates)
   * marked as needed to be recomposited.
   */
  const virtual gfx::TiledIntRegion& GetInvalidRegion() {
    return mInvalidRegion;
  }
  void AddInvalidRegion(const nsIntRegion& aRegion) {
    mInvalidRegion.Add(aRegion);
  }

  /**
   * Mark the entirety of the layer's visible region as being invalid.
   */
  void SetInvalidRectToVisibleRegion() {
    mInvalidRegion.SetEmpty();
    mInvalidRegion.Add(GetVisibleRegion().ToUnknownRegion());
  }

  /**
   * Adds to the current invalid rect.
   */
  void AddInvalidRect(const gfx::IntRect& aRect) { mInvalidRegion.Add(aRect); }

  /**
   * Clear the invalid rect, marking the layer as being identical to what is
   * currently composited.
   */
  virtual void ClearInvalidRegion() { mInvalidRegion.SetEmpty(); }

  // These functions allow attaching an AsyncPanZoomController to this layer,
  // and can be used anytime.
  // A layer has an APZC at index aIndex only-if
  // GetFrameMetrics(aIndex).IsScrollable(); attempting to get an APZC for a
  // non-scrollable metrics will return null. The reverse is also generally true
  // (that if GetFrameMetrics(aIndex).IsScrollable() is true, then the layer
  // will have an APZC). However, it only holds on the the compositor-side layer
  // tree, and only after the APZ code has had a chance to rebuild its internal
  // hit-testing tree using the layer tree. Also, it may not hold in certain
  // "exceptional" scenarios such as if the layer tree doesn't have a
  // GeckoContentController registered for it, or if there is a malicious
  // content process trying to trip up the compositor over IPC. The aIndex for
  // these functions must be less than GetScrollMetadataCount().
  void SetAsyncPanZoomController(uint32_t aIndex,
                                 AsyncPanZoomController* controller);
  AsyncPanZoomController* GetAsyncPanZoomController(uint32_t aIndex) const;
  // The ScrollMetadataChanged function is used internally to ensure the APZC
  // array length matches the frame metrics array length.

  virtual void ClearCachedResources() {}

  virtual bool SupportsAsyncUpdate() { return false; }

 private:
  void ScrollMetadataChanged();

 public:
  void ApplyPendingUpdatesForThisTransaction();

#ifdef DEBUG
  void SetDebugColorIndex(uint32_t aIndex) { mDebugColorIndex = aIndex; }
  uint32_t GetDebugColorIndex() { return mDebugColorIndex; }
#endif

  void Mutated() { mManager->Mutated(this); }
  void MutatedSimple() { mManager->MutatedSimple(this); }

  virtual int32_t GetMaxLayerSize() { return Manager()->GetMaxTextureSize(); }

  RenderTargetRect TransformRectToRenderTarget(const LayerIntRect& aRect);

  /**
   * Add debugging information to the layer dump.
   */
  void AddExtraDumpInfo(const nsACString& aStr) {
#ifdef MOZ_DUMP_PAINTING
    mExtraDumpInfo.AppendElement(aStr);
#endif
  }

  /**
   * Clear debugging information. Useful for recycling.
   */
  void ClearExtraDumpInfo() {
#ifdef MOZ_DUMP_PAINTING
    mExtraDumpInfo.Clear();
#endif
  }

  AnimationInfo& GetAnimationInfo() { return mAnimationInfo; }

 protected:
  Layer(LayerManager* aManager, void* aImplData);

  // Protected destructor, to discourage deletion outside of Release():
  virtual ~Layer();

  /**
   * We can snap layer transforms for two reasons:
   * 1) To avoid unnecessary resampling when a transform is a translation
   * by a non-integer number of pixels.
   * Snapping the translation to an integer number of pixels avoids
   * blurring the layer and can be faster to composite.
   * 2) When a layer is used to render a rectangular object, we need to
   * emulate the rendering of rectangular inactive content and snap the
   * edges of the rectangle to pixel boundaries. This is both to ensure
   * layer rendering is consistent with inactive content rendering, and to
   * avoid seams.
   * This function implements type 1 snapping. If aTransform is a 2D
   * translation, and this layer's layer manager has enabled snapping
   * (which is the default), return aTransform with the translation snapped
   * to nearest pixels. Otherwise just return aTransform. Call this when the
   * layer does not correspond to a single rectangular content object.
   * This function does not try to snap if aTransform has a scale, because in
   * that case resampling is inevitable and there's no point in trying to
   * avoid it. In fact snapping can cause problems because pixel edges in the
   * layer's content can be rendered unpredictably (jiggling) as the scale
   * interacts with the snapping of the translation, especially with animated
   * transforms.
   * @param aResidualTransform a transform to apply before the result transform
   * in order to get the results to completely match aTransform.
   */
  gfx::Matrix4x4 SnapTransformTranslation(const gfx::Matrix4x4& aTransform,
                                          gfx::Matrix* aResidualTransform);
  gfx::Matrix4x4 SnapTransformTranslation3D(const gfx::Matrix4x4& aTransform,
                                            gfx::Matrix* aResidualTransform);
  /**
   * See comment for SnapTransformTranslation.
   * This function implements type 2 snapping. If aTransform is a translation
   * and/or scale, transform aSnapRect by aTransform, snap to pixel boundaries,
   * and return the transform that maps aSnapRect to that rect. Otherwise
   * just return aTransform.
   * @param aSnapRect a rectangle whose edges should be snapped to pixel
   * boundaries in the destination surface.
   * @param aResidualTransform a transform to apply before the result transform
   * in order to get the results to completely match aTransform.
   */
  gfx::Matrix4x4 SnapTransform(const gfx::Matrix4x4& aTransform,
                               const gfxRect& aSnapRect,
                               gfx::Matrix* aResidualTransform);

  LayerManager* mManager;
  ContainerLayer* mParent;
  Layer* mNextSibling;
  Layer* mPrevSibling;
  void* mImplData;
  RefPtr<Layer> mMaskLayer;
  nsTArray<RefPtr<Layer>> mAncestorMaskLayers;
  // Look for out-of-bound in the middle of the structure
  mozilla::CorruptionCanary mCanary;
  gfx::UserData mUserData;
  SimpleLayerAttributes mSimpleAttrs;
  LayerIntRegion mVisibleRegion;
  nsTArray<ScrollMetadata> mScrollMetadata;
  EventRegions mEventRegions;
  // A mutation of |mTransform| that we've queued to be applied at the
  // end of the next transaction (if nothing else overrides it in the
  // meantime).
  UniquePtr<gfx::Matrix4x4> mPendingTransform;
  gfx::Matrix4x4 mEffectiveTransform;
  AnimationInfo mAnimationInfo;
  Maybe<ParentLayerIntRect> mClipRect;
  gfx::IntRect mTileSourceRect;
  gfx::TiledIntRegion mInvalidRegion;
  nsTArray<RefPtr<AsyncPanZoomController>> mApzcs;
  bool mUseTileSourceRect;
#ifdef DEBUG
  uint32_t mDebugColorIndex;
#endif
#ifdef MOZ_DUMP_PAINTING
  nsTArray<nsCString> mExtraDumpInfo;
#endif
  // Store display list log.
  nsCString mDisplayListLog;
};

/**
 * A Layer which other layers render into. It holds references to its
 * children.
 */
class ContainerLayer : public Layer {
 public:
  virtual ~ContainerLayer();

  /**
   * CONSTRUCTION PHASE ONLY
   * Insert aChild into the child list of this container. aChild must
   * not be currently in any child list or the root for the layer manager.
   * If aAfter is non-null, it must be a child of this container and
   * we insert after that layer. If it's null we insert at the start.
   */
  virtual bool InsertAfter(Layer* aChild, Layer* aAfter);
  /**
   * CONSTRUCTION PHASE ONLY
   * Remove aChild from the child list of this container. aChild must
   * be a child of this container.
   */
  virtual bool RemoveChild(Layer* aChild);
  /**
   * CONSTRUCTION PHASE ONLY
   * Reposition aChild from the child list of this container. aChild must
   * be a child of this container.
   * If aAfter is non-null, it must be a child of this container and we
   * reposition after that layer. If it's null, we reposition at the start.
   */
  virtual bool RepositionChild(Layer* aChild, Layer* aAfter);

  void SetPreScale(float aXScale, float aYScale) {
    if (mPreXScale == aXScale && mPreYScale == aYScale) {
      return;
    }

    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) PreScale", this));
    mPreXScale = aXScale;
    mPreYScale = aYScale;
    Mutated();
  }

  void SetInheritedScale(float aXScale, float aYScale) {
    if (mInheritedXScale == aXScale && mInheritedYScale == aYScale) {
      return;
    }

    MOZ_LAYERS_LOG_IF_SHADOWABLE(this,
                                 ("Layer::Mutated(%p) InheritedScale", this));
    mInheritedXScale = aXScale;
    mInheritedYScale = aYScale;
    Mutated();
  }

  void SetScaleToResolution(float aResolution) {
    if (mPresShellResolution == aResolution) {
      return;
    }

    MOZ_LAYERS_LOG_IF_SHADOWABLE(
        this, ("Layer::Mutated(%p) ScaleToResolution", this));
    mPresShellResolution = aResolution;
    Mutated();
  }

  enum class SortMode {
    WITH_GEOMETRY,
    WITHOUT_GEOMETRY,
  };

  nsTArray<LayerPolygon> SortChildrenBy3DZOrder(SortMode aSortMode);

  ContainerLayer* AsContainerLayer() override { return this; }
  const ContainerLayer* AsContainerLayer() const override { return this; }

  // These getters can be used anytime.
  Layer* GetFirstChild() const override { return mFirstChild; }
  Layer* GetLastChild() const override { return mLastChild; }
  float GetPreXScale() const { return mPreXScale; }
  float GetPreYScale() const { return mPreYScale; }
  float GetInheritedXScale() const { return mInheritedXScale; }
  float GetInheritedYScale() const { return mInheritedYScale; }
  float GetPresShellResolution() const { return mPresShellResolution; }

  MOZ_LAYER_DECL_NAME("ContainerLayer", TYPE_CONTAINER)

  /**
   * ContainerLayer backends need to override ComputeEffectiveTransforms
   * since the decision about whether to use a temporary surface for the
   * container is backend-specific. ComputeEffectiveTransforms must also set
   * mUseIntermediateSurface.
   */
  void ComputeEffectiveTransforms(
      const gfx::Matrix4x4& aTransformToSurface) override = 0;

  /**
   * Call this only after ComputeEffectiveTransforms has been invoked
   * on this layer.
   * Returns true if this will use an intermediate surface. This is largely
   * backend-dependent, but it affects the operation of GetEffectiveOpacity().
   */
  bool UseIntermediateSurface() { return mUseIntermediateSurface; }

  /**
   * Returns the rectangle covered by the intermediate surface,
   * in this layer's coordinate system.
   *
   * NOTE: Since this layer has an intermediate surface it follows
   *       that LayerPixel == RenderTargetPixel
   */
  RenderTargetIntRect GetIntermediateSurfaceRect();

  /**
   * Returns true if this container has more than one non-empty child
   */
  bool HasMultipleChildren();

  void SetChildrenChanged(bool aVal) { mChildrenChanged = aVal; }

  // If |aRect| is null, the entire layer should be considered invalid for
  // compositing.
  virtual void SetInvalidCompositeRect(const gfx::IntRect* aRect) {}

 protected:
  friend class ReadbackProcessor;

  // Note that this is not virtual, and is based on the implementation of
  // ContainerLayer::RemoveChild, so it should only be called where you would
  // want to explicitly call the base class implementation of RemoveChild;
  // e.g., while (mFirstChild) ContainerLayer::RemoveChild(mFirstChild);
  void RemoveAllChildren();

  void DidInsertChild(Layer* aLayer);
  void DidRemoveChild(Layer* aLayer);

  bool AnyAncestorOrThisIs3DContextLeaf();

  void Collect3DContextLeaves(nsTArray<Layer*>& aToSort);

  // Collects child layers that do not extend 3D context. For ContainerLayers
  // that do extend 3D context, the 3D context leaves are collected.
  nsTArray<Layer*> CollectChildren() {
    nsTArray<Layer*> children;

    for (Layer* layer = GetFirstChild(); layer;
         layer = layer->GetNextSibling()) {
      ContainerLayer* container = layer->AsContainerLayer();

      if (container && container->Extend3DContext() &&
          !container->UseIntermediateSurface()) {
        container->Collect3DContextLeaves(children);
      } else {
        children.AppendElement(layer);
      }
    }

    return children;
  }

  ContainerLayer(LayerManager* aManager, void* aImplData);

  /**
   * A default implementation of ComputeEffectiveTransforms for use by OpenGL
   * and D3D.
   */
  void DefaultComputeEffectiveTransforms(
      const gfx::Matrix4x4& aTransformToSurface);

  /**
   * Loops over the children calling ComputeEffectiveTransforms on them.
   */
  void ComputeEffectiveTransformsForChildren(
      const gfx::Matrix4x4& aTransformToSurface);

  virtual void PrintInfo(std::stringstream& aStream,
                         const char* aPrefix) override;

  /**
   * True for if the container start a new 3D context extended by one
   * or more children.
   */
  bool Creates3DContextWithExtendingChildren();

  Layer* mFirstChild;
  Layer* mLastChild;
  float mPreXScale;
  float mPreYScale;
  // The resolution scale inherited from the parent layer. This will already
  // be part of mTransform.
  float mInheritedXScale;
  float mInheritedYScale;
  // For layers corresponding to an nsDisplayAsyncZoom, the resolution of the
  // associated pres shell; for other layers, 1.0.
  float mPresShellResolution;
  bool mUseIntermediateSurface;
  bool mMayHaveReadbackChild;
  // This is updated by ComputeDifferences. This will be true if we need to
  // invalidate the intermediate surface.
  bool mChildrenChanged;
};

void SetAntialiasingFlags(Layer* aLayer, gfx::DrawTarget* aTarget);

#ifdef MOZ_DUMP_PAINTING
void WriteSnapshotToDumpFile(Layer* aLayer, gfx::DataSourceSurface* aSurf);
void WriteSnapshotToDumpFile(LayerManager* aManager,
                             gfx::DataSourceSurface* aSurf);
void WriteSnapshotToDumpFile(Compositor* aCompositor, gfx::DrawTarget* aTarget);
#endif

// A utility function used by different LayerManager implementations.
gfx::IntRect ToOutsideIntRect(const gfxRect& aRect);

void RecordCompositionPayloadsPresented(
    const TimeStamp& aCompositionEndTime,
    const nsTArray<CompositionPayload>& aPayloads);

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_LAYERS_H */
