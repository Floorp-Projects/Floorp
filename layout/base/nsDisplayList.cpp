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

#include "mozilla/layers/PLayers.h"

#include "nsDisplayList.h"

#include "nsCSSRendering.h"
#include "nsRenderingContext.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsRegion.h"
#include "nsFrameManager.h"
#include "gfxContext.h"
#include "nsStyleStructInlines.h"
#include "nsStyleTransformMatrix.h"
#include "gfxMatrix.h"
#include "nsSVGIntegrationUtils.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"
#include "nsThemeConstants.h"

#include "imgIContainer.h"
#include "nsIInterfaceRequestorUtils.h"
#include "BasicLayers.h"
#include "nsBoxFrame.h"
#include "nsViewportFrame.h"
#include "nsSVGEffects.h"
#include "nsSVGElement.h"
#include "nsSVGClipPathFrame.h"
#include "sampler.h"
#include "nsAnimationManager.h"
#include "nsIViewManager.h"

#include "mozilla/StandardInteger.h"

using namespace mozilla;
using namespace mozilla::layers;
typedef FrameMetrics::ViewID ViewID;

static void AddTransformFunctions(nsCSSValueList* aList,
                                  nsStyleContext* aContext,
                                  nsPresContext* aPresContext,
                                  nsRect& aBounds,
                                  float aAppUnitsPerPixel,
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
          aBounds.Width(), aAppUnitsPerPixel);
        aFunctions.AppendElement(Translation(x, 0, 0));
        break;
      }
      case eCSSKeyword_translatey:
      {
        double y = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, canStoreInRuleTree,
          aBounds.Height(), aAppUnitsPerPixel);
        aFunctions.AppendElement(Translation(0, y, 0));
        break;
      }
      case eCSSKeyword_translatez:
      {
        double z = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, canStoreInRuleTree,
          0, aAppUnitsPerPixel);
        aFunctions.AppendElement(Translation(0, 0, z));
        break;
      }
      case eCSSKeyword_translate:
      {
        double x = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, canStoreInRuleTree,
          aBounds.Width(), aAppUnitsPerPixel);
        // translate(x) is shorthand for translate(x, 0)
        double y = 0;
        if (array->Count() == 3) {
           y = nsStyleTransformMatrix::ProcessTranslatePart(
            array->Item(2), aContext, aPresContext, canStoreInRuleTree,
            aBounds.Height(), aAppUnitsPerPixel);
        }
        aFunctions.AppendElement(Translation(x, y, 0));
        break;
      }
      case eCSSKeyword_translate3d:
      {
        double x = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, canStoreInRuleTree,
          aBounds.Width(), aAppUnitsPerPixel);
        double y = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(2), aContext, aPresContext, canStoreInRuleTree,
          aBounds.Height(), aAppUnitsPerPixel);
        double z = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(3), aContext, aPresContext, canStoreInRuleTree,
          0, aAppUnitsPerPixel);

        aFunctions.AppendElement(Translation(x, y, z));
        break;
      }
      case eCSSKeyword_skewx:
      {
        double x = array->Item(1).GetFloatValue();
        aFunctions.AppendElement(SkewX(x));
        break;
      }
      case eCSSKeyword_skewy:
      {
        double y = array->Item(1).GetFloatValue();
        aFunctions.AppendElement(SkewY(y));
        break;
      }
      case eCSSKeyword_matrix:
      {
        gfx3DMatrix matrix;
        matrix._11 = array->Item(1).GetFloatValue();
        matrix._12 = array->Item(2).GetFloatValue();
        matrix._13 = 0;
        matrix._14 = array->Item(3).GetFloatValue();
        matrix._21 = array->Item(4).GetFloatValue();
        matrix._22 = array->Item(5).GetFloatValue();
        matrix._23 = 0;
        matrix._24 = array->Item(6).GetFloatValue();
        matrix._31 = 0;
        matrix._32 = 0;
        matrix._33 = 1;
        matrix._34 = 0;
        matrix._41 = 0;
        matrix._42 = 0;
        matrix._43 = 0;
        matrix._44 = 1;
        aFunctions.AppendElement(TransformMatrix(matrix));
        break;
      }
      case eCSSKeyword_matrix3d:
      {
        gfx3DMatrix matrix;
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

  PRUint32 type = aCTF.GetType() == nsTimingFunction::StepStart ? 1 : 2;
  return TimingFunction(StepFunction(aCTF.GetSteps(), type));
}

static void
AddTransformAnimations(ElementAnimations* ea, Layer* aLayer,
                       const nsPoint& aOrigin)
{
  if (!ea)
    return;
  NS_ASSERTION(aLayer->AsContainerLayer(), "Should only animate ContainerLayer");
  nsIFrame* frame = ea->mElement->GetPrimaryFrame();
  nsRect bounds = nsDisplayTransform::GetFrameBoundsForTransform(frame);
  float scale = nsDeviceContext::AppUnitsPerCSSPixel();
  gfxPoint3D offsetToTransformOrigin =
    nsDisplayTransform::GetDeltaToMozTransformOrigin(frame, scale, &bounds);
  gfxPoint3D offsetToPerspectiveOrigin =
    nsDisplayTransform::GetDeltaToMozPerspectiveOrigin(frame, scale);
  nscoord perspective = 0.0;
  nsStyleContext* parentStyleContext = frame->GetStyleContext()->GetParent();
  if (parentStyleContext) {
    const nsStyleDisplay* disp = parentStyleContext->GetStyleDisplay();
    if (disp && disp->mChildPerspective.GetUnit() == eStyleUnit_Coord) {
      perspective = disp->mChildPerspective.GetCoordValue();
    }
  }

  for (PRUint32 animIdx = 0; animIdx < ea->mAnimations.Length(); animIdx++) {
    ElementAnimation* anim = &ea->mAnimations[animIdx];
    if (!anim->CanPerformOnCompositor(ea->mElement, TimeStamp::Now())) {
      continue;
    }
    float iterations = anim->mIterationCount != NS_IEEEPositiveInfinity()
                         ? anim->mIterationCount : -1;
    for (PRUint32 propIdx = 0; propIdx < anim->mProperties.Length(); propIdx++) {
      AnimationProperty* property = &anim->mProperties[propIdx];
      InfallibleTArray<AnimationSegment> segments;

      if (property->mProperty != eCSSProperty_transform) {
        continue;
      }

      for (PRUint32 segIdx = 0; segIdx < property->mSegments.Length(); segIdx++) {
        AnimationPropertySegment* segment = &property->mSegments[segIdx];
        nsCSSValueList* list = segment->mFromValue.GetCSSValueListValue();
        InfallibleTArray<TransformFunction> fromFunctions;
        AddTransformFunctions(list, frame->GetStyleContext(),
                              frame->PresContext(), bounds,
                              scale, fromFunctions);

        list = segment->mToValue.GetCSSValueListValue();
        InfallibleTArray<TransformFunction> toFunctions;
        AddTransformFunctions(list, frame->GetStyleContext(),
                              frame->PresContext(), bounds,
                              scale, toFunctions);

        segments.AppendElement(AnimationSegment(fromFunctions, toFunctions,
                                                segment->mFromKey, segment->mToKey,
                                                ToTimingFunction(segment->mTimingFunction)));
      }

      if (segments.Length() == 0) {
        continue;
      }
      aLayer->AddAnimation(Animation(anim->mStartTime,
                                     anim->mIterationDuration,
                                     segments,
                                     iterations,
                                     anim->mDirection,
                                     TransformData(aOrigin, offsetToTransformOrigin,
                                                   offsetToPerspectiveOrigin,
                                                   bounds, perspective)));
     }
  }
}

static void
AddOpacityAnimations(ElementAnimations* ea, Layer* aLayer)
{
  if (!ea)
    return;
  NS_ASSERTION(aLayer->AsContainerLayer(), "Should only animate ContainerLayer");
  for (PRUint32 animIdx = 0; animIdx < ea->mAnimations.Length(); animIdx++) {
    ElementAnimation* anim = &ea->mAnimations[animIdx];
    float iterations = anim->mIterationCount != NS_IEEEPositiveInfinity()
                         ? anim->mIterationCount : -1;
    if (!anim->CanPerformOnCompositor(ea->mElement, TimeStamp::Now())) {
      continue;
    }
    for (PRUint32 propIdx = 0; propIdx < anim->mProperties.Length(); propIdx++) {
      AnimationProperty* property = &anim->mProperties[propIdx];
      InfallibleTArray<AnimationSegment> segments;
      if (property->mProperty != eCSSProperty_opacity) {
        continue;
      }

      for (PRUint32 segIdx = 0; segIdx < property->mSegments.Length(); segIdx++) {
        AnimationPropertySegment* segment = &property->mSegments[segIdx];
        segments.AppendElement(AnimationSegment(Opacity(segment->mFromValue.GetFloatValue()),
                                                Opacity(segment->mToValue.GetFloatValue()),
                                                segment->mFromKey,
                                                segment->mToKey,
                                                ToTimingFunction(segment->mTimingFunction)));
      }

      aLayer->AddAnimation(Animation(anim->mStartTime,
                                     anim->mIterationDuration,
                                     segments,
                                     iterations,
                                     anim->mDirection,
                                     null_t()));
     }
  }
}

nsDisplayListBuilder::nsDisplayListBuilder(nsIFrame* aReferenceFrame,
    Mode aMode, bool aBuildCaret)
    : mReferenceFrame(aReferenceFrame),
      mIgnoreScrollFrame(nsnull),
      mCurrentTableItem(nsnull),
      mFinalTransparentRegion(nsnull),
      mCachedOffsetFrame(aReferenceFrame),
      mCachedOffset(0, 0),
      mGlassDisplayItem(nsnull),
      mMode(aMode),
      mBuildCaret(aBuildCaret),
      mIgnoreSuppression(false),
      mHadToIgnoreSuppression(false),
      mIsAtRootOfPseudoStackingContext(false),
      mIncludeAllOutOfFlows(false),
      mSelectedFramesOnly(false),
      mAccurateVisibleRegions(false),
      mInTransform(false),
      mSyncDecodeImages(false),
      mIsPaintingToWindow(false),
      mHasDisplayPort(false),
      mHasFixedItems(false),
      mIsInFixedPosition(false),
      mIsCompositingCheap(false)
{
  MOZ_COUNT_CTOR(nsDisplayListBuilder);
  PL_InitArenaPool(&mPool, "displayListArena", 1024,
                   NS_MAX(NS_ALIGNMENT_OF(void*),NS_ALIGNMENT_OF(double))-1);

  nsPresContext* pc = aReferenceFrame->PresContext();
  nsIPresShell *shell = pc->PresShell();
  if (pc->IsRenderingOnlySelection()) {
    nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(shell));
    if (selcon) {
      selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                           getter_AddRefs(mBoundingSelection));
    }
  }

  if(mReferenceFrame->GetType() == nsGkAtoms::viewportFrame) {
    ViewportFrame* viewportFrame = static_cast<ViewportFrame*>(mReferenceFrame);
    if (!viewportFrame->GetChildList(nsIFrame::kFixedList).IsEmpty()) {
      mHasFixedItems = true;
    }
  }

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

static bool IsFixedFrame(nsIFrame* aFrame)
{
  return aFrame && aFrame->GetParent() && !aFrame->GetParent()->GetParent();
}

static bool IsFixedItem(nsDisplayItem *aItem, nsDisplayListBuilder* aBuilder)
{
  nsIFrame* activeScrolledRoot =
    nsLayoutUtils::GetActiveScrolledRootFor(aItem, aBuilder);
  return activeScrolledRoot &&
    !nsLayoutUtils::IsScrolledByRootContentDocumentDisplayportScrolling(activeScrolledRoot, aBuilder);
}

static bool ForceVisiblityForFixedItem(nsDisplayListBuilder* aBuilder,
                                       nsDisplayItem* aItem)
{
  return aBuilder->GetDisplayPort() && aBuilder->GetHasFixedItems() &&
         IsFixedItem(aItem, aBuilder);
}

