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
#include "nsTableRowGroupFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableCellFrame.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsIView.h"
#include "nsIPtr.h"
#include "nsIReflowCommand.h"

#ifdef NS_DEBUG
static PRBool gsDebug1 = PR_FALSE;
static PRBool gsDebug2 = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
#else
static const PRBool gsDebug1 = PR_FALSE;
static const PRBool gsDebug2 = PR_FALSE;
#endif

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

/* ----------- RowGroupReflowState ---------- */

struct RowGroupReflowState {
  // Our reflow state
  const nsReflowState& reflowState;

  // The body's available size (computed from the body's parent)
  nsSize availSize;

  // Margin tracking information
  nscoord prevMaxPosBottomMargin;
  nscoord prevMaxNegBottomMargin;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running y-offset
  nscoord y;

  // Flag used to set aMaxElementSize to my first row
  PRBool  firstRow;

  // Remember the height of the first row, because it's our maxElementHeight (plus header/footers)
  nscoord firstRowHeight;

  RowGroupReflowState(nsIPresContext*      aPresContext,
                      const nsReflowState& aReflowState)
    : reflowState(aReflowState)
  {
    availSize.width = reflowState.maxSize.width;
    availSize.height = reflowState.maxSize.height;
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    y=0;  // border/padding/margin???
    unconstrainedWidth = PRBool(reflowState.maxSize.width == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(reflowState.maxSize.height == NS_UNCONSTRAINEDSIZE);
    firstRow = PR_TRUE;
    firstRowHeight=0;
  }

  ~RowGroupReflowState() {
  }
};




/* ----------- nsTableRowGroupFrame ---------- */

nsTableRowGroupFrame::nsTableRowGroupFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsContainerFrame(aContent, aParentFrame)
{
  mType = aContent->GetTag();       // mType: REFCNT++
}

nsTableRowGroupFrame::~nsTableRowGroupFrame()
{
  if (nsnull!=mType)
    NS_RELEASE(mType);              // mType: REFCNT--
}

NS_METHOD nsTableRowGroupFrame::GetRowGroupType(nsIAtom *& aType)
{
  NS_ADDREF(mType);
  aType=mType;
  return NS_OK;
}


NS_METHOD nsTableRowGroupFrame::Paint(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect&        aDirtyRect)
{

  // for debug...
  /*
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(128,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
  */

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

// aDirtyRect is in our coordinate system
// child rect's are also in our coordinate system
/** overloaded method from nsContainerFrame.  The difference is that 
  * we don't want to clip our children, so a cell can do a rowspan
  */
void nsTableRowGroupFrame::PaintChildren(nsIPresContext&      aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect&        aDirtyRect)
{
  nsIFrame* kid = mFirstChild;
  while (nsnull != kid) {
    nsIView *pView;
     
    kid->GetView(pView);
    if (nsnull == pView) {
      nsRect kidRect;
      kid->GetRect(kidRect);
      nsRect damageArea(aDirtyRect);
      // Translate damage area into kid's coordinate system
      nsRect kidDamageArea(damageArea.x - kidRect.x, damageArea.y - kidRect.y,
                           damageArea.width, damageArea.height);
      aRenderingContext.PushState();
      aRenderingContext.Translate(kidRect.x, kidRect.y);
      kid->Paint(aPresContext, aRenderingContext, kidDamageArea);
      if (nsIFrame::GetShowFrameBorders()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
      }
      aRenderingContext.PopState();
    }
    else
      NS_RELEASE(pView);
    kid->GetNextSibling(kid);
  }
}

// Collapse child's top margin with previous bottom margin
nscoord nsTableRowGroupFrame::GetTopMarginFor(nsIPresContext*      aCX,
                                              RowGroupReflowState& aState,
                                              const nsMargin& aKidMargin)
{
  nscoord margin;
  nscoord maxNegTopMargin = 0;
  nscoord maxPosTopMargin = 0;
  if ((margin = aKidMargin.top) < 0) {
    maxNegTopMargin = -margin;
  } else {
    maxPosTopMargin = margin;
  }

  nscoord maxPos = PR_MAX(aState.prevMaxPosBottomMargin, maxPosTopMargin);
  nscoord maxNeg = PR_MAX(aState.prevMaxNegBottomMargin, maxNegTopMargin);
  margin = maxPos - maxNeg;

  return margin;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame, and includes
// any left/top margin.
void nsTableRowGroupFrame::PlaceChild( nsIPresContext*    aPresContext,
																			 RowGroupReflowState& aState,
																			 nsIFrame*          aKidFrame,
																			 const nsRect&      aKidRect,
																			 nsSize*            aMaxElementSize,
																			 nsSize&            aKidMaxElementSize)
{
  if (PR_TRUE==gsDebug1)
    printf ("rowgroup: placing row at %d, %d, %d, %d\n",
           aKidRect.x, aKidRect.y, aKidRect.width, aKidRect.height);

  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // Adjust the running y-offset
  aState.y += aKidRect.height;

  // If our height is constrained then update the available height
  if (PR_FALSE == aState.unconstrainedHeight) {
    aState.availSize.height -= aKidRect.height;
  }

  // Update the maximum element size
  if (PR_TRUE==aState.firstRow)
  {
    aState.firstRow = PR_FALSE;
    aState.firstRowHeight = aKidRect.height;
    if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = aKidMaxElementSize.width;
      aMaxElementSize->height = aKidMaxElementSize.height;
    }
  }
}

/**
 * Reflow the frames we've already created
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully reflowed all the mapped children and false
 *            otherwise, e.g. we pushed children to the next in flow
 */
PRBool nsTableRowGroupFrame::ReflowMappedChildren( nsIPresContext*      aPresContext,
                                                   RowGroupReflowState& aState,
                                                   nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": reflow mapped (childCount=%d) [%d,%d,%c]\n",
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableRowGroupFrame* flow = (nsTableRowGroupFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableRowGroupFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  NS_PRECONDITION(nsnull != mFirstChild, "no children");

  PRInt32   childCount = 0;
  nsIFrame* prevKidFrame = nsnull;

  // Remember our original mLastContentIsComplete so that if we end up
  // having to push children, we have the correct value to hand to
  // PushChildren.
  PRBool    lastContentIsComplete = mLastContentIsComplete;

  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRBool    result = PR_TRUE;

  for (nsIFrame*  kidFrame = mFirstChild; nsnull != kidFrame; ) {
    nsSize                  kidAvailSize(aState.availSize);
    if (0>=kidAvailSize.height)
      kidAvailSize.height = 1;      // XXX: HaCk - we don't handle negative heights yet
    nsReflowMetrics desiredSize(pKidMaxElementSize);
    desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;
    nsReflowStatus  status;

    // Get top margin for this kid
    nsIContentPtr kid;
     
    kidFrame->GetContent(kid.AssignRef());
    nsIStyleContextPtr kidSC;
     
    kidFrame->GetStyleContext(aPresContext, kidSC.AssignRef());
    const nsStyleSpacing* kidSpacing = (const nsStyleSpacing*)
      kidSC->GetStyleData(eStyleStruct_Spacing);
    nsMargin kidMargin;
    kidSpacing->CalcMarginFor(this, kidMargin);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidMargin);
    nscoord bottomMargin = kidMargin.bottom;

    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    if (PR_FALSE == aState.unconstrainedHeight) {
      kidAvailSize.height -= topMargin;
    }
    // Subtract off for left and right margin
    if (PR_FALSE == aState.unconstrainedWidth) {
      kidAvailSize.width -= kidMargin.left + kidMargin.right;
    }

    // Reflow the child into the available space
    nsReflowState kidReflowState(kidFrame, aState.reflowState, kidAvailSize,
                                 eReflowReason_Resize);
    kidFrame->WillReflow(*aPresContext);
    status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);

    // Did the child fit?
    if ((kidFrame != mFirstChild) &&
        ((kidAvailSize.height <= 0) ||
         (desiredSize.height > kidAvailSize.height)))
    {
      // The child's height is too big to fit at all in our remaining space,
      // and it's not our first child.
      //
      // Note that if the width is too big that's okay and we allow the
      // child to extend horizontally outside of the reflow area

      // Since we are giving the next-in-flow our last child, we
      // give it our original mLastContentIsComplete, too (in case we
      // are pushing into an empty next-in-flow)
      PushChildren(kidFrame, prevKidFrame, lastContentIsComplete);
			SetLastContentOffset(prevKidFrame);

      // Our mLastContentIsComplete was already set by the last kid we
      // reflowed reflow's status
      result = PR_FALSE;
      break;
    }

    // Place the child after taking into account it's margin
    aState.y += topMargin;
    nsRect kidRect (0, 0, desiredSize.width, desiredSize.height);
    kidRect.x += kidMargin.left;
    kidRect.y += aState.y;
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize,
               kidMaxElementSize);
    if (bottomMargin < 0) {
      aState.prevMaxNegBottomMargin = -bottomMargin;
    } else {
      aState.prevMaxPosBottomMargin = bottomMargin;
    }
    childCount++;

		// Remember where we just were in case we end up pushing children
		prevKidFrame = kidFrame;

    // Update mLastContentIsComplete now that this kid fits
    mLastContentIsComplete = NS_FRAME_IS_COMPLETE(status);

    /* Row groups should not create continuing frames for rows 
     * unless they absolutely have to!
     * check to see if this is absolutely necessary (with new params from troy)
     * otherwise PushChildren and bail.
     */
    // Special handling for incomplete children
    if (NS_FRAME_IS_NOT_COMPLETE(status)) {
      // XXX It's good to assume that we might still have room
      // even if the child didn't complete (floaters will want this)
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
			PRBool lastContentIsComplete = mLastContentIsComplete;
      if (nsnull == kidNextInFlow) {
        // No the child isn't complete, and it doesn't have a next in flow so
        // create a continuing frame. This hooks the child into the flow.
        nsIFrame* continuingFrame;
        nsIStyleContext* kidSC;
        kidFrame->GetStyleContext(aPresContext, kidSC);
        kidFrame->CreateContinuingFrame(aPresContext, this, kidSC,
                                        continuingFrame);
        NS_RELEASE(kidSC);

        // Insert the frame. We'll reflow it next pass through the loop
        nsIFrame* nextSib;
         
        kidFrame->GetNextSibling(nextSib);
        continuingFrame->SetNextSibling(nextSib);
        kidFrame->SetNextSibling(continuingFrame);
        if (nsnull == nextSib) {
          // Assume that the continuation frame we just created is
          // complete, for now. It will get reflowed by our
          // next-in-flow (we are going to push it now)
          lastContentIsComplete = PR_TRUE;
        }
      }
      // We've used up all of our available space so push the remaining
      // children to the next-in-flow
      nsIFrame* nextSibling;
       
      kidFrame->GetNextSibling(nextSibling);
      if (nsnull != nextSibling) {
        PushChildren(nextSibling, kidFrame, lastContentIsComplete);
        SetLastContentOffset(prevKidFrame);
      }
      result = PR_FALSE;
      break;
    }

    // Add back in the left and right margins, because one row does not 
    // impact another row's width
    if (PR_FALSE == aState.unconstrainedWidth) {
      kidAvailSize.width += kidMargin.left + kidMargin.right;
    }

    // Get the next child
    kidFrame->GetNextSibling(kidFrame);

  }

