/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRangeFrame.h"

#include "nsContentCreatorFunctions.h"
#include "nsContentList.h"
#include "nsContentUtils.h"
#include "nsCSSRenderingBorders.h"
#include "nsFontMetrics.h"
#include "nsFormControlFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "nsPresContext.h"
#include "nsNodeInfoManager.h"
#include "nsRenderingContext.h"
#include "mozilla/dom/Element.h"
#include "prtypes.h"

#include <algorithm>

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

#define LONG_SIDE_TO_SHORT_SIDE_RATIO 10

using namespace mozilla;

nsIFrame*
NS_NewRangeFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsRangeFrame(aContext);
}

nsRangeFrame::nsRangeFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
{
}

nsRangeFrame::~nsRangeFrame()
{
}

NS_IMPL_FRAMEARENA_HELPERS(nsRangeFrame)

NS_QUERYFRAME_HEAD(nsRangeFrame)
  NS_QUERYFRAME_ENTRY(nsRangeFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void
nsRangeFrame::Init(nsIContent* aContent,
                   nsIFrame*   aParent,
                   nsIFrame*   aPrevInFlow)
{
  // B2G's AsyncPanZoomController::ReceiveInputEvent handles touch events
  // without checking whether the out-of-process document that it controls
  // will handle them, unless it has been told that the document might do so.
  // This is for perf reasons, otherwise it has to wait for the event to be
  // round-tripped to the other process and back, delaying panning, etc.
  // We must call SetHasTouchEventListeners() in order to get APZC to wait
  // until the event has been round-tripped and check whether it has been
  // handled, otherwise B2G will end up panning the document when the user
  // tries to drag our thumb.
  //
  nsIPresShell* presShell = PresContext()->GetPresShell();
  if (presShell) {
    nsIDocument* document = presShell->GetDocument();
    if (document) {
      nsPIDOMWindow* innerWin = document->GetInnerWindow();
      if (innerWin) {
        innerWin->SetHasTouchEventListeners();
      }
    }
  }
  return nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

void
nsRangeFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_ASSERTION(!GetPrevContinuation() && !GetNextContinuation(),
               "nsRangeFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsContentUtils::DestroyAnonymousContent(&mTrackDiv);
  nsContentUtils::DestroyAnonymousContent(&mProgressDiv);
  nsContentUtils::DestroyAnonymousContent(&mThumbDiv);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

nsresult
nsRangeFrame::MakeAnonymousDiv(nsIContent** aResult,
                               nsCSSPseudoElements::Type aPseudoType,
                               nsTArray<ContentInfo>& aElements)
{
  // Get the NodeInfoManager and tag necessary to create the anonymous divs.
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::div, nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = NS_NewHTMLElement(aResult, nodeInfo.forget(),
                                  dom::NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);
  // Associate the pseudo-element with the anonymous child.
  nsRefPtr<nsStyleContext> newStyleContext =
    PresContext()->StyleSet()->ResolvePseudoElementStyle(mContent->AsElement(),
                                                         aPseudoType,
                                                         StyleContext());

  if (!aElements.AppendElement(ContentInfo(*aResult, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
nsRangeFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  nsresult rv;

  // Create the ::-moz-range-track pseuto-element (a div):
  rv = MakeAnonymousDiv(getter_AddRefs(mTrackDiv),
                        nsCSSPseudoElements::ePseudo_mozRangeTrack,
                        aElements);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the ::-moz-range-progress pseudo-element (a div):
  rv = MakeAnonymousDiv(getter_AddRefs(mProgressDiv),
                        nsCSSPseudoElements::ePseudo_mozRangeProgress,
                        aElements);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the ::-moz-range-thumb pseudo-element (a div):
  rv = MakeAnonymousDiv(getter_AddRefs(mThumbDiv),
                        nsCSSPseudoElements::ePseudo_mozRangeThumb,
                        aElements);
  return rv;
}

void
nsRangeFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                       uint32_t aFilter)
{
  aElements.MaybeAppendElement(mTrackDiv);
  aElements.MaybeAppendElement(mProgressDiv);
  aElements.MaybeAppendElement(mThumbDiv);
}

class nsDisplayRangeFocusRing : public nsDisplayItem
{
public:
  nsDisplayRangeFocusRing(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayRangeFocusRing);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayRangeFocusRing() {
    MOZ_COUNT_DTOR(nsDisplayRangeFocusRing);
  }
#endif
  
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
  NS_DISPLAY_DECL_NAME("RangeFocusRing", TYPE_OUTLINE)
};

void
nsDisplayRangeFocusRing::Paint(nsDisplayListBuilder* aBuilder,
                               nsRenderingContext* aCtx)
{
  nsPresContext *presContext = mFrame->PresContext();
  nscoord appUnitsPerDevPixel = presContext->DevPixelsToAppUnits(1);
  gfxContext* ctx = aCtx->ThebesContext();
  nsRect r = nsRect(ToReferenceFrame(), mFrame->GetSize());
  gfxRect pxRect(nsLayoutUtils::RectToGfxRect(r, appUnitsPerDevPixel));
  uint8_t borderStyles[4] = { NS_STYLE_BORDER_STYLE_DOTTED,
                              NS_STYLE_BORDER_STYLE_DOTTED,
                              NS_STYLE_BORDER_STYLE_DOTTED,
                              NS_STYLE_BORDER_STYLE_DOTTED };
  gfxFloat borderWidths[4] = { 1, 1, 1, 1 };
  gfxCornerSizes borderRadii(0);
  nscolor borderColors[4] = { NS_RGB(0, 0, 0), NS_RGB(0, 0, 0),
                              NS_RGB(0, 0, 0), NS_RGB(0, 0, 0) };
  nsStyleContext* bgContext = mFrame->StyleContext();
  nscolor bgColor =
    bgContext->GetVisitedDependentColor(eCSSProperty_background_color);

  ctx->Save();
  nsCSSBorderRenderer br(appUnitsPerDevPixel,
                         ctx,
                         pxRect,
                         borderStyles,
                         borderWidths,
                         borderRadii,
                         borderColors,
                         nullptr,
                         0,
                         bgColor);
  br.DrawBorders();
  ctx->Restore();
}

void
nsRangeFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  if (IsThemed()) {
    DisplayBorderBackgroundOutline(aBuilder, aLists);
    // Only create items for the thumb. Specifically, we do not want
    // the track to paint, since *our* background is used to paint
    // the track, and we don't want the unthemed track painting over
    // the top of the themed track.
    // This logic is copied from
    // nsContainerFrame::BuildDisplayListForNonBlockChildren as
    // called by BuildDisplayListForInline.
    nsIFrame* thumb = mThumbDiv->GetPrimaryFrame();
    if (thumb) {
      nsDisplayListSet set(aLists, aLists.Content());
      BuildDisplayListForChild(aBuilder, thumb, aDirtyRect, set, DISPLAY_CHILD_INLINE);
    }
  } else {
    BuildDisplayListForInline(aBuilder, aDirtyRect, aLists);
  }

  // Draw a focus outline if appropriate:
  nsEventStates eventStates = mContent->AsElement()->State();
  if (!eventStates.HasState(NS_EVENT_STATE_FOCUSRING) ||
      eventStates.HasState(NS_EVENT_STATE_DISABLED)) {
    return;
  }
  nsPresContext *presContext = PresContext();
  const nsStyleDisplay *disp = StyleDisplay();
  if ((!IsThemed(disp) ||
       !presContext->GetTheme()->ThemeDrawsFocusForWidget(disp->mAppearance)) &&
      IsVisibleForPainting(aBuilder)) {
    aLists.Content()->AppendNewToTop(
      new (aBuilder) nsDisplayRangeFocusRing(aBuilder, this));
  }
}

NS_IMETHODIMP
nsRangeFrame::Reflow(nsPresContext*           aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsRangeFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_ASSERTION(mTrackDiv, "::-moz-range-track div must exist!");
  NS_ASSERTION(mProgressDiv, "::-moz-range-progress div must exist!");
  NS_ASSERTION(mThumbDiv, "::-moz-range-thumb div must exist!");
  NS_ASSERTION(!GetPrevContinuation() && !GetNextContinuation(),
               "nsRangeFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }

  nscoord computedHeight = aReflowState.ComputedHeight();
  if (computedHeight == NS_AUTOHEIGHT) {
    computedHeight = 0;
  }
  aDesiredSize.width = aReflowState.ComputedWidth() +
                       aReflowState.mComputedBorderPadding.LeftRight();
  aDesiredSize.height = computedHeight +
                        aReflowState.mComputedBorderPadding.TopBottom();

  nsresult rv =
    ReflowAnonymousContent(aPresContext, aDesiredSize, aReflowState);
  NS_ENSURE_SUCCESS(rv, rv);

  aDesiredSize.SetOverflowAreasToDesiredBounds();

  nsIFrame* trackFrame = mTrackDiv->GetPrimaryFrame();
  if (trackFrame) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, trackFrame);
  }

  nsIFrame* rangeProgressFrame = mProgressDiv->GetPrimaryFrame();
  if (rangeProgressFrame) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, rangeProgressFrame);
  }

  nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();
  if (thumbFrame) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, thumbFrame);
  }

  FinishAndStoreOverflow(&aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  return NS_OK;
}

