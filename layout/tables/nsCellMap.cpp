/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsLayoutAtoms.h"
#include "nsVoidArray.h"
#include "nsCellMap.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"

// CellData 

MOZ_DECL_CTOR_COUNTER(CellData);

CellData::CellData()
{
  MOZ_COUNT_CTOR(CellData);
  mOrigCell    = nsnull;
  mRowSpanData = nsnull;
  mColSpanData = nsnull;
}

#ifdef NS_BUILD_REFCNT_LOGGING
CellData::CellData(nsTableCellFrame* aOrigCell, CellData* aRowSpanData,
                   CellData* aColSpanData)
  : mOrigCell(aOrigCell),
    mRowSpanData(aRowSpanData),
    mColSpanData(aColSpanData)
{
  MOZ_COUNT_CTOR(CellData);
}
#endif

CellData::~CellData()
{
  MOZ_COUNT_DTOR(CellData);
}

MOZ_DECL_CTOR_COUNTER(nsCellMap);

// nsCellMap

nsCellMap::nsCellMap(int aRowCount, int aColCount)
  : mRowCount(0),
    mNextAvailRowIndex(0)
{
  MOZ_COUNT_CTOR(nsCellMap);
  Grow(aRowCount, aColCount);
}

nsCellMap::~nsCellMap()
{
  MOZ_COUNT_DTOR(nsCellMap);

  PRInt32 mapRowCount = mRows.Count();
  PRInt32 colCount    = mCols.Count();
  PRInt32 colX;
  for (PRInt32 rowX = 0; rowX < mapRowCount; rowX++) {
    nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(rowX));
    for (colX = 0; colX < colCount; colX++) {
      CellData* data = (CellData *)(row->ElementAt(colX));
      if (data) {
        delete data;
      } 
    }
    delete row;
  }
  for (colX = 0; colX < colCount; colX++) {
    nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
    if (colInfo) {
      delete colInfo;
    }
  }
}

void nsCellMap::AddColsAtEnd(PRUint32 aNumCols)
{
  Grow(mRowCount, mCols.Count() + aNumCols);
}

PRInt32 nsCellMap::GetNextAvailRowIndex()
{
  return mNextAvailRowIndex++;
}

void nsCellMap::Grow(PRInt32 aNumMapRows, 
                     PRInt32 aNumCols)
{
  PRInt32 origNumMapRows = mRows.Count();
  PRInt32 origNumCols    = mCols.Count();

  // if the number of columns has increased, we need to add extra cols to mNumColsOriginating and
  // each row in mRows.
  if (origNumCols < aNumCols) {
    PRInt32 colX;
    for (PRInt32 rowX = 0; rowX < origNumMapRows; rowX++) {
      nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(rowX));
      const PRInt32 colsInRow = row->Count();
      for (colX = colsInRow; colX < aNumCols; colX++) {
        row->AppendElement(nsnull);
      }
    }
    // update the col array 
    for (colX = origNumCols; colX < aNumCols; colX++) {
      nsColInfo* colInfo = new nsColInfo();
      if (colInfo) {
        mCols.AppendElement(colInfo);
      }
    }
  }

  // if the number of rows has increased, add the extra rows. the new
  // rows get the new number of cols
  PRInt32 newRows = aNumMapRows - origNumMapRows; 
  for ( ; newRows > 0; newRows--) {
    nsVoidArray* row;
    row = (0 == aNumCols) ? new nsVoidArray() : new nsVoidArray(aNumCols);
    if (row) {
      mRows.AppendElement(row);
    }
  }
}

void
nsCellMap::InsertRows(nsVoidArray& aRows,
                      PRInt32      aFirstRowIndex,
                      PRBool       aConsiderSpans)
{
  PRInt32 numCols = mCols.Count();

  if (aFirstRowIndex > mRowCount) {
    NS_ASSERTION(PR_FALSE, "nsCellMap::InsertRows bad first row index");
    return;
  }

  if (!aConsiderSpans) {
    ExpandWithRows(aRows, aFirstRowIndex);
    return;
  }

  // if any cells span into or out of the row being inserted, then rebuild
  PRBool spansCauseRebuild = CellsSpanInOrOut(aFirstRowIndex, aFirstRowIndex,
                                              0, numCols - 1);

  // if any of the new cells span out of the new rows being added, then rebuild
  // XXX it would be better to only rebuild the portion of the map that follows the new rows
  if (!spansCauseRebuild && (aFirstRowIndex < mRows.Count())) {
    spansCauseRebuild = CellsSpanOut(aRows);
  }

  if (spansCauseRebuild) {
    RebuildConsideringRows(aFirstRowIndex, &aRows);
  }
  else {
    ExpandWithRows(aRows, aFirstRowIndex);
  }
}

void
nsCellMap::RemoveRows(PRInt32 aFirstRowIndex,
                      PRInt32 aNumRowsToRemove,
                      PRBool  aConsiderSpans)
{
  PRInt32 numRows = mRows.Count();
  PRInt32 numCols = mCols.Count();

  if (aFirstRowIndex >= numRows) {
    return;
  }
  if (!aConsiderSpans) {
    ShrinkWithoutRows(aFirstRowIndex, aNumRowsToRemove);
    return;
  }
  PRInt32 endRowIndex = aFirstRowIndex + aNumRowsToRemove - 1;
  if (endRowIndex >= numRows) {
    NS_ASSERTION(PR_FALSE, "nsCellMap::RemoveRows tried to remove too many rows");
    endRowIndex = numRows - 1;
  }
  PRBool spansCauseRebuild = CellsSpanInOrOut(aFirstRowIndex, endRowIndex,
                                              0, numCols - 1);

  if (spansCauseRebuild) {
    RebuildConsideringRows(aFirstRowIndex, nsnull, aNumRowsToRemove);
  }
  else {
    ShrinkWithoutRows(aFirstRowIndex, aNumRowsToRemove);
  }
}

