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
#include "mozilla/dom/AnimationPlayer.h" // for AnimationPlayer
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Point.h"          // for RoundedToInt, PointTyped
#include "mozilla/gfx/Rect.h"           // for RoundedToInt, RectTyped
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorParent.h" // for CompositorParent, etc
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "nsCSSPropList.h"
#include "nsCoord.h"                    // for NSAppUnitsToFloatPixels, etc
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsDeviceContext.h"            // for nsDeviceContext
#include "nsDisplayList.h"              // for nsDisplayTransform, etc
#include "nsMathUtils.h"                // for NS_round
#include "nsPoint.h"                    // for nsPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray
#include "UnitTransforms.h"             // for TransformTo
#if defined(MOZ_WIDGET_ANDROID)
# include <android/log.h>
# include "AndroidBridge.h"
#endif
#include "GeckoProfiler.h"

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
ContentMightReflowOnOrientationChange(const nsIntRect& rect)
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

static bool
GetBaseTransform2D(Layer* aLayer, Matrix* aTransform)
{
  // Start with the animated transform if there is one
  return (aLayer->AsLayerComposite()->GetShadowTransformSetByAnimation() ?
          aLayer->GetLocalTransform() : aLayer->GetTransform()).Is2D(aTransform);
}

static void
TransformClipRect(Layer* aLayer,
                  const Matrix4x4& aTransform)
{
  const nsIntRect* clipRect = aLayer->GetClipRect();
  if (clipRect) {
    LayerIntRect transformed = TransformTo<LayerPixel>(
        aTransform, LayerIntRect::FromUntyped(*clipRect));
    nsIntRect shadowClip = LayerIntRect::ToUntyped(transformed);
    aLayer->AsLayerComposite()->SetShadowClipRect(&shadowClip);
  }
}

static void
TranslateShadowLayer2D(Layer* aLayer,
                       const gfxPoint& aTranslation,
                       bool aAdjustClipRect)
{
  // This layer might also be a scrollable layer and have an async transform.
  // To make sure we don't clobber that, we start with the shadow transform.
  // Any adjustments to the shadow transform made in this function in previous
  // frames have been cleared in ClearAsyncTransforms(), so such adjustments
  // will not compound over successive frames.
  Matrix layerTransform;
  if (!aLayer->GetLocalTransform().Is2D(&layerTransform)) {
    return;
  }

  // Apply the 2D translation to the layer transform.
  layerTransform._31 += aTranslation.x;
  layerTransform._32 += aTranslation.y;

  // The transform already takes the resolution scale into account.  Since we
  // will apply the resolution scale again when computing the effective
  // transform, we must apply the inverse resolution scale here.
  Matrix4x4 layerTransform3D = Matrix4x4::From2D(layerTransform);
  if (ContainerLayer* c = aLayer->AsContainerLayer()) {
    layerTransform3D.PreScale(1.0f/c->GetPreXScale(),
                              1.0f/c->GetPreYScale(),
                              1);
  }
  layerTransform3D.PostScale(1.0f/aLayer->GetPostXScale(),
                             1.0f/aLayer->GetPostYScale(),
                             1);

  LayerComposite* layerComposite = aLayer->AsLayerComposite();
  layerComposite->SetShadowTransform(layerTransform3D);
  layerComposite->SetShadowTransformSetByAnimation(false);

  if (aAdjustClipRect) {
    TransformClipRect(aLayer, Matrix4x4::Translation(aTranslation.x, aTranslation.y, 0));
  }
}

