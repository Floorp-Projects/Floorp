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
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "nsIView.h"
#include "nsIPtr.h"
#include "nsIReflowCommand.h"
#include "nsHTMLIIDs.h"
#include "nsIDeviceContext.h"
#include "nsHTMLAtoms.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsDebugIR = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugIR = PR_FALSE;
#endif

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

/* ----------- RowGroupReflowState ---------- */

struct RowGroupReflowState {
  nsIPresContext& mPresContext;  // Our pres context
  const nsHTMLReflowState& reflowState;  // Our reflow state

  // The available size (computed from the parent)
  nsSize availSize;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running y-offset
  nscoord y;

  // Flag used to set maxElementSize to my first row
  PRBool  firstRow;

  // Remember the height of the first row, because it's our maxElementHeight (plus header/footers)
  nscoord firstRowHeight;

  nsTableFrame *tableFrame;

  RowGroupReflowState(nsIPresContext&          aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsTableFrame *           aTableFrame)
    : mPresContext(aPresContext),
      reflowState(aReflowState)
  {
    availSize.width = reflowState.availableWidth;
    availSize.height = reflowState.availableHeight;
    y=0;  // border/padding???
    unconstrainedWidth = PRBool(reflowState.availableWidth == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(reflowState.availableHeight == NS_UNCONSTRAINEDSIZE);
    firstRow = PR_TRUE;
    firstRowHeight=0;
    tableFrame = aTableFrame;
  }

  ~RowGroupReflowState() {
  }
};



/* ----------- nsTableRowGroupFrame ---------- */

nsresult
nsTableRowGroupFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kITableRowGroupIID, NS_ITABLEROWGROUPFRAME_IID);
  if (aIID.Equals(kITableRowGroupIID)) {
    *aInstancePtr = (void*)this;
    return NS_OK;
  } else {
    return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
  }
}

NS_METHOD nsTableRowGroupFrame::GetRowCount(PRInt32 &aCount)
{
  // init out-param
  aCount=0;

  // loop through children, adding one to aCount for every legit row
  nsIFrame *childFrame = mFrames.FirstChild();
  while (PR_TRUE)
  {
    if (nsnull==childFrame)
      break;
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
      aCount++;
    childFrame->GetNextSibling(childFrame);
  }
  return NS_OK;
}

PRInt32 nsTableRowGroupFrame::GetStartRowIndex()
{
  PRInt32 result = -1;
  nsIFrame *childFrame = mFrames.FirstChild();
  while (PR_TRUE)
  {
    if (nsnull==childFrame)
      break;
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      result = ((nsTableRowFrame *)childFrame)->GetRowIndex();
      break;
    }
    childFrame->GetNextSibling(childFrame);
  }
  return result;
}

NS_METHOD nsTableRowGroupFrame::GetMaxColumns(PRInt32 &aMaxColumns) const
{
  aMaxColumns=0;
  // loop through children, remembering the max of the columns in each row
  nsIFrame *childFrame = mFrames.FirstChild();
  while (PR_TRUE)
  {
    if (nsnull==childFrame)
      break;
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      PRInt32 colCount = ((nsTableRowFrame *)childFrame)->GetMaxColumns();
      aMaxColumns = PR_MAX(aMaxColumns, colCount);
    }
    childFrame->GetNextSibling(childFrame);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsTableRowGroupFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                          nsIAtom*        aListName,
                                          nsIFrame*       aChildList)
{
  mFrames.SetFrames(aChildList);
  return NS_OK;
}

NS_METHOD nsTableRowGroupFrame::Paint(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect&        aDirtyRect,
                                      nsFramePaintLayer aWhichLayer)
{

  // for debug...
  /*
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(128,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
  */

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
}

PRIntn
nsTableRowGroupFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

// aDirtyRect is in our coordinate system
// child rect's are also in our coordinate system
/** overloaded method from nsContainerFrame.  The difference is that 
  * we don't want to clip our children, so a cell can do a rowspan
  */
void nsTableRowGroupFrame::PaintChildren(nsIPresContext&      aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect&        aDirtyRect,
                                         nsFramePaintLayer    aWhichLayer)
{
  nsIFrame* kid = mFrames.FirstChild();
  while (nsnull != kid) {
    nsIView *pView;
     
    kid->GetView(pView);
    if (nsnull == pView) {
      PRBool clipState;
      nsRect kidRect;
      kid->GetRect(kidRect);
      nsRect damageArea(aDirtyRect);
      // Translate damage area into kid's coordinate system
      nsRect kidDamageArea(damageArea.x - kidRect.x, damageArea.y - kidRect.y,
                           damageArea.width, damageArea.height);
      aRenderingContext.PushState();
      aRenderingContext.Translate(kidRect.x, kidRect.y);
      kid->Paint(aPresContext, aRenderingContext, kidDamageArea, aWhichLayer);
      if ((eFramePaintLayer_Overlay == aWhichLayer) && GetShowFrameBorders()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
      }
      aRenderingContext.PopState(clipState);
    }
    kid->GetNextSibling(kid);
  }
}

/* we overload this here because rows have children that can span outside of themselves.
 * so the default "get the child rect, see if it contains the event point" action isn't
 * sufficient.  We have to ask the row if it has a child that contains the point.
 */
NS_IMETHODIMP
nsTableRowGroupFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  nsIFrame* kid;
  nsRect kidRect;
  nsPoint tmp;

  FirstChild(nsnull, kid);
  while (nsnull != kid) {
    kid->GetRect(kidRect);
    const nsStyleDisplay *childDisplay;
    kid->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay) {
      if (((nsTableRowFrame*)(kid))->Contains(aPoint)) {
        tmp.MoveTo(aPoint.x - kidRect.x, aPoint.y - kidRect.y);
        return kid->GetFrameForPoint(tmp, aFrame);
      }
    }
    kid->GetNextSibling(kid);
  }
  *aFrame = this;
  return NS_ERROR_FAILURE;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame
