/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
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
#include "nsIReflowCommand.h"
#include "nsHTMLIIDs.h"
#include "nsIDeviceContext.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleSet.h"
#include "nsIPresShell.h"
#include "nsLayoutAtoms.h"
#include "nsCSSRendering.h"
#include "nsHTMLParts.h"

#include "nsCellMap.h"//table cell navigation

/* ----------- nsTableRowGroupFrame ---------- */
nsrefcnt nsTableRowGroupFrame::AddRef(void)
{
  return 1;//implementation of nsLineIterator
}

nsrefcnt nsTableRowGroupFrame::Release(void)
{
  return 1;//implementation of nsLineIterator
}


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
  }
  else if (aIID.Equals(NS_GET_IID(nsILineIteratorNavigator))) 
  { // note there is no addref here, frames are not addref'd
    *aInstancePtr = (void*)(nsILineIteratorNavigator*)this;
    return NS_OK;
  }
  else {
    return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
  }
}

NS_METHOD nsTableRowGroupFrame::GetRowCount(PRInt32 &aCount, PRBool aDeepCount)
{
  // init out-param
  aCount=0;

  // loop through children, adding one to aCount for every legit row
  nsIFrame *childFrame = GetFirstFrame();
  while (PR_TRUE)
  {
    if (nsnull==childFrame)
      break;
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
      aCount++;
    else if (aDeepCount && NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay) {
      PRInt32 childRowGroupCount;
      ((nsTableRowGroupFrame*)childFrame)->GetRowCount(childRowGroupCount);
      aCount += childRowGroupCount;
    }
    GetNextFrame(childFrame, &childFrame);
  }
  return NS_OK;
}

PRInt32 nsTableRowGroupFrame::GetStartRowIndex()
{
  PRInt32 result = -1;
  nsIFrame *childFrame = GetFirstFrame();
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
    else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == childDisplay->mDisplay) {
      result = ((nsTableRowGroupFrame*)childFrame)->GetStartRowIndex();
      if (result != -1)
        break;
    }
    
    GetNextFrame(childFrame, &childFrame);
  }
  // if the row group doesn't have any children, get it the hard way
  if (-1 == result) {
    nsTableFrame* tableFrame;
    nsTableFrame::GetTableFrame(this, tableFrame);
    if (tableFrame) {
      return tableFrame->GetStartRowIndex(*this);
    }
  }
      
  return result;
}

nsresult
nsTableRowGroupFrame::InitRepeatedFrame(nsIPresContext*       aPresContext,
                                        nsTableRowGroupFrame* aHeaderFooterFrame)
{
  nsIFrame* originalRowFrame;
  nsIFrame* copyRowFrame = GetFirstFrame();

  aHeaderFooterFrame->FirstChild(aPresContext, nsnull, &originalRowFrame);
  while (copyRowFrame) {
    // Set the row frame index
    int rowIndex = ((nsTableRowFrame*)originalRowFrame)->GetRowIndex();
    ((nsTableRowFrame*)copyRowFrame)->SetRowIndex(rowIndex);

    // For each table cell frame set its column index
    nsIFrame* originalCellFrame;
    nsIFrame* copyCellFrame;
    originalRowFrame->FirstChild(aPresContext, nsnull, &originalCellFrame);
    copyRowFrame->FirstChild(aPresContext, nsnull, &copyCellFrame);
    while (copyCellFrame) {
      nsIAtom*  frameType;
      copyCellFrame->GetFrameType(&frameType);

      if (nsLayoutAtoms::tableCellFrame == frameType) {
  #ifdef NS_DEBUG
        nsIContent* content1;
        nsIContent* content2;
        originalCellFrame->GetContent(&content1);
        copyCellFrame->GetContent(&content2);
        NS_ASSERTION(content1 == content2, "cell frames have different content");
        NS_IF_RELEASE(content1);
        NS_IF_RELEASE(content2);
  #endif
        PRInt32 colIndex;
        ((nsTableCellFrame*)originalCellFrame)->GetColIndex(colIndex);
        ((nsTableCellFrame*)copyCellFrame)->InitCellFrame(colIndex);
      }
      NS_IF_RELEASE(frameType);

      // Move to the next cell frame
      copyCellFrame->GetNextSibling(&copyCellFrame);
      originalCellFrame->GetNextSibling(&originalCellFrame);
    }

    // Move to the next row frame
    GetNextFrame(originalRowFrame, &originalRowFrame);
    GetNextFrame(copyRowFrame, &copyRowFrame);
  }

  return NS_OK;
}

NS_METHOD nsTableRowGroupFrame::Paint(nsIPresContext*      aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect&        aDirtyRect,
                                      nsFramePaintLayer    aWhichLayer)
{
  nsresult rv;
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    nsCompatibility mode;
    aPresContext->GetCompatibilityMode(&mode);
    if (eCompatibility_Standard == mode) {
      const nsStyleDisplay* disp =
        (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
      if (disp->IsVisibleOrCollapsed()) {
        const nsStyleSpacing* spacing =
          (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
        const nsStyleColor* color =
          (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        nsTableFrame* tableFrame = nsnull;
        rv = nsTableFrame::GetTableFrame(this, tableFrame);
        if (NS_FAILED(rv) || (nsnull == tableFrame)) {
          return rv;
        }
        nsRect rect(0,0,mRect.width, mRect.height);
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *spacing, 0, 0);
      }
    }
  }

#ifdef DEBUG
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
  /*nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);*/

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
void nsTableRowGroupFrame::PaintChildren(nsIPresContext*      aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect&        aDirtyRect,
                                         nsFramePaintLayer    aWhichLayer)
{
  nsIFrame* kid = GetFirstFrame();
  while (nsnull != kid) {
    nsIView *pView;
     
    kid->GetView(aPresContext, &pView);
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
#ifdef DEBUG
      if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) &&
          GetShowFrameBorders()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
      }
#endif
      aRenderingContext.PopState(clipState);
    }
    GetNextFrame(kid, &kid);
  }
}

NS_IMETHODIMP
nsTableRowGroupFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame
void nsTableRowGroupFrame::PlaceChild(nsIPresContext*      aPresContext,
																			RowGroupReflowState& aReflowState,
																			nsIFrame*            aKidFrame,
																			nsHTMLReflowMetrics& aDesiredSize,
                                      nscoord              aX,
                                      nscoord              aY,
																			nsSize*              aMaxElementSize,
																			nsSize&              aKidMaxElementSize)
{
  // Place and size the child
  FinishReflowChild(aKidFrame, aPresContext, aDesiredSize, aX, aY, 0);

  // Adjust the running y-offset
  aReflowState.y += aDesiredSize.height;

  // If our height is constrained then update the available height
  if (PR_FALSE == aReflowState.unconstrainedHeight) {
    aReflowState.availSize.height -= aDesiredSize.height;
  }

  // Update the maximum element size
  if (PR_TRUE==aReflowState.firstRow)
  {
    aReflowState.firstRow = PR_FALSE;
    if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = aKidMaxElementSize.width;
      aMaxElementSize->height = aKidMaxElementSize.height;
    }
  }
  else if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = PR_MAX(aMaxElementSize->width, aKidMaxElementSize.width);
  }
}

/**
 * Reflow the frames we've already created
 *
 * @param   aPresContext presentation context to use
 * @param   aReflowState current inline state
 * @return  true if we successfully reflowed all the mapped children and false
 *            otherwise, e.g. we pushed children to the next in flow
 */