static bool
AccumulateLayerTransforms2D(Layer* aLayer,
                            Layer* aAncestor,
                            Matrix& aMatrix)
{
  // Accumulate the transforms between this layer and the subtree root layer.
  for (Layer* l = aLayer; l && l != aAncestor; l = l->GetParent()) {
    Matrix l2D;
    if (!GetBaseTransform2D(l, &l2D)) {
      return false;
    }
    aMatrix *= l2D;
  }

  return true;
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
  bool isRootFixed = aLayer->GetIsFixedPosition() &&
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
  // This currently only works for fixed layers with 2D transforms.

  // Accumulate the transforms between this layer and the subtree root layer.
  Matrix ancestorTransform;
  if (!AccumulateLayerTransforms2D(aLayer->GetParent(), aTransformedSubtreeRoot,
                                   ancestorTransform)) {
    return;
  }

  Matrix oldRootTransform;
  Matrix newRootTransform;
  if (!aPreviousTransformForRoot.Is2D(&oldRootTransform) ||
      !aCurrentTransformForRoot.Is2D(&newRootTransform)) {
    return;
  }

  // Calculate the cumulative transforms between the subtree root with the
  // old transform and the current transform.
  Matrix oldCumulativeTransform = ancestorTransform * oldRootTransform;
  Matrix newCumulativeTransform = ancestorTransform * newRootTransform;
  if (newCumulativeTransform.IsSingular()) {
    return;
  }
  Matrix newCumulativeTransformInverse = newCumulativeTransform.Inverse();

  // Now work out the translation necessary to make sure the layer doesn't
  // move given the new sub-tree root transform.
  Matrix layerTransform;
  if (!GetBaseTransform2D(aLayer, &layerTransform)) {
    return;
  }

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

  // Finally, apply the 2D translation to the layer transform. Note that in
  // general we need to apply the same translation to the layer's clip rect, so
  // that the effective transform on the clip rect takes it back to where it was
  // originally, had there been no async scroll. In the case where the
  // fixed/sticky layer is the same as aTransformedSubtreeRoot, then the clip
  // rect is not affected by the scroll-induced async scroll transform anyway
  // (since the clip is applied post-transform) so we don't need to make the
  // adjustment.
  TranslateShadowLayer2D(aLayer, ThebesPoint(translation), aLayer != aTransformedSubtreeRoot);
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
  // we expect all our transform data to arrive in css pixels, so here we must
  // adjust to dev pixels.
  double cssPerDev = double(nsDeviceContext::AppUnitsPerCSSPixel())
                     / double(data.appUnitsPerDevPixel());
  Point3D transformOrigin = data.transformOrigin();
  transformOrigin.x = transformOrigin.x * cssPerDev;
  transformOrigin.y = transformOrigin.y * cssPerDev;
  Point3D perspectiveOrigin = data.perspectiveOrigin();
  perspectiveOrigin.x = perspectiveOrigin.x * cssPerDev;
  perspectiveOrigin.y = perspectiveOrigin.y * cssPerDev;
  nsDisplayTransform::FrameTransformProperties props(interpolatedList,
                                                     transformOrigin,
                                                     perspectiveOrigin,
                                                     data.perspective());
  gfx3DMatrix transform =
    nsDisplayTransform::GetResultingTransformMatrix(props, origin,
                                                    data.appUnitsPerDevPixel(),
                                                    &data.bounds());
  Point3D scaledOrigin =
    Point3D(NS_round(NSAppUnitsToFloatPixels(origin.x, data.appUnitsPerDevPixel())),
            NS_round(NSAppUnitsToFloatPixels(origin.y, data.appUnitsPerDevPixel())),
            0.0f);

  transform.Translate(scaledOrigin);

  InfallibleTArray<TransformFunction> functions;
  functions.AppendElement(TransformMatrix(ToMatrix4x4(transform)));
  *aValue = functions;
}