void nsDisplayListBuilder::SetDisplayPort(const nsRect& aDisplayPort)
{
    static bool fixedPositionLayersEnabled = getenv("MOZ_ENABLE_FIXED_POSITION_LAYERS") != 0;
    if (fixedPositionLayersEnabled) {
      mHasDisplayPort = true;
      mDisplayPort = aDisplayPort;
    }
}

void nsDisplayListBuilder::MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame,
                                                        nsIFrame* aFrame,
                                                        const nsRect& aDirtyRect)
{
  nsRect dirty = aDirtyRect - aFrame->GetOffsetTo(aDirtyFrame);
  nsRect overflowRect = aFrame->GetVisualOverflowRect();

  if (mHasDisplayPort && IsFixedFrame(aFrame)) {
    dirty = overflowRect;
  }

  if (!dirty.IntersectRect(dirty, overflowRect))
    return;
  aFrame->Properties().Set(nsDisplayListBuilder::OutOfFlowDirtyRectProperty(),
                           new nsRect(dirty));

  MarkFrameForDisplay(aFrame, aDirtyFrame);
}

static void UnmarkFrameForDisplay(nsIFrame* aFrame) {
  nsPresContext* presContext = aFrame->PresContext();
  presContext->PropertyTable()->
    Delete(aFrame, nsDisplayListBuilder::OutOfFlowDirtyRectProperty());

  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderFor(f)) {
    if (!(f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO))
      return;
    f->RemoveStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
  }
}

static void RecordFrameMetrics(nsIFrame* aForFrame,
                               nsIFrame* aScrollFrame,
                               ContainerLayer* aRoot,
                               const nsRect& aVisibleRect,
                               const nsRect& aViewport,
                               nsRect* aDisplayPort,
                               ViewID aScrollId,
                               const nsDisplayItem::ContainerParameters& aContainerParameters) {
  nsPresContext* presContext = aForFrame->PresContext();
  PRInt32 auPerDevPixel = presContext->AppUnitsPerDevPixel();

  nsIntRect visible = aVisibleRect.ScaleToNearestPixels(
    aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);
  aRoot->SetVisibleRegion(nsIntRegion(visible));

  FrameMetrics metrics;

  metrics.mViewport = aViewport.ScaleToNearestPixels(
    aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);

  if (aDisplayPort) {
    metrics.mDisplayPort = aDisplayPort->ScaleToNearestPixels(
      aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);
  }

  nsIScrollableFrame* scrollableFrame = nsnull;
  if (aScrollFrame)
    scrollableFrame = aScrollFrame->GetScrollTargetFrame();

  if (scrollableFrame) {
    nsRect contentBounds = scrollableFrame->GetScrollRange();
    contentBounds.width += scrollableFrame->GetScrollPortRect().width;
    contentBounds.height += scrollableFrame->GetScrollPortRect().height;
    metrics.mCSSContentRect =
      mozilla::gfx::Rect(nsPresContext::AppUnitsToFloatCSSPixels(contentBounds.x),
                         nsPresContext::AppUnitsToFloatCSSPixels(contentBounds.y),
                         nsPresContext::AppUnitsToFloatCSSPixels(contentBounds.width),
                         nsPresContext::AppUnitsToFloatCSSPixels(contentBounds.height));
    metrics.mContentRect = contentBounds.ScaleToNearestPixels(
      aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);
    metrics.mViewportScrollOffset = scrollableFrame->GetScrollPosition().ScaleToNearestPixels(
      aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);
  }
  else {
    nsRect contentBounds = aForFrame->GetRect();
    metrics.mCSSContentRect =
      mozilla::gfx::Rect(nsPresContext::AppUnitsToFloatCSSPixels(contentBounds.x),
                         nsPresContext::AppUnitsToFloatCSSPixels(contentBounds.y),
                         nsPresContext::AppUnitsToFloatCSSPixels(contentBounds.width),
                         nsPresContext::AppUnitsToFloatCSSPixels(contentBounds.height));
    metrics.mContentRect = contentBounds.ScaleToNearestPixels(
      aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);
  }

  metrics.mScrollId = aScrollId;

  nsIPresShell* presShell = presContext->GetPresShell();
  metrics.mResolution = gfxSize(presShell->GetXResolution(), presShell->GetYResolution());

  aRoot->SetFrameMetrics(metrics);
}

nsDisplayListBuilder::~nsDisplayListBuilder() {
  NS_ASSERTION(mFramesMarkedForDisplay.Length() == 0,
               "All frames should have been unmarked");
  NS_ASSERTION(mPresShellStates.Length() == 0,
               "All presshells should have been exited");
  NS_ASSERTION(!mCurrentTableItem, "No table item should be active");

  PL_FreeArenaPool(&mPool);
  PL_FinishArenaPool(&mPool);
  MOZ_COUNT_DTOR(nsDisplayListBuilder);
}

PRUint32
nsDisplayListBuilder::GetBackgroundPaintFlags() {
  PRUint32 flags = 0;
  if (mSyncDecodeImages) {
    flags |= nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES;
  }
  if (mIsPaintingToWindow) {
    flags |= nsCSSRendering::PAINTBG_TO_WINDOW;
  }
  return flags;
}

