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

class nsVoidArray;
class nsTableColFrame;
class nsTableCellFrame;

/** nsCellMap is a support class for nsTablePart.  
  * It maintains an Rows x Columns grid onto which the cells of the table are mapped.
  * This makes processing of rowspan and colspan attributes much easier.
  * Each cell is represented by a CellData object.
  *
  * @see CellData
  * @see nsTableFrame::BuildCellMap
  * @see nsTableFrame::GrowCellMap
  * @see nsTableFrame::BuildCellIntoMap
  *
  * acts like a 2-dimensional array, so all offsets are 0-indexed
  */
class nsCellMap
{
protected:
  /** storage for CellData pointers */
  PRInt32 *mCells;       ///XXX CellData *?

  /** a cache of the column frames, by col index */
  nsVoidArray * mColFrames;

  /** the number of rows */
  PRInt32 mRowCount;      // in java, we could just do fCellMap.length;

  /** the number of columns (the max of all row lengths) */
  PRInt32 mColCount;

public:
  nsCellMap(PRInt32 aRows, PRInt32 aColumns);

  // NOT VIRTUAL BECAUSE THIS CLASS SHOULD **NEVER** BE SUBCLASSED  
  ~nsCellMap();

  /** initialize the CellMap to (aRows x aColumns) */
  void Reset(PRInt32 aRows, PRInt32 aColumns);

  /** return the CellData for the cell at (aRowIndex,aColIndex) */
  CellData * GetCellAt(PRInt32 aRowIndex, PRInt32 aColIndex) const;

  /** return the nsTableCellFrame for the cell at (aRowIndex, aColIndex) */
  nsTableCellFrame * GetCellFrameAt(PRInt32 aRowIndex, PRInt32 aColIndex) const;

  /** assign aCellData to the cell at (aRow,aColumn) */
  void SetCellAt(CellData *aCellData, PRInt32 aRow, PRInt32 aColumn);

  /** expand the CellMap to have aColCount columns.  The number of rows remains the same */
  void GrowTo(PRInt32 aColCount);

  /** return the total number of columns in the table represented by this CellMap */
  PRInt32 GetColCount() const;

  /** return the total number of rows in the table represented by this CellMap */
  PRInt32 GetRowCount() const;

  nsTableColFrame * GetColumnFrame(PRInt32 aColIndex);

  void AppendColumnFrame(nsTableColFrame *aColFrame);

  PRBool RowImpactedBySpanningCell(PRInt32 aRowIndex);

  PRBool ColumnImpactedBySpanningCell(PRInt32 aColIndex);

  /** for debugging */
  void DumpCellMap() const;

};

/* ----- inline methods ----- */

inline CellData * nsCellMap::GetCellAt(PRInt32 aRowIndex, PRInt32 aColIndex) const
{
  NS_PRECONDITION(0<=aRowIndex && aRowIndex < mRowCount, "bad aRowIndex arg");
  NS_PRECONDITION(0<=aColIndex && aColIndex < mColCount, "bad aColIndex arg");

  PRInt32 index = (aRowIndex*mColCount)+aColIndex;
  return (CellData *)mCells[index];
}

inline nsTableCellFrame * nsCellMap::GetCellFrameAt(PRInt32 aRowIndex, PRInt32 aColIndex) const
{
  NS_PRECONDITION(0<=aRowIndex && aRowIndex < mRowCount, "bad aRowIndex arg");
  NS_PRECONDITION(0<=aColIndex && aColIndex < mColCount, "bad aColIndex arg");

  nsTableCellFrame *result = nsnull;
  CellData * cellData = GetCellAt(aRowIndex, aColIndex);
  if (nsnull!=cellData)
    result = cellData->mCell;
  return result;
}

inline PRInt32 nsCellMap::GetColCount() const
{ 
  return mColCount; 
}

inline PRInt32 nsCellMap::GetRowCount() const
{ 
  return mRowCount; 
}

inline void nsCellMap::AppendColumnFrame(nsTableColFrame *aColFrame)
{
  mColFrames->AppendElement(aColFrame);
  // sanity check
  NS_ASSERTION(mColFrames->Count()<=mColCount, "too many columns appended to CellMap");
}


#endif
