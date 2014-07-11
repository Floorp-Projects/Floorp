/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * structures that represent things to be painted (ordered in z-order),
 * used during painting and hit testing
 */

#include "mozilla/dom/TabChild.h"
#include "mozilla/layers/PLayerTransaction.h"

#include "nsDisplayList.h"

#include "nsCSSRendering.h"
#include "nsRenderingContext.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsRegion.h"
#include "nsStyleStructInlines.h"
#include "nsStyleTransformMatrix.h"
#include "gfxMatrix.h"
#include "gfxPrefs.h"
#include "nsSVGIntegrationUtils.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"
#include "nsIFrameInlines.h"
#include "nsThemeConstants.h"
#include "LayerTreeInvalidation.h"

#include "imgIContainer.h"
#include "BasicLayers.h"
#include "nsBoxFrame.h"
#include "nsViewportFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsSVGEffects.h"
#include "nsSVGElement.h"
#include "nsSVGClipPathFrame.h"
#include "GeckoProfiler.h"
#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "nsViewManager.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "nsCanvasFrame.h"
#include "StickyScrollContainer.h"
#include "mozilla/EventStates.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "ActiveLayerTracker.h"
#include "nsContentUtils.h"
#include "nsPrintfCString.h"
#include "UnitTransforms.h"
#include "LayersLogging.h"

#include <stdint.h>
#include <algorithm>

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::layout;
typedef FrameMetrics::ViewID ViewID;

static inline nsIFrame*
GetTransformRootFrame(nsIFrame* aFrame)
{
  return nsLayoutUtils::GetTransformRootFrame(aFrame);
}

static void AddTransformFunctions(nsCSSValueList* aList,
                                  nsStyleContext* aContext,
                                  nsPresContext* aPresContext,
                                  nsRect& aBounds,
                                  InfallibleTArray<TransformFunction>& aFunctions)
{
  if (aList->mValue.GetUnit() == eCSSUnit_None) {
    return;
  }

  for (const nsCSSValueList* curr = aList; curr; curr = curr->mNext) {
    const nsCSSValue& currElem = curr->mValue;
    NS_ASSERTION(currElem.GetUnit() == eCSSUnit_Function,
                 "Stream should consist solely of functions!");
    nsCSSValue::Array* array = currElem.GetArrayValue();
    bool canStoreInRuleTree = true;
    switch (nsStyleTransformMatrix::TransformFunctionOf(array)) {
      case eCSSKeyword_rotatex:
      {
        double theta = array->Item(1).GetAngleValueInRadians();
        aFunctions.AppendElement(RotationX(theta));
        break;
      }
      case eCSSKeyword_rotatey:
      {
        double theta = array->Item(1).GetAngleValueInRadians();
        aFunctions.AppendElement(RotationY(theta));
        break;
      }
      case eCSSKeyword_rotatez:
      {
        double theta = array->Item(1).GetAngleValueInRadians();
        aFunctions.AppendElement(RotationZ(theta));
        break;
      }
      case eCSSKeyword_rotate:
      {
        double theta = array->Item(1).GetAngleValueInRadians();
        aFunctions.AppendElement(Rotation(theta));
        break;
      }
      case eCSSKeyword_rotate3d:
      {
        double x = array->Item(1).GetFloatValue();
        double y = array->Item(2).GetFloatValue();
        double z = array->Item(3).GetFloatValue();
        double theta = array->Item(4).GetAngleValueInRadians();
        aFunctions.AppendElement(Rotation3D(x, y, z, theta));
        break;
      }
      case eCSSKeyword_scalex:
      {
        double x = array->Item(1).GetFloatValue();
        aFunctions.AppendElement(Scale(x, 1, 1));
        break;
      }
      case eCSSKeyword_scaley:
      {
        double y = array->Item(1).GetFloatValue();
        aFunctions.AppendElement(Scale(1, y, 1));
        break;
      }
      case eCSSKeyword_scalez:
      {
        double z = array->Item(1).GetFloatValue();
        aFunctions.AppendElement(Scale(1, 1, z));
        break;
      }
      case eCSSKeyword_scale:
      {
        double x = array->Item(1).GetFloatValue();
        // scale(x) is shorthand for scale(x, x);
        double y = array->Count() == 2 ? x : array->Item(2).GetFloatValue();
        aFunctions.AppendElement(Scale(x, y, 1));
        break;
      }
      case eCSSKeyword_scale3d:
      {
        double x = array->Item(1).GetFloatValue();
        double y = array->Item(2).GetFloatValue();
        double z = array->Item(3).GetFloatValue();
        aFunctions.AppendElement(Scale(x, y, z));
        break;
      }
      case eCSSKeyword_translatex:
      {
        double x = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, canStoreInRuleTree,
          aBounds.Width());
        aFunctions.AppendElement(Translation(x, 0, 0));
        break;
      }
      case eCSSKeyword_translatey:
      {
        double y = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, canStoreInRuleTree,
          aBounds.Height());
        aFunctions.AppendElement(Translation(0, y, 0));
        break;
      }
      case eCSSKeyword_translatez:
      {
        double z = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, canStoreInRuleTree,
          0);
        aFunctions.AppendElement(Translation(0, 0, z));
        break;
      }
      case eCSSKeyword_translate:
      {
        double x = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, canStoreInRuleTree,
          aBounds.Width());
        // translate(x) is shorthand for translate(x, 0)
        double y = 0;
        if (array->Count() == 3) {
           y = nsStyleTransformMatrix::ProcessTranslatePart(
            array->Item(2), aContext, aPresContext, canStoreInRuleTree,
            aBounds.Height());
        }
        aFunctions.AppendElement(Translation(x, y, 0));
        break;
      }
      case eCSSKeyword_translate3d:
      {
        double x = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, canStoreInRuleTree,
          aBounds.Width());
        double y = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(2), aContext, aPresContext, canStoreInRuleTree,
          aBounds.Height());
        double z = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(3), aContext, aPresContext, canStoreInRuleTree,
          0);

        aFunctions.AppendElement(Translation(x, y, z));
        break;
      }
      case eCSSKeyword_skewx:
      {
        double x = array->Item(1).GetAngleValueInRadians();
        aFunctions.AppendElement(SkewX(x));
        break;
      }
      case eCSSKeyword_skewy:
      {
        double y = array->Item(1).GetAngleValueInRadians();
        aFunctions.AppendElement(SkewY(y));
        break;
      }
      case eCSSKeyword_skew:
      {
        double x = array->Item(1).GetAngleValueInRadians();
        // skew(x) is shorthand for skew(x, 0)
        double y = 0;
        if (array->Count() == 3) {
          y = array->Item(2).GetAngleValueInRadians();
        }
        aFunctions.AppendElement(Skew(x, y));
        break;
      }
      case eCSSKeyword_matrix:
      {
        gfx::Matrix4x4 matrix;
        matrix._11 = array->Item(1).GetFloatValue();
        matrix._12 = array->Item(2).GetFloatValue();
        matrix._13 = 0;
        matrix._14 = 0;
        matrix._21 = array->Item(3).GetFloatValue();
        matrix._22 = array->Item(4).GetFloatValue();
        matrix._23 = 0;
        matrix._24 = 0;
        matrix._31 = 0;
        matrix._32 = 0;
        matrix._33 = 1;
        matrix._34 = 0;
        matrix._41 = array->Item(5).GetFloatValue();
        matrix._42 = array->Item(6).GetFloatValue();
        matrix._43 = 0;
        matrix._44 = 1;
        aFunctions.AppendElement(TransformMatrix(matrix));
        break;
      }
      case eCSSKeyword_matrix3d:
      {
        gfx::Matrix4x4 matrix;
        matrix._11 = array->Item(1).GetFloatValue();
        matrix._12 = array->Item(2).GetFloatValue();
        matrix._13 = array->Item(3).GetFloatValue();
        matrix._14 = array->Item(4).GetFloatValue();
        matrix._21 = array->Item(5).GetFloatValue();
        matrix._22 = array->Item(6).GetFloatValue();
        matrix._23 = array->Item(7).GetFloatValue();
        matrix._24 = array->Item(8).GetFloatValue();
        matrix._31 = array->Item(9).GetFloatValue();
        matrix._32 = array->Item(10).GetFloatValue();
        matrix._33 = array->Item(11).GetFloatValue();
        matrix._34 = array->Item(12).GetFloatValue();
        matrix._41 = array->Item(13).GetFloatValue();
        matrix._42 = array->Item(14).GetFloatValue();
        matrix._43 = array->Item(15).GetFloatValue();
        matrix._44 = array->Item(16).GetFloatValue();
        aFunctions.AppendElement(TransformMatrix(matrix));
        break;
      }
      case eCSSKeyword_interpolatematrix:
      {
        gfx3DMatrix matrix;
        nsStyleTransformMatrix::ProcessInterpolateMatrix(matrix, array,
                                                         aContext,
                                                         aPresContext,
                                                         canStoreInRuleTree,
                                                         aBounds);
        gfx::Matrix4x4 transform;
        gfx::ToMatrix4x4(matrix, transform);
        aFunctions.AppendElement(TransformMatrix(transform));
        break;
      }
      case eCSSKeyword_perspective:
      {
        aFunctions.AppendElement(Perspective(array->Item(1).GetFloatValue()));
        break;
      }
      default:
        NS_ERROR("Function not handled yet!");
    }
  }
}

static TimingFunction
ToTimingFunction(css::ComputedTimingFunction& aCTF)
{
  if (aCTF.GetType() == nsTimingFunction::Function) {
    const nsSMILKeySpline* spline = aCTF.GetFunction();
    return TimingFunction(CubicBezierFunction(spline->X1(), spline->Y1(),
                                              spline->X2(), spline->Y2()));
  }

  uint32_t type = aCTF.GetType() == nsTimingFunction::StepStart ? 1 : 2;
  return TimingFunction(StepFunction(aCTF.GetSteps(), type));
}

static void
AddAnimationForProperty(nsIFrame* aFrame, nsCSSProperty aProperty,
                        mozilla::ElementAnimation* ea, Layer* aLayer,
                        AnimationData& aData, bool aPending)
{
  NS_ASSERTION(aLayer->AsContainerLayer(), "Should only animate ContainerLayer");
  nsStyleContext* styleContext = aFrame->StyleContext();
  nsPresContext* presContext = aFrame->PresContext();
  nsRect bounds = nsDisplayTransform::GetFrameBoundsForTransform(aFrame);

  mozilla::layers::Animation* animation =
    aPending ?
    aLayer->AddAnimationForNextTransaction() :
    aLayer->AddAnimation();

  animation->startTime() = ea->mStartTime + ea->mTiming.mDelay;
  animation->duration() = ea->mTiming.mIterationDuration;
  animation->iterationCount() = ea->mTiming.mIterationCount;
  animation->direction() = ea->mTiming.mDirection;
  animation->property() = aProperty;
  animation->data() = aData;

  for (uint32_t propIdx = 0; propIdx < ea->mProperties.Length(); propIdx++) {
    AnimationProperty* property = &ea->mProperties[propIdx];

    if (aProperty != property->mProperty) {
      continue;
    }

    for (uint32_t segIdx = 0; segIdx < property->mSegments.Length(); segIdx++) {
      AnimationPropertySegment* segment = &property->mSegments[segIdx];

      AnimationSegment* animSegment = animation->segments().AppendElement();
      if (aProperty == eCSSProperty_transform) {
        animSegment->startState() = InfallibleTArray<TransformFunction>();
        animSegment->endState() = InfallibleTArray<TransformFunction>();

        nsCSSValueSharedList* list = segment->mFromValue.GetCSSValueSharedListValue();
        AddTransformFunctions(list->mHead, styleContext, presContext, bounds,
                              animSegment->startState().get_ArrayOfTransformFunction());

        list = segment->mToValue.GetCSSValueSharedListValue();
        AddTransformFunctions(list->mHead, styleContext, presContext, bounds,
                              animSegment->endState().get_ArrayOfTransformFunction());
      } else if (aProperty == eCSSProperty_opacity) {
        animSegment->startState() = segment->mFromValue.GetFloatValue();
        animSegment->endState() = segment->mToValue.GetFloatValue();
      }

      animSegment->startPortion() = segment->mFromKey;
      animSegment->endPortion() = segment->mToKey;
      animSegment->sampleFn() = ToTimingFunction(segment->mTimingFunction);
    }
  }
}

static void
AddAnimationsForProperty(nsIFrame* aFrame, nsCSSProperty aProperty,
                         ElementAnimationPtrArray& aAnimations,
                         Layer* aLayer, AnimationData& aData,
                         bool aPending) {
  mozilla::TimeStamp currentTime =
    aFrame->PresContext()->RefreshDriver()->MostRecentRefresh();
  for (uint32_t animIdx = 0; animIdx < aAnimations.Length(); animIdx++) {
    mozilla::ElementAnimation* anim = aAnimations[animIdx];
    if (!(anim->HasAnimationOfProperty(aProperty) &&
          anim->IsRunningAt(currentTime))) {
      continue;
    }
    AddAnimationForProperty(aFrame, aProperty, anim, aLayer, aData, aPending);
    anim->mIsRunningOnCompositor = true;
  }
}

/* static */ void
nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(Layer* aLayer,
                                                         nsDisplayListBuilder* aBuilder,
                                                         nsDisplayItem* aItem,
                                                         nsIFrame* aFrame,
                                                         nsCSSProperty aProperty)
{
  // This function can be called in two ways:  from
  // nsDisplay*::BuildLayer while constructing a layer (with all
  // pointers non-null), or from RestyleManager's handling of
  // UpdateOpacityLayer/UpdateTransformLayer hints.
  MOZ_ASSERT(!aBuilder == !aItem,
             "should only be called in two configurations, with both "
             "aBuilder and aItem, or with neither");
  MOZ_ASSERT(!aItem || aFrame == aItem->Frame(), "frame mismatch");

  bool pending = !aBuilder;

  if (pending) {
    aLayer->ClearAnimationsForNextTransaction();
  } else {
    aLayer->ClearAnimations();
  }

  nsIContent* content = aFrame->GetContent();
  if (!content) {
    return;
  }
  ElementAnimationCollection* transitions =
    nsTransitionManager::GetAnimationsForCompositor(content, aProperty);
  ElementAnimationCollection* animations =
    nsAnimationManager::GetAnimationsForCompositor(content, aProperty);

  if (!animations && !transitions) {
    return;
  }

  // If the frame is not prerendered, bail out.
  // Do this check only during layer construction; during updating the
  // caller is required to check it appropriately.
  if (aItem && !aItem->CanUseAsyncAnimations(aBuilder)) {
    // AnimationManager or TransitionManager need to know that we refused to
    // run this animation asynchronously so that they will not throttle the
    // main thread animation.
    aFrame->Properties().Set(nsIFrame::RefusedAsyncAnimation(),
                            reinterpret_cast<void*>(intptr_t(true)));

    // We need to schedule another refresh driver run so that AnimationManager
    // or TransitionManager get a chance to unthrottle the animation.
    aFrame->SchedulePaint();
    return;
  }

  AnimationData data;
  if (aProperty == eCSSProperty_transform) {
    nsRect bounds = nsDisplayTransform::GetFrameBoundsForTransform(aFrame);
    // all data passed directly to the compositor should be in css pixels
    float scale = nsDeviceContext::AppUnitsPerCSSPixel();
    gfxPoint3D offsetToTransformOrigin =
      nsDisplayTransform::GetDeltaToTransformOrigin(aFrame, scale, &bounds);
    gfxPoint3D offsetToPerspectiveOrigin =
      nsDisplayTransform::GetDeltaToPerspectiveOrigin(aFrame, scale);
    nscoord perspective = 0.0;
    nsStyleContext* parentStyleContext = aFrame->StyleContext()->GetParent();
    if (parentStyleContext) {
      const nsStyleDisplay* disp = parentStyleContext->StyleDisplay();
      if (disp && disp->mChildPerspective.GetUnit() == eStyleUnit_Coord) {
        perspective = disp->mChildPerspective.GetCoordValue();
      }
    }
    nsPoint origin;
    if (aItem) {
      origin = aItem->ToReferenceFrame();
    } else {
      // transform display items used a reference frame computed from
      // their GetTransformRootFrame().
      nsIFrame* referenceFrame =
        nsLayoutUtils::GetReferenceFrame(GetTransformRootFrame(aFrame));
      origin = aFrame->GetOffsetToCrossDoc(referenceFrame);
    }

    data = TransformData(origin, offsetToTransformOrigin,
                         offsetToPerspectiveOrigin, bounds, perspective,
                         aFrame->PresContext()->AppUnitsPerDevPixel());
  } else if (aProperty == eCSSProperty_opacity) {
    data = null_t();
  }

  if (transitions) {
    AddAnimationsForProperty(aFrame, aProperty, transitions->mAnimations,
                             aLayer, data, pending);
    aLayer->SetAnimationGeneration(transitions->mAnimationGeneration);
  }

  if (animations) {
    AddAnimationsForProperty(aFrame, aProperty, animations->mAnimations,
                             aLayer, data, pending);
    aLayer->SetAnimationGeneration(animations->mAnimationGeneration);
  }
}

nsDisplayListBuilder::nsDisplayListBuilder(nsIFrame* aReferenceFrame,
    Mode aMode, bool aBuildCaret)
    : mReferenceFrame(aReferenceFrame),
      mIgnoreScrollFrame(nullptr),
      mLayerEventRegions(nullptr),
      mCurrentTableItem(nullptr),
      mFinalTransparentRegion(nullptr),
      mCachedOffsetFrame(aReferenceFrame),
      mCachedReferenceFrame(aReferenceFrame),
      mCachedOffset(0, 0),
      mGlassDisplayItem(nullptr),
      mMode(aMode),
      mCurrentScrollParentId(FrameMetrics::NULL_SCROLL_ID),
      mCurrentScrollbarTarget(FrameMetrics::NULL_SCROLL_ID),
      mCurrentScrollbarFlags(0),
      mBuildCaret(aBuildCaret),
      mIgnoreSuppression(false),
      mHadToIgnoreSuppression(false),
      mIsAtRootOfPseudoStackingContext(false),
      mIncludeAllOutOfFlows(false),
      mDescendIntoSubdocuments(true),
      mSelectedFramesOnly(false),
      mAccurateVisibleRegions(false),
      mAllowMergingAndFlattening(true),
      mWillComputePluginGeometry(false),
      mInTransform(false),
      mInFixedPos(false),
      mSyncDecodeImages(false),
      mIsPaintingToWindow(false),
      mIsCompositingCheap(false),
      mContainsPluginItem(false),
      mAncestorHasTouchEventHandler(false),
      mHaveScrollableDisplayPort(false)
{
  MOZ_COUNT_CTOR(nsDisplayListBuilder);
  PL_InitArenaPool(&mPool, "displayListArena", 1024,
                   std::max(NS_ALIGNMENT_OF(void*),NS_ALIGNMENT_OF(double))-1);

  nsPresContext* pc = aReferenceFrame->PresContext();
  nsIPresShell *shell = pc->PresShell();
  if (pc->IsRenderingOnlySelection()) {
    nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(shell));
    if (selcon) {
      selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                           getter_AddRefs(mBoundingSelection));
    }
  }

  nsCSSRendering::BeginFrameTreesLocked();
  PR_STATIC_ASSERT(nsDisplayItem::TYPE_MAX < (1 << nsDisplayItem::TYPE_BITS));
}

static void MarkFrameForDisplay(nsIFrame* aFrame, nsIFrame* aStopAtFrame) {
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderFor(f)) {
    if (f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)
      return;
    f->AddStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

void nsDisplayListBuilder::SetContainsBlendMode(uint8_t aBlendMode)
{
  MOZ_ASSERT(aBlendMode != NS_STYLE_BLEND_NORMAL);
  gfxContext::GraphicsOperator op = nsCSSRendering::GetGFXBlendMode(aBlendMode);
  mContainedBlendModes += gfx::CompositionOpForOp(op);
}

void nsDisplayListBuilder::MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame,
                                                        nsIFrame* aFrame,
                                                        const nsRect& aDirtyRect)
{
  nsRect dirtyRectRelativeToDirtyFrame = aDirtyRect;
  if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(aFrame)) {
    NS_ASSERTION(aDirtyFrame == aFrame->GetParent(), "Dirty frame should be viewport frame");
    // position: fixed items are reflowed into and only drawn inside the
    // viewport, or the scroll position clamping scrollport size, if one is
    // set.
    nsIPresShell* ps = aFrame->PresContext()->PresShell();
    dirtyRectRelativeToDirtyFrame.MoveTo(0, 0);
    if (ps->IsScrollPositionClampingScrollPortSizeSet()) {
      dirtyRectRelativeToDirtyFrame.SizeTo(ps->GetScrollPositionClampingScrollPortSize());
    } else {
      dirtyRectRelativeToDirtyFrame.SizeTo(aDirtyFrame->GetSize());
    }
  }

  nsRect dirty = dirtyRectRelativeToDirtyFrame - aFrame->GetOffsetTo(aDirtyFrame);
  nsRect overflowRect = aFrame->GetVisualOverflowRect();

  if (aFrame->IsTransformed() &&
      nsLayoutUtils::HasAnimationsForCompositor(aFrame->GetContent(),
                                                eCSSProperty_transform)) {
   /**
    * Add a fuzz factor to the overflow rectangle so that elements only just
    * out of view are pulled into the display list, so they can be
    * prerendered if necessary.
    */
    overflowRect.Inflate(nsPresContext::CSSPixelsToAppUnits(32));
  }

  if (!dirty.IntersectRect(dirty, overflowRect))
    return;
  const DisplayItemClip* clip = mClipState.GetClipForContainingBlockDescendants();
  OutOfFlowDisplayData* data = clip ? new OutOfFlowDisplayData(*clip, dirty)
    : new OutOfFlowDisplayData(dirty);
  aFrame->Properties().Set(nsDisplayListBuilder::OutOfFlowDisplayDataProperty(), data);

  MarkFrameForDisplay(aFrame, aDirtyFrame);
}

static void UnmarkFrameForDisplay(nsIFrame* aFrame) {
  nsPresContext* presContext = aFrame->PresContext();
  presContext->PropertyTable()->
    Delete(aFrame, nsDisplayListBuilder::OutOfFlowDisplayDataProperty());

  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderFor(f)) {
    if (!(f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO))
      return;
    f->RemoveStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
  }
}

