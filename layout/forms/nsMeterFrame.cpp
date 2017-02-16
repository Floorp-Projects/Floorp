/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMeterFrame.h"

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
#include "mozilla/dom/HTMLMeterElement.h"
#include "nsContentList.h"
#include "nsCSSPseudoElements.h"
#include "nsStyleSet.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "nsThemeConstants.h"
#include <algorithm>

using namespace mozilla;
using mozilla::dom::Element;
using mozilla::dom::HTMLMeterElement;

nsIFrame*
NS_NewMeterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMeterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMeterFrame)

nsMeterFrame::nsMeterFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
  , mBarDiv(nullptr)
{
}

nsMeterFrame::~nsMeterFrame()
{
}

void
nsMeterFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_ASSERTION(!GetPrevContinuation(),
               "nsMeterFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsContentUtils::DestroyAnonymousContent(&mBarDiv);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

nsIAtom*
nsMeterFrame::GetType() const
{
  return nsGkAtoms::meterFrame;
}

nsresult
nsMeterFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  // Get the NodeInfoManager and tag necessary to create the meter bar div.
  nsCOMPtr<nsIDocument> doc = mContent->GetComposedDoc();

  // Create the div.
  mBarDiv = doc->CreateHTMLElement(nsGkAtoms::div);

  // Associate ::-moz-meter-bar pseudo-element to the anonymous child.
  CSSPseudoElementType pseudoType = CSSPseudoElementType::mozMeterBar;
  RefPtr<nsStyleContext> newStyleContext = PresContext()->StyleSet()->
    ResolvePseudoElementStyle(mContent->AsElement(), pseudoType,
                              StyleContext(), mBarDiv->AsElement());

  if (!aElements.AppendElement(ContentInfo(mBarDiv, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
nsMeterFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                       uint32_t aFilter)
{
  if (mBarDiv) {
    aElements.AppendElement(mBarDiv);
  }
}

NS_QUERYFRAME_HEAD(nsMeterFrame)
  NS_QUERYFRAME_ENTRY(nsMeterFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)


void
nsMeterFrame::Reflow(nsPresContext*           aPresContext,
                     ReflowOutput&     aDesiredSize,
                     const ReflowInput& aReflowInput,
                     nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsMeterFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);

  NS_ASSERTION(mBarDiv, "Meter bar div must exist!");
  NS_ASSERTION(!GetPrevContinuation(),
               "nsMeterFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }

  nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();
  NS_ASSERTION(barFrame, "The meter frame should have a child with a frame!");

  ReflowBarFrame(barFrame, aPresContext, aReflowInput, aStatus);

  aDesiredSize.SetSize(aReflowInput.GetWritingMode(),
                       aReflowInput.ComputedSizeWithBorderPadding());

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  ConsiderChildOverflow(aDesiredSize.mOverflowAreas, barFrame);
  FinishAndStoreOverflow(&aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

void
nsMeterFrame::ReflowBarFrame(nsIFrame*                aBarFrame,
                             nsPresContext*           aPresContext,
                             const ReflowInput& aReflowInput,
                             nsReflowStatus&          aStatus)
{
  bool vertical = ResolvedOrientationIsVertical();
  WritingMode wm = aBarFrame->GetWritingMode();
  LogicalSize availSize = aReflowInput.ComputedSize(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  ReflowInput reflowInput(aPresContext, aReflowInput,
                                aBarFrame, availSize);
  nscoord size = vertical ? aReflowInput.ComputedHeight()
                          : aReflowInput.ComputedWidth();
  nscoord xoffset = aReflowInput.ComputedPhysicalBorderPadding().left;
  nscoord yoffset = aReflowInput.ComputedPhysicalBorderPadding().top;

  // NOTE: Introduce a new function getPosition in the content part ?
  HTMLMeterElement* meterElement = static_cast<HTMLMeterElement*>(mContent);

  double max = meterElement->Max();
  double min = meterElement->Min();
  double value = meterElement->Value();

  double position = max - min;
  position = position != 0 ? (value - min) / position : 1;

  size = NSToCoordRound(size * position);

  if (!vertical && (wm.IsVertical() ? wm.IsVerticalRL() : !wm.IsBidiLTR())) {
    xoffset += aReflowInput.ComputedWidth() - size;
  }

  // The bar position is *always* constrained.
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

  xoffset += reflowInput.ComputedPhysicalMargin().left;
  yoffset += reflowInput.ComputedPhysicalMargin().top;

  ReflowOutput barDesiredSize(reflowInput);
  ReflowChild(aBarFrame, aPresContext, barDesiredSize, reflowInput, xoffset,
              yoffset, 0, aStatus);
  FinishReflowChild(aBarFrame, aPresContext, barDesiredSize, &reflowInput,
                    xoffset, yoffset, 0);
}

nsresult
nsMeterFrame::AttributeChanged(int32_t  aNameSpaceID,
                               nsIAtom* aAttribute,
                               int32_t  aModType)
{
  NS_ASSERTION(mBarDiv, "Meter bar div must exist!");

  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::value ||
       aAttribute == nsGkAtoms::max   ||
       aAttribute == nsGkAtoms::min )) {
    nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();
    NS_ASSERTION(barFrame, "The meter frame should have a child with a frame!");
    PresContext()->PresShell()->FrameNeedsReflow(barFrame,
                                                 nsIPresShell::eResize,
                                                 NS_FRAME_IS_DIRTY);
    InvalidateFrame();
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                            aModType);
}

LogicalSize
nsMeterFrame::ComputeAutoSize(nsRenderingContext* aRenderingContext,
                              WritingMode         aWM,
                              const LogicalSize&  aCBSize,
                              nscoord             aAvailableISize,
                              const LogicalSize&  aMargin,
                              const LogicalSize&  aBorder,
                              const LogicalSize&  aPadding,
                              ComputeSizeFlags    aFlags)
{
  RefPtr<nsFontMetrics> fontMet =
    nsLayoutUtils::GetFontMetricsForFrame(this, 1.0f);

  const WritingMode wm = GetWritingMode();
  LogicalSize autoSize(wm);
  autoSize.BSize(wm) = autoSize.ISize(wm) = fontMet->Font().size; // 1em

  if (ResolvedOrientationIsVertical() == wm.IsVertical()) {
    autoSize.ISize(wm) *= 5; // 5em
  } else {
    autoSize.BSize(wm) *= 5; // 5em
  }

  return autoSize.ConvertTo(aWM, wm);
}

nscoord
nsMeterFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  RefPtr<nsFontMetrics> fontMet =
    nsLayoutUtils::GetFontMetricsForFrame(this, 1.0f);

  nscoord minISize = fontMet->Font().size; // 1em

  if (ResolvedOrientationIsVertical() == GetWritingMode().IsVertical()) {
    // The orientation is inline
    minISize *= 5; // 5em
  }

  return minISize;
}

nscoord
nsMeterFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  return GetMinISize(aRenderingContext);
}

bool
nsMeterFrame::ShouldUseNativeStyle() const
{
  nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();

  // Use the native style if these conditions are satisfied:
  // - both frames use the native appearance;
  // - neither frame has author specified rules setting the border or the
  //   background.
  return StyleDisplay()->mAppearance == NS_THEME_METERBAR &&
         !PresContext()->HasAuthorSpecifiedRules(this,
                                                 NS_AUTHOR_SPECIFIED_BORDER | NS_AUTHOR_SPECIFIED_BACKGROUND) &&
         barFrame &&
         barFrame->StyleDisplay()->mAppearance == NS_THEME_METERCHUNK &&
         !PresContext()->HasAuthorSpecifiedRules(barFrame,
                                                 NS_AUTHOR_SPECIFIED_BORDER | NS_AUTHOR_SPECIFIED_BACKGROUND);
}

Element*
nsMeterFrame::GetPseudoElement(CSSPseudoElementType aType)
{
  if (aType == CSSPseudoElementType::mozMeterBar) {
    return mBarDiv;
  }

  return nsContainerFrame::GetPseudoElement(aType);
}