NS_METHOD nsTableRowGroupFrame::ReflowMappedChildren(nsIPresContext*      aPresContext,
                                                     nsHTMLReflowMetrics& aDesiredSize,
                                                     RowGroupReflowState& aReflowState,
                                                     nsReflowStatus&      aStatus,
                                                     nsTableRowFrame *    aStartFrame,
                                                     nsReflowReason       aReason,
                                                     PRBool               aDoSiblings,
                                                     PRBool               aDirtyOnly)
{
  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aDesiredSize.maxElementSize) ? &kidMaxElementSize : nsnull;

  nsTableFrame* tableFrame = nsnull;
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (NS_FAILED(rv) || !tableFrame) return rv;

  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  if (!ContinueReflow(nsnull, aPresContext, aReflowState.y, aReflowState.availSize.height))
      return rv;

  nsIFrame*  kidFrame;
  if (nsnull==aStartFrame) {
    kidFrame = GetFirstFrameForReflow(aPresContext);
    ReflowBeforeRowLayout(aPresContext, aDesiredSize, aReflowState, aStatus, aReason);
  }
  else
    kidFrame = aStartFrame;
                   
  PRUint8 borderStyle = aReflowState.tableFrame->GetBorderCollapseStyle();
 
  for ( ; nsnull != kidFrame; ) 
  {
    // Get the frame state bits
    nsFrameState  frameState;
    kidFrame->GetFrameState(&frameState);

    // See if we should only reflow the dirty child frames
    PRBool  doReflowChild = PR_TRUE;
    if (aDirtyOnly) {
      if ((frameState & NS_FRAME_IS_DIRTY) == 0) {
        doReflowChild = PR_FALSE;
      }
    }

    // Reflow the row frame
    if (doReflowChild) {
      nsSize kidAvailSize(aReflowState.availSize);
      if (0>=kidAvailSize.height)
        kidAvailSize.height = 1;      // XXX: HaCk - we don't handle negative heights yet
      nsHTMLReflowMetrics desiredSize(pKidMaxElementSize);
      desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;
  
      // Reflow the child into the available space, giving it as much room as
      // it wants. We'll deal with splitting later after we've computed the row
      // heights, taking into account cells with row spans...
      kidAvailSize.height = NS_UNCONSTRAINEDSIZE;
      nsReflowReason  reason = aReason;
      if (aDirtyOnly) {
        if (frameState & NS_FRAME_FIRST_REFLOW) {
          // Newly inserted frame
          reason = eReflowReason_Initial;
        }
      }
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState, kidFrame,
                                       kidAvailSize, reason);
     
      // If this isn't the first row frame, then we can't be at the top of
      // the page anymore...
      if (kidFrame != GetFirstFrame()) {
        kidReflowState.isTopOfPage = PR_FALSE;
      }

      if (aReflowState.tableFrame->RowGroupsShouldBeConstrained()) {
        // Only applies to the tree widget.
        const nsStyleDisplay *rowDisplay;
        kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
        if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == rowDisplay->mDisplay &&
            aReflowState.availSize.height != NS_UNCONSTRAINEDSIZE) {
          kidReflowState.availableHeight = aReflowState.availSize.height;
        }
      }
  
      rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                       0, aReflowState.y, 0, aStatus);
  
      // Place the child
      nsRect kidRect (0, aReflowState.y, desiredSize.width, desiredSize.height);
      PlaceChild(aPresContext, aReflowState, kidFrame, desiredSize, 0,
                 aReflowState.y, aDesiredSize.maxElementSize, kidMaxElementSize);
  
      /* if the table has collapsing borders, we need to reset the length of the shared vertical borders
       * for the table and the cells that overlap this row
       */
      if ((eReflowReason_Initial != aReflowState.reflowState.reason) && 
          (NS_STYLE_BORDER_COLLAPSE==borderStyle)) {
        const nsStyleDisplay *childDisplay;
        kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
        if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay) {
          PRInt32 rowIndex = ((nsTableRowFrame*)kidFrame)->GetRowIndex();
          aReflowState.tableFrame->SetBorderEdgeLength(NS_SIDE_LEFT,
                                                       rowIndex,
                                                       kidRect.height);
          aReflowState.tableFrame->SetBorderEdgeLength(NS_SIDE_RIGHT,
                                                       rowIndex,
                                                       kidRect.height);
          PRInt32 colCount = aReflowState.tableFrame->GetColCount();
          PRInt32 colIndex = 0;
          nsIFrame *cellFrame;
          for (colIndex = 0; colIndex < colCount; colIndex++) {
            cellFrame = aReflowState.tableFrame->GetCellInfoAt(rowIndex, colIndex);
            if (cellFrame) {
              const nsStyleDisplay *cellDisplay;
              cellFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)cellDisplay));
              if (NS_STYLE_DISPLAY_TABLE_CELL == cellDisplay->mDisplay) {
                ((nsTableCellFrame *)(cellFrame))->SetBorderEdgeLength(NS_SIDE_LEFT,
                                                                       rowIndex,
                                                                       kidRect.height);
                ((nsTableCellFrame *)(cellFrame))->SetBorderEdgeLength(NS_SIDE_RIGHT,
                                                                       rowIndex,
                                                                       kidRect.height);
              }
            }
          }
        }
      }
      aReflowState.y += cellSpacingY;
    } else {
      // Adjust the running y-offset so we know where the next row should
      // be placed
      nsSize  kidSize;

      kidFrame->GetSize(kidSize);
      aReflowState.y += kidSize.height + cellSpacingY;
    }

    if (PR_FALSE==aDoSiblings)
      break;

    if (!ContinueReflow(kidFrame, aPresContext, aReflowState.y, aReflowState.availSize.height))
      break;

    // Get the next child
    GetNextFrameForReflow(aPresContext, kidFrame, &kidFrame);
  }

  // Call our post-row reflow hook
  ReflowAfterRowLayout(aPresContext, aDesiredSize, aReflowState, aStatus, aReason);
  return rv;
}

/**
 * Pull-up all the row frames from our next-in-flow
 */
NS_METHOD nsTableRowGroupFrame::PullUpAllRowFrames(nsIPresContext* aPresContext)
{
  if (mNextInFlow) {
    nsTableRowGroupFrame* nextInFlow = (nsTableRowGroupFrame*)mNextInFlow;
  
    while (nsnull != nextInFlow) {
      // Any frames on the next-in-flow's overflow list?
      nsIFrame* nextOverflowFrames = nextInFlow->GetOverflowFrames(aPresContext,
                                                                   PR_TRUE);
      if (nextOverflowFrames) {
        // Yes, append them to its child list
        nextInFlow->mFrames.AppendFrames(nextInFlow, nextOverflowFrames);
      }

      // Any row frames?
      if (nextInFlow->mFrames.NotEmpty()) {
        // When pushing and pulling frames we need to check for whether any
        // views need to be reparented.
        for (nsIFrame* f = nextInFlow->GetFirstFrame(); f; GetNextFrame(f, &f)) {
          nsHTMLContainerFrame::ReparentFrameView(aPresContext, f, nextInFlow, this);
        }
        // Append them to our child list
        mFrames.AppendFrames(this, nextInFlow->mFrames);
      }

      // Move to the next-in-flow        
      nextInFlow->GetNextInFlow((nsIFrame**)&nextInFlow);
    }
  }

  return NS_OK;
}

void nsTableRowGroupFrame::GetNextRowSibling(nsIFrame** aRowFrame)
{
  if (!*aRowFrame) return;
  GetNextFrame(*aRowFrame, aRowFrame);
  while(*aRowFrame) {
    const nsStyleDisplay *display;
    (*aRowFrame)->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
    if (NS_STYLE_DISPLAY_TABLE_ROW == display->mDisplay) {
      return;
    }
    GetNextFrame(*aRowFrame, aRowFrame);
  }
}

// allocate the height of rows which have no cells originating in them
// except with cells with rowspan > 1. Store the height as negative
// to distinguish them from regular rows.
void 
AllocateSpecialHeight(nsIPresContext* aPresContext,
                      nsTableFrame*   aTableFrame,
                      nsIFrame*       aRowFrame,
                      nscoord&        aHeight)
{
  nsIFrame* cellFrame;
  aRowFrame->FirstChild(aPresContext, nsnull, &cellFrame);
  while (cellFrame) {
    nsCOMPtr<nsIAtom> cellType;
    cellFrame->GetFrameType(getter_AddRefs(cellType));
    if (nsLayoutAtoms::tableCellFrame == cellType.get()) {
      PRInt32 rowSpan = aTableFrame->GetEffectiveRowSpan((nsTableCellFrame&)*cellFrame);
      if (rowSpan > 1) {
        // use a simple average to allocate the special row. This is not exact, 
        // but much better than nothing.
        nsSize cellDesSize = ((nsTableCellFrame*)cellFrame)->GetDesiredSize();
        ((nsTableRowFrame*)aRowFrame)->CalculateCellActualSize(cellFrame, cellDesSize.width, 
                                                               cellDesSize.height, cellDesSize.width);
        PRInt32 propHeight = NSToCoordRound((float)cellDesSize.height / (float)rowSpan);
        // special rows store the largest negative value 
        aHeight = PR_MIN(aHeight, -propHeight); 
      }
    }
    cellFrame->GetNextSibling(&cellFrame);
  }
}

