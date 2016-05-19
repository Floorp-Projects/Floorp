/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProgressFrame.h"

#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsFormControlFrame.h"
#include "nsFontMetrics.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLProgressElement.h"
#include "nsContentList.h"
#include "nsCSSPseudoElements.h"
#include "nsStyleSet.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "nsThemeConstants.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame*
NS_NewProgressFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsProgressFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsProgressFrame)

nsProgressFrame::nsProgressFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
  , mBarDiv(nullptr)
{
}

nsProgressFrame::~nsProgressFrame()
{
}

void
nsProgressFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_ASSERTION(!GetPrevContinuation(),
               "nsProgressFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsContentUtils::DestroyAnonymousContent(&mBarDiv);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

nsIAtom*
nsProgressFrame::GetType() const
{
  return nsGkAtoms::progressFrame;
}

nsresult
nsProgressFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  // Create the progress bar div.
  nsCOMPtr<nsIDocument> doc = mContent->GetComposedDoc();
  mBarDiv = doc->CreateHTMLElement(nsGkAtoms::div);

  // Associate ::-moz-progress-bar pseudo-element to the anonymous child.
  CSSPseudoElementType pseudoType = CSSPseudoElementType::mozProgressBar;
  RefPtr<nsStyleContext> newStyleContext = PresContext()->StyleSet()->
    ResolvePseudoElementStyle(mContent->AsElement(), pseudoType,
                              StyleContext(), mBarDiv->AsElement());

  if (!aElements.AppendElement(ContentInfo(mBarDiv, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
nsProgressFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                          uint32_t aFilter)
{
  if (mBarDiv) {
    aElements.AppendElement(mBarDiv);
  }
}

NS_QUERYFRAME_HEAD(nsProgressFrame)
  NS_QUERYFRAME_ENTRY(nsProgressFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)


void
nsProgressFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  BuildDisplayListForInline(aBuilder, aDirtyRect, aLists);
}

void
nsProgressFrame::Reflow(nsPresContext*           aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsProgressFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_ASSERTION(mBarDiv, "Progress bar div must exist!");
  NS_ASSERTION(!GetPrevContinuation(),
               "nsProgressFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }

  nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();
  NS_ASSERTION(barFrame, "The progress frame should have a child with a frame!");

  ReflowBarFrame(barFrame, aPresContext, aReflowState, aStatus);

  aDesiredSize.SetSize(aReflowState.GetWritingMode(),
                       aReflowState.ComputedSizeWithBorderPadding());
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  ConsiderChildOverflow(aDesiredSize.mOverflowAreas, barFrame);
  FinishAndStoreOverflow(&aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

void
nsProgressFrame::ReflowBarFrame(nsIFrame*                aBarFrame,
                                nsPresContext*           aPresContext,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
  bool vertical = ResolvedOrientationIsVertical();
  WritingMode wm = aBarFrame->GetWritingMode();
  LogicalSize availSize = aReflowState.ComputedSize(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  nsHTMLReflowState reflowState(aPresContext, aReflowState,
                                aBarFrame, availSize);
  nscoord size = vertical ? aReflowState.ComputedHeight()
                          : aReflowState.ComputedWidth();
  nscoord xoffset = aReflowState.ComputedPhysicalBorderPadding().left;
  nscoord yoffset = aReflowState.ComputedPhysicalBorderPadding().top;

  double position = static_cast<HTMLProgressElement*>(mContent)->Position();

  // Force the bar's size to match the current progress.
  // When indeterminate, the progress' size will be 100%.
  if (position >= 0.0) {
    size *= position;
  }

  if (!vertical && (wm.IsVertical() ? wm.IsVerticalRL() : !wm.IsBidiLTR())) {
    xoffset += aReflowState.ComputedWidth() - size;
  }

  // The bar size is fixed in these cases:
  // - the progress position is determined: the bar size is fixed according
  //   to it's value.
  // - the progress position is indeterminate and the bar appearance should be
  //   shown as native: the bar size is forced to 100%.
  // Otherwise (when the progress is indeterminate and the bar appearance isn't
  // native), the bar size isn't fixed and can be set by the author.
  if (position != -1 || ShouldUseNativeStyle()) {
    if (vertical) {
      // We want the bar to begin at the bottom.
      yoffset += aReflowState.ComputedHeight() - size;

      size -= reflowState.ComputedPhysicalMargin().TopBottom() +
              reflowState.ComputedPhysicalBorderPadding().TopBottom();
      size = std::max(size, 0);
      reflowState.SetComputedHeight(size);
    } else {
      size -= reflowState.ComputedPhysicalMargin().LeftRight() +
              reflowState.ComputedPhysicalBorderPadding().LeftRight();
      size = std::max(size, 0);
      reflowState.SetComputedWidth(size);
    }
  } else if (vertical) {
    // For vertical progress bars, we need to position the bar specificly when
    // the width isn't constrained (position == -1 and !ShouldUseNativeStyle())
    // because aReflowState.ComputedHeight() - size == 0.
    yoffset += aReflowState.ComputedHeight() - reflowState.ComputedHeight();
  }

  xoffset += reflowState.ComputedPhysicalMargin().left;
  yoffset += reflowState.ComputedPhysicalMargin().top;

  nsHTMLReflowMetrics barDesiredSize(aReflowState);
  ReflowChild(aBarFrame, aPresContext, barDesiredSize, reflowState, xoffset,
              yoffset, 0, aStatus);
  FinishReflowChild(aBarFrame, aPresContext, barDesiredSize, &reflowState,
                    xoffset, yoffset, 0);
}

nsresult
nsProgressFrame::AttributeChanged(int32_t  aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  int32_t  aModType)
{
  NS_ASSERTION(mBarDiv, "Progress bar div must exist!");

  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max)) {
    nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();
    NS_ASSERTION(barFrame, "The progress frame should have a child with a frame!");
    PresContext()->PresShell()->FrameNeedsReflow(barFrame, nsIPresShell::eResize,
                                                 NS_FRAME_IS_DIRTY);
    InvalidateFrame();
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

LogicalSize
nsProgressFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 WritingMode aWM,
                                 const LogicalSize& aCBSize,
                                 nscoord aAvailableISize,
                                 const LogicalSize& aMargin,
                                 const LogicalSize& aBorder,
                                 const LogicalSize& aPadding,
                                 bool aShrinkWrap)
{
  const WritingMode wm = GetWritingMode();
  LogicalSize autoSize(wm);
  autoSize.BSize(wm) = autoSize.ISize(wm) =
    NSToCoordRound(StyleFont()->mFont.size *
                   nsLayoutUtils::FontSizeInflationFor(this)); // 1em

  if (ResolvedOrientationIsVertical() == wm.IsVertical()) {
    autoSize.ISize(wm) *= 10; // 10em
  } else {
    autoSize.BSize(wm) *= 10; // 10em
  }

  return autoSize.ConvertTo(aWM, wm);
}

nscoord
nsProgressFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  RefPtr<nsFontMetrics> fontMet =
    nsLayoutUtils::GetFontMetricsForFrame(this, 1.0f);

  nscoord minISize = fontMet->Font().size; // 1em

  if (ResolvedOrientationIsVertical() == GetWritingMode().IsVertical()) {
    // The orientation is inline
    minISize *= 10; // 10em
  }

  return minISize;
}

nscoord
nsProgressFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  return GetMinISize(aRenderingContext);
}

bool
nsProgressFrame::ShouldUseNativeStyle() const
{
  nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();

  // Use the native style if these conditions are satisfied:
  // - both frames use the native appearance;
  // - neither frame has author specified rules setting the border or the
  //   background.
  return StyleDisplay()->mAppearance == NS_THEME_PROGRESSBAR &&
         !PresContext()->HasAuthorSpecifiedRules(this,
                                                 NS_AUTHOR_SPECIFIED_BORDER | NS_AUTHOR_SPECIFIED_BACKGROUND) &&
         barFrame &&
         barFrame->StyleDisplay()->mAppearance == NS_THEME_PROGRESSCHUNK &&
         !PresContext()->HasAuthorSpecifiedRules(barFrame,
                                                 NS_AUTHOR_SPECIFIED_BORDER | NS_AUTHOR_SPECIFIED_BACKGROUND);
}

Element*
nsProgressFrame::GetPseudoElement(CSSPseudoElementType aType)
{
  if (aType == CSSPseudoElementType::mozProgressBar) {
    return mBarDiv;
  }

  return nsContainerFrame::GetPseudoElement(aType);
}
