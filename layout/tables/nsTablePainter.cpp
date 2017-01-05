/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTableFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsTablePainter.h"
#include "nsCSSRendering.h"
#include "nsDisplayList.h"
#include "mozilla/WritingModes.h"

/* ~*~ Table Background Painting ~*~

   Mozilla's Table Background painting follows CSS2.1:17.5.1
   That section does not, however, describe the effect of
   borders on background image positioning. What we do is:

     - in separate borders, the borders are passed in so that
       their width figures in image positioning, even for rows/cols, which
       don't have visible borders. This is done to allow authors
       to position row backgrounds by, for example, aligning the
       top left corner with the top left padding corner of the
       top left table cell in the row in cases where all cells
       have consistent border widths. If we didn't honor these
       invisible borders, there would be no way to align
       backgrounds with the padding edges, and designs would be
       lost underneath the border.

     - in collapsing borders, because the borders collapse, we
       use the -continuous border- width to synthesize a border
       style and pass that in instead of using the element's
       assigned style directly.

       The continuous border on a given edge of an element is
       the collapse of all borders guaranteed to be continuous
       along that edge. Cell borders are ignored (because, for
       example, setting a thick border on the leftmost cell
       should not shift the row background over; this way a
       striped background set on <tr> will line up across rows
       even if the cells are assigned arbitrary border widths.

       For example, the continuous border on the top edge of a
       row group is the collapse of any row group, row, and
       table borders involved. (The first row group's top would
       be [table-top + row group top + first row top]. It's bottom
       would be [row group bottom + last row bottom + next row
       top + next row group top].)
       The top edge of a column group likewise includes the
       table top, row group top, and first row top borders. However,
       it *also* includes its own top border, since that is guaranteed
       to be continuous. It does not include column borders because
       those are not guaranteed to be continuous: there may be two
       columns with different borders in a single column group.

       An alternative would be to define the continuous border as
         [table? + row group + row] for horizontal
         [table? + col group + col] for vertical
       This makes it easier to line up backgrounds across elements
       despite varying border widths, but it does not give much
       flexibility in aligning /to/ those border widths.
*/


/* ~*~ TableBackgroundPainter ~*~

   The TableBackgroundPainter is created and destroyed in one painting call.
   Its principal function is PaintTable, which paints all table element
   backgrounds. The initial code in that method sets up an array of column
   data that caches the background styles and the border sizes for the
   columns and colgroups in TableBackgroundData structs in mCols. Data for
   BC borders are calculated and stashed in a synthesized border style struct
   in the data struct since collapsed borders aren't the same width as style-
   assigned borders. The data struct optimizes by only doing this if there's
   an image background; otherwise we don't care. //XXX should also check background-origin
   The class then loops through the row groups, rows, and cells. At the cell
   level, it paints the backgrounds, one over the other, inside the cell rect.

   The exception to this pattern is when a table element creates a (pseudo)
   stacking context. Elements with stacking contexts (e.g., 'opacity' applied)
   are <dfn>passed through</dfn>, which means their data (and their
   descendants' data) are not cached. The full loop is still executed, however,
   so that underlying layers can get painted at the cell level.

   The TableBackgroundPainter is then destroyed.

   Elements with stacking contexts set up their own painter to finish the
   painting process, since they were skipped. They call the appropriate
   sub-part of the loop (e.g. PaintRow) which will paint the frame and
   descendants.

   XXX views are going
 */

using namespace mozilla;
using namespace mozilla::image;

TableBackgroundPainter::TableBackgroundData::TableBackgroundData()
  : mFrame(nullptr)
  , mVisible(false)
  , mUsesSynthBorder(false)
{
}

TableBackgroundPainter::TableBackgroundData::TableBackgroundData(nsIFrame* aFrame)
  : mFrame(aFrame)
  , mRect(aFrame->GetRect())
  , mVisible(mFrame->IsVisibleForPainting())
  , mUsesSynthBorder(false)
{
}

