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

class CellData;

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
  PRInt32 *mCells;

  /** the number of rows */
  int mRowCount;      // in java, we could just do fCellMap.length;

  /** the number of columns (the max of all row lengths) */
  int mColCount;      // in java, we could just do fCellMap[i].length

public:
  nsCellMap(int aRows, int aColumns);

  // NOT VIRTUAL BECAUSE THIS CLASS SHOULD **NEVER** BE SUBCLASSED  
  ~nsCellMap();

  /** initialize the CellMap to (aRows x aColumns) */
  void Reset(int aRows, int aColumns);

  /** return the CellData for the cell at (aRow,aColumn) */
  CellData * GetCellAt(int aRow, int aColumn) const;

  /** assign aCellData to the cell at (aRow,aColumn) */
  void SetCellAt(CellData *aCellData, int aRow, int aColumn);

  /** expand the CellMap to have aColCount columns.  The number of rows remains the same */
  void GrowTo(int aColCount);

  /** return the total number of columns in the table represented by this CellMap */
  int GetColCount() const;

  /** return the total number of rows in the table represented by this CellMap */
  int GetRowCount() const;

  /** for debugging, print out this CellMap */
  void DumpCellMap() const;

};

inline CellData * nsCellMap::GetCellAt(int aRow, int aColumn) const
{
  int index = (aRow*mColCount)+aColumn;
  return (CellData *)mCells[index];
}

inline int nsCellMap::GetColCount() const
{ 
  return mColCount; 
}

inline int nsCellMap::GetRowCount() const
{ 
  return mRowCount; 
}

#endif
