/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsTableRowGroupFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "nsIView.h"
#include "nsReflowPath.h"
#include "nsIDeviceContext.h"
#include "nsHTMLAtoms.h"
#include "nsIPresShell.h"
#include "nsLayoutAtoms.h"
#include "nsCSSRendering.h"
#include "nsHTMLParts.h"
#include "nsCSSFrameConstructor.h"

#include "nsCellMap.h"//table cell navigation

nsTableRowGroupFrame::nsTableRowGroupFrame()
{
  SetRepeatable(PR_FALSE);
#ifdef DEBUG_TABLE_REFLOW_TIMING
  mTimer = new nsReflowTimer(this);
#endif
}

nsTableRowGroupFrame::~nsTableRowGroupFrame()
{
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflowDone(this);
#endif
}

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

/* virtual */ PRBool
nsTableRowGroupFrame::IsContainingBlock() const
{
  return PR_TRUE;
}

PRInt32
nsTableRowGroupFrame::GetRowCount()
{  
  PRInt32 count = 0; // init return

  // loop through children, adding one to aCount for every legit row
  nsIFrame* childFrame = GetFirstFrame();
  while (PR_TRUE) {
    if (!childFrame)
      break;
    if (NS_STYLE_DISPLAY_TABLE_ROW == childFrame->GetStyleDisplay()->mDisplay)
      count++;
    GetNextFrame(childFrame, &childFrame);
  }
  return count;
}

PRInt32 nsTableRowGroupFrame::GetStartRowIndex()
{
  PRInt32 result = -1;
  nsIFrame* childFrame = GetFirstFrame();
  while (PR_TRUE) {
    if (!childFrame)
      break;
    if (NS_STYLE_DISPLAY_TABLE_ROW == childFrame->GetStyleDisplay()->mDisplay) {
      result = ((nsTableRowFrame *)childFrame)->GetRowIndex();
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
  nsTableRowFrame* copyRowFrame = GetFirstRow();
  nsTableRowFrame* originalRowFrame = aHeaderFooterFrame->GetFirstRow();
  while (copyRowFrame && originalRowFrame) {
    int rowIndex = originalRowFrame->GetRowIndex();
    copyRowFrame->SetRowIndex(rowIndex);

    // For each table cell frame set its column index
    nsTableCellFrame* originalCellFrame = originalRowFrame->GetFirstCell();
    nsTableCellFrame* copyCellFrame     = copyRowFrame->GetFirstCell();
    while (copyCellFrame && originalCellFrame) {
      NS_ASSERTION(originalCellFrame->GetContent() == copyCellFrame->GetContent(),
                   "cell frames have different content");
      PRInt32 colIndex;
      originalCellFrame->GetColIndex(colIndex);
      copyCellFrame->SetColIndex(colIndex);
        
      // Move to the next cell frame
      copyCellFrame     = copyCellFrame->GetNextCell();
      originalCellFrame = originalCellFrame->GetNextCell();
    }
    
    // Move to the next row frame
    originalRowFrame = originalRowFrame->GetNextRow();
    copyRowFrame = copyRowFrame->GetNextRow();
  }

  return NS_OK;
}

NS_METHOD nsTableRowGroupFrame::Paint(nsIPresContext*      aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect&        aDirtyRect,
                                      nsFramePaintLayer    aWhichLayer,
                                      PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_FALSE, &isVisible)) && !isVisible) {
    return NS_OK;
  }

#ifdef DEBUG
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer &&
      //direct (not table-called) background paint
      !(aFlags & (NS_PAINT_FLAG_TABLE_BG_PAINT | NS_PAINT_FLAG_TABLE_CELL_BG_PASS))) {
    nsTableFrame* tableFrame;
    nsTableFrame::GetTableFrame(this, tableFrame);
    NS_ASSERTION(tableFrame, "null table frame");

    TableBackgroundPainter painter(tableFrame,
                                   TableBackgroundPainter::eOrigin_TableRowGroup,
                                   aPresContext, aRenderingContext,
                                   aDirtyRect);
    nsresult rv = painter.PaintRowGroup(this);
    if (NS_FAILED(rv)) return rv;
    aFlags |= NS_PAINT_FLAG_TABLE_BG_PAINT;
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect,
                aWhichLayer, aFlags);
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


NS_IMETHODIMP
nsTableRowGroupFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, PR_FALSE, aFrame);
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame
void 
nsTableRowGroupFrame::PlaceChild(nsIPresContext*        aPresContext,
                                 nsRowGroupReflowState& aReflowState,
                                 nsIFrame*              aKidFrame,
                                 nsHTMLReflowMetrics&   aDesiredSize)
{
  // Place and size the child
  FinishReflowChild(aKidFrame, aPresContext, nsnull, aDesiredSize, 0, aReflowState.y, 0);

  // Adjust the running y-offset
  aReflowState.y += aDesiredSize.height;

  // If our height is constrained then update the available height
  if (NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height) {
    aReflowState.availSize.height -= aDesiredSize.height;
  }
}

void
nsTableRowGroupFrame::InitChildReflowState(nsIPresContext&    aPresContext, 
                                           PRBool             aBorderCollapse,
                                           float              aPixelsToTwips,
                                           nsHTMLReflowState& aReflowState)                                    
{
  nsMargin collapseBorder;
  nsMargin padding(0,0,0,0);
  nsMargin* pCollapseBorder = nsnull;
  if (aBorderCollapse) {
    if (aReflowState.frame) {
      if (nsLayoutAtoms::tableRowFrame == aReflowState.frame->GetType()) {
        nsTableRowFrame* rowFrame = (nsTableRowFrame*)aReflowState.frame;
        pCollapseBorder = rowFrame->GetBCBorderWidth(aPixelsToTwips, collapseBorder);
      }
    }
  }
  aReflowState.Init(&aPresContext, -1, -1, pCollapseBorder, &padding);
}

// Reflow the frames we've already created. If aDirtyOnly is set then only
// reflow dirty frames. This assumes that all of the dirty frames are contiguous.
NS_METHOD 
nsTableRowGroupFrame::ReflowChildren(nsIPresContext*        aPresContext,
                                     nsHTMLReflowMetrics&   aDesiredSize,
                                     nsRowGroupReflowState& aReflowState,
                                     nsReflowStatus&        aStatus,
                                     nsTableRowFrame*       aStartFrame,
                                     PRBool                 aDirtyOnly,
                                     nsTableRowFrame**      aFirstRowReflowed,
                                     PRBool*                aPageBreakBeforeEnd)
{
  if (aPageBreakBeforeEnd) 
    *aPageBreakBeforeEnd = PR_FALSE;

  nsTableFrame* tableFrame = nsnull;
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame); if (!tableFrame) ABORT1(rv);

  PRBool borderCollapse = tableFrame->IsBorderCollapse();
  GET_PIXELS_TO_TWIPS(aPresContext, p2t);

  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  PRBool isPaginated = aPresContext->IsPaginated();

  if (aFirstRowReflowed) {
    *aFirstRowReflowed = nsnull;
  }
  nsIFrame* lastReflowedRow = nsnull;
  PRBool    adjustSiblings  = PR_TRUE;
  nsIFrame* kidFrame = (aStartFrame) ? aStartFrame : mFrames.FirstChild();

  for ( ; kidFrame; kidFrame = kidFrame->GetNextSibling()) {
    // See if we should only reflow the dirty child frames
    PRBool doReflowChild = PR_TRUE;
    if (aDirtyOnly && ((kidFrame->GetStateBits() & NS_FRAME_IS_DIRTY) == 0)) {
      doReflowChild = PR_FALSE;
    }
    nsIAtom* kidType = kidFrame->GetType();
    if (aReflowState.reflowState.mFlags.mSpecialHeightReflow) {
      if (!isPaginated && (nsLayoutAtoms::tableRowFrame == kidType &&
                           !((nsTableRowFrame*)kidFrame)->NeedSpecialReflow())) {
        doReflowChild = PR_FALSE;
      }
    }

    // Reflow the row frame
    if (doReflowChild) {
      nsSize kidAvailSize(aReflowState.availSize);
      if (0 >= kidAvailSize.height)
        kidAvailSize.height = 1;      // XXX: HaCk - we don't handle negative heights yet
      nsHTMLReflowMetrics desiredSize(aDesiredSize.mComputeMEW);
      desiredSize.width = desiredSize.height = desiredSize.ascent = desiredSize.descent = 0;
  
      // Reflow the child into the available space, giving it as much height as
      // it wants. We'll deal with splitting later after we've computed the row
      // heights, taking into account cells with row spans...
      kidAvailSize.height = NS_UNCONSTRAINEDSIZE;
      // If the incremental reflow command is a StyleChanged reflow and
      // it's target is the current frame, then make sure we send
      // StyleChange reflow reasons down to the children so that they
      // don't over-optimize their reflow.
      nsReflowReason reason = aReflowState.reason;
      if (eReflowReason_Incremental == aReflowState.reason) {
        nsHTMLReflowCommand* command = aReflowState.reflowState.path->mReflowCommand;
        if (command) {
          nsReflowType type;
          command->GetType(type);
          if (eReflowType_StyleChanged == type) {
            reason = eReflowReason_StyleChange;
          }
        }
      }
      if (kidFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
        reason = eReflowReason_Initial;
      }
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState, kidFrame,
                                       kidAvailSize, reason);
      InitChildReflowState(*aPresContext, borderCollapse, p2t, kidReflowState);
     
      // If this isn't the first row, then we can't be at the top of the page
      if (kidFrame != GetFirstFrame()) {
        kidReflowState.mFlags.mIsTopOfPage = PR_FALSE;
      }

      rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                       0, aReflowState.y, 0, aStatus);

      // Place the child
      PlaceChild(aPresContext, aReflowState, kidFrame, desiredSize);
      aReflowState.y += cellSpacingY;
      lastReflowedRow = kidFrame;

      if (aFirstRowReflowed && !*aFirstRowReflowed) { 
        if (nsLayoutAtoms::tableRowFrame == kidType) {
          *aFirstRowReflowed = (nsTableRowFrame*)kidFrame;
        }
      }
      if (isPaginated && aPageBreakBeforeEnd && !*aPageBreakBeforeEnd && 
          (nsLayoutAtoms::tableRowFrame == kidType)) {
        nsTableRowFrame* nextRow = ((nsTableRowFrame*)kidFrame)->GetNextRow();
        if (nextRow) {
          *aPageBreakBeforeEnd = nsTableFrame::PageBreakAfter(*kidFrame, nextRow);
        }
      }
    } else {
      // were done reflowing, so see if we need to reposition the rows that follow
      if (lastReflowedRow) { 
        if (tableFrame->NeedsReflow(aReflowState.reflowState)) {
          adjustSiblings = PR_FALSE;
          break; // don't bother if the table will reflow everything.
        }
      }
      // Adjust the running y-offset so we know where the next row should be placed
      aReflowState.y += kidFrame->GetSize().height + cellSpacingY;
    }
    ConsiderChildOverflow(aPresContext, aDesiredSize.mOverflowArea, kidFrame);
  }

  // adjust the rows after the ones that were reflowed
  if (lastReflowedRow && adjustSiblings) {
    nsIFrame* nextRow = lastReflowedRow->GetNextSibling();
    if (nextRow) {
      nscoord deltaY = cellSpacingY + lastReflowedRow->GetRect().YMost()
        - nextRow->GetPosition().y;
      if (deltaY != 0) {
        AdjustSiblingsAfterReflow(aPresContext, aReflowState, lastReflowedRow, deltaY);
      }
    }
  }

  if (aReflowState.reflowState.mFlags.mSpecialHeightReflow) {
    aDesiredSize.height = mRect.height;
  }
  return rv;
}

