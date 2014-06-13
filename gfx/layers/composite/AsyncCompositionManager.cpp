/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/AsyncCompositionManager.h"
#include <stdint.h>                     // for uint32_t
#include "AnimationCommon.h"            // for ComputedTimingFunction
#include "CompositorParent.h"           // for CompositorParent, etc
#include "FrameMetrics.h"               // for FrameMetrics
#include "LayerManagerComposite.h"      // for LayerManagerComposite, etc
#include "Layers.h"                     // for Layer, ContainerLayer, etc
#include "gfxPoint.h"                   // for gfxPoint, gfxSize
#include "gfxPoint3D.h"                 // for gfxPoint3D
#include "mozilla/WidgetUtils.h"        // for ComputeTransformForRotation
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Point.h"          // for RoundedToInt, PointTyped
#include "mozilla/gfx/Rect.h"           // for RoundedToInt, RectTyped
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "nsAnimationManager.h"         // for ElementAnimations
#include "nsCSSPropList.h"
#include "nsCoord.h"                    // for NSAppUnitsToFloatPixels, etc
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsDeviceContext.h"            // for nsDeviceContext
#include "nsDisplayList.h"              // for nsDisplayTransform, etc
#include "nsMathUtils.h"                // for NS_round
#include "nsPoint.h"                    // for nsPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsStyleAnimation.h"           // for nsStyleAnimation::Value, etc
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray
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
              ContentMightReflowOnOrientationChange(aTargetConfig.clientBounds())) {
            aReady = false;
          }
        }

        if (OP == Resolve) {
          ref->ConnectReferentLayer(referent);
        } else {
          ref->DetachReferentLayer(referent);
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
    mLayerManager->SetWorldTransform(
      ComputeTransformForRotation(mTargetConfig.naturalBounds(),
                                  mTargetConfig.rotation()));
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
TranslateShadowLayer2D(Layer* aLayer,
                       const gfxPoint& aTranslation)
{
  Matrix layerTransform;
  if (!GetBaseTransform2D(aLayer, &layerTransform)) {
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
    layerTransform3D.Scale(1.0f/c->GetPreXScale(),
                           1.0f/c->GetPreYScale(),
                           1);
  }
  layerTransform3D = layerTransform3D *
    Matrix4x4().Scale(1.0f/aLayer->GetPostXScale(),
                      1.0f/aLayer->GetPostYScale(),
                      1);

  LayerComposite* layerComposite = aLayer->AsLayerComposite();
  layerComposite->SetShadowTransform(layerTransform3D);
  layerComposite->SetShadowTransformSetByAnimation(false);

  const nsIntRect* clipRect = aLayer->GetClipRect();
  if (clipRect) {
    nsIntRect transformedClipRect(*clipRect);
    transformedClipRect.MoveBy(aTranslation.x, aTranslation.y);
    layerComposite->SetShadowClipRect(&transformedClipRect);
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
                                                   const Matrix4x4& aPreviousTransformForRoot,
                                                   const Matrix4x4& aCurrentTransformForRoot,
                                                   const LayerMargin& aFixedLayerMargins)
{
  bool isRootFixed = aLayer->GetIsFixedPosition() &&
    !aLayer->GetParent()->GetIsFixedPosition();
  bool isStickyForSubtree = aLayer->GetIsStickyPosition() &&
    aTransformedSubtreeRoot->AsContainerLayer() &&
    aLayer->GetStickyScrollContainerId() ==
      aTransformedSubtreeRoot->AsContainerLayer()->GetFrameMetrics().GetScrollId();
  if (aLayer != aTransformedSubtreeRoot && (isRootFixed || isStickyForSubtree)) {
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
    Matrix newCumulativeTransformInverse = newCumulativeTransform;
    newCumulativeTransformInverse.Invert();

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

    // Finally, apply the 2D translation to the layer transform.
    TranslateShadowLayer2D(aLayer, ThebesPoint(translation));

    // The transform has now been applied, so there's no need to iterate over
    // child layers.
    return;
  }

  // Fixed layers are relative to their nearest scrollable layer, so when we
  // encounter a scrollable layer, bail. ApplyAsyncContentTransformToTree will
  // have already recursed on this layer and called AlignFixedAndStickyLayers
  // on it with its own transforms.
  if (aLayer->AsContainerLayer() &&
      aLayer->AsContainerLayer()->GetFrameMetrics().IsScrollable() &&
      aLayer != aTransformedSubtreeRoot) {
    return;
  }

  for (Layer* child = aLayer->GetFirstChild();
       child; child = child->GetNextSibling()) {
    AlignFixedAndStickyLayers(child, aTransformedSubtreeRoot,
                              aPreviousTransformForRoot,
                              aCurrentTransformForRoot, aFixedLayerMargins);
  }
}

static void
SampleValue(float aPortion, Animation& aAnimation, nsStyleAnimation::Value& aStart,
            nsStyleAnimation::Value& aEnd, Animatable* aValue)
{
  nsStyleAnimation::Value interpolatedValue;
  NS_ASSERTION(aStart.GetUnit() == aEnd.GetUnit() ||
               aStart.GetUnit() == nsStyleAnimation::eUnit_None ||
               aEnd.GetUnit() == nsStyleAnimation::eUnit_None, "Must have same unit");
  nsStyleAnimation::Interpolate(aAnimation.property(), aStart, aEnd,
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
  gfxPoint3D transformOrigin = data.transformOrigin();
  transformOrigin.x = transformOrigin.x * cssPerDev;
  transformOrigin.y = transformOrigin.y * cssPerDev;
  gfxPoint3D perspectiveOrigin = data.perspectiveOrigin();
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
  gfxPoint3D scaledOrigin =
    gfxPoint3D(NS_round(NSAppUnitsToFloatPixels(origin.x, data.appUnitsPerDevPixel())),
               NS_round(NSAppUnitsToFloatPixels(origin.y, data.appUnitsPerDevPixel())),
               0.0f);

  transform.Translate(scaledOrigin);

  InfallibleTArray<TransformFunction> functions;
  Matrix4x4 realTransform;
  ToMatrix4x4(transform, realTransform);
  functions.AppendElement(TransformMatrix(realTransform));
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
    timing.mIterationCount = animation.iterationCount();
    timing.mDirection = animation.direction();
    // Animations typically only run on the compositor during their active
    // interval but if we end up sampling them outside that range (for
    // example, while they are waiting to be removed) we currently just
    // assume that we should fill.
    timing.mFillMode = NS_STYLE_ANIMATION_FILL_MODE_BOTH;

    ComputedTiming computedTiming =
      ElementAnimation::GetComputedTimingAt(elapsedDuration, timing);

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
        matrix = matrix * Matrix4x4().Scale(c->GetInheritedXScale(),
                                            c->GetInheritedYScale(),
                                            1);
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

Matrix4x4
CombineWithCSSTransform(const gfx3DMatrix& treeTransform, Layer* aLayer)
{
  Matrix4x4 result;
  ToMatrix4x4(treeTransform, result);
  result = result * aLayer->GetTransform();
  return result;
}

bool
AsyncCompositionManager::ApplyAsyncContentTransformToTree(TimeStamp aCurrentFrame,
                                                          Layer *aLayer,
                                                          bool* aWantNextFrame)
{
  bool appliedTransform = false;
  for (Layer* child = aLayer->GetFirstChild();
      child; child = child->GetNextSibling()) {
    appliedTransform |=
      ApplyAsyncContentTransformToTree(aCurrentFrame, child, aWantNextFrame);
  }

  ContainerLayer* container = aLayer->AsContainerLayer();
  if (!container) {
    return appliedTransform;
  }

  if (AsyncPanZoomController* controller = container->GetAsyncPanZoomController()) {
    LayerComposite* layerComposite = aLayer->AsLayerComposite();
    Matrix4x4 oldTransform = aLayer->GetTransform();

    ViewTransform treeTransformWithoutOverscroll, overscrollTransform;
    ScreenPoint scrollOffset;
    *aWantNextFrame |=
      controller->SampleContentTransformForFrame(aCurrentFrame,
                                                 &treeTransformWithoutOverscroll,
                                                 scrollOffset,
                                                 &overscrollTransform);

    const FrameMetrics& metrics = container->GetFrameMetrics();
    CSSToLayerScale paintScale = metrics.LayersPixelsPerCSSPixel();
    CSSRect displayPort(metrics.mCriticalDisplayPort.IsEmpty() ?
                        metrics.mDisplayPort : metrics.mCriticalDisplayPort);
    LayerMargin fixedLayerMargins(0, 0, 0, 0);
    ScreenPoint offset(0, 0);
    SyncFrameMetrics(scrollOffset, treeTransformWithoutOverscroll.mScale.scale,
                     metrics.mScrollableRect, mLayersUpdated, displayPort,
                     paintScale, mIsFirstPaint, fixedLayerMargins, offset);

    mIsFirstPaint = false;
    mLayersUpdated = false;

    // Apply the render offset
    mLayerManager->GetCompositor()->SetScreenRenderOffset(offset);

    Matrix4x4 transform = CombineWithCSSTransform(
        treeTransformWithoutOverscroll * overscrollTransform, aLayer);

    // GetTransform already takes the pre- and post-scale into account.  Since we
    // will apply the pre- and post-scale again when computing the effective
    // transform, we must apply the inverses here.
    transform.Scale(1.0f/container->GetPreXScale(),
                    1.0f/container->GetPreYScale(),
                    1);
    transform = transform * Matrix4x4().Scale(1.0f/aLayer->GetPostXScale(),
                                              1.0f/aLayer->GetPostYScale(),
                                              1);
    layerComposite->SetShadowTransform(transform);
    NS_ASSERTION(!layerComposite->GetShadowTransformSetByAnimation(),
                 "overwriting animated transform!");

    // Apply resolution scaling to the old transform - the layer tree as it is
    // doesn't have the necessary transform to display correctly.
    LayoutDeviceToLayerScale resolution = metrics.mCumulativeResolution;
    oldTransform.Scale(resolution.scale, resolution.scale, 1);

    // For the purpose of aligning fixed and sticky layers, we disregard
    // the overscroll transform when computing the 'aCurrentTransformForRoot'
    // parameter. This ensures that the overscroll transform is not unapplied,
    // and therefore that the visual effect applies to fixed and sticky layers.
    Matrix4x4 transformWithoutOverscroll = CombineWithCSSTransform(
        treeTransformWithoutOverscroll, aLayer);
    AlignFixedAndStickyLayers(aLayer, aLayer, oldTransform,
                              transformWithoutOverscroll, fixedLayerMargins);

    appliedTransform = true;
  }

  if (container->GetScrollbarDirection() != Layer::NONE) {
    ApplyAsyncTransformToScrollbar(aCurrentFrame, container);
  }
  return appliedTransform;
}

static bool
LayerHasNonContainerDescendants(ContainerLayer* aContainer)
{
  for (Layer* child = aContainer->GetFirstChild();
       child; child = child->GetNextSibling()) {
    ContainerLayer* container = child->AsContainerLayer();
    if (!container || LayerHasNonContainerDescendants(container)) {
      return true;
    }
  }

  return false;
}

static bool
LayerIsContainerForScrollbarTarget(Layer* aTarget, ContainerLayer* aScrollbar)
{
  if (!aTarget->AsContainerLayer()) {
    return false;
  }
  AsyncPanZoomController* apzc = aTarget->AsContainerLayer()->GetAsyncPanZoomController();
  if (!apzc) {
    return false;
  }
  const FrameMetrics& metrics = aTarget->AsContainerLayer()->GetFrameMetrics();
  if (metrics.GetScrollId() != aScrollbar->GetScrollbarTargetContainerId()) {
    return false;
  }
  return true;
}

static void
ApplyAsyncTransformToScrollbarForContent(TimeStamp aCurrentFrame, ContainerLayer* aScrollbar,
                                         Layer* aContent, bool aScrollbarIsChild)
{
  ContainerLayer* content = aContent->AsContainerLayer();

  // We only apply the transform if the scroll-target layer has non-container
  // children (i.e. when it has some possibly-visible content). This is to
  // avoid moving scroll-bars in the situation that only a scroll information
  // layer has been built for a scroll frame, as this would result in a
  // disparity between scrollbars and visible content.
  if (!LayerHasNonContainerDescendants(content)) {
    return;
  }

  const FrameMetrics& metrics = content->GetFrameMetrics();
  AsyncPanZoomController* apzc = content->GetAsyncPanZoomController();

  if (aScrollbarIsChild) {
    // Because we try to apply the scrollbar transform before we apply the async transform on
    // the actual content, we need to ensure that the APZC has updated any pending animations
    // to the current frame timestamp before we extract the transforms from it. The code in this
    // block accomplishes that and throws away the temp variables.
    // TODO: it might be cleaner to do a pass through the layer tree to advance all the APZC
    // transforms before updating the layer shadow transforms. That will allow removal of this code.
    ViewTransform treeTransform;
    ScreenPoint scrollOffset;
    apzc->SampleContentTransformForFrame(aCurrentFrame, &treeTransform, scrollOffset);
  }

  gfx3DMatrix asyncTransform = gfx3DMatrix(apzc->GetCurrentAsyncTransform());
  gfx3DMatrix nontransientTransform = apzc->GetNontransientAsyncTransform();
  gfx3DMatrix transientTransform = asyncTransform * nontransientTransform.Inverse();

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
    scrollbarTransform = scrollbarTransform * Matrix4x4().Scale(1.f, 1.f / transientTransform.GetYScale(), 1.f);
    scrollbarTransform = scrollbarTransform * Matrix4x4().Translate(0, -transientTransform._42 * scale, 0);
  }
  if (aScrollbar->GetScrollbarDirection() == Layer::HORIZONTAL) {
    float scale = metrics.CalculateCompositedSizeInCssPixels().width / metrics.mScrollableRect.width;
    scrollbarTransform = scrollbarTransform * Matrix4x4().Scale(1.f / transientTransform.GetXScale(), 1.f, 1.f);
    scrollbarTransform = scrollbarTransform * Matrix4x4().Translate(-transientTransform._41 * scale, 0, 0);
  }

  Matrix4x4 transform = scrollbarTransform * aScrollbar->GetTransform();

  if (aScrollbarIsChild) {
    // If the scrollbar layer is a child of the content it is a scrollbar for, then we
    // need to do an extra untransform to cancel out the transient async transform on
    // the content. This is needed because otherwise that transient async transform is
    // part of the effective transform of this scrollbar, and the scrollbar will jitter
    // as the content scrolls.
    Matrix4x4 targetUntransform;
    ToMatrix4x4(transientTransform.Inverse(), targetUntransform);
    transform = transform * targetUntransform;
  }

  // GetTransform already takes the pre- and post-scale into account.  Since we
  // will apply the pre- and post-scale again when computing the effective
  // transform, we must apply the inverses here.
  transform.Scale(1.0f/aScrollbar->GetPreXScale(),
                  1.0f/aScrollbar->GetPreYScale(),
                  1);
  transform = transform * Matrix4x4().Scale(1.0f/aScrollbar->GetPostXScale(),
                                            1.0f/aScrollbar->GetPostYScale(),
                                            1);
  aScrollbar->AsLayerComposite()->SetShadowTransform(transform);
}

static Layer*
FindScrolledLayerForScrollbar(ContainerLayer* aLayer, bool* aOutIsAncestor)
{
  // Search all siblings of aLayer and of its ancestors.
  for (Layer* ancestor = aLayer; ancestor; ancestor = ancestor->GetParent()) {
    for (Layer* scrollTarget = ancestor;
         scrollTarget;
         scrollTarget = scrollTarget->GetPrevSibling()) {
      if (scrollTarget != aLayer &&
          LayerIsContainerForScrollbarTarget(scrollTarget, aLayer)) {
        *aOutIsAncestor = (scrollTarget == ancestor);
        return scrollTarget;
      }
    }
    for (Layer* scrollTarget = ancestor->GetNextSibling();
         scrollTarget;
         scrollTarget = scrollTarget->GetNextSibling()) {
      if (LayerIsContainerForScrollbarTarget(scrollTarget, aLayer)) {
        *aOutIsAncestor = false;
        return scrollTarget;
      }
    }
  }
  return nullptr;
}

void
AsyncCompositionManager::ApplyAsyncTransformToScrollbar(TimeStamp aCurrentFrame, ContainerLayer* aLayer)
{
  // If this layer corresponds to a scrollbar, then there should be a layer that
  // is a previous sibling or a parent that has a matching ViewID on its FrameMetrics.
  // That is the content that this scrollbar is for. We pick up the transient
  // async transform from that layer and use it to update the scrollbar position.
  // Note that it is possible that the content layer is no longer there; in
  // this case we don't need to do anything because there can't be an async
  // transform on the content.
  bool isAncestor = false;
  Layer* scrollTarget = FindScrolledLayerForScrollbar(aLayer, &isAncestor);
  if (scrollTarget) {
    ApplyAsyncTransformToScrollbarForContent(aCurrentFrame, aLayer, scrollTarget,
                                             isAncestor);
  }
}

void
AsyncCompositionManager::TransformScrollableLayer(Layer* aLayer)
{
  LayerComposite* layerComposite = aLayer->AsLayerComposite();
  ContainerLayer* container = aLayer->AsContainerLayer();

  const FrameMetrics& metrics = container->GetFrameMetrics();
  // We must apply the resolution scale before a pan/zoom transform, so we call
  // GetTransform here.
  gfx3DMatrix currentTransform;
  To3DMatrix(aLayer->GetTransform(), currentTransform);
  Matrix4x4 oldTransform = aLayer->GetTransform();

  gfx3DMatrix treeTransform;

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
  LayerToScreenScale zoomAdjust = userZoom / geckoZoom;

  LayerPoint geckoScroll(0, 0);
  if (metrics.IsScrollable()) {
    geckoScroll = metrics.GetScrollOffset() * geckoZoom;
  }

  LayerPoint translation = (userScroll / zoomAdjust) - geckoScroll;
  treeTransform = gfx3DMatrix(ViewTransform(-translation,
                                            userZoom
                                          / metrics.mDevPixelsPerCSSPixel
                                          / metrics.GetParentResolution()));

  // The transform already takes the resolution scale into account.  Since we
  // will apply the resolution scale again when computing the effective
  // transform, we must apply the inverse resolution scale here.
  gfx3DMatrix computedTransform = treeTransform * currentTransform;
  computedTransform.Scale(1.0f/container->GetPreXScale(),
                          1.0f/container->GetPreYScale(),
                          1);
  computedTransform.ScalePost(1.0f/container->GetPostXScale(),
                              1.0f/container->GetPostYScale(),
                              1);
  Matrix4x4 matrix;
  ToMatrix4x4(computedTransform, matrix);
  layerComposite->SetShadowTransform(matrix);
  NS_ASSERTION(!layerComposite->GetShadowTransformSetByAnimation(),
               "overwriting animated transform!");

  // Apply resolution scaling to the old transform - the layer tree as it is
  // doesn't have the necessary transform to display correctly.
  oldTransform.Scale(metrics.mResolution.scale, metrics.mResolution.scale, 1);

  // Make sure that overscroll and under-zoom are represented in the old
  // transform so that fixed position content moves and scales accordingly.
  // These calculations will effectively scale and offset fixed position layers
  // in screen space when the compensatory transform is performed in
  // AlignFixedAndStickyLayers.
  ScreenRect contentScreenRect = mContentRect * userZoom;
  gfxPoint3D overscrollTranslation;
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
  oldTransform.Translate(overscrollTranslation.x,
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
  oldTransform.Scale(underZoomScale.width, underZoomScale.height, 1);

  // Make sure fixed position layers don't move away from their anchor points
  // when we're asynchronously panning or zooming
  AlignFixedAndStickyLayers(aLayer, aLayer, oldTransform,
                            aLayer->GetLocalTransform(), fixedLayerMargins);
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
  if (!ApplyAsyncContentTransformToTree(aCurrentFrame, root, &wantNextFrame)) {
    nsAutoTArray<Layer*,1> scrollableLayers;
#ifdef MOZ_WIDGET_ANDROID
    scrollableLayers.AppendElement(mLayerManager->GetPrimaryScrollableLayer());
#else
    mLayerManager->GetScrollableLayers(scrollableLayers);
#endif

    for (uint32_t i = 0; i < scrollableLayers.Length(); i++) {
      if (scrollableLayers[i]) {
        TransformScrollableLayer(scrollableLayers[i]);
      }
    }
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