nsresult
nsRangeFrame::ReflowAnonymousContent(nsPresContext*           aPresContext,
                                     nsHTMLReflowMetrics&     aDesiredSize,
                                     const nsHTMLReflowState& aReflowState)
{
  // The width/height of our content box, which is the available width/height
  // for our anonymous content:
  nscoord rangeFrameContentBoxWidth = aReflowState.ComputedWidth();
  nscoord rangeFrameContentBoxHeight = aReflowState.ComputedHeight();
  if (rangeFrameContentBoxHeight == NS_AUTOHEIGHT) {
    rangeFrameContentBoxHeight = 0;
  }

  nsIFrame* trackFrame = mTrackDiv->GetPrimaryFrame();

  if (trackFrame) { // display:none?

    // Position the track:
    // The idea here is that we allow content authors to style the width,
    // height, border and padding of the track, but we ignore margin and
    // positioning properties and do the positioning ourself to keep the center
    // of the track's border box on the center of the nsRangeFrame's content
    // box.

    nsHTMLReflowState trackReflowState(aPresContext, aReflowState, trackFrame,
                                       nsSize(aReflowState.ComputedWidth(),
                                              NS_UNCONSTRAINEDSIZE));

    // Find the x/y position of the track frame such that it will be positioned
    // as described above. These coordinates are with respect to the
    // nsRangeFrame's border-box.
    nscoord trackX = rangeFrameContentBoxWidth / 2;
    nscoord trackY = rangeFrameContentBoxHeight / 2;

    // Account for the track's border and padding (we ignore its margin):
    trackX -= trackReflowState.mComputedBorderPadding.left +
                trackReflowState.ComputedWidth() / 2;
    trackY -= trackReflowState.mComputedBorderPadding.top +
                trackReflowState.ComputedHeight() / 2;

    // Make relative to our border box instead of our content box:
    trackX += aReflowState.mComputedBorderPadding.left;
    trackY += aReflowState.mComputedBorderPadding.top;

    nsReflowStatus frameStatus;
    nsHTMLReflowMetrics trackDesiredSize;
    nsresult rv = ReflowChild(trackFrame, aPresContext, trackDesiredSize,
                              trackReflowState, trackX, trackY, 0, frameStatus);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(NS_FRAME_IS_FULLY_COMPLETE(frameStatus),
               "We gave our child unconstrained height, so it should be complete");
    rv = FinishReflowChild(trackFrame, aPresContext, &trackReflowState,
                           trackDesiredSize, trackX, trackY, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();

  if (thumbFrame) { // display:none?
    nsHTMLReflowState thumbReflowState(aPresContext, aReflowState, thumbFrame,
                                       nsSize(aReflowState.ComputedWidth(),
                                              NS_UNCONSTRAINEDSIZE));

    // Where we position the thumb depends on its size, so we first reflow
    // the thumb at {0,0} to obtain its size, then position it afterwards.

    nsReflowStatus frameStatus;
    nsHTMLReflowMetrics thumbDesiredSize;
    nsresult rv = ReflowChild(thumbFrame, aPresContext, thumbDesiredSize,
                              thumbReflowState, 0, 0, 0, frameStatus);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(NS_FRAME_IS_FULLY_COMPLETE(frameStatus),
               "We gave our child unconstrained height, so it should be complete");
    rv = FinishReflowChild(thumbFrame, aPresContext, &thumbReflowState,
                           thumbDesiredSize, 0, 0, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    DoUpdateThumbPosition(thumbFrame, nsSize(aDesiredSize.width,
                                             aDesiredSize.height));
  }

  nsIFrame* rangeProgressFrame = mProgressDiv->GetPrimaryFrame();

  if (rangeProgressFrame) { // display:none?
    nsHTMLReflowState progressReflowState(aPresContext, aReflowState,
                                          rangeProgressFrame,
                                          nsSize(aReflowState.ComputedWidth(),
                                                 NS_UNCONSTRAINEDSIZE));

    // We first reflow the range-progress frame at {0,0} to obtain its
    // unadjusted dimensions, then we adjust it to so that the appropriate edge
    // ends at the thumb.

    nsReflowStatus frameStatus;
    nsHTMLReflowMetrics progressDesiredSize;
    nsresult rv = ReflowChild(rangeProgressFrame, aPresContext,
                              progressDesiredSize, progressReflowState, 0, 0,
                              0, frameStatus);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(NS_FRAME_IS_FULLY_COMPLETE(frameStatus),
               "We gave our child unconstrained height, so it should be complete");
    rv = FinishReflowChild(rangeProgressFrame, aPresContext,
                           &progressReflowState, progressDesiredSize, 0, 0, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    DoUpdateRangeProgressFrame(rangeProgressFrame, nsSize(aDesiredSize.width,
                                                          aDesiredSize.height));
  }

  return NS_OK;
}

#ifdef ACCESSIBILITY
a11y::AccType
nsRangeFrame::AccessibleType()
{
  return a11y::eHTMLRangeType;
}
#endif

double
nsRangeFrame::GetValueAsFractionOfRange()
{
  MOZ_ASSERT(mContent->IsHTML(nsGkAtoms::input), "bad cast");
  dom::HTMLInputElement* input = static_cast<dom::HTMLInputElement*>(mContent);

  MOZ_ASSERT(input->GetType() == NS_FORM_INPUT_RANGE);

  Decimal value = input->GetValueAsDecimal();
  Decimal minimum = input->GetMinimum();
  Decimal maximum = input->GetMaximum();

  MOZ_ASSERT(value.isFinite() && minimum.isFinite() && maximum.isFinite(),
             "type=range should have a default maximum/minimum");
  
  if (maximum <= minimum) {
    MOZ_ASSERT(value == minimum, "Unsanitized value");
    return 0.0;
  }
  
  MOZ_ASSERT(value >= minimum && value <= maximum, "Unsanitized value");
  
  return ((value - minimum) / (maximum - minimum)).toDouble();
}

Decimal
nsRangeFrame::GetValueAtEventPoint(nsGUIEvent* aEvent)
{
  MOZ_ASSERT(aEvent->eventStructType == NS_MOUSE_EVENT ||
             aEvent->eventStructType == NS_TOUCH_EVENT,
             "Unexpected event type - aEvent->refPoint may be meaningless");

  MOZ_ASSERT(mContent->IsHTML(nsGkAtoms::input), "bad cast");
  dom::HTMLInputElement* input = static_cast<dom::HTMLInputElement*>(mContent);

  MOZ_ASSERT(input->GetType() == NS_FORM_INPUT_RANGE);

  Decimal minimum = input->GetMinimum();
  Decimal maximum = input->GetMaximum();
  MOZ_ASSERT(minimum.isFinite() && maximum.isFinite(),
             "type=range should have a default maximum/minimum");
  if (maximum <= minimum) {
    return minimum;
  }
  Decimal range = maximum - minimum;

  LayoutDeviceIntPoint absPoint;
  if (aEvent->eventStructType == NS_TOUCH_EVENT) {
    MOZ_ASSERT(static_cast<nsTouchEvent*>(aEvent)->touches.Length() == 1,
               "Unexpected number of touches");
    absPoint = LayoutDeviceIntPoint::FromUntyped(
      static_cast<nsTouchEvent*>(aEvent)->touches[0]->mRefPoint);
  } else {
    absPoint = aEvent->refPoint;
  }
  nsPoint point =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, 
      LayoutDeviceIntPoint::ToUntyped(absPoint), this);

  if (point == nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE)) {
    // We don't want to change the current value for this error state.
    return static_cast<dom::HTMLInputElement*>(mContent)->GetValueAsDecimal();
  }

  nsRect rangeContentRect = GetContentRectRelativeToSelf();
  nsSize thumbSize;

  if (IsThemed()) {
    // We need to get the size of the thumb from the theme.
    nsPresContext *presContext = PresContext();
    nsRefPtr<nsRenderingContext> tmpCtx =
      presContext->PresShell()->GetReferenceRenderingContext();
    bool notUsedCanOverride;
    nsIntSize size;
    presContext->GetTheme()->
      GetMinimumWidgetSize(tmpCtx.get(), this, NS_THEME_RANGE_THUMB, &size,
                           &notUsedCanOverride);
    thumbSize.width = presContext->DevPixelsToAppUnits(size.width);
    thumbSize.height = presContext->DevPixelsToAppUnits(size.height);
    MOZ_ASSERT(thumbSize.width > 0 && thumbSize.height > 0);
  } else {
    nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();
    if (thumbFrame) { // diplay:none?
      thumbSize = thumbFrame->GetSize();
    }
  }

  Decimal fraction;
  if (IsHorizontal()) {
    nscoord traversableDistance = rangeContentRect.width - thumbSize.width;
    if (traversableDistance <= 0) {
      return minimum;
    }
    nscoord posAtStart = rangeContentRect.x + thumbSize.width/2;
    nscoord posAtEnd = posAtStart + traversableDistance;
    nscoord posOfPoint = mozilla::clamped(point.x, posAtStart, posAtEnd);
    fraction = Decimal(posOfPoint - posAtStart) / traversableDistance;
    if (StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
      fraction = Decimal(1) - fraction;
    }
  } else {
    nscoord traversableDistance = rangeContentRect.height - thumbSize.height;
    if (traversableDistance <= 0) {
      return minimum;
    }
    nscoord posAtStart = rangeContentRect.y + thumbSize.height/2;
    nscoord posAtEnd = posAtStart + traversableDistance;
    nscoord posOfPoint = mozilla::clamped(point.y, posAtStart, posAtEnd);
    // For a vertical range, the top (posAtStart) is the highest value, so we
    // subtract the fraction from 1.0 to get that polarity correct.
    fraction = Decimal(1) - Decimal(posOfPoint - posAtStart) / traversableDistance;
  }

  MOZ_ASSERT(fraction >= 0 && fraction <= 1);
  return minimum + fraction * range;
}

void
nsRangeFrame::UpdateForValueChange()
{
  if (NS_SUBTREE_DIRTY(this)) {
    return; // we're going to be updated when we reflow
  }
  nsIFrame* rangeProgressFrame = mProgressDiv->GetPrimaryFrame();
  nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();
  if (!rangeProgressFrame && !thumbFrame) {
    return; // diplay:none?
  }
  if (rangeProgressFrame) {
    DoUpdateRangeProgressFrame(rangeProgressFrame, GetSize());
  }
  if (thumbFrame) {
    DoUpdateThumbPosition(thumbFrame, GetSize());
  }
  if (IsThemed()) {
    // We don't know the exact dimensions or location of the thumb when native
    // theming is applied, so we just repaint the entire range.
    InvalidateFrame();
  }

#ifdef ACCESSIBILITY
  nsAccessibilityService* accService = nsIPresShell::AccService();
  if (accService) {
    accService->RangeValueChanged(PresContext()->PresShell(), mContent);
  }
#endif

  SchedulePaint();
}

void
nsRangeFrame::DoUpdateThumbPosition(nsIFrame* aThumbFrame,
                                    const nsSize& aRangeSize)
{
  MOZ_ASSERT(aThumbFrame);

  // The idea here is that we want to position the thumb so that the center
  // of the thumb is on an imaginary line drawn from the middle of one edge
  // of the range frame's content box to the middle of the opposite edge of
  // its content box (the opposite edges being the left/right edge if the
  // range is horizontal, or else the top/bottom edges if the range is
  // vertical). How far along this line the center of the thumb is placed
  // depends on the value of the range.

  nsMargin borderAndPadding = GetUsedBorderAndPadding();
  nsPoint newPosition(borderAndPadding.left, borderAndPadding.top);

  nsSize rangeContentBoxSize(aRangeSize);
  rangeContentBoxSize.width -= borderAndPadding.LeftRight();
  rangeContentBoxSize.height -= borderAndPadding.TopBottom();

  nsSize thumbSize = aThumbFrame->GetSize();
  double fraction = GetValueAsFractionOfRange();
  MOZ_ASSERT(fraction >= 0.0 && fraction <= 1.0);

  // We are called under Reflow, so we need to pass IsHorizontal a valid rect.
  nsSize frameSizeOverride(aRangeSize.width, aRangeSize.height);
  if (IsHorizontal(&frameSizeOverride)) {
    if (thumbSize.width < rangeContentBoxSize.width) {
      nscoord traversableDistance =
        rangeContentBoxSize.width - thumbSize.width;
      if (StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
        newPosition.x += NSToCoordRound((1.0 - fraction) * traversableDistance);
      } else {
        newPosition.x += NSToCoordRound(fraction * traversableDistance);
      }
      newPosition.y += (rangeContentBoxSize.height - thumbSize.height)/2;
    }
  } else {
    if (thumbSize.height < rangeContentBoxSize.height) {
      nscoord traversableDistance =
        rangeContentBoxSize.height - thumbSize.height;
      newPosition.x += (rangeContentBoxSize.width - thumbSize.width)/2;
      newPosition.y += NSToCoordRound((1.0 - fraction) * traversableDistance);
    }
  }
  aThumbFrame->SetPosition(newPosition);
}

void
nsRangeFrame::DoUpdateRangeProgressFrame(nsIFrame* aRangeProgressFrame,
                                         const nsSize& aRangeSize)
{
  MOZ_ASSERT(aRangeProgressFrame);

  // The idea here is that we want to position the ::-moz-range-progress
  // pseudo-element so that the center line running along its length is on the
  // corresponding center line of the nsRangeFrame's content box. In the other
  // dimension, we align the "start" edge of the ::-moz-range-progress
  // pseudo-element's border-box with the corresponding edge of the
  // nsRangeFrame's content box, and we size the progress element's border-box
  // to have a length of GetValueAsFractionOfRange() times the nsRangeFrame's
  // content-box size.

  nsMargin borderAndPadding = GetUsedBorderAndPadding();
  nsSize progSize = aRangeProgressFrame->GetSize();
  nsRect progRect(borderAndPadding.left, borderAndPadding.top,
                  progSize.width, progSize.height);

  nsSize rangeContentBoxSize(aRangeSize);
  rangeContentBoxSize.width -= borderAndPadding.LeftRight();
  rangeContentBoxSize.height -= borderAndPadding.TopBottom();

  double fraction = GetValueAsFractionOfRange();
  MOZ_ASSERT(fraction >= 0.0 && fraction <= 1.0);

  // We are called under Reflow, so we need to pass IsHorizontal a valid rect.
  nsSize frameSizeOverride(aRangeSize.width, aRangeSize.height);
  if (IsHorizontal(&frameSizeOverride)) {
    nscoord progLength = NSToCoordRound(fraction * rangeContentBoxSize.width);
    if (StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
      progRect.x += rangeContentBoxSize.width - progLength;
    }
    progRect.y += (rangeContentBoxSize.height - progSize.height)/2;
    progRect.width = progLength;
  } else {
    nscoord progLength = NSToCoordRound(fraction * rangeContentBoxSize.height);
    progRect.x += (rangeContentBoxSize.width - progSize.width)/2;
    progRect.y += rangeContentBoxSize.height - progLength;
    progRect.height = progLength;
  }
  aRangeProgressFrame->SetRect(progRect);
}

NS_IMETHODIMP
nsRangeFrame::AttributeChanged(int32_t  aNameSpaceID,
                               nsIAtom* aAttribute,
                               int32_t  aModType)
{
  NS_ASSERTION(mTrackDiv, "The track div must exist!");
  NS_ASSERTION(mThumbDiv, "The thumb div must exist!");

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::value ||
        aAttribute == nsGkAtoms::min ||
        aAttribute == nsGkAtoms::max ||
        aAttribute == nsGkAtoms::step) {
      // We want to update the position of the thumb, except in one special
      // case: If the value attribute is being set, it is possible that we are
      // in the middle of a type change away from type=range, under the
      // SetAttr(..., nsGkAtoms::value, ...) call in HTMLInputElement::
      // HandleTypeChange. In that case the HTMLInputElement's type will
      // already have changed, and if we call UpdateForValueChange()
      // we'll fail the asserts under that call that check the type of our
      // HTMLInputElement. Given that we're changing away from being a range
      // and this frame will shortly be destroyed, there's no point in calling
      // UpdateForValueChange() anyway.
      MOZ_ASSERT(mContent->IsHTML(nsGkAtoms::input), "bad cast");
      bool typeIsRange = static_cast<dom::HTMLInputElement*>(mContent)->GetType() ==
                           NS_FORM_INPUT_RANGE;
      MOZ_ASSERT(typeIsRange || aAttribute == nsGkAtoms::value, "why?");
      if (typeIsRange) {
        UpdateForValueChange();
      }
    } else if (aAttribute == nsGkAtoms::orient) {
      PresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eResize,
                                                   NS_FRAME_IS_DIRTY);
    }
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

nsSize
nsRangeFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                              nsSize aCBSize, nscoord aAvailableWidth,
                              nsSize aMargin, nsSize aBorder,
                              nsSize aPadding, bool aShrinkWrap)
{
  nscoord oneEm = NSToCoordRound(StyleFont()->mFont.size *
                                 nsLayoutUtils::FontSizeInflationFor(this)); // 1em

  // frameSizeOverride values just gets us to fall back to being horizontal
  // (the actual values are irrelevant, as long as width > height):
  nsSize frameSizeOverride(10,1);
  bool isHorizontal = IsHorizontal(&frameSizeOverride);

  nsSize autoSize;

  // nsFrame::ComputeSize calls GetMinimumWidgetSize to prevent us from being
  // given too small a size when we're natively themed. If we're themed, we set
  // our "thickness" dimension to zero below and rely on that
  // GetMinimumWidgetSize check to correct that dimension to the natural
  // thickness of a slider in the current theme.

  if (isHorizontal) {
    autoSize.width = LONG_SIDE_TO_SHORT_SIDE_RATIO * oneEm;
    autoSize.height = IsThemed() ? 0 : oneEm;
  } else {
    autoSize.width = IsThemed() ? 0 : oneEm;
    autoSize.height = LONG_SIDE_TO_SHORT_SIDE_RATIO * oneEm;
  }

  return autoSize;
}