nsTableRowFrame*  
nsTableRowGroupFrame::GetFirstRow() 
{
  for (nsIFrame* childFrame = GetFirstFrame(); childFrame;
       childFrame = childFrame->GetNextSibling()) {
    if (nsLayoutAtoms::tableRowFrame == childFrame->GetType()) {
      return (nsTableRowFrame*)childFrame;
    }
  }
  return nsnull;
}


struct RowInfo {
  RowInfo() { height = pctHeight = hasStyleHeight = hasPctHeight = isSpecial = 0; }
  unsigned height;       // content height or fixed height, excluding pct height
  unsigned pctHeight:29; // pct height
  unsigned hasStyleHeight:1; 
  unsigned hasPctHeight:1; 
  unsigned isSpecial:1; // there is no cell originating in the row with rowspan=1 and there are at
                        // least 2 cells spanning the row and there is no style height on the row
};

static void
UpdateHeights(RowInfo& aRowInfo,
              nscoord  aAdditionalHeight,
              nscoord& aTotal,
              nscoord& aUnconstrainedTotal)
{
  aRowInfo.height += aAdditionalHeight;
  aTotal          += aAdditionalHeight;
  if (!aRowInfo.hasStyleHeight) {
    aUnconstrainedTotal += aAdditionalHeight;
  }
}

void 
nsTableRowGroupFrame::DidResizeRows(nsIPresContext&          aPresContext,
                                    const nsHTMLReflowState& aReflowState,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    nsTableRowFrame*         aStartRowFrameIn)
{
  // update the cells spanning rows with their new heights
  // this is the place where all of the cells in the row get set to the height of the row
  PRInt32 rowIndex;
  nsTableRowFrame* rowFrame;
  nsTableRowFrame* startRowFrame = (aStartRowFrameIn) ? aStartRowFrameIn: GetFirstRow();
  if (!aStartRowFrameIn || startRowFrame == GetFirstRow()) {
    // Reset the overflow area
    aDesiredSize.mOverflowArea = nsRect(0, 0, 0, 0);
  }
  for (rowFrame = startRowFrame, rowIndex = 0; rowFrame; rowFrame = rowFrame->GetNextRow(), rowIndex++) {
    rowFrame->DidResize(&aPresContext, aReflowState);
    ConsiderChildOverflow(&aPresContext, aDesiredSize.mOverflowArea, rowFrame);
  }
}

static void
CacheRowHeightsForPrinting(nsIPresContext*  aPresContext,
                           nsTableRowFrame* aFirstRow)
{
  for (nsTableRowFrame* row = aFirstRow; row; row = row->GetNextRow()) {
    if (!row->GetPrevInFlow()) {
      row->SetHasUnpaginatedHeight(PR_TRUE);
      row->SetUnpaginatedHeight(aPresContext, row->GetSize().height);
    }
  }
}