static PRUint64 RegionArea(const nsRegion& aRegion)
{
  PRUint64 area = 0;
  nsRegionRectIterator iter(aRegion);
  const nsRect* r;
  while ((r = iter.Next()) != nsnull) {
    area += PRUint64(r->width)*r->height;
  }
  return area;
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
      RegionArea(tmp) <= RegionArea(*aVisibleRegion)/2) {
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
  state->mCaretFrame = nsnull;
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

  if (state->mCaretFrame) {
    // Check if the dirty rect intersects with the caret's dirty rect.
    nsRect caretRect =
      caret->GetCaretRect() + state->mCaretFrame->GetOffsetTo(aReferenceFrame);
    if (caretRect.Intersects(aDirtyRect)) {
      // Okay, our rects intersect, let's mark the frame and all of its ancestors.
      mFramesMarkedForDisplay.AppendElement(state->mCaretFrame);
      MarkFrameForDisplay(state->mCaretFrame, nsnull);
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

  // Unmark and pop off the frames marked for display in this pres shell.
  PRUint32 firstFrameForShell = CurrentPresShellState()->mFirstFrameMarkedForDisplay;
  for (PRUint32 i = firstFrameForShell;
       i < mFramesMarkedForDisplay.Length(); ++i) {
    UnmarkFrameForDisplay(mFramesMarkedForDisplay[i]);
  }
  mFramesMarkedForDisplay.SetLength(firstFrameForShell);
  mPresShellStates.SetLength(mPresShellStates.Length() - 1);
}

void
nsDisplayListBuilder::MarkFramesForDisplayList(nsIFrame* aDirtyFrame,
                                               const nsFrameList& aFrames,
                                               const nsRect& aDirtyRect) {
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
  return tmp;
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
  while ((item = RemoveBottom()) != nsnull) {
    if (item->GetType() == nsDisplayItem::TYPE_WRAP_LIST) {
      item->GetList()->FlattenTo(aElements);
      item->~nsDisplayItem();
    } else {
      aElements->AppendElement(item);
    }
  }
}

nsRect
nsDisplayList::GetBounds(nsDisplayListBuilder* aBuilder) const {
  nsRect bounds;
  for (nsDisplayItem* i = GetBottom(); i != nsnull; i = i->GetAbove()) {
    bool snap;
    bounds.UnionRect(bounds, i->GetBounds(aBuilder, &snap));
  }
  return bounds;
}

bool
nsDisplayList::ComputeVisibilityForRoot(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion) {
  SAMPLE_LABEL("nsDisplayList", "ComputeVisibilityForRoot");
  nsRegion r;
  r.And(*aVisibleRegion, GetBounds(aBuilder));
  return ComputeVisibilityForSublist(aBuilder, aVisibleRegion, r.GetBounds(), r.GetBounds());
}

static nsRegion
TreatAsOpaque(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
{
  bool snap;
  nsRegion opaque = aItem->GetOpaqueRegion(aBuilder, &snap);
  if (aBuilder->IsForPluginGeometry()) {
    // Treat all chrome items as opaque, unless their frames are opacity:0.
    // Since opacity:0 frames generate an nsDisplayOpacity, that item will
    // not be treated as opaque here, so opacity:0 chrome content will be
    // effectively ignored, as it should be.
    nsIFrame* f = aItem->GetUnderlyingFrame();
    if (f && f->PresContext()->IsChrome() && f->GetStyleDisplay()->mOpacity != 0.0) {
      opaque = aItem->GetBounds(aBuilder, &snap);
    }
  }
  return opaque;
}

static nsRect
GetDisplayPortBounds(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem)
{
  // GetDisplayPortBounds() rectangle is used in order to restrict fixed aItem's
  // visible bounds. nsDisplayTransform bounds already take item's
  // transform into account, so there is no need to apply it here one more time.
  // Start TransformRectToBoundsInAncestor() calculations from aItem's frame
  // parent in this case.
  nsIFrame* frame = aItem->GetUnderlyingFrame();
  if (aItem->GetType() == nsDisplayItem::TYPE_TRANSFORM) {
    frame = nsLayoutUtils::GetCrossDocParentFrame(frame);
  }

  const nsRect* displayport = aBuilder->GetDisplayPort();
  nsRect result = nsLayoutUtils::TransformAncestorRectToFrame(
                    frame,
                    nsRect(0, 0, displayport->width, displayport->height),
                    aBuilder->ReferenceFrame());
  result.MoveBy(aBuilder->ToReferenceFrame(frame));
  return result;
}

bool
nsDisplayList::ComputeVisibilityForSublist(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aListVisibleBounds,
                                           const nsRect& aAllowVisibleRegionExpansion) {
  bool snap;
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

  for (PRInt32 i = elements.Length() - 1; i >= 0; --i) {
    nsDisplayItem* item = elements[i];
    nsDisplayItem* belowItem = i < 1 ? nsnull : elements[i - 1];

    if (belowItem && item->TryMerge(aBuilder, belowItem)) {
      belowItem->~nsDisplayItem();
      elements.ReplaceElementsAt(i - 1, 1, item);
      continue;
    }

    nsDisplayList* list = item->GetList();
    if (list && item->ShouldFlattenAway(aBuilder)) {
      // The elements on the list >= i no longer serve any use.
      elements.SetLength(i);
      list->FlattenTo(&elements);
      i = elements.Length();
      item->~nsDisplayItem();
      continue;
    }

    nsRect bounds = item->GetBounds(aBuilder, &snap);

    nsRegion itemVisible;
    if (ForceVisiblityForFixedItem(aBuilder, item)) {
      itemVisible.And(GetDisplayPortBounds(aBuilder, item), bounds);
    } else {
      itemVisible.And(*aVisibleRegion, bounds);
    }
    item->mVisibleRect = itemVisible.GetBounds();

    if (item->ComputeVisibility(aBuilder, aVisibleRegion, aAllowVisibleRegionExpansion)) {
      anyVisible = true;
      nsRegion opaque = TreatAsOpaque(item, aBuilder);
      // Subtract opaque item from the visible region
      aBuilder->SubtractFromVisibleRegion(aVisibleRegion, opaque);
      if (aBuilder->NeedToForceTransparentSurfaceForItem(item) ||
          (list && list->NeedsTransparentSurface())) {
        forceTransparentSurface = true;
      }
    }
    AppendToBottom(item);
  }

  mIsOpaque = !aVisibleRegion->Intersects(mVisibleRect);
  mForceTransparentSurface = forceTransparentSurface;
#ifdef DEBUG
  mDidComputeVisibility = true;
#endif
  return anyVisible;
}

void nsDisplayList::PaintRoot(nsDisplayListBuilder* aBuilder,
                              nsRenderingContext* aCtx,
                              PRUint32 aFlags) const {
  SAMPLE_LABEL("nsDisplayList", "PaintRoot");
  PaintForFrame(aBuilder, aCtx, aBuilder->ReferenceFrame(), aFlags);
}

/**
 * We paint by executing a layer manager transaction, constructing a
 * single layer representing the display list, and then making it the
 * root of the layer manager, drawing into the ThebesLayers.
 */
void nsDisplayList::PaintForFrame(nsDisplayListBuilder* aBuilder,
                                  nsRenderingContext* aCtx,
                                  nsIFrame* aForFrame,
                                  PRUint32 aFlags) const {
  NS_ASSERTION(mDidComputeVisibility,
               "Must call ComputeVisibility before calling Paint");

  nsRefPtr<LayerManager> layerManager;
  bool allowRetaining = false;
  bool doBeginTransaction = true;
  if (aFlags & PAINT_USE_WIDGET_LAYERS) {
    nsIFrame* referenceFrame = aBuilder->ReferenceFrame();
    NS_ASSERTION(referenceFrame == nsLayoutUtils::GetDisplayRootFrame(referenceFrame),
                 "Reference frame must be a display root for us to use the layer manager");
    nsIWidget* window = referenceFrame->GetNearestWidget();
    if (window) {
      layerManager = window->GetLayerManager(&allowRetaining);
      if (layerManager) {
        doBeginTransaction = !(aFlags & PAINT_EXISTING_TRANSACTION);
      }
    }
  }
  if (!layerManager) {
    if (!aCtx) {
      NS_WARNING("Nowhere to paint into");
      return;
    }
    layerManager = new BasicLayerManager();
  }

  FrameLayerBuilder *layerBuilder = new FrameLayerBuilder();
  layerBuilder->Init(aBuilder);
  layerManager->SetUserData(&gLayerManagerLayerBuilder, new LayerManagerLayerBuilder(layerBuilder));

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
  if (allowRetaining) {
    layerBuilder->DidBeginRetainedLayerTransaction(layerManager);
  }

  nsPresContext* presContext = aForFrame->PresContext();
  nsIPresShell* presShell = presContext->GetPresShell();

  nsDisplayItem::ContainerParameters containerParameters
    (presShell->GetXResolution(), presShell->GetYResolution());
  nsRefPtr<ContainerLayer> root = layerBuilder->
    BuildContainerLayerFor(aBuilder, layerManager, aForFrame, nsnull, *this,
                           containerParameters, nsnull);

  if (!root) {
    layerManager->RemoveUserData(&gLayerManagerLayerBuilder);
    return;
  }
  // Root is being scaled up by the X/Y resolution. Scale it back down.
  gfx3DMatrix rootTransform = root->GetTransform()*
    gfx3DMatrix::ScalingMatrix(1.0f/containerParameters.mXScale,
                               1.0f/containerParameters.mYScale, 1.0f);
  root->SetTransform(rootTransform);

  ViewID id = presContext->IsRootContentDocument() ? FrameMetrics::ROOT_SCROLL_ID
                                                   : FrameMetrics::NULL_SCROLL_ID;

  nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
  nsRect displayport;
  bool usingDisplayport = false;
  if (rootScrollFrame) {
    nsIContent* content = rootScrollFrame->GetContent();
    if (content) {
      usingDisplayport = nsLayoutUtils::GetDisplayPort(content, &displayport);
    }
  }
  RecordFrameMetrics(aForFrame, rootScrollFrame,
                     root, mVisibleRect, mVisibleRect,
                     (usingDisplayport ? &displayport : nsnull), id,
                     containerParameters);
  if (usingDisplayport &&
      !(root->GetContentFlags() & Layer::CONTENT_OPAQUE)) {
    // See bug 693938, attachment 567017
    NS_WARNING("We don't support transparent content with displayports, force it to be opqaue");
    root->SetContentFlags(Layer::CONTENT_OPAQUE);
  }

  layerManager->SetRoot(root);
  layerBuilder->WillEndTransaction(layerManager);
  bool temp = aBuilder->SetIsCompositingCheap(layerManager->IsCompositingCheap());
  layerManager->EndTransaction(FrameLayerBuilder::DrawThebesLayer,
                               aBuilder);
  aBuilder->SetIsCompositingCheap(temp);
  layerBuilder->DidEndTransaction(layerManager);

  if (aFlags & PAINT_FLUSH_LAYERS) {
    FrameLayerBuilder::InvalidateAllLayers(layerManager);
  }

  nsCSSRendering::DidPaint();
  layerManager->RemoveUserData(&gLayerManagerLayerBuilder);
}

PRUint32 nsDisplayList::Count() const {
  PRUint32 count = 0;
  for (nsDisplayItem* i = GetBottom(); i; i = i->GetAbove()) {
    ++count;
  }
  return count;
}

nsDisplayItem* nsDisplayList::RemoveBottom() {
  nsDisplayItem* item = mSentinel.mAbove;
  if (!item)
    return nsnull;
  mSentinel.mAbove = item->mAbove;
  if (item == mTop) {
    // must have been the only item
    mTop = &mSentinel;
  }
  item->mAbove = nsnull;
  return item;
}

void nsDisplayList::DeleteAll() {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nsnull) {
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
    frame = frame->GetParentBox();
  }
  return false;
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
  PRUint32 length = aSource.Length();
  for (PRUint32 i = 0; i < length; i++) {
    aDest->MoveElementsFrom(aSource[i].mFrames);
  }
  aSource.Clear();
}

void nsDisplayList::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                            nsDisplayItem::HitTestState* aState,
                            nsTArray<nsIFrame*> *aOutFrames) const {
  PRInt32 itemBufferStart = aState->mItemBuffer.Length();
  nsDisplayItem* item;
  for (item = GetBottom(); item; item = item->GetAbove()) {
    aState->mItemBuffer.AppendElement(item);
  }
  nsAutoTArray<FramesWithDepth, 16> temp;
  for (PRInt32 i = aState->mItemBuffer.Length() - 1; i >= itemBufferStart; --i) {
    // Pop element off the end of the buffer. We want to shorten the buffer
    // so that recursive calls to HitTest have more buffer space.
    item = aState->mItemBuffer[i];
    aState->mItemBuffer.SetLength(i);

    bool snap;
    if (aRect.Intersects(item->GetBounds(aBuilder, &snap))) {
      nsAutoTArray<nsIFrame*, 16> outFrames;
      item->HitTest(aBuilder, aRect, aState, &outFrames);

      // For 3d transforms with preserve-3d we add hit frames into the temp list
      // so we can sort them later, otherwise we add them directly to the output list.
      nsTArray<nsIFrame*> *writeFrames = aOutFrames;
      if (item->GetType() == nsDisplayItem::TYPE_TRANSFORM &&
          item->GetUnderlyingFrame()->Preserves3D()) {
        if (outFrames.Length()) {
          nsDisplayTransform *transform = static_cast<nsDisplayTransform*>(item);
          nsPoint point = aRect.TopLeft();
          // A 1x1 rect means a point, otherwise use the center of the rect
          if (aRect.width != 1 || aRect.height != 1) {
            point = aRect.Center();
          }
          temp.AppendElement(FramesWithDepth(transform->GetHitDepthAtPoint(point)));
          writeFrames = &temp[temp.Length() - 1].mFrames;
        }
      } else {
        // We may have just finished a run of consecutive preserve-3d transforms,
        // so flush these into the destination array before processing our frame list.
        FlushFramesArray(temp, aOutFrames);
      }

      for (PRUint32 j = 0; j < outFrames.Length(); j++) {
        nsIFrame *f = outFrames.ElementAt(j);
        // Handle the XUL 'mousethrough' feature and 'pointer-events'.
        if (!GetMouseThrough(f) &&
            f->GetStyleVisibility()->mPointerEvents != NS_STYLE_POINTER_EVENTS_NONE) {
          writeFrames->AppendElement(f);
        }
      }
    }
  }
  // Clear any remaining preserve-3d transforms.
  FlushFramesArray(temp, aOutFrames);
  NS_ASSERTION(aState->mItemBuffer.Length() == PRUint32(itemBufferStart),
               "How did we forget to pop some elements?");
}

static void Sort(nsDisplayList* aList, PRInt32 aCount, nsDisplayList::SortLEQ aCmp,
                 void* aClosure) {
  if (aCount < 2)
    return;

  nsDisplayList list1;
  nsDisplayList list2;
  int i;
  PRInt32 half = aCount/2;
  bool sorted = true;
  nsDisplayItem* prev = nsnull;
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

static bool IsContentLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                           void* aClosure) {
  // These GetUnderlyingFrame calls return non-null because we're only used
  // in sorting
  return nsLayoutUtils::CompareTreePosition(
      aItem1->GetUnderlyingFrame()->GetContent(),
      aItem2->GetUnderlyingFrame()->GetContent(),
      static_cast<nsIContent*>(aClosure)) <= 0;
}

static bool IsZOrderLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                          void* aClosure) {
  // These GetUnderlyingFrame calls return non-null because we're only used
  // in sorting.  Note that we can't just take the difference of the two
  // z-indices here, because that might overflow a 32-bit int.
  PRInt32 index1 = nsLayoutUtils::GetZIndex(aItem1->GetUnderlyingFrame());
  PRInt32 index2 = nsLayoutUtils::GetZIndex(aItem2->GetUnderlyingFrame());
  return index1 <= index2;
}

void nsDisplayList::ExplodeAnonymousChildLists(nsDisplayListBuilder* aBuilder) {
  // See if there's anything to do
  bool anyAnonymousItems = false;
  nsDisplayItem* i;
  for (i = GetBottom(); i != nsnull; i = i->GetAbove()) {
    if (!i->GetUnderlyingFrame()) {
      anyAnonymousItems = true;
      break;
    }
  }
  if (!anyAnonymousItems)
    return;

  nsDisplayList tmp;
  while ((i = RemoveBottom()) != nsnull) {
    if (i->GetUnderlyingFrame()) {
      tmp.AppendToTop(i);
    } else {
      nsDisplayList* list = i->GetList();
      NS_ASSERTION(list, "leaf items can't be anonymous");
      list->ExplodeAnonymousChildLists(aBuilder);
      nsDisplayItem* j;
      while ((j = list->RemoveBottom()) != nsnull) {
        tmp.AppendToTop(static_cast<nsDisplayWrapList*>(i)->
            WrapWithClone(aBuilder, j));
      }
      i->~nsDisplayItem();
    }
  }

  AppendToTop(&tmp);
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
  ExplodeAnonymousChildLists(aBuilder);
  ::Sort(this, Count(), aCmp, aClosure);
}

bool nsDisplayItem::RecomputeVisibility(nsDisplayListBuilder* aBuilder,
                                          nsRegion* aVisibleRegion) {
  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);

  nsRegion itemVisible;
  if (ForceVisiblityForFixedItem(aBuilder, this)) {
    itemVisible.And(GetDisplayPortBounds(aBuilder, this), bounds);
  } else {
    itemVisible.And(*aVisibleRegion, bounds);
  }
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
  aBuilder->RegisterThemeGeometry(aFrame->GetStyleDisplay()->mAppearance,
      borderBox.ToNearestPixels(aFrame->PresContext()->AppUnitsPerDevPixel()));
}

nsDisplayBackground::nsDisplayBackground(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame)
  : nsDisplayItem(aBuilder, aFrame)
{
  MOZ_COUNT_CTOR(nsDisplayBackground);
  const nsStyleDisplay* disp = mFrame->GetStyleDisplay();
  mIsThemed = mFrame->IsThemed(disp, &mThemeTransparency);

  if (mIsThemed) {
    // Perform necessary RegisterThemeGeometry
    if (disp->mAppearance == NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR ||
        disp->mAppearance == NS_THEME_TOOLBAR) {
      RegisterThemeGeometry(aBuilder, aFrame);
    } else if (disp->mAppearance == NS_THEME_WIN_BORDERLESS_GLASS ||
               disp->mAppearance == NS_THEME_WIN_GLASS) {
      aBuilder->SetGlassDisplayItem(this);
    }
  } else {
    // Set HasFixedItems if we construct a background-attachment:fixed item
    nsPresContext* presContext = mFrame->PresContext();
    nsStyleContext* bgSC;
    bool hasBG = nsCSSRendering::FindBackground(presContext, mFrame, &bgSC);
    if (hasBG && bgSC->GetStyleBackground()->HasFixedBackground()) {
      aBuilder->SetHasFixedItems();
    }
  }
}