inline bool
TableBackgroundPainter::TableBackgroundData::ShouldSetBCBorder() const
{
  /* we only need accurate border data when positioning background images*/
  if (!mVisible) {
    return false;
  }

  const nsStyleImageLayers& layers = mFrame->StyleBackground()->mImage;
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, layers) {
    if (!layers.mLayers[i].mImage.IsEmpty())
      return true;
  }
  return false;
}

void
TableBackgroundPainter::TableBackgroundData::SetBCBorder(const nsMargin& aBorder)
{
  mUsesSynthBorder = true;
  mSynthBorderWidths = aBorder;
}

nsStyleBorder
TableBackgroundPainter::TableBackgroundData::StyleBorder(const nsStyleBorder& aZeroBorder) const
{
  MOZ_ASSERT(mVisible, "Don't call StyleBorder on an invisible TableBackgroundData");

  if (mUsesSynthBorder) {
    nsStyleBorder result = aZeroBorder;
    NS_FOR_CSS_SIDES(side) {
      result.SetBorderWidth(side, mSynthBorderWidths.Side(side));
    }
    return result;
  }

  MOZ_ASSERT(mFrame);

  return *mFrame->StyleBorder();
}

TableBackgroundPainter::TableBackgroundPainter(nsTableFrame*        aTableFrame,
                                               Origin               aOrigin,
                                               nsPresContext*       aPresContext,
                                               nsRenderingContext& aRenderingContext,
                                               const nsRect&        aDirtyRect,
                                               const nsPoint&       aRenderPt,
                                               uint32_t             aBGPaintFlags)
  : mPresContext(aPresContext),
    mRenderingContext(aRenderingContext),
    mRenderPt(aRenderPt),
    mDirtyRect(aDirtyRect),
    mOrigin(aOrigin),
    mZeroBorder(aPresContext),
    mBGPaintFlags(aBGPaintFlags)
{
  MOZ_COUNT_CTOR(TableBackgroundPainter);

  NS_FOR_CSS_SIDES(side) {
    mZeroBorder.SetBorderStyle(side, NS_STYLE_BORDER_STYLE_SOLID);
    mZeroBorder.SetBorderWidth(side, 0);
  }

  mIsBorderCollapse = aTableFrame->IsBorderCollapse();
#ifdef DEBUG
  mCompatMode = mPresContext->CompatibilityMode();
#endif
  mNumCols = aTableFrame->GetColCount();
}

TableBackgroundPainter::~TableBackgroundPainter()
{
  MOZ_COUNT_DTOR(TableBackgroundPainter);
}

DrawResult
TableBackgroundPainter::PaintTableFrame(nsTableFrame*         aTableFrame,
                                        nsTableRowGroupFrame* aFirstRowGroup,
                                        nsTableRowGroupFrame* aLastRowGroup,
                                        const nsMargin&       aDeflate)
{
  MOZ_ASSERT(aTableFrame, "null frame");
  TableBackgroundData tableData(aTableFrame);
  tableData.mRect.MoveTo(0,0); //using table's coords
  tableData.mRect.Deflate(aDeflate);
  WritingMode wm = aTableFrame->GetWritingMode();
  if (mIsBorderCollapse && tableData.ShouldSetBCBorder()) {
    if (aFirstRowGroup && aLastRowGroup && mNumCols > 0) {
      //only handle non-degenerate tables; we need a more robust BC model
      //to make degenerate tables' borders reasonable to deal with
      LogicalMargin border(wm);
      LogicalMargin tempBorder(wm);
      nsTableColFrame* colFrame = aTableFrame->GetColFrame(mNumCols - 1);
      if (colFrame) {
        colFrame->GetContinuousBCBorderWidth(wm, tempBorder);
      }
      border.IEnd(wm) = tempBorder.IEnd(wm);

      aLastRowGroup->GetContinuousBCBorderWidth(wm, tempBorder);
      border.BEnd(wm) = tempBorder.BEnd(wm);

      nsTableRowFrame* rowFrame = aFirstRowGroup->GetFirstRow();
      if (rowFrame) {
        rowFrame->GetContinuousBCBorderWidth(wm, tempBorder);
        border.BStart(wm) = tempBorder.BStart(wm);
      }

      border.IStart(wm) = aTableFrame->GetContinuousIStartBCBorderWidth();

      tableData.SetBCBorder(border.GetPhysicalMargin(wm));
    }
  }

  DrawResult result = DrawResult::SUCCESS;

  if (tableData.IsVisible()) {
    nsCSSRendering::PaintBGParams params =
      nsCSSRendering::PaintBGParams::ForAllLayers(*mPresContext,
                                                  mRenderingContext,
                                                  mDirtyRect,
                                                  tableData.mRect + mRenderPt,
                                                  tableData.mFrame,
                                                  mBGPaintFlags);

    result &=
      nsCSSRendering::PaintBackgroundWithSC(params,
                                            tableData.mFrame->StyleContext(),
                                            tableData.StyleBorder(mZeroBorder));
  }

  return result;
}