static void RecordFrameMetrics(nsIFrame* aForFrame,
                               nsIFrame* aScrollFrame,
                               const nsIFrame* aReferenceFrame,
                               ContainerLayer* aRoot,
                               const nsRect& aVisibleRect,
                               const nsRect& aViewport,
                               bool aForceNullScrollId,
                               bool aIsRoot,
                               const ContainerLayerParameters& aContainerParameters) {
  nsPresContext* presContext = aForFrame->PresContext();
  int32_t auPerDevPixel = presContext->AppUnitsPerDevPixel();
  LayoutDeviceToLayerScale resolution(aContainerParameters.mXScale, aContainerParameters.mYScale);

  nsIntRect visible = aVisibleRect.ScaleToNearestPixels(
    resolution.scale, resolution.scale, auPerDevPixel);
  aRoot->SetVisibleRegion(visible);

  nsIPresShell* presShell = presContext->GetPresShell();
  FrameMetrics metrics;
  metrics.mViewport = CSSRect::FromAppUnits(aViewport);

  ViewID scrollId = FrameMetrics::NULL_SCROLL_ID;
  nsIContent* content = aScrollFrame ? aScrollFrame->GetContent() : nullptr;
  if (content) {
    if (!aForceNullScrollId) {
      scrollId = nsLayoutUtils::FindOrCreateIDFor(content);
    }
    nsRect dp;
    if (nsLayoutUtils::GetDisplayPort(content, &dp)) {
      metrics.mDisplayPort = CSSRect::FromAppUnits(dp);
      nsLayoutUtils::LogTestDataForPaint(presShell, scrollId, "displayport",
          metrics.mDisplayPort);
    }
    if (nsLayoutUtils::GetCriticalDisplayPort(content, &dp)) {
      metrics.mCriticalDisplayPort = CSSRect::FromAppUnits(dp);
    }
    DisplayPortMarginsPropertyData* marginsData =
        static_cast<DisplayPortMarginsPropertyData*>(content->GetProperty(nsGkAtoms::DisplayPortMargins));
    if (marginsData) {
      metrics.SetDisplayPortMargins(marginsData->mMargins);
    }
  }

  nsIScrollableFrame* scrollableFrame = nullptr;
  if (aScrollFrame)
    scrollableFrame = aScrollFrame->GetScrollTargetFrame();

  metrics.mScrollableRect = CSSRect::FromAppUnits(
    nsLayoutUtils::CalculateScrollableRectForFrame(scrollableFrame, aForFrame));

  if (scrollableFrame) {
    nsPoint scrollPosition = scrollableFrame->GetScrollPosition();
    metrics.SetScrollOffset(CSSPoint::FromAppUnits(scrollPosition));

    // If the frame was scrolled since the last layers update, and by
    // something other than the APZ code, we want to tell the APZ to update
    // its scroll offset.
    nsIAtom* originOfLastScroll = scrollableFrame->OriginOfLastScroll();
    if (originOfLastScroll && originOfLastScroll != nsGkAtoms::apz) {
      metrics.SetScrollOffsetUpdated(scrollableFrame->CurrentScrollGeneration());
    }
  }

  metrics.SetScrollId(scrollId);
  metrics.mIsRoot = aIsRoot;

  // Only the root scrollable frame for a given presShell should pick up
  // the presShell's resolution. All the other frames are 1.0.
  if (aScrollFrame == presShell->GetRootScrollFrame()) {
    metrics.mResolution = ParentLayerToLayerScale(presShell->GetXResolution(),
                                                  presShell->GetYResolution());
  } else {
    metrics.mResolution = ParentLayerToLayerScale(1.0f);
  }

  // For the cumulateive resolution, multiply the resolutions of all the
  // presShells back up to the root
  metrics.mCumulativeResolution = LayoutDeviceToLayerScale(1.0f);
  nsIPresShell* curPresShell = presShell;
  while (curPresShell != nullptr) {
    ParentLayerToLayerScale presShellResolution(curPresShell->GetXResolution(),
                                                curPresShell->GetYResolution());
    metrics.mCumulativeResolution.scale *= presShellResolution.scale;
    nsPresContext* parentContext = curPresShell->GetPresContext()->GetParentPresContext();
    curPresShell = parentContext ? parentContext->GetPresShell() : nullptr;
  }

  metrics.mDevPixelsPerCSSPixel = CSSToLayoutDeviceScale(
    (float)nsPresContext::AppUnitsPerCSSPixel() / auPerDevPixel);

  // Initially, AsyncPanZoomController should render the content to the screen
  // at the painted resolution.
  const LayerToScreenScale layerToScreenScale(1.0f);
  metrics.SetZoom(metrics.mCumulativeResolution * metrics.mDevPixelsPerCSSPixel
                  * layerToScreenScale);

  if (presShell) {
    nsIDocument* document = nullptr;
    document = presShell->GetDocument();
    if (document) {
      nsCOMPtr<nsPIDOMWindow> innerWin(document->GetInnerWindow());
      if (innerWin) {
        metrics.mMayHaveTouchListeners = innerWin->HasTouchEventListeners();
      }
    }
    metrics.mMayHaveTouchCaret = presShell->MayHaveTouchCaret();
  }

  LayoutDeviceToParentLayerScale layoutToParentLayerScale =
    // The ScreenToParentLayerScale should be mTransformScale which is not calculated yet,
    // but we don't yet handle CSS transforms, so we assume it's 1 here.
    metrics.mCumulativeResolution * LayerToScreenScale(1.0) * ScreenToParentLayerScale(1.0);

  // Calculate the composition bounds as the size of the scroll frame and
  // its origin relative to the reference frame.
  // If aScrollFrame is null, we are in a document without a root scroll frame,
  // so it's a xul document. In this case, use the size of the viewport frame.
  nsIFrame* frameForCompositionBoundsCalculation = aScrollFrame ? aScrollFrame : aForFrame;
  nsRect compositionBounds(frameForCompositionBoundsCalculation->GetOffsetToCrossDoc(aReferenceFrame),
                           frameForCompositionBoundsCalculation->GetSize());
  metrics.mCompositionBounds = LayoutDeviceRect::FromAppUnits(compositionBounds, auPerDevPixel)
                                * layoutToParentLayerScale;

  // For the root scroll frame of the root content document, the above calculation
  // will yield the size of the viewport frame as the composition bounds, which
  // doesn't actually correspond to what is visible when
  // nsIDOMWindowUtils::setCSSViewport has been called to modify the visible area of
  // the prescontext that the viewport frame is reflowed into. In that case if our
  // document has a widget then the widget's bounds will correspond to what is
  // visible. If we don't have a widget the root view's bounds correspond to what
  // would be visible because they don't get modified by setCSSViewport.
  bool isRootScrollFrame = aScrollFrame == presShell->GetRootScrollFrame();
  bool isRootContentDocRootScrollFrame = isRootScrollFrame
                                      && presContext->IsRootContentDocument();
  if (isRootContentDocRootScrollFrame) {
    if (nsIFrame* rootFrame = presShell->GetRootFrame()) {
      if (nsView* view = rootFrame->GetView()) {
        nsRect viewBoundsAppUnits = view->GetBounds() + rootFrame->GetOffsetToCrossDoc(aReferenceFrame);
        ParentLayerRect viewBounds = LayoutDeviceRect::FromAppUnits(viewBoundsAppUnits, auPerDevPixel)
                                     * layoutToParentLayerScale;

        // On Android, we need to do things a bit differently to get things
        // right (see bug 983208, bug 988882). We use the bounds of the nearest
        // widget, but clamp the height to the view bounds height. This clamping
        // is done to get correct results for a page where the page is sized to
        // the screen and thus the dynamic toolbar never disappears. In such a
        // case, we want the composition bounds to exclude the toolbar height,
        // but the widget bounds includes it. We don't currently have a good way
        // of knowing about the toolbar height, but clamping to the view bounds
        // height gives the correct answer in the cases we care about.
        nsIWidget* widget =
#ifdef MOZ_WIDGET_ANDROID
            rootFrame->GetNearestWidget();
#else
            view->GetWidget();
#endif
        if (widget) {
          nsIntRect widgetBounds;
          widget->GetBounds(widgetBounds);
          metrics.mCompositionBounds = ParentLayerRect(ViewAs<ParentLayerPixel>(widgetBounds));
#ifdef MOZ_WIDGET_ANDROID
          if (viewBounds.height < metrics.mCompositionBounds.height) {
            metrics.mCompositionBounds.height = viewBounds.height;
          }
#endif
        } else {
          metrics.mCompositionBounds = viewBounds;
        }
      }
    }
  }

  // Adjust composition bounds for the size of scroll bars.
  if (scrollableFrame && !LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars)) {
    nsMargin sizes = scrollableFrame->GetActualScrollbarSizes();
    // Scrollbars are not subject to scaling, so CSS pixels = layer pixels for them.
    ParentLayerMargin boundMargins = CSSMargin::FromAppUnits(sizes) * CSSToParentLayerScale(1.0f);
    metrics.mCompositionBounds.Deflate(boundMargins);
  }

  metrics.SetRootCompositionSize(
    nsLayoutUtils::CalculateRootCompositionSize(aScrollFrame ? aScrollFrame : aForFrame,
                                                isRootContentDocRootScrollFrame, metrics));

  if (gfxPrefs::APZPrintTree()) {
    if (nsIContent* content = frameForCompositionBoundsCalculation->GetContent()) {
      nsAutoString contentDescription;
      content->Describe(contentDescription);
      metrics.SetContentDescription(NS_LossyConvertUTF16toASCII(contentDescription).get());
    }
  }

  metrics.SetPresShellId(presShell->GetPresShellId());

  // If the scroll frame's content is marked 'scrollgrab', record this
  // in the FrameMetrics so APZ knows to provide the scroll grabbing
  // behaviour.
  if (aScrollFrame && nsContentUtils::HasScrollgrab(aScrollFrame->GetContent())) {
    metrics.SetHasScrollgrab(true);
  }

  aRoot->SetFrameMetrics(metrics);

  // Also compute and set the background color on the container.
  // This is needed for APZ overscrolling support.
  if (aScrollFrame) {
    // FindBackground() does not work for a root scroll frame, need to use the
    // root frame instead.
    nsIFrame* backgroundFrame = isRootScrollFrame ? presShell->GetRootFrame() : aScrollFrame;
    nsStyleContext* backgroundStyle;
    if (nsCSSRendering::FindBackground(backgroundFrame, &backgroundStyle)) {
      aRoot->SetBackgroundColor(backgroundStyle->StyleBackground()->mBackgroundColor);
    }
  }
}

nsDisplayListBuilder::~nsDisplayListBuilder() {
  NS_ASSERTION(mFramesMarkedForDisplay.Length() == 0,
               "All frames should have been unmarked");
  NS_ASSERTION(mPresShellStates.Length() == 0,
               "All presshells should have been exited");
  NS_ASSERTION(!mCurrentTableItem, "No table item should be active");

  nsCSSRendering::EndFrameTreesLocked();

  for (uint32_t i = 0; i < mDisplayItemClipsToDestroy.Length(); ++i) {
    mDisplayItemClipsToDestroy[i]->DisplayItemClip::~DisplayItemClip();
  }

  PL_FinishArenaPool(&mPool);
  MOZ_COUNT_DTOR(nsDisplayListBuilder);
}

uint32_t
nsDisplayListBuilder::GetBackgroundPaintFlags() {
  uint32_t flags = 0;
  if (mSyncDecodeImages) {
    flags |= nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES;
  }
  if (mIsPaintingToWindow) {
    flags |= nsCSSRendering::PAINTBG_TO_WINDOW;
  }
  return flags;
}

void
nsDisplayListBuilder::SubtractFromVisibleRegion(nsRegion* aVisibleRegion,
                                                const nsRegion& aRegion)
{
  if (aRegion.IsEmpty())
    return;

  nsRegion tmp;
  tmp.Sub(*aVisibleRegion, aRegion);
  // Don't let *aVisibleRegion get too complex, but don't let it fluff out
  // to its bounds either, which can be very bad (see bug 516740).
  // Do let aVisibleRegion get more complex if by doing so we reduce its
  // area by at least half.
  if (GetAccurateVisibleRegions() || tmp.GetNumRects() <= 15 ||
      tmp.Area() <= aVisibleRegion->Area()/2) {
    *aVisibleRegion = tmp;
  }
}

nsCaret *
nsDisplayListBuilder::GetCaret() {
  nsRefPtr<nsCaret> caret = CurrentPresShellState()->mPresShell->GetCaret();
  return caret;
}

void
nsDisplayListBuilder::EnterPresShell(nsIFrame* aReferenceFrame,
                                     const nsRect& aDirtyRect) {
  PresShellState* state = mPresShellStates.AppendElement();
  if (!state)
    return;
  state->mPresShell = aReferenceFrame->PresContext()->PresShell();
  state->mCaretFrame = nullptr;
  state->mFirstFrameMarkedForDisplay = mFramesMarkedForDisplay.Length();

  state->mPresShell->UpdateCanvasBackground();

  if (mIsPaintingToWindow) {
    mReferenceFrame->AddPaintedPresShell(state->mPresShell);

    state->mPresShell->IncrementPaintCount();
  }

  bool buildCaret = mBuildCaret;
  if (mIgnoreSuppression || !state->mPresShell->IsPaintingSuppressed()) {
    if (state->mPresShell->IsPaintingSuppressed()) {
      mHadToIgnoreSuppression = true;
    }
    state->mIsBackgroundOnly = false;
  } else {
    state->mIsBackgroundOnly = true;
    buildCaret = false;
  }

  if (!buildCaret)
    return;

  nsRefPtr<nsCaret> caret = state->mPresShell->GetCaret();
  state->mCaretFrame = caret->GetCaretFrame();
  NS_ASSERTION(state->mCaretFrame == caret->GetCaretFrame(),
               "GetCaretFrame() is unstable");

  if (state->mCaretFrame) {
    // Check if the dirty rect intersects with the caret's dirty rect.
    nsRect caretRect =
      caret->GetCaretRect() + state->mCaretFrame->GetOffsetTo(aReferenceFrame);
    if (caretRect.Intersects(aDirtyRect)) {
      // Okay, our rects intersect, let's mark the frame and all of its ancestors.
      mFramesMarkedForDisplay.AppendElement(state->mCaretFrame);
      MarkFrameForDisplay(state->mCaretFrame, nullptr);
    }
  }
}

void
nsDisplayListBuilder::LeavePresShell(nsIFrame* aReferenceFrame,
                                     const nsRect& aDirtyRect) {
  if (CurrentPresShellState()->mPresShell != aReferenceFrame->PresContext()->PresShell()) {
    // Must have not allocated a state for this presshell, presumably due
    // to OOM.
    return;
  }

  ResetMarkedFramesForDisplayList();
  mPresShellStates.SetLength(mPresShellStates.Length() - 1);
}

void
nsDisplayListBuilder::ResetMarkedFramesForDisplayList()
{
  // Unmark and pop off the frames marked for display in this pres shell.
  uint32_t firstFrameForShell = CurrentPresShellState()->mFirstFrameMarkedForDisplay;
  for (uint32_t i = firstFrameForShell;
       i < mFramesMarkedForDisplay.Length(); ++i) {
    UnmarkFrameForDisplay(mFramesMarkedForDisplay[i]);
  }
  mFramesMarkedForDisplay.SetLength(firstFrameForShell);
}

void
nsDisplayListBuilder::MarkFramesForDisplayList(nsIFrame* aDirtyFrame,
                                               const nsFrameList& aFrames,
                                               const nsRect& aDirtyRect) {
  mFramesMarkedForDisplay.SetCapacity(mFramesMarkedForDisplay.Length() + aFrames.GetLength());
  for (nsFrameList::Enumerator e(aFrames); !e.AtEnd(); e.Next()) {
    mFramesMarkedForDisplay.AppendElement(e.get());
    MarkOutOfFlowFrameForDisplay(aDirtyFrame, e.get(), aDirtyRect);
  }
}

void
nsDisplayListBuilder::MarkPreserve3DFramesForDisplayList(nsIFrame* aDirtyFrame, const nsRect& aDirtyRect)
{
  nsAutoTArray<nsIFrame::ChildList,4> childListArray;
  aDirtyFrame->GetChildLists(&childListArray);
  nsIFrame::ChildListArrayIterator lists(childListArray);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame *child = childFrames.get();
      if (child->Preserves3D()) {
        mFramesMarkedForDisplay.AppendElement(child);
        nsRect dirty = aDirtyRect - child->GetOffsetTo(aDirtyFrame);

        child->Properties().Set(nsDisplayListBuilder::Preserve3DDirtyRectProperty(),
                           new nsRect(dirty));

        MarkFrameForDisplay(child, aDirtyFrame);
      }
    }
  }
}

void*
nsDisplayListBuilder::Allocate(size_t aSize) {
  void *tmp;
  PL_ARENA_ALLOCATE(tmp, &mPool, aSize);
  if (!tmp) {
    NS_RUNTIMEABORT("out of memory");
  }
  return tmp;
}

const DisplayItemClip*
nsDisplayListBuilder::AllocateDisplayItemClip(const DisplayItemClip& aOriginal)
{
  void* p = Allocate(sizeof(DisplayItemClip));
  if (!aOriginal.GetRoundedRectCount()) {
    memcpy(p, &aOriginal, sizeof(DisplayItemClip));
    return static_cast<DisplayItemClip*>(p);
  }

  DisplayItemClip* c = new (p) DisplayItemClip(aOriginal);
  mDisplayItemClipsToDestroy.AppendElement(c);
  return c;
}

void nsDisplayListSet::MoveTo(const nsDisplayListSet& aDestination) const
{
  aDestination.BorderBackground()->AppendToTop(BorderBackground());
  aDestination.BlockBorderBackgrounds()->AppendToTop(BlockBorderBackgrounds());
  aDestination.Floats()->AppendToTop(Floats());
  aDestination.Content()->AppendToTop(Content());
  aDestination.PositionedDescendants()->AppendToTop(PositionedDescendants());
  aDestination.Outlines()->AppendToTop(Outlines());
}

void
nsDisplayList::FlattenTo(nsTArray<nsDisplayItem*>* aElements) {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nullptr) {
    if (item->GetType() == nsDisplayItem::TYPE_WRAP_LIST) {
      item->GetSameCoordinateSystemChildren()->FlattenTo(aElements);
      item->~nsDisplayItem();
    } else {
      aElements->AppendElement(item);
    }
  }
}

nsRect
nsDisplayList::GetBounds(nsDisplayListBuilder* aBuilder) const {
  nsRect bounds;
  for (nsDisplayItem* i = GetBottom(); i != nullptr; i = i->GetAbove()) {
    bounds.UnionRect(bounds, i->GetClippedBounds(aBuilder));
  }
  return bounds;
}

bool
nsDisplayList::ComputeVisibilityForRoot(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion,
                                        nsIFrame* aDisplayPortFrame) {
  PROFILER_LABEL("nsDisplayList", "ComputeVisibilityForRoot",
    js::ProfileEntry::Category::GRAPHICS);

  nsRegion r;
  r.And(*aVisibleRegion, GetBounds(aBuilder));
  return ComputeVisibilityForSublist(aBuilder, aVisibleRegion,
                                     r.GetBounds(), r.GetBounds(),
                                     aDisplayPortFrame);
}

static nsRegion
TreatAsOpaque(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
{
  bool snap;
  nsRegion opaque = aItem->GetOpaqueRegion(aBuilder, &snap);
  if (aBuilder->IsForPluginGeometry()) {
    // Treat all leaf chrome items as opaque, unless their frames are opacity:0.
    // Since opacity:0 frames generate an nsDisplayOpacity, that item will
    // not be treated as opaque here, so opacity:0 chrome content will be
    // effectively ignored, as it should be.
    // We treat leaf chrome items as opaque to ensure that they cover
    // content plugins, for security reasons.
    // Non-leaf chrome items don't render contents of their own so shouldn't
    // be treated as opaque (and their bounds is just the union of their
    // children, which might be a large area their contents don't really cover).
    nsIFrame* f = aItem->Frame();
    if (f->PresContext()->IsChrome() && !aItem->GetChildren() &&
        f->StyleDisplay()->mOpacity != 0.0) {
      opaque = aItem->GetBounds(aBuilder, &snap);
    }
  }
  if (opaque.IsEmpty()) {
    return opaque;
  }
  nsRegion opaqueClipped;
  nsRegionRectIterator iter(opaque);
  for (const nsRect* r = iter.Next(); r; r = iter.Next()) {
    opaqueClipped.Or(opaqueClipped, aItem->GetClip().ApproximateIntersectInward(*r));
  }
  return opaqueClipped;
}

/* Checks if aPotentialScrollItem is a scroll layer item and aPotentialScrollbarItem
 * is an overlay scrollbar item for the same scroll frame.
 */
static bool
IsScrollLayerItemAndOverlayScrollbarForScrollFrame(
  nsDisplayItem* aPotentialScrollItem, nsDisplayItem* aPotentialScrollbarItem)
{
  if (aPotentialScrollItem->GetType() == nsDisplayItem::TYPE_SCROLL_LAYER &&
      aPotentialScrollbarItem &&
      aPotentialScrollbarItem->GetType() == nsDisplayItem::TYPE_OWN_LAYER &&
      LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars)) {
    nsDisplayScrollLayer* scrollItem =
      static_cast<nsDisplayScrollLayer*>(aPotentialScrollItem);
    nsDisplayOwnLayer* layerItem =
      static_cast<nsDisplayOwnLayer*>(aPotentialScrollbarItem);
    if ((layerItem->GetFlags() &
         (nsDisplayOwnLayer::VERTICAL_SCROLLBAR |
          nsDisplayOwnLayer::HORIZONTAL_SCROLLBAR)) &&
        layerItem->Frame()->GetParent() == scrollItem->GetScrollFrame()) {
      return true;
    }
  }
  return false;
}

bool
nsDisplayList::ComputeVisibilityForSublist(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aListVisibleBounds,
                                           const nsRect& aAllowVisibleRegionExpansion,
                                           nsIFrame* aDisplayPortFrame) {
#ifdef DEBUG
  nsRegion r;
  r.And(*aVisibleRegion, GetBounds(aBuilder));
  NS_ASSERTION(r.GetBounds().IsEqualInterior(aListVisibleBounds),
               "bad aListVisibleBounds");
#endif

  mVisibleRect = aListVisibleBounds;
  bool anyVisible = false;

  nsAutoTArray<nsDisplayItem*, 512> elements;
  FlattenTo(&elements);

  bool forceTransparentSurface = false;

  for (int32_t i = elements.Length() - 1; i >= 0; --i) {
    nsDisplayItem* item = elements[i];
    nsDisplayItem* belowItem = i < 1 ? nullptr : elements[i - 1];

    nsDisplayList* list = item->GetSameCoordinateSystemChildren();
    if (aBuilder->AllowMergingAndFlattening()) {
      if (belowItem && item->TryMerge(aBuilder, belowItem)) {
        belowItem->~nsDisplayItem();
        elements.ReplaceElementsAt(i - 1, 1, item);
        continue;
      }

      // If an overlay scrollbar item is between a scroll layer item and the
      // other scroll layer items that we need to merge with just move the
      // scrollbar item up, that way it will be on top of the scrolled content
      // and we can try to merge all the scroll layer items.
      if (IsScrollLayerItemAndOverlayScrollbarForScrollFrame(item, belowItem)) {
        elements[i] = belowItem;
        elements[i-1] = item;
        i++;
        continue;
      }

      if (list && item->ShouldFlattenAway(aBuilder)) {
        // The elements on the list >= i no longer serve any use.
        elements.SetLength(i);
        list->FlattenTo(&elements);
        i = elements.Length();
        item->~nsDisplayItem();
        continue;
      }
    }

    nsRect bounds = item->GetClippedBounds(aBuilder);

    nsRegion itemVisible;
    itemVisible.And(*aVisibleRegion, bounds);
    item->mVisibleRect = itemVisible.GetBounds();

    if (item->ComputeVisibility(aBuilder, aVisibleRegion,
                                aAllowVisibleRegionExpansion.Intersect(bounds))) {
      anyVisible = true;

      // If we're in a displayport, we need to make sure that fixed position
      // items do not subtract from the visible region, as async scrolling
      // may expose these occluded areas.
      // If the item is fixed pos in the same document as the displayport
      // then don't let it occlude this list. The only other case possible
      // is that the fixed pos content is in a child document, in which it
      // would scroll with the rest of the content.
      bool occlude = true;
      nsIPresShell* presShell = nullptr;
      if (aDisplayPortFrame && item->IsInFixedPos()) {
        if (item->Frame()->PresContext() == aDisplayPortFrame->PresContext()) {
          occlude = false;
          presShell = aDisplayPortFrame->PresContext()->PresShell();
        }
      }

      nsRegion opaque = TreatAsOpaque(item, aBuilder);
      if (occlude) {
        // Subtract opaque item from the visible region
        aBuilder->SubtractFromVisibleRegion(aVisibleRegion, opaque);
      } else if (presShell &&
                 presShell->IsScrollPositionClampingScrollPortSizeSet() &&
                 !opaque.IsEmpty()) {
        // We make an exception if the fixed position item would fully occlude
        // the scroll position clamping scroll-port. In that case, it's very
        // unlikely that it will become visible via async scrolling, so we let
        // it occlude.
        nsRect scrollClampingScrollPort(nsPoint(0, 0),
          presShell->GetScrollPositionClampingScrollPortSize());
        if (opaque.Contains(scrollClampingScrollPort)) {
          aVisibleRegion->SetEmpty();
        }
      }

      if (aBuilder->NeedToForceTransparentSurfaceForItem(item) ||
          (list && list->NeedsTransparentSurface())) {
        forceTransparentSurface = true;
      }
    }
    AppendToBottom(item);
  }

  mIsOpaque = !aVisibleRegion->Intersects(mVisibleRect);
  mForceTransparentSurface = forceTransparentSurface;
#if defined(DEBUG) || defined(MOZ_DUMP_PAINTING)
  mDidComputeVisibility = true;
#endif
  return anyVisible;
}

void nsDisplayList::PaintRoot(nsDisplayListBuilder* aBuilder,
                              nsRenderingContext* aCtx,
                              uint32_t aFlags) const {
  PROFILER_LABEL("nsDisplayList", "PaintRoot",
    js::ProfileEntry::Category::GRAPHICS);

  PaintForFrame(aBuilder, aCtx, aBuilder->RootReferenceFrame(), aFlags);
}

