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

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
#endif

nsCellMap::nsCellMap(int aRowCount, int aColCount)
  : mRowCount(0),
    mColCount(0),
    mTotalRowCount(0)
{
  mRows = nsnull;
  mColFrames = nsnull;
  mMinColSpans = nsnull;
  Reset(aRowCount, aColCount);
}

nsCellMap::~nsCellMap()
{
  if (nsnull!=mRows)
  {
    for (int i=mTotalRowCount-1; 0<i; i--)
    {
      nsVoidArray *row = (nsVoidArray *)(mRows->ElementAt(i));
      for (int j=mColCount; 0<j ;j--)
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
  if (nsnull != mMinColSpans)
    delete [] mMinColSpans;
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

  if (nsnull==mRowCount)
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
  for ( ; newRows>0; newRows--)
  {
    nsVoidArray *row;
    if (0!=aColCount)
      row = new nsVoidArray(aColCount);
    else
      row = new nsVoidArray();
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
  PRInt32 newRows = (aRowIndex+1)-mRowCount;    // add 1 to the "index" to get a "count"
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


