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
 * The Original Code is TableBackgroundPainter implementation.
 *
 * The Initial Developer of the Original Code is
 * Elika J. Etemad ("fantasai") <fantasai@inkedblade.net>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsTableFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include "nsTablePainter.h"
#include "nsCSSRendering.h"

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

       For example, the continous border on the top edge of a
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

   The exception to this pattern is when a table element has a view.
   Elements with views are <dfn>passed through</dfn>, which means their data
   (and their descendants' data) are not cached. The full loop is still
   executed, however, so that underlying layers can get painted at the cell
   level.

   The TableBackgroundPainter is then destroyed.

   Elements with views set up their own painter to finish the painting
   process, since they were skipped. They call the appropriate sub-part
   of the loop (e.g. PaintRow) which will paint the frame and descendants.
 */

TableBackgroundPainter::TableBackgroundData::TableBackgroundData()
  : mFrame(nsnull),
    mBackground(nsnull),
    mBorder(nsnull),
    mSynthBorder(nsnull)
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
    mSynthBorder = nsnull;
  }
}

void
TableBackgroundPainter::TableBackgroundData::Clear()
{
  mRect.Empty();
  mFrame = nsnull;
  mBorder = nsnull;
  mBackground = nsnull;
}

void
TableBackgroundPainter::TableBackgroundData::SetFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null frame");
  mFrame = aFrame;
  mRect = aFrame->GetRect();
}

void
TableBackgroundPainter::TableBackgroundData::SetFull(nsPresContext*      aPresContext,
                                                     nsIRenderingContext& aRenderingContext,
                                                     nsIFrame*            aFrame)
{
  NS_PRECONDITION(aFrame, "null frame");
  mFrame = aFrame;
  mRect = aFrame->GetRect();
  /* IsVisibleForPainting doesn't use aRenderingContext except in nsTextFrames,
     so we're not going to bother translating.*/
  PRBool isVisible;
  nsresult rv = aFrame->IsVisibleForPainting(aPresContext, aRenderingContext,
                                             PR_TRUE, &isVisible);
  if (NS_SUCCEEDED(rv) && isVisible &&
      aFrame->GetStyleVisibility()->IsVisible()) {
    mBackground = aFrame->GetStyleBackground();
    mBorder = aFrame->GetStyleBorder();
  }
}

inline PRBool
TableBackgroundPainter::TableBackgroundData::ShouldSetBCBorder()
{
  /* we only need accurate border data when positioning background images*/
  return mBackground && !(mBackground->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE);
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

  nsStyleCoord coord(aBorder.top);
  mSynthBorder->mBorder.SetTop(coord);
  coord.SetCoordValue(aBorder.right);
  mSynthBorder->mBorder.SetRight(coord);
  coord.SetCoordValue(aBorder.bottom);
  mSynthBorder->mBorder.SetBottom(coord);
  coord.SetCoordValue(aBorder.left);
  mSynthBorder->mBorder.SetLeft(coord);
  mSynthBorder->RecalcData();

  mBorder = mSynthBorder;
  return NS_OK;
}

TableBackgroundPainter::TableBackgroundPainter(nsTableFrame*        aTableFrame,
                                               Origin               aOrigin,
                                               nsPresContext*      aPresContext,
                                               nsIRenderingContext& aRenderingContext,
                                               const nsRect&        aDirtyRect)
  : mPresContext(aPresContext),
    mRenderingContext(aRenderingContext),
    mDirtyRect(aDirtyRect),
    mOrigin(aOrigin),
    mCols(nsnull),
    mZeroBorder(aPresContext)
{
  MOZ_COUNT_CTOR(TableBackgroundPainter);

  mZeroBorder.SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_SOLID);
  mZeroBorder.SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_SOLID);
  mZeroBorder.SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_SOLID);
  mZeroBorder.SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_SOLID);
  nsStyleCoord coord(0);
  mZeroBorder.mBorder.SetTop(coord);
  mZeroBorder.mBorder.SetRight(coord);
  mZeroBorder.mBorder.SetBottom(coord);
  mZeroBorder.mBorder.SetLeft(coord);
  mZeroBorder.RecalcData();

  mZeroPadding.RecalcData();

  mP2t = mPresContext->ScaledPixelsToTwips();
  mIsBorderCollapse = aTableFrame->IsBorderCollapse();
#ifdef DEBUG
  mCompatMode = mPresContext->CompatibilityMode();
#endif
  mNumCols = aTableFrame->GetColCount();
}