void nsTableRowGroupFrame::PlaceChild(nsIPresContext&      aPresContext,
																			RowGroupReflowState& aReflowState,
																			nsIFrame*            aKidFrame,
																			const nsRect&        aKidRect,
																			nsSize*              aMaxElementSize,
																			nsSize&              aKidMaxElementSize)
{
  if (PR_TRUE==gsDebug)
    printf ("rowgroup %p: placing row at %d, %d, %d, %d\n",
            this, aKidRect.x, aKidRect.y, aKidRect.width, aKidRect.height);

  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // Adjust the running y-offset
  aReflowState.y += aKidRect.height;

  // If our height is constrained then update the available height
  if (PR_FALSE == aReflowState.unconstrainedHeight) {
    aReflowState.availSize.height -= aKidRect.height;
  }

  // Update the maximum element size
  if (PR_TRUE==aReflowState.firstRow)
  {
    aReflowState.firstRow = PR_FALSE;
    aReflowState.firstRowHeight = aKidRect.height;
    if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = aKidMaxElementSize.width;
      aMaxElementSize->height = aKidMaxElementSize.height;
    }
  }
  else if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = PR_MAX(aMaxElementSize->width, aKidMaxElementSize.width);
  }
  if (gsDebug && nsnull != aMaxElementSize)
    printf ("rowgroup %p: placing row %p with width = %d and MES= %d\n",
            this, aKidFrame, aKidRect.width, aMaxElementSize->width);
}

/**
 * Reflow the frames we've already created
 *
 * @param   aPresContext presentation context to use
 * @param   aReflowState current inline state
 * @return  true if we successfully reflowed all the mapped children and false
 *            otherwise, e.g. we pushed children to the next in flow
 */
NS_METHOD nsTableRowGroupFrame::ReflowMappedChildren(nsIPresContext&      aPresContext,
                                                     nsHTMLReflowMetrics& aDesiredSize,
                                                     RowGroupReflowState& aReflowState,
                                                     nsReflowStatus&      aStatus,
                                                     nsTableRowFrame *    aStartFrame,
                                                     nsReflowReason       aReason,
                                                     PRBool               aDoSiblings)
{
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: ReflowMappedChildren\n");
  nsIFrame* prevKidFrame = nsnull;
  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aDesiredSize.maxElementSize) ? &kidMaxElementSize : nsnull;
  nsresult  rv = NS_OK;
  nsIFrame*  kidFrame;
  if (nsnull==aStartFrame)
    kidFrame = mFrames.FirstChild();
  else
    kidFrame = aStartFrame;

  PRUint8 borderStyle = aReflowState.tableFrame->GetBorderCollapseStyle();

  for ( ; nsnull != kidFrame; ) 
  {
    nsSize kidAvailSize(aReflowState.availSize);
    if (0>=kidAvailSize.height)
      kidAvailSize.height = 1;      // XXX: HaCk - we don't handle negative heights yet
    nsHTMLReflowMetrics desiredSize(pKidMaxElementSize);
    desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;

    // Reflow the child into the available space
#if 1
    // XXX Give it as much room as it wants. We'll deal with splitting later
    // after we've computed the row heights taking into account cells with
    // row spans...
    kidAvailSize.height = NS_UNCONSTRAINEDSIZE;
#endif
    nsHTMLReflowState kidReflowState(aPresContext, kidFrame, aReflowState.reflowState,
                                     kidAvailSize, aReason);
    if (kidFrame != mFrames.FirstChild()) {
      // If this isn't the first row frame, then we can't be at the top of
      // the page anymore...
      kidReflowState.isTopOfPage = PR_FALSE;
    }

    if ((PR_TRUE==gsDebug) || (PR_TRUE==gsDebugIR))
      printf("%p RG reflowing child %p with avail width = %d, reason = %d\n",
                        this, kidFrame, kidAvailSize.width, aReason);
    rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, aStatus);
    if (gsDebug) printf("%p RG child %p returned desired width = %d\n",
                        this, kidFrame, desiredSize.width);

#if 0
    // Did the child fit?
    if ((kidFrame != mFrames.FirstChild()) &&
        ((kidAvailSize.height <= 0) ||
         (desiredSize.height > kidAvailSize.height)))
    {
      // The child's height is too big to fit at all in our remaining space,
      // and it's not our first child.
      //
      // Note that if the width is too big that's okay and we allow the
      // child to extend horizontally outside of the reflow area
      PushChildren(kidFrame, prevKidFrame);
      aStatus = NS_FRAME_NOT_COMPLETE;
      break;
    }
#endif

    nsRect kidRect (0, aReflowState.y, desiredSize.width, desiredSize.height);
    PlaceChild(aPresContext, aReflowState, kidFrame, kidRect, aDesiredSize.maxElementSize,
               kidMaxElementSize);

    /* if the table has collapsing borders, we need to reset the length of the shared vertical borders
     * for the table and the cells that overlap this row
     */
    if ((eReflowReason_Initial != aReflowState.reflowState.reason) && (NS_STYLE_BORDER_COLLAPSE==borderStyle))
    {
      const nsStyleDisplay *childDisplay;
      kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
      if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
      {
        PRInt32 rowIndex = ((nsTableRowFrame*)kidFrame)->GetRowIndex();
        aReflowState.tableFrame->SetBorderEdgeLength(NS_SIDE_LEFT,
                                                     rowIndex,
                                                     kidRect.height);
        aReflowState.tableFrame->SetBorderEdgeLength(NS_SIDE_RIGHT,
                                                     rowIndex,
                                                     kidRect.height);
        PRInt32 colCount = aReflowState.tableFrame->GetColCount();
        PRInt32 colIndex = 0;
        nsIFrame *cellFrame = aReflowState.tableFrame->GetCellFrameAt(rowIndex, colIndex);
        while (colIndex<colCount)
        {
          if (nsnull!=cellFrame)
          {
            const nsStyleDisplay *cellDisplay;
            cellFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)cellDisplay));
            if (NS_STYLE_DISPLAY_TABLE_CELL == cellDisplay->mDisplay)
            {
              ((nsTableCellFrame *)(cellFrame))->SetBorderEdgeLength(NS_SIDE_LEFT,
                                                                     rowIndex,
                                                                     kidRect.height);
              ((nsTableCellFrame *)(cellFrame))->SetBorderEdgeLength(NS_SIDE_RIGHT,
                                                                     rowIndex,
                                                                     kidRect.height);
            }
          }
          colIndex++;
          cellFrame = aReflowState.tableFrame->GetCellFrameAt(rowIndex, colIndex);
        }
      }
    }


		// Remember where we just were in case we end up pushing children
		prevKidFrame = kidFrame;

