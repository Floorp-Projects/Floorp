/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsTableOuterFrame.h"
#include "nsTableFrame.h"
#include "nsBodyFrame.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsCSSLayout.h"
#include "nsVoidArray.h"
#include "nsIReflowCommand.h"
#include "nsIPtr.h"
#include "prinrval.h"
#include "nsHTMLIIDs.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsTiming = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
#define NOISY_MARGINS
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsTiming = PR_FALSE;
#endif

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

struct OuterTableReflowState {

  // The presentation context
  nsIPresContext *pc;

  // Our reflow state
  const nsHTMLReflowState& reflowState;

  // The total available size (computed from the parent)
  nsSize availSize;
  // The available size for the inner table frame
  nsSize innerTableMaxSize;

  // Margin tracking information
  nscoord prevMaxPosBottomMargin;
  nscoord prevMaxNegBottomMargin;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running y-offset
  nscoord y;

  OuterTableReflowState(nsIPresContext*          aPresContext,
                        const nsHTMLReflowState& aReflowState)
    : reflowState(aReflowState)
  {
    pc = aPresContext;
    availSize.width = reflowState.maxSize.width;
    availSize.height = reflowState.maxSize.height;
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    y=0;  // border/padding/margin???
    unconstrainedWidth = PRBool(aReflowState.maxSize.width == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(aReflowState.maxSize.height == NS_UNCONSTRAINEDSIZE);
    innerTableMaxSize.width=0;
    innerTableMaxSize.height=0;
  }

  ~OuterTableReflowState() {
  }
};



/* ----------- nsTableOuterFrame ---------- */

/**
  */
nsTableOuterFrame::nsTableOuterFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsContainerFrame(aContent, aParentFrame),
  mInnerTableFrame(nsnull),
  mCaptionFrame(nsnull),
  mMinCaptionWidth(0),
  mDesiredSize(nsnull)
{
}

NS_IMETHODIMP nsTableOuterFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  mFirstChild = aChildList;

  // Set our internal member data
  mInnerTableFrame = mFirstChild;
  if (2 == LengthOf(mFirstChild)) {
    mFirstChild->GetNextSibling(mCaptionFrame);
  }

  return NS_OK;
}

