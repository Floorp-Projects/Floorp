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


#ifdef NS_DEBUG
static PRBool gsDebug1 = PR_FALSE;
static PRBool gsDebug2 = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
#else
static const PRBool gsDebug1 = PR_FALSE;
static const PRBool gsDebug2 = PR_FALSE;
#endif

static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);

/* ----------- RowGroupReflowState ---------- */

struct RowGroupReflowState {

  // The body's style molecule
  nsStyleMolecule* mol;

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

  RowGroupReflowState(nsIPresContext*  aPresContext,
                      const nsSize&    aMaxSize,
                      nsStyleMolecule* aMol)
  {
    mol = aMol;
    availSize.width = aMaxSize.width;
    availSize.height = aMaxSize.height;
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    y=0;  // border/padding/margin???
    unconstrainedWidth = PRBool(aMaxSize.width == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(aMaxSize.height == NS_UNCONSTRAINEDSIZE);
    firstRow = PR_TRUE;
    firstRowHeight=0;
  }

  ~RowGroupReflowState() {
  }
};




/* ----------- nsTableRowGroupFrame ---------- */

nsTableRowGroupFrame::nsTableRowGroupFrame(nsIContent* aContent,
                     PRInt32     aIndexInParent,
                     nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aIndexInParent, aParentFrame)
{
}

nsTableRowGroupFrame::~nsTableRowGroupFrame()
{
}


void nsTableRowGroupFrame::Paint(nsIPresContext& aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect&        aDirtyRect)
{

  // for debug...
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(128,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
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
    kid = kid->GetNextSibling();
  }
}

// Collapse child's top margin with previous bottom margin
nscoord nsTableRowGroupFrame::GetTopMarginFor(nsIPresContext*      aCX,
                                              RowGroupReflowState& aState,
                                              nsStyleMolecule*     aKidMol)
{
  nscoord margin;
  nscoord maxNegTopMargin = 0;
  nscoord maxPosTopMargin = 0;
  if ((margin = aKidMol->margin.top) < 0) {
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
void nsTableRowGroupFrame::PlaceChild(nsIPresContext*    aPresContext,
                             RowGroupReflowState& aState,
                             nsIFrame*          aKidFrame,
                             const nsRect&      aKidRect,
                             nsStyleMolecule*   aKidMol,
                             nsSize*            aMaxElementSize,
                             nsSize&            aKidMaxElementSize)
{
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
    nsReflowMetrics         desiredSize;
    nsIFrame::ReflowStatus  status;

    // Get top margin for this kid
    nsIContent* kid = kidFrame->GetContent();
    nsIStyleContext* kidSC = kidFrame->GetStyleContext(aPresContext);
    nsStyleMolecule* kidMol = (nsStyleMolecule*)kidSC->GetData(kStyleMoleculeSID);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidMol);
    nscoord bottomMargin = kidMol->margin.bottom;
    NS_RELEASE(kid);
    NS_RELEASE(kidSC);

    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    if (PR_FALSE == aState.unconstrainedHeight) {
      kidAvailSize.height -= topMargin;
    }
    // Subtract off for left and right margin
    if (PR_FALSE == aState.unconstrainedWidth) {
      kidAvailSize.width -= kidMol->margin.left + kidMol->margin.right;
    }

    // Only skip the reflow if this is not our first child and we are
    // out of space.
    if ((kidFrame == mFirstChild) || (kidAvailSize.height > 0)) {
      // Reflow the child into the available space
      status = ReflowChild(kidFrame, aPresContext, desiredSize,
                           kidAvailSize, pKidMaxElementSize);
    }

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

      // Our mLastContentIsComplete was already set by the last kid we
      // reflowed reflow's status
      result = PR_FALSE;
      break;
    }

    // Place the child after taking into account it's margin
    aState.y += topMargin;
    nsRect kidRect (0, 0, desiredSize.width, desiredSize.height);
    kidRect.x += kidMol->margin.left;
    kidRect.y += aState.y;
    PlaceChild(aPresContext, aState, kidFrame, kidRect, kidMol, aMaxElementSize,
               kidMaxElementSize);
    if (bottomMargin < 0) {
      aState.prevMaxNegBottomMargin = -bottomMargin;
    } else {
      aState.prevMaxPosBottomMargin = bottomMargin;
    }
    childCount++;

    // Update mLastContentIsComplete now that this kid fits
    mLastContentIsComplete = PRBool(status == frComplete);

    /* Row groups should not create continuing frames for rows 
     * unless they absolutely have to!
     * check to see if this is absolutely necessary (with new params from troy)
     * otherwise PushChildren and bail.
     */
    // Special handling for incomplete children
    if (frNotComplete == status) {
      // XXX It's good to assume that we might still have room
      // even if the child didn't complete (floaters will want this)
      nsIFrame* kidNextInFlow = kidFrame->GetNextInFlow();
      if (nsnull == kidNextInFlow) {
        // No the child isn't complete, and it doesn't have a next in flow so
        // create a continuing frame. This hooks the child into the flow.
        nsIFrame* continuingFrame =
          kidFrame->CreateContinuingFrame(aPresContext, this);

        // Insert the frame. We'll reflow it next pass through the loop
        nsIFrame* nextSib = kidFrame->GetNextSibling();
        continuingFrame->SetNextSibling(nextSib);
        kidFrame->SetNextSibling(continuingFrame);
        if (nsnull == nextSib) {
          // Assume that the continuation frame we just created is
          // complete, for now. It will get reflowed by our
          // next-in-flow (we are going to push it now)
          lastContentIsComplete = PR_TRUE;
        }
      }
    }

    // Get the next child
    prevKidFrame = kidFrame;
    kidFrame = kidFrame->GetNextSibling();

    // XXX talk with troy about checking for available space here
  }

