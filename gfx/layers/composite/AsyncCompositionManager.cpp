/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/AsyncCompositionManager.h"
#include <stdint.h>                 // for uint32_t
#include "LayerManagerComposite.h"  // for LayerManagerComposite, etc
#include "Layers.h"                 // for Layer, ContainerLayer, etc
#include "gfxPoint.h"               // for gfxPoint, gfxSize
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ServoBindings.h"  // for Servo_AnimationValue_GetOpacity, etc
#include "mozilla/ScopeExit.h"      // for MakeScopeExit
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/WidgetUtils.h"      // for ComputeTransformForRotation
#include "mozilla/gfx/BaseRect.h"     // for BaseRect
#include "mozilla/gfx/Point.h"        // for RoundedToInt, PointTyped
#include "mozilla/gfx/Rect.h"         // for RoundedToInt, RectTyped
#include "mozilla/gfx/ScaleFactor.h"  // for ScaleFactor
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/APZSampler.h"  // for APZSampler
#include "mozilla/layers/APZUtils.h"    // for CompleteAsyncTransform
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorAnimationStorage.h"  // for CompositorAnimationStorage
#include "mozilla/layers/CompositorBridgeParent.h"  // for CompositorBridgeParent, etc
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/LayerAnimationUtils.h"  // for TimingFunctionToComputedTimingFunction
#include "mozilla/layers/LayerMetricsWrapper.h"  // for LayerMetricsWrapper
#include "mozilla/layers/SampleTime.h"
#include "nsCoord.h"                 // for NSAppUnitsToFloatPixels, etc
#include "nsDebug.h"                 // for NS_ASSERTION, etc
#include "nsDeviceContext.h"         // for nsDeviceContext
#include "nsDisplayList.h"           // for nsDisplayTransform, etc
#include "nsMathUtils.h"             // for NS_round
#include "nsPoint.h"                 // for nsPoint
#include "nsRect.h"                  // for mozilla::gfx::IntRect
#include "nsRegion.h"                // for nsIntRegion
#include "nsTArray.h"                // for nsTArray, nsTArray_Impl, etc
#include "nsTArrayForwardDeclare.h"  // for nsTArray
#include "UnitTransforms.h"          // for TransformTo
#if defined(MOZ_WIDGET_ANDROID)
#  include <android/log.h>
#  include "mozilla/layers/UiCompositorControllerParent.h"
#  include "mozilla/widget/AndroidCompositorWidget.h"
#endif
#include "FrameUniformityData.h"
#include "TreeTraversal.h"  // for ForEachNode, BreadthFirstSearch
#include "VsyncSource.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

static bool IsSameDimension(hal::ScreenOrientation o1,
                            hal::ScreenOrientation o2) {
  bool isO1portrait = (o1 == hal::eScreenOrientation_PortraitPrimary ||
                       o1 == hal::eScreenOrientation_PortraitSecondary);
  bool isO2portrait = (o2 == hal::eScreenOrientation_PortraitPrimary ||
                       o2 == hal::eScreenOrientation_PortraitSecondary);
  return !(isO1portrait ^ isO2portrait);
}

static bool ContentMightReflowOnOrientationChange(const IntRect& rect) {
  return rect.Width() != rect.Height();
}

AsyncCompositionManager::AsyncCompositionManager(
    CompositorBridgeParent* aParent, HostLayerManager* aManager)
    : mLayerManager(aManager),
      mIsFirstPaint(true),
      mLayersUpdated(false),
      mReadyForCompose(true),
      mCompositorBridge(aParent) {
  MOZ_ASSERT(mCompositorBridge);
}

AsyncCompositionManager::~AsyncCompositionManager() = default;

void AsyncCompositionManager::ResolveRefLayers(
    CompositorBridgeParent* aCompositor, bool* aHasRemoteContent) {
  if (aHasRemoteContent) {
    *aHasRemoteContent = false;
  }

  if (!mLayerManager->GetRoot()) {
    return;
  }

  mReadyForCompose = true;
  bool hasRemoteContent = false;

  ForEachNode<ForwardIterator>(mLayerManager->GetRoot(), [&](Layer* layer) {
    RefLayer* refLayer = layer->AsRefLayer();
    if (!refLayer) {
      return;
    }

    hasRemoteContent = true;
    const CompositorBridgeParent::LayerTreeState* state =
        CompositorBridgeParent::GetIndirectShadowTree(
            refLayer->GetReferentId());
    if (!state) {
      return;
    }

    Layer* referent = state->mRoot;
    if (!referent) {
      return;
    }

    if (!refLayer->GetLocalVisibleRegion().IsEmpty()) {
      hal::ScreenOrientation chromeOrientation = mTargetConfig.orientation();
      hal::ScreenOrientation contentOrientation =
          state->mTargetConfig.orientation();
      if (!IsSameDimension(chromeOrientation, contentOrientation) &&
          ContentMightReflowOnOrientationChange(
              mTargetConfig.naturalBounds())) {
        mReadyForCompose = false;
      }
    }

    refLayer->ConnectReferentLayer(referent);
  });

  if (aHasRemoteContent) {
    *aHasRemoteContent = hasRemoteContent;
  }
}

void AsyncCompositionManager::DetachRefLayers() {
  if (!mLayerManager->GetRoot()) {
    return;
  }

  mReadyForCompose = false;

  ForEachNodePostOrder<ForwardIterator>(
      mLayerManager->GetRoot(), [&](Layer* layer) {
        RefLayer* refLayer = layer->AsRefLayer();
        if (!refLayer) {
          return;
        }

        const CompositorBridgeParent::LayerTreeState* state =
            CompositorBridgeParent::GetIndirectShadowTree(
                refLayer->GetReferentId());
        if (!state) {
          return;
        }

        Layer* referent = state->mRoot;
        if (referent) {
          refLayer->DetachReferentLayer(referent);
        }
      });
}

void AsyncCompositionManager::ComputeRotation() {
  if (!mTargetConfig.naturalBounds().IsEmpty()) {
    mWorldTransform = ComputeTransformForRotation(mTargetConfig.naturalBounds(),
                                                  mTargetConfig.rotation());
  }
}

static void GetBaseTransform(Layer* aLayer, Matrix4x4* aTransform) {
  // Start with the animated transform if there is one
  *aTransform = (aLayer->AsHostLayer()->GetShadowTransformSetByAnimation()
                     ? aLayer->GetLocalTransform()
                     : aLayer->GetTransform());
}

static void TransformClipRect(
    Layer* aLayer, const ParentLayerToParentLayerMatrix4x4& aTransform) {
  MOZ_ASSERT(aTransform.Is2D());
  const Maybe<ParentLayerIntRect>& clipRect =
      aLayer->AsHostLayer()->GetShadowClipRect();
  if (clipRect) {
    ParentLayerIntRect transformed = TransformBy(aTransform, *clipRect);
    aLayer->AsHostLayer()->SetShadowClipRect(Some(transformed));
  }
}

