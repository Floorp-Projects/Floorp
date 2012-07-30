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
   The class then loops through the row groups, rows, and cells. It uses
   the mRowGroup and mRow TableBackgroundData structs to cache data for
   the current frame in the loop. At the cell level, it paints the backgrounds,
   one over the other, inside the cell rect.

   The exception to this pattern is when a table element creates a (pseudo)
   stacking context. Elements with stacking contexts (e.g., 'opacity' applied)
   are <dfn>passed through</dfn>, which means their data (and their
   descendants' data) are not cached. The full loop is still executed, however,
   so that underlying layers can get painted at the cell level.

   The TableBackgroundPainter is then destroyed.

   Elements with stacking contexts set up their own painter to finish the
   painting process, since they were skipped. They call the appropriate
   sub-part of the loop (e.g. PaintRow) which will paint the frame and
   descendants. Note that it is permissible according to CSS2.1 to ignore'
   'position:relative' (and implicitly, 'opacity') on table parts so that
   table parts can never create stacking contexts; if we want to, we can
   implement that, and then we won't have to deal with TableBackgroundPainter
   being used anywhere but from the nsTableFrame.
   
   XXX views are going 
 */

TableBackgroundPainter::TableBackgroundData::TableBackgroundData()
  : mFrame(nullptr),
    mVisible(false),
    mBorder(nullptr),
    mSynthBorder(nullptr)
{
  MOZ_COUNT_CTOR(TableBackgroundData);
}

TableBackgroundPainter::TableBackgroundData::~TableBackgroundData()
{
  NS_ASSERTION(!mSynthBorder, "must call Destroy before dtor");
  MOZ_COUNT_DTOR(TableBackgroundData);
}

void
TableBackgroundPainter::TableBackgroundData::Destroy(nsPresContext* aPresContext)
{
  NS_PRECONDITION(aPresContext, "null prescontext");
  if (mSynthBorder) {
    mSynthBorder->Destroy(aPresContext);
    mSynthBorder = nullptr;
  }
}

void
TableBackgroundPainter::TableBackgroundData::Clear()
{
  mRect.SetEmpty();
  mFrame = nullptr;
  mBorder = nullptr;
  mVisible = false;
}

void
TableBackgroundPainter::TableBackgroundData::SetFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null frame");
  mFrame = aFrame;
  mRect = aFrame->GetRect();
}

void
TableBackgroundPainter::TableBackgroundData::SetData()
{
  NS_PRECONDITION(mFrame, "null frame");
  if (mFrame->IsVisibleForPainting()) {
    mVisible = true;
    mBorder = mFrame->GetStyleBorder();
  }
}

void
TableBackgroundPainter::TableBackgroundData::SetFull(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null frame");
  SetFrame(aFrame);
  SetData();
}

inline bool
TableBackgroundPainter::TableBackgroundData::ShouldSetBCBorder()
{
  /* we only need accurate border data when positioning background images*/
  if (!mVisible) {
    return false;
  }

  const nsStyleBackground *bg = mFrame->GetStyleBackground();
  NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
    if (!bg->mLayers[i].mImage.IsEmpty())
      return true;
  }
  return false;
}