// This calculates the height of rows starting at aStartRowFrameIn and takes into account 
// style height on the row group, style heights on rows and cells, style heights on rowspans. 
// Actual row heights will be adjusted later if the table has a style height.
// Even if rows don't change height, this method must be called to set the heights of each
// cell in the row to the height of its row.
void 
nsTableRowGroupFrame::CalculateRowHeights(nsIPresContext*          aPresContext, 
                                          nsHTMLReflowMetrics&     aDesiredSize,
                                          const nsHTMLReflowState& aReflowState,
                                          nsTableRowFrame*         aStartRowFrameIn)
{
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (!aPresContext || !tableFrame) return;

  PRBool isPaginated = aPresContext->IsPaginated();

  // all table cells have the same top and bottom margins, namely cellSpacingY
  nscoord cellSpacingY = tableFrame->GetCellSpacingY();
  float p2t;
  p2t = aPresContext->PixelsToTwips();

  // find the nearest row index at or before aStartRowFrameIn that isn't spanned into. 
  // If we have a computed height, then we can't compute the heights
  // incrementally from aStartRowFrameIn, and we must start at the first row.
  PRInt32 rgStart = GetStartRowIndex();
  PRInt32 startRowIndex = (aStartRowFrameIn) ? aStartRowFrameIn->GetRowIndex() : rgStart;
  PRInt32 startRowIndexSave = startRowIndex;
  if ((NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) && (aReflowState.mComputedHeight > 0)) {
    startRowIndex = rgStart;
  }
  else {
    while (startRowIndex > rgStart) {
      if (!tableFrame->RowIsSpannedInto(startRowIndex)) 
        break;
      startRowIndex--;
    }
  }
  // find the row corresponding to the row index we just found
  nsTableRowFrame* startRowFrame = aStartRowFrameIn;
  if (!startRowFrame || (startRowIndex != startRowIndexSave)) {
    PRInt32 rowX = rgStart;
    for (startRowFrame = GetFirstRow(); startRowFrame; startRowFrame = startRowFrame->GetNextRow()) {
      if (rowX >= startRowIndex)
        break;
      rowX++;
    }
  }
      
  if (!startRowFrame) return;

  // the current row group height is the y origin of the 1st row we are about to calculated a height for
  nscoord startRowGroupHeight = startRowFrame->GetPosition().y;

  PRInt32 numRows = GetRowCount() - (startRowFrame->GetRowIndex() - GetStartRowIndex());
  // collect the current height of each row.  nscoord* rowHeights = nsnull;
  RowInfo* rowInfo;
  if (numRows > 0) {
    rowInfo = new RowInfo[numRows];
    if (!rowInfo) return;
    memset (rowInfo, 0, numRows*sizeof(RowInfo));
  } 
  else return;

  PRBool  hasRowSpanningCell = PR_FALSE;
  nscoord heightOfRows = 0;
  nscoord heightOfUnStyledRows = 0;
  // Get the height of each row without considering rowspans. This will be the max of 
  // the largest desired height of each cell, the largest style height of each cell, 
  // the style height of the row.
  nscoord pctHeightBasis = GetHeightBasis(aReflowState);
  PRInt32 rowIndex; // the index in rowInfo, not among the rows in the row group
  nsTableRowFrame* rowFrame;
  for (rowFrame = startRowFrame, rowIndex = 0; rowFrame; rowFrame = rowFrame->GetNextRow(), rowIndex++) {
    nscoord nonPctHeight = rowFrame->GetContentHeight();
    if (isPaginated) {
      nonPctHeight = PR_MAX(nonPctHeight, rowFrame->GetSize().height);
    }
    if (!rowFrame->GetPrevInFlow()) {
      if (rowFrame->HasPctHeight()) {
        rowInfo[rowIndex].hasPctHeight = PR_TRUE;
        rowInfo[rowIndex].pctHeight = nsTableFrame::RoundToPixel(rowFrame->GetHeight(pctHeightBasis), p2t);
      }
      rowInfo[rowIndex].hasStyleHeight = rowFrame->HasStyleHeight();
      nonPctHeight = nsTableFrame::RoundToPixel(PR_MAX(nonPctHeight, rowFrame->GetFixedHeight()), p2t);
    }
    UpdateHeights(rowInfo[rowIndex], nonPctHeight, heightOfRows, heightOfUnStyledRows);

    if (!rowInfo[rowIndex].hasStyleHeight) {
      if (isPaginated || tableFrame->HasMoreThanOneCell(rowIndex + startRowIndex)) {
        rowInfo[rowIndex].isSpecial = PR_TRUE;
        // iteratate the row's cell frames to see if any do not have rowspan > 1
        nsTableCellFrame* cellFrame = rowFrame->GetFirstCell();
        while (cellFrame) {
          PRInt32 rowSpan = tableFrame->GetEffectiveRowSpan(rowIndex + startRowIndex, *cellFrame);
          if (1 == rowSpan) { 
            rowInfo[rowIndex].isSpecial = PR_FALSE;
            break;
          }
          cellFrame = cellFrame->GetNextCell(); 
        }
      }
    }
    // See if a cell spans into the row. If so we'll have to do the next step
    if (!hasRowSpanningCell) {
      if (tableFrame->RowIsSpannedInto(rowIndex + startRowIndex)) {
        hasRowSpanningCell = PR_TRUE;
      }
    }
  }

  if (hasRowSpanningCell) {
    // Get the height of cells with rowspans and allocate any extra space to the rows they span 
    // iteratate the child frames and process the row frames among them
    for (rowFrame = startRowFrame, rowIndex = 0; rowFrame; rowFrame = rowFrame->GetNextRow(), rowIndex++) {
      // See if the row has an originating cell with rowspan > 1. We cannot determine this for a row in a 
      // continued row group by calling RowHasSpanningCells, because the row's fif may not have any originating
      // cells yet the row may have a continued cell which originates in it.
      if (mPrevInFlow || tableFrame->RowHasSpanningCells(startRowIndex + rowIndex)) {
        nsTableCellFrame* cellFrame = rowFrame->GetFirstCell();
        // iteratate the row's cell frames 
        while (cellFrame) {
          PRInt32 rowSpan = tableFrame->GetEffectiveRowSpan(rowIndex + startRowIndex, *cellFrame);
          if (rowSpan > 1) { // a cell with rowspan > 1, determine the height of the rows it spans
            nscoord heightOfRowsSpanned = 0;
            nscoord heightOfUnStyledRowsSpanned = 0;
            nscoord numSpecialRowsSpanned = 0; 
            nscoord cellSpacingTotal = 0;
            PRInt32 spanX;
            for (spanX = 0; spanX < rowSpan; spanX++) {
              heightOfRowsSpanned += rowInfo[rowIndex + spanX].height;
              if (!rowInfo[rowIndex + spanX].hasStyleHeight) {
                heightOfUnStyledRowsSpanned += rowInfo[rowIndex + spanX].height;
              }
              if (0 != spanX) {
                cellSpacingTotal += cellSpacingY;
              }
              if (rowInfo[rowIndex + spanX].isSpecial) {
                numSpecialRowsSpanned++;
              }
            } 
            nscoord heightOfAreaSpanned = heightOfRowsSpanned + cellSpacingTotal;
            // get the height of the cell 
            nsSize cellFrameSize = cellFrame->GetSize();
            nsSize cellDesSize = cellFrame->GetDesiredSize();
            rowFrame->CalculateCellActualSize(cellFrame, cellDesSize.width, 
                                              cellDesSize.height, cellDesSize.width);
            cellFrameSize.height = cellDesSize.height;
            if (cellFrame->HasVerticalAlignBaseline()) {
              // to ensure that a spanning cell with a long descender doesn't
              // collide with the next row, we need to take into account the shift
              // that will be done to align the cell on the baseline of the row.
              cellFrameSize.height += rowFrame->GetMaxCellAscent() - cellFrame->GetDesiredAscent();
            }
  
            if (heightOfAreaSpanned < cellFrameSize.height) {
              // the cell's height is larger than the available space of the rows it
              // spans so distribute the excess height to the rows affected
              nscoord extra     = cellFrameSize.height - heightOfAreaSpanned;
              nscoord extraUsed = 0;
              if (0 == numSpecialRowsSpanned) {
                //NS_ASSERTION(heightOfRowsSpanned > 0, "invalid row span situation");
                PRBool haveUnStyledRowsSpanned = (heightOfUnStyledRowsSpanned > 0);
                nscoord divisor = (haveUnStyledRowsSpanned) 
                                  ? heightOfUnStyledRowsSpanned : heightOfRowsSpanned;
                if (divisor > 0) {
                  for (spanX = rowSpan - 1; spanX >= 0; spanX--) {
                    if (!haveUnStyledRowsSpanned || !rowInfo[rowIndex + spanX].hasStyleHeight) {
                      // The amount of additional space each row gets is proportional to its height
                      float percent = ((float)rowInfo[rowIndex + spanX].height) / ((float)divisor);
                    
                      // give rows their percentage, except for the first row which gets the remainder
                      nscoord extraForRow = (0 == spanX) ? extra - extraUsed  
                                                         : NSToCoordRound(((float)(extra)) * percent);
                      extraForRow = PR_MIN(nsTableFrame::RoundToPixel(extraForRow, p2t), extra - extraUsed);
                      // update the row height
                      UpdateHeights(rowInfo[rowIndex + spanX], extraForRow, heightOfRows, heightOfUnStyledRows);
                      extraUsed += extraForRow;
                      if (extraUsed >= extra) {
                        NS_ASSERTION((extraUsed == extra), "invalid row height calculation");
                        break;
                      }
                    }
                  }
                }
                else {
                  // put everything in the last row
                  UpdateHeights(rowInfo[rowIndex + rowSpan - 1], extra, heightOfRows, heightOfUnStyledRows);
                }
              }
              else {
                // give the extra to the special rows
                nscoord numSpecialRowsAllocated = 0;
                for (spanX = rowSpan - 1; spanX >= 0; spanX--) {
                  if (rowInfo[rowIndex + spanX].isSpecial) {
                    // The amount of additional space each degenerate row gets is proportional to the number of them
                    float percent = 1.0f / ((float)numSpecialRowsSpanned);
                    
                    // give rows their percentage, except for the first row which gets the remainder
                    nscoord extraForRow = (numSpecialRowsSpanned - 1 == numSpecialRowsAllocated) 
                                          ? extra - extraUsed  
                                          : NSToCoordRound(((float)(extra)) * percent);
                    extraForRow = PR_MIN(nsTableFrame::RoundToPixel(extraForRow, p2t), extra - extraUsed);
                    // update the row height
                    UpdateHeights(rowInfo[rowIndex + spanX], extraForRow, heightOfRows, heightOfUnStyledRows);
                    extraUsed += extraForRow;
                    if (extraUsed >= extra) {
                      NS_ASSERTION((extraUsed == extra), "invalid row height calculation");
                      break;
                    }
                  }
                }
              }
            } 
          } // if (rowSpan > 1)
          cellFrame = cellFrame->GetNextCell(); 
        } // while (cellFrame)
      } // if (tableFrame->RowHasSpanningCells(startRowIndex + rowIndex) {
    } // while (rowFrame)
  }

  // pct height rows have already got their content heights. Give them their pct heights up to pctHeightBasis
  nscoord extra = pctHeightBasis - heightOfRows;
  for (rowFrame = startRowFrame, rowIndex = 0; rowFrame && (extra > 0); rowFrame = rowFrame->GetNextRow(), rowIndex++) {
    RowInfo& rInfo = rowInfo[rowIndex];
    if (rInfo.hasPctHeight) {
      nscoord rowExtra = (rInfo.pctHeight > rInfo.height)  
                         ? rInfo.pctHeight - rInfo.height: 0;
      rowExtra = PR_MIN(rowExtra, extra);
      UpdateHeights(rInfo, rowExtra, heightOfRows, heightOfUnStyledRows);
      extra -= rowExtra;
    }
  }

  PRBool styleHeightAllocation = PR_FALSE;
  nscoord rowGroupHeight = startRowGroupHeight + heightOfRows + ((numRows - 1) * cellSpacingY);
  // if we have a style height, allocate the extra height to unconstrained rows
  if ((aReflowState.mComputedHeight > rowGroupHeight) && 
      (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight)) {
    nscoord extraComputedHeight = aReflowState.mComputedHeight - rowGroupHeight;
    nscoord extraUsed = 0;
    PRBool haveUnStyledRows = (heightOfUnStyledRows > 0);
    nscoord divisor = (haveUnStyledRows) 
                      ? heightOfUnStyledRows : heightOfRows;
    if (divisor > 0) {
      styleHeightAllocation = PR_TRUE;
      for (rowIndex = 0; rowIndex < numRows; rowIndex++) {
        if (!haveUnStyledRows || !rowInfo[rowIndex].hasStyleHeight) {
          // The amount of additional space each row gets is based on the
          // percentage of space it occupies
          float percent = ((float)rowInfo[rowIndex].height) / ((float)divisor);
          // give rows their percentage, except for the last row which gets the remainder
          nscoord extraForRow = (numRows - 1 == rowIndex) 
                                ? extraComputedHeight - extraUsed  
                                : NSToCoordRound(((float)extraComputedHeight) * percent);
          extraForRow = PR_MIN(nsTableFrame::RoundToPixel(extraForRow, p2t), extraComputedHeight - extraUsed);
          // update the row height
          UpdateHeights(rowInfo[rowIndex], extraForRow, heightOfRows, heightOfUnStyledRows);
          extraUsed += extraForRow;
          if (extraUsed >= extraComputedHeight) {
            NS_ASSERTION((extraUsed == extraComputedHeight), "invalid row height calculation");
            break;
          }
        }
      }
    }
    rowGroupHeight = aReflowState.mComputedHeight;
  }

  nscoord yOrigin = startRowGroupHeight;
  // update the rows with their (potentially) new heights
  for (rowFrame = startRowFrame, rowIndex = 0; rowFrame; rowFrame = rowFrame->GetNextRow(), rowIndex++) {
    nsRect rowBounds = rowFrame->GetRect(); 

    PRBool movedFrame = (rowBounds.y != yOrigin);  
    nscoord rowHeight = (rowInfo[rowIndex].height > 0) ? rowInfo[rowIndex].height : 0;
    
    if (movedFrame || (rowHeight != rowBounds.height)) {
      // Resize the row to its final size and position
      rowBounds.y = yOrigin;
      rowBounds.height = rowHeight;
      rowFrame->SetRect(rowBounds);
    }
    if (movedFrame) {
      nsTableFrame::RePositionViews(aPresContext, rowFrame);
    }
    yOrigin += rowHeight + cellSpacingY;
  }

  if (isPaginated && styleHeightAllocation) {
    // since the row group has a style height, cache the row heights, so next in flows can honor them 
    CacheRowHeightsForPrinting(aPresContext, GetFirstRow());
  }

  DidResizeRows(*aPresContext, aReflowState, aDesiredSize, startRowFrame);

  aDesiredSize.height = rowGroupHeight; // Adjust our desired size
  delete [] rowInfo; // cleanup
}