  // Update the child count
  mChildCount = childCount;
  NS_POSTCONDITION(LengthOf(mFirstChild) == mChildCount, "bad child count");

  // Set the last content offset based on the last child we mapped.
  NS_ASSERTION(LastChild() == prevKidFrame, "unexpected last child");
  SetLastContentOffset(prevKidFrame);

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
  NS_PRECONDITION(nsnull != mNextInFlow, "null next-in-flow");
  nsTableRowGroupFrame*  nextInFlow = (nsTableRowGroupFrame*)mNextInFlow;
  nsSize        kidMaxElementSize;
  nsSize*       pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;

  // The frame previous to the current frame we are reflowing. This
  // starts out initially as our last frame.
  nsIFrame*     prevKidFrame = LastChild();

  // This will hold the prevKidFrame's mLastContentIsComplete
  // status. If we have to push the frame that follows prevKidFrame
  // then this will become our mLastContentIsComplete state. Since
  // prevKidFrame is initially our last frame, it's completion status
  // is our mLastContentIsComplete value.
  PRBool        prevLastContentIsComplete = mLastContentIsComplete;
  PRBool        result = PR_TRUE;

  while (nsnull != nextInFlow) {
    nsReflowMetrics desiredSize;
    nsSize  kidAvailSize(aState.availSize);

    // Get first available frame from the next-in-flow
    nsIFrame* kidFrame = PullUpOneChild(nextInFlow, prevKidFrame);
    if (nsnull == kidFrame) {
      // We've pulled up all the children from that next-in-flow, so
      // move to the next next-in-flow.
      nextInFlow = (nsTableRowGroupFrame*) nextInFlow->mNextInFlow;
      continue;
    }

    // Get top margin for this kid
    nsIContent* kid = kidFrame->GetContent();
    nsIStyleContext* kidSC = kidFrame->GetStyleContext(aPresContext);
    nsStyleMolecule* kidMol = (nsStyleMolecule*)kidSC->GetData(kStyleMoleculeSID);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidMol);
    nscoord bottomMargin = kidMol->margin.bottom;

    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    if (PR_FALSE == aState.unconstrainedHeight) {
      kidAvailSize.height -= topMargin;
    }
    // Subtract off for left and right margin
    if (PR_FALSE == aState.unconstrainedWidth) {
      kidAvailSize.width -= kidMol->margin.left + kidMol->margin.right;
    }

