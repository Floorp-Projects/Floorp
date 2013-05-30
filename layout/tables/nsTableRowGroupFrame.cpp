/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsTableRowGroupFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
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
#include <algorithm>

using namespace mozilla;
using namespace mozilla::layout;

nsTableRowGroupFrame::nsTableRowGroupFrame(nsStyleContext* aContext):
  nsContainerFrame(aContext)
{
  SetRepeatable(false);
}

nsTableRowGroupFrame::~nsTableRowGroupFrame()
{
}

NS_QUERYFRAME_HEAD(nsTableRowGroupFrame)
  NS_QUERYFRAME_ENTRY(nsTableRowGroupFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

int32_t
nsTableRowGroupFrame::GetRowCount()
{
#ifdef DEBUG
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    NS_ASSERTION(e.get()->StyleDisplay()->mDisplay ==
                   NS_STYLE_DISPLAY_TABLE_ROW,
                 "Unexpected display");
    NS_ASSERTION(e.get()->GetType() == nsGkAtoms::tableRowFrame,
                 "Unexpected frame type");
  }
#endif
  
  return mFrames.GetLength();
}

int32_t nsTableRowGroupFrame::GetStartRowIndex()
{
  int32_t result = -1;
  if (mFrames.NotEmpty()) {
    NS_ASSERTION(mFrames.FirstChild()->GetType() == nsGkAtoms::tableRowFrame,
                 "Unexpected frame type");
    result = static_cast<nsTableRowFrame*>(mFrames.FirstChild())->GetRowIndex();
  }
  // if the row group doesn't have any children, get it the hard way
  if (-1 == result) {
    nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
    return tableFrame->GetStartRowIndex(this);
  }
      
  return result;
}

void  nsTableRowGroupFrame::AdjustRowIndices(int32_t aRowIndex,
                                             int32_t anAdjustment)
{
  nsIFrame* rowFrame = GetFirstPrincipalChild();
  for ( ; rowFrame; rowFrame = rowFrame->GetNextSibling()) {
    if (NS_STYLE_DISPLAY_TABLE_ROW==rowFrame->StyleDisplay()->mDisplay) {
      int32_t index = ((nsTableRowFrame*)rowFrame)->GetRowIndex();
      if (index >= aRowIndex)
        ((nsTableRowFrame *)rowFrame)->SetRowIndex(index+anAdjustment);
    }
  }
}
nsresult
nsTableRowGroupFrame::InitRepeatedFrame(nsPresContext*        aPresContext,
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
      int32_t colIndex;
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

/**
 * We need a custom display item for table row backgrounds. This is only used
 * when the table row is the root of a stacking context (e.g., has 'opacity').
 * Table row backgrounds can extend beyond the row frame bounds, when
 * the row contains row-spanning cells.
 */
class nsDisplayTableRowGroupBackground : public nsDisplayTableItem {
public:
  nsDisplayTableRowGroupBackground(nsDisplayListBuilder* aBuilder,
                                   nsTableRowGroupFrame* aFrame) :
    nsDisplayTableItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayTableRowGroupBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTableRowGroupBackground() {
    MOZ_COUNT_DTOR(nsDisplayTableRowGroupBackground);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);

  NS_DISPLAY_DECL_NAME("TableRowGroupBackground", TYPE_TABLE_ROW_GROUP_BACKGROUND)
};

void
nsDisplayTableRowGroupBackground::Paint(nsDisplayListBuilder* aBuilder,
                                        nsRenderingContext* aCtx)
{
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(mFrame);
  TableBackgroundPainter painter(tableFrame,
                                 TableBackgroundPainter::eOrigin_TableRowGroup,
                                 mFrame->PresContext(), *aCtx,
                                 mVisibleRect, ToReferenceFrame(),
                                 aBuilder->GetBackgroundPaintFlags());
  painter.PaintRowGroup(static_cast<nsTableRowGroupFrame*>(mFrame));
}

// Handle the child-traversal part of DisplayGenericTablePart
static void
DisplayRows(nsDisplayListBuilder* aBuilder, nsFrame* aFrame,
            const nsRect& aDirtyRect, const nsDisplayListSet& aLists)
{
  nscoord overflowAbove;
  nsTableRowGroupFrame* f = static_cast<nsTableRowGroupFrame*>(aFrame);
  // Don't try to use the row cursor if we have to descend into placeholders;
  // we might have rows containing placeholders, where the row's overflow
  // area doesn't intersect the dirty rect but we need to descend into the row
  // to see out of flows.
  // Note that we really want to check ShouldDescendIntoFrame for all
  // the rows in |f|, but that's exactly what we're trying to avoid, so we
  // approximate it by checking it for |f|: if it's true for any row
  // in |f| then it's true for |f| itself.
  nsIFrame* kid = aBuilder->ShouldDescendIntoFrame(f) ?
    nullptr : f->GetFirstRowContaining(aDirtyRect.y, &overflowAbove);
  
  if (kid) {
    // have a cursor, use it
    while (kid) {
      if (kid->GetRect().y - overflowAbove >= aDirtyRect.YMost())
        break;
      f->BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
      kid = kid->GetNextSibling();
    }
    return;
  }
  
  // No cursor. Traverse children the hard way and build a cursor while we're at it
  nsTableRowGroupFrame::FrameCursorData* cursor = f->SetupRowCursor();
  kid = f->GetFirstPrincipalChild();
  while (kid) {
    f->BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
    
    if (cursor) {
      if (!cursor->AppendFrame(kid)) {
        f->ClearRowCursor();
        return;
      }
    }
  
    kid = kid->GetNextSibling();
  }
  if (cursor) {
    cursor->FinishBuildingCursor();
  }
}

void
nsTableRowGroupFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                       const nsRect&           aDirtyRect,
                                       const nsDisplayListSet& aLists)
{
  nsDisplayTableItem* item = nullptr;
  if (IsVisibleInSelection(aBuilder)) {
    bool isRoot = aBuilder->IsAtRootOfPseudoStackingContext();
    if (isRoot) {
      // This background is created regardless of whether this frame is
      // visible or not. Visibility decisions are delegated to the
      // table background painter.
      item = new (aBuilder) nsDisplayTableRowGroupBackground(aBuilder, this);
      aLists.BorderBackground()->AppendNewToTop(item);
    }
  }  
  nsTableFrame::DisplayGenericTablePart(aBuilder, this, aDirtyRect,
                                        aLists, item, DisplayRows);
}

int
nsTableRowGroupFrame::GetSkipSides() const
{
  int skip = 0;
  if (nullptr != GetPrevInFlow()) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nullptr != GetNextInFlow()) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame
void 
nsTableRowGroupFrame::PlaceChild(nsPresContext*         aPresContext,
                                 nsRowGroupReflowState& aReflowState,
                                 nsIFrame*              aKidFrame,
                                 nsHTMLReflowMetrics&   aDesiredSize,
                                 const nsRect&          aOriginalKidRect,
                                 const nsRect&          aOriginalKidVisualOverflow)
{
  bool isFirstReflow =
    (aKidFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW) != 0;

  // Place and size the child
  FinishReflowChild(aKidFrame, aPresContext, nullptr, aDesiredSize, 0,
                    aReflowState.y, 0);

  nsTableFrame::InvalidateTableFrame(aKidFrame, aOriginalKidRect,
                                     aOriginalKidVisualOverflow, isFirstReflow);

  // Adjust the running y-offset
  aReflowState.y += aDesiredSize.height;

  // If our height is constrained then update the available height
  if (NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height) {
    aReflowState.availSize.height -= aDesiredSize.height;
  }
}