// Called by IR_TargetIsChild() to adjust the sibling frames that follow
// after an incremental reflow of aKidFrame.
// This function is not used for paginated mode so we don't need to deal
// with continuing frames, and it's only called if aKidFrame has no
// cells that span into it and no cells that span across it. That way
// we don't have to deal with rowspans
nsresult
nsTableRowGroupFrame::AdjustSiblingsAfterReflow(nsIPresContext*        aPresContext,
                                                nsRowGroupReflowState& aReflowState,
                                                nsIFrame*              aKidFrame,
                                                nscoord                aDeltaY)
{
  NS_PRECONDITION(NS_UNCONSTRAINEDSIZE == aReflowState.reflowState.availableHeight,
                  "we're not in galley mode");
  nsIFrame* lastKidFrame = aKidFrame;

  // Move the frames that follow aKidFrame by aDeltaY 
  for (nsIFrame* kidFrame = aKidFrame->GetNextSibling(); kidFrame;
       kidFrame = kidFrame->GetNextSibling()) {
    // Move the frame if we need to
    if (aDeltaY != 0) {
      kidFrame->SetPosition(kidFrame->GetPosition() + nsPoint(0, aDeltaY));
      nsTableFrame::RePositionViews(aPresContext, kidFrame);
    }

    // Remember the last frame
    lastKidFrame = kidFrame;
  }

  // Update our running y-offset to reflect the bottommost child
  aReflowState.y = lastKidFrame->GetRect().YMost();

  return NS_OK;
}

// Create a continuing frame, add it to the child list, and then push it
// and the frames that follow
void 
nsTableRowGroupFrame::CreateContinuingRowFrame(nsIPresContext& aPresContext,
                                               nsIFrame&       aRowFrame,
                                               nsIFrame**      aContRowFrame)
{
  // XXX what is the row index?
  if (!aContRowFrame) {NS_ASSERTION(PR_FALSE, "bad call"); return;}
  // create the continuing frame which will create continuing cell frames
  aPresContext.PresShell()->FrameConstructor()->
    CreateContinuingFrame(&aPresContext, &aRowFrame, this, aContRowFrame);
  if (!*aContRowFrame) return;

  // Add the continuing row frame to the child list
  nsIFrame* nextRow;
  GetNextFrame(&aRowFrame, &nextRow);
  (*aContRowFrame)->SetNextSibling(nextRow);
  aRowFrame.SetNextSibling(*aContRowFrame);
          
  // Push the continuing row frame and the frames that follow
  PushChildren(&aPresContext, *aContRowFrame, &aRowFrame);
}

// Reflow the cells with rowspan > 1 which originate between aFirstRow
// and end on or after aLastRow. aFirstTruncatedRow is the highest row on the
// page that contains a cell which cannot split on this page 
void
nsTableRowGroupFrame::SplitSpanningCells(nsIPresContext&          aPresContext,
                                         const nsHTMLReflowState& aReflowState,
                                         nsTableFrame&            aTable,
                                         nsTableRowFrame&         aFirstRow, 
                                         nsTableRowFrame&         aLastRow,  
                                         PRBool                   aFirstRowIsTopOfPage,
                                         nscoord                  aAvailHeight,
                                         nsTableRowFrame*&        aContRow,
                                         nsTableRowFrame*&        aFirstTruncatedRow,
                                         nscoord&                 aDesiredHeight)
{
  aFirstTruncatedRow = nsnull;
  aDesiredHeight     = 0;

  PRInt32 lastRowIndex = aLastRow.GetRowIndex();
  PRBool wasLast = PR_FALSE;
  // Iterate the rows between aFirstRow and aLastRow
  for (nsTableRowFrame* row = &aFirstRow; !wasLast; row = row->GetNextRow()) {
    wasLast = (row == &aLastRow);
    PRInt32 rowIndex = row->GetRowIndex();
    nsPoint rowPos = row->GetPosition();
    // Iterate the cells looking for those that have rowspan > 1
    for (nsTableCellFrame* cell = row->GetFirstCell(); cell; cell = cell->GetNextCell()) {
      PRInt32 rowSpan = aTable.GetEffectiveRowSpan(rowIndex, *cell);
      // Only reflow rowspan > 1 cells which span aLastRow. Those which don't span aLastRow
      // were reflowed correctly during the unconstrained height reflow. 
      if ((rowSpan > 1) && (rowIndex + rowSpan > lastRowIndex)) {
        nsReflowStatus status;
        // Ask the row to reflow the cell to the height of all the rows it spans up through aLastRow
        // aAvailHeight is the space between the row group start and the end of the page
        nscoord cellAvailHeight = aAvailHeight - rowPos.y;
        nscoord cellHeight = row->ReflowCellFrame(&aPresContext, aReflowState, cell, 
                                                  cellAvailHeight, status);
        aDesiredHeight = PR_MAX(aDesiredHeight, rowPos.y + cellHeight);
        if (NS_FRAME_IS_COMPLETE(status)) {
          if (cellHeight > cellAvailHeight) {
            aFirstTruncatedRow = row;
            if ((row != &aFirstRow) || !aFirstRowIsTopOfPage) {
              // return now, since we will be getting another reflow after either (1) row is 
              // moved to the next page or (2) the row group is moved to the next page
              return;
            }
          }
        }
        else {
          if (!aContRow) {
            CreateContinuingRowFrame(aPresContext, aLastRow, (nsIFrame**)&aContRow);
          }
          if (aContRow) {
            if (row != &aLastRow) {
              // aContRow needs a continuation for cell, since cell spanned into aLastRow 
              // but does not originate there
              nsTableCellFrame* contCell = nsnull;
              aPresContext.PresShell()->FrameConstructor()->
                CreateContinuingFrame(&aPresContext, cell, &aLastRow,
                                      (nsIFrame**)&contCell);
              PRInt32 colIndex;
              cell->GetColIndex(colIndex);
              aContRow->InsertCellFrame(contCell, colIndex);
            }
          }
        }
      }
    }
  }
}

// Remove the next-in-flow of the row, its cells and their cell blocks. This 
// is necessary in case the row doesn't need a continuation later on or needs 
// a continuation which doesn't have the same number of cells that now exist. 
void
nsTableRowGroupFrame::UndoContinuedRow(nsIPresContext*  aPresContext,
                                       nsTableRowFrame* aRow)
{
  if (!aRow) return; // allow null aRow to avoid callers doing null checks

  // rowBefore was the prev-sibling of aRow's next-sibling before aRow was created
  nsTableRowFrame* rowBefore = (nsTableRowFrame*)aRow->GetPrevInFlow();

  nsIFrame* firstOverflow = GetOverflowFrames(aPresContext, PR_TRUE); 
  if (!rowBefore || !firstOverflow || (firstOverflow != aRow)) {
    NS_ASSERTION(PR_FALSE, "invalid continued row");
    return;
  }

  // Remove aRow from the sibling chain and hook its next-sibling up with rowBefore
  rowBefore->SetNextSibling(aRow->GetNextSibling());

  // Destroy the row, its cells, and their cell blocks. Cell blocks that have split
  // will not have reflowed yet to pick up content from any overflow lines.
  aRow->Destroy(aPresContext);
}

static nsTableRowFrame* 
GetRowBefore(nsTableRowFrame& aStartRow,
             nsTableRowFrame& aRow)
{
  nsTableRowFrame* rowBefore = nsnull;
  for (nsTableRowFrame* sib = &aStartRow; sib && (sib != &aRow); sib = sib->GetNextRow()) {
    rowBefore = sib;
  }
  return rowBefore;
}