// Similar to TransformFixedClip(), but only transforms the fixed part of the
// clip.
static void TransformFixedClip(
    Layer* aLayer, const ParentLayerToParentLayerMatrix4x4& aTransform,
    AsyncCompositionManager::ClipParts& aClipParts) {
  MOZ_ASSERT(aTransform.Is2D());
  if (aClipParts.mFixedClip) {
    *aClipParts.mFixedClip = TransformBy(aTransform, *aClipParts.mFixedClip);
    aLayer->AsHostLayer()->SetShadowClipRect(aClipParts.Intersect());
  }
}

/**
 * Set the given transform as the shadow transform on the layer, assuming
 * that the given transform already has the pre- and post-scales applied.
 * That is, this function cancels out the pre- and post-scales from aTransform
 * before setting it as the shadow transform on the layer, so that when
 * the layer's effective transform is computed, the pre- and post-scales will
 * only be applied once.
 */
static void SetShadowTransform(Layer* aLayer,
                               LayerToParentLayerMatrix4x4 aTransform) {
  if (ContainerLayer* c = aLayer->AsContainerLayer()) {
    aTransform.PreScale(1.0f / c->GetPreXScale(), 1.0f / c->GetPreYScale(), 1);
  }
  aTransform.PostScale(1.0f / aLayer->GetPostXScale(),
                       1.0f / aLayer->GetPostYScale(), 1);
  aLayer->AsHostLayer()->SetShadowBaseTransform(aTransform.ToUnknownMatrix());
}

static void TranslateShadowLayer(
    Layer* aLayer, const ParentLayerPoint& aTranslation, bool aAdjustClipRect,
    AsyncCompositionManager::ClipPartsCache* aClipPartsCache) {
  // This layer might also be a scrollable layer and have an async transform.
  // To make sure we don't clobber that, we start with the shadow transform.
  // (i.e. GetLocalTransform() instead of GetTransform()).
  // Note that the shadow transform is reset on every frame of composition so
  // we don't have to worry about the adjustments compounding over successive
  // frames.
  LayerToParentLayerMatrix4x4 layerTransform = aLayer->GetLocalTransformTyped();

  // Apply the translation to the layer transform.
  layerTransform.PostTranslate(aTranslation);

  SetShadowTransform(aLayer, layerTransform);
  aLayer->AsHostLayer()->SetShadowTransformSetByAnimation(false);

  if (aAdjustClipRect) {
    auto transform =
        ParentLayerToParentLayerMatrix4x4::Translation(aTranslation);
    // If we're passed a clip parts cache, only transform the fixed part of
    // the clip.
    if (aClipPartsCache) {
      auto iter = aClipPartsCache->find(aLayer);
      MOZ_ASSERT(iter != aClipPartsCache->end());
      TransformFixedClip(aLayer, transform, iter->second);
    } else {
      TransformClipRect(aLayer, transform);
    }

    // If a fixed- or sticky-position layer has a mask layer, that mask should
    // move along with the layer, so apply the translation to the mask layer
    // too.
    if (Layer* maskLayer = aLayer->GetMaskLayer()) {
      TranslateShadowLayer(maskLayer, aTranslation, false, aClipPartsCache);
    }
  }
}

static void AccumulateLayerTransforms(Layer* aLayer, Layer* aAncestor,
                                      Matrix4x4& aMatrix) {
  // Accumulate the transforms between this layer and the subtree root layer.
  for (Layer* l = aLayer; l && l != aAncestor; l = l->GetParent()) {
    Matrix4x4 transform;
    GetBaseTransform(l, &transform);
    aMatrix *= transform;
  }
}

/**
 * Finds the metrics on |aLayer| with scroll id |aScrollId|, and returns a
 * LayerMetricsWrapper representing the (layer, metrics) pair, or the null
 * LayerMetricsWrapper if no matching metrics could be found.
 */
static LayerMetricsWrapper FindMetricsWithScrollId(
    Layer* aLayer, ScrollableLayerGuid::ViewID aScrollId) {
  for (uint64_t i = 0; i < aLayer->GetScrollMetadataCount(); ++i) {
    if (aLayer->GetFrameMetrics(i).GetScrollId() == aScrollId) {
      return LayerMetricsWrapper(aLayer, i);
    }
  }
  return LayerMetricsWrapper();
}

/**
 * Checks whether the (layer, metrics) pair (aTransformedLayer,
 * aTransformedMetrics) is on the path from |aFixedLayer| to the metrics with
 * scroll id |aFixedWithRespectTo|, inclusive.
 */
static bool AsyncTransformShouldBeUnapplied(
    Layer* aFixedLayer, ScrollableLayerGuid::ViewID aFixedWithRespectTo,
    Layer* aTransformedLayer, ScrollableLayerGuid::ViewID aTransformedMetrics) {
  LayerMetricsWrapper transformed =
      FindMetricsWithScrollId(aTransformedLayer, aTransformedMetrics);
  if (!transformed.IsValid()) {
    return false;
  }
  // It's important to start at the bottom, because the fixed layer itself
  // could have the transformed metrics, and they can be at the bottom.
  LayerMetricsWrapper current(aFixedLayer,
                              LayerMetricsWrapper::StartAt::BOTTOM);
  bool encounteredTransformedLayer = false;
  // The transformed layer is on the path from |aFixedLayer| to the fixed-to
  // layer if as we walk up the (layer, metrics) tree starting from
  // |aFixedLayer|, we *first* encounter the transformed layer, and *then* (or
  // at the same time) the fixed-to layer.
  while (current) {
    if (!encounteredTransformedLayer && current == transformed) {
      encounteredTransformedLayer = true;
    }
    if (current.Metrics().GetScrollId() == aFixedWithRespectTo) {
      return encounteredTransformedLayer;
    }
    current = current.GetParent();
    // It's possible that we reach a layers id boundary before we reach an
    // ancestor with the scroll id |aFixedWithRespectTo| (this could happen
    // e.g. if the scroll frame with that scroll id uses containerless
    // scrolling). In such a case, stop the walk, as a new layers id could
    // have a different layer with scroll id |aFixedWithRespectTo| which we
    // don't intend to match.
    if (current && current.AsRefLayer() != nullptr) {
      break;
    }
  }
  return false;
}

// If |aLayer| is fixed or sticky, returns the scroll id of the scroll frame
// that it's fixed or sticky to. Otherwise, returns Nothing().
static Maybe<ScrollableLayerGuid::ViewID> IsFixedOrSticky(Layer* aLayer) {
  bool isRootOfFixedSubtree = aLayer->GetIsFixedPosition() &&
                              !aLayer->GetParent()->GetIsFixedPosition();
  if (isRootOfFixedSubtree) {
    return Some(aLayer->GetFixedPositionScrollContainerId());
  }
  if (aLayer->GetIsStickyPosition()) {
    return Some(aLayer->GetStickyScrollContainerId());
  }
  return Nothing();
}