nsresult
TableBackgroundPainter::TableBackgroundData::SetBCBorder(nsMargin& aBorder,
                                                         TableBackgroundPainter* aPainter)
{
  NS_PRECONDITION(aPainter, "null painter");
  if (!mSynthBorder) {
    mSynthBorder = new (aPainter->mPresContext)
                        nsStyleBorder(aPainter->mZeroBorder);
    if (!mSynthBorder) return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_FOR_CSS_SIDES(side) {
    mSynthBorder->SetBorderWidth(side, aBorder.Side(side));
  }
  
  mBorder = mSynthBorder;
  return NS_OK;
}

TableBackgroundPainter::TableBackgroundPainter(nsTableFrame*        aTableFrame,
                                               Origin               aOrigin,
                                               nsPresContext*       aPresContext,
                                               nsRenderingContext& aRenderingContext,
                                               const nsRect&        aDirtyRect,
                                               const nsPoint&       aRenderPt,
                                               PRUint32             aBGPaintFlags)
  : mPresContext(aPresContext),
    mRenderingContext(aRenderingContext),
    mRenderPt(aRenderPt),
    mDirtyRect(aDirtyRect),
    mOrigin(aOrigin),
    mCols(nullptr),
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
  if (mCols) {
    TableBackgroundData* lastColGroup = nullptr;
    for (PRUint32 i = 0; i < mNumCols; i++) {
      if (mCols[i].mColGroup != lastColGroup) {
        lastColGroup = mCols[i].mColGroup;
        NS_ASSERTION(mCols[i].mColGroup, "colgroup data should not be null - bug 237421");
        // we need to wallpaper a over zero pointer deref, bug 237421 will have the real fix
        if(lastColGroup)
          lastColGroup->Destroy(mPresContext);
        delete lastColGroup;
      }
      mCols[i].mColGroup = nullptr;
      mCols[i].mCol.Destroy(mPresContext);
    }
    delete [] mCols;
  }
  mRowGroup.Destroy(mPresContext);
  mRow.Destroy(mPresContext);
  MOZ_COUNT_DTOR(TableBackgroundPainter);
}

nsresult
TableBackgroundPainter::PaintTableFrame(nsTableFrame*         aTableFrame,
                                        nsTableRowGroupFrame* aFirstRowGroup,
                                        nsTableRowGroupFrame* aLastRowGroup,
                                        const nsMargin&       aDeflate)
{
  NS_PRECONDITION(aTableFrame, "null frame");
  TableBackgroundData tableData;
  tableData.SetFull(aTableFrame);
  tableData.mRect.MoveTo(0,0); //using table's coords
  tableData.mRect.Deflate(aDeflate);
  if (mIsBorderCollapse && tableData.ShouldSetBCBorder()) {
    if (aFirstRowGroup && aLastRowGroup && mNumCols > 0) {
      //only handle non-degenerate tables; we need a more robust BC model
      //to make degenerate tables' borders reasonable to deal with
      nsMargin border, tempBorder;
      nsTableColFrame* colFrame = aTableFrame->GetColFrame(mNumCols - 1);
      if (colFrame) {
        colFrame->GetContinuousBCBorderWidth(tempBorder);
      }
      border.right = tempBorder.right;

      aLastRowGroup->GetContinuousBCBorderWidth(tempBorder);
      border.bottom = tempBorder.bottom;

      nsTableRowFrame* rowFrame = aFirstRowGroup->GetFirstRow();
      if (rowFrame) {
        rowFrame->GetContinuousBCBorderWidth(tempBorder);
        border.top = tempBorder.top;
      }

      border.left = aTableFrame->GetContinuousLeftBCBorderWidth();

      nsresult rv = tableData.SetBCBorder(border, this);
      if (NS_FAILED(rv)) {
        tableData.Destroy(mPresContext);
        return rv;
      }
    }
  }
  if (tableData.IsVisible()) {
    nsCSSRendering::PaintBackgroundWithSC(mPresContext, mRenderingContext,
                                          tableData.mFrame, mDirtyRect,
                                          tableData.mRect + mRenderPt,
                                          tableData.mFrame->GetStyleContext(),
                                          *tableData.mBorder,
                                          mBGPaintFlags);
  }
  tableData.Destroy(mPresContext);
  return NS_OK;
}

void
TableBackgroundPainter::TranslateContext(nscoord aDX,
                                         nscoord aDY)
{
  mRenderPt += nsPoint(aDX, aDY);
  if (mCols) {
    TableBackgroundData* lastColGroup = nullptr;
    for (PRUint32 i = 0; i < mNumCols; i++) {
      mCols[i].mCol.mRect.MoveBy(-aDX, -aDY);
      if (lastColGroup != mCols[i].mColGroup) {
        NS_ASSERTION(mCols[i].mColGroup, "colgroup data should not be null - bug 237421");
        // we need to wallpaper a over zero pointer deref, bug 237421 will have the real fix
        if (!mCols[i].mColGroup)
          return;
        mCols[i].mColGroup->mRect.MoveBy(-aDX, -aDY);
        lastColGroup = mCols[i].mColGroup;
      }
    }
  }
}

