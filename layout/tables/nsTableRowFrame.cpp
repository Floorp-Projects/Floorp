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
#include "nsIView.h"
#include "nsIPtr.h"
#include "nsIReflowCommand.h"

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

  // the running x-offset
  nscoord x;

  // Height of tallest cell (excluding cells with rowspan > 1)
  nscoord maxCellHeight;    // just the height of the cell frame
  nscoord maxCellVertSpace; // the maximum MAX(cellheight + topMargin + bottomMargin)
  
  nsTableFrame *tableFrame;
   

  RowReflowState( nsIPresContext*      aPresContext,
                  const nsReflowState& aReflowState,
                  nsTableFrame*        aTableFrame)
    : reflowState(aReflowState)
  {
    availSize.width = reflowState.maxSize.width;
    availSize.height = reflowState.maxSize.height;
    maxCellHeight = 0;
    maxCellVertSpace = 0;
    tableFrame = aTableFrame;
    x=0;
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
 * Called for a resize reflow. Typically because the column widths have
 * changed. Reflows all the existing table cell frames
 */
nsresult nsTableRowFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                       RowReflowState&  aState,
                                       nsReflowMetrics& aDesiredSize)
{
  NS_PRECONDITION(nsnull != mFirstChild, "no children");

  nsSize      kidMaxElementSize;
  PRBool      result = PR_TRUE;
  PRInt32     prevColIndex = -1;       // remember the col index of the previous cell to handle rowspans into this row
  nsSize*     pKidMaxElementSize = (nsnull != aDesiredSize.maxElementSize) ?
                                     &kidMaxElementSize : nsnull;
  nscoord     maxCellTopMargin = 0;
  nscoord     maxCellBottomMargin = 0;

  // Reflow each of our existing cell frames
  for (nsIFrame*  kidFrame = mFirstChild; nsnull != kidFrame; ) {
    nsSize kidAvailSize(aState.availSize);
    if (0>=kidAvailSize.height)
      kidAvailSize.height = 1;      // XXX: HaCk - we don't handle negative heights yet

    nsReflowMetrics         desiredSize(pKidMaxElementSize);
    desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;

    // Get the frame's margins, and compare the top and bottom margin
    // against our current max values
    nsMargin       kidMargin;
    aState.tableFrame->GetCellMarginData((nsTableCellFrame *)kidFrame,kidMargin);
    if (kidMargin.top > maxCellTopMargin)
      maxCellTopMargin = kidMargin.top;
    if (kidMargin.bottom > maxCellBottomMargin)
      maxCellBottomMargin = kidMargin.bottom;
 
    // Figure out the amount of available size for the child (subtract
    // off the top margin we are going to apply to it)
    
    //XXX TROY??? unconstrainedHeight was removed from aState, what should happen here?
    /*
    if (PR_FALSE == aState.unconstrainedHeight) 
    {
      kidAvailSize.height -= kidMargin.top + kidMargin.bottom;
    }
    */

    // left and right margins already taken into account by table layout strategy

    // Compute the x-origin for the child, taking into account straddlers (cells from prior
    // rows with rowspans > 1)
    PRInt32 cellColIndex = ((nsTableCellFrame *)kidFrame)->GetColIndex();
    if (prevColIndex != (cellColIndex-1))
    { // if this cell is not immediately adjacent to the previous cell, factor in missing col info
      for (PRInt32 colIndex=prevColIndex+1; colIndex<cellColIndex; colIndex++)
      {
        aState.x += aState.tableFrame->GetColumnWidth(colIndex);
        aState.x += kidMargin.left + kidMargin.right;
      }
    }
    aState.x += kidMargin.left;

    // at this point, we know the column widths.  
    // so we get the avail width from the known column widths
    PRInt32 cellColSpan = ((nsTableCellFrame *)kidFrame)->GetColSpan();
    nscoord availWidth = 0;
    for (PRInt32 numColSpan=0; numColSpan<cellColSpan; numColSpan++)
    {
      availWidth += aState.tableFrame->GetColumnWidth(cellColIndex+numColSpan);
      if (0<numColSpan)
      {
        availWidth += kidMargin.right;
        if (0!=cellColIndex)
          availWidth += kidMargin.left;
      }
    }

    prevColIndex = cellColIndex + (cellColSpan-1);  // remember the rightmost column this cell spans into


    // If the available width is the same as last time we reflowed the cell,
    // then just use the previous desired size
    //
    // XXX Wouldn't it be cleaner (but slightly less efficient) for the row to
    // just reflow the cell, and have the cell decide whether it could use the
    // cached value rather than having the row make that determination?
    if (availWidth != ((nsTableCellFrame *)kidFrame)->GetPriorAvailWidth())
    {
      // Always let the cell be as high as it wants. We ignore the height that's
      // passed in and always place the entire row. Let the row group decide
      // whether we fit or wehther the entire row is pushed
      nsSize  kidAvailSize(availWidth, NS_UNCONSTRAINEDSIZE);

      // Reflow the child
      kidFrame->WillReflow(*aPresContext);
      kidFrame->MoveTo(aState.x, kidMargin.top);
      kidAvailSize.width = availWidth;
      nsReflowState kidReflowState(kidFrame, aState.reflowState, kidAvailSize,
                                   eReflowReason_Resize);
      nsReflowStatus status = ReflowChild(kidFrame, aPresContext, desiredSize,
                                          kidReflowState);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "unexpected reflow status");

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
      nsSize priorSize = ((nsTableCellFrame *)kidFrame)->GetDesiredSize();
      desiredSize.width = priorSize.width;
      desiredSize.height = priorSize.height;
    }

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

    nscoord cellWidth = desiredSize.width;
    // begin special Nav4 compatibility code
    if (0==cellWidth)
    {
      cellWidth = aState.tableFrame->GetColumnWidth(cellColIndex);
    }
    // end special Nav4 compatibility code

    // Place the child
    nsRect kidRect (aState.x, kidMargin.top, cellWidth, cellHeight);

    PlaceChild(aPresContext, aState, kidFrame, kidRect, aDesiredSize.maxElementSize,
               pKidMaxElementSize);
    aState.x += kidMargin.right;  // add in right margin only after cell has been placed

    // Get the next child
    kidFrame->GetNextSibling(kidFrame);

  }

  SetMaxChildHeight(aState.maxCellHeight,maxCellTopMargin, maxCellBottomMargin);  // remember height of tallest child who doesn't have a row span

  // Return our desired size. Note that our desired width is just whatever width
  // we were given by the row group frame
  aDesiredSize.width = aState.availSize.width;
  aDesiredSize.height = aState.maxCellVertSpace;   

  return NS_OK;
}

