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
#ifndef nsCellMap_h__
#define nsCellMap_h__

#include "nscore.h"
#include "celldata.h"
#include "nsVoidArray.h"
class nsTableColFrame;
class nsTableCellFrame;

/** nsCellMap is a support class for nsTablePart.  
  * It maintains an Rows x Columns grid onto which the cells of the table are mapped.
  * This makes processing of rowspan and colspan attributes much easier.
  * Each cell is represented by a CellData object.
  *
  * @see CellData
  * @see nsTableFrame::AddCellToMap
  * @see nsTableFrame::GrowCellMap
  * @see nsTableFrame::BuildCellIntoMap
  *
  * mRows is an array of rows.  a row cannot be null.
  * each row is an array of cells.  a cell can be null.
  */
class nsCellMap
{
public:
  /** constructor 
    * @param aNumRows - initial number of rows
	  * @param aNumColumns - initial number of columns
	  */
  nsCellMap(PRInt32 aNumRows, 
            PRInt32 aNumCols);

  /** destructor
    * NOT VIRTUAL BECAUSE THIS CLASS SHOULD **NEVER** BE SUBCLASSED  
	  */
  ~nsCellMap();

  /** return the CellData for the cell at (aTableRowIndex, aTableColIndex) */
  CellData* GetCellAt(PRInt32 aRowIndex, 
                      PRInt32 aColIndex) const;

  /** append the cellFrame at the end of the row at aRowIndex and return the col index
    */
  PRInt32 AppendCell(nsTableCellFrame* aCellFrame, 
                     PRInt32           aRowIndex);

  void RemoveCell(nsTableCellFrame* aCellFrame,
                  PRInt32           aRowIndex);

  PRInt32 GetNextAvailRowIndex();

  PRInt32 GetEffectiveColSpan(nsTableCellFrame* aCell) const;
  PRInt32 GetEffectiveColSpan(PRInt32                 aColIndex, 
                              const nsTableCellFrame* aCell) const;
  PRInt32 GetNumCellsIn(PRInt32 aColIndex) const;
  PRInt32 GetNumCellsOriginatingIn(PRInt32 aColIndex) const;

  PRInt32 GetNumCollapsedRows() const;
  PRBool IsRowCollapsedAt(PRInt32 aRowIndex) const;
  void SetRowCollapsedAt(PRInt32 aRowIndex, 
                         PRBool  aValue);

  PRInt32 GetNumCollapsedCols() const;
  PRBool IsColCollapsedAt(PRInt32 aColIndex) const;
  void SetColCollapsedAt(PRInt32 aColIndex, 
                         PRBool aValue);

  /** return the total number of columns in the table represented by this CellMap */
  PRInt32 GetColCount() const;

  /** return the actual number of rows in the table represented by this CellMap */
  PRInt32 GetRowCount() const;

  /** return the column frame associated with aColIndex */
  nsTableColFrame* GetColumnFrame(PRInt32 aColIndex) const;

  /** add a column frame to the list of column frames
    * column frames must be added in order
		*/
  void AppendColumnFrame(nsTableColFrame *aColFrame);

  /** empty the column frame cache */
  void ClearColumnCache();

  // temporary until nsTableFrame::GetCellData uses GetCellFrameAt
  nsTableCellFrame* GetCellFrameOriginatingAt(PRInt32 aRowX, 
                                              PRInt32 aColX) const;

  nsTableCellFrame* GetCellInfoAt(PRInt32  aRowX, 
                                  PRInt32  aColX, 
                                  PRBool*  aOriginates = nsnull, 
                                  PRInt32* aColSpan = nsnull) const;

  void AddColsAtEnd(PRUint32 aNumCols);

  PRBool RowIsSpannedInto(PRInt32 aRowIndex) const;
  PRBool RowHasSpanningCells(PRInt32 aRowIndex) const;
  PRBool ColIsSpannedInto(PRInt32 aColIndex) const;
  PRBool ColHasSpanningCells(PRInt32 aColIndex) const;

  /** dump a representation of the cell map to stdout for debugging */
#ifdef NS_DEBUG
  void Dump() const;
#endif

protected:
  /** set the CellMap to (aNumRows x aNumColumns) */
  void Grow(PRInt32 aNumMapRows, 
            PRInt32 aNumCols);

  /** assign aCellData to the cell at (aRow,aColumn) */
  void SetMapCellAt(CellData& aCellData, 
                    PRInt32   aMapRowIndex, 
                    PRInt32   aColIndex);

  CellData* GetMapCellAt(PRInt32 aMapRowIndex, 
                         PRInt32 aColIndex) const;

  PRInt32 GetNumCellsIn(PRInt32 aColIndex, 
                        PRBool aOriginating) const;

  /** an array containing col array. It can be larger than mRowCount due to
    * row spans extending beyond the table */
  nsVoidArray mRows; 

  /** an array of col frames. It is as large as mRowCount */
  nsVoidArray mColFrames;

  /** an array of PRInt32[2] indexed by col and giving the number of cells originating
    * and occupying each col. */
  nsVoidArray mNumCellsInCol;

  // an array of booleans where the ith element indicates if the ith row is collapsed
  PRPackedBool* mIsCollapsedRows;
  PRInt32       mNumCollapsedRows;

  // an array of booleans where the ith element indicates if the ith col is collapsed
  PRPackedBool* mIsCollapsedCols;
  PRInt32       mNumCollapsedCols;

  /** the number of rows in the table which is <= the number of rows in the cell map
    * due to row spans extending beyond the end of the table (dead rows) */
  PRInt32 mRowCount;

  PRInt32 mNextAvailRowIndex;
};

/* ----- inline methods ----- */
inline CellData* nsCellMap::GetCellAt(PRInt32 aRowIndex, 
                                      PRInt32 aColIndex) const
{
  if ((0 > aRowIndex) || (aRowIndex >= mRowCount) || 
      (0 > aColIndex) || (aColIndex >= mNumCellsInCol.Count())) {
    //bug 9024 tickled this
    //printf("%s \n", "nsCellMap::GetCellAt called with invalid row or col index"); // XXX look at this when bug 10911 get fixed
    return nsnull;
  }
  
  CellData* result = nsnull;
  nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(aRowIndex));
  if (row) 
    result = (CellData *)(row->ElementAt(aColIndex));
  return result;
}

inline CellData* nsCellMap::GetMapCellAt(PRInt32 aMapRowIndex, 
                                         PRInt32 aColIndex) const
{
  if ((0 > aMapRowIndex) || (aMapRowIndex >= mRows.Count()) || 
      (0 > aColIndex) || (aColIndex >= mNumCellsInCol.Count())) {
    //see bug 9024 comments above 
    //printf("%s \n", "nsCellMap::GetMapCellAt called with invalid row or col index"); // XXX look at this when bug 10911 get fixed
    return nsnull;
  }

  CellData* result = nsnull;
  nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(aMapRowIndex));
  if (row)
    result = (CellData *)(row->ElementAt(aColIndex));
  return result;
}

inline PRInt32 nsCellMap::GetColCount() const
{ 
  return mNumCellsInCol.Count();
}

inline PRInt32 nsCellMap::GetRowCount() const
{ 
  return mRowCount; 
}

inline void nsCellMap::AppendColumnFrame(nsTableColFrame *aColFrame)
{
  mColFrames.AppendElement(aColFrame);
}

inline void nsCellMap::ClearColumnCache()
{
  mColFrames.Clear();
}


#endif