void
nsCellMap::AppendCol()
{ 
  Grow(mRows.Count(), mCols.Count() + 1);
}

PRInt32 nsCellMap::AppendCell(nsTableCellFrame* aCellFrame, 
                              PRInt32           aRowIndex,
                              PRBool            aRebuildIfNecessary)
{
  NS_ASSERTION(aCellFrame, "bad cell frame");

  PRInt32 origNumMapRows = mRows.Count();
  PRInt32 origNumCols    = mCols.Count();

  // get the first null CellData in the desired row. It may be 1 past the end if there are none
  PRInt32 startColIndex;
  if (aRowIndex < origNumMapRows) {
    for (startColIndex = 0; startColIndex < origNumCols; startColIndex++) {
      CellData* data = GetMapCellAt(aRowIndex, startColIndex);
      if (!data) {
        break; // we found the col
      }
    }
  }
  else {
    startColIndex = 0;
  }

  PRInt32 rowSpan = aCellFrame->GetRowSpan();
  PRInt32 colSpan = aCellFrame->GetColSpan();

  // if the new cell could potentially span into other rows and collide with 
  // originating cells there, we will play it safe and just rebuild the map
  if (aRebuildIfNecessary && (aRowIndex < mRowCount - 1) && (rowSpan > 1)) {
    nsVoidArray newCellArray;
    newCellArray.AppendElement(aCellFrame);
    RebuildConsideringCells(&newCellArray, aRowIndex, startColIndex, PR_TRUE);
    return startColIndex;
  }

  mRowCount = PR_MAX(mRowCount, aRowIndex + 1);

  // Grow. we may need to add new rows/cols 
  PRInt32 spanNumRows = aRowIndex + rowSpan;
  PRInt32 spanNumCols = startColIndex + colSpan;
  if ((spanNumRows >= origNumMapRows) || (spanNumCols >= origNumCols)) {
    Grow(spanNumRows, spanNumCols);
  }

  // Setup CellData for this cell
  CellData* origData = new CellData(aCellFrame, nsnull, nsnull);
  SetMapCellAt(*origData, aRowIndex, startColIndex);

  // initialize the cell frame
  aCellFrame->InitCellFrame(startColIndex);

  // Create CellData objects for the rows that this cell spans. Set
  // their mOrigCell to nsnull and their mSpanData to point to data.
  for (PRInt32 rowX = aRowIndex; rowX < spanNumRows; rowX++) {
    for (PRInt32 colX = startColIndex; colX < spanNumCols; colX++) {
      if ((rowX != aRowIndex) || (colX != startColIndex)) { // skip orig cell data done above
        CellData* cellData = GetMapCellAt(rowX, colX);
        if (cellData) {
          NS_ASSERTION(!cellData->mOrigCell, "cannot overlap originating cell");
          if (rowX > aRowIndex) { // row spanning into cell
            if (cellData->mRowSpanData) {
              NS_ASSERTION(PR_FALSE, "too many overlaps");
            }
            else {
              cellData->mRowSpanData = origData;
            }
          }
          if (colX > startColIndex) { // col spanning into cell
            if (cellData->mColSpanData) {
              NS_ASSERTION(PR_FALSE, "too many overlaps");
            }
            else {
              cellData->mColSpanData = origData;
              nsColInfo* colInfo = (nsColInfo *) mCols.ElementAt(colX);
              colInfo->mNumCellsSpan++;
            }
          }
        }
        else { 
          cellData = new CellData(nsnull, nsnull, nsnull);
          if (rowX > aRowIndex) {
            cellData->mRowSpanData = origData;
          }
          if (colX > startColIndex) {
            cellData->mColSpanData = origData;
          }
          SetMapCellAt(*cellData, rowX, colX);
        }
      }
    }
  }
  //Dump();
  return startColIndex;
}

PRBool nsCellMap::CellsSpanOut(nsVoidArray& aRows)
{ 
  PRInt32 numNewRows = aRows.Count();
  for (PRInt32 rowX = 0; rowX < numNewRows; rowX++) {
    nsIFrame* rowFrame = (nsIFrame *) aRows.ElementAt(rowX);
    nsIFrame* cellFrame = nsnull;
    rowFrame->FirstChild(nsnull, &cellFrame);
    while (cellFrame) {
      nsIAtom* frameType;
      cellFrame->GetFrameType(&frameType);
      if (nsLayoutAtoms::tableCellFrame == frameType) {
        PRInt32 rowSpan = ((nsTableCellFrame *)cellFrame)->GetRowSpan();
        if (rowX + rowSpan > numNewRows) {
          NS_RELEASE(frameType);
          return PR_TRUE;
        }
      }
      NS_IF_RELEASE(frameType);
      cellFrame->GetNextSibling(&cellFrame);
    }
  }
  return PR_FALSE;
}
    