/**
 * Called for the initial reflow. Creates each table cell frame, and
 * reflows it to gets its minimum and maximum sizes
 */
nsresult
nsTableRowFrame::InitialReflow(nsIPresContext*  aPresContext,
                               RowReflowState&  aState,
                               nsReflowMetrics& aDesiredSize)
{
  // For our initial reflow we expect to be given an unconstrained width
  NS_PRECONDITION(NS_UNCONSTRAINEDSIZE == aState.availSize.width, "bad size");

  // Place our children, one at a time, until we are out of children
  nsSize    kidMaxElementSize;
  PRInt32   kidIndex = 0;
  nsIFrame* prevKidFrame = nsnull;
  nscoord   maxTopMargin = 0;
  nscoord   maxBottomMargin = 0;
  nscoord   x = 0;
  nsresult  result;

  for (;;) {
    // Get the next content object
    nsIContent* cell = mContent->ChildAt(kidIndex);
    if (nsnull == cell) {
      break;  // no more content
    }

    // Create a child frame -- always an nsTableCell frame
    nsIStyleContext*    kidSC = aPresContext->ResolveStyleContextFor(cell, this, PR_TRUE);
    nsIContentDelegate* kidDel = cell->GetDelegate(aPresContext);
    nsIFrame*           kidFrame;

    nsresult result = kidDel->CreateFrame(aPresContext, cell, this, kidSC, kidFrame);
    NS_RELEASE(kidDel);
    NS_RELEASE(cell);
    if (NS_FAILED(result)) {
      NS_RELEASE(kidSC);
      break;
    }

    // Because we're not splittable always allow the child to be as high as
    // it wants. The default available width is also unconstrained so we can
    // get the child's maximum width
    nsSize  kidAvailSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

    // See if there's a width constraint for the cell
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

    // Reflow the child. We always want back the max element size even if the
    // caller doesn't ask for it, because the table needs it to compute column
    // widths
    nsReflowMetrics kidSize(&kidMaxElementSize);
    nsReflowState   kidReflowState(kidFrame, aState.reflowState, kidAvailSize,
                                   eReflowReason_Initial);
    nsReflowStatus  status;

    kidFrame->WillReflow(*aPresContext);
    status = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState);
    ((nsTableCellFrame *)kidFrame)->SetPass1DesiredSize(kidSize);
    ((nsTableCellFrame *)kidFrame)->SetPass1MaxElementSize(kidMaxElementSize);
    NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "unexpected child reflow status");

    if (gsDebug1) 
    {
        printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                NS_FRAME_IS_COMPLETE(status)?"complete":"NOT complete", 
                kidSize.width, kidSize.height, 
                kidMaxElementSize.width, kidMaxElementSize.height);
    }

    // Place the child
    x += margin.left;
    nsRect kidRect(x, topMargin, kidSize.width, kidSize.height);
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aDesiredSize.maxElementSize,
               &kidMaxElementSize);
    x += kidSize.width + margin.right;

    // Move to the next content child
    prevKidFrame = kidFrame;
    kidIndex++;
  }

  SetMaxChildHeight(aState.maxCellHeight, maxTopMargin, maxBottomMargin);  // remember height of tallest child who doesn't have a row span

  // Return our desired size
  aDesiredSize.width = x;
  aDesiredSize.height = aState.maxCellVertSpace;   

  // Update the content mapping
  if (nsnull != prevKidFrame) {
    SetLastContentOffset(kidIndex - 1);
#ifdef NS_DEBUG
    NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
    PRInt32 len = LengthOf(mFirstChild);
    NS_ASSERTION(len == mChildCount, "bad child count");
#endif
  }

  return result;
}