  // Update the child count
  mChildCount = childCount;

#ifdef NS_DEBUG
  NS_POSTCONDITION(LengthOf(mFirstChild) == mChildCount, "bad child count");

  nsIFrame* lastChild;
  PRInt32   lastIndexInParent;

  LastChild(lastChild);
  lastChild->GetContentIndex(lastIndexInParent);
	NS_POSTCONDITION(lastIndexInParent == mLastContentOffset, "bad last content offset");
#endif

#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": reflow mapped %sok (childCount=%d) [%d,%d,%c]\n",
         (result ? "" : "NOT "),
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableRowGroupFrame* flow = (nsTableRowGroupFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableRowGroupFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  return result;
}

/**
 * Try and pull-up frames from our next-in-flow
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully pulled-up all the children and false
 *            otherwise, e.g. child didn't fit
 */
PRBool nsTableRowGroupFrame::PullUpChildren(nsIPresContext*      aPresContext,
                                            RowGroupReflowState& aState,
                                            nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": pullup (childCount=%d) [%d,%d,%c]\n",
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableRowGroupFrame* flow = (nsTableRowGroupFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableRowGroupFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  nsTableRowGroupFrame* nextInFlow = (nsTableRowGroupFrame*)mNextInFlow;
  nsSize         kidMaxElementSize;
  nsSize*        pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
#ifdef NS_DEBUG
  PRInt32        kidIndex = NextChildOffset();
#endif
  nsIFrame*      prevKidFrame;
   
  LastChild(prevKidFrame);

  // This will hold the prevKidFrame's mLastContentIsComplete
  // status. If we have to push the frame that follows prevKidFrame
  // then this will become our mLastContentIsComplete state. Since
  // prevKidFrame is initially our last frame, it's completion status
  // is our mLastContentIsComplete value.
  PRBool        prevLastContentIsComplete = mLastContentIsComplete;

  PRBool        result = PR_TRUE;

  while (nsnull != nextInFlow) {
    nsReflowMetrics kidSize(pKidMaxElementSize);
    kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;
    nsReflowStatus    status;

    // Get the next child
    nsIFrame* kidFrame = nextInFlow->mFirstChild;

    // Any more child frames?
    if (nsnull == kidFrame) {
      // No. Any frames on its overflow list?
      if (nsnull != nextInFlow->mOverflowList) {
        // Move the overflow list to become the child list
//        NS_ABORT();
        nextInFlow->AppendChildren(nextInFlow->mOverflowList);
        nextInFlow->mOverflowList = nsnull;
        kidFrame = nextInFlow->mFirstChild;
      } else {
        // We've pulled up all the children, so move to the next-in-flow.
        nextInFlow->GetNextInFlow((nsIFrame*&)nextInFlow);
        continue;
      }
    }

    // See if the child fits in the available space. If it fits or
    // it's splittable then reflow it. The reason we can't just move
    // it is that we still need ascent/descent information
    nsSize            kidFrameSize;
    nsSplittableType  kidIsSplittable;

    kidFrame->GetSize(kidFrameSize);
    kidFrame->IsSplittable(kidIsSplittable);
    if ((kidFrameSize.height > aState.availSize.height) &&
        NS_FRAME_IS_NOT_SPLITTABLE(kidIsSplittable)) {
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }
    nsReflowState kidReflowState(kidFrame, aState.reflowState, aState.availSize,
                                 eReflowReason_Resize);
    kidFrame->WillReflow(*aPresContext);
    status = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState);

    // Did the child fit?
    if ((kidSize.height > aState.availSize.height) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }

    // Place the child
    //aState.y += topMargin;
    nsRect kidRect (0, 0, kidSize.width, kidSize.height);
    //kidRect.x += kidMol->margin.left;
    kidRect.y += aState.y;
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize, *pKidMaxElementSize);

    // Remove the frame from its current parent
    kidFrame->GetNextSibling(nextInFlow->mFirstChild);
    nextInFlow->mChildCount--;
    // Update the next-in-flows first content offset
    if (nsnull != nextInFlow->mFirstChild) {
      nextInFlow->SetFirstContentOffset(nextInFlow->mFirstChild);
    }

    // Link the frame into our list of children
    kidFrame->SetGeometricParent(this);
    nsIFrame* kidContentParent;

    kidFrame->GetContentParent(kidContentParent);
    if (nextInFlow == kidContentParent) {
      kidFrame->SetContentParent(this);
    }
    if (nsnull == prevKidFrame) {
      mFirstChild = kidFrame;
      SetFirstContentOffset(kidFrame);
    } else {
      prevKidFrame->SetNextSibling(kidFrame);
    }
    kidFrame->SetNextSibling(nsnull);
    mChildCount++;

    // Remember where we just were in case we end up pushing children
    prevKidFrame = kidFrame;
    prevLastContentIsComplete = mLastContentIsComplete;

    // Is the child we just pulled up complete?
    mLastContentIsComplete = NS_FRAME_IS_COMPLETE(status);
    if (NS_FRAME_IS_NOT_COMPLETE(status)) {
      // No the child isn't complete
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a
        // continuing frame. The creation appends it to the flow and
        // prepares it for reflow.
        nsIFrame* continuingFrame;
        nsIStyleContext* kidSC;
        kidFrame->GetStyleContext(aPresContext, kidSC);
        kidFrame->CreateContinuingFrame(aPresContext, this, kidSC,
                                        continuingFrame);
        NS_RELEASE(kidSC);
        NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

        // Add the continuing frame to our sibling list and then push
        // it to the next-in-flow. This ensures the next-in-flow's
        // content offsets and child count are set properly. Note that
        // we can safely assume that the continuation is complete so
        // we pass PR_TRUE into PushChidren in case our next-in-flow
        // was just drained and now needs to know it's
        // mLastContentIsComplete state.
        kidFrame->SetNextSibling(continuingFrame);

        PushChildren(continuingFrame, kidFrame, PR_TRUE);

        // After we push the continuation frame we don't need to fuss
        // with mLastContentIsComplete beause the continuation frame
        // is no longer on *our* list.
      }

      // If the child isn't complete then it means that we've used up
      // all of our available space.
      result = PR_FALSE;
      break;
    }
  }

  // Update our last content offset
  if (nsnull != prevKidFrame) {
    NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
    SetLastContentOffset(prevKidFrame);
  }

  // We need to make sure the first content offset is correct for any empty
  // next-in-flow frames (frames where we pulled up all the child frames)
  nextInFlow = (nsTableRowGroupFrame*)mNextInFlow;
  if ((nsnull != nextInFlow) && (nsnull == nextInFlow->mFirstChild)) {
    // We have at least one empty frame. Did we succesfully pull up all the
    // child frames?
    if (PR_FALSE == result) {
      // No, so we need to adjust the first content offset of all the empty
      // frames
      AdjustOffsetOfEmptyNextInFlows();
#ifdef NS_DEBUG
    } else {
      // Yes, we successfully pulled up all the child frames which means all
      // the next-in-flows must be empty. Do a sanity check
      while (nsnull != nextInFlow) {
        NS_ASSERTION(nsnull == nextInFlow->mFirstChild, "non-empty next-in-flow");
        nextInFlow->GetNextInFlow((nsIFrame*&)nextInFlow);
      }
#endif
    }
  }