void
nsTableRowGroupFrame::InitChildReflowState(nsPresContext&     aPresContext, 
                                           bool               aBorderCollapse,
                                           nsHTMLReflowState& aReflowState)                                    
{
  nsMargin collapseBorder;
  nsMargin padding(0,0,0,0);
  nsMargin* pCollapseBorder = nullptr;
  if (aBorderCollapse) {
    nsTableRowFrame *rowFrame = do_QueryFrame(aReflowState.frame);
    if (rowFrame) {
      pCollapseBorder = rowFrame->GetBCBorderWidth(collapseBorder);
    }
  }
  aReflowState.Init(&aPresContext, -1, -1, pCollapseBorder, &padding);
}

static void
CacheRowHeightsForPrinting(nsPresContext*   aPresContext,
                           nsTableRowFrame* aFirstRow)
{
  for (nsTableRowFrame* row = aFirstRow; row; row = row->GetNextRow()) {
    if (!row->GetPrevInFlow()) {
      row->SetHasUnpaginatedHeight(true);
      row->SetUnpaginatedHeight(aPresContext, row->GetSize().height);
    }
  }
}

nsresult
nsTableRowGroupFrame::ReflowChildren(nsPresContext*         aPresContext,
                                     nsHTMLReflowMetrics&   aDesiredSize,
                                     nsRowGroupReflowState& aReflowState,
                                     nsReflowStatus&        aStatus,
                                     bool*                aPageBreakBeforeEnd)
{
  if (aPageBreakBeforeEnd) 
    *aPageBreakBeforeEnd = false;

  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  nsresult rv = NS_OK;
  const bool borderCollapse = tableFrame->IsBorderCollapse();
  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  // XXXldb Should we really be checking this rather than available height?
  // (Think about multi-column layout!)
  bool isPaginated = aPresContext->IsPaginated() && 
                       NS_UNCONSTRAINEDSIZE != aReflowState.availSize.height;

  bool haveRow = false;
  bool reflowAllKids = aReflowState.reflowState.ShouldReflowAllKids() ||
                         tableFrame->IsGeometryDirty();
  bool needToCalcRowHeights = reflowAllKids;

  nsIFrame *prevKidFrame = nullptr;
  for (nsIFrame* kidFrame = mFrames.FirstChild(); kidFrame;
       prevKidFrame = kidFrame, kidFrame = kidFrame->GetNextSibling()) {
    nsTableRowFrame *rowFrame = do_QueryFrame(kidFrame);
    if (!rowFrame) {
      // XXXldb nsCSSFrameConstructor needs to enforce this!
      NS_NOTREACHED("yikes, a non-row child");
      continue;
    }

    haveRow = true;

    // Reflow the row frame
    if (reflowAllKids ||
        NS_SUBTREE_DIRTY(kidFrame) ||
        (aReflowState.reflowState.mFlags.mSpecialHeightReflow &&
         (isPaginated || (kidFrame->GetStateBits() &
                          NS_FRAME_CONTAINS_RELATIVE_HEIGHT)))) {
      nsRect oldKidRect = kidFrame->GetRect();
      nsRect oldKidVisualOverflow = kidFrame->GetVisualOverflowRect();

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
                                       -1, -1, false);
      InitChildReflowState(*aPresContext, borderCollapse, kidReflowState);

      // This can indicate that columns were resized.
      if (aReflowState.reflowState.mFlags.mHResize)
        kidReflowState.mFlags.mHResize = true;
     
      NS_ASSERTION(kidFrame == mFrames.FirstChild() || prevKidFrame, 
                   "If we're not on the first frame, we should have a "
                   "previous sibling...");
      // If prev row has nonzero YMost, then we can't be at the top of the page
      if (prevKidFrame && prevKidFrame->GetRect().YMost() > 0) {
        kidReflowState.mFlags.mIsTopOfPage = false;
      }

      rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                       0, aReflowState.y, 0, aStatus);

      // Place the child
      PlaceChild(aPresContext, aReflowState, kidFrame, desiredSize,
                 oldKidRect, oldKidVisualOverflow);
      aReflowState.y += cellSpacingY;

      if (!reflowAllKids) {
        if (IsSimpleRowFrame(aReflowState.tableFrame, kidFrame)) {
          // Inform the row of its new height.
          rowFrame->DidResize();
          // the overflow area may have changed inflate the overflow area
          const nsStylePosition *stylePos = StylePosition();
          nsStyleUnit unit = stylePos->mHeight.GetUnit();
          if (aReflowState.tableFrame->IsAutoHeight() &&
              unit != eStyleUnit_Coord) {
            // Because other cells in the row may need to be aligned
            // differently, repaint the entire row
            nsRect kidRect(0, aReflowState.y,
                           desiredSize.width, desiredSize.height);
            InvalidateFrame();
          }
          else if (oldKidRect.height != desiredSize.height)
            needToCalcRowHeights = true;
        } else {
          needToCalcRowHeights = true;
        }
      }

      if (isPaginated && aPageBreakBeforeEnd && !*aPageBreakBeforeEnd) {
        nsTableRowFrame* nextRow = rowFrame->GetNextRow();
        if (nextRow) {
          *aPageBreakBeforeEnd = nsTableFrame::PageBreakAfter(kidFrame, nextRow);
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
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, kidFrame);
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
      InvalidateFrame();
    }
  }

  return rv;
}

nsTableRowFrame*  
nsTableRowGroupFrame::GetFirstRow() 
{
  for (nsIFrame* childFrame = mFrames.FirstChild(); childFrame;
       childFrame = childFrame->GetNextSibling()) {
    nsTableRowFrame *rowFrame = do_QueryFrame(childFrame);
    if (rowFrame) {
      return rowFrame;
    }
  }
  return nullptr;
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
  aDesiredSize.mOverflowAreas.Clear();
  for (nsTableRowFrame* rowFrame = GetFirstRow();
       rowFrame; rowFrame = rowFrame->GetNextRow()) {
    rowFrame->DidResize();
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, rowFrame);
  }
}