// Recover the reflow state to what it should be if aKidFrame is about
// to be reflowed
//
// The things in the RowReflowState object we need to restore are:
// - maxCellHeight
// - maxVertCellSpace
nsresult nsTableRowFrame::RecoverState(RowReflowState& aState,
                                       nsIFrame*       aKidFrame)
{
  // Walk the list of children looking for aKidFrame
  nsIFrame* prevKidFrame = nsnull;
  for (nsIFrame* frame = mFirstChild; frame != aKidFrame;) {
    PRInt32 rowSpan = ((nsTableCellFrame*)frame)->GetRowSpan();
    if (mMinRowSpan == rowSpan) {
      nsRect  rect;
      frame->GetRect(rect);

      // Update maxCellHeight
      if (rect.height > aState.maxCellHeight) {
        aState.maxCellHeight = rect.height;
      }

      // Update maxCellVertHeight
      nsMargin margin;
  
      if (aState.tableFrame->GetCellMarginData((nsTableCellFrame *)frame, margin) == NS_OK)
      {
        nscoord height = rect.height + margin.top + margin.bottom;
        if (height > aState.maxCellVertSpace) {
          aState.maxCellVertSpace = height;
        }
      }
    }

    // XXX We also need to recover the max element size...

    // Remember the frame that precedes aKidFrame
    prevKidFrame = frame;
    frame->GetNextSibling(frame);
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

  // Figure out the amount of available space for the child
  nsMargin  kidMargin;
  aState.tableFrame->GetCellMarginData((nsTableCellFrame *)kidFrame,kidMargin);

  // Figure out the amount of available space for the child
  // XXX Shakey...
  nsSize    kidAvailSize(aState.availSize); // XXX: top and bottom margins?

  // Compute the x-origin for the child, taking into account straddlers (cells from prior
  // rows with rowspans > 1)
  nscoord x = kidMargin.left;
  PRInt32 cellColIndex = ((nsTableCellFrame *)kidFrame)->GetColIndex();
  for (PRInt32 colIndex=0; colIndex<cellColIndex; colIndex++)
  {
    x += aState.tableFrame->GetColumnWidth(colIndex);
  }

  // at this point, we know the column widths.  
  // so we get the avail width from the known column widths
  PRInt32 cellStartingCol = ((nsTableCellFrame *)kidFrame)->GetColIndex();
  PRInt32 cellColSpan = ((nsTableCellFrame *)kidFrame)->GetColSpan();
  nscoord availWidth = 0;
  for (PRInt32 numColSpan=0; numColSpan<cellColSpan; numColSpan++)
    availWidth += aState.tableFrame->GetColumnWidth(cellStartingCol+numColSpan);
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

  // Update the cell layout data.
  ((nsTableCellFrame *)kidFrame)->SetPass1DesiredSize(desiredSize);
  ((nsTableCellFrame *)kidFrame)->SetPass1MaxElementSize(kidMaxElementSize);

  // Now reflow the cell again this time constraining the width
  // XXX Ignore for now the possibility that the column width has changed...
  kidReflowState.maxSize.width = availWidth;
  status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);

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

  nscoord cellWidth = desiredSize.width;
  // begin special Nav4 compatibility code
  if (0==cellWidth)
  {
    cellWidth = aState.tableFrame->GetColumnWidth(cellColIndex);
  }
  // end special Nav4 compatibility code

  // Now place the child
  nsRect kidRect (x, kidMargin.top, cellWidth, cellHeight);

  PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize,
             &kidMaxElementSize);

  // Now iterate over the remaining cells, and update our max cell
  // height and our running x-offset
  //
  // We don't have to re-position any of the child frames that follow, because
  // the column width hasn't changed...
  nscoord maxCellTopMargin = 0;
  nscoord maxCellBottomMargin = 0;
  kidFrame->GetNextSibling((nsIFrame*&)kidFrame);
  while (nsnull != kidFrame) {
    PRInt32 rowSpan = ((nsTableCellFrame*)kidFrame)->GetRowSpan();
    nsRect  rect;
    kidFrame->GetRect(rect);
    if (mMinRowSpan == rowSpan) {
      // Update maxCellHeight
      if (rect.height > aState.maxCellHeight) {
        aState.maxCellHeight = rect.height;
      }

      // Update maxCellVertHeight
      nsMargin margin;
  
      if (aState.tableFrame->GetCellMarginData((nsTableCellFrame *)kidFrame, margin) == NS_OK)
      {
        nscoord height = rect.height + margin.top + margin.bottom;
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

  // Initialize 'out' parameters
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }
  aStatus = NS_FRAME_COMPLETE;  // we're never continued

  // Initialize our internal data
  ResetMaxChildHeight();

  // Initialize our automatic state object
  nsTableFrame* tableFrame;
  mContentParent->GetContentParent((nsIFrame*&)tableFrame);
  RowReflowState state(aPresContext, aReflowState, tableFrame);

  // Do the reflow
  nsresult  result;

  switch (aReflowState.reason) {
  case eReflowReason_Initial:
    NS_ASSERTION(nsnull == mFirstChild, "unexpected reflow reason");
    result = InitialReflow(aPresContext, state, aDesiredSize);
    GetMinRowSpan();
    FixMinCellHeight();
    break;

  case eReflowReason_Resize:
    result = ResizeReflow(aPresContext, state, aDesiredSize);
    break;

  case eReflowReason_Incremental:
    result = IncrementalReflow(aPresContext, state, aReflowState,
                               aDesiredSize.maxElementSize);
    break;

  }

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

  return result;
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