#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif

#ifdef NOISY
  ListTag(stdout);
  printf(": pullup %sok (childCount=%d) [%d,%d,%c]\n",
         (result ? "" : "NOT "),
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableRowGroupFrame* flow = (nsTableRowGroupFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableRowGroupFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  return result;
}

/**
 * Create new frames for content we haven't yet mapped
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  frComplete if all content has been mapped and frNotComplete
 *            if we should be continued
 */
nsReflowStatus
nsTableRowGroupFrame::ReflowUnmappedChildren(nsIPresContext*      aPresContext,
                                             RowGroupReflowState& aState,
                                             nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  nsIFrame*       kidPrevInFlow = nsnull;
  nsReflowStatus  result = NS_FRAME_NOT_COMPLETE;

  // If we have no children and we have a prev-in-flow then we need to pick
  // up where it left off. If we have children, e.g. we're being resized, then
  // our content offset should already be set correctly...
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    nsTableRowGroupFrame* prev = (nsTableRowGroupFrame*)mPrevInFlow;
    NS_ASSERTION(prev->mLastContentOffset >= prev->mFirstContentOffset, "bad prevInFlow");

    mFirstContentOffset = prev->NextChildOffset();
    if (!prev->mLastContentIsComplete) {
      // Our prev-in-flow's last child is not complete
      prev->LastChild(kidPrevInFlow);
    }
  }

	mLastContentIsComplete = PR_TRUE;

  // Place our children, one at a time, until we are out of children
  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRInt32   kidIndex = NextChildOffset();
  nsIFrame* prevKidFrame;
   
  LastChild(prevKidFrame);  // XXX remember this...

  for (;;) {
    // Get the next content object
    nsIContentPtr kid = mContent->ChildAt(kidIndex);
    if (kid.IsNull()) {
      result = NS_FRAME_COMPLETE;
      break;
    }

    // Make sure we still have room left
    if (aState.availSize.height <= 0) {
      // Note: return status was set to frNotComplete above...
      break;
    }

    // Resolve style
    nsIStyleContextPtr kidSC =
      aPresContext->ResolveStyleContextFor(kid, this, PR_TRUE);
    const nsStyleSpacing* kidSpacing = (const nsStyleSpacing*)
      kidSC->GetStyleData(eStyleStruct_Spacing);
    nsMargin kidMargin;
    kidSpacing->CalcMarginFor(this, kidMargin);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidMargin);
    nscoord bottomMargin = kidMargin.bottom;

    nsIFrame* kidFrame;

    // Create a child frame
    if (nsnull == kidPrevInFlow) {
      nsIContentDelegate* kidDel = nsnull;
      kidDel = kid->GetDelegate(aPresContext);
      nsresult rv = kidDel->CreateFrame(aPresContext, kid, this, kidSC,
                                        kidFrame);
      NS_RELEASE(kidDel);
    } else {
      kidPrevInFlow->CreateContinuingFrame(aPresContext, this, kidSC,
                                           kidFrame);
    }

    // Try to reflow the child into the available space. It might not
    // fit or might need continuing.
    nsReflowMetrics kidSize(pKidMaxElementSize);
    kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;
    nsReflowState   kidReflowState(kidFrame, aState.reflowState, aState.availSize,
                                   eReflowReason_Initial);
    kidFrame->WillReflow(*aPresContext);
    nsReflowStatus status = ReflowChild(kidFrame,aPresContext, kidSize,
                                        kidReflowState);

    // Did the child fit?
    if ((kidSize.height > aState.availSize.height) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child. Add the frame to our overflow list
      NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");
      mOverflowList = kidFrame;
      prevKidFrame->SetNextSibling(nsnull);
      break;
    }

    // Place the child
    //aState.y += topMargin;
    nsRect kidRect (0, 0, kidSize.width, kidSize.height);
    //kidRect.x += kidMol->margin.left;
    kidRect.y += aState.y;
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize, *pKidMaxElementSize);

    // Link child frame into the list of children
    if (nsnull != prevKidFrame) {
      prevKidFrame->SetNextSibling(kidFrame);
    } else {
      mFirstChild = kidFrame;  // our first child
      SetFirstContentOffset(kidFrame);
    }
    prevKidFrame = kidFrame;
    mChildCount++;
    kidIndex++;

    // Did the child complete?
    if (NS_FRAME_IS_NOT_COMPLETE(status)) {
      // If the child isn't complete then it means that we've used up
      // all of our available space
      mLastContentIsComplete = PR_FALSE;
      break;
    }
    kidPrevInFlow = nsnull;
  }

  // Update the content mapping
  NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
  SetLastContentOffset(prevKidFrame);
