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
#include "nsTableRowFrame.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableCell.h"
#include "nsCellLayoutData.h"
#include "nsIView.h"

#ifdef NS_DEBUG
static PRBool gsDebug1 = PR_FALSE;
static PRBool gsDebug2 = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
#else
static const PRBool gsDebug1 = PR_FALSE;
static const PRBool gsDebug2 = PR_FALSE;
#endif

static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);

/* ----------- RowReflowState ---------- */

struct RowReflowState {
  // The body's available size (computed from the body's parent)
  nsSize availSize;

  // Margin tracking information
  nscoord prevMaxPosBottomMargin;
  nscoord prevMaxNegBottomMargin;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running x-offset
  nscoord x;

  // Height of tallest cell (excluding cells with rowspan>1)
  nscoord maxCellHeight;
  
  nsTableFrame *tableFrame;
   

  RowReflowState( nsIPresContext*  aPresContext,
                  const nsSize&    aMaxSize)
  {
    availSize.width = aMaxSize.width;
    availSize.height = aMaxSize.height;
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    x=0;
    unconstrainedWidth = PRBool(aMaxSize.width == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(aMaxSize.height == NS_UNCONSTRAINEDSIZE);
    maxCellHeight=0;
    tableFrame = nsnull;
  }

  ~RowReflowState() {
  }
};




/* ----------- nsTableRowpFrame ---------- */

nsTableRowFrame::nsTableRowFrame(nsIContent* aContent,
                                 PRInt32     aIndexInParent,
                                 nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aIndexInParent, aParentFrame),
    mTallestCell(0)
{
}

nsTableRowFrame::~nsTableRowFrame()
{
}

void nsTableRowFrame::ResetMaxChildHeight()
{
  mTallestCell=0;
}

void nsTableRowFrame::SetMaxChildHeight(PRInt32 aChildHeight)
{
  if (mTallestCell<aChildHeight)
    mTallestCell = aChildHeight;
}

NS_METHOD nsTableRowFrame::Paint(nsIPresContext& aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect& aDirtyRect)
{
  // for debug...
  /*
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,0,128));
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
void nsTableRowFrame::PaintChildren(nsIPresContext&      aPresContext,
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
    } else {
      NS_RELEASE(pView);
    }
    kid->GetNextSibling(kid);
  }
}


/** returns the tallest child in this row (ignoring any cell with rowspans) */
PRInt32 nsTableRowFrame::GetTallestChild() const
{
  return mTallestCell;
}

// Collapse child's top margin with previous bottom margin
nscoord nsTableRowFrame::GetTopMarginFor( nsIPresContext*  aCX,
                                          RowReflowState&  aState,
                                          nsStyleSpacing* aKidSpacing)
{
  nscoord margin;
  nscoord maxNegTopMargin = 0;
  nscoord maxPosTopMargin = 0;
  if ((margin = aKidSpacing->mMargin.top) < 0) {
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
void nsTableRowFrame::PlaceChild(nsIPresContext*    aPresContext,
																 RowReflowState&    aState,
																 nsIFrame*          aKidFrame,
																 const nsRect&      aKidRect,
																 nsSize*            aMaxElementSize,
																 nsSize&            aKidMaxElementSize)
{
  if (PR_TRUE==gsDebug1)
    printf ("row: placing cell at %d, %d, %d, %d\n",
           aKidRect.x, aKidRect.y, aKidRect.width, aKidRect.height);

  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // Update the maximum element size
  PRInt32 rowSpan = ((nsTableCellFrame*)aKidFrame)->GetRowSpan();
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width += aKidMaxElementSize.width;
    if ((1==rowSpan) && (aKidMaxElementSize.height>aMaxElementSize->height))
    {
      aMaxElementSize->height = aKidMaxElementSize.height;
    }
  }
  if ((1==rowSpan) && (aState.maxCellHeight<aKidRect.height))
  {
    aState.maxCellHeight = aKidRect.height;
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
PRBool nsTableRowFrame::ReflowMappedChildren(nsIPresContext* aPresContext,
                                             RowReflowState& aState,
                                             nsSize*         aMaxElementSize)
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
    nsTableRowFrame* flow = (nsTableRowFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableRowFrame*) flow->mNextInFlow;
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
    nsIContent* cell = nsnull;
    kidFrame->GetContent(cell);                                  // cell: REFCNT++
    nsSize kidAvailSize(aState.availSize);
    if (0>=kidAvailSize.height)
      kidAvailSize.height = 1;      // XXX: HaCk - we don't handle negative heights yet
    nsReflowMetrics         desiredSize;
    nsIFrame::ReflowStatus  status;

    // Get top margin for this kid
    nsIStyleContext* kidSC;
    kidFrame->GetStyleContext(aPresContext, kidSC);
    nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
      kidSC->GetData(kStyleSpacingSID);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidSpacing);
    nscoord bottomMargin = kidSpacing->mMargin.bottom;
    NS_RELEASE(kidSC);

    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    if (PR_FALSE == aState.unconstrainedHeight) {
      kidAvailSize.height -= topMargin;
    }
    // Subtract off for left and right margin
    if (PR_FALSE == aState.unconstrainedWidth) {
      kidAvailSize.width -= kidSpacing->mMargin.left + kidSpacing->mMargin.right;
    }

    if (NS_UNCONSTRAINEDSIZE == aState.availSize.width)
    {
      // Reflow the child into the available space
      status = ReflowChild(kidFrame, aPresContext, desiredSize,
                           kidAvailSize, pKidMaxElementSize);
      nsCellLayoutData kidLayoutData((nsTableCellFrame *)kidFrame, &desiredSize, pKidMaxElementSize);
      aState.tableFrame->SetCellLayoutData(&kidLayoutData, (nsTableCell *)cell);
    }
    else
    { // we're in a constrained situation, so get the avail width from the known column widths
      nsCellLayoutData *cellData = aState.tableFrame->GetCellLayoutData((nsTableCell *)cell);
      PRInt32 cellStartingCol = ((nsTableCell *)cell)->GetColIndex();
      PRInt32 cellColSpan = ((nsTableCell *)cell)->GetColSpan();
      nscoord availWidth = 0;
      for (PRInt32 numColSpan=0; numColSpan<cellColSpan; numColSpan++)
        availWidth += aState.tableFrame->GetColumnWidth(cellStartingCol+numColSpan);
//      NS_ASSERTION(0<availWidth, "illegal width for this column");
      kidAvailSize.width = availWidth;
      status = ReflowChild(kidFrame, aPresContext, desiredSize, 
                           kidAvailSize, pKidMaxElementSize);
    }
    if (nsnull!=pKidMaxElementSize)
    {
      if (gsDebug1)
      {
        if (nsnull!=pKidMaxElementSize)
          printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                  status==frComplete?"complete":"NOT complete", 
                  desiredSize.width, desiredSize.height, 
                  pKidMaxElementSize->width, pKidMaxElementSize->height);
        else
          printf("reflow of cell returned result = %s with desired=%d,%d, min = nsnull\n",
                  status==frComplete?"complete":"NOT complete", 
                  desiredSize.width, desiredSize.height);
      }
    }
    NS_RELEASE(cell);                                                          // cell: REFCNT--
    // Did the child fit?
    if ((kidFrame != mFirstChild) &&
        ((kidAvailSize.height <= 0) ||
         (desiredSize.height > kidAvailSize.height)))
    {
      // The child's height is too big to fit at all in our remaining space,
      // and it's not our first child.

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

    // Adjust the running x-offset, taking into account intruders (cells from prior rows with rowspans > 1)
    aState.x = 0;
    PRInt32 cellColIndex = ((nsTableCellFrame *)kidFrame)->GetColIndex();
    for (PRInt32 colIndex=0; colIndex<cellColIndex; colIndex++)
          aState.x += aState.tableFrame->GetColumnWidth(colIndex);
    // Place the child after taking into account it's margin
    nsRect kidRect (aState.x, topMargin, desiredSize.width, desiredSize.height);
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
    mLastContentIsComplete = PRBool(status == frComplete);

    /* Rows should not create continuing frames for cells 
     * unless they absolutely have to!
     * check to see if this is absolutely necessary (with new params from troy)
     * otherwise PushChildren and bail.
     */
    // Special handling for incomplete children
    if (frNotComplete == status) {
      // XXX It's good to assume that we might still have room
      // even if the child didn't complete (floaters will want this)
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
			PRBool lastContentIsComplete = mLastContentIsComplete;
      if (nsnull == kidNextInFlow) {
        // No the child isn't complete, and it doesn't have a next in flow so
        // create a continuing frame. This hooks the child into the flow.
        nsIFrame* continuingFrame;
         
        kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);

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
      kidAvailSize.width += kidSpacing->mMargin.left + kidSpacing->mMargin.right;
    }

    // Get the next child
    kidFrame->GetNextSibling(kidFrame);

  }

  SetMaxChildHeight(aState.maxCellHeight);  // remember height of tallest child who doesn't have a row span

  // Update the child count
  mChildCount = childCount;

#ifdef NS_DEBUG
  NS_POSTCONDITION(LengthOf(mFirstChild) == mChildCount, "bad child count");

  nsIFrame* lastChild;
  PRInt32   lastIndexInParent;

  LastChild(lastChild);
  lastChild->GetIndexInParent(lastIndexInParent);
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
    nsTableRowFrame* flow = (nsTableRowFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableRowFrame*) flow->mNextInFlow;
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
PRBool nsTableRowFrame::PullUpChildren(nsIPresContext*      aPresContext,
                                            RowReflowState& aState,
                                            nsSize*         aMaxElementSize)
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
    nsTableRowFrame* flow = (nsTableRowFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableRowFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  nsTableRowFrame* nextInFlow = (nsTableRowFrame*)mNextInFlow;
  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  nsSize    kidAvailSize(aState.availSize);
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
    nsReflowMetrics desiredSize;
    ReflowStatus    status;

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
    nsSize          kidFrameSize;
    SplittableType  kidIsSplittable;

    kidFrame->GetSize(kidFrameSize);
    kidFrame->IsSplittable(kidIsSplittable);
    
    if ((kidFrameSize.height > aState.availSize.height) &&
        (kidIsSplittable == frNotSplittable)) {
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }

    nsIContent *cell = nsnull;
    kidFrame->GetContent(cell);                                               // cell: REFNCT++
    // we're in a constrained situation, so get the avail width from the known column widths
    nsCellLayoutData *cellData = aState.tableFrame->GetCellLayoutData((nsTableCell *)cell);
    PRInt32 cellStartingCol = ((nsTableCell *)cell)->GetColIndex();
    PRInt32 cellColSpan = ((nsTableCell *)cell)->GetColSpan();
    nscoord availWidth = 0;
    for (PRInt32 numColSpan=0; numColSpan<cellColSpan; numColSpan++)
      availWidth += aState.tableFrame->GetColumnWidth(cellStartingCol+numColSpan);
    NS_ASSERTION(0<availWidth, "illegal width for this column");
    kidAvailSize.width = availWidth;
    status = ReflowChild(kidFrame, aPresContext, desiredSize, 
                         kidAvailSize, pKidMaxElementSize);
    if (nsnull!=pKidMaxElementSize)
    {
      if (gsDebug1) 
      {
        if (nsnull!=pKidMaxElementSize)
          printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                  status==frComplete?"complete":"NOT complete", 
                  desiredSize.width, desiredSize.height, 
                  pKidMaxElementSize->width, pKidMaxElementSize->height);
        else
          printf("reflow of cell returned result = %s with desired=%d,%d, min = nsnull\n",
                  status==frComplete?"complete":"NOT complete", 
                  desiredSize.width, desiredSize.height);
      }
    }
    NS_RELEASE(cell);                                                          // cell: REFCNT--

    // Did the child fit?
    if ((desiredSize.height > aState.availSize.height) && (nsnull != mFirstChild)) {
      // The child is too tall to fit in the available space, and it's
      // not our first child
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }

    // Adjust the running x-offset, taking into account intruders (cells from prior rows with rowspans > 1)
    aState.x = 0;
    PRInt32 cellColIndex = ((nsTableCellFrame *)kidFrame)->GetColIndex();
    for (PRInt32 colIndex=0; colIndex<cellColIndex; colIndex++)
          aState.x += aState.tableFrame->GetColumnWidth(colIndex);
    // Place the child
    //nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidMol);
    nsRect kidRect (aState.x, 0, desiredSize.width, desiredSize.height);
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
    mLastContentIsComplete = PRBool(status == frComplete);
    if (frNotComplete == status) {
      // No the child isn't complete
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a
        // continuing frame. The creation appends it to the flow and
        // prepares it for reflow.
        nsIFrame* continuingFrame;
         
        kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);
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
  nextInFlow = (nsTableRowFrame*)mNextInFlow;
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
    nsTableRowFrame* flow = (nsTableRowFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableRowFrame*) flow->mNextInFlow;
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
nsTableRowFrame::ReflowUnmappedChildren( nsIPresContext*      aPresContext,
                                         RowReflowState&      aState,
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
    nsTableRowFrame* prev = (nsTableRowFrame*)mPrevInFlow;
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
  nsSize    kidAvailSize(aState.availSize);
  PRInt32   kidIndex = NextChildOffset();
  nsIFrame* prevKidFrame;
   
  LastChild(prevKidFrame);  // XXX remember this...

  for (;;) {
    // Get the next content object
    nsIContent* cell = mContent->ChildAt(kidIndex);
    if (nsnull == cell) {
      result = frComplete;
      break;
    }

    // Make sure we still have room left
    if (aState.availSize.height <= 0) {
      // Note: return status was set to frNotComplete above...
      NS_RELEASE(cell);                                                        // cell: REFCNT--(a)
      break;
    }

    // Resolve style
    nsIStyleContext* kidStyleContext =
      aPresContext->ResolveStyleContextFor(cell, this);
    nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
      kidStyleContext->GetData(kStyleSpacingSID);
    nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidSpacing);
    nscoord bottomMargin = kidSpacing->mMargin.bottom;

    nsIFrame* kidFrame;

    // Create a child frame
    if (nsnull == kidPrevInFlow) {
      nsIContentDelegate* kidDel = nsnull;
      kidDel = cell->GetDelegate(aPresContext);
      kidFrame = kidDel->CreateFrame(aPresContext, cell, kidIndex, this);
      NS_RELEASE(kidDel);
      kidFrame->SetStyleContext(kidStyleContext);
    } else {
      kidPrevInFlow->CreateContinuingFrame(aPresContext, this, kidFrame);
    }
    NS_RELEASE(kidStyleContext);

    // Try to reflow the child into the available space. It might not
    // fit or might need continuing.
    nsReflowMetrics desiredSize;
    ReflowStatus status;
    if (NS_UNCONSTRAINEDSIZE == aState.availSize.width)
    {
      // Reflow the child into the available space
      status = ReflowChild(kidFrame, aPresContext, desiredSize,
                           kidAvailSize, pKidMaxElementSize);
      nsCellLayoutData kidLayoutData((nsTableCellFrame *)kidFrame, &desiredSize, pKidMaxElementSize);
      aState.tableFrame->SetCellLayoutData(&kidLayoutData, (nsTableCell *)cell);
    }
    else
    { // we're in a constrained situation, so get the avail width from the known column widths
      nsCellLayoutData *cellData = aState.tableFrame->GetCellLayoutData((nsTableCell *)cell);
      PRInt32 cellStartingCol = ((nsTableCell *)cell)->GetColIndex();
      PRInt32 cellColSpan = ((nsTableCell *)cell)->GetColSpan();
      nscoord availWidth = 0;
      for (PRInt32 numColSpan=0; numColSpan<cellColSpan; numColSpan++)
        availWidth += aState.tableFrame->GetColumnWidth(cellStartingCol+numColSpan);
      NS_ASSERTION(0<availWidth, "illegal width for this column");
      kidAvailSize.width = availWidth;
      status = ReflowChild(kidFrame, aPresContext, desiredSize, 
                           kidAvailSize, pKidMaxElementSize);
    }
    if (nsnull!=pKidMaxElementSize)
    {
      if (gsDebug1) 
      {
        if (nsnull!=pKidMaxElementSize)
          printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                  status==frComplete?"complete":"NOT complete", 
                  desiredSize.width, desiredSize.height, 
                  pKidMaxElementSize->width, pKidMaxElementSize->height);
        else
          printf("reflow of cell returned result = %s with desired=%d,%d, min = nsnull\n",
                  status==frComplete?"complete":"NOT complete", 
                  desiredSize.width, desiredSize.height);
      }
    }
    NS_RELEASE(cell);                                                          // cell: REFCNT--

    // Did the child fit?
    if ((desiredSize.height > aState.availSize.height) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child. Add the frame to our overflow list
      NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");
      mOverflowList = kidFrame;
      prevKidFrame->SetNextSibling(nsnull);
      break;
    }

    aState.x = 0;
    // if we're constrained, compute the x offset where the cell should be put
    if (NS_UNCONSTRAINEDSIZE!=aState.availSize.width)
    {
      // Adjust the running x-offset, taking into account intruders (cells from prior rows with rowspans > 1)
      PRInt32 cellColIndex = ((nsTableCellFrame *)kidFrame)->GetColIndex();
      for (PRInt32 colIndex=0; colIndex<cellColIndex; colIndex++)
            aState.x += aState.tableFrame->GetColumnWidth(colIndex);
    }
    // Place the child
    nsRect kidRect (aState.x, topMargin, desiredSize.width, desiredSize.height);
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
    if (frNotComplete == status) {
      // If the child isn't complete then it means that we've used up
      // all of our available space
      mLastContentIsComplete = PR_FALSE;
      break;
    }
    kidPrevInFlow = nsnull;
  }

  SetMaxChildHeight(aState.maxCellHeight);  // remember height of tallest child who doesn't have a row span

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


/** Layout the entire row.
  * This method stacks rows horizontally according to HTML 4.0 rules.
  * Rows are responsible for layout of their children (cells).
  */
NS_METHOD
nsTableRowFrame::ResizeReflow(nsIPresContext*  aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              const nsSize&    aMaxSize,
                              nsSize*          aMaxElementSize,
                              ReflowStatus&    aStatus)
{
  if (gsDebug1==PR_TRUE)
    printf("nsTableRowFrame::ResizeReflow - aMaxSize = %d, %d\n",
            aMaxSize.width, aMaxSize.height);
#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  // Initialize out parameter
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }

  // Initialize our internal data
  ResetMaxChildHeight();

  PRBool reflowMappedOK = PR_TRUE;

  aStatus = frComplete;

  // Check for an overflow list
  MoveOverflowToChildList();

  RowReflowState state(aPresContext, aMaxSize);
  mContentParent->GetContentParent((nsIFrame*&)(state.tableFrame));

  // Reflow the existing frames
  if (nsnull != mFirstChild) {
    reflowMappedOK = ReflowMappedChildren(aPresContext, state, aMaxElementSize);
    if (PR_FALSE == reflowMappedOK) {
      aStatus = frNotComplete;
    }
  }

  // Did we successfully relow our mapped children?
  if (PR_TRUE == reflowMappedOK) {
    // Any space left?
    if (state.availSize.height <= 0) {
      // No space left. Don't try to pull-up children or reflow unmapped
      if (NextChildOffset() < mContent->ChildCount()) {
        aStatus = frNotComplete;
      }
    } else if (NextChildOffset() < mContent->ChildCount()) {
      // Try and pull-up some children from a next-in-flow
      if (PullUpChildren(aPresContext, state, aMaxElementSize)) {
        // If we still have unmapped children then create some new frames
        if (NextChildOffset() < mContent->ChildCount()) {
          aStatus = ReflowUnmappedChildren(aPresContext, state, aMaxElementSize);
        }
      } else {
        // We were unable to pull-up all the existing frames from the
        // next in flow
        aStatus = frNotComplete;
      }
    }
  }

  if (frComplete == aStatus) {
    // Don't forget to add in the bottom margin from our last child.
    // Only add it in if there's room for it.
    nscoord margin = state.prevMaxPosBottomMargin -
      state.prevMaxNegBottomMargin;
  }

  // Return our desired rect
  aDesiredSize.width = aMaxSize.width;
  aDesiredSize.height = state.maxCellHeight;   

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif

  if (gsDebug1==PR_TRUE) 
  {
    if (nsnull!=aMaxElementSize)
      printf("nsTableRowFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=%d,%d\n",
              aStatus==frComplete?"Complete":"Not Complete",
              aDesiredSize.width, aDesiredSize.height,
              aMaxElementSize->width, aMaxElementSize->height);
    else
      printf("nsTableRowFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=NSNULL\n", 
             aStatus==frComplete?"Complete":"Not Complete",
             aDesiredSize.width, aDesiredSize.height);
  }

  return NS_OK;

}

NS_METHOD
nsTableRowFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                             nsReflowMetrics& aDesiredSize,
                             const nsSize&    aMaxSize,
                             nsReflowCommand& aReflowCommand,
                             ReflowStatus&    aStatus)
{
  if (gsDebug1==PR_TRUE) printf("nsTableRowFrame::IncrementalReflow\n");
  aStatus = frComplete;
  return NS_OK;
}

NS_METHOD nsTableRowFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                                 nsIFrame*       aParent,
                                                 nsIFrame*&      aContinuingFrame)
{
  if (gsDebug1==PR_TRUE) printf("nsTableRowFrame::CreateContinuingFrame\n");
  nsTableRowFrame* cf = new nsTableRowFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aPresContext, aParent, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

nsresult nsTableRowFrame::NewFrame( nsIFrame** aInstancePtrResult,
                                    nsIContent* aContent,
                                    PRInt32     aIndexInParent,
                                    nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableRowFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