void AsyncCompositionManager::AlignFixedAndStickyLayers(
    Layer* aTransformedSubtreeRoot, Layer* aStartTraversalAt,
    SideBits aStuckSides, ScrollableLayerGuid::ViewID aTransformScrollId,
    const LayerToParentLayerMatrix4x4& aPreviousTransformForRoot,
    const LayerToParentLayerMatrix4x4& aCurrentTransformForRoot,
    const ScreenMargin& aFixedLayerMargins, ClipPartsCache& aClipPartsCache,
    const ScreenMargin& aGeckoFixedLayerMargins) {
  Layer* layer = aStartTraversalAt;
  bool needsAsyncTransformUnapplied = false;
  if (Maybe<ScrollableLayerGuid::ViewID> fixedTo = IsFixedOrSticky(layer)) {
    needsAsyncTransformUnapplied = AsyncTransformShouldBeUnapplied(
        layer, *fixedTo, aTransformedSubtreeRoot, aTransformScrollId);
  }

  // We want to process all the fixed and sticky descendants of
  // aTransformedSubtreeRoot. Once we do encounter such a descendant, we don't
  // need to recurse any deeper because the adjustment to the fixed or sticky
  // layer will apply to its subtree.
  if (!needsAsyncTransformUnapplied) {
    for (Layer* child = layer->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      AlignFixedAndStickyLayers(aTransformedSubtreeRoot, child, aStuckSides,
                                aTransformScrollId, aPreviousTransformForRoot,
                                aCurrentTransformForRoot, aFixedLayerMargins,
                                aClipPartsCache, aGeckoFixedLayerMargins);
    }
    return;
  }

  AdjustFixedOrStickyLayer(aTransformedSubtreeRoot, layer, aStuckSides,
                           aTransformScrollId, aPreviousTransformForRoot,
                           aCurrentTransformForRoot, aFixedLayerMargins,
                           aClipPartsCache, aGeckoFixedLayerMargins);
}

// Determine the amount of overlap between the 1D vector |aTranslation|
// and the interval [aMin, aMax].
static gfxFloat IntervalOverlap(gfxFloat aTranslation, gfxFloat aMin,
                                gfxFloat aMax) {
  if (aTranslation > 0) {
    return std::max(0.0, std::min(aMax, aTranslation) - std::max(aMin, 0.0));
  }

  return std::min(0.0, std::max(aMin, aTranslation) - std::min(aMax, 0.0));
}

void AsyncCompositionManager::AdjustFixedOrStickyLayer(
    Layer* aTransformedSubtreeRoot, Layer* aFixedOrSticky, SideBits aStuckSides,
    ScrollableLayerGuid::ViewID aTransformScrollId,
    const LayerToParentLayerMatrix4x4& aPreviousTransformForRoot,
    const LayerToParentLayerMatrix4x4& aCurrentTransformForRoot,
    const ScreenMargin& aFixedLayerMargins, ClipPartsCache& aClipPartsCache,
    const ScreenMargin& aGeckoFixedLayerMargins) {
  Layer* layer = aFixedOrSticky;

  // Insert a translation so that the position of the anchor point is the same
  // before and after the change to the transform of aTransformedSubtreeRoot.

  // Accumulate the transforms between this layer and the subtree root layer.
  Matrix4x4 ancestorTransform;
  if (layer != aTransformedSubtreeRoot) {
    AccumulateLayerTransforms(layer->GetParent(), aTransformedSubtreeRoot,
                              ancestorTransform);
  }
  ancestorTransform.NudgeToIntegersFixedEpsilon();

  // A transform creates a containing block for fixed-position descendants,
  // so there shouldn't be a transform in between the fixed layer and
  // the subtree root layer.
  if (layer->GetIsFixedPosition()) {
    MOZ_ASSERT(ancestorTransform.IsIdentity());
  }

  // Calculate the cumulative transforms between the subtree root with the
  // old transform and the current transform.
  // For coordinate purposes, we'll treat the subtree root layer as the
  // "parent" layer, even though it could be a farther ancestor.
  auto oldCumulativeTransform = ViewAs<LayerToParentLayerMatrix4x4>(
      ancestorTransform * aPreviousTransformForRoot.ToUnknownMatrix());
  auto newCumulativeTransform = ViewAs<LayerToParentLayerMatrix4x4>(
      ancestorTransform * aCurrentTransformForRoot.ToUnknownMatrix());

  // We're going to be inverting |newCumulativeTransform|. If it's singular,
  // there's nothing we can do.
  if (newCumulativeTransform.IsSingular()) {
    return;
  }

  // Since we create container layers for fixed layers, there shouldn't
  // a local CSS or OMTA transform on the fixed layer, either (any local
  // transform would go onto a descendant layer inside the container
  // layer).
#ifdef DEBUG
  Matrix4x4 localTransform;
  GetBaseTransform(layer, &localTransform);
  localTransform.NudgeToIntegersFixedEpsilon();
  MOZ_ASSERT(localTransform.IsIdentity());
#endif

  // Now work out the translation necessary to make sure the layer doesn't
  // move given the new sub-tree root transform.

  // Get the layer's fixed anchor point, in the layer's local coordinate space
  // (before any transform is applied).
  LayerPoint anchor = layer->GetFixedPositionAnchor();

  SideBits sideBits = layer->GetFixedPositionSides();
  if (layer->GetIsStickyPosition()) {
    // For sticky items, it may be that only some of the sides are actively
    // stuck. Only take into account those sides.
    sideBits &= aStuckSides;
  }

  // Offset the layer's anchor point to make sure fixed position content
  // respects content document fixed position margins.
  ScreenPoint offset = apz::ComputeFixedMarginsOffset(
      aFixedLayerMargins, sideBits,
      // For sticky layers, we don't need to factor aGeckoFixedLayerMargins
      // because Gecko doesn't shift the position of sticky elements for dynamic
      // toolbar movements.
      layer->GetIsStickyPosition() ? ScreenMargin() : aGeckoFixedLayerMargins);

  // Fixed margins only apply to layers fixed to the root, so we can view
  // the offset in layer space.
  LayerPoint offsetAnchor =
      anchor + ViewAs<LayerPixel>(
                   offset, PixelCastJustification::ScreenIsParentLayerForRoot);

  // Additionally transform the anchor to compensate for the change
  // from the old transform to the new transform. We do
  // this by using the old transform to take the offset anchor back into
  // subtree root space, and then the inverse of the new transform
  // to bring it back to layer space.
  ParentLayerPoint offsetAnchorInSubtreeRootSpace =
      oldCumulativeTransform.TransformPoint(offsetAnchor);
  LayerPoint transformedAnchor =
      newCumulativeTransform.Inverse().TransformPoint(
          offsetAnchorInSubtreeRootSpace);

  // We want to translate the layer by the difference between
  // |transformedAnchor| and |anchor|.
  LayerPoint translation = transformedAnchor - anchor;

  // A fixed layer will "consume" (be unadjusted by) the entire translation
  // calculated above. A sticky layer may consume all, part, or none of it,
  // depending on where we are relative to its sticky scroll range.
  // The remainder of the translation (the unconsumed portion) needs to
  // be propagated to descendant fixed/sticky layers.
  LayerPoint unconsumedTranslation;

  if (layer->GetIsStickyPosition()) {
    // For sticky positioned layers, the difference between the two rectangles
    // defines a pair of translation intervals in each dimension through which
    // the layer should not move relative to the scroll container. To
    // accomplish this, we limit each dimension of the |translation| to that
    // part of it which overlaps those intervals.
    const LayerRectAbsolute& stickyOuter = layer->GetStickyScrollRangeOuter();
    const LayerRectAbsolute& stickyInner = layer->GetStickyScrollRangeInner();

    LayerPoint originalTranslation = translation;
    translation.y =
        IntervalOverlap(translation.y, stickyOuter.Y(), stickyOuter.YMost()) -
        IntervalOverlap(translation.y, stickyInner.Y(), stickyInner.YMost());
    translation.x =
        IntervalOverlap(translation.x, stickyOuter.X(), stickyOuter.XMost()) -
        IntervalOverlap(translation.x, stickyInner.X(), stickyInner.XMost());
    unconsumedTranslation = translation - originalTranslation;
  }

  // Finally, apply the translation to the layer transform. Note that in cases
  // where the async transform on |aTransformedSubtreeRoot| affects this layer's
  // clip rect, we need to apply the same translation to said clip rect, so
  // that the effective transform on the clip rect takes it back to where it was
  // originally, had there been no async scroll.
  TranslateShadowLayer(
      layer,
      ViewAs<ParentLayerPixel>(translation,
                               PixelCastJustification::NoTransformOnLayer),
      true, &aClipPartsCache);

  // Propragate the unconsumed portion of the translation to descendant
  // fixed/sticky layers.
  if (unconsumedTranslation != LayerPoint()) {
    // Take the computations we performed to derive |translation| from
    // |aCurrentTransformForRoot|, and perform them in reverse, keeping other
    // quantities fixed, to come up with a new transform |newTransform| that
    // would produce |unconsumedTranslation|.
    LayerPoint newTransformedAnchor = unconsumedTranslation + anchor;
    ParentLayerPoint newTransformedAnchorInSubtreeRootSpace =
        oldCumulativeTransform.TransformPoint(newTransformedAnchor);
    LayerToParentLayerMatrix4x4 newTransform = aPreviousTransformForRoot;
    newTransform.PostTranslate(newTransformedAnchorInSubtreeRootSpace -
                               offsetAnchorInSubtreeRootSpace);

    // Propagate this new transform to our descendants as the new value of
    // |aCurrentTransformForRoot|. This allows them to consume the unconsumed
    // translation.
    for (Layer* child = layer->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      AlignFixedAndStickyLayers(aTransformedSubtreeRoot, child, aStuckSides,
                                aTransformScrollId, aPreviousTransformForRoot,
                                newTransform, aFixedLayerMargins,
                                aClipPartsCache, aGeckoFixedLayerMargins);
    }
  }
}