// return PR_TRUE if any cells have rows spans into or out of the region 
// defined by the row and col indices or any cells have colspans into the region
PRBool nsCellMap::CellsSpanInOrOut(PRInt32 aStartRowIndex, 
                                   PRInt32 aEndRowIndex,
                                   PRInt32 aStartColIndex, 
                                   PRInt32 aEndColIndex)
{
  for (PRInt32 colX = aStartColIndex; colX <= aEndColIndex; colX++) {
    CellData* cellData;
    if (aStartRowIndex > 0) {
      cellData = GetMapCellAt(aStartRowIndex, colX);
      if (cellData && (cellData->mRowSpanData)) {
        return PR_TRUE; // a cell row spans into
      }
    }
    if (aEndRowIndex < mRowCount - 1) {
      cellData = GetMapCellAt(aEndRowIndex + 1, colX);
      if ((cellData) && (cellData->mRowSpanData)) {
        return PR_TRUE; // a cell row spans out
      }
    }
  }
  if (aStartColIndex > 0) {
    for (PRInt32 rowX = aStartRowIndex; rowX <= aEndRowIndex; rowX++) {
      CellData* cellData = GetMapCellAt(rowX, aStartColIndex);
      if (cellData && (cellData->mColSpanData)) {
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

void nsCellMap::InsertCells(nsVoidArray& aCellFrames, 
                            PRInt32     aRowIndex,
                            PRInt32      aColIndexBefore)
{
  if (aCellFrames.Count() == 0) return;
  PRInt32 numCols = mCols.Count();
  if (aColIndexBefore >= numCols) {
    NS_ASSERTION(PR_FALSE, "bad arg in nsCellMap::InsertCellAt");
    return;
  }

  // get the starting col index of the 1st new cells 
  PRInt32 startColIndex;
  for (startColIndex = aColIndexBefore + 1; startColIndex < numCols; startColIndex++) {
    CellData* data = GetMapCellAt(aRowIndex, startColIndex);
    if (data && data->mOrigCell) {
      break; // we found the col index
    }
  }

  // record whether inserted cells are going to cause complications due 
  // to existing row spans, col spans or table sizing. 
  PRBool spansCauseRebuild = PR_FALSE;

  // check that all cells have the same row span
  PRInt32 numNewCells = aCellFrames.Count();
  PRInt32 rowSpan = 0;
  for (PRInt32 cellX = 0; cellX < numNewCells; cellX++) {
    nsTableCellFrame* cell = (nsTableCellFrame*) aCellFrames.ElementAt(cellX);
    PRInt32 rowSpan2 = cell->GetRowSpan();
    if (rowSpan == 0) {
      rowSpan = rowSpan2;
    }
    else if (rowSpan != rowSpan2) {
      spansCauseRebuild = PR_TRUE;
      break;
    }
  }

  // check if the new cells will cause the table to add more rows
  if (!spansCauseRebuild) {
    if (mRows.Count() < aRowIndex + rowSpan) {
      spansCauseRebuild = PR_TRUE;
    }
  }

  if (!spansCauseRebuild) {
    spansCauseRebuild = CellsSpanInOrOut(aRowIndex, aRowIndex + rowSpan - 1, startColIndex, numCols - 1);
  }

  if (spansCauseRebuild) {
    RebuildConsideringCells(&aCellFrames, aRowIndex, startColIndex, PR_TRUE);
  }
  else {
    ExpandWithCells(aCellFrames, aRowIndex, startColIndex, rowSpan);
  }

}

PRBool
nsCellMap::CreateEmptyRow(PRInt32 aRowIndex,
                          PRInt32 aNumCols)
{
  nsVoidArray* row;
  row = (0 == aNumCols) ? new nsVoidArray() : new nsVoidArray(aNumCols);
  if (!row) {
    return PR_FALSE;
  }
  mRows.InsertElementAt(row, aRowIndex);

  return PR_TRUE;
}

 
void
nsCellMap::ExpandWithRows(nsVoidArray& aRowFrames,
                          PRInt32      aStartRowIndex)
{
  PRInt32 numNewRows  = aRowFrames.Count();;
  PRInt32 origNumCols = mCols.Count();
  PRInt32 endRowIndex = aStartRowIndex + numNewRows - 1;

  // create the new rows first
  PRInt32 newRowIndex = 0;
  PRInt32 rowX;
  for (rowX = aStartRowIndex; rowX <= endRowIndex; rowX++) {
    if (!CreateEmptyRow(rowX, origNumCols)) {
      return;
    }
    newRowIndex++;
  }
  mRowCount += numNewRows;

  newRowIndex = 0;
  for (rowX = aStartRowIndex; rowX <= endRowIndex; rowX++) {
    nsTableRowFrame* rFrame = (nsTableRowFrame *)aRowFrames.ElementAt(newRowIndex);
    // append cells 
    nsIFrame* cFrame = nsnull;
    rFrame->FirstChild(nsnull, &cFrame);
    while (cFrame) {
      nsIAtom* cFrameType;
      cFrame->GetFrameType(&cFrameType);
      if (nsLayoutAtoms::tableCellFrame == cFrameType) {
        AppendCell((nsTableCellFrame *)cFrame, rowX, PR_FALSE);
      }
      NS_IF_RELEASE(cFrameType);
      cFrame->GetNextSibling(&cFrame);
    }
    newRowIndex++;
  }
  mNextAvailRowIndex += numNewRows;
}

void nsCellMap::ExpandWithCells(nsVoidArray& aCellFrames,
                                PRInt32      aRowIndex,
                                PRInt32      aColIndex,
                                PRInt32      aRowSpan)
{
  PRInt32 endRowIndex = aRowIndex + aRowSpan - 1;
  PRInt32 startColIndex = aColIndex;
  PRInt32 endColIndex;
  PRInt32 numCells = aCellFrames.Count();
  PRInt32 totalColSpan = 0;

  // add cellData entries for the space taken up by the new cells
  for (PRInt32 cellX = 0; cellX < numCells; cellX++) {
    nsTableCellFrame* cellFrame = (nsTableCellFrame*) aCellFrames.ElementAt(cellX);
    CellData* origData = new CellData(cellFrame, nsnull, nsnull); // the originating cell
    if (!origData) return;

    // set the starting and ending col index for the new cell
    PRInt32 colSpan = cellFrame->GetColSpan();
    totalColSpan += colSpan;
    if (cellX == 0) {
      endColIndex = aColIndex + colSpan - 1;
    }
    else {
      startColIndex = endColIndex + 1;
      endColIndex   = startColIndex + colSpan - 1;
    }
 
    // add the originating cell data and any cell data corresponding to row/col spans
    for (PRInt32 rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
      nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
      for (PRInt32 colX = aColIndex; colX <= endColIndex; colX++) {
        row->InsertElementAt(nsnull, colX);
        CellData* data = origData;
        if ((rowX != aRowIndex) || (colX != startColIndex)) {
          data = new CellData(nsnull, nsnull, nsnull);
          if (!data) return;
          if (rowX > aRowIndex) {
            data->mRowSpanData = origData;
          }
          if (colX > startColIndex) {
            data->mColSpanData = origData;
          }
        }
        SetMapCellAt(*data, rowX, colX); // this increments the mNumColsOrigInCol array
      }
    }
    cellFrame->InitCellFrame(startColIndex); 
  }

  // if new columns were added to the rows occupied by the new cells, 
  // update the other rows to have the same number of cols
  PRInt32 numNewCols = ((nsVoidArray*)mRows.ElementAt(aRowIndex))->Count() - mCols.Count();
  if (numNewCols > 0) {
    Grow(mRows.Count(), mCols.Count() + numNewCols);
  }

  // update the row and col info due to shifting
  for (PRInt32 rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
    PRInt32 numCols = row->Count();
    for (PRInt32 colX = aColIndex + totalColSpan; colX < numCols; colX++) {
      CellData* data = (CellData*) row->ElementAt(colX);
      if (data) {
        if (data->mOrigCell) {
          // a cell that gets moved needs adjustment as well as it new orignating col
          data->mOrigCell->SetColIndex(colX);
          nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
          colInfo->mNumCellsOrig++;
        }
        else if (data->mColSpanData) {
          nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
          colInfo->mNumCellsSpan++;
        }
      }
      data = (CellData*) row->ElementAt(colX - totalColSpan);
      if (data) {
        if (data->mOrigCell) {
          // the old originating col of a moved cell needs adjustment
          nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX - totalColSpan);
          colInfo->mNumCellsOrig--;
        }
        else if (data->mColSpanData) {
          nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
          colInfo->mNumCellsSpan--;
        }
      }
    }
  }
}

void nsCellMap::ShrinkWithoutRows(PRInt32 aStartRowIndex,
                                  PRInt32 aNumRowsToRemove)
{
  PRInt32 endRowIndex = aStartRowIndex + aNumRowsToRemove - 1;
  PRInt32 colCount = mCols.Count();
  for (PRInt32 rowX = endRowIndex; rowX >= aStartRowIndex; rowX--) {
    nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(rowX));
    PRInt32 colX;
    for (colX = 0; colX < colCount; colX++) {
      CellData* data = (CellData *) row->ElementAt(colX);
      if (data) {
        // Adjust the column counts.
        if (data->mOrigCell) {
          // Decrement the column count.
          nsColInfo* colInfo = (nsColInfo *)(mCols.ElementAt(colX));
          colInfo->mNumCellsOrig--;
        }
        else if (data->mColSpanData) {
          nsColInfo* colInfo = (nsColInfo *)(mCols.ElementAt(colX));
          colInfo->mNumCellsSpan--;
        }
      }
    }

    // Delete our row information.
    for (colX = 0; colX < colCount; colX++) {
      CellData* data = (CellData *)(row->ElementAt(colX));
      if (data) {
        delete data;
      }
    }

    mRows.RemoveElementAt(rowX);
    delete row;

    // Decrement our row and next available index counts.
    mRowCount--;
    mNextAvailRowIndex--;

    // remove cols that may not be needed any more due to the removal of the rows
    RemoveUnusedCols(colCount);
  }
}

void nsCellMap::ShrinkWithoutCell(nsTableCellFrame& aCellFrame,
                                  PRInt32           aRowIndex,
                                  PRInt32           aColIndex)
{
  PRInt32 rowSpan     = aCellFrame.GetRowSpan();
  PRInt32 colSpan     = aCellFrame.GetColSpan();
  PRInt32 endRowIndex = aRowIndex + rowSpan - 1;
  PRInt32 endColIndex = aColIndex + colSpan - 1;

  PRInt32 colX;
  // adjust the col counts due to the deleted cell before removing it
  for (colX = aColIndex; colX <= endColIndex; colX++) {
    nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
    if (colX == aColIndex) {
      colInfo->mNumCellsOrig--;
    }
    else {
      colInfo->mNumCellsSpan--;
    }
  }

  // remove the deleted cell and cellData entries for it
  PRInt32 rowX;
  for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
    for (colX = aColIndex; colX <= endColIndex; colX++) {
      row->RemoveElementAt(colX);
    }
    // put back null entries in the row to make it the right size
    for (colX = 0; colX < colSpan; colX++) {
      row->AppendElement(nsnull);
    }
  }

  PRInt32 numCols = mCols.Count();

  // update the row and col info due to shifting
  for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
    for (colX = aColIndex; colX < numCols - colSpan; colX++) {
      CellData* data = (CellData*) row->ElementAt(colX);
      if (data) {
        if (data->mOrigCell) {
          // a cell that gets moved to the left needs adjustment in its new location 
          data->mOrigCell->SetColIndex(colX);
          nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
          colInfo->mNumCellsOrig++;
          // a cell that gets moved to the left needs adjustment in its old location 
          colInfo = (nsColInfo *)mCols.ElementAt(colX + colSpan);
          if (colInfo) {
            colInfo->mNumCellsOrig--;
          }
        }
        else if (data->mColSpanData) {
          // a cell that gets moved to the left needs adjustment in its new location 
          nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
          colInfo->mNumCellsSpan++;
          // a cell that gets moved to the left needs adjustment in its old location 
          colInfo = (nsColInfo *)mCols.ElementAt(colX + colSpan);
          if (colInfo) {
            colInfo->mNumCellsSpan--;
          }
        }
      }
    }
  }

  // remove cols that may not be needed any more due to the removal of the cell
  RemoveUnusedCols(mCols.Count());
}

