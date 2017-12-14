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

#include "nsCellMap.h" //table cell navigation
#include <algorithm>

using namespace mozilla;
using namespace mozilla::layout;

namespace mozilla {

struct TableRowGroupReflowInput {
  const ReflowInput& reflowInput;  // Our reflow state

  nsTableFrame* tableFrame;

  // The available size (computed from the parent)
  mozilla::LogicalSize availSize;

  // Running block-offset
  nscoord bCoord;

  TableRowGroupReflowInput(const ReflowInput& aReflowInput,
                        nsTableFrame*            aTableFrame)
      : reflowInput(aReflowInput)
      , tableFrame(aTableFrame)
      , availSize(aReflowInput.GetWritingMode(),
                  aReflowInput.AvailableISize(),
                  aReflowInput.AvailableBSize())
      , bCoord(0)
  {
  }

  ~TableRowGroupReflowInput() {}
};

} // namespace mozilla

nsTableRowGroupFrame::nsTableRowGroupFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext, kClassID)
{
  SetRepeatable(false);
}

nsTableRowGroupFrame::~nsTableRowGroupFrame()
{
}

void
nsTableRowGroupFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  if (HasAnyStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN)) {
    nsTableFrame::UnregisterPositionedTablePart(this, aDestructRoot);
  }

  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
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
                 mozilla::StyleDisplay::TableRow,
                 "Unexpected display");
    NS_ASSERTION(e.get()->IsTableRowFrame(), "Unexpected frame type");
  }
#endif

  return mFrames.GetLength();
}

int32_t nsTableRowGroupFrame::GetStartRowIndex()
{
  int32_t result = -1;
  if (mFrames.NotEmpty()) {
    NS_ASSERTION(mFrames.FirstChild()->IsTableRowFrame(),
                 "Unexpected frame type");
    result = static_cast<nsTableRowFrame*>(mFrames.FirstChild())->GetRowIndex();
  }
  // if the row group doesn't have any children, get it the hard way
  if (-1 == result) {
    return GetTableFrame()->GetStartRowIndex(this);
  }

  return result;
}

void  nsTableRowGroupFrame::AdjustRowIndices(int32_t aRowIndex,
                                             int32_t anAdjustment)
{
  for (nsIFrame* rowFrame : mFrames) {
    if (mozilla::StyleDisplay::TableRow == rowFrame->StyleDisplay()->mDisplay) {
      int32_t index = ((nsTableRowFrame*)rowFrame)->GetRowIndex();
      if (index >= aRowIndex)
        ((nsTableRowFrame *)rowFrame)->SetRowIndex(index+anAdjustment);
    }
  }
}

int32_t
nsTableRowGroupFrame::GetAdjustmentForStoredIndex(int32_t aStoredIndex)
{
  nsTableFrame* tableFrame = GetTableFrame();
  return tableFrame->GetAdjustmentForStoredIndex(aStoredIndex);
}

void
nsTableRowGroupFrame::MarkRowsAsDeleted(nsTableRowFrame& aStartRowFrame,
                                        int32_t          aNumRowsToDelete)
{
  nsTableRowFrame* currentRowFrame = &aStartRowFrame;
  for (;;) {
    // XXXneerja - Instead of calling AddDeletedRowIndex() per row frame
    // it is possible to change AddDeleteRowIndex to instead take
    // <start row index> and <num of rows to mark for deletion> as arguments.
    // The problem that emerges here is mDeletedRowIndexRanges only stores
    // disjoint index ranges and since AddDeletedRowIndex() must operate on
    // the "stored" index, in some cases it is possible that the range
    // of indices to delete becomes overlapping EG: Deleting rows 9 - 11 and
    // then from the remaining rows deleting the *new* rows 7 to 20.
    // Handling these overlapping ranges is much more complicated to
    // implement and so I opted to add the deleted row index of one row at a
    // time and maintain the invariant that the range of deleted row indices
    // is always disjoint.
    currentRowFrame->AddDeletedRowIndex();
    if (--aNumRowsToDelete == 0) {
      break;
    }
    currentRowFrame = do_QueryFrame(currentRowFrame->GetNextSibling());
    if (!currentRowFrame) {
      MOZ_ASSERT_UNREACHABLE("expected another row frame");
      break;
    }
  }
}

void
nsTableRowGroupFrame::AddDeletedRowIndex(int32_t aDeletedRowStoredIndex)
{
  nsTableFrame* tableFrame = GetTableFrame();
  return tableFrame->AddDeletedRowIndex(aDeletedRowStoredIndex);
}

nsresult
nsTableRowGroupFrame::InitRepeatedFrame(nsTableRowGroupFrame* aHeaderFooterFrame)
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
      uint32_t colIndex = originalCellFrame->ColIndex();
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

// Handle the child-traversal part of DisplayGenericTablePart
static void
DisplayRows(nsDisplayListBuilder* aBuilder, nsFrame* aFrame,
            const nsDisplayListSet& aLists)
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
  nsIFrame* kid = aBuilder->ShouldDescendIntoFrame(f, true) ?
    nullptr : f->GetFirstRowContaining(aBuilder->GetVisibleRect().y, &overflowAbove);

  if (kid) {
    // have a cursor, use it
    while (kid) {
      if (kid->GetRect().y - overflowAbove >= aBuilder->GetVisibleRect().YMost() &&
          kid->GetNormalRect().y - overflowAbove >= aBuilder->GetVisibleRect().YMost())
        break;
      f->BuildDisplayListForChild(aBuilder, kid, aLists);
      kid = kid->GetNextSibling();
    }
    return;
  }

  // No cursor. Traverse children the hard way and build a cursor while we're at it
  nsTableRowGroupFrame::FrameCursorData* cursor = f->SetupRowCursor();
  kid = f->PrincipalChildList().FirstChild();
  while (kid) {
    f->BuildDisplayListForChild(aBuilder, kid, aLists);

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
                                       const nsDisplayListSet& aLists)
{
  nsTableFrame::DisplayGenericTablePart(aBuilder, this, aLists, DisplayRows);
}

nsIFrame::LogicalSides
nsTableRowGroupFrame::GetLogicalSkipSides(const ReflowInput* aReflowInput) const
{
  if (MOZ_UNLIKELY(StyleBorder()->mBoxDecorationBreak ==
                     StyleBoxDecorationBreak::Clone)) {
    return LogicalSides();
  }

  LogicalSides skip;
  if (nullptr != GetPrevInFlow()) {
    skip |= eLogicalSideBitsBStart;
  }
  if (nullptr != GetNextInFlow()) {
    skip |= eLogicalSideBitsBEnd;
  }
  return skip;
}

// Position and size aKidFrame and update our reflow state.
void
nsTableRowGroupFrame::PlaceChild(nsPresContext*         aPresContext,
                                 TableRowGroupReflowInput& aReflowInput,
                                 nsIFrame*              aKidFrame,
                                 WritingMode            aWM,
                                 const LogicalPoint&    aKidPosition,
                                 const nsSize&          aContainerSize,
                                 ReflowOutput&   aDesiredSize,
                                 const nsRect&          aOriginalKidRect,
                                 const nsRect&          aOriginalKidVisualOverflow)
{
  bool isFirstReflow = aKidFrame->HasAnyStateBits(NS_FRAME_FIRST_REFLOW);

  // Place and size the child
  FinishReflowChild(aKidFrame, aPresContext, aDesiredSize, nullptr,
                    aWM, aKidPosition, aContainerSize, 0);

  nsTableFrame* tableFrame = GetTableFrame();
  if (tableFrame->IsBorderCollapse()) {
    nsTableFrame::InvalidateTableFrame(aKidFrame, aOriginalKidRect,
                                       aOriginalKidVisualOverflow, isFirstReflow);
  }

  // Adjust the running block-offset
  aReflowInput.bCoord += aDesiredSize.BSize(aWM);

  // If our block-size is constrained then update the available bsize
  if (NS_UNCONSTRAINEDSIZE != aReflowInput.availSize.BSize(aWM)) {
    aReflowInput.availSize.BSize(aWM) -= aDesiredSize.BSize(aWM);
  }
}

