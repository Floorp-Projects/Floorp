/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/AsyncCompositionManager.h"
#include <stdint.h>                     // for uint32_t
#include "FrameMetrics.h"               // for FrameMetrics
#include "LayerManagerComposite.h"      // for LayerManagerComposite, etc
#include "Layers.h"                     // for Layer, ContainerLayer, etc
#include "gfxPoint.h"                   // for gfxPoint, gfxSize
#include "gfxPrefs.h"                   // for gfxPrefs
#include "mozilla/ServoBindings.h"      // for Servo_AnimationValue_GetOpacity, etc
#include "mozilla/WidgetUtils.h"        // for ComputeTransformForRotation
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Point.h"          // for RoundedToInt, PointTyped
#include "mozilla/gfx/Rect.h"           // for RoundedToInt, RectTyped
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/APZSampler.h"  // for APZSampler
#include "mozilla/layers/APZUtils.h"    // for CompleteAsyncTransform
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorBridgeParent.h" // for CompositorBridgeParent, etc
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/LayerAnimationUtils.h" // for TimingFunctionToComputedTimingFunction
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "nsCoord.h"                    // for NSAppUnitsToFloatPixels, etc
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsDeviceContext.h"            // for nsDeviceContext
#include "nsDisplayList.h"              // for nsDisplayTransform, etc
#include "nsMathUtils.h"                // for NS_round
#include "nsPoint.h"                    // for nsPoint
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray
#include "UnitTransforms.h"             // for TransformTo
#include "gfxPrefs.h"
#if defined(MOZ_WIDGET_ANDROID)
# include <android/log.h>
# include "mozilla/layers/UiCompositorControllerParent.h"
# include "mozilla/widget/AndroidCompositorWidget.h"
#endif
#include "GeckoProfiler.h"
#include "FrameUniformityData.h"
#include "TreeTraversal.h"              // for ForEachNode, BreadthFirstSearch
#include "VsyncSource.h"

struct nsCSSValueSharedList;

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

static bool
IsSameDimension(dom::ScreenOrientationInternal o1, dom::ScreenOrientationInternal o2)
{
  bool isO1portrait = (o1 == dom::eScreenOrientation_PortraitPrimary || o1 == dom::eScreenOrientation_PortraitSecondary);
  bool isO2portrait = (o2 == dom::eScreenOrientation_PortraitPrimary || o2 == dom::eScreenOrientation_PortraitSecondary);
  return !(isO1portrait ^ isO2portrait);
}

static bool
ContentMightReflowOnOrientationChange(const IntRect& rect)
{
  return rect.Width() != rect.Height();
}

AsyncCompositionManager::AsyncCompositionManager(CompositorBridgeParent* aParent,
                                                 HostLayerManager* aManager)
  : mLayerManager(aManager)
  , mIsFirstPaint(true)
  , mLayersUpdated(false)
  , mReadyForCompose(true)
  , mCompositorBridge(aParent)
{
  MOZ_ASSERT(mCompositorBridge);
}

AsyncCompositionManager::~AsyncCompositionManager()
{
}

void
AsyncCompositionManager::ResolveRefLayers(CompositorBridgeParent* aCompositor,
                                          bool* aHasRemoteContent,
                                          bool* aResolvePlugins)
{
  if (aHasRemoteContent) {
    *aHasRemoteContent = false;
  }

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
  // If valid *aResolvePlugins indicates if we need to update plugin geometry
  // when we walk the tree.
  bool resolvePlugins = (aCompositor && aResolvePlugins && *aResolvePlugins);
#endif

  if (!mLayerManager->GetRoot()) {
    // Updated the return value since this result controls completing composition.
    if (aResolvePlugins) {
      *aResolvePlugins = false;
    }
    return;
  }

  mReadyForCompose = true;
  bool hasRemoteContent = false;
  bool didResolvePlugins = false;

  ForEachNode<ForwardIterator>(
    mLayerManager->GetRoot(),
    [&](Layer* layer)
    {
      RefLayer* refLayer = layer->AsRefLayer();
      if (!refLayer) {
        return;
      }

      hasRemoteContent = true;
      const CompositorBridgeParent::LayerTreeState* state =
        CompositorBridgeParent::GetIndirectShadowTree(refLayer->GetReferentId());
      if (!state) {
        return;
      }

      Layer* referent = state->mRoot;
      if (!referent) {
        return;
      }

      if (!refLayer->GetLocalVisibleRegion().IsEmpty()) {
        dom::ScreenOrientationInternal chromeOrientation =
          mTargetConfig.orientation();
        dom::ScreenOrientationInternal contentOrientation =
          state->mTargetConfig.orientation();
        if (!IsSameDimension(chromeOrientation, contentOrientation) &&
            ContentMightReflowOnOrientationChange(mTargetConfig.naturalBounds())) {
          mReadyForCompose = false;
        }
      }

      refLayer->ConnectReferentLayer(referent);

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
      if (resolvePlugins) {
        didResolvePlugins |=
          aCompositor->UpdatePluginWindowState(refLayer->GetReferentId());
      }
#endif
    });

  if (aHasRemoteContent) {
    *aHasRemoteContent = hasRemoteContent;
  }
  if (aResolvePlugins) {
    *aResolvePlugins = didResolvePlugins;
  }
}