PRInt32
nsCellMap::RemoveUnusedCols(PRInt32 aMaxToRemove)
{
  PRInt32 numColsRemoved = 0;
  for (PRInt32 colX = mCols.Count() - 1; colX >= 0; colX--) {
    nsColInfo* colInfo = (nsColInfo *) mCols.ElementAt(colX);
    if (!colInfo || (colInfo->mNumCellsOrig > 0) || (colInfo->mNumCellsSpan > 0)) {
      return numColsRemoved;
    }
    else { 
      // remove the col from the cols array
      colInfo = (nsColInfo *) mCols.ElementAt(colX);
      mCols.RemoveElementAt(colX);
      delete colInfo;

      PRInt32 numMapRows = mRows.Count();
      // remove the col from each of the rows
      for (PRInt32 rowX = 0; rowX < numMapRows; rowX++) {
        nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
        CellData* data = (CellData*) row->ElementAt(colX);
        delete data;
        row->RemoveElementAt(colX);
      }
      numColsRemoved++;
      if (numColsRemoved >= aMaxToRemove) {
        return numColsRemoved;
      }
    }
  }
  return numColsRemoved;
}


void
nsCellMap::RebuildConsideringRows(PRInt32      aStartRowIndex,
                                  nsVoidArray* aRowsToInsert,
                                  PRBool       aNumRowsToRemove)
{
  // copy the old cell map into a new array
  PRInt32 numOrigRows = mRows.Count();
  PRInt32 numOrigCols = mCols.Count();
  void** origRows = new void*[numOrigRows];
  if (!origRows) return;
  PRInt32 rowX, colX;
  // copy the orig rows
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
    origRows[rowX] = row;
  }
  for (colX = 0; colX < numOrigCols; colX++) {
    nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
    colInfo->mNumCellsOrig = 0;
  }
  mRows.Clear();
  mRowCount = 0;
  mNextAvailRowIndex = 0;
  if (aRowsToInsert) {
    Grow(numOrigRows, numOrigCols);
  }

  // put back the rows before the affected ones just as before
  for (rowX = 0; rowX < aStartRowIndex; rowX++) {
    nsVoidArray* row = (nsVoidArray *)origRows[rowX];
    PRInt32 numCols = row->Count();
    for (colX = 0; colX < numCols; colX++) {
      // put in the original cell from the cell map
      CellData* data = (CellData*) row->ElementAt(colX);
      if (data && data->mOrigCell) {
        AppendCell(data->mOrigCell, rowX, PR_FALSE);
      }
    }
    mNextAvailRowIndex++;
  }
  PRInt32 copyStartRowIndex;
  if (aRowsToInsert) {
    // add in the new cells and create rows if necessary
    PRInt32 numNewRows = aRowsToInsert->Count();
    rowX = aStartRowIndex;
    for (PRInt32 newRowX = 0; newRowX < numNewRows; newRowX++) {
      nsTableRowFrame* rFrame = (nsTableRowFrame *)aRowsToInsert->ElementAt(newRowX);
      nsIFrame* cFrame = nsnull;
      rFrame->FirstChild(nsnull, &cFrame);
      while (cFrame) {
        nsIAtom* cFrameType;
        cFrame->GetFrameType(&cFrameType);
        if (nsLayoutAtoms::tableCellFrame == cFrameType) {
          AppendCell((nsTableCellFrame *)cFrame, rowX, PR_FALSE);
        }
        NS_IF_RELEASE(cFrameType);
        cFrame->GetNextSibling(&cFrame);
      }
      rowX++;
    }
    copyStartRowIndex = aStartRowIndex;
  }
  else {
    rowX = aStartRowIndex;
    copyStartRowIndex = aStartRowIndex + aNumRowsToRemove;
  }
  // put back the rows after the affected ones just as before
  PRInt32 copyEndRowIndex = numOrigRows - 1;
  for (PRInt32 copyRowX = copyStartRowIndex; copyRowX <= copyEndRowIndex; copyRowX++) {
    nsVoidArray* row = (nsVoidArray *)origRows[copyRowX];
    PRInt32 numCols = row->Count();
    for (colX = 0; colX < numCols; colX++) {
      // put in the original cell from the cell map
      CellData* data = (CellData*) row->ElementAt(colX);
      if (data && data->mOrigCell) {
        AppendCell(data->mOrigCell, rowX, PR_FALSE);
      }
    }
    rowX++;
  }

  mNextAvailRowIndex = mRowCount;

  // delete the old cell map
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    nsVoidArray* row = (nsVoidArray *)origRows[rowX];
    PRInt32 len = row->Count();
    for (colX = 0; colX < len; colX++) {
      CellData* data = (CellData*) row->ElementAt(colX);
      delete data;
    }
    delete row;
  }
  delete [] origRows;
}

