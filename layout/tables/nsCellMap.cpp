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

#include "nsVoidArray.h"
#include "nsCellMap.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
#endif

nsCellMap::nsCellMap(int aRowCount, int aColCount)
  : mRowCount(0),
    mColCount(0),
    mTotalRowCount(0),
    mNumCollapsedRows(0),
    mNumCollapsedCols(0)
{
  mRows = nsnull;
  mColFrames = nsnull;
  mMinColSpans = nsnull;
  mIsCollapsedRows = nsnull;
  mIsCollapsedCols = nsnull;
  Reset(aRowCount, aColCount);
}

nsCellMap::~nsCellMap()
{
  if (nsnull!=mRows)
  {
    for (int i=mTotalRowCount-1; 0<=i; i--)
    {
      nsVoidArray *row = (nsVoidArray *)(mRows->ElementAt(i));
      for (int j=mColCount-1; 0<=j ;j--)
      {
        CellData* data = (CellData *)(row->ElementAt(j));
        if (data != nsnull)
        {
          delete data;
        }
      } 
      delete row;
    }
    delete mRows;
  }
  if (nsnull != mColFrames)
    delete mColFrames;
  // mMinColSpans may be null, it is only set for tables that need it
  if (nsnull != mMinColSpans)
    delete [] mMinColSpans;
  if (nsnull != mIsCollapsedRows) {
    delete [] mIsCollapsedRows;
    mIsCollapsedRows = nsnull;
    mNumCollapsedRows = 0;
  }
  if (nsnull != mIsCollapsedCols) {
    delete [] mIsCollapsedCols;
    mIsCollapsedCols = nsnull;
    mNumCollapsedCols = 0;
  }
  mRows = nsnull;
  mColFrames = nsnull;
  mMinColSpans = nsnull;
};


void nsCellMap::Reset(int aRowCount, int aColCount)
{
  if (gsDebug) printf("calling Reset(%d,%d) with mRC=%d, mCC=%d, mTRC=%d\n",
                      aRowCount, aColCount, mRowCount, mColCount, mTotalRowCount);
  if (nsnull==mColFrames)
  {
    mColFrames = new nsVoidArray(); // don't give the array a count, because null col frames are illegal (unlike null cell entries in a row)
  }

  if (nsnull==mRows)
  {
    mRows = new nsVoidArray();  // don't give the array a count, because null rows are illegal (unlike null cell entries in a row)
  }

  // void arrays force the caller to handle null padding elements themselves
  // so if the number of columns has increased, we need to add extra cols to each row
  PRInt32 newCols = mColCount-aColCount;
  for (PRInt32 rowIndex=0; rowIndex<mRowCount; rowIndex++)
  {
    nsVoidArray *row = (nsVoidArray *)(mRows->ElementAt(rowIndex));
    const PRInt32 colsInRow = row->Count();
    if (colsInRow == aColCount)
      break;  // we already have enough columns in each row
    for (PRInt32 colIndex = colsInRow; colIndex<aColCount; colIndex++)
      row->AppendElement(nsnull);
  }

  // if the number of rows has increased, add the extra rows
  PRInt32 newRows = aRowCount-mTotalRowCount; // (new row count) - (total row allocation)
  for ( ; newRows > 0; newRows--) {
    nsVoidArray *row;
    if (0 != aColCount) {
      row = new nsVoidArray(aColCount);
    } else {
      row = new nsVoidArray();
    }
    mRows->AppendElement(row);
  }

  mRowCount = aRowCount;
  mTotalRowCount = PR_MAX(mTotalRowCount, mRowCount);
  mColCount = aColCount;

  if (gsDebug) printf("leaving Reset with mRC=%d, mCC=%d, mTRC=%d\n",
                      mRowCount, mColCount, mTotalRowCount);
}

void nsCellMap::DumpCellMap() const
{
  if (gsDebug==PR_TRUE)
  {
    printf("Cell Map =\n");
    for (int rowIndex=0; rowIndex<mRowCount ; rowIndex++)
    {
      nsVoidArray *row = (nsVoidArray *)(mRows->ElementAt(rowIndex)); 
      for (int colIndex=0; colIndex<mColCount; colIndex++)
      {
        CellData* data = (CellData *)(row->ElementAt(colIndex));
        printf("Cell [%d,%d] = %p  for index = %d\n", rowIndex, colIndex, data);
      }
    }
  }
}