void
AsyncCompositionManager::DetachRefLayers()
{
  if (!mLayerManager->GetRoot()) {
    return;
  }

  mReadyForCompose = false;

  ForEachNodePostOrder<ForwardIterator>(mLayerManager->GetRoot(),
    [&](Layer* layer)
    {
      RefLayer* refLayer = layer->AsRefLayer();
      if (!refLayer) {
        return;
      }

      const CompositorBridgeParent::LayerTreeState* state =
        CompositorBridgeParent::GetIndirectShadowTree(refLayer->GetReferentId());
      if (!state) {
        return;
      }

      Layer* referent = state->mRoot;
      if (referent) {
        refLayer->DetachReferentLayer(referent);
      }
    });
}

void
AsyncCompositionManager::ComputeRotation()
{
  if (!mTargetConfig.naturalBounds().IsEmpty()) {
    mWorldTransform =
      ComputeTransformForRotation(mTargetConfig.naturalBounds(),
                                  mTargetConfig.rotation());
  }
}

#ifdef DEBUG
static void
GetBaseTransform(Layer* aLayer, Matrix4x4* aTransform)
{
  // Start with the animated transform if there is one
  *aTransform =
    (aLayer->AsHostLayer()->GetShadowTransformSetByAnimation()
        ? aLayer->GetLocalTransform()
        : aLayer->GetTransform());
}
#endif

static void
TransformClipRect(Layer* aLayer,
                  const ParentLayerToParentLayerMatrix4x4& aTransform)
{
  MOZ_ASSERT(aTransform.Is2D());
  const Maybe<ParentLayerIntRect>& clipRect = aLayer->AsHostLayer()->GetShadowClipRect();
  if (clipRect) {
    ParentLayerIntRect transformed = TransformBy(aTransform, *clipRect);
    aLayer->AsHostLayer()->SetShadowClipRect(Some(transformed));
  }
}

// Similar to TransformFixedClip(), but only transforms the fixed part of the
// clip.
static void
TransformFixedClip(Layer* aLayer,
                   const ParentLayerToParentLayerMatrix4x4& aTransform,
                   AsyncCompositionManager::ClipParts& aClipParts)
{
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
static void
SetShadowTransform(Layer* aLayer, LayerToParentLayerMatrix4x4 aTransform)
{
  if (ContainerLayer* c = aLayer->AsContainerLayer()) {
    aTransform.PreScale(1.0f / c->GetPreXScale(),
                        1.0f / c->GetPreYScale(),
                        1);
  }
  aTransform.PostScale(1.0f / aLayer->GetPostXScale(),
                       1.0f / aLayer->GetPostYScale(),
                       1);
  aLayer->AsHostLayer()->SetShadowBaseTransform(aTransform.ToUnknownMatrix());
}

static void
TranslateShadowLayer(Layer* aLayer,
                     const ParentLayerPoint& aTranslation,
                     bool aAdjustClipRect,
                     AsyncCompositionManager::ClipPartsCache* aClipPartsCache)
{
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
    auto transform = ParentLayerToParentLayerMatrix4x4::Translation(aTranslation);
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
    // move along with the layer, so apply the translation to the mask layer too.
    if (Layer* maskLayer = aLayer->GetMaskLayer()) {
      TranslateShadowLayer(maskLayer, aTranslation, false, aClipPartsCache);
    }
  }
}

#ifdef DEBUG
static void
AccumulateLayerTransforms(Layer* aLayer,
                          Layer* aAncestor,
                          Matrix4x4& aMatrix)
{
  // Accumulate the transforms between this layer and the subtree root layer.
  for (Layer* l = aLayer; l && l != aAncestor; l = l->GetParent()) {
    Matrix4x4 transform;
    GetBaseTransform(l, &transform);
    aMatrix *= transform;
  }
}
#endif

static LayerPoint
GetLayerFixedMarginsOffset(Layer* aLayer,
                           const ScreenMargin& aFixedLayerMargins)
{
  // Work out the necessary translation, in root scrollable layer space.
  // Because fixed layer margins are stored relative to the root scrollable
  // layer, we can just take the difference between these values.
  LayerPoint translation;
  int32_t sides = aLayer->GetFixedPositionSides();

  if ((sides & eSideBitsLeftRight) == eSideBitsLeftRight) {
    translation.x += (aFixedLayerMargins.left - aFixedLayerMargins.right) / 2;
  } else if (sides & eSideBitsRight) {
    translation.x -= aFixedLayerMargins.right;
  } else if (sides & eSideBitsLeft) {
    translation.x += aFixedLayerMargins.left;
  }

  if ((sides & eSideBitsTopBottom) == eSideBitsTopBottom) {
    translation.y += (aFixedLayerMargins.top - aFixedLayerMargins.bottom) / 2;
  } else if (sides & eSideBitsBottom) {
    translation.y -= aFixedLayerMargins.bottom;
  } else if (sides & eSideBitsTop) {
    translation.y += aFixedLayerMargins.top;
  }

  return translation;
}

static gfxFloat
IntervalOverlap(gfxFloat aTranslation, gfxFloat aMin, gfxFloat aMax)
{
  // Determine the amount of overlap between the 1D vector |aTranslation|
  // and the interval [aMin, aMax].
  if (aTranslation > 0) {
    return std::max(0.0, std::min(aMax, aTranslation) - std::max(aMin, 0.0));
  } else {
    return std::min(0.0, std::max(aMin, aTranslation) - std::min(aMax, 0.0));
  }
}

/**
 * Finds the metrics on |aLayer| with scroll id |aScrollId|, and returns a
 * LayerMetricsWrapper representing the (layer, metrics) pair, or the null
 * LayerMetricsWrapper if no matching metrics could be found.
 */