/* CalculateRowHeights provides default heights for all rows in the rowgroup.
 * Actual row heights are ultimately determined by the table, when the table
 * height attribute is factored in.
 */
void nsTableRowGroupFrame::CalculateRowHeights(nsIPresContext*          aPresContext, 
                                               nsHTMLReflowMetrics&     aDesiredSize,
                                               const nsHTMLReflowState& aReflowState)
{
  nsTableFrame* tableFrame = nsnull;
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (NS_FAILED(rv) || nsnull==tableFrame) return;

  // all table cells have the same top and bottom margins, namely cellSpacingY
  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  PRBool  hasRowSpanningCell = PR_FALSE;
  PRInt32 numRows;
  GetRowCount(numRows, PR_FALSE);
  // collect the current height of each row. rows which have 0 height because 
  // they have no cells originating in them without rowspans > 1, are referred to as
  // special rows. The current height of a special row will be a negative number until
  // it comes time to actually resize frames.
  nscoord* rowHeights = nsnull;
  if (numRows > 0) {
    rowHeights = new nscoord[numRows];
    if (!rowHeights) return;
    nsCRT::memset (rowHeights, 0, numRows*sizeof(nscoord));
  } // else - tree row groups need not have rows directly beneath them

  // Step 1:  get the height of the tallest cell in the row and save it for
  //          pass 2. This height is for table cells that originate in this
  //          row and that don't span into the rows that follow
  nsIFrame* rowFrame = GetFirstFrame();
  PRInt32 rowIndex = 0;

  // For row groups that are split across pages, the first row frame won't
  // necessarily be index 0
  PRInt32 startRowIndex = -1;
  while (rowFrame) {
    nsCOMPtr<nsIAtom> frameType;
    rowFrame->GetFrameType(getter_AddRefs(frameType));
    if (nsLayoutAtoms::tableRowFrame == frameType.get()) {
      if (startRowIndex == -1) {
        startRowIndex = ((nsTableRowFrame*)rowFrame)->GetRowIndex();
      }
      // get the height of the tallest cell in the row (excluding cells that span rows)
      rowHeights[rowIndex] = ((nsTableRowFrame*)rowFrame)->GetTallestCell();
      // See if a cell spans into the row. If so we'll have to do step 2
      if (!hasRowSpanningCell) {
        if (tableFrame->RowIsSpannedInto(rowIndex + startRowIndex)) {
          hasRowSpanningCell = PR_TRUE;
        }
      }
      // special rows need to have some values, so they will get allocations 
      // later. If left at 0, they would get nothing.
      if (0 == rowHeights[rowIndex]) {
        AllocateSpecialHeight(aPresContext, tableFrame, rowFrame, rowHeights[rowIndex]);
      }
      rowIndex++;
    }
    GetNextFrame(rowFrame, &rowFrame); // Get the next row
  }

  // Step 2:  Now account for cells that span rows. We only do this if there are cells
  //          that span rows. A spanning cell's height is the sum of the heights of the 
  //          rows it spans, or it's own desired height, whichever is greater.
  //          If the cell's desired height is the larger value, resize the rows and contained
  //          cells by an equal percentage of the additional space.
  //          We go through this loop twice.  The first time, we are adjusting cell heights
  //          on the fly. The second time through the loop, we're ensuring that subsequent 
  //          row-spanning cells didn't change prior calculations. Since we are guaranteed 
  //          to have found the max height spanners the first time through, we know we only 
  //          need two passes, not an arbitrary number.
  if (hasRowSpanningCell) {
    nscoord deltaY = 0;
    for (PRInt32 counter = 0; counter < 2; counter++) {
      rowFrame = GetFirstFrame();
      rowIndex = 0;
      while (rowFrame) {
        nsCOMPtr<nsIAtom> rowType;
        rowFrame->GetFrameType(getter_AddRefs(rowType));
        if (nsLayoutAtoms::tableRowFrame == rowType.get()) {
          // check this row for a cell with rowspans
          nsIFrame* cellFrame;
          rowFrame->FirstChild(aPresContext, nsnull, &cellFrame);
          while (cellFrame) {
            nsCOMPtr<nsIAtom> cellType;
            cellFrame->GetFrameType(getter_AddRefs(cellType));
            if (nsLayoutAtoms::tableCellFrame == cellType.get()) {
              PRInt32 rowSpan = tableFrame->GetEffectiveRowSpan(rowIndex + startRowIndex,
                                                                (nsTableCellFrame&)*cellFrame);
              if (rowSpan > 1) { // found a cell with rowspan > 1, determine the height 
                                 // of the rows it spans
                nscoord heightOfRowsSpanned = 0;
                nscoord cellSpacingOfRowsSpanned = 0;
                PRInt32 spanX;
                PRBool cellsOrigInSpan = PR_FALSE; // do any cells originate in the spanned rows
                for (spanX = 0; spanX < rowSpan; spanX++) {
                  PRInt32 rIndex = rowIndex + spanX;
                  if (rowHeights[rIndex] > 0) {
                    // don't consider negative values of special rows
                    heightOfRowsSpanned += rowHeights[rowIndex + spanX];
                    cellsOrigInSpan = PR_TRUE;
                  }
                  if (0 != spanX) {
                    cellSpacingOfRowsSpanned += cellSpacingY;
                  }
                }
                nscoord availHeightOfRowsSpanned = heightOfRowsSpanned + cellSpacingOfRowsSpanned;
  
                // see if the cell's height fits within the rows it spans. If this is
                // pass 1 then use the cell's desired height and not the current height
                // of its frame. That way this works for incremental reflow, too
                nsSize  cellFrameSize;
                cellFrame->GetSize(cellFrameSize);
                if (0 == counter) {
                  nsSize cellDesSize = ((nsTableCellFrame*)cellFrame)->GetDesiredSize();
                  ((nsTableRowFrame*)rowFrame)->CalculateCellActualSize(cellFrame, cellDesSize.width, 
                                                                        cellDesSize.height, cellDesSize.width);
                  cellFrameSize.height = cellDesSize.height;
                  // see if the cell has 'vertical-align: baseline'
                  if (((nsTableCellFrame*)cellFrame)->HasVerticalAlignBaseline()) {
                    // to ensure that a spanning cell with a long descender doesn't
                    // collide with the next row, we need to take into account the shift
                    // that will be done to align the cell on the baseline of the row.
                    cellFrameSize.height += ((nsTableRowFrame*)rowFrame)->GetMaxCellAscent() 
                                          - ((nsTableCellFrame*)cellFrame)->GetDesiredAscent();
                  }
                }
  
                if (availHeightOfRowsSpanned >= cellFrameSize.height) {
                  // the cell's height fits with the available space of the rows it
                  // spans. Set the cell frame's height
                  cellFrame->SizeTo(aPresContext, cellFrameSize.width, availHeightOfRowsSpanned);
                  // Realign cell content based on new height
                  ((nsTableCellFrame*)cellFrame)->VerticallyAlignChild(aPresContext, aReflowState, ((nsTableRowFrame*)rowFrame)->GetMaxCellAscent());
                }
                else {
                  // the cell's height is larger than the available space of the rows it
                  // spans so distribute the excess height to the rows affected
                  nscoord excessAvail = cellFrameSize.height - availHeightOfRowsSpanned;
                  nscoord excessBasis = excessAvail;
  
                  nsTableRowFrame* rowFrameToBeResized = (nsTableRowFrame *)rowFrame;
                  // iterate every row starting at last row spanned and up to the row with
                  // the spanning cell. do this bottom up so that special rows can get a full
                  // allocation before other rows.
                  PRInt32 startRowIndex = rowIndex + rowSpan - 1;
                  for (PRInt32 rowX = startRowIndex; (rowX >= rowIndex) && (excessAvail > 0); rowX--) {
                    nscoord excessForRow = 0;
                    // special rows gets as much as they can
                    if (rowHeights[rowX] <= 0) {
                      if ((rowX == startRowIndex) || (!cellsOrigInSpan)) {
                        if (0 == rowHeights[rowX]) {
                          // give it all since no cell originates in the row
                          excessForRow = excessBasis;
                        }
                        else { // don't let the allocation excced what it needs
                          excessForRow = (excessBasis > -rowHeights[rowX]) ? -rowHeights[rowX] : excessBasis;
                        }
                        rowHeights[rowX] = excessForRow;
                        excessBasis -= excessForRow;
                        excessAvail -= excessForRow;
                      }
                    }
                    else if (cellsOrigInSpan) { // normal rows
                      // The amount of additional space each normal row gets is based on the
                      // percentage of space it occupies, i.e. they don't all get the
                      // same amount of available space
                      float percent = ((float)rowHeights[rowX]) / ((float)heightOfRowsSpanned);
                    
                      // give rows their percentage, except for the first row which gets
                      // the remainder
                      excessForRow = (rowX == rowIndex) 
                                     ? excessAvail  
                                     : NSToCoordRound(((float)(excessBasis)) * percent);
                      // update the row height
                      rowHeights[rowX] += excessForRow;
                      excessAvail -= excessForRow;
                    }
                    // Get the next row frame
                    GetNextRowSibling((nsIFrame**)&rowFrameToBeResized);
                  }
                  // if excessAvail is > 0 it is because !cellsOrigInSpan and the 
                  // allocation involving special rows couldn't allocate everything. 
                  // just give the remainder to the last row spanned.
                  if (excessAvail > 0) {
                    if (rowHeights[startRowIndex] >= 0) {
                      rowHeights[startRowIndex] += excessAvail;
                    }
                    else {
                      rowHeights[startRowIndex] = excessAvail;
                    }
                  }
                }
              }
            }
            cellFrame->GetNextSibling(&cellFrame); // Get the next row child (cell frame)
          }
  
          // If this is pass 2 then resize the row to its final size and move the
          // row's position if the previous rows have caused a shift
          if (1 == counter) {
            PRBool movedFrame = (deltaY != 0);
            nsRect rowBounds;
  
            // Move the row to the correct position
            rowFrame->GetRect(rowBounds); // added
            rowBounds.y += deltaY;
  
            // Adjust our running delta
            nscoord rowHeight = (rowHeights[rowIndex] > 0) ? rowHeights[rowIndex] : 0;
            deltaY += rowHeight - rowBounds.height;
  
            // Resize the row to its final size and position
            rowBounds.height = rowHeight;
            rowFrame->SetRect(aPresContext, rowBounds);

            if (movedFrame) {
              // Make sure any views are positioned properly
              nsIView* view;
              rowFrame->GetView(aPresContext, &view);
              if (view) {
                nsContainerFrame::PositionFrameView(aPresContext, rowFrame, view);
              } else {
                nsContainerFrame::PositionChildViews(aPresContext, rowFrame);
              }
            }
          }
          
          rowIndex++;
        }
        GetNextFrame(rowFrame, &rowFrame); // Get the next rowgroup child (row frame)
      }
    }
  }

  // step 3: notify the rows of their new heights
  nscoord rowGroupHeight = 0;
  rowFrame = GetFirstFrame();
  rowIndex = 0;
  while (rowFrame) {
    if (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth) {
      nsCOMPtr<nsIAtom> rowType;
      rowFrame->GetFrameType(getter_AddRefs(rowType));
      if (nsLayoutAtoms::tableRowFrame == rowType.get()) {
        // Notify the row of the new size
        ((nsTableRowFrame *)rowFrame)->DidResize(aPresContext, aReflowState);
      }
    }

    // Update the running row group height. The height includes frames that
    // aren't rows as well
    nsSize rowSize;
    rowFrame->GetSize(rowSize);
    rowGroupHeight += rowSize.height;
    if (0 != rowIndex) {
      rowGroupHeight += cellSpacingY;
    }

    GetNextFrame(rowFrame, &rowFrame); // Get the next row
    rowIndex++;
  }

  aDesiredSize.height = rowGroupHeight; // Adjust our desired size
  delete [] rowHeights; // cleanup
}