nsresult
TableBackgroundPainter::PaintTable(nsTableFrame*   aTableFrame,
                                   const nsMargin& aDeflate,
                                   bool            aPaintTableBackground)
{
  NS_PRECONDITION(aTableFrame, "null table frame");

  nsTableFrame::RowGroupArray rowGroups;
  aTableFrame->OrderRowGroups(rowGroups);

  if (rowGroups.Length() < 1) { //degenerate case
    if (aPaintTableBackground) {
      PaintTableFrame(aTableFrame, nullptr, nullptr, nsMargin(0,0,0,0));
    }
    /* No cells; nothing else to paint */
    return NS_OK;
  }

  if (aPaintTableBackground) {
    PaintTableFrame(aTableFrame, rowGroups[0], rowGroups[rowGroups.Length() - 1],
                    aDeflate);
  }

  /*Set up column background/border data*/
  if (mNumCols > 0) {
    nsFrameList& colGroupList = aTableFrame->GetColGroups();
    NS_ASSERTION(colGroupList.FirstChild(), "table should have at least one colgroup");

    mCols = new ColData[mNumCols];
    if (!mCols) return NS_ERROR_OUT_OF_MEMORY;

    TableBackgroundData* cgData = nullptr;
    nsMargin border;
    /* BC left borders aren't stored on cols, but the previous column's
       right border is the next one's left border.*/
    //Start with table's left border.
    nscoord lastLeftBorder = aTableFrame->GetContinuousLeftBCBorderWidth();
    for (nsTableColGroupFrame* cgFrame = static_cast<nsTableColGroupFrame*>(colGroupList.FirstChild());
         cgFrame; cgFrame = static_cast<nsTableColGroupFrame*>(cgFrame->GetNextSibling())) {

      if (cgFrame->GetColCount() < 1) {
        //No columns, no cells, so no need for data
        continue;
      }

      /*Create data struct for column group*/
      cgData = new TableBackgroundData;
      if (!cgData) return NS_ERROR_OUT_OF_MEMORY;
      cgData->SetFull(cgFrame);
      if (mIsBorderCollapse && cgData->ShouldSetBCBorder()) {
        border.left = lastLeftBorder;
        cgFrame->GetContinuousBCBorderWidth(border);
        nsresult rv = cgData->SetBCBorder(border, this);
        if (NS_FAILED(rv)) {
          cgData->Destroy(mPresContext);
          delete cgData;
          return rv;
        }
      }

      // Boolean that indicates whether mCols took ownership of cgData
      bool cgDataOwnershipTaken = false;
      
      /*Loop over columns in this colgroup*/
      for (nsTableColFrame* col = cgFrame->GetFirstColumn(); col;
           col = static_cast<nsTableColFrame*>(col->GetNextSibling())) {
        /*Create data struct for column*/
        PRUint32 colIndex = col->GetColIndex();
        NS_ASSERTION(colIndex < mNumCols, "prevent array boundary violation");
        if (mNumCols <= colIndex)
          break;
        mCols[colIndex].mCol.SetFull(col);
        //Bring column mRect into table's coord system
        mCols[colIndex].mCol.mRect.MoveBy(cgData->mRect.x, cgData->mRect.y);
        //link to parent colgroup's data
        mCols[colIndex].mColGroup = cgData;
        cgDataOwnershipTaken = true;
        if (mIsBorderCollapse) {
          border.left = lastLeftBorder;
          lastLeftBorder = col->GetContinuousBCBorderWidth(border);
          if (mCols[colIndex].mCol.ShouldSetBCBorder()) {
            nsresult rv = mCols[colIndex].mCol.SetBCBorder(border, this);
            if (NS_FAILED(rv)) return rv;
          }
        }
      }

      if (!cgDataOwnershipTaken) {
        cgData->Destroy(mPresContext);
        delete cgData;
      }
    }
  }

  for (PRUint32 i = 0; i < rowGroups.Length(); i++) {
    nsTableRowGroupFrame* rg = rowGroups[i];
    mRowGroup.SetFrame(rg);
    // Need to compute the right rect via GetOffsetTo, since the row
    // group may not be a child of the table.
    mRowGroup.mRect.MoveTo(rg->GetOffsetTo(aTableFrame));
    if (mRowGroup.mRect.Intersects(mDirtyRect - mRenderPt)) {
      nsresult rv = PaintRowGroup(rg, rg->IsPseudoStackingContextFromStyle());
      if (NS_FAILED(rv)) return rv;
    }
  }
  return NS_OK;
}

