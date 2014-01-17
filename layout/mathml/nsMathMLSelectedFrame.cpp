/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLSelectedFrame.h"
#include "nsDisplayList.h"

nsMathMLSelectedFrame::~nsMathMLSelectedFrame()
{
}

void
nsMathMLSelectedFrame::Init(nsIContent*      aContent,
                            nsIFrame*        aParent,
                            nsIFrame*        aPrevInFlow)
{
  // Init our local attributes
  mInvalidMarkup = false;
  mSelectedFrame = nullptr;

  // Let the base class do the rest
  nsMathMLContainerFrame::Init(aContent, aParent, aPrevInFlow);
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

NS_IMETHODIMP
nsMathMLSelectedFrame::SetInitialChildList(ChildListID     aListID,
                                           nsFrameList&    aChildList)
{
  nsresult rv = nsMathMLContainerFrame::SetInitialChildList(aListID,
                                                            aChildList);
  // This very first call to GetSelectedFrame() will cause us to be marked as an
  // embellished operator if the selected child is an embellished operator
  GetSelectedFrame();
  return rv;
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

// Only reflow the selected child ...
NS_IMETHODIMP
nsMathMLSelectedFrame::Reflow(nsPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;
  aDesiredSize.Width() = aDesiredSize.Height() = 0;
  aDesiredSize.SetTopAscent(0);
  mBoundingMetrics = nsBoundingMetrics();
  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    nsSize availSize(aReflowState.ComputedWidth(), NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext, aDesiredSize,
                     childReflowState, aStatus);
    SaveReflowAndBoundingMetricsFor(childFrame, aDesiredSize,
                                    aDesiredSize.mBoundingMetrics);
    mBoundingMetrics = aDesiredSize.mBoundingMetrics;
  }
  FinalizeReflow(*aReflowState.rendContext, aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

// Only place the selected child ...
/* virtual */ nsresult
nsMathMLSelectedFrame::Place(nsRenderingContext& aRenderingContext,
                             bool                 aPlaceOrigin,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  nsIFrame* childFrame = GetSelectedFrame();

  if (mInvalidMarkup) {
    return ReflowError(aRenderingContext, aDesiredSize);
  }

  aDesiredSize.Width() = aDesiredSize.Height() = 0;
  aDesiredSize.SetTopAscent(0);
  mBoundingMetrics = nsBoundingMetrics();
  if (childFrame) {
    GetReflowAndBoundingMetricsFor(childFrame, aDesiredSize, mBoundingMetrics);
    if (aPlaceOrigin) {
      FinishReflowChild(childFrame, PresContext(), aDesiredSize, nullptr, 0, 0, 0);
    }
    mReference.x = 0;
    mReference.y = aDesiredSize.TopAscent();
  }
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  return NS_OK;
}