    ReflowStatus status;
    do {
      // Only skip the reflow if this is not our first child and we are
      // out of space.
      if ((kidFrame == mFirstChild) || (kidAvailSize.height > 0)) {
        status = ReflowChild(kidFrame, aPresContext, desiredSize,
                             kidAvailSize, pKidMaxElementSize);
      }

      // Did the child fit?
      if ((kidFrame != mFirstChild) &&
          ((kidAvailSize.height <= 0) ||
           (desiredSize.height > kidAvailSize.height)))
      {
        // The child's height is too big to fit at all in our remaining space,
        // and it wouldn't have been our first child.
        //
        // Note that if the width is too big that's okay and we allow the
        // child to extend horizontally outside of the reflow area
        PRBool lastComplete = PRBool(nsnull == kidFrame->GetNextInFlow());
        PushChildren(kidFrame, prevKidFrame, lastComplete);
        mLastContentIsComplete = prevLastContentIsComplete;
        mChildCount--;
        result = PR_FALSE;
        NS_RELEASE(kid);
        NS_RELEASE(kidSC);
        goto push_done;
      }

      // Place the child
      aState.y += topMargin;
      nsRect kidRect (0, 0, desiredSize.width, desiredSize.height);
      kidRect.x += kidMol->margin.left;
      kidRect.y += aState.y;
      PlaceChild(aPresContext, aState, kidFrame, kidRect, kidMol, aMaxElementSize,
                 kidMaxElementSize);
      if (bottomMargin < 0) {
        aState.prevMaxNegBottomMargin = -bottomMargin;
      } else {
        aState.prevMaxPosBottomMargin = bottomMargin;
      }
      mLastContentIsComplete = PRBool(status == frComplete);

#ifdef NOISY
      ListTag(stdout);
      printf(": pulled up ");
      ((nsFrame*)kidFrame)->ListTag(stdout);
      printf("\n");
#endif

      /* Row groups should not create continuing frames for rows 
       * unless they absolutely have to!
       * check to see if this is absolutely necessary (with new params from troy)
       * otherwise PushChildren and bail, as above.
       */
      // Is the child we just pulled up complete?
      if (frNotComplete == status) {
        // No the child isn't complete.
        nsIFrame* kidNextInFlow = kidFrame->GetNextInFlow();
        if (nsnull == kidNextInFlow) {
          // The child doesn't have a next-in-flow so create a
          // continuing frame. The creation appends it to the flow and
          // prepares it for reflow.
          nsIFrame* continuingFrame =
            kidFrame->CreateContinuingFrame(aPresContext, this);

          // Add the continuing frame to our sibling list.
          continuingFrame->SetNextSibling(kidFrame->GetNextSibling());
          kidFrame->SetNextSibling(continuingFrame);
          prevKidFrame = kidFrame;
          prevLastContentIsComplete = mLastContentIsComplete;
          kidFrame = continuingFrame;
          mChildCount++;
        } else {
          // The child has a next-in-flow, but it's not one of ours.
          // It *must* be in one of our next-in-flows. Collect it
          // then.
          NS_ASSERTION(kidNextInFlow->GetGeometricParent() != this,
                       "busted kid next-in-flow");
          break;
        }
      }
    } while (frNotComplete == status);
    NS_RELEASE(kid);
    NS_RELEASE(kidSC);

    prevKidFrame = kidFrame;
    prevLastContentIsComplete = mLastContentIsComplete;
  }

 push_done:;
  // Update our last content index
  if (nsnull != prevKidFrame) {
    NS_ASSERTION(LastChild() == prevKidFrame, "bad last child");
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
        nextInFlow = (nsTableRowGroupFrame*)nextInFlow->GetNextInFlow();
      }
