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
#include "nsCellLayoutData.h"
#include "nsColLayoutData.h"
#include "nsIView.h"
#include "nsIPtr.h"
#include "nsIReflowCommand.h"

NS_DEF_PTR(nsIStyleContext);

const nsIID kTableRowFrameCID = NS_TABLEROWFRAME_CID;

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

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running x-offset
  nscoord x;

  // Height of tallest cell (excluding cells with rowspan > 1)
  nscoord maxCellHeight;    // just the height of the cell frame
  nscoord maxCellVertSpace; // the maximum MAX(cellheight + topMargin + bottomMargin)
  
  nsTableFrame *tableFrame;
   

  RowReflowState( nsIPresContext*      aPresContext,
                  const nsReflowState& aReflowState)
    : reflowState(aReflowState)
  {
    availSize.width = reflowState.maxSize.width;
    availSize.height = reflowState.maxSize.height;
    x=0;
    unconstrainedWidth = PRBool(reflowState.maxSize.width == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(reflowState.maxSize.height == NS_UNCONSTRAINEDSIZE);
    maxCellHeight = 0;
    maxCellVertSpace = 0;
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
    mCellMaxBottomMargin(0),
    mMinRowSpan(1)
{
}

nsTableRowFrame::~nsTableRowFrame()
{
}

/**
 * Post-reflow hook. This is where the table row does its post-processing
 */
NS_METHOD
nsTableRowFrame::DidReflow(nsIPresContext& aPresContext,
                           nsDidReflowStatus aStatus)
{
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    // Resize and re-align the cell frames based on our row height
    nscoord           cellHeight = mRect.height - mCellMaxTopMargin - mCellMaxBottomMargin;
    nsTableCellFrame *cellFrame = (nsTableCellFrame*)mFirstChild;
    nsTableFrame*     tableFrame;
    mContentParent->GetContentParent((nsIFrame*&)tableFrame);
    while (nsnull != cellFrame)
    {
      PRInt32 rowSpan = tableFrame->GetEffectiveRowSpan(mRowIndex, cellFrame);
      if (1==rowSpan)
      {
        // resize the cell's height
        nsSize  cellFrameSize;
        cellFrame->GetSize(cellFrameSize);
        cellFrame->SizeTo(cellFrameSize.width, cellHeight);
  
        // realign cell content based on the new height
        cellFrame->VerticallyAlignChild(&aPresContext);
      }
  
      // Get the next cell
      cellFrame->GetNextSibling((nsIFrame*&)cellFrame);
    }
  }

  // Let our base class do the usual work
  return nsContainerFrame::DidReflow(aPresContext, aStatus);
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

void nsTableRowFrame::SetRowIndex (int aRowIndex)
{
  mRowIndex = aRowIndex;
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

PRInt32 nsTableRowFrame::GetMaxColumns() const
{
  int sum = 0;
  nsTableCellFrame *cell=(nsTableCellFrame *)mFirstChild;
  while (nsnull!=cell) {
    sum += cell->GetColSpan();
    cell->GetNextSibling((nsIFrame *&)cell);
  }
  return sum;
}

/* GetMinRowSpan is needed for deviant cases where every cell in a row has a rowspan > 1.
 * It sets mMinRowSpan, which is used in FixMinCellHeight and PlaceChild
 */
void nsTableRowFrame::GetMinRowSpan()
{
  PRInt32 minRowSpan=-1;
  nsIFrame *frame=mFirstChild;
  while (nsnull!=frame)
  {
    PRInt32 rowSpan = ((nsTableCellFrame *)frame)->GetRowSpan();
    if (-1==minRowSpan)
      minRowSpan = rowSpan;
    else if (minRowSpan>rowSpan)
      minRowSpan = rowSpan;
    frame->GetNextSibling(frame);
  }
  mMinRowSpan = minRowSpan;
}

void nsTableRowFrame::FixMinCellHeight()
{
  nsIFrame *frame=mFirstChild;
  while (nsnull!=frame)
  {
    PRInt32 rowSpan = ((nsTableCellFrame *)frame)->GetRowSpan();
    if (mMinRowSpan==rowSpan)
    {
      nsRect rect;
      frame->GetRect(rect);
      if (rect.height > mTallestCell)
        mTallestCell = rect.height;
    }
    frame->GetNextSibling(frame);
  }
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame, and includes
// any left/top margin.
void nsTableRowFrame::PlaceChild(nsIPresContext*    aPresContext,
																 RowReflowState&    aState,
																 nsIFrame*          aKidFrame,
																 const nsRect&      aKidRect,
																 nsSize*            aMaxElementSize,
																 nsSize*            aKidMaxElementSize)
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
    aMaxElementSize->width += aKidMaxElementSize->width;
    if ((mMinRowSpan==rowSpan) && (aKidMaxElementSize->height>aMaxElementSize->height))
    {
      aMaxElementSize->height = aKidMaxElementSize->height;
    }
  }

  if (mMinRowSpan == rowSpan)
  {
    // Update maxCellHeight
    if (aKidRect.height > aState.maxCellHeight)
      aState.maxCellHeight = aKidRect.height;

    // Update maxCellVertSpace
    nsMargin margin;

    if (aState.tableFrame->GetCellMarginData((nsTableCellFrame *)aKidFrame, margin) == NS_OK)
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

  // Reflow each of our existing child frames
  for (nsIFrame*  kidFrame = mFirstChild; nsnull != kidFrame; ) {
    nsSize kidAvailSize(aState.availSize);
    if (0>=kidAvailSize.height)
      kidAvailSize.height = 1;      // XXX: HaCk - we don't handle negative heights yet

    // Get the frame's margins and compare the top and bottom margin
    // against our current max values
    nsMargin       kidMargin;
    aState.tableFrame->GetCellMarginData((nsTableCellFrame *)kidFrame,kidMargin);
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

    // left and right margins already taken into account by table layout strategy

    // Compute the x-origin for the child
    //
    // XXX Having to re-compute the value each time from scratch is very inefficent...
    nscoord x = ComputeCellXOffset(aState, kidFrame, kidMargin);

    // Compute the cell avail width from the column widths
    nscoord availWidth = ComputeCellAvailWidth(aState, kidFrame);

    // If the available space is the same as last time we reflowed the cell
    // then just use the previous desired size
    //
    // XXX Wouldn't it be cleaner for the row to reflow the cell and have the
    // cell decide whether it could use the cached value rather than having
    // the row make that determination?
    nsReflowMetrics desiredSize(pKidMaxElementSize);
    nsReflowStatus  status;
    if (availWidth != ((nsTableCellFrame *)kidFrame)->GetPriorAvailWidth())
    {
      // Reflow the child
      kidFrame->WillReflow(*aPresContext);
      kidFrame->MoveTo(x, kidMargin.top);
      kidAvailSize.width = availWidth;
      nsReflowState kidReflowState(kidFrame, aState.reflowState, kidAvailSize,
                                   eReflowReason_Resize);
      status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);

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
    else
    {
      nsSize priorSize = ((nsTableCellFrame *)kidFrame)->GetPriorDesiredSize();
      desiredSize.width = priorSize.width;
      desiredSize.height = priorSize.height;
      status = NS_FRAME_COMPLETE; // XXX: in paginated world, this doesn't work!
    }

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

    // Place the child after taking into account its margin and attributes
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

    nscoord cellWidth = desiredSize.width;
    // begin special Nav4 compatibility code
    if (0==cellWidth)
    {
      PRInt32 cellColIndex = ((nsTableCellFrame *)kidFrame)->GetColIndex();
      cellWidth = aState.tableFrame->GetColumnWidth(cellColIndex);
    }
    // end special Nav4 compatibility code

    // Adjust the running x-offset
    aState.x = x;

    // Place the child
    nsRect kidRect (x, kidMargin.top, cellWidth, cellHeight);

    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize,
               pKidMaxElementSize);
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

    // we're in a constrained situation, so get the avail width from the known column widths
    nsCellLayoutData *cellData = aState.tableFrame->GetCellLayoutData((nsTableCellFrame *)kidFrame);
    PRInt32 cellStartingCol = ((nsTableCellFrame *)kidFrame)->GetColIndex();
    PRInt32 cellColSpan = ((nsTableCellFrame *)kidFrame)->GetColSpan();
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
    nsRect kidRect (aState.x, 0, desiredSize.width, desiredSize.height);
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize, pKidMaxElementSize);

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

// Compute the x-origin for the child by summing up the width of each
// prior column. This correctly handles cells from prior rows with rowspans
// > 1
nscoord nsTableRowFrame::ComputeCellXOffset(const RowReflowState& aState,
                                            nsIFrame*             aKidFrame,
                                            const nsMargin&       aKidMargin) const
{
  nscoord x = aKidMargin.left;
  PRInt32 cellColIndex = ((nsTableCellFrame *)aKidFrame)->GetColIndex();
  
  for (PRInt32 colIndex = 0; colIndex < cellColIndex; colIndex++)
  {
    x += aState.tableFrame->GetColumnWidth(colIndex);
  }

  return x;
}

// Computes the avail width for the cell. Takes into account column
// spans
nscoord nsTableRowFrame::ComputeCellAvailWidth(const RowReflowState& aState,
                                               nsIFrame*             aKidFrame) const
{
  nscoord           availWidth = 0;
  nsCellLayoutData *cellData = aState.tableFrame->GetCellLayoutData((nsTableCellFrame *)aKidFrame);
  PRInt32           cellStartingCol = ((nsTableCellFrame *)aKidFrame)->GetColIndex();
  PRInt32           cellColSpan = ((nsTableCellFrame *)aKidFrame)->GetColSpan();

  for (PRInt32 numColSpan = 0; numColSpan < cellColSpan; numColSpan++)
  {
    availWidth += aState.tableFrame->GetColumnWidth(cellStartingCol + numColSpan);
  }

  return availWidth;
}

/**
 * Create new frames for content we haven't yet mapped
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  Always returns NS_FRAME_COMPLETE. Rows always map every cell in the
 *            row, and they are never continued
 */
nsReflowStatus
nsTableRowFrame::ReflowUnmappedChildren(nsIPresContext* aPresContext,
                                        RowReflowState& aState,
                                        nsSize*         aMaxElementSize)
{
  // Place our children, one at a time, until we are out of children
  nsSize    kidMaxElementSize;
  PRInt32   kidIndex = 0;
  nsIFrame* prevKidFrame = nsnull;
  nscoord   maxTopMargin = 0;
  nscoord   maxBottomMargin = 0;

  for (;;) {
    // For our initial reflow we expect to be given an unconstrained width
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE == aState.availSize.width, "bad size");

    // Get the next content object
    nsIContent* cell = mContent->ChildAt(kidIndex);
    if (nsnull == cell) {
      break;  // no more content
    }

    // Create a child frame -- always an nsTableCell frame
    nsIStyleContext*    kidSC = aPresContext->ResolveStyleContextFor(cell, this, PR_TRUE);
    nsIContentDelegate* kidDel = cell->GetDelegate(aPresContext);
    nsIFrame*           kidFrame;

    nsresult rv = kidDel->CreateFrame(aPresContext, cell, this, kidSC, kidFrame);
    NS_RELEASE(kidDel);
    NS_RELEASE(cell);

    // Get the child's available space. Because we're not splittable always allow
    // the child to be as high as it wants. The default avail width is also
    // unconstrained so we can get the child's max width
    nsSize  kidAvailSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

    // See if there's a width constraint that overrides
    const nsStylePosition* cellPosition = (const nsStylePosition*)
      kidSC->GetStyleData(eStyleStruct_Position);
    if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) 
    {
      kidAvailSize.width = cellPosition->mWidth.GetCoordValue();
    }
    NS_RELEASE(kidSC);

    // Get the child's margins
    nsMargin  margin;
    nscoord   topMargin = 0;

    if (aState.tableFrame->GetCellMarginData((nsTableCellFrame *)kidFrame, margin) == NS_OK)
    {
      topMargin = margin.top;
    }
   
    maxTopMargin = PR_MAX(margin.top, maxTopMargin);
    maxBottomMargin = PR_MAX(margin.bottom, maxBottomMargin);
    
    // Link child frame into the list of children
    if (nsnull != prevKidFrame) {
      prevKidFrame->SetNextSibling(kidFrame);
    } else {
      mFirstChild = kidFrame;  // our first child
      SetFirstContentOffset(kidIndex);
    }
    mChildCount++;

    // Reflow the child. We always want to know the max element size even if the
    // caller doesn't want it, because we need it to compute column widths
    nsReflowMetrics kidSize(&kidMaxElementSize);
    nsReflowState   kidReflowState(kidFrame, aState.reflowState, kidAvailSize,
                                   eReflowReason_Initial);
    nsReflowStatus  status;

    kidFrame->WillReflow(*aPresContext);
    status = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState);
    NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "unexpected child reflow status");

    // Allocate and set the cell layout data
    nsCellLayoutData *kidLayoutData = new nsCellLayoutData((nsTableCellFrame *)kidFrame,
                                                           &kidSize, &kidMaxElementSize);
    ((nsTableCellFrame *)kidFrame)->SetCellLayoutData(kidLayoutData);

    if (gsDebug1) 
    {
        printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                NS_FRAME_IS_COMPLETE(status)?"complete":"NOT complete", 
                kidSize.width, kidSize.height, 
                kidMaxElementSize.width, kidMaxElementSize.height);
    }

    // Place the child
    aState.x += margin.left;
    nsRect kidRect (aState.x, topMargin, kidSize.width, kidSize.height);
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize, &kidMaxElementSize);
    aState.x += margin.right;

    // Move to the next content child
    prevKidFrame = kidFrame;
    kidIndex++;
  }

  SetMaxChildHeight(aState.maxCellHeight, maxTopMargin, maxBottomMargin);  // remember height of tallest child who doesn't have a row span

  // Update the content mapping
  if (nsnull != prevKidFrame) {
    SetLastContentOffset(kidIndex - 1);
#ifdef NS_DEBUG
    NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
    PRInt32 prevKidIndex;
    prevKidFrame->GetContentIndex(prevKidIndex);
    NS_ASSERTION((kidIndex - 1) == prevKidIndex, "bad index");
    PRInt32 len = LengthOf(mFirstChild);
    NS_ASSERTION(len == mChildCount, "bad child count");
#endif
  }

  return NS_FRAME_COMPLETE;
}