// Called by IR_TargetIsChild() to adjust the sibling frames that follow
// after an incremental reflow of aKidFrame.
// This function is not used for paginated mode so we don't need to deal
// with continuing frames, and it's only called if aKidFrame has no
// cells that span into it and no cells that span across it. That way
// we don't have to deal with rowspans
nsresult
nsTableRowGroupFrame::AdjustSiblingsAfterReflow(nsIPresContext*      aPresContext,
                                                RowGroupReflowState& aReflowState,
                                                nsIFrame*            aKidFrame,
                                                nsSize*              aMaxElementSize,
                                                nscoord              aDeltaY)
{
  NS_PRECONDITION(NS_UNCONSTRAINEDSIZE == aReflowState.reflowState.availableHeight,
                  "we're not in galley mode");
  nsIFrame* lastKidFrame = aKidFrame;

  // Move the frames that follow aKidFrame by aDeltaY and update the max element
  // size
  nsIFrame* kidFrame;
  for (aKidFrame->GetNextSibling(&kidFrame); kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
    // Update the max element size
    if (aMaxElementSize) {
      const nsStyleDisplay *display;
      aKidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
      if (NS_STYLE_DISPLAY_TABLE_ROW == display->mDisplay) {
        nsSize kidMaxElementSize;
        ((nsTableRowFrame*)kidFrame)->GetMaxElementSize(kidMaxElementSize);
        
        aMaxElementSize->width = PR_MAX(aMaxElementSize->width, kidMaxElementSize.width);
      }
    }

    // Move the frame if we need to
    if (aDeltaY != 0) {
      nsPoint origin;
  
      // Adjust the y-origin
      kidFrame->GetOrigin(origin);
      origin.y += aDeltaY;
  
      kidFrame->MoveTo(aPresContext, origin.x, origin.y);
    }

    // Remember the last frame
    lastKidFrame = kidFrame;
  }

  // Update our running y-offset to reflect the bottommost child
  nsRect  rect;
  lastKidFrame->GetRect(rect);
  aReflowState.y = rect.YMost();

  return NS_OK;
}

