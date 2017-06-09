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
nsLeafFrame::GetMinISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  result = GetIntrinsicISize();
  return result;
}

/* virtual */ nscoord
nsLeafFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  result = GetIntrinsicISize();
  return result;
}

/* virtual */
LogicalSize
nsLeafFrame::ComputeAutoSize(gfxContext*         aRenderingContext,
                             WritingMode         aWM,
                             const LogicalSize&  aCBSize,
                             nscoord             aAvailableISize,
                             const LogicalSize&  aMargin,
                             const LogicalSize&  aBorder,
                             const LogicalSize&  aPadding,
                             ComputeSizeFlags    aFlags)
{
  const WritingMode wm = GetWritingMode();
  LogicalSize result(wm, GetIntrinsicISize(), GetIntrinsicBSize());
  return result.ConvertTo(aWM, wm);
}

void
nsLeafFrame::Reflow(nsPresContext* aPresContext,
                    ReflowOutput& aMetrics,
                    const ReflowInput& aReflowInput,
                    nsReflowStatus& aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsLeafFrame");
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsLeafFrame::Reflow: aMaxSize=%d,%d",
                  aReflowInput.AvailableWidth(), aReflowInput.AvailableHeight()));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  DoReflow(aPresContext, aMetrics, aReflowInput, aStatus);

  FinishAndStoreOverflow(&aMetrics, aReflowInput.mStyleDisplay);
}

void
nsLeafFrame::DoReflow(nsPresContext* aPresContext,
                      ReflowOutput& aMetrics,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus)
{
  NS_ASSERTION(aReflowInput.ComputedWidth() != NS_UNCONSTRAINEDSIZE,
               "Shouldn't have unconstrained stuff here "
               "thanks to the rules of reflow");
  NS_ASSERTION(NS_INTRINSICSIZE != aReflowInput.ComputedHeight(),
               "Shouldn't have unconstrained stuff here "
               "thanks to ComputeAutoSize");

  // XXX how should border&padding effect baseline alignment?
  // => descent = borderPadding.bottom for example
  WritingMode wm = aReflowInput.GetWritingMode();
  aMetrics.SetSize(wm, aReflowInput.ComputedSizeWithBorderPadding());

  aStatus.Reset();

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsLeafFrame::DoReflow: size=%d,%d",
                  aMetrics.ISize(wm), aMetrics.BSize(wm)));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);

  aMetrics.SetOverflowAreasToDesiredBounds();
}

nscoord
nsLeafFrame::GetIntrinsicBSize()
{
  NS_NOTREACHED("Someone didn't override Reflow or ComputeAutoSize");
  return 0;
}

void
nsLeafFrame::SizeToAvailSize(const ReflowInput& aReflowInput,
                             ReflowOutput& aDesiredSize)
{
  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalSize size(wm, aReflowInput.AvailableISize(), // FRAME
                   aReflowInput.AvailableBSize());
  aDesiredSize.SetSize(wm, size);
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize, aReflowInput.mStyleDisplay);
}