nsresult
nsTableRowGroupFrame::SplitRowGroup(nsIPresContext*          aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aReflowState,
                                    nsTableFrame*            aTableFrame,
                                    nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  nsTableRowFrame* prevRowFrame = nsnull;
  aDesiredSize.height = 0;

  GET_PIXELS_TO_TWIPS(aPresContext, p2t);
  nscoord availWidth  = nsTableFrame::RoundToPixel(aReflowState.availableWidth, p2t);
  nscoord availHeight = nsTableFrame::RoundToPixel(aReflowState.availableHeight, p2t);
  
  PRBool  borderCollapse = ((nsTableFrame*)aTableFrame->GetFirstInFlow())->IsBorderCollapse();
  nscoord cellSpacingY   = aTableFrame->GetCellSpacingY();

  // get the page height
  nsRect actualRect;
  nsRect adjRect;
  aPresContext->GetPageDim(&actualRect, &adjRect);
  nscoord pageHeight = actualRect.height;

  PRBool isTopOfPage = aReflowState.mFlags.mIsTopOfPage;
  nsTableRowFrame* firstRowThisPage = GetFirstRow();

  // Walk each of the row frames looking for the first row frame that doesn't fit 
  // in the available space
  for (nsTableRowFrame* rowFrame = firstRowThisPage; rowFrame; rowFrame = rowFrame->GetNextRow()) {
    PRBool rowIsOnPage = PR_TRUE;
    nsRect rowRect = rowFrame->GetRect();
    // See if the row fits on this page
    if (rowRect.YMost() > availHeight) {
      nsTableRowFrame* contRow = nsnull;
      // Reflow the row in the availabe space and have it split if it is the 1st
      // row (on the page) or there is at least 5% of the current page available 
      // XXX this 5% should be made a preference 
      if (!prevRowFrame || (availHeight - aDesiredSize.height > pageHeight / 20)) { 
        nsSize availSize(availWidth, PR_MAX(availHeight - rowRect.y, 0));
        // don't let the available height exceed what CalculateRowHeights set for it
        availSize.height = PR_MIN(availSize.height, rowRect.height);

        nsHTMLReflowState rowReflowState(aPresContext, aReflowState, rowFrame, availSize, 
                                         eReflowReason_Resize);
        InitChildReflowState(*aPresContext, borderCollapse, p2t, rowReflowState);
        rowReflowState.mFlags.mIsTopOfPage = isTopOfPage; // set top of page
        nsHTMLReflowMetrics rowMetrics(PR_FALSE);

        // Reflow the cell with the constrained height. A cell with rowspan >1 will get this
        // reflow later during SplitSpanningCells.
        rv = ReflowChild(rowFrame, aPresContext, rowMetrics, rowReflowState,
                         0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
        if (NS_FAILED(rv)) return rv;
        rowFrame->SetSize(nsSize(rowMetrics.width, rowMetrics.height));
        rowFrame->DidReflow(aPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);
        rowFrame->DidResize(aPresContext, aReflowState);

        if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
          // The row frame is incomplete and all of the rowspan 1 cells' block frames split
          if ((rowMetrics.height <= rowReflowState.availableHeight) || isTopOfPage) {
            // The row stays on this page because either it split ok or we're on the top of page.
            // If top of page and the height exceeded the avail height, then there will be data loss
            NS_WARN_IF_FALSE(rowMetrics.height <= rowReflowState.availableHeight, 
                            "data loss - incomplete row needed more height than available, on top of page");
            CreateContinuingRowFrame(*aPresContext, *rowFrame, (nsIFrame**)&contRow);
            if (contRow) {
              aDesiredSize.height += rowMetrics.height;
              if (prevRowFrame) 
                aDesiredSize.height += cellSpacingY;
            }
            else return NS_ERROR_NULL_POINTER;
          }
          else {
            // Put the row on the next page to give it more height 
            rowIsOnPage = PR_FALSE;
          }
        } 
        else {
          // The row frame is complete because either (1) it's minimum height is greater than the 
          // available height we gave it, or (2) it may have been given a larger height through 
          // style than it's content, or (3) it contains a rowspan >1 cell which hasn't been
          // reflowed with a constrained height yet (we will find out when SplitSpanningCells is
          // called below)
          if (rowMetrics.height >= availSize.height) {
            // cases (1) and (2)
            if (isTopOfPage) { 
              // We're on top of the page, so keep the row on this page. There will be data loss.
              // Push the row frame that follows
              nsTableRowFrame* nextRowFrame = rowFrame->GetNextRow();
              if (nextRowFrame) {
                aStatus = NS_FRAME_NOT_COMPLETE;
              }
              aDesiredSize.height += rowMetrics.height;
              if (prevRowFrame) 
                aDesiredSize.height += cellSpacingY;
              NS_WARNING("data loss - complete row needed more height than available, on top of page");
            }
            else {  
              // We're not on top of the page, so put the row on the next page to give it more height 
              rowIsOnPage = PR_FALSE;
            }
          }
        }
      } //if (!prevRowFrame || (availHeight - aDesiredSize.height > pageHeight / 20))
      else { 
        // put the row on the next page to give it more height
        rowIsOnPage = PR_FALSE;
      }

      nsTableRowFrame* lastRowThisPage = rowFrame;
      if (!rowIsOnPage) {
        if (prevRowFrame) {
          availHeight -= prevRowFrame->GetRect().YMost();
          lastRowThisPage = prevRowFrame;
          isTopOfPage = (lastRowThisPage == firstRowThisPage) && aReflowState.mFlags.mIsTopOfPage;
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
        else {
          // We can't push children, so let our parent reflow us again with more space
          aDesiredSize.height = rowRect.YMost();
          break;
        }
      }
      // reflow the cells with rowspan >1 that occur on the page

      nsTableRowFrame* firstTruncatedRow;
      nscoord yMost;
      SplitSpanningCells(*aPresContext, aReflowState, *aTableFrame, *firstRowThisPage,
                         *lastRowThisPage, aReflowState.mFlags.mIsTopOfPage, availHeight, contRow, 
                         firstTruncatedRow, yMost);
      if (firstTruncatedRow) {
        // A rowspan >1 cell did not fit (and could not split) in the space we gave it
        if (firstTruncatedRow == firstRowThisPage) {
          if (aReflowState.mFlags.mIsTopOfPage) {
            NS_WARNING("data loss in a row spanned cell");
          }
          else {
            // We can't push children, so let our parent reflow us again with more space
            aDesiredSize.height = rowRect.YMost();
            aStatus = NS_FRAME_COMPLETE;
            UndoContinuedRow(aPresContext, contRow);
          }
        }
        else { // (firstTruncatedRow != firstRowThisPage)
          // Try to put firstTruncateRow on the next page 
          nsTableRowFrame* rowBefore = ::GetRowBefore(*firstRowThisPage, *firstTruncatedRow);
          availHeight -= rowBefore->GetRect().YMost();

          UndoContinuedRow(aPresContext, contRow);
          nsTableRowFrame* oldLastRowThisPage = lastRowThisPage;
          lastRowThisPage = firstTruncatedRow;
          aStatus = NS_FRAME_NOT_COMPLETE;

          // Call SplitSpanningCells again with rowBefore as the last row on the page
          SplitSpanningCells(*aPresContext, aReflowState, *aTableFrame, 
                             *firstRowThisPage, *rowBefore, aReflowState.mFlags.mIsTopOfPage, 
                             availHeight, contRow, firstTruncatedRow, aDesiredSize.height);
          if (firstTruncatedRow) {
            if (aReflowState.mFlags.mIsTopOfPage) {
              // We were better off with the 1st call to SplitSpanningCells, do it again
              UndoContinuedRow(aPresContext, contRow);
              lastRowThisPage = oldLastRowThisPage;
              SplitSpanningCells(*aPresContext, aReflowState, *aTableFrame, *firstRowThisPage,
                                 *lastRowThisPage, aReflowState.mFlags.mIsTopOfPage, availHeight, contRow, 
                                 firstTruncatedRow, aDesiredSize.height);
              NS_WARNING("data loss in a row spanned cell");
            }
            else {
              // Let our parent reflow us again with more space
              aDesiredSize.height = rowRect.YMost();
              aStatus = NS_FRAME_COMPLETE;
              UndoContinuedRow(aPresContext, contRow);
            }
          }
        } // if (firstTruncatedRow == firstRowThisPage)
      } // if (firstTruncatedRow)
      else {
        aDesiredSize.height = PR_MAX(aDesiredSize.height, yMost);
        if (contRow) {
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
      }
      if (NS_FRAME_IS_NOT_COMPLETE(aStatus) && !contRow) {
        nsTableRowFrame* nextRow = lastRowThisPage->GetNextRow();
        if (nextRow) {
          PushChildren(aPresContext, nextRow, lastRowThisPage);
        }
      }
      break;
    } // if (rowRect.YMost() > availHeight)
    else { 
      aDesiredSize.height = rowRect.YMost();
      prevRowFrame = rowFrame;
      // see if there is a page break after the row
      nsTableRowFrame* nextRow = rowFrame->GetNextRow();
      if (nextRow && nsTableFrame::PageBreakAfter(*rowFrame, nextRow)) {
        PushChildren(aPresContext, nextRow, rowFrame);
        aStatus = NS_FRAME_NOT_COMPLETE;
        break;
      }
    }
    isTopOfPage = PR_FALSE; // after the 1st row, we can't be on top of the page any more.
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
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState);
#endif

  nsresult rv = NS_OK;
  aStatus     = NS_FRAME_COMPLETE;
        
  PRBool isPaginated = aPresContext->IsPaginated();

  nsTableFrame* tableFrame = nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (!aPresContext || !tableFrame) return NS_ERROR_NULL_POINTER;

  // see if a special height reflow needs to occur due to having a pct height
  if (!NeedSpecialReflow()) 
    nsTableFrame::CheckRequestSpecialHeightReflow(aReflowState);

  nsRowGroupReflowState state(aReflowState, tableFrame);
  PRBool haveDesiredHeight = PR_FALSE;
  const nsStyleVisibility* groupVis = GetStyleVisibility();
  PRBool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupVis->mVisible);
  if (collapseGroup) {
    tableFrame->SetNeedToCollapseRows(PR_TRUE);
  }

  if (eReflowReason_Incremental == aReflowState.reason) {
    rv = IncrementalReflow(aPresContext, aDesiredSize, state, aStatus);
  } 
  else { 
    // Check for an overflow list
    MoveOverflowToChildList(aPresContext);
  
    // Reflow the existing frames. 
    PRBool splitDueToPageBreak = PR_FALSE;
    rv = ReflowChildren(aPresContext, aDesiredSize, state, aStatus,
                        nsnull, PR_FALSE, nsnull, &splitDueToPageBreak);
  
    // Return our desired rect
    aDesiredSize.width = aReflowState.availableWidth;
    aDesiredSize.height = state.y;

    // shrink wrap rows to height of tallest cell in that row
    PRBool isTableUnconstrainedReflow = 
      (NS_UNCONSTRAINEDSIZE == aReflowState.parentReflowState->availableWidth);

    // Avoid calling CalculateRowHeights. We can avoid it if the table is going to be
    // doing a pass 2 reflow. In the case where the table is getting an unconstrained
    // reflow, then we need to do this because the table will skip the pass 2 reflow,
    // but we need to correctly calculate the row group height and we can't if there
    // are row spans unless we do this step
    if (aReflowState.mFlags.mSpecialHeightReflow) {
      DidResizeRows(*aPresContext, aReflowState, aDesiredSize);
      if (isPaginated) {
        CacheRowHeightsForPrinting(aPresContext, GetFirstRow());
      }
    }
    else if ((eReflowReason_Initial != aReflowState.reason) || 
             isTableUnconstrainedReflow                     ||
             isPaginated) {
      CalculateRowHeights(aPresContext, aDesiredSize, aReflowState);
      haveDesiredHeight = PR_TRUE;
    }

    // See if all the frames fit
    if ((NS_FRAME_NOT_COMPLETE == aStatus) || splitDueToPageBreak || 
        (aDesiredSize.height > aReflowState.availableHeight)) {
      // Nope, find a place to split the row group 
      PRBool specialReflow = (PRBool)aReflowState.mFlags.mSpecialHeightReflow;
      ((nsHTMLReflowState::ReflowStateFlags&)aReflowState.mFlags).mSpecialHeightReflow = PR_FALSE;

      SplitRowGroup(aPresContext, aDesiredSize, aReflowState, tableFrame, aStatus);

      ((nsHTMLReflowState::ReflowStateFlags&)aReflowState.mFlags).mSpecialHeightReflow = specialReflow;
    }
  }
  SetHasStyleHeight((NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) &&
                    (aReflowState.mComputedHeight > 0)); 
  
  if (aReflowState.mFlags.mSpecialHeightReflow) {
    SetNeedSpecialReflow(PR_FALSE);
  }

  // just set our width to what was available. The table will calculate the width and not use our value.
  aDesiredSize.width = aReflowState.availableWidth;
  if (!haveDesiredHeight) {
    // calculate the height based on the rect of the last row
    aDesiredSize.height = GetHeightOfRows();
  }

  aDesiredSize.mOverflowArea.UnionRect(aDesiredSize.mOverflowArea, nsRect(0, 0, aDesiredSize.width,
	                                                                      aDesiredSize.height)); 
  FinishAndStoreOverflow(&aDesiredSize);
#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState, &aDesiredSize, aStatus);
#endif
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}


NS_METHOD 
nsTableRowGroupFrame::IncrementalReflow(nsIPresContext*        aPresContext,
                                        nsHTMLReflowMetrics&   aDesiredSize,
                                        nsRowGroupReflowState& aReflowState,
                                        nsReflowStatus&        aStatus)
{
  // the row group is a target if its path has a reflow command
  nsHTMLReflowCommand* command = aReflowState.reflowState.path->mReflowCommand;
  if (command)
    IR_TargetIsMe(aPresContext, aDesiredSize, aReflowState, aStatus);

  // see if the chidren are targets as well
  // XXXwaterson Note that this will cause us to RecoverState (which
  // is O(n) in the number of child rows) once for each reflow
  // target. It'd probably be better to invert the loops; i.e., walk
  // the rows, checking each to see if it's an IR target (which could
  // be done in O(1) if we do hashing in the reflow path).
  nsReflowPath::iterator iter = aReflowState.reflowState.path->FirstChild();
  nsReflowPath::iterator end = aReflowState.reflowState.path->EndChildren();
  for (; iter != end; ++iter)
    IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, *iter);

  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::AppendFrames(nsIPresContext* aPresContext,
                                   nsIPresShell&   aPresShell,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aFrameList)
{
  // collect the new row frames in an array
  nsAutoVoidArray rows;
  for (nsIFrame* rowFrame = aFrameList; rowFrame;
       rowFrame = rowFrame->GetNextSibling()) {
    if (nsLayoutAtoms::tableRowFrame == rowFrame->GetType()) {
      rows.AppendElement(rowFrame);
    }
  }

  PRInt32 rowIndex = GetRowCount();
  // Append the frames to the sibling chain
  mFrames.AppendFrames(nsnull, aFrameList);

  if (rows.Count() > 0) {
    nsTableFrame* tableFrame = nsnull;
    nsTableFrame::GetTableFrame(this, tableFrame);
    if (tableFrame) {
      tableFrame->AppendRows(*aPresContext, *this, rowIndex, rows);
      // Reflow the new frames. They're already marked dirty, so generate a reflow
      // command that tells us to reflow our dirty child frames
      nsTableFrame::AppendDirtyReflowCommand(&aPresShell, this);
      if (tableFrame->RowIsSpannedInto(rowIndex)) {
        tableFrame->SetNeedStrategyInit(PR_TRUE);
      }
      else if (!tableFrame->IsAutoHeight()) {
        // The table isn't auto height, so any previously reflowed rows
        // it contains were already adjusted so that they take up all of
        // the table's height. We need to trigger a strategy balance to
        // ensure that all rows are resized properly during the dirty reflow we
        // generated above.

        tableFrame->SetNeedStrategyBalance(PR_TRUE);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::InsertFrames(nsIPresContext* aPresContext,
                                   nsIPresShell&   aPresShell,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aPrevFrame,
                                   nsIFrame*       aFrameList)
{
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame) {
    NS_ASSERTION(PR_FALSE, "no table frame");
    return NS_ERROR_NULL_POINTER;
  }
  // collect the new row frames in an array
  nsVoidArray rows;
  PRBool gotFirstRow = PR_FALSE;
  for (nsIFrame* rowFrame = aFrameList; rowFrame;
       rowFrame = rowFrame->GetNextSibling()) {
    if (nsLayoutAtoms::tableRowFrame == rowFrame->GetType()) {
      rows.AppendElement(rowFrame);
      if (!gotFirstRow) {
        ((nsTableRowFrame*)rowFrame)->SetFirstInserted(PR_TRUE);
        gotFirstRow = PR_TRUE;
        tableFrame->SetRowInserted(PR_TRUE);
      }
    }
  }

  PRInt32 startRowIndex = GetStartRowIndex();
  // Insert the frames in the sibling chain
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);

  PRInt32 numRows = rows.Count();
  if (numRows > 0) {
    nsTableRowFrame* prevRow = (nsTableRowFrame *)nsTableFrame::GetFrameAtOrBefore(this, aPrevFrame, nsLayoutAtoms::tableRowFrame);
    PRInt32 rowIndex = (prevRow) ? prevRow->GetRowIndex() + 1 : startRowIndex;
    tableFrame->InsertRows(*aPresContext, *this, rows, rowIndex, PR_TRUE);

    // Reflow the new frames. They're already marked dirty, so generate a reflow
    // command that tells us to reflow our dirty child frames
    nsTableFrame::AppendDirtyReflowCommand(&aPresShell, this);
    if (tableFrame->RowIsSpannedInto(rowIndex) || 
        tableFrame->RowHasSpanningCells(rowIndex + numRows - 1)) {
      tableFrame->SetNeedStrategyInit(PR_TRUE);
    }
    else if (!tableFrame->IsAutoHeight()) {
      // The table isn't auto height, so any previously reflowed rows
      // it contains were already adjusted so that they take up all of
      // the table's height. We need to trigger a strategy balance to
      // ensure that all rows are resized properly during the dirty reflow we
      // generated above.

      tableFrame->SetNeedStrategyBalance(PR_TRUE);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::RemoveFrame(nsIPresContext* aPresContext,
                                  nsIPresShell&   aPresShell,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aOldFrame)
{
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (tableFrame) {
    if (nsLayoutAtoms::tableRowFrame == aOldFrame->GetType()) {
      // remove the rows from the table (and flag a rebalance)
      tableFrame->RemoveRows(*aPresContext, (nsTableRowFrame &)*aOldFrame, 1, PR_TRUE);

      // XXX this could be optimized (see nsTableFrame::RemoveRows)
      tableFrame->SetNeedStrategyInit(PR_TRUE);
      // Because we haven't added any new frames we don't need to do a pass1
      // reflow. Just generate a reflow command so we reflow the table 
      nsTableFrame::AppendDirtyReflowCommand(&aPresShell, this);
    }
  }
  mFrames.DestroyFrame(aPresContext, aOldFrame);

  return NS_OK;
}

NS_METHOD 
nsTableRowGroupFrame::IR_TargetIsMe(nsIPresContext*        aPresContext,
                                    nsHTMLReflowMetrics&   aDesiredSize,
                                    nsRowGroupReflowState& aReflowState,
                                    nsReflowStatus&        aStatus)
{
  nsresult rv = NS_FRAME_COMPLETE;
  nsReflowType type;
  aReflowState.reflowState.path->mReflowCommand->GetType(type);

  switch (type) {
    case eReflowType_ReflowDirty: {
      nsRowGroupReflowState state(aReflowState);
      state.reason = eReflowReason_Resize;
      // Reflow the dirty child frames. Typically this is newly added frames.
      nsTableRowFrame* firstRowReflowed;
      rv = ReflowChildren(aPresContext, aDesiredSize, state, aStatus,
                          nsnull, PR_TRUE, &firstRowReflowed);
      CalculateRowHeights(aPresContext, aDesiredSize, aReflowState.reflowState, firstRowReflowed);
      break;
    }
    case eReflowType_StyleChanged :
      rv = IR_StyleChanged(aPresContext, aDesiredSize, aReflowState, aStatus);
      break;
    case eReflowType_ContentChanged :
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

nscoord 
nsTableRowGroupFrame::GetHeightBasis(const nsHTMLReflowState& aReflowState)
{
  nscoord result = 0;
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame((nsIFrame*)this, tableFrame);
  if (tableFrame) {
    if ((aReflowState.mComputedHeight > 0) && (aReflowState.mComputedHeight < NS_UNCONSTRAINEDSIZE)) {
      nscoord cellSpacing = PR_MAX(0, GetRowCount() - 1) * tableFrame->GetCellSpacingY();
      result = aReflowState.mComputedHeight - cellSpacing;
    }
    else {
      const nsHTMLReflowState* parentRS = aReflowState.parentReflowState;
      if (parentRS && (tableFrame != parentRS->frame)) {
        parentRS = parentRS->parentReflowState;
      }
      if (parentRS && (tableFrame == parentRS->frame) && 
          (parentRS->mComputedHeight > 0) && (parentRS->mComputedHeight < NS_UNCONSTRAINEDSIZE)) {
        nscoord cellSpacing = PR_MAX(0, tableFrame->GetRowCount() + 1) * tableFrame->GetCellSpacingY();
        result = parentRS->mComputedHeight - cellSpacing;
      }
    }
  }

  return result;
}

nscoord 
nsTableRowGroupFrame::GetHeightOfRows()
{
  nsTableFrame* tableFrame = nsnull;
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame) return 0;

  nscoord height = 0;

  // enumerate the rows and total their heights
  nsIFrame* rowFrame = GetFirstChild(nsnull);
  PRInt32 numRows = 0;
  while (rowFrame) {
    if (NS_STYLE_DISPLAY_TABLE_ROW == rowFrame->GetStyleDisplay()->mDisplay) {
      height += rowFrame->GetSize().height;
      numRows++;
    }
    GetNextFrame(rowFrame, &rowFrame);
  }
  if (numRows > 1) {
    height += (numRows - 1) * tableFrame->GetCellSpacingY(); // add in cell spacing
  }

  return height;
}

// Recovers the reflow state to what it should be if aKidFrame is about
// to be reflowed. Restores availSize, y
nsresult
nsTableRowGroupFrame::RecoverState(nsRowGroupReflowState& aReflowState,
                                   nsIFrame*              aKidFrame)
{
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  aReflowState.y = 0;

  // Walk the list of children up to aKidFrame
  for (nsIFrame* frame = mFrames.FirstChild(); frame && (frame != aKidFrame);
       frame = frame->GetNextSibling()) {
    if (frame->GetType() == nsLayoutAtoms::tableRowFrame) {
      // Update the running y-offset
      nsSize kidSize = frame->GetSize();
      aReflowState.y += kidSize.height + cellSpacingY;

      // If our height is constrained then update the available height
      if (NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height) {
        aReflowState.availSize.height -= kidSize.height;
      }
    }
  }

  return NS_OK;
}

PRBool
nsTableRowGroupFrame::IsSimpleRowFrame(nsTableFrame* aTableFrame,
                                       nsIFrame*     aFrame)
{
  // Make sure it's a row frame and not a row group frame
  if (aFrame->GetType() == nsLayoutAtoms::tableRowFrame) {
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

nsIFrame*
GetLastRowSibling(nsIFrame* aRowFrame)
{
  nsIFrame* lastRowFrame = nsnull;
  for (nsIFrame* lastFrame = aRowFrame; lastFrame;
       lastFrame = lastFrame->GetNextSibling()) {
    if (nsLayoutAtoms::tableRowFrame == lastFrame->GetType()) {
      lastRowFrame = lastFrame;
    }
  }
  return lastRowFrame;
}

NS_METHOD 
nsTableRowGroupFrame::IR_TargetIsChild(nsIPresContext*        aPresContext,
                                       nsHTMLReflowMetrics&   aDesiredSize,
                                       nsRowGroupReflowState& aReflowState,
                                       nsReflowStatus&        aStatus,
                                       nsIFrame*              aNextFrame)

{
  nsresult rv;
  
  nsTableFrame* tableFrame = nsnull; 
  nsTableFrame::GetTableFrame(this, tableFrame); if (!tableFrame) ABORT1(NS_ERROR_NULL_POINTER);
  GET_PIXELS_TO_TWIPS(aPresContext, p2t);

  // Recover the state as if aNextFrame is about to be reflowed
  RecoverState(aReflowState, aNextFrame);

  // Remember the old rect
  nsSize oldKidSize = aNextFrame->GetSize();

  // Reflow the child giving it as much room as it wants. We'll deal with
  // splitting later after final determination of rows heights taking into
  // account cells with row spans...
  nsSize            kidAvailSize(aReflowState.availSize.width, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState, aNextFrame, 
                                   kidAvailSize, aReflowState.reason);
  InitChildReflowState(*aPresContext, tableFrame->IsBorderCollapse(), p2t, kidReflowState);

  nsHTMLReflowMetrics desiredSize(aDesiredSize.mComputeMEW,
                                  aDesiredSize.mFlags);

  // Pass along the reflow command
  rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState,
                   0, aReflowState.y, 0, aStatus);

  // Place the row frame
  nsRect  kidRect(0, aReflowState.y, desiredSize.width, desiredSize.height);
  PlaceChild(aPresContext, aReflowState, aNextFrame, desiredSize);

  // See if the table needs a reflow (e.g., if the column widths have
  // changed). If so, just return and don't bother adjusting the rows
  // that follow
  if (!aReflowState.tableFrame->NeedsReflow(aReflowState.reflowState)) {
    // Only call CalculateRowHeights() if necessary since it can be expensive
    PRBool needToCalcRowHeights = PR_FALSE; 
    if (IsSimpleRowFrame(aReflowState.tableFrame, aNextFrame)) {
      // See if the row changed height
      if (oldKidSize.height == desiredSize.height) {
        // We don't need to do any painting. The row frame has made sure that
        // the cell is properly positioned, and done any necessary repainting.
        // Just calculate our desired height
        aDesiredSize.height = GetLastRowSibling(mFrames.FirstChild())->GetRect().YMost();
      } else {
        // Inform the row of its new height.
        ((nsTableRowFrame*)aNextFrame)->DidResize(aPresContext, aReflowState.reflowState);
        // the overflow area may have changed inflate the overflow area
        if (aReflowState.tableFrame->IsAutoHeight()) {
          // Because other cells in the row may need to be be aligned differently,
          // repaint the entire row
          // XXX Improve this so the row knows it should bitblt (or repaint) those
          // cells that change position...
          Invalidate(kidRect);
          
          // Invalidate the area we're offseting. Note that we only repaint within
          // our existing frame bounds.
          // XXX It would be better to bitblt the row frames and not repaint,
          // but we don't have such a view manager function yet...
          if (kidRect.YMost() < mRect.height) {
            nsRect  dirtyRect(0, kidRect.YMost(),
                              mRect.width, mRect.height - kidRect.YMost());
            Invalidate(dirtyRect);
          }

          // Adjust the frames that follow
          AdjustSiblingsAfterReflow(aPresContext, aReflowState, aNextFrame,
                                    desiredSize.height - oldKidSize.height);
          aDesiredSize.height = aReflowState.y;
        }
        else needToCalcRowHeights = PR_TRUE;
      }
    } else {
      if (desiredSize.mNothingChanged) { // mNothingChanges currently only works when a cell is the target
        // the cell frame did not change size. Just calculate our desired height
        aDesiredSize.height = GetLastRowSibling(mFrames.FirstChild())->GetRect().YMost();
      } 
      else needToCalcRowHeights = PR_TRUE;
    }
    if (needToCalcRowHeights) {
      // Adjust the frames that follow... 
      // XXX is this needed since CalculateRowHeights will be called?
      //AdjustSiblingsAfterReflow(aPresContext, aReflowState, aNextFrame,
      //                          desiredSize.height - oldKidSize.height);
    
      // Now recalculate the row heights
      CalculateRowHeights(aPresContext, aDesiredSize, aReflowState.reflowState);
      
      // Because we don't know what changed repaint everything.
      // XXX We should change CalculateRowHeights() to return the bounding
      // rect of what changed. Or whether anything moved or changed size...
      nsRect  dirtyRect(0, 0, mRect.width, mRect.height);
      Invalidate(dirtyRect);
    }
    else {
      // need to recover the  OverflowArea
      for (nsTableRowFrame* rowFrame = GetFirstRow(); rowFrame; rowFrame = rowFrame->GetNextRow()) {
        ConsiderChildOverflow(aPresContext, aDesiredSize.mOverflowArea, rowFrame);
      }
      FinishAndStoreOverflow(&aDesiredSize);
    }
  }
  
  // Return our desired width
  //aDesiredSize.width = aReflowState.reflowState.availableWidth;

  if (mNextInFlow) {
    aStatus = NS_FRAME_NOT_COMPLETE;
  }

  return rv;
}

NS_METHOD 
nsTableRowGroupFrame::IR_StyleChanged(nsIPresContext*        aPresContext,
                                      nsHTMLReflowMetrics&   aDesiredSize,
                                      nsRowGroupReflowState& aReflowState,
                                      nsReflowStatus&        aStatus)
{
  nsresult rv = NS_OK;
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet before we were called here
  // XXX: we can optimize this when we know which style attribute changed
  aReflowState.tableFrame->SetNeedStrategyInit(PR_TRUE);
  nsRowGroupReflowState state(aReflowState);
  nsTableRowFrame* firstRowReflowed;
  rv = ReflowChildren(aPresContext, aDesiredSize, state, aStatus,
                          nsnull, PR_FALSE, &firstRowReflowed);
  CalculateRowHeights(aPresContext, aDesiredSize, aReflowState.reflowState, firstRowReflowed);
      
  return rv;
}

nsIAtom*
nsTableRowGroupFrame::GetType() const
{
  return nsLayoutAtoms::tableRowGroupFrame;
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

NS_IMETHODIMP
nsTableRowGroupFrame::Init(nsIPresContext*  aPresContext,
                           nsIContent*      aContent,
                           nsIFrame*        aParent,
                           nsStyleContext*  aContext,
                           nsIFrame*        aPrevInFlow)
{
  nsresult  rv;

  // Let the base class do its processing
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);

  // record that children that are ignorable whitespace should be excluded 
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

  return rv;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableRowGroupFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableRowGroup"), aResult);
}
#endif

nsMargin* 
nsTableRowGroupFrame::GetBCBorderWidth(float     aPixelsToTwips,
                                       nsMargin& aBorder)
{
  aBorder.left = aBorder.right = 0;

  nsTableRowFrame* firstRowFrame = nsnull;
  nsTableRowFrame* lastRowFrame = nsnull;
  for (nsTableRowFrame* rowFrame = GetFirstRow(); rowFrame; rowFrame = rowFrame->GetNextRow()) {
    if (!firstRowFrame) {
      firstRowFrame = rowFrame;
    }
    lastRowFrame = rowFrame;
  }
  if (firstRowFrame) {
    aBorder.top    = firstRowFrame->GetTopBCBorderWidth(&aPixelsToTwips);
    aBorder.bottom = lastRowFrame->GetBottomBCBorderWidth(&aPixelsToTwips);
  }

  return &aBorder;
}

void nsTableRowGroupFrame::SetContinuousBCBorderWidth(PRUint8     aForSide,
                                                      BCPixelSize aPixelValue)
{
  switch (aForSide) {
    case NS_SIDE_RIGHT:
      mRightContBorderWidth = aPixelValue;
      return;
    case NS_SIDE_BOTTOM:
      mBottomContBorderWidth = aPixelValue;
      return;
    case NS_SIDE_LEFT:
      mLeftContBorderWidth = aPixelValue;
      return;
    default:
      NS_ERROR("invalid NS_SIDE argument");
  }
}

//nsILineIterator methods for nsTableFrame
NS_IMETHODIMP
nsTableRowGroupFrame::GetNumLines(PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = GetRowCount();
  return *aResult; // XXX should return NS_OK
}

NS_IMETHODIMP
nsTableRowGroupFrame::GetDirection(PRBool* aIsRightToLeft)
{
  NS_ENSURE_ARG_POINTER(aIsRightToLeft);
  *aIsRightToLeft = PR_FALSE;
  return NS_OK;
}
  
NS_IMETHODIMP
nsTableRowGroupFrame::GetLine(PRInt32    aLineNumber, 
                              nsIFrame** aFirstFrameOnLine, 
                              PRInt32*   aNumFramesOnLine,
                              nsRect&    aLineBounds, 
                              PRUint32*  aLineFlags)
{
  NS_ENSURE_ARG_POINTER(aFirstFrameOnLine);
  NS_ENSURE_ARG_POINTER(aNumFramesOnLine);
  NS_ENSURE_ARG_POINTER(aLineFlags);

  nsTableFrame* parentFrame = nsnull; 
  if (NS_FAILED(nsTableFrame::GetTableFrame(this, parentFrame)))
    return NS_ERROR_FAILURE;

  nsTableCellMap* cellMap = parentFrame->GetCellMap();
  if (!cellMap)
     return NS_ERROR_FAILURE;

  if (aLineNumber >= cellMap->GetRowCount())
    return NS_ERROR_INVALID_ARG;
  
  *aLineFlags = 0;/// should we fill these in later?
  // not gonna touch aLineBounds right now

  CellData* firstCellData = cellMap->GetDataAt(aLineNumber, 0);
  if (!firstCellData)
    return NS_ERROR_FAILURE;

  *aFirstFrameOnLine = (nsIFrame*)firstCellData->GetCellFrame();
  if (!(*aFirstFrameOnLine))
  {
    while((aLineNumber > 0)&&(!(*aFirstFrameOnLine)))
    {
      aLineNumber--;
      firstCellData = cellMap->GetDataAt(aLineNumber, 0);
      *aFirstFrameOnLine = (nsIFrame*)firstCellData->GetCellFrame();
    }
  }
  *aNumFramesOnLine = cellMap->GetNumCellsOriginatingInRow(aLineNumber);
  return NS_OK;
}
  
NS_IMETHODIMP
nsTableRowGroupFrame::FindLineContaining(nsIFrame* aFrame, 
                                         PRInt32*  aLineNumberResult)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  NS_ENSURE_ARG_POINTER(aLineNumberResult);

  // make sure it is a rowFrame in the RowGroup
  // - it should be, but we do not validate in every case (see bug 88849)
  if (aFrame->GetType() != nsLayoutAtoms::tableRowFrame) {
    NS_WARNING("RowGroup contains a frame that is not a row");
    *aLineNumberResult = 0;
    return NS_ERROR_FAILURE;
  } 

  nsTableRowFrame* rowFrame = (nsTableRowFrame*)aFrame;
  *aLineNumberResult = rowFrame->GetRowIndex();

  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::FindLineAt(nscoord  aY, 
                                 PRInt32* aLineNumberResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#ifdef IBMBIDI
NS_IMETHODIMP
nsTableRowGroupFrame::CheckLineOrder(PRInt32                  aLine,
                                     PRBool                   *aIsReordered,
                                     nsIFrame                 **aFirstVisual,
                                     nsIFrame                 **aLastVisual)
{
  *aIsReordered = PR_FALSE;
  return NS_OK;
}
#endif // IBMBIDI
  
NS_IMETHODIMP
nsTableRowGroupFrame::FindFrameAt(PRInt32    aLineNumber, 
                                  nscoord    aX, 
#ifdef IBMBIDI
                                  PRBool     aCouldBeReordered,
#endif // IBMBIDI
                                  nsIFrame** aFrameFound,
                                  PRBool*    aXIsBeforeFirstFrame, 
                                  PRBool*    aXIsAfterLastFrame)
{
  PRInt32 colCount = 0;
  CellData* cellData;
  nsIFrame* tempFrame = nsnull;

  nsTableFrame* parentFrame = nsnull;
  nsTableFrame::GetTableFrame(this, parentFrame);
  nsTableCellMap* cellMap = parentFrame->GetCellMap();
  if (!cellMap)
     return NS_ERROR_FAILURE;

  colCount = cellMap->GetColCount();

  *aXIsBeforeFirstFrame = PR_FALSE;
  *aXIsAfterLastFrame = PR_FALSE;

  PRBool gotParentRect = PR_FALSE;
  for (PRInt32 i = 0; i < colCount; i++)
  {
    cellData = cellMap->GetDataAt(aLineNumber, i);
    if (!cellData)
      continue; // we hit a cellmap hole
    if (!cellData->IsOrig())
      continue;
    tempFrame = (nsIFrame*)cellData->GetCellFrame();

    if (!tempFrame)
      continue;
    
    nsRect tempRect = tempFrame->GetRect();//offsetting x to be in row coordinates
    if(!gotParentRect)
    {//only do this once
      nsIFrame* tempParentFrame = tempFrame->GetParent();
      if(!tempParentFrame)
        return NS_ERROR_FAILURE;

      aX -= tempParentFrame->GetPosition().x;
      gotParentRect = PR_TRUE;
    }

    if (i==0 &&(aX <= 0))//short circuit for negative x coords
    {
      *aXIsBeforeFirstFrame = PR_TRUE;
      *aFrameFound = tempFrame;
      return NS_OK;
    }
    if (aX < tempRect.x)
    {
      return NS_ERROR_FAILURE;
    }
    if(aX < tempRect.XMost())
    {
      *aFrameFound = tempFrame;
      return NS_OK;
    }
  }
  //x coord not found in frame, return last frame
  *aXIsAfterLastFrame = PR_TRUE;
  *aFrameFound = tempFrame;
  if (!(*aFrameFound))
    return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::GetNextSiblingOnLine(nsIFrame*& aFrame, 
                                           PRInt32    aLineNumber)
{
  NS_ENSURE_ARG_POINTER(aFrame);

  nsITableCellLayout* cellFrame;
  nsresult result = CallQueryInterface(aFrame, &cellFrame);
  if (NS_FAILED(result))
    return result;

  nsTableFrame* parentFrame = nsnull;
  result = nsTableFrame::GetTableFrame(this, parentFrame);
  nsTableCellMap* cellMap = parentFrame->GetCellMap();
  if (!cellMap)
     return NS_ERROR_FAILURE;


  PRInt32 colIndex;
  PRInt32& colIndexRef = colIndex;
  cellFrame->GetColIndex(colIndexRef);

  CellData* cellData = cellMap->GetDataAt(aLineNumber, colIndex + 1);
  
  if (!cellData)// if this isnt a valid cell, drop down and check the next line
  {
    cellData = cellMap->GetDataAt(aLineNumber + 1, 0);
    if (!cellData)
    {
      //*aFrame = nsnull;
      return NS_ERROR_FAILURE;
    }
  }

  aFrame = (nsIFrame*)cellData->GetCellFrame();
  if (!aFrame)
  {
    //PRInt32 numCellsInRow = cellMap->GetNumCellsOriginatingInRow(aLineNumber) - 1;
    PRInt32 tempCol = colIndex + 1;
    PRInt32 tempRow = aLineNumber;
    while ((tempCol > 0) && (!aFrame))
    {
      tempCol--;
      cellData = cellMap->GetDataAt(aLineNumber, tempCol);
      aFrame = (nsIFrame*)cellData->GetCellFrame();
      if (!aFrame && (tempCol==0))
      {
        while ((tempRow > 0) && (!aFrame))
        {
          tempRow--;
          cellData = cellMap->GetDataAt(tempRow, 0);
          aFrame = (nsIFrame*)cellData->GetCellFrame();
        }
      }
    }
  }
  return NS_OK;
}

//end nsLineIterator methods