#if 0
    /* Row groups should not create continuing frames for rows 
     * unless they absolutely have to!
     * check to see if this is absolutely necessary (with new params from troy)
     * otherwise PushChildren and bail.
     */
    // Special handling for incomplete children
    if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
      // XXX It's good to assume that we might still have room
      // even if the child didn't complete (floaters will want this)
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // No the child isn't complete, and it doesn't have a next in flow so
        // create a continuing frame. This hooks the child into the flow.
        nsIFrame* continuingFrame;
        nsIStyleContext* kidSC;
        kidFrame->GetStyleContext(kidSC);
        kidFrame->CreateContinuingFrame(aPresContext, this, kidSC,
                                        continuingFrame);
        NS_RELEASE(kidSC);

        // Insert the frame. We'll reflow it next pass through the loop
        nsIFrame* nextSib;
         
        kidFrame->GetNextSibling(nextSib);
        continuingFrame->SetNextSibling(nextSib);
        kidFrame->SetNextSibling(continuingFrame);
      }
      // We've used up all of our available space so push the remaining
      // children to the next-in-flow
      nsIFrame* nextSibling;
       
      kidFrame->GetNextSibling(nextSibling);
      if (nsnull != nextSibling) {
        PushChildren(nextSibling, kidFrame);
      }
      break;
    }
#endif

    if (PR_FALSE==aDoSiblings)
      break;

    // Get the next child
    kidFrame->GetNextSibling(kidFrame);
  }

  return rv;
}

/**
 * Try and pull-up frames from our next-in-flow
 *
 * @param   aPresContext presentation context to use
 * @param   aReflowState current inline state
 * @return  NS_FRAME_COMPLETE if there are no frames to pull-up or if we pulled
 *            up all the next-in-flow frames, and NS_FRAME_NOT_COMPLETE otherwise
 */
NS_METHOD nsTableRowGroupFrame::PullUpChildren(nsIPresContext&      aPresContext,
                                               nsHTMLReflowMetrics& aDesiredSize,
                                               RowGroupReflowState& aReflowState,
                                               nsReflowStatus&      aStatus)
{
// XXX if this code is activated then fix the nsRowGroupFrame cast, it might be an nsScrollFrame
  nsTableRowGroupFrame* nextInFlow = (nsTableRowGroupFrame*)mNextInFlow;
  nsSize         kidMaxElementSize;
  nsSize*        pKidMaxElementSize = (nsnull != aDesiredSize.maxElementSize) ? &kidMaxElementSize : nsnull;
  nsIFrame*      prevKidFrame = mFrames.LastChild();
  nsresult       rv = NS_OK;

  aStatus = NS_FRAME_COMPLETE;
  while (nsnull != nextInFlow) {
    nsHTMLReflowMetrics kidSize(pKidMaxElementSize);
    kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;

    // Get the next child
    nsIFrame* kidFrame = nextInFlow->mFrames.FirstChild();

    // Any more row frames?
    if (nsnull == kidFrame) {
      // No. Any frames on its overflow list?
      if (nextInFlow->mOverflowFrames.NotEmpty()) {
        // Move the overflow list to become the child list
        nextInFlow->AppendChildren(nextInFlow->mOverflowFrames.FirstChild(), PR_TRUE);
        nextInFlow->mOverflowFrames.SetFrames(nsnull);
        kidFrame = nextInFlow->mFrames.FirstChild();
      } else {
        // We've pulled up all the children, so move to the next-in-flow.
        nextInFlow->GetNextInFlow((nsIFrame*&)nextInFlow);
        continue;
      }
    }

    // See if the child fits in the available space. If it fits or
    // it's splittable then reflow it
    nsSize            kidFrameSize;
    nsSplittableType  kidIsSplittable;

    kidFrame->GetSize(kidFrameSize);
    kidFrame->IsSplittable(kidIsSplittable);
    if ((kidFrameSize.height > aReflowState.availSize.height) &&
        NS_FRAME_IS_NOT_SPLITTABLE(kidIsSplittable)) {
      aStatus = NS_FRAME_NOT_COMPLETE;
      break;
    }
    nsHTMLReflowState kidReflowState(aPresContext, kidFrame,
                                     aReflowState.reflowState, aReflowState.availSize,
                                     eReflowReason_Resize);

    rv = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, aStatus);

    // Did the child fit?
    if (kidSize.height > aReflowState.availSize.height) {
      aStatus = NS_FRAME_NOT_COMPLETE;  // the child doesn't fit
      break;
    }

    // Place the child
    nsRect kidRect (0, 0, kidSize.width, kidSize.height);
    kidRect.y += aReflowState.y;
    PlaceChild(aPresContext, aReflowState, kidFrame, kidRect, aDesiredSize.maxElementSize, *pKidMaxElementSize);

    // Remove the frame from its current parent
    // XXX We need to do this BEFORE we reflow the child; otherwise we're reflowing
    // it without its geometric parent set properly...
    nextInFlow->mFrames.RemoveFirstChild();

    // Link the frame into our list of children
    kidFrame->SetParent(this);

    if (nsnull == prevKidFrame) {
      mFrames.SetFrames(kidFrame);
    } else {
      prevKidFrame->SetNextSibling(kidFrame);
    }
    kidFrame->SetNextSibling(nsnull);

    // Remember where we just were in case we end up pushing children
    prevKidFrame = kidFrame;

    // Is the child we just pulled up complete?
    if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
      // No the child isn't complete
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a
        // continuing frame. The creation appends it to the flow and
        // prepares it for reflow.
        nsIFrame* continuingFrame;
        nsIStyleContext* kidSC;
        kidFrame->GetStyleContext(kidSC);
        kidFrame->CreateContinuingFrame(aPresContext, this, kidSC,
                                        continuingFrame);
        NS_RELEASE(kidSC);
        NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

        // Add the continuing frame to our sibling list and then push
        // it to the next-in-flow. This ensures the next-in-flow's
        // content offsets and child count are set properly. Note that
        // we can safely assume that the continuation is complete so
        // we pass PR_TRUE into PushChidren
        kidFrame->SetNextSibling(continuingFrame);

        PushChildren(continuingFrame, kidFrame);
      }

      // If the child isn't complete then it means that we've used up
      // all of our available space.
      aStatus = NS_FRAME_NOT_COMPLETE;
      break;
    }
  }

  return rv;
}

