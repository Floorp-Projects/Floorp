/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/AsyncCompositionManager.h"
#include <stdint.h>                     // for uint32_t
#include "apz/src/AsyncPanZoomController.h"
#include "FrameMetrics.h"               // for FrameMetrics
#include "LayerManagerComposite.h"      // for LayerManagerComposite, etc
#include "Layers.h"                     // for Layer, ContainerLayer, etc
#include "gfxPoint.h"                   // for gfxPoint, gfxSize
#include "mozilla/StyleAnimationValue.h" // for StyleAnimationValue, etc
#include "mozilla/WidgetUtils.h"        // for ComputeTransformForRotation
#include "mozilla/dom/KeyframeEffect.h" // for KeyframeEffectReadOnly
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Point.h"          // for RoundedToInt, PointTyped
#include "mozilla/gfx/Rect.h"           // for RoundedToInt, RectTyped
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorParent.h" // for CompositorParent, etc
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
#if defined(MOZ_WIDGET_ANDROID)
# include <android/log.h>
# include "AndroidBridge.h"
#endif
#include "GeckoProfiler.h"
#include "FrameUniformityData.h"

struct nsCSSValueSharedList;

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

enum Op { Resolve, Detach };

static bool
IsSameDimension(dom::ScreenOrientation o1, dom::ScreenOrientation o2)
{
  bool isO1portrait = (o1 == dom::eScreenOrientation_PortraitPrimary || o1 == dom::eScreenOrientation_PortraitSecondary);
  bool isO2portrait = (o2 == dom::eScreenOrientation_PortraitPrimary || o2 == dom::eScreenOrientation_PortraitSecondary);
  return !(isO1portrait ^ isO2portrait);
}

static bool
ContentMightReflowOnOrientationChange(const IntRect& rect)
{
  return rect.width != rect.height;
}

template<Op OP>
static void
WalkTheTree(Layer* aLayer,
            bool& aReady,
            const TargetConfig& aTargetConfig)
{
  if (RefLayer* ref = aLayer->AsRefLayer()) {
    if (const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(ref->GetReferentId())) {
      if (Layer* referent = state->mRoot) {
        if (!ref->GetVisibleRegion().IsEmpty()) {
          dom::ScreenOrientation chromeOrientation = aTargetConfig.orientation();
          dom::ScreenOrientation contentOrientation = state->mTargetConfig.orientation();
          if (!IsSameDimension(chromeOrientation, contentOrientation) &&
              ContentMightReflowOnOrientationChange(aTargetConfig.naturalBounds())) {
            aReady = false;
          }
        }

        if (OP == Resolve) {
          ref->ConnectReferentLayer(referent);
        } else {
          ref->DetachReferentLayer(referent);
          WalkTheTree<OP>(referent, aReady, aTargetConfig);
        }
      }
    }
  }
  for (Layer* child = aLayer->GetFirstChild();
       child; child = child->GetNextSibling()) {
    WalkTheTree<OP>(child, aReady, aTargetConfig);
  }
}

AsyncCompositionManager::AsyncCompositionManager(LayerManagerComposite* aManager)
  : mLayerManager(aManager)
  , mIsFirstPaint(true)
  , mLayersUpdated(false)
  , mReadyForCompose(true)
{
}

AsyncCompositionManager::~AsyncCompositionManager()
{
}

void
AsyncCompositionManager::ResolveRefLayers()
{
  if (!mLayerManager->GetRoot()) {
    return;
  }

  mReadyForCompose = true;
  WalkTheTree<Resolve>(mLayerManager->GetRoot(),
                       mReadyForCompose,
                       mTargetConfig);
}