TableBackgroundPainter::~TableBackgroundPainter()
{
  if (mCols) {
    TableBackgroundData* lastColGroup = nsnull;
    for (PRUint32 i = 0; i < mNumCols; i++) {
      if (mCols[i].mColGroup != lastColGroup) {
        lastColGroup = mCols[i].mColGroup;
        NS_ASSERTION(mCols[i].mColGroup, "colgroup data should not be null - bug 237421");
        // we need to wallpaper a over zero pointer deref, bug 237421 will have the real fix
        if(lastColGroup)
          lastColGroup->Destroy(mPresContext);
        delete lastColGroup;
      }
      mCols[i].mColGroup = nsnull;
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
                                        nsMargin*             aDeflate)
{
  NS_PRECONDITION(aTableFrame, "null frame");
  TableBackgroundData tableData;
  tableData.SetFull(mPresContext, mRenderingContext, aTableFrame);
  tableData.mRect.MoveTo(0,0); //using table's coords
  if (aDeflate) {
    tableData.mRect.Deflate(*aDeflate);
  }
  if (mIsBorderCollapse && tableData.ShouldSetBCBorder()) {
    if (aFirstRowGroup && aLastRowGroup && mNumCols > 0) {
      //only handle non-degenerate tables; we need a more robust BC model
      //to make degenerate tables' borders reasonable to deal with
      nsMargin border, tempBorder;
      nsTableColFrame* colFrame = aTableFrame->GetColFrame(mNumCols - 1);
      if (colFrame) {
        colFrame->GetContinuousBCBorderWidth(mP2t, tempBorder);
      }
      border.right = tempBorder.right;

      aLastRowGroup->GetContinuousBCBorderWidth(mP2t, tempBorder);
      border.bottom = tempBorder.bottom;

      nsTableRowFrame* rowFrame = aFirstRowGroup->GetFirstRow();
      if (rowFrame) {
        rowFrame->GetContinuousBCBorderWidth(mP2t, tempBorder);
        border.top = tempBorder.top;
      }

      border.left = aTableFrame->GetContinuousLeftBCBorderWidth(mP2t);

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
                                          tableData.mRect,
                                          *tableData.mBackground,
                                          *tableData.mBorder,
                                          mZeroPadding, PR_TRUE);
  }
  tableData.Destroy(mPresContext);
  return NS_OK;
}

void
TableBackgroundPainter::TranslateContext(nscoord aDX,
                                         nscoord aDY)
{
  mRenderingContext.Translate(aDX, aDY);
  mDirtyRect.MoveBy(-aDX, -aDY);
  if (mCols) {
    TableBackgroundData* lastColGroup = nsnull;
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
TableBackgroundPainter::PaintTable(nsTableFrame* aTableFrame,
                                   nsMargin*     aDeflate)
{
  NS_PRECONDITION(aTableFrame, "null table frame");

  nsVoidArray rowGroups;
  PRUint32 numRowGroups;
  aTableFrame->OrderRowGroups(rowGroups, numRowGroups);

  if (numRowGroups < 1) { //degenerate case
    PaintTableFrame(aTableFrame,nsnull, nsnull, nsnull);
    /* No cells; nothing else to paint */
    return NS_OK;
  }

  PaintTableFrame(aTableFrame,
                  aTableFrame->GetRowGroupFrame(NS_STATIC_CAST(nsIFrame*, rowGroups.ElementAt(0))),
                  aTableFrame->GetRowGroupFrame(NS_STATIC_CAST(nsIFrame*, rowGroups.ElementAt(numRowGroups - 1))),
                  aDeflate);

  /*Set up column background/border data*/
  if (mNumCols > 0) {
    nsFrameList& colGroupList = aTableFrame->GetColGroups();
    NS_ASSERTION(colGroupList.FirstChild(), "table should have at least one colgroup");

    mCols = new ColData[mNumCols];
    if (!mCols) return NS_ERROR_OUT_OF_MEMORY;

    TableBackgroundData* cgData = nsnull;
    nsMargin border;
    /* BC left borders aren't stored on cols, but the previous column's
       right border is the next one's left border.*/
    //Start with table's left border.
    nscoord lastLeftBorder = aTableFrame->GetContinuousLeftBCBorderWidth(mP2t);
    for (nsTableColGroupFrame* cgFrame = NS_STATIC_CAST(nsTableColGroupFrame*, colGroupList.FirstChild());
         cgFrame; cgFrame = NS_STATIC_CAST(nsTableColGroupFrame*, cgFrame->GetNextSibling())) {

      if (cgFrame->GetColCount() < 1) {
        //No columns, no cells, so no need for data
        continue;
      }

      /*Create data struct for column group*/
      cgData = new TableBackgroundData;
      if (!cgData) return NS_ERROR_OUT_OF_MEMORY;
      cgData->SetFull(mPresContext, mRenderingContext, cgFrame);
      if (mIsBorderCollapse && cgData->ShouldSetBCBorder()) {
        border.left = lastLeftBorder;
        cgFrame->GetContinuousBCBorderWidth(mP2t, border);
        nsresult rv = cgData->SetBCBorder(border, this);
        if (NS_FAILED(rv)) {
          cgData->Destroy(mPresContext);
          delete cgData;
          return rv;
        }
      }

      /*Loop over columns in this colgroup*/
      if (cgData->IsVisible()) {
        for (nsTableColFrame* col = cgFrame->GetFirstColumn(); col;
             col = NS_STATIC_CAST(nsTableColFrame*, col->GetNextSibling())) {
          /*Create data struct for column*/
          PRUint32 colIndex = col->GetColIndex();
          NS_ASSERTION(colIndex < mNumCols, "prevent array boundary violation");
          if (mNumCols <= colIndex)
            break;
          mCols[colIndex].mCol.SetFull(mPresContext, mRenderingContext, col);
          //Bring column mRect into table's coord system
          mCols[colIndex].mCol.mRect.MoveBy(cgData->mRect.x, cgData->mRect.y);
          //link to parent colgroup's data
          mCols[colIndex].mColGroup = cgData;
          if (mIsBorderCollapse) {
            border.left = lastLeftBorder;
            lastLeftBorder = col->GetContinuousBCBorderWidth(mP2t, border);
            if (mCols[colIndex].mCol.ShouldSetBCBorder()) {
              nsresult rv = mCols[colIndex].mCol.SetBCBorder(border, this);
              if (NS_FAILED(rv)) return rv;
            }
          }
        }
      }
    }
  }

  for (PRUint32 i = 0; i < numRowGroups; i++) {
    nsTableRowGroupFrame* rg = nsTableFrame::GetRowGroupFrame(NS_STATIC_CAST(nsIFrame*, rowGroups.ElementAt(i)));
    nsRect rgRect = rg->GetRect();
    if (rgRect.Intersects(mDirtyRect)) {
      nsresult rv = PaintRowGroup(rg, rg->HasView());
      if (NS_FAILED(rv)) return rv;
    }
  }
  return NS_OK;
}

nsresult
TableBackgroundPainter::PaintRowGroup(nsTableRowGroupFrame* aFrame,
                                      PRBool                aPassThrough)
{
  NS_PRECONDITION(aFrame, "null frame");

  nsTableRowFrame* firstRow = aFrame->GetFirstRow();

  /* Load row group data */
  if (!aPassThrough) {
    mRowGroup.SetFull(mPresContext, mRenderingContext, aFrame);
    if (mIsBorderCollapse && mRowGroup.ShouldSetBCBorder()) {
      nsMargin border;
      if (firstRow) {
        //pick up first row's top border (= rg top border)
        firstRow->GetContinuousBCBorderWidth(mP2t, border);
        /* (row group doesn't store its top border) */
      }
      //overwrite sides+bottom borders with rg's own
      aFrame->GetContinuousBCBorderWidth(mP2t, border);
      nsresult res = mRowGroup.SetBCBorder(border, this);
      if (!NS_SUCCEEDED(res)) {
        return res;
      }
    }
    aPassThrough = !mRowGroup.IsVisible();
  }
  else {
    mRowGroup.SetFrame(aFrame);
  }

  /* translate everything into row group coord system*/
  if (eOrigin_TableRowGroup != mOrigin) {
    TranslateContext(mRowGroup.mRect.x, mRowGroup.mRect.y);
  }
  nsRect rgRect = mRowGroup.mRect;
  mRowGroup.mRect.MoveTo(0, 0);

  /* paint */
  for (nsTableRowFrame* row = firstRow; row; row = row->GetNextRow()) {
    nsRect rect = row->GetRect();
    if (mDirtyRect.YMost() >= rect.y) { //Intersect wouldn't handle rowspans
      nsresult rv = PaintRow(row, aPassThrough || row->HasView());
      if (NS_FAILED(rv)) return rv;
    }
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
                                 PRBool           aPassThrough)
{
  NS_PRECONDITION(aFrame, "null frame");

  /* Load row data */
  if (!aPassThrough) {
    mRow.SetFull(mPresContext, mRenderingContext, aFrame);
    if (mIsBorderCollapse && mRow.ShouldSetBCBorder()) {
      nsMargin border;
      nsTableRowFrame* nextRow = aFrame->GetNextRow();
      if (nextRow) { //outer top below us is inner bottom for us
        border.bottom = nextRow->GetOuterTopContBCBorderWidth(mP2t);
      }
      else { //acquire rg's bottom border
        nsTableRowGroupFrame* rowGroup = NS_STATIC_CAST(nsTableRowGroupFrame*, aFrame->GetParent());
        rowGroup->GetContinuousBCBorderWidth(mP2t, border);
      }
      //get the rest of the borders; will overwrite all but bottom
      aFrame->GetContinuousBCBorderWidth(mP2t, border);

      nsresult res = mRow.SetBCBorder(border, this);
      if (!NS_SUCCEEDED(res)) {
        return res;
      }
    }
    aPassThrough = !mRow.IsVisible();
  }
  else {
    mRow.SetFrame(aFrame);
  }

  /* Translate */
  if (eOrigin_TableRow == mOrigin) {
    /* If we originate from the row, then make the row the origin. */
    mRow.mRect.MoveTo(0, 0);
  }
  //else: Use row group's coord system -> no translation necessary

  for (nsTableCellFrame* cell = aFrame->GetFirstCell(); cell; cell = cell->GetNextCell()) {
    mCellRect = cell->GetRect();
    //Translate to use the same coord system as mRow.
    mCellRect.MoveBy(mRow.mRect.x, mRow.mRect.y);
    if (mCellRect.Intersects(mDirtyRect)) {
      nsresult rv = PaintCell(cell, aPassThrough || cell->HasView());
      if (NS_FAILED(rv)) return rv;
    }
  }

  /* Unload row data */
  mRow.Clear();
  return NS_OK;
}

nsresult
TableBackgroundPainter::PaintCell(nsTableCellFrame* aCell,
                                  PRBool aPassSelf)
{
  NS_PRECONDITION(aCell, "null frame");

  const nsStyleTableBorder* cellTableStyle;
  cellTableStyle = aCell->GetStyleTableBorder();
  if (!(NS_STYLE_TABLE_EMPTY_CELLS_SHOW == cellTableStyle->mEmptyCells ||
        NS_STYLE_TABLE_EMPTY_CELLS_SHOW_BACKGROUND == cellTableStyle->mEmptyCells)
      && aCell->GetContentEmpty()) {
    return NS_OK;
  }

  PRInt32 colIndex;
  aCell->GetColIndex(colIndex);

  //Paint column group background
  if (mCols && mCols[colIndex].mColGroup && mCols[colIndex].mColGroup->IsVisible()) {
    nsCSSRendering::PaintBackgroundWithSC(mPresContext, mRenderingContext,
                                          mCols[colIndex].mColGroup->mFrame, mDirtyRect,
                                          mCols[colIndex].mColGroup->mRect,
                                          *mCols[colIndex].mColGroup->mBackground,
                                          *mCols[colIndex].mColGroup->mBorder,
                                          mZeroPadding, PR_TRUE, &mCellRect);
  }

  //Paint column background
  if (mCols && mCols[colIndex].mCol.IsVisible()) {
    nsCSSRendering::PaintBackgroundWithSC(mPresContext, mRenderingContext,
                                          mCols[colIndex].mCol.mFrame, mDirtyRect,
                                          mCols[colIndex].mCol.mRect,
                                          *mCols[colIndex].mCol.mBackground,
                                          *mCols[colIndex].mCol.mBorder,
                                          mZeroPadding, PR_TRUE, &mCellRect);
  }

  //Paint row group background
  if (mRowGroup.IsVisible()) {
    nsCSSRendering::PaintBackgroundWithSC(mPresContext, mRenderingContext,
                                          mRowGroup.mFrame, mDirtyRect, mRowGroup.mRect,
                                          *mRowGroup.mBackground, *mRowGroup.mBorder,
                                          mZeroPadding, PR_TRUE, &mCellRect);
  }

  //Paint row background
  if (mRow.IsVisible()) {
    nsCSSRendering::PaintBackgroundWithSC(mPresContext, mRenderingContext,
                                          mRow.mFrame, mDirtyRect, mRow.mRect,
                                          *mRow.mBackground, *mRow.mBorder,
                                          mZeroPadding, PR_TRUE, &mCellRect);
  }

  //Paint cell background in border-collapse unless we're just passing
  if (mIsBorderCollapse && !aPassSelf) {
    mRenderingContext.PushState();
    mRenderingContext.Translate(mCellRect.x, mCellRect.y);
    mDirtyRect.MoveBy(-mCellRect.x, -mCellRect.y);
    aCell->Paint(mPresContext, mRenderingContext, mDirtyRect,
                 NS_FRAME_PAINT_LAYER_BACKGROUND,
                 NS_PAINT_FLAG_TABLE_BG_PAINT | NS_PAINT_FLAG_TABLE_CELL_BG_PASS);
    mDirtyRect.MoveBy(mCellRect.x, mCellRect.y);
    mRenderingContext.PopState();
  }

  return NS_OK;
}
