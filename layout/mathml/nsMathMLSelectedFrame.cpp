/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLSelectedFrame.h"
#include "nsDisplayList.h"

using namespace mozilla;

nsMathMLSelectedFrame::~nsMathMLSelectedFrame()
{
}

NS_IMETHODIMP
nsMathMLSelectedFrame::TransmitAutomaticData()
{
  // Note that to determine space-like and embellished op properties:
  //   - <semantics> behaves the same as <maction>
  //   - <annotation-xml> behaves the same as <mrow>

  // The REC defines the following element to be space-like:
  // * an maction element whose selected sub-expression exists and is
  //   space-like;
  nsIMathMLFrame* mathMLFrame = do_QueryFrame(mSelectedFrame);
  if (mathMLFrame && mathMLFrame->IsSpaceLike()) {
    mPresentationData.flags |= NS_MATHML_SPACE_LIKE;
  } else {
    mPresentationData.flags &= ~NS_MATHML_SPACE_LIKE;
  }

  // The REC defines the following element to be an embellished operator:
  // * an maction element whose selected sub-expression exists and is an
  //   embellished operator;
  mPresentationData.baseFrame = mSelectedFrame;
  GetEmbellishDataFrom(mSelectedFrame, mEmbellishData);

  return NS_OK;
}

nsresult
nsMathMLSelectedFrame::ChildListChanged(int32_t aModType)
{
  GetSelectedFrame();
  return nsMathMLContainerFrame::ChildListChanged(aModType);
}

void
nsMathMLSelectedFrame::SetInitialChildList(ChildListID     aListID,
                                           nsFrameList&    aChildList)
{
  nsMathMLContainerFrame::SetInitialChildList(aListID, aChildList);
  // This very first call to GetSelectedFrame() will cause us to be marked as an
  // embellished operator if the selected child is an embellished operator
  GetSelectedFrame();
}

//  Only paint the selected child...
void
nsMathMLSelectedFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                        const nsRect&           aDirtyRect,
                                        const nsDisplayListSet& aLists)
{
  // Report an error if something wrong was found in this frame.
  // We can't call nsDisplayMathMLError from here,
  // so ask nsMathMLContainerFrame to do the work for us.
  if (NS_MATHML_HAS_ERROR(mPresentationData.flags)) {
    nsMathMLContainerFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
    return;
  }

  DisplayBorderBackgroundOutline(aBuilder, aLists);

  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    // Put the child's background directly onto the content list
    nsDisplayListSet set(aLists, aLists.Content());
    // The children should be in content order
    BuildDisplayListForChild(aBuilder, childFrame, aDirtyRect, set);
  }

#if defined(DEBUG) && defined(SHOW_BOUNDING_BOX)
  // visual debug
  DisplayBoundingMetrics(aBuilder, this, mReference, mBoundingMetrics, aLists);
#endif
}

/* virtual */
LogicalSize
nsMathMLSelectedFrame::ComputeSize(gfxContext *aRenderingContext,
                                   WritingMode aWM,
                                   const LogicalSize& aCBSize,
                                   nscoord aAvailableISize,
                                   const LogicalSize& aMargin,
                                   const LogicalSize& aBorder,
                                   const LogicalSize& aPadding,
                                   ComputeSizeFlags aFlags)
{
  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    // Delegate size computation to the child frame.
    // Try to account for border/padding/margin on this frame and the child,
    // though we don't really support them during reflow anyway...
    nscoord availableISize = aAvailableISize - aBorder.ISize(aWM) -
        aPadding.ISize(aWM) - aMargin.ISize(aWM);
    LogicalSize cbSize = aCBSize - aBorder - aPadding - aMargin;
    SizeComputationInput offsetState(childFrame, aRenderingContext, aWM,
                                 availableISize);
    LogicalSize size =
        childFrame->ComputeSize(aRenderingContext, aWM, cbSize,
            availableISize, offsetState.ComputedLogicalMargin().Size(aWM),
            offsetState.ComputedLogicalBorderPadding().Size(aWM) -
            offsetState.ComputedLogicalPadding().Size(aWM),
            offsetState.ComputedLogicalPadding().Size(aWM),
            aFlags);
    return size + offsetState.ComputedLogicalBorderPadding().Size(aWM);
  }
  return LogicalSize(aWM);
}

// Only reflow the selected child ...
void
nsMathMLSelectedFrame::Reflow(nsPresContext*          aPresContext,
                              ReflowOutput&     aDesiredSize,
                              const ReflowInput& aReflowInput,
                              nsReflowStatus&          aStatus)
{
  MarkInReflow();
  mPresentationData.flags &= ~NS_MATHML_ERROR;
  aStatus.Reset();
  aDesiredSize.ClearSize();
  aDesiredSize.SetBlockStartAscent(0);
  mBoundingMetrics = nsBoundingMetrics();
  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    WritingMode wm = childFrame->GetWritingMode();
    LogicalSize availSize = aReflowInput.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    ReflowInput childReflowInput(aPresContext, aReflowInput,
                                       childFrame, availSize);
    ReflowChild(childFrame, aPresContext, aDesiredSize,
                childReflowInput, aStatus);
    SaveReflowAndBoundingMetricsFor(childFrame, aDesiredSize,
                                    aDesiredSize.mBoundingMetrics);
    mBoundingMetrics = aDesiredSize.mBoundingMetrics;
  }
  FinalizeReflow(aReflowInput.mRenderingContext->GetDrawTarget(), aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

// Only place the selected child ...
/* virtual */ nsresult
nsMathMLSelectedFrame::Place(DrawTarget*          aDrawTarget,
                             bool                 aPlaceOrigin,
                             ReflowOutput& aDesiredSize)
{
  nsIFrame* childFrame = GetSelectedFrame();

  if (mInvalidMarkup) {
    return ReflowError(aDrawTarget, aDesiredSize);
  }

  aDesiredSize.ClearSize();
  aDesiredSize.SetBlockStartAscent(0);
  mBoundingMetrics = nsBoundingMetrics();
  if (childFrame) {
    GetReflowAndBoundingMetricsFor(childFrame, aDesiredSize, mBoundingMetrics);
    if (aPlaceOrigin) {
      FinishReflowChild(childFrame, PresContext(), aDesiredSize, nullptr, 0, 0, 0);
    }
    mReference.x = 0;
    mReference.y = aDesiredSize.BlockStartAscent();
  }
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  return NS_OK;
}