// Helper for RoundedRectIntersectsRect.
static bool
CheckCorner(nscoord aXOffset, nscoord aYOffset,
            nscoord aXRadius, nscoord aYRadius)
{
  NS_ABORT_IF_FALSE(aXOffset > 0 && aYOffset > 0,
                    "must not pass nonpositives to CheckCorner");
  NS_ABORT_IF_FALSE(aXRadius >= 0 && aYRadius >= 0,
                    "must not pass negatives to CheckCorner");

  // Avoid floating point math unless we're either (1) within the
  // quarter-ellipse area at the rounded corner or (2) outside the
  // rounding.
  if (aXOffset >= aXRadius || aYOffset >= aYRadius)
    return true;

  // Convert coordinates to a unit circle with (0,0) as the center of
  // curvature, and see if we're inside the circle or outside.
  float scaledX = float(aXRadius - aXOffset) / float(aXRadius);
  float scaledY = float(aYRadius - aYOffset) / float(aYRadius);
  return scaledX * scaledX + scaledY * scaledY < 1.0f;
}


/**
 * Return whether any part of aTestRect is inside of the rounded
 * rectangle formed by aBounds and aRadii (which are indexed by the
 * NS_CORNER_* constants in nsStyleConsts.h).
 *
 * See also RoundedRectContainsRect.
 */
static bool
RoundedRectIntersectsRect(const nsRect& aRoundedRect, nscoord aRadii[8],
                          const nsRect& aTestRect)
{
  NS_ABORT_IF_FALSE(aTestRect.Intersects(aRoundedRect),
                    "we should already have tested basic rect intersection");

  // distances from this edge of aRoundedRect to opposite edge of aTestRect,
  // which we know are positive due to the Intersects check above.
  nsMargin insets;
  insets.top = aTestRect.YMost() - aRoundedRect.y;
  insets.right = aRoundedRect.XMost() - aTestRect.x;
  insets.bottom = aRoundedRect.YMost() - aTestRect.y;
  insets.left = aTestRect.XMost() - aRoundedRect.x;

  // Check whether the bottom-right corner of aTestRect is inside the
  // top left corner of aBounds when rounded by aRadii, etc.  If any
  // corner is not, then fail; otherwise succeed.
  return CheckCorner(insets.left, insets.top,
                     aRadii[NS_CORNER_TOP_LEFT_X],
                     aRadii[NS_CORNER_TOP_LEFT_Y]) &&
         CheckCorner(insets.right, insets.top,
                     aRadii[NS_CORNER_TOP_RIGHT_X],
                     aRadii[NS_CORNER_TOP_RIGHT_Y]) &&
         CheckCorner(insets.right, insets.bottom,
                     aRadii[NS_CORNER_BOTTOM_RIGHT_X],
                     aRadii[NS_CORNER_BOTTOM_RIGHT_Y]) &&
         CheckCorner(insets.left, insets.bottom,
                     aRadii[NS_CORNER_BOTTOM_LEFT_X],
                     aRadii[NS_CORNER_BOTTOM_LEFT_Y]);
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
         RoundedRectIntersectsRect(nsRect(aFrameToReferenceFrame,
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
nsDisplayBackground::IsSingleFixedPositionImage(nsDisplayListBuilder* aBuilder, const nsRect& aClipRect)
{
  if (mIsThemed)
    return false;

  nsPresContext* presContext = mFrame->PresContext();
  nsStyleContext* bgSC;
  if (!nsCSSRendering::FindBackground(presContext, mFrame, &bgSC))
    return false;

  bool drawBackgroundImage;
  bool drawBackgroundColor;
  nsCSSRendering::DetermineBackgroundColor(presContext,
                                           bgSC,
                                           mFrame,
                                           drawBackgroundImage,
                                           drawBackgroundColor);

  // For now we don't know how to draw image layers with a background color.
  if (!drawBackgroundImage || drawBackgroundColor)
    return false;

  const nsStyleBackground *bg = bgSC->GetStyleBackground();

  // We could pretty easily support multiple image layers, but for now we
  // just punt here.
  if (bg->mLayers.Length() != 1)
    return false;

  PRUint32 flags = aBuilder->GetBackgroundPaintFlags();
  nsPoint offset = ToReferenceFrame();
  nsRect borderArea = nsRect(offset, mFrame->GetSize());

  const nsStyleBackground::Layer &layer = bg->mLayers[0];

  if (layer.mAttachment != NS_STYLE_BG_ATTACHMENT_FIXED)
    return false;

  nsBackgroundLayerState state =
    nsCSSRendering::PrepareBackgroundLayer(presContext,
                                           mFrame,
                                           flags,
                                           borderArea,
                                           aClipRect,
                                           *bg,
                                           layer);

  nsImageRenderer* imageRenderer = &state.mImageRenderer;
  // We only care about images here, not gradients.
  if (!imageRenderer->IsRasterImage())
    return false;

  PRInt32 appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  mDestRect = nsLayoutUtils::RectToGfxRect(state.mFillArea, appUnitsPerDevPixel);

  return true;
}

bool
nsDisplayBackground::TryOptimizeToImageLayer(nsDisplayListBuilder* aBuilder)
{
  if (mIsThemed)
    return false;

  nsPresContext* presContext = mFrame->PresContext();
  nsStyleContext* bgSC;
  if (!nsCSSRendering::FindBackground(presContext, mFrame, &bgSC))
    return false;

  bool drawBackgroundImage;
  bool drawBackgroundColor;
  nsCSSRendering::DetermineBackgroundColor(presContext,
                                           bgSC,
                                           mFrame,
                                           drawBackgroundImage,
                                           drawBackgroundColor);

  // For now we don't know how to draw image layers with a background color.
  if (!drawBackgroundImage || drawBackgroundColor)
    return false;

  const nsStyleBackground *bg = bgSC->GetStyleBackground();

  // We could pretty easily support multiple image layers, but for now we
  // just punt here.
  if (bg->mLayers.Length() != 1)
    return false;

  PRUint32 flags = aBuilder->GetBackgroundPaintFlags();
  nsPoint offset = ToReferenceFrame();
  nsRect borderArea = nsRect(offset, mFrame->GetSize());

  const nsStyleBackground::Layer &layer = bg->mLayers[0];

  nsBackgroundLayerState state =
    nsCSSRendering::PrepareBackgroundLayer(presContext,
                                           mFrame,
                                           flags,
                                           borderArea,
                                           borderArea,
                                           *bg,
                                           layer);

  nsImageRenderer* imageRenderer = &state.mImageRenderer;
  // We only care about images here, not gradients.
  if (imageRenderer->IsRasterImage())
    return false;

  nsRefPtr<ImageContainer> imageContainer = imageRenderer->GetContainer();
  // Image is not ready to be made into a layer yet
  if (!imageContainer)
    return false;

  // We currently can't handle tiled or partial backgrounds.
  if (!state.mDestArea.IsEqualEdges(state.mFillArea)) {
    return false;
  }

  // Sub-pixel alignment is hard, lets punt on that.
  if (state.mAnchor != nsPoint(0.0f, 0.0f)) {
    return false;
  }

  PRInt32 appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  mDestRect = nsLayoutUtils::RectToGfxRect(state.mDestArea, appUnitsPerDevPixel);
  mImageContainer = imageContainer;

  // Ok, we can turn this into a layer if needed.
  return true;
}

LayerState
nsDisplayBackground::GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const FrameLayerBuilder::ContainerParameters& aParameters)
{
  if (!aManager->IsCompositingCheap() ||
      !nsLayoutUtils::GPUImageScalingEnabled() ||
      !TryOptimizeToImageLayer(aBuilder)) {
    return LAYER_NONE;
  }

  gfxSize imageSize = mImageContainer->GetCurrentSize();
  NS_ASSERTION(imageSize.width != 0 && imageSize.height != 0, "Invalid image size!");

  gfxRect destRect = mDestRect;

  destRect.width *= aParameters.mXScale;
  destRect.height *= aParameters.mYScale;

  // Calculate the scaling factor for the frame.
  gfxSize scale = gfxSize(destRect.width / imageSize.width, destRect.height / imageSize.height);

  // If we are not scaling at all, no point in separating this into a layer.
  if (scale.width == 1.0f && scale.height == 1.0f) {
    return LAYER_INACTIVE;
  }

  // If the target size is pretty small, no point in using a layer.
  if (destRect.width * destRect.height < 64 * 64) {
    return LAYER_INACTIVE;
  }

  return LAYER_ACTIVE;
}

already_AddRefed<Layer>
nsDisplayBackground::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerParameters& aParameters)
{
  nsRefPtr<ImageLayer> layer = aManager->CreateImageLayer();
  layer->SetContainer(mImageContainer);
  ConfigureLayer(layer);
  return layer.forget();
}

void
nsDisplayBackground::ConfigureLayer(ImageLayer* aLayer)
{
  aLayer->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(mFrame));

  gfxIntSize imageSize = mImageContainer->GetCurrentSize();
  NS_ASSERTION(imageSize.width != 0 && imageSize.height != 0, "Invalid image size!");

  gfxMatrix transform;
  transform.Translate(mDestRect.TopLeft());
  transform.Scale(mDestRect.width/imageSize.width,
                  mDestRect.height/imageSize.height);
  aLayer->SetTransform(gfx3DMatrix::From2D(transform));

  aLayer->SetVisibleRegion(nsIntRect(0, 0, imageSize.width, imageSize.height));
}

void
nsDisplayBackground::HitTest(nsDisplayListBuilder* aBuilder,
                             const nsRect& aRect,
                             HitTestState* aState,
                             nsTArray<nsIFrame*> *aOutFrames)
{
  if (mIsThemed) {
    // For theme backgrounds, assume that any point in our border rect is a hit.
    if (!nsRect(ToReferenceFrame(), mFrame->GetSize()).Intersects(aRect))
      return;
  } else {
    if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
      // aRect doesn't intersect our border-radius curve.
      return;
    }
  }

  aOutFrames->AppendElement(mFrame);
}

bool
nsDisplayBackground::ComputeVisibility(nsDisplayListBuilder* aBuilder,
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
  nsStyleContext* bgSC;
  return mIsThemed ||
    nsCSSRendering::FindBackground(mFrame->PresContext(), mFrame, &bgSC);
}

nsRegion
nsDisplayBackground::GetInsideClipRegion(nsPresContext* aPresContext,
                                         PRUint8 aClip, const nsRect& aRect,
                                         bool* aSnap)
{
  nsRegion result;
  if (aRect.IsEmpty())
    return result;

  nscoord radii[8];
  nsRect clipRect;
  bool haveRadii;
  switch (aClip) {
  case NS_STYLE_BG_CLIP_BORDER:
    haveRadii = mFrame->GetBorderRadii(radii);
    clipRect = nsRect(ToReferenceFrame(), mFrame->GetSize());
    break;
  case NS_STYLE_BG_CLIP_PADDING:
    haveRadii = mFrame->GetPaddingBoxBorderRadii(radii);
    clipRect = mFrame->GetPaddingRect() - mFrame->GetPosition() + ToReferenceFrame();
    break;
  case NS_STYLE_BG_CLIP_CONTENT:
    haveRadii = mFrame->GetContentBoxBorderRadii(radii);
    clipRect = mFrame->GetContentRect() - mFrame->GetPosition() + ToReferenceFrame();
    break;
  default:
    NS_NOTREACHED("Unknown clip type");
    return result;
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
nsDisplayBackground::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                     bool* aSnap) {
  nsRegion result;
  *aSnap = false;
  // theme background overrides any other background
  if (mIsThemed) {
    if (mThemeTransparency == nsITheme::eOpaque) {
      result = GetBounds(aBuilder, aSnap);
    }
    return result;
  }

  nsStyleContext* bgSC;
  nsPresContext* presContext = mFrame->PresContext();
  if (!nsCSSRendering::FindBackground(presContext, mFrame, &bgSC))
    return result;
  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  const nsStyleBackground::Layer& bottomLayer = bg->BottomLayer();

  *aSnap = true;

  nsRect borderBox = nsRect(ToReferenceFrame(), mFrame->GetSize());
  if (NS_GET_A(bg->mBackgroundColor) == 255 &&
      !nsCSSRendering::IsCanvasFrame(mFrame)) {
    result = GetInsideClipRegion(presContext, bottomLayer.mClip, borderBox, aSnap);
  }

  // For policies other than EACH_BOX, don't try to optimize here, since
  // this could easily lead to O(N^2) behavior inside InlineBackgroundData,
  // which expects frames to be sent to it in content order, not reverse
  // content order which we'll produce here.
  // Of course, if there's only one frame in the flow, it doesn't matter.
  if (bg->mBackgroundInlinePolicy == NS_STYLE_BG_INLINE_POLICY_EACH_BOX ||
      (!mFrame->GetPrevContinuation() && !mFrame->GetNextContinuation())) {
    NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
      const nsStyleBackground::Layer& layer = bg->mLayers[i];
      if (layer.mImage.IsOpaque()) {
        nsRect r = nsCSSRendering::GetBackgroundLayerRect(presContext, mFrame,
            borderBox, *bg, layer);
        result.Or(result, GetInsideClipRegion(presContext, layer.mClip, r, aSnap));
      }
    }
  }

  return result;
}