void
TableBackgroundPainter::TranslateContext(nscoord aDX,
                                         nscoord aDY)
{
  mRenderPt += nsPoint(aDX, aDY);
  for (auto& col : mCols) {
    col.mCol.mRect.MoveBy(-aDX, -aDY);
  }
  for (auto& colGroup : mColGroups) {
    colGroup.mRect.MoveBy(-aDX, -aDY);
  }
}

TableBackgroundPainter::ColData::ColData(nsIFrame* aFrame, TableBackgroundData& aColGroupBGData)
  : mCol(aFrame)
  , mColGroup(aColGroupBGData)
{
}

DrawResult
TableBackgroundPainter::PaintTable(nsTableFrame*   aTableFrame,
                                   const nsMargin& aDeflate,
                                   bool            aPaintTableBackground)
{
  NS_PRECONDITION(aTableFrame, "null table frame");

  nsTableFrame::RowGroupArray rowGroups;
  aTableFrame->OrderRowGroups(rowGroups);
  WritingMode wm = aTableFrame->GetWritingMode();

  DrawResult result = DrawResult::SUCCESS;

  if (rowGroups.Length() < 1) { //degenerate case
    if (aPaintTableBackground) {
      result &= PaintTableFrame(aTableFrame, nullptr, nullptr, nsMargin(0,0,0,0));
    }
    /* No cells; nothing else to paint */
    return result;
  }

  if (aPaintTableBackground) {
    result &=
      PaintTableFrame(aTableFrame, rowGroups[0], rowGroups[rowGroups.Length() - 1],
                      aDeflate);
  }

  /*Set up column background/border data*/
  if (mNumCols > 0) {
    nsFrameList& colGroupList = aTableFrame->GetColGroups();
    NS_ASSERTION(colGroupList.FirstChild(), "table should have at least one colgroup");

    // Collect all col group frames first so that we know how many there are.
    nsTArray<nsTableColGroupFrame*> colGroupFrames;
    for (nsTableColGroupFrame* cgFrame = static_cast<nsTableColGroupFrame*>(colGroupList.FirstChild());
         cgFrame; cgFrame = static_cast<nsTableColGroupFrame*>(cgFrame->GetNextSibling())) {

      if (cgFrame->GetColCount() < 1) {
        //No columns, no cells, so no need for data
        continue;
      }
      colGroupFrames.AppendElement(cgFrame);
    }

    // Ensure that mColGroups won't reallocate during the loop below, because
    // we grab references to its contents and need those to stay valid until
    // mColGroups is destroyed as part of TablePainter destruction.
    mColGroups.SetCapacity(colGroupFrames.Length());

    LogicalMargin border(wm);
    /* BC iStart borders aren't stored on cols, but the previous column's
       iEnd border is the next one's iStart border.*/
    //Start with table's iStart border.
    nscoord lastIStartBorder = aTableFrame->GetContinuousIStartBCBorderWidth();

    for (nsTableColGroupFrame* cgFrame : colGroupFrames) {
      /*Create data struct for column group*/
      TableBackgroundData& cgData = *mColGroups.AppendElement(TableBackgroundData(cgFrame));
      if (mIsBorderCollapse && cgData.ShouldSetBCBorder()) {
        border.IStart(wm) = lastIStartBorder;
        cgFrame->GetContinuousBCBorderWidth(wm, border);
        cgData.SetBCBorder(border.GetPhysicalMargin(wm));
      }

      /*Loop over columns in this colgroup*/
      for (nsTableColFrame* col = cgFrame->GetFirstColumn(); col;
           col = static_cast<nsTableColFrame*>(col->GetNextSibling())) {
        MOZ_ASSERT(size_t(col->GetColIndex()) == mCols.Length());
        // Store a reference to the colGroup in the ColData element.
        ColData& colData = *mCols.AppendElement(ColData(col, cgData));
        //Bring column mRect into table's coord system
        colData.mCol.mRect.MoveBy(cgData.mRect.x, cgData.mRect.y);
        if (mIsBorderCollapse) {
          border.IStart(wm) = lastIStartBorder;
          lastIStartBorder = col->GetContinuousBCBorderWidth(wm, border);
          if (colData.mCol.ShouldSetBCBorder()) {
            colData.mCol.SetBCBorder(border.GetPhysicalMargin(wm));
          }
        }
      }
    }
  }

  for (uint32_t i = 0; i < rowGroups.Length(); i++) {
    nsTableRowGroupFrame* rg = rowGroups[i];
    TableBackgroundData rowGroupBGData(rg);
    // Need to compute the right rect via GetOffsetTo, since the row
    // group may not be a child of the table.
    rowGroupBGData.mRect.MoveTo(rg->GetOffsetTo(aTableFrame));

    // We have to draw backgrounds not only within the overflow region of this
    // row group, but also possibly (in the case of column / column group
    // backgrounds) at its pre-relative-positioning location.
    nsRect rgVisualOverflow = rg->GetVisualOverflowRectRelativeToSelf();
    nsRect rgOverflowRect = rgVisualOverflow + rg->GetPosition();
    nsRect rgNormalRect = rgVisualOverflow + rg->GetNormalPosition();

    if (rgOverflowRect.Union(rgNormalRect).Intersects(mDirtyRect - mRenderPt)) {
      result &=
        PaintRowGroup(rg, rowGroupBGData, rg->IsPseudoStackingContextFromStyle());
    }
  }

  return result;
}