/**
 * We paint by executing a layer manager transaction, constructing a
 * single layer representing the display list, and then making it the
 * root of the layer manager, drawing into the ThebesLayers.
 */
void nsDisplayList::PaintForFrame(nsDisplayListBuilder* aBuilder,
                                  nsRenderingContext* aCtx,
                                  nsIFrame* aForFrame,
                                  uint32_t aFlags) const {
  NS_ASSERTION(mDidComputeVisibility,
               "Must call ComputeVisibility before calling Paint");

  nsRefPtr<LayerManager> layerManager;
  bool widgetTransaction = false;
  bool allowRetaining = false;
  bool doBeginTransaction = true;
  nsView *view = nullptr;
  if (aFlags & PAINT_USE_WIDGET_LAYERS) {
    nsIFrame* rootReferenceFrame = aBuilder->RootReferenceFrame();
    view = rootReferenceFrame->GetView();
    NS_ASSERTION(rootReferenceFrame == nsLayoutUtils::GetDisplayRootFrame(rootReferenceFrame),
                 "Reference frame must be a display root for us to use the layer manager");
    nsIWidget* window = rootReferenceFrame->GetNearestWidget();
    if (window) {
      layerManager = window->GetLayerManager(&allowRetaining);
      if (layerManager) {
        doBeginTransaction = !(aFlags & PAINT_EXISTING_TRANSACTION);
        widgetTransaction = true;
      }
    }
  }
  if (!layerManager) {
    if (!aCtx) {
      NS_WARNING("Nowhere to paint into");
      return;
    }
    layerManager = new BasicLayerManager(BasicLayerManager::BLM_OFFSCREEN);
  }

  // Store the existing layer builder to reinstate it on return.
  FrameLayerBuilder *oldBuilder = layerManager->GetLayerBuilder();

  FrameLayerBuilder *layerBuilder = new FrameLayerBuilder();
  layerBuilder->Init(aBuilder, layerManager);

  if (aFlags & PAINT_COMPRESSED) {
    layerBuilder->SetLayerTreeCompressionMode();
  }

  if (aFlags & PAINT_FLUSH_LAYERS) {
    FrameLayerBuilder::InvalidateAllLayers(layerManager);
  }

  if (doBeginTransaction) {
    if (aCtx) {
      layerManager->BeginTransactionWithTarget(aCtx->ThebesContext());
    } else {
      layerManager->BeginTransaction();
    }
  }
  if (widgetTransaction) {
    layerBuilder->DidBeginRetainedLayerTransaction(layerManager);
  }

  nsPresContext* presContext = aForFrame->PresContext();
  nsIPresShell* presShell = presContext->GetPresShell();

  NotifySubDocInvalidationFunc computeInvalidFunc =
    presContext->MayHavePaintEventListenerInSubDocument() ? nsPresContext::NotifySubDocInvalidation : 0;
  bool computeInvalidRect = (computeInvalidFunc ||
                             (!layerManager->IsCompositingCheap() && layerManager->NeedsWidgetInvalidation())) &&
                            widgetTransaction;

  nsAutoPtr<LayerProperties> props(computeInvalidRect ? 
                                     LayerProperties::CloneFrom(layerManager->GetRoot()) : 
                                     nullptr);

  ContainerLayerParameters containerParameters
    (presShell->GetXResolution(), presShell->GetYResolution());
  nsRefPtr<ContainerLayer> root = layerBuilder->
    BuildContainerLayerFor(aBuilder, layerManager, aForFrame, nullptr, *this,
                           containerParameters, nullptr);

  nsIDocument* document = nullptr;
  if (presShell) {
    document = presShell->GetDocument();
  }

  if (widgetTransaction ||
      // SVG-as-an-image docs don't paint as part of the retained layer tree,
      // but they still need the invalidation state bits cleared in order for
      // invalidation for CSS/SMIL animation to work properly.
      (document && document->IsBeingUsedAsImage())) {
    aForFrame->ClearInvalidationStateBits();
  }

  if (!root) {
    layerManager->SetUserData(&gLayerManagerLayerBuilder, oldBuilder);
    return;
  }
  // Root is being scaled up by the X/Y resolution. Scale it back down.
  root->SetPostScale(1.0f/containerParameters.mXScale,
                     1.0f/containerParameters.mYScale);

  bool isRoot = presContext->IsRootContentDocument();

  nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
  bool usingDisplayport = false;
  if (rootScrollFrame) {
    nsIContent* content = rootScrollFrame->GetContent();
    if (content) {
      usingDisplayport = nsLayoutUtils::GetDisplayPort(content, nullptr);
    }
  }

  nsRect viewport(aBuilder->ToReferenceFrame(aForFrame), aForFrame->GetSize());

  RecordFrameMetrics(aForFrame, rootScrollFrame,
                     aBuilder->FindReferenceFrameFor(aForFrame),
                     root, mVisibleRect, viewport,
                     !isRoot, isRoot, containerParameters);
  if (usingDisplayport &&
      !(root->GetContentFlags() & Layer::CONTENT_OPAQUE)) {
    // See bug 693938, attachment 567017
    NS_WARNING("Transparent content with displayports can be expensive.");
  }

  layerManager->SetRoot(root);
  layerBuilder->WillEndTransaction();
  bool temp = aBuilder->SetIsCompositingCheap(layerManager->IsCompositingCheap());
  LayerManager::EndTransactionFlags flags = LayerManager::END_DEFAULT;
  if (layerManager->NeedsWidgetInvalidation()) {
    if (aFlags & PAINT_NO_COMPOSITE) {
      flags = LayerManager::END_NO_COMPOSITE;
    }
  } else {
    // Client layer managers never composite directly, so
    // we don't need to worry about END_NO_COMPOSITE.
    if (aBuilder->WillComputePluginGeometry()) {
      flags = LayerManager::END_NO_REMOTE_COMPOSITE;
    }
  }

  MaybeSetupTransactionIdAllocator(layerManager, view);

  layerManager->EndTransaction(FrameLayerBuilder::DrawThebesLayer,
                               aBuilder, flags);
  aBuilder->SetIsCompositingCheap(temp);
  layerBuilder->DidEndTransaction();

  nsIntRegion invalid;
  if (props) {
    invalid = props->ComputeDifferences(root, computeInvalidFunc);
  } else if (widgetTransaction) {
    LayerProperties::ClearInvalidations(root);
  }

  bool shouldInvalidate = layerManager->NeedsWidgetInvalidation();
  if (view) {
    if (props) {
      if (!invalid.IsEmpty()) {
        nsIntRect bounds = invalid.GetBounds();
        nsRect rect(presContext->DevPixelsToAppUnits(bounds.x),
                    presContext->DevPixelsToAppUnits(bounds.y),
                    presContext->DevPixelsToAppUnits(bounds.width),
                    presContext->DevPixelsToAppUnits(bounds.height));
        if (shouldInvalidate) {
          view->GetViewManager()->InvalidateViewNoSuppression(view, rect);
        }
        presContext->NotifyInvalidation(bounds, 0);
      }
    } else if (shouldInvalidate) {
      view->GetViewManager()->InvalidateView(view);
    }
  }

  if (aFlags & PAINT_FLUSH_LAYERS) {
    FrameLayerBuilder::InvalidateAllLayers(layerManager);
  }

  layerManager->SetUserData(&gLayerManagerLayerBuilder, oldBuilder);
}

uint32_t nsDisplayList::Count() const {
  uint32_t count = 0;
  for (nsDisplayItem* i = GetBottom(); i; i = i->GetAbove()) {
    ++count;
  }
  return count;
}

nsDisplayItem* nsDisplayList::RemoveBottom() {
  nsDisplayItem* item = mSentinel.mAbove;
  if (!item)
    return nullptr;
  mSentinel.mAbove = item->mAbove;
  if (item == mTop) {
    // must have been the only item
    mTop = &mSentinel;
  }
  item->mAbove = nullptr;
  return item;
}

void nsDisplayList::DeleteAll() {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nullptr) {
    item->~nsDisplayItem();
  }
}

static bool
GetMouseThrough(const nsIFrame* aFrame)
{
  if (!aFrame->IsBoxFrame())
    return false;

  const nsIFrame* frame = aFrame;
  while (frame) {
    if (frame->GetStateBits() & NS_FRAME_MOUSE_THROUGH_ALWAYS) {
      return true;
    } else if (frame->GetStateBits() & NS_FRAME_MOUSE_THROUGH_NEVER) {
      return false;
    }
    frame = nsBox::GetParentBox(frame);
  }
  return false;
}

static bool
IsFrameReceivingPointerEvents(nsIFrame* aFrame)
{
  nsSubDocumentFrame* frame = do_QueryFrame(aFrame);
  if (frame && frame->PassPointerEventsToChildren()) {
    return true;
  }
  return NS_STYLE_POINTER_EVENTS_NONE !=
    aFrame->StyleVisibility()->GetEffectivePointerEvents(aFrame);
}

// A list of frames, and their z depth. Used for sorting
// the results of hit testing.
struct FramesWithDepth
{
  FramesWithDepth(float aDepth) :
    mDepth(aDepth)
  {}

  bool operator<(const FramesWithDepth& aOther) const {
    if (mDepth != aOther.mDepth) {
      // We want to sort so that the shallowest item (highest depth value) is first
      return mDepth > aOther.mDepth;
    }
    return this < &aOther;
  }
  bool operator==(const FramesWithDepth& aOther) const {
    return this == &aOther;
  }

  float mDepth;
  nsTArray<nsIFrame*> mFrames;
};

// Sort the frames by depth and then moves all the contained frames to the destination
void FlushFramesArray(nsTArray<FramesWithDepth>& aSource, nsTArray<nsIFrame*>* aDest)
{
  if (aSource.IsEmpty()) {
    return;
  }
  aSource.Sort();
  uint32_t length = aSource.Length();
  for (uint32_t i = 0; i < length; i++) {
    aDest->MoveElementsFrom(aSource[i].mFrames);
  }
  aSource.Clear();
}

void nsDisplayList::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                            nsDisplayItem::HitTestState* aState,
                            nsTArray<nsIFrame*> *aOutFrames) const {
  int32_t itemBufferStart = aState->mItemBuffer.Length();
  nsDisplayItem* item;
  for (item = GetBottom(); item; item = item->GetAbove()) {
    aState->mItemBuffer.AppendElement(item);
  }
  nsAutoTArray<FramesWithDepth, 16> temp;
  for (int32_t i = aState->mItemBuffer.Length() - 1; i >= itemBufferStart; --i) {
    // Pop element off the end of the buffer. We want to shorten the buffer
    // so that recursive calls to HitTest have more buffer space.
    item = aState->mItemBuffer[i];
    aState->mItemBuffer.SetLength(i);

    bool snap;
    nsRect r = item->GetBounds(aBuilder, &snap).Intersect(aRect);
    if (item->GetClip().MayIntersect(r)) {
      nsAutoTArray<nsIFrame*, 16> outFrames;
      item->HitTest(aBuilder, aRect, aState, &outFrames);

      // For 3d transforms with preserve-3d we add hit frames into the temp list
      // so we can sort them later, otherwise we add them directly to the output list.
      nsTArray<nsIFrame*> *writeFrames = aOutFrames;
      if (item->GetType() == nsDisplayItem::TYPE_TRANSFORM &&
          item->Frame()->Preserves3D()) {
        if (outFrames.Length()) {
          nsDisplayTransform *transform = static_cast<nsDisplayTransform*>(item);
          nsPoint point = aRect.TopLeft();
          // A 1x1 rect means a point, otherwise use the center of the rect
          if (aRect.width != 1 || aRect.height != 1) {
            point = aRect.Center();
          }
          temp.AppendElement(FramesWithDepth(transform->GetHitDepthAtPoint(aBuilder, point)));
          writeFrames = &temp[temp.Length() - 1].mFrames;
        }
      } else {
        // We may have just finished a run of consecutive preserve-3d transforms,
        // so flush these into the destination array before processing our frame list.
        FlushFramesArray(temp, aOutFrames);
      }

      for (uint32_t j = 0; j < outFrames.Length(); j++) {
        nsIFrame *f = outFrames.ElementAt(j);
        // Handle the XUL 'mousethrough' feature and 'pointer-events'.
        if (!GetMouseThrough(f) && IsFrameReceivingPointerEvents(f)) {
          writeFrames->AppendElement(f);
        }
      }
    }
  }
  // Clear any remaining preserve-3d transforms.
  FlushFramesArray(temp, aOutFrames);
  NS_ASSERTION(aState->mItemBuffer.Length() == uint32_t(itemBufferStart),
               "How did we forget to pop some elements?");
}

static void Sort(nsDisplayList* aList, int32_t aCount, nsDisplayList::SortLEQ aCmp,
                 void* aClosure) {
  if (aCount < 2)
    return;

  nsDisplayList list1;
  nsDisplayList list2;
  int i;
  int32_t half = aCount/2;
  bool sorted = true;
  nsDisplayItem* prev = nullptr;
  for (i = 0; i < aCount; ++i) {
    nsDisplayItem* item = aList->RemoveBottom();
    (i < half ? &list1 : &list2)->AppendToTop(item);
    if (sorted && prev && !aCmp(prev, item, aClosure)) {
      sorted = false;
    }
    prev = item;
  }
  if (sorted) {
    aList->AppendToTop(&list1);
    aList->AppendToTop(&list2);
    return;
  }

  Sort(&list1, half, aCmp, aClosure);
  Sort(&list2, aCount - half, aCmp, aClosure);

  for (i = 0; i < aCount; ++i) {
    if (list1.GetBottom() &&
        (!list2.GetBottom() ||
         aCmp(list1.GetBottom(), list2.GetBottom(), aClosure))) {
      aList->AppendToTop(list1.RemoveBottom());
    } else {
      aList->AppendToTop(list2.RemoveBottom());
    }
  }
}

static nsIContent* FindContentInDocument(nsDisplayItem* aItem, nsIDocument* aDoc) {
  nsIFrame* f = aItem->Frame();
  while (f) {
    nsPresContext* pc = f->PresContext();
    if (pc->Document() == aDoc) {
      return f->GetContent();
    }
    f = nsLayoutUtils::GetCrossDocParentFrame(pc->PresShell()->GetRootFrame());
  }
  return nullptr;
}

static bool IsContentLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                         void* aClosure) {
  nsIContent* commonAncestor = static_cast<nsIContent*>(aClosure);
  // It's possible that the nsIContent for aItem1 or aItem2 is in a subdocument
  // of commonAncestor, because display items for subdocuments have been
  // mixed into the same list. Ensure that we're looking at content
  // in commonAncestor's document.
  nsIDocument* commonAncestorDoc = commonAncestor->OwnerDoc();
  nsIContent* content1 = FindContentInDocument(aItem1, commonAncestorDoc);
  nsIContent* content2 = FindContentInDocument(aItem2, commonAncestorDoc);
  if (!content1 || !content2) {
    NS_ERROR("Document trees are mixed up!");
    // Something weird going on
    return true;
  }
  return nsLayoutUtils::CompareTreePosition(content1, content2, commonAncestor) <= 0;
}

static bool IsZOrderLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                        void* aClosure) {
  // Note that we can't just take the difference of the two
  // z-indices here, because that might overflow a 32-bit int.
  return aItem1->ZIndex() <= aItem2->ZIndex();
}

void nsDisplayList::SortByZOrder(nsDisplayListBuilder* aBuilder,
                                 nsIContent* aCommonAncestor) {
  Sort(aBuilder, IsZOrderLEQ, aCommonAncestor);
}

void nsDisplayList::SortByContentOrder(nsDisplayListBuilder* aBuilder,
                                       nsIContent* aCommonAncestor) {
  Sort(aBuilder, IsContentLEQ, aCommonAncestor);
}

void nsDisplayList::Sort(nsDisplayListBuilder* aBuilder,
                         SortLEQ aCmp, void* aClosure) {
  ::Sort(this, Count(), aCmp, aClosure);
}

void
nsDisplayItem::AddInvalidRegionForSyncDecodeBackgroundImages(
  nsDisplayListBuilder* aBuilder,
  const nsDisplayItemGeometry* aGeometry,
  nsRegion* aInvalidRegion)
{
  if (aBuilder->ShouldSyncDecodeImages()) {
    if (!nsCSSRendering::AreAllBackgroundImagesDecodedForFrame(mFrame)) {
      bool snap;
      aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
    }
  }
}

/* static */ bool
nsDisplayItem::ForceActiveLayers()
{
  static bool sForce = false;
  static bool sForceCached = false;

  if (!sForceCached) {
    Preferences::AddBoolVarCache(&sForce, "layers.force-active", false);
    sForceCached = true;
  }

  return sForce;
}

/* static */ int32_t
nsDisplayItem::MaxActiveLayers()
{
  static int32_t sMaxLayers = false;
  static bool sMaxLayersCached = false;

  if (!sMaxLayersCached) {
    Preferences::AddIntVarCache(&sMaxLayers, "layers.max-active", -1);
    sMaxLayersCached = true;
  }

  return sMaxLayers;
}

int32_t
nsDisplayItem::ZIndex() const
{
  if (!mFrame->IsPositioned() && !mFrame->IsFlexOrGridItem())
    return 0;

  const nsStylePosition* position = mFrame->StylePosition();
  if (position->mZIndex.GetUnit() == eStyleUnit_Integer)
    return position->mZIndex.GetIntValue();

  // sort the auto and 0 elements together
  return 0;
}

bool
nsDisplayItem::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion,
                                 const nsRect& aAllowVisibleRegionExpansion)
{
  return !mVisibleRect.IsEmpty() &&
    !IsInvisibleInRect(aVisibleRegion->GetBounds());
}

bool
nsDisplayItem::RecomputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion) {
  nsRect bounds = GetClippedBounds(aBuilder);

  nsRegion itemVisible;
  itemVisible.And(*aVisibleRegion, bounds);
  mVisibleRect = itemVisible.GetBounds();

  // When we recompute visibility within layers we don't need to
  // expand the visible region for content behind plugins (the plugin
  // is not in the layer).
  if (!ComputeVisibility(aBuilder, aVisibleRegion, nsRect()))
    return false;

  nsRegion opaque = TreatAsOpaque(this, aBuilder);
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, opaque);
  return true;
}

nsRect
nsDisplayItem::GetClippedBounds(nsDisplayListBuilder* aBuilder)
{
  bool snap;
  nsRect r = GetBounds(aBuilder, &snap);
  return GetClip().ApplyNonRoundedIntersection(r);
}

nsRect
nsDisplaySolidColor::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  *aSnap = true;
  return mBounds;
}

void
nsDisplaySolidColor::Paint(nsDisplayListBuilder* aBuilder,
                           nsRenderingContext* aCtx)
{
  aCtx->SetColor(mColor);
  aCtx->FillRect(mVisibleRect);
}

#ifdef MOZ_DUMP_PAINTING
void
nsDisplaySolidColor::WriteDebugInfo(nsACString& aTo)
{
  aTo += nsPrintfCString(" (rgba %d,%d,%d,%d)",
                 NS_GET_R(mColor), NS_GET_G(mColor),
                 NS_GET_B(mColor), NS_GET_A(mColor));
}
#endif

static void
RegisterThemeGeometry(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
{
  nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(aFrame);

  for (nsIFrame* f = aFrame; f; f = f->GetParent()) {
    // Bail out if we're in a transformed subtree
    if (f->IsTransformed())
      return;
    // Bail out if we're not in the displayRoot's document
    if (!f->GetParent() && f != displayRoot)
      return;
  }

  nsRect borderBox(aFrame->GetOffsetTo(displayRoot), aFrame->GetSize());
  aBuilder->RegisterThemeGeometry(aFrame->StyleDisplay()->mAppearance,
      borderBox.ToNearestPixels(aFrame->PresContext()->AppUnitsPerDevPixel()));
}

nsDisplayBackgroundImage::nsDisplayBackgroundImage(nsDisplayListBuilder* aBuilder,
                                                   nsIFrame* aFrame,
                                                   uint32_t aLayer,
                                                   const nsStyleBackground* aBackgroundStyle)
  : nsDisplayImageContainer(aBuilder, aFrame)
  , mBackgroundStyle(aBackgroundStyle)
  , mLayer(aLayer)
{
  MOZ_COUNT_CTOR(nsDisplayBackgroundImage);

  mBounds = GetBoundsInternal(aBuilder);
}

nsDisplayBackgroundImage::~nsDisplayBackgroundImage()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_DTOR(nsDisplayBackgroundImage);
#endif
}

static nsStyleContext* GetBackgroundStyleContext(nsIFrame* aFrame)
{
  nsStyleContext *sc;
  if (!nsCSSRendering::FindBackground(aFrame, &sc)) {
    // We don't want to bail out if moz-appearance is set on a root
    // node. If it has a parent content node, bail because it's not
    // a root, other wise keep going in order to let the theme stuff
    // draw the background. The canvas really should be drawing the
    // bg, but there's no way to hook that up via css.
    if (!aFrame->StyleDisplay()->mAppearance) {
      return nullptr;
    }

    nsIContent* content = aFrame->GetContent();
    if (!content || content->GetParent()) {
      return nullptr;
    }

    sc = aFrame->StyleContext();
  }
  return sc;
}

/*static*/ bool
nsDisplayBackgroundImage::AppendBackgroundItemsToTop(nsDisplayListBuilder* aBuilder,
                                                     nsIFrame* aFrame,
                                                     nsDisplayList* aList)
{
  nsStyleContext* bgSC = nullptr;
  const nsStyleBackground* bg = nullptr;
  nsPresContext* presContext = aFrame->PresContext();
  bool isThemed = aFrame->IsThemed();
  if (!isThemed) {
    bgSC = GetBackgroundStyleContext(aFrame);
    if (bgSC) {
      bg = bgSC->StyleBackground();
    }
  }

  bool drawBackgroundColor = false;
  nscolor color;
  if (!nsCSSRendering::IsCanvasFrame(aFrame) && bg) {
    bool drawBackgroundImage;
    color =
      nsCSSRendering::DetermineBackgroundColor(presContext, bgSC, aFrame,
                                               drawBackgroundImage, drawBackgroundColor);
  }

  // An auxiliary list is necessary in case we have background blending; if that
  // is the case, background items need to be wrapped by a blend container to
  // isolate blending to the background
  nsDisplayList bgItemList;
  // Even if we don't actually have a background color to paint, we may still need
  // to create an item for hit testing.
  if ((drawBackgroundColor && color != NS_RGBA(0,0,0,0)) ||
      aBuilder->IsForEventDelivery()) {
    bgItemList.AppendNewToTop(
        new (aBuilder) nsDisplayBackgroundColor(aBuilder, aFrame, bg,
                                                drawBackgroundColor ? color : NS_RGBA(0, 0, 0, 0)));
  }

  if (isThemed) {
    nsDisplayThemedBackground* bgItem =
      new (aBuilder) nsDisplayThemedBackground(aBuilder, aFrame);
    bgItemList.AppendNewToTop(bgItem);
    aList->AppendToTop(&bgItemList);
    return true;
  }

  if (!bg) {
    aList->AppendToTop(&bgItemList);
    return false;
  }
 
  bool needBlendContainer = false;

  // Passing bg == nullptr in this macro will result in one iteration with
  // i = 0.
  NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
    if (bg->mLayers[i].mImage.IsEmpty()) {
      continue;
    }

    if (bg->mLayers[i].mBlendMode != NS_STYLE_BLEND_NORMAL) {
      needBlendContainer = true;
    }

    nsDisplayBackgroundImage* bgItem =
      new (aBuilder) nsDisplayBackgroundImage(aBuilder, aFrame, i, bg);
    bgItemList.AppendNewToTop(bgItem);
  }

  if (needBlendContainer) {
    bgItemList.AppendNewToTop(
      new (aBuilder) nsDisplayBlendContainer(aBuilder, aFrame, &bgItemList));
  }

  aList->AppendToTop(&bgItemList);
  return false;
}

// Check that the rounded border of aFrame, added to aToReferenceFrame,
// intersects aRect.  Assumes that the unrounded border has already
// been checked for intersection.
static bool
RoundedBorderIntersectsRect(nsIFrame* aFrame,
                            const nsPoint& aFrameToReferenceFrame,
                            const nsRect& aTestRect)
{
  if (!nsRect(aFrameToReferenceFrame, aFrame->GetSize()).Intersects(aTestRect))
    return false;

  nscoord radii[8];
  return !aFrame->GetBorderRadii(radii) ||
         nsLayoutUtils::RoundedRectIntersectsRect(nsRect(aFrameToReferenceFrame,
                                                  aFrame->GetSize()),
                                                  radii, aTestRect);
}

