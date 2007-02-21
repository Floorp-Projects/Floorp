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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsCSSRendering.h"
#include "nsHTMLParts.h"
#include "nsCSSFrameConstructor.h"
#include "nsDisplayList.h"

#include "nsCellMap.h"//table cell navigation

nsTableRowGroupFrame::nsTableRowGroupFrame(nsStyleContext* aContext):
  nsHTMLContainerFrame(aContext)
{
  SetRepeatable(PR_FALSE);
}

nsTableRowGroupFrame::~nsTableRowGroupFrame()
{
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
    nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
    if (tableFrame) {
      return tableFrame->GetStartRowIndex(*this);
    }
  }
      
  return result;
}

void  nsTableRowGroupFrame::AdjustRowIndices(PRInt32 aRowIndex,
                                             PRInt32 anAdjustment)
{
  nsIFrame* rowFrame = GetFirstChild(nsnull);
  for ( ; rowFrame; rowFrame = rowFrame->GetNextSibling()) {
    if (NS_STYLE_DISPLAY_TABLE_ROW==rowFrame->GetStyleDisplay()->mDisplay) {
      PRInt32 index = ((nsTableRowFrame*)rowFrame)->GetRowIndex();
      if (index >= aRowIndex)
        ((nsTableRowFrame *)rowFrame)->SetRowIndex(index+anAdjustment);
    }
  }
}
nsresult
nsTableRowGroupFrame::InitRepeatedFrame(nsPresContext*       aPresContext,
                                        nsTableRowGroupFrame* aHeaderFooterFrame)
{
  nsTableRowFrame* copyRowFrame = GetFirstRow();
  nsTableRowFrame* originalRowFrame = aHeaderFooterFrame->GetFirstRow();
  AddStateBits(NS_REPEATED_ROW_OR_ROWGROUP);
  while (copyRowFrame && originalRowFrame) {
    copyRowFrame->AddStateBits(NS_REPEATED_ROW_OR_ROWGROUP);
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

static void
PaintRowGroupBackground(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                        const nsRect& aDirtyRect, nsPoint aPt)
{
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(aFrame);

  nsIRenderingContext::AutoPushTranslation translate(aCtx, aPt.x, aPt.y);
  TableBackgroundPainter painter(tableFrame,
                                 TableBackgroundPainter::eOrigin_TableRowGroup,
                                 aFrame->GetPresContext(), *aCtx,
                                 aDirtyRect - aPt);
  painter.PaintRowGroup(NS_STATIC_CAST(nsTableRowGroupFrame*, aFrame));
}

// Handle the child-traversal part of DisplayGenericTablePart
static nsresult
DisplayRows(nsDisplayListBuilder* aBuilder, nsFrame* aFrame,
            const nsRect& aDirtyRect, const nsDisplayListSet& aLists)
{
  nscoord overflowAbove;
  nsTableRowGroupFrame* f = NS_STATIC_CAST(nsTableRowGroupFrame*, aFrame);
  // Don't try to use the row cursor if we have to descend into placeholders;
  // we might have rows containing placeholders, where the row's overflow
  // area doesn't intersect the dirty rect but we need to descend into the row
  // to see out of flows
  nsIFrame* kid = f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO
    ? nsnull : f->GetFirstRowContaining(aDirtyRect.y, &overflowAbove);
  
  if (kid) {
    // have a cursor, use it
    while (kid) {
      if (kid->GetRect().y - overflowAbove >= aDirtyRect.YMost())
        break;
      nsresult rv = f->BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
      NS_ENSURE_SUCCESS(rv, rv);
      kid = kid->GetNextSibling();
    }
    return NS_OK;
  }
  
  // No cursor. Traverse children the hard way and build a cursor while we're at it
  nsTableRowGroupFrame::FrameCursorData* cursor = f->SetupRowCursor();
  kid = f->GetFirstChild(nsnull);
  while (kid) {
    nsresult rv = f->BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
    if (NS_FAILED(rv)) {
      f->ClearRowCursor();
      return rv;
    }
    
    if (cursor) {
      if (!cursor->AppendFrame(kid)) {
        f->ClearRowCursor();
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  
    kid = kid->GetNextSibling();
  }
  if (cursor) {
    cursor->FinishBuildingCursor();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                       const nsRect&           aDirtyRect,
                                       const nsDisplayListSet& aLists)
{
  if (!IsVisibleInSelection(aBuilder))
    return NS_OK;

  PRBool isRoot = aBuilder->IsAtRootOfPseudoStackingContext();
  if (isRoot) {
    // This background is created regardless of whether this frame is
    // visible or not. Visibility decisions are delegated to the
    // table background painter.
    nsresult rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(this, PaintRowGroupBackground, "TableRowGroupBackground"));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  return nsTableFrame::DisplayGenericTablePart(aBuilder, this, aDirtyRect,
                                               aLists, isRoot, DisplayRows);
}

PRIntn
nsTableRowGroupFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != GetPrevInFlow()) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != GetNextInFlow()) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame
void 
nsTableRowGroupFrame::PlaceChild(nsPresContext*        aPresContext,
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
nsTableRowGroupFrame::InitChildReflowState(nsPresContext&    aPresContext, 
                                           PRBool             aBorderCollapse,
                                           nsHTMLReflowState& aReflowState)                                    
{
  nsMargin collapseBorder;
  nsMargin padding(0,0,0,0);
  nsMargin* pCollapseBorder = nsnull;
  if (aBorderCollapse) {
    if (aReflowState.frame) {
      if (nsGkAtoms::tableRowFrame == aReflowState.frame->GetType()) {
        nsTableRowFrame* rowFrame = (nsTableRowFrame*)aReflowState.frame;
        pCollapseBorder = rowFrame->GetBCBorderWidth(collapseBorder);
      }
    }
  }
  aReflowState.Init(&aPresContext, -1, -1, pCollapseBorder, &padding);
}

static void
CacheRowHeightsForPrinting(nsPresContext*  aPresContext,
                           nsTableRowFrame* aFirstRow)
{
  for (nsTableRowFrame* row = aFirstRow; row; row = row->GetNextRow()) {
    if (!row->GetPrevInFlow()) {
      row->SetHasUnpaginatedHeight(PR_TRUE);
      row->SetUnpaginatedHeight(aPresContext, row->GetSize().height);
    }
  }
}

NS_METHOD 
nsTableRowGroupFrame::ReflowChildren(nsPresContext*        aPresContext,
                                     nsHTMLReflowMetrics&   aDesiredSize,
                                     nsRowGroupReflowState& aReflowState,
                                     nsReflowStatus&        aStatus,
                                     PRBool*                aPageBreakBeforeEnd)
{
  if (aPageBreakBeforeEnd) 
    *aPageBreakBeforeEnd = PR_FALSE;

  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (!tableFrame)
    ABORT1(NS_ERROR_NULL_POINTER);

  nsresult rv = NS_OK;

  PRBool borderCollapse = tableFrame->IsBorderCollapse();

  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  // XXXldb Should we really be checking this rather than available height?
  // (Think about multi-column layout!)
  PRBool isPaginated = aPresContext->IsPaginated();

  PRBool haveRow = PR_FALSE;
  PRBool reflowAllKids = aReflowState.reflowState.ShouldReflowAllKids() ||
                         tableFrame->IsGeometryDirty();
  PRBool needToCalcRowHeights = reflowAllKids;

  for (nsIFrame* kidFrame = mFrames.FirstChild(); kidFrame;
       kidFrame = kidFrame->GetNextSibling()) {
    if (kidFrame->GetType() != nsGkAtoms::tableRowFrame) {
      // XXXldb nsCSSFrameConstructor needs to enforce this!
      NS_NOTREACHED("yikes, a non-row child");
      continue;
    }

    haveRow = PR_TRUE;

    // Reflow the row frame
    if (reflowAllKids ||
        (kidFrame->GetStateBits() &
         (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) ||
        (aReflowState.reflowState.mFlags.mSpecialHeightReflow &&
         (isPaginated || (kidFrame->GetStateBits() &
                          NS_FRAME_CONTAINS_RELATIVE_HEIGHT)))) {
      nsSize oldKidSize = kidFrame->GetSize();

      // XXXldb We used to only pass aDesiredSize.mFlags through for the
      // incremental reflow codepath.
      nsHTMLReflowMetrics desiredSize(aDesiredSize.mFlags);
      desiredSize.width = desiredSize.height = 0;
  
      // Reflow the child into the available space, giving it as much height as
      // it wants. We'll deal with splitting later after we've computed the row
      // heights, taking into account cells with row spans...
      nsSize kidAvailSize(aReflowState.availSize.width, NS_UNCONSTRAINEDSIZE);
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState,
                                       kidFrame, kidAvailSize,
                                       -1, -1, PR_FALSE);
      InitChildReflowState(*aPresContext, borderCollapse, kidReflowState);

      // This can indicate that columns were resized.
      if (aReflowState.reflowState.mFlags.mHResize)
        kidReflowState.mFlags.mHResize = PR_TRUE;
     
      // If this isn't the first row, then we can't be at the top of the page
      if (kidFrame != GetFirstFrame()) {
        kidReflowState.mFlags.mIsTopOfPage = PR_FALSE;
      }

      rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                       0, aReflowState.y, 0, aStatus);

      // Place the child
      PlaceChild(aPresContext, aReflowState, kidFrame, desiredSize);
      aReflowState.y += cellSpacingY;

      if (!reflowAllKids) {
        if (IsSimpleRowFrame(aReflowState.tableFrame, kidFrame)) {
          // Inform the row of its new height.
          ((nsTableRowFrame*)kidFrame)->DidResize();
          // the overflow area may have changed inflate the overflow area
          if (aReflowState.tableFrame->IsAutoHeight()) {
            // Because other cells in the row may need to be aligned
            // differently, repaint the entire row
            nsRect kidRect(0, aReflowState.y,
                           desiredSize.width, desiredSize.height);
            Invalidate(kidRect);
            
            // Invalidate the area we're offseting. Note that we only
            // repaint within our existing frame bounds.
            if (kidRect.YMost() < mRect.height) {
              nsRect  dirtyRect(0, kidRect.YMost(),
                                mRect.width, mRect.height - kidRect.YMost());
              Invalidate(dirtyRect);
            }
          }
          else if (oldKidSize.height != desiredSize.height)
            needToCalcRowHeights = PR_TRUE;
        } else {
          needToCalcRowHeights = PR_TRUE;
        }
      }

      if (isPaginated && aPageBreakBeforeEnd && !*aPageBreakBeforeEnd) {
        nsTableRowFrame* nextRow = ((nsTableRowFrame*)kidFrame)->GetNextRow();
        if (nextRow) {
          *aPageBreakBeforeEnd = nsTableFrame::PageBreakAfter(*kidFrame, nextRow);
        }
      }
    } else {
      SlideChild(aReflowState, kidFrame);

      // Adjust the running y-offset so we know where the next row should be placed
      nscoord height = kidFrame->GetSize().height + cellSpacingY;
      aReflowState.y += height;

      if (NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height) {
        aReflowState.availSize.height -= height;
      }
    }
    ConsiderChildOverflow(aDesiredSize.mOverflowArea, kidFrame);
  }

  if (haveRow)
    aReflowState.y -= cellSpacingY;

  // Return our desired rect
  aDesiredSize.width = aReflowState.reflowState.availableWidth;
  aDesiredSize.height = aReflowState.y;

  if (aReflowState.reflowState.mFlags.mSpecialHeightReflow) {
    DidResizeRows(aDesiredSize);
    if (isPaginated) {
      CacheRowHeightsForPrinting(aPresContext, GetFirstRow());
    }
  }
  else if (needToCalcRowHeights) {
    CalculateRowHeights(aPresContext, aDesiredSize, aReflowState.reflowState);
    if (!reflowAllKids) {
      // Because we don't know what changed repaint everything.
      // XXX We should change CalculateRowHeights() to return the bounding
      // rect of what changed. Or whether anything moved or changed size...
      nsRect  dirtyRect(0, 0, mRect.width, mRect.height);
      Invalidate(dirtyRect);
    }
  }

  return rv;
}

nsTableRowFrame*  
nsTableRowGroupFrame::GetFirstRow() 
{
  for (nsIFrame* childFrame = GetFirstFrame(); childFrame;
       childFrame = childFrame->GetNextSibling()) {
    if (nsGkAtoms::tableRowFrame == childFrame->GetType()) {
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
nsTableRowGroupFrame::DidResizeRows(nsHTMLReflowMetrics& aDesiredSize)
{
  // update the cells spanning rows with their new heights
  // this is the place where all of the cells in the row get set to the height of the row
  // Reset the overflow area
  aDesiredSize.mOverflowArea = nsRect(0, 0, 0, 0);
  for (nsTableRowFrame* rowFrame = GetFirstRow();
       rowFrame; rowFrame = rowFrame->GetNextRow()) {
    rowFrame->DidResize();
    ConsiderChildOverflow(aDesiredSize.mOverflowArea, rowFrame);
  }
}

// This calculates the height of all the rows and takes into account 
// style height on the row group, style heights on rows and cells, style heights on rowspans. 
// Actual row heights will be adjusted later if the table has a style height.
// Even if rows don't change height, this method must be called to set the heights of each
// cell in the row to the height of its row.
void 
nsTableRowGroupFrame::CalculateRowHeights(nsPresContext*          aPresContext, 
                                          nsHTMLReflowMetrics&     aDesiredSize,
                                          const nsHTMLReflowState& aReflowState)
{
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (!tableFrame) return;

  PRBool isPaginated = aPresContext->IsPaginated();

  // all table cells have the same top and bottom margins, namely cellSpacingY
  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  PRInt32 numEffCols = tableFrame->GetEffectiveColCount();

  PRInt32 startRowIndex = GetStartRowIndex();
  // find the row corresponding to the row index we just found
  nsTableRowFrame* startRowFrame = GetFirstRow();

  if (!startRowFrame) return;

  // the current row group height is the y origin of the 1st row we are about to calculated a height for
  nscoord startRowGroupHeight = startRowFrame->GetPosition().y;

  PRInt32 numRows = GetRowCount() - (startRowFrame->GetRowIndex() - GetStartRowIndex());
  // collect the current height of each row.  nscoord* rowHeights = nsnull;
  if (numRows <= 0)
    return;

  nsTArray<RowInfo> rowInfo;
  if (!rowInfo.AppendElements(numRows)) {
    return;
  }

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
        rowInfo[rowIndex].pctHeight = nsTableFrame::RoundToPixel(rowFrame->GetHeight(pctHeightBasis));
      }
      rowInfo[rowIndex].hasStyleHeight = rowFrame->HasStyleHeight();
      nonPctHeight = nsTableFrame::RoundToPixel(PR_MAX(nonPctHeight, rowFrame->GetFixedHeight()));
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
      if (tableFrame->RowIsSpannedInto(rowIndex + startRowIndex, numEffCols)) {
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
      if (GetPrevInFlow() || tableFrame->RowHasSpanningCells(startRowIndex + rowIndex, numEffCols)) {
        nsTableCellFrame* cellFrame = rowFrame->GetFirstCell();
        // iteratate the row's cell frames 
        while (cellFrame) {
          PRInt32 rowSpan = tableFrame->GetEffectiveRowSpan(rowIndex + startRowIndex, *cellFrame);
          if ((rowIndex + rowSpan) > numRows) {
            // there might be rows pushed already to the nextInFlow
            rowSpan = numRows - rowIndex;
          }
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
              cellFrameSize.height += rowFrame->GetMaxCellAscent() -
                                      cellFrame->GetCellBaseline();
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
                      extraForRow = PR_MIN(nsTableFrame::RoundToPixel(extraForRow), extra - extraUsed);
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
                    extraForRow = PR_MIN(nsTableFrame::RoundToPixel(extraForRow), extra - extraUsed);
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
          extraForRow = PR_MIN(nsTableFrame::RoundToPixel(extraForRow), extraComputedHeight - extraUsed);
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
      nsTableFrame::RePositionViews(rowFrame);
    }
    yOrigin += rowHeight + cellSpacingY;
  }

  if (isPaginated && styleHeightAllocation) {
    // since the row group has a style height, cache the row heights, so next in flows can honor them 
    CacheRowHeightsForPrinting(aPresContext, GetFirstRow());
  }

  DidResizeRows(aDesiredSize);

  aDesiredSize.height = rowGroupHeight; // Adjust our desired size
}

nscoord
nsTableRowGroupFrame::CollapseRowGroupIfNecessary(nscoord aYTotalOffset,
                                                  nscoord aWidth)
{
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);

  const nsStyleVisibility* groupVis = GetStyleVisibility();
  PRBool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupVis->mVisible);
  if (collapseGroup) {
    tableFrame->SetNeedToCollapse(PR_TRUE);
  }

  nsRect overflowArea(0, 0, 0, 0);

  nsTableRowFrame* rowFrame= GetFirstRow();
  PRBool didCollapse = PR_FALSE;
  nscoord yGroupOffset = 0;
  while (rowFrame) {
    yGroupOffset += rowFrame->CollapseRowIfNecessary(yGroupOffset,
                                                     aWidth, collapseGroup,
                                                     didCollapse);
    ConsiderChildOverflow(overflowArea, rowFrame);
    rowFrame = rowFrame->GetNextRow();
  }

  nsRect groupRect = GetRect();
  groupRect.height -= yGroupOffset;
  if (didCollapse) {
    // add back the cellspacing between rowgroups
    groupRect.height += tableFrame->GetCellSpacingY();
  }

  groupRect.y -= aYTotalOffset;
  groupRect.width = aWidth;
  SetRect(groupRect);
  overflowArea.UnionRect(nsRect(0, 0, groupRect.width, groupRect.height),
                         overflowArea);
  FinishAndStoreOverflow(&overflowArea, nsSize(groupRect.width,
                                              groupRect.height));
  nsTableFrame::RePositionViews(this);
  return yGroupOffset;
}

// Move a child that was skipped during an incremental reflow.
// This function is not used for paginated mode so we don't need to deal
// with continuing frames, and it's only called if aKidFrame has no
// cells that span into it and no cells that span across it. That way
// we don't have to deal with rowspans
// XXX Is it still true that it's not used for paginated mode?
void
nsTableRowGroupFrame::SlideChild(nsRowGroupReflowState& aReflowState,
                                 nsIFrame*              aKidFrame)
{
  NS_PRECONDITION(NS_UNCONSTRAINEDSIZE == aReflowState.reflowState.availableHeight,
                  "we're not in galley mode");

  // Move the frame if we need to
  nsPoint oldPosition = aKidFrame->GetPosition();
  nsPoint newPosition = oldPosition;
  newPosition.y = aReflowState.y;
  if (oldPosition.y != newPosition.y) {
    aKidFrame->SetPosition(newPosition);
    nsTableFrame::RePositionViews(aKidFrame);
  }
}

// Create a continuing frame, add it to the child list, and then push it
// and the frames that follow
void 
nsTableRowGroupFrame::CreateContinuingRowFrame(nsPresContext& aPresContext,
                                               nsIFrame&       aRowFrame,
                                               nsIFrame**      aContRowFrame)
{
  // XXX what is the row index?
  if (!aContRowFrame) {NS_ASSERTION(PR_FALSE, "bad call"); return;}
  // create the continuing frame which will create continuing cell frames
  nsresult rv = aPresContext.PresShell()->FrameConstructor()->
    CreateContinuingFrame(&aPresContext, &aRowFrame, this, aContRowFrame);
  if (NS_FAILED(rv)) {
    *aContRowFrame = nsnull;
    return;
  }

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
nsTableRowGroupFrame::SplitSpanningCells(nsPresContext&          aPresContext,
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
        PRBool isTopOfPage = (row == &aFirstRow) && aFirstRowIsTopOfPage;
        nscoord cellHeight = row->ReflowCellFrame(&aPresContext, aReflowState,
                                                  isTopOfPage, cell,
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
nsTableRowGroupFrame::UndoContinuedRow(nsPresContext*  aPresContext,
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
  aRow->Destroy();
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
nsTableRowGroupFrame::SplitRowGroup(nsPresContext*          aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aReflowState,
                                    nsTableFrame*            aTableFrame,
                                    nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aPresContext->IsPaginated(), "SplitRowGroup currently supports only paged media"); 

  nsresult rv = NS_OK;
  nsTableRowFrame* prevRowFrame = nsnull;
  aDesiredSize.height = 0;

  nscoord availWidth  = (NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) ?
                        NS_UNCONSTRAINEDSIZE :
                        nsTableFrame::RoundToPixel(aReflowState.availableWidth);
  nscoord availHeight = (NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight) ?
                        NS_UNCONSTRAINEDSIZE :
                        nsTableFrame::RoundToPixel(aReflowState.availableHeight);
  
  PRBool  borderCollapse = ((nsTableFrame*)aTableFrame->GetFirstInFlow())->IsBorderCollapse();
  nscoord cellSpacingY   = aTableFrame->GetCellSpacingY();
  
  // get the page height
  nscoord pageHeight = aPresContext->GetPageSize().height;
  NS_ASSERTION(pageHeight != NS_UNCONSTRAINEDSIZE, 
               "The table shouldn't be split when there should be space");

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

        nsHTMLReflowState rowReflowState(aPresContext, aReflowState,
                                         rowFrame, availSize,
                                         -1, -1, PR_FALSE);
                                         
        InitChildReflowState(*aPresContext, borderCollapse, rowReflowState);
        rowReflowState.mFlags.mIsTopOfPage = isTopOfPage; // set top of page
        nsHTMLReflowMetrics rowMetrics;

        // Reflow the cell with the constrained height. A cell with rowspan >1 will get this
        // reflow later during SplitSpanningCells.
        rv = ReflowChild(rowFrame, aPresContext, rowMetrics, rowReflowState,
                         0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
        if (NS_FAILED(rv)) return rv;
        rowFrame->SetSize(nsSize(rowMetrics.width, rowMetrics.height));
        rowFrame->DidReflow(aPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);
        rowFrame->DidResize();

        if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
          // The row frame is incomplete and all of the rowspan 1 cells' block frames split
          if ((rowMetrics.height <= rowReflowState.availableHeight) || isTopOfPage) {
            // The row stays on this page because either it split ok or we're on the top of page.
            // If top of page and the height exceeded the avail height, then there will be data loss
            NS_ASSERTION(rowMetrics.height <= rowReflowState.availableHeight, 
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
          // The row frame is complete because either (1) its minimum height is greater than the 
          // available height we gave it, or (2) it may have been given a larger height through 
          // style than its content, or (3) it contains a rowspan >1 cell which hasn't been
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
            contRow = nsnull;
          }
        }
        else { // (firstTruncatedRow != firstRowThisPage)
          // Try to put firstTruncateRow on the next page 
          nsTableRowFrame* rowBefore = ::GetRowBefore(*firstRowThisPage, *firstTruncatedRow);
          availHeight -= rowBefore->GetRect().YMost();

          UndoContinuedRow(aPresContext, contRow);
          contRow = nsnull;
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
              contRow = nsnull;
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
              contRow = nsnull;
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
nsTableRowGroupFrame::Reflow(nsPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableRowGroupFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  nsresult rv = NS_OK;
  aStatus     = NS_FRAME_COMPLETE;
        
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (!tableFrame) return NS_ERROR_NULL_POINTER;

  // Row geometry may be going to change so we need to invalidate any row cursor.
  ClearRowCursor();

  // see if a special height reflow needs to occur due to having a pct height
  nsTableFrame::CheckRequestSpecialHeightReflow(aReflowState);

  nsRowGroupReflowState state(aReflowState, tableFrame);
  const nsStyleVisibility* groupVis = GetStyleVisibility();
  PRBool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupVis->mVisible);
  if (collapseGroup) {
    tableFrame->SetNeedToCollapse(PR_TRUE);
  }

  // Check for an overflow list
  MoveOverflowToChildList(aPresContext);

  // Reflow the existing frames. 
  PRBool splitDueToPageBreak = PR_FALSE;
  rv = ReflowChildren(aPresContext, aDesiredSize, state, aStatus,
                      &splitDueToPageBreak);

  // See if all the frames fit. Do not try to split anything if we're
  // not paginated ... we can't split across columns yet.
  if (aReflowState.mFlags.mTableIsSplittable &&
      (NS_FRAME_NOT_COMPLETE == aStatus || splitDueToPageBreak || 
       aDesiredSize.height > aReflowState.availableHeight)) {
    // Nope, find a place to split the row group 
    PRBool specialReflow = (PRBool)aReflowState.mFlags.mSpecialHeightReflow;
    ((nsHTMLReflowState::ReflowStateFlags&)aReflowState.mFlags).mSpecialHeightReflow = PR_FALSE;

    SplitRowGroup(aPresContext, aDesiredSize, aReflowState, tableFrame, aStatus);

    ((nsHTMLReflowState::ReflowStateFlags&)aReflowState.mFlags).mSpecialHeightReflow = specialReflow;
  }

  // If we have a next-in-flow, then we're not complete
  // XXXldb This used to be done only for the incremental reflow codepath.
  if (GetNextInFlow()) {
    aStatus = NS_FRAME_NOT_COMPLETE;
  }

  SetHasStyleHeight((NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) &&
                    (aReflowState.mComputedHeight > 0)); 
  
  // just set our width to what was available. The table will calculate the width and not use our value.
  aDesiredSize.width = aReflowState.availableWidth;

  // if we have a nextinflow we are not complete
  if (GetNextInFlow()) {
    aStatus |= NS_FRAME_NOT_COMPLETE;
  }
  aDesiredSize.mOverflowArea.UnionRect(aDesiredSize.mOverflowArea, nsRect(0, 0, aDesiredSize.width,
	                                                                      aDesiredSize.height)); 
  FinishAndStoreOverflow(&aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

NS_IMETHODIMP
nsTableRowGroupFrame::AppendFrames(nsIAtom*        aListName,
                                   nsIFrame*       aFrameList)
{
  NS_ASSERTION(!aListName, "unexpected child list");

  ClearRowCursor();

  // collect the new row frames in an array
  nsAutoVoidArray rows;
  for (nsIFrame* rowFrame = aFrameList; rowFrame;
       rowFrame = rowFrame->GetNextSibling()) {
    if (nsGkAtoms::tableRowFrame == rowFrame->GetType()) {
      rows.AppendElement(rowFrame);
    }
  }

  PRInt32 rowIndex = GetRowCount();
  // Append the frames to the sibling chain
  mFrames.AppendFrames(nsnull, aFrameList);

  if (rows.Count() > 0) {
    nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
    if (tableFrame) {
      tableFrame->AppendRows(*this, rowIndex, rows);
      AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
      GetPresContext()->PresShell()->FrameNeedsReflow(this,
                                                    nsIPresShell::eTreeChange);
      tableFrame->SetGeometryDirty();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::InsertFrames(nsIAtom*        aListName,
                                   nsIFrame*       aPrevFrame,
                                   nsIFrame*       aFrameList)
{
  NS_ASSERTION(!aListName, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  ClearRowCursor();

  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (!tableFrame)
    return NS_ERROR_NULL_POINTER;

  // collect the new row frames in an array
  nsVoidArray rows;
  PRBool gotFirstRow = PR_FALSE;
  for (nsIFrame* rowFrame = aFrameList; rowFrame;
       rowFrame = rowFrame->GetNextSibling()) {
    if (nsGkAtoms::tableRowFrame == rowFrame->GetType()) {
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
    nsTableRowFrame* prevRow = (nsTableRowFrame *)nsTableFrame::GetFrameAtOrBefore(this, aPrevFrame, nsGkAtoms::tableRowFrame);
    PRInt32 rowIndex = (prevRow) ? prevRow->GetRowIndex() + 1 : startRowIndex;
    tableFrame->InsertRows(*this, rows, rowIndex, PR_TRUE);

    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
    GetPresContext()->PresShell()->FrameNeedsReflow(this,
                                                    nsIPresShell::eTreeChange);
    tableFrame->SetGeometryDirty();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::RemoveFrame(nsIAtom*        aListName,
                                  nsIFrame*       aOldFrame)
{
  NS_ASSERTION(!aListName, "unexpected child list");

  ClearRowCursor();

  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (tableFrame) {
    if (nsGkAtoms::tableRowFrame == aOldFrame->GetType()) {
      // remove the rows from the table (and flag a rebalance)
      tableFrame->RemoveRows((nsTableRowFrame &)*aOldFrame, 1, PR_TRUE);

      AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
      GetPresContext()->PresShell()->FrameNeedsReflow(this,
                                                    nsIPresShell::eTreeChange);
      tableFrame->SetGeometryDirty();
    }
  }
  mFrames.DestroyFrame(aOldFrame);

  return NS_OK;
}

/* virtual */ nsMargin
nsTableRowGroupFrame::GetUsedMargin() const
{
  return nsMargin(0,0,0,0);
}

/* virtual */ nsMargin
nsTableRowGroupFrame::GetUsedBorder() const
{
  return nsMargin(0,0,0,0);
}

/* virtual */ nsMargin
nsTableRowGroupFrame::GetUsedPadding() const
{
  return nsMargin(0,0,0,0);
}

nscoord 
nsTableRowGroupFrame::GetHeightBasis(const nsHTMLReflowState& aReflowState)
{
  nscoord result = 0;
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
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

PRBool
nsTableRowGroupFrame::IsSimpleRowFrame(nsTableFrame* aTableFrame,
                                       nsIFrame*     aFrame)
{
  // Make sure it's a row frame and not a row group frame
  if (aFrame->GetType() == nsGkAtoms::tableRowFrame) {
    PRInt32 rowIndex = ((nsTableRowFrame*)aFrame)->GetRowIndex();
    
    // It's a simple row frame if there are no cells that span into or
    // across the row
    PRInt32 numEffCols = aTableFrame->GetEffectiveColCount();
    if (!aTableFrame->RowIsSpannedInto(rowIndex, numEffCols) &&
        !aTableFrame->RowHasSpanningCells(rowIndex, numEffCols)) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsIAtom*
nsTableRowGroupFrame::GetType() const
{
  return nsGkAtoms::tableRowGroupFrame;
}


/* ----- global methods ----- */

nsIFrame*
NS_NewTableRowGroupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableRowGroupFrame(aContext);
}

NS_IMETHODIMP
nsTableRowGroupFrame::Init(nsIContent*      aContent,
                           nsIFrame*        aParent,
                           nsIFrame*        aPrevInFlow)
{
  // Let the base class do its processing
  nsresult rv = nsHTMLContainerFrame::Init(aContent, aParent, aPrevInFlow);

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
nsTableRowGroupFrame::GetBCBorderWidth(nsMargin& aBorder)
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
    aBorder.top    = nsPresContext::CSSPixelsToAppUnits(firstRowFrame->GetTopBCBorderWidth());
    aBorder.bottom = nsPresContext::CSSPixelsToAppUnits(lastRowFrame->GetBottomBCBorderWidth());
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

  nsTableFrame* parentFrame = nsTableFrame::GetTableFrame(this);
  if (!parentFrame)
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

  *aNumFramesOnLine = cellMap->GetNumCellsOriginatingInRow(aLineNumber);
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
  if  (!(*aFirstFrameOnLine)) {
    NS_ERROR("Failed to find cell frame for cell data");
    *aNumFramesOnLine = 0;
  }
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
  if (aFrame->GetType() != nsGkAtoms::tableRowFrame) {
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
  *aFirstVisual = nsnull;
  *aLastVisual = nsnull;
  return NS_OK;
}
#endif // IBMBIDI
  
NS_IMETHODIMP
nsTableRowGroupFrame::FindFrameAt(PRInt32    aLineNumber, 
                                  nscoord    aX, 
                                  nsIFrame** aFrameFound,
                                  PRBool*    aXIsBeforeFirstFrame, 
                                  PRBool*    aXIsAfterLastFrame)
{
  PRInt32 colCount = 0;
  CellData* cellData;
  nsIFrame* tempFrame = nsnull;

  nsTableFrame* parentFrame = nsTableFrame::GetTableFrame(this);
  if (!parentFrame)
    return NS_ERROR_FAILURE;

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

  nsTableFrame* parentFrame = nsTableFrame::GetTableFrame(this);
  if (!parentFrame)
    return NS_ERROR_FAILURE;
  nsTableCellMap* cellMap = parentFrame->GetCellMap();
  if (!cellMap)
     return NS_ERROR_FAILURE;


  PRInt32 colIndex;
  PRInt32& colIndexRef = colIndex;
  cellFrame->GetColIndex(colIndexRef);

  CellData* cellData = cellMap->GetDataAt(aLineNumber, colIndex + 1);
  
  if (!cellData)// if this isn't a valid cell, drop down and check the next line
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

static void
DestroyFrameCursorData(void* aObject, nsIAtom* aPropertyName,
                       void* aPropertyValue, void* aData)
{
  delete NS_STATIC_CAST(nsTableRowGroupFrame::FrameCursorData*, aPropertyValue);
}

void
nsTableRowGroupFrame::ClearRowCursor()
{
  if (!(GetStateBits() & NS_ROWGROUP_HAS_ROW_CURSOR))
    return;

  RemoveStateBits(NS_ROWGROUP_HAS_ROW_CURSOR);
  DeleteProperty(nsGkAtoms::rowCursorProperty);
}

nsTableRowGroupFrame::FrameCursorData*
nsTableRowGroupFrame::SetupRowCursor()
{
  if (GetStateBits() & NS_ROWGROUP_HAS_ROW_CURSOR) {
    // We already have a valid row cursor. Don't waste time rebuilding it.
    return nsnull;
  }

  nsIFrame* f = mFrames.FirstChild();
  PRInt32 count;
  for (count = 0; f && count < MIN_ROWS_NEEDING_CURSOR; ++count) {
    f = f->GetNextSibling();
  }
  if (!f) {
    // Less than MIN_ROWS_NEEDING_CURSOR rows, so just don't bother
    return nsnull;
  }

  FrameCursorData* data = new FrameCursorData();
  if (!data)
    return nsnull;
  nsresult rv = SetProperty(nsGkAtoms::rowCursorProperty, data,
                            DestroyFrameCursorData);
  if (NS_FAILED(rv)) {
    delete data;
    return nsnull;
  }
  AddStateBits(NS_ROWGROUP_HAS_ROW_CURSOR);
  return data;
}

nsIFrame*
nsTableRowGroupFrame::GetFirstRowContaining(nscoord aY, nscoord* aOverflowAbove)
{
  if (!(GetStateBits() & NS_ROWGROUP_HAS_ROW_CURSOR))
    return nsnull;

  FrameCursorData* property = NS_STATIC_CAST(FrameCursorData*,
    GetProperty(nsGkAtoms::rowCursorProperty));
  PRUint32 cursorIndex = property->mCursorIndex;
  PRUint32 frameCount = property->mFrames.Length();
  if (cursorIndex >= frameCount)
    return nsnull;
  nsIFrame* cursorFrame = property->mFrames[cursorIndex];

  // The cursor's frame list excludes frames with empty overflow-area, so
  // we don't need to check that here.
  
  // We use property->mOverflowBelow here instead of computing the frame's
  // true overflowArea.YMost(), because it is essential for the thresholds
  // to form a monotonically increasing sequence. Otherwise we would break
  // encountering a row whose overflowArea.YMost() is <= aY but which has
  // a row above it containing cell(s) that span to include aY.
  while (cursorIndex > 0 &&
         cursorFrame->GetRect().YMost() + property->mOverflowBelow > aY) {
    --cursorIndex;
    cursorFrame = property->mFrames[cursorIndex];
  }
  while (cursorIndex + 1 < frameCount &&
         cursorFrame->GetRect().YMost() + property->mOverflowBelow <= aY) {
    ++cursorIndex;
    cursorFrame = property->mFrames[cursorIndex];
  }

  property->mCursorIndex = cursorIndex;
  *aOverflowAbove = property->mOverflowAbove;
  return cursorFrame;
}

PRBool
nsTableRowGroupFrame::FrameCursorData::AppendFrame(nsIFrame* aFrame)
{
  nsRect overflowRect = aFrame->GetOverflowRect();
  if (overflowRect.IsEmpty())
    return PR_TRUE;
  nscoord overflowAbove = -overflowRect.y;
  nscoord overflowBelow = overflowRect.YMost() - aFrame->GetSize().height;
  mOverflowAbove = PR_MAX(mOverflowAbove, overflowAbove);
  mOverflowBelow = PR_MAX(mOverflowBelow, overflowBelow);
  return mFrames.AppendElement(aFrame) != nsnull;
}