DrawResult
TableBackgroundPainter::PaintRowGroup(nsTableRowGroupFrame* aFrame)
{
  return PaintRowGroup(aFrame, TableBackgroundData(aFrame), false);
}

DrawResult
TableBackgroundPainter::PaintRowGroup(nsTableRowGroupFrame* aFrame,
                                      TableBackgroundData   aRowGroupBGData,
                                      bool                  aPassThrough)
{
  MOZ_ASSERT(aFrame, "null frame");

  nsTableRowFrame* firstRow = aFrame->GetFirstRow();
  WritingMode wm = aFrame->GetWritingMode();

  /* Load row group data */
  if (aPassThrough) {
    aRowGroupBGData.MakeInvisible();
  } else {
    if (mIsBorderCollapse && aRowGroupBGData.ShouldSetBCBorder()) {
      LogicalMargin border(wm);
      if (firstRow) {
        //pick up first row's bstart border (= rg bstart border)
        firstRow->GetContinuousBCBorderWidth(wm, border);
        /* (row group doesn't store its bstart border) */
      }
      //overwrite sides+bottom borders with rg's own
      aFrame->GetContinuousBCBorderWidth(wm, border);
      aRowGroupBGData.SetBCBorder(border.GetPhysicalMargin(wm));
    }
    aPassThrough = !aRowGroupBGData.IsVisible();
  }

  /* translate everything into row group coord system*/
  if (eOrigin_TableRowGroup != mOrigin) {
    TranslateContext(aRowGroupBGData.mRect.x, aRowGroupBGData.mRect.y);
  }
  nsRect rgRect = aRowGroupBGData.mRect;
  aRowGroupBGData.mRect.MoveTo(0, 0);

  /* Find the right row to start with */

  // Note that mDirtyRect  - mRenderPt is guaranteed to be in the row
  // group's coordinate system here, so passing its .y to
  // GetFirstRowContaining is ok.
  nscoord overflowAbove;
  nsIFrame* cursor = aFrame->GetFirstRowContaining(mDirtyRect.y - mRenderPt.y, &overflowAbove);

  // Sadly, it seems like there may be non-row frames in there... or something?
  // There are certainly null-checks in GetFirstRow() and GetNextRow().  :(
  while (cursor && cursor->GetType() != nsGkAtoms::tableRowFrame) {
    cursor = cursor->GetNextSibling();
  }

  // It's OK if cursor is null here.
  nsTableRowFrame* row = static_cast<nsTableRowFrame*>(cursor);
  if (!row) {
    // No useful cursor; just start at the top.  Don't bother to set up a
    // cursor; if we've gotten this far then we've already built the display
    // list for the rowgroup, so not having a cursor means that there's some
    // good reason we don't have a cursor and we shouldn't create one here.
    row = firstRow;
  }

  DrawResult result = DrawResult::SUCCESS;

  /* Finally paint */
  for (; row; row = row->GetNextRow()) {
    TableBackgroundData rowBackgroundData(row);

    // Be sure to consider our positions both pre- and post-relative
    // positioning, since we potentially need to paint at both places.
    nscoord rowY = std::min(rowBackgroundData.mRect.y, row->GetNormalPosition().y);

    // Intersect wouldn't handle rowspans.
    if (cursor &&
        (mDirtyRect.YMost() - mRenderPt.y) <= (rowY - overflowAbove)) {
      // All done; cells originating in later rows can't intersect mDirtyRect.
      break;
    }

    result &=
      PaintRow(row, aRowGroupBGData, rowBackgroundData,
               aPassThrough || row->IsPseudoStackingContextFromStyle());
  }

  /* translate back into table coord system */
  if (eOrigin_TableRowGroup != mOrigin) {
    TranslateContext(-rgRect.x, -rgRect.y);
  }

  return result;
}