void nsCellMap::GrowToRow(PRInt32 aRowCount)
{
  Reset(aRowCount, mColCount);
}

void nsCellMap::GrowToCol(PRInt32 aColCount)
{
  Reset(mRowCount, aColCount);
}

void nsCellMap::SetCellAt(CellData *aCell, int aRowIndex, int aColIndex)
{
  NS_PRECONDITION(nsnull!=aCell, "bad aCell");
  PRInt32 newRows = (aRowIndex+1)-mTotalRowCount;    // add 1 to the "index" to get a "count"
  if (0<newRows)
  {
    for ( ; newRows>0; newRows--)
    {
      nsVoidArray *row = new nsVoidArray(mColCount);
      mRows->AppendElement(row);
    }
    mTotalRowCount = aRowIndex+1; // remember to always add 1 to an index when you want a count
  }

  CellData* cell = GetCellAt(aRowIndex,aColIndex);
  if (cell != nsnull)
    delete cell;
  nsVoidArray *row = (nsVoidArray *)(mRows->ElementAt(aRowIndex));
  row->ReplaceElementAt(aCell, aColIndex);
  if (gsDebug) printf("leaving SetCellAt(%p,%d,%d) with mRC=%d, mCC=%d, mTRC=%d\n",
                      aCell, aRowIndex, aColIndex, mRowCount, mColCount, mTotalRowCount);
}

nsTableColFrame* nsCellMap::GetColumnFrame(PRInt32 aColIndex) const
{
  NS_ASSERTION(nsnull!=mColFrames, "bad state");
  return (nsTableColFrame *)(mColFrames->ElementAt(aColIndex));
}


void nsCellMap::SetMinColSpan(PRInt32 aColIndex, PRBool aColSpan)
{
  NS_ASSERTION(aColIndex<mColCount, "bad aColIndex");
  NS_ASSERTION(aColSpan>=1, "bad aColSpan");

  // initialize the data structure if not already done
  if (nsnull==mMinColSpans)
  {
    mMinColSpans = new PRInt32[mColCount];
    for (PRInt32 i=0; i<mColCount; i++)
      mMinColSpans[i]=1;
  }

  mMinColSpans[aColIndex] = aColSpan;
}

PRInt32 nsCellMap::GetMinColSpan(PRInt32 aColIndex) const
{
  NS_ASSERTION(aColIndex<mColCount, "bad aColIndex");

  PRInt32 result = 1; // default is 1, mMinColSpans need not be allocated for tables with no spans
  if (nsnull!=mMinColSpans)
    result = mMinColSpans[aColIndex];
  return result;
}

/** return the index of the next column in aRowIndex that does not have a cell assigned to it */
PRInt32 nsCellMap::GetNextAvailColIndex(PRInt32 aRowIndex, PRInt32 aColIndex) const
{
  PRInt32 result = 0;
  if (aColIndex > mColCount)
  {
    result = aColIndex;
  }
  else
  {
    if (aRowIndex < mRowCount)
    {
      result = aColIndex;
      nsVoidArray *row = (nsVoidArray *)(mRows->ElementAt(aRowIndex));
      PRInt32 count = row->Count();
      for (PRInt32 colIndex=aColIndex; colIndex<count; colIndex++)
      {
        void * data = row->ElementAt(colIndex);
        if (nsnull==data)
        {
          result = colIndex;
          break;
        }
        result++;
      }
    }
  }
  return result;
}

/** returns PR_TRUE if the row at aRowIndex has any cells that are the result
  * of a row-spanning cell above it.  So, given this table:<BR>
  * <PRE>
  * TABLE
  *   TR
  *    TD ROWSPAN=2
  *    TD
  *   TR
  *    TD
  * </PRE>
  * RowIsSpannedInto(0) returns PR_FALSE, and RowIsSpannedInto(1) returns PR_TRUE.
  * @see RowHasSpanningCells
  */