// Returns TRUE if aContainedRect is guaranteed to be contained in
// the rounded rect defined by aRoundedRect and aRadii. Complex cases are
// handled conservatively by returning FALSE in some situations where
// a more thorough analysis could return TRUE.
//
// See also RoundedRectIntersectsRect.
static bool RoundedRectContainsRect(const nsRect& aRoundedRect,
                                    const nscoord aRadii[8],
                                    const nsRect& aContainedRect) {
  nsRegion rgn = nsLayoutUtils::RoundedRectIntersectRect(aRoundedRect, aRadii, aContainedRect);
  return rgn.Contains(aContainedRect);
}

bool
nsDisplayBackgroundImage::IsSingleFixedPositionImage(nsDisplayListBuilder* aBuilder,
                                                     const nsRect& aClipRect,
                                                     gfxRect* aDestRect)
{
  if (!mBackgroundStyle)
    return false;

  if (mBackgroundStyle->mLayers.Length() != 1)
    return false;

  nsPresContext* presContext = mFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  const nsStyleBackground::Layer &layer = mBackgroundStyle->mLayers[mLayer];

  if (layer.mAttachment != NS_STYLE_BG_ATTACHMENT_FIXED)
    return false;

  nsBackgroundLayerState state =
    nsCSSRendering::PrepareBackgroundLayer(presContext, mFrame, flags,
                                           borderArea, aClipRect, layer);
  nsImageRenderer* imageRenderer = &state.mImageRenderer;
  // We only care about images here, not gradients.
  if (!imageRenderer->IsRasterImage())
    return false;

  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  *aDestRect = nsLayoutUtils::RectToGfxRect(state.mFillArea, appUnitsPerDevPixel);

  return true;
}

bool
nsDisplayBackgroundImage::TryOptimizeToImageLayer(LayerManager* aManager,
                                                  nsDisplayListBuilder* aBuilder)
{
  if (!mBackgroundStyle)
    return false;

  nsPresContext* presContext = mFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  const nsStyleBackground::Layer &layer = mBackgroundStyle->mLayers[mLayer];

  if (layer.mClip != NS_STYLE_BG_CLIP_BORDER) {
    return false;
  }
  nscoord radii[8];
  if (mFrame->GetBorderRadii(radii)) {
    return false;
  }

  nsBackgroundLayerState state =
    nsCSSRendering::PrepareBackgroundLayer(presContext, mFrame, flags,
                                           borderArea, borderArea, layer);
  nsImageRenderer* imageRenderer = &state.mImageRenderer;
  // We only care about images here, not gradients.
  if (!imageRenderer->IsRasterImage())
    return false;

  nsRefPtr<ImageContainer> imageContainer = imageRenderer->GetContainer(aManager);
  // Image is not ready to be made into a layer yet
  if (!imageContainer)
    return false;

  // We currently can't handle tiled or partial backgrounds.
  if (!state.mDestArea.IsEqualEdges(state.mFillArea)) {
    return false;
  }

  // XXX Ignoring state.mAnchor. ImageLayer drawing snaps mDestArea edges to
  // layer pixel boundaries. This should be OK for now.

  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  mDestRect = nsLayoutUtils::RectToGfxRect(state.mDestArea, appUnitsPerDevPixel);
  mImageContainer = imageContainer;

  // Ok, we can turn this into a layer if needed.
  return true;
}

already_AddRefed<ImageContainer>
nsDisplayBackgroundImage::GetContainer(LayerManager* aManager,
                                       nsDisplayListBuilder *aBuilder)
{
  if (!TryOptimizeToImageLayer(aManager, aBuilder)) {
    return nullptr;
  }

  nsRefPtr<ImageContainer> container = mImageContainer;

  return container.forget();
}

LayerState
nsDisplayBackgroundImage::GetLayerState(nsDisplayListBuilder* aBuilder,
                                        LayerManager* aManager,
                                        const ContainerLayerParameters& aParameters)
{
  bool animated = false;
  if (mBackgroundStyle) {
    const nsStyleBackground::Layer &layer = mBackgroundStyle->mLayers[mLayer];
    const nsStyleImage* image = &layer.mImage;
    if (image->GetType() == eStyleImageType_Image) {
      imgIRequest* imgreq = image->GetImageData();
      nsCOMPtr<imgIContainer> image;
      if (NS_SUCCEEDED(imgreq->GetImage(getter_AddRefs(image))) && image) {
        if (NS_FAILED(image->GetAnimated(&animated))) {
          animated = false;
        }
      }
    }
  }

  if (!animated ||
      !nsLayoutUtils::AnimatedImageLayersEnabled()) {
    if (!aManager->IsCompositingCheap() ||
        !nsLayoutUtils::GPUImageScalingEnabled()) {
      return LAYER_NONE;
    }
  }

  if (!TryOptimizeToImageLayer(aManager, aBuilder)) {
    return LAYER_NONE;
  }

  if (!animated) {
    mozilla::gfx::IntSize imageSize = mImageContainer->GetCurrentSize();
    NS_ASSERTION(imageSize.width != 0 && imageSize.height != 0, "Invalid image size!");

    gfxRect destRect = mDestRect;

    destRect.width *= aParameters.mXScale;
    destRect.height *= aParameters.mYScale;

    // Calculate the scaling factor for the frame.
    gfxSize scale = gfxSize(destRect.width / imageSize.width, destRect.height / imageSize.height);

    // If we are not scaling at all, no point in separating this into a layer.
    if (scale.width == 1.0f && scale.height == 1.0f) {
      return LAYER_NONE;
    }

    // If the target size is pretty small, no point in using a layer.
    if (destRect.width * destRect.height < 64 * 64) {
      return LAYER_NONE;
    }
  }

  return LAYER_ACTIVE;
}

already_AddRefed<Layer>
nsDisplayBackgroundImage::BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aParameters)
{
  nsRefPtr<ImageLayer> layer = static_cast<ImageLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer)
      return nullptr;
  }
  layer->SetContainer(mImageContainer);
  ConfigureLayer(layer, aParameters.mOffset);
  return layer.forget();
}

void
nsDisplayBackgroundImage::ConfigureLayer(ImageLayer* aLayer, const nsIntPoint& aOffset)
{
  aLayer->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(mFrame));

  mozilla::gfx::IntSize imageSize = mImageContainer->GetCurrentSize();
  NS_ASSERTION(imageSize.width != 0 && imageSize.height != 0, "Invalid image size!");

  gfxPoint p = mDestRect.TopLeft() + aOffset;
  gfx::Matrix transform;
  transform.Translate(p.x, p.y);
  transform.Scale(mDestRect.width/imageSize.width,
                  mDestRect.height/imageSize.height);
  aLayer->SetBaseTransform(gfx::Matrix4x4::From2D(transform));
  aLayer->SetVisibleRegion(nsIntRect(0, 0, imageSize.width, imageSize.height));
}

void
nsDisplayBackgroundImage::HitTest(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aRect,
                                  HitTestState* aState,
                                  nsTArray<nsIFrame*> *aOutFrames)
{
  if (RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

bool
nsDisplayBackgroundImage::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                            nsRegion* aVisibleRegion,
                                            const nsRect& aAllowVisibleRegionExpansion)
{
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion)) {
    return false;
  }

  // Return false if the background was propagated away from this
  // frame. We don't want this display item to show up and confuse
  // anything.
  return mBackgroundStyle;
}

/* static */ nsRegion
nsDisplayBackgroundImage::GetInsideClipRegion(nsDisplayItem* aItem,
                                              nsPresContext* aPresContext,
                                              uint8_t aClip, const nsRect& aRect,
                                              bool* aSnap)
{
  nsRegion result;
  if (aRect.IsEmpty())
    return result;

  nsIFrame *frame = aItem->Frame();

  nscoord radii[8];
  nsRect clipRect;
  bool haveRadii;
  if (frame->GetType() == nsGkAtoms::canvasFrame) {
    nsCanvasFrame* canvasFrame = static_cast<nsCanvasFrame*>(frame);
    haveRadii = false;
    clipRect = canvasFrame->CanvasArea() + aItem->ToReferenceFrame();
  } else {
    switch (aClip) {
    case NS_STYLE_BG_CLIP_BORDER:
      haveRadii = frame->GetBorderRadii(radii);
      clipRect = nsRect(aItem->ToReferenceFrame(), frame->GetSize());
      break;
    case NS_STYLE_BG_CLIP_PADDING:
      haveRadii = frame->GetPaddingBoxBorderRadii(radii);
      clipRect = frame->GetPaddingRect() - frame->GetPosition() + aItem->ToReferenceFrame();
      break;
    case NS_STYLE_BG_CLIP_CONTENT:
      haveRadii = frame->GetContentBoxBorderRadii(radii);
      clipRect = frame->GetContentRect() - frame->GetPosition() + aItem->ToReferenceFrame();
      break;
    default:
      NS_NOTREACHED("Unknown clip type");
      return result;
    }
  }

  if (haveRadii) {
    *aSnap = false;
    result = nsLayoutUtils::RoundedRectIntersectRect(clipRect, radii, aRect);
  } else {
    result = clipRect.Intersect(aRect);
  }
  return result;
}

nsRegion
nsDisplayBackgroundImage::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap) {
  nsRegion result;
  *aSnap = false;

  if (!mBackgroundStyle)
    return result;


  *aSnap = true;

  // For NS_STYLE_BOX_DECORATION_BREAK_SLICE, don't try to optimize here, since
  // this could easily lead to O(N^2) behavior inside InlineBackgroundData,
  // which expects frames to be sent to it in content order, not reverse
  // content order which we'll produce here.
  // Of course, if there's only one frame in the flow, it doesn't matter.
  if (mFrame->StyleBorder()->mBoxDecorationBreak ==
        NS_STYLE_BOX_DECORATION_BREAK_CLONE ||
      (!mFrame->GetPrevContinuation() && !mFrame->GetNextContinuation())) {
    const nsStyleBackground::Layer& layer = mBackgroundStyle->mLayers[mLayer];
    if (layer.mImage.IsOpaque() && layer.mBlendMode == NS_STYLE_BLEND_NORMAL) {
      nsPresContext* presContext = mFrame->PresContext();
      result = GetInsideClipRegion(this, presContext, layer.mClip, mBounds, aSnap);
    }
  }

  return result;
}

bool
nsDisplayBackgroundImage::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) {
  if (!mBackgroundStyle) {
    *aColor = NS_RGBA(0,0,0,0);
    return true;
  }
  return false;
}

bool
nsDisplayBackgroundImage::IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                         nsIFrame* aFrame)
{
  if (!mBackgroundStyle)
    return false;
  if (!mBackgroundStyle->HasFixedBackground())
    return false;

  // If aFrame is mFrame or an ancestor in this document, and aFrame is
  // not the viewport frame, then moving aFrame will move mFrame
  // relative to the viewport, so our fixed-pos background will change.
  return aFrame->GetParent() &&
    (aFrame == mFrame ||
     nsLayoutUtils::IsProperAncestorFrame(aFrame, mFrame));
}

nsRect
nsDisplayBackgroundImage::GetPositioningArea()
{
  if (!mBackgroundStyle) {
    return nsRect();
  }
  nsIFrame* attachedToFrame;
  return nsCSSRendering::ComputeBackgroundPositioningArea(
      mFrame->PresContext(), mFrame,
      nsRect(ToReferenceFrame(), mFrame->GetSize()),
      mBackgroundStyle->mLayers[mLayer],
      &attachedToFrame) + ToReferenceFrame();
}

bool
nsDisplayBackgroundImage::RenderingMightDependOnPositioningAreaSizeChange()
{
  if (!mBackgroundStyle)
    return false;

  nscoord radii[8];
  if (mFrame->GetBorderRadii(radii)) {
    // A change in the size of the positioning area might change the position
    // of the rounded corners.
    return true;
  }

  const nsStyleBackground::Layer &layer = mBackgroundStyle->mLayers[mLayer];
  if (layer.RenderingMightDependOnPositioningAreaSizeChange()) {
    return true;
  }
  return false;
}

static void CheckForBorderItem(nsDisplayItem *aItem, uint32_t& aFlags)
{
  nsDisplayItem* nextItem = aItem->GetAbove();
  while (nextItem && nextItem->GetType() == nsDisplayItem::TYPE_BACKGROUND) {
    nextItem = nextItem->GetAbove();
  }
  if (nextItem && 
      nextItem->Frame() == aItem->Frame() &&
      nextItem->GetType() == nsDisplayItem::TYPE_BORDER) {
    aFlags |= nsCSSRendering::PAINTBG_WILL_PAINT_BORDER;
  }
}

void
nsDisplayBackgroundImage::Paint(nsDisplayListBuilder* aBuilder,
                                nsRenderingContext* aCtx) {
  PaintInternal(aBuilder, aCtx, mVisibleRect, nullptr);
}

void
nsDisplayBackgroundImage::PaintInternal(nsDisplayListBuilder* aBuilder,
                                        nsRenderingContext* aCtx, const nsRect& aBounds,
                                        nsRect* aClipRect) {
  nsPoint offset = ToReferenceFrame();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  CheckForBorderItem(this, flags);

  nsCSSRendering::PaintBackground(mFrame->PresContext(), *aCtx, mFrame,
                                  aBounds,
                                  nsRect(offset, mFrame->GetSize()),
                                  flags, aClipRect, mLayer);

}

void nsDisplayBackgroundImage::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                         const nsDisplayItemGeometry* aGeometry,
                                                         nsRegion* aInvalidRegion)
{
  if (!mBackgroundStyle) {
    return;
  }

  const nsDisplayBackgroundGeometry* geometry = static_cast<const nsDisplayBackgroundGeometry*>(aGeometry);

  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  nsRect positioningArea = GetPositioningArea();
  if (positioningArea.TopLeft() != geometry->mPositioningArea.TopLeft() ||
      (positioningArea.Size() != geometry->mPositioningArea.Size() &&
       RenderingMightDependOnPositioningAreaSizeChange())) {
    // Positioning area changed in a way that could cause everything to change,
    // so invalidate everything (both old and new painting areas).
    aInvalidRegion->Or(bounds, geometry->mBounds);

    if (positioningArea.Size() != geometry->mPositioningArea.Size()) {
      NotifyRenderingChanged();
    }
    return;
  }
  if (aBuilder->ShouldSyncDecodeImages()) {
    if (mBackgroundStyle &&
        !nsCSSRendering::IsBackgroundImageDecodedForStyleContextAndLayer(mBackgroundStyle, mLayer)) {
      aInvalidRegion->Or(*aInvalidRegion, bounds);

      NotifyRenderingChanged();
    }
  }
  if (!bounds.IsEqualInterior(geometry->mBounds)) {
    // Positioning area is unchanged, so invalidate just the change in the
    // painting area.
    aInvalidRegion->Xor(bounds, geometry->mBounds);

    NotifyRenderingChanged();
  }
}

nsRect
nsDisplayBackgroundImage::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = true;
  return mBounds;
}

nsRect
nsDisplayBackgroundImage::GetBoundsInternal(nsDisplayListBuilder* aBuilder) {
  nsPresContext* presContext = mFrame->PresContext();

  if (!mBackgroundStyle) {
    return nsRect();
  }

  nsRect borderBox = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsRect clipRect = borderBox;
  if (mFrame->GetType() == nsGkAtoms::canvasFrame) {
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    clipRect = frame->CanvasArea() + ToReferenceFrame();
  }
  const nsStyleBackground::Layer& layer = mBackgroundStyle->mLayers[mLayer];
  return nsCSSRendering::GetBackgroundLayerRect(presContext, mFrame,
                                                borderBox, clipRect, layer,
                                                aBuilder->GetBackgroundPaintFlags());
}

uint32_t
nsDisplayBackgroundImage::GetPerFrameKey()
{
  return (mLayer << nsDisplayItem::TYPE_BITS) |
    nsDisplayItem::GetPerFrameKey();
}

nsDisplayThemedBackground::nsDisplayThemedBackground(nsDisplayListBuilder* aBuilder,
                                                     nsIFrame* aFrame)
  : nsDisplayItem(aBuilder, aFrame)
{
  MOZ_COUNT_CTOR(nsDisplayThemedBackground);

  const nsStyleDisplay* disp = mFrame->StyleDisplay();
  mAppearance = disp->mAppearance;
  mFrame->IsThemed(disp, &mThemeTransparency);

  // Perform necessary RegisterThemeGeometry
  switch (disp->mAppearance) {
    case NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR:
    case NS_THEME_TOOLBAR:
    case NS_THEME_WINDOW_TITLEBAR:
    case NS_THEME_WINDOW_BUTTON_BOX:
    case NS_THEME_MOZ_MAC_FULLSCREEN_BUTTON:
    case NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED:
      RegisterThemeGeometry(aBuilder, aFrame);
      break;
    case NS_THEME_WIN_BORDERLESS_GLASS:
    case NS_THEME_WIN_GLASS:
      aBuilder->SetGlassDisplayItem(this);
      break;
  }

  mBounds = GetBoundsInternal();
}

nsDisplayThemedBackground::~nsDisplayThemedBackground()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_DTOR(nsDisplayThemedBackground);
#endif
}

#ifdef MOZ_DUMP_PAINTING
void
nsDisplayThemedBackground::WriteDebugInfo(nsACString& aTo)
{
  aTo += nsPrintfCString(" (themed, appearance:%d)", mAppearance);
}
#endif

void
nsDisplayThemedBackground::HitTest(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aRect,
                                  HitTestState* aState,
                                  nsTArray<nsIFrame*> *aOutFrames)
{
  // Assume that any point in our border rect is a hit.
  if (nsRect(ToReferenceFrame(), mFrame->GetSize()).Intersects(aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

nsRegion
nsDisplayThemedBackground::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) {
  nsRegion result;
  *aSnap = false;

  if (mThemeTransparency == nsITheme::eOpaque) {
    result = nsRect(ToReferenceFrame(), mFrame->GetSize());
  }
  return result;
}

bool
nsDisplayThemedBackground::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) {
  if (mAppearance == NS_THEME_WIN_BORDERLESS_GLASS ||
      mAppearance == NS_THEME_WIN_GLASS) {
    *aColor = NS_RGBA(0,0,0,0);
    return true;
  }
  return false;
}

nsRect
nsDisplayThemedBackground::GetPositioningArea()
{
  return nsRect(ToReferenceFrame(), mFrame->GetSize());
}

void
nsDisplayThemedBackground::Paint(nsDisplayListBuilder* aBuilder,
                                 nsRenderingContext* aCtx)
{
  PaintInternal(aBuilder, aCtx, mVisibleRect, nullptr);
}


void
nsDisplayThemedBackground::PaintInternal(nsDisplayListBuilder* aBuilder,
                                         nsRenderingContext* aCtx, const nsRect& aBounds,
                                         nsRect* aClipRect)
{
  // XXXzw this ignores aClipRect.
  nsPresContext* presContext = mFrame->PresContext();
  nsITheme *theme = presContext->GetTheme();
  nsRect borderArea(ToReferenceFrame(), mFrame->GetSize());
  nsRect drawing(borderArea);
  theme->GetWidgetOverflow(presContext->DeviceContext(), mFrame, mAppearance,
                           &drawing);
  drawing.IntersectRect(drawing, aBounds);
  theme->DrawWidgetBackground(aCtx, mFrame, mAppearance, borderArea, drawing);
}

bool nsDisplayThemedBackground::IsWindowActive()
{
  EventStates docState = mFrame->GetContent()->OwnerDoc()->GetDocumentState();
  return !docState.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
}

void nsDisplayThemedBackground::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                          const nsDisplayItemGeometry* aGeometry,
                                                          nsRegion* aInvalidRegion)
{
  const nsDisplayThemedBackgroundGeometry* geometry = static_cast<const nsDisplayThemedBackgroundGeometry*>(aGeometry);

  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  nsRect positioningArea = GetPositioningArea();
  if (!positioningArea.IsEqualInterior(geometry->mPositioningArea)) {
    // Invalidate everything (both old and new painting areas).
    aInvalidRegion->Or(bounds, geometry->mBounds);
    return;
  }
  if (!bounds.IsEqualInterior(geometry->mBounds)) {
    // Positioning area is unchanged, so invalidate just the change in the
    // painting area.
    aInvalidRegion->Xor(bounds, geometry->mBounds);
  }
  nsITheme* theme = mFrame->PresContext()->GetTheme();
  if (theme->WidgetAppearanceDependsOnWindowFocus(mAppearance) &&
      IsWindowActive() != geometry->mWindowIsActive) {
    aInvalidRegion->Or(*aInvalidRegion, bounds);
  }
}

nsRect
nsDisplayThemedBackground::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = true;
  return mBounds;
}

nsRect
nsDisplayThemedBackground::GetBoundsInternal() {
  nsPresContext* presContext = mFrame->PresContext();

  nsRect r(nsPoint(0,0), mFrame->GetSize());
  presContext->GetTheme()->
      GetWidgetOverflow(presContext->DeviceContext(), mFrame,
                        mFrame->StyleDisplay()->mAppearance, &r);
#ifdef XP_MACOSX
  // Bug 748219
  r.Inflate(mFrame->PresContext()->AppUnitsPerDevPixel());
#endif

  return r + ToReferenceFrame();
}

bool
nsDisplayBackgroundColor::ApplyOpacity(nsDisplayListBuilder* aBuilder,
                                       float aOpacity,
                                       const DisplayItemClip* aClip)
{
  mColor.a = mColor.a * aOpacity;
  if (aClip) {
    IntersectClip(aBuilder, *aClip);
  }
  return true;
}

void
nsDisplayBackgroundColor::Paint(nsDisplayListBuilder* aBuilder,
                                nsRenderingContext* aCtx)
{
  if (mColor == NS_RGBA(0, 0, 0, 0)) {
    return;
  }

  nsPoint offset = ToReferenceFrame();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  CheckForBorderItem(this, flags);
  nsCSSRendering::PaintBackgroundColor(mColor, mFrame->PresContext(), *aCtx, mFrame,
                                       mVisibleRect,
                                       nsRect(offset, mFrame->GetSize()),
                                       flags);
}

nsRegion
nsDisplayBackgroundColor::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap)
{
  if (mColor.a != 1) {
    return nsRegion();
  }

  if (!mBackgroundStyle)
    return nsRegion();

  *aSnap = true;

  const nsStyleBackground::Layer& bottomLayer = mBackgroundStyle->BottomLayer();
  nsRect borderBox = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  return nsDisplayBackgroundImage::GetInsideClipRegion(this, presContext, bottomLayer.mClip, borderBox, aSnap);
}

bool
nsDisplayBackgroundColor::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor)
{
  *aColor = NS_RGBA_FROM_GFXRGBA(mColor);

  if (!mBackgroundStyle)
    return true;

  return (!nsLayoutUtils::HasNonZeroCorner(mFrame->StyleBorder()->mBorderRadius) &&
          mBackgroundStyle->BottomLayer().mClip == NS_STYLE_BG_CLIP_BORDER);
}

void
nsDisplayBackgroundColor::HitTest(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aRect,
                                  HitTestState* aState,
                                  nsTArray<nsIFrame*> *aOutFrames)
{
  if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    // aRect doesn't intersect our border-radius curve.
    return;
  }

  aOutFrames->AppendElement(mFrame);
}

#ifdef MOZ_DUMP_PAINTING
void
nsDisplayBackgroundColor::WriteDebugInfo(nsACString& aTo)
{
  aTo += nsPrintfCString(" (rgba %f,%f,%f,%f)",
          mColor.r, mColor.g,
          mColor.b, mColor.a);
}
#endif

nsRect
nsDisplayOutline::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

void
nsDisplayOutline::Paint(nsDisplayListBuilder* aBuilder,
                        nsRenderingContext* aCtx) {
  // TODO join outlines together
  nsPoint offset = ToReferenceFrame();
  nsCSSRendering::PaintOutline(mFrame->PresContext(), *aCtx, mFrame,
                               mVisibleRect,
                               nsRect(offset, mFrame->GetSize()),
                               mFrame->StyleContext());
}

