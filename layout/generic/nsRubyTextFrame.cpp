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
NS_QUERYFRAME_TAIL_INHERITING(nsRubyTextFrameSuper)

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

/* virtual */ bool
nsRubyTextFrame::CanContinueTextRun() const
{
  return false;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRubyTextFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("RubyText"), aResult);
}
#endif



/* virtual */ void
nsRubyTextFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  if (GetStateBits() & NS_RUBY_TEXT_FRAME_AUTOHIDE) {
    return;
  }

  nsRubyTextFrameSuper::BuildDisplayList(aBuilder, aDirtyRect, aLists);
}

/* virtual */ void
nsRubyTextFrame::Reflow(nsPresContext* aPresContext,
                        nsHTMLReflowMetrics& aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus& aStatus)
{
  // Even if we want to hide this frame, we have to reflow it first.
  // If we leave it dirty, changes to its content will never be
  // propagated to the ancestors, then it won't be displayed even if
  // the content is no longer the same, until next reflow triggered by
  // some other change. In general, we always reflow all the frames we
  // created. There might be other problems if we don't do that.
  nsRubyTextFrameSuper::Reflow(aPresContext, aDesiredSize,
                               aReflowState, aStatus);

  if (GetStateBits() & NS_RUBY_TEXT_FRAME_AUTOHIDE) {
    // Reset the ISize. The BSize is not changed so that it won't
    // affect vertical positioning in unexpected way.
    WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
    aDesiredSize.ISize(lineWM) = 0;
    aDesiredSize.SetOverflowAreasToDesiredBounds();
  }
}