#endif
    }
  }

#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
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
nsIFrame::ReflowStatus
nsTableRowGroupFrame::ReflowUnmappedChildren(nsIPresContext*      aPresContext,
                                             RowGroupReflowState& aState,
                                             nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  nsIFrame*     kidPrevInFlow = nsnull;
  ReflowStatus  result = frNotComplete;

  // If we have no children and we have a prev-in-flow then we need to pick
  // up where it left off. If we have children, e.g. we're being resized, then
  // our content offset should already be set correctly...
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    nsTableRowGroupFrame* prev = (nsTableRowGroupFrame*)mPrevInFlow;
    NS_ASSERTION(prev->mLastContentOffset >= prev->mFirstContentOffset, "bad prevInFlow");

    mFirstContentOffset = prev->NextChildOffset();
    if (!prev->mLastContentIsComplete) {
      // Our prev-in-flow's last child is not complete
      kidPrevInFlow = prev->LastChild();
    }
  }

  PRBool originalLastContentIsComplete = mLastContentIsComplete;

  // Place our children, one at a time, until we are out of children
  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRInt32   kidIndex = NextChildOffset();
  nsIFrame* prevKidFrame = LastChild();  // XXX remember this...

  for (;;) {
    // Get the next content object
    nsIContent* kid = mContent->ChildAt(kidIndex);
    if (nsnull == kid) {
      result = frComplete;
      break;
    }

    // Make sure we still have room left
    if (aState.availSize.height <= 0) {
      // Note: return status was set to frNotComplete above...
      NS_RELEASE(kid);
      break;
    }

    // Resolve style
    nsIStyleContext* kidStyleContext =
      aPresContext->ResolveStyleContextFor(kid, this);
    nsStyleMolecule* kidMol =
      (nsStyleMolecule*)kidStyleContext->GetData(kStyleMoleculeSID);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidMol);
    nscoord bottomMargin = kidMol->margin.bottom;

    //nsBlockFrame*   pseudoFrame = nsnull;
    nsIFrame*       kidFrame;
    ReflowStatus    status;

    // Create a child frame
    if (nsnull == kidPrevInFlow) {
      nsIContentDelegate* kidDel = nsnull;
      kidDel = kid->GetDelegate(aPresContext);
      kidFrame = kidDel->CreateFrame(aPresContext, kid, kidIndex, this);
      NS_RELEASE(kidDel);
      kidFrame->SetStyleContext(kidStyleContext);
    } else {
      kidFrame = kidPrevInFlow->CreateContinuingFrame(aPresContext, this);
    }

    // Link the child frame into the list of children and update the
    // child count
    if (nsnull != prevKidFrame) {
      NS_ASSERTION(nsnull == prevKidFrame->GetNextSibling(), "bad append");
      prevKidFrame->SetNextSibling(kidFrame);
    } else {
      NS_ASSERTION(nsnull == mFirstChild, "bad create");
      mFirstChild = kidFrame;
      SetFirstContentOffset(kidFrame);
    }
    mChildCount++;

    do {
      nsSize  kidAvailSize(aState.availSize);
      nsReflowMetrics  desiredSize;

      // Figure out the amount of available size for the child (subtract
      // off the margin we are going to apply to it)
      if (PR_FALSE == aState.unconstrainedHeight) {
        kidAvailSize.height -= topMargin;
      }
      // Subtract off for left and right margin
      if (PR_FALSE == aState.unconstrainedWidth) {
        kidAvailSize.width -= kidMol->margin.left + kidMol->margin.right;
      }

      // Try to reflow the child into the available space. It might not
      // fit or might need continuing
      if (kidAvailSize.height > 0) {
        status = ReflowChild(kidFrame, aPresContext, desiredSize,
                             kidAvailSize, pKidMaxElementSize);
      }

      // Did the child fit?
      if ((nsnull != mFirstChild) &&
          ((kidAvailSize.height <= 0) ||
           (desiredSize.height > kidAvailSize.height))) {
        // The child's height is too big to fit in our remaining
        // space, and it's not our first child.
        NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");
        NS_ASSERTION(nsnull == mNextInFlow, "whoops");

        // Chop off the part of our child list that's being overflowed
        NS_ASSERTION(prevKidFrame->GetNextSibling() == kidFrame, "bad list");
        prevKidFrame->SetNextSibling(nsnull);

        // Create overflow list
        mOverflowList = kidFrame;

        // Fixup child count by subtracting off the number of children
        // that just ended up on the reflow list.
        PRInt32 overflowKids = 0;
        nsIFrame* f = kidFrame;
        while (nsnull != f) {
          overflowKids++;
          f = f->GetNextSibling();
        }
        mChildCount -= overflowKids;
        NS_RELEASE(kidStyleContext);
        NS_RELEASE(kid);
        goto done;
      }

      // Advance y by the topMargin between children. Zero out the
      // topMargin in case this frame is continued because
      // continuations do not have a top margin. Update the prev
      // bottom margin state in the body reflow state so that we can
      // apply the bottom margin when we hit the next child (or
      // finish).
      aState.y += topMargin;
      nsRect kidRect (0, 0, desiredSize.width, desiredSize.height);
      kidRect.x += kidMol->margin.left;
      kidRect.y += aState.y;
      PlaceChild(aPresContext, aState, kidFrame, kidRect, kidMol, aMaxElementSize,
                 kidMaxElementSize);
      if (bottomMargin < 0) {
        aState.prevMaxNegBottomMargin = -bottomMargin;
      } else {
        aState.prevMaxPosBottomMargin = bottomMargin;
      }
      topMargin = 0;

      mLastContentIsComplete = PRBool(status == frComplete);
      if (frNotComplete == status) {
        // Child didn't complete so create a continuing frame
        kidPrevInFlow = kidFrame;
        nsIFrame* continuingFrame =
          kidFrame->CreateContinuingFrame(aPresContext, this);

        // Add the continuing frame to the sibling list
        continuingFrame->SetNextSibling(kidFrame->GetNextSibling());
        kidFrame->SetNextSibling(continuingFrame);
        prevKidFrame = kidFrame;
        kidFrame = continuingFrame;
        mChildCount++;

        /*
        if (nsnull != pseudoFrame) {
          pseudoFrame = (nsBlockFrame*) kidFrame;
        }
        */
        // XXX We probably shouldn't assume that there is no room for
        // the continuation
      }
    } while (frNotComplete == status);
    NS_RELEASE(kidStyleContext);
    NS_RELEASE(kid);

    prevKidFrame = kidFrame;
    kidPrevInFlow = nsnull;

    // Update the kidIndex
    /*
    if (nsnull != pseudoFrame) {
      // Adjust kidIndex to reflect the number of children mapped by
      // the pseudo frame
      kidIndex = pseudoFrame->NextChildOffset();
    } else {
    */
      kidIndex++;
    /*
    }
    */
  }