void nsCellMap::RebuildConsideringCells(nsVoidArray* aCellFrames,
                                        PRInt32      aRowIndex,
                                        PRInt32      aColIndex,
                                        PRBool       aInsert)
{
  // copy the old cell map into a new array
  PRInt32 numOrigRows = mRows.Count();
  PRInt32 numOrigCols = mCols.Count();
  void** origRows = new void*[numOrigRows];
  if (!origRows) return;
  PRInt32 rowX;
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
    origRows[rowX] = row;
  }
  // reinitialize data members
  mRows.Clear();
  mRowCount = 0;
  mNextAvailRowIndex = numOrigRows;
  Grow(numOrigRows, numOrigCols);

  PRInt32 numNewCells = (aCellFrames) ? aCellFrames->Count() : 0;
  // build the new cell map 
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    nsVoidArray* row = (nsVoidArray *)origRows[rowX];
    PRInt32 numCols = row->Count();
    for (PRInt32 colX = 0; colX < numCols; colX++) {
      if ((rowX == aRowIndex) && (colX == aColIndex)) { 
        if (aInsert) { // put in the new cells
          for (PRInt32 cellX = 0; cellX < numNewCells; cellX++) {
            nsTableCellFrame* cell = (nsTableCellFrame*)aCellFrames->ElementAt(cellX);
            AppendCell(cell, rowX, PR_FALSE);
          }
        }
        else {
          continue; // do not put the deleted cell back
        }
      }
      // put in the original cell from the cell map
      CellData* data = (CellData*) row->ElementAt(colX);
      if (data && data->mOrigCell) {
        AppendCell(data->mOrigCell, rowX, PR_FALSE);
      }
    }
  }

  // delete the old cell map
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    nsVoidArray* row = (nsVoidArray *)origRows[rowX];
    PRInt32 len = row->Count();
    for (PRInt32 colX = 0; colX < len; colX++) {
      CellData* data = (CellData*) row->ElementAt(colX);
      delete data;
    }
    delete row;
  }
  delete [] origRows;
}