void
nsTableRowGroupFrame::InitChildReflowInput(nsPresContext&     aPresContext,
                                           bool               aBorderCollapse,
                                           ReflowInput& aReflowInput)
{
  nsMargin collapseBorder;
  nsMargin padding(0,0,0,0);
  nsMargin* pCollapseBorder = nullptr;
  if (aBorderCollapse) {
    nsTableRowFrame *rowFrame = do_QueryFrame(aReflowInput.mFrame);
    if (rowFrame) {
      WritingMode wm = GetWritingMode();
      LogicalMargin border = rowFrame->GetBCBorderWidth(wm);
      collapseBorder = border.GetPhysicalMargin(wm);
      pCollapseBorder = &collapseBorder;
    }
  }
  aReflowInput.Init(&aPresContext, nullptr, pCollapseBorder, &padding);
}

static void
CacheRowBSizesForPrinting(nsPresContext*   aPresContext,
                          nsTableRowFrame* aFirstRow,
                          WritingMode      aWM)
{
  for (nsTableRowFrame* row = aFirstRow; row; row = row->GetNextRow()) {
    if (!row->GetPrevInFlow()) {
      row->SetHasUnpaginatedBSize(true);
      row->SetUnpaginatedBSize(aPresContext, row->BSize(aWM));
    }
  }
}

void
nsTableRowGroupFrame::ReflowChildren(nsPresContext*         aPresContext,
                                     ReflowOutput&   aDesiredSize,
                                     TableRowGroupReflowInput& aReflowInput,
                                     nsReflowStatus&        aStatus,
                                     bool*                aPageBreakBeforeEnd)
{
  if (aPageBreakBeforeEnd) {
    *aPageBreakBeforeEnd = false;
  }

  WritingMode wm = aReflowInput.reflowInput.GetWritingMode();
  nsTableFrame* tableFrame = GetTableFrame();
  const bool borderCollapse = tableFrame->IsBorderCollapse();

  // XXXldb Should we really be checking IsPaginated(),
  // or should we *only* check available block-size?
  // (Think about multi-column layout!)
  bool isPaginated = aPresContext->IsPaginated() &&
                     NS_UNCONSTRAINEDSIZE != aReflowInput.availSize.BSize(wm);

  bool haveRow = false;
  bool reflowAllKids = aReflowInput.reflowInput.ShouldReflowAllKids() ||
                         tableFrame->IsGeometryDirty();

  // in vertical-rl mode, we always need the row bsizes in order to
  // get the necessary containerSize for placing our kids
  bool needToCalcRowBSizes = reflowAllKids || wm.IsVerticalRL();

  nsSize containerSize =
    aReflowInput.reflowInput.ComputedSizeAsContainerIfConstrained();

  nsIFrame *prevKidFrame = nullptr;
  for (nsIFrame* kidFrame = mFrames.FirstChild(); kidFrame;
       prevKidFrame = kidFrame, kidFrame = kidFrame->GetNextSibling()) {
    nsTableRowFrame *rowFrame = do_QueryFrame(kidFrame);
    if (!rowFrame) {
      // XXXldb nsCSSFrameConstructor needs to enforce this!
      NS_NOTREACHED("yikes, a non-row child");
      continue;
    }
    nscoord cellSpacingB = tableFrame->GetRowSpacing(rowFrame->GetRowIndex());
    haveRow = true;

    // Reflow the row frame
    if (reflowAllKids ||
        NS_SUBTREE_DIRTY(kidFrame) ||
        (aReflowInput.reflowInput.mFlags.mSpecialBSizeReflow &&
         (isPaginated ||
          kidFrame->HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE)))) {
      LogicalRect oldKidRect = kidFrame->GetLogicalRect(wm, containerSize);
      nsRect oldKidVisualOverflow = kidFrame->GetVisualOverflowRect();

      // XXXldb We used to only pass aDesiredSize.mFlags through for the
      // incremental reflow codepath.
      ReflowOutput desiredSize(aReflowInput.reflowInput,
                                      aDesiredSize.mFlags);
      desiredSize.ClearSize();

      // Reflow the child into the available space, giving it as much bsize as
      // it wants. We'll deal with splitting later after we've computed the row
      // bsizes, taking into account cells with row spans...
      LogicalSize kidAvailSize = aReflowInput.availSize;
      kidAvailSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
      ReflowInput kidReflowInput(aPresContext, aReflowInput.reflowInput,
                                       kidFrame, kidAvailSize,
                                       nullptr,
                                       ReflowInput::CALLER_WILL_INIT);
      InitChildReflowInput(*aPresContext, borderCollapse, kidReflowInput);

      // This can indicate that columns were resized.
      if (aReflowInput.reflowInput.IsIResize()) {
        kidReflowInput.SetIResize(true);
      }

      NS_ASSERTION(kidFrame == mFrames.FirstChild() || prevKidFrame,
                   "If we're not on the first frame, we should have a "
                   "previous sibling...");
      // If prev row has nonzero YMost, then we can't be at the top of the page
      if (prevKidFrame && prevKidFrame->GetNormalRect().YMost() > 0) {
        kidReflowInput.mFlags.mIsTopOfPage = false;
      }

      LogicalPoint kidPosition(wm, 0, aReflowInput.bCoord);
      ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowInput,
                  wm, kidPosition, containerSize, 0, aStatus);
      kidReflowInput.ApplyRelativePositioning(&kidPosition, containerSize);

      // Place the child
      PlaceChild(aPresContext, aReflowInput, kidFrame,
                 wm, kidPosition, containerSize,
                 desiredSize, oldKidRect.GetPhysicalRect(wm, containerSize),
                 oldKidVisualOverflow);
      aReflowInput.bCoord += cellSpacingB;

      if (!reflowAllKids) {
        if (IsSimpleRowFrame(aReflowInput.tableFrame, rowFrame)) {
          // Inform the row of its new bsize.
          rowFrame->DidResize();
          // the overflow area may have changed inflate the overflow area
          const nsStylePosition *stylePos = StylePosition();
          nsStyleUnit unit = stylePos->BSize(wm).GetUnit();
          if (aReflowInput.tableFrame->IsAutoBSize(wm) &&
              unit != eStyleUnit_Coord) {
            // Because other cells in the row may need to be aligned
            // differently, repaint the entire row
            InvalidateFrame();
          } else if (oldKidRect.BSize(wm) != desiredSize.BSize(wm)) {
            needToCalcRowBSizes = true;
          }
        } else {
          needToCalcRowBSizes = true;
        }
      }

      if (isPaginated && aPageBreakBeforeEnd && !*aPageBreakBeforeEnd) {
        nsTableRowFrame* nextRow = rowFrame->GetNextRow();
        if (nextRow) {
          *aPageBreakBeforeEnd = nsTableFrame::PageBreakAfter(kidFrame, nextRow);
        }
      }
    } else {
      SlideChild(aReflowInput, kidFrame);

      // Adjust the running b-offset so we know where the next row should be placed
      nscoord bSize = kidFrame->BSize(wm) + cellSpacingB;
      aReflowInput.bCoord += bSize;

      if (NS_UNCONSTRAINEDSIZE != aReflowInput.availSize.BSize(wm)) {
        aReflowInput.availSize.BSize(wm) -= bSize;
      }
    }
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, kidFrame);
  }

  if (haveRow) {
    aReflowInput.bCoord -= tableFrame->GetRowSpacing(GetStartRowIndex() +
                                                     GetRowCount());
  }

  // Return our desired rect
  aDesiredSize.ISize(wm) = aReflowInput.reflowInput.AvailableISize();
  aDesiredSize.BSize(wm) = aReflowInput.bCoord;

  if (aReflowInput.reflowInput.mFlags.mSpecialBSizeReflow) {
    DidResizeRows(aDesiredSize);
    if (isPaginated) {
      CacheRowBSizesForPrinting(aPresContext, GetFirstRow(), wm);
    }
  }
  else if (needToCalcRowBSizes) {
    CalculateRowBSizes(aPresContext, aDesiredSize, aReflowInput.reflowInput);
    if (!reflowAllKids) {
      InvalidateFrame();
    }
  }
}

nsTableRowFrame*
nsTableRowGroupFrame::GetFirstRow()
{
  for (nsIFrame* childFrame : mFrames) {
    nsTableRowFrame* rowFrame = do_QueryFrame(childFrame);
    if (rowFrame) {
      return rowFrame;
    }
  }
  return nullptr;
}