// This calculates the height of all the rows and takes into account 
// style height on the row group, style heights on rows and cells, style heights on rowspans. 
// Actual row heights will be adjusted later if the table has a style height.
// Even if rows don't change height, this method must be called to set the heights of each
// cell in the row to the height of its row.
void 
nsTableRowGroupFrame::CalculateRowHeights(nsPresContext*           aPresContext, 
                                          nsHTMLReflowMetrics&     aDesiredSize,
                                          const nsHTMLReflowState& aReflowState)
{
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  const bool isPaginated = aPresContext->IsPaginated();

  // all table cells have the same top and bottom margins, namely cellSpacingY
  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  int32_t numEffCols = tableFrame->GetEffectiveColCount();

  int32_t startRowIndex = GetStartRowIndex();
  // find the row corresponding to the row index we just found
  nsTableRowFrame* startRowFrame = GetFirstRow();

  if (!startRowFrame) return;

  // the current row group height is the y origin of the 1st row we are about to calculated a height for
  nscoord startRowGroupHeight = startRowFrame->GetPosition().y;

  int32_t numRows = GetRowCount() - (startRowFrame->GetRowIndex() - GetStartRowIndex());
  // collect the current height of each row.  nscoord* rowHeights = nullptr;
  if (numRows <= 0)
    return;

  nsTArray<RowInfo> rowInfo;
  if (!rowInfo.AppendElements(numRows)) {
    return;
  }

  bool    hasRowSpanningCell = false;
  nscoord heightOfRows = 0;
  nscoord heightOfUnStyledRows = 0;
  // Get the height of each row without considering rowspans. This will be the max of 
  // the largest desired height of each cell, the largest style height of each cell, 
  // the style height of the row.
  nscoord pctHeightBasis = GetHeightBasis(aReflowState);
  int32_t rowIndex; // the index in rowInfo, not among the rows in the row group
  nsTableRowFrame* rowFrame;
  for (rowFrame = startRowFrame, rowIndex = 0; rowFrame; rowFrame = rowFrame->GetNextRow(), rowIndex++) {
    nscoord nonPctHeight = rowFrame->GetContentHeight();
    if (isPaginated) {
      nonPctHeight = std::max(nonPctHeight, rowFrame->GetSize().height);
    }
    if (!rowFrame->GetPrevInFlow()) {
      if (rowFrame->HasPctHeight()) {
        rowInfo[rowIndex].hasPctHeight = true;
        rowInfo[rowIndex].pctHeight = rowFrame->GetHeight(pctHeightBasis);
      }
      rowInfo[rowIndex].hasStyleHeight = rowFrame->HasStyleHeight();
      nonPctHeight = std::max(nonPctHeight, rowFrame->GetFixedHeight());
    }
    UpdateHeights(rowInfo[rowIndex], nonPctHeight, heightOfRows, heightOfUnStyledRows);

    if (!rowInfo[rowIndex].hasStyleHeight) {
      if (isPaginated || tableFrame->HasMoreThanOneCell(rowIndex + startRowIndex)) {
        rowInfo[rowIndex].isSpecial = true;
        // iteratate the row's cell frames to see if any do not have rowspan > 1
        nsTableCellFrame* cellFrame = rowFrame->GetFirstCell();
        while (cellFrame) {
          int32_t rowSpan = tableFrame->GetEffectiveRowSpan(rowIndex + startRowIndex, *cellFrame);
          if (1 == rowSpan) { 
            rowInfo[rowIndex].isSpecial = false;
            break;
          }
          cellFrame = cellFrame->GetNextCell(); 
        }
      }
    }
    // See if a cell spans into the row. If so we'll have to do the next step
    if (!hasRowSpanningCell) {
      if (tableFrame->RowIsSpannedInto(rowIndex + startRowIndex, numEffCols)) {
        hasRowSpanningCell = true;
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
          int32_t rowSpan = tableFrame->GetEffectiveRowSpan(rowIndex + startRowIndex, *cellFrame);
          if ((rowIndex + rowSpan) > numRows) {
            // there might be rows pushed already to the nextInFlow
            rowSpan = numRows - rowIndex;
          }
          if (rowSpan > 1) { // a cell with rowspan > 1, determine the height of the rows it spans
            nscoord heightOfRowsSpanned = 0;
            nscoord heightOfUnStyledRowsSpanned = 0;
            nscoord numSpecialRowsSpanned = 0; 
            nscoord cellSpacingTotal = 0;
            int32_t spanX;
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
            rowFrame->CalculateCellActualHeight(cellFrame, cellDesSize.height);
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
                bool haveUnStyledRowsSpanned = (heightOfUnStyledRowsSpanned > 0);
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
                      extraForRow = std::min(extraForRow, extra - extraUsed);
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
                    extraForRow = std::min(extraForRow, extra - extraUsed);
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
      rowExtra = std::min(rowExtra, extra);
      UpdateHeights(rInfo, rowExtra, heightOfRows, heightOfUnStyledRows);
      extra -= rowExtra;
    }
  }

  bool styleHeightAllocation = false;
  nscoord rowGroupHeight = startRowGroupHeight + heightOfRows + ((numRows - 1) * cellSpacingY);
  // if we have a style height, allocate the extra height to unconstrained rows
  if ((aReflowState.ComputedHeight() > rowGroupHeight) && 
      (NS_UNCONSTRAINEDSIZE != aReflowState.ComputedHeight())) {
    nscoord extraComputedHeight = aReflowState.ComputedHeight() - rowGroupHeight;
    nscoord extraUsed = 0;
    bool haveUnStyledRows = (heightOfUnStyledRows > 0);
    nscoord divisor = (haveUnStyledRows) 
                      ? heightOfUnStyledRows : heightOfRows;
    if (divisor > 0) {
      styleHeightAllocation = true;
      for (rowIndex = 0; rowIndex < numRows; rowIndex++) {
        if (!haveUnStyledRows || !rowInfo[rowIndex].hasStyleHeight) {
          // The amount of additional space each row gets is based on the
          // percentage of space it occupies
          float percent = ((float)rowInfo[rowIndex].height) / ((float)divisor);
          // give rows their percentage, except for the last row which gets the remainder
          nscoord extraForRow = (numRows - 1 == rowIndex) 
                                ? extraComputedHeight - extraUsed  
                                : NSToCoordRound(((float)extraComputedHeight) * percent);
          extraForRow = std::min(extraForRow, extraComputedHeight - extraUsed);
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
    rowGroupHeight = aReflowState.ComputedHeight();
  }

  nscoord yOrigin = startRowGroupHeight;
  // update the rows with their (potentially) new heights
  for (rowFrame = startRowFrame, rowIndex = 0; rowFrame; rowFrame = rowFrame->GetNextRow(), rowIndex++) {
    nsRect rowBounds = rowFrame->GetRect();
    nsRect rowVisualOverflow = rowFrame->GetVisualOverflowRect();

    bool movedFrame = (rowBounds.y != yOrigin);  
    nscoord rowHeight = (rowInfo[rowIndex].height > 0) ? rowInfo[rowIndex].height : 0;
    
    if (movedFrame || (rowHeight != rowBounds.height)) {
      // Resize/move the row to its final size and position
      if (movedFrame) {
        rowFrame->InvalidateFrameSubtree();
      }
      
      rowFrame->SetRect(nsRect(rowBounds.x, yOrigin, rowBounds.width,
                               rowHeight));

      nsTableFrame::InvalidateTableFrame(rowFrame, rowBounds, rowVisualOverflow,
                                         false);
    }
    if (movedFrame) {
      nsTableFrame::RePositionViews(rowFrame);
      // XXXbz we don't need to update our overflow area?
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
  const nsStyleVisibility* groupVis = StyleVisibility();
  bool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupVis->mVisible);
  if (collapseGroup) {
    tableFrame->SetNeedToCollapse(true);
  }

  nsOverflowAreas overflow;

  nsTableRowFrame* rowFrame= GetFirstRow();
  bool didCollapse = false;
  nscoord yGroupOffset = 0;
  while (rowFrame) {
    yGroupOffset += rowFrame->CollapseRowIfNecessary(yGroupOffset,
                                                     aWidth, collapseGroup,
                                                     didCollapse);
    ConsiderChildOverflow(overflow, rowFrame);
    rowFrame = rowFrame->GetNextRow();
  }

  nsRect groupRect = GetRect();
  nsRect oldGroupRect = groupRect;
  nsRect oldGroupVisualOverflow = GetVisualOverflowRect();
  
  groupRect.height -= yGroupOffset;
  if (didCollapse) {
    // add back the cellspacing between rowgroups
    groupRect.height += tableFrame->GetCellSpacingY();
  }

  groupRect.y -= aYTotalOffset;
  groupRect.width = aWidth;

  if (aYTotalOffset != 0) {
    InvalidateFrameSubtree();
  }
  
  SetRect(groupRect);
  overflow.UnionAllWith(nsRect(0, 0, groupRect.width, groupRect.height));
  FinishAndStoreOverflow(overflow, nsSize(groupRect.width, groupRect.height));
  nsTableFrame::RePositionViews(this);
  nsTableFrame::InvalidateTableFrame(this, oldGroupRect, oldGroupVisualOverflow,
                                     false);

  return yGroupOffset;
}

// Move a child that was skipped during a reflow.
void
nsTableRowGroupFrame::SlideChild(nsRowGroupReflowState& aReflowState,
                                 nsIFrame*              aKidFrame)
{
  // Move the frame if we need to
  nsPoint oldPosition = aKidFrame->GetPosition();
  nsPoint newPosition = oldPosition;
  newPosition.y = aReflowState.y;
  if (oldPosition.y != newPosition.y) {
    aKidFrame->InvalidateFrameSubtree();
    aKidFrame->SetPosition(newPosition);
    nsTableFrame::RePositionViews(aKidFrame);
    aKidFrame->InvalidateFrameSubtree();
  }
}

// Create a continuing frame, add it to the child list, and then push it
// and the frames that follow
void 
nsTableRowGroupFrame::CreateContinuingRowFrame(nsPresContext& aPresContext,
                                               nsIFrame&      aRowFrame,
                                               nsIFrame**     aContRowFrame)
{
  // XXX what is the row index?
  if (!aContRowFrame) {NS_ASSERTION(false, "bad call"); return;}
  // create the continuing frame which will create continuing cell frames
  *aContRowFrame = aPresContext.PresShell()->FrameConstructor()->
    CreateContinuingFrame(&aPresContext, &aRowFrame, this);

  // Add the continuing row frame to the child list
  mFrames.InsertFrame(nullptr, &aRowFrame, *aContRowFrame);

  // Push the continuing row frame and the frames that follow
  PushChildren(&aPresContext, *aContRowFrame, &aRowFrame);
}

// Reflow the cells with rowspan > 1 which originate between aFirstRow
// and end on or after aLastRow. aFirstTruncatedRow is the highest row on the
// page that contains a cell which cannot split on this page 
void
nsTableRowGroupFrame::SplitSpanningCells(nsPresContext&           aPresContext,
                                         const nsHTMLReflowState& aReflowState,
                                         nsTableFrame&            aTable,
                                         nsTableRowFrame&         aFirstRow, 
                                         nsTableRowFrame&         aLastRow,  
                                         bool                     aFirstRowIsTopOfPage,
                                         nscoord                  aSpanningRowBottom,
                                         nsTableRowFrame*&        aContRow,
                                         nsTableRowFrame*&        aFirstTruncatedRow,
                                         nscoord&                 aDesiredHeight)
{
  NS_ASSERTION(aSpanningRowBottom >= 0, "Can't split negative heights");
  aFirstTruncatedRow = nullptr;
  aDesiredHeight     = 0;

  const bool borderCollapse = aTable.IsBorderCollapse();
  int32_t lastRowIndex = aLastRow.GetRowIndex();
  bool wasLast = false;
  bool haveRowSpan = false;
  // Iterate the rows between aFirstRow and aLastRow
  for (nsTableRowFrame* row = &aFirstRow; !wasLast; row = row->GetNextRow()) {
    wasLast = (row == &aLastRow);
    int32_t rowIndex = row->GetRowIndex();
    nsPoint rowPos = row->GetPosition();
    // Iterate the cells looking for those that have rowspan > 1
    for (nsTableCellFrame* cell = row->GetFirstCell(); cell; cell = cell->GetNextCell()) {
      int32_t rowSpan = aTable.GetEffectiveRowSpan(rowIndex, *cell);
      // Only reflow rowspan > 1 cells which span aLastRow. Those which don't span aLastRow
      // were reflowed correctly during the unconstrained height reflow. 
      if ((rowSpan > 1) && (rowIndex + rowSpan > lastRowIndex)) {
        haveRowSpan = true;
        nsReflowStatus status;
        // Ask the row to reflow the cell to the height of all the rows it spans up through aLastRow
        // aAvailHeight is the space between the row group start and the end of the page
        nscoord cellAvailHeight = aSpanningRowBottom - rowPos.y;
        NS_ASSERTION(cellAvailHeight >= 0, "No space for cell?");
        bool isTopOfPage = (row == &aFirstRow) && aFirstRowIsTopOfPage;

        nsRect rowRect = row->GetRect();
        nsSize rowAvailSize(aReflowState.availableWidth,
                            std::max(aReflowState.availableHeight - rowRect.y,
                                   0));
        // don't let the available height exceed what
        // CalculateRowHeights set for it
        rowAvailSize.height = std::min(rowAvailSize.height, rowRect.height);
        nsHTMLReflowState rowReflowState(&aPresContext, aReflowState,
                                         row, rowAvailSize,
                                         -1, -1, false);
        InitChildReflowState(aPresContext, borderCollapse, rowReflowState);
        rowReflowState.mFlags.mIsTopOfPage = isTopOfPage; // set top of page

        nscoord cellHeight = row->ReflowCellFrame(&aPresContext, rowReflowState,
                                                  isTopOfPage, cell,
                                                  cellAvailHeight, status);
        aDesiredHeight = std::max(aDesiredHeight, rowPos.y + cellHeight);
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
              nsTableCellFrame* contCell = static_cast<nsTableCellFrame*>(
                aPresContext.PresShell()->FrameConstructor()->
                  CreateContinuingFrame(&aPresContext, cell, &aLastRow));
              int32_t colIndex;
              cell->GetColIndex(colIndex);
              aContRow->InsertCellFrame(contCell, colIndex);
            }
          }
        }
      }
    }
  }
  if (!haveRowSpan) {
    aDesiredHeight = aLastRow.GetRect().YMost();
  }
}

// Remove the next-in-flow of the row, its cells and their cell blocks. This 
// is necessary in case the row doesn't need a continuation later on or needs 
// a continuation which doesn't have the same number of cells that now exist. 
void
nsTableRowGroupFrame::UndoContinuedRow(nsPresContext*   aPresContext,
                                       nsTableRowFrame* aRow)
{
  if (!aRow) return; // allow null aRow to avoid callers doing null checks

  // rowBefore was the prev-sibling of aRow's next-sibling before aRow was created
  nsTableRowFrame* rowBefore = (nsTableRowFrame*)aRow->GetPrevInFlow();
  NS_PRECONDITION(mFrames.ContainsFrame(rowBefore),
                  "rowBefore not in our frame list?");

  AutoFrameListPtr overflows(aPresContext, StealOverflowFrames());
  if (!rowBefore || !overflows || overflows->IsEmpty() ||
      overflows->FirstChild() != aRow) {
    NS_ERROR("invalid continued row");
    return;
  }

  // Destroy aRow, its cells, and their cell blocks. Cell blocks that have split
  // will not have reflowed yet to pick up content from any overflow lines.
  overflows->DestroyFrame(aRow);

  // Put the overflow rows into our child list
  if (!overflows->IsEmpty()) {
    mFrames.InsertFrames(nullptr, rowBefore, *overflows);
  }
}

static nsTableRowFrame* 
GetRowBefore(nsTableRowFrame& aStartRow,
             nsTableRowFrame& aRow)
{
  nsTableRowFrame* rowBefore = nullptr;
  for (nsTableRowFrame* sib = &aStartRow; sib && (sib != &aRow); sib = sib->GetNextRow()) {
    rowBefore = sib;
  }
  return rowBefore;
}

nsresult
nsTableRowGroupFrame::SplitRowGroup(nsPresContext*           aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aReflowState,
                                    nsTableFrame*            aTableFrame,
                                    nsReflowStatus&          aStatus,
                                    bool                     aRowForcedPageBreak)
{
  NS_PRECONDITION(aPresContext->IsPaginated(), "SplitRowGroup currently supports only paged media"); 

  nsresult rv = NS_OK;
  nsTableRowFrame* prevRowFrame = nullptr;
  aDesiredSize.height = 0;

  nscoord availWidth  = aReflowState.availableWidth;
  nscoord availHeight = aReflowState.availableHeight;
  
  const bool borderCollapse = aTableFrame->IsBorderCollapse();
  nscoord cellSpacingY = aTableFrame->GetCellSpacingY();
  
  // get the page height
  nscoord pageHeight = aPresContext->GetPageSize().height;
  NS_ASSERTION(pageHeight != NS_UNCONSTRAINEDSIZE, 
               "The table shouldn't be split when there should be space");

  bool isTopOfPage = aReflowState.mFlags.mIsTopOfPage;
  nsTableRowFrame* firstRowThisPage = GetFirstRow();

  // Need to dirty the table's geometry, or else the row might skip
  // reflowing its cell as an optimization.
  aTableFrame->SetGeometryDirty();

  // Walk each of the row frames looking for the first row frame that doesn't fit 
  // in the available space
  for (nsTableRowFrame* rowFrame = firstRowThisPage; rowFrame; rowFrame = rowFrame->GetNextRow()) {
    bool rowIsOnPage = true;
    nsRect rowRect = rowFrame->GetRect();
    // See if the row fits on this page
    if (rowRect.YMost() > availHeight) {
      nsTableRowFrame* contRow = nullptr;
      // Reflow the row in the availabe space and have it split if it is the 1st
      // row (on the page) or there is at least 5% of the current page available 
      // XXX this 5% should be made a preference 
      if (!prevRowFrame || (availHeight - aDesiredSize.height > pageHeight / 20)) { 
        nsSize availSize(availWidth, std::max(availHeight - rowRect.y, 0));
        // don't let the available height exceed what CalculateRowHeights set for it
        availSize.height = std::min(availSize.height, rowRect.height);

        nsHTMLReflowState rowReflowState(aPresContext, aReflowState,
                                         rowFrame, availSize,
                                         -1, -1, false);
                                         
        InitChildReflowState(*aPresContext, borderCollapse, rowReflowState);
        rowReflowState.mFlags.mIsTopOfPage = isTopOfPage; // set top of page
        nsHTMLReflowMetrics rowMetrics;

        // Get the old size before we reflow.
        nsRect oldRowRect = rowFrame->GetRect();
        nsRect oldRowVisualOverflow = rowFrame->GetVisualOverflowRect();

        // Reflow the cell with the constrained height. A cell with rowspan >1 will get this
        // reflow later during SplitSpanningCells.
        rv = ReflowChild(rowFrame, aPresContext, rowMetrics, rowReflowState,
                         0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
        if (NS_FAILED(rv)) return rv;
        rowFrame->SetSize(nsSize(rowMetrics.width, rowMetrics.height));
        rowFrame->DidReflow(aPresContext, nullptr, nsDidReflowStatus::FINISHED);
        rowFrame->DidResize();

        if (!aRowForcedPageBreak && !NS_FRAME_IS_FULLY_COMPLETE(aStatus) &&
            ShouldAvoidBreakInside(aReflowState)) {
          aStatus = NS_INLINE_LINE_BREAK_BEFORE();
          break;
        }

        nsTableFrame::InvalidateTableFrame(rowFrame, oldRowRect,
                                           oldRowVisualOverflow,
                                           false);

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
            rowIsOnPage = false;
          }
        } 
        else {
          // The row frame is complete because either (1) its minimum height is greater than the 
          // available height we gave it, or (2) it may have been given a larger height through 
          // style than its content, or (3) it contains a rowspan >1 cell which hasn't been
          // reflowed with a constrained height yet (we will find out when SplitSpanningCells is
          // called below)
          if (rowMetrics.height > availSize.height ||
              (NS_INLINE_IS_BREAK_BEFORE(aStatus) && !aRowForcedPageBreak)) {
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
              rowIsOnPage = false;
            }
          }
        }
      } //if (!prevRowFrame || (availHeight - aDesiredSize.height > pageHeight / 20))
      else { 
        // put the row on the next page to give it more height
        rowIsOnPage = false;
      }

      nsTableRowFrame* lastRowThisPage = rowFrame;
      nscoord spanningRowBottom = availHeight;
      if (!rowIsOnPage) {
        NS_ASSERTION(!contRow, "We should not have created a continuation if none of this row fits");
        if (!aRowForcedPageBreak && ShouldAvoidBreakInside(aReflowState)) {
          aStatus = NS_INLINE_LINE_BREAK_BEFORE();
          break;
        }
        if (prevRowFrame) {
          spanningRowBottom = prevRowFrame->GetRect().YMost();
          lastRowThisPage = prevRowFrame;
          isTopOfPage = (lastRowThisPage == firstRowThisPage) && aReflowState.mFlags.mIsTopOfPage;
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
        else {
          // We can't push children, so let our parent reflow us again with more space
          aDesiredSize.height = rowRect.YMost();
          aStatus = NS_FRAME_COMPLETE;
          break;
        }
      }
      // reflow the cells with rowspan >1 that occur on the page

      nsTableRowFrame* firstTruncatedRow;
      nscoord yMost;
      SplitSpanningCells(*aPresContext, aReflowState, *aTableFrame, *firstRowThisPage,
                         *lastRowThisPage, aReflowState.mFlags.mIsTopOfPage, spanningRowBottom, contRow, 
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
            contRow = nullptr;
          }
        }
        else { // (firstTruncatedRow != firstRowThisPage)
          // Try to put firstTruncateRow on the next page 
          nsTableRowFrame* rowBefore = ::GetRowBefore(*firstRowThisPage, *firstTruncatedRow);
          nscoord oldSpanningRowBottom = spanningRowBottom;
          spanningRowBottom = rowBefore->GetRect().YMost();

          UndoContinuedRow(aPresContext, contRow);
          contRow = nullptr;
          nsTableRowFrame* oldLastRowThisPage = lastRowThisPage;
          lastRowThisPage = rowBefore;
          aStatus = NS_FRAME_NOT_COMPLETE;

          // Call SplitSpanningCells again with rowBefore as the last row on the page
          SplitSpanningCells(*aPresContext, aReflowState, *aTableFrame, 
                             *firstRowThisPage, *rowBefore, aReflowState.mFlags.mIsTopOfPage, 
                             spanningRowBottom, contRow, firstTruncatedRow, aDesiredSize.height);
          if (firstTruncatedRow) {
            if (aReflowState.mFlags.mIsTopOfPage) {
              // We were better off with the 1st call to SplitSpanningCells, do it again
              UndoContinuedRow(aPresContext, contRow);
              contRow = nullptr;
              lastRowThisPage = oldLastRowThisPage;
              spanningRowBottom = oldSpanningRowBottom;
              SplitSpanningCells(*aPresContext, aReflowState, *aTableFrame, *firstRowThisPage,
                                 *lastRowThisPage, aReflowState.mFlags.mIsTopOfPage, spanningRowBottom, contRow, 
                                 firstTruncatedRow, aDesiredSize.height);
              NS_WARNING("data loss in a row spanned cell");
            }
            else {
              // Let our parent reflow us again with more space
              aDesiredSize.height = rowRect.YMost();
              aStatus = NS_FRAME_COMPLETE;
              UndoContinuedRow(aPresContext, contRow);
              contRow = nullptr;
            }
          }
        } // if (firstTruncatedRow == firstRowThisPage)
      } // if (firstTruncatedRow)
      else {
        aDesiredSize.height = std::max(aDesiredSize.height, yMost);
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
      if (nextRow && nsTableFrame::PageBreakAfter(rowFrame, nextRow)) {
        PushChildren(aPresContext, nextRow, rowFrame);
        aStatus = NS_FRAME_NOT_COMPLETE;
        break;
      }
    }
    // after the 1st row that has a height, we can't be on top
    // of the page anymore.
    isTopOfPage = isTopOfPage && rowRect.YMost() == 0;
  }
  return NS_OK;
}

/** Layout the entire row group.
  * This method stacks rows vertically according to HTML 4.0 rules.
  * Rows are responsible for layout of their children.
  */
NS_METHOD
nsTableRowGroupFrame::Reflow(nsPresContext*           aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableRowGroupFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  nsresult rv = NS_OK;
  aStatus     = NS_FRAME_COMPLETE;

  // Row geometry may be going to change so we need to invalidate any row cursor.
  ClearRowCursor();

  // see if a special height reflow needs to occur due to having a pct height
  nsTableFrame::CheckRequestSpecialHeightReflow(aReflowState);

  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  nsRowGroupReflowState state(aReflowState, tableFrame);
  const nsStyleVisibility* groupVis = StyleVisibility();
  bool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupVis->mVisible);
  if (collapseGroup) {
    tableFrame->SetNeedToCollapse(true);
  }

  // Check for an overflow list
  MoveOverflowToChildList(aPresContext);

  // Reflow the existing frames. 
  bool splitDueToPageBreak = false;
  rv = ReflowChildren(aPresContext, aDesiredSize, state, aStatus,
                      &splitDueToPageBreak);

  // See if all the frames fit. Do not try to split anything if we're
  // not paginated ... we can't split across columns yet.
  if (aReflowState.mFlags.mTableIsSplittable &&
      NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight &&
      (NS_FRAME_NOT_COMPLETE == aStatus || splitDueToPageBreak || 
       aDesiredSize.height > aReflowState.availableHeight)) {
    // Nope, find a place to split the row group 
    bool specialReflow = (bool)aReflowState.mFlags.mSpecialHeightReflow;
    ((nsHTMLReflowState::ReflowStateFlags&)aReflowState.mFlags).mSpecialHeightReflow = false;

    SplitRowGroup(aPresContext, aDesiredSize, aReflowState, tableFrame, aStatus,
                  splitDueToPageBreak);

    ((nsHTMLReflowState::ReflowStateFlags&)aReflowState.mFlags).mSpecialHeightReflow = specialReflow;
  }

  // XXXmats The following is just bogus.  We leave it here for now because
  // ReflowChildren should pull up rows from our next-in-flow before returning
  // a Complete status, but doesn't (bug 804888).
  if (GetNextInFlow() && GetNextInFlow()->GetFirstPrincipalChild()) {
    NS_FRAME_SET_INCOMPLETE(aStatus);
  }

  SetHasStyleHeight((NS_UNCONSTRAINEDSIZE != aReflowState.ComputedHeight()) &&
                    (aReflowState.ComputedHeight() > 0)); 
  
  // just set our width to what was available. The table will calculate the width and not use our value.
  aDesiredSize.width = aReflowState.availableWidth;

  aDesiredSize.UnionOverflowAreasWithDesiredBounds();

  // If our parent is in initial reflow, it'll handle invalidating our
  // entire overflow rect.
  if (!(GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW) &&
      nsSize(aDesiredSize.width, aDesiredSize.height) != mRect.Size()) {
    InvalidateFrame();
  }
  
  FinishAndStoreOverflow(&aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

bool
nsTableRowGroupFrame::UpdateOverflow()
{
  // Row cursor invariants depend on the visual overflow area of the rows,
  // which may have changed, so we need to clear the cursor now.
  ClearRowCursor();
  return nsContainerFrame::UpdateOverflow();
}

/* virtual */ void
nsTableRowGroupFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsContainerFrame::DidSetStyleContext(aOldStyleContext);

  if (!aOldStyleContext) //avoid this on init
    return;
     
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (tableFrame->IsBorderCollapse() &&
      tableFrame->BCRecalcNeeded(aOldStyleContext, StyleContext())) {
    nsIntRect damageArea(0, GetStartRowIndex(), tableFrame->GetColCount(),
                         GetRowCount());
    tableFrame->AddBCDamageArea(damageArea);
  }
}

NS_IMETHODIMP
nsTableRowGroupFrame::AppendFrames(ChildListID     aListID,
                                   nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  ClearRowCursor();

  // collect the new row frames in an array
  // XXXbz why are we doing the QI stuff?  There shouldn't be any non-rows here.
  nsAutoTArray<nsTableRowFrame*, 8> rows;
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    nsTableRowFrame *rowFrame = do_QueryFrame(e.get());
    NS_ASSERTION(rowFrame, "Unexpected frame; frame constructor screwed up");
    if (rowFrame) {
      NS_ASSERTION(NS_STYLE_DISPLAY_TABLE_ROW ==
                     e.get()->StyleDisplay()->mDisplay,
                   "wrong display type on rowframe");      
      rows.AppendElement(rowFrame);
    }
  }

  int32_t rowIndex = GetRowCount();
  // Append the frames to the sibling chain
  mFrames.AppendFrames(nullptr, aFrameList);

  if (rows.Length() > 0) {
    nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
    tableFrame->AppendRows(this, rowIndex, rows);
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN);
    tableFrame->SetGeometryDirty();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::InsertFrames(ChildListID     aListID,
                                   nsIFrame*       aPrevFrame,
                                   nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  ClearRowCursor();

  // collect the new row frames in an array
  // XXXbz why are we doing the QI stuff?  There shouldn't be any non-rows here.
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  nsTArray<nsTableRowFrame*> rows;
  bool gotFirstRow = false;
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    nsTableRowFrame *rowFrame = do_QueryFrame(e.get());
    NS_ASSERTION(rowFrame, "Unexpected frame; frame constructor screwed up");
    if (rowFrame) {
      NS_ASSERTION(NS_STYLE_DISPLAY_TABLE_ROW ==
                     e.get()->StyleDisplay()->mDisplay,
                   "wrong display type on rowframe");      
      rows.AppendElement(rowFrame);
      if (!gotFirstRow) {
        rowFrame->SetFirstInserted(true);
        gotFirstRow = true;
        tableFrame->SetRowInserted(true);
      }
    }
  }

  int32_t startRowIndex = GetStartRowIndex();
  // Insert the frames in the sibling chain
  mFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);

  int32_t numRows = rows.Length();
  if (numRows > 0) {
    nsTableRowFrame* prevRow = (nsTableRowFrame *)nsTableFrame::GetFrameAtOrBefore(this, aPrevFrame, nsGkAtoms::tableRowFrame);
    int32_t rowIndex = (prevRow) ? prevRow->GetRowIndex() + 1 : startRowIndex;
    tableFrame->InsertRows(this, rows, rowIndex, true);

    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN);
    tableFrame->SetGeometryDirty();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::RemoveFrame(ChildListID     aListID,
                                  nsIFrame*       aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  ClearRowCursor();

  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  // XXX why are we doing the QI stuff?  There shouldn't be any non-rows here.
  nsTableRowFrame* rowFrame = do_QueryFrame(aOldFrame);
  if (rowFrame) {
    // remove the rows from the table (and flag a rebalance)
    tableFrame->RemoveRows(*rowFrame, 1, true);

    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN);
    tableFrame->SetGeometryDirty();
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
  if ((aReflowState.ComputedHeight() > 0) && (aReflowState.ComputedHeight() < NS_UNCONSTRAINEDSIZE)) {
    nscoord cellSpacing = std::max(0, GetRowCount() - 1) * tableFrame->GetCellSpacingY();
    result = aReflowState.ComputedHeight() - cellSpacing;
  }
  else {
    const nsHTMLReflowState* parentRS = aReflowState.parentReflowState;
    if (parentRS && (tableFrame != parentRS->frame)) {
      parentRS = parentRS->parentReflowState;
    }
    if (parentRS && (tableFrame == parentRS->frame) && 
        (parentRS->ComputedHeight() > 0) && (parentRS->ComputedHeight() < NS_UNCONSTRAINEDSIZE)) {
      nscoord cellSpacing = std::max(0, tableFrame->GetRowCount() + 1) * tableFrame->GetCellSpacingY();
      result = parentRS->ComputedHeight() - cellSpacing;
    }
  }

  return result;
}