NS_METHOD nsTableOuterFrame::Paint(nsIPresContext& aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   const nsRect& aDirtyRect)
{
  // for debug...
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

PRBool nsTableOuterFrame::NeedsReflow(const nsSize& aMaxSize)
{
  PRBool result=PR_TRUE;
  if (nsnull!=mInnerTableFrame)
    result = ((nsTableFrame *)mInnerTableFrame)->NeedsReflow(aMaxSize);
  return result;
}

// Recover the reflow state to what it should be if aKidFrame is about
// to be reflowed
nsresult nsTableOuterFrame::RecoverState(OuterTableReflowState& aState,
                                         nsIFrame*              aKidFrame)
{
  // Get aKidFrame's previous sibling
  nsIFrame* prevKidFrame = nsnull;
  for (nsIFrame* frame = mFirstChild; frame != aKidFrame;) {
    prevKidFrame = frame;
    frame->GetNextSibling(frame);
  }

  if (nsnull != prevKidFrame) {
    nsRect  rect;

    // Set our running y-offset
    prevKidFrame->GetRect(rect);
    aState.y = rect.YMost();

    // Adjust the available space
    if (PR_FALSE == aState.unconstrainedHeight) {
      aState.availSize.height -= aState.y;
    }

    // Get the previous frame's bottom margin
    const nsStyleSpacing* kidSpacing;
    prevKidFrame->GetStyleData(eStyleStruct_Spacing, (nsStyleStruct *&)kidSpacing);
    nsMargin margin;
    kidSpacing->CalcMarginFor(prevKidFrame, margin);
    if (margin.bottom < 0) {
      aState.prevMaxNegBottomMargin = -margin.bottom;
    } else {
      aState.prevMaxPosBottomMargin = margin.bottom;
    }
  }

  // Set the inner table max size
  nsSize  innerTableSize(0,0);

  mInnerTableFrame->GetSize(innerTableSize);
  aState.innerTableMaxSize.width = innerTableSize.width;
  aState.innerTableMaxSize.height = aState.reflowState.maxSize.height;

  return NS_OK;
}

nsresult nsTableOuterFrame::AdjustSiblingsAfterReflow(nsIPresContext*        aPresContext,
                                                      OuterTableReflowState& aState,
                                                      nsIFrame*              aKidFrame,
                                                      nscoord                aDeltaY)
{
  nsIFrame* lastKidFrame = aKidFrame;

  if (aDeltaY != 0) {
    // Move the frames that follow aKidFrame by aDeltaY
    nsIFrame* kidFrame;

    aKidFrame->GetNextSibling(kidFrame);
    while (nsnull != kidFrame) {
      nsPoint origin;
  
      // XXX We can't just slide the child if it has a next-in-flow
      kidFrame->GetOrigin(origin);
      origin.y += aDeltaY;
  
      // XXX We need to send move notifications to the frame...
      nsIHTMLReflow*  htmlReflow;
      if (NS_OK == kidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
        htmlReflow->WillReflow(*aPresContext);
      }
      kidFrame->MoveTo(origin.x, origin.y);

      // Get the next frame
      lastKidFrame = kidFrame;
      kidFrame->GetNextSibling(kidFrame);
    }

  } else {
    // Get the last frame
    lastKidFrame = LastFrame(mFirstChild);
  }

  // Update our running y-offset to reflect the bottommost child
  nsRect  rect;

  lastKidFrame->GetRect(rect);
  aState.y = rect.YMost();

  // Get the bottom margin for the last child frame
  const nsStyleSpacing* kidSpacing;
  lastKidFrame->GetStyleData(eStyleStruct_Spacing, (nsStyleStruct *&)kidSpacing);
  nsMargin margin;
  kidSpacing->CalcMarginFor(lastKidFrame, margin);
  if (margin.bottom < 0) {
    aState.prevMaxNegBottomMargin = -margin.bottom;
  } else {
    aState.prevMaxPosBottomMargin = margin.bottom;
  }

  return NS_OK;
}

nsresult nsTableOuterFrame::IncrementalReflow(nsIPresContext* aPresContext,
                                              OuterTableReflowState& aState,
                                              nsHTMLReflowMetrics& aDesiredSize,
                                              const nsHTMLReflowState& aReflowState,
                                              nsReflowStatus& aStatus)
{
  nsresult  rv = NS_OK;

  // Get the next frame in the reflow chain
  nsIFrame* kidFrame;
  aReflowState.reflowCommand->GetNext(kidFrame);

  // Recover our reflow state
  RecoverState(aState, kidFrame);

  // Pass along the reflow command
  // XXX Check if we're the target...
  nsSize          kidMaxElementSize(0,0);
  nsSize*         pKidMaxElementSize = (nsnull != aDesiredSize.maxElementSize) ?
                                                  &kidMaxElementSize : nsnull;
  nsHTMLReflowMetrics kidSize(pKidMaxElementSize);
  nsRect              oldKidRect;

  kidFrame->GetRect(oldKidRect);

  // Get the top margin for the child
  const nsStyleSpacing* kidSpacing;
  kidFrame->GetStyleData(eStyleStruct_Spacing, (nsStyleStruct *&)kidSpacing);
  nsMargin kidMargin;
  kidSpacing->CalcMarginFor(kidFrame, kidMargin);
  nscoord topMargin = kidMargin.top;
  nscoord bottomMargin = kidMargin.bottom;

  // Figure out the amount of available size for the child (subtract
  // off the top margin we are going to apply to it)
  if (PR_FALSE == aState.unconstrainedHeight) {
    aState.availSize.height -= topMargin;
  }
  // Subtract off for left and right margin
  if (PR_FALSE == aState.unconstrainedWidth) {
    aState.availSize.width -= kidMargin.left + kidMargin.right;
  }

  // Now do the reflow
  aState.y += topMargin;
  nsIHTMLReflow* htmlReflow;
  if (NS_OK == kidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
    htmlReflow->WillReflow(*aPresContext);
    nsHTMLReflowState kidReflowState(kidFrame, aState.reflowState, aState.availSize);
    if (kidFrame != mInnerTableFrame) {
      // Reflow captions to the width of the inner table
      kidReflowState.maxSize.width = aState.innerTableMaxSize.width;
    }
    htmlReflow->Reflow(*aPresContext, kidSize, kidReflowState, aStatus);

    // Place the child frame after taking into account its margin
    nsRect kidRect (kidMargin.left, aState.y, kidSize.width, kidSize.height);
    PlaceChild(aState, kidFrame, kidRect, aDesiredSize.maxElementSize, kidMaxElementSize);
    if (bottomMargin < 0) {
      aState.prevMaxNegBottomMargin = -bottomMargin;
    } else {
      aState.prevMaxPosBottomMargin = bottomMargin;
    }

    // XXX We're not correctly dealing with maxElementSize...
  
    // XXX The type of kidFrame matters
    //      if it's this table's caption, we need to get a new maxElementSize and if the
    //      maxElementSize > tableWidth, reflow the table
    //      otherwise, it's destined for the inner table.  Do the inner table's
    //      incremental reflow, and then reflow the caption if necessary.
  
    // Adjust the frames that follow
    rv = AdjustSiblingsAfterReflow(aPresContext, aState, kidFrame,
                                   kidRect.YMost() - oldKidRect.YMost());
  }

  return rv;
}

/**
 * Called by the Reflow() member function to compute the table width
 */
nscoord nsTableOuterFrame::GetTableWidth(const nsHTMLReflowState& aReflowState)
{
  nscoord maxWidth;

  // Figure out the overall table width constraint. Default case, get 100% of
  // available space
  if (NS_UNCONSTRAINEDSIZE == aReflowState.maxSize.width) {
    maxWidth = aReflowState.maxSize.width;

  } else {
    const nsStylePosition* position =
      (const nsStylePosition*)mStyleContext->GetStyleData(eStyleStruct_Position);
  
    switch (position->mWidth.GetUnit()) {
    case eStyleUnit_Coord:
      maxWidth = position->mWidth.GetCoordValue();
      break;
  
    case eStyleUnit_Auto:
      maxWidth = aReflowState.maxSize.width;
      break;
  
    case eStyleUnit_Percent:
      maxWidth = (nscoord)((float)aReflowState.maxSize.width *
                           position->mWidth.GetPercentValue());
      break;

    case eStyleUnit_Proportional:
    case eStyleUnit_Inherit:
      // XXX for now these fall through
  
    default:
      maxWidth = aReflowState.maxSize.width;
      break;
    }
  
    // Subtract out border and padding
    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    nsMargin borderPadding;
    spacing->CalcBorderPaddingFor(this, borderPadding);
  
    maxWidth -= borderPadding.left + borderPadding.right;
    if (maxWidth <= 0) {
      // Nonsense style specification
      maxWidth = 0;
    }
  }

  return maxWidth;
}

/**
  * Reflow is a multi-step process.
  * 1. First we reflow the caption frame and get its maximum element size. We
  *    do this once during our initial reflow and whenever the caption changes
  *    incrementally
  * 2. Next we reflow the inner table. This gives us the actual table width.
  *    The table must be at least as wide as the caption maximum element size
  * 3. Now that we have the table width we reflow the caption and gets its
  *    desired height
  * 4. Then we place the caption and the inner table
  *
  * If the table height is constrained, e.g. page mode, then it's possible the
  * inner table no longer fits and has to be reflowed again this time with s
  * smaller available height
  */
NS_METHOD nsTableOuterFrame::Reflow(nsIPresContext& aPresContext,
                                    nsHTMLReflowMetrics& aDesiredSize,
                                    const nsHTMLReflowState& aReflowState,
                                    nsReflowStatus& aStatus)
{
  if (PR_TRUE==gsDebug)
    printf("%p: nsTableOuterFrame::Reflow : maxSize=%d,%d\n",
           this, aReflowState.maxSize.width, aReflowState.maxSize.height);

  PRIntervalTime startTime;
  if (gsTiming) {
    startTime = PR_IntervalNow();
  }

  // Initialize out parameters
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }
  aStatus = NS_FRAME_COMPLETE;

  // Initialize our local reflow state
  OuterTableReflowState state(&aPresContext, aReflowState);
  if (eReflowReason_Incremental == aReflowState.reason) {
    IncrementalReflow(&aPresContext, state, aDesiredSize, aReflowState, aStatus);

    // Return our desired rect
    aDesiredSize.width  = state.innerTableMaxSize.width;

  } else {
    if (eReflowReason_Initial == aReflowState.reason) {
      //KIPP/TROY:  uncomment the following line for your own testing, do not check it in
      // NS_ASSERTION(nsnull == mFirstChild, "unexpected reflow reason");

      // Set up our kids.  They're already present, on an overflow list, 
      // or there are none so we'll create them now
      MoveOverflowToChildList();

      // Lay out the caption and get its maximum element size
      if (nsnull != mCaptionFrame) {
        nsSize              maxElementSize;
        nsHTMLReflowMetrics captionSize(&maxElementSize);
        nsHTMLReflowState   captionReflowState(mCaptionFrame, aReflowState,
                                               nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE),
                                               eReflowReason_Initial);
        nsIHTMLReflow*      htmlReflow;

        if (NS_OK == mCaptionFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
          htmlReflow->WillReflow(aPresContext);
          htmlReflow->Reflow(aPresContext, captionSize, captionReflowState, aStatus);
          mMinCaptionWidth = maxElementSize.width;
        }
      }
    }

    // At this point, we must have an inner table frame, and we might have a caption
    NS_ASSERTION(nsnull != mFirstChild, "no children");
    NS_ASSERTION(nsnull != mInnerTableFrame, "no mInnerTableFrame");

    // Compute the width to use for the table. In the case of an auto sizing
    // table this represents the maximum available width
    nscoord tableWidth = GetTableWidth(aReflowState);

    // If the caption max element size is larger, then use it instead.
    if (mMinCaptionWidth > tableWidth) {
      tableWidth = mMinCaptionWidth;
    }

    // First reflow the inner table
    nsHTMLReflowState   innerReflowState(mInnerTableFrame, aReflowState,
                                         nsSize(tableWidth, aReflowState.maxSize.height));
    nsHTMLReflowMetrics innerSize(aDesiredSize.maxElementSize); 

    ReflowChild(mInnerTableFrame, aPresContext, innerSize, innerReflowState, aStatus);

    // Table's max element size is the MAX of the caption's max element size
    // and the inner table's max element size...
    if (nsnull != aDesiredSize.maxElementSize) {
      if (mMinCaptionWidth > aDesiredSize.maxElementSize->width) {
        aDesiredSize.maxElementSize->width = mMinCaptionWidth;
      }
    }
    state.innerTableMaxSize.width = innerSize.width;

    // Now that we know the table width we can reflow the caption, and
    // place the caption and the inner table
    if (nsnull != mCaptionFrame) {
      // Get the caption's margin
      nsMargin              captionMargin;
      const nsStyleSpacing* spacing;

      mCaptionFrame->GetStyleData(eStyleStruct_Spacing, (nsStyleStruct *&)spacing);
      spacing->CalcMarginFor(mCaptionFrame, captionMargin);

      // Compute the caption's y-origin
      nscoord captionY = captionMargin.top;
      const nsStyleText* captionTextStyle;
      mCaptionFrame->GetStyleData(eStyleStruct_Text, ((nsStyleStruct *&)captionTextStyle));
      if ((captionTextStyle->mVerticalAlign.GetUnit()==eStyleUnit_Enumerated) &&
          (captionTextStyle->mVerticalAlign.GetIntValue()==NS_STYLE_VERTICAL_ALIGN_BOTTOM)) {
        captionY += innerSize.height;
      }

      // Reflow the caption. Let it be as high as it wants
      nsHTMLReflowState   captionReflowState(mCaptionFrame, state.reflowState,
                                             nsSize(innerSize.width, NS_UNCONSTRAINEDSIZE),
                                             eReflowReason_Resize);
      nsHTMLReflowMetrics captionSize(nsnull);
      nsIHTMLReflow*      htmlReflow;
      nsRect captionRect(captionMargin.left, captionY, 0, 0);

      if (NS_OK == mCaptionFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
        nsReflowStatus  captionStatus;
        htmlReflow->WillReflow(aPresContext);
        htmlReflow->Reflow(aPresContext, captionSize, captionReflowState,
                           captionStatus);
        NS_ASSERTION(NS_FRAME_IS_COMPLETE(captionStatus), "unexpected reflow status");

        // XXX If the height is constrained then we need to check whether the inner
        // table still fits...

        // Place the caption
        captionRect.SizeTo(captionSize.width, captionSize.height);
        mCaptionFrame->SetRect(captionRect);
      }

      // Place the inner table
      nscoord innerY;
      if ((captionTextStyle->mVerticalAlign.GetUnit()!=eStyleUnit_Enumerated) ||
          (captionTextStyle->mVerticalAlign.GetIntValue()!=NS_STYLE_VERTICAL_ALIGN_BOTTOM)) {
        // top caption
        innerY = captionRect.YMost() + captionMargin.bottom;
        state.y = innerY + innerSize.height;
      } else {
        // bottom caption
        innerY = 0;
        state.y = captionRect.YMost() + captionMargin.bottom;
      }
      nsRect innerRect(0, innerY, innerSize.width, innerSize.height);
      mInnerTableFrame->SetRect(innerRect);

    } else {
      // Place the inner table
      nsRect innerRect(0, 0, innerSize.width, innerSize.height);
      mInnerTableFrame->SetRect(innerRect);
      state.y = innerSize.height;
    }
  }
  
  /* if we're incomplete, take up all the remaining height so we don't waste time
   * trying to lay out in a slot that we know isn't tall enough to fit our minimum.
   * otherwise, we're as tall as our kids want us to be */
  if (NS_FRAME_IS_NOT_COMPLETE(aStatus))
    aDesiredSize.height = aReflowState.maxSize.height;
  else 
    aDesiredSize.height = state.y;
  // Return our desired rect
  aDesiredSize.width = state.innerTableMaxSize.width;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (gsDebug==PR_TRUE) 
  {
    if (nsnull!=aDesiredSize.maxElementSize)
      printf("%p: Outer frame Reflow complete, returning %s with aDesiredSize = %d,%d and aMaxElementSize=%d,%d\n",
              this, NS_FRAME_IS_COMPLETE(aStatus)? "Complete" : "Not Complete",
              aDesiredSize.width, aDesiredSize.height, 
              aDesiredSize.maxElementSize->width, aDesiredSize.maxElementSize->height);
    else
      printf("%p: Outer frame Reflow complete, returning %s with aDesiredSize = %d,%d and NSNULL aMaxElementSize\n",
              this, NS_FRAME_IS_COMPLETE(aStatus)? "Complete" : "Not Complete",
              aDesiredSize.width, aDesiredSize.height);
  }

  if (gsTiming) {
    PRIntervalTime endTime = PR_IntervalNow();
    printf("Table reflow took %ld ticks for frame %d\n",
           endTime-startTime, this);/* XXX need to use LL_* macros! */
  }

  return NS_OK;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame, and includes