nsresult
nsTableRowGroupFrame::SplitRowGroup(nsIPresContext*          aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aReflowState,
                                    nsTableFrame*            aTableFrame,
                                    nsReflowStatus&          aStatus)
{
  nsIFrame* prevRowFrame = nsnull;
  nsresult  rv = NS_OK;

  // Walk each of the row frames looking for the first row frame that
  // doesn't fit in the available space
  for (nsIFrame* rowFrame = GetFirstFrame(); rowFrame; GetNextFrame(rowFrame, &rowFrame)) {
    nsRect  bounds;

    rowFrame->GetRect(bounds);
    if (bounds.YMost() > aReflowState.availableHeight) {
      // If this is the first row frame then we need to split it
      if (!prevRowFrame) {
        // Reflow the row in the available space and have it split
        nsSize              availSize(aReflowState.availableWidth,
                                      aReflowState.availableHeight - bounds.y);
        nsHTMLReflowState   rowReflowState(aPresContext, aReflowState, rowFrame,
                                          availSize, eReflowReason_Resize);
        nsHTMLReflowMetrics desiredSize(nsnull);

        rv = ReflowChild(rowFrame, aPresContext, desiredSize, rowReflowState,
                         0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
        rowFrame->SizeTo(aPresContext, desiredSize.width, desiredSize.height);
        rowFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
        ((nsTableRowFrame *)rowFrame)->DidResize(aPresContext, aReflowState);
        aDesiredSize.height = desiredSize.height;

        if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
#ifdef NS_DEBUG
          // Verify it doesn't already have a next-in-flow. The reason it should
          // not already have a next-in-flow is that ReflowMappedChildren() reflows
          // the row frames with an unconstrained available height
          nsIFrame* nextInFlow;
          rowFrame->GetNextInFlow(&nextInFlow);
          NS_ASSERTION(!nextInFlow, "row frame already has next-in-flow");
#endif
          // Create a continuing frame, add it to the child list, and then push it
          // and the frames that follow
          nsIFrame*        contRowFrame;
          nsIPresShell*    presShell;
          nsIStyleSet*     styleSet;
          
          aPresContext->GetShell(&presShell);
          presShell->GetStyleSet(&styleSet);
          NS_RELEASE(presShell);
          styleSet->CreateContinuingFrame(aPresContext, rowFrame, this, &contRowFrame);
          NS_RELEASE(styleSet);
  
          // Add it to the child list
          nsIFrame* nextRow;
          GetNextFrame(rowFrame, &nextRow);
          contRowFrame->SetNextSibling(nextRow);
          rowFrame->SetNextSibling(contRowFrame);
          
          // Push the continuing row frame and the frames that follow
          PushChildren(aPresContext, contRowFrame, rowFrame);
          aStatus = NS_FRAME_NOT_COMPLETE;

        } else {
          // The row frame is complete. It may be the case that it's minimum
          // height was greater than the available height we gave it
          nsIFrame* nextRowFrame;

          // Push the frame that follows
          GetNextFrame(rowFrame, &nextRowFrame);
          
          if (nextRowFrame) {
            PushChildren(aPresContext, nextRowFrame, rowFrame);
          }
          aStatus = nextRowFrame ? NS_FRAME_NOT_COMPLETE : NS_FRAME_COMPLETE;
        }

      } else {
        // See whether the row frame has cells that span into it
        const nsStyleDisplay *display;
        rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
        if (NS_STYLE_DISPLAY_TABLE_ROW != display->mDisplay) {
          NS_ASSERTION(PR_FALSE, "uh oh, not a row frame...");
        }
        PRInt32           rowIndex = ((nsTableRowFrame*)rowFrame)->GetRowIndex();
        PRInt32           colCount = aTableFrame->GetColCount();
        nsTableCellFrame* prevCellFrame = nsnull;

        for (PRInt32 colIndex = 0; colIndex < colCount; colIndex++) {
          nsTableCellFrame* cellFrame = aTableFrame->GetCellInfoAt(rowIndex, colIndex);
          if (!cellFrame) {
            // There is no cell at this column index. Go check the next column index
            continue;
          }

          // See if the cell frame is really in this row, or whether it's a
          // row span from a previous row
          PRInt32 realRowIndex;
          cellFrame->GetRowIndex(realRowIndex);
          if (realRowIndex == rowIndex) {
            prevCellFrame = cellFrame;
          } else {
            // This cell frame spans into the row we're pushing, so we need to:
            // - reflow the cell frame into the new smaller space
            // - create a continuing frame for the cell frame (we do this regardless
            //   of whether it's complete)
            // - add the continuing frame to the row frame we're pushing
            nsIFrame*         parentFrame;
            nsPoint           firstRowOrigin, lastRowOrigin;
            nsReflowStatus    status;

            // Ask the cell frame's parent to reflow it to the height of all the
            // rows it spans between its parent frame and the row we're pushing
            cellFrame->GetParent(&parentFrame);
            parentFrame->GetOrigin(firstRowOrigin);
            rowFrame->GetOrigin(lastRowOrigin);
            ((nsTableRowFrame*)parentFrame)->ReflowCellFrame(aPresContext,
              aReflowState, cellFrame, lastRowOrigin.y - firstRowOrigin.y, status);
            
            // Create the continuing cell frame
            nsIFrame*         contCellFrame;
            nsIPresShell*     presShell;
            nsIStyleSet*      styleSet;

            aPresContext->GetShell(&presShell);
            presShell->GetStyleSet(&styleSet);
            NS_RELEASE(presShell);
            styleSet->CreateContinuingFrame(aPresContext, cellFrame, rowFrame, &contCellFrame);
            NS_RELEASE(styleSet);

            // Add it to the row's child list
            ((nsTableRowFrame*)rowFrame)->InsertCellFrame((nsTableCellFrame*)contCellFrame,
                                                          prevCellFrame);
            prevCellFrame = (nsTableCellFrame*)contCellFrame;
          }
        }

        // Push this row frame and those that follow to the next-in-flow
        PushChildren(aPresContext, rowFrame, prevRowFrame);
        aStatus = NS_FRAME_NOT_COMPLETE;
        aDesiredSize.height = bounds.y;
      }

      break;
    }

    prevRowFrame = rowFrame;
  }

  return NS_OK;
}

/** Layout the entire row group.
  * This method stacks rows vertically according to HTML 4.0 rules.
  * Rows are responsible for layout of their children.
  */
NS_METHOD
nsTableRowGroupFrame::Reflow(nsIPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableRowGroupFrame", aReflowState.reason);
  if (nsDebugTable::gRflRowGrp) nsTableFrame::DebugReflow("TRG::Rfl", this, &aReflowState, nsnull);
  nsresult rv=NS_OK;

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
    aStatus = NS_FRAME_COMPLETE;
  
    // Check for an overflow list
    MoveOverflowToChildList(aPresContext);
  
    // Reflow the existing frames. Before we do, pull-up any row frames from
    // our next-in-flow.
    // XXX It would be more efficient to do this if we have room left after
    // reflowing the frames we have, the problem is we don't know if we have
    // room left until after we call CalculateRowHeights()...
    PullUpAllRowFrames(aPresContext);
    rv = ReflowMappedChildren(aPresContext, aDesiredSize, state, aStatus,
                                nsnull, aReflowState.reason, PR_TRUE);
  
    // Return our desired rect
    aDesiredSize.width = aReflowState.availableWidth;
    aDesiredSize.height = state.y;

    // account for scroll bars. XXX needs optimization/caching
    if (nsnull != aDesiredSize.maxElementSize) {
      nsIAtom* pseudoTag;
 
      mStyleContext->GetPseudoType(pseudoTag);
      if (pseudoTag == nsLayoutAtoms::scrolledContentPseudo) {
        nsIFrame* scrollFrame;
        GetParent(&scrollFrame);
        const nsStyleDisplay *display;
        scrollFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
        if ((NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) ||
            (NS_STYLE_OVERFLOW_AUTO   == display->mOverflow)) {
          float sbWidth, sbHeight;
          nsCOMPtr<nsIDeviceContext> dc;
          aPresContext->GetDeviceContext(getter_AddRefs(dc));

          dc->GetScrollBarDimensions(sbWidth, sbHeight);
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
    PRBool  isTableUnconstrainedReflow = NS_UNCONSTRAINEDSIZE ==
                                         aReflowState.parentReflowState->availableWidth;

    // Skip this step if possible. We can skip it if the table is going to be
    // doing a pass 2 reflow. In the case where the table is getting an unconstrained
    // reflow, then we need to do this because the table will skip the pass 2 reflow,
    // but we need to correctly calculate the row group height and we can't if there
    // are row spans unless we do this step
    if ((eReflowReason_Initial != aReflowState.reason) || isTableUnconstrainedReflow) {
      CalculateRowHeights(aPresContext, aDesiredSize, aReflowState);
    }

    // See if all the frames fit
    if (aDesiredSize.height > aReflowState.availableHeight && 
        !tableFrame->RowGroupsShouldBeConstrained()) {
      // Nope, find a place to split the row group
      SplitRowGroup(aPresContext, aDesiredSize, aReflowState, tableFrame, aStatus);
    }
  }

  // If we computed our max element element size, then cache it so we can return
  // it later when asked
  if (aDesiredSize.maxElementSize) {
    mMaxElementSize = *aDesiredSize.maxElementSize;
  }
  
  if (nsDebugTable::gRflRowGrp) nsTableFrame::DebugReflow("TRG::Rfl ex", this, nsnull, &aDesiredSize, aStatus);
  return rv;
}


NS_METHOD nsTableRowGroupFrame::IncrementalReflow(nsIPresContext* aPresContext,
                                                  nsHTMLReflowMetrics& aDesiredSize,
                                                  RowGroupReflowState& aReflowState,
                                                  nsReflowStatus& aStatus)
{
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

      rv = IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, nextFrame);
    }
  }
  return rv;
}