// if computing this and related info gets expensive, we can easily 
// cache it.  The only thing to remember is to rebuild the cache 
// whenever a row|col|cell is added/deleted, or a span attribute is changed.
PRBool nsCellMap::RowIsSpannedInto(PRInt32 aRowIndex)
{
  NS_PRECONDITION (0<=aRowIndex && aRowIndex<GetRowCount(), "bad row index arg");
  PRBool result = PR_FALSE;
	PRInt32 colCount = GetColCount();
	for (PRInt32 colIndex=0; colIndex<colCount; colIndex++)
	{
		CellData *cd = GetCellAt(aRowIndex, colIndex);
		if (cd != nsnull)
		{ // there's really a cell at (aRowIndex, colIndex)
			if (nsnull==cd->mCell)
			{ // the cell at (aRowIndex, colIndex) is the result of a span
				nsTableCellFrame *cell = cd->mRealCell->mCell;
				NS_ASSERTION(nsnull!=cell, "bad cell map state, missing real cell");
				PRInt32 realRowIndex;
        cell->GetRowIndex (realRowIndex);
				if (realRowIndex!=aRowIndex)
				{ // the span is caused by a rowspan
					result = PR_TRUE;
					break;
				}
			}
		}
  }
  return result;
}

/** returns PR_TRUE if the row at aRowIndex has any cells that have a rowspan>1
  * So, given this table:<BR>
  * <PRE>
  * TABLE
  *   TR
  *    TD ROWSPAN=2
  *    TD
  *   TR
  *    TD
  * </PRE>
  * RowHasSpanningCells(0) returns PR_TRUE, and RowHasSpanningCells(1) returns PR_FALSE.
  * @see RowIsSpannedInto
  */
PRBool nsCellMap::RowHasSpanningCells(PRInt32 aRowIndex)
{
  NS_PRECONDITION (0<=aRowIndex && aRowIndex<GetRowCount(), "bad row index arg");
  PRBool result = PR_FALSE;
  const PRInt32 rowCount = GetRowCount();
  if (aRowIndex!=rowCount-1)
  { // aRowIndex is not the last row, so we check the next row after aRowIndex for spanners
    const PRInt32 colCount = GetColCount();
    for (PRInt32 colIndex=0; colIndex<colCount; colIndex++)
    {
      PRInt32 nextRowIndex = aRowIndex+1;
      CellData *cd =GetCellAt(nextRowIndex, colIndex);
      if (cd != nsnull)
      { // there's really a cell at (nextRowIndex, colIndex)
        if (nsnull==cd->mCell)
        { // the cell at (nextRowIndex, colIndex) is the result of a span
          nsTableCellFrame *cell = cd->mRealCell->mCell;
          NS_ASSERTION(nsnull!=cell, "bad cell map state, missing real cell");
          PRInt32 realRowIndex;
          cell->GetRowIndex (realRowIndex);
          if (realRowIndex!=nextRowIndex)
          { // the span is caused by a rowspan
            CellData *spanningCell = GetCellAt(aRowIndex, colIndex);
            if (nsnull!=spanningCell)
            { // there's really a cell at (aRowIndex, colIndex)
              if (nsnull!=spanningCell->mCell)
              { // aRowIndex is where the rowspan originated
                result = PR_TRUE;
                break;
              }
            }
          }
        }
      }
    }
  }
  return result;
}


/** returns PR_TRUE if the col at aColIndex has any cells that are the result
  * of a col-spanning cell.  So, given this table:<BR>
  * <PRE>
  * TABLE
  *   TR
  *    TD COLSPAN=2
  *    TD
  *    TD
  * </PRE>
  * ColIsSpannedInto(0) returns PR_FALSE, ColIsSpannedInto(1) returns PR_TRUE,
  * and ColIsSpannedInto(2) returns PR_FALSE.
  * @see ColHasSpanningCells
  */
PRBool nsCellMap::ColIsSpannedInto(PRInt32 aColIndex)
{
  PRInt32 colCount = GetColCount();
  NS_PRECONDITION (0<=aColIndex && aColIndex<colCount, "bad col index arg");
  PRBool result = PR_FALSE;
  PRInt32 rowCount = GetRowCount();
  for (PRInt32 rowIndex=0; rowIndex<rowCount; rowIndex++)
  {
    CellData *cd =GetCellAt(rowIndex, aColIndex);
    if (cd != nsnull)
    { // there's really a cell at (aRowIndex, aColIndex)
      if (nsnull==cd->mCell)
      { // the cell at (rowIndex, aColIndex) is the result of a span
        nsTableCellFrame *cell = cd->mRealCell->mCell;
        NS_ASSERTION(nsnull!=cell, "bad cell map state, missing real cell");
        PRInt32 realColIndex;
        cell->GetColIndex (realColIndex);
        if (realColIndex!=aColIndex)
        { // the span is caused by a colspan
          result = PR_TRUE;
          break;
        }
      }
    }
  }
  return result;
}