bool
nsTableRowGroupFrame::IsSimpleRowFrame(nsTableFrame* aTableFrame,
                                       nsIFrame*     aFrame)
{
  // Make sure it's a row frame and not a row group frame
  nsTableRowFrame *rowFrame = do_QueryFrame(aFrame);
  if (rowFrame) {
    int32_t rowIndex = rowFrame->GetRowIndex();
    
    // It's a simple row frame if there are no cells that span into or
    // across the row
    int32_t numEffCols = aTableFrame->GetEffectiveColCount();
    if (!aTableFrame->RowIsSpannedInto(rowIndex, numEffCols) &&
        !aTableFrame->RowHasSpanningCells(rowIndex, numEffCols)) {
      return true;
    }
  }

  return false;
}

nsIAtom*
nsTableRowGroupFrame::GetType() const
{
  return nsGkAtoms::tableRowGroupFrame;
}

/** find page break before the first row **/
bool 
nsTableRowGroupFrame::HasInternalBreakBefore() const
{
 nsIFrame* firstChild = mFrames.FirstChild(); 
  if (!firstChild)
    return false;
  return firstChild->StyleDisplay()->mBreakBefore;
}

/** find page break after the last row **/
bool 
nsTableRowGroupFrame::HasInternalBreakAfter() const
{
  nsIFrame* lastChild = mFrames.LastChild(); 
  if (!lastChild)
    return false;
  return lastChild->StyleDisplay()->mBreakAfter;
}
/* ----- global methods ----- */