// Helper function. It marks the table frame as dirty and generates
// a reflow command
nsresult
nsTableRowGroupFrame::AddTableDirtyReflowCommand(nsIPresContext* aPresContext,
                                                 nsIPresShell&   aPresShell,
                                                 nsIFrame*       aTableFrame)
{
  nsFrameState      frameState;
  nsIFrame*         tableParentFrame;
  nsIReflowCommand* reflowCmd;
  nsresult          rv;

  // Mark the table frame as dirty
  aTableFrame->GetFrameState(&frameState);
  frameState |= NS_FRAME_IS_DIRTY;
  aTableFrame->SetFrameState(frameState);

  // Target the reflow comamnd at its parent frame
  aTableFrame->GetParent(&tableParentFrame);
  rv = NS_NewHTMLReflowCommand(&reflowCmd, tableParentFrame,
                               nsIReflowCommand::ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    // Add the reflow command
    rv = aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }

  return rv;
}

#if 0
      // Reflow the new frames. They're already marked dirty, so generate a reflow
      // command that tells us to reflow our dirty child frames
      nsIReflowCommand* reflowCmd;

      if (NS_SUCCEEDED(NS_NewHTMLReflowCommand(&reflowCmd, this,
                                               nsIReflowCommand::ReflowDirty))) {
        aPresShell.AppendReflowCommand(reflowCmd);
        NS_RELEASE(reflowCmd);
      }
#endif

// this does not get called for trees
NS_IMETHODIMP
nsTableRowGroupFrame::AppendFrames(nsIPresContext* aPresContext,
                                   nsIPresShell&   aPresShell,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aFrameList)
{
  // collect the new row frames in an array
  nsVoidArray rows;
  for (nsIFrame* rowFrame = aFrameList; rowFrame; rowFrame->GetNextSibling(&rowFrame)) {
    nsIAtom* frameType;
    rowFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::tableRowFrame == frameType) {
      rows.AppendElement(rowFrame);
    }
    NS_IF_RELEASE(frameType);
  }

  PRInt32 rowIndex;
  GetRowCount(rowIndex);
  // Append the frames to the sibling chain
  mFrames.AppendFrames(nsnull, aFrameList);

  if (rows.Count() > 0) {
    nsTableFrame* tableFrame = nsnull;
    nsTableFrame::GetTableFrame(this, tableFrame);
    if (tableFrame) {
      tableFrame->AppendRows(*aPresContext, *this, rowIndex, rows);

      // Because the number of columns may have changed invalidate the column widths
      tableFrame->InvalidateColumnWidths();

      // Reflow the new frames. They're already marked dirty, so generate a reflow
      // command that tells us to reflow our dirty child frames
      nsIReflowCommand* reflowCmd;

      if (NS_SUCCEEDED(NS_NewHTMLReflowCommand(&reflowCmd, this,
                                               nsIReflowCommand::ReflowDirty))) {
        aPresShell.AppendReflowCommand(reflowCmd);
        NS_RELEASE(reflowCmd);
      }
    }
  }

  return NS_OK;
}

// this does not get called for trees
NS_IMETHODIMP
nsTableRowGroupFrame::InsertFrames(nsIPresContext* aPresContext,
                                   nsIPresShell&   aPresShell,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aPrevFrame,
                                   nsIFrame*       aFrameList)
{
  // collect the new row frames in an array
  nsVoidArray rows;
  for (nsIFrame* rowFrame = aFrameList; rowFrame; rowFrame->GetNextSibling(&rowFrame)) {
    nsIAtom* frameType;
    rowFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::tableRowFrame == frameType) {
      rows.AppendElement(rowFrame);
    }
    NS_IF_RELEASE(frameType);
  }

  // Insert the frames in the sibling chain
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);

  if (rows.Count() > 0) {
    nsTableFrame* tableFrame = nsnull;
    nsTableFrame::GetTableFrame(this, tableFrame);
    if (tableFrame) {
      nsTableRowFrame* prevRow = (nsTableRowFrame *)nsTableFrame::GetFrameAtOrBefore(aPresContext, this, aPrevFrame, nsLayoutAtoms::tableRowFrame);
      PRInt32 rowIndex = (prevRow) ? prevRow->GetRowIndex() + 1 : 0;
      tableFrame->InsertRows(*aPresContext, *this, rows, rowIndex, PR_TRUE);

      // Reflow the new frames. They're already marked dirty, so generate a reflow
      // command that tells us to reflow our dirty child frames
      nsIReflowCommand* reflowCmd;

      if (NS_SUCCEEDED(NS_NewHTMLReflowCommand(&reflowCmd, this,
                                               nsIReflowCommand::ReflowDirty))) {
        aPresShell.AppendReflowCommand(reflowCmd);
        NS_RELEASE(reflowCmd);
      }

      // Because the number of columns may have changed invalidate the column widths
      tableFrame->InvalidateColumnWidths();

      // Generate a reflow command so we reflow the table itself. This will
      // do a pass-1 reflow of all the rows including any rows we just added
      AddTableDirtyReflowCommand(aPresContext, aPresShell, tableFrame);
    }
  }

  return NS_OK;
}

// this does not get called for trees
NS_IMETHODIMP
nsTableRowGroupFrame::RemoveFrame(nsIPresContext* aPresContext,
                                  nsIPresShell&   aPresShell,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aOldFrame)
{
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (tableFrame) {
    nsIAtom* frameType;
    aOldFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::tableRowFrame == frameType) {
      PRInt32 firstRowIndex = ((nsTableRowFrame *)aOldFrame)->GetRowIndex();

      tableFrame->RemoveRows(*aPresContext, firstRowIndex, 1, PR_TRUE);
      // Because the number of columns may have changed invalidate the column widths
      tableFrame->InvalidateColumnWidths();

      // Because we haven't added any new frames we don't need to do a pass1
      // reflow. Just generate a reflow command so we reflow the table itself
      AddTableDirtyReflowCommand(aPresContext, aPresShell, tableFrame);
    }
    NS_IF_RELEASE(frameType);
  }
  mFrames.DestroyFrame(aPresContext, aOldFrame);

  return NS_OK;
}

NS_METHOD nsTableRowGroupFrame::IR_TargetIsMe(nsIPresContext*      aPresContext,
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
  switch (type)
  {
  case nsIReflowCommand::ReflowDirty:
    // Reflow the dirty child frames. Typically this is newly added frames.
    // XXX What we really should do is do a pass-1 reflow of newly added
    // frames (only if necessary, i.e., the table isn't fixed layout), then
    // see if column widtsh changed and decide whether to do the pass-2 reflow
    // of just the dirty rows or have the table rebalance column widths and
    // do a pass-2 reflow of all rows
    rv = ReflowMappedChildren(aPresContext, aDesiredSize, aReflowState, aStatus,
                              nsnull, aReflowState.reflowState.reason, PR_TRUE, PR_TRUE);

    // If any column widths have to change due to this, rebalance column widths.
    // XXX need to calculate this, but for now just do it
    aReflowState.tableFrame->InvalidateColumnWidths();  
    break;

  case nsIReflowCommand::StyleChanged :
    rv = IR_StyleChanged(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case nsIReflowCommand::ContentChanged :
    NS_ASSERTION(PR_FALSE, "illegal reflow type: ContentChanged");
    rv = NS_ERROR_ILLEGAL_VALUE;
    break;
  
  default:
    NS_NOTYETIMPLEMENTED("unexpected reflow command type");
    rv = NS_ERROR_NOT_IMPLEMENTED;
    break;
  }

  // XXX If we have a next-in-flow, then we're not complete
  if (mNextInFlow) {
    aStatus = NS_FRAME_NOT_COMPLETE;
  }
  return rv;
}

NS_METHOD nsTableRowGroupFrame::GetHeightOfRows(nsIPresContext* aPresContext, 
                                                nscoord&        aResult)
{
  nsTableFrame* tableFrame = nsnull;
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (NS_FAILED(rv) || !tableFrame) return rv;

  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  // the rows in rowGroupFrame need to be expanded by rowHeightDelta[i]
  // and the rowgroup itself needs to be expanded by SUM(row height deltas)
  nsIFrame* rowFrame = nsnull;
  rv = FirstChild(aPresContext, nsnull, &rowFrame);
  while ((NS_SUCCEEDED(rv)) && (nsnull!=rowFrame)) {
    const nsStyleDisplay* rowDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)rowDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == rowDisplay->mDisplay) { 
      nsRect rowRect;
      rowFrame->GetRect(rowRect);
      aResult += rowRect.height;
    }
    else if (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == rowDisplay->mDisplay) {
      ((nsTableRowGroupFrame*)rowFrame)->GetHeightOfRows(aPresContext, aResult);
    }
    GetNextFrame(rowFrame, &rowFrame);
  }
  return NS_OK;
}

