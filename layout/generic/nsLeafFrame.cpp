/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for rendering objects that do not have child lists */

#include "nsCOMPtr.h"
#include "nsLeafFrame.h"
#include "nsContainerFrame.h"
#include "nsHTMLParts.h"
#include "nsPresContext.h"

nsLeafFrame::~nsLeafFrame()
{
}

NS_IMPL_FRAMEARENA_HELPERS(nsLeafFrame)

/* virtual */ nscoord
nsLeafFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  result = GetIntrinsicWidth();
  return result;
}

/* virtual */ nscoord
nsLeafFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  result = GetIntrinsicWidth();
  return result;
}

/* virtual */ nsSize
nsLeafFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder,
                             nsSize aPadding, bool aShrinkWrap)
{
  return nsSize(GetIntrinsicWidth(), GetIntrinsicHeight());
}

NS_IMETHODIMP
nsLeafFrame::Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsLeafFrame");
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsLeafFrame::Reflow: aMaxSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  DoReflow(aPresContext, aMetrics, aReflowState, aStatus);

  FinishAndStoreOverflow(&aMetrics);
  return NS_OK;
}

NS_IMETHODIMP
nsLeafFrame::DoReflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus)
{
  NS_ASSERTION(aReflowState.ComputedWidth() != NS_UNCONSTRAINEDSIZE,
               "Shouldn't have unconstrained stuff here "
               "Thanks to the rules of reflow");
  NS_ASSERTION(NS_INTRINSICSIZE != aReflowState.ComputedHeight(),
               "Shouldn't have unconstrained stuff here "
               "thanks to ComputeAutoSize");  

  aMetrics.width = aReflowState.ComputedWidth();
  aMetrics.height = aReflowState.ComputedHeight();
  
  AddBordersAndPadding(aReflowState, aMetrics);
  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsLeafFrame::DoReflow: size=%d,%d",
                  aMetrics.width, aMetrics.height));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);

  aMetrics.SetOverflowAreasToDesiredBounds();

  return NS_OK;
}

nscoord
nsLeafFrame::GetIntrinsicHeight()
{
  NS_NOTREACHED("Someone didn't override Reflow or ComputeAutoSize");
  return 0;
}

// XXX how should border&padding effect baseline alignment?
// => descent = borderPadding.bottom for example
void
nsLeafFrame::AddBordersAndPadding(const nsHTMLReflowState& aReflowState,
                                  nsHTMLReflowMetrics& aMetrics)
{
  aMetrics.width += aReflowState.mComputedBorderPadding.LeftRight();
  aMetrics.height += aReflowState.mComputedBorderPadding.TopBottom();
}

void
nsLeafFrame::SizeToAvailSize(const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width  = aReflowState.availableWidth; // FRAME
  aDesiredSize.height = aReflowState.availableHeight;
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize);  
}