bool
nsDisplayOutline::IsInvisibleInRect(const nsRect& aRect)
{
  const nsStyleOutline* outline = mFrame->StyleOutline();
  nsRect borderBox(ToReferenceFrame(), mFrame->GetSize());
  if (borderBox.Contains(aRect) &&
      !nsLayoutUtils::HasNonZeroCorner(outline->mOutlineRadius)) {
    if (outline->mOutlineOffset >= 0) {
      // aRect is entirely inside the border-rect, and the outline isn't
      // rendered inside the border-rect, so the outline is not visible.
      return true;
    }
  }

  return false;
}

void
nsDisplayEventReceiver::HitTest(nsDisplayListBuilder* aBuilder,
                                const nsRect& aRect,
                                HitTestState* aState,
                                nsTArray<nsIFrame*> *aOutFrames)
{
  if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    // aRect doesn't intersect our border-radius curve.
    return;
  }

  aOutFrames->AppendElement(mFrame);
}

void
nsDisplayLayerEventRegions::AddFrame(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame)
{
  NS_ASSERTION(aBuilder->FindReferenceFrameFor(aFrame) == aBuilder->FindReferenceFrameFor(mFrame),
               "Reference frame mismatch");
  uint8_t pointerEvents = aFrame->StyleVisibility()->mPointerEvents;
  if (pointerEvents == NS_STYLE_POINTER_EVENTS_NONE) {
    return;
  }
  // XXX handle other pointerEvents values for SVG
  // XXX Do something clever here for the common case where the border box
  // is obviously entirely inside mHitRegion.
  nsRect borderBox(aBuilder->ToReferenceFrame(aFrame), aFrame->GetSize());
  const DisplayItemClip* clip = aBuilder->ClipState().GetCurrentCombinedClip(aBuilder);
  bool borderBoxHasRoundedCorners =
    nsLayoutUtils::HasNonZeroCorner(aFrame->StyleBorder()->mBorderRadius);
  if (clip) {
    borderBox = clip->ApplyNonRoundedIntersection(borderBox);
    if (clip->GetRoundedRectCount() > 0) {
      borderBoxHasRoundedCorners = true;
    }
  }
  if (borderBoxHasRoundedCorners ||
      (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
    mMaybeHitRegion.Or(mMaybeHitRegion, borderBox);
  } else {
    mHitRegion.Or(mHitRegion, borderBox);
  }
  if (aBuilder->GetAncestorHasTouchEventHandler()) {
    mDispatchToContentHitRegion.Or(mDispatchToContentHitRegion, borderBox);
  }
}

void
nsDisplayCaret::Paint(nsDisplayListBuilder* aBuilder,
                      nsRenderingContext* aCtx) {
  // Note: Because we exist, we know that the caret is visible, so we don't
  // need to check for the caret's visibility.
  mCaret->PaintCaret(aBuilder, aCtx, mFrame, ToReferenceFrame());
}

bool
nsDisplayBorder::IsInvisibleInRect(const nsRect& aRect)
{
  nsRect paddingRect = mFrame->GetPaddingRect() - mFrame->GetPosition() +
    ToReferenceFrame();
  const nsStyleBorder *styleBorder;
  if (paddingRect.Contains(aRect) &&
      !(styleBorder = mFrame->StyleBorder())->IsBorderImageLoaded() &&
      !nsLayoutUtils::HasNonZeroCorner(styleBorder->mBorderRadius)) {
    // aRect is entirely inside the content rect, and no part
    // of the border is rendered inside the content rect, so we are not
    // visible
    // Skip this if there's a border-image (which draws a background
    // too) or if there is a border-radius (which makes the border draw
    // further in).
    return true;
  }

  return false;
}
  
nsDisplayItemGeometry* 
nsDisplayBorder::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsDisplayBorderGeometry(this, aBuilder);
}

void
nsDisplayBorder::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                           const nsDisplayItemGeometry* aGeometry,
                                           nsRegion* aInvalidRegion)
{
  const nsDisplayBorderGeometry* geometry = static_cast<const nsDisplayBorderGeometry*>(aGeometry);
  bool snap;
  if (!geometry->mBounds.IsEqualInterior(GetBounds(aBuilder, &snap)) ||
      !geometry->mContentRect.IsEqualInterior(GetContentRect())) {
    // We can probably get away with only invalidating the difference
    // between the border and padding rects, but the XUL ui at least
    // is apparently painting a background with this?
    aInvalidRegion->Or(GetBounds(aBuilder, &snap), geometry->mBounds);
  }
}
  
void
nsDisplayBorder::Paint(nsDisplayListBuilder* aBuilder,
                       nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsCSSRendering::PaintBorder(mFrame->PresContext(), *aCtx, mFrame,
                              mVisibleRect,
                              nsRect(offset, mFrame->GetSize()),
                              mFrame->StyleContext(),
                              mFrame->GetSkipSides());
}

nsRect
nsDisplayBorder::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  *aSnap = true;
  return CalculateBounds(*mFrame->StyleBorder());
}

nsRect
nsDisplayBorder::CalculateBounds(const nsStyleBorder& aStyleBorder)
{
  nsRect borderBounds(ToReferenceFrame(), mFrame->GetSize());
  if (aStyleBorder.IsBorderImageLoaded()) {
    borderBounds.Inflate(aStyleBorder.GetImageOutset());
    return borderBounds;
  } else {
    nsMargin border = aStyleBorder.GetComputedBorder();
    nsRect result;
    if (border.top > 0) {
      result = nsRect(borderBounds.X(), borderBounds.Y(), borderBounds.Width(), border.top);
    }
    if (border.right > 0) {
      result.UnionRect(result, nsRect(borderBounds.XMost() - border.right, borderBounds.Y(), border.right, borderBounds.Height()));
    }
    if (border.bottom > 0) {
      result.UnionRect(result, nsRect(borderBounds.X(), borderBounds.YMost() - border.bottom, borderBounds.Width(), border.bottom));
    }
    if (border.left > 0) {
      result.UnionRect(result, nsRect(borderBounds.X(), borderBounds.Y(), border.left, borderBounds.Height()));
    }

    return result;
  }
}

// Given a region, compute a conservative approximation to it as a list
// of rectangles that aren't vertically adjacent (i.e., vertically
// adjacent or overlapping rectangles are combined).
// Right now this is only approximate, some vertically overlapping rectangles
// aren't guaranteed to be combined.
static void
ComputeDisjointRectangles(const nsRegion& aRegion,
                          nsTArray<nsRect>* aRects) {
  nscoord accumulationMargin = nsPresContext::CSSPixelsToAppUnits(25);
  nsRect accumulated;
  nsRegionRectIterator iter(aRegion);
  while (true) {
    const nsRect* r = iter.Next();
    if (r && !accumulated.IsEmpty() &&
        accumulated.YMost() >= r->y - accumulationMargin) {
      accumulated.UnionRect(accumulated, *r);
      continue;
    }

    if (!accumulated.IsEmpty()) {
      aRects->AppendElement(accumulated);
      accumulated.SetEmpty();
    }

    if (!r)
      break;

    accumulated = *r;
  }
}

void
nsDisplayBoxShadowOuter::Paint(nsDisplayListBuilder* aBuilder,
                               nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = mFrame->VisualBorderRectRelativeToSelf() + offset;
  nsPresContext* presContext = mFrame->PresContext();
  nsAutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  PROFILER_LABEL("nsDisplayBoxShadowOuter", "Paint",
    js::ProfileEntry::Category::GRAPHICS);

  for (uint32_t i = 0; i < rects.Length(); ++i) {
    nsCSSRendering::PaintBoxShadowOuter(presContext, *aCtx, mFrame,
                                        borderRect, rects[i], mOpacity);
  }
}

nsRect
nsDisplayBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mBounds;
}

nsRect
nsDisplayBoxShadowOuter::GetBoundsInternal() {
  return nsLayoutUtils::GetBoxShadowRectForFrame(mFrame, mFrame->GetSize()) +
         ToReferenceFrame();
}

bool
nsDisplayBoxShadowOuter::IsInvisibleInRect(const nsRect& aRect)
{
  nsPoint origin = ToReferenceFrame();
  nsRect frameRect(origin, mFrame->GetSize());
  if (!frameRect.Contains(aRect))
    return false;

  // the visible region is entirely inside the border-rect, and box shadows
  // never render within the border-rect (unless there's a border radius).
  nscoord twipsRadii[8];
  bool hasBorderRadii = mFrame->GetBorderRadii(twipsRadii);
  if (!hasBorderRadii)
    return true;

  return RoundedRectContainsRect(frameRect, twipsRadii, aRect);
}

bool
nsDisplayBoxShadowOuter::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aAllowVisibleRegionExpansion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion)) {
    return false;
  }

  // Store the actual visible region
  mVisibleRegion.And(*aVisibleRegion, mVisibleRect);
  return true;
}

void
nsDisplayBoxShadowOuter::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                   const nsDisplayItemGeometry* aGeometry,
                                                   nsRegion* aInvalidRegion)
{
  const nsDisplayItemGenericGeometry* geometry =
    static_cast<const nsDisplayItemGenericGeometry*>(aGeometry);
  bool snap;
  if (!geometry->mBounds.IsEqualInterior(GetBounds(aBuilder, &snap)) ||
      !geometry->mBorderRect.IsEqualInterior(GetBorderRect())) {
    nsRegion oldShadow, newShadow;
    nscoord dontCare[8];
    bool hasBorderRadius = mFrame->GetBorderRadii(dontCare);
    if (hasBorderRadius) {
      // If we have rounded corners then we need to invalidate the frame area
      // too since we paint into it.
      oldShadow = geometry->mBounds;
      newShadow = GetBounds(aBuilder, &snap);
    } else {
      oldShadow = oldShadow.Sub(geometry->mBounds, geometry->mBorderRect);
      newShadow = newShadow.Sub(GetBounds(aBuilder, &snap), GetBorderRect());
    }
    aInvalidRegion->Or(oldShadow, newShadow);
  }
}


void
nsDisplayBoxShadowInner::Paint(nsDisplayListBuilder* aBuilder,
                               nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  nsAutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  PROFILER_LABEL("nsDisplayBoxShadowInner", "Paint",
    js::ProfileEntry::Category::GRAPHICS);

  for (uint32_t i = 0; i < rects.Length(); ++i) {
    aCtx->PushState();
    aCtx->IntersectClip(rects[i]);
    nsCSSRendering::PaintBoxShadowInner(presContext, *aCtx, mFrame,
                                        borderRect, rects[i]);
    aCtx->PopState();
  }
}

bool
nsDisplayBoxShadowInner::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aAllowVisibleRegionExpansion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion)) {
    return false;
  }

  // Store the actual visible region
  mVisibleRegion.And(*aVisibleRegion, mVisibleRect);
  return true;
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList)
  : nsDisplayItem(aBuilder, aFrame)
  , mOverrideZIndex(0)
{
  mList.AppendToTop(aList);
  UpdateBounds(aBuilder);

  if (!aFrame || !aFrame->IsTransformed()) {
    return;
  }

  // If the frame is a preserve-3d parent, then we will create transforms
  // inside this list afterwards (see WrapPreserve3DList in nsFrame.cpp).
  // In this case we will always be outside of the transform, so share
  // our parents reference frame.
  if (aFrame->Preserves3DChildren()) {
    mReferenceFrame = 
      aBuilder->FindReferenceFrameFor(GetTransformRootFrame(aFrame));
    mToReferenceFrame = aFrame->GetOffsetToCrossDoc(mReferenceFrame);
    return;
  }

  // If we're a transformed frame, then we need to find out if we're inside
  // the nsDisplayTransform or outside of it. Frames inside the transform
  // need mReferenceFrame == mFrame, outside needs the next ancestor
  // reference frame.
  // If we're inside the transform, then the nsDisplayItem constructor
  // will have done the right thing.
  // If we're outside the transform, then we should have only one child
  // (since nsDisplayTransform wraps all actual content), and that child
  // will have the correct reference frame set (since nsDisplayTransform
  // handles this explictly).
  //
  // Preserve-3d can cause us to have multiple nsDisplayTransform
  // children.
  nsDisplayItem *i = mList.GetBottom();
  if (i && (!i->GetAbove() || i->GetType() == TYPE_TRANSFORM) && 
      i->Frame() == mFrame) {
    mReferenceFrame = i->ReferenceFrame();
    mToReferenceFrame = i->ToReferenceFrame();
  }
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayItem* aItem)
  : nsDisplayItem(aBuilder, aFrame)
  , mOverrideZIndex(0)
{
  mList.AppendToTop(aItem);
  UpdateBounds(aBuilder);
  
  if (!aFrame || !aFrame->IsTransformed()) {
    return;
  }

  if (aFrame->Preserves3DChildren()) {
    mReferenceFrame = 
      aBuilder->FindReferenceFrameFor(GetTransformRootFrame(aFrame));
    mToReferenceFrame = aFrame->GetOffsetToCrossDoc(mReferenceFrame);
    return;
  }

  // See the previous nsDisplayWrapList constructor
  if (aItem->Frame() == aFrame) {
    mReferenceFrame = aItem->ReferenceFrame();
    mToReferenceFrame = aItem->ToReferenceFrame();
  }
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayItem* aItem,
                                     const nsIFrame* aReferenceFrame,
                                     const nsPoint& aToReferenceFrame)
  : nsDisplayItem(aBuilder, aFrame, aReferenceFrame, aToReferenceFrame)
  , mOverrideZIndex(0)
{
  mList.AppendToTop(aItem);
  mBounds = mList.GetBounds(aBuilder);
}

nsDisplayWrapList::~nsDisplayWrapList() {
  mList.DeleteAll();
}

void
nsDisplayWrapList::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                           HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) {
  mList.HitTest(aBuilder, aRect, aState, aOutFrames);
}

nsRect
nsDisplayWrapList::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mBounds;
}

bool
nsDisplayWrapList::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                     nsRegion* aVisibleRegion,
                                     const nsRect& aAllowVisibleRegionExpansion) {
  // Convert the passed in visible region to our appunits.
  nsRegion visibleRegion;
  // mVisibleRect has been clipped to GetClippedBounds
  visibleRegion.And(*aVisibleRegion, mVisibleRect);
  nsRegion originalVisibleRegion = visibleRegion;

  bool retval =
    mList.ComputeVisibilityForSublist(aBuilder, &visibleRegion,
                                      mVisibleRect,
                                      aAllowVisibleRegionExpansion);

  nsRegion removed;
  // removed = originalVisibleRegion - visibleRegion
  removed.Sub(originalVisibleRegion, visibleRegion);
  // aVisibleRegion = aVisibleRegion - removed (modulo any simplifications
  // SubtractFromVisibleRegion does)
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, removed);

  return retval;
}

nsRegion
nsDisplayWrapList::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) {
  *aSnap = false;
  nsRegion result;
  if (mList.IsOpaque()) {
    // Everything within GetBounds that's visible is opaque.
    result = GetBounds(aBuilder, aSnap);
  }
  return result;
}

bool nsDisplayWrapList::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) {
  // We could try to do something but let's conservatively just return false.
  return false;
}

bool nsDisplayWrapList::IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                         nsIFrame* aFrame) {
  NS_WARNING("nsDisplayWrapList::IsVaryingRelativeToMovingFrame called unexpectedly");
  // We could try to do something but let's conservatively just return true.
  return true;
}

void nsDisplayWrapList::Paint(nsDisplayListBuilder* aBuilder,
                              nsRenderingContext* aCtx) {
  NS_ERROR("nsDisplayWrapList should have been flattened away for painting");
}

/**
 * Returns true if all descendant display items can be placed in the same
 * ThebesLayer --- GetLayerState returns LAYER_INACTIVE or LAYER_NONE,
 * and they all have the expected animated geometry root.
 */
static LayerState
RequiredLayerStateForChildren(nsDisplayListBuilder* aBuilder,
                              LayerManager* aManager,
                              const ContainerLayerParameters& aParameters,
                              const nsDisplayList& aList,
                              nsIFrame* aExpectedAnimatedGeometryRootForChildren)
{
  LayerState result = LAYER_INACTIVE;
  for (nsDisplayItem* i = aList.GetBottom(); i; i = i->GetAbove()) {
    if (result == LAYER_INACTIVE &&
        nsLayoutUtils::GetAnimatedGeometryRootFor(i, aBuilder) !=
          aExpectedAnimatedGeometryRootForChildren) {
      result = LAYER_ACTIVE;
    }

    LayerState state = i->GetLayerState(aBuilder, aManager, aParameters);
    if ((state == LAYER_ACTIVE || state == LAYER_ACTIVE_FORCE) &&
        state > result) {
      result = state;
    }
    if (state == LAYER_ACTIVE_EMPTY && state > result) {
      result = LAYER_ACTIVE_FORCE;
    }
    if (state == LAYER_NONE) {
      nsDisplayList* list = i->GetSameCoordinateSystemChildren();
      if (list) {
        LayerState childState =
          RequiredLayerStateForChildren(aBuilder, aManager, aParameters, *list,
              aExpectedAnimatedGeometryRootForChildren);
        if (childState > result) {
          result = childState;
        }
      }
    }
  }
  return result;
}

nsRect nsDisplayWrapList::GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder)
{
  nsRect bounds;
  for (nsDisplayItem* i = mList.GetBottom(); i; i = i->GetAbove()) {
    bounds.UnionRect(bounds, i->GetComponentAlphaBounds(aBuilder));
  }
  return bounds;
}

static nsresult
WrapDisplayList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayList* aList, nsDisplayWrapper* aWrapper) {
  if (!aList->GetTop())
    return NS_OK;
  nsDisplayItem* item = aWrapper->WrapList(aBuilder, aFrame, aList);
  if (!item)
    return NS_ERROR_OUT_OF_MEMORY;
  // aList was emptied
  aList->AppendToTop(item);
  return NS_OK;
}

static nsresult
WrapEachDisplayItem(nsDisplayListBuilder* aBuilder,
                    nsDisplayList* aList, nsDisplayWrapper* aWrapper) {
  nsDisplayList newList;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom())) {
    item = aWrapper->WrapItem(aBuilder, item);
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;
    newList.AppendToTop(item);
  }
  // aList was emptied
  aList->AppendToTop(&newList);
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapLists(nsDisplayListBuilder* aBuilder,
    nsIFrame* aFrame, const nsDisplayListSet& aIn, const nsDisplayListSet& aOut)
{
  nsresult rv = WrapListsInPlace(aBuilder, aFrame, aIn);
  NS_ENSURE_SUCCESS(rv, rv);

  if (&aOut == &aIn)
    return NS_OK;
  aOut.BorderBackground()->AppendToTop(aIn.BorderBackground());
  aOut.BlockBorderBackgrounds()->AppendToTop(aIn.BlockBorderBackgrounds());
  aOut.Floats()->AppendToTop(aIn.Floats());
  aOut.Content()->AppendToTop(aIn.Content());
  aOut.PositionedDescendants()->AppendToTop(aIn.PositionedDescendants());
  aOut.Outlines()->AppendToTop(aIn.Outlines());
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapListsInPlace(nsDisplayListBuilder* aBuilder,
    nsIFrame* aFrame, const nsDisplayListSet& aLists)
{
  nsresult rv;
  if (WrapBorderBackground()) {
    // Our border-backgrounds are in-flow
    rv = WrapDisplayList(aBuilder, aFrame, aLists.BorderBackground(), this);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // Our block border-backgrounds are in-flow
  rv = WrapDisplayList(aBuilder, aFrame, aLists.BlockBorderBackgrounds(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The floats are not in flow
  rv = WrapEachDisplayItem(aBuilder, aLists.Floats(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // Our child content is in flow
  rv = WrapDisplayList(aBuilder, aFrame, aLists.Content(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The positioned descendants may not be in-flow
  rv = WrapEachDisplayItem(aBuilder, aLists.PositionedDescendants(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The outlines may not be in-flow
  return WrapEachDisplayItem(aBuilder, aLists.Outlines(), this);
}

nsDisplayOpacity::nsDisplayOpacity(nsDisplayListBuilder* aBuilder,
                                   nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList)
    , mOpacity(aFrame->StyleDisplay()->mOpacity) {
  MOZ_COUNT_CTOR(nsDisplayOpacity);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayOpacity::~nsDisplayOpacity() {
  MOZ_COUNT_DTOR(nsDisplayOpacity);
}
#endif

nsRegion nsDisplayOpacity::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) {
  *aSnap = false;
  // The only time where mOpacity == 1.0 should be when we have will-change.
  // We could report this as opaque then but when the will-change value starts
  // animating the element would become non opaque and could cause repaints.
  return nsRegion();
}

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer>
nsDisplayOpacity::BuildLayer(nsDisplayListBuilder* aBuilder,
                             LayerManager* aManager,
                             const ContainerLayerParameters& aContainerParameters) {
  if (mOpacity == 0 && mFrame->GetContent() &&
      !nsLayoutUtils::HasAnimations(mFrame->GetContent(), eCSSProperty_opacity)) {
    return nullptr;
  }
  nsRefPtr<Layer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           aContainerParameters, nullptr);
  if (!container)
    return nullptr;

  container->SetOpacity(mOpacity);
  nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(container, aBuilder,
                                                           this, mFrame,
                                                           eCSSProperty_opacity);
  return container.forget();
}

/**
 * This doesn't take into account layer scaling --- the layer may be
 * rendered at a higher (or lower) resolution, affecting the retained layer
 * size --- but this should be good enough.
 */
static bool
IsItemTooSmallForActiveLayer(nsDisplayItem* aItem)
{
  nsIntRect visibleDevPixels = aItem->GetVisibleRect().ToOutsidePixels(
          aItem->Frame()->PresContext()->AppUnitsPerDevPixel());
  static const int MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS = 16;
  return visibleDevPixels.Size() <
    nsIntSize(MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS, MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS);
}

bool
nsDisplayOpacity::NeedsActiveLayer()
{
  if (ActiveLayerTracker::IsStyleAnimated(mFrame, eCSSProperty_opacity) &&
      !IsItemTooSmallForActiveLayer(this))
    return true;
  if (mFrame->GetContent()) {
    if (nsLayoutUtils::HasAnimationsForCompositor(mFrame->GetContent(),
                                                  eCSSProperty_opacity)) {
      return true;
    }
  }
  return false;
}

bool
nsDisplayOpacity::ApplyOpacity(nsDisplayListBuilder* aBuilder,
                             float aOpacity,
                             const DisplayItemClip* aClip)
{
  mOpacity = mOpacity * aOpacity;
  if (aClip) {
    IntersectClip(aBuilder, *aClip);
  }
  return true;
}

bool
nsDisplayOpacity::ShouldFlattenAway(nsDisplayListBuilder* aBuilder)
{
  if (NeedsActiveLayer())
    return false;

  nsDisplayItem* child = mList.GetBottom();
  // Only try folding our opacity down if we have a single
  // child. We could potentially do this also if we had multiple
  // children as long as they don't overlap.
  if (!child || child->GetAbove()) {
    return false;
  }

  return child->ApplyOpacity(aBuilder, mOpacity, mClip);
}

nsDisplayItem::LayerState
nsDisplayOpacity::GetLayerState(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aParameters) {
  if (NeedsActiveLayer())
    return LAYER_ACTIVE;

  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters, mList,
    nsLayoutUtils::GetAnimatedGeometryRootFor(this, aBuilder));
}

bool
nsDisplayOpacity::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion,
                                    const nsRect& aAllowVisibleRegionExpansion) {
  // Our children are translucent so we should not allow them to subtract
  // area from aVisibleRegion. We do need to find out what is visible under
  // our children in the temporary compositing buffer, because if our children
  // paint our entire bounds opaquely then we don't need an alpha channel in
  // the temporary compositing buffer.
  nsRect bounds = GetClippedBounds(aBuilder);
  nsRegion visibleUnderChildren;
  visibleUnderChildren.And(*aVisibleRegion, bounds);
  nsRect allowExpansion = bounds.Intersect(aAllowVisibleRegionExpansion);
  return
    nsDisplayWrapList::ComputeVisibility(aBuilder, &visibleUnderChildren,
                                         allowExpansion);
}

bool nsDisplayOpacity::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_OPACITY)
    return false;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  MergeFromTrackingMergedFrames(static_cast<nsDisplayOpacity*>(aItem));
  return true;
}

#ifdef MOZ_DUMP_PAINTING
void
nsDisplayOpacity::WriteDebugInfo(nsACString& aTo)
{
  aTo += nsPrintfCString(" (opacity %f)", mOpacity);
}
#endif

nsDisplayMixBlendMode::nsDisplayMixBlendMode(nsDisplayListBuilder* aBuilder,
                                             nsIFrame* aFrame, nsDisplayList* aList,
                                             uint32_t aFlags)
: nsDisplayWrapList(aBuilder, aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplayMixBlendMode);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayMixBlendMode::~nsDisplayMixBlendMode() {
  MOZ_COUNT_DTOR(nsDisplayMixBlendMode);
}
#endif

nsRegion nsDisplayMixBlendMode::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                                bool* aSnap) {
  *aSnap = false;
  // We are never considered opaque
  return nsRegion();
}

LayerState
nsDisplayMixBlendMode::GetLayerState(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aParameters)
{
  gfxContext::GraphicsOperator op = nsCSSRendering::GetGFXBlendMode(mFrame->StyleDisplay()->mMixBlendMode);
  if (aManager->SupportsMixBlendMode(gfx::CompositionOpForOp(op))) {
    return LAYER_ACTIVE;
  }
  return LAYER_INACTIVE;
}

// nsDisplayMixBlendMode uses layers for rendering
already_AddRefed<Layer>
nsDisplayMixBlendMode::BuildLayer(nsDisplayListBuilder* aBuilder,
                                  LayerManager* aManager,
                                  const ContainerLayerParameters& aContainerParameters) {
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  nsRefPtr<Layer> container = aManager->GetLayerBuilder()->
  BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                         newContainerParameters, nullptr);
  if (!container) {
    return nullptr;
  }

  container->DeprecatedSetMixBlendMode(nsCSSRendering::GetGFXBlendMode(mFrame->StyleDisplay()->mMixBlendMode));

  return container.forget();
}

bool nsDisplayMixBlendMode::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                              nsRegion* aVisibleRegion,
                                              const nsRect& aAllowVisibleRegionExpansion) {
  // Our children are need their backdrop so we should not allow them to subtract
  // area from aVisibleRegion. We do need to find out what is visible under
  // our children in the temporary compositing buffer, because if our children
  // paint our entire bounds opaquely then we don't need an alpha channel in
  // the temporary compositing buffer.
  nsRect bounds = GetClippedBounds(aBuilder);
  nsRegion visibleUnderChildren;
  visibleUnderChildren.And(*aVisibleRegion, bounds);
  nsRect allowExpansion = bounds.Intersect(aAllowVisibleRegionExpansion);
  return
  nsDisplayWrapList::ComputeVisibility(aBuilder, &visibleUnderChildren,
                                       allowExpansion);
}

bool nsDisplayMixBlendMode::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_MIX_BLEND_MODE)
    return false;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  MergeFromTrackingMergedFrames(static_cast<nsDisplayMixBlendMode*>(aItem));
  return true;
}