DrawResult
TableBackgroundPainter::PaintRow(nsTableRowFrame* aFrame)
{
  return PaintRow(aFrame, TableBackgroundData(), TableBackgroundData(aFrame), false);
}

DrawResult
TableBackgroundPainter::PaintRow(nsTableRowFrame* aFrame,
                                 const TableBackgroundData& aRowGroupBGData,
                                 TableBackgroundData aRowBGData,
                                 bool             aPassThrough)
{
  MOZ_ASSERT(aFrame, "null frame");

  /* Load row data */
  WritingMode wm = aFrame->GetWritingMode();
  if (aPassThrough) {
    aRowBGData.MakeInvisible();
  } else {
    if (mIsBorderCollapse && aRowBGData.ShouldSetBCBorder()) {
      LogicalMargin border(wm);
      nsTableRowFrame* nextRow = aFrame->GetNextRow();
      if (nextRow) { //outer bStart after us is inner bEnd for us
        border.BEnd(wm) = nextRow->GetOuterBStartContBCBorderWidth();
      }
      else { //acquire rg's bEnd border
        nsTableRowGroupFrame* rowGroup = static_cast<nsTableRowGroupFrame*>(aFrame->GetParent());
        rowGroup->GetContinuousBCBorderWidth(wm, border);
      }
      //get the rest of the borders; will overwrite all but bEnd
      aFrame->GetContinuousBCBorderWidth(wm, border);

      aRowBGData.SetBCBorder(border.GetPhysicalMargin(wm));
    }
    aPassThrough = !aRowBGData.IsVisible();
  }

  /* Translate */
  if (eOrigin_TableRow == mOrigin) {
    /* If we originate from the row, then make the row the origin. */
    aRowBGData.mRect.MoveTo(0, 0);
  }
  //else: Use row group's coord system -> no translation necessary

  DrawResult result = DrawResult::SUCCESS;

  for (nsTableCellFrame* cell = aFrame->GetFirstCell(); cell; cell = cell->GetNextCell()) {
    nsRect cellBGRect, rowBGRect, rowGroupBGRect, colBGRect;
    ComputeCellBackgrounds(cell, aRowGroupBGData, aRowBGData,
                           cellBGRect, rowBGRect,
                           rowGroupBGRect, colBGRect);

    // Find the union of all the cell background layers.
    nsRect combinedRect(cellBGRect);
    combinedRect.UnionRect(combinedRect, rowBGRect);
    combinedRect.UnionRect(combinedRect, rowGroupBGRect);
    combinedRect.UnionRect(combinedRect, colBGRect);

    if (combinedRect.Intersects(mDirtyRect)) {
      bool passCell = aPassThrough || cell->IsPseudoStackingContextFromStyle();
      result &=
        PaintCell(cell, aRowGroupBGData, aRowBGData, cellBGRect, rowBGRect,
                  rowGroupBGRect, colBGRect, passCell);
    }
  }

  return result;
}