done:
  // Update the content mapping
  NS_ASSERTION(LastChild() == prevKidFrame, "bad last child");
  if (0 != mChildCount) {
    SetLastContentOffset(prevKidFrame);
  }
#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
  VerifyLastIsComplete();
#endif
  return result;
}

/** Layout the entire row group.
  * This method stacks rows vertically according to HTML 4.0 rules.
  * Rows are responsible for layout of their children.
  */
nsIFrame::ReflowStatus
nsTableRowGroupFrame::ResizeReflow( nsIPresContext*  aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize&    aMaxSize,
                                    nsSize*          aMaxElementSize)
{
  if (gsDebug1==PR_TRUE)
    printf("nsTableRowGroupFrame::ResizeReflow - aMaxSize = %d, %d\n",
            aMaxSize.width, aMaxSize.height);
#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  // Initialize out parameter
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }

  PRBool        reflowMappedOK = PR_TRUE;
  ReflowStatus  status = frComplete;

  // Check for an overflow list
  MoveOverflowToChildList();

  nsStyleMolecule* myMol =
    (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);
  RowGroupReflowState state(aPresContext, aMaxSize, myMol);

  // Reflow the existing frames
  if (nsnull != mFirstChild) {
    reflowMappedOK = ReflowMappedChildren(aPresContext, state, aMaxElementSize);
    if (PR_FALSE == reflowMappedOK) {
      status = frNotComplete;
    }
  }

  // Did we successfully relow our mapped children?
  if (PR_TRUE == reflowMappedOK) {
    // Any space left?
    if ((nsnull != mFirstChild) && (state.availSize.height <= 0)) {
      // No space left. Don't try to pull-up children or reflow unmapped
      if (NextChildOffset() < mContent->ChildCount()) {
        status = frNotComplete;
      }
    } else if (NextChildOffset() < mContent->ChildCount()) {
      // Try and pull-up some children from a next-in-flow
      if ((nsnull == mNextInFlow) ||
          PullUpChildren(aPresContext, state, aMaxElementSize)) {
        // If we still have unmapped children then create some new frames
        if (NextChildOffset() < mContent->ChildCount()) {
          status = ReflowUnmappedChildren(aPresContext, state, aMaxElementSize);
        }
      } else {
        // We were unable to pull-up all the existing frames from the
        // next in flow
        status = frNotComplete;
      }
    }
  }

  if (frComplete == status) {
    // Don't forget to add in the bottom margin from our last child.
    // Only add it in if there's room for it.
    nscoord margin = state.prevMaxPosBottomMargin -
      state.prevMaxNegBottomMargin;
    if (state.availSize.height >= margin) {
      state.y += margin;
    }
  }

  // Return our desired rect
  NS_ASSERTION(0<state.firstRowHeight, "illegal firstRowHeight after reflow");
  NS_ASSERTION(0<state.y, "illegal height after reflow");
  aDesiredSize.width = aMaxSize.width;
  aDesiredSize.height = state.y;