void nsCellMap::RemoveCell(nsTableCellFrame* aCellFrame,
                           PRInt32          aRowIndex)
{
  PRInt32 numRows = mRows.Count();
  if ((aRowIndex < 0) || (aRowIndex >= numRows)) {
    NS_ASSERTION(PR_FALSE, "bad arg in nsCellMap::RemoveCell");
    return;
  }
  PRInt32 numCols = mCols.Count();

  // get the starting col index of the cell to remove
  PRInt32 startColIndex;
  for (startColIndex = 0; startColIndex < numCols; startColIndex++) {
    CellData* data = GetMapCellAt(aRowIndex, startColIndex);
    if (data && (aCellFrame == data->mOrigCell)) {
      break; // we found the col index
    }
  }

  PRInt32 rowSpan = aCellFrame->GetRowSpan();
  PRInt32 endRowIndex = aRowIndex + rowSpan - 1;
  PRInt32 endColIndex = startColIndex + aCellFrame->GetColSpan() - 1;
  // record whether removing the cells is going to cause complications due 
  // to existing row spans, col spans or table sizing. 
  PRBool spansCauseRebuild = PR_FALSE;

  // check if removing the cell will cause the table to reduce the number of rows
  if (endRowIndex == numRows) {
    spansCauseRebuild = PR_TRUE;
    for (PRInt32 rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
      for (PRInt32 colX = 0; colX < numCols; colX++) {
        if ((colX < startColIndex) || (colX > endColIndex)) {
          CellData* data = GetMapCellAt(rowX, colX);
          if (data) {
            // there is either an originating or spanned cell in a row of the cell 
            spansCauseRebuild = PR_FALSE;
            break;
          }
        }
      }
    }
  }

  if (!spansCauseRebuild) {
    spansCauseRebuild = CellsSpanInOrOut(aRowIndex, aRowIndex + rowSpan - 1, startColIndex, numCols - 1);
  }

  if (spansCauseRebuild) {
    RebuildConsideringCells(nsnull, aRowIndex, startColIndex, PR_FALSE);
  }
  else {
    ShrinkWithoutCell(*aCellFrame, aRowIndex, startColIndex);
  }
}