bool AsyncCompositionManager::SampleAnimations(Layer* aLayer,
                                               TimeStamp aCurrentFrameTime) {
  CompositorAnimationStorage* storage =
      mCompositorBridge->GetAnimationStorage();
  MOZ_ASSERT(storage);

  return storage->SampleAnimations(aLayer, mCompositorBridge,
                                   mPreviousFrameTimeStamp, aCurrentFrameTime);
}

void AsyncCompositionManager::RecordShadowTransforms(Layer* aLayer) {
  MOZ_ASSERT(StaticPrefs::gfx_vsync_collect_scroll_transforms());
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  ForEachNodePostOrder<ForwardIterator>(aLayer, [this](Layer* layer) {
    for (uint32_t i = 0; i < layer->GetScrollMetadataCount(); i++) {
      if (!layer->GetFrameMetrics(i).IsScrollable()) {
        continue;
      }
      gfx::Matrix4x4 shadowTransform =
          layer->AsHostLayer()->GetShadowBaseTransform();
      if (!shadowTransform.Is2D()) {
        continue;
      }

      Matrix transform = shadowTransform.As2D();
      if (transform.IsTranslation() && !shadowTransform.IsIdentity()) {
        Point translation = transform.GetTranslation();
        mLayerTransformRecorder.RecordTransform(layer, translation);
        return;
      }
    }
  });
}

static AsyncTransformComponentMatrix AdjustForClip(
    const AsyncTransformComponentMatrix& asyncTransform, Layer* aLayer) {
  AsyncTransformComponentMatrix result = asyncTransform;

  // Container layers start at the origin, but they are clipped to where they
  // actually have content on the screen. The tree transform is meant to apply
  // to the clipped area. If the tree transform includes a scale component,
  // then applying it to container as-is will produce incorrect results. To
  // avoid this, translate the layer so that the clip rect starts at the origin,
  // apply the tree transform, and translate back.
  if (const Maybe<ParentLayerIntRect>& shadowClipRect =
          aLayer->AsHostLayer()->GetShadowClipRect()) {
    if (shadowClipRect->TopLeft() !=
        ParentLayerIntPoint()) {  // avoid a gratuitous change of basis
      result.ChangeBasis(shadowClipRect->X(), shadowClipRect->Y(), 0);
    }
  }
  return result;
}

static void ExpandRootClipRect(Layer* aLayer,
                               const ScreenMargin& aFixedLayerMargins) {
  // For Fennec we want to expand the root scrollable layer clip rect based on
  // the fixed position margins. In particular, we want this while the dynamic
  // toolbar is in the process of sliding offscreen and the area of the
  // LayerView visible to the user is larger than the viewport size that Gecko
  // knows about (and therefore larger than the clip rect). We could also just
  // clear the clip rect on aLayer entirely but this seems more precise.
  Maybe<ParentLayerIntRect> rootClipRect =
      aLayer->AsHostLayer()->GetShadowClipRect();
  if (rootClipRect && aFixedLayerMargins != ScreenMargin()) {
#ifndef MOZ_WIDGET_ANDROID
    // We should never enter here on anything other than Fennec, since
    // aFixedLayerMargins should be empty everywhere else.
    MOZ_ASSERT(false);
#endif
    ParentLayerRect rect(rootClipRect.value());
    rect.Deflate(ViewAs<ParentLayerPixel>(
        aFixedLayerMargins,
        PixelCastJustification::ScreenIsParentLayerForRoot));
    aLayer->AsHostLayer()->SetShadowClipRect(Some(RoundedOut(rect)));
  }
}

