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
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableCell.h"
#include "nsCellLayoutData.h"
#include "nsColLayoutData.h"
#include "nsIView.h"
#include "nsIPtr.h"

NS_DEF_PTR(nsIStyleContext);

#ifdef NS_DEBUG
static PRBool gsDebug1 = PR_FALSE;
static PRBool gsDebug2 = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
#else
static const PRBool gsDebug1 = PR_FALSE;
static const PRBool gsDebug2 = PR_FALSE;
#endif

/* ----------- RowReflowState ---------- */

struct RowReflowState {
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

  // Running x-offset
  nscoord x;

  // Height of tallest cell (excluding cells with rowspan>1)
  nscoord maxCellHeight;
  nscoord maxCellVertSpace; // the maximum MAX(cellheight+top+bottom)
  nscoord maxCellHorzSpace; // the maximum MAX(cellheight+top+bottom)

  
  nsTableFrame *tableFrame;
   

  RowReflowState( nsIPresContext*      aPresContext,
                  const nsReflowState& aReflowState)
    : reflowState(aReflowState)
  {
    availSize.width = reflowState.maxSize.width;
    availSize.height = reflowState.maxSize.height;
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    x=0;
    unconstrainedWidth = PRBool(reflowState.maxSize.width == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(reflowState.maxSize.height == NS_UNCONSTRAINEDSIZE);
    maxCellHeight=0;
    maxCellVertSpace=0;
    maxCellHorzSpace=0;
    tableFrame = nsnull;
  }

  ~RowReflowState() {
  }
};




/* ----------- nsTableRowpFrame ---------- */

nsTableRowFrame::nsTableRowFrame(nsIContent* aContent,
                                 nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aParentFrame),
    mTallestCell(0),
    mCellMaxTopMargin(0),
    mCellMaxBottomMargin(0)
{
}

nsTableRowFrame::~nsTableRowFrame()
{
}

void nsTableRowFrame::ResetMaxChildHeight()
{
  mTallestCell=0;
  mCellMaxTopMargin=0;
  mCellMaxBottomMargin=0;
}

void nsTableRowFrame::SetMaxChildHeight(nscoord aChildHeight, nscoord aTopMargin, nscoord aBottomMargin)
{
  if (mTallestCell<aChildHeight)
    mTallestCell = aChildHeight;

  if (mCellMaxTopMargin<aTopMargin)
    mCellMaxTopMargin = aTopMargin;

  if (mCellMaxBottomMargin<aBottomMargin)
    mCellMaxBottomMargin = aBottomMargin;
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
      nsRect damageArea;
      PRBool overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
      if (overlap) {
        // Translate damage area into kid's coordinate system
        nsRect kidDamageArea(damageArea.x - kidRect.x,
                             damageArea.y - kidRect.y,
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
    } else {
      NS_RELEASE(pView);
    }
    kid->GetNextSibling(kid);
  }
}


/** returns the height of the tallest child in this row (ignoring any cell with rowspans) */
nscoord nsTableRowFrame::GetTallestChild() const
{
  return mTallestCell;
}

nscoord nsTableRowFrame::GetChildMaxTopMargin() const
{
  return mCellMaxTopMargin;
}

nscoord nsTableRowFrame::GetChildMaxBottomMargin() const
{
  return mCellMaxBottomMargin;
}


// Collapse child's top margin with previous bottom margin
nscoord nsTableRowFrame::GetTopMarginFor( nsIPresContext*  aCX,
                                          RowReflowState&  aState,
                                          const nsMargin&  aKidMargin)
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

  // update the running total for the row width
  aState.x += aKidRect.width;