bool
nsDisplayBackground::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) {
  // theme background overrides any other background
  if (mIsThemed) {
    const nsStyleDisplay* disp = mFrame->GetStyleDisplay();
    if (disp->mAppearance == NS_THEME_WIN_BORDERLESS_GLASS ||
        disp->mAppearance == NS_THEME_WIN_GLASS) {
      *aColor = NS_RGBA(0,0,0,0);
      return true;
    }
    return false;
  }

  nsStyleContext *bgSC;
  bool hasBG =
    nsCSSRendering::FindBackground(mFrame->PresContext(), mFrame, &bgSC);
  if (!hasBG) {
    *aColor = NS_RGBA(0,0,0,0);
    return true;
  }
  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  if (bg->BottomLayer().mImage.IsEmpty() &&
      bg->mImageCount == 1 &&
      !nsLayoutUtils::HasNonZeroCorner(mFrame->GetStyleBorder()->mBorderRadius) &&
      bg->BottomLayer().mClip == NS_STYLE_BG_CLIP_BORDER) {
    // Canvas frames don't actually render their background color, since that
    // gets propagated to the solid color of the viewport
    // (see nsCSSRendering::PaintBackgroundWithSC)
    *aColor = nsCSSRendering::IsCanvasFrame(mFrame) ? NS_RGBA(0,0,0,0)
        : bg->mBackgroundColor;
    return true;
  }
  return false;
}

bool
nsDisplayBackground::IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                    nsIFrame* aFrame)
{
  // theme background overrides any other background and is never fixed
  if (mIsThemed)
    return false;

  nsPresContext* presContext = mFrame->PresContext();
  nsStyleContext *bgSC;
  bool hasBG =
    nsCSSRendering::FindBackground(presContext, mFrame, &bgSC);
  if (!hasBG)
    return false;
  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  if (!bg->HasFixedBackground())
    return false;

  // If aFrame is mFrame or an ancestor in this document, and aFrame is
  // not the viewport frame, then moving aFrame will move mFrame
  // relative to the viewport, so our fixed-pos background will change.
  return aFrame->GetParent() &&
    (aFrame == mFrame ||
     nsLayoutUtils::IsProperAncestorFrame(aFrame, mFrame));
}

bool
nsDisplayBackground::ShouldFixToViewport(nsDisplayListBuilder* aBuilder)
{
  if (mIsThemed)
    return false;

  nsPresContext* presContext = mFrame->PresContext();
  nsStyleContext* bgSC;
  bool hasBG =
    nsCSSRendering::FindBackground(presContext, mFrame, &bgSC);
  if (!hasBG)
    return false;

  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  if (!bg->HasFixedBackground())
    return false;

  NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
    const nsStyleBackground::Layer& layer = bg->mLayers[i];
    if (layer.mAttachment != NS_STYLE_BG_ATTACHMENT_FIXED &&
        !layer.mImage.IsEmpty()) {
      return false;
    }
    if (layer.mClip != NS_STYLE_BG_CLIP_BORDER)
      return false;
  }

  if (nsLayoutUtils::HasNonZeroCorner(mFrame->GetStyleBorder()->mBorderRadius))
    return false;

  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  nsIFrame* rootScrollFrame = presContext->PresShell()->GetRootScrollFrame();
  if (!rootScrollFrame)
    return false;
  nsIScrollableFrame* scrollable = do_QueryFrame(rootScrollFrame);
  nsRect scrollport = scrollable->GetScrollPortRect() +
    aBuilder->ToReferenceFrame(rootScrollFrame);
  return bounds.Contains(scrollport);
}

void
nsDisplayBackground::Paint(nsDisplayListBuilder* aBuilder,
                           nsRenderingContext* aCtx) {

  nsPoint offset = ToReferenceFrame();
  PRUint32 flags = aBuilder->GetBackgroundPaintFlags();
  nsDisplayItem* nextItem = GetAbove();
  if (nextItem && nextItem->GetUnderlyingFrame() == mFrame &&
      nextItem->GetType() == TYPE_BORDER) {
    flags |= nsCSSRendering::PAINTBG_WILL_PAINT_BORDER;
  }
  nsCSSRendering::PaintBackground(mFrame->PresContext(), *aCtx, mFrame,
                                  mVisibleRect,
                                  nsRect(offset, mFrame->GetSize()),
                                  flags);
}

nsRect
nsDisplayBackground::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  nsRect r(nsPoint(0,0), mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();

  if (mIsThemed) {
    presContext->GetTheme()->
        GetWidgetOverflow(presContext->DeviceContext(), mFrame,
                          mFrame->GetStyleDisplay()->mAppearance, &r);
  }

  *aSnap = true;
  return r + ToReferenceFrame();
}

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
                               mFrame->GetStyleContext());
}

bool
nsDisplayOutline::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion,
                                    const nsRect& aAllowVisibleRegionExpansion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion)) {
    return false;
  }

  const nsStyleOutline* outline = mFrame->GetStyleOutline();
  nsRect borderBox(ToReferenceFrame(), mFrame->GetSize());
  if (borderBox.Contains(aVisibleRegion->GetBounds()) &&
      !nsLayoutUtils::HasNonZeroCorner(outline->mOutlineRadius)) {
    if (outline->mOutlineOffset >= 0) {
      // the visible region is entirely inside the border-rect, and the outline
      // isn't rendered inside the border-rect, so the outline is not visible
      return false;
    }
  }

  return true;
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
nsDisplayCaret::Paint(nsDisplayListBuilder* aBuilder,
                      nsRenderingContext* aCtx) {
  // Note: Because we exist, we know that the caret is visible, so we don't
  // need to check for the caret's visibility.
  mCaret->PaintCaret(aBuilder, aCtx, mFrame, ToReferenceFrame());
}

bool
nsDisplayBorder::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion)) {
    return false;
  }

  nsRect paddingRect = mFrame->GetPaddingRect() - mFrame->GetPosition() +
    ToReferenceFrame();
  const nsStyleBorder *styleBorder;
  if (paddingRect.Contains(aVisibleRegion->GetBounds()) &&
      !(styleBorder = mFrame->GetStyleBorder())->IsBorderImageLoaded() &&
      !nsLayoutUtils::HasNonZeroCorner(styleBorder->mBorderRadius)) {
    // the visible region is entirely inside the content rect, and no part
    // of the border is rendered inside the content rect, so we are not
    // visible
    // Skip this if there's a border-image (which draws a background
    // too) or if there is a border-radius (which makes the border draw
    // further in).
    return false;
  }

  return true;
}

void
nsDisplayBorder::Paint(nsDisplayListBuilder* aBuilder,
                       nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsCSSRendering::PaintBorder(mFrame->PresContext(), *aCtx, mFrame,
                              mVisibleRect,
                              nsRect(offset, mFrame->GetSize()),
                              mFrame->GetStyleContext(),
                              mFrame->GetSkipSides());
}

nsRect
nsDisplayBorder::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  nsRect borderBounds(ToReferenceFrame(), mFrame->GetSize());
  borderBounds.Inflate(mFrame->GetStyleBorder()->GetImageOutset());
  *aSnap = true;
  return borderBounds;
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
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  nsAutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  SAMPLE_LABEL("nsDisplayBoxShadowOuter", "Paint");
  for (PRUint32 i = 0; i < rects.Length(); ++i) {
    aCtx->PushState();
    aCtx->IntersectClip(rects[i]);
    nsCSSRendering::PaintBoxShadowOuter(presContext, *aCtx, mFrame,
                                        borderRect, rects[i]);
    aCtx->PopState();
  }
}

nsRect
nsDisplayBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
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

  nsPoint origin = ToReferenceFrame();
  nsRect visibleBounds = aVisibleRegion->GetBounds();
  nsRect frameRect(origin, mFrame->GetSize());
  if (!frameRect.Contains(visibleBounds))
    return true;

  // the visible region is entirely inside the border-rect, and box shadows
  // never render within the border-rect (unless there's a border radius).
  nscoord twipsRadii[8];
  bool hasBorderRadii = mFrame->GetBorderRadii(twipsRadii);
  if (!hasBorderRadii)
    return false;

  return !RoundedRectContainsRect(frameRect, twipsRadii, visibleBounds);
}

void
nsDisplayBoxShadowInner::Paint(nsDisplayListBuilder* aBuilder,
                               nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  nsAutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  SAMPLE_LABEL("nsDisplayBoxShadowInner", "Paint");
  for (PRUint32 i = 0; i < rects.Length(); ++i) {
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
  : nsDisplayItem(aBuilder, aFrame) {
  mList.AppendToTop(aList);
  UpdateBounds(aBuilder);
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayItem* aItem)
  : nsDisplayItem(aBuilder, aFrame) {
  mList.AppendToTop(aItem);
  UpdateBounds(aBuilder);
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayItem* aItem,
                                     const nsPoint& aToReferenceFrame)
  : nsDisplayItem(aBuilder, aFrame, aToReferenceFrame) {
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
  return mList.ComputeVisibilityForSublist(aBuilder, aVisibleRegion,
                                           mVisibleRect,
                                           aAllowVisibleRegionExpansion);
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

bool nsDisplayWrapList::ChildrenCanBeInactive(nsDisplayListBuilder* aBuilder,
                                              LayerManager* aManager,
                                              const ContainerParameters& aParameters,
                                              const nsDisplayList& aList,
                                              nsIFrame* aActiveScrolledRoot) {
  for (nsDisplayItem* i = aList.GetBottom(); i; i = i->GetAbove()) {
    nsIFrame* f = i->GetUnderlyingFrame();
    if (f) {
      nsIFrame* activeScrolledRoot =
        nsLayoutUtils::GetActiveScrolledRootFor(f, nsnull);
      if (activeScrolledRoot != aActiveScrolledRoot)
        return false;
    }

    LayerState state = i->GetLayerState(aBuilder, aManager, aParameters);
    if (state == LAYER_ACTIVE)
      return false;
    if (state == LAYER_NONE) {
      nsDisplayList* list = i->GetList();
      if (list && !ChildrenCanBeInactive(aBuilder, aManager, aParameters, *list, aActiveScrolledRoot))
        return false;
    }
  }
  return true;
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
    : nsDisplayWrapList(aBuilder, aFrame, aList) {
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
  // We are never opaque, if our opacity was < 1 then we wouldn't have
  // been created.
  return nsRegion();
}

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer>
nsDisplayOpacity::BuildLayer(nsDisplayListBuilder* aBuilder,
                             LayerManager* aManager,
                             const ContainerParameters& aContainerParameters) {
  nsRefPtr<Layer> container = GetLayerBuilderForManager(aManager)->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           aContainerParameters, nsnull);
  if (!container)
    return nsnull;

  container->SetOpacity(mFrame->GetStyleDisplay()->mOpacity);

  container->ClearAnimations();
  ElementAnimations* ea =
    nsAnimationManager::GetAnimationsForCompositor(mFrame->GetContent(),
                                                   eCSSProperty_opacity);
  AddOpacityAnimations(ea, container);

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
          aItem->GetUnderlyingFrame()->PresContext()->AppUnitsPerDevPixel());
  static const int MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS = 16;
  return visibleDevPixels.Size() <
    nsIntSize(MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS, MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS);
}

nsDisplayItem::LayerState
nsDisplayOpacity::GetLayerState(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerParameters& aParameters) {
  if (mFrame->AreLayersMarkedActive(nsChangeHint_UpdateOpacityLayer) &&
      !IsItemTooSmallForActiveLayer(this))
    return LAYER_ACTIVE;
  if (mFrame->GetContent()) {
    if (nsAnimationManager::GetAnimationsForCompositor(mFrame->GetContent(),
                                                       eCSSProperty_opacity)) {
      return LAYER_ACTIVE;
    }
  }
  nsIFrame* activeScrolledRoot =
    nsLayoutUtils::GetActiveScrolledRootFor(mFrame, nsnull);
  return !ChildrenCanBeInactive(aBuilder, aManager, aParameters, mList, activeScrolledRoot)
      ? LAYER_ACTIVE : LAYER_INACTIVE;
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
  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
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
  if (aItem->GetUnderlyingFrame()->GetContent() != mFrame->GetContent())
    return false;
  MergeFromTrackingMergedFrames(static_cast<nsDisplayOpacity*>(aItem));
  return true;
}

nsDisplayOwnLayer::nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList) {
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
                              const ContainerParameters& aContainerParameters) {
  nsRefPtr<Layer> layer = GetLayerBuilderForManager(aManager)->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           aContainerParameters, nsnull);
  return layer.forget();
}