// Recovers the reflow state to what it should be if aKidFrame is about
// to be reflowed. Restores the following:
// - availSize
// - y
// - firstRow
nsresult
nsTableRowGroupFrame::RecoverState(RowGroupReflowState& aReflowState,
                                   nsIFrame*            aKidFrame,
                                   nsSize*              aMaxElementSize)
{
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  // Walk the list of children looking for aKidFrame
  for (nsIFrame* frame = mFrames.FirstChild(); frame; frame->GetNextSibling(&frame)) {
    if (frame == aKidFrame) {
      break;
    }

    // Update the running y-offset
    nsSize  kidSize;
    frame->GetSize(kidSize);
    aReflowState.y += cellSpacingY + kidSize.height;

    // If our height is constrained then update the available height
    if (PR_FALSE == aReflowState.unconstrainedHeight) {
      aReflowState.availSize.height -= kidSize.height;
    }

    // Update the maximum element size
    if (aMaxElementSize) {
      const nsStyleDisplay *display;
      frame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
      if (NS_STYLE_DISPLAY_TABLE_ROW == display->mDisplay) {
        // Get the row frame's cached max element size
        nsSize kidMaxElementSize;
        ((nsTableRowFrame*)frame)->GetMaxElementSize(kidMaxElementSize);
      
        if (aReflowState.firstRow) {
          aMaxElementSize->width = kidMaxElementSize.width;
          aMaxElementSize->height = kidMaxElementSize.height;
        } else {
          aMaxElementSize->width = PR_MAX(aMaxElementSize->width, kidMaxElementSize.width);
        }
      }
    }

    aReflowState.firstRow = PR_FALSE;
  }

  return NS_OK;
}

PRBool
nsTableRowGroupFrame::IsSimpleRowFrame(nsTableFrame* aTableFrame,
                                       nsIFrame*     aFrame)
{
  // Make sure it's a row frame and not a row group frame
  nsCOMPtr<nsIAtom>  frameType;

  aFrame->GetFrameType(getter_AddRefs(frameType));
  if (frameType.get() == nsLayoutAtoms::tableRowFrame) {
    PRInt32 rowIndex = ((nsTableRowFrame*)aFrame)->GetRowIndex();
    
    // It's a simple row frame if there are no cells that span into or
    // across the row
    if (!aTableFrame->RowIsSpannedInto(rowIndex) &&
        !aTableFrame->RowHasSpanningCells(rowIndex)) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nscoord
GetFrameYMost(nsIFrame* aFrame)
{
  nsRect rect;
  aFrame->GetRect(rect);
  return rect.YMost();
}

nsIFrame*
GetLastRowSibling(nsIFrame* aRowFrame)
{
  nsIFrame* lastRowFrame = nsnull;
  nsIFrame* lastFrame    = aRowFrame;
  while (lastFrame) {
    nsCOMPtr<nsIAtom> fType;
    lastFrame->GetFrameType(getter_AddRefs(fType));
    if (nsLayoutAtoms::tableRowFrame == fType.get()) {
      lastRowFrame = lastFrame;
    }
    lastFrame->GetNextSibling(&lastFrame);
  }
  return lastRowFrame;
}

NS_METHOD nsTableRowGroupFrame::IR_TargetIsChild(nsIPresContext*      aPresContext,
                                                 nsHTMLReflowMetrics& aDesiredSize,
                                                 RowGroupReflowState& aReflowState,
                                                 nsReflowStatus&      aStatus,
                                                 nsIFrame *           aNextFrame)

{
  nsresult rv;
  
  // Recover the state as if aNextFrame is about to be reflowed
  RecoverState(aReflowState, aNextFrame, aDesiredSize.maxElementSize);

  // Remember the old rect
  nsRect  oldKidRect;
  aNextFrame->GetRect(oldKidRect);

  // Reflow the child giving it as much room as it wants. We'll deal with
  // splitting later after final determination of rows heights taking into
  // account cells with row spans...
  nsSize              kidAvailSize(aReflowState.availSize.width, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState   kidReflowState(aPresContext, aReflowState.reflowState,
                                     aNextFrame, kidAvailSize);
  nsSize              kidMaxElementSize;
  nsHTMLReflowMetrics desiredSize(aDesiredSize.maxElementSize ? &kidMaxElementSize : nsnull,
                                  aDesiredSize.mFlags);

  // Pass along the reflow command
  rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState,
                   0, aReflowState.y, 0, aStatus);

  // Place the row frame
  nsRect  kidRect(0, aReflowState.y, desiredSize.width, desiredSize.height);
  PlaceChild(aPresContext, aReflowState, aNextFrame, desiredSize, 0,
             aReflowState.y, aDesiredSize.maxElementSize, kidMaxElementSize);

  // See if the table needs a reflow (e.g., if the column widths have
  // changed). If so, just return and don't bother adjusting the rows
  // that follow
  if (!aReflowState.tableFrame->NeedsReflow(aReflowState.reflowState)) {
    // If the row has no cells that span into or across the row, then we
    // don't have to call CalculateRowHeights() which is quite expensive
    if (IsSimpleRowFrame(aReflowState.tableFrame, aNextFrame)) {
      // See if the row changed height
      if (oldKidRect.height == desiredSize.height) {
        // We don't need to do any painting. The row frame has made sure that
        // the cell is properly positioned, and done any necessary repainting.
        // Just calculate our desired height
        aDesiredSize.height = GetFrameYMost(GetLastRowSibling(mFrames.FirstChild()));
      } else {
        // Inform the row of its new height.
        ((nsTableRowFrame*)aNextFrame)->DidResize(aPresContext, aReflowState.reflowState);

        // Because other cells in the row may need to be be aligned differently,
        // repaint the entire row
        // XXX Improve this so the row knows it should bitblt (or repaint) those
        // cells that change position...
        Invalidate(aPresContext, kidRect);

        // Invalidate the area we're offseting. Note that we only repaint within
        // our existing frame bounds.
        // XXX It would be better to bitblt the row frames and not repaint,
        // but we don't have such a view manager function yet...
        if (kidRect.YMost() < mRect.height) {
          nsRect  dirtyRect(0, kidRect.YMost(),
                            mRect.width, mRect.height - kidRect.YMost());
          Invalidate(aPresContext, dirtyRect);
        }

        // Adjust the frames that follow
        AdjustSiblingsAfterReflow(aPresContext, aReflowState, aNextFrame,
                                  aDesiredSize.maxElementSize,
                                  desiredSize.height - oldKidRect.height);
        aDesiredSize.height = aReflowState.y;
      }
    } else {
      if (desiredSize.mNothingChanged) { // mNothingChanges currently only works when a cell is the target
        // the cell frame did not change size. Just calculate our desired height
        aDesiredSize.height = GetFrameYMost(GetLastRowSibling(mFrames.FirstChild()));
      } else {
        // Adjust the frames that follow...
        AdjustSiblingsAfterReflow(aPresContext, aReflowState, aNextFrame,
                                  aDesiredSize.maxElementSize,
                                  desiredSize.height - oldKidRect.height);
    
        // Now recalculate the row heights
        CalculateRowHeights(aPresContext, aDesiredSize, aReflowState.reflowState);
      
        // Because we don't know what changed repaint everything.
        // XXX We should change CalculateRowHeights() to return the bounding
        // rect of what changed. Or whether anything moved or changed size...
        nsRect  dirtyRect(0, 0, mRect.width, mRect.height);
        Invalidate(aPresContext, dirtyRect);
      }
    }
  }
  
  // Return our desired width
  aDesiredSize.width = aReflowState.reflowState.availableWidth;

  if (mNextInFlow) {
    aStatus = NS_FRAME_NOT_COMPLETE;
  }

  return rv;
}

NS_METHOD nsTableRowGroupFrame::IR_StyleChanged(nsIPresContext*      aPresContext,
                                                nsHTMLReflowMetrics& aDesiredSize,
                                                RowGroupReflowState& aReflowState,
                                                nsReflowStatus&      aStatus)
{
  nsresult rv = NS_OK;
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet before we were called here
  // XXX: we can optimize this when we know which style attribute changed
  aReflowState.tableFrame->InvalidateFirstPassCache();
  return rv;
}

NS_IMETHODIMP
nsTableRowGroupFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::tableRowGroupFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}


/* ----- global methods ----- */

nsresult 
NS_NewTableRowGroupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableRowGroupFrame* it = new (aPresShell) nsTableRowGroupFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableRowGroupFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TableRowGroup", aResult);
}