// Recover the reflow state to what it should be if aKidFrame is about
// to be reflowed
nsresult nsTableRowFrame::RecoverState(RowReflowState& aState,
                                       nsIFrame*       aKidFrame)
{
  // Walk the list of children looking for aKidFrame
  nsIFrame* prevKidFrame = nsnull;
  for (nsIFrame* frame = mFirstChild; frame != aKidFrame;) {
    PRInt32 rowSpan = ((nsTableCellFrame*)frame)->GetRowSpan();
    if (mMinRowSpan == rowSpan) {
      // XXX This isn't quite right. We also need to check whether the cell
      // has a height property that affects the cell...
      nsSize  desiredSize = ((nsTableCellFrame *)frame)->GetPriorDesiredSize();
      
      // Update maxCellHeight
      if (desiredSize.height > aState.maxCellHeight) {
        aState.maxCellHeight = desiredSize.height;
      }

      // Update maxCellVertHeight
      nsMargin margin;
  
      if (aState.tableFrame->GetCellMarginData((nsTableCellFrame *)frame, margin) == NS_OK)
      {
        nscoord height = desiredSize.height + margin.top + margin.bottom;
        if (height > aState.maxCellVertSpace) {
          aState.maxCellVertSpace = height;
        }
      }
    }

    // XXX We also need to recover the max element size if requested by the
    // caller...
    //
    // We should be using GetReflowMetrics() to get information from the
    // table cell, and that will include the max element size...

    // Remember the frame that precedes aKidFrame
    prevKidFrame = frame;
    frame->GetNextSibling(frame);
  }

  if (nsnull != prevKidFrame) {
    nsRect  rect;

    // Set our running x-offset
    prevKidFrame->GetRect(rect);
    aState.x = rect.XMost();
  }

  return NS_OK;
}