nscoord
nsRangeFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  // nsFrame::ComputeSize calls GetMinimumWidgetSize to prevent us from being
  // given too small a size when we're natively themed. If we aren't native
  // themed, we don't mind how small we're sized.
  return nscoord(0);
}

nscoord
nsRangeFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  // frameSizeOverride values just gets us to fall back to being horizontal:
  nsSize frameSizeOverride(10,1);
  bool isHorizontal = IsHorizontal(&frameSizeOverride);

  if (!isHorizontal && IsThemed()) {
    // nsFrame::ComputeSize calls GetMinimumWidgetSize to prevent us from being
    // given too small a size when we're natively themed. We return zero and
    // depend on that correction to get our "natuaral" width when we're a
    // vertical slider.
    return 0;
  }

  nscoord prefWidth = NSToCoordRound(StyleFont()->mFont.size *
                                     nsLayoutUtils::FontSizeInflationFor(this)); // 1em

  if (isHorizontal) {
    prefWidth *= LONG_SIDE_TO_SHORT_SIDE_RATIO;
  }

  return prefWidth;
}

bool
nsRangeFrame::IsHorizontal(const nsSize *aFrameSizeOverride) const
{
  dom::HTMLInputElement* element = static_cast<dom::HTMLInputElement*>(mContent);
  return !element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::orient,
                               nsGkAtoms::vertical, eCaseMatters);
}