#ifdef NS_DEBUG
  PostReflowCheck(status);
#endif

  if (gsDebug1==PR_TRUE) 
  {
    if (nsnull!=aMaxElementSize)
      printf("nsTableRowGroupFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=%d,%d\n",
              status==frComplete?"Complete":"Not Complete",
              aDesiredSize.width, aDesiredSize.height,
              aMaxElementSize->width, aMaxElementSize->height);
    else
      printf("nsTableRowGroupFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=NSNULL\n", 
             status==frComplete?"Complete":"Not Complete",
             aDesiredSize.width, aDesiredSize.height);
  }

  return status;

}

nsIFrame::ReflowStatus
nsTableRowGroupFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                        nsReflowMetrics& aDesiredSize,
                                        const nsSize&    aMaxSize,
                                        nsReflowCommand& aReflowCommand)
{
  if (gsDebug1==PR_TRUE) printf("nsTableRowGroupFrame::IncrementalReflow\n");

  // total hack for now, just some hard-coded values
  aDesiredSize.width = aMaxSize.width;
  aDesiredSize.height = aMaxSize.height;

  return frComplete;
}

nsIFrame* nsTableRowGroupFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                                      nsIFrame*       aParent)
{
  nsTableRowGroupFrame* cf = new nsTableRowGroupFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aPresContext, aParent, cf);
  if (PR_TRUE==gsDebug1) printf("nsTableRowGroupFrame::CCF parent = %p, this=%p, cf=%p\n", aParent, this, cf);
  return cf;
}

/* ----- static methods ----- */

nsresult nsTableRowGroupFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                        nsIContent* aContent,
                                        PRInt32     aIndexInParent,
                                        nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableRowGroupFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}