nsresult
TableBackgroundPainter::PaintRowGroup(nsTableRowGroupFrame* aFrame,
                                      bool                  aPassThrough)
{
  NS_PRECONDITION(aFrame, "null frame");

  if (!mRowGroup.mFrame) {
    mRowGroup.SetFrame(aFrame);
  }

  nsTableRowFrame* firstRow = aFrame->GetFirstRow();

  /* Load row group data */
  if (!aPassThrough) {
    mRowGroup.SetData();
    if (mIsBorderCollapse && mRowGroup.ShouldSetBCBorder()) {
      nsMargin border;
      if (firstRow) {
        //pick up first row's top border (= rg top border)
        firstRow->GetContinuousBCBorderWidth(border);
        /* (row group doesn't store its top border) */
      }
      //overwrite sides+bottom borders with rg's own
      aFrame->GetContinuousBCBorderWidth(border);
      nsresult res = mRowGroup.SetBCBorder(border, this);
      if (!NS_SUCCEEDED(res)) {
        return res;
      }
    }
    aPassThrough = !mRowGroup.IsVisible();
  }

  /* translate everything into row group coord system*/
  if (eOrigin_TableRowGroup != mOrigin) {
    TranslateContext(mRowGroup.mRect.x, mRowGroup.mRect.y);
  }
  nsRect rgRect = mRowGroup.mRect;
  mRowGroup.mRect.MoveTo(0, 0);

  /* Find the right row to start with */
  nscoord ignored; // We don't care about overflow above, since what we really
                   // care about are backgrounds and overflow above doesn't
                   // correspond to backgrounds, since cells can't span up from
                   // their originating row.  We do care about overflow below,
                   // however, since that can be due to rowspans.

  // Note that mDirtyRect  - mRenderPt is guaranteed to be in the row
  // group's coordinate system here, so passing its .y to
  // GetFirstRowContaining is ok.
  nsIFrame* cursor = aFrame->GetFirstRowContaining(mDirtyRect.y - mRenderPt.y, &ignored);

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
  
  /* Finally paint */
  for (; row; row = row->GetNextRow()) {
    mRow.SetFrame(row);
    if (mDirtyRect.YMost() - mRenderPt.y < mRow.mRect.y) { // Intersect wouldn't handle
                                             // rowspans.

      // All done; cells originating in later rows can't intersect mDirtyRect.
      break;
    }
    
    nsresult rv = PaintRow(row, aPassThrough || row->IsPseudoStackingContextFromStyle());
    if (NS_FAILED(rv)) return rv;
  }

  /* translate back into table coord system */
  if (eOrigin_TableRowGroup != mOrigin) {
    TranslateContext(-rgRect.x, -rgRect.y);
  }
  
  /* unload rg data */
  mRowGroup.Clear();

  return NS_OK;
}

nsresult
TableBackgroundPainter::PaintRow(nsTableRowFrame* aFrame,
                                 bool             aPassThrough)
{
  NS_PRECONDITION(aFrame, "null frame");

  if (!mRow.mFrame) {
    mRow.SetFrame(aFrame);
  }

  /* Load row data */
  if (!aPassThrough) {
    mRow.SetData();
    if (mIsBorderCollapse && mRow.ShouldSetBCBorder()) {
      nsMargin border;
      nsTableRowFrame* nextRow = aFrame->GetNextRow();
      if (nextRow) { //outer top below us is inner bottom for us
        border.bottom = nextRow->GetOuterTopContBCBorderWidth();
      }
      else { //acquire rg's bottom border
        nsTableRowGroupFrame* rowGroup = static_cast<nsTableRowGroupFrame*>(aFrame->GetParent());
        rowGroup->GetContinuousBCBorderWidth(border);
      }
      //get the rest of the borders; will overwrite all but bottom
      aFrame->GetContinuousBCBorderWidth(border);

      nsresult res = mRow.SetBCBorder(border, this);
      if (!NS_SUCCEEDED(res)) {
        return res;
      }
    }
    aPassThrough = !mRow.IsVisible();
  }

  /* Translate */
  if (eOrigin_TableRow == mOrigin) {
    /* If we originate from the row, then make the row the origin. */
    mRow.mRect.MoveTo(0, 0);
  }
  //else: Use row group's coord system -> no translation necessary

  for (nsTableCellFrame* cell = aFrame->GetFirstCell(); cell; cell = cell->GetNextCell()) {
    //Translate to use the same coord system as mRow.
    mCellRect = cell->GetRect() + mRow.mRect.TopLeft() + mRenderPt;
    if (mCellRect.Intersects(mDirtyRect)) {
      nsresult rv = PaintCell(cell, aPassThrough || cell->IsPseudoStackingContextFromStyle());
      if (NS_FAILED(rv)) return rv;
    }
  }

  /* Unload row data */
  mRow.Clear();
  return NS_OK;
}