/* CalculateRowHeights provides default heights for all rows in the rowgroup.
 * Actual row heights are ultimately determined by the table, when the table
 * height attribute is factored in.
 */
void nsTableRowGroupFrame::CalculateRowHeights(nsIPresContext& aPresContext, 
                                               nsHTMLReflowMetrics& aDesiredSize,
                                               const nsHTMLReflowState& aReflowState)
{
  if (gsDebug) printf("TRGF CalculateRowHeights begin\n");
  // iterate children and for each row get its height
  PRBool atLeastOneRowSpanningCell = PR_FALSE;
  PRInt32 numRows;
  GetRowCount(numRows);
  PRInt32 *rowHeights = new PRInt32[numRows];
  nsCRT::memset (rowHeights, 0, numRows*sizeof(PRInt32));

  /* Step 1:  get the height of the tallest cell in the row and save it for
   *          pass 2
   */
  nsIFrame* rowFrame = mFrames.FirstChild();
  PRInt32 rowIndex = 0;
  while (nsnull != rowFrame)
  {
    const nsStyleDisplay *childDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if ((NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay) &&
        (NS_STYLE_VISIBILITY_COLLAPSE != childDisplay->mVisible))
    {
      // get the height of the tallest cell in the row (excluding cells that span rows)
      nscoord maxCellHeight       = ((nsTableRowFrame*)rowFrame)->GetTallestChild();
      nscoord maxCellTopMargin    = ((nsTableRowFrame*)rowFrame)->GetChildMaxTopMargin();
      nscoord maxCellBottomMargin = ((nsTableRowFrame*)rowFrame)->GetChildMaxBottomMargin();
      nscoord maxRowHeight = maxCellHeight + maxCellTopMargin + maxCellBottomMargin;
      if (gsDebug) printf("TRGF CalcRowH: for row %d(%p), maxCellH=%d, maxCTopMargin=%d, maxCBM=%d\n",
                            rowIndex, rowFrame, maxCellHeight, maxCellTopMargin, maxCellBottomMargin);
      if (gsDebug) printf("  rowHeight=%d\n", maxRowHeight);

      // save the row height for pass 2 below
      rowHeights[rowIndex] = maxRowHeight;
      // Update top and bottom inner margin if applicable
      rowIndex++;
    }
    // Get the next row
    rowFrame->GetNextSibling(rowFrame);
    }

  /* Step 2:  Now account for cells that span rows.
   *          A spanning cell's height is the sum of the heights of the rows it spans,
   *          or it's own desired height, whichever is greater.
   *          If the cell's desired height is the larger value, resize the rows and contained
   *          cells by an equal percentage of the additional space.
   *          We go through this loop twice.  The first time, we are adjusting cell heights
   *          and row y-offsets on the fly.
   *          The second time through the loop, we're ensuring that subsequent row-spanning cells
   *          didn't change prior calculations.  
   *          Since we are guaranteed to have found the max height spanners the first time through, 
   *          we know we only need two passes, not an arbitrary number.
   */
  /* TODO
   * 1. optimization, if (PR_TRUE==atLeastOneRowSpanningCell) ... otherwise skip this step entirely
   *    we can get this info trivially from the cell map
   */
  nsTableFrame *tableFrame=nsnull;
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (NS_FAILED(rv) || nsnull==tableFrame)
    return;
  PRInt32 rowGroupHeight;
  for (PRInt32 counter=0; counter<2; counter++)
  {
    rowGroupHeight = 0;
    rowFrame = mFrames.FirstChild();
    rowIndex = 0;
    while (nsnull != rowFrame)
    {
      const nsStyleDisplay *childDisplay;
      rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
      if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
      {
        if (gsDebug) printf("TRGF CalcRowH: Step 2 for row %d (%p)...\n", rowIndex, rowFrame);
        // check this row for a cell with rowspans
        nsIFrame *cellFrame;
        rowFrame->FirstChild(nsnull, cellFrame);
        while (nsnull != cellFrame)
        {
          const nsStyleDisplay *childDisplay;
          cellFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
          if (NS_STYLE_DISPLAY_TABLE_CELL == childDisplay->mDisplay)
          {
            if (gsDebug) printf("TRGF CalcRowH:   for cell %p...\n", cellFrame);
            PRInt32 rowSpan = tableFrame->GetEffectiveRowSpan(rowIndex,(nsTableCellFrame*)cellFrame);
            if (rowSpan > 1)
            { // found a cell with rowspan > 1, determine its height
              if (gsDebug) printf("TRGF CalcRowH:   cell %p has rowspan=%d\n", cellFrame, rowSpan);
              nscoord heightOfRowsSpanned = 0;
              PRInt32 i;
              for ( i = 0; i < rowSpan; i++)
                heightOfRowsSpanned += rowHeights[rowIndex + i];
              if (gsDebug) printf("TRGF CalcRowH:   heightOfRowsSpanned=%d\n", heightOfRowsSpanned);
        
              /* if the cell height fits in the rows, expand the spanning cell's height and slap it in */
              nsSize  cellFrameSize;
              cellFrame->GetSize(cellFrameSize);
              if (heightOfRowsSpanned > cellFrameSize.height)
              {
                if (gsDebug) printf("TRGF CalcRowH:   spanning cell fits in rows spanned, had h=%d, expanded to %d\n", 
                                    cellFrameSize.height, heightOfRowsSpanned);
                cellFrame->SizeTo(cellFrameSize.width, heightOfRowsSpanned);
                // Realign cell content based on new height
                ((nsTableCellFrame*)cellFrame)->VerticallyAlignChild();
              }
              /* otherwise, distribute the excess height to the rows effected.
               * push all subsequent rows down by the total change in height of all the rows above it
               */
              else
              {
                PRInt32 excessHeight = cellFrameSize.height - heightOfRowsSpanned;
                if (gsDebug) printf("TRGF CalcRowH:   excessHeight=%d\n", excessHeight);
                // for every row starting at the row with the spanning cell...
                nsTableRowFrame *rowFrameToBeResized = (nsTableRowFrame *)rowFrame;
                PRInt32 *excessForRow = new PRInt32[numRows];
                nsCRT::memset (excessForRow, 0, numRows*sizeof(PRInt32));
                for (i = rowIndex; i < numRows; i++)
                {
                  if (gsDebug) printf("TRGF CalcRowH:     for row index=%d\n", i);
                  // if the row is within the spanned range, resize the row
                  if (i < (rowIndex + rowSpan))
                  {
                    float percent = ((float)rowHeights[i]) / ((float)heightOfRowsSpanned);
                    excessForRow[i] = NSToCoordRound(((float)(excessHeight)) * percent); 
                    if (gsDebug) printf("TRGF CalcRowH:   for row %d, excessHeight=%d from percent %f\n", 
                                        i, excessForRow[i], percent);
                    // update the row height
                    rowHeights[i] += excessForRow[i];

                    // adjust the height of the row
                    nsSize  rowFrameSize;
                    rowFrameToBeResized->GetSize(rowFrameSize);
                    rowFrameToBeResized->SizeTo(rowFrameSize.width, rowHeights[i]);
                    if (gsDebug) printf("TRGF CalcRowH:     row %d (%p) sized to %d\n", 
                                        i, rowFrameToBeResized, rowHeights[i]);
                  }

                  // if we're dealing with a row below the row containing the spanning cell, 
                  // push that row down by the amount we've expanded the cell heights by
                  if ((i >= rowIndex) && (i != 0))
                  {
                    nsRect rowRect;
               
                    rowFrameToBeResized->GetRect(rowRect);
                    nscoord delta=0;
                    for (PRInt32 j=0; j<i; j++)
                      delta += excessForRow[j];
                    if (delta > excessHeight)
                      delta = excessHeight;
                    rowFrameToBeResized->MoveTo(rowRect.x, rowRect.y + delta);
                    if (gsDebug) printf("TRGF CalcRowH:     row %d (%p) moved to %d after delta %d\n", 
                                         i, rowFrameToBeResized, rowRect.y + delta, delta);
                  }
                  // Get the next row frame
                  rowFrameToBeResized->GetNextSibling((nsIFrame*&)rowFrameToBeResized);
                }
                delete []excessForRow;
              }
            }
          }
          // Get the next row child (cell frame)
          cellFrame->GetNextSibling(cellFrame);
        }
        // Update the running row group height
        rowGroupHeight += rowHeights[rowIndex];
        rowIndex++;
      }
      // Get the next rowgroup child (row frame)
      rowFrame->GetNextSibling(rowFrame);
    }
  }

  /* step 3: finally, notify the rows of their new heights */
  rowFrame = mFrames.FirstChild();
  while (nsnull != rowFrame)
  {
    const nsStyleDisplay *childDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      ((nsTableRowFrame *)rowFrame)->DidResize(aPresContext, aReflowState);
    }
    // Get the next row
    rowFrame->GetNextSibling(rowFrame);
  }

  // Adjust our desired size
  aDesiredSize.height = rowGroupHeight;

  // cleanup
  delete []rowHeights;
}