nsDisplayFixedPosition::nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder,
                                               nsIFrame* aFrame,
                                               nsIFrame* aFixedPosFrame,
                                               nsDisplayList* aList)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList)
    , mFixedPosFrame(aFixedPosFrame) {
  MOZ_COUNT_CTOR(nsDisplayFixedPosition);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayFixedPosition::~nsDisplayFixedPosition() {
  MOZ_COUNT_DTOR(nsDisplayFixedPosition);
}
#endif

already_AddRefed<Layer>
nsDisplayFixedPosition::BuildLayer(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aContainerParameters) {
  nsRefPtr<Layer> layer =
    nsDisplayOwnLayer::BuildLayer(aBuilder, aManager, aContainerParameters);

  // Work out the anchor point for this fixed position layer. We assume that
  // any positioning set (left/top/right/bottom) indicates that the
  // corresponding side of its container should be the anchor point,
  // defaulting to top-left.
  nsIFrame* viewportFrame = mFixedPosFrame->GetParent();
  nsPresContext *presContext = viewportFrame->PresContext();

  // Fixed position frames are reflowed into the scroll-port size if one has
  // been set.
  nsSize containingBlockSize = viewportFrame->GetSize();
  if (presContext->PresShell()->IsScrollPositionClampingScrollPortSizeSet()) {
    containingBlockSize = presContext->PresShell()->
      GetScrollPositionClampingScrollPortSize();
  }

  // Find out the rect of the viewport frame relative to the reference frame.
  // This, in conjunction with the container scale, will correspond to the
  // coordinate-space of the built layer.
  float factor = presContext->AppUnitsPerDevPixel();
  nsPoint origin = aBuilder->ToReferenceFrame(viewportFrame);
  gfxRect anchorRect(NSAppUnitsToFloatPixels(origin.x, factor) *
                       aContainerParameters.mXScale,
                     NSAppUnitsToFloatPixels(origin.y, factor) *
                       aContainerParameters.mYScale,
                     NSAppUnitsToFloatPixels(containingBlockSize.width, factor) *
                       aContainerParameters.mXScale,
                     NSAppUnitsToFloatPixels(containingBlockSize.height, factor) *
                       aContainerParameters.mYScale);

  gfxPoint anchor(anchorRect.x, anchorRect.y);

  const nsStylePosition* position = mFixedPosFrame->GetStylePosition();
  if (position->mOffset.GetRightUnit() != eStyleUnit_Auto)
    anchor.x = anchorRect.XMost();
  if (position->mOffset.GetBottomUnit() != eStyleUnit_Auto)
    anchor.y = anchorRect.YMost();

  layer->SetFixedPositionAnchor(anchor);

  return layer.forget();
}

nsDisplayScrollLayer::nsDisplayScrollLayer(nsDisplayListBuilder* aBuilder,
                                           nsDisplayList* aList,
                                           nsIFrame* aForFrame,
                                           nsIFrame* aScrolledFrame,
                                           nsIFrame* aScrollFrame)
  : nsDisplayWrapList(aBuilder, aForFrame, aList)
  , mScrollFrame(aScrollFrame)
  , mScrolledFrame(aScrolledFrame)
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

already_AddRefed<Layer>
nsDisplayScrollLayer::BuildLayer(nsDisplayListBuilder* aBuilder,
                                 LayerManager* aManager,
                                 const ContainerParameters& aContainerParameters) {
  nsRefPtr<ContainerLayer> layer = GetLayerBuilderForManager(aManager)->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           aContainerParameters, nsnull);

  // Get the already set unique ID for scrolling this content remotely.
  // Or, if not set, generate a new ID.
  nsIContent* content = mScrolledFrame->GetContent();
  ViewID scrollId = nsLayoutUtils::FindIDFor(content);

  nsRect viewport = mScrollFrame->GetRect() -
                    mScrollFrame->GetPosition() +
                    aBuilder->ToReferenceFrame(mScrollFrame);

  bool usingDisplayport = false;
  nsRect displayport;
  if (content) {
    usingDisplayport = nsLayoutUtils::GetDisplayPort(content, &displayport);
  }
  RecordFrameMetrics(mScrolledFrame, mScrollFrame, layer, mVisibleRect, viewport,
                     (usingDisplayport ? &displayport : nsnull), scrollId,
                     aContainerParameters);

  return layer.forget();
}

bool
nsDisplayScrollLayer::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion,
                                        const nsRect& aAllowVisibleRegionExpansion)
{
  nsRect displayport;
  if (nsLayoutUtils::GetDisplayPort(mScrolledFrame->GetContent(), &displayport)) {
    // The visible region for the children may be much bigger than the hole we
    // are viewing the children from, so that the compositor process has enough
    // content to asynchronously pan while content is being refreshed.

    nsRegion childVisibleRegion = displayport + aBuilder->ToReferenceFrame(mScrollFrame);

    nsRect boundedRect =
      childVisibleRegion.GetBounds().Intersect(mList.GetBounds(aBuilder));
    nsRect allowExpansion = boundedRect.Intersect(aAllowVisibleRegionExpansion);
    bool visible = mList.ComputeVisibilityForSublist(
      aBuilder, &childVisibleRegion, boundedRect, allowExpansion);
    mVisibleRect = boundedRect;

    return visible;

  } else {
    return nsDisplayWrapList::ComputeVisibility(aBuilder, aVisibleRegion,
                                                aAllowVisibleRegionExpansion);
  }
}

LayerState
nsDisplayScrollLayer::GetLayerState(nsDisplayListBuilder* aBuilder,
                                    LayerManager* aManager,
                                    const ContainerParameters& aParameters)
{
  // Force this as a layer so we can scroll asynchronously.
  // This causes incorrect rendering for rounded clips!
  return LAYER_ACTIVE_FORCE;
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

bool
nsDisplayScrollLayer::ShouldFlattenAway(nsDisplayListBuilder* aBuilder)
{
  return GetScrollLayerCount() > 1;
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

intptr_t
nsDisplayScrollLayer::RemoveScrollLayerCount()
{
  intptr_t result = GetScrollLayerCount();
  FrameProperties props = mScrolledFrame->Properties();
  props.Remove(nsIFrame::ScrollLayerCount());
  return result;
}


nsDisplayScrollInfoLayer::nsDisplayScrollInfoLayer(
  nsDisplayListBuilder* aBuilder,
  nsIFrame* aScrolledFrame,
  nsIFrame* aScrollFrame)
  : nsDisplayScrollLayer(aBuilder, aScrolledFrame, aScrolledFrame, aScrollFrame)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollInfoLayer);
#endif
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayScrollInfoLayer::~nsDisplayScrollInfoLayer()
{
  MOZ_COUNT_DTOR(nsDisplayScrollInfoLayer);
}
#endif

LayerState
nsDisplayScrollInfoLayer::GetLayerState(nsDisplayListBuilder* aBuilder,
                                        LayerManager* aManager,
                                        const ContainerParameters& aParameters)
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
  return RemoveScrollLayerCount() == 1;
}

nsDisplayClip::nsDisplayClip(nsDisplayListBuilder* aBuilder,
                             nsIFrame* aFrame, nsDisplayItem* aItem,
                             const nsRect& aRect)
   : nsDisplayWrapList(aBuilder, aFrame, aItem,
       aFrame == aItem->GetUnderlyingFrame() ? aItem->ToReferenceFrame() : aBuilder->ToReferenceFrame(aFrame)),
     mClip(aRect) {
  MOZ_COUNT_CTOR(nsDisplayClip);
}

nsDisplayClip::nsDisplayClip(nsDisplayListBuilder* aBuilder,
                             nsIFrame* aFrame, nsDisplayList* aList,
                             const nsRect& aRect)
   : nsDisplayWrapList(aBuilder, aFrame, aList), mClip(aRect) {
  MOZ_COUNT_CTOR(nsDisplayClip);
}

nsRect nsDisplayClip::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  nsRect r = nsDisplayWrapList::GetBounds(aBuilder, aSnap);
  *aSnap = false;
  return mClip.Intersect(r);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayClip::~nsDisplayClip() {
  MOZ_COUNT_DTOR(nsDisplayClip);
}
#endif

void nsDisplayClip::Paint(nsDisplayListBuilder* aBuilder,
                          nsRenderingContext* aCtx) {
  NS_ERROR("nsDisplayClip should have been flattened away for painting");
}

bool nsDisplayClip::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion,
                                        const nsRect& aAllowVisibleRegionExpansion) {
  nsRegion clipped;
  clipped.And(*aVisibleRegion, mClip);

  nsRegion finalClipped(clipped);
  nsRect allowExpansion = mClip.Intersect(aAllowVisibleRegionExpansion);
  bool anyVisible =
    nsDisplayWrapList::ComputeVisibility(aBuilder, &finalClipped,
                                         allowExpansion);

  nsRegion removed;
  removed.Sub(clipped, finalClipped);
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, removed);

  return anyVisible;
}

bool nsDisplayClip::TryMerge(nsDisplayListBuilder* aBuilder,
                             nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_CLIP)
    return false;
  nsDisplayClip* other = static_cast<nsDisplayClip*>(aItem);
  if (!other->mClip.IsEqualInterior(mClip))
    return false;
  // No need to track merged frames for clipping
  MergeFrom(other);
  return true;
}

nsDisplayWrapList* nsDisplayClip::WrapWithClone(nsDisplayListBuilder* aBuilder,
                                                nsDisplayItem* aItem) {
  return new (aBuilder)
    nsDisplayClip(aBuilder, aItem->GetUnderlyingFrame(), aItem, mClip);
}

nsDisplayClipRoundedRect::nsDisplayClipRoundedRect(
                             nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             nsDisplayItem* aItem,
                             const nsRect& aRect, nscoord aRadii[8])
    : nsDisplayClip(aBuilder, aFrame, aItem, aRect)
{
  MOZ_COUNT_CTOR(nsDisplayClipRoundedRect);
  memcpy(mRadii, aRadii, sizeof(mRadii));
}

nsDisplayClipRoundedRect::nsDisplayClipRoundedRect(
                             nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             nsDisplayList* aList,
                             const nsRect& aRect, nscoord aRadii[8])
    : nsDisplayClip(aBuilder, aFrame, aList, aRect)
{
  MOZ_COUNT_CTOR(nsDisplayClipRoundedRect);
  memcpy(mRadii, aRadii, sizeof(mRadii));
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayClipRoundedRect::~nsDisplayClipRoundedRect()
{
  MOZ_COUNT_DTOR(nsDisplayClipRoundedRect);
}
#endif

nsRegion
nsDisplayClipRoundedRect::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap)
{
  *aSnap = false;
  return nsRegion();
}

