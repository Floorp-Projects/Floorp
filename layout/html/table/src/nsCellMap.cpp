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
#include "nsCRT.h"
#include "nsCellMap.h"
#include "nsTablePart.h"
#include <stdio.h>/* XXX */ // for printf

#ifdef NS_DEBUG
static PRBool gsDebug1 = PR_FALSE;
#else
static const PRBool gsDebug1 = PR_FALSE;
#endif

static PRInt32 gBytesPerPointer = sizeof(PRInt32);

nsCellMap::nsCellMap(int aRows, int aColumns)
  : mRowCount(aRows),
    mColCount(aColumns)
{
  mCells = nsnull;
  Reset(aRows, aColumns);
}

nsCellMap::~nsCellMap()
{
  for (int i=0;i<mRowCount;i++)
  {
    for (int j=0;j<mColCount;j++)
    {
      int index = (i*mColCount)+j;
      CellData* data = (CellData*)mCells[index];
      if (data != nsnull)
      {
        delete data;
        mCells[index] = 0;
      }
    } 
  }
  delete [] mCells;
  mCells = nsnull;
};


void nsCellMap::Reset(int aRows, int aColumns)
{
  // needs to be more efficient, to reuse space if possible
  if (nsnull!=mCells)
  {
    delete [] mCells;
    mCells = nsnull;
  }
  mRowCount = aRows;
  mColCount = aColumns;
  mCells = new PRInt32 [mRowCount*mColCount*gBytesPerPointer];
  nsCRT::memset (mCells, 0, (mRowCount*mColCount)*gBytesPerPointer);
}

void nsCellMap::GrowTo(int aColCount)
{
  if (aColCount <= mColCount)
    return;
  PRInt32 * newCells = new PRInt32 [mRowCount*aColCount*gBytesPerPointer];
  for (int rowIndex = 0; rowIndex < mRowCount; rowIndex++)
  {
    PRInt32* rowMap = (PRInt32*)&(newCells[rowIndex*aColCount]);
    nsCRT::memset (rowMap, 0, aColCount*gBytesPerPointer);
    nsCRT::memcpy(rowMap, &(mCells[rowIndex*mColCount]), mColCount*gBytesPerPointer);
  }
  if (mCells != nsnull)
    delete [] mCells;
  mCells = newCells;
  mColCount = aColCount;
}

CellData * nsCellMap::GetCellAt(int aRow, int aColumn) const
{
  int index = (aRow*mColCount)+aColumn;
  if (gsDebug1==PR_TRUE) 
  {
    printf("GetCellAt [%d, %d] returning %d\n", aRow, aColumn, mCells[index]);
  }
  return (CellData *)mCells[index];
}

void nsCellMap::SetCellAt(CellData *aCell, int aRow, int aColumn)
{
  //Assert aRow, aColumn
  int index = (aRow*mColCount)+aColumn;
  CellData* cell = GetCellAt(aRow,aColumn);
  if (cell != nsnull)
    delete cell;
  mCells[index] = (PRInt32)aCell;
  if (gsDebug1==PR_TRUE) 
  {
    printf("SetCellAt [%d, %d] setting %d\n", aRow, aColumn, aCell);
  }
}

int nsCellMap::GetColCount() const
{ 
  return mColCount; 
}

int nsCellMap::GetRowCount() const
{ 
  return mRowCount; 
}

void nsCellMap::DumpCellMap() const
{
  if (gsDebug1==PR_TRUE)
  {
    printf("Cell Map =\n");
    for (int i=0;i<mRowCount;i++)
      for (int j=0;j<mColCount;j++)
      {
        int index = (i*mColCount)+j;
        printf("Cell [%d,%d] = %d  for index = %d\n", i, j, mCells[index], index);
      }
  }
}