double
nsRangeFrame::GetMin() const
{
  return static_cast<dom::HTMLInputElement*>(mContent)->GetMinimum().toDouble();
}

double
nsRangeFrame::GetMax() const
{
  return static_cast<dom::HTMLInputElement*>(mContent)->GetMaximum().toDouble();
}

double
nsRangeFrame::GetValue() const
{
  return static_cast<dom::HTMLInputElement*>(mContent)->GetValueAsDecimal().toDouble();
}

nsIAtom*
nsRangeFrame::GetType() const
{
  return nsGkAtoms::rangeFrame;
}

#define STYLES_DISABLING_NATIVE_THEMING \
  NS_AUTHOR_SPECIFIED_BACKGROUND | \
  NS_AUTHOR_SPECIFIED_PADDING | \
  NS_AUTHOR_SPECIFIED_BORDER

bool
nsRangeFrame::ShouldUseNativeStyle() const
{
  return (StyleDisplay()->mAppearance == NS_THEME_RANGE) &&
         !PresContext()->HasAuthorSpecifiedRules(const_cast<nsRangeFrame*>(this),
                                                 (NS_AUTHOR_SPECIFIED_BORDER |
                                                  NS_AUTHOR_SPECIFIED_BACKGROUND)) &&
         !PresContext()->HasAuthorSpecifiedRules(mTrackDiv->GetPrimaryFrame(),
                                                 STYLES_DISABLING_NATIVE_THEMING) &&
         !PresContext()->HasAuthorSpecifiedRules(mProgressDiv->GetPrimaryFrame(),
                                                 STYLES_DISABLING_NATIVE_THEMING) &&
         !PresContext()->HasAuthorSpecifiedRules(mThumbDiv->GetPrimaryFrame(),
                                                 STYLES_DISABLING_NATIVE_THEMING);
}