DrawResult
TableBackgroundPainter::PaintCell(nsTableCellFrame* aCell,
                                  const TableBackgroundData& aRowGroupBGData,
                                  const TableBackgroundData& aRowBGData,
                                  nsRect&           aCellBGRect,
                                  nsRect&           aRowBGRect,
                                  nsRect&           aRowGroupBGRect,
                                  nsRect&           aColBGRect,
                                  bool              aPassSelf)
{
  MOZ_ASSERT(aCell, "null frame");

  const nsStyleTableBorder* cellTableStyle;
  cellTableStyle = aCell->StyleTableBorder();
  if (NS_STYLE_TABLE_EMPTY_CELLS_SHOW != cellTableStyle->mEmptyCells &&
      aCell->GetContentEmpty() && !mIsBorderCollapse) {
    return DrawResult::SUCCESS;
  }

  int32_t colIndex;
  aCell->GetColIndex(colIndex);
  // We're checking mNumCols instead of mCols.Length() here because mCols can
  // be empty even if mNumCols > 0.
  NS_ASSERTION(size_t(colIndex) < mNumCols, "out-of-bounds column index");
  if (size_t(colIndex) >= mNumCols) {
    return DrawResult::SUCCESS;
  }

  // If callers call PaintRowGroup or PaintRow directly, we haven't processed
  // our columns. Ignore column / col group backgrounds in that case.
  bool haveColumns = !mCols.IsEmpty();

  DrawResult result = DrawResult::SUCCESS;

  //Paint column group background
  if (haveColumns && mCols[colIndex].mColGroup.IsVisible()) {
    nsCSSRendering::PaintBGParams params =
      nsCSSRendering::PaintBGParams::ForAllLayers(*mPresContext, mRenderingContext,
                                                 mDirtyRect,
                                                 mCols[colIndex].mColGroup.mRect + mRenderPt,
                                                 mCols[colIndex].mColGroup.mFrame,
                                                 mBGPaintFlags);
    params.bgClipRect = &aColBGRect;
    result &=
      nsCSSRendering::PaintBackgroundWithSC(params,
                                            mCols[colIndex].mColGroup.mFrame->StyleContext(),
                                            mCols[colIndex].mColGroup.StyleBorder(mZeroBorder));
  }

  //Paint column background
  if (haveColumns && mCols[colIndex].mCol.IsVisible()) {
    nsCSSRendering::PaintBGParams params =
      nsCSSRendering::PaintBGParams::ForAllLayers(*mPresContext, mRenderingContext,
                                                  mDirtyRect,
                                                  mCols[colIndex].mCol.mRect + mRenderPt,
                                                  mCols[colIndex].mCol.mFrame,
                                                  mBGPaintFlags);
    params.bgClipRect = &aColBGRect;
    result &=
      nsCSSRendering::PaintBackgroundWithSC(params,
                                            mCols[colIndex].mCol.mFrame->StyleContext(),
                                            mCols[colIndex].mCol.StyleBorder(mZeroBorder));
  }

  //Paint row group background
  if (aRowGroupBGData.IsVisible()) {
    nsCSSRendering::PaintBGParams params =
      nsCSSRendering::PaintBGParams::ForAllLayers(*mPresContext, mRenderingContext,
                                                  mDirtyRect,
                                                  aRowGroupBGData.mRect + mRenderPt,
                                                  aRowGroupBGData.mFrame, mBGPaintFlags);
    params.bgClipRect = &aRowGroupBGRect;
    result &=
      nsCSSRendering::PaintBackgroundWithSC(params,
                                            aRowGroupBGData.mFrame->StyleContext(),
                                            aRowGroupBGData.StyleBorder(mZeroBorder));
  }

  //Paint row background
  if (aRowBGData.IsVisible()) {
    nsCSSRendering::PaintBGParams params =
      nsCSSRendering::PaintBGParams::ForAllLayers(*mPresContext, mRenderingContext,
                                                  mDirtyRect,
                                                  aRowBGData.mRect + mRenderPt,
                                                  aRowBGData.mFrame, mBGPaintFlags);
    params.bgClipRect = &aRowBGRect;
    result &=
      nsCSSRendering::PaintBackgroundWithSC(params,
                                            aRowBGData.mFrame->StyleContext(),
                                            aRowBGData.StyleBorder(mZeroBorder));
  }

  //Paint cell background in border-collapse unless we're just passing
  if (mIsBorderCollapse && !aPassSelf) {
    result &=
      aCell->PaintCellBackground(mRenderingContext, mDirtyRect,
                                 aCellBGRect.TopLeft(), mBGPaintFlags);
  }

  return result;
}