nsIFrame*
NS_NewTableRowGroupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableRowGroupFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableRowGroupFrame)

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
  aBorder.left = aBorder.right = aBorder.top = aBorder.bottom = 0;

  nsTableRowFrame* firstRowFrame = nullptr;
  nsTableRowFrame* lastRowFrame = nullptr;
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

void nsTableRowGroupFrame::SetContinuousBCBorderWidth(uint8_t     aForSide,
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

//nsILineIterator methods
int32_t
nsTableRowGroupFrame::GetNumLines()
{
  return GetRowCount();
}

bool
nsTableRowGroupFrame::GetDirection()
{
  nsTableFrame* table = nsTableFrame::GetTableFrame(this);
  return (NS_STYLE_DIRECTION_RTL ==
          table->StyleVisibility()->mDirection);
}
  
NS_IMETHODIMP
nsTableRowGroupFrame::GetLine(int32_t    aLineNumber, 
                              nsIFrame** aFirstFrameOnLine, 
                              int32_t*   aNumFramesOnLine,
                              nsRect&    aLineBounds, 
                              uint32_t*  aLineFlags)
{
  NS_ENSURE_ARG_POINTER(aFirstFrameOnLine);
  NS_ENSURE_ARG_POINTER(aNumFramesOnLine);
  NS_ENSURE_ARG_POINTER(aLineFlags);

  nsTableFrame* table = nsTableFrame::GetTableFrame(this);
  nsTableCellMap* cellMap = table->GetCellMap();

  *aLineFlags = 0;
  *aFirstFrameOnLine = nullptr;
  *aNumFramesOnLine = 0;
  aLineBounds.SetRect(0, 0, 0, 0);
  
  if ((aLineNumber < 0) || (aLineNumber >=  GetRowCount())) {
    return NS_OK;
  }
  aLineNumber += GetStartRowIndex(); 

  *aNumFramesOnLine = cellMap->GetNumCellsOriginatingInRow(aLineNumber);
  if (*aNumFramesOnLine == 0) {
    return NS_OK;
  }
  int32_t colCount = table->GetColCount();
  for (int32_t i = 0; i < colCount; i++) {
    CellData* data = cellMap->GetDataAt(aLineNumber, i);
    if (data && data->IsOrig()) {
      *aFirstFrameOnLine = (nsIFrame*)data->GetCellFrame();
      nsIFrame* parent = (*aFirstFrameOnLine)->GetParent();
      aLineBounds = parent->GetRect();
      return NS_OK;
    }
  }
  NS_ERROR("cellmap is lying");
  return NS_ERROR_FAILURE;
}
  
int32_t
nsTableRowGroupFrame::FindLineContaining(nsIFrame* aFrame, int32_t aStartLine)
{
  NS_ENSURE_TRUE(aFrame, -1);
  
  nsTableRowFrame *rowFrame = do_QueryFrame(aFrame);
  NS_ASSERTION(rowFrame, "RowGroup contains a frame that is not a row");

  int32_t rowIndexInGroup = rowFrame->GetRowIndex() - GetStartRowIndex();

  return rowIndexInGroup >= aStartLine ? rowIndexInGroup : -1;
}

#ifdef IBMBIDI
NS_IMETHODIMP
nsTableRowGroupFrame::CheckLineOrder(int32_t                  aLine,
                                     bool                     *aIsReordered,
                                     nsIFrame                 **aFirstVisual,
                                     nsIFrame                 **aLastVisual)
{
  *aIsReordered = false;
  *aFirstVisual = nullptr;
  *aLastVisual = nullptr;
  return NS_OK;
}
#endif // IBMBIDI
  
NS_IMETHODIMP
nsTableRowGroupFrame::FindFrameAt(int32_t    aLineNumber, 
                                  nscoord    aX, 
                                  nsIFrame** aFrameFound,
                                  bool*    aXIsBeforeFirstFrame, 
                                  bool*    aXIsAfterLastFrame)
{
   nsTableFrame* table = nsTableFrame::GetTableFrame(this);
   nsTableCellMap* cellMap = table->GetCellMap();
   
   *aFrameFound = nullptr;
   *aXIsBeforeFirstFrame = true;
   *aXIsAfterLastFrame = false;

   aLineNumber += GetStartRowIndex();
   int32_t numCells = cellMap->GetNumCellsOriginatingInRow(aLineNumber);
   if (numCells == 0) {
     return NS_OK;
   }
  
   nsIFrame* frame = nullptr;
   int32_t colCount = table->GetColCount();
   for (int32_t i = 0; i < colCount; i++) {
     CellData* data = cellMap->GetDataAt(aLineNumber, i);
     if (data && data->IsOrig()) {
       frame = (nsIFrame*)data->GetCellFrame();
       break;
     }
   }
   NS_ASSERTION(frame, "cellmap is lying");
   bool isRTL = (NS_STYLE_DIRECTION_RTL ==
                   table->StyleVisibility()->mDirection);
   
   nsIFrame* closestFromLeft = nullptr;
   nsIFrame* closestFromRight = nullptr;
   int32_t n = numCells;
   nsIFrame* firstFrame = frame;
   while (n--) {
     nsRect rect = frame->GetRect();
     if (rect.width > 0) {
       // If aX is inside this frame - this is it
       if (rect.x <= aX && rect.XMost() > aX) {
         closestFromLeft = closestFromRight = frame;
         break;
       }
       if (rect.x < aX) {
         if (!closestFromLeft ||
             rect.XMost() > closestFromLeft->GetRect().XMost())
           closestFromLeft = frame;
       }
       else {
         if (!closestFromRight ||
             rect.x < closestFromRight->GetRect().x)
           closestFromRight = frame;
       }
     }
     frame = frame->GetNextSibling();
   }
   if (!closestFromLeft && !closestFromRight) {
     // All frames were zero-width. Just take the first one.
     closestFromLeft = closestFromRight = firstFrame;
   }
   *aXIsBeforeFirstFrame = isRTL ? !closestFromRight : !closestFromLeft;
   *aXIsAfterLastFrame =   isRTL ? !closestFromLeft : !closestFromRight;
   if (closestFromLeft == closestFromRight) {
     *aFrameFound = closestFromLeft;
   }
   else if (!closestFromLeft) {
     *aFrameFound = closestFromRight;
   }
   else if (!closestFromRight) {
     *aFrameFound = closestFromLeft;
   }
   else { // we're between two frames
     nscoord delta = closestFromRight->GetRect().x -
                     closestFromLeft->GetRect().XMost();
     if (aX < closestFromLeft->GetRect().XMost() + delta/2)
       *aFrameFound = closestFromLeft;
     else
       *aFrameFound = closestFromRight;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::GetNextSiblingOnLine(nsIFrame*& aFrame, 
                                           int32_t    aLineNumber)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  aFrame = aFrame->GetNextSibling();
  return NS_OK;
}

//end nsLineIterator methods

static void
DestroyFrameCursorData(void* aPropertyValue)
{
  delete static_cast<nsTableRowGroupFrame::FrameCursorData*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(RowCursorProperty, DestroyFrameCursorData)

void
nsTableRowGroupFrame::ClearRowCursor()
{
  if (!(GetStateBits() & NS_ROWGROUP_HAS_ROW_CURSOR))
    return;

  RemoveStateBits(NS_ROWGROUP_HAS_ROW_CURSOR);
  Properties().Delete(RowCursorProperty());
}

nsTableRowGroupFrame::FrameCursorData*
nsTableRowGroupFrame::SetupRowCursor()
{
  if (GetStateBits() & NS_ROWGROUP_HAS_ROW_CURSOR) {
    // We already have a valid row cursor. Don't waste time rebuilding it.
    return nullptr;
  }

  nsIFrame* f = mFrames.FirstChild();
  int32_t count;
  for (count = 0; f && count < MIN_ROWS_NEEDING_CURSOR; ++count) {
    f = f->GetNextSibling();
  }
  if (!f) {
    // Less than MIN_ROWS_NEEDING_CURSOR rows, so just don't bother
    return nullptr;
  }

  FrameCursorData* data = new FrameCursorData();
  if (!data)
    return nullptr;
  Properties().Set(RowCursorProperty(), data);
  AddStateBits(NS_ROWGROUP_HAS_ROW_CURSOR);
  return data;
}

nsIFrame*
nsTableRowGroupFrame::GetFirstRowContaining(nscoord aY, nscoord* aOverflowAbove)
{
  if (!(GetStateBits() & NS_ROWGROUP_HAS_ROW_CURSOR))
    return nullptr;

  FrameCursorData* property = static_cast<FrameCursorData*>
    (Properties().Get(RowCursorProperty()));
  uint32_t cursorIndex = property->mCursorIndex;
  uint32_t frameCount = property->mFrames.Length();
  if (cursorIndex >= frameCount)
    return nullptr;
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

bool
nsTableRowGroupFrame::FrameCursorData::AppendFrame(nsIFrame* aFrame)
{
  nsRect overflowRect = aFrame->GetVisualOverflowRect();
  if (overflowRect.IsEmpty())
    return true;
  nscoord overflowAbove = -overflowRect.y;
  nscoord overflowBelow = overflowRect.YMost() - aFrame->GetSize().height;
  mOverflowAbove = std::max(mOverflowAbove, overflowAbove);
  mOverflowBelow = std::max(mOverflowBelow, overflowBelow);
  return mFrames.AppendElement(aFrame) != nullptr;
}
  
void 
nsTableRowGroupFrame::InvalidateFrame(uint32_t aDisplayItemKey)
{
  nsIFrame::InvalidateFrame(aDisplayItemKey);
  GetParent()->InvalidateFrameWithRect(GetVisualOverflowRect() + GetPosition(), aDisplayItemKey);
}

void 
nsTableRowGroupFrame::InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey)
{
  nsIFrame::InvalidateFrameWithRect(aRect, aDisplayItemKey);
  // If we have filters applied that would affects our bounds, then
  // we get an inactive layer created and this is computed
  // within FrameLayerBuilder
  GetParent()->InvalidateFrameWithRect(aRect + GetPosition(), aDisplayItemKey);
}