static bool
SampleAnimations(Layer* aLayer, TimeStamp aPoint)
{
  AnimationArray& animations = aLayer->GetAnimations();
  InfallibleTArray<AnimData>& animationData = aLayer->GetAnimationData();

  bool activeAnimations = false;

  for (uint32_t i = animations.Length(); i-- !=0; ) {
    Animation& animation = animations[i];
    AnimData& animData = animationData[i];

    activeAnimations = true;

    TimeDuration elapsedDuration = aPoint - animation.startTime();
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
      dom::Animation::GetComputedTimingAt(
        Nullable<TimeDuration>(elapsedDuration), timing);

    NS_ABORT_IF_FALSE(0.0 <= computedTiming.mTimeFraction &&
                      computedTiming.mTimeFraction <= 1.0,
                      "time fraction should be in [0-1]");

    int segmentIndex = 0;
    AnimationSegment* segment = animation.segments().Elements();
    while (segment->endPortion() < computedTiming.mTimeFraction) {
      ++segment;
      ++segmentIndex;
    }

    double positionInSegment =
      (computedTiming.mTimeFraction - segment->startPortion()) /
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

Matrix4x4
AdjustAndCombineWithCSSTransform(const Matrix4x4& asyncTransform, Layer* aLayer)
{
  Matrix4x4 result = asyncTransform;

  // Container layers start at the origin, but they are clipped to where they
  // actually have content on the screen. The tree transform is meant to apply
  // to the clipped area. If the tree transform includes a scale component,
  // then applying it to container as-is will produce incorrect results. To
  // avoid this, translate the layer so that the clip rect starts at the origin,
  // apply the tree transform, and translate back.
  if (const nsIntRect* shadowClipRect = aLayer->AsLayerComposite()->GetShadowClipRect()) {
    if (shadowClipRect->TopLeft() != nsIntPoint()) {  // avoid a gratuitous change of basis
      result.ChangeBasis(shadowClipRect->x, shadowClipRect->y, 0);
    }
  }

  // Combine the async transform with the layer's CSS transform.
  result = aLayer->GetTransform() * result;
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

  LayerComposite* layerComposite = aLayer->AsLayerComposite();
  Matrix4x4 oldTransform = aLayer->GetTransform();

  Matrix4x4 combinedAsyncTransformWithoutOverscroll;
  Matrix4x4 combinedAsyncTransform;
  bool hasAsyncTransform = false;
  LayerMargin fixedLayerMargins(0, 0, 0, 0);

  for (uint32_t i = 0; i < aLayer->GetFrameMetricsCount(); i++) {
    AsyncPanZoomController* controller = aLayer->GetAsyncPanZoomController(i);
    if (!controller) {
      continue;
    }

    hasAsyncTransform = true;

    ViewTransform asyncTransformWithoutOverscroll;
    ScreenPoint scrollOffset;
    controller->SampleContentTransformForFrame(&asyncTransformWithoutOverscroll,
                                               scrollOffset);
    Matrix4x4 overscrollTransform = controller->GetOverscrollTransform();

    if (!aLayer->IsScrollInfoLayer()) {
      controller->MarkAsyncTransformAppliedToContent();
    }

    const FrameMetrics& metrics = aLayer->GetFrameMetrics(i);
    CSSToLayerScale paintScale = metrics.LayersPixelsPerCSSPixel();
    CSSRect displayPort(metrics.mCriticalDisplayPort.IsEmpty() ?
                        metrics.mDisplayPort : metrics.mCriticalDisplayPort);
    ScreenPoint offset(0, 0);
    // XXX this call to SyncFrameMetrics is not currently being used. It will be cleaned
    // up as part of bug 776030 or one of its dependencies.
    SyncFrameMetrics(scrollOffset, asyncTransformWithoutOverscroll.mScale.scale,
                     metrics.mScrollableRect, mLayersUpdated, displayPort,
                     paintScale, mIsFirstPaint, fixedLayerMargins, offset);

    mIsFirstPaint = false;
    mLayersUpdated = false;

    // Apply the render offset
    mLayerManager->GetCompositor()->SetScreenRenderOffset(offset);

    combinedAsyncTransformWithoutOverscroll *= asyncTransformWithoutOverscroll;
    combinedAsyncTransform *= (Matrix4x4(asyncTransformWithoutOverscroll) * overscrollTransform);
  }

  if (hasAsyncTransform) {
    Matrix4x4 transform = AdjustAndCombineWithCSSTransform(combinedAsyncTransform, aLayer);

    // GetTransform already takes the pre- and post-scale into account.  Since we
    // will apply the pre- and post-scale again when computing the effective
    // transform, we must apply the inverses here.
    if (ContainerLayer* container = aLayer->AsContainerLayer()) {
      transform.PreScale(1.0f/container->GetPreXScale(),
                         1.0f/container->GetPreYScale(),
                         1);
    }
    transform.PostScale(1.0f/aLayer->GetPostXScale(),
                        1.0f/aLayer->GetPostYScale(),
                        1);
    layerComposite->SetShadowTransform(transform);
    NS_ASSERTION(!layerComposite->GetShadowTransformSetByAnimation(),
                 "overwriting animated transform!");

    const FrameMetrics& bottom = LayerMetricsWrapper::BottommostScrollableMetrics(aLayer);
    MOZ_ASSERT(bottom.IsScrollable());  // must be true because hasAsyncTransform is true

    // Apply resolution scaling to the old transform - the layer tree as it is
    // doesn't have the necessary transform to display correctly. We use the
    // bottom-most scrollable metrics because that should have the most accurate
    // cumulative resolution for aLayer.
    LayoutDeviceToLayerScale resolution = bottom.mCumulativeResolution;
    oldTransform.PreScale(resolution.scale, resolution.scale, 1);

    // For the purpose of aligning fixed and sticky layers, we disregard
    // the overscroll transform when computing the 'aCurrentTransformForRoot'
    // parameter. This ensures that the overscroll transform is not unapplied,
    // and therefore that the visual effect applies to fixed and sticky layers.
    Matrix4x4 transformWithoutOverscroll = AdjustAndCombineWithCSSTransform(
        combinedAsyncTransformWithoutOverscroll, aLayer);
    // Since fixed/sticky layers are relative to their nearest scrolling ancestor,
    // we use the ViewID from the bottommost scrollable metrics here.
    AlignFixedAndStickyLayers(aLayer, aLayer, bottom.GetScrollId(), oldTransform,
                              transformWithoutOverscroll, fixedLayerMargins);

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
  Matrix4x4 nontransientTransform = apzc->GetNontransientAsyncTransform();
  Matrix4x4 transientTransform = nontransientTransform.Inverse() * asyncTransform;

  // |transientTransform| represents the amount by which we have scrolled and
  // zoomed since the last paint. Because the scrollbar was sized and positioned based
  // on the painted content, we need to adjust it based on transientTransform so that
  // it reflects what the user is actually seeing now.
  // - The scroll thumb needs to be scaled in the direction of scrolling by the inverse
  //   of the transientTransform scale (representing the zoom). This is because zooming
  //   in decreases the fraction of the whole scrollable rect that is in view.
  // - It needs to be translated in opposite direction of the transientTransform
  //   translation (representing the scroll). This is because scrolling down, which
  //   translates the layer content up, should result in moving the scroll thumb down.
  //   The amount of the translation to the scroll thumb should be such that the ratio
  //   of the translation to the size of the scroll port is the same as the ratio
  //   of the scroll amount to the size of the scrollable rect.
  Matrix4x4 scrollbarTransform;
  if (aScrollbar->GetScrollbarDirection() == Layer::VERTICAL) {
    float scale = metrics.CalculateCompositedSizeInCssPixels().height / metrics.mScrollableRect.height;
    if (aScrollbarIsDescendant) {
      // In cases where the scrollbar is a descendant of the content, the
      // scrollbar gets painted at the same resolution as the content. Since the
      // coordinate space we apply this transform in includes the resolution, we
      // need to adjust for it as well here. Note that in another
      // aScrollbarIsDescendant hunk below we unapply the entire async
      // transform, which includes the nontransientasync transform and would
      // normally account for the resolution.
      scale *= metrics.mResolution.scale;
    }
    scrollbarTransform.PostScale(1.f, 1.f / transientTransform._22, 1.f);
    scrollbarTransform.PostTranslate(0, -transientTransform._42 * scale, 0);
  }
  if (aScrollbar->GetScrollbarDirection() == Layer::HORIZONTAL) {
    float scale = metrics.CalculateCompositedSizeInCssPixels().width / metrics.mScrollableRect.width;
    if (aScrollbarIsDescendant) {
      scale *= metrics.mResolution.scale;
    }
    scrollbarTransform.PostScale(1.f / transientTransform._11, 1.f, 1.f);
    scrollbarTransform.PostTranslate(-transientTransform._41 * scale, 0, 0);
  }

  Matrix4x4 transform = scrollbarTransform * aScrollbar->GetTransform();

  if (aScrollbarIsDescendant) {
    // If the scrollbar layer is a child of the content it is a scrollbar for, then we
    // need to do an extra untransform to cancel out the async transform on
    // the content. This is needed because layout positions and sizes the
    // scrollbar on the assumption that there is no async transform, and without
    // this code the scrollbar will end up in the wrong place.
    //
    // Since the async transform is applied on top of the content's regular
    // transform, we need to make sure to unapply the async transform in the
    // same coordinate space. This requires applying the content transform and
    // then unapplying it after unapplying the async transform.
    Matrix4x4 asyncUntransform = (asyncTransform * apzc->GetOverscrollTransform()).Inverse();
    Matrix4x4 contentTransform = aContent.GetTransform();
    Matrix4x4 contentUntransform = contentTransform.Inverse();

    Matrix4x4 compensation = contentTransform * asyncUntransform * contentUntransform;
    transform = transform * compensation;

    // We also need to make a corresponding change on the clip rect of all the
    // layers on the ancestor chain from the scrollbar layer up to but not
    // including the layer with the async transform. Otherwise the scrollbar
    // shifts but gets clipped and so appears to flicker.
    for (Layer* ancestor = aScrollbar; ancestor != aContent.GetLayer(); ancestor = ancestor->GetParent()) {
      TransformClipRect(ancestor, compensation);
    }
  }

  // GetTransform already takes the pre- and post-scale into account.  Since we
  // will apply the pre- and post-scale again when computing the effective
  // transform, we must apply the inverses here.
  if (ContainerLayer* container = aScrollbar->AsContainerLayer()) {
    transform.PreScale(1.0f/container->GetPreXScale(),
                       1.0f/container->GetPreYScale(),
                        1);
  }
  transform.PostScale(1.0f/aScrollbar->GetPostXScale(),
                      1.0f/aScrollbar->GetPostYScale(),
                      1);
  aScrollbar->AsLayerComposite()->SetShadowTransform(transform);
}

static LayerMetricsWrapper
FindScrolledLayerRecursive(Layer* aScrollbar, const LayerMetricsWrapper& aSubtreeRoot)
{
  if (LayerIsScrollbarTarget(aSubtreeRoot, aScrollbar)) {
    return aSubtreeRoot;
  }
  for (LayerMetricsWrapper child = aSubtreeRoot.GetFirstChild(); child;
         child = child.GetNextSibling()) {
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
  // Search ancestors first.
  LayerMetricsWrapper scrollbar(aScrollbar);
  for (LayerMetricsWrapper ancestor = scrollbar; ancestor; ancestor = ancestor.GetParent()) {
    if (LayerIsScrollbarTarget(ancestor, aScrollbar)) {
      *aOutIsAncestor = true;
      return ancestor;
    }
  }

  // If the scrolled target is not an ancestor, search the whole layer tree.
  // XXX It would be much better to search the APZC tree instead of the layer
  // tree. That way we would ignore non-scrollable layers, and we'd only visit
  // each scroll ID once. In the end we only need the APZC and the FrameMetrics
  // of the scrolled target.
  *aOutIsAncestor = false;
  LayerMetricsWrapper root(aScrollbar->Manager()->GetRoot());
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
  LayerComposite* layerComposite = aLayer->AsLayerComposite();

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

  CSSToLayerScale geckoZoom = metrics.LayersPixelsPerCSSPixel();

  LayerIntPoint scrollOffsetLayerPixels = RoundedToInt(metrics.GetScrollOffset() * geckoZoom);

  if (mIsFirstPaint) {
    mContentRect = metrics.mScrollableRect;
    SetFirstPaintViewport(scrollOffsetLayerPixels,
                          geckoZoom,
                          mContentRect);
    mIsFirstPaint = false;
  } else if (!metrics.mScrollableRect.IsEqualEdges(mContentRect)) {
    mContentRect = metrics.mScrollableRect;
    SetPageRect(mContentRect);
  }

  // We synchronise the viewport information with Java after sending the above
  // notifications, so that Java can take these into account in its response.
  // Calculate the absolute display port to send to Java
  LayerIntRect displayPort = RoundedToInt(
    (metrics.mCriticalDisplayPort.IsEmpty()
      ? metrics.mDisplayPort
      : metrics.mCriticalDisplayPort
    ) * geckoZoom);
  displayPort += scrollOffsetLayerPixels;

  LayerMargin fixedLayerMargins(0, 0, 0, 0);
  ScreenPoint offset(0, 0);

  // Ideally we would initialize userZoom to AsyncPanZoomController::CalculateResolution(metrics)
  // but this causes a reftest-ipc test to fail (see bug 883646 comment 27). The reason for this
  // appears to be that metrics.mZoom is poorly initialized in some scenarios. In these scenarios,
  // however, we can assume there is no async zooming in progress and so the following statement
  // works fine.
  CSSToScreenScale userZoom(metrics.mDevPixelsPerCSSPixel * metrics.mCumulativeResolution * LayerToScreenScale(1));
  ScreenPoint userScroll = metrics.GetScrollOffset() * userZoom;
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
  ScreenPoint geckoScroll(0, 0);
  if (metrics.IsScrollable()) {
    geckoScroll = metrics.GetScrollOffset() * userZoom;
  }
  ParentLayerToScreenScale scale = userZoom
                                  / metrics.mDevPixelsPerCSSPixel
                                  / metrics.GetParentResolution();
  ScreenPoint translation = userScroll - geckoScroll;
  Matrix4x4 treeTransform = ViewTransform(scale, -translation);

  // The transform already takes the resolution scale into account.  Since we
  // will apply the resolution scale again when computing the effective
  // transform, we must apply the inverse resolution scale here.
  Matrix4x4 computedTransform = oldTransform * treeTransform;
  if (ContainerLayer* container = aLayer->AsContainerLayer()) {
    computedTransform.PreScale(1.0f/container->GetPreXScale(),
                               1.0f/container->GetPreYScale(),
                               1);
  }
  computedTransform.PostScale(1.0f/aLayer->GetPostXScale(),
                              1.0f/aLayer->GetPostYScale(),
                              1);
  layerComposite->SetShadowTransform(computedTransform);
  NS_ASSERTION(!layerComposite->GetShadowTransformSetByAnimation(),
               "overwriting animated transform!");

  // Apply resolution scaling to the old transform - the layer tree as it is
  // doesn't have the necessary transform to display correctly.
  oldTransform.PreScale(metrics.mResolution.scale, metrics.mResolution.scale, 1);

  // Make sure that overscroll and under-zoom are represented in the old
  // transform so that fixed position content moves and scales accordingly.
  // These calculations will effectively scale and offset fixed position layers
  // in screen space when the compensatory transform is performed in
  // AlignFixedAndStickyLayers.
  ScreenRect contentScreenRect = mContentRect * userZoom;
  Point3D overscrollTranslation;
  if (userScroll.x < contentScreenRect.x) {
    overscrollTranslation.x = contentScreenRect.x - userScroll.x;
  } else if (userScroll.x + metrics.mCompositionBounds.width > contentScreenRect.XMost()) {
    overscrollTranslation.x = contentScreenRect.XMost() -
      (userScroll.x + metrics.mCompositionBounds.width);
  }
  if (userScroll.y < contentScreenRect.y) {
    overscrollTranslation.y = contentScreenRect.y - userScroll.y;
  } else if (userScroll.y + metrics.mCompositionBounds.height > contentScreenRect.YMost()) {
    overscrollTranslation.y = contentScreenRect.YMost() -
      (userScroll.y + metrics.mCompositionBounds.height);
  }
  oldTransform.PreTranslate(overscrollTranslation.x,
                            overscrollTranslation.y,
                            overscrollTranslation.z);

  gfx::Size underZoomScale(1.0f, 1.0f);
  if (mContentRect.width * userZoom.scale < metrics.mCompositionBounds.width) {
    underZoomScale.width = (mContentRect.width * userZoom.scale) /
      metrics.mCompositionBounds.width;
  }
  if (mContentRect.height * userZoom.scale < metrics.mCompositionBounds.height) {
    underZoomScale.height = (mContentRect.height * userZoom.scale) /
      metrics.mCompositionBounds.height;
  }
  oldTransform.PreScale(underZoomScale.width, underZoomScale.height, 1);

  // Make sure fixed position layers don't move away from their anchor points
  // when we're asynchronously panning or zooming
  AlignFixedAndStickyLayers(aLayer, aLayer, metrics.GetScrollId(), oldTransform,
                            aLayer->GetLocalTransform(), fixedLayerMargins);
}

void
ClearAsyncTransforms(Layer* aLayer)
{
  if (!aLayer->AsLayerComposite()->GetShadowTransformSetByAnimation()) {
    aLayer->AsLayerComposite()->SetShadowTransform(aLayer->GetBaseTransform());
  }
  for (Layer* child = aLayer->GetFirstChild();
      child; child = child->GetNextSibling()) {
    ClearAsyncTransforms(child);
  }
}

bool
AsyncCompositionManager::TransformShadowTree(TimeStamp aCurrentFrame)
{
  PROFILER_LABEL("AsyncCompositionManager", "TransformShadowTree",
    js::ProfileEntry::Category::GRAPHICS);

  Layer* root = mLayerManager->GetRoot();
  if (!root) {
    return false;
  }


  // NB: we must sample animations *before* sampling pan/zoom
  // transforms.
  bool wantNextFrame = SampleAnimations(root, aCurrentFrame);

  // Clear any async transforms (not due to animations) set in previous frames.
  // This is necessary because some things called by
  // ApplyAsyncContentTransformToTree (in particular, TranslateShadowLayer2D),
  // add to the shadow transform rather than overwriting it.
  ClearAsyncTransforms(root);

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

  LayerComposite* rootComposite = root->AsLayerComposite();

  gfx::Matrix4x4 trans = rootComposite->GetShadowTransform();
  trans *= gfx::Matrix4x4::From2D(mWorldTransform);
  rootComposite->SetShadowTransform(trans);


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
                                          ScreenPoint& aScrollOffset,
                                          CSSToScreenScale& aScale,
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
AsyncCompositionManager::SyncFrameMetrics(const ScreenPoint& aScrollOffset,
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
