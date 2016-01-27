/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for rendering objects that do not have child lists */

#include "nsLeafFrame.h"
#include "nsPresContext.h"

using namespace mozilla;

nsLeafFrame::~nsLeafFrame()
{
}

/* virtual */ nscoord
nsLeafFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  result = GetIntrinsicISize();
  return result;
}

/* virtual */ nscoord
nsLeafFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  result = GetIntrinsicISize();
  return result;
}

/* virtual */
LogicalSize
nsLeafFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                             WritingMode aWM,
                             const LogicalSize& aCBSize,
                             nscoord aAvailableISize,
                             const LogicalSize& aMargin,
                             const LogicalSize& aBorder,
                             const LogicalSize& aPadding,
                             bool aShrinkWrap)
{
  const WritingMode wm = GetWritingMode();
  LogicalSize result(wm, GetIntrinsicISize(), GetIntrinsicBSize());
  return result.ConvertTo(aWM, wm);
}

void
nsLeafFrame::Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsLeafFrame");
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsLeafFrame::Reflow: aMaxSize=%d,%d",
                  aReflowState.AvailableWidth(), aReflowState.AvailableHeight()));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  DoReflow(aPresContext, aMetrics, aReflowState, aStatus);

  FinishAndStoreOverflow(&aMetrics);
}

void
nsLeafFrame::DoReflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus)
{
  NS_ASSERTION(aReflowState.ComputedWidth() != NS_UNCONSTRAINEDSIZE,
               "Shouldn't have unconstrained stuff here "
               "thanks to the rules of reflow");
  NS_ASSERTION(NS_INTRINSICSIZE != aReflowState.ComputedHeight(),
               "Shouldn't have unconstrained stuff here "
               "thanks to ComputeAutoSize");

  // XXX how should border&padding effect baseline alignment?
  // => descent = borderPadding.bottom for example
  WritingMode wm = aReflowState.GetWritingMode();
  aMetrics.SetSize(wm, aReflowState.ComputedSizeWithBorderPadding());

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsLeafFrame::DoReflow: size=%d,%d",
                  aMetrics.ISize(wm), aMetrics.BSize(wm)));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);

  aMetrics.SetOverflowAreasToDesiredBounds();
}

nscoord
nsLeafFrame::GetIntrinsicBSize()
{
  NS_NOTREACHED("Someone didn't override Reflow or ComputeAutoSize");
  return 0;
}

void
nsLeafFrame::SizeToAvailSize(const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  WritingMode wm = aReflowState.GetWritingMode();
  LogicalSize size(wm, aReflowState.AvailableISize(), // FRAME
                   aReflowState.AvailableBSize());
  aDesiredSize.SetSize(wm, size);
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize);  
}