nsDisplayBlendContainer::nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame, nsDisplayList* aList,
                                                 BlendModeSet& aContainedBlendModes)
    : nsDisplayWrapList(aBuilder, aFrame, aList)
    , mContainedBlendModes(aContainedBlendModes)
    , mCanBeActive(true)
{
  MOZ_COUNT_CTOR(nsDisplayBlendContainer);
}

nsDisplayBlendContainer::nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList)
    , mCanBeActive(false)
{
  MOZ_COUNT_CTOR(nsDisplayBlendContainer);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayBlendContainer::~nsDisplayBlendContainer() {
  MOZ_COUNT_DTOR(nsDisplayBlendContainer);
}
#endif

// nsDisplayBlendContainer uses layers for rendering
already_AddRefed<Layer>
nsDisplayBlendContainer::BuildLayer(nsDisplayListBuilder* aBuilder,
                                    LayerManager* aManager,
                                    const ContainerLayerParameters& aContainerParameters) {
  // turn off anti-aliasing in the parent stacking context because it changes
  // how the group is initialized.
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  nsRefPtr<Layer> container = aManager->GetLayerBuilder()->
  BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                         newContainerParameters, nullptr);
  if (!container) {
    return nullptr;
  }
  
  container->SetForceIsolatedGroup(true);
  return container.forget();
}

bool nsDisplayBlendContainer::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_BLEND_CONTAINER)
    return false;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  MergeFromTrackingMergedFrames(static_cast<nsDisplayBlendContainer*>(aItem));
  return true;
}

nsDisplayOwnLayer::nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList,
                                     uint32_t aFlags, ViewID aScrollTarget)
    : nsDisplayWrapList(aBuilder, aFrame, aList)
    , mFlags(aFlags)
    , mScrollTarget(aScrollTarget) {
  MOZ_COUNT_CTOR(nsDisplayOwnLayer);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayOwnLayer::~nsDisplayOwnLayer() {
  MOZ_COUNT_DTOR(nsDisplayOwnLayer);
}
#endif

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer>
nsDisplayOwnLayer::BuildLayer(nsDisplayListBuilder* aBuilder,
                              LayerManager* aManager,
                              const ContainerLayerParameters& aContainerParameters) {
  nsRefPtr<ContainerLayer> layer = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           aContainerParameters, nullptr);
  if (mFlags & VERTICAL_SCROLLBAR) {
    layer->SetScrollbarData(mScrollTarget, Layer::ScrollDirection::VERTICAL);
  }
  if (mFlags & HORIZONTAL_SCROLLBAR) {
    layer->SetScrollbarData(mScrollTarget, Layer::ScrollDirection::HORIZONTAL);
  }

  if (mFlags & GENERATE_SUBDOC_INVALIDATIONS) {
    mFrame->PresContext()->SetNotifySubDocInvalidationData(layer);
  }
  return layer.forget();
}

nsDisplaySubDocument::nsDisplaySubDocument(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame, nsDisplayList* aList,
                                           uint32_t aFlags)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList, aFlags)
    , mScrollParentId(aBuilder->GetCurrentScrollParentId())
{
  MOZ_COUNT_CTOR(nsDisplaySubDocument);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplaySubDocument::~nsDisplaySubDocument() {
  MOZ_COUNT_DTOR(nsDisplaySubDocument);
}
#endif

already_AddRefed<Layer>
nsDisplaySubDocument::BuildLayer(nsDisplayListBuilder* aBuilder,
                                 LayerManager* aManager,
                                 const ContainerLayerParameters& aContainerParameters) {
  nsRefPtr<Layer> layer = nsDisplayOwnLayer::BuildLayer(
    aBuilder, aManager, aContainerParameters);

  if (!(mFlags & GENERATE_SCROLLABLE_LAYER)) {
    return layer.forget();
  }

  NS_ASSERTION(layer->AsContainerLayer(), "nsDisplayOwnLayer should have made a ContainerLayer");
  if (ContainerLayer* container = layer->AsContainerLayer()) {
    nsPresContext* presContext = mFrame->PresContext();
    nsIFrame* rootScrollFrame = presContext->PresShell()->GetRootScrollFrame();
    bool isRootContentDocument = presContext->IsRootContentDocument();

    nsRect viewport = mFrame->GetRect() -
                      mFrame->GetPosition() +
                      mFrame->GetOffsetToCrossDoc(ReferenceFrame());

    container->SetScrollHandoffParentId(mScrollParentId);
    RecordFrameMetrics(mFrame, rootScrollFrame, ReferenceFrame(),
                       container, mList.GetVisibleRect(), viewport,
                       false, isRootContentDocument, aContainerParameters);
  }

  return layer.forget();
}

nsRect
nsDisplaySubDocument::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  bool usingDisplayPort =
    nsLayoutUtils::ViewportHasDisplayPort(mFrame->PresContext());

  if ((mFlags & GENERATE_SCROLLABLE_LAYER) && usingDisplayPort) {
    *aSnap = false;
    return mFrame->GetRect() + aBuilder->ToReferenceFrame(mFrame);
  }

  return nsDisplayOwnLayer::GetBounds(aBuilder, aSnap);
}

bool
nsDisplaySubDocument::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion,
                                        const nsRect& aAllowVisibleRegionExpansion)
{
  nsRect displayport;
  bool usingDisplayPort =
    nsLayoutUtils::ViewportHasDisplayPort(mFrame->PresContext(), &displayport);

  if (!(mFlags & GENERATE_SCROLLABLE_LAYER) || !usingDisplayPort) {
    return nsDisplayWrapList::ComputeVisibility(aBuilder, aVisibleRegion,
                                                aAllowVisibleRegionExpansion);
  }

  nsRegion childVisibleRegion;
  // The visible region for the children may be much bigger than the hole we
  // are viewing the children from, so that the compositor process has enough
  // content to asynchronously pan while content is being refreshed.
  childVisibleRegion = displayport + mFrame->GetOffsetToCrossDoc(ReferenceFrame());

  nsRect boundedRect =
    childVisibleRegion.GetBounds().Intersect(mList.GetBounds(aBuilder));
  nsRect allowExpansion = boundedRect.Intersect(aAllowVisibleRegionExpansion);
  bool visible = mList.ComputeVisibilityForSublist(
    aBuilder, &childVisibleRegion, boundedRect, allowExpansion,
    usingDisplayPort ? mFrame : nullptr);
  // We don't allow this computation to influence aVisibleRegion, on the
  // assumption that the layer can be asynchronously scrolled so we'll
  // definitely need all the content under it.

  return visible;
}

bool
nsDisplaySubDocument::ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder)
{
  bool usingDisplayPort =
    nsLayoutUtils::ViewportHasDisplayPort(mFrame->PresContext());

  if ((mFlags & GENERATE_SCROLLABLE_LAYER) && usingDisplayPort) {
    return true;
  }

  return nsDisplayOwnLayer::ShouldBuildLayerEvenIfInvisible(aBuilder);
}

nsRegion
nsDisplaySubDocument::GetOpaqueRegion(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  bool usingDisplayPort =
    nsLayoutUtils::ViewportHasDisplayPort(mFrame->PresContext());

  if ((mFlags & GENERATE_SCROLLABLE_LAYER) && usingDisplayPort) {
    *aSnap = false;
    return nsRegion();
  }

  return nsDisplayOwnLayer::GetOpaqueRegion(aBuilder, aSnap);
}

nsDisplayResolution::nsDisplayResolution(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame, nsDisplayList* aList,
                                         uint32_t aFlags)
    : nsDisplaySubDocument(aBuilder, aFrame, aList, aFlags) {
  MOZ_COUNT_CTOR(nsDisplayResolution);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayResolution::~nsDisplayResolution() {
  MOZ_COUNT_DTOR(nsDisplayResolution);
}
#endif

already_AddRefed<Layer>
nsDisplayResolution::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aContainerParameters) {
  nsIPresShell* presShell = mFrame->PresContext()->PresShell();
  ContainerLayerParameters containerParameters(
    presShell->GetXResolution(), presShell->GetYResolution(), nsIntPoint(),
    aContainerParameters);

  nsRefPtr<Layer> layer = nsDisplaySubDocument::BuildLayer(
    aBuilder, aManager, containerParameters);
  layer->SetPostScale(1.0f / presShell->GetXResolution(),
                      1.0f / presShell->GetYResolution());
  return layer.forget();
}

nsDisplayStickyPosition::nsDisplayStickyPosition(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame,
                                                 nsDisplayList* aList)
  : nsDisplayOwnLayer(aBuilder, aFrame, aList)
{
  MOZ_COUNT_CTOR(nsDisplayStickyPosition);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayStickyPosition::~nsDisplayStickyPosition() {
  MOZ_COUNT_DTOR(nsDisplayStickyPosition);
}
#endif

already_AddRefed<Layer>
nsDisplayStickyPosition::BuildLayer(nsDisplayListBuilder* aBuilder,
                                    LayerManager* aManager,
                                    const ContainerLayerParameters& aContainerParameters) {
  nsRefPtr<Layer> layer =
    nsDisplayOwnLayer::BuildLayer(aBuilder, aManager, aContainerParameters);

  StickyScrollContainer* stickyScrollContainer = StickyScrollContainer::
    GetStickyScrollContainerForFrame(mFrame);
  if (!stickyScrollContainer) {
    return layer.forget();
  }

  nsIFrame* scrollFrame = do_QueryFrame(stickyScrollContainer->ScrollFrame());
  nsPresContext* presContext = scrollFrame->PresContext();

  // Sticky position frames whose scroll frame is the root scroll frame are
  // reflowed into the scroll-port size if one has been set.
  nsSize scrollFrameSize = scrollFrame->GetSize();
  if (scrollFrame == presContext->PresShell()->GetRootScrollFrame() &&
      presContext->PresShell()->IsScrollPositionClampingScrollPortSizeSet()) {
    scrollFrameSize = presContext->PresShell()->
      GetScrollPositionClampingScrollPortSize();
  }

  nsLayoutUtils::SetFixedPositionLayerData(layer, scrollFrame,
    nsRect(scrollFrame->GetOffsetToCrossDoc(ReferenceFrame()), scrollFrameSize),
    mFrame, presContext, aContainerParameters);

  ViewID scrollId = nsLayoutUtils::FindOrCreateIDFor(
    stickyScrollContainer->ScrollFrame()->GetScrolledFrame()->GetContent());

  float factor = presContext->AppUnitsPerDevPixel();
  nsRect outer;
  nsRect inner;
  stickyScrollContainer->GetScrollRanges(mFrame, &outer, &inner);
  LayerRect stickyOuter(NSAppUnitsToFloatPixels(outer.x, factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(outer.y, factor) *
                          aContainerParameters.mYScale,
                        NSAppUnitsToFloatPixels(outer.width, factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(outer.height, factor) *
                          aContainerParameters.mYScale);
  LayerRect stickyInner(NSAppUnitsToFloatPixels(inner.x, factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(inner.y, factor) *
                          aContainerParameters.mYScale,
                        NSAppUnitsToFloatPixels(inner.width, factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(inner.height, factor) *
                          aContainerParameters.mYScale);
  layer->SetStickyPositionData(scrollId, stickyOuter, stickyInner);

  return layer.forget();
}

bool nsDisplayStickyPosition::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_STICKY_POSITION)
    return false;
  // Items with the same fixed position frame can be merged.
  nsDisplayStickyPosition* other = static_cast<nsDisplayStickyPosition*>(aItem);
  if (other->mFrame != mFrame)
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  MergeFromTrackingMergedFrames(other);
  return true;
}

nsDisplayScrollLayer::nsDisplayScrollLayer(nsDisplayListBuilder* aBuilder,
                                           nsDisplayList* aList,
                                           nsIFrame* aForFrame,
                                           nsIFrame* aScrolledFrame,
                                           nsIFrame* aScrollFrame)
  : nsDisplayWrapList(aBuilder, aForFrame, aList)
  , mScrollFrame(aScrollFrame)
  , mScrolledFrame(aScrolledFrame)
  , mScrollParentId(aBuilder->GetCurrentScrollParentId())
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollLayer);
#endif

  NS_ASSERTION(mScrolledFrame && mScrolledFrame->GetContent(),
               "Need a child frame with content");
}

nsDisplayScrollLayer::nsDisplayScrollLayer(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem,
                                           nsIFrame* aForFrame,
                                           nsIFrame* aScrolledFrame,
                                           nsIFrame* aScrollFrame)
  : nsDisplayWrapList(aBuilder, aForFrame, aItem)
  , mScrollFrame(aScrollFrame)
  , mScrolledFrame(aScrolledFrame)
  , mScrollParentId(aBuilder->GetCurrentScrollParentId())
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollLayer);
#endif

  NS_ASSERTION(mScrolledFrame && mScrolledFrame->GetContent(),
               "Need a child frame with content");
}

nsDisplayScrollLayer::nsDisplayScrollLayer(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aForFrame,
                                           nsIFrame* aScrolledFrame,
                                           nsIFrame* aScrollFrame)
  : nsDisplayWrapList(aBuilder, aForFrame)
  , mScrollFrame(aScrollFrame)
  , mScrolledFrame(aScrolledFrame)
  , mScrollParentId(aBuilder->GetCurrentScrollParentId())
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollLayer);
#endif

  NS_ASSERTION(mScrolledFrame && mScrolledFrame->GetContent(),
               "Need a child frame with content");
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayScrollLayer::~nsDisplayScrollLayer()
{
  MOZ_COUNT_DTOR(nsDisplayScrollLayer);
}
#endif

nsRect
nsDisplayScrollLayer::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  nsIScrollableFrame* sf = do_QueryFrame(mScrollFrame);
  if (sf) {
    *aSnap = false;
    return sf->GetScrollPortRect() + aBuilder->ToReferenceFrame(mScrollFrame);
  }
  return nsDisplayWrapList::GetBounds(aBuilder, aSnap);
}

already_AddRefed<Layer>
nsDisplayScrollLayer::BuildLayer(nsDisplayListBuilder* aBuilder,
                                 LayerManager* aManager,
                                 const ContainerLayerParameters& aContainerParameters) {
  nsRefPtr<ContainerLayer> layer = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           aContainerParameters, nullptr);

  nsRect viewport = mScrollFrame->GetRect() -
                    mScrollFrame->GetPosition() +
                    mScrollFrame->GetOffsetToCrossDoc(ReferenceFrame());

  layer->SetScrollHandoffParentId(mScrollParentId);
  RecordFrameMetrics(mScrolledFrame, mScrollFrame, ReferenceFrame(), layer,
                     mList.GetVisibleRect(), viewport,
                     false, false, aContainerParameters);

  return layer.forget();
}

bool
nsDisplayScrollLayer::ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder)
{
  if (nsLayoutUtils::GetDisplayPort(mScrolledFrame->GetContent(), nullptr)) {
    return true;
  }

  return nsDisplayWrapList::ShouldBuildLayerEvenIfInvisible(aBuilder);
}

bool
nsDisplayScrollLayer::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion,
                                        const nsRect& aAllowVisibleRegionExpansion)
{
  nsRect displayport;
  bool usingDisplayPort =
    nsLayoutUtils::GetDisplayPort(mScrolledFrame->GetContent(), &displayport);
  nsRegion childVisibleRegion;
  if (usingDisplayPort) {
    // The visible region for the children may be much bigger than the hole we
    // are viewing the children from, so that the compositor process has enough
    // content to asynchronously pan while content is being refreshed.
    childVisibleRegion = displayport + mScrollFrame->GetOffsetToCrossDoc(ReferenceFrame());
  } else {
    bool snap;
    childVisibleRegion = GetBounds(aBuilder, &snap);
  }

  nsRect boundedRect =
    childVisibleRegion.GetBounds().Intersect(mList.GetBounds(aBuilder));
  nsRect allowExpansion = boundedRect.Intersect(aAllowVisibleRegionExpansion);
  bool visible = mList.ComputeVisibilityForSublist(
    aBuilder, &childVisibleRegion, boundedRect, allowExpansion,
    usingDisplayPort ? mScrollFrame : nullptr);
  // We don't allow this computation to influence aVisibleRegion, on the
  // assumption that the layer can be asynchronously scrolled so we'll
  // definitely need all the content under it.

  return visible;
}

LayerState
nsDisplayScrollLayer::GetLayerState(nsDisplayListBuilder* aBuilder,
                                    LayerManager* aManager,
                                    const ContainerLayerParameters& aParameters)
{
  // Force this as a layer so we can scroll asynchronously.
  // This causes incorrect rendering for rounded clips!
  return LAYER_ACTIVE_FORCE;
}

// Check if we are going to clip an abs pos item that we don't contain.
// Root scroll frames clip all their descendants, so we don't need to worry
// about them.
bool
WouldCauseIncorrectClippingOnAbsPosItem(nsDisplayListBuilder* aBuilder,
                                        nsDisplayScrollLayer* aItem)
{
  nsIFrame* scrollFrame = aItem->GetScrollFrame();
  nsIPresShell* presShell = scrollFrame->PresContext()->PresShell();
  if (scrollFrame == presShell->GetRootScrollFrame()) {
    return false;
  }
  nsIFrame* scrolledFrame = aItem->GetScrolledFrame();
  nsIFrame* frame = aItem->Frame();
  if (frame == scrolledFrame || !frame->IsAbsolutelyPositioned() ||
      nsLayoutUtils::IsAncestorFrameCrossDoc(scrollFrame, frame, presShell->GetRootFrame())) {
    return false;
  }
  if (!aItem->GetClip().IsRectAffectedByClip(aItem->GetChildren()->GetBounds(aBuilder))) {
    return false;
  }
  return true;
}

bool
nsDisplayScrollLayer::TryMerge(nsDisplayListBuilder* aBuilder,
                               nsDisplayItem* aItem)
{
  if (aItem->GetType() != TYPE_SCROLL_LAYER) {
    return false;
  }
  nsDisplayScrollLayer* other = static_cast<nsDisplayScrollLayer*>(aItem);
  if (other->mScrolledFrame != this->mScrolledFrame) {
    return false;
  }
  if (aItem->GetClip() != GetClip()) {
    return false;
  }

  if (WouldCauseIncorrectClippingOnAbsPosItem(aBuilder, this) ||
      WouldCauseIncorrectClippingOnAbsPosItem(aBuilder, other)) {
    return false;
  }

  NS_ASSERTION(other->mReferenceFrame == mReferenceFrame,
               "Must have the same reference frame!");

  FrameProperties props = mScrolledFrame->Properties();
  props.Set(nsIFrame::ScrollLayerCount(),
    reinterpret_cast<void*>(GetScrollLayerCount() - 1));

  // Swap frames with the other item before doing MergeFrom.
  // XXX - This ensures that the frame associated with a scroll layer after
  // merging is the first, rather than the last. This tends to change less,
  // ensuring we're more likely to retain the associated gfx layer.
  // See Bug 729534 and Bug 731641.
  nsIFrame* tmp = mFrame;
  mFrame = other->mFrame;
  other->mFrame = tmp;
  MergeFromTrackingMergedFrames(other);
  return true;
}

void
PropagateClip(nsDisplayListBuilder* aBuilder, const DisplayItemClip& aClip,
              nsDisplayList* aList)
{
  for (nsDisplayItem* i = aList->GetBottom(); i != nullptr; i = i->GetAbove()) {
    DisplayItemClip clip(i->GetClip());
    clip.IntersectWith(aClip);
    i->SetClip(aBuilder, clip);
    nsDisplayList* list = i->GetSameCoordinateSystemChildren();
    if (list) {
      PropagateClip(aBuilder, aClip, list);
    }
  }
}

bool
nsDisplayScrollLayer::ShouldFlattenAway(nsDisplayListBuilder* aBuilder)
{
  bool badAbsPosClip = WouldCauseIncorrectClippingOnAbsPosItem(aBuilder, this);
  if (GetScrollLayerCount() > 1 || badAbsPosClip) {
    // Propagate our clip to our children. The clip for the scroll frame is
    // on this item, but not our child items so that they can draw non-visible
    // parts of the display port. But if we are flattening we failed and can't
    // draw the extra content, so it needs to be clipped.
    // But don't induce our clip on abs pos frames that we shouldn't be clipping.
    if (!badAbsPosClip) {
      PropagateClip(aBuilder, GetClip(), &mList);
    }

    // Output something so the failure can be noted.
    nsresult status;
    mScrolledFrame->GetContent()->GetProperty(nsGkAtoms::AsyncScrollLayerCreationFailed, &status);
    if (status == NS_PROPTABLE_PROP_NOT_THERE) {
      mScrolledFrame->GetContent()->SetProperty(nsGkAtoms::AsyncScrollLayerCreationFailed, nullptr);
      if (badAbsPosClip) {
        printf_stderr("Async scrollable layer creation failed: scroll layer would induce incorrent clipping to an abs pos item.\n");
      } else {
        printf_stderr("Async scrollable layer creation failed: scroll layer can't have scrollable and non-scrollable items interleaved.\n");
      }
#ifdef MOZ_DUMP_PAINTING
      std::stringstream ss;
      nsFrame::PrintDisplayItem(aBuilder, this, ss, true, false);
      printf_stderr("%s\n", ss.str().c_str());
#endif
    }

    return true;
  }
  if (mFrame != mScrolledFrame) {
    mMergedFrames.AppendElement(mFrame);
    mFrame = mScrolledFrame;
  }
  return false;
}

intptr_t
nsDisplayScrollLayer::GetScrollLayerCount()
{
  FrameProperties props = mScrolledFrame->Properties();
#ifdef DEBUG
  bool hasCount = false;
  intptr_t result = reinterpret_cast<intptr_t>(
    props.Get(nsIFrame::ScrollLayerCount(), &hasCount));
  // If this aborts, then the property was either not added before scroll
  // layers were created or the property was deleted to early. If the latter,
  // make sure that nsDisplayScrollInfoLayer is on the bottom of the list so
  // that it is processed last.
  NS_ABORT_IF_FALSE(hasCount, "nsDisplayScrollLayer should always be defined");
  return result;
#else
  return reinterpret_cast<intptr_t>(props.Get(nsIFrame::ScrollLayerCount()));
#endif
}