  // Update the maximum element size
  PRInt32 rowSpan = ((nsTableCellFrame*)aKidFrame)->GetRowSpan();
  if (nsnull != aMaxElementSize) 
  {
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
  if ((1==rowSpan))
  {
    nsMargin margin(0,0,0,0);

    if (aState.tableFrame->GetCellMarginData(aKidFrame, margin) == NS_OK)
    {
      nscoord height = aKidRect.height + margin.top + margin.bottom;
  
      if (height > aState.maxCellVertSpace)
        aState.maxCellVertSpace = height;
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

  nsSize      kidMaxElementSize;
  nsSize*     pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRBool      result = PR_TRUE;
  nscoord     maxCellTopMargin = 0;
  nscoord     maxCellBottomMargin = 0;

  for (nsIFrame*  kidFrame = mFirstChild; nsnull != kidFrame; ) {
    
    nsIContent* content = nsnull;
    kidFrame->GetContent(content);            // cell: REFCNT++
    nsTableCell* cell = (nsTableCell*)content;
    
    nsSize kidAvailSize(aState.availSize);
    if (0>=kidAvailSize.height)
      kidAvailSize.height = 1;      // XXX: HaCk - we don't handle negative heights yet

    nsReflowMetrics         desiredSize(pKidMaxElementSize);
    desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;
    nsReflowStatus          status;

    nsMargin       kidMargin(0,0,0,0);
    aState.tableFrame->GetCellMarginData(kidFrame,kidMargin);
    if (kidMargin.top > maxCellTopMargin)
      maxCellTopMargin = kidMargin.top;
    if (kidMargin.bottom > maxCellBottomMargin)
      maxCellBottomMargin = kidMargin.bottom;

    
 
    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    if (PR_FALSE == aState.unconstrainedHeight) 
    {
      kidAvailSize.height -= kidMargin.top + kidMargin.bottom;
    }
    // Subtract off for left and right margin
    if (PR_FALSE == aState.unconstrainedWidth) {
      kidAvailSize.width -= kidMargin.left + kidMargin.right;
    }

    if (NS_UNCONSTRAINEDSIZE == aState.availSize.width)
    {
      // Reflow the child into the available space
      nsReflowState kidReflowState(kidFrame, aState.reflowState, kidAvailSize,
                                   eReflowReason_Resize);
      kidFrame->WillReflow(*aPresContext);
      status = ReflowChild(kidFrame, aPresContext, desiredSize,
                           kidReflowState);
      nsCellLayoutData kidLayoutData((nsTableCellFrame *)kidFrame, &desiredSize, pKidMaxElementSize);
      aState.tableFrame->SetCellLayoutData(aPresContext, &kidLayoutData, cell);
    }
    else
    { // we're in a constrained situation, so get the avail width from the known column widths
      nsCellLayoutData *cellData = aState.tableFrame->GetCellLayoutData(cell);
      PRInt32 cellStartingCol = cell->GetColIndex();
      PRInt32 cellColSpan = cell->GetColSpan();
      nscoord availWidth = 0;
      for (PRInt32 numColSpan=0; numColSpan<cellColSpan; numColSpan++)
        availWidth += aState.tableFrame->GetColumnWidth(cellStartingCol+numColSpan);

      kidAvailSize.width = availWidth;
      nsReflowState kidReflowState(kidFrame, aState.reflowState, kidAvailSize,
                                   eReflowReason_Resize);
      kidFrame->WillReflow(*aPresContext);
      status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);
    }
    if (gsDebug1)
    {
      if (nsnull!=pKidMaxElementSize)
        printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                NS_FRAME_IS_COMPLETE(status)?"complete":"NOT complete", 
                desiredSize.width, desiredSize.height, 
                pKidMaxElementSize->width, pKidMaxElementSize->height);
      else
        printf("reflow of cell returned result = %s with desired=%d,%d, min = nsnull\n",
                NS_FRAME_IS_COMPLETE(status)?"complete":"NOT complete", 
                desiredSize.width, desiredSize.height);
    }
    NS_RELEASE(content);                                                          // cell: REFCNT--
    cell = nsnull;

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
    aState.x = kidMargin.left;
    PRInt32 cellColIndex = ((nsTableCellFrame *)kidFrame)->GetColIndex();
    for (PRInt32 colIndex=0; colIndex<cellColIndex; colIndex++)
          aState.x += aState.tableFrame->GetColumnWidth(colIndex);
    // Place the child after taking into account it's margin and attributes
    nscoord specifiedHeight = 0;
    nscoord cellHeight = desiredSize.height;
    nsIStyleContextPtr kidSC;
    kidFrame->GetStyleContext(aPresContext, kidSC.AssignRef());
    const nsStylePosition* kidPosition = (const nsStylePosition*)
      kidSC->GetStyleData(eStyleStruct_Position);
    switch (kidPosition->mHeight.GetUnit()) {
    case eStyleUnit_Coord:
      specifiedHeight = kidPosition->mHeight.GetCoordValue();
      break;

    case eStyleUnit_Inherit:
      // XXX for now, do nothing
    default:
    case eStyleUnit_Auto:
      break;
    }
    if (specifiedHeight>cellHeight)
      cellHeight = specifiedHeight;
    nsRect kidRect (aState.x, kidMargin.top, desiredSize.width, cellHeight);
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize,
               kidMaxElementSize);
    if (kidMargin.bottom < 0) 
    {
      aState.prevMaxNegBottomMargin = -kidMargin.bottom;
    } 
    else 
    {
      aState.prevMaxPosBottomMargin = kidMargin.bottom;
    }
    childCount++;

		// Remember where we just were in case we end up pushing children
		prevKidFrame = kidFrame;

    // Update mLastContentIsComplete now that this kid fits
    mLastContentIsComplete = NS_FRAME_IS_COMPLETE(status);

    /* Rows should not create continuing frames for cells 
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

  SetMaxChildHeight(aState.maxCellHeight,maxCellTopMargin, maxCellBottomMargin);  // remember height of tallest child who doesn't have a row span

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
    nsReflowMetrics desiredSize(pKidMaxElementSize);
    desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;
    nsReflowStatus  status;

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
    nsSize            kidFrameSize(0,0);
    nsSplittableType  kidIsSplittable;

    kidFrame->GetSize(kidFrameSize);
    kidFrame->IsSplittable(kidIsSplittable);
    
    if ((kidFrameSize.height > aState.availSize.height) &&
        NS_FRAME_IS_NOT_SPLITTABLE(kidIsSplittable)) {
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }

    nsIContent *content = nsnull;
    kidFrame->GetContent(content);                                               // cell: REFNCT++
    nsTableCell*  cell = (nsTableCell*)content;

    // we're in a constrained situation, so get the avail width from the known column widths
    nsCellLayoutData *cellData = aState.tableFrame->GetCellLayoutData(cell);
    PRInt32 cellStartingCol = cell->GetColIndex();
    PRInt32 cellColSpan = cell->GetColSpan();
    nscoord availWidth = 0;
    for (PRInt32 numColSpan=0; numColSpan<cellColSpan; numColSpan++)
      availWidth += aState.tableFrame->GetColumnWidth(cellStartingCol+numColSpan);
    NS_ASSERTION(0<availWidth, "illegal width for this column");
    kidAvailSize.width = availWidth;
    nsReflowState kidReflowState(kidFrame, aState.reflowState, kidAvailSize,
                                 eReflowReason_Resize);
    kidFrame->WillReflow(*aPresContext);
    status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);
    if (nsnull!=pKidMaxElementSize)
    {
      if (gsDebug1) 
      {
        if (nsnull!=pKidMaxElementSize)
          printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                  NS_FRAME_IS_COMPLETE(status)?"complete":"NOT complete", 
                  desiredSize.width, desiredSize.height, 
                  pKidMaxElementSize->width, pKidMaxElementSize->height);
        else
          printf("reflow of cell returned result = %s with desired=%d,%d, min = nsnull\n",
                  NS_FRAME_IS_COMPLETE(status)?"complete":"NOT complete", 
                  desiredSize.width, desiredSize.height);
      }
    }
    NS_RELEASE(content);                                                          // cell: REFCNT--
    cell = nsnull;

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
 * @return  NS_FRAME_COMPLETE if all content has been mapped and NS_FRAME_NOT_COMPLETE
 *            if we should be continued
 */
nsReflowStatus
nsTableRowFrame::ReflowUnmappedChildren( nsIPresContext*      aPresContext,
                                         RowReflowState&      aState,
                                         nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  nsIFrame*     kidPrevInFlow = nsnull;
  nsReflowStatus  result = NS_FRAME_NOT_COMPLETE;

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
  nsSize    kidMaxElementSize(0,0);
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  nsSize    kidAvailSize(aState.availSize);
  PRInt32   kidIndex = NextChildOffset();
  nsIFrame* prevKidFrame;
  
  nscoord   maxTopMargin = 0;
  nscoord   maxBottomMargin = 0;

  LastChild(prevKidFrame);  // XXX remember this...

  for (;;) {
    // Get the next content object
    nsIContent* cell = mContent->ChildAt(kidIndex);
    if (nsnull == cell) {
      result = NS_FRAME_COMPLETE;
      break;
    }

    // Make sure we still have room left
    if (aState.availSize.height <= 0) {
      // Note: return status was set to NS_FRAME_NOT_COMPLETE above...
      NS_RELEASE(cell);                                                        // cell: REFCNT--(a)
      break;
    }

    nsIFrame* kidFrame;

    // Create a child frame -- always an nsTableCell frame
    nsIStyleContext* kidStyleContext = aPresContext->ResolveStyleContextFor(cell, this);
    if (nsnull == kidPrevInFlow) {
      nsIContentDelegate* kidDel = nsnull;
      kidDel = cell->GetDelegate(aPresContext);
      nsresult rv = kidDel->CreateFrame(aPresContext, cell,
                                        this, kidStyleContext, kidFrame);
      NS_RELEASE(kidDel);
    } else {
      kidPrevInFlow->CreateContinuingFrame(aPresContext, this,
                                           kidStyleContext, kidFrame);
    }
    /* since I'm creating the cell frame, this is the first time through reflow
     * it's a good time to set the column style from the cell's width attribute
     * if this is the first row
     */
    SetColumnStyleFromCell(aPresContext, (nsTableCellFrame *)kidFrame, kidStyleContext);
    NS_RELEASE(kidStyleContext);


    nsMargin  margin(0,0,0,0);
    nscoord   topMargin = 0;
    nscoord   bottomMargin = 0;

    if (aState.tableFrame->GetCellMarginData(kidFrame, margin) == NS_OK)
    {
      topMargin = margin.top;
      bottomMargin = margin.bottom;
    }
   
    maxTopMargin = PR_MAX(margin.top,maxTopMargin);
    maxBottomMargin = PR_MAX(margin.bottom,maxBottomMargin);
    
    // Try to reflow the child into the available space. It might not
    // fit or might need continuing.
    nsReflowMetrics desiredSize(pKidMaxElementSize);
    desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;
    nsReflowStatus status;
    if (NS_UNCONSTRAINEDSIZE == aState.availSize.width)
    {
      nsReflowState kidReflowState(kidFrame, aState.reflowState, kidAvailSize,
                                   eReflowReason_Initial);
      // Reflow the child into the available space
      kidFrame->WillReflow(*aPresContext);
      status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);
      nsCellLayoutData kidLayoutData((nsTableCellFrame *)kidFrame, &desiredSize, pKidMaxElementSize);
      aState.tableFrame->SetCellLayoutData(aPresContext, &kidLayoutData, (nsTableCell *)cell);
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
      nsReflowState kidReflowState(kidFrame, aState.reflowState, kidAvailSize, eReflowReason_Initial);
      kidFrame->WillReflow(*aPresContext);
      status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);
    }
    if (nsnull!=pKidMaxElementSize)
    {
      if (gsDebug1) 
      {
        if (nsnull!=pKidMaxElementSize)
          printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                  NS_FRAME_IS_COMPLETE(status)?"complete":"NOT complete", 
                  desiredSize.width, desiredSize.height, 
                  pKidMaxElementSize->width, pKidMaxElementSize->height);
        else
          printf("reflow of cell returned result = %s with desired=%d,%d, min = nsnull\n",
                  NS_FRAME_IS_COMPLETE(status)?"complete":"NOT complete", 
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

    aState.x = margin.left;
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
    if (NS_FRAME_IS_NOT_COMPLETE(status)) {
      // If the child isn't complete then it means that we've used up
      // all of our available space
      mLastContentIsComplete = PR_FALSE;
      break;
    }
    kidPrevInFlow = nsnull;
  }

  SetMaxChildHeight(aState.maxCellHeight, maxTopMargin, maxBottomMargin);  // remember height of tallest child who doesn't have a row span

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
nsTableRowFrame::Reflow(nsIPresContext*      aPresContext,
                        nsReflowMetrics&     aDesiredSize,
                        const nsReflowState& aReflowState,
                        nsReflowStatus&      aStatus)
{
  if (gsDebug1==PR_TRUE)
    printf("nsTableRowFrame::Reflow - aMaxSize = %d, %d\n",
            aReflowState.maxSize.width, aReflowState.maxSize.height);
#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  // Initialize our internal data
  ResetMaxChildHeight();

  PRBool reflowMappedOK = PR_TRUE;

  aStatus = NS_FRAME_COMPLETE;

  // Check for an overflow list
  MoveOverflowToChildList();

  RowReflowState state(aPresContext, aReflowState);
  mContentParent->GetContentParent((nsIFrame*&)(state.tableFrame));

  // Reflow the existing frames
  if (nsnull != mFirstChild) {
    reflowMappedOK = ReflowMappedChildren(aPresContext, state, aDesiredSize.maxElementSize);
    if (PR_FALSE == reflowMappedOK) {
      aStatus = NS_FRAME_NOT_COMPLETE;
    }
  }

  // Did we successfully relow our mapped children?
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
  }

  // Return our desired rect
  aDesiredSize.width = state.x;
  aDesiredSize.height = state.maxCellVertSpace;   

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif

  if (gsDebug1==PR_TRUE) 
  {
    if (nsnull!=aDesiredSize.maxElementSize)
      printf("nsTableRowFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=%d,%d\n",
              NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
              aDesiredSize.width, aDesiredSize.height,
              aDesiredSize.maxElementSize->width, aDesiredSize.maxElementSize->height);
    else
      printf("nsTableRowFrame::RR returning: %s with aDesiredSize=%d,%d, aMES=NSNULL\n", 
             NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
             aDesiredSize.width, aDesiredSize.height);
  }

  return NS_OK;

}

NS_METHOD
nsTableRowFrame::CreateContinuingFrame(nsIPresContext*  aPresContext,
                                       nsIFrame*        aParent,
                                       nsIStyleContext* aStyleContext,
                                       nsIFrame*&       aContinuingFrame)
{
  if (gsDebug1==PR_TRUE) printf("nsTableRowFrame::CreateContinuingFrame\n");
  nsTableRowFrame* cf = new nsTableRowFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

NS_METHOD
nsTableRowFrame::SetColumnStyleFromCell(nsIPresContext*   aPresContext,
                                        nsTableCellFrame* aCellFrame, 
                                        nsIStyleContext*  aCellSC)
{
  // if this cell is in the first row, then the width attribute
  // also acts as the width attribute for the entire column
  if ((nsnull!=aCellSC) && (nsnull!=aCellFrame))
  {
    if (0==mRowIndex)
    {
      // get the cell style info
      const nsStylePosition* cellPosition = (const nsStylePosition*) aCellSC->GetStyleData(eStyleStruct_Position);
      if ((eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) ||
           (eStyleUnit_Percent==cellPosition->mWidth.GetUnit())) {

        // compute the width per column spanned
        PRInt32 colSpan = aCellFrame->GetColSpan();
        nsTableFrame *tableFrame;
        mGeometricParent->GetGeometricParent((nsIFrame *&)tableFrame);
        for (PRInt32 i=0; i<colSpan; i++)
        {
          // get the appropriate column frame
          nsTableColFrame *colFrame;
          tableFrame->GetColumnFrame(i+aCellFrame->GetColIndex(), colFrame);
          // get the column style and set the width attribute
          nsIStyleContextPtr colSC;
          colFrame->GetStyleContext(aPresContext, colSC.AssignRef());
          nsStylePosition* colPosition = (nsStylePosition*) colSC->GetMutableStyleData(eStyleStruct_Position);
          // set the column width attribute
          if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit())
          {
            nscoord width = cellPosition->mWidth.GetCoordValue();
            colPosition->mWidth.SetCoordValue(width/colSpan);
          }
          else
          {
            float width = cellPosition->mWidth.GetPercentValue();
            colPosition->mWidth.SetPercentValue(width/colSpan);
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult nsTableRowFrame::NewFrame( nsIFrame** aInstancePtrResult,
                                    nsIContent* aContent,
                                    nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableRowFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}


// For Debugging ONLY
NS_METHOD nsTableRowFrame::MoveTo(nscoord aX, nscoord aY)
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

NS_METHOD nsTableRowFrame::SizeTo(nscoord aWidth, nscoord aHeight)
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