NS_IMETHODIMP
nsTableRowGroupFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = sizeof(*this);
  *aResult = sum;
  return NS_OK;
}
#endif

//nsILineIterator methods for nsTableFrame
NS_IMETHODIMP
nsTableRowGroupFrame::GetNumLines(PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  return GetRowCount(*aResult);
}

NS_IMETHODIMP
nsTableRowGroupFrame::GetDirection(PRBool* aIsRightToLeft)
{
  NS_ENSURE_ARG_POINTER(aIsRightToLeft);
  *aIsRightToLeft = PR_FALSE;
  return NS_OK;
}
  
NS_IMETHODIMP
nsTableRowGroupFrame::GetLine(PRInt32 aLineNumber, nsIFrame** aFirstFrameOnLine, PRInt32* aNumFramesOnLine,
                     nsRect& aLineBounds, PRUint32* aLineFlags)
{
  NS_ENSURE_ARG_POINTER(aFirstFrameOnLine);
  NS_ENSURE_ARG_POINTER(aNumFramesOnLine);
  NS_ENSURE_ARG_POINTER(aLineFlags);

  nsTableFrame* parentFrame = nsnull; 
  if (NS_FAILED(nsTableFrame::GetTableFrame(this, parentFrame)))
    return NS_ERROR_FAILURE;

  nsTableCellMap* cellMap = parentFrame->GetCellMap();
  if(!cellMap)
     return NS_ERROR_FAILURE;

  if(aLineNumber >= cellMap->GetRowCount())
    return NS_ERROR_INVALID_ARG;
  
  *aLineFlags = 0;/// should we fill these in later?
  // not gonna touch aLineBounds right now

  CellData* firstCellData = cellMap->GetCellAt(aLineNumber, 0);
  *aFirstFrameOnLine = (nsIFrame*)firstCellData->GetCellFrame();
  if(!(*aFirstFrameOnLine))
  {
    while((aLineNumber > 0)&&(!(*aFirstFrameOnLine)))
    {
      aLineNumber--;
      firstCellData = cellMap->GetCellAt(aLineNumber, 0);
      *aFirstFrameOnLine = (nsIFrame*)firstCellData->GetCellFrame();
    }
  }
  *aNumFramesOnLine = cellMap->GetNumCellsOriginatingInRow(aLineNumber);
  return NS_OK;
}
  
NS_IMETHODIMP
nsTableRowGroupFrame::FindLineContaining(nsIFrame* aFrame, PRInt32* aLineNumberResult)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  NS_ENSURE_ARG_POINTER(aLineNumberResult);

  nsTableRowFrame* rowFrame = (nsTableRowFrame*)aFrame;
  *aLineNumberResult = rowFrame->GetRowIndex();

  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::FindLineAt(nscoord aY, PRInt32* aLineNumberResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
  
NS_IMETHODIMP
nsTableRowGroupFrame::FindFrameAt(PRInt32 aLineNumber, nscoord aX, nsIFrame** aFrameFound,
                         PRBool* aXIsBeforeFirstFrame, PRBool* aXIsAfterLastFrame)
{
  PRInt32 cellCount = 0;
  CellData* cellData;
  nsIFrame* tempFrame;
  nsRect tempRect;
  nsRect& tempRectRef = tempRect;
  nsresult rv;

  nsTableFrame* parentFrame = nsnull;
  rv = nsTableFrame::GetTableFrame(this, parentFrame);
  nsTableCellMap* cellMap = parentFrame->GetCellMap();
  if(!cellMap)
     return NS_ERROR_FAILURE;

  cellCount = cellMap->GetNumCellsOriginatingInRow(aLineNumber);

  *aXIsBeforeFirstFrame = PR_FALSE;
  *aXIsAfterLastFrame = PR_FALSE;

  PRBool gotParentRect = PR_FALSE;
  for(int i =0;i < cellCount; i++)
  {
    cellData = cellMap->GetCellAt(aLineNumber, i);
    tempFrame = (nsIFrame*)cellData->GetCellFrame();

    if(!tempFrame)
      continue;
    
    tempFrame->GetRect(tempRectRef);//offsetting x to be in row coordinates
    if(!gotParentRect)
    {//only do this once
      nsRect parentRect;
      nsRect& parentRectRef = parentRect;
      nsIFrame* tempParentFrame;

      rv = tempFrame->GetParent(&tempParentFrame);
    
      if(NS_FAILED(rv) || !tempParentFrame)
        return rv?rv:NS_ERROR_FAILURE;

      tempParentFrame->GetRect(parentRectRef);
      aX -= parentRect.x;
      gotParentRect = PR_TRUE;
    }

    if(i==0 &&(aX <= 0))//short circuit for negative x coords
    {
      *aXIsBeforeFirstFrame = PR_TRUE;
      *aFrameFound = tempFrame;
      return NS_OK;
    }
    if(aX < tempRect.x)
    {
      return NS_ERROR_FAILURE;
    }
    if(aX < (tempRect.x + tempRect.width))
    {
      *aFrameFound = tempFrame;
      return NS_OK;
    }
  }
  //x coord not found in frame, return last frame
  *aXIsAfterLastFrame = PR_TRUE;
  *aFrameFound = tempFrame;
  if(!(*aFrameFound))
    return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber)
{
  NS_ENSURE_ARG_POINTER(aFrame);

  nsITableCellLayout* cellFrame;
  nsresult result = aFrame->QueryInterface(NS_GET_IID(nsITableCellLayout),(void**)&cellFrame);
  
  if(NS_FAILED(result) || !cellFrame)
    return result?result:NS_ERROR_FAILURE;
  
  nsTableFrame* parentFrame = nsnull;
  result = nsTableFrame::GetTableFrame(this, parentFrame);
  nsTableCellMap* cellMap = parentFrame->GetCellMap();
  if(!cellMap)
     return NS_ERROR_FAILURE;


  PRInt32 colIndex;
  PRInt32& colIndexRef = colIndex;
  cellFrame->GetColIndex(colIndexRef);

  CellData* cellData = cellMap->GetCellAt(aLineNumber, colIndex + 1);
  
  if(!cellData)// if this isnt a valid cell, drop down and check the next line
  {
    cellData = cellMap->GetCellAt(aLineNumber + 1, 0);
    if(!cellData)
    {
      //*aFrame = nsnull;
      return NS_ERROR_FAILURE;
    }
  }

  aFrame = (nsIFrame*)cellData->GetCellFrame();
  if(!aFrame)
  {
    //PRInt32 numCellsInRow = cellMap->GetNumCellsOriginatingInRow(aLineNumber) - 1;
    PRInt32 tempCol = colIndex + 1;
    PRInt32 tempRow = aLineNumber;
    while((tempCol > 0) && (!aFrame))
    {
      tempCol--;
      cellData = cellMap->GetCellAt(aLineNumber, tempCol);
      aFrame = (nsIFrame*)cellData->GetCellFrame();
      if(!aFrame && (tempCol==0))
      {
        while((tempRow > 0) && (!aFrame))
        {
          tempRow--;
          cellData = cellMap->GetCellAt(tempRow, 0);
          aFrame = (nsIFrame*)cellData->GetCellFrame();
        }
      }
    }
  }
  return NS_OK;
}

//end nsLineIterator methods

