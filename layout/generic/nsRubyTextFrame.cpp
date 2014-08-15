/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby-text" */

#include "nsRubyTextFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "WritingModes.h"
#include "nsLineLayout.h"

using namespace mozilla;

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsRubyTextFrame)
  NS_QUERYFRAME_ENTRY(nsRubyTextFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsRubyTextFrame)

nsContainerFrame*
NS_NewRubyTextFrame(nsIPresShell* aPresShell,
                    nsStyleContext* aContext)
{
  return new (aPresShell) nsRubyTextFrame(aContext);
}


//----------------------------------------------------------------------

// nsRubyTextFrame Method Implementations
// ======================================

nsIAtom*
nsRubyTextFrame::GetType() const
{
  return nsGkAtoms::rubyTextFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRubyTextFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("RubyText"), aResult);
}
#endif

/* virtual */ bool 
nsRubyTextFrame::IsFrameOfType(uint32_t aFlags) const 
{
  return nsContainerFrame::IsFrameOfType(aFlags & 
         ~(nsIFrame::eLineParticipant));
}

/* virtual */ nscoord
nsRubyTextFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::MinISizeFromInline(this, aRenderingContext);
}

/* virtual */ nscoord
nsRubyTextFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::PrefISizeFromInline(this, aRenderingContext);
}

/* virtual */ void
nsRubyTextFrame::AddInlineMinISize(nsRenderingContext *aRenderingContext,
                                   nsIFrame::InlineMinISizeData *aData)
{
  for (nsFrameList::Enumerator e(PrincipalChildList()); !e.AtEnd(); e.Next()) {
    e.get()->AddInlineMinISize(aRenderingContext, aData);
  }
}

/* virtual */ void
nsRubyTextFrame::AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                                    nsIFrame::InlinePrefISizeData *aData)
{
  for (nsFrameList::Enumerator e(PrincipalChildList()); !e.AtEnd(); e.Next()) {
    e.get()->AddInlinePrefISize(aRenderingContext, aData);
  }
}

/* virtual */ nscoord
nsRubyTextFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  return mBaseline;
}

/* virtual */ void
nsRubyTextFrame::Reflow(nsPresContext* aPresContext,
                        nsHTMLReflowMetrics& aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsRubyBaseFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  
  if (!aReflowState.mLineLayout) {
    NS_ASSERTION(aReflowState.mLineLayout,
                 "No line layout provided to RubyTextFrame reflow method.");
    aStatus = NS_FRAME_COMPLETE;
    return;
  }

  WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  WritingMode frameWM = aReflowState.GetWritingMode();
  LogicalMargin borderPadding = aReflowState.ComputedLogicalBorderPadding();
  aStatus = NS_FRAME_COMPLETE;
  LogicalSize availSize(lineWM, aReflowState.AvailableWidth(),
                        aReflowState.AvailableHeight());

  // Begin the span for the ruby text frame
  nscoord availableISize = aReflowState.AvailableISize();
  NS_ASSERTION(availableISize != NS_UNCONSTRAINEDSIZE,
               "should no longer use available widths");
  // Subtract off inline axis border+padding from availableISize
  availableISize -= borderPadding.IStartEnd(frameWM);
  aReflowState.mLineLayout->BeginSpan(this, &aReflowState,
                                      borderPadding.IStart(frameWM),
                                      availableISize, &mBaseline);

  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    nsReflowStatus frameReflowStatus;
    nsHTMLReflowMetrics metrics(aReflowState, aDesiredSize.mFlags);

    bool pushedFrame;
    aReflowState.mLineLayout->ReflowFrame(e.get(), frameReflowStatus,
                                          &metrics, pushedFrame);
    NS_ASSERTION(!pushedFrame,
                 "Ruby line breaking is not yet implemented");
    e.get()->SetSize(LogicalSize(lineWM, metrics.ISize(lineWM),
                                 metrics.BSize(lineWM)));
  }

  aDesiredSize.ISize(lineWM) = aReflowState.mLineLayout->EndSpan(this);
  nsLayoutUtils::SetBSizeFromFontMetrics(this, aDesiredSize, aReflowState,
                                         borderPadding, lineWM, frameWM);

}