nsresult nsTableRowFrame::IncrementalReflow(nsIPresContext*      aPresContext,
                                            RowReflowState&      aState,
                                            const nsReflowState& aReflowState,
                                            nsSize*              aMaxElementSize)
{
  nsresult  status;

  // XXX Deal with the case where the reflow command is targeted at us
  nsIFrame* target;
  aReflowState.reflowCommand->GetTarget(target);
  if (this == target) {
    NS_NOTYETIMPLEMENTED("unexpected reflow command");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Get the next frame in the reflow chain
  nsIFrame* kidFrame;
  aReflowState.reflowCommand->GetNext(kidFrame);

  // Recover our reflow state
  RecoverState(aState, kidFrame);

  // Get the frame's margins
  nsMargin  kidMargin;
  aState.tableFrame->GetCellMarginData((nsTableCellFrame *)kidFrame,kidMargin);

  // Figure out the amount of available space for the child
  nsSize    kidAvailSize(aState.availSize);
  if (PR_FALSE == aState.unconstrainedHeight) 
  {
    kidAvailSize.height -= kidMargin.top + kidMargin.bottom;
  }

  // Compute the x-origin for the child, taking into account straddlers (cells
  // from prior rows with rowspans > 1)
  nscoord x = ComputeCellXOffset(aState, kidFrame, kidMargin);

  // Compute the cell avail width from the column widths
  nscoord availWidth = ComputeCellAvailWidth(aState, kidFrame);
  kidAvailSize.width = availWidth;

  // Pass along the reflow command. Reflow the child with an unconstrained
  // width and get its maxElementSize
  nsSize          kidMaxElementSize;
  nsReflowMetrics desiredSize(&kidMaxElementSize);
  nsReflowState kidReflowState(kidFrame, aReflowState, kidAvailSize);
  kidFrame->WillReflow(*aPresContext);
  kidFrame->MoveTo(x, kidMargin.top);

  // XXX Unfortunately we need to reflow the child several times.
  // The first time is for the incremental reflow command. We can't pass in
  // a max width of NS_UNCONSTRAINEDSIZE, because the max width must match
  // the width of the previous reflow...
  status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);

  // Now do the regular pass 1 reflow and gather the max width and max element
  // size.
  // XXX It would be nice if we could skip this step and the next step if the
  // column width isn't dependent on the max cell width...
  kidReflowState.reason = eReflowReason_Resize;
  kidReflowState.reflowCommand = nsnull;
  kidReflowState.maxSize.width = NS_UNCONSTRAINEDSIZE;
  status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);

  // Update the cell layout data. Note that we need to do this for both
  // the data the cell maintains AND the data the table maintains...
  nsCellLayoutData *kidLayoutData = ((nsTableCellFrame *)kidFrame)->GetCellLayoutData();

  kidLayoutData->SetDesiredSize(&desiredSize);
  kidLayoutData->SetMaxElementSize(&kidMaxElementSize);

  kidLayoutData = aState.tableFrame->GetCellLayoutData((nsTableCellFrame*)kidFrame);
  kidLayoutData->SetDesiredSize(&desiredSize);
  kidLayoutData->SetMaxElementSize(&kidMaxElementSize);

  // Now reflow the cell again this time constraining the width
  // XXX Ignore for now the possibility that the column width has changed...
  kidReflowState.maxSize.width = availWidth;
  status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);

  // Place the child after taking into account its margin and attributes
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

  nscoord cellWidth = desiredSize.width;
  // begin special Nav4 compatibility code
  if (0==cellWidth)
  {
    PRInt32 cellColIndex = ((nsTableCellFrame *)kidFrame)->GetColIndex();
    cellWidth = aState.tableFrame->GetColumnWidth(cellColIndex);
  }
  // end special Nav4 compatibility code

  // Adjust the running x-offset
  aState.x = x;

  // Now place the child
  nsRect kidRect (x, kidMargin.top, cellWidth, cellHeight);

  PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize,
             &kidMaxElementSize);

  // Now iterate over the remaining cells, and update our max cell
  // height and our running x-offset
  //
  // We don't have to re-position the x-origin of any of the child frames 
  // that follow, because the column width hasn't changed...
  nscoord maxCellTopMargin = 0;
  nscoord maxCellBottomMargin = 0;
  kidFrame->GetNextSibling((nsIFrame*&)kidFrame);
  while (nsnull != kidFrame) {
    PRInt32 rowSpan = ((nsTableCellFrame*)kidFrame)->GetRowSpan();
    nsRect  rect;
    kidFrame->GetRect(rect);
    if (mMinRowSpan == rowSpan) {
      nsSize  desiredSize = ((nsTableCellFrame *)kidFrame)->GetPriorDesiredSize();

      // Update maxCellHeight
      if (desiredSize.height > aState.maxCellHeight) {
        aState.maxCellHeight = desiredSize.height;
      }

      // Update maxCellVertHeight
      nsMargin margin;
  
      if (aState.tableFrame->GetCellMarginData((nsTableCellFrame *)kidFrame, margin) == NS_OK)
      {
        nscoord height = desiredSize.height + margin.top + margin.bottom;
        if (height > aState.maxCellVertSpace) {
          aState.maxCellVertSpace = height;
        }

        // XXX Currently we assume cells have margins, but that isn't correct...
        if (margin.top > maxCellTopMargin) {
          maxCellTopMargin = margin.top;
        }
        if (margin.bottom > maxCellBottomMargin) {
          maxCellBottomMargin = margin.bottom;
        }
      }
    }

    // Set our running x-offset
    aState.x = rect.XMost();

    // Get the next cell frame
    kidFrame->GetNextSibling((nsIFrame*&)kidFrame);
  }

  SetMaxChildHeight(aState.maxCellHeight,maxCellTopMargin, maxCellBottomMargin);
  return status;
}

/** Layout the entire row.
  * This method stacks cells horizontally according to HTML 4.0 rules.
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

  RowReflowState state(aPresContext, aReflowState);
  mContentParent->GetContentParent((nsIFrame*&)(state.tableFrame));

  if (eReflowReason_Incremental == aReflowState.reason) {
    aStatus = IncrementalReflow(aPresContext, state, aReflowState,
                                aDesiredSize.maxElementSize);
  } else {
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
            GetMinRowSpan();
            FixMinCellHeight();
          }
        } else {
          // We were unable to pull-up all the existing frames from the
          // next in flow
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
      }
    }
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
  // Because rows are always complete we should never be asked to create
  // a continuikng frame
  NS_ERROR("Unexpected request");
  return NS_ERROR_NOT_IMPLEMENTED;
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

NS_METHOD
nsTableRowFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kTableRowFrameCID)) {
    *aInstancePtr = (void*) (this);
    return NS_OK;
  }
  return nsContainerFrame::QueryInterface(aIID, aInstancePtr);
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