#ifdef MOZ_DUMP_PAINTING
void
nsDisplayScrollLayer::WriteDebugInfo(nsACString& aTo)
{
  aTo += nsPrintfCString(" (scrollframe %p scrolledframe %p)",
                         mScrollFrame, mScrolledFrame);
}
#endif

nsDisplayScrollInfoLayer::nsDisplayScrollInfoLayer(
  nsDisplayListBuilder* aBuilder,
  nsIFrame* aScrolledFrame,
  nsIFrame* aScrollFrame)
  : nsDisplayScrollLayer(aBuilder, aScrollFrame, aScrolledFrame, aScrollFrame)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollInfoLayer);
#endif
}

nsDisplayScrollInfoLayer::~nsDisplayScrollInfoLayer()
{
  FrameProperties props = mScrolledFrame->Properties();
  props.Remove(nsIFrame::ScrollLayerCount());
  MOZ_COUNT_DTOR(nsDisplayScrollInfoLayer);
}

nsRect
nsDisplayScrollInfoLayer::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  return nsDisplayWrapList::GetBounds(aBuilder, aSnap);
}

LayerState
nsDisplayScrollInfoLayer::GetLayerState(nsDisplayListBuilder* aBuilder,
                                        LayerManager* aManager,
                                        const ContainerLayerParameters& aParameters)
{
  return LAYER_ACTIVE_EMPTY;
}

bool
nsDisplayScrollInfoLayer::TryMerge(nsDisplayListBuilder* aBuilder,
                                   nsDisplayItem* aItem)
{
  return false;
}

bool
nsDisplayScrollInfoLayer::ShouldFlattenAway(nsDisplayListBuilder* aBuilder)
{
  // Layer metadata for a particular scroll frame needs to be unique. Only
  // one nsDisplayScrollLayer (with rendered content) or one
  // nsDisplayScrollInfoLayer (with only the metadata) should survive the
  // visibility computation.
  return GetScrollLayerCount() == 1;
}

nsDisplayZoom::nsDisplayZoom(nsDisplayListBuilder* aBuilder,
                             nsIFrame* aFrame, nsDisplayList* aList,
                             int32_t aAPD, int32_t aParentAPD,
                             uint32_t aFlags)
    : nsDisplaySubDocument(aBuilder, aFrame, aList, aFlags)
    , mAPD(aAPD), mParentAPD(aParentAPD) {
  MOZ_COUNT_CTOR(nsDisplayZoom);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayZoom::~nsDisplayZoom() {
  MOZ_COUNT_DTOR(nsDisplayZoom);
}
#endif

nsRect nsDisplayZoom::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  nsRect bounds = nsDisplaySubDocument::GetBounds(aBuilder, aSnap);
  *aSnap = false;
  return bounds.ConvertAppUnitsRoundOut(mAPD, mParentAPD);
}

void nsDisplayZoom::HitTest(nsDisplayListBuilder *aBuilder,
                            const nsRect& aRect,
                            HitTestState *aState,
                            nsTArray<nsIFrame*> *aOutFrames)
{
  nsRect rect;
  // A 1x1 rect indicates we are just hit testing a point, so pass down a 1x1
  // rect as well instead of possibly rounding the width or height to zero.
  if (aRect.width == 1 && aRect.height == 1) {
    rect.MoveTo(aRect.TopLeft().ConvertAppUnits(mParentAPD, mAPD));
    rect.width = rect.height = 1;
  } else {
    rect = aRect.ConvertAppUnitsRoundOut(mParentAPD, mAPD);
  }
  mList.HitTest(aBuilder, rect, aState, aOutFrames);
}

void nsDisplayZoom::Paint(nsDisplayListBuilder* aBuilder,
                          nsRenderingContext* aCtx)
{
  mList.PaintForFrame(aBuilder, aCtx, mFrame, nsDisplayList::PAINT_DEFAULT);
}

bool nsDisplayZoom::ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                      nsRegion *aVisibleRegion,
                                      const nsRect& aAllowVisibleRegionExpansion)
{
  // Convert the passed in visible region to our appunits.
  nsRegion visibleRegion;
  // mVisibleRect has been clipped to GetClippedBounds
  visibleRegion.And(*aVisibleRegion, mVisibleRect);
  visibleRegion = visibleRegion.ConvertAppUnitsRoundOut(mParentAPD, mAPD);
  nsRegion originalVisibleRegion = visibleRegion;

  nsRect transformedVisibleRect =
    mVisibleRect.ConvertAppUnitsRoundOut(mParentAPD, mAPD);
  nsRect allowExpansion =
    aAllowVisibleRegionExpansion.ConvertAppUnitsRoundIn(mParentAPD, mAPD);
  bool retval;
  // If we are to generate a scrollable layer we call
  // nsDisplaySubDocument::ComputeVisibility to make the necessary adjustments
  // for ComputeVisibility, it does all it's calculations in the child APD.
  bool usingDisplayPort =
    nsLayoutUtils::ViewportHasDisplayPort(mFrame->PresContext());
  if (!(mFlags & GENERATE_SCROLLABLE_LAYER) || !usingDisplayPort) {
    retval =
      mList.ComputeVisibilityForSublist(aBuilder, &visibleRegion,
                                        transformedVisibleRect,
                                        allowExpansion);
  } else {
    retval =
      nsDisplaySubDocument::ComputeVisibility(aBuilder, &visibleRegion,
                                              allowExpansion);
  }

  nsRegion removed;
  // removed = originalVisibleRegion - visibleRegion
  removed.Sub(originalVisibleRegion, visibleRegion);
  // Convert removed region to parent appunits.
  removed = removed.ConvertAppUnitsRoundIn(mAPD, mParentAPD);
  // aVisibleRegion = aVisibleRegion - removed (modulo any simplifications
  // SubtractFromVisibleRegion does)
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, removed);

  return retval;
}

///////////////////////////////////////////////////
// nsDisplayTransform Implementation
//

// Write #define UNIFIED_CONTINUATIONS here to have the transform property try
// to transform content with continuations as one unified block instead of
// several smaller ones.  This is currently disabled because it doesn't work
// correctly, since when the frames are initially being reflowed, their
// continuations all compute their bounding rects independently of each other
// and consequently get the wrong value.  Write #define DEBUG_HIT here to have
// the nsDisplayTransform class dump out a bunch of information about hit
// detection.
#undef  UNIFIED_CONTINUATIONS
#undef  DEBUG_HIT

/* Returns the bounds of a frame as defined for transforms.  If
 * UNIFIED_CONTINUATIONS is not defined, this is simply the frame's bounding
 * rectangle, translated to the origin. Otherwise, returns the smallest
 * rectangle containing a frame and all of its continuations.  For example, if
 * there is a <span> element with several continuations split over several
 * lines, this function will return the rectangle containing all of those
 * continuations.  This rectangle is relative to the origin of the frame's local
 * coordinate space.
 */
#ifndef UNIFIED_CONTINUATIONS

nsRect
nsDisplayTransform::GetFrameBoundsForTransform(const nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "Can't get the bounds of a nonexistent frame!");

  if (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
    // TODO: SVG needs to define what percentage translations resolve against.
    return nsRect();
  }

  return nsRect(nsPoint(0, 0), aFrame->GetSize());
}

#else

nsRect
nsDisplayTransform::GetFrameBoundsForTransform(const nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "Can't get the bounds of a nonexistent frame!");

  nsRect result;

  if (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
    // TODO: SVG needs to define what percentage translations resolve against.
    return result;
  }

  /* Iterate through the continuation list, unioning together all the
   * bounding rects.
   */
  for (const nsIFrame *currFrame = aFrame->FirstContinuation();
       currFrame != nullptr;
       currFrame = currFrame->GetNextContinuation())
    {
      /* Get the frame rect in local coordinates, then translate back to the
       * original coordinates.
       */
      result.UnionRect(result, nsRect(currFrame->GetOffsetTo(aFrame),
                                      currFrame->GetSize()));
    }

  return result;
}

#endif

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                                       nsDisplayList *aList, ComputeTransformFunction aTransformGetter, 
                                       uint32_t aIndex) 
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aList)
  , mTransformGetter(aTransformGetter)
  , mIndex(aIndex)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  NS_ABORT_IF_FALSE(aFrame, "Must have a frame!");
  NS_ABORT_IF_FALSE(!aFrame->IsTransformed(), "Can't specify a transform getter for a transformed frame!");
  mStoredList.SetClip(aBuilder, DisplayItemClip::NoClip());
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                                       nsDisplayList *aList, uint32_t aIndex) 
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aList)
  , mTransformGetter(nullptr)
  , mIndex(aIndex)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  NS_ABORT_IF_FALSE(aFrame, "Must have a frame!");
  mReferenceFrame = 
    aBuilder->FindReferenceFrameFor(GetTransformRootFrame(aFrame));
  mToReferenceFrame = aFrame->GetOffsetToCrossDoc(mReferenceFrame);
  mStoredList.SetClip(aBuilder, DisplayItemClip::NoClip());
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                                       nsDisplayItem *aItem, uint32_t aIndex)
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aItem)
  , mTransformGetter(nullptr)
  , mIndex(aIndex)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  NS_ABORT_IF_FALSE(aFrame, "Must have a frame!");
  mReferenceFrame =
    aBuilder->FindReferenceFrameFor(GetTransformRootFrame(aFrame));
  mToReferenceFrame = aFrame->GetOffsetToCrossDoc(mReferenceFrame);
  mStoredList.SetClip(aBuilder, DisplayItemClip::NoClip());
}

/* Returns the delta specified by the -moz-transform-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the transform origin.  This function is
 * called off the main thread.
 */
/* static */ gfxPoint3D
nsDisplayTransform::GetDeltaToTransformOrigin(const nsIFrame* aFrame,
                                              float aAppUnitsPerPixel,
                                              const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->IsTransformed() || aFrame->StyleDisplay()->BackfaceIsHidden(),
                  "Shouldn't get a delta for an untransformed frame!");

  if (!aFrame->IsTransformed()) {
    return gfxPoint3D();
  }

  /* For both of the coordinates, if the value of -moz-transform is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */
  const nsStyleDisplay* display = aFrame->StyleDisplay();
  nsRect boundingRect = (aBoundsOverride ? *aBoundsOverride :
                         nsDisplayTransform::GetFrameBoundsForTransform(aFrame));

  /* Allows us to access named variables by index. */
  float coords[3];
  const nscoord* dimensions[2] =
    {&boundingRect.width, &boundingRect.height};

  for (uint8_t index = 0; index < 2; ++index) {
    /* If the -moz-transform-origin specifies a percentage, take the percentage
     * of the size of the box.
     */
    const nsStyleCoord &coord = display->mTransformOrigin[index];
    if (coord.GetUnit() == eStyleUnit_Calc) {
      const nsStyleCoord::Calc *calc = coord.GetCalcValue();
      coords[index] =
        NSAppUnitsToFloatPixels(*dimensions[index], aAppUnitsPerPixel) *
          calc->mPercent +
        NSAppUnitsToFloatPixels(calc->mLength, aAppUnitsPerPixel);
    } else if (coord.GetUnit() == eStyleUnit_Percent) {
      coords[index] =
        NSAppUnitsToFloatPixels(*dimensions[index], aAppUnitsPerPixel) *
        coord.GetPercentValue();
    } else {
      NS_ABORT_IF_FALSE(coord.GetUnit() == eStyleUnit_Coord, "unexpected unit");
      coords[index] =
        NSAppUnitsToFloatPixels(coord.GetCoordValue(), aAppUnitsPerPixel);
    }
    if ((aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) &&
        coord.GetUnit() != eStyleUnit_Percent) {
      // <length> values represent offsets from the origin of the SVG element's
      // user space, not the top left of its bounds, so we must adjust for that:
      nscoord offset =
        (index == 0) ? aFrame->GetPosition().x : aFrame->GetPosition().y;
      coords[index] -= NSAppUnitsToFloatPixels(offset, aAppUnitsPerPixel);
    }
  }

  coords[2] = NSAppUnitsToFloatPixels(display->mTransformOrigin[2].GetCoordValue(),
                                      aAppUnitsPerPixel);
  /* Adjust based on the origin of the rectangle. */
  coords[0] += NSAppUnitsToFloatPixels(boundingRect.x, aAppUnitsPerPixel);
  coords[1] += NSAppUnitsToFloatPixels(boundingRect.y, aAppUnitsPerPixel);

  return gfxPoint3D(coords[0], coords[1], coords[2]);
}

/* Returns the delta specified by the -moz-perspective-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the perspective origin. This function is
 * called off the main thread.
 */
/* static */ gfxPoint3D
nsDisplayTransform::GetDeltaToPerspectiveOrigin(const nsIFrame* aFrame,
                                                float aAppUnitsPerPixel)
{
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->IsTransformed() || aFrame->StyleDisplay()->BackfaceIsHidden(),
                  "Shouldn't get a delta for an untransformed frame!");

  if (!aFrame->IsTransformed()) {
    return gfxPoint3D();
  }

  /* For both of the coordinates, if the value of -moz-perspective-origin is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */

  //TODO: Should this be using our bounds or the parent's bounds?
  // How do we handle aBoundsOverride in the latter case?
  nsIFrame* parent = aFrame->GetParentStyleContextFrame();
  if (!parent) {
    return gfxPoint3D();
  }
  const nsStyleDisplay* display = parent->StyleDisplay();
  nsRect boundingRect = nsDisplayTransform::GetFrameBoundsForTransform(parent);

  /* Allows us to access named variables by index. */
  gfxPoint3D result;
  result.z = 0.0f;
  gfxFloat* coords[2] = {&result.x, &result.y};
  const nscoord* dimensions[2] =
    {&boundingRect.width, &boundingRect.height};

  for (uint8_t index = 0; index < 2; ++index) {
    /* If the -moz-transform-origin specifies a percentage, take the percentage
     * of the size of the box.
     */
    const nsStyleCoord &coord = display->mPerspectiveOrigin[index];
    if (coord.GetUnit() == eStyleUnit_Calc) {
      const nsStyleCoord::Calc *calc = coord.GetCalcValue();
      *coords[index] =
        NSAppUnitsToFloatPixels(*dimensions[index], aAppUnitsPerPixel) *
          calc->mPercent +
        NSAppUnitsToFloatPixels(calc->mLength, aAppUnitsPerPixel);
    } else if (coord.GetUnit() == eStyleUnit_Percent) {
      *coords[index] =
        NSAppUnitsToFloatPixels(*dimensions[index], aAppUnitsPerPixel) *
        coord.GetPercentValue();
    } else {
      NS_ABORT_IF_FALSE(coord.GetUnit() == eStyleUnit_Coord, "unexpected unit");
      *coords[index] =
        NSAppUnitsToFloatPixels(coord.GetCoordValue(), aAppUnitsPerPixel);
    }
  }

  nsPoint parentOffset = aFrame->GetOffsetTo(parent);
  gfxPoint3D gfxOffset(
               NSAppUnitsToFloatPixels(parentOffset.x, aAppUnitsPerPixel),
               NSAppUnitsToFloatPixels(parentOffset.y, aAppUnitsPerPixel),
               0.0f);

  return result - gfxOffset;
}

nsDisplayTransform::FrameTransformProperties::FrameTransformProperties(const nsIFrame* aFrame,
                                                                       float aAppUnitsPerPixel,
                                                                       const nsRect* aBoundsOverride)
  : mFrame(aFrame)
  , mTransformList(aFrame->StyleDisplay()->mSpecifiedTransform)
  , mToTransformOrigin(GetDeltaToTransformOrigin(aFrame, aAppUnitsPerPixel, aBoundsOverride))
  , mToPerspectiveOrigin(GetDeltaToPerspectiveOrigin(aFrame, aAppUnitsPerPixel))
  , mChildPerspective(0)
{
  const nsStyleDisplay* parentDisp = nullptr;
  nsStyleContext* parentStyleContext = aFrame->StyleContext()->GetParent();
  if (parentStyleContext) {
    parentDisp = parentStyleContext->StyleDisplay();
  }
  if (parentDisp && parentDisp->mChildPerspective.GetUnit() == eStyleUnit_Coord) {
    mChildPerspective = parentDisp->mChildPerspective.GetCoordValue();
  }
}

/* Wraps up the -moz-transform matrix in a change-of-basis matrix pair that
 * translates from local coordinate space to transform coordinate space, then
 * hands it back.
 */
gfx3DMatrix
nsDisplayTransform::GetResultingTransformMatrix(const FrameTransformProperties& aProperties,
                                                const nsPoint& aOrigin,
                                                float aAppUnitsPerPixel,
                                                const nsRect* aBoundsOverride,
                                                nsIFrame** aOutAncestor)
{
  return GetResultingTransformMatrixInternal(aProperties, aOrigin, aAppUnitsPerPixel,
                                             aBoundsOverride, aOutAncestor, false);
}
 
gfx3DMatrix
nsDisplayTransform::GetResultingTransformMatrix(const nsIFrame* aFrame,
                                                const nsPoint& aOrigin,
                                                float aAppUnitsPerPixel,
                                                const nsRect* aBoundsOverride,
                                                nsIFrame** aOutAncestor,
                                                bool aOffsetByOrigin)
{
  FrameTransformProperties props(aFrame,
                                 aAppUnitsPerPixel,
                                 aBoundsOverride);

  return GetResultingTransformMatrixInternal(props, aOrigin, aAppUnitsPerPixel,
                                             aBoundsOverride, aOutAncestor,
                                             aOffsetByOrigin);
}

gfx3DMatrix
nsDisplayTransform::GetResultingTransformMatrixInternal(const FrameTransformProperties& aProperties,
                                                        const nsPoint& aOrigin,
                                                        float aAppUnitsPerPixel,
                                                        const nsRect* aBoundsOverride,
                                                        nsIFrame** aOutAncestor,
                                                        bool aOffsetByOrigin)
{
  const nsIFrame *frame = aProperties.mFrame;

  if (aOutAncestor) {
    *aOutAncestor = nsLayoutUtils::GetCrossDocParentFrame(frame);
  }

  /* Get the underlying transform matrix.  This requires us to get the
   * bounds of the frame.
   */
  nsRect bounds = (aBoundsOverride ? *aBoundsOverride :
                   nsDisplayTransform::GetFrameBoundsForTransform(frame));

  /* Get the matrix, then change its basis to factor in the origin. */
  bool dummy;
  gfx3DMatrix result;
  // Call IsSVGTransformed() regardless of the value of
  // disp->mSpecifiedTransform, since we still need any transformFromSVGParent.
  mozilla::gfx::Matrix svgTransform, transformFromSVGParent;
  bool hasSVGTransforms =
    frame && frame->IsSVGTransformed(&svgTransform, &transformFromSVGParent);
  /* Transformed frames always have a transform, or are preserving 3d (and might still have perspective!) */
  if (aProperties.mTransformList) {
    result = nsStyleTransformMatrix::ReadTransforms(aProperties.mTransformList->mHead,
                                                    frame ? frame->StyleContext() : nullptr,
                                                    frame ? frame->PresContext() : nullptr,
                                                    dummy, bounds, aAppUnitsPerPixel);
  } else if (hasSVGTransforms) {
    // Correct the translation components for zoom:
    float pixelsPerCSSPx = frame->PresContext()->AppUnitsPerCSSPixel() /
                             aAppUnitsPerPixel;
    svgTransform._31 *= pixelsPerCSSPx;
    svgTransform._32 *= pixelsPerCSSPx;
    result = gfx3DMatrix::From2D(ThebesMatrix(svgTransform));
  }

  if (hasSVGTransforms && !transformFromSVGParent.IsIdentity()) {
    // Correct the translation components for zoom:
    float pixelsPerCSSPx = frame->PresContext()->AppUnitsPerCSSPixel() /
                             aAppUnitsPerPixel;
    transformFromSVGParent._31 *= pixelsPerCSSPx;
    transformFromSVGParent._32 *= pixelsPerCSSPx;
    result = result * gfx3DMatrix::From2D(ThebesMatrix(transformFromSVGParent));
  }

  if (aProperties.mChildPerspective > 0.0) {
    gfx3DMatrix perspective;
    perspective._34 =
      -1.0 / NSAppUnitsToFloatPixels(aProperties.mChildPerspective, aAppUnitsPerPixel);
    /* At the point when perspective is applied, we have been translated to the transform origin.
     * The translation to the perspective origin is the difference between these values.
     */
    result = result * nsLayoutUtils::ChangeMatrixBasis(aProperties.mToPerspectiveOrigin - aProperties.mToTransformOrigin, perspective);
  }

  /* Account for the -moz-transform-origin property by translating the
   * coordinate space to the new origin.
   */
  gfxPoint3D newOrigin =
    gfxPoint3D(NSAppUnitsToFloatPixels(aOrigin.x, aAppUnitsPerPixel),
               NSAppUnitsToFloatPixels(aOrigin.y, aAppUnitsPerPixel),
               0.0f);
  gfxPoint3D roundedOrigin(hasSVGTransforms ? newOrigin.x : NS_round(newOrigin.x),
                           hasSVGTransforms ? newOrigin.y : NS_round(newOrigin.y),
                           0);
  gfxPoint3D offsetBetweenOrigins = roundedOrigin + aProperties.mToTransformOrigin;

  if (frame && frame->Preserves3D()) {
    // Include the transform set on our parent
    NS_ASSERTION(frame->GetParent() &&
                 frame->GetParent()->IsTransformed() &&
                 frame->GetParent()->Preserves3DChildren(),
                 "Preserve3D mismatch!");
    FrameTransformProperties props(frame->GetParent(),
                                   aAppUnitsPerPixel,
                                   nullptr);
    gfx3DMatrix parent =
      GetResultingTransformMatrixInternal(props,
                                          aOrigin - frame->GetPosition(),
                                          aAppUnitsPerPixel, nullptr,
                                          aOutAncestor, false);

    result = nsLayoutUtils::ChangeMatrixBasis(offsetBetweenOrigins, result) * parent;
    if (aOffsetByOrigin) {
      result.Translate(roundedOrigin);
    }
    return result;
  }

  if (aOffsetByOrigin) {
    // We can fold the final translation by roundedOrigin into the first matrix
    // basis change translation. This is more stable against variation due to
    // insufficient floating point precision than reversing the translation
    // afterwards.
    result.Translate(-aProperties.mToTransformOrigin);
    result.TranslatePost(offsetBetweenOrigins);
  } else {
    result = nsLayoutUtils::ChangeMatrixBasis(offsetBetweenOrigins, result);
  }
  return result;
}

bool
nsDisplayOpacity::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder)
{
  if (ActiveLayerTracker::IsStyleAnimated(mFrame, eCSSProperty_opacity)) {
    return true;
  }

  if (nsLayoutUtils::IsAnimationLoggingEnabled()) {
    nsCString message;
    message.AppendLiteral("Performance warning: Async animation disabled because frame was not marked active for opacity animation");
    ElementAnimationCollection::LogAsyncAnimationFailure(message,
                                                         Frame()->GetContent());
  }
  return false;
}

bool
nsDisplayTransform::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder)
{
  return ShouldPrerenderTransformedContent(aBuilder,
                                           Frame(),
                                           nsLayoutUtils::IsAnimationLoggingEnabled());
}