nsresult nsTableRowGroupFrame::AdjustSiblingsAfterReflow(nsIPresContext&      aPresContext,
                                                         RowGroupReflowState& aReflowState,
                                                         nsIFrame*            aKidFrame,
                                                         nscoord              aDeltaY)
{
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: AdjustSiblingsAfterReflow\n");
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
      nsIHTMLReflow* htmlReflow;
      if (NS_OK == kidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
        htmlReflow->WillReflow(aPresContext);
      }
      kidFrame->MoveTo(origin.x, origin.y);

      // Get the next frame
      lastKidFrame = kidFrame;
      kidFrame->GetNextSibling(kidFrame);
    }

  } else {
    // Get the last frame
    lastKidFrame = mFrames.LastChild();
  }

  // XXX Deal with cells that have rowspans.

  // Update our running y-offset to reflect the bottommost child
  nsRect  rect;
  lastKidFrame->GetRect(rect);
  aReflowState.y = rect.YMost();

  return NS_OK;
}

nsresult
nsTableRowGroupFrame::SplitRowGroup(nsIPresContext&          aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aReflowState,
                                    nsReflowStatus&          aStatus)
{
  nsIFrame* prevKidFrame = nsnull;
  nsresult  rv = NS_OK;

  // Walk each of the row frames looking for the first row frame that
  // doesn't fit in the available space
  for (nsIFrame* kidFrame = mFrames.FirstChild(); nsnull != kidFrame; kidFrame->GetNextSibling(kidFrame)) {
    nsRect  bounds;

    kidFrame->GetRect(bounds);
    // XXX This check isn't correct if there's a footer...
    if (bounds.YMost() > aReflowState.availableHeight) {
      // If this is the first row frame then we need to split it
      if (nsnull == prevKidFrame) {
        // Reflow the row in the available space and have it split
        nsSize  kidAvailSize(aReflowState.availableWidth,
                             aReflowState.availableHeight - bounds.y);
        nsHTMLReflowState   kidReflowState(aPresContext, kidFrame, aReflowState,
                                           kidAvailSize, eReflowReason_Resize);
        nsHTMLReflowMetrics desiredSize(nsnull);

        rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, aStatus);
        kidFrame->SizeTo(desiredSize.width, desiredSize.height);
        ((nsTableRowFrame *)kidFrame)->DidResize(aPresContext, aReflowState);
        aDesiredSize.height = desiredSize.height;

        if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
          // Create a continuing frame, add it to the child list, and then push it
          // and the frames that follow
          // XXX Check whether it already has a next-in-flow
          nsIFrame*        continuingFrame;
          nsIStyleContext* kidSC;
  
          kidFrame->GetStyleContext(kidSC);
          kidFrame->CreateContinuingFrame(aPresContext, this, kidSC, continuingFrame);
          NS_RELEASE(kidSC);
  
          // Add it to the child list
          nsIFrame* nextSibling;
  
          kidFrame->GetNextSibling(nextSibling);
          continuingFrame->SetNextSibling(nextSibling);
          kidFrame->SetNextSibling(continuingFrame);
  
          // Push it and the frames that follow
          PushChildren(continuingFrame, kidFrame);
        }

      } else {
        // See whether the row frame has cells that span into it or across it
        PushChildren(kidFrame, prevKidFrame);
        aDesiredSize.height = bounds.y;
      }

      aStatus = NS_FRAME_NOT_COMPLETE;
      break;
    }

    prevKidFrame = kidFrame;
  }

  return NS_OK;
}