void
nsDisplayClipRoundedRect::HitTest(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aRect, HitTestState* aState,
                                  nsTArray<nsIFrame*> *aOutFrames)
{
  if (!RoundedRectIntersectsRect(mClip, mRadii, aRect)) {
    // aRect doesn't intersect our border-radius curve.

    // FIXME: This isn't quite sufficient for aRect having nontrivial
    // size (which is the unusual case here), since it's possible that
    // the part of aRect that intersects the the rounded rect isn't the
    // part that intersects the items in mList.
    return;
  }

  mList.HitTest(aBuilder, aRect, aState, aOutFrames);
}

nsDisplayWrapList*
nsDisplayClipRoundedRect::WrapWithClone(nsDisplayListBuilder* aBuilder,
                                        nsDisplayItem* aItem) {
  return new (aBuilder)
    nsDisplayClipRoundedRect(aBuilder, aItem->GetUnderlyingFrame(), aItem,
                             mClip, mRadii);
}

bool nsDisplayClipRoundedRect::ComputeVisibility(
                                    nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion,
                                    const nsRect& aAllowVisibleRegionExpansion)
{
  nsRegion clipped;
  clipped.And(*aVisibleRegion, mClip);

  return nsDisplayWrapList::ComputeVisibility(aBuilder, &clipped, nsRect());
  // FIXME: Remove a *conservative* opaque region from aVisibleRegion
  // (like in nsDisplayClip::ComputeVisibility).
}

bool nsDisplayClipRoundedRect::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem)
{
  if (aItem->GetType() != TYPE_CLIP_ROUNDED_RECT)
    return false;
  nsDisplayClipRoundedRect* other =
    static_cast<nsDisplayClipRoundedRect*>(aItem);
  if (!mClip.IsEqualInterior(other->mClip) ||
      memcmp(mRadii, other->mRadii, sizeof(mRadii)) != 0)
    return false;
  // No need to track merged frames for clipping
  MergeFrom(other);
  return true;
}