PRInt32 nsCellMap::GetNumCellsOriginatingInCol(PRInt32 aColIndex) const
{
  PRInt32 colCount = mCols.Count();
  if ((aColIndex >= 0) && (aColIndex < colCount)) {
    return ((nsColInfo *)mCols.ElementAt(aColIndex))->mNumCellsOrig;
  }
  else {
    NS_ASSERTION(PR_FALSE, "nsCellMap::GetNumCellsOriginatingInCol - bad col index");
    return 0;
  }
}

#ifdef NS_DEBUG
void nsCellMap::Dump() const
{
  printf("***** START CELL MAP DUMP *****\n");
  PRInt32 mapRowCount = mRows.Count();
  PRInt32 colCount = mCols.Count();
  PRInt32 colIndex;
  printf("mapRowCount=%d tableRowCount=%d, colCount=%d \n", mapRowCount, mRowCount, colCount);
  PRInt32 rowIndex;

  for (rowIndex = 0; rowIndex < mapRowCount; rowIndex++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowIndex);
    printf("row %d : ", rowIndex);
    for (colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = (CellData *)row->ElementAt(colIndex);
      if (cd) {
        if (cd->mOrigCell) {
          printf("C%d,%d  ", rowIndex, colIndex);
        } else {
          nsTableCellFrame* cell = nsnull;
          if (cd->mRowSpanData) {
            cell = cd->mRowSpanData->mOrigCell;
            // check the validity of the cell map
            nsVoidArray* rowAbove = (nsVoidArray *)mRows.ElementAt(rowIndex - 1);
            CellData* cdAbove = (CellData *)rowAbove->ElementAt(colIndex);
            if ((cdAbove->mOrigCell != cell) && (cdAbove->mRowSpanData->mOrigCell != cell)) {
              NS_ASSERTION(PR_FALSE, "bad row span data");
            }
            printf("R ");
          }
          if (cd->mColSpanData) {
            cell = cd->mColSpanData->mOrigCell;
            // check the validity of the cell map
            CellData* cdBefore = (CellData *)row->ElementAt(colIndex - 1);
            if ((cdBefore->mOrigCell != cell) && (cdBefore->mColSpanData->mOrigCell != cell)) {
              NS_ASSERTION(PR_FALSE, "bad col span data");
            }
            printf("C ");
          }
          if (!(cd->mRowSpanData && cd->mColSpanData)) {
            printf("  ");
          } 
          printf("  ");  
        }
      } else {
        printf("----  ");
      }
    }
    printf("\n");
  }

  // output info mapping Ci,j to cell address
  PRInt32 cellCount = 0;
  for (PRInt32 rIndex = 0; rIndex < mapRowCount; rIndex++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rIndex);
    for (colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = (CellData *)row->ElementAt(colIndex);
      if (cd) {
        if (cd->mOrigCell) {
          nsTableCellFrame* cellFrame = cd->mOrigCell;
          PRInt32 cellFrameColIndex;
          cellFrame->GetColIndex(cellFrameColIndex);
          printf("C%d,%d=%p(%d)  ", rIndex, colIndex, cellFrame, cellFrameColIndex);
          cellCount++;
        }
      }
    }
    printf("\n");
  }

  // output col info
	printf ("\ncols array orig/span-> ");
	for (colIndex = 0; colIndex < colCount; colIndex++) {
    nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colIndex);
		printf ("%d=%d/%d ", colIndex, colInfo->mNumCellsOrig, colInfo->mNumCellsSpan);
	}
  printf("\n***** end CellMap Dump *****\n");
}
#endif

// only called if the cell at aMapRowIndex, aColIndex is null
void nsCellMap::SetMapCellAt(CellData& aNewCell, 
                             PRInt32   aMapRowIndex, 
                             PRInt32   aColIndex)
{
  nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(aMapRowIndex));
  row->ReplaceElementAt(&aNewCell, aColIndex);
  // update the originating cell counts if cell originates in this row, col
  nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(aColIndex);
  if (aNewCell.mOrigCell) { 
    colInfo->mNumCellsOrig++;
  }
  else if (aNewCell.mColSpanData) {
    colInfo->mNumCellsSpan++;
  }
}

PRInt32 nsCellMap::GetEffectiveColSpan(PRInt32                 aColIndex, 
                                       const nsTableCellFrame* aCell) const
{
  NS_PRECONDITION(nsnull != aCell, "bad cell arg");

  PRInt32 initialRowX;
  aCell->GetRowIndex(initialRowX);

  PRInt32 effColSpan = 0;
  PRInt32 colCount = mCols.Count();
  for (PRInt32 colX = aColIndex; colX < colCount; colX++) {
    PRBool found = PR_FALSE;
    CellData* cellData = GetCellAt(initialRowX, colX);
    if (cellData) {
      if (cellData->mOrigCell) {
        if (cellData->mOrigCell == aCell) {
          found = PR_TRUE;
        }
      }
      else if (cellData->mColSpanData && 
              (cellData->mColSpanData->mOrigCell == aCell)) {
        found = PR_TRUE;
      }
    }
    if (found) {
      effColSpan++;
    }
    else {
      break;
    }
  }

  NS_ASSERTION(effColSpan > 0, "invalid col span or col index");
  return effColSpan;
}