/** Layout the entire row group.
  * This method stacks rows vertically according to HTML 4.0 rules.
  * Rows are responsible for layout of their children.
  */
NS_METHOD
nsTableRowGroupFrame::Reflow(nsIPresContext&          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  nsresult rv=NS_OK;
  if (gsDebug==PR_TRUE)
    printf("nsTableRowGroupFrame::Reflow - aMaxSize = %d, %d\n",
            aReflowState.availableWidth, aReflowState.availableHeight);

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  nsTableFrame *tableFrame=nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (NS_FAILED(rv))
    return rv;
  else if (tableFrame == nsnull)
    return NS_ERROR_NULL_POINTER;

  RowGroupReflowState state(aPresContext, aReflowState, tableFrame);

  if (eReflowReason_Incremental == aReflowState.reason) {
    rv = IncrementalReflow(aPresContext, aDesiredSize, state, aStatus);
  } else {
    PRBool reflowMappedOK = PR_TRUE;
  
    aStatus = NS_FRAME_COMPLETE;
  
    // Check for an overflow list
    MoveOverflowToChildList();
  
    // Reflow the existing frames
    if (nsnull != mFrames.FirstChild()) {
      rv = ReflowMappedChildren(aPresContext, aDesiredSize, state, aStatus,
                                nsnull, aReflowState.reason, PR_TRUE);
    }
  
    // XXX We need to figure out what to do about this...
#if 0
    // XXX if this code is activated then fix PullUpChildren to not case nsRowGroupFrame, it might be an nsScrollFrame
    // Did we successfully reflow our mapped children?
    if (NS_FRAME_COMPLETE==aStatus) {
      // Try and pull-up some children from a next-in-flow
      rv = PullUpChildren(aPresContext, aDesiredSize, state, aStatus);
    }
#endif
  
    // Return our desired rect
    aDesiredSize.width = aReflowState.availableWidth;
    aDesiredSize.height = state.y;

    // account for scroll bars. XXX needs optimization/caching
    if (nsnull != aDesiredSize.maxElementSize) {
      nsIAtom* pseudoTag;
 
      mStyleContext->GetPseudoType(pseudoTag);
      if (pseudoTag == nsHTMLAtoms::scrolledContentPseudo) {
        nsIFrame* scrollFrame;
        GetParent(scrollFrame);
        const nsStyleDisplay *display;
        scrollFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
        if ((NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) ||
            (NS_STYLE_OVERFLOW_AUTO   == display->mOverflow)) {
          float sbWidth, sbHeight;
          nsIDeviceContext* dc = aPresContext.GetDeviceContext();

          dc->GetScrollBarDimensions(sbWidth, sbHeight);
          NS_RELEASE(dc);
          aDesiredSize.maxElementSize->width += NSToCoordRound(sbWidth);
          // If scrollbars are always visible then add in the hor sb height 
          if (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) {
            aDesiredSize.maxElementSize->height += NSToCoordRound(sbHeight);
          }
        }
      }
      NS_IF_RELEASE(pseudoTag);
    }

    // shrink wrap rows to height of tallest cell in that row
    if (eReflowReason_Initial != aReflowState.reason) {
      CalculateRowHeights(aPresContext, aDesiredSize, aReflowState);
    }

    // See if all the frames fit
    if (aDesiredSize.height > aReflowState.availableHeight) {
      // Nope, find a place to split the row group
      SplitRowGroup(aPresContext, aDesiredSize, aReflowState, aStatus);
    }
  }

  // check the visibility in the final pass. If it is collapse, set our desired size to 0
  if (nsTableFrame::IsFinalPass(aReflowState)) {
    const nsStyleDisplay *display;
    GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
    if (NS_STYLE_VISIBILITY_COLLAPSE == display->mVisible) {
      aDesiredSize.width  = 0;
      aDesiredSize.height = 0;
    }
  }

  if (gsDebug==PR_TRUE) 
  {
    if (nsnull!=aDesiredSize.maxElementSize)
      printf("nsTableRowGroupFrame %p returning: %s with aDesiredSize=%d,%d, aMES=%d,%d\n",
              this, NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
              aDesiredSize.width, aDesiredSize.height,
              aDesiredSize.maxElementSize->width, aDesiredSize.maxElementSize->height);
    else
      printf("nsTableRowGroupFrame %p returning: %s with aDesiredSize=%d,%d, aMES=NSNULL\n", 
             this, NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
             aDesiredSize.width, aDesiredSize.height);
  }
  return rv;
}


NS_METHOD nsTableRowGroupFrame::IncrementalReflow(nsIPresContext& aPresContext,
                                                  nsHTMLReflowMetrics& aDesiredSize,
                                                  RowGroupReflowState& aReflowState,
                                                  nsReflowStatus& aStatus)
{
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: IncrementalReflow\n");
  nsresult  rv = NS_OK;

  // determine if this frame is the target or not
  nsIFrame *target=nsnull;
  rv = aReflowState.reflowState.reflowCommand->GetTarget(target);
  if ((PR_TRUE==NS_SUCCEEDED(rv)) && (nsnull!=target))
  {
    if (this==target)
      rv = IR_TargetIsMe(aPresContext, aDesiredSize, aReflowState, aStatus);
    else
    {
      // Get the next frame in the reflow chain
      nsIFrame* nextFrame;
      aReflowState.reflowState.reflowCommand->GetNext(nextFrame);

      // Recover our reflow state
      //RecoverState(state, nextFrame);
      rv = IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, nextFrame);
    }
  }
  return rv;
}