nsDisplayZoom::nsDisplayZoom(nsDisplayListBuilder* aBuilder,
                             nsIFrame* aFrame, nsDisplayList* aList,
                             PRInt32 aAPD, PRInt32 aParentAPD)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList), mAPD(aAPD),
      mParentAPD(aParentAPD) {
  MOZ_COUNT_CTOR(nsDisplayZoom);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayZoom::~nsDisplayZoom() {
  MOZ_COUNT_DTOR(nsDisplayZoom);
}
#endif

nsRect nsDisplayZoom::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  nsRect bounds = nsDisplayWrapList::GetBounds(aBuilder, aSnap);
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
  nsRegion visibleRegion =
    aVisibleRegion->ConvertAppUnitsRoundOut(mParentAPD, mAPD);
  nsRegion originalVisibleRegion = visibleRegion;

  nsRect transformedVisibleRect =
    mVisibleRect.ConvertAppUnitsRoundOut(mParentAPD, mAPD);
  nsRect allowExpansion =
    aAllowVisibleRegionExpansion.ConvertAppUnitsRoundIn(mParentAPD, mAPD);
  bool retval =
    mList.ComputeVisibilityForSublist(aBuilder, &visibleRegion,
                                      transformedVisibleRect,
                                      allowExpansion);

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
  for (const nsIFrame *currFrame = aFrame->GetFirstContinuation();
       currFrame != nsnull;
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

/* Returns the delta specified by the -moz-transform-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the transform origin.  This function is
 * called off the main thread.
 */
/* static */ gfxPoint3D
nsDisplayTransform::GetDeltaToMozTransformOrigin(const nsIFrame* aFrame,
                                                 float aAppUnitsPerPixel,
                                                 const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->IsTransformed(),
                  "Shouldn't get a delta for an untransformed frame!");

  /* For both of the coordinates, if the value of -moz-transform is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */
  const nsStyleDisplay* display = aFrame->GetStyleDisplay();
  nsRect boundingRect = (aBoundsOverride ? *aBoundsOverride :
                         nsDisplayTransform::GetFrameBoundsForTransform(aFrame));

  /* Allows us to access named variables by index. */
  float coords[3];
  const nscoord* dimensions[2] =
    {&boundingRect.width, &boundingRect.height};

  for (PRUint8 index = 0; index < 2; ++index) {
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
nsDisplayTransform::GetDeltaToMozPerspectiveOrigin(const nsIFrame* aFrame,
                                                   float aAppUnitsPerPixel)
{
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->IsTransformed(),
                  "Shouldn't get a delta for an untransformed frame!");
  NS_PRECONDITION(aFrame->GetParentStyleContextFrame(),
                  "Can't get delta without a style parent!");

  /* For both of the coordinates, if the value of -moz-perspective-origin is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */

  //TODO: Should this be using our bounds or the parent's bounds?
  // How do we handle aBoundsOverride in the latter case?
  nsIFrame* parent = aFrame->GetParentStyleContextFrame();
  const nsStyleDisplay* display = parent->GetStyleDisplay();
  nsRect boundingRect = nsDisplayTransform::GetFrameBoundsForTransform(parent);

  /* Allows us to access named variables by index. */
  gfxPoint3D result;
  result.z = 0.0f;
  gfxFloat* coords[2] = {&result.x, &result.y};
  const nscoord* dimensions[2] =
    {&boundingRect.width, &boundingRect.height};

  for (PRUint8 index = 0; index < 2; ++index) {
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

/* Wraps up the -moz-transform matrix in a change-of-basis matrix pair that
 * translates from local coordinate space to transform coordinate space, then
 * hands it back.
 */
gfx3DMatrix
nsDisplayTransform::GetResultingTransformMatrix(const nsIFrame* aFrame,
                                                const nsPoint& aOrigin,
                                                float aAppUnitsPerPixel,
                                                const nsRect* aBoundsOverride,
                                                const nsCSSValueList* aTransformOverride,
                                                gfxPoint3D* aToMozOrigin,
                                                gfxPoint3D* aToPerspectiveOrigin,
                                                nscoord* aChildPerspective,
                                                nsIFrame** aOutAncestor)
{
  NS_PRECONDITION(aFrame || (aToMozOrigin && aBoundsOverride && aToPerspectiveOrigin &&
                             aTransformOverride && aChildPerspective),
                  "Should have frame or necessary infromation to construct matrix");

  NS_PRECONDITION(!(aFrame && (aToMozOrigin || aToPerspectiveOrigin ||
                             aTransformOverride || aChildPerspective)),
                  "Should not have both frame and necessary infromation to construct matrix");

  if (aOutAncestor) {
      *aOutAncestor = nsLayoutUtils::GetCrossDocParentFrame(aFrame);
  }

  /* Account for the -moz-transform-origin property by translating the
   * coordinate space to the new origin.
   */
  gfxPoint3D toMozOrigin =
    aFrame ? GetDeltaToMozTransformOrigin(aFrame, aAppUnitsPerPixel, aBoundsOverride) : *aToMozOrigin;
  gfxPoint3D newOrigin =
    gfxPoint3D(NSAppUnitsToFloatPixels(aOrigin.x, aAppUnitsPerPixel),
               NSAppUnitsToFloatPixels(aOrigin.y, aAppUnitsPerPixel),
               0.0f);

  /* Get the underlying transform matrix.  This requires us to get the
   * bounds of the frame.
   */
  const nsStyleDisplay* disp = aFrame ? aFrame->GetStyleDisplay() : nsnull;
  nsRect bounds = (aBoundsOverride ? *aBoundsOverride :
                   nsDisplayTransform::GetFrameBoundsForTransform(aFrame));

  /* Get the matrix, then change its basis to factor in the origin. */
  bool dummy;
  gfx3DMatrix result;
  // Call IsSVGTransformed() regardless of the value of
  // disp->mSpecifiedTransform, since we still need any transformFromSVGParent.
  gfxMatrix svgTransform, transformFromSVGParent;
  bool hasSVGTransforms =
    aFrame && aFrame->IsSVGTransformed(&svgTransform, &transformFromSVGParent);
  /* Transformed frames always have a transform, or are preserving 3d (and might still have perspective!) */
  if (aTransformOverride) {
    result = nsStyleTransformMatrix::ReadTransforms(aTransformOverride, nsnull, nsnull,
                                                    dummy, bounds, aAppUnitsPerPixel);
  } else if (disp->mSpecifiedTransform) {
    result = nsStyleTransformMatrix::ReadTransforms(disp->mSpecifiedTransform,
                                                    aFrame->GetStyleContext(),
                                                    aFrame->PresContext(),
                                                    dummy, bounds, aAppUnitsPerPixel);
  } else if (hasSVGTransforms) {
    // Correct the translation components for zoom:
    float pixelsPerCSSPx = aFrame->PresContext()->AppUnitsPerCSSPixel() /
                             aAppUnitsPerPixel;
    svgTransform.x0 *= pixelsPerCSSPx;
    svgTransform.y0 *= pixelsPerCSSPx;
    result = gfx3DMatrix::From2D(svgTransform);
  }

  if (hasSVGTransforms && !transformFromSVGParent.IsIdentity()) {
    // Correct the translation components for zoom:
    float pixelsPerCSSPx = aFrame->PresContext()->AppUnitsPerCSSPixel() /
                             aAppUnitsPerPixel;
    transformFromSVGParent.x0 *= pixelsPerCSSPx;
    transformFromSVGParent.y0 *= pixelsPerCSSPx;
    result = result * gfx3DMatrix::From2D(transformFromSVGParent);
  }

  const nsStyleDisplay* parentDisp = nsnull;
  nsStyleContext* parentStyleContext = aFrame ? aFrame->GetStyleContext()->GetParent(): nsnull;
  if (parentStyleContext) {
    parentDisp = parentStyleContext->GetStyleDisplay();
  }
  nscoord perspectiveCoord = 0;
  if (parentDisp && parentDisp->mChildPerspective.GetUnit() == eStyleUnit_Coord) {
    perspectiveCoord = parentDisp->mChildPerspective.GetCoordValue();
  }
  if (aChildPerspective) {
    perspectiveCoord = *aChildPerspective;
  }

  if (nsLayoutUtils::Are3DTransformsEnabled() && perspectiveCoord > 0.0) {
    gfx3DMatrix perspective;
    perspective._34 =
      -1.0 / NSAppUnitsToFloatPixels(perspectiveCoord, aAppUnitsPerPixel);
    /* At the point when perspective is applied, we have been translated to the transform origin.
     * The translation to the perspective origin is the difference between these values.
     */
    gfxPoint3D toPerspectiveOrigin = aFrame ? GetDeltaToMozPerspectiveOrigin(aFrame, aAppUnitsPerPixel) : *aToPerspectiveOrigin;
    result = result * nsLayoutUtils::ChangeMatrixBasis(toPerspectiveOrigin - toMozOrigin, perspective);
  }

  if (aFrame && aFrame->Preserves3D() && nsLayoutUtils::Are3DTransformsEnabled()) {
      // Include the transform set on our parent
      NS_ASSERTION(aFrame->GetParent() &&
                   aFrame->GetParent()->IsTransformed() &&
                   aFrame->GetParent()->Preserves3DChildren(),
                   "Preserve3D mismatch!");
      gfx3DMatrix parent =
        GetResultingTransformMatrix(aFrame->GetParent(),
                                    aOrigin - aFrame->GetPosition(),
                                    aAppUnitsPerPixel, nsnull, nsnull, nsnull,
                                    nsnull, nsnull, aOutAncestor);
      return nsLayoutUtils::ChangeMatrixBasis(newOrigin + toMozOrigin, result) * parent;
  }

  return nsLayoutUtils::ChangeMatrixBasis
    (newOrigin + toMozOrigin, result);
}

bool
nsDisplayTransform::ShouldPrerenderTransformedContent(nsDisplayListBuilder* aBuilder,
                                                      nsIFrame* aFrame)
{
  if (aFrame->AreLayersMarkedActive(nsChangeHint_UpdateTransformLayer)) {
    nsSize refSize = aBuilder->ReferenceFrame()->GetSize();
    // Only prerender if the transformed frame's size is <= the
    // reference frame size (~viewport), allowing a 1/8th fuzz factor
    // for shadows, borders, etc.
    refSize += nsSize(refSize.width / 8, refSize.height / 8);
    if (aFrame->GetVisualOverflowRectRelativeToSelf().Size() <= refSize) {
      // Bug 717521 - pre-render max 4096 x 4096 device pixels.
      nscoord max = aFrame->PresContext()->DevPixelsToAppUnits(4096);
      nsRect visual = aFrame->GetVisualOverflowRect();
      if (visual.width <= max && visual.height <= max) {
        return true;
      }
    }
  }
  return false;
}

/* If the matrix is singular, or a hidden backface is shown, the frame won't be visible or hit. */
static bool IsFrameVisible(nsIFrame* aFrame, const gfx3DMatrix& aMatrix)
{
  if (aMatrix.IsSingular()) {
    return false;
  }
  if (aFrame->GetStyleDisplay()->mBackfaceVisibility == NS_STYLE_BACKFACE_VISIBILITY_HIDDEN &&
      aMatrix.IsBackfaceVisible()) {
    return false;
  }
  return true;
}

const gfx3DMatrix&
nsDisplayTransform::GetTransform(float aAppUnitsPerPixel)
{
  if (mTransform.IsIdentity() || mCachedAppUnitsPerPixel != aAppUnitsPerPixel) {
    mTransform =
      GetResultingTransformMatrix(mFrame, ToReferenceFrame(),
                                  aAppUnitsPerPixel);
    mCachedAppUnitsPerPixel = aAppUnitsPerPixel;
  }
  return mTransform;
}

already_AddRefed<Layer> nsDisplayTransform::BuildLayer(nsDisplayListBuilder *aBuilder,
                                                       LayerManager *aManager,
                                                       const ContainerParameters& aContainerParameters)
{
  const gfx3DMatrix& newTransformMatrix =
    GetTransform(mFrame->PresContext()->AppUnitsPerDevPixel());

  if (!IsFrameVisible(mFrame, newTransformMatrix)) {
    return nsnull;
  }

  nsRefPtr<ContainerLayer> container = GetLayerBuilderForManager(aManager)->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, *mStoredList.GetList(),
                           aContainerParameters, &newTransformMatrix);

  // Add the preserve-3d flag for this layer, BuildContainerLayerFor clears all flags,
  // so we never need to explicitely unset this flag.
  if (mFrame->Preserves3D() || mFrame->Preserves3DChildren()) {
    container->SetContentFlags(container->GetContentFlags() | Layer::CONTENT_PRESERVE_3D);
  }

  container->ClearAnimations();
  ElementAnimations* ea =
    nsAnimationManager::GetAnimationsForCompositor(mFrame->GetContent(),
                                                   eCSSProperty_transform);
  AddTransformAnimations(ea, container, ToReferenceFrame());

  return container.forget();
}

nsDisplayItem::LayerState
nsDisplayTransform::GetLayerState(nsDisplayListBuilder* aBuilder,
                                  LayerManager* aManager,
                                  const ContainerParameters& aParameters) {
  // Here we check if the *post-transform* bounds of this item are big enough
  // to justify an active layer.
  if (mFrame->AreLayersMarkedActive(nsChangeHint_UpdateTransformLayer) &&
      !IsItemTooSmallForActiveLayer(this))
    return LAYER_ACTIVE;
  if (!GetTransform(mFrame->PresContext()->AppUnitsPerDevPixel()).Is2D() || mFrame->Preserves3D())
    return LAYER_ACTIVE;
  if (mFrame->GetContent()) {
    if (nsAnimationManager::GetAnimationsForCompositor(mFrame->GetContent(),
                                                       eCSSProperty_transform)) {
      return LAYER_ACTIVE;
    }
  }
  nsIFrame* activeScrolledRoot =
    nsLayoutUtils::GetActiveScrolledRootFor(mFrame, nsnull);
  return !mStoredList.ChildrenCanBeInactive(aBuilder,
                                            aManager,
                                            aParameters,
                                            *mStoredList.GetList(),
                                            activeScrolledRoot)
      ? LAYER_ACTIVE : LAYER_INACTIVE;
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
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  if (ShouldPrerenderTransformedContent(aBuilder, mFrame) ||
      !UntransformRectMatrix(mVisibleRect,
                             GetTransform(factor),
                             factor,
                             &untransformedVisibleRect))
  {
    untransformedVisibleRect = mFrame->GetVisualOverflowRectRelativeToSelf() +
                               aBuilder->ToReferenceFrame(mFrame);
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
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  gfx3DMatrix matrix = GetTransform(factor);

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
    gfxPoint point = matrix.Inverse().ProjectPoint(
                       gfxPoint(NSAppUnitsToFloatPixels(aRect.x, factor),
                                NSAppUnitsToFloatPixels(aRect.y, factor)));

    resultingRect = nsRect(NSFloatPixelsToAppUnits(float(point.x), factor),
                           NSFloatPixelsToAppUnits(float(point.y), factor),
                           1, 1);

  } else {
    gfxRect originalRect(NSAppUnitsToFloatPixels(aRect.x, factor),
                         NSAppUnitsToFloatPixels(aRect.y, factor),
                         NSAppUnitsToFloatPixels(aRect.width, factor),
                         NSAppUnitsToFloatPixels(aRect.height, factor));

    gfxRect rect = matrix.Inverse().ProjectRectBounds(originalRect);;

    resultingRect = nsRect(NSFloatPixelsToAppUnits(float(rect.X()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Y()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Width()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Height()), factor));
  }


#ifdef DEBUG_HIT
  printf("Frame: %p\n", dynamic_cast<void *>(mFrame));
  printf("  Untransformed point: (%f, %f)\n", resultingRect.X(), resultingRect.Y());
  PRUint32 originalFrameCount = aOutFrames.Length();
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
nsDisplayTransform::GetHitDepthAtPoint(const nsPoint& aPoint)
{
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  gfx3DMatrix matrix = GetTransform(factor);

  NS_ASSERTION(IsFrameVisible(mFrame, matrix), "We can't have hit a frame that isn't visible!");

  gfxPoint point =
    matrix.Inverse().ProjectPoint(gfxPoint(NSAppUnitsToFloatPixels(aPoint.x, factor),
                                           NSAppUnitsToFloatPixels(aPoint.y, factor)));

  gfxPoint3D transformed = matrix.Transform3D(gfxPoint3D(point.x, point.y, 0));
  return transformed.z;
}

/* The bounding rectangle for the object is the overflow rectangle translated
 * by the reference point.
 */
nsRect nsDisplayTransform::GetBounds(nsDisplayListBuilder *aBuilder, bool* aSnap)
{
  nsRect untransformedBounds =
    ShouldPrerenderTransformedContent(aBuilder, mFrame) ?
    mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame() :
    mStoredList.GetBounds(aBuilder, aSnap);
  *aSnap = false;
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  return nsLayoutUtils::MatrixTransformRect(untransformedBounds,
                                            GetTransform(factor),
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
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  if (!UntransformRectMatrix(mVisibleRect, GetTransform(factor), factor, &untransformedVisible)) {
      return nsRegion();
  }

  const gfx3DMatrix& matrix = GetTransform(nsPresContext::AppUnitsPerCSSPixel());

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
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  if (!UntransformRectMatrix(mVisibleRect, GetTransform(factor), factor, &untransformedVisible)) {
    return false;
  }
  const gfx3DMatrix& matrix = GetTransform(nsPresContext::AppUnitsPerCSSPixel());

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
  if (aItem->GetUnderlyingFrame()->GetContent() != mFrame->GetContent())
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

  float factor = nsPresContext::AppUnitsPerCSSPixel();
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

  float factor = nsPresContext::AppUnitsPerCSSPixel();
  return nsLayoutUtils::MatrixTransformRectOut
    (aUntransformedBounds,
     GetResultingTransformMatrix(aFrame, aOrigin, factor, aBoundsOverride),
     factor);
}

bool nsDisplayTransform::UntransformRectMatrix(const nsRect &aUntransformedBounds,
                                               const gfx3DMatrix& aMatrix,
                                               float aAppUnitsPerPixel,
                                               nsRect *aOutRect)
{
  if (aMatrix.IsSingular())
    return false;

  gfxRect result(NSAppUnitsToFloatPixels(aUntransformedBounds.x, aAppUnitsPerPixel),
                 NSAppUnitsToFloatPixels(aUntransformedBounds.y, aAppUnitsPerPixel),
                 NSAppUnitsToFloatPixels(aUntransformedBounds.width, aAppUnitsPerPixel),
                 NSAppUnitsToFloatPixels(aUntransformedBounds.height, aAppUnitsPerPixel));

  /* We want to untransform the matrix, so invert the transformation first! */
  result = aMatrix.Inverse().ProjectRectBounds(result);

  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(result, aAppUnitsPerPixel);

  return true;
}

bool nsDisplayTransform::UntransformRect(const nsRect &aUntransformedBounds,
                                           const nsIFrame* aFrame,
                                           const nsPoint &aOrigin,
                                           nsRect* aOutRect)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");

  /* Grab the matrix.  If the transform is degenerate, just hand back the
   * empty rect.
   */
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  gfx3DMatrix matrix = GetResultingTransformMatrix(aFrame, aOrigin, factor);

  return UntransformRectMatrix(aUntransformedBounds, matrix, factor, aOutRect);
}

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
                                   const ContainerParameters& aParameters)
{
  return LAYER_SVG_EFFECTS;
}

already_AddRefed<Layer>
nsDisplaySVGEffects::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerParameters& aContainerParameters)
{
  const nsIContent* content = mFrame->GetContent();
  bool hasSVGLayout = (mFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  if (hasSVGLayout) {
    nsISVGChildFrame *svgChildFrame = do_QueryFrame(mFrame);
    if (!svgChildFrame || !mFrame->GetContent()->IsSVG()) {
      NS_ASSERTION(false, "why?");
      return nsnull;
    }
    if (!static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
      return nsnull; // The SVG spec says not to draw filters for this
    }
  }

  float opacity = mFrame->GetStyleDisplay()->mOpacity;
  if (opacity == 0.0f)
    return nsnull;

  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(mFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  bool isOK = true;
  effectProperties.GetClipPathFrame(&isOK);
  effectProperties.GetMaskFrame(&isOK);
  effectProperties.GetFilterFrame(&isOK);

  if (!isOK) {
    return nsnull;
  }

  nsRefPtr<ContainerLayer> container = GetLayerBuilderForManager(aManager)->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           aContainerParameters, nsnull);

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
  if (aItem->GetUnderlyingFrame()->GetContent() != mFrame->GetContent())
    return false;
  nsDisplaySVGEffects* other = static_cast<nsDisplaySVGEffects*>(aItem);
  MergeFromTrackingMergedFrames(other);
  mEffectsBounds.UnionRect(mEffectsBounds,
    other->mEffectsBounds + other->mFrame->GetOffsetTo(mFrame));
  return true;
}

#ifdef MOZ_DUMP_PAINTING
void
nsDisplaySVGEffects::PrintEffects(FILE* aOutput)
{
  nsIFrame* firstFrame =
    nsLayoutUtils::GetFirstContinuationOrSpecialSibling(mFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);
  bool isOK = true;
  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame(&isOK);
  bool first = true;
  fprintf(aOutput, " effects=(");
  if (mFrame->GetStyleDisplay()->mOpacity != 1.0f) {
    first = false;
    fprintf(aOutput, "opacity(%f)", mFrame->GetStyleDisplay()->mOpacity);
  }
  if (clipPathFrame) {
    if (!first) {
      fprintf(aOutput, ", ");
    }
    fprintf(aOutput, "clip(%s)", clipPathFrame->IsTrivial() ? "trivial" : "non-trivial");
    first = false;
  }
  if (effectProperties.GetFilterFrame(&isOK)) {
    if (!first) {
      fprintf(aOutput, ", ");
    }
    fprintf(aOutput, "filter");
    first = false;
  }
  if (effectProperties.GetMaskFrame(&isOK)) {
    if (!first) {
      fprintf(aOutput, ", ");
    }
    fprintf(aOutput, "mask");
  }
  fprintf(aOutput, ")");
}
#endif