#ifdef MOZ_WIDGET_ANDROID
static void MoveScrollbarForLayerMargin(
    Layer* aRoot, ScrollableLayerGuid::ViewID aRootScrollId,
    const ScreenMargin& aFixedLayerMargins) {
  // See bug 1223928 comment 9 - once we can detect the RCD with just the
  // isRootContent flag on the metrics, we can probably move this code into
  // ApplyAsyncTransformToScrollbar rather than having it as a separate
  // adjustment on the layer tree.
  Layer* scrollbar =
      BreadthFirstSearch<ReverseIterator>(aRoot, [aRootScrollId](Layer* aNode) {
        return (aNode->GetScrollbarData().IsThumb() &&
                aNode->GetScrollbarData().mDirection.isSome() &&
                *aNode->GetScrollbarData().mDirection ==
                    ScrollDirection::eHorizontal &&
                aNode->GetScrollbarData().mTargetViewId == aRootScrollId);
      });
  if (scrollbar) {
    // Shift the horizontal scrollbar down into the new space exposed by the
    // dynamic toolbar hiding. Technically we should also scale the vertical
    // scrollbar a bit to expand into the new space but it's not as noticeable
    // and it would add a lot more complexity, so we're going with the "it's not
    // worth it" justification.
    TranslateShadowLayer(scrollbar,
                         ParentLayerPoint(0, -aFixedLayerMargins.bottom), true,
                         nullptr);
    if (scrollbar->GetParent()) {
      // The layer that has the HORIZONTAL direction sits inside another
      // ContainerLayer. This ContainerLayer also has a clip rect that causes
      // the scrollbar to get clipped. We need to expand that clip rect to
      // prevent that from happening. This is kind of ugly in that we're
      // assuming a particular layer tree structure but short of adding more
      // flags to the layer there doesn't appear to be a good way to do this.
      ExpandRootClipRect(scrollbar->GetParent(), aFixedLayerMargins);
    }
  }
}
#endif

