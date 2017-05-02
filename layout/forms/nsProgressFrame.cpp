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
  : nsContainerFrame(aContext, FrameType::Progress)
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

nsresult
nsProgressFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  // Create the progress bar div.
  nsCOMPtr<nsIDocument> doc = mContent->GetComposedDoc();
  mBarDiv = doc->CreateHTMLElement(nsGkAtoms::div);

  // Associate ::-moz-progress-bar pseudo-element to the anonymous child.
  mBarDiv->SetPseudoElementType(CSSPseudoElementType::mozProgressBar);

  if (!aElements.AppendElement(mBarDiv)) {
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
                        ReflowOutput&     aDesiredSize,
                        const ReflowInput& aReflowInput,
                        nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsProgressFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);

  NS_ASSERTION(mBarDiv, "Progress bar div must exist!");
  NS_ASSERTION(PrincipalChildList().GetLength() == 1 &&
               PrincipalChildList().FirstChild() == mBarDiv->GetPrimaryFrame(),
               "unexpected child frames");
  NS_ASSERTION(!GetPrevContinuation(),
               "nsProgressFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }

  aDesiredSize.SetSize(aReflowInput.GetWritingMode(),
                       aReflowInput.ComputedSizeWithBorderPadding());
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  for (auto childFrame : PrincipalChildList()) {
    ReflowChildFrame(childFrame, aPresContext, aReflowInput, aStatus);
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, childFrame);
  }

  FinishAndStoreOverflow(&aDesiredSize);

  aStatus.Reset();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

void
nsProgressFrame::ReflowChildFrame(nsIFrame*          aChild,
                                  nsPresContext*     aPresContext,
                                  const ReflowInput& aReflowInput,
                                  nsReflowStatus&    aStatus)
{
  bool vertical = ResolvedOrientationIsVertical();
  WritingMode wm = aChild->GetWritingMode();
  LogicalSize availSize = aReflowInput.ComputedSize(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  ReflowInput reflowInput(aPresContext, aReflowInput, aChild, availSize);
  nscoord size = vertical ? aReflowInput.ComputedHeight()
                          : aReflowInput.ComputedWidth();
  nscoord xoffset = aReflowInput.ComputedPhysicalBorderPadding().left;
  nscoord yoffset = aReflowInput.ComputedPhysicalBorderPadding().top;

  double position = static_cast<HTMLProgressElement*>(mContent)->Position();

  // Force the bar's size to match the current progress.
  // When indeterminate, the progress' size will be 100%.
  if (position >= 0.0) {
    size *= position;
  }

  if (!vertical && (wm.IsVertical() ? wm.IsVerticalRL() : !wm.IsBidiLTR())) {
    xoffset += aReflowInput.ComputedWidth() - size;
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
      yoffset += aReflowInput.ComputedHeight() - size;

      size -= reflowInput.ComputedPhysicalMargin().TopBottom() +
              reflowInput.ComputedPhysicalBorderPadding().TopBottom();
      size = std::max(size, 0);
      reflowInput.SetComputedHeight(size);
    } else {
      size -= reflowInput.ComputedPhysicalMargin().LeftRight() +
              reflowInput.ComputedPhysicalBorderPadding().LeftRight();
      size = std::max(size, 0);
      reflowInput.SetComputedWidth(size);
    }
  } else if (vertical) {
    // For vertical progress bars, we need to position the bar specificly when
    // the width isn't constrained (position == -1 and !ShouldUseNativeStyle())
    // because aReflowInput.ComputedHeight() - size == 0.
    yoffset += aReflowInput.ComputedHeight() - reflowInput.ComputedHeight();
  }

  xoffset += reflowInput.ComputedPhysicalMargin().left;
  yoffset += reflowInput.ComputedPhysicalMargin().top;

  ReflowOutput barDesiredSize(aReflowInput);
  ReflowChild(aChild, aPresContext, barDesiredSize, reflowInput, xoffset,
              yoffset, 0, aStatus);
  FinishReflowChild(aChild, aPresContext, barDesiredSize, &reflowInput,
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
    auto shell = PresContext()->PresShell();
    for (auto childFrame : PrincipalChildList()) {
      shell->FrameNeedsReflow(childFrame, nsIPresShell::eResize,
                              NS_FRAME_IS_DIRTY);
    }
    InvalidateFrame();
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

LogicalSize
nsProgressFrame::ComputeAutoSize(nsRenderingContext* aRenderingContext,
                                 WritingMode         aWM,
                                 const LogicalSize&  aCBSize,
                                 nscoord             aAvailableISize,
                                 const LogicalSize&  aMargin,
                                 const LogicalSize&  aBorder,
                                 const LogicalSize&  aPadding,
                                 ComputeSizeFlags    aFlags)
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
  nsIFrame* barFrame = PrincipalChildList().FirstChild();

  // Use the native style if these conditions are satisfied:
  // - both frames use the native appearance;
  // - neither frame has author specified rules setting the border or the
  //   background.
  return StyleDisplay()->UsedAppearance() == NS_THEME_PROGRESSBAR &&
         !PresContext()->HasAuthorSpecifiedRules(this,
                                                 NS_AUTHOR_SPECIFIED_BORDER | NS_AUTHOR_SPECIFIED_BACKGROUND) &&
         barFrame &&
         barFrame->StyleDisplay()->UsedAppearance() == NS_THEME_PROGRESSCHUNK &&
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
