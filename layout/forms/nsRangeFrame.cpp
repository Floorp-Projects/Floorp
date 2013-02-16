/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRangeFrame.h"

#include "nsHTMLInputElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsNodeInfoManager.h"
#include "nsINodeInfo.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsFormControlFrame.h"
#include "nsContentList.h"
#include "nsFontMetrics.h"
#include "mozilla/dom/Element.h"
#include "nsContentList.h"

#include <algorithm>

NS_IMETHODIMP
nsRangeFrame::SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList)
{
  nsresult rv = nsContainerFrame::SetInitialChildList(aListID, aChildList);
  return rv;
}

nsIFrame*
NS_NewRangeFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsRangeFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsRangeFrame)

nsRangeFrame::nsRangeFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
  , mThumbDiv(nullptr)
  , mProgressDiv(nullptr)
{
}

nsRangeFrame::~nsRangeFrame()
{
}

void
nsRangeFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_ASSERTION(!GetPrevContinuation(),
               "nsRangeFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsContentUtils::DestroyAnonymousContent(&mThumbDiv);
  nsContentUtils::DestroyAnonymousContent(&mProgressDiv);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

nsresult
nsRangeFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  // Get the NodeInfoManager and tag necessary to create the progress bar div.
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::div, nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  // Create the div.
  nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::div, nullptr,
                                                 kNameSpaceID_XHTML,
                                                 nsIDOMNode::ELEMENT_NODE);
  nsresult rv = NS_NewHTMLElement(getter_AddRefs(mThumbDiv), nodeInfo.forget(),
                         mozilla::dom::NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCSSPseudoElements::Type pseudoType = nsCSSPseudoElements::ePseudo_mozRangeThumb;
  nsRefPtr<nsStyleContext> newStyleContext = PresContext()->StyleSet()->
    ResolvePseudoElementStyle(mContent->AsElement(), pseudoType, GetStyleContext());

  if (!aElements.AppendElement(ContentInfo(mThumbDiv, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Progress div
  rv = NS_NewHTMLElement(getter_AddRefs(mProgressDiv), nodeInfo.forget(),
                         mozilla::dom::NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);
  pseudoType = nsCSSPseudoElements::ePseudo_mozRangeActive;
  newStyleContext = PresContext()->StyleSet()->
    ResolvePseudoElementStyle(mContent->AsElement(), pseudoType, GetStyleContext());

  if (!aElements.AppendElement(ContentInfo(mProgressDiv, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
nsRangeFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                       uint32_t aFilter)
{
  aElements.MaybeAppendElement(mThumbDiv);
  aElements.MaybeAppendElement(mProgressDiv);
}

NS_QUERYFRAME_HEAD(nsRangeFrame)
  NS_QUERYFRAME_ENTRY(nsRangeFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)


NS_IMETHODIMP
nsRangeFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  return BuildDisplayListForInline(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP nsRangeFrame::Reflow(nsPresContext*           aPresContext,
                                      nsHTMLReflowMetrics&     aDesiredSize,
                                      const nsHTMLReflowState& aReflowState,
                                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsRangeFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_ASSERTION(mThumbDiv, "Thumb div must exist!");
  NS_ASSERTION(!GetPrevContinuation(),
               "nsRangeFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }

  aDesiredSize.width = aReflowState.ComputedWidth() +
                       aReflowState.mComputedBorderPadding.LeftRight();
  aDesiredSize.height = aReflowState.ComputedHeight() +
                        aReflowState.mComputedBorderPadding.TopBottom();

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  nsIFrame* barFrame = mThumbDiv->GetPrimaryFrame();
  ConsiderChildOverflow(aDesiredSize.mOverflowAreas, barFrame);

  nsIFrame* progressFrame = mProgressDiv->GetPrimaryFrame();
  ConsiderChildOverflow(aDesiredSize.mOverflowAreas, progressFrame);

  FinishAndStoreOverflow(&aDesiredSize);

  ReflowBarFrame(aPresContext, aReflowState, aStatus);

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  return NS_OK;
}

void
nsRangeFrame::ReflowBarFrame(nsPresContext*           aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  nsIFrame* barFrame = mThumbDiv->GetPrimaryFrame();
  NS_ASSERTION(barFrame, "The range frame should have a child with a frame!");

  nsIFrame* progressFrame = mProgressDiv->GetPrimaryFrame();
  NS_ASSERTION(progressFrame, "The range frame should have a child with a frame!");

  bool vertical = !IsHorizontal(aReflowState.ComputedWidth(), aReflowState.ComputedHeight());
  nsHTMLReflowState barReflowState(aPresContext, aReflowState, barFrame,
                                nsSize(aReflowState.ComputedWidth(),
                                       NS_UNCONSTRAINEDSIZE));
  nsHTMLReflowState progressReflowState(aPresContext, aReflowState, progressFrame,
                                        nsSize(aReflowState.ComputedWidth(),
                                               NS_UNCONSTRAINEDSIZE));

  nscoord parentHeight = aReflowState.ComputedHeight() + aReflowState.mComputedBorderPadding.TopBottom();
  nscoord parentWidth = aReflowState.ComputedWidth() + aReflowState.mComputedBorderPadding.LeftRight();

  nscoord size = vertical ? aReflowState.ComputedHeight() : aReflowState.ComputedWidth();
  nscoord progressSize = size;

  nsSize thumbSize = barFrame->GetSize();
  size -= vertical ? thumbSize.height : thumbSize.width;

  nscoord xoffset = vertical ? 0 : aReflowState.mComputedBorderPadding.left;
  nscoord yoffset = vertical ? aReflowState.mComputedBorderPadding.top : 0;
  nscoord xProgressOffset = 0;
  nscoord yProgressOffset = 0;

  // center the thumb in the box
  xoffset += vertical ? (parentWidth - barReflowState.ComputedWidth()) / 2   : 0;
  yoffset += vertical ? 0 : (parentHeight - barReflowState.ComputedHeight()) / 2;

  nsHTMLInputElement* element = static_cast<nsHTMLInputElement*>(mContent);
  double position = element->GetPositionAsPercent();

  if (position >= 0.0) {
    size *= vertical ? (1 - position) : position;
    progressSize *= vertical ? (1 - position) : position;
  }

  if (!vertical && GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
    xoffset += parentWidth - size;
    xProgressOffset += parentWidth - progressSize;
  }

  if (position != -1 || ShouldUseNativeStyle()) {
    if (vertical) {
      size -= barReflowState.mComputedMargin.TopBottom() +
              barReflowState.mComputedBorderPadding.TopBottom();
      size = std::max(size, 0);
      yoffset += size;
      yProgressOffset = yoffset;

      progressSize = parentHeight - yoffset;
      progressSize = std::max(progressSize, 0);
      progressReflowState.SetComputedHeight(progressSize);
      progressReflowState.SetComputedWidth(parentWidth);
    } else {
      size -= barReflowState.mComputedMargin.LeftRight() +
              barReflowState.mComputedBorderPadding.LeftRight();
      size = std::max(size, 0);
      xoffset += size;

      progressReflowState.SetComputedWidth(xoffset + thumbSize.width);
      progressReflowState.SetComputedHeight(parentHeight);
    }
  } else if (vertical) {
    yoffset += parentHeight - barReflowState.ComputedHeight();
    yProgressOffset += parentHeight - progressReflowState.ComputedHeight();
  }

  nsHTMLReflowMetrics barDesiredSize;
  ReflowChild(barFrame, aPresContext, barDesiredSize, barReflowState, xoffset, yoffset, 0, aStatus);
  FinishReflowChild(barFrame, aPresContext, &barReflowState, barDesiredSize, xoffset, yoffset, 0);

  nsHTMLReflowMetrics progressDesiredSize;
  ReflowChild(progressFrame, aPresContext, progressDesiredSize, progressReflowState, 0,
              xProgressOffset, yProgressOffset, aStatus);
  FinishReflowChild(progressFrame, aPresContext, &progressReflowState, progressDesiredSize,
                    xProgressOffset, yProgressOffset, 0);}

NS_IMETHODIMP
nsRangeFrame::AttributeChanged(int32_t  aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  int32_t  aModType)
{
  NS_ASSERTION(mThumbDiv, "Thumb div must exist!");
  NS_ASSERTION(mProgressDiv, "Progress div must exist!");

  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max)) {
    nsIFrame* barFrame = mThumbDiv->GetPrimaryFrame();
    NS_ASSERTION(barFrame, "The range frame should have a child with a frame!");
    PresContext()->PresShell()->FrameNeedsReflow(barFrame, nsIPresShell::eResize,
                                                 NS_FRAME_IS_DIRTY);

    nsIFrame* progressFrame = mProgressDiv->GetPrimaryFrame();
    NS_ASSERTION(progressFrame, "The range frame should have a child with a frame!");
    PresContext()->PresShell()->FrameNeedsReflow(progressFrame, nsIPresShell::eResize,
                                                 NS_FRAME_IS_DIRTY);

    InvalidateFrame();
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

nsSize
nsRangeFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap)
{
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  nsRefPtr<nsFontMetrics> fontMet;
  NS_ENSURE_SUCCESS(nsLayoutUtils::GetFontMetricsForFrame(this,
                                                          getter_AddRefs(fontMet),
                                                          inflation),
                    nsSize(0, 0));

  nsSize autoSize;
  autoSize.height = autoSize.width = fontMet->Font().size; // 1em

  if (IsHorizontal(autoSize.width, autoSize.height)) {
    autoSize.width *= 10; // 10em
  } else {
    autoSize.height *= 10; // 10em
  }

  return autoSize;
}

nscoord
nsRangeFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nsRefPtr<nsFontMetrics> fontMet;
  NS_ENSURE_SUCCESS(
      nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet)), 0);

  nscoord minWidth = fontMet->Font().size; // 1em

  nsSize size = GetSize();
  if (IsHorizontal(size.width, size.height)) {
    minWidth *= 10; // 10em
  }

  return minWidth;
}

nscoord
nsRangeFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  return GetMinWidth(aRenderingContext);
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

bool
nsRangeFrame::ShouldUseNativeStyle() const
{
  // Use the native style if these conditions are satisfied:
  // - both frames use the native appearance;
  // - neither frame has author specified rules setting the border or the
  //   background.
  return (GetStyleDisplay()->mAppearance == NS_THEME_SCALE_HORIZONTAL) &&
         (mThumbDiv->GetPrimaryFrame()->GetStyleDisplay()->mAppearance == NS_THEME_SCALE_THUMB_HORIZONTAL) &&
         !PresContext()->HasAuthorSpecifiedRules(const_cast<nsRangeFrame*>(this),
                                                 NS_AUTHOR_SPECIFIED_BORDER | NS_AUTHOR_SPECIFIED_BACKGROUND) &&
         !PresContext()->HasAuthorSpecifiedRules(mThumbDiv->GetPrimaryFrame(),
                                                 NS_AUTHOR_SPECIFIED_BORDER | NS_AUTHOR_SPECIFIED_BACKGROUND);
}

bool nsRangeFrame::IsHorizontal(nscoord width, nscoord height) {
  if (GetStyleDisplay()->mOrient == NS_STYLE_ORIENT_VERTICAL)
    return false;

  return width >= height;
}