bool AsyncCompositionManager::ApplyAsyncContentTransformToTree(
    Layer* aLayer, bool* aOutFoundRoot) {
  bool appliedTransform = false;
  std::stack<Maybe<ParentLayerIntRect>> stackDeferredClips;
  std::stack<LayersId> layersIds;
  layersIds.push(mCompositorBridge->RootLayerTreeId());

  // Maps layers to their ClipParts. The parts are not stored individually
  // on the layer, but during AlignFixedAndStickyLayers we need access to
  // the individual parts for descendant layers.
  ClipPartsCache clipPartsCache;

  Layer* zoomContainer = nullptr;
  Maybe<LayerMetricsWrapper> zoomedMetrics;

  ForEachNode<ForwardIterator>(
      aLayer,
      [&](Layer* layer) {
        if (layer->AsRefLayer()) {
          layersIds.push(layer->AsRefLayer()->GetReferentId());
        }

        stackDeferredClips.push(Maybe<ParentLayerIntRect>());

        // If we encounter the async zoom container, find the corresponding
        // APZC and stash it into |zoomedMetrics|.
        // (We stash it in the form of a LayerMetricsWrapper because
        // APZSampler requires going through that rather than using the APZC
        // directly.)
        // We do this on the way down the tree (i.e. here in the pre-action)
        // so that by the time we encounter the layers with the RCD-RSF's
        // scroll metadata (which will be descendants of the async zoom
        // container), we can check for it and know we should only apply the
        // scroll portion of the async transform to those layers (as the zoom
        // portion will go on the async zoom container).
        if (Maybe<ScrollableLayerGuid::ViewID> zoomedScrollId =
                layer->GetAsyncZoomContainerId()) {
          zoomContainer = layer;
          ForEachNode<ForwardIterator>(
              LayerMetricsWrapper(layer),
              [zoomedScrollId, &zoomedMetrics](LayerMetricsWrapper aWrapper) {
                // Do not descend into layer subtrees with a different layers
                // id.
                if (aWrapper.AsRefLayer()) {
                  return TraversalFlag::Skip;
                }

                if (aWrapper.Metrics().GetScrollId() == *zoomedScrollId) {
                  zoomedMetrics = Some(aWrapper);
                  MOZ_ASSERT(zoomedMetrics->GetApzc());
                  return TraversalFlag::Abort;
                }

                return TraversalFlag::Continue;
              });
        }
      },
      [&](Layer* layer) {
        Maybe<ParentLayerIntRect> clipDeferredFromChildren =
            stackDeferredClips.top();
        stackDeferredClips.pop();
        MOZ_ASSERT(!layersIds.empty());
        LayersId currentLayersId = layersIds.top();
        LayerToParentLayerMatrix4x4 oldTransform =
            layer->GetTransformTyped() * AsyncTransformMatrix();

        AsyncTransformComponentMatrix combinedAsyncTransform;
        bool hasAsyncTransform = false;
        // Only set on the root layer for Android.
        ScreenMargin fixedLayerMargins;

        // Each layer has multiple clips:
        //  - Its local clip, which is fixed to the layer contents, i.e. it
        //    moves with those async transforms which the layer contents move
        //    with.
        //  - Its scrolled clip, which moves with all async transforms.
        //  - For each ScrollMetadata on the layer, a scroll clip. This
        //    includes the composition bounds and any other clips induced by
        //    layout. This moves with async transforms from ScrollMetadatas
        //    above it.
        // In this function, these clips are combined into two shadow clip
        // parts:
        //  - The fixed clip, which consists of the local clip only, initially
        //    transformed by all async transforms.
        //  - The scrolled clip, which consists of the other clips, transformed
        //    by the appropriate transforms.
        // These two parts are kept separate for now, because for fixed layers,
        // we need to adjust the fixed clip (to cancel out some async
        // transforms). The parts are kept in a cache which is cleared at the
        // beginning of every composite. The final shadow clip for the layer is
        // the intersection of the (possibly adjusted) fixed clip and the
        // scrolled clip.
        ClipParts& clipParts = clipPartsCache[layer];
        clipParts.mFixedClip = layer->GetClipRect();
        clipParts.mScrolledClip = layer->GetScrolledClipRect();

        // If we are a perspective transform ContainerLayer, apply the clip
        // deferred from our child (if there is any) before we iterate over our
        // frame metrics, because this clip is subject to all async transforms
        // of this layer. Since this clip came from the a scroll clip on the
        // child, it becomes part of our scrolled clip.
        clipParts.mScrolledClip = IntersectMaybeRects(clipDeferredFromChildren,
                                                      clipParts.mScrolledClip);

        // The transform of a mask layer is relative to the masked layer's
        // parent layer. So whenever we apply an async transform to a layer, we
        // need to apply that same transform to the layer's own mask layer. A
        // layer can also have "ancestor" mask layers for any rounded clips from
        // its ancestor scroll frames. A scroll frame mask layer only needs to
        // be async transformed for async scrolls of this scroll frame's
        // ancestor scroll frames, not for async scrolls of this scroll frame
        // itself. In the loop below, we iterate over scroll frames from inside
        // to outside. At each iteration, this array contains the layer's
        // ancestor mask layers of all scroll frames inside the current one.
        nsTArray<Layer*> ancestorMaskLayers;

        // The layer's scrolled clip can have an ancestor mask layer as well,
        // which is moved by all async scrolls on this layer.
        if (const Maybe<LayerClip>& scrolledClip = layer->GetScrolledClip()) {
          if (scrolledClip->GetMaskLayerIndex()) {
            ancestorMaskLayers.AppendElement(layer->GetAncestorMaskLayerAt(
                *scrolledClip->GetMaskLayerIndex()));
          }
        }

        if (RefPtr<APZSampler> sampler = mCompositorBridge->GetAPZSampler()) {
          for (uint32_t i = 0; i < layer->GetScrollMetadataCount(); i++) {
            LayerMetricsWrapper wrapper(layer, i);
            if (!wrapper.GetApzc()) {
              continue;
            }

            const FrameMetrics& metrics = wrapper.Metrics();
            MOZ_ASSERT(metrics.IsScrollable());

            hasAsyncTransform = true;

            AsyncTransformComponents asyncTransformComponents =
                (zoomedMetrics &&
                 sampler->GetGuid(*zoomedMetrics) == sampler->GetGuid(wrapper))
                    ? AsyncTransformComponents{AsyncTransformComponent::eLayout}
                    : LayoutAndVisual;
            AsyncTransformComponentMatrix asyncTransform =
                sampler->GetCurrentAsyncTransformWithOverscroll(
                    wrapper, asyncTransformComponents);
            Maybe<CompositionPayload> payload =
                sampler->NotifyScrollSampling(wrapper);
            // The scroll latency should be measured between composition and the
            // first scrolling event. Otherwise we observe metrics with <16ms
            // latency even when frame.delay is enabled.
            if (payload.isSome()) {
              mLayerManager->RegisterPayload(*payload);
            }

            if (!layer->IsScrollableWithoutContent()) {
              sampler->MarkAsyncTransformAppliedToContent(wrapper);
            }

            const ScrollMetadata& scrollMetadata = wrapper.Metadata();

#if defined(MOZ_WIDGET_ANDROID)
            // If we find a metrics which is the root content doc, use that. If
            // not, use the root layer. Since this function recurses on children
            // first we should only end up using the root layer if the entire
            // tree was devoid of a root content metrics. This is a temporary
            // solution; in the long term we should not need the root content
            // metrics at all. See bug 1201529 comment 6 for details.
            if (!(*aOutFoundRoot)) {
              *aOutFoundRoot =
                  metrics.IsRootContent() ||        /* RCD */
                  (layer->GetParent() == nullptr && /* rootmost metrics */
                   i + 1 >= layer->GetScrollMetadataCount());
              if (*aOutFoundRoot) {
                mRootScrollableId = metrics.GetScrollId();
                Compositor* compositor = mLayerManager->GetCompositor();
                if (CompositorBridgeParent* bridge =
                        compositor->GetCompositorBridgeParent()) {
                  LayersId rootLayerTreeId = bridge->RootLayerTreeId();
                  GeckoViewMetrics gvMetrics =
                      sampler->GetGeckoViewMetrics(wrapper);
                  if (mIsFirstPaint || GeckoViewMetricsHaveUpdated(gvMetrics)) {
                    if (RefPtr<UiCompositorControllerParent> uiController =
                            UiCompositorControllerParent::
                                GetFromRootLayerTreeId(rootLayerTreeId)) {
                      uiController->NotifyUpdateScreenMetrics(gvMetrics);
                    }
                    mLastMetrics = gvMetrics;
                  }
                  if (mIsFirstPaint) {
                    if (RefPtr<UiCompositorControllerParent> uiController =
                            UiCompositorControllerParent::
                                GetFromRootLayerTreeId(rootLayerTreeId)) {
                      uiController->NotifyFirstPaint();
                    }
                    mIsFirstPaint = false;
                  }
                  if (mLayersUpdated) {
                    LayersId rootLayerTreeId = bridge->RootLayerTreeId();
                    if (RefPtr<UiCompositorControllerParent> uiController =
                            UiCompositorControllerParent::
                                GetFromRootLayerTreeId(rootLayerTreeId)) {
                      uiController->NotifyLayersUpdated();
                    }
                    mLayersUpdated = false;
                  }
                }
                fixedLayerMargins = GetFixedLayerMargins();
              }
            }
#else
            *aOutFoundRoot = false;
            // Non-Android platforms still care about this flag being cleared
            // after the first call to TransformShadowTree().
            mIsFirstPaint = false;
#endif

            // Transform the current local clips by this APZC's async transform.
            MOZ_ASSERT(asyncTransform.Is2D());
            if (clipParts.mFixedClip) {
              *clipParts.mFixedClip =
                  TransformBy(asyncTransform, *clipParts.mFixedClip);
            }
            if (clipParts.mScrolledClip) {
              *clipParts.mScrolledClip =
                  TransformBy(asyncTransform, *clipParts.mScrolledClip);
            }
            // Note: we don't set the layer's shadow clip rect property yet;
            // AlignFixedAndStickyLayers will use the clip parts from the clip
            // parts cache.

            combinedAsyncTransform *= asyncTransform;

            // For the purpose of aligning fixed and sticky layers, we disregard
            // the overscroll transform as well as any OMTA transform when
            // computing the 'aCurrentTransformForRoot' parameter. This ensures
            // that the overscroll and OMTA transforms are not unapplied, and
            // therefore that the visual effects apply to fixed and sticky
            // layers. We do this by using GetTransform() as the base transform
            // rather than GetLocalTransform(), which would include those
            // factors.
            LayerToParentLayerMatrix4x4 transformWithoutOverscrollOrOmta =
                layer->GetTransformTyped() *
                CompleteAsyncTransform(AdjustForClip(asyncTransform, layer));
            // See bug 1630274 for why we pass eNone here; fixing that bug will
            // probably end up changing this to be more correct.
            AlignFixedAndStickyLayers(layer, layer, SideBits::eNone,
                                      metrics.GetScrollId(), oldTransform,
                                      transformWithoutOverscrollOrOmta,
                                      fixedLayerMargins, clipPartsCache,
                                      sampler->GetGeckoFixedLayerMargins());

            // Combine the local clip with the ancestor scrollframe clip. This
            // is not included in the async transform above, since the ancestor
            // clip should not move with this APZC.
            if (scrollMetadata.HasScrollClip()) {
              ParentLayerIntRect clip =
                  scrollMetadata.ScrollClip().GetClipRect();
              if (layer->GetParent() &&
                  layer->GetParent()->GetTransformIsPerspective()) {
                // If our parent layer has a perspective transform, we want to
                // apply our scroll clip to it instead of to this layer (see bug
                // 1168263). A layer with a perspective transform shouldn't have
                // multiple children with FrameMetrics, nor a child with
                // multiple FrameMetrics. (A child with multiple FrameMetrics
                // would mean that there's *another* scrollable element between
                // the one with the CSS perspective and the transformed element.
                // But you'd have to use preserve-3d on the inner scrollable
                // element in order to have the perspective apply to the
                // transformed child, and preserve-3d is not supported on
                // scrollable elements, so this case can't occur.)
                MOZ_ASSERT(!stackDeferredClips.top());
                stackDeferredClips.top().emplace(clip);
              } else {
                clipParts.mScrolledClip =
                    IntersectMaybeRects(Some(clip), clipParts.mScrolledClip);
              }
            }

            // Do the same for the ancestor mask layers: ancestorMaskLayers
            // contains the ancestor mask layers for scroll frames *inside* the
            // current scroll frame, so these are the ones we need to shift by
            // our async transform.
            for (Layer* ancestorMaskLayer : ancestorMaskLayers) {
              SetShadowTransform(
                  ancestorMaskLayer,
                  ancestorMaskLayer->GetLocalTransformTyped() * asyncTransform);
            }

            // Append the ancestor mask layer for this scroll frame to
            // ancestorMaskLayers.
            if (scrollMetadata.HasScrollClip()) {
              const LayerClip& scrollClip = scrollMetadata.ScrollClip();
              if (scrollClip.GetMaskLayerIndex()) {
                size_t maskLayerIndex = scrollClip.GetMaskLayerIndex().value();
                Layer* ancestorMaskLayer =
                    layer->GetAncestorMaskLayerAt(maskLayerIndex);
                ancestorMaskLayers.AppendElement(ancestorMaskLayer);
              }
            }
          }

          if (Maybe<ScrollableLayerGuid::ViewID> zoomedScrollId =
                  layer->GetAsyncZoomContainerId()) {
            if (zoomedMetrics) {
              AsyncTransformComponentMatrix zoomTransform =
                  sampler->GetCurrentAsyncTransformWithOverscroll(
                      *zoomedMetrics, {AsyncTransformComponent::eVisual});
              hasAsyncTransform = true;
              combinedAsyncTransform *= zoomTransform;
            } else {
              // TODO: Is this normal? It happens on some pages, such as
              // about:config on mobile, for just one frame or so, before the
              // scroll metadata for zoomedScrollId appears in the layer tree.
            }
          }

          auto IsFixedToZoomContainer = [&](Layer* aFixedLayer) {
            if (!zoomedMetrics) {
              return false;
            }
            ScrollableLayerGuid::ViewID targetId =
                aFixedLayer->GetFixedPositionScrollContainerId();
            MOZ_ASSERT(targetId != ScrollableLayerGuid::NULL_SCROLL_ID);
            ScrollableLayerGuid rootContent = sampler->GetGuid(*zoomedMetrics);
            return rootContent.mScrollId == targetId &&
                   rootContent.mLayersId == currentLayersId;
          };

          auto SidesStuckToZoomContainer = [&](Layer* aLayer) -> SideBits {
            SideBits result = SideBits::eNone;
            if (!zoomedMetrics) {
              return result;
            }
            if (!aLayer->GetIsStickyPosition()) {
              return result;
            }

            ScrollableLayerGuid::ViewID targetId =
                aLayer->GetStickyScrollContainerId();
            if (targetId == ScrollableLayerGuid::NULL_SCROLL_ID) {
              return result;
            }

            ScrollableLayerGuid rootContent = sampler->GetGuid(*zoomedMetrics);
            if (rootContent.mScrollId != targetId ||
                rootContent.mLayersId != currentLayersId) {
              return result;
            }

            ParentLayerPoint translation =
                sampler
                    ->GetCurrentAsyncTransform(
                        *zoomedMetrics, {AsyncTransformComponent::eLayout})
                    .mTranslation;

            if (apz::IsStuckAtBottom(translation.y,
                                     aLayer->GetStickyScrollRangeInner(),
                                     aLayer->GetStickyScrollRangeOuter())) {
              result |= SideBits::eBottom;
            }
            if (apz::IsStuckAtTop(translation.y,
                                  aLayer->GetStickyScrollRangeInner(),
                                  aLayer->GetStickyScrollRangeOuter())) {
              result |= SideBits::eTop;
            }
            return result;
          };

          // Layers fixed to the RCD-RSF no longer need
          // AdjustFixedOrStickyLayer() to scroll them by the eVisual transform,
          // as that's now applied to the async zoom container itself. However,
          // we still need to adjust them by the fixed layer margins to
          // account for dynamic toolbar transitions. This is also handled by
          // AdjustFixedOrStickyLayer(), so we now call it with empty transforms
          // to get it to perform just the fixed margins adjustment.
          SideBits stuckSides = SidesStuckToZoomContainer(layer);
          if (zoomedMetrics && ((layer->GetIsFixedPosition() &&
                                 !layer->GetParent()->GetIsFixedPosition() &&
                                 IsFixedToZoomContainer(layer)) ||
                                stuckSides != SideBits::eNone)) {
            LayerToParentLayerMatrix4x4 emptyTransform;
            ScreenMargin marginsForFixedLayer = GetFixedLayerMargins();
            AdjustFixedOrStickyLayer(zoomContainer, layer, stuckSides,
                                     sampler->GetGuid(*zoomedMetrics).mScrollId,
                                     emptyTransform, emptyTransform,
                                     marginsForFixedLayer, clipPartsCache,
                                     sampler->GetGeckoFixedLayerMargins());
          }
        }

        bool clipChanged = (hasAsyncTransform || clipDeferredFromChildren ||
                            layer->GetScrolledClipRect());
        if (clipChanged) {
          // Intersect the two clip parts and apply them to the layer.
          // During ApplyAsyncContentTransformTree on an ancestor layer,
          // AlignFixedAndStickyLayers may overwrite this with a new clip it
          // computes from the clip parts, but if that doesn't happen, this
          // is the layer's final clip rect.
          layer->AsHostLayer()->SetShadowClipRect(clipParts.Intersect());
        }

        if (hasAsyncTransform) {
          // Apply the APZ transform on top of GetLocalTransform() here (rather
          // than GetTransform()) in case the OMTA code in SampleAnimations
          // already set a shadow transform; in that case we want to apply ours
          // on top of that one rather than clobber it.
          SetShadowTransform(layer,
                             layer->GetLocalTransformTyped() *
                                 AdjustForClip(combinedAsyncTransform, layer));

          // Do the same for the layer's own mask layer, if it has one.
          if (Layer* maskLayer = layer->GetMaskLayer()) {
            SetShadowTransform(maskLayer, maskLayer->GetLocalTransformTyped() *
                                              combinedAsyncTransform);
          }

          appliedTransform = true;
        }

        ExpandRootClipRect(layer, fixedLayerMargins);

        if (layer->GetScrollbarData().mScrollbarLayerType ==
            layers::ScrollbarLayerType::Thumb) {
          ApplyAsyncTransformToScrollbar(layer);
        }

        if (layer->AsRefLayer()) {
          MOZ_ASSERT(layersIds.size() > 1);
          layersIds.pop();
        }
      });

  return appliedTransform;
}