// any left/top margin.
void nsTableOuterFrame::PlaceChild( OuterTableReflowState& aState,
                                    nsIFrame*          aKidFrame,
                                    const nsRect&      aKidRect,
                                    nsSize*            aMaxElementSize,
                                    nsSize&            aKidMaxElementSize)
{
  if (PR_TRUE==gsDebug) 
    printf ("outer table place child: %p with aKidRect %d %d %d %d\n", 
                                 aKidFrame, aKidRect.x, aKidRect.y,
                                 aKidRect.width, aKidRect.height);
  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // Adjust the running y-offset
  aState.y += aKidRect.height;

  // If our height is constrained then update the available height
  if (PR_FALSE == aState.unconstrainedHeight) {
    aState.availSize.height -= aKidRect.height;
  }

  /* Update the maximum element size, which is the sum of:
   *   the maxElementSize of our first row
   *   plus the maxElementSize of the top caption if we include it
   *   plus the maxElementSize of the bottom caption if we include it
   */
  if (aKidFrame == mCaptionFrame)
  {
    if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = aKidMaxElementSize.width;
      aMaxElementSize->height = aKidMaxElementSize.height;
    }
  }
}


NS_METHOD
nsTableOuterFrame::CreateContinuingFrame(nsIPresContext&  aPresContext,
                                         nsIFrame*        aParent,
                                         nsIStyleContext* aStyleContext,
                                         nsIFrame*&       aContinuingFrame)
{
  nsTableOuterFrame* cf = new nsTableOuterFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  if (PR_TRUE==gsDebug)
    printf("nsTableOuterFrame::CCF parent = %p, this=%p, cf=%p\n", aParent, this, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

void
nsTableOuterFrame::PrepareContinuingFrame(nsIPresContext&    aPresContext,
                                          nsIFrame*          aParent,
                                          nsIStyleContext*   aStyleContext,
                                          nsTableOuterFrame* aContFrame)
{
  // Append the continuing frame to the flow
  aContFrame->AppendToFlow(this);
  aContFrame->SetStyleContext(&aPresContext, aStyleContext);
}

NS_METHOD nsTableOuterFrame::VerifyTree() const
{
#ifdef NS_DEBUG
  
#endif 
  return NS_OK;
}

/**
 * Remove and delete aChild's next-in-flow(s). Updates the sibling and flow
 * pointers.
 *
 * Updates the child count and content offsets of all containers that are
 * affected
 *
 * Overloaded here because nsContainerFrame makes assumptions about pseudo-frames
 * that are not true for tables.
 *
 * @param   aChild child this child's next-in-flow
 * @return  PR_TRUE if successful and PR_FALSE otherwise
 */
PRBool nsTableOuterFrame::DeleteChildsNextInFlow(nsIPresContext& aPresContext, nsIFrame* aChild)
{
  NS_PRECONDITION(IsChild(aChild), "bad geometric parent");

  nsIFrame* nextInFlow;
   
  aChild->GetNextInFlow(nextInFlow);

  NS_PRECONDITION(nsnull != nextInFlow, "null next-in-flow");
  nsTableOuterFrame* parent;
   
  nextInFlow->GetGeometricParent((nsIFrame*&)parent);

  // If the next-in-flow has a next-in-flow then delete it too (and
  // delete it first).
  nsIFrame* nextNextInFlow;

  nextInFlow->GetNextInFlow(nextNextInFlow);
  if (nsnull != nextNextInFlow) {
    parent->DeleteChildsNextInFlow(aPresContext, nextInFlow);
  }

#ifdef NS_DEBUG
  PRInt32   childCount;
  nsIFrame* firstChild;

  nextInFlow->FirstChild(firstChild);
  childCount = LengthOf(firstChild);

  NS_ASSERTION(childCount == 0, "deleting !empty next-in-flow");

  NS_ASSERTION((0 == childCount) && (nsnull == firstChild), "deleting !empty next-in-flow");
#endif

  // Disconnect the next-in-flow from the flow list
  nextInFlow->BreakFromPrevFlow();

  // Take the next-in-flow out of the parent's child list
  if (parent->mFirstChild == nextInFlow) {
    nextInFlow->GetNextSibling(parent->mFirstChild);

  } else {
    nsIFrame* nextSibling;

    // Because the next-in-flow is not the first child of the parent
    // we know that it shares a parent with aChild. Therefore, we need
    // to capture the next-in-flow's next sibling (in case the
    // next-in-flow is the last next-in-flow for aChild AND the
    // next-in-flow is not the last child in parent)
    NS_ASSERTION(parent->IsChild(aChild), "screwy flow");
    aChild->GetNextSibling(nextSibling);
    NS_ASSERTION(nextSibling == nextInFlow, "unexpected sibling");

    nextInFlow->GetNextSibling(nextSibling);
    aChild->SetNextSibling(nextSibling);
  }

  // Delete the next-in-flow frame and adjust it's parent's child count
  nextInFlow->DeleteFrame(aPresContext);

#ifdef NS_DEBUG
  aChild->GetNextInFlow(nextInFlow);
  NS_POSTCONDITION(nsnull == nextInFlow, "non null next-in-flow");
#endif

  return PR_TRUE;
}


/* ----- global methods ----- */

nsresult 
NS_NewTableOuterFrame( nsIContent* aContent,
                       nsIFrame*   aParentFrame,
                       nsIFrame*&  aResult)
{
  nsIFrame* it = new nsTableOuterFrame(aContent, aParentFrame);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = it;
  return NS_OK;
}

/* ----- debugging methods ----- */
NS_METHOD nsTableOuterFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
{
  // if a filter is present, only output this frame if the filter says we should
  // since this could be any "tag" with the right display type, we'll
  // just pretend it's a tbody
  if (nsnull==aFilter)
    return nsContainerFrame::List(out, aIndent, aFilter);

  nsAutoString tagString("tbody");
  PRBool outputMe = aFilter->OutputTag(&tagString);
  if (PR_TRUE==outputMe)
  {
    // Indent
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

    // Output the tag and rect
    nsIAtom* tag;
    mContent->GetTag(tag);
    if (tag != nsnull) {
      nsAutoString buf;
      tag->ToString(buf);
      fputs(buf, out);
      NS_RELEASE(tag);
    }
    PRInt32 contentIndex;

    GetContentIndex(contentIndex);
    fprintf(out, "(%d)", contentIndex);
    out << mRect;
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("\n", out);
  }
  // Output the children
  if (nsnull != mFirstChild) {
    if (PR_TRUE==outputMe)
    {
      if (0 != mState) {
        fprintf(out, " [state=%08x]\n", mState);
      }
    }
    for (nsIFrame* child = mFirstChild; child; child->GetNextSibling(child)) {
      child->List(out, aIndent + 1, aFilter);
    }
  } else {
    if (PR_TRUE==outputMe)
    {
      if (0 != mState) {
        fprintf(out, " [state=%08x]\n", mState);
      }
    }
  }
  return NS_OK;
}