nsTableCellFrame* nsCellMap::GetCellFrameOriginatingAt(PRInt32 aRowX, 
                                                       PRInt32 aColX) const
{
  CellData* data = GetCellAt(aRowX, aColX);
  if (data) {
    return data->mOrigCell;
  }
  return nsnull;
}

nsTableCellFrame* nsCellMap::GetCellInfoAt(PRInt32  aRowX, 
                                           PRInt32  aColX, 
                                           PRBool*  aOriginates, 
                                           PRInt32* aColSpan) const
{
  if (aOriginates)
    *aOriginates = PR_FALSE;
  CellData* data = GetCellAt(aRowX, aColX);
  nsTableCellFrame* cellFrame = nsnull;  
  if (data) {
    if (data->mOrigCell) {
      cellFrame = data->mOrigCell;
      if (aOriginates)
        *aOriginates = PR_TRUE;
      if (aColSpan) {
        PRInt32 initialColIndex;
        cellFrame->GetColIndex(initialColIndex);
        *aColSpan = GetEffectiveColSpan(initialColIndex, cellFrame);
      }
    }
    else {
      if (data->mRowSpanData) {
        cellFrame = data->mRowSpanData->mOrigCell;
      }
      else if (data->mColSpanData) {
        cellFrame = data->mColSpanData->mOrigCell;
      }
      if (aColSpan)
        *aColSpan = 0;
    }
  }
  return cellFrame;
}
  

PRBool nsCellMap::RowIsSpannedInto(PRInt32 aRowIndex) const
{
  if ((0 > aRowIndex) || (aRowIndex >= mRowCount)) {
    return PR_FALSE;
  }
  PRInt32 colCount = mCols.Count();
	for (PRInt32 colIndex = 0; colIndex < colCount; colIndex++) {
		CellData* cd = GetCellAt(aRowIndex, colIndex);
		if (cd) { // there's really a cell at (aRowIndex, colIndex)
			if (!cd->mOrigCell) { // the cell at (aRowIndex, colIndex) is the result of a span
				if (cd->mRowSpanData && cd->mRowSpanData->mOrigCell)
          return PR_TRUE;
			}
		}
  }
  return PR_FALSE;
}

PRBool nsCellMap::RowHasSpanningCells(PRInt32 aRowIndex) const
{
  if ((0 > aRowIndex) || (aRowIndex >= mRowCount)) {
    return PR_FALSE;
  }
  PRInt32 colCount = mCols.Count();
  if (aRowIndex != mRowCount - 1) {
    // aRowIndex is not the last row, so we check the next row after aRowIndex for spanners
    for (PRInt32 colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = GetCellAt(aRowIndex, colIndex);
      if (cd && (cd->mOrigCell)) { // cell originates 
        CellData* cd2 = GetCellAt(aRowIndex + 1, colIndex);
        if (cd2 && !cd2->mOrigCell && cd2->mRowSpanData) { // cd2 is spanned by a row
          if (cd->mOrigCell == cd2->mRowSpanData->mOrigCell)
            return PR_TRUE;
        }
      }
    }
  }
  return PR_FALSE;
}

PRBool nsCellMap::ColIsSpannedInto(PRInt32 aColIndex) const
{
  if ((0 > aColIndex) || (aColIndex >= mCols.Count())) {
    return PR_FALSE;
  }
	for (PRInt32 rowIndex = 0; rowIndex < mRowCount; rowIndex++) {
		CellData* cd = GetCellAt(rowIndex, aColIndex);
		if (cd) { // there's really a cell at (aRowIndex, colIndex)
			if (!cd->mOrigCell) { // the cell at (aRowIndex, colIndex) is the result of a span
				if (cd->mColSpanData && cd->mColSpanData->mOrigCell)
          return PR_TRUE;
			}
		}
  }
  return PR_FALSE;
}

PRBool nsCellMap::ColHasSpanningCells(PRInt32 aColIndex) const
{
  NS_PRECONDITION (aColIndex < mCols.Count(), "bad col index arg");
  PRInt32 colCount = mCols.Count();
  if ((0 > aColIndex) || (aColIndex >= colCount - 1)) 
    return PR_FALSE;
 
  for (PRInt32 rowIndex = 0; rowIndex < mRowCount; rowIndex++) {
    CellData* cd = GetCellAt(rowIndex, aColIndex);
    if (cd && (cd->mOrigCell)) { // cell originates 
      CellData* cd2 = GetCellAt(rowIndex + 1, aColIndex);
      if (cd2 && !cd2->mOrigCell && cd2->mColSpanData) { // cd2 is spanned by a col
        if (cd->mOrigCell == cd2->mColSpanData->mOrigCell)
          return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

#ifdef DEBUG
void nsCellMap::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  NS_PRECONDITION(aResult, "null OUT parameter pointer");
  PRUint32 sum = sizeof(*this);

  // Add in the size of the void arrays. Because we have emnbedded objects
  // and not pointers to void arrays, we need to subtract out the size of the
  // embedded object so it isn't double counted
  PRUint32 voidArraySize;

  mRows.SizeOf(aHandler, &voidArraySize);
  sum += voidArraySize - sizeof(mRows);
  mCols.SizeOf(aHandler, &voidArraySize);
  sum += voidArraySize - sizeof(mCols);

  *aResult = sum;
}
#endif