#if defined(MOZ_WIDGET_ANDROID)
bool AsyncCompositionManager::GeckoViewMetricsHaveUpdated(
    const GeckoViewMetrics& aMetrics) {
  return RoundedToInt(mLastMetrics.mVisualScrollOffset) !=
             RoundedToInt(aMetrics.mVisualScrollOffset) ||
         mLastMetrics.mZoom != aMetrics.mZoom;
}
#endif

static bool LayerIsScrollbarTarget(const LayerMetricsWrapper& aTarget,
                                   Layer* aScrollbar) {
  if (!aTarget.GetApzc()) {
    return false;
  }
  const FrameMetrics& metrics = aTarget.Metrics();
  MOZ_ASSERT(metrics.IsScrollable());
  if (metrics.GetScrollId() != aScrollbar->GetScrollbarData().mTargetViewId) {
    return false;
  }
  return !metrics.IsScrollInfoLayer();
}

static void ApplyAsyncTransformToScrollbarForContent(
    const RefPtr<APZSampler>& aSampler, Layer* aScrollbar,
    const LayerMetricsWrapper& aContent, bool aScrollbarIsDescendant) {
  AsyncTransformComponentMatrix clipTransform;

  MOZ_ASSERT(aSampler);
  LayerToParentLayerMatrix4x4 transform =
      aSampler->ComputeTransformForScrollThumb(
          aScrollbar->GetLocalTransformTyped(), aContent,
          aScrollbar->GetScrollbarData(), aScrollbarIsDescendant,
          &clipTransform);

  if (aScrollbarIsDescendant) {
    // We also need to make a corresponding change on the clip rect of all the
    // layers on the ancestor chain from the scrollbar layer up to but not
    // including the layer with the async transform. Otherwise the scrollbar
    // shifts but gets clipped and so appears to flicker.
    for (Layer* ancestor = aScrollbar; ancestor != aContent.GetLayer();
         ancestor = ancestor->GetParent()) {
      TransformClipRect(ancestor, clipTransform);
    }
  }

  SetShadowTransform(aScrollbar, transform);
}