nsresult
TableBackgroundPainter::PaintCell(nsTableCellFrame* aCell,
                                  bool aPassSelf)
{
  NS_PRECONDITION(aCell, "null frame");

  const nsStyleTableBorder* cellTableStyle;
  cellTableStyle = aCell->GetStyleTableBorder();
  if (!(NS_STYLE_TABLE_EMPTY_CELLS_SHOW == cellTableStyle->mEmptyCells ||
        NS_STYLE_TABLE_EMPTY_CELLS_SHOW_BACKGROUND == cellTableStyle->mEmptyCells)
      && aCell->GetContentEmpty() && !mIsBorderCollapse) {
    return NS_OK;
  }

  PRInt32 colIndex;
  aCell->GetColIndex(colIndex);
  NS_ASSERTION(colIndex < PRInt32(mNumCols), "prevent array boundary violation");
  if (PRInt32(mNumCols) <= colIndex)
    return NS_OK;

  //Paint column group background
  if (mCols && mCols[colIndex].mColGroup && mCols[colIndex].mColGroup->IsVisible()) {
    nsCSSRendering::PaintBackgroundWithSC(mPresContext, mRenderingContext,
                                          mCols[colIndex].mColGroup->mFrame, mDirtyRect,
                                          mCols[colIndex].mColGroup->mRect + mRenderPt,
                                          mCols[colIndex].mColGroup->mFrame->GetStyleContext(),
                                          *mCols[colIndex].mColGroup->mBorder,
                                          mBGPaintFlags, &mCellRect);
  }

  //Paint column background
  if (mCols && mCols[colIndex].mCol.IsVisible()) {
    nsCSSRendering::PaintBackgroundWithSC(mPresContext, mRenderingContext,
                                          mCols[colIndex].mCol.mFrame, mDirtyRect,
                                          mCols[colIndex].mCol.mRect + mRenderPt,
                                          mCols[colIndex].mCol.mFrame->GetStyleContext(),
                                          *mCols[colIndex].mCol.mBorder,
                                          mBGPaintFlags, &mCellRect);
  }

  //Paint row group background
  if (mRowGroup.IsVisible()) {
    nsCSSRendering::PaintBackgroundWithSC(mPresContext, mRenderingContext,
                                          mRowGroup.mFrame, mDirtyRect,
                                          mRowGroup.mRect + mRenderPt,
                                          mRowGroup.mFrame->GetStyleContext(),
                                          *mRowGroup.mBorder,
                                          mBGPaintFlags, &mCellRect);
  }

  //Paint row background
  if (mRow.IsVisible()) {
    nsCSSRendering::PaintBackgroundWithSC(mPresContext, mRenderingContext,
                                          mRow.mFrame, mDirtyRect,
                                          mRow.mRect + mRenderPt,
                                          mRow.mFrame->GetStyleContext(),
                                          *mRow.mBorder,
                                          mBGPaintFlags, &mCellRect);
  }

  //Paint cell background in border-collapse unless we're just passing
  if (mIsBorderCollapse && !aPassSelf) {
    aCell->PaintCellBackground(mRenderingContext, mDirtyRect,
                               mCellRect.TopLeft(), mBGPaintFlags);
  }

  return NS_OK;
}