NS_METHOD nsTableRowGroupFrame::IR_TargetIsMe(nsIPresContext&      aPresContext,
                                              nsHTMLReflowMetrics& aDesiredSize,
                                              RowGroupReflowState& aReflowState,
                                              nsReflowStatus&      aStatus)
{
  nsresult rv = NS_FRAME_COMPLETE;
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowState.reflowCommand->GetType(type);
  nsIFrame *objectFrame;
  aReflowState.reflowState.reflowCommand->GetChildFrame(objectFrame); 
  const nsStyleDisplay *childDisplay=nsnull;
  if (nsnull!=objectFrame)
    objectFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
  if (PR_TRUE==gsDebugIR) printf("TRGF IR: TargetIsMe with type=%d\n", type);
  switch (type)
  {
  case nsIReflowCommand::FrameInserted :
    NS_ASSERTION(nsnull!=objectFrame, "bad objectFrame");
    NS_ASSERTION(nsnull!=childDisplay, "bad childDisplay");
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      rv = IR_RowInserted(aPresContext, aDesiredSize, aReflowState, aStatus, 
                          (nsTableRowFrame *)objectFrame, PR_FALSE);
    }
    else
    {
      rv = AddFrame(aReflowState.reflowState, objectFrame);
    }
    break;
  
  case nsIReflowCommand::FrameAppended :
    NS_ASSERTION(nsnull!=objectFrame, "bad objectFrame");
    NS_ASSERTION(nsnull!=childDisplay, "bad childDisplay");
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      rv = IR_RowAppended(aPresContext, aDesiredSize, aReflowState, aStatus, 
                          (nsTableRowFrame *)objectFrame);
    }
    else
    { // no optimization to be done for Unknown frame types, so just reuse the Inserted method
      rv = AddFrame(aReflowState.reflowState, objectFrame);
    }
    break;

  /*
  case nsIReflowCommand::FrameReplaced :

  */

  case nsIReflowCommand::FrameRemoved :
    NS_ASSERTION(nsnull!=objectFrame, "bad objectFrame");
    NS_ASSERTION(nsnull!=childDisplay, "bad childDisplay");
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      rv = IR_RowRemoved(aPresContext, aDesiredSize, aReflowState, aStatus, 
                         (nsTableRowFrame *)objectFrame);
    }
    else
    {
      rv = RemoveFrame(objectFrame);
    }
    break;

  case nsIReflowCommand::StyleChanged :
    rv = IR_StyleChanged(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case nsIReflowCommand::ContentChanged :
    NS_ASSERTION(PR_FALSE, "illegal reflow type: ContentChanged");
    rv = NS_ERROR_ILLEGAL_VALUE;
    break;
  
  case nsIReflowCommand::PullupReflow:
  case nsIReflowCommand::PushReflow:
  case nsIReflowCommand::CheckPullupReflow :
  case nsIReflowCommand::UserDefined :
    NS_NOTYETIMPLEMENTED("unimplemented reflow command type");
    rv = NS_ERROR_NOT_IMPLEMENTED;
    if (PR_TRUE==gsDebugIR) printf("TRGF IR: reflow command not implemented.\n");
    break;
  }

  return rv;
}

NS_METHOD nsTableRowGroupFrame::IR_RowInserted(nsIPresContext&      aPresContext,
                                               nsHTMLReflowMetrics& aDesiredSize,
                                               RowGroupReflowState& aReflowState,
                                               nsReflowStatus&      aStatus,
                                               nsTableRowFrame *    aInsertedFrame,
                                               PRBool               aReplace)
{
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: IR_RowInserted\n");
  nsresult rv = AddFrame(aReflowState.reflowState, (nsIFrame*)aInsertedFrame);
  if (NS_FAILED(rv))
    return rv;

  if (PR_TRUE==aReflowState.tableFrame->RequiresPass1Layout())
  {
    // do a pass-1 layout of all the cells in the inserted row
    //XXX: check the table frame to see if we can skip this
    rv = ReflowMappedChildren(aPresContext, aDesiredSize, aReflowState, aStatus, 
                              aInsertedFrame, eReflowReason_Initial, PR_FALSE);
    if (NS_FAILED(rv))
      return rv;
  }

  aReflowState.tableFrame->InvalidateCellMap();
  aReflowState.tableFrame->InvalidateColumnCache();

  return rv;
}

NS_METHOD nsTableRowGroupFrame::DidAppendRow(nsTableRowFrame *aRowFrame)
{
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: DidAppendRow\n");
  nsresult rv=NS_OK;
  /* need to make space in the cell map.  Remeber that row spans can't cross row groups
     once the space is made, tell the row to initizalize its children.  
     it will automatically find the row to initialize into.
     but this is tough because a cell in aInsertedFrame could have a rowspan
     which must be respected if a subsequent row is appended.
  */
  rv = aRowFrame->InitChildren();
  return rv;
}

// support method that tells us if there are any rows in the table after our rows
// returns PR_TRUE if there are no rows after ours
PRBool nsTableRowGroupFrame::NoRowsFollow()
{
  PRBool result = PR_TRUE;
  nsIFrame *nextSib=nsnull;
  GetNextSibling(nextSib);
  while (nsnull!=nextSib)
  {
    const nsStyleDisplay *sibDisplay;
    nextSib->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)sibDisplay));
    if ((NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == sibDisplay->mDisplay) ||
        (NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == sibDisplay->mDisplay) ||
        (NS_STYLE_DISPLAY_TABLE_ROW_GROUP    == sibDisplay->mDisplay))
    {
      nsIFrame *childFrame=nsnull;
      nextSib->FirstChild(nsnull, childFrame);
      while (nsnull!=childFrame)
      {
        const nsStyleDisplay *childDisplay;
        childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
        if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
        { // found a row 
          result = PR_FALSE;
          break;
        }
      }
    }
    nextSib->GetNextSibling(nextSib);
  }
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: NoRowsFollow returning %d\n", result);
  return result;
}