static LayerMetricsWrapper FindScrolledLayerForScrollbar(Layer* aScrollbar,
                                                         bool* aOutIsAncestor) {
  // First check if the scrolled layer is an ancestor of the scrollbar layer.
  LayerMetricsWrapper root(aScrollbar->Manager()->GetRoot());
  LayerMetricsWrapper prevAncestor(aScrollbar);
  LayerMetricsWrapper scrolledLayer;

  for (LayerMetricsWrapper ancestor(aScrollbar); ancestor;
       ancestor = ancestor.GetParent()) {
    // Don't walk into remote layer trees; the scrollbar will always be in
    // the same layer space.
    if (ancestor.AsRefLayer()) {
      root = prevAncestor;
      break;
    }
    prevAncestor = ancestor;

    if (LayerIsScrollbarTarget(ancestor, aScrollbar)) {
      *aOutIsAncestor = true;
      return ancestor;
    }
  }

  // Search the entire layer space of the scrollbar.
  ForEachNode<ForwardIterator>(root, [&root, &scrolledLayer, &aScrollbar](
                                         LayerMetricsWrapper aLayerMetrics) {
    // Do not recurse into RefLayers, since our initial aSubtreeRoot is the
    // root (or RefLayer root) of a single layer space to search.
    if (root != aLayerMetrics && aLayerMetrics.AsRefLayer()) {
      return TraversalFlag::Skip;
    }
    if (LayerIsScrollbarTarget(aLayerMetrics, aScrollbar)) {
      scrolledLayer = aLayerMetrics;
      return TraversalFlag::Abort;
    }
    return TraversalFlag::Continue;
  });
  return scrolledLayer;
}

void AsyncCompositionManager::ApplyAsyncTransformToScrollbar(Layer* aLayer) {
  // If this layer corresponds to a scrollbar, then there should be a layer that
  // is a previous sibling or a parent that has a matching ViewID on its
  // FrameMetrics. That is the content that this scrollbar is for. We pick up
  // the transient async transform from that layer and use it to update the
  // scrollbar position. Note that it is possible that the content layer is no
  // longer there; in this case we don't need to do anything because there can't
  // be an async transform on the content.
  bool isAncestor = false;
  const LayerMetricsWrapper& scrollTarget =
      FindScrolledLayerForScrollbar(aLayer, &isAncestor);
  if (scrollTarget) {
    ApplyAsyncTransformToScrollbarForContent(mCompositorBridge->GetAPZSampler(),
                                             aLayer, scrollTarget, isAncestor);
  }
}

void AsyncCompositionManager::GetFrameUniformity(
    FrameUniformityData* aOutData) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mLayerTransformRecorder.EndTest(aOutData);
}

bool AsyncCompositionManager::TransformShadowTree(
    const SampleTime& aCurrentFrame, TimeDuration aVsyncRate,
    CompositorBridgeParentBase::TransformsToSkip aSkip) {
  AUTO_PROFILER_LABEL("AsyncCompositionManager::TransformShadowTree", GRAPHICS);

  Layer* root = mLayerManager->GetRoot();
  if (!root) {
    return false;
  }

  // First, compute and set the shadow transforms from OMT animations.
  // NB: we must sample animations *before* sampling pan/zoom
  // transforms.
  bool wantNextFrame = SampleAnimations(root, aCurrentFrame.Time());

  // Advance animations to the next expected vsync timestamp, if we can
  // get it.
  SampleTime nextFrame = aCurrentFrame;

  MOZ_ASSERT(aVsyncRate != TimeDuration::Forever());
  if (aVsyncRate != TimeDuration::Forever()) {
    nextFrame = nextFrame + aVsyncRate;
  }

  // Reset the previous time stamp if we don't already have any running
  // animations to avoid using the time which is far behind for newly
  // started animations.
  mPreviousFrameTimeStamp = wantNextFrame ? aCurrentFrame.Time() : TimeStamp();

  if (!(aSkip & CompositorBridgeParentBase::TransformsToSkip::APZ)) {
    bool apzAnimating = false;
    if (RefPtr<APZSampler> apz = mCompositorBridge->GetAPZSampler()) {
      apzAnimating = apz->AdvanceAnimations(nextFrame);
    }
    wantNextFrame |= apzAnimating;

    // Apply an async content transform to any layer that has
    // an async pan zoom controller.
    bool foundRoot = false;
    if (ApplyAsyncContentTransformToTree(root, &foundRoot)) {
#if defined(MOZ_WIDGET_ANDROID)
      MOZ_ASSERT(foundRoot);
      if (foundRoot && GetFixedLayerMargins() != ScreenMargin()) {
        MoveScrollbarForLayerMargin(root, mRootScrollableId,
                                    GetFixedLayerMargins());
      }
#endif
    }
  }

  HostLayer* rootComposite = root->AsHostLayer();

  gfx::Matrix4x4 trans = rootComposite->GetShadowBaseTransform();
  trans *= gfx::Matrix4x4::From2D(mWorldTransform);
  rootComposite->SetShadowBaseTransform(trans);

  if (StaticPrefs::gfx_vsync_collect_scroll_transforms()) {
    RecordShadowTransforms(root);
  }

  return wantNextFrame;
}

void AsyncCompositionManager::SetFixedLayerMargins(ScreenIntCoord aTop,
                                                   ScreenIntCoord aBottom) {
  mFixedLayerMargins.top = aTop;
  mFixedLayerMargins.bottom = aBottom;
}
ScreenMargin AsyncCompositionManager::GetFixedLayerMargins() const {
  ScreenMargin result = mFixedLayerMargins;
  if (StaticPrefs::apz_fixed_margin_override_enabled()) {
    result.top = StaticPrefs::apz_fixed_margin_override_top();
    result.bottom = StaticPrefs::apz_fixed_margin_override_bottom();
  }
  return result;
}

}  // namespace layers
}  // namespace mozilla
