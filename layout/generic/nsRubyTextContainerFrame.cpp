/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text-container" */

#include "nsRubyTextContainerFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "WritingModes.h"
#include "mozilla/UniquePtr.h"

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsRubyTextContainerFrame)
  NS_QUERYFRAME_ENTRY(nsRubyTextContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsRubyTextContainerFrame)

nsContainerFrame*
NS_NewRubyTextContainerFrame(nsIPresShell* aPresShell,
                             nsStyleContext* aContext)
{
  return new (aPresShell) nsRubyTextContainerFrame(aContext);
}


//----------------------------------------------------------------------

// nsRubyTextContainerFrame Method Implementations
// ===============================================

nsIAtom*
nsRubyTextContainerFrame::GetType() const
{
  return nsGkAtoms::rubyTextContainerFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRubyTextContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("RubyTextContainer"), aResult);
}
#endif

void
nsRubyTextContainerFrame::BeginRTCLineLayout(nsPresContext* aPresContext,
                                             const nsHTMLReflowState& aReflowState)
{
  // Construct block reflow state and line layout
  nscoord consumedBSize = GetConsumedBSize();

  ClearLineCursor();

  mISize = 0;

  nsBlockReflowState state(aReflowState, aPresContext, this, true, true,
                           false, consumedBSize);

  NS_ASSERTION(!mLines.empty(),
    "There should be at least one line in the ruby text container");
  line_iterator firstLine = begin_lines();
  mLineLayout = mozilla::MakeUnique<nsLineLayout>(
                           state.mPresContext,
                           state.mReflowState.mFloatManager,
                           &state.mReflowState, &firstLine);
  mLineLayout->Init(&state, state.mMinLineHeight, state.mLineNumber);

  mozilla::WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  mozilla::LogicalRect lineRect(state.mContentArea);
  nscoord iStart = lineRect.IStart(lineWM);
  nscoord availISize = lineRect.ISize(lineWM);
  nscoord availBSize = NS_UNCONSTRAINEDSIZE;

  mLineLayout->BeginLineReflow(iStart, state.mBCoord,
                              availISize, availBSize,
                              false,
                              false,
                              lineWM, state.mContainerWidth);
}

void
nsRubyTextContainerFrame::ReflowRubyTextFrame(
                            nsRubyTextFrame* rtFrame,
                            nsIFrame* rbFrame,
                            nscoord baseStart,
                            nsPresContext* aPresContext,
                            nsHTMLReflowMetrics& aDesiredSize,
                            const nsHTMLReflowState& aReflowState)
{
  nsReflowStatus frameReflowStatus;
  nsHTMLReflowMetrics metrics(aReflowState, aDesiredSize.mFlags);
  mozilla::WritingMode lineWM = mLineLayout->GetWritingMode();
  mozilla::LogicalSize availSize(lineWM, aReflowState.AvailableWidth(),
                   aReflowState.AvailableHeight());
  nsHTMLReflowState childReflowState(aPresContext, aReflowState, rtFrame, availSize);

  // Determine the inline coordinate for the text frame by centering over
  // the corresponding base frame
  int baseWidth;
  if (rbFrame) {
    baseWidth = rbFrame->ISize();

    // If this is the last ruby annotation, it gets paired with ALL remaining
    // ruby bases
    if (!rtFrame->GetNextSibling()) {
      rbFrame = rbFrame->GetNextSibling();
      while (rbFrame) {
        baseWidth += rbFrame->ISize();
        rbFrame = rbFrame->GetNextSibling();
      }
    }
  } else {
    baseWidth = 0;
  }
  
  int baseCenter = baseStart + baseWidth / 2;
  // FIXME: Find a way to avoid using GetPrefISize here, potentially by moving
  // the frame after it has reflowed.
  nscoord ICoord = baseCenter - rtFrame->GetPrefISize(aReflowState.rendContext) / 2;
  if (ICoord > mLineLayout->GetCurrentICoord()) {
    mLineLayout->AdvanceICoord(ICoord - mLineLayout->GetCurrentICoord());
  } 

  bool pushedFrame;
  mLineLayout->ReflowFrame(rtFrame, frameReflowStatus,
                           &metrics, pushedFrame);

  NS_ASSERTION(!pushedFrame, "Ruby line breaking is not yet implemented");

  mISize += metrics.ISize(lineWM);
  rtFrame->SetSize(nsSize(metrics.ISize(lineWM), metrics.BSize(lineWM)));
  FinishReflowChild(rtFrame, aPresContext, metrics, &childReflowState, 0, 0,
                    NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_MOVE_VIEW);
} 

/* virtual */ void
nsRubyTextContainerFrame::Reflow(nsPresContext* aPresContext,
                                 nsHTMLReflowMetrics& aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsRubyTextContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // All rt children have already been reflowed. All we need to do is clean up
  // the line layout.

  aStatus = NS_FRAME_COMPLETE;
  mozilla::WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  mozilla::WritingMode frameWM = aReflowState.GetWritingMode();
  mozilla::LogicalMargin borderPadding =
    aReflowState.ComputedLogicalBorderPadding();

  aDesiredSize.ISize(lineWM) = mISize;
  nsLayoutUtils::SetBSizeFromFontMetrics(this, aDesiredSize, aReflowState,
                                         borderPadding, lineWM, frameWM);

  nscoord bsize = aDesiredSize.BSize(lineWM);
  if (!mLines.empty()) {
    // Okay to use BlockStartAscent because it has just been correctly set by
    // nsLayoutUtils::SetBSizeFromFontMetrics.
    mLines.begin()->SetLogicalAscent(aDesiredSize.BlockStartAscent());
    mLines.begin()->SetBounds(aReflowState.GetWritingMode(), 0, 0, mISize,
                              bsize, mISize);
  }

  if (mLineLayout) {
    mLineLayout->EndLineReflow();
    mLineLayout = nullptr;
  }
}
