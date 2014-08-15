/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-base-container" */

#include "nsRubyBaseContainerFrame.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "WritingModes.h"

using namespace mozilla;

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsRubyBaseContainerFrame)
  NS_QUERYFRAME_ENTRY(nsRubyBaseContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsRubyBaseContainerFrame)

nsContainerFrame*
NS_NewRubyBaseContainerFrame(nsIPresShell* aPresShell,
                             nsStyleContext* aContext)
{
  return new (aPresShell) nsRubyBaseContainerFrame(aContext);
}


//----------------------------------------------------------------------

// nsRubyBaseContainerFrame Method Implementations
// ===============================================

nsIAtom*
nsRubyBaseContainerFrame::GetType() const
{
  return nsGkAtoms::rubyBaseContainerFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRubyBaseContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("RubyBaseContainer"), aResult);
}
#endif


void nsRubyBaseContainerFrame::AppendTextContainer(nsIFrame* aFrame)
{
  nsRubyTextContainerFrame* rtcFrame = do_QueryFrame(aFrame);
  if (rtcFrame) {
    mTextContainers.AppendElement(rtcFrame);
  }
}

void nsRubyBaseContainerFrame::ClearTextContainers() {
  mTextContainers.Clear();
}

/* virtual */ void
nsRubyBaseContainerFrame::Reflow(nsPresContext* aPresContext,
                                 nsHTMLReflowMetrics& aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsRubyBaseContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  if (!aReflowState.mLineLayout) {
    NS_ASSERTION(
      aReflowState.mLineLayout,
      "No line layout provided to RubyBaseContainerFrame reflow method.");
    aStatus = NS_FRAME_COMPLETE;
    return;
  }

  aStatus = NS_FRAME_COMPLETE;
  nscoord isize = 0;
  int baseNum = 0;
  nscoord leftoverSpace = 0;
  nscoord spaceApart = 0;
  WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  WritingMode frameWM = aReflowState.GetWritingMode();
  LogicalMargin borderPadding =
    aReflowState.ComputedLogicalBorderPadding();
  nscoord baseStart = 0;

  LogicalSize availSize(lineWM, aReflowState.AvailableWidth(),
                        aReflowState.AvailableHeight());

  // Begin the line layout for each ruby text container in advance.
  for (uint32_t i = 0; i < mTextContainers.Length(); i++) {
    nsRubyTextContainerFrame* rtcFrame = mTextContainers.ElementAt(i);
    nsHTMLReflowState rtcReflowState(aPresContext,
                                     *aReflowState.parentReflowState,
                                     rtcFrame, availSize);
    rtcReflowState.mLineLayout = aReflowState.mLineLayout;
    // FIXME: Avoid using/needing the rtcReflowState argument
    rtcFrame->BeginRTCLineLayout(aPresContext, rtcReflowState);
  }

  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    nsIFrame* rbFrame = e.get();
    if (rbFrame->GetType() != nsGkAtoms::rubyBaseFrame) {
      NS_ASSERTION(false, "Unrecognized child type for ruby base container");
      continue;
    }

    nsReflowStatus frameReflowStatus;
    nsHTMLReflowMetrics metrics(aReflowState, aDesiredSize.mFlags);

    // Determine if we need more spacing between bases in the inline direction
    // depending on the inline size of the corresponding annotations
    // FIXME: The use of GetPrefISize here and below is easier but not ideal. It
    // would be better to use metrics from reflow.
    nscoord prefWidth = rbFrame->GetPrefISize(aReflowState.rendContext);
    nscoord textWidth = 0;

    for (uint32_t i = 0; i < mTextContainers.Length(); i++) {
      nsRubyTextFrame* rtFrame = do_QueryFrame(mTextContainers.ElementAt(i)->
                          PrincipalChildList().FrameAt(baseNum));
      if (rtFrame) {
        int newWidth = rtFrame->GetPrefISize(aReflowState.rendContext);
        if (newWidth > textWidth) {
          textWidth = newWidth;
        }
      }
    }
    if (textWidth > prefWidth) {
      spaceApart = std::max((textWidth - prefWidth) / 2, spaceApart);
      leftoverSpace = spaceApart;
    } else {
      spaceApart = leftoverSpace;
      leftoverSpace = 0;
    }
    if (spaceApart > 0) {
      aReflowState.mLineLayout->AdvanceICoord(spaceApart);
    }
    baseStart = aReflowState.mLineLayout->GetCurrentICoord();

    bool pushedFrame;
    aReflowState.mLineLayout->ReflowFrame(rbFrame, frameReflowStatus,
                                          &metrics, pushedFrame);
    NS_ASSERTION(!pushedFrame, "Ruby line breaking is not yet implemented");

    isize += metrics.ISize(lineWM);
    rbFrame->SetSize(LogicalSize(lineWM, metrics.ISize(lineWM),
                                 metrics.BSize(lineWM)));
    FinishReflowChild(rbFrame, aPresContext, metrics, &aReflowState, 0, 0,
                      NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_MOVE_VIEW);

    // Now reflow the ruby text boxes that correspond to this ruby base box.
    for (uint32_t i = 0; i < mTextContainers.Length(); i++) {
      nsRubyTextFrame* rtFrame = do_QueryFrame(mTextContainers.ElementAt(i)->
                          PrincipalChildList().FrameAt(baseNum));
      nsRubyTextContainerFrame* rtcFrame = mTextContainers.ElementAt(i);
      if (rtFrame) {
        nsHTMLReflowMetrics rtcMetrics(*aReflowState.parentReflowState,
                                       aDesiredSize.mFlags);
        nsHTMLReflowState rtcReflowState(aPresContext,
                                         *aReflowState.parentReflowState,
                                         rtcFrame, availSize);
        rtcReflowState.mLineLayout = rtcFrame->GetLineLayout();
        rtcFrame->ReflowRubyTextFrame(rtFrame, rbFrame, baseStart,
                                      aPresContext, rtcMetrics,
                                      rtcReflowState);
      }
    }
    baseNum++;
  }

  // Reflow ruby annotations which do not have a corresponding ruby base box due
  // to a ruby base shortage. According to the spec, an empty ruby base is
  // assumed to exist for each of these annotations.
  bool continueReflow = true;
  while (continueReflow) {
    continueReflow = false;
    for (uint32_t i = 0; i < mTextContainers.Length(); i++) {
      nsRubyTextFrame* rtFrame = do_QueryFrame(mTextContainers.ElementAt(i)->
                          PrincipalChildList().FrameAt(baseNum));
      nsRubyTextContainerFrame* rtcFrame = mTextContainers.ElementAt(i);
      if (rtFrame) {
        continueReflow = true;
        nsHTMLReflowMetrics rtcMetrics(*aReflowState.parentReflowState,
                                       aDesiredSize.mFlags);
        nsHTMLReflowState rtcReflowState(aPresContext,
                                         *aReflowState.parentReflowState,
                                         rtcFrame, availSize);
        rtcReflowState.mLineLayout = rtcFrame->GetLineLayout();
        rtcFrame->ReflowRubyTextFrame(rtFrame, nullptr, baseStart,
                                      aPresContext, rtcMetrics,
                                      rtcReflowState);
        // Update the inline coord to make space for subsequent ruby annotations
        // (since there is no corresponding base inline size to use).
        baseStart += rtcMetrics.ISize(lineWM);
      }
    }
    baseNum++;
  }

  aDesiredSize.ISize(lineWM) = isize;
  nsLayoutUtils::SetBSizeFromFontMetrics(this, aDesiredSize, aReflowState,
                                         borderPadding, lineWM, frameWM);
}