void
TableBackgroundPainter::ComputeCellBackgrounds(nsTableCellFrame* aCell,
                                               const TableBackgroundData& aRowGroupBGData,
                                               const TableBackgroundData& aRowBGData,
                                               nsRect&           aCellBGRect,
                                               nsRect&           aRowBGRect,
                                               nsRect&           aRowGroupBGRect,
                                               nsRect&           aColBGRect)
{
  // We need to compute table background layer rects for this cell space,
  // adjusted for possible relative positioning. This behavior is not specified
  // at the time of this writing, but the approach below should be web
  // compatible.
  //
  // Our goal is that relative positioning of a table part should leave
  // backgrounds *under* that part unchanged. ("Under" being defined by CSS 2.1
  // Section 17.5.1.) If a cell is positioned, we do not expect the row
  // background to move. On the other hand, the backgrounds of layers *above*
  // the positioned part are taken along for the ride -- for example,
  // positioning a row group will also cause the row background to be drawn in
  // the new location, unless it has further positioning applied.
  //
  // Each table part layer has its position stored in the coordinate space of
  // the layer below (which is to say, its geometric parent), and the stored
  // position is the post-relative-positioning one.  The position of each
  // background layer rect is thus determined by peeling off successive table
  // part layers, removing the contribution of each layer's positioning one by
  // one.  Every rect we generate will be the same size, the size of the cell
  // space.

  // We cannot rely on the row group background data to be available, since some
  // callers enter through PaintRow.
  nsIFrame* rowGroupFrame =
    aRowGroupBGData.mFrame ? aRowGroupBGData.mFrame : aRowBGData.mFrame->GetParent();

  // The cell background goes at the cell's position, translated to use the same
  // coordinate system as aRowBGData.
  aCellBGRect = aCell->GetRect() + aRowBGData.mRect.TopLeft() + mRenderPt;

  // The row background goes at the normal position of the cell, which is to say
  // the position without relative positioning applied.
  aRowBGRect = aCellBGRect + (aCell->GetNormalPosition() - aCell->GetPosition());

  // The row group background goes at the position we'd find the cell if neither
  // the cell's relative positioning nor the row's were applied.
  aRowGroupBGRect = aRowBGRect +
                    (aRowBGData.mFrame->GetNormalPosition() - aRowBGData.mFrame->GetPosition());

  // The column and column group backgrounds (they're always at the same
  // location, since relative positioning doesn't apply to columns or column
  // groups) are drawn at the position we'd find the cell if none of the cell's,
  // row's, or row group's relative positioning were applied.
  aColBGRect = aRowGroupBGRect +
             (rowGroupFrame->GetNormalPosition() - rowGroupFrame->GetPosition());

}