nsTableRowFrame*
nsTableRowGroupFrame::GetLastRow()
{
  for (auto iter = mFrames.rbegin(), end = mFrames.rend(); iter != end; ++iter) {
    nsTableRowFrame* rowFrame = do_QueryFrame(*iter);
    if (rowFrame) {
      return rowFrame;
    }
  }
  return nullptr;
}


struct RowInfo {
  RowInfo() { bSize = pctBSize = hasStyleBSize = hasPctBSize = isSpecial = 0; }
  unsigned bSize;       // content bsize or fixed bsize, excluding pct bsize
  unsigned pctBSize:29; // pct bsize
  unsigned hasStyleBSize:1;
  unsigned hasPctBSize:1;
  unsigned isSpecial:1; // there is no cell originating in the row with rowspan=1 and there are at
                        // least 2 cells spanning the row and there is no style bsize on the row
};

static void
UpdateBSizes(RowInfo& aRowInfo,
             nscoord  aAdditionalBSize,
             nscoord& aTotal,
             nscoord& aUnconstrainedTotal)
{
  aRowInfo.bSize += aAdditionalBSize;
  aTotal         += aAdditionalBSize;
  if (!aRowInfo.hasStyleBSize) {
    aUnconstrainedTotal += aAdditionalBSize;
  }
}

void
nsTableRowGroupFrame::DidResizeRows(ReflowOutput& aDesiredSize)
{
  // Update the cells spanning rows with their new bsizes.
  // This is the place where all of the cells in the row get set to the bsize
  // of the row.
  // Reset the overflow area.
  aDesiredSize.mOverflowAreas.Clear();
  for (nsTableRowFrame* rowFrame = GetFirstRow();
       rowFrame; rowFrame = rowFrame->GetNextRow()) {
    rowFrame->DidResize();
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, rowFrame);
  }
}