void
AsyncCompositionManager::DetachRefLayers()
{
  if (!mLayerManager->GetRoot()) {
    return;
  }
  WalkTheTree<Detach>(mLayerManager->GetRoot(),
                      mReadyForCompose,
                      mTargetConfig);
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

static void
GetBaseTransform(Layer* aLayer, Matrix4x4* aTransform)
{
  // Start with the animated transform if there is one
  *aTransform =
    (aLayer->AsLayerComposite()->GetShadowTransformSetByAnimation()
        ? aLayer->GetLocalTransform()
        : aLayer->GetTransform());
}

static void
TransformClipRect(Layer* aLayer,
                  const Matrix4x4& aTransform)
{
  const Maybe<ParentLayerIntRect>& clipRect = aLayer->AsLayerComposite()->GetShadowClipRect();
  if (clipRect) {
    ParentLayerIntRect transformed = TransformTo<ParentLayerPixel>(aTransform, *clipRect);
    aLayer->AsLayerComposite()->SetShadowClipRect(Some(transformed));
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
SetShadowTransform(Layer* aLayer, Matrix4x4 aTransform)
{
  if (ContainerLayer* c = aLayer->AsContainerLayer()) {
    aTransform.PreScale(1.0f / c->GetPreXScale(),
                        1.0f / c->GetPreYScale(),
                        1);
  }
  aTransform.PostScale(1.0f / aLayer->GetPostXScale(),
                       1.0f / aLayer->GetPostYScale(),
                       1);
  aLayer->AsLayerComposite()->SetShadowTransform(aTransform);
}

static void
TranslateShadowLayer(Layer* aLayer,
                     const gfxPoint& aTranslation,
                     bool aAdjustClipRect)
{
  // This layer might also be a scrollable layer and have an async transform.
  // To make sure we don't clobber that, we start with the shadow transform.
  // (i.e. GetLocalTransform() instead of GetTransform()).
  // Note that the shadow transform is reset on every frame of composition so
  // we don't have to worry about the adjustments compounding over successive
  // frames.
  Matrix4x4 layerTransform = aLayer->GetLocalTransform();

  // Apply the translation to the layer transform.
  layerTransform.PostTranslate(aTranslation.x, aTranslation.y, 0);

  SetShadowTransform(aLayer, layerTransform);
  aLayer->AsLayerComposite()->SetShadowTransformSetByAnimation(false);

  if (aAdjustClipRect) {
    TransformClipRect(aLayer, Matrix4x4::Translation(aTranslation.x, aTranslation.y, 0));
  }

  // If a fixed- or sticky-position layer has a mask layer, that mask should
  // move along with the layer, so apply the translation to the mask layer too.
  if (Layer* maskLayer = aLayer->GetMaskLayer()) {
    TranslateShadowLayer(maskLayer, aTranslation, false);
  }
}

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

static LayerPoint
GetLayerFixedMarginsOffset(Layer* aLayer,
                           const LayerMargin& aFixedLayerMargins)
{
  // Work out the necessary translation, in root scrollable layer space.
  // Because fixed layer margins are stored relative to the root scrollable
  // layer, we can just take the difference between these values.
  LayerPoint translation;
  const LayerPoint& anchor = aLayer->GetFixedPositionAnchor();
  const LayerMargin& fixedMargins = aLayer->GetFixedPositionMargins();

  if (fixedMargins.left >= 0) {
    if (anchor.x > 0) {
      translation.x -= aFixedLayerMargins.right - fixedMargins.right;
    } else {
      translation.x += aFixedLayerMargins.left - fixedMargins.left;
    }
  }

  if (fixedMargins.top >= 0) {
    if (anchor.y > 0) {
      translation.y -= aFixedLayerMargins.bottom - fixedMargins.bottom;
    } else {
      translation.y += aFixedLayerMargins.top - fixedMargins.top;
    }
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

void
AsyncCompositionManager::AlignFixedAndStickyLayers(Layer* aLayer,
                                                   Layer* aTransformedSubtreeRoot,
                                                   FrameMetrics::ViewID aTransformScrollId,
                                                   const Matrix4x4& aPreviousTransformForRoot,
                                                   const Matrix4x4& aCurrentTransformForRoot,
                                                   const LayerMargin& aFixedLayerMargins)
{
  // If aLayer == aTransformedSubtreeRoot, then treat aLayer as fixed relative
  // to the ancestor scrollable layer rather than relative to itself.
  bool isRootFixed = aLayer->GetIsFixedPosition() &&
    aLayer != aTransformedSubtreeRoot &&
    !aLayer->GetParent()->GetIsFixedPosition();
  bool isStickyForSubtree = aLayer->GetIsStickyPosition() &&
    aLayer->GetStickyScrollContainerId() == aTransformScrollId;
  bool isFixedOrSticky = (isRootFixed || isStickyForSubtree);

  // We want to process all the fixed and sticky children of
  // aTransformedSubtreeRoot. Also, once we do encounter such a child, we don't
  // need to recurse any deeper because the fixed layers are relative to their
  // nearest scrollable layer.
  if (!isFixedOrSticky) {
    // ApplyAsyncContentTransformToTree will call this function again for
    // nested scrollable layers, so we don't need to recurse if the layer is
    // scrollable.
    if (aLayer == aTransformedSubtreeRoot || !aLayer->HasScrollableFrameMetrics()) {
      for (Layer* child = aLayer->GetFirstChild(); child; child = child->GetNextSibling()) {
        AlignFixedAndStickyLayers(child, aTransformedSubtreeRoot, aTransformScrollId,
                                  aPreviousTransformForRoot,
                                  aCurrentTransformForRoot, aFixedLayerMargins);
      }
    }
    return;
  }

  // Insert a translation so that the position of the anchor point is the same
  // before and after the change to the transform of aTransformedSubtreeRoot.

  // Accumulate the transforms between this layer and the subtree root layer.
  Matrix4x4 ancestorTransform;
  AccumulateLayerTransforms(aLayer->GetParent(), aTransformedSubtreeRoot,
                            ancestorTransform);

  // Calculate the cumulative transforms between the subtree root with the
  // old transform and the current transform.
  Matrix4x4 oldCumulativeTransform = ancestorTransform * aPreviousTransformForRoot;
  Matrix4x4 newCumulativeTransform = ancestorTransform * aCurrentTransformForRoot;
  if (newCumulativeTransform.IsSingular()) {
    return;
  }
  Matrix4x4 newCumulativeTransformInverse = newCumulativeTransform.Inverse();

  // Now work out the translation necessary to make sure the layer doesn't
  // move given the new sub-tree root transform.
  Matrix4x4 layerTransform;
  GetBaseTransform(aLayer, &layerTransform);

  // Calculate any offset necessary, in previous transform sub-tree root
  // space. This is used to make sure fixed position content respects
  // content document fixed position margins.
  LayerPoint offsetInOldSubtreeLayerSpace = GetLayerFixedMarginsOffset(aLayer, aFixedLayerMargins);

  // Add the above offset to the anchor point so we can offset the layer by
  // and amount that's specified in old subtree layer space.
  const LayerPoint& anchorInOldSubtreeLayerSpace = aLayer->GetFixedPositionAnchor();
  LayerPoint offsetAnchorInOldSubtreeLayerSpace = anchorInOldSubtreeLayerSpace + offsetInOldSubtreeLayerSpace;

  // Add the local layer transform to the two points to make the equation
  // below this section more convenient.
  Point anchor(anchorInOldSubtreeLayerSpace.x, anchorInOldSubtreeLayerSpace.y);
  Point offsetAnchor(offsetAnchorInOldSubtreeLayerSpace.x, offsetAnchorInOldSubtreeLayerSpace.y);
  Point locallyTransformedAnchor = layerTransform * anchor;
  Point locallyTransformedOffsetAnchor = layerTransform * offsetAnchor;

  // Transforming the locallyTransformedAnchor by oldCumulativeTransform
  // returns the layer's anchor point relative to the parent of
  // aTransformedSubtreeRoot, before the new transform was applied.
  // Then, applying newCumulativeTransformInverse maps that point relative
  // to the layer's parent, which is the same coordinate space as
  // locallyTransformedAnchor again, allowing us to subtract them and find
  // out the offset necessary to make sure the layer stays stationary.
  Point oldAnchorPositionInNewSpace =
    newCumulativeTransformInverse * (oldCumulativeTransform * locallyTransformedOffsetAnchor);
  Point translation = oldAnchorPositionInNewSpace - locallyTransformedAnchor;

  if (aLayer->GetIsStickyPosition()) {
    // For sticky positioned layers, the difference between the two rectangles
    // defines a pair of translation intervals in each dimension through which
    // the layer should not move relative to the scroll container. To
    // accomplish this, we limit each dimension of the |translation| to that
    // part of it which overlaps those intervals.
    const LayerRect& stickyOuter = aLayer->GetStickyScrollRangeOuter();
    const LayerRect& stickyInner = aLayer->GetStickyScrollRangeInner();

    translation.y = IntervalOverlap(translation.y, stickyOuter.y, stickyOuter.YMost()) -
                    IntervalOverlap(translation.y, stickyInner.y, stickyInner.YMost());
    translation.x = IntervalOverlap(translation.x, stickyOuter.x, stickyOuter.XMost()) -
                    IntervalOverlap(translation.x, stickyInner.x, stickyInner.XMost());
  }

  // Finally, apply the translation to the layer transform. Note that in
  // general we need to apply the same translation to the layer's clip rect, so
  // that the effective transform on the clip rect takes it back to where it was
  // originally, had there been no async scroll. In the case where the
  // fixed/sticky layer is the same as aTransformedSubtreeRoot, then the clip
  // rect is not affected by the scroll-induced async scroll transform anyway
  // (since the clip is applied post-transform) so we don't need to make the
  // adjustment.
  TranslateShadowLayer(aLayer, ThebesPoint(translation), aLayer != aTransformedSubtreeRoot);
}

static void
SampleValue(float aPortion, Animation& aAnimation, StyleAnimationValue& aStart,
            StyleAnimationValue& aEnd, Animatable* aValue)
{
  StyleAnimationValue interpolatedValue;
  NS_ASSERTION(aStart.GetUnit() == aEnd.GetUnit() ||
               aStart.GetUnit() == StyleAnimationValue::eUnit_None ||
               aEnd.GetUnit() == StyleAnimationValue::eUnit_None,
               "Must have same unit");
  StyleAnimationValue::Interpolate(aAnimation.property(), aStart, aEnd,
                                aPortion, interpolatedValue);
  if (aAnimation.property() == eCSSProperty_opacity) {
    *aValue = interpolatedValue.GetFloatValue();
    return;
  }

  nsCSSValueSharedList* interpolatedList =
    interpolatedValue.GetCSSValueSharedListValue();

  TransformData& data = aAnimation.data().get_TransformData();
  nsPoint origin = data.origin();
  // we expect all our transform data to arrive in device pixels
  Point3D transformOrigin = data.transformOrigin();
  Point3D perspectiveOrigin = data.perspectiveOrigin();
  nsDisplayTransform::FrameTransformProperties props(interpolatedList,
                                                     transformOrigin,
                                                     perspectiveOrigin,
                                                     data.perspective());
  Matrix4x4 transform =
    nsDisplayTransform::GetResultingTransformMatrix(props, origin,
                                                    data.appUnitsPerDevPixel(),
                                                    &data.bounds());
  Point3D scaledOrigin =
    Point3D(NS_round(NSAppUnitsToFloatPixels(origin.x, data.appUnitsPerDevPixel())),
            NS_round(NSAppUnitsToFloatPixels(origin.y, data.appUnitsPerDevPixel())),
            0.0f);

  transform.PreTranslate(scaledOrigin);

  InfallibleTArray<TransformFunction> functions;
  functions.AppendElement(TransformMatrix(transform));
  *aValue = functions;
}

static bool
SampleAnimations(Layer* aLayer, TimeStamp aPoint)
{
  AnimationArray& animations = aLayer->GetAnimations();
  InfallibleTArray<AnimData>& animationData = aLayer->GetAnimationData();

  bool activeAnimations = false;

  // Process in order, since later animations override earlier ones.
  for (size_t i = 0, iEnd = animations.Length(); i < iEnd; ++i) {
    Animation& animation = animations[i];
    AnimData& animData = animationData[i];

    activeAnimations = true;

    MOZ_ASSERT(!animation.startTime().IsNull(),
               "Failed to resolve start time of pending animations");
    TimeDuration elapsedDuration =
      (aPoint - animation.startTime()).MultDouble(animation.playbackRate());
    // Skip animations that are yet to start.
    //
    // Currently, this should only happen when the refresh driver is under test
    // control and is made to produce a time in the past or is restored from
    // test control causing it to jump backwards in time.
    //
    // Since activeAnimations is true, this could mean we keep compositing
    // unnecessarily during the delay, but so long as this only happens while
    // the refresh driver is under test control that should be ok.
    if (elapsedDuration.ToSeconds() < 0) {
      continue;
    }

    AnimationTiming timing;
    timing.mIterationDuration = animation.duration();
    // Currently animations run on the compositor have their delay factored
    // into their start time, hence the delay is effectively zero.
    timing.mDelay = TimeDuration(0);
    timing.mIterationCount = animation.iterationCount();
    timing.mDirection = animation.direction();
    // Animations typically only run on the compositor during their active
    // interval but if we end up sampling them outside that range (for
    // example, while they are waiting to be removed) we currently just
    // assume that we should fill.
    timing.mFillMode = NS_STYLE_ANIMATION_FILL_MODE_BOTH;

    ComputedTiming computedTiming =
      dom::KeyframeEffectReadOnly::GetComputedTimingAt(
        Nullable<TimeDuration>(elapsedDuration), timing);

    MOZ_ASSERT(0.0 <= computedTiming.mProgress &&
               computedTiming.mProgress <= 1.0,
               "iteration progress should be in [0-1]");

    int segmentIndex = 0;
    AnimationSegment* segment = animation.segments().Elements();
    while (segment->endPortion() < computedTiming.mProgress) {
      ++segment;
      ++segmentIndex;
    }

    double positionInSegment =
      (computedTiming.mProgress - segment->startPortion()) /
      (segment->endPortion() - segment->startPortion());

    double portion =
      animData.mFunctions[segmentIndex]->GetValue(positionInSegment);

    // interpolate the property
    Animatable interpolatedValue;
    SampleValue(portion, animation, animData.mStartValues[segmentIndex],
                animData.mEndValues[segmentIndex], &interpolatedValue);
    LayerComposite* layerComposite = aLayer->AsLayerComposite();
    switch (animation.property()) {
    case eCSSProperty_opacity:
    {
      layerComposite->SetShadowOpacity(interpolatedValue.get_float());
      break;
    }
    case eCSSProperty_transform:
    {
      Matrix4x4 matrix = interpolatedValue.get_ArrayOfTransformFunction()[0].get_TransformMatrix().value();
      if (ContainerLayer* c = aLayer->AsContainerLayer()) {
        matrix.PostScale(c->GetInheritedXScale(), c->GetInheritedYScale(), 1);
      }
      layerComposite->SetShadowTransform(matrix);
      layerComposite->SetShadowTransformSetByAnimation(true);
      break;
    }
    default:
      NS_WARNING("Unhandled animated property");
    }
  }

  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    activeAnimations |= SampleAnimations(child, aPoint);
  }

  return activeAnimations;
}

static bool
SampleAPZAnimations(const LayerMetricsWrapper& aLayer, TimeStamp aSampleTime)
{
  bool activeAnimations = false;
  for (LayerMetricsWrapper child = aLayer.GetFirstChild(); child;
        child = child.GetNextSibling()) {
    activeAnimations |= SampleAPZAnimations(child, aSampleTime);
  }

  if (AsyncPanZoomController* apzc = aLayer.GetApzc()) {
    activeAnimations |= apzc->AdvanceAnimations(aSampleTime);
  }

  return activeAnimations;
}

void
AsyncCompositionManager::RecordShadowTransforms(Layer* aLayer)
{
  MOZ_ASSERT(gfxPrefs::CollectScrollTransforms());
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());

  for (Layer* child = aLayer->GetFirstChild();
      child; child = child->GetNextSibling()) {
      RecordShadowTransforms(child);
  }

  for (uint32_t i = 0; i < aLayer->GetFrameMetricsCount(); i++) {
    AsyncPanZoomController* apzc = aLayer->GetAsyncPanZoomController(i);
    if (!apzc) {
      continue;
    }
    gfx::Matrix4x4 shadowTransform = aLayer->AsLayerComposite()->GetShadowTransform();
    if (!shadowTransform.Is2D()) {
      continue;
    }

    Matrix transform = shadowTransform.As2D();
    if (transform.IsTranslation() && !shadowTransform.IsIdentity()) {
      Point translation = transform.GetTranslation();
      mLayerTransformRecorder.RecordTransform(aLayer, translation);
      return;
    }
  }
}

Matrix4x4
AdjustForClip(const Matrix4x4& asyncTransform, Layer* aLayer)
{
  Matrix4x4 result = asyncTransform;

  // Container layers start at the origin, but they are clipped to where they
  // actually have content on the screen. The tree transform is meant to apply
  // to the clipped area. If the tree transform includes a scale component,
  // then applying it to container as-is will produce incorrect results. To
  // avoid this, translate the layer so that the clip rect starts at the origin,
  // apply the tree transform, and translate back.
  if (const Maybe<ParentLayerIntRect>& shadowClipRect = aLayer->AsLayerComposite()->GetShadowClipRect()) {
    if (shadowClipRect->TopLeft() != ParentLayerIntPoint()) {  // avoid a gratuitous change of basis
      result.ChangeBasis(shadowClipRect->x, shadowClipRect->y, 0);
    }
  }
  return result;
}

bool
AsyncCompositionManager::ApplyAsyncContentTransformToTree(Layer *aLayer)
{
  bool appliedTransform = false;
  for (Layer* child = aLayer->GetFirstChild();
      child; child = child->GetNextSibling()) {
    appliedTransform |=
      ApplyAsyncContentTransformToTree(child);
  }

  Matrix4x4 oldTransform = aLayer->GetTransform();

  Matrix4x4 combinedAsyncTransformWithoutOverscroll;
  Matrix4x4 combinedAsyncTransform;
  bool hasAsyncTransform = false;
  LayerMargin fixedLayerMargins(0, 0, 0, 0);

  // Each layer has multiple clips. Its local clip, which must move with async
  // transforms, and its scrollframe clips, which are the clips between each
  // scrollframe and its ancestor scrollframe. Scrollframe clips include the
  // composition bounds and any other clips induced by layout.
  //
  // The final clip for the layer is the intersection of these clips.
  Maybe<ParentLayerIntRect> asyncClip = aLayer->GetClipRect();

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

  for (uint32_t i = 0; i < aLayer->GetFrameMetricsCount(); i++) {
    AsyncPanZoomController* controller = aLayer->GetAsyncPanZoomController(i);
    if (!controller) {
      continue;
    }

    hasAsyncTransform = true;

    ViewTransform asyncTransformWithoutOverscroll;
    ParentLayerPoint scrollOffset;
    controller->SampleContentTransformForFrame(&asyncTransformWithoutOverscroll,
                                               scrollOffset);
    Matrix4x4 overscrollTransform = controller->GetOverscrollTransform();
    Matrix4x4 asyncTransform =
      Matrix4x4(asyncTransformWithoutOverscroll) * overscrollTransform;

    if (!aLayer->IsScrollInfoLayer()) {
      controller->MarkAsyncTransformAppliedToContent();
    }

    const FrameMetrics& metrics = aLayer->GetFrameMetrics(i);
    ScreenPoint offset(0, 0);
    // TODO: When we enable APZ on Fennec, we'll need to call SyncFrameMetrics here.
    // When doing so, it might be useful to look at how it was called here before
    // bug 1036967 removed the (dead) call.

#if defined(MOZ_ANDROID_APZ)
    if (mIsFirstPaint) {
      CSSToLayerScale geckoZoom = metrics.LayersPixelsPerCSSPixel().ToScaleFactor();
      LayerIntPoint scrollOffsetLayerPixels = RoundedToInt(metrics.GetScrollOffset() * geckoZoom);
      mContentRect = metrics.GetScrollableRect();
      SetFirstPaintViewport(scrollOffsetLayerPixels,
                            geckoZoom,
                            mContentRect);
    }
#endif

    mIsFirstPaint = false;
    mLayersUpdated = false;

    // Apply the render offset
    mLayerManager->GetCompositor()->SetScreenRenderOffset(offset);

    // Transform the current local clip by this APZC's async transform. If we're
    // using containerful scrolling, then the clip is not part of the scrolled
    // frame and should not be transformed.
    if (asyncClip && !metrics.UsesContainerScrolling()) {
      asyncClip = Some(TransformTo<ParentLayerPixel>(asyncTransform, *asyncClip));
    }

    // Combine the local clip with the ancestor scrollframe clip. This is not
    // included in the async transform above, since the ancestor clip should not
    // move with this APZC.
    if (metrics.HasClipRect()) {
      ParentLayerIntRect clip = metrics.ClipRect();
      if (asyncClip) {
        asyncClip = Some(clip.Intersect(*asyncClip));
      } else {
        asyncClip = Some(clip);
      }
    }

    // Do the same for the ancestor mask layers: ancestorMaskLayers contains
    // the ancestor mask layers for scroll frames *inside* the current scroll
    // frame, so these are the ones we need to shift by our async transform.
    for (Layer* ancestorMaskLayer : ancestorMaskLayers) {
      SetShadowTransform(ancestorMaskLayer,
          ancestorMaskLayer->GetLocalTransform() * asyncTransform);
    }

    // Append the ancestor mask layer for this scroll frame to ancestorMaskLayers.
    if (metrics.GetMaskLayerIndex()) {
      size_t maskLayerIndex = metrics.GetMaskLayerIndex().value();
      Layer* ancestorMaskLayer = aLayer->GetAncestorMaskLayerAt(maskLayerIndex);
      ancestorMaskLayers.AppendElement(ancestorMaskLayer);
    }

    combinedAsyncTransformWithoutOverscroll *= asyncTransformWithoutOverscroll;
    combinedAsyncTransform *= asyncTransform;
  }

  if (hasAsyncTransform) {
    if (asyncClip) {
      aLayer->AsLayerComposite()->SetShadowClipRect(asyncClip);
    }

    // Apply the APZ transform on top of GetLocalTransform() here (rather than
    // GetTransform()) in case the OMTA code in SampleAnimations already set a
    // shadow transform; in that case we want to apply ours on top of that one
    // rather than clobber it.
    SetShadowTransform(aLayer,
        aLayer->GetLocalTransform() * AdjustForClip(combinedAsyncTransform, aLayer));

    // Do the same for the layer's own mask layer, if it has one.
    if (Layer* maskLayer = aLayer->GetMaskLayer()) {
      SetShadowTransform(maskLayer,
          maskLayer->GetLocalTransform() * combinedAsyncTransform);
    }

    const FrameMetrics& bottom = LayerMetricsWrapper::BottommostScrollableMetrics(aLayer);
    MOZ_ASSERT(bottom.IsScrollable());  // must be true because hasAsyncTransform is true

    // For the purpose of aligning fixed and sticky layers, we disregard
    // the overscroll transform as well as any OMTA transform when computing the
    // 'aCurrentTransformForRoot' parameter. This ensures that the overscroll
    // and OMTA transforms are not unapplied, and therefore that the visual
    // effects apply to fixed and sticky layers. We do this by using
    // GetTransform() as the base transform rather than GetLocalTransform(),
    // which would include those factors.
    Matrix4x4 transformWithoutOverscrollOrOmta = aLayer->GetTransform() *
        AdjustForClip(combinedAsyncTransformWithoutOverscroll, aLayer);
    // Since fixed/sticky layers are relative to their nearest scrolling ancestor,
    // we use the ViewID from the bottommost scrollable metrics here.
    AlignFixedAndStickyLayers(aLayer, aLayer, bottom.GetScrollId(), oldTransform,
                              transformWithoutOverscrollOrOmta, fixedLayerMargins);

    appliedTransform = true;
  }

  if (aLayer->GetScrollbarDirection() != Layer::NONE) {
    ApplyAsyncTransformToScrollbar(aLayer);
  }
  return appliedTransform;
}

static bool
LayerIsScrollbarTarget(const LayerMetricsWrapper& aTarget, Layer* aScrollbar)
{
  AsyncPanZoomController* apzc = aTarget.GetApzc();
  if (!apzc) {
    return false;
  }
  const FrameMetrics& metrics = aTarget.Metrics();
  if (metrics.GetScrollId() != aScrollbar->GetScrollbarTargetContainerId()) {
    return false;
  }
  return !aTarget.IsScrollInfoLayer();
}

static void
ApplyAsyncTransformToScrollbarForContent(Layer* aScrollbar,
                                         const LayerMetricsWrapper& aContent,
                                         bool aScrollbarIsDescendant)
{
  // We only apply the transform if the scroll-target layer has non-container
  // children (i.e. when it has some possibly-visible content). This is to
  // avoid moving scroll-bars in the situation that only a scroll information
  // layer has been built for a scroll frame, as this would result in a
  // disparity between scrollbars and visible content.
  if (aContent.IsScrollInfoLayer()) {
    return;
  }

  const FrameMetrics& metrics = aContent.Metrics();
  AsyncPanZoomController* apzc = aContent.GetApzc();

  Matrix4x4 asyncTransform = apzc->GetCurrentAsyncTransform();

  // |asyncTransform| represents the amount by which we have scrolled and
  // zoomed since the last paint. Because the scrollbar was sized and positioned based
  // on the painted content, we need to adjust it based on asyncTransform so that
  // it reflects what the user is actually seeing now.
  Matrix4x4 scrollbarTransform;
  if (aScrollbar->GetScrollbarDirection() == Layer::VERTICAL) {
    const ParentLayerCoord asyncScrollY = asyncTransform._42;
    const float asyncZoomY = asyncTransform._22;

    // The scroll thumb needs to be scaled in the direction of scrolling by the
    // inverse of the async zoom. This is because zooming in decreases the
    // fraction of the whole srollable rect that is in view.
    const float yScale = 1.f / asyncZoomY;

    // Note: |metrics.GetZoom()| doesn't yet include the async zoom.
    const CSSToParentLayerScale effectiveZoom(metrics.GetZoom().yScale * asyncZoomY);

    // Here we convert the scrollbar thumb ratio into a true unitless ratio by
    // dividing out the conversion factor from the scrollframe's parent's space
    // to the scrollframe's space.
    const float ratio = aScrollbar->GetScrollbarThumbRatio() /
        (metrics.GetPresShellResolution() * asyncZoomY);
    // The scroll thumb needs to be translated in opposite direction of the
    // async scroll. This is because scrolling down, which translates the layer
    // content up, should result in moving the scroll thumb down.
    ParentLayerCoord yTranslation = -asyncScrollY * ratio;

    // The scroll thumb additionally needs to be translated to compensate for
    // the scale applied above. The origin with respect to which the scale is
    // applied is the origin of the entire scrollbar, rather than the origin of
    // the scroll thumb (meaning, for a vertical scrollbar it's at the top of
    // the composition bounds). This means that empty space above the thumb
    // is scaled too, effectively translating the thumb. We undo that
    // translation here.
    // (One can think of the adjustment being done to the translation here as
    // a change of basis. We have a method to help with that,
    // Matrix4x4::ChangeBasis(), but it wouldn't necessarily make the code
    // cleaner in this case).
    const CSSCoord thumbOrigin = (metrics.GetScrollOffset().y * ratio);
    const CSSCoord thumbOriginScaled = thumbOrigin * yScale;
    const CSSCoord thumbOriginDelta = thumbOriginScaled - thumbOrigin;
    const ParentLayerCoord thumbOriginDeltaPL = thumbOriginDelta * effectiveZoom;
    yTranslation -= thumbOriginDeltaPL;

    if (metrics.IsRootContent()) {
      // Scrollbar for the root are painted at the same resolution as the
      // content. Since the coordinate space we apply this transform in includes
      // the resolution, we need to adjust for it as well here. Note that in
      // another metrics.IsRootContent() hunk below we apply a
      // resolution-cancelling transform which ensures the scroll thumb isn't
      // actually rendered at a larger scale.
      yTranslation *= metrics.GetPresShellResolution();
    }

    scrollbarTransform.PostScale(1.f, yScale, 1.f);
    scrollbarTransform.PostTranslate(0, yTranslation, 0);
  }
  if (aScrollbar->GetScrollbarDirection() == Layer::HORIZONTAL) {
    // See detailed comments under the VERTICAL case.

    const ParentLayerCoord asyncScrollX = asyncTransform._41;
    const float asyncZoomX = asyncTransform._11;

    const float xScale = 1.f / asyncZoomX;

    const CSSToParentLayerScale effectiveZoom(metrics.GetZoom().xScale * asyncZoomX);

    const float ratio = aScrollbar->GetScrollbarThumbRatio() /
        (metrics.GetPresShellResolution() * asyncZoomX);
    ParentLayerCoord xTranslation = -asyncScrollX * ratio;

    const CSSCoord thumbOrigin = (metrics.GetScrollOffset().x * ratio);
    const CSSCoord thumbOriginScaled = thumbOrigin * xScale;
    const CSSCoord thumbOriginDelta = thumbOriginScaled - thumbOrigin;
    const ParentLayerCoord thumbOriginDeltaPL = thumbOriginDelta * effectiveZoom;
    xTranslation -= thumbOriginDeltaPL;

    if (metrics.IsRootContent()) {
      xTranslation *= metrics.GetPresShellResolution();
    }

    scrollbarTransform.PostScale(xScale, 1.f, 1.f);
    scrollbarTransform.PostTranslate(xTranslation, 0, 0);
  }

  Matrix4x4 transform = aScrollbar->GetLocalTransform() * scrollbarTransform;

  Matrix4x4 compensation;
  // If the scrollbar layer is for the root then the content's resolution
  // applies to the scrollbar as well. Since we don't actually want the scroll
  // thumb's size to vary with the zoom (other than its length reflecting the
  // fraction of the scrollable length that's in view, which is taken care of
  // above), we apply a transform to cancel out this resolution.
  if (metrics.IsRootContent()) {
    compensation =
        Matrix4x4::Scaling(metrics.GetPresShellResolution(),
                           metrics.GetPresShellResolution(),
                           1.0f).Inverse();
  }
  // If the scrollbar layer is a child of the content it is a scrollbar for,
  // then we need to adjust for any async transform (including an overscroll
  // transform) on the content. This needs to be cancelled out because layout
  // positions and sizes the scrollbar on the assumption that there is no async
  // transform, and without this adjustment the scrollbar will end up in the
  // wrong place.
  //
  // Note that since the async transform is applied on top of the content's
  // regular transform, we need to make sure to unapply the async transform in
  // the same coordinate space. This requires applying the content transform
  // and then unapplying it after unapplying the async transform.
  if (aScrollbarIsDescendant) {
    Matrix4x4 asyncUntransform = (asyncTransform * apzc->GetOverscrollTransform()).Inverse();
    Matrix4x4 contentTransform = aContent.GetTransform();
    Matrix4x4 contentUntransform = contentTransform.Inverse();

    Matrix4x4 asyncCompensation = contentTransform
                                * asyncUntransform
                                * contentUntransform;

    compensation = compensation * asyncCompensation;

    // We also need to make a corresponding change on the clip rect of all the
    // layers on the ancestor chain from the scrollbar layer up to but not
    // including the layer with the async transform. Otherwise the scrollbar
    // shifts but gets clipped and so appears to flicker.
    for (Layer* ancestor = aScrollbar; ancestor != aContent.GetLayer(); ancestor = ancestor->GetParent()) {
      TransformClipRect(ancestor, asyncCompensation);
    }
  }
  transform = transform * compensation;

  SetShadowTransform(aScrollbar, transform);
}

static LayerMetricsWrapper
FindScrolledLayerRecursive(Layer* aScrollbar, const LayerMetricsWrapper& aSubtreeRoot)
{
  if (LayerIsScrollbarTarget(aSubtreeRoot, aScrollbar)) {
    return aSubtreeRoot;
  }

  for (LayerMetricsWrapper child = aSubtreeRoot.GetFirstChild();
       child;
       child = child.GetNextSibling())
  {
    // Do not recurse into RefLayers, since our initial aSubtreeRoot is the
    // root (or RefLayer root) of a single layer space to search.
    if (child.AsRefLayer()) {
      continue;
    }

    LayerMetricsWrapper target = FindScrolledLayerRecursive(aScrollbar, child);
    if (target) {
      return target;
    }
  }
  return LayerMetricsWrapper();
}

static LayerMetricsWrapper
FindScrolledLayerForScrollbar(Layer* aScrollbar, bool* aOutIsAncestor)
{
  // First check if the scrolled layer is an ancestor of the scrollbar layer.
  LayerMetricsWrapper root(aScrollbar->Manager()->GetRoot());
  LayerMetricsWrapper prevAncestor(aScrollbar);
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
  return FindScrolledLayerRecursive(aScrollbar, root);
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
    ApplyAsyncTransformToScrollbarForContent(aLayer, scrollTarget, isAncestor);
  }
}

void
AsyncCompositionManager::TransformScrollableLayer(Layer* aLayer)
{
  FrameMetrics metrics = LayerMetricsWrapper::TopmostScrollableMetrics(aLayer);
  if (!metrics.IsScrollable()) {
    // On Fennec it's possible that the there is no scrollable layer in the
    // tree, and this function just gets called with the root layer. In that
    // case TopmostScrollableMetrics will return an empty FrameMetrics but we
    // still want to use the actual non-scrollable metrics from the layer.
    metrics = LayerMetricsWrapper::BottommostMetrics(aLayer);
  }

  // We must apply the resolution scale before a pan/zoom transform, so we call
  // GetTransform here.
  Matrix4x4 oldTransform = aLayer->GetTransform();

  CSSToLayerScale geckoZoom = metrics.LayersPixelsPerCSSPixel().ToScaleFactor();

  LayerIntPoint scrollOffsetLayerPixels = RoundedToInt(metrics.GetScrollOffset() * geckoZoom);

  if (mIsFirstPaint) {
    mContentRect = metrics.GetScrollableRect();
    SetFirstPaintViewport(scrollOffsetLayerPixels,
                          geckoZoom,
                          mContentRect);
    mIsFirstPaint = false;
  } else if (!metrics.GetScrollableRect().IsEqualEdges(mContentRect)) {
    mContentRect = metrics.GetScrollableRect();
    SetPageRect(mContentRect);
  }

  // We synchronise the viewport information with Java after sending the above
  // notifications, so that Java can take these into account in its response.
  // Calculate the absolute display port to send to Java
  LayerIntRect displayPort = RoundedToInt(
    (metrics.GetCriticalDisplayPort().IsEmpty()
      ? metrics.GetDisplayPort()
      : metrics.GetCriticalDisplayPort()
    ) * geckoZoom);
  displayPort += scrollOffsetLayerPixels;

  LayerMargin fixedLayerMargins(0, 0, 0, 0);
  ScreenPoint offset(0, 0);

  // Ideally we would initialize userZoom to AsyncPanZoomController::CalculateResolution(metrics)
  // but this causes a reftest-ipc test to fail (see bug 883646 comment 27). The reason for this
  // appears to be that metrics.mZoom is poorly initialized in some scenarios. In these scenarios,
  // however, we can assume there is no async zooming in progress and so the following statement
  // works fine.
  CSSToParentLayerScale userZoom(metrics.GetDevPixelsPerCSSPixel()
                                 // This function only applies to the root scrollable frame,
                                 // for which we can assume that x and y scales are equal.
                               * metrics.GetCumulativeResolution().ToScaleFactor()
                               * LayerToParentLayerScale(1));
  ParentLayerPoint userScroll = metrics.GetScrollOffset() * userZoom;
  SyncViewportInfo(displayPort, geckoZoom, mLayersUpdated,
                   userScroll, userZoom, fixedLayerMargins,
                   offset);
  mLayersUpdated = false;

  // Apply the render offset
  mLayerManager->GetCompositor()->SetScreenRenderOffset(offset);

  // Handle transformations for asynchronous panning and zooming. We determine the
  // zoom used by Gecko from the transformation set on the root layer, and we
  // determine the scroll offset used by Gecko from the frame metrics of the
  // primary scrollable layer. We compare this to the user zoom and scroll
  // offset in the view transform we obtained from Java in order to compute the
  // transformation we need to apply.
  ParentLayerPoint geckoScroll(0, 0);
  if (metrics.IsScrollable()) {
    geckoScroll = metrics.GetScrollOffset() * userZoom;
  }

  LayerToParentLayerScale asyncZoom = userZoom / metrics.LayersPixelsPerCSSPixel().ToScaleFactor();
  ParentLayerPoint translation = userScroll - geckoScroll;
  Matrix4x4 treeTransform = ViewTransform(asyncZoom, -translation);

  // Apply the tree transform on top of GetLocalTransform() here (rather than
  // GetTransform()) in case the OMTA code in SampleAnimations already set a
  // shadow transform; in that case we want to apply ours on top of that one
  // rather than clobber it.
  SetShadowTransform(aLayer, aLayer->GetLocalTransform() * treeTransform);

  // Make sure that overscroll and under-zoom are represented in the old
  // transform so that fixed position content moves and scales accordingly.
  // These calculations will effectively scale and offset fixed position layers
  // in screen space when the compensatory transform is performed in
  // AlignFixedAndStickyLayers.
  ParentLayerRect contentScreenRect = mContentRect * userZoom;
  Point3D overscrollTranslation;
  if (userScroll.x < contentScreenRect.x) {
    overscrollTranslation.x = contentScreenRect.x - userScroll.x;
  } else if (userScroll.x + metrics.GetCompositionBounds().width > contentScreenRect.XMost()) {
    overscrollTranslation.x = contentScreenRect.XMost() -
      (userScroll.x + metrics.GetCompositionBounds().width);
  }
  if (userScroll.y < contentScreenRect.y) {
    overscrollTranslation.y = contentScreenRect.y - userScroll.y;
  } else if (userScroll.y + metrics.GetCompositionBounds().height > contentScreenRect.YMost()) {
    overscrollTranslation.y = contentScreenRect.YMost() -
      (userScroll.y + metrics.GetCompositionBounds().height);
  }
  oldTransform.PreTranslate(overscrollTranslation.x,
                            overscrollTranslation.y,
                            overscrollTranslation.z);

  gfx::Size underZoomScale(1.0f, 1.0f);
  if (mContentRect.width * userZoom.scale < metrics.GetCompositionBounds().width) {
    underZoomScale.width = (mContentRect.width * userZoom.scale) /
      metrics.GetCompositionBounds().width;
  }
  if (mContentRect.height * userZoom.scale < metrics.GetCompositionBounds().height) {
    underZoomScale.height = (mContentRect.height * userZoom.scale) /
      metrics.GetCompositionBounds().height;
  }
  oldTransform.PreScale(underZoomScale.width, underZoomScale.height, 1);

  // Make sure fixed position layers don't move away from their anchor points
  // when we're asynchronously panning or zooming
  AlignFixedAndStickyLayers(aLayer, aLayer, metrics.GetScrollId(), oldTransform,
                            aLayer->GetLocalTransform(), fixedLayerMargins);
}

void
AsyncCompositionManager::GetFrameUniformity(FrameUniformityData* aOutData)
{
  MOZ_ASSERT(CompositorParent::IsInCompositorThread());
  mLayerTransformRecorder.EndTest(aOutData);
}

bool
AsyncCompositionManager::TransformShadowTree(TimeStamp aCurrentFrame,
                                             TransformsToSkip aSkip)
{
  PROFILER_LABEL("AsyncCompositionManager", "TransformShadowTree",
    js::ProfileEntry::Category::GRAPHICS);

  Layer* root = mLayerManager->GetRoot();
  if (!root) {
    return false;
  }

  // First, compute and set the shadow transforms from OMT animations.
  // NB: we must sample animations *before* sampling pan/zoom
  // transforms.
  bool wantNextFrame = SampleAnimations(root, aCurrentFrame);

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
    wantNextFrame |= SampleAPZAnimations(LayerMetricsWrapper(root), aCurrentFrame);
    if (!ApplyAsyncContentTransformToTree(root)) {
      nsAutoTArray<Layer*,1> scrollableLayers;
#ifdef MOZ_WIDGET_ANDROID
      mLayerManager->GetRootScrollableLayers(scrollableLayers);
#else
      mLayerManager->GetScrollableLayers(scrollableLayers);
#endif

      for (uint32_t i = 0; i < scrollableLayers.Length(); i++) {
        if (scrollableLayers[i]) {
          TransformScrollableLayer(scrollableLayers[i]);
        }
      }
    }
  }

  LayerComposite* rootComposite = root->AsLayerComposite();

  gfx::Matrix4x4 trans = rootComposite->GetShadowTransform();
  trans *= gfx::Matrix4x4::From2D(mWorldTransform);
  rootComposite->SetShadowTransform(trans);

  if (gfxPrefs::CollectScrollTransforms()) {
    RecordShadowTransforms(root);
  }

  return wantNextFrame;
}

void
AsyncCompositionManager::SetFirstPaintViewport(const LayerIntPoint& aOffset,
                                               const CSSToLayerScale& aZoom,
                                               const CSSRect& aCssPageRect)
{
#ifdef MOZ_WIDGET_ANDROID
  AndroidBridge::Bridge()->SetFirstPaintViewport(aOffset, aZoom, aCssPageRect);
#endif
}

void
AsyncCompositionManager::SetPageRect(const CSSRect& aCssPageRect)
{
#ifdef MOZ_WIDGET_ANDROID
  AndroidBridge::Bridge()->SetPageRect(aCssPageRect);
#endif
}

void
AsyncCompositionManager::SyncViewportInfo(const LayerIntRect& aDisplayPort,
                                          const CSSToLayerScale& aDisplayResolution,
                                          bool aLayersUpdated,
                                          ParentLayerPoint& aScrollOffset,
                                          CSSToParentLayerScale& aScale,
                                          LayerMargin& aFixedLayerMargins,
                                          ScreenPoint& aOffset)
{
#ifdef MOZ_WIDGET_ANDROID
  AndroidBridge::Bridge()->SyncViewportInfo(aDisplayPort,
                                            aDisplayResolution,
                                            aLayersUpdated,
                                            aScrollOffset,
                                            aScale,
                                            aFixedLayerMargins,
                                            aOffset);
#endif
}

void
AsyncCompositionManager::SyncFrameMetrics(const ParentLayerPoint& aScrollOffset,
                                          float aZoom,
                                          const CSSRect& aCssPageRect,
                                          bool aLayersUpdated,
                                          const CSSRect& aDisplayPort,
                                          const CSSToLayerScale& aDisplayResolution,
                                          bool aIsFirstPaint,
                                          LayerMargin& aFixedLayerMargins,
                                          ScreenPoint& aOffset)
{
#ifdef MOZ_WIDGET_ANDROID
  AndroidBridge::Bridge()->SyncFrameMetrics(aScrollOffset, aZoom, aCssPageRect,
                                            aLayersUpdated, aDisplayPort,
                                            aDisplayResolution, aIsFirstPaint,
                                            aFixedLayerMargins, aOffset);
#endif
}

} // namespace layers
} // namespace mozilla