/** returns PR_TRUE if the row at aColIndex has any cells that have a colspan>1
  * So, given this table:<BR>
  * <PRE>
  * TABLE
  *   TR
  *    TD COLSPAN=2
  *    TD
  * </PRE>
  * ColHasSpanningCells(0) returns PR_TRUE, and ColHasSpanningCells(1) returns PR_FALSE.
  * @see ColIsSpannedInto
  */
PRBool nsCellMap::ColHasSpanningCells(PRInt32 aColIndex)
{
  PRBool result = PR_FALSE;
  const PRInt32 colCount = GetColCount();
  if (aColIndex<colCount-1)
  { // aColIndex is not the last col, so we check the next col after aColIndex for spanners
    const PRInt32 rowCount = GetRowCount();
    for (PRInt32 rowIndex=0; rowIndex<rowCount; rowIndex++)
    {
      PRInt32 nextColIndex = aColIndex+1;
      CellData *cd =GetCellAt(rowIndex, nextColIndex);
      if (cd != nsnull)
      { // there's really a cell at (rowIndex, nextColIndex)
        if (nsnull==cd->mCell)
        { // the cell at (rowIndex, nextColIndex) is the result of a span
          nsTableCellFrame *cell = cd->mRealCell->mCell;
          NS_ASSERTION(nsnull!=cell, "bad cell map state, missing real cell");
          PRInt32 realColIndex;
          cell->GetColIndex (realColIndex);
          if (realColIndex!=nextColIndex)
          { // the span is caused by a colspan
            CellData *spanningCell =GetCellAt(rowIndex, aColIndex);
            if (nsnull!=spanningCell)
            { // there's really a cell at (rowIndex, aColIndex)
              if (nsnull!=spanningCell->mCell)
              { // aCowIndex is where the cowspan originated
                result = PR_TRUE;
                break;
              }
            }
          }
        }
      }
    }
  }
  return result;
}

PRInt32 nsCellMap::GetNumCollapsedRows()
{
  return mNumCollapsedRows;
}

PRBool nsCellMap::IsRowCollapsedAt(PRInt32 aRow)
{
  if ((aRow >= 0) && (aRow < mTotalRowCount)) {
    if (mIsCollapsedRows) {
      return mIsCollapsedRows[aRow];
    }
  }
  return PR_FALSE;
}

void nsCellMap::SetRowCollapsedAt(PRInt32 aRow, PRBool aValue)
{
  if ((aRow >= 0) && (aRow < mRowCount)) {
    if (nsnull == mIsCollapsedRows) {
      mIsCollapsedRows = new PRBool[mRowCount];
      for (PRInt32 i = 0; i < mRowCount; i++) {
        mIsCollapsedRows[i] = PR_FALSE;
      }
    }
    if (mIsCollapsedRows[aRow] != aValue) {
      if (PR_TRUE == aValue) {
        mNumCollapsedRows++;
      } else {
        mNumCollapsedRows--;
      }
      mIsCollapsedRows[aRow] = aValue; 
    }
  }
}

PRInt32 nsCellMap::GetNumCollapsedCols()
{
  return mNumCollapsedCols;
}

PRBool nsCellMap::IsColCollapsedAt(PRInt32 aCol)
{
  if ((aCol >= 0) && (aCol < mColCount)) {
    if (mIsCollapsedCols) {
      return mIsCollapsedCols[aCol];
    }
  }
  return PR_FALSE;
}

void nsCellMap::SetColCollapsedAt(PRInt32 aCol, PRBool aValue)
{
  if ((aCol >= 0) && (aCol < mColCount)) {
    if (nsnull == mIsCollapsedCols) {
      mIsCollapsedCols = new PRBool[mColCount];
      for (PRInt32 i = 0; i < mColCount; i++) {
        mIsCollapsedCols[i] = PR_FALSE;
      }
    }
    if (mIsCollapsedCols[aCol] != aValue) {
      if (PR_TRUE == aValue) {
        mNumCollapsedCols++;
      } else {
        mNumCollapsedCols--;
      }
      mIsCollapsedCols[aCol] = aValue; 
    }
  }
}

