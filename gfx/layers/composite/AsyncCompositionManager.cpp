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
#include "gfxMatrix.h"                  // for gfxMatrix
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

struct nsCSSValueList;

using namespace mozilla::dom;

namespace mozilla {
namespace layers {

enum Op { Resolve, Detach };

static bool
IsSameDimension(ScreenOrientation o1, ScreenOrientation o2)
{
  bool isO1portrait = (o1 == eScreenOrientation_PortraitPrimary || o1 == eScreenOrientation_PortraitSecondary);
  bool isO2portrait = (o2 == eScreenOrientation_PortraitPrimary || o2 == eScreenOrientation_PortraitSecondary);
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
          ScreenOrientation chromeOrientation = aTargetConfig.orientation();
          ScreenOrientation contentOrientation = state->mTargetConfig.orientation();
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
  WalkTheTree<Resolve>(mLayerManager->GetRoot(),
                       mReadyForCompose,
                       mTargetConfig);
}

void
AsyncCompositionManager::DetachRefLayers()
{
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
GetBaseTransform2D(Layer* aLayer, gfxMatrix* aTransform)
{
  // Start with the animated transform if there is one
  return (aLayer->AsLayerComposite()->GetShadowTransformSetByAnimation() ?
          aLayer->GetLocalTransform() : aLayer->GetTransform()).Is2D(aTransform);
}

static void
TranslateShadowLayer2D(Layer* aLayer,
                       const gfxPoint& aTranslation)
{
  gfxMatrix layerTransform;
  if (!GetBaseTransform2D(aLayer, &layerTransform)) {
    return;
  }

  // Apply the 2D translation to the layer transform.
  layerTransform.x0 += aTranslation.x;
  layerTransform.y0 += aTranslation.y;

  // The transform already takes the resolution scale into account.  Since we
  // will apply the resolution scale again when computing the effective
  // transform, we must apply the inverse resolution scale here.
  gfx3DMatrix layerTransform3D = gfx3DMatrix::From2D(layerTransform);
  if (ContainerLayer* c = aLayer->AsContainerLayer()) {
    layerTransform3D.Scale(1.0f/c->GetPreXScale(),
                           1.0f/c->GetPreYScale(),
                           1);
  }
  layerTransform3D.ScalePost(1.0f/aLayer->GetPostXScale(),
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
                            gfxMatrix& aMatrix)
{
  // Accumulate the transforms between this layer and the subtree root layer.
  for (Layer* l = aLayer; l && l != aAncestor; l = l->GetParent()) {
    gfxMatrix l2D;
    if (!GetBaseTransform2D(l, &l2D)) {
      return false;
    }
    aMatrix.Multiply(l2D);
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
                                                   const gfx3DMatrix& aPreviousTransformForRoot,
                                                   const LayerMargin& aFixedLayerMargins)
{
  bool isRootFixed = aLayer->GetIsFixedPosition() &&
    !aLayer->GetParent()->GetIsFixedPosition();
  bool isStickyForSubtree = aLayer->GetIsStickyPosition() &&
    aTransformedSubtreeRoot->AsContainerLayer() &&
    aLayer->GetStickyScrollContainerId() ==
      aTransformedSubtreeRoot->AsContainerLayer()->GetFrameMetrics().mScrollId;
  if (aLayer != aTransformedSubtreeRoot && (isRootFixed || isStickyForSubtree)) {
    // Insert a translation so that the position of the anchor point is the same
    // before and after the change to the transform of aTransformedSubtreeRoot.
    // This currently only works for fixed layers with 2D transforms.

    // Accumulate the transforms between this layer and the subtree root layer.
    gfxMatrix ancestorTransform;
    if (!AccumulateLayerTransforms2D(aLayer->GetParent(), aTransformedSubtreeRoot,
                                     ancestorTransform)) {
      return;
    }

    gfxMatrix oldRootTransform;
    gfxMatrix newRootTransform;
    if (!aPreviousTransformForRoot.Is2D(&oldRootTransform) ||
        !aTransformedSubtreeRoot->GetLocalTransform().Is2D(&newRootTransform)) {
      return;
    }

    // Calculate the cumulative transforms between the subtree root with the
    // old transform and the current transform.
    gfxMatrix oldCumulativeTransform = ancestorTransform * oldRootTransform;
    gfxMatrix newCumulativeTransform = ancestorTransform * newRootTransform;
    if (newCumulativeTransform.IsSingular()) {
      return;
    }
    gfxMatrix newCumulativeTransformInverse = newCumulativeTransform;
    newCumulativeTransformInverse.Invert();

    // Now work out the translation necessary to make sure the layer doesn't
    // move given the new sub-tree root transform.
    gfxMatrix layerTransform;
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
    gfxPoint anchor(anchorInOldSubtreeLayerSpace.x, anchorInOldSubtreeLayerSpace.y);
    gfxPoint offsetAnchor(offsetAnchorInOldSubtreeLayerSpace.x, offsetAnchorInOldSubtreeLayerSpace.y);
    gfxPoint locallyTransformedAnchor = layerTransform.Transform(anchor);
    gfxPoint locallyTransformedOffsetAnchor = layerTransform.Transform(offsetAnchor);

    // Transforming the locallyTransformedAnchor by oldCumulativeTransform
    // returns the layer's anchor point relative to the parent of
    // aTransformedSubtreeRoot, before the new transform was applied.
    // Then, applying newCumulativeTransformInverse maps that point relative
    // to the layer's parent, which is the same coordinate space as
    // locallyTransformedAnchor again, allowing us to subtract them and find
    // out the offset necessary to make sure the layer stays stationary.
    gfxPoint oldAnchorPositionInNewSpace =
      newCumulativeTransformInverse.Transform(
        oldCumulativeTransform.Transform(locallyTransformedOffsetAnchor));
    gfxPoint translation = oldAnchorPositionInNewSpace - locallyTransformedAnchor;

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
    TranslateShadowLayer2D(aLayer, translation);

    // The transform has now been applied, so there's no need to iterate over
    // child layers.
    return;
  }

  for (Layer* child = aLayer->GetFirstChild();
       child; child = child->GetNextSibling()) {
    AlignFixedAndStickyLayers(child, aTransformedSubtreeRoot,
                              aPreviousTransformForRoot, aFixedLayerMargins);
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

  nsCSSValueList* interpolatedList = interpolatedValue.GetCSSValueListValue();

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
  functions.AppendElement(TransformMatrix(transform));
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

    double numIterations = animation.numIterations() != -1 ?
      animation.numIterations() : NS_IEEEPositiveInfinity();
    double positionInIteration =
      ElementAnimations::GetPositionInIteration(aPoint - animation.startTime(),
                                                animation.duration(),
                                                numIterations,
                                                animation.direction());

    NS_ABORT_IF_FALSE(0.0 <= positionInIteration &&
                      positionInIteration <= 1.0,
                      "position should be in [0-1]");

    int segmentIndex = 0;
    AnimationSegment* segment = animation.segments().Elements();
    while (segment->endPortion() < positionInIteration) {
      ++segment;
      ++segmentIndex;
    }

    double positionInSegment = (positionInIteration - segment->startPortion()) /
                                 (segment->endPortion() - segment->startPortion());

    double portion = animData.mFunctions[segmentIndex]->GetValue(positionInSegment);

    activeAnimations = true;

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
      gfx3DMatrix matrix = interpolatedValue.get_ArrayOfTransformFunction()[0].get_TransformMatrix().value();
      if (ContainerLayer* c = aLayer->AsContainerLayer()) {
        matrix.ScalePost(c->GetInheritedXScale(),
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
    gfx3DMatrix oldTransform = aLayer->GetTransform();

    ViewTransform treeTransform;
    ScreenPoint scrollOffset;
    *aWantNextFrame |=
      controller->SampleContentTransformForFrame(aCurrentFrame,
                                                 &treeTransform,
                                                 scrollOffset);

    const gfx3DMatrix& rootTransform = mLayerManager->GetRoot()->GetTransform();
    const FrameMetrics& metrics = container->GetFrameMetrics();
    // XXX We use rootTransform instead of metrics.mResolution here because on
    // Fennec the resolution is set on the root layer rather than the scrollable layer.
    // The SyncFrameMetrics call and the paintScale variable are used on Fennec only
    // so it doesn't affect any other platforms. See bug 732971.
    CSSToLayerScale paintScale = metrics.mDevPixelsPerCSSPixel
      / LayerToLayoutDeviceScale(rootTransform.GetXScale(), rootTransform.GetYScale());
    CSSRect displayPort(metrics.mCriticalDisplayPort.IsEmpty() ?
                        metrics.mDisplayPort : metrics.mCriticalDisplayPort);
    LayerMargin fixedLayerMargins(0, 0, 0, 0);
    ScreenPoint offset(0, 0);
    SyncFrameMetrics(scrollOffset, treeTransform.mScale.scale, metrics.mScrollableRect,
                     mLayersUpdated, displayPort, paintScale,
                     mIsFirstPaint, fixedLayerMargins, offset);

    mIsFirstPaint = false;
    mLayersUpdated = false;

    // Apply the render offset
    mLayerManager->GetCompositor()->SetScreenRenderOffset(offset);

    gfx3DMatrix transform(gfx3DMatrix(treeTransform) * aLayer->GetTransform());
    // The transform already takes the resolution scale into account.  Since we
    // will apply the resolution scale again when computing the effective
    // transform, we must apply the inverse resolution scale here.
    transform.Scale(1.0f/container->GetPreXScale(),
                    1.0f/container->GetPreYScale(),
                    1);
    transform.ScalePost(1.0f/aLayer->GetPostXScale(),
                        1.0f/aLayer->GetPostYScale(),
                        1);
    layerComposite->SetShadowTransform(transform);
    NS_ASSERTION(!layerComposite->GetShadowTransformSetByAnimation(),
                 "overwriting animated transform!");

    // Apply resolution scaling to the old transform - the layer tree as it is
    // doesn't have the necessary transform to display correctly.
#ifdef MOZ_WIDGET_ANDROID
    // XXX We use rootTransform instead of the resolution on the individual layer's
    // FrameMetrics on Fennec because the resolution is set on the root layer rather
    // than the scrollable layer. See bug 732971. On non-Fennec we do the right thing.
    LayoutDeviceToLayerScale resolution(1.0 / rootTransform.GetXScale(),
                                        1.0 / rootTransform.GetYScale());
#else
    LayoutDeviceToLayerScale resolution = metrics.mCumulativeResolution;
#endif
    oldTransform.Scale(resolution.scale, resolution.scale, 1);

    AlignFixedAndStickyLayers(aLayer, aLayer, oldTransform, fixedLayerMargins);

    appliedTransform = true;
  }

  return appliedTransform;
}

void
AsyncCompositionManager::TransformScrollableLayer(Layer* aLayer, const LayoutDeviceToLayerScale& aResolution)
{
  LayerComposite* layerComposite = aLayer->AsLayerComposite();
  ContainerLayer* container = aLayer->AsContainerLayer();

  const FrameMetrics& metrics = container->GetFrameMetrics();
  // We must apply the resolution scale before a pan/zoom transform, so we call
  // GetTransform here.
  const gfx3DMatrix& currentTransform = aLayer->GetTransform();
  gfx3DMatrix oldTransform = currentTransform;

  gfx3DMatrix treeTransform;

  CSSToLayerScale geckoZoom = metrics.mDevPixelsPerCSSPixel * aResolution;

  LayerIntPoint scrollOffsetLayerPixels = RoundedToInt(metrics.mScrollOffset * geckoZoom);

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
  ScreenPoint userScroll = metrics.mScrollOffset * userZoom;
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

  LayerIntPoint geckoScroll(0, 0);
  if (metrics.IsScrollable()) {
    geckoScroll = scrollOffsetLayerPixels;
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
  layerComposite->SetShadowTransform(computedTransform);
  NS_ASSERTION(!layerComposite->GetShadowTransformSetByAnimation(),
               "overwriting animated transform!");

  // Apply resolution scaling to the old transform - the layer tree as it is
  // doesn't have the necessary transform to display correctly.
  oldTransform.Scale(aResolution.scale, aResolution.scale, 1);

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
  oldTransform.Translate(overscrollTranslation);

  gfxSize underZoomScale(1.0f, 1.0f);
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
  AlignFixedAndStickyLayers(aLayer, aLayer, oldTransform, fixedLayerMargins);
}

bool
AsyncCompositionManager::TransformShadowTree(TimeStamp aCurrentFrame)
{
  PROFILER_LABEL("AsyncCompositionManager", "TransformShadowTree");
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
#ifdef MOZ_WIDGET_ANDROID
        // XXX We use rootTransform instead of the resolution on the individual layer's
        // FrameMetrics on Fennec because the resolution is set on the root layer rather
        // than the scrollable layer. See bug 732971. On non-Fennec we do the right thing.
        const gfx3DMatrix& rootTransform = root->GetTransform();
        LayoutDeviceToLayerScale resolution(1.0 / rootTransform.GetXScale(),
                                            1.0 / rootTransform.GetYScale());
#else
        LayoutDeviceToLayerScale resolution =
            scrollableLayers[i]->AsContainerLayer()->GetFrameMetrics().mCumulativeResolution;
#endif
        TransformScrollableLayer(scrollableLayers[i], resolution);
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