#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
#endif
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  return result;
}


/** Layout the entire row group.
  * This method stacks rows vertically according to HTML 4.0 rules.
  * Rows are responsible for layout of their children.
  */
NS_METHOD
nsTableRowGroupFrame::Reflow(nsIPresContext*      aPresContext,
                             nsReflowMetrics&     aDesiredSize,
                             const nsReflowState& aReflowState,
                             nsReflowStatus&      aStatus)
{
  if (gsDebug1==PR_TRUE)
    printf("nsTableRowGroupFrame::Reflow - aMaxSize = %d, %d\n",
            aReflowState.maxSize.width, aReflowState.maxSize.height);
#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  if (eReflowReason_Incremental == aReflowState.reason) {
    // XXX Recover state
    // XXX Deal with the case where the reflow command is targeted at us
    nsIFrame* kidFrame;
    aReflowState.reflowCommand->GetNext(kidFrame);

    // Pass along the reflow command
    nsReflowMetrics desiredSize(nsnull);
    // XXX Correctly compute the available space...
    nsReflowState kidReflowState(kidFrame, aReflowState, aReflowState.maxSize);
    kidFrame->WillReflow(*aPresContext);
    aStatus = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);

    // XXX Compute desired size...

  } else {
    // Initialize out parameter
    if (nsnull != aDesiredSize.maxElementSize) {
      aDesiredSize.maxElementSize->width = 0;
      aDesiredSize.maxElementSize->height = 0;
    }
  
    PRBool reflowMappedOK = PR_TRUE;
  
    aStatus = NS_FRAME_COMPLETE;
  
    // Check for an overflow list
    MoveOverflowToChildList();
  
    RowGroupReflowState state(aPresContext, aReflowState);
  
    // Reflow the existing frames
    if (nsnull != mFirstChild) {
      reflowMappedOK = ReflowMappedChildren(aPresContext, state, aDesiredSize.maxElementSize);
      if (PR_FALSE == reflowMappedOK) {
        aStatus = NS_FRAME_NOT_COMPLETE;
      }
    }
  
    // Did we successfully reflow our mapped children?
    if (PR_TRUE == reflowMappedOK) {
      // Any space left?
      if (state.availSize.height <= 0) {
        // No space left. Don't try to pull-up children or reflow unmapped
        if (NextChildOffset() < mContent->ChildCount()) {
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
      } else if (NextChildOffset() < mContent->ChildCount()) {
        // Try and pull-up some children from a next-in-flow
        if (PullUpChildren(aPresContext, state, aDesiredSize.maxElementSize)) {
          // If we still have unmapped children then create some new frames
          if (NextChildOffset() < mContent->ChildCount()) {
            aStatus = ReflowUnmappedChildren(aPresContext, state, aDesiredSize.maxElementSize);
          }
        } else {
          // We were unable to pull-up all the existing frames from the
          // next in flow
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
      }
    }
  
    if (NS_FRAME_IS_COMPLETE(aStatus)) {
      // Don't forget to add in the bottom margin from our last child.
      // Only add it in if there's room for it.
      nscoord margin = state.prevMaxPosBottomMargin -
        state.prevMaxNegBottomMargin;
      if (state.availSize.height >= margin) {
        state.y += margin;
      }
    }
  
    // Return our desired rect
    //NS_ASSERTION(0<state.firstRowHeight, "illegal firstRowHeight after reflow");
    //NS_ASSERTION(0<state.y, "illegal height after reflow");
    aDesiredSize.width = aReflowState.maxSize.width;
    aDesiredSize.height = state.y;   
  }

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif

  if (gsDebug1==PR_TRUE) 
  {
    if (nsnull!=aDesiredSize.maxElementSize)
      printf("nsTableRowGroupFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=%d,%d\n",
              NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
              aDesiredSize.width, aDesiredSize.height,
              aDesiredSize.maxElementSize->width, aDesiredSize.maxElementSize->height);
    else
      printf("nsTableRowGroupFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=NSNULL\n", 
             NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
             aDesiredSize.width, aDesiredSize.height);
  }

  return NS_OK;

}

NS_METHOD
nsTableRowGroupFrame::CreateContinuingFrame(nsIPresContext*  aPresContext,
                                            nsIFrame*        aParent,
                                            nsIStyleContext* aStyleContext,
                                            nsIFrame*&       aContinuingFrame)
{
  nsTableRowGroupFrame* cf = new nsTableRowGroupFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  if (PR_TRUE==gsDebug1) printf("nsTableRowGroupFrame::CCF parent = %p, this=%p, cf=%p\n", aParent, this, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

/* ----- static methods ----- */

nsresult nsTableRowGroupFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                        nsIContent* aContent,
                                        nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableRowGroupFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}




// For Debugging ONLY
NS_METHOD nsTableRowGroupFrame::MoveTo(nscoord aX, nscoord aY)
{
  if ((aX != mRect.x) || (aY != mRect.y)) {
    mRect.x = aX;
    mRect.y = aY;

    nsIView* view;
    GetView(view);

    // Let the view know
    if (nsnull != view) {
      // Position view relative to it's parent, not relative to our
      // parent frame (our parent frame may not have a view).
      nsIView* parentWithView;
      nsPoint origin;
      GetOffsetFromView(origin, parentWithView);
      view->SetPosition(origin.x, origin.y);
      NS_IF_RELEASE(parentWithView);
    }
  }

  return NS_OK;
}

NS_METHOD nsTableRowGroupFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  mRect.width = aWidth;
  mRect.height = aHeight;

  nsIView* view;
  GetView(view);

  // Let the view know
  if (nsnull != view) {
    view->SetDimensions(aWidth, aHeight);
  }
  return NS_OK;
}