// This calculates the bsize of all the rows and takes into account
// style bsize on the row group, style bsizes on rows and cells, style bsizes on rowspans.
// Actual row bsizes will be adjusted later if the table has a style bsize.
// Even if rows don't change bsize, this method must be called to set the bsizes of each
// cell in the row to the bsize of its row.
void
nsTableRowGroupFrame::CalculateRowBSizes(nsPresContext*           aPresContext,
                                          ReflowOutput&     aDesiredSize,
                                          const ReflowInput& aReflowInput)
{
  nsTableFrame* tableFrame = GetTableFrame();
  const bool isPaginated = aPresContext->IsPaginated();

  int32_t numEffCols = tableFrame->GetEffectiveColCount();

  int32_t startRowIndex = GetStartRowIndex();
  // find the row corresponding to the row index we just found
  nsTableRowFrame* startRowFrame = GetFirstRow();

  if (!startRowFrame) {
    return;
  }

  // The current row group block-size is the block-origin of the 1st row
  // we are about to calculate a block-size for.
  WritingMode wm = aReflowInput.GetWritingMode();
  nsSize containerSize; // actual value is unimportant as we're initially
                        // computing sizes, not physical positions
  nscoord startRowGroupBSize =
    startRowFrame->GetLogicalNormalPosition(wm, containerSize).B(wm);

  int32_t numRows = GetRowCount() - (startRowFrame->GetRowIndex() - GetStartRowIndex());
  // Collect the current bsize of each row.
  if (numRows <= 0)
    return;

  AutoTArray<RowInfo, 32> rowInfo;
  if (!rowInfo.AppendElements(numRows)) {
    return;
  }

  bool    hasRowSpanningCell = false;
  nscoord bSizeOfRows = 0;
  nscoord bSizeOfUnStyledRows = 0;
  // Get the bsize of each row without considering rowspans. This will be the max of
  // the largest desired bsize of each cell, the largest style bsize of each cell,
  // the style bsize of the row.
  nscoord pctBSizeBasis = GetBSizeBasis(aReflowInput);
  int32_t rowIndex; // the index in rowInfo, not among the rows in the row group
  nsTableRowFrame* rowFrame;
  for (rowFrame = startRowFrame, rowIndex = 0; rowFrame; rowFrame = rowFrame->GetNextRow(), rowIndex++) {
    nscoord nonPctBSize = rowFrame->GetContentBSize();
    if (isPaginated) {
      nonPctBSize = std::max(nonPctBSize, rowFrame->BSize(wm));
    }
    if (!rowFrame->GetPrevInFlow()) {
      if (rowFrame->HasPctBSize()) {
        rowInfo[rowIndex].hasPctBSize = true;
        rowInfo[rowIndex].pctBSize = rowFrame->GetInitialBSize(pctBSizeBasis);
      }
      rowInfo[rowIndex].hasStyleBSize = rowFrame->HasStyleBSize();
      nonPctBSize = std::max(nonPctBSize, rowFrame->GetFixedBSize());
    }
    UpdateBSizes(rowInfo[rowIndex], nonPctBSize, bSizeOfRows, bSizeOfUnStyledRows);

    if (!rowInfo[rowIndex].hasStyleBSize) {
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
    // Get the bsize of cells with rowspans and allocate any extra space to the rows they span
    // iteratate the child frames and process the row frames among them
    for (rowFrame = startRowFrame, rowIndex = 0; rowFrame; rowFrame = rowFrame->GetNextRow(), rowIndex++) {
      // See if the row has an originating cell with rowspan > 1. We cannot determine this for a row in a
      // continued row group by calling RowHasSpanningCells, because the row's fif may not have any originating
      // cells yet the row may have a continued cell which originates in it.
      if (GetPrevInFlow() || tableFrame->RowHasSpanningCells(startRowIndex + rowIndex, numEffCols)) {
        nsTableCellFrame* cellFrame = rowFrame->GetFirstCell();
        // iteratate the row's cell frames
        while (cellFrame) {
          nscoord cellSpacingB = tableFrame->GetRowSpacing(startRowIndex + rowIndex);
          int32_t rowSpan = tableFrame->GetEffectiveRowSpan(rowIndex + startRowIndex, *cellFrame);
          if ((rowIndex + rowSpan) > numRows) {
            // there might be rows pushed already to the nextInFlow
            rowSpan = numRows - rowIndex;
          }
          if (rowSpan > 1) { // a cell with rowspan > 1, determine the bsize of the rows it spans
            nscoord bsizeOfRowsSpanned = 0;
            nscoord bsizeOfUnStyledRowsSpanned = 0;
            nscoord numSpecialRowsSpanned = 0;
            nscoord cellSpacingTotal = 0;
            int32_t spanX;
            for (spanX = 0; spanX < rowSpan; spanX++) {
              bsizeOfRowsSpanned += rowInfo[rowIndex + spanX].bSize;
              if (!rowInfo[rowIndex + spanX].hasStyleBSize) {
                bsizeOfUnStyledRowsSpanned += rowInfo[rowIndex + spanX].bSize;
              }
              if (0 != spanX) {
                cellSpacingTotal += cellSpacingB;
              }
              if (rowInfo[rowIndex + spanX].isSpecial) {
                numSpecialRowsSpanned++;
              }
            }
            nscoord bsizeOfAreaSpanned = bsizeOfRowsSpanned + cellSpacingTotal;
            // get the bsize of the cell
            LogicalSize cellFrameSize = cellFrame->GetLogicalSize(wm);
            LogicalSize cellDesSize = cellFrame->GetDesiredSize();
            rowFrame->CalculateCellActualBSize(cellFrame, cellDesSize.BSize(wm), wm);
            cellFrameSize.BSize(wm) = cellDesSize.BSize(wm);
            if (cellFrame->HasVerticalAlignBaseline()) {
              // to ensure that a spanning cell with a long descender doesn't
              // collide with the next row, we need to take into account the shift
              // that will be done to align the cell on the baseline of the row.
              cellFrameSize.BSize(wm) += rowFrame->GetMaxCellAscent() -
                                         cellFrame->GetCellBaseline();
            }

            if (bsizeOfAreaSpanned < cellFrameSize.BSize(wm)) {
              // the cell's bsize is larger than the available space of the rows it
              // spans so distribute the excess bsize to the rows affected
              nscoord extra     = cellFrameSize.BSize(wm) - bsizeOfAreaSpanned;
              nscoord extraUsed = 0;
              if (0 == numSpecialRowsSpanned) {
                //NS_ASSERTION(bsizeOfRowsSpanned > 0, "invalid row span situation");
                bool haveUnStyledRowsSpanned = (bsizeOfUnStyledRowsSpanned > 0);
                nscoord divisor = (haveUnStyledRowsSpanned)
                                  ? bsizeOfUnStyledRowsSpanned : bsizeOfRowsSpanned;
                if (divisor > 0) {
                  for (spanX = rowSpan - 1; spanX >= 0; spanX--) {
                    if (!haveUnStyledRowsSpanned || !rowInfo[rowIndex + spanX].hasStyleBSize) {
                      // The amount of additional space each row gets is proportional to its bsize
                      float percent = ((float)rowInfo[rowIndex + spanX].bSize) / ((float)divisor);

                      // give rows their percentage, except for the first row which gets the remainder
                      nscoord extraForRow = (0 == spanX) ? extra - extraUsed
                                                         : NSToCoordRound(((float)(extra)) * percent);
                      extraForRow = std::min(extraForRow, extra - extraUsed);
                      // update the row bsize
                      UpdateBSizes(rowInfo[rowIndex + spanX], extraForRow, bSizeOfRows, bSizeOfUnStyledRows);
                      extraUsed += extraForRow;
                      if (extraUsed >= extra) {
                        NS_ASSERTION((extraUsed == extra), "invalid row bsize calculation");
                        break;
                      }
                    }
                  }
                }
                else {
                  // put everything in the last row
                  UpdateBSizes(rowInfo[rowIndex + rowSpan - 1], extra, bSizeOfRows, bSizeOfUnStyledRows);
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
                    // update the row bsize
                    UpdateBSizes(rowInfo[rowIndex + spanX], extraForRow, bSizeOfRows, bSizeOfUnStyledRows);
                    extraUsed += extraForRow;
                    if (extraUsed >= extra) {
                      NS_ASSERTION((extraUsed == extra), "invalid row bsize calculation");
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

  // pct bsize rows have already got their content bsizes.
  // Give them their pct bsizes up to pctBSizeBasis
  nscoord extra = pctBSizeBasis - bSizeOfRows;
  for (rowFrame = startRowFrame, rowIndex = 0; rowFrame && (extra > 0);
       rowFrame = rowFrame->GetNextRow(), rowIndex++) {
    RowInfo& rInfo = rowInfo[rowIndex];
    if (rInfo.hasPctBSize) {
      nscoord rowExtra = (rInfo.pctBSize > rInfo.bSize)
                         ? rInfo.pctBSize - rInfo.bSize: 0;
      rowExtra = std::min(rowExtra, extra);
      UpdateBSizes(rInfo, rowExtra, bSizeOfRows, bSizeOfUnStyledRows);
      extra -= rowExtra;
    }
  }

  bool styleBSizeAllocation = false;
  nscoord rowGroupBSize = startRowGroupBSize + bSizeOfRows +
                           tableFrame->GetRowSpacing(0, numRows-1);
  // if we have a style bsize, allocate the extra bsize to unconstrained rows
  if ((aReflowInput.ComputedBSize() > rowGroupBSize) &&
      (NS_UNCONSTRAINEDSIZE != aReflowInput.ComputedBSize())) {
    nscoord extraComputedBSize = aReflowInput.ComputedBSize() - rowGroupBSize;
    nscoord extraUsed = 0;
    bool haveUnStyledRows = (bSizeOfUnStyledRows > 0);
    nscoord divisor = (haveUnStyledRows)
                      ? bSizeOfUnStyledRows : bSizeOfRows;
    if (divisor > 0) {
      styleBSizeAllocation = true;
      for (rowIndex = 0; rowIndex < numRows; rowIndex++) {
        if (!haveUnStyledRows || !rowInfo[rowIndex].hasStyleBSize) {
          // The amount of additional space each row gets is based on the
          // percentage of space it occupies
          float percent = ((float)rowInfo[rowIndex].bSize) / ((float)divisor);
          // give rows their percentage, except for the last row which gets the remainder
          nscoord extraForRow = (numRows - 1 == rowIndex)
                                ? extraComputedBSize - extraUsed
                                : NSToCoordRound(((float)extraComputedBSize) * percent);
          extraForRow = std::min(extraForRow, extraComputedBSize - extraUsed);
          // update the row bsize
          UpdateBSizes(rowInfo[rowIndex], extraForRow, bSizeOfRows, bSizeOfUnStyledRows);
          extraUsed += extraForRow;
          if (extraUsed >= extraComputedBSize) {
            NS_ASSERTION((extraUsed == extraComputedBSize), "invalid row bsize calculation");
            break;
          }
        }
      }
    }
    rowGroupBSize = aReflowInput.ComputedBSize();
  }

  if (wm.IsVertical()) {
    // we need the correct containerSize below for block positioning in
    // vertical-rl writing mode
    containerSize.width = rowGroupBSize;
  }

  nscoord bOrigin = startRowGroupBSize;
  // update the rows with their (potentially) new bsizes
  for (rowFrame = startRowFrame, rowIndex = 0; rowFrame;
       rowFrame = rowFrame->GetNextRow(), rowIndex++) {
    nsRect rowBounds = rowFrame->GetRect();
    LogicalSize rowBoundsSize(wm, rowBounds.Size());
    nsRect rowVisualOverflow = rowFrame->GetVisualOverflowRect();
    nscoord deltaB =
      bOrigin - rowFrame->GetLogicalNormalPosition(wm, containerSize).B(wm);

    nscoord rowBSize = (rowInfo[rowIndex].bSize > 0) ? rowInfo[rowIndex].bSize : 0;

    if (deltaB != 0 || (rowBSize != rowBoundsSize.BSize(wm))) {
      // Resize/move the row to its final size and position
      if (deltaB != 0) {
        rowFrame->InvalidateFrameSubtree();
      }

      rowFrame->MovePositionBy(wm, LogicalPoint(wm, 0, deltaB));
      rowFrame->SetSize(LogicalSize(wm, rowBoundsSize.ISize(wm), rowBSize));

      nsTableFrame::InvalidateTableFrame(rowFrame, rowBounds, rowVisualOverflow,
                                         false);

      if (deltaB != 0) {
        nsTableFrame::RePositionViews(rowFrame);
        // XXXbz we don't need to update our overflow area?
      }
    }
    bOrigin += rowBSize + tableFrame->GetRowSpacing(startRowIndex + rowIndex);
  }

  if (isPaginated && styleBSizeAllocation) {
    // since the row group has a style bsize, cache the row bsizes,
    // so next in flows can honor them
    CacheRowBSizesForPrinting(aPresContext, GetFirstRow(), wm);
  }

  DidResizeRows(aDesiredSize);

  aDesiredSize.BSize(wm) = rowGroupBSize; // Adjust our desired size
}

nscoord
nsTableRowGroupFrame::CollapseRowGroupIfNecessary(nscoord aBTotalOffset,
                                                  nscoord aISize,
                                                  WritingMode aWM)
{
  nsTableFrame* tableFrame = GetTableFrame();
  nsSize containerSize = tableFrame->GetSize();
  const nsStyleVisibility* groupVis = StyleVisibility();
  bool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupVis->mVisible);
  if (collapseGroup) {
    tableFrame->SetNeedToCollapse(true);
  }

  nsOverflowAreas overflow;

  nsTableRowFrame* rowFrame = GetFirstRow();
  bool didCollapse = false;
  nscoord bGroupOffset = 0;
  while (rowFrame) {
    bGroupOffset += rowFrame->CollapseRowIfNecessary(bGroupOffset,
                                                     aISize, collapseGroup,
                                                     didCollapse);
    ConsiderChildOverflow(overflow, rowFrame);
    rowFrame = rowFrame->GetNextRow();
  }

  LogicalRect groupRect = GetLogicalRect(aWM, containerSize);
  nsRect oldGroupRect = GetRect();
  nsRect oldGroupVisualOverflow = GetVisualOverflowRect();

  groupRect.BSize(aWM) -= bGroupOffset;
  if (didCollapse) {
    // add back the cellspacing between rowgroups
    groupRect.BSize(aWM) += tableFrame->GetRowSpacing(GetStartRowIndex() +
                                                      GetRowCount());
  }

  groupRect.BStart(aWM) -= aBTotalOffset;
  groupRect.ISize(aWM) = aISize;

  if (aBTotalOffset != 0) {
    InvalidateFrameSubtree();
  }

  SetRect(aWM, groupRect, containerSize);
  overflow.UnionAllWith(nsRect(0, 0, groupRect.Width(aWM),
                               groupRect.Height(aWM)));
  FinishAndStoreOverflow(overflow, groupRect.Size(aWM).GetPhysicalSize(aWM));
  nsTableFrame::RePositionViews(this);
  nsTableFrame::InvalidateTableFrame(this, oldGroupRect, oldGroupVisualOverflow,
                                     false);

  return bGroupOffset;
}

// Move a child that was skipped during a reflow.
void
nsTableRowGroupFrame::SlideChild(TableRowGroupReflowInput& aReflowInput,
                                 nsIFrame*              aKidFrame)
{
  // Move the frame if we need to.
  WritingMode wm = aReflowInput.reflowInput.GetWritingMode();
  const nsSize containerSize =
    aReflowInput.reflowInput.ComputedSizeAsContainerIfConstrained();
  LogicalPoint oldPosition =
    aKidFrame->GetLogicalNormalPosition(wm, containerSize);
  LogicalPoint newPosition = oldPosition;
  newPosition.B(wm) = aReflowInput.bCoord;
  if (oldPosition.B(wm) != newPosition.B(wm)) {
    aKidFrame->InvalidateFrameSubtree();
    aReflowInput.reflowInput.ApplyRelativePositioning(&newPosition,
                                                      containerSize);
    aKidFrame->SetPosition(wm, newPosition, containerSize);
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
  PushChildren(*aContRowFrame, &aRowFrame);
}

// Reflow the cells with rowspan > 1 which originate between aFirstRow
// and end on or after aLastRow. aFirstTruncatedRow is the highest row on the
// page that contains a cell which cannot split on this page
void
nsTableRowGroupFrame::SplitSpanningCells(nsPresContext&           aPresContext,
                                         const ReflowInput& aReflowInput,
                                         nsTableFrame&            aTable,
                                         nsTableRowFrame&         aFirstRow,
                                         nsTableRowFrame&         aLastRow,
                                         bool                     aFirstRowIsTopOfPage,
                                         nscoord                  aSpanningRowBEnd,
                                         nsTableRowFrame*&        aContRow,
                                         nsTableRowFrame*&        aFirstTruncatedRow,
                                         nscoord&                 aDesiredBSize)
{
  NS_ASSERTION(aSpanningRowBEnd >= 0, "Can't split negative bsizes");
  aFirstTruncatedRow = nullptr;
  aDesiredBSize     = 0;

  const bool borderCollapse = aTable.IsBorderCollapse();
  int32_t lastRowIndex = aLastRow.GetRowIndex();
  bool wasLast = false;
  bool haveRowSpan = false;
  // Iterate the rows between aFirstRow and aLastRow
  for (nsTableRowFrame* row = &aFirstRow; !wasLast; row = row->GetNextRow()) {
    wasLast = (row == &aLastRow);
    int32_t rowIndex = row->GetRowIndex();
    nsPoint rowPos = row->GetNormalPosition();
    // Iterate the cells looking for those that have rowspan > 1
    for (nsTableCellFrame* cell = row->GetFirstCell(); cell; cell = cell->GetNextCell()) {
      int32_t rowSpan = aTable.GetEffectiveRowSpan(rowIndex, *cell);
      // Only reflow rowspan > 1 cells which span aLastRow. Those which don't span aLastRow
      // were reflowed correctly during the unconstrained bsize reflow.
      if ((rowSpan > 1) && (rowIndex + rowSpan > lastRowIndex)) {
        haveRowSpan = true;
        nsReflowStatus status;
        // Ask the row to reflow the cell to the bsize of all the rows it spans up through aLastRow
        // cellAvailBSize is the space between the row group start and the end of the page
        nscoord cellAvailBSize = aSpanningRowBEnd - rowPos.y;
        NS_ASSERTION(cellAvailBSize >= 0, "No space for cell?");
        bool isTopOfPage = (row == &aFirstRow) && aFirstRowIsTopOfPage;

        nsRect rowRect = row->GetNormalRect();
        nsSize rowAvailSize(aReflowInput.AvailableWidth(),
                            std::max(aReflowInput.AvailableHeight() - rowRect.y,
                                   0));
        // don't let the available height exceed what
        // CalculateRowBSizes set for it
        rowAvailSize.height = std::min(rowAvailSize.height, rowRect.height);
        ReflowInput rowReflowInput(&aPresContext, aReflowInput, row,
                                         LogicalSize(row->GetWritingMode(),
                                                     rowAvailSize),
                                         nullptr,
                                         ReflowInput::CALLER_WILL_INIT);
        InitChildReflowInput(aPresContext, borderCollapse, rowReflowInput);
        rowReflowInput.mFlags.mIsTopOfPage = isTopOfPage; // set top of page

        nscoord cellBSize = row->ReflowCellFrame(&aPresContext, rowReflowInput,
                                                  isTopOfPage, cell,
                                                  cellAvailBSize, status);
        aDesiredBSize = std::max(aDesiredBSize, rowPos.y + cellBSize);
        if (status.IsComplete()) {
          if (cellBSize > cellAvailBSize) {
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
              uint32_t colIndex = cell->ColIndex();
              aContRow->InsertCellFrame(contCell, colIndex);
            }
          }
        }
      }
    }
  }
  if (!haveRowSpan) {
    aDesiredBSize = aLastRow.GetNormalRect().YMost();
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
                                    ReflowOutput&     aDesiredSize,
                                    const ReflowInput& aReflowInput,
                                    nsTableFrame*            aTableFrame,
                                    nsReflowStatus&          aStatus,
                                    bool                     aRowForcedPageBreak)
{
  NS_PRECONDITION(aPresContext->IsPaginated(), "SplitRowGroup currently supports only paged media");

  nsTableRowFrame* prevRowFrame = nullptr;
  aDesiredSize.Height() = 0;

  nscoord availWidth  = aReflowInput.AvailableWidth();
  nscoord availHeight = aReflowInput.AvailableHeight();

  const bool borderCollapse = aTableFrame->IsBorderCollapse();

  // get the page height
  nscoord pageHeight = aPresContext->GetPageSize().height;
  NS_ASSERTION(pageHeight != NS_UNCONSTRAINEDSIZE,
               "The table shouldn't be split when there should be space");

  bool isTopOfPage = aReflowInput.mFlags.mIsTopOfPage;
  nsTableRowFrame* firstRowThisPage = GetFirstRow();

  // Need to dirty the table's geometry, or else the row might skip
  // reflowing its cell as an optimization.
  aTableFrame->SetGeometryDirty();

  // Walk each of the row frames looking for the first row frame that doesn't fit
  // in the available space
  for (nsTableRowFrame* rowFrame = firstRowThisPage; rowFrame; rowFrame = rowFrame->GetNextRow()) {
    bool rowIsOnPage = true;
    nscoord cellSpacingB = aTableFrame->GetRowSpacing(rowFrame->GetRowIndex());
    nsRect rowRect = rowFrame->GetNormalRect();
    // See if the row fits on this page
    if (rowRect.YMost() > availHeight) {
      nsTableRowFrame* contRow = nullptr;
      // Reflow the row in the availabe space and have it split if it is the 1st
      // row (on the page) or there is at least 5% of the current page available
      // XXX this 5% should be made a preference
      if (!prevRowFrame || (availHeight - aDesiredSize.Height() > pageHeight / 20)) {
        nsSize availSize(availWidth, std::max(availHeight - rowRect.y, 0));
        // don't let the available height exceed what CalculateRowHeights set for it
        availSize.height = std::min(availSize.height, rowRect.height);

        ReflowInput rowReflowInput(aPresContext, aReflowInput, rowFrame,
                                         LogicalSize(rowFrame->GetWritingMode(),
                                                     availSize),
                                         nullptr,
                                         ReflowInput::CALLER_WILL_INIT);

        InitChildReflowInput(*aPresContext, borderCollapse, rowReflowInput);
        rowReflowInput.mFlags.mIsTopOfPage = isTopOfPage; // set top of page
        ReflowOutput rowMetrics(aReflowInput);

        // Get the old size before we reflow.
        nsRect oldRowRect = rowFrame->GetRect();
        nsRect oldRowVisualOverflow = rowFrame->GetVisualOverflowRect();

        // Reflow the cell with the constrained height. A cell with rowspan >1 will get this
        // reflow later during SplitSpanningCells.
        ReflowChild(rowFrame, aPresContext, rowMetrics, rowReflowInput,
                    0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
        rowFrame->SetSize(nsSize(rowMetrics.Width(), rowMetrics.Height()));
        rowFrame->DidReflow(aPresContext, nullptr);
        rowFrame->DidResize();

        if (!aRowForcedPageBreak && !aStatus.IsFullyComplete() &&
            ShouldAvoidBreakInside(aReflowInput)) {
          aStatus.SetInlineLineBreakBeforeAndReset();
          break;
        }

        nsTableFrame::InvalidateTableFrame(rowFrame, oldRowRect,
                                           oldRowVisualOverflow,
                                           false);

        if (aStatus.IsIncomplete()) {
          // The row frame is incomplete and all of the rowspan 1 cells' block frames split
          if ((rowMetrics.Height() <= rowReflowInput.AvailableHeight()) || isTopOfPage) {
            // The row stays on this page because either it split ok or we're on the top of page.
            // If top of page and the height exceeded the avail height, then there will be data loss
            NS_ASSERTION(rowMetrics.Height() <= rowReflowInput.AvailableHeight(),
                         "data loss - incomplete row needed more height than available, on top of page");
            CreateContinuingRowFrame(*aPresContext, *rowFrame, (nsIFrame**)&contRow);
            if (contRow) {
              aDesiredSize.Height() += rowMetrics.Height();
              if (prevRowFrame)
                aDesiredSize.Height() += cellSpacingB;
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
          if (rowMetrics.Height() > availSize.height ||
              (aStatus.IsInlineBreakBefore() && !aRowForcedPageBreak)) {
            // cases (1) and (2)
            if (isTopOfPage) {
              // We're on top of the page, so keep the row on this page. There will be data loss.
              // Push the row frame that follows
              nsTableRowFrame* nextRowFrame = rowFrame->GetNextRow();
              if (nextRowFrame) {
                aStatus.Reset();
                aStatus.SetIncomplete();
              }
              aDesiredSize.Height() += rowMetrics.Height();
              if (prevRowFrame)
                aDesiredSize.Height() += cellSpacingB;
              NS_WARNING("data loss - complete row needed more height than available, on top of page");
            }
            else {
              // We're not on top of the page, so put the row on the next page to give it more height
              rowIsOnPage = false;
            }
          }
        }
      } //if (!prevRowFrame || (availHeight - aDesiredSize.Height() > pageHeight / 20))
      else {
        // put the row on the next page to give it more height
        rowIsOnPage = false;
      }

      nsTableRowFrame* lastRowThisPage = rowFrame;
      nscoord spanningRowBottom = availHeight;
      if (!rowIsOnPage) {
        NS_ASSERTION(!contRow, "We should not have created a continuation if none of this row fits");
        if (!aRowForcedPageBreak && ShouldAvoidBreakInside(aReflowInput)) {
          aStatus.SetInlineLineBreakBeforeAndReset();
          break;
        }
        if (prevRowFrame) {
          spanningRowBottom = prevRowFrame->GetNormalRect().YMost();
          lastRowThisPage = prevRowFrame;
          isTopOfPage = (lastRowThisPage == firstRowThisPage) && aReflowInput.mFlags.mIsTopOfPage;
          aStatus.Reset();
          aStatus.SetIncomplete();
        }
        else {
          // We can't push children, so let our parent reflow us again with more space
          aDesiredSize.Height() = rowRect.YMost();
          aStatus.Reset();
          break;
        }
      }
      // reflow the cells with rowspan >1 that occur on the page

      nsTableRowFrame* firstTruncatedRow;
      nscoord bMost;
      SplitSpanningCells(*aPresContext, aReflowInput, *aTableFrame, *firstRowThisPage,
                         *lastRowThisPage, aReflowInput.mFlags.mIsTopOfPage, spanningRowBottom, contRow,
                         firstTruncatedRow, bMost);
      if (firstTruncatedRow) {
        // A rowspan >1 cell did not fit (and could not split) in the space we gave it
        if (firstTruncatedRow == firstRowThisPage) {
          if (aReflowInput.mFlags.mIsTopOfPage) {
            NS_WARNING("data loss in a row spanned cell");
          }
          else {
            // We can't push children, so let our parent reflow us again with more space
            aDesiredSize.Height() = rowRect.YMost();
            aStatus.Reset();
            UndoContinuedRow(aPresContext, contRow);
            contRow = nullptr;
          }
        }
        else { // (firstTruncatedRow != firstRowThisPage)
          // Try to put firstTruncateRow on the next page
          nsTableRowFrame* rowBefore = ::GetRowBefore(*firstRowThisPage, *firstTruncatedRow);
          nscoord oldSpanningRowBottom = spanningRowBottom;
          spanningRowBottom = rowBefore->GetNormalRect().YMost();

          UndoContinuedRow(aPresContext, contRow);
          contRow = nullptr;
          nsTableRowFrame* oldLastRowThisPage = lastRowThisPage;
          lastRowThisPage = rowBefore;
          aStatus.Reset();
          aStatus.SetIncomplete();

          // Call SplitSpanningCells again with rowBefore as the last row on the page
          SplitSpanningCells(*aPresContext, aReflowInput, *aTableFrame,
                             *firstRowThisPage, *rowBefore, aReflowInput.mFlags.mIsTopOfPage,
                             spanningRowBottom, contRow, firstTruncatedRow, aDesiredSize.Height());
          if (firstTruncatedRow) {
            if (aReflowInput.mFlags.mIsTopOfPage) {
              // We were better off with the 1st call to SplitSpanningCells, do it again
              UndoContinuedRow(aPresContext, contRow);
              contRow = nullptr;
              lastRowThisPage = oldLastRowThisPage;
              spanningRowBottom = oldSpanningRowBottom;
              SplitSpanningCells(*aPresContext, aReflowInput, *aTableFrame, *firstRowThisPage,
                                 *lastRowThisPage, aReflowInput.mFlags.mIsTopOfPage, spanningRowBottom, contRow,
                                 firstTruncatedRow, aDesiredSize.Height());
              NS_WARNING("data loss in a row spanned cell");
            }
            else {
              // Let our parent reflow us again with more space
              aDesiredSize.Height() = rowRect.YMost();
              aStatus.Reset();
              UndoContinuedRow(aPresContext, contRow);
              contRow = nullptr;
            }
          }
        } // if (firstTruncatedRow == firstRowThisPage)
      } // if (firstTruncatedRow)
      else {
        aDesiredSize.Height() = std::max(aDesiredSize.Height(), bMost);
        if (contRow) {
          aStatus.Reset();
          aStatus.SetIncomplete();
        }
      }
      if (aStatus.IsIncomplete() && !contRow) {
        nsTableRowFrame* nextRow = lastRowThisPage->GetNextRow();
        if (nextRow) {
          PushChildren(nextRow, lastRowThisPage);
        }
      }
      break;
    } // if (rowRect.YMost() > availHeight)
    else {
      aDesiredSize.Height() = rowRect.YMost();
      prevRowFrame = rowFrame;
      // see if there is a page break after the row
      nsTableRowFrame* nextRow = rowFrame->GetNextRow();
      if (nextRow && nsTableFrame::PageBreakAfter(rowFrame, nextRow)) {
        PushChildren(nextRow, rowFrame);
        aStatus.Reset();
        aStatus.SetIncomplete();
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
void
nsTableRowGroupFrame::Reflow(nsPresContext*           aPresContext,
                             ReflowOutput&     aDesiredSize,
                             const ReflowInput& aReflowInput,
                             nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsTableRowGroupFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  // Row geometry may be going to change so we need to invalidate any row cursor.
  ClearRowCursor();

  // see if a special bsize reflow needs to occur due to having a pct bsize
  nsTableFrame::CheckRequestSpecialBSizeReflow(aReflowInput);

  nsTableFrame* tableFrame = GetTableFrame();
  TableRowGroupReflowInput state(aReflowInput, tableFrame);
  const nsStyleVisibility* groupVis = StyleVisibility();
  bool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE == groupVis->mVisible);
  if (collapseGroup) {
    tableFrame->SetNeedToCollapse(true);
  }

  // Check for an overflow list
  MoveOverflowToChildList();

  // Reflow the existing frames.
  bool splitDueToPageBreak = false;
  ReflowChildren(aPresContext, aDesiredSize, state, aStatus,
                 &splitDueToPageBreak);

  // See if all the frames fit. Do not try to split anything if we're
  // not paginated ... we can't split across columns yet.
  if (aReflowInput.mFlags.mTableIsSplittable &&
      NS_UNCONSTRAINEDSIZE != aReflowInput.AvailableHeight() &&
      (aStatus.IsIncomplete() || splitDueToPageBreak ||
       aDesiredSize.Height() > aReflowInput.AvailableHeight())) {
    // Nope, find a place to split the row group
    bool specialReflow = (bool)aReflowInput.mFlags.mSpecialBSizeReflow;
    ((ReflowInput::ReflowInputFlags&)aReflowInput.mFlags).mSpecialBSizeReflow = false;

    SplitRowGroup(aPresContext, aDesiredSize, aReflowInput, tableFrame, aStatus,
                  splitDueToPageBreak);

    ((ReflowInput::ReflowInputFlags&)aReflowInput.mFlags).mSpecialBSizeReflow = specialReflow;
  }

  // XXXmats The following is just bogus.  We leave it here for now because
  // ReflowChildren should pull up rows from our next-in-flow before returning
  // a Complete status, but doesn't (bug 804888).
  if (GetNextInFlow() && GetNextInFlow()->PrincipalChildList().FirstChild()) {
    aStatus.SetIncomplete();
  }

  SetHasStyleBSize((NS_UNCONSTRAINEDSIZE != aReflowInput.ComputedBSize()) &&
                    (aReflowInput.ComputedBSize() > 0));

  // Just set our isize to what was available.
  // The table will calculate the isize and not use our value.
  WritingMode wm = aReflowInput.GetWritingMode();
  aDesiredSize.ISize(wm) = aReflowInput.AvailableISize();

  aDesiredSize.UnionOverflowAreasWithDesiredBounds();

  // If our parent is in initial reflow, it'll handle invalidating our
  // entire overflow rect.
  if (!GetParent()->HasAnyStateBits(NS_FRAME_FIRST_REFLOW) &&
      nsSize(aDesiredSize.Width(), aDesiredSize.Height()) != mRect.Size()) {
    InvalidateFrame();
  }

  FinishAndStoreOverflow(&aDesiredSize);

  // Any absolutely-positioned children will get reflowed in
  // nsFrame::FixupPositionedTableParts in another pass, so propagate our
  // dirtiness to them before our parent clears our dirty bits.
  PushDirtyBitToAbsoluteFrames();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

bool
nsTableRowGroupFrame::ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas)
{
  // Row cursor invariants depend on the visual overflow area of the rows,
  // which may have changed, so we need to clear the cursor now.
  ClearRowCursor();
  return nsContainerFrame::ComputeCustomOverflow(aOverflowAreas);
}

/* virtual */ void
nsTableRowGroupFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsContainerFrame::DidSetStyleContext(aOldStyleContext);

  if (!aOldStyleContext) //avoid this on init
    return;

  nsTableFrame* tableFrame = GetTableFrame();
  if (tableFrame->IsBorderCollapse() &&
      tableFrame->BCRecalcNeeded(aOldStyleContext, StyleContext())) {
    TableArea damageArea(0, GetStartRowIndex(), tableFrame->GetColCount(),
                         GetRowCount());
    tableFrame->AddBCDamageArea(damageArea);
  }
}

void
nsTableRowGroupFrame::AppendFrames(ChildListID     aListID,
                                   nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  DrainSelfOverflowList(); // ensure the last frame is in mFrames
  ClearRowCursor();

  // collect the new row frames in an array
  // XXXbz why are we doing the QI stuff?  There shouldn't be any non-rows here.
  AutoTArray<nsTableRowFrame*, 8> rows;
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    nsTableRowFrame *rowFrame = do_QueryFrame(e.get());
    NS_ASSERTION(rowFrame, "Unexpected frame; frame constructor screwed up");
    if (rowFrame) {
      NS_ASSERTION(mozilla::StyleDisplay::TableRow ==
                     e.get()->StyleDisplay()->mDisplay,
                   "wrong display type on rowframe");
      rows.AppendElement(rowFrame);
    }
  }

  int32_t rowIndex = GetRowCount();
  // Append the frames to the sibling chain
  mFrames.AppendFrames(nullptr, aFrameList);

  if (rows.Length() > 0) {
    nsTableFrame* tableFrame = GetTableFrame();
    tableFrame->AppendRows(this, rowIndex, rows);
    PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                  NS_FRAME_HAS_DIRTY_CHILDREN);
    tableFrame->SetGeometryDirty();
  }
}

void
nsTableRowGroupFrame::InsertFrames(ChildListID     aListID,
                                   nsIFrame*       aPrevFrame,
                                   nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  DrainSelfOverflowList(); // ensure aPrevFrame is in mFrames
  ClearRowCursor();

  // collect the new row frames in an array
  // XXXbz why are we doing the QI stuff?  There shouldn't be any non-rows here.
  nsTableFrame* tableFrame = GetTableFrame();
  nsTArray<nsTableRowFrame*> rows;
  bool gotFirstRow = false;
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    nsTableRowFrame *rowFrame = do_QueryFrame(e.get());
    NS_ASSERTION(rowFrame, "Unexpected frame; frame constructor screwed up");
    if (rowFrame) {
      NS_ASSERTION(mozilla::StyleDisplay::TableRow ==
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
    nsTableRowFrame* prevRow = (nsTableRowFrame *)nsTableFrame::GetFrameAtOrBefore(this, aPrevFrame, LayoutFrameType::TableRow);
    int32_t rowIndex = (prevRow) ? prevRow->GetRowIndex() + 1 : startRowIndex;
    tableFrame->InsertRows(this, rows, rowIndex, true);

    PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                  NS_FRAME_HAS_DIRTY_CHILDREN);
    tableFrame->SetGeometryDirty();
  }
}

void
nsTableRowGroupFrame::RemoveFrame(ChildListID     aListID,
                                  nsIFrame*       aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  ClearRowCursor();

  // XXX why are we doing the QI stuff?  There shouldn't be any non-rows here.
  nsTableRowFrame* rowFrame = do_QueryFrame(aOldFrame);
  if (rowFrame) {
    nsTableFrame* tableFrame = GetTableFrame();
    // remove the rows from the table (and flag a rebalance)
    tableFrame->RemoveRows(*rowFrame, 1, true);

    PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                  NS_FRAME_HAS_DIRTY_CHILDREN);
    tableFrame->SetGeometryDirty();
  }
  mFrames.DestroyFrame(aOldFrame);
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
nsTableRowGroupFrame::GetBSizeBasis(const ReflowInput& aReflowInput)
{
  nscoord result = 0;
  nsTableFrame* tableFrame = GetTableFrame();
  int32_t startRowIndex = GetStartRowIndex();
  if ((aReflowInput.ComputedBSize() > 0) && (aReflowInput.ComputedBSize() < NS_UNCONSTRAINEDSIZE)) {
    nscoord cellSpacing = tableFrame->GetRowSpacing(startRowIndex,
                                                    std::max(startRowIndex,
                                                             startRowIndex + GetRowCount() - 1));
    result = aReflowInput.ComputedBSize() - cellSpacing;
  }
  else {
    const ReflowInput* parentRI = aReflowInput.mParentReflowInput;
    if (parentRI && (tableFrame != parentRI->mFrame)) {
      parentRI = parentRI->mParentReflowInput;
    }
    if (parentRI && (tableFrame == parentRI->mFrame) &&
        (parentRI->ComputedBSize() > 0) && (parentRI->ComputedBSize() < NS_UNCONSTRAINEDSIZE)) {
      nscoord cellSpacing = tableFrame->GetRowSpacing(-1, tableFrame->GetRowCount());
      result = parentRI->ComputedBSize() - cellSpacing;
    }
  }

  return result;
}

bool
nsTableRowGroupFrame::IsSimpleRowFrame(nsTableFrame* aTableFrame,
                                       nsTableRowFrame* aRowFrame)
{
  int32_t rowIndex = aRowFrame->GetRowIndex();

  // It's a simple row frame if there are no cells that span into or
  // across the row
  int32_t numEffCols = aTableFrame->GetEffectiveColCount();
  if (!aTableFrame->RowIsSpannedInto(rowIndex, numEffCols) &&
      !aTableFrame->RowHasSpanningCells(rowIndex, numEffCols)) {
    return true;
  }

  return false;
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

nsTableRowGroupFrame*
NS_NewTableRowGroupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableRowGroupFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableRowGroupFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult
nsTableRowGroupFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableRowGroup"), aResult);
}
#endif

LogicalMargin
nsTableRowGroupFrame::GetBCBorderWidth(WritingMode aWM)
{
  LogicalMargin border(aWM);
  nsTableRowFrame* firstRowFrame = nullptr;
  nsTableRowFrame* lastRowFrame = nullptr;
  for (nsTableRowFrame* rowFrame = GetFirstRow(); rowFrame; rowFrame = rowFrame->GetNextRow()) {
    if (!firstRowFrame) {
      firstRowFrame = rowFrame;
    }
    lastRowFrame = rowFrame;
  }
  if (firstRowFrame) {
    border.BStart(aWM) = PresContext()->DevPixelsToAppUnits(
      firstRowFrame->GetBStartBCBorderWidth());
    border.BEnd(aWM) = PresContext()->DevPixelsToAppUnits(
      lastRowFrame->GetBEndBCBorderWidth());
  }
  return border;
}

void nsTableRowGroupFrame::SetContinuousBCBorderWidth(LogicalSide aForSide,
                                                      BCPixelSize aPixelValue)
{
  switch (aForSide) {
    case eLogicalSideIEnd:
      mIEndContBorderWidth = aPixelValue;
      return;
    case eLogicalSideBEnd:
      mBEndContBorderWidth = aPixelValue;
      return;
    case eLogicalSideIStart:
      mIStartContBorderWidth = aPixelValue;
      return;
    default:
      NS_ERROR("invalid LogicalSide argument");
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
  return (NS_STYLE_DIRECTION_RTL ==
          GetTableFrame()->StyleVisibility()->mDirection);
}

NS_IMETHODIMP
nsTableRowGroupFrame::GetLine(int32_t    aLineNumber,
                              nsIFrame** aFirstFrameOnLine,
                              int32_t*   aNumFramesOnLine,
                              nsRect&    aLineBounds)
{
  NS_ENSURE_ARG_POINTER(aFirstFrameOnLine);
  NS_ENSURE_ARG_POINTER(aNumFramesOnLine);

  nsTableFrame* table = GetTableFrame();
  nsTableCellMap* cellMap = table->GetCellMap();

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

NS_IMETHODIMP
nsTableRowGroupFrame::FindFrameAt(int32_t    aLineNumber,
                                  nsPoint    aPos,
                                  nsIFrame** aFrameFound,
                                  bool*    aPosIsBeforeFirstFrame,
                                  bool*    aPosIsAfterLastFrame)
{
  nsTableFrame* table = GetTableFrame();
  nsTableCellMap* cellMap = table->GetCellMap();

  WritingMode wm = table->GetWritingMode();
  nsSize containerSize = table->GetSize();
  LogicalPoint pos(wm, aPos, containerSize);

  *aFrameFound = nullptr;
  *aPosIsBeforeFirstFrame = true;
  *aPosIsAfterLastFrame = false;

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

  nsIFrame* closestFromStart = nullptr;
  nsIFrame* closestFromEnd = nullptr;
  int32_t n = numCells;
  nsIFrame* firstFrame = frame;
  while (n--) {
    LogicalRect rect = frame->GetLogicalRect(wm, containerSize);
    if (rect.ISize(wm) > 0) {
      // If pos.I() is inside this frame - this is it
      if (rect.IStart(wm) <= pos.I(wm) && rect.IEnd(wm) > pos.I(wm)) {
        closestFromStart = closestFromEnd = frame;
        break;
      }
      if (rect.IStart(wm) < pos.I(wm)) {
        if (!closestFromStart ||
            rect.IEnd(wm) > closestFromStart->
                              GetLogicalRect(wm, containerSize).IEnd(wm))
          closestFromStart = frame;
      }
      else {
        if (!closestFromEnd ||
            rect.IStart(wm) < closestFromEnd->
                                GetLogicalRect(wm, containerSize).IStart(wm))
          closestFromEnd = frame;
      }
    }
    frame = frame->GetNextSibling();
  }
  if (!closestFromStart && !closestFromEnd) {
    // All frames were zero-width. Just take the first one.
    closestFromStart = closestFromEnd = firstFrame;
  }
  *aPosIsBeforeFirstFrame = isRTL ? !closestFromEnd : !closestFromStart;
  *aPosIsAfterLastFrame =   isRTL ? !closestFromStart : !closestFromEnd;
  if (closestFromStart == closestFromEnd) {
    *aFrameFound = closestFromStart;
  }
  else if (!closestFromStart) {
    *aFrameFound = closestFromEnd;
  }
  else if (!closestFromEnd) {
    *aFrameFound = closestFromStart;
  }
  else { // we're between two frames
    nscoord delta =
      closestFromEnd->GetLogicalRect(wm, containerSize).IStart(wm) -
      closestFromStart->GetLogicalRect(wm, containerSize).IEnd(wm);
    if (pos.I(wm) < closestFromStart->
                      GetLogicalRect(wm, containerSize).IEnd(wm) + delta/2) {
      *aFrameFound = closestFromStart;
    } else {
      *aFrameFound = closestFromEnd;
    }
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

NS_DECLARE_FRAME_PROPERTY_DELETABLE(RowCursorProperty,
                                    nsTableRowGroupFrame::FrameCursorData)

void
nsTableRowGroupFrame::ClearRowCursor()
{
  if (!HasAnyStateBits(NS_ROWGROUP_HAS_ROW_CURSOR)) {
    return;
  }

  RemoveStateBits(NS_ROWGROUP_HAS_ROW_CURSOR);
  DeleteProperty(RowCursorProperty());
}

nsTableRowGroupFrame::FrameCursorData*
nsTableRowGroupFrame::SetupRowCursor()
{
  if (HasAnyStateBits(NS_ROWGROUP_HAS_ROW_CURSOR)) {
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
  SetProperty(RowCursorProperty(), data);
  AddStateBits(NS_ROWGROUP_HAS_ROW_CURSOR);
  return data;
}

nsIFrame*
nsTableRowGroupFrame::GetFirstRowContaining(nscoord aY, nscoord* aOverflowAbove)
{
  if (!HasAnyStateBits(NS_ROWGROUP_HAS_ROW_CURSOR)) {
    return nullptr;
  }

  FrameCursorData* property = GetProperty(RowCursorProperty());
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
         cursorFrame->GetNormalRect().YMost() + property->mOverflowBelow > aY) {
    --cursorIndex;
    cursorFrame = property->mFrames[cursorIndex];
  }
  while (cursorIndex + 1 < frameCount &&
         cursorFrame->GetNormalRect().YMost() + property->mOverflowBelow <= aY) {
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
  // Relative positioning can cause table parts to move, but we will still paint
  // the backgrounds for the parts under them at their 'normal' position. That
  // means that we must consider the overflow rects at both positions. For
  // example, if we use relative positioning to move a row-spanning cell, we
  // will still paint the row background for that cell at its normal position,
  // which will overflow the row.
  // XXX(seth): This probably isn't correct in the presence of transforms.
  nsRect positionedOverflowRect = aFrame->GetVisualOverflowRect();
  nsPoint positionedToNormal = aFrame->GetNormalPosition() - aFrame->GetPosition();
  nsRect normalOverflowRect = positionedOverflowRect + positionedToNormal;

  nsRect overflowRect = positionedOverflowRect.Union(normalOverflowRect);
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
  if (GetTableFrame()->IsBorderCollapse() && StyleBorder()->HasBorder()) {
    GetParent()->InvalidateFrameWithRect(GetVisualOverflowRect() + GetPosition(), aDisplayItemKey);
  }
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