static LayerMetricsWrapper
FindMetricsWithScrollId(Layer* aLayer, FrameMetrics::ViewID aScrollId)
{
  for (uint64_t i = 0; i < aLayer->GetScrollMetadataCount(); ++i) {
    if (aLayer->GetFrameMetrics(i).GetScrollId() == aScrollId) {
      return LayerMetricsWrapper(aLayer, i);
    }
  }
  return LayerMetricsWrapper();
}

/**
 * Checks whether the (layer, metrics) pair (aTransformedLayer, aTransformedMetrics)
 * is on the path from |aFixedLayer| to the metrics with scroll id
 * |aFixedWithRespectTo|, inclusive.
 */
static bool
AsyncTransformShouldBeUnapplied(Layer* aFixedLayer,
                                FrameMetrics::ViewID aFixedWithRespectTo,
                                Layer* aTransformedLayer,
                                FrameMetrics::ViewID aTransformedMetrics)
{
  LayerMetricsWrapper transformed = FindMetricsWithScrollId(aTransformedLayer, aTransformedMetrics);
  if (!transformed.IsValid()) {
    return false;
  }
  // It's important to start at the bottom, because the fixed layer itself
  // could have the transformed metrics, and they can be at the bottom.
  LayerMetricsWrapper current(aFixedLayer, LayerMetricsWrapper::StartAt::BOTTOM);
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
static Maybe<FrameMetrics::ViewID>
IsFixedOrSticky(Layer* aLayer)
{
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

void
AsyncCompositionManager::AlignFixedAndStickyLayers(Layer* aTransformedSubtreeRoot,
                                                   Layer* aStartTraversalAt,
                                                   FrameMetrics::ViewID aTransformScrollId,
                                                   const LayerToParentLayerMatrix4x4& aPreviousTransformForRoot,
                                                   const LayerToParentLayerMatrix4x4& aCurrentTransformForRoot,
                                                   const ScreenMargin& aFixedLayerMargins,
                                                   ClipPartsCache* aClipPartsCache)
{
  // We're going to be inverting |aCurrentTransformForRoot|.
  // If it's singular, there's nothing we can do.
  if (aCurrentTransformForRoot.IsSingular()) {
    return;
  }

  Layer* layer = aStartTraversalAt;
  bool needsAsyncTransformUnapplied = false;
  if (Maybe<FrameMetrics::ViewID> fixedTo = IsFixedOrSticky(layer)) {
    needsAsyncTransformUnapplied = AsyncTransformShouldBeUnapplied(layer,
        *fixedTo, aTransformedSubtreeRoot, aTransformScrollId);
  }

  // We want to process all the fixed and sticky descendants of
  // aTransformedSubtreeRoot. Once we do encounter such a descendant, we don't
  // need to recurse any deeper because the adjustment to the fixed or sticky
  // layer will apply to its subtree.
  if (!needsAsyncTransformUnapplied) {
    for (Layer* child = layer->GetFirstChild(); child; child = child->GetNextSibling()) {
      AlignFixedAndStickyLayers(aTransformedSubtreeRoot, child,
          aTransformScrollId, aPreviousTransformForRoot,
          aCurrentTransformForRoot, aFixedLayerMargins, aClipPartsCache);
    }
    return;
  }

  // Insert a translation so that the position of the anchor point is the same
  // before and after the change to the transform of aTransformedSubtreeRoot.

  // A transform creates a containing block for fixed-position descendants,
  // so there shouldn't be a transform in between the fixed layer and
  // the subtree root layer.
#ifdef DEBUG
  Matrix4x4 ancestorTransform;
  if (layer != aTransformedSubtreeRoot) {
    AccumulateLayerTransforms(layer->GetParent(), aTransformedSubtreeRoot,
                              ancestorTransform);
  }
  ancestorTransform.NudgeToIntegersFixedEpsilon();
  MOZ_ASSERT(ancestorTransform.IsIdentity());
#endif

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

  // Offset the layer's anchor point to make sure fixed position content
  // respects content document fixed position margins.
  LayerPoint offsetAnchor = anchor + GetLayerFixedMarginsOffset(layer, aFixedLayerMargins);

  // Additionally transform the anchor to compensate for the change
  // from the old transform to the new transform. We do
  // this by using the old transform to take the offset anchor back into
  // subtree root space, and then the inverse of the new transform
  // to bring it back to layer space.
  ParentLayerPoint offsetAnchorInSubtreeRootSpace =
      aPreviousTransformForRoot.TransformPoint(offsetAnchor);
  LayerPoint transformedAnchor = aCurrentTransformForRoot.Inverse()
      .TransformPoint(offsetAnchorInSubtreeRootSpace);

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
    translation.y = IntervalOverlap(translation.y, stickyOuter.Y(), stickyOuter.YMost()) -
                    IntervalOverlap(translation.y, stickyInner.Y(), stickyInner.YMost());
    translation.x = IntervalOverlap(translation.x, stickyOuter.X(), stickyOuter.XMost()) -
                    IntervalOverlap(translation.x, stickyInner.X(), stickyInner.XMost());
    unconsumedTranslation = translation - originalTranslation;
  }

  // Finally, apply the translation to the layer transform. Note that in cases
  // where the async transform on |aTransformedSubtreeRoot| affects this layer's
  // clip rect, we need to apply the same translation to said clip rect, so
  // that the effective transform on the clip rect takes it back to where it was
  // originally, had there been no async scroll.
  TranslateShadowLayer(layer, ViewAs<ParentLayerPixel>(translation,
      PixelCastJustification::NoTransformOnLayer), true, aClipPartsCache);

  // Propragate the unconsumed portion of the translation to descendant
  // fixed/sticky layers.
  if (unconsumedTranslation != LayerPoint()) {
    // Take the computations we performed to derive |translation| from
    // |aCurrentTransformForRoot|, and perform them in reverse, keeping other
    // quantities fixed, to come up with a new transform |newTransform| that
    // would produce |unconsumedTranslation|.
    LayerPoint newTransformedAnchor = unconsumedTranslation + anchor;
    ParentLayerPoint newTransformedAnchorInSubtreeRootSpace =
        aPreviousTransformForRoot.TransformPoint(newTransformedAnchor);
    LayerToParentLayerMatrix4x4 newTransform = aPreviousTransformForRoot;
    newTransform.PostTranslate(newTransformedAnchorInSubtreeRootSpace -
                               offsetAnchorInSubtreeRootSpace);

    // Propagate this new transform to our descendants as the new value of
    // |aCurrentTransformForRoot|. This allows them to consume the unconsumed
    // translation.
    for (Layer* child = layer->GetFirstChild(); child; child = child->GetNextSibling()) {
      AlignFixedAndStickyLayers(aTransformedSubtreeRoot, child, aTransformScrollId,
          aPreviousTransformForRoot, newTransform, aFixedLayerMargins, aClipPartsCache);
    }
  }
}

static void
ApplyAnimatedValue(Layer* aLayer,
                   CompositorAnimationStorage* aStorage,
                   nsCSSPropertyID aProperty,
                   const AnimationData& aAnimationData,
                   const RefPtr<RawServoAnimationValue>& aValue)
{
  if (!aValue) {
    // Return gracefully if we have no valid AnimationValue.
    return;
  }

  HostLayer* layerCompositor = aLayer->AsHostLayer();
  switch (aProperty) {
    case eCSSProperty_opacity: {
      float opacity = Servo_AnimationValue_GetOpacity(aValue);
      layerCompositor->SetShadowOpacity(opacity);
      layerCompositor->SetShadowOpacitySetByAnimation(true);
      aStorage->SetAnimatedValue(aLayer->GetCompositorAnimationsId(), opacity);

      break;
    }
    case eCSSProperty_transform: {
      RefPtr<nsCSSValueSharedList> list;
      Servo_AnimationValue_GetTransform(aValue, &list);
      const TransformData& transformData = aAnimationData.get_TransformData();
      nsPoint origin = transformData.origin();
      // we expect all our transform data to arrive in device pixels
      Point3D transformOrigin = transformData.transformOrigin();
      nsDisplayTransform::FrameTransformProperties props(Move(list),
                                                         transformOrigin);

      Matrix4x4 transform =
        nsDisplayTransform::GetResultingTransformMatrix(props, origin,
                                                        transformData.appUnitsPerDevPixel(),
                                                        0, &transformData.bounds());
      Matrix4x4 frameTransform = transform;

      // If our parent layer is a perspective layer, then the offset into reference
      // frame coordinates is already on that layer. If not, then we need to ask
      // for it to be added here.
      if (!aLayer->GetParent() ||
          !aLayer->GetParent()->GetTransformIsPerspective()) {
        nsLayoutUtils::PostTranslate(transform, origin,
                                     transformData.appUnitsPerDevPixel(),
                                     true);
      }

      if (ContainerLayer* c = aLayer->AsContainerLayer()) {
        transform.PostScale(c->GetInheritedXScale(), c->GetInheritedYScale(), 1);
      }

      layerCompositor->SetShadowBaseTransform(transform);
      layerCompositor->SetShadowTransformSetByAnimation(true);
      aStorage->SetAnimatedValue(aLayer->GetCompositorAnimationsId(),
                                 Move(transform), Move(frameTransform),
                                 transformData);
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled animated property");
  }
}

static bool
SampleAnimations(Layer* aLayer,
                 CompositorAnimationStorage* aStorage,
                 TimeStamp aTime)
{
  bool isAnimating = false;

  ForEachNode<ForwardIterator>(
      aLayer,
      [&] (Layer* layer)
      {
        AnimationArray& animations = layer->GetAnimations();
        if (animations.IsEmpty()) {
          return;
        }
        isAnimating = true;
        bool hasInEffectAnimations = false;
        RefPtr<RawServoAnimationValue> animationValue =
          layer->GetBaseAnimationStyle();
        AnimationHelper::SampleAnimationForEachNode(aTime,
                                                    animations,
                                                    layer->GetAnimationData(),
                                                    animationValue,
                                                    hasInEffectAnimations);
        if (hasInEffectAnimations) {
          Animation& animation = animations.LastElement();
          ApplyAnimatedValue(layer,
                             aStorage,
                             animation.property(),
                             animation.data(),
                             animationValue);
        }
      });

  return isAnimating;
}

void
AsyncCompositionManager::RecordShadowTransforms(Layer* aLayer)
{
  MOZ_ASSERT(gfxPrefs::CollectScrollTransforms());
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  ForEachNodePostOrder<ForwardIterator>(
      aLayer,
      [this] (Layer* layer)
      {
        for (uint32_t i = 0; i < layer->GetScrollMetadataCount(); i++) {
          if (!layer->GetFrameMetrics(i).IsScrollable()) {
            continue;
          }
          gfx::Matrix4x4 shadowTransform = layer->AsHostLayer()->GetShadowBaseTransform();
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

static AsyncTransformComponentMatrix
AdjustForClip(const AsyncTransformComponentMatrix& asyncTransform, Layer* aLayer)
{
  AsyncTransformComponentMatrix result = asyncTransform;

  // Container layers start at the origin, but they are clipped to where they
  // actually have content on the screen. The tree transform is meant to apply
  // to the clipped area. If the tree transform includes a scale component,
  // then applying it to container as-is will produce incorrect results. To
  // avoid this, translate the layer so that the clip rect starts at the origin,
  // apply the tree transform, and translate back.
  if (const Maybe<ParentLayerIntRect>& shadowClipRect = aLayer->AsHostLayer()->GetShadowClipRect()) {
    if (shadowClipRect->TopLeft() != ParentLayerIntPoint()) {  // avoid a gratuitous change of basis
      result.ChangeBasis(shadowClipRect->X(), shadowClipRect->Y(), 0);
    }
  }
  return result;
}

static void
ExpandRootClipRect(Layer* aLayer, const ScreenMargin& aFixedLayerMargins)
{
  // For Fennec we want to expand the root scrollable layer clip rect based on
  // the fixed position margins. In particular, we want this while the dynamic
  // toolbar is in the process of sliding offscreen and the area of the
  // LayerView visible to the user is larger than the viewport size that Gecko
  // knows about (and therefore larger than the clip rect). We could also just
  // clear the clip rect on aLayer entirely but this seems more precise.
  Maybe<ParentLayerIntRect> rootClipRect = aLayer->AsHostLayer()->GetShadowClipRect();
  if (rootClipRect && aFixedLayerMargins != ScreenMargin()) {
#ifndef MOZ_WIDGET_ANDROID
    // We should never enter here on anything other than Fennec, since
    // aFixedLayerMargins should be empty everywhere else.
    MOZ_ASSERT(false);
#endif
    ParentLayerRect rect(rootClipRect.value());
    rect.Deflate(ViewAs<ParentLayerPixel>(aFixedLayerMargins,
      PixelCastJustification::ScreenIsParentLayerForRoot));
    aLayer->AsHostLayer()->SetShadowClipRect(Some(RoundedOut(rect)));
  }
}

#ifdef MOZ_WIDGET_ANDROID
static void
MoveScrollbarForLayerMargin(Layer* aRoot, FrameMetrics::ViewID aRootScrollId,
                            const ScreenMargin& aFixedLayerMargins)
{
  // See bug 1223928 comment 9 - once we can detect the RCD with just the
  // isRootContent flag on the metrics, we can probably move this code into
  // ApplyAsyncTransformToScrollbar rather than having it as a separate
  // adjustment on the layer tree.
  Layer* scrollbar = BreadthFirstSearch<ReverseIterator>(aRoot,
    [aRootScrollId](Layer* aNode) {
      return (aNode->GetScrollbarData().mDirection.isSome() &&
              *aNode->GetScrollbarData().mDirection == ScrollDirection::eHorizontal &&
              aNode->GetScrollbarData().mTargetViewId == aRootScrollId);
    });
  if (scrollbar) {
    // Shift the horizontal scrollbar down into the new space exposed by the
    // dynamic toolbar hiding. Technically we should also scale the vertical
    // scrollbar a bit to expand into the new space but it's not as noticeable
    // and it would add a lot more complexity, so we're going with the "it's not
    // worth it" justification.
    TranslateShadowLayer(scrollbar, ParentLayerPoint(0, -aFixedLayerMargins.bottom), true, nullptr);
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

bool
AsyncCompositionManager::ApplyAsyncContentTransformToTree(Layer *aLayer,
                                                          bool* aOutFoundRoot)
{
  bool appliedTransform = false;
  std::stack<Maybe<ParentLayerIntRect>> stackDeferredClips;

  // Maps layers to their ClipParts. The parts are not stored individually
  // on the layer, but during AlignFixedAndStickyLayers we need access to
  // the individual parts for descendant layers.
  ClipPartsCache clipPartsCache;

  ForEachNode<ForwardIterator>(
      aLayer,
      [&stackDeferredClips] (Layer* layer)
      {
        stackDeferredClips.push(Maybe<ParentLayerIntRect>());
      },
      [this, &aOutFoundRoot, &stackDeferredClips, &appliedTransform, &clipPartsCache] (Layer* layer)
      {
        Maybe<ParentLayerIntRect> clipDeferredFromChildren = stackDeferredClips.top();
        stackDeferredClips.pop();
        LayerToParentLayerMatrix4x4 oldTransform = layer->GetTransformTyped() *
            AsyncTransformMatrix();

        AsyncTransformComponentMatrix combinedAsyncTransform;
        bool hasAsyncTransform = false;
        // Only set on the root layer for Android.
        ScreenMargin fixedLayerMargins;

        // Each layer has multiple clips:
        //  - Its local clip, which is fixed to the layer contents, i.e. it moves
        //    with those async transforms which the layer contents move with.
        //  - Its scrolled clip, which moves with all async transforms.
        //  - For each ScrollMetadata on the layer, a scroll clip. This includes
        //    the composition bounds and any other clips induced by layout. This
        //    moves with async transforms from ScrollMetadatas above it.
        // In this function, these clips are combined into two shadow clip parts:
        //  - The fixed clip, which consists of the local clip only, initially
        //    transformed by all async transforms.
        //  - The scrolled clip, which consists of the other clips, transformed by
        //    the appropriate transforms.
        // These two parts are kept separate for now, because for fixed layers, we
        // need to adjust the fixed clip (to cancel out some async transforms).
        // The parts are kept in a cache which is cleared at the beginning of every
        // composite.
        // The final shadow clip for the layer is the intersection of the (possibly
        // adjusted) fixed clip and the scrolled clip.
        ClipParts& clipParts = clipPartsCache[layer];
        clipParts.mFixedClip = layer->GetClipRect();
        clipParts.mScrolledClip = layer->GetScrolledClipRect();

        // If we are a perspective transform ContainerLayer, apply the clip deferred
        // from our child (if there is any) before we iterate over our frame metrics,
        // because this clip is subject to all async transforms of this layer.
        // Since this clip came from the a scroll clip on the child, it becomes part
        // of our scrolled clip.
        clipParts.mScrolledClip = IntersectMaybeRects(
            clipDeferredFromChildren, clipParts.mScrolledClip);

        // The transform of a mask layer is relative to the masked layer's parent
        // layer. So whenever we apply an async transform to a layer, we need to
        // apply that same transform to the layer's own mask layer.
        // A layer can also have "ancestor" mask layers for any rounded clips from
        // its ancestor scroll frames. A scroll frame mask layer only needs to be
        // async transformed for async scrolls of this scroll frame's ancestor
        // scroll frames, not for async scrolls of this scroll frame itself.
        // In the loop below, we iterate over scroll frames from inside to outside.
        // At each iteration, this array contains the layer's ancestor mask layers
        // of all scroll frames inside the current one.
        nsTArray<Layer*> ancestorMaskLayers;

        // The layer's scrolled clip can have an ancestor mask layer as well,
        // which is moved by all async scrolls on this layer.
        if (const Maybe<LayerClip>& scrolledClip = layer->GetScrolledClip()) {
          if (scrolledClip->GetMaskLayerIndex()) {
            ancestorMaskLayers.AppendElement(
                layer->GetAncestorMaskLayerAt(*scrolledClip->GetMaskLayerIndex()));
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

            AsyncTransform asyncTransformWithoutOverscroll =
                sampler->GetCurrentAsyncTransform(wrapper);
            AsyncTransformComponentMatrix overscrollTransform =
                sampler->GetOverscrollTransform(wrapper);
            AsyncTransformComponentMatrix asyncTransform =
                AsyncTransformComponentMatrix(asyncTransformWithoutOverscroll)
              * overscrollTransform;

            if (!layer->IsScrollableWithoutContent()) {
              sampler->MarkAsyncTransformAppliedToContent(wrapper);
            }

            const ScrollMetadata& scrollMetadata = wrapper.Metadata();

#if defined(MOZ_WIDGET_ANDROID)
            // If we find a metrics which is the root content doc, use that. If not, use
            // the root layer. Since this function recurses on children first we should
            // only end up using the root layer if the entire tree was devoid of a
            // root content metrics. This is a temporary solution; in the long term we
            // should not need the root content metrics at all. See bug 1201529 comment
            // 6 for details.
            if (!(*aOutFoundRoot)) {
              *aOutFoundRoot = metrics.IsRootContent() ||       /* RCD */
                    (layer->GetParent() == nullptr &&          /* rootmost metrics */
                     i + 1 >= layer->GetScrollMetadataCount());
              if (*aOutFoundRoot) {
                mRootScrollableId = metrics.GetScrollId();
                Compositor* compositor = mLayerManager->GetCompositor();
                if (CompositorBridgeParent* bridge = compositor->GetCompositorBridgeParent()) {
                  AndroidDynamicToolbarAnimator* animator = bridge->GetAndroidDynamicToolbarAnimator();
                  MOZ_ASSERT(animator);
                  if (mIsFirstPaint) {
                    animator->UpdateRootFrameMetrics(metrics);
                    animator->FirstPaint();
                    mIsFirstPaint = false;
                  }
                  if (mLayersUpdated) {
                    animator->NotifyLayersUpdated();
                    mLayersUpdated = false;
                  }
                  // If this is not actually the root content then the animator is not getting updated in AsyncPanZoomController::NotifyLayersUpdated
                  // because the root content document is not scrollable. So update it here so it knows if the root composition size has changed.
                  if (!metrics.IsRootContent()) {
                    animator->MaybeUpdateCompositionSizeAndRootFrameMetrics(metrics);
                  }
                }
                fixedLayerMargins = mFixedLayerMargins;
              }
            }
#else
            *aOutFoundRoot = false;
            // Non-Android platforms still care about this flag being cleared after
            // the first call to TransformShadowTree().
            mIsFirstPaint = false;
#endif

            // Transform the current local clips by this APZC's async transform. If we're
            // using containerful scrolling, then the clip is not part of the scrolled
            // frame and should not be transformed.
            if (!scrollMetadata.UsesContainerScrolling()) {
              MOZ_ASSERT(asyncTransform.Is2D());
              if (clipParts.mFixedClip) {
                *clipParts.mFixedClip = TransformBy(asyncTransform, *clipParts.mFixedClip);
              }
              if (clipParts.mScrolledClip) {
                *clipParts.mScrolledClip = TransformBy(asyncTransform, *clipParts.mScrolledClip);
              }
            }
            // Note: we don't set the layer's shadow clip rect property yet;
            // AlignFixedAndStickyLayers will use the clip parts from the clip parts
            // cache.

            combinedAsyncTransform *= asyncTransform;

            // For the purpose of aligning fixed and sticky layers, we disregard
            // the overscroll transform as well as any OMTA transform when computing the
            // 'aCurrentTransformForRoot' parameter. This ensures that the overscroll
            // and OMTA transforms are not unapplied, and therefore that the visual
            // effects apply to fixed and sticky layers. We do this by using
            // GetTransform() as the base transform rather than GetLocalTransform(),
            // which would include those factors.
            LayerToParentLayerMatrix4x4 transformWithoutOverscrollOrOmta =
                layer->GetTransformTyped()
              * CompleteAsyncTransform(
                  AdjustForClip(asyncTransformWithoutOverscroll, layer));

            AlignFixedAndStickyLayers(layer, layer, metrics.GetScrollId(), oldTransform,
                                      transformWithoutOverscrollOrOmta, fixedLayerMargins,
                                      &clipPartsCache);

            // Combine the local clip with the ancestor scrollframe clip. This is not
            // included in the async transform above, since the ancestor clip should not
            // move with this APZC.
            if (scrollMetadata.HasScrollClip()) {
              ParentLayerIntRect clip = scrollMetadata.ScrollClip().GetClipRect();
              if (layer->GetParent() && layer->GetParent()->GetTransformIsPerspective()) {
                // If our parent layer has a perspective transform, we want to apply
                // our scroll clip to it instead of to this layer (see bug 1168263).
                // A layer with a perspective transform shouldn't have multiple
                // children with FrameMetrics, nor a child with multiple FrameMetrics.
                // (A child with multiple FrameMetrics would mean that there's *another*
                // scrollable element between the one with the CSS perspective and the
                // transformed element. But you'd have to use preserve-3d on the inner
                // scrollable element in order to have the perspective apply to the
                // transformed child, and preserve-3d is not supported on scrollable
                // elements, so this case can't occur.)
                MOZ_ASSERT(!stackDeferredClips.top());
                stackDeferredClips.top().emplace(clip);
              } else {
                clipParts.mScrolledClip = IntersectMaybeRects(Some(clip),
                    clipParts.mScrolledClip);
              }
            }

            // Do the same for the ancestor mask layers: ancestorMaskLayers contains
            // the ancestor mask layers for scroll frames *inside* the current scroll
            // frame, so these are the ones we need to shift by our async transform.
            for (Layer* ancestorMaskLayer : ancestorMaskLayers) {
              SetShadowTransform(ancestorMaskLayer,
                  ancestorMaskLayer->GetLocalTransformTyped() * asyncTransform);
            }

            // Append the ancestor mask layer for this scroll frame to ancestorMaskLayers.
            if (scrollMetadata.HasScrollClip()) {
              const LayerClip& scrollClip = scrollMetadata.ScrollClip();
              if (scrollClip.GetMaskLayerIndex()) {
                size_t maskLayerIndex = scrollClip.GetMaskLayerIndex().value();
                Layer* ancestorMaskLayer = layer->GetAncestorMaskLayerAt(maskLayerIndex);
                ancestorMaskLayers.AppendElement(ancestorMaskLayer);
              }
            }
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
          // Apply the APZ transform on top of GetLocalTransform() here (rather than
          // GetTransform()) in case the OMTA code in SampleAnimations already set a
          // shadow transform; in that case we want to apply ours on top of that one
          // rather than clobber it.
          SetShadowTransform(layer,
              layer->GetLocalTransformTyped()
            * AdjustForClip(combinedAsyncTransform, layer));

          // Do the same for the layer's own mask layer, if it has one.
          if (Layer* maskLayer = layer->GetMaskLayer()) {
            SetShadowTransform(maskLayer,
                maskLayer->GetLocalTransformTyped() * combinedAsyncTransform);
          }

          appliedTransform = true;
        }

        ExpandRootClipRect(layer, fixedLayerMargins);

        if (layer->GetScrollbarData().mScrollbarLayerType == layers::ScrollbarLayerType::Thumb) {
          ApplyAsyncTransformToScrollbar(layer);
        }
      });

  return appliedTransform;
}

static bool
LayerIsScrollbarTarget(const LayerMetricsWrapper& aTarget, Layer* aScrollbar)
{
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

static void
ApplyAsyncTransformToScrollbarForContent(const RefPtr<APZSampler>& aSampler,
                                         Layer* aScrollbar,
                                         const LayerMetricsWrapper& aContent,
                                         bool aScrollbarIsDescendant)
{
  AsyncTransformComponentMatrix clipTransform;

  MOZ_ASSERT(aSampler);
  LayerToParentLayerMatrix4x4 transform =
      aSampler->ComputeTransformForScrollThumb(
          aScrollbar->GetLocalTransformTyped(),
          aContent,
          aScrollbar->GetScrollbarData(),
          aScrollbarIsDescendant,
          &clipTransform);

  if (aScrollbarIsDescendant) {
    // We also need to make a corresponding change on the clip rect of all the
    // layers on the ancestor chain from the scrollbar layer up to but not
    // including the layer with the async transform. Otherwise the scrollbar
    // shifts but gets clipped and so appears to flicker.
    for (Layer* ancestor = aScrollbar; ancestor != aContent.GetLayer(); ancestor = ancestor->GetParent()) {
      TransformClipRect(ancestor, clipTransform);
    }
  }

  SetShadowTransform(aScrollbar, transform);
}

static LayerMetricsWrapper
FindScrolledLayerForScrollbar(Layer* aScrollbar, bool* aOutIsAncestor)
{
  // First check if the scrolled layer is an ancestor of the scrollbar layer.
  LayerMetricsWrapper root(aScrollbar->Manager()->GetRoot());
  LayerMetricsWrapper prevAncestor(aScrollbar);
  LayerMetricsWrapper scrolledLayer;

  for (LayerMetricsWrapper ancestor(aScrollbar); ancestor; ancestor = ancestor.GetParent()) {
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
  ForEachNode<ForwardIterator>(
      root,
      [&root, &scrolledLayer, &aScrollbar](LayerMetricsWrapper aLayerMetrics)
      {
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
      }
  );
  return scrolledLayer;
}

void
AsyncCompositionManager::ApplyAsyncTransformToScrollbar(Layer* aLayer)
{
  // If this layer corresponds to a scrollbar, then there should be a layer that
  // is a previous sibling or a parent that has a matching ViewID on its FrameMetrics.
  // That is the content that this scrollbar is for. We pick up the transient
  // async transform from that layer and use it to update the scrollbar position.
  // Note that it is possible that the content layer is no longer there; in
  // this case we don't need to do anything because there can't be an async
  // transform on the content.
  bool isAncestor = false;
  const LayerMetricsWrapper& scrollTarget = FindScrolledLayerForScrollbar(aLayer, &isAncestor);
  if (scrollTarget) {
    ApplyAsyncTransformToScrollbarForContent(mCompositorBridge->GetAPZSampler(),
        aLayer, scrollTarget, isAncestor);
  }
}

void
AsyncCompositionManager::GetFrameUniformity(FrameUniformityData* aOutData)
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  mLayerTransformRecorder.EndTest(aOutData);
}

bool
AsyncCompositionManager::TransformShadowTree(TimeStamp aCurrentFrame,
                                             TimeDuration aVsyncRate,
                                             TransformsToSkip aSkip)
{
  AUTO_PROFILER_LABEL("AsyncCompositionManager::TransformShadowTree", GRAPHICS);

  Layer* root = mLayerManager->GetRoot();
  if (!root) {
    return false;
  }

  CompositorAnimationStorage* storage =
    mCompositorBridge->GetAnimationStorage();
  // First, compute and set the shadow transforms from OMT animations.
  // NB: we must sample animations *before* sampling pan/zoom
  // transforms.
  // Use a previous vsync time to make main thread animations and compositor
  // more in sync with each other.
  // On the initial frame we use aVsyncTimestamp here so the timestamp on the
  // second frame are the same as the initial frame, but it does not matter.
  bool wantNextFrame =
    SampleAnimations(root,
                     storage,
                     !mPreviousFrameTimeStamp.IsNull() ?
                       mPreviousFrameTimeStamp : aCurrentFrame);

  if (!wantNextFrame) {
    // Clean up the CompositorAnimationStorage because
    // there are no active animations running
    storage->Clear();
  }

  // Advance animations to the next expected vsync timestamp, if we can
  // get it.
  TimeStamp nextFrame = aCurrentFrame;

  MOZ_ASSERT(aVsyncRate != TimeDuration::Forever());
  if (aVsyncRate != TimeDuration::Forever()) {
    nextFrame += aVsyncRate;
  }

#if defined(MOZ_WIDGET_ANDROID)
  Compositor* compositor = mLayerManager->GetCompositor();
  if (CompositorBridgeParent* bridge = compositor->GetCompositorBridgeParent()) {
    AndroidDynamicToolbarAnimator* animator = bridge->GetAndroidDynamicToolbarAnimator();
    MOZ_ASSERT(animator);
    wantNextFrame |= animator->UpdateAnimation(nextFrame);
  }
#endif // defined(MOZ_WIDGET_ANDROID)

  // Reset the previous time stamp if we don't already have any running
  // animations to avoid using the time which is far behind for newly
  // started animations.
  mPreviousFrameTimeStamp = wantNextFrame ? aCurrentFrame : TimeStamp();

  if (!(aSkip & TransformsToSkip::APZ)) {
    // FIXME/bug 775437: unify this interface with the ~native-fennec
    // derived code
    //
    // Attempt to apply an async content transform to any layer that has
    // an async pan zoom controller (which means that it is rendered
    // async using Gecko). If this fails, fall back to transforming the
    // primary scrollable layer.  "Failing" here means that we don't
    // find a frame that is async scrollable.  Note that the fallback
    // code also includes Fennec which is rendered async.  Fennec uses
    // its own platform-specific async rendering that is done partially
    // in Gecko and partially in Java.
    bool foundRoot = false;
    if (ApplyAsyncContentTransformToTree(root, &foundRoot)) {
#if defined(MOZ_WIDGET_ANDROID)
      MOZ_ASSERT(foundRoot);
      if (foundRoot && mFixedLayerMargins != ScreenMargin()) {
        MoveScrollbarForLayerMargin(root, mRootScrollableId, mFixedLayerMargins);
      }
#endif
    }

    bool apzAnimating = false;
    if (RefPtr<APZSampler> apz = mCompositorBridge->GetAPZSampler()) {
      apzAnimating = apz->SampleAnimations(LayerMetricsWrapper(root), nextFrame);
    }
    wantNextFrame |= apzAnimating;
  }

  HostLayer* rootComposite = root->AsHostLayer();

  gfx::Matrix4x4 trans = rootComposite->GetShadowBaseTransform();
  trans *= gfx::Matrix4x4::From2D(mWorldTransform);
  rootComposite->SetShadowBaseTransform(trans);

  if (gfxPrefs::CollectScrollTransforms()) {
    RecordShadowTransforms(root);
  }

  return wantNextFrame;
}

#if defined(MOZ_WIDGET_ANDROID)
void
AsyncCompositionManager::SetFixedLayerMargins(ScreenIntCoord aTop, ScreenIntCoord aBottom)
{
  mFixedLayerMargins.top = aTop;
  mFixedLayerMargins.bottom = aBottom;
}
#endif // defined(MOZ_WIDGET_ANDROID)

} // namespace layers
} // namespace mozilla
