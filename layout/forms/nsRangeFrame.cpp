/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRangeFrame.h"

#include "nsContentCreatorFunctions.h"
#include "nsContentList.h"
#include "nsContentUtils.h"
#include "nsFontMetrics.h"
#include "nsFormControlFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "nsHTMLInputElement.h"
#include "nsPresContext.h"
#include "nsNodeInfoManager.h"
#include "nsRenderingContext.h"
#include "mozilla/dom/Element.h"
#include "prtypes.h"

#include <algorithm>

#define LONG_SIDE_TO_SHORT_SIDE_RATIO 10

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
nsRangeFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_ASSERTION(!GetPrevContinuation() && !GetNextContinuation(),
               "nsRangeFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsContentUtils::DestroyAnonymousContent(&mTrackDiv);
  nsContentUtils::DestroyAnonymousContent(&mThumbDiv);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

nsresult
nsRangeFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  // Get the NodeInfoManager and tag necessary to create the anonymous divs.
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();

  // Create the track div:
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::div, nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = NS_NewHTMLElement(getter_AddRefs(mTrackDiv), nodeInfo.forget(),
                                  mozilla::dom::NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);
  // Associate ::-moz-range-track pseudo-element to the anonymous child.
  nsCSSPseudoElements::Type pseudoType =
    nsCSSPseudoElements::ePseudo_mozRangeTrack;
  nsRefPtr<nsStyleContext> newStyleContext =
    PresContext()->StyleSet()->ResolvePseudoElementStyle(mContent->AsElement(),
                                                         pseudoType,
                                                         StyleContext());

  if (!aElements.AppendElement(ContentInfo(mTrackDiv, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Create the thumb div:
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::div, nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);
  rv = NS_NewHTMLElement(getter_AddRefs(mThumbDiv), nodeInfo.forget(),
                         mozilla::dom::NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);
  // Associate ::-moz-range-thumb pseudo-element to the anonymous child.
  pseudoType = nsCSSPseudoElements::ePseudo_mozRangeThumb;
  newStyleContext =
    PresContext()->StyleSet()->ResolvePseudoElementStyle(mContent->AsElement(),
                                                         pseudoType,
                                                         StyleContext());

  if (!aElements.AppendElement(ContentInfo(mThumbDiv, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
nsRangeFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                       uint32_t aFilter)
{
  aElements.MaybeAppendElement(mTrackDiv);
  aElements.MaybeAppendElement(mThumbDiv);
}

void
nsRangeFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  BuildDisplayListForInline(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP
nsRangeFrame::Reflow(nsPresContext*           aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsRangeFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_ASSERTION(mTrackDiv, "Track div must exist!");
  NS_ASSERTION(mThumbDiv, "Thumb div must exist!");
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
  if (ShouldUseNativeStyle()) {
    return NS_OK; // No need to reflow since we're not using these frames
  }

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

    nsReflowStatus frameStatus = NS_FRAME_COMPLETE;
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

    nsReflowStatus frameStatus = NS_FRAME_COMPLETE;
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

  return NS_OK;
}

double
nsRangeFrame::GetValueAsFractionOfRange()
{
  MOZ_ASSERT(mContent->IsHTML(nsGkAtoms::input), "bad cast");
  nsHTMLInputElement* input = static_cast<nsHTMLInputElement*>(mContent);

  MOZ_ASSERT(input->GetType() == NS_FORM_INPUT_RANGE);

  double value = input->GetValueAsDouble();
  double minimum = input->GetMinimum();
  double maximum = input->GetMaximum();

  MOZ_ASSERT(MOZ_DOUBLE_IS_FINITE(value) &&
             MOZ_DOUBLE_IS_FINITE(minimum) &&
             MOZ_DOUBLE_IS_FINITE(maximum),
             "type=range should have a default maximum/minimum");
  
  if (maximum <= minimum) {
    MOZ_ASSERT(value == minimum, "Unsanitized value");
    return 0.0;
  }
  
  MOZ_ASSERT(value >= minimum && value <= maximum, "Unsanitized value");
  
  return (value - minimum) / (maximum - minimum);
}

double
nsRangeFrame::GetValueAtEventPoint(nsGUIEvent* aEvent)
{
  MOZ_ASSERT(aEvent->eventStructType == NS_MOUSE_EVENT ||
             aEvent->eventStructType == NS_TOUCH_EVENT,
             "Unexpected event type - aEvent->refPoint may be meaningless");

  MOZ_ASSERT(mContent->IsHTML(nsGkAtoms::input), "bad cast");
  nsHTMLInputElement* input = static_cast<nsHTMLInputElement*>(mContent);

  MOZ_ASSERT(input->GetType() == NS_FORM_INPUT_RANGE);

  double minimum = input->GetMinimum();
  double maximum = input->GetMaximum();
  MOZ_ASSERT(MOZ_DOUBLE_IS_FINITE(minimum) &&
             MOZ_DOUBLE_IS_FINITE(maximum),
             "type=range should have a default maximum/minimum");
  if (maximum <= minimum) {
    return minimum;
  }
  double range = maximum - minimum;

  nsIntPoint absPoint;
  if (aEvent->eventStructType == NS_TOUCH_EVENT) {
    MOZ_ASSERT(static_cast<nsTouchEvent*>(aEvent)->touches.Length() == 1,
               "Unexpected number of touches");
    absPoint = static_cast<nsTouchEvent*>(aEvent)->touches[0]->mRefPoint;
  } else {
    absPoint = aEvent->refPoint;
  }
  nsPoint point =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, absPoint, this);

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

  double fraction;
  if (IsHorizontal()) {
    nscoord traversableDistance = rangeContentRect.width - thumbSize.width;
    if (traversableDistance <= 0) {
      return minimum;
    }
    nscoord posAtStart = rangeContentRect.x + thumbSize.width/2;
    nscoord posAtEnd = posAtStart + traversableDistance;
    nscoord posOfPoint = mozilla::clamped(point.x, posAtStart, posAtEnd);
    fraction = (posOfPoint - posAtStart) / double(traversableDistance);
    if (StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
      fraction = 1.0 - fraction;
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
    fraction = 1.0 - (posOfPoint - posAtStart) / double(traversableDistance);
  }

  MOZ_ASSERT(fraction >= 0.0 && fraction <= 1.0);
  return minimum + fraction * range;
}

void
nsRangeFrame::UpdateThumbPositionForValueChange()
{
  if (NS_SUBTREE_DIRTY(this)) {
    return; // we're going to be updated when we reflow
  }
  nsIFrame* thumbFrame = mThumbDiv->GetPrimaryFrame();
  if (!thumbFrame) {
    return; // diplay:none?
  }
  DoUpdateThumbPosition(thumbFrame, GetSize());
  if (IsThemed()) {
    // We don't know the exact dimensions or location of the thumb when native
    // theming is applied, so we just repaint the entire range.
    InvalidateFrame();
  }
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

NS_IMETHODIMP
nsRangeFrame::AttributeChanged(int32_t  aNameSpaceID,
                               nsIAtom* aAttribute,
                               int32_t  aModType)
{
  NS_ASSERTION(mTrackDiv, "The track div must exist!");
  NS_ASSERTION(mThumbDiv, "The thumb div must exist!");

  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::value ||
       aAttribute == nsGkAtoms::min ||
       aAttribute == nsGkAtoms::max ||
       aAttribute == nsGkAtoms::step)) {
    // We want to update the position of the thumb, except in one special case:
    // If the value attribute is being set, it is possible that we are in the
    // middle of a type change away from type=range, under the
    // SetAttr(..., nsGkAtoms::value, ...) call in nsHTMLInputElement::
    // HandleTypeChange. In that case the nsHTMLInputElement's type will
    // already have changed, and if we call UpdateThumbPositionForValueChange()
    // we'll fail the asserts under that call that check the type of our
    // nsHTMLInputElement. Given that we're changing away from being a range
    // and this frame will shortly be destroyed, there's no point in calling
    // UpdateThumbPositionForValueChange() anyway.
    MOZ_ASSERT(mContent->IsHTML(nsGkAtoms::input), "bad cast");
    bool typeIsRange = static_cast<nsHTMLInputElement*>(mContent)->GetType() ==
                         NS_FORM_INPUT_RANGE;
    MOZ_ASSERT(typeIsRange || aAttribute == nsGkAtoms::value, "why?");
    if (typeIsRange) {
      UpdateThumbPositionForValueChange();
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
  return true; // until we decide how to support vertical range (bug 840820)
}

double
nsRangeFrame::GetMin() const
{
  return static_cast<nsHTMLInputElement*>(mContent)->GetMinimum();
}

double
nsRangeFrame::GetMax() const
{
  return static_cast<nsHTMLInputElement*>(mContent)->GetMaximum();
}

double
nsRangeFrame::GetValue() const
{
  return static_cast<nsHTMLInputElement*>(mContent)->GetValueAsDouble();
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
                                                 STYLES_DISABLING_NATIVE_THEMING) &&
         !PresContext()->HasAuthorSpecifiedRules(mTrackDiv->GetPrimaryFrame(),
                                                 STYLES_DISABLING_NATIVE_THEMING) &&
         !PresContext()->HasAuthorSpecifiedRules(mThumbDiv->GetPrimaryFrame(),
                                                 STYLES_DISABLING_NATIVE_THEMING);
}