// since we know we're doing an append here, we can optimize
NS_METHOD nsTableRowGroupFrame::IR_RowAppended(nsIPresContext&      aPresContext,
                                               nsHTMLReflowMetrics& aDesiredSize,
                                               RowGroupReflowState& aReflowState,
                                               nsReflowStatus&      aStatus,
                                               nsTableRowFrame *    aAppendedFrame)
{
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: IR_RowAppended\n");
  // hook aAppendedFrame into the child list
  nsresult rv = AddFrame(aReflowState.reflowState, (nsIFrame*)aAppendedFrame);
  if (NS_FAILED(rv))
    return rv;

  /* we have 2 paths to choose from.  If we know that aAppendedFrame is
   * the last row in the table, we can optimize.  Otherwise, we have to
   * treat it like an insert
   */
  if (PR_TRUE==NoRowsFollow())
  { // aAppendedRow is the last row, so do the optimized route
    // account for the cells in the row aAppendedFrame
    // this will add the content of the rowgroup to the cell map
    rv = DidAppendRow(aAppendedFrame);
    if (NS_FAILED(rv))
      return rv;

    if (PR_TRUE==aReflowState.tableFrame->RequiresPass1Layout())
    {
      // do a pass1 reflow of the new row
      //XXX: check the table frame to see if we can skip this
      rv = ReflowMappedChildren(aPresContext, aDesiredSize, aReflowState, aStatus, 
                                aAppendedFrame, eReflowReason_Initial, PR_FALSE);
      if (NS_FAILED(rv))
        return rv;
    }

    // if any column widths have to change due to this, rebalance column widths
    //XXX need to calculate this, but for now just do it
    aReflowState.tableFrame->InvalidateColumnWidths();  
  }
  else
  {
    // do a pass1 reflow of the new row
    //XXX: check the table frame to see if we can skip this
    rv = ReflowMappedChildren(aPresContext, aDesiredSize, aReflowState, aStatus, 
                              aAppendedFrame, eReflowReason_Initial, PR_FALSE);
    if (NS_FAILED(rv))
      return rv;

    aReflowState.tableFrame->InvalidateCellMap();
    aReflowState.tableFrame->InvalidateColumnCache();
  }

  return rv;
}

NS_METHOD nsTableRowGroupFrame::IR_RowRemoved(nsIPresContext&      aPresContext,
                                              nsHTMLReflowMetrics& aDesiredSize,
                                              RowGroupReflowState& aReflowState,
                                              nsReflowStatus&      aStatus,
                                              nsTableRowFrame *    aDeletedFrame)
{
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: IR_RowRemoved\n");
  nsresult rv = RemoveFrame((nsIFrame *)aDeletedFrame);
  if (NS_SUCCEEDED(rv))
  {
    aReflowState.tableFrame->InvalidateCellMap();
    aReflowState.tableFrame->InvalidateColumnCache();

    // if any column widths have to change due to this, rebalance column widths
    //XXX need to calculate this, but for now just do it
    aReflowState.tableFrame->InvalidateColumnWidths();
  }

  return rv;
}

NS_METHOD nsTableRowGroupFrame::IR_TargetIsChild(nsIPresContext&      aPresContext,
                                                 nsHTMLReflowMetrics& aDesiredSize,
                                                 RowGroupReflowState& aReflowState,
                                                 nsReflowStatus&      aStatus,
                                                 nsIFrame *           aNextFrame)

{
  nsresult rv;
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: IR_TargetIsChild\n");
  // XXX Recover state

  // Remember the old rect
  nsRect  oldKidRect;
  aNextFrame->GetRect(oldKidRect);

  // Pass along the reflow command
  // XXX Correctly compute the available space...
  nsSize  availSpace(aReflowState.reflowState.availableWidth, aReflowState.reflowState.availableHeight);
  nsHTMLReflowState   kidReflowState(aPresContext, aNextFrame, aReflowState.reflowState, availSpace);
  nsHTMLReflowMetrics desiredSize(nsnull);

  rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState, aStatus);

  // Resize the row frame
  nsRect  kidRect;
  aNextFrame->GetRect(kidRect);
  aNextFrame->SizeTo(desiredSize.width, desiredSize.height);

  // Adjust the frames that follow...
  AdjustSiblingsAfterReflow(aPresContext, aReflowState, aNextFrame, desiredSize.height -
                            oldKidRect.height);

  // Return of desired size
  aDesiredSize.width = aReflowState.reflowState.availableWidth;
  aDesiredSize.height = aReflowState.y;

  return rv;
}

NS_METHOD nsTableRowGroupFrame::IR_StyleChanged(nsIPresContext&      aPresContext,
                                                nsHTMLReflowMetrics& aDesiredSize,
                                                RowGroupReflowState& aReflowState,
                                                nsReflowStatus&      aStatus)
{
  if (PR_TRUE==gsDebugIR) printf("TRGF: IR_StyleChanged for frame %p\n", this);
  nsresult rv = NS_OK;
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet before we were called here
  // XXX: we can optimize this when we know which style attribute changed
  aReflowState.tableFrame->InvalidateFirstPassCache();
  return rv;
}

// XXX this doesn't handle scrolling, it that is even possible in this context
NS_METHOD
nsTableRowGroupFrame::CreateContinuingFrame(nsIPresContext&  aPresContext,
                                            nsIFrame*        aParent,
                                            nsIStyleContext* aStyleContext,
                                            nsIFrame*&       aContinuingFrame)
{
  nsTableRowGroupFrame* cf = new nsTableRowGroupFrame;
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  cf->Init(aPresContext, mContent, aParent, aStyleContext);
  cf->AppendToFlow(this);
  if (PR_TRUE==gsDebug) printf("nsTableRowGroupFrame::CCF parent = %p, this=%p, cf=%p\n", aParent, this, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

/* ----- global methods ----- */

nsresult 
NS_NewTableRowGroupFrame(nsIFrame*& aResult)
{
  nsIFrame* it = new nsTableRowGroupFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = it;
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TableRowGroup", aResult);
}