/* static */ bool
nsDisplayTransform::ShouldPrerenderTransformedContent(nsDisplayListBuilder* aBuilder,
                                                      nsIFrame* aFrame,
                                                      bool aLogAnimations)
{
  // Elements whose transform has been modified recently, or which
  // have a compositor-animated transform, can be prerendered. An element
  // might have only just had its transform animated in which case
  // the ActiveLayerManager may not have been notified yet.
  if (!ActiveLayerTracker::IsStyleAnimated(aFrame, eCSSProperty_transform) &&
      (!aFrame->GetContent() ||
       !nsLayoutUtils::HasAnimationsForCompositor(aFrame->GetContent(),
                                                  eCSSProperty_transform))) {
    if (aLogAnimations) {
      nsCString message;
      message.AppendLiteral("Performance warning: Async animation disabled because frame was not marked active for transform animation");
      ElementAnimationCollection::LogAsyncAnimationFailure(message,
                                                           aFrame->GetContent());
    }
    return false;
  }

  nsSize refSize = aBuilder->RootReferenceFrame()->GetSize();
  // Only prerender if the transformed frame's size is <= the
  // reference frame size (~viewport), allowing a 1/8th fuzz factor
  // for shadows, borders, etc.
  refSize += nsSize(refSize.width / 8, refSize.height / 8);
  nsSize frameSize = aFrame->GetVisualOverflowRectRelativeToSelf().Size();
  if (frameSize <= refSize) {
    // Bug 717521 - pre-render max 4096 x 4096 device pixels.
    nscoord max = aFrame->PresContext()->DevPixelsToAppUnits(4096);
    nsRect visual = aFrame->GetVisualOverflowRect();
    if (visual.width <= max && visual.height <= max) {
      return true;
    }
  }

  if (aLogAnimations) {
    nsCString message;
    message.AppendLiteral("Performance warning: Async animation disabled because frame size (");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(frameSize.width));
    message.AppendLiteral(", ");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(frameSize.height));
    message.AppendLiteral(") is bigger than the viewport (");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(refSize.width));
    message.AppendLiteral(", ");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(refSize.height));
    message.Append(')');
    ElementAnimationCollection::LogAsyncAnimationFailure(message,
                                                         aFrame->GetContent());
  }
  return false;
}

/* If the matrix is singular, or a hidden backface is shown, the frame won't be visible or hit. */
static bool IsFrameVisible(nsIFrame* aFrame, const gfx3DMatrix& aMatrix)
{
  if (aMatrix.IsSingular()) {
    return false;
  }
  if (aFrame->StyleDisplay()->mBackfaceVisibility == NS_STYLE_BACKFACE_VISIBILITY_HIDDEN &&
      aMatrix.IsBackfaceVisible()) {
    return false;
  }
  return true;
}

const gfx3DMatrix&
nsDisplayTransform::GetTransform()
{
  if (mTransform.IsIdentity()) {
    float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
    gfxPoint3D newOrigin =
      gfxPoint3D(NSAppUnitsToFloatPixels(mToReferenceFrame.x, scale),
                 NSAppUnitsToFloatPixels(mToReferenceFrame.y, scale),
                  0.0f);
    if (mTransformGetter) {
      mTransform = mTransformGetter(mFrame, scale);
      mTransform = nsLayoutUtils::ChangeMatrixBasis(newOrigin, mTransform);
    } else {
      /**
       * Passing true as the final argument means that we want to shift the
       * coordinates to be relative to our reference frame instead of relative
       * to this frame.
       * When we have preserve-3d, our reference frame is already guaranteed
       * to be an ancestor of the preserve-3d chain, so we only need to do
       * this once.
       */
      mTransform =
        GetResultingTransformMatrix(mFrame, ToReferenceFrame(), scale,
                                    nullptr, nullptr, true);
    }
  }
  return mTransform;
}

bool
nsDisplayTransform::ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder)
{
  return ShouldPrerenderTransformedContent(aBuilder, mFrame, false);
}

already_AddRefed<Layer> nsDisplayTransform::BuildLayer(nsDisplayListBuilder *aBuilder,
                                                       LayerManager *aManager,
                                                       const ContainerLayerParameters& aContainerParameters)
{
  const gfx3DMatrix& newTransformMatrix = GetTransform();

  if (mFrame->StyleDisplay()->mBackfaceVisibility == NS_STYLE_BACKFACE_VISIBILITY_HIDDEN &&
      newTransformMatrix.IsBackfaceVisible()) {
    return nullptr;
  }

  uint32_t flags = ShouldPrerenderTransformedContent(aBuilder, mFrame, false) ?
    FrameLayerBuilder::CONTAINER_NOT_CLIPPED_BY_ANCESTORS : 0;
  nsRefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, *mStoredList.GetChildren(),
                           aContainerParameters, &newTransformMatrix, flags);

  if (!container) {
    return nullptr;
  }

  // Add the preserve-3d flag for this layer, BuildContainerLayerFor clears all flags,
  // so we never need to explicitely unset this flag.
  if (mFrame->Preserves3D() || mFrame->Preserves3DChildren()) {
    container->SetContentFlags(container->GetContentFlags() | Layer::CONTENT_PRESERVE_3D);
  } else {
    container->SetContentFlags(container->GetContentFlags() & ~Layer::CONTENT_PRESERVE_3D);
  }

  nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(container, aBuilder,
                                                           this, mFrame,
                                                           eCSSProperty_transform);
  if (ShouldPrerenderTransformedContent(aBuilder, mFrame, false)) {
    container->SetUserData(nsIFrame::LayerIsPrerenderedDataKey(),
                           /*the value is irrelevant*/nullptr);
    container->SetContentFlags(container->GetContentFlags() | Layer::CONTENT_MAY_CHANGE_TRANSFORM);
  } else {
    container->RemoveUserData(nsIFrame::LayerIsPrerenderedDataKey());
    container->SetContentFlags(container->GetContentFlags() & ~Layer::CONTENT_MAY_CHANGE_TRANSFORM);
  }
  return container.forget();
}

nsDisplayItem::LayerState
nsDisplayTransform::GetLayerState(nsDisplayListBuilder* aBuilder,
                                  LayerManager* aManager,
                                  const ContainerLayerParameters& aParameters) {
  // If the transform is 3d, or the layer takes part in preserve-3d sorting
  // then we *always* want this to be an active layer.
  if (!GetTransform().Is2D() || mFrame->Preserves3D()) {
    return LAYER_ACTIVE_FORCE;
  }
  // Here we check if the *post-transform* bounds of this item are big enough
  // to justify an active layer.
  if (ActiveLayerTracker::IsStyleAnimated(mFrame, eCSSProperty_transform) &&
      !IsItemTooSmallForActiveLayer(this))
    return LAYER_ACTIVE;
  if (mFrame->GetContent()) {
    if (nsLayoutUtils::HasAnimationsForCompositor(mFrame->GetContent(),
                                                  eCSSProperty_transform)) {
      return LAYER_ACTIVE;
    }
  }

  const nsStyleDisplay* disp = mFrame->StyleDisplay();
  if ((disp->mWillChangeBitField & NS_STYLE_WILL_CHANGE_TRANSFORM)) {
    return LAYER_ACTIVE;
  }

  // Expect the child display items to have this frame as their animated
  // geometry root (since it will be their reference frame). If they have a
  // different animated geometry root, we'll make this an active layer so the
  // animation can be accelerated.
  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters,
    *mStoredList.GetChildren(), Frame());
}

bool nsDisplayTransform::ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                             nsRegion *aVisibleRegion,
                                             const nsRect& aAllowVisibleRegionExpansion)
{
  /* As we do this, we need to be sure to
   * untransform the visible rect, since we want everything that's painting to
   * think that it's painting in its original rectangular coordinate space.
   * If we can't untransform, take the entire overflow rect */
  nsRect untransformedVisibleRect;
  if (ShouldPrerenderTransformedContent(aBuilder, mFrame) ||
      !UntransformVisibleRect(aBuilder, &untransformedVisibleRect))
  {
    untransformedVisibleRect = mFrame->GetVisualOverflowRectRelativeToSelf();
  }
  nsRegion untransformedVisible = untransformedVisibleRect;
  // Call RecomputeVisiblity instead of ComputeVisibility since
  // nsDisplayItem::ComputeVisibility should only be called from
  // nsDisplayList::ComputeVisibility (which sets mVisibleRect on the item)
  mStoredList.RecomputeVisibility(aBuilder, &untransformedVisible);
  return true;
}

#ifdef DEBUG_HIT
#include <time.h>
#endif

/* HitTest does some fun stuff with matrix transforms to obtain the answer. */
void nsDisplayTransform::HitTest(nsDisplayListBuilder *aBuilder,
                                 const nsRect& aRect,
                                 HitTestState *aState,
                                 nsTArray<nsIFrame*> *aOutFrames)
{
  /* Here's how this works:
   * 1. Get the matrix.  If it's singular, abort (clearly we didn't hit
   *    anything).
   * 2. Invert the matrix.
   * 3. Use it to transform the rect into the correct space.
   * 4. Pass that rect down through to the list's version of HitTest.
   */
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  gfx3DMatrix matrix = GetTransform();

  if (!IsFrameVisible(mFrame, matrix)) {
    return;
  }

  /* We want to go from transformed-space to regular space.
   * Thus we have to invert the matrix, which normally does
   * the reverse operation (e.g. regular->transformed)
   */

  /* Now, apply the transform and pass it down the channel. */
  nsRect resultingRect;
  if (aRect.width == 1 && aRect.height == 1) {
    // Magic width/height indicating we're hit testing a point, not a rect
    gfxPointH3D point = matrix.Inverse().ProjectPoint(gfxPoint(NSAppUnitsToFloatPixels(aRect.x, factor),
                                                               NSAppUnitsToFloatPixels(aRect.y, factor)));
    if (!point.HasPositiveWCoord()) {
      return;
    }

    gfxPoint point2d = point.As2DPoint();

    resultingRect = nsRect(NSFloatPixelsToAppUnits(float(point2d.x), factor),
                           NSFloatPixelsToAppUnits(float(point2d.y), factor),
                           1, 1);

  } else {
    gfxRect originalRect(NSAppUnitsToFloatPixels(aRect.x, factor),
                         NSAppUnitsToFloatPixels(aRect.y, factor),
                         NSAppUnitsToFloatPixels(aRect.width, factor),
                         NSAppUnitsToFloatPixels(aRect.height, factor));

    gfxRect rect = matrix.Inverse().ProjectRectBounds(originalRect);

    bool snap;
    nsRect childBounds = mStoredList.GetBounds(aBuilder, &snap);
    gfxRect childGfxBounds(NSAppUnitsToFloatPixels(childBounds.x, factor),
                           NSAppUnitsToFloatPixels(childBounds.y, factor),
                           NSAppUnitsToFloatPixels(childBounds.width, factor),
                           NSAppUnitsToFloatPixels(childBounds.height, factor));
    rect = rect.Intersect(childGfxBounds);

    resultingRect = nsRect(NSFloatPixelsToAppUnits(float(rect.X()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Y()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Width()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Height()), factor));
  }

  if (resultingRect.IsEmpty()) {
    return;
  }


#ifdef DEBUG_HIT
  printf("Frame: %p\n", dynamic_cast<void *>(mFrame));
  printf("  Untransformed point: (%f, %f)\n", resultingRect.X(), resultingRect.Y());
  uint32_t originalFrameCount = aOutFrames.Length();
#endif

  mStoredList.HitTest(aBuilder, resultingRect, aState, aOutFrames);

#ifdef DEBUG_HIT
  if (originalFrameCount != aOutFrames.Length())
    printf("  Hit! Time: %f, first frame: %p\n", static_cast<double>(clock()),
           dynamic_cast<void *>(aOutFrames.ElementAt(0)));
  printf("=== end of hit test ===\n");
#endif

}

float
nsDisplayTransform::GetHitDepthAtPoint(nsDisplayListBuilder* aBuilder, const nsPoint& aPoint)
{
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  gfx3DMatrix matrix = GetTransform();

  NS_ASSERTION(IsFrameVisible(mFrame, matrix), "We can't have hit a frame that isn't visible!");

  gfxPointH3D point = matrix.Inverse().ProjectPoint(gfxPoint(NSAppUnitsToFloatPixels(aPoint.x, factor),
                                                             NSAppUnitsToFloatPixels(aPoint.y, factor)));
  NS_ASSERTION(point.HasPositiveWCoord(), "Why are we trying to get the depth for a point we didn't hit?");

  gfxPoint point2d = point.As2DPoint();

  gfxPoint3D transformed = matrix.Transform3D(gfxPoint3D(point2d.x, point2d.y, 0));
  return transformed.z;
}

/* The bounding rectangle for the object is the overflow rectangle translated
 * by the reference point.
 */
nsRect nsDisplayTransform::GetBounds(nsDisplayListBuilder *aBuilder, bool* aSnap)
{
  nsRect untransformedBounds =
    ShouldPrerenderTransformedContent(aBuilder, mFrame) ?
    mFrame->GetVisualOverflowRectRelativeToSelf() :
    mStoredList.GetBounds(aBuilder, aSnap);
  *aSnap = false;
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  return nsLayoutUtils::MatrixTransformRect(untransformedBounds,
                                            GetTransform(),
                                            factor);
}

/* The transform is opaque iff the transform consists solely of scales and
 * translations and if the underlying content is opaque.  Thus if the transform
 * is of the form
 *
 * |a c e|
 * |b d f|
 * |0 0 1|
 *
 * We need b and c to be zero.
 *
 * We also need to check whether the underlying opaque content completely fills
 * our visible rect. We use UntransformRect which expands to the axis-aligned
 * bounding rect, but that's OK since if
 * mStoredList.GetVisibleRect().Contains(untransformedVisible), then it
 * certainly contains the actual (non-axis-aligned) untransformed rect.
 */
nsRegion nsDisplayTransform::GetOpaqueRegion(nsDisplayListBuilder *aBuilder,
                                             bool* aSnap)
{
  *aSnap = false;
  nsRect untransformedVisible;
  // If we're going to prerender all our content, pretend like we
  // don't have opqaue content so that everything under us is rendered
  // as well.  That will increase graphics memory usage if our frame
  // covers the entire window, but it allows our transform to be
  // updated extremely cheaply, without invalidating any other
  // content.
  if (ShouldPrerenderTransformedContent(aBuilder, mFrame) ||
      !UntransformVisibleRect(aBuilder, &untransformedVisible)) {
      return nsRegion();
  }

  const gfx3DMatrix& matrix = GetTransform();

  nsRegion result;
  gfxMatrix matrix2d;
  bool tmpSnap;
  if (matrix.Is2D(&matrix2d) &&
      matrix2d.PreservesAxisAlignedRectangles() &&
      mStoredList.GetOpaqueRegion(aBuilder, &tmpSnap).Contains(untransformedVisible)) {
    result = mVisibleRect;
  }
  return result;
}

/* The transform is uniform if it fills the entire bounding rect and the
 * wrapped list is uniform.  See GetOpaqueRegion for discussion of why this
 * works.
 */
bool nsDisplayTransform::IsUniform(nsDisplayListBuilder *aBuilder, nscolor* aColor)
{
  nsRect untransformedVisible;
  if (!UntransformVisibleRect(aBuilder, &untransformedVisible)) {
    return false;
  }
  const gfx3DMatrix& matrix = GetTransform();

  gfxMatrix matrix2d;
  return matrix.Is2D(&matrix2d) &&
         matrix2d.PreservesAxisAlignedRectangles() &&
         mStoredList.GetVisibleRect().Contains(untransformedVisible) &&
         mStoredList.IsUniform(aBuilder, aColor);
}

/* If UNIFIED_CONTINUATIONS is defined, we can merge two display lists that
 * share the same underlying content.  Otherwise, doing so results in graphical
 * glitches.
 */
#ifndef UNIFIED_CONTINUATIONS

bool
nsDisplayTransform::TryMerge(nsDisplayListBuilder *aBuilder,
                             nsDisplayItem *aItem)
{
  return false;
}

#else

bool
nsDisplayTransform::TryMerge(nsDisplayListBuilder *aBuilder,
                             nsDisplayItem *aItem)
{
  NS_PRECONDITION(aItem, "Why did you try merging with a null item?");
  NS_PRECONDITION(aBuilder, "Why did you try merging with a null builder?");

  /* Make sure that we're dealing with two transforms. */
  if (aItem->GetType() != TYPE_TRANSFORM)
    return false;

  /* Check to see that both frames are part of the same content. */
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;

  if (aItem->GetClip() != GetClip())
    return false;

  /* Now, move everything over to this frame and signal that
   * we merged things!
   */
  mStoredList.MergeFrom(&static_cast<nsDisplayTransform*>(aItem)->mStoredList);
  return true;
}

#endif

/* TransformRect takes in as parameters a rectangle (in app space) and returns
 * the smallest rectangle (in app space) containing the transformed image of
 * that rectangle.  That is, it takes the four corners of the rectangle,
 * transforms them according to the matrix associated with the specified frame,
 * then returns the smallest rectangle containing the four transformed points.
 *
 * @param aUntransformedBounds The rectangle (in app units) to transform.
 * @param aFrame The frame whose transformation should be applied.
 * @param aOrigin The delta from the frame origin to the coordinate space origin
 * @param aBoundsOverride (optional) Force the frame bounds to be the
 *        specified bounds.
 * @return The smallest rectangle containing the image of the transformed
 *         rectangle.
 */
nsRect nsDisplayTransform::TransformRect(const nsRect &aUntransformedBounds,
                                         const nsIFrame* aFrame,
                                         const nsPoint &aOrigin,
                                         const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();
  return nsLayoutUtils::MatrixTransformRect
    (aUntransformedBounds,
     GetResultingTransformMatrix(aFrame, aOrigin, factor, aBoundsOverride),
     factor);
}

nsRect nsDisplayTransform::TransformRectOut(const nsRect &aUntransformedBounds,
                                            const nsIFrame* aFrame,
                                            const nsPoint &aOrigin,
                                            const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();
  return nsLayoutUtils::MatrixTransformRectOut
    (aUntransformedBounds,
     GetResultingTransformMatrix(aFrame, aOrigin, factor, aBoundsOverride),
     factor);
}

bool nsDisplayTransform::UntransformRect(const nsRect &aTransformedBounds,
                                         const nsRect &aChildBounds,
                                         const nsIFrame* aFrame,
                                         const nsPoint &aOrigin,
                                         nsRect *aOutRect)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();

  gfx3DMatrix transform = GetResultingTransformMatrix(aFrame, aOrigin, factor, nullptr);
  if (transform.IsSingular()) {
    return false;
  }

  gfxRect result(NSAppUnitsToFloatPixels(aTransformedBounds.x, factor),
                 NSAppUnitsToFloatPixels(aTransformedBounds.y, factor),
                 NSAppUnitsToFloatPixels(aTransformedBounds.width, factor),
                 NSAppUnitsToFloatPixels(aTransformedBounds.height, factor));

  gfxRect childGfxBounds(NSAppUnitsToFloatPixels(aChildBounds.x, factor),
                         NSAppUnitsToFloatPixels(aChildBounds.y, factor),
                         NSAppUnitsToFloatPixels(aChildBounds.width, factor),
                         NSAppUnitsToFloatPixels(aChildBounds.height, factor));

  result = transform.Inverse().ProjectRectBounds(result);
  result = result.Intersect(childGfxBounds);
  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(result, factor);
  return true;
}

bool nsDisplayTransform::UntransformVisibleRect(nsDisplayListBuilder* aBuilder,
                                                nsRect *aOutRect)
{
  const gfx3DMatrix& matrix = GetTransform();
  if (matrix.IsSingular())
    return false;

  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  gfxRect result(NSAppUnitsToFloatPixels(mVisibleRect.x, factor),
                 NSAppUnitsToFloatPixels(mVisibleRect.y, factor),
                 NSAppUnitsToFloatPixels(mVisibleRect.width, factor),
                 NSAppUnitsToFloatPixels(mVisibleRect.height, factor));

  bool snap;
  nsRect childBounds = mStoredList.GetBounds(aBuilder, &snap);
  gfxRect childGfxBounds(NSAppUnitsToFloatPixels(childBounds.x, factor),
                         NSAppUnitsToFloatPixels(childBounds.y, factor),
                         NSAppUnitsToFloatPixels(childBounds.width, factor),
                         NSAppUnitsToFloatPixels(childBounds.height, factor));

  /* We want to untransform the matrix, so invert the transformation first! */
  result = matrix.Inverse().ProjectRectBounds(result);
  result = result.Intersect(childGfxBounds);

  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(result, factor);

  return true;
}

#ifdef MOZ_DUMP_PAINTING
void
nsDisplayTransform::WriteDebugInfo(nsACString& aTo)
{
  gfx::Matrix4x4 transform;
  gfx::ToMatrix4x4(GetTransform(), transform);
  std::stringstream ss;
  AppendToString(ss, transform);
  aTo += ss.str().c_str();
}
#endif

nsDisplaySVGEffects::nsDisplaySVGEffects(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList),
      mEffectsBounds(aFrame->GetVisualOverflowRectRelativeToSelf())
{
  MOZ_COUNT_CTOR(nsDisplaySVGEffects);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplaySVGEffects::~nsDisplaySVGEffects()
{
  MOZ_COUNT_DTOR(nsDisplaySVGEffects);
}
#endif

nsRegion nsDisplaySVGEffects::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                              bool* aSnap)
{
  *aSnap = false;
  return nsRegion();
}

void
nsDisplaySVGEffects::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                             HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  nsPoint rectCenter(aRect.x + aRect.width / 2, aRect.y + aRect.height / 2);
  if (nsSVGIntegrationUtils::HitTestFrameForEffects(mFrame,
      rectCenter - ToReferenceFrame())) {
    mList.HitTest(aBuilder, aRect, aState, aOutFrames);
  }
}

void
nsDisplaySVGEffects::PaintAsLayer(nsDisplayListBuilder* aBuilder,
                                  nsRenderingContext* aCtx,
                                  LayerManager* aManager)
{
  nsSVGIntegrationUtils::PaintFramesWithEffects(aCtx, mFrame,
                                                mVisibleRect,
                                                aBuilder, aManager);
}

LayerState
nsDisplaySVGEffects::GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters)
{
  return LAYER_SVG_EFFECTS;
}

already_AddRefed<Layer>
nsDisplaySVGEffects::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aContainerParameters)
{
  const nsIContent* content = mFrame->GetContent();
  bool hasSVGLayout = (mFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  if (hasSVGLayout) {
    nsISVGChildFrame *svgChildFrame = do_QueryFrame(mFrame);
    if (!svgChildFrame || !mFrame->GetContent()->IsSVG()) {
      NS_ASSERTION(false, "why?");
      return nullptr;
    }
    if (!static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
      return nullptr; // The SVG spec says not to draw filters for this
    }
  }

  float opacity = mFrame->StyleDisplay()->mOpacity;
  if (opacity == 0.0f)
    return nullptr;

  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  bool isOK = effectProperties.HasNoFilterOrHasValidFilter();
  effectProperties.GetClipPathFrame(&isOK);
  effectProperties.GetMaskFrame(&isOK);

  if (!isOK) {
    return nullptr;
  }

  ContainerLayerParameters newContainerParameters = aContainerParameters;
  if (effectProperties.HasValidFilter()) {
    newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;
  }

  nsRefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           newContainerParameters, nullptr);

  return container.forget();
}

bool nsDisplaySVGEffects::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                              nsRegion* aVisibleRegion,
                                              const nsRect& aAllowVisibleRegionExpansion) {
  nsPoint offset = ToReferenceFrame();
  nsRect dirtyRect =
    nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(mFrame,
                                                           mVisibleRect - offset) +
    offset;

  // Our children may be made translucent or arbitrarily deformed so we should
  // not allow them to subtract area from aVisibleRegion.
  nsRegion childrenVisible(dirtyRect);
  nsRect r = dirtyRect.Intersect(mList.GetBounds(aBuilder));
  mList.ComputeVisibilityForSublist(aBuilder, &childrenVisible, r, nsRect());
  return true;
}

bool nsDisplaySVGEffects::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem)
{
  if (aItem->GetType() != TYPE_SVG_EFFECTS)
    return false;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplaySVGEffects
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  nsDisplaySVGEffects* other = static_cast<nsDisplaySVGEffects*>(aItem);
  MergeFromTrackingMergedFrames(other);
  mEffectsBounds.UnionRect(mEffectsBounds,
    other->mEffectsBounds + other->mFrame->GetOffsetTo(mFrame));
  return true;
}

#ifdef MOZ_DUMP_PAINTING
void
nsDisplaySVGEffects::PrintEffects(nsACString& aTo)
{
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);
  bool isOK = true;
  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame(&isOK);
  bool first = true;
  aTo += " effects=(";
  if (mFrame->StyleDisplay()->mOpacity != 1.0f) {
    first = false;
    aTo += nsPrintfCString("opacity(%f)", mFrame->StyleDisplay()->mOpacity);
  }
  if (clipPathFrame) {
    if (!first) {
      aTo += ", ";
    }
    aTo += nsPrintfCString("clip(%s)", clipPathFrame->IsTrivial() ? "trivial" : "non-trivial");
    first = false;
  }
  if (effectProperties.HasValidFilter()) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "filter";
    first = false;
  }
  if (effectProperties.GetMaskFrame(&isOK)) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "mask";
  }
  aTo += ")";
}
#endif

