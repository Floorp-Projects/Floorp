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
#include "nsVoidArray.h"
#include "nsCellMap.h"
#include "nsTableFrame.h"

#ifdef NS_DEBUG
static PRBool gsDebug1 = PR_FALSE;
#else
static const PRBool gsDebug1 = PR_FALSE;
#endif

static const PRInt32 gBytesPerPointer = sizeof(PRInt32);

nsCellMap::nsCellMap(int aRows, int aColumns)
  : mRowCount(aRows),
    mColCount(aColumns)
{
  mCells = nsnull;
  mColFrames = nsnull;
  mMinColSpans = nsnull;
  Reset(aRows, aColumns);
}

nsCellMap::~nsCellMap()
{
  if (nsnull!=mCells)
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
  }
  if (nsnull != mColFrames)
    delete mColFrames;
  if (nsnull != mMinColSpans)
    delete [] mMinColSpans;
  mCells = nsnull;
  mColFrames = nsnull;
  mMinColSpans = nsnull;
};


void nsCellMap::Reset(int aRows, int aColumns)
{
  if (nsnull==mColFrames)
  {
    mColFrames = new nsVoidArray();
  }

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

void nsCellMap::SetCellAt(CellData *aCell, int aRow, int aColumn)
{
  //Assert aRow, aColumn
  int index = (aRow*mColCount)+aColumn;
  CellData* cell = GetCellAt(aRow,aColumn);
  if (cell != nsnull)
    delete cell;
  mCells[index] = (PRInt32)aCell;
}

nsTableColFrame* nsCellMap::GetColumnFrame(PRInt32 aColIndex)
{
  return (nsTableColFrame *)(mColFrames->ElementAt(aColIndex));
}


void nsCellMap::SetMinColSpan(PRInt32 aColIndex, PRBool aColSpan)
{
  NS_ASSERTION(aColIndex<mColCount, "bad aColIndex");
  NS_ASSERTION(aColSpan>=0, "bad aColSpan");

  // initialize the data structure if not already done
  if (nsnull==mMinColSpans)
  {
    mMinColSpans = new PRInt32[mColCount];
    for (PRInt32 i=0; i<mColCount; i++)
      mMinColSpans[i]=1;
  }

  mMinColSpans[aColIndex] = aColSpan;
}

PRInt32 nsCellMap::GetMinColSpan(PRInt32 aColIndex)
{
  NS_ASSERTION(aColIndex<mColCount, "bad aColIndex");

  PRInt32 result = 1; // default is 1, mMinColSpans need not be allocated for tables with no spans
  if (nsnull!=mMinColSpans)
    result = mMinColSpans[aColIndex];
  return result;
}




