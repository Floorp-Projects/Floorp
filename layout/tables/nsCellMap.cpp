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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsVoidArray.h"
#include "nsCellMap.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableRowGroupFrame.h"

// Empty static array used for SafeElementAt() calls on mRows.
static nsCellMap::CellDataArray sEmptyRow;

// CellData 

CellData::CellData(nsTableCellFrame* aOrigCell)
{
  MOZ_COUNT_CTOR(CellData);
  mOrigCell = aOrigCell;
}

CellData::~CellData()
{
  MOZ_COUNT_DTOR(CellData);
}

BCCellData::BCCellData(nsTableCellFrame* aOrigCell)
:CellData(aOrigCell)
{
  MOZ_COUNT_CTOR(BCCellData);
}

BCCellData::~BCCellData()
{
  MOZ_COUNT_DTOR(BCCellData);
}

// nsTableCellMap

nsTableCellMap::nsTableCellMap(nsTableFrame&   aTableFrame,
                               PRBool          aBorderCollapse)
:mTableFrame(aTableFrame), mFirstMap(nsnull), mBCInfo(nsnull)
{
  MOZ_COUNT_CTOR(nsTableCellMap);

  nsAutoVoidArray orderedRowGroups;
  PRUint32 numRowGroups;
  aTableFrame.OrderRowGroups(orderedRowGroups, numRowGroups);

  for (PRUint32 rgX = 0; rgX < numRowGroups; rgX++) {
    nsTableRowGroupFrame* rgFrame =
      nsTableFrame::GetRowGroupFrame((nsIFrame*)orderedRowGroups.ElementAt(rgX));
    if (rgFrame) {
      nsTableRowGroupFrame* prior = (0 == rgX)
        ? nsnull : nsTableFrame::GetRowGroupFrame((nsIFrame*)orderedRowGroups.ElementAt(rgX - 1));
      InsertGroupCellMap(*rgFrame, prior);
    }
  }
  if (aBorderCollapse) {
    mBCInfo = new BCInfo();
  }
}

nsTableCellMap::~nsTableCellMap()
{
  MOZ_COUNT_DTOR(nsTableCellMap);

  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    nsCellMap* next = cellMap->GetNextSibling();
    delete cellMap;
    cellMap = next;
  }

  PRInt32 colCount = mCols.Count();
  for (PRInt32 colX = 0; colX < colCount; colX++) {
    nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
    if (colInfo) {
      delete colInfo;
    }
  }
  if (mBCInfo) {
    DeleteRightBottomBorders();
    delete mBCInfo;
  }
}

// Get the bcData holding the border segments of the right edge of the table
BCData*
nsTableCellMap::GetRightMostBorder(PRInt32 aRowIndex)
{
  if (!mBCInfo) ABORT1(nsnull);

  PRInt32 numRows = mBCInfo->mRightBorders.Count();
  if (aRowIndex < numRows) {
    return (BCData*)mBCInfo->mRightBorders.ElementAt(aRowIndex);
  }

  BCData* bcData;
  PRInt32 rowX = numRows;

  do {
    bcData = new BCData();
    if (!bcData) ABORT1(nsnull);
    mBCInfo->mRightBorders.AppendElement(bcData);
  } while (++rowX <= aRowIndex);

  return bcData;
}

// Get the bcData holding the border segments of the bottom edge of the table
BCData*
nsTableCellMap::GetBottomMostBorder(PRInt32 aColIndex)
{
  if (!mBCInfo) ABORT1(nsnull);

  PRInt32 numCols = mBCInfo->mBottomBorders.Count();
  if (aColIndex < numCols) {
    return (BCData*)mBCInfo->mBottomBorders.ElementAt(aColIndex);
  }

  BCData* bcData;
  PRInt32 colX = numCols;

  do {
    bcData = new BCData();
    if (!bcData) ABORT1(nsnull);
    mBCInfo->mBottomBorders.AppendElement(bcData);
  } while (++colX <= aColIndex);

  return bcData;
}

// delete the borders corresponding to the right and bottom edges of the table
void 
nsTableCellMap::DeleteRightBottomBorders()
{
  if (mBCInfo) {
    PRInt32 numCols = mBCInfo->mBottomBorders.Count();
    if (numCols > 0) {
      for (PRInt32 colX = numCols - 1; colX >= 0; colX--) {
        BCData* bcData = (BCData*)mBCInfo->mBottomBorders.ElementAt(colX);
        if (bcData) {
          delete bcData;
        }
        mBCInfo->mBottomBorders.RemoveElementAt(colX);
      }
    }
    PRUint32 numRows = mBCInfo->mRightBorders.Count();
    if (numRows > 0) {
      for (PRInt32 rowX = numRows - 1; rowX >= 0; rowX--) {
        BCData* bcData = (BCData*)mBCInfo->mRightBorders.ElementAt(rowX);
        if (bcData) {
          delete bcData;
        }
        mBCInfo->mRightBorders.RemoveElementAt(rowX);
      }
    }
  }
}
      
void 
nsTableCellMap::InsertGroupCellMap(nsCellMap* aPrevMap,
                                   nsCellMap& aNewMap)
{
  nsCellMap* next;
  if (aPrevMap) {
    next = aPrevMap->GetNextSibling();
    aPrevMap->SetNextSibling(&aNewMap);
  }
  else {
    next = mFirstMap;
    mFirstMap = &aNewMap;
  }
  aNewMap.SetNextSibling(next);
}

void nsTableCellMap::InsertGroupCellMap(nsTableRowGroupFrame&  aNewGroup,
                                        nsTableRowGroupFrame*& aPrevGroup)
{
  nsCellMap* newMap = new nsCellMap(aNewGroup, mBCInfo != nsnull);
  if (newMap) {
    nsCellMap* prevMap = nsnull;
    nsCellMap* lastMap = mFirstMap;
    if (aPrevGroup) {
      nsCellMap* map = mFirstMap;
      while (map) {
        lastMap = map;
        if (map->GetRowGroup() == aPrevGroup) {
          prevMap = map;
          break;
        }
        map = map->GetNextSibling();
      }
    }
    if (!prevMap) {
      if (aPrevGroup) {
        prevMap = lastMap;
        aPrevGroup = (prevMap) ? prevMap->GetRowGroup() : nsnull;
      }
      else {
        aPrevGroup = nsnull;
      }
    }
    InsertGroupCellMap(prevMap, *newMap);
  }
}

void nsTableCellMap::RemoveGroupCellMap(nsTableRowGroupFrame* aGroup)
{
  nsCellMap* map = mFirstMap;
  nsCellMap* prior = nsnull;
  while (map) {
    if (map->GetRowGroup() == aGroup) {
      nsCellMap* next = map->GetNextSibling();
      if (mFirstMap == map) {
        mFirstMap = next;
      }
      else {
        prior->SetNextSibling(next);
      }
      delete map;
      break;
    }
    prior = map;
    map = map->GetNextSibling();
  }
}

static nsCellMap*
FindMapFor(const nsTableRowGroupFrame* aRowGroup,
           nsCellMap* aStart,
           const nsCellMap* aEnd)
{
  for (nsCellMap* map = aStart; map != aEnd; map = map->GetNextSibling()) {
    if (aRowGroup == map->GetRowGroup()) {
      return map;
    }
  }

  return nsnull;
}

nsCellMap* 
nsTableCellMap::GetMapFor(const nsTableRowGroupFrame* aRowGroup,
                          nsCellMap* aStartHint) const
{
  NS_PRECONDITION(aRowGroup, "Must have a rowgroup");
  NS_ASSERTION(!aRowGroup->GetPrevInFlow(), "GetMapFor called with continuation");
  if (aStartHint) {
    nsCellMap* map = FindMapFor(aRowGroup, aStartHint, nsnull);
    if (map) {
      return map;
    }
  }

  nsCellMap* map = FindMapFor(aRowGroup, mFirstMap, aStartHint);
  if (map) {
    return map;
  }
  
  // if aRowGroup is a repeated header or footer find the header or footer it was repeated from
  if (aRowGroup->IsRepeatable()) {
    nsTableFrame* fifTable = NS_STATIC_CAST(nsTableFrame*, mTableFrame.GetFirstInFlow());

    nsAutoVoidArray rowGroups;
    PRUint32 numRowGroups;
    nsTableRowGroupFrame *thead, *tfoot;
    // find the original header/footer 
    fifTable->OrderRowGroups(rowGroups, numRowGroups, &thead, &tfoot);

    const nsStyleDisplay* display = aRowGroup->GetStyleDisplay();
    nsTableRowGroupFrame* rgOrig = 
      (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == display->mDisplay) ? thead : tfoot; 
    // find the row group cell map using the original header/footer
    if (rgOrig && rgOrig != aRowGroup) {
      return GetMapFor(rgOrig, aStartHint);
    }
  }

  return nsnull;
}

void
nsTableCellMap::Synchronize(nsTableFrame* aTableFrame)
{
  nsAutoVoidArray orderedRowGroups;
  nsAutoVoidArray maps;
  PRUint32 numRowGroups;
  PRInt32 mapIndex;

  maps.Clear();
  aTableFrame->OrderRowGroups(orderedRowGroups, numRowGroups);
  if (!numRowGroups) {
    return;
  }

  // Scope |map| outside the loop so we can use it as a hint.
  nsCellMap* map = nsnull;
  for (PRUint32 rgX = 0; rgX < numRowGroups; rgX++) {
    nsTableRowGroupFrame* rgFrame =
      nsTableFrame::GetRowGroupFrame((nsIFrame*)orderedRowGroups.ElementAt(rgX));
    if (rgFrame) {
      map = GetMapFor(rgFrame, map);
      if (map) {
        if (!maps.AppendElement(map)) {
          delete map;
          NS_WARNING("Could not AppendElement");
        }
      }
    }
  }
  mapIndex = maps.Count() - 1;
  nsCellMap* nextMap = (nsCellMap*) maps.ElementAt(mapIndex);
  nextMap->SetNextSibling(nsnull);
  for (mapIndex-- ; mapIndex >= 0; mapIndex--) {
    nsCellMap* map = (nsCellMap*) maps.ElementAt(mapIndex);
    map->SetNextSibling(nextMap);
    nextMap = map;
  }
  mFirstMap = nextMap;
}

PRBool
nsTableCellMap::HasMoreThanOneCell(PRInt32 aRowIndex) const
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->HasMoreThanOneCell(rowIndex);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  return PR_FALSE;
}

PRInt32
nsTableCellMap::GetNumCellsOriginatingInRow(PRInt32 aRowIndex) const
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->GetNumCellsOriginatingInRow(rowIndex);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  return 0;
}
PRInt32 
nsTableCellMap::GetEffectiveRowSpan(PRInt32 aRowIndex,
                                    PRInt32 aColIndex) const
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->GetRowSpan(rowIndex, aColIndex, PR_TRUE);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  NS_NOTREACHED("Bogus row index?");
  return 0;
}

PRInt32 
nsTableCellMap::GetEffectiveColSpan(PRInt32 aRowIndex,
                                    PRInt32 aColIndex) const
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      PRBool zeroColSpan;
      return map->GetEffectiveColSpan(*this, rowIndex, aColIndex, zeroColSpan);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  NS_NOTREACHED("Bogus row index?");
  return 0;
}

nsTableCellFrame* 
nsTableCellMap::GetCellFrame(PRInt32   aRowIndex,
                             PRInt32   aColIndex,
                             CellData& aData,
                             PRBool    aUseRowIfOverlap) const
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->GetCellFrame(rowIndex, aColIndex, aData, aUseRowIfOverlap);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  return nsnull;
}

nsColInfo* 
nsTableCellMap::GetColInfoAt(PRInt32 aColIndex)
{
  PRInt32 numColsToAdd = aColIndex + 1 - mCols.Count();
  if (numColsToAdd > 0) {
    AddColsAtEnd(numColsToAdd);  // XXX this could fail to add cols in theory
  }
  return (nsColInfo*)mCols.ElementAt(aColIndex);
}

PRInt32 
nsTableCellMap::GetRowCount() const
{
  PRInt32 numRows = 0;
  nsCellMap* map = mFirstMap;
  while (map) {
    numRows += map->GetRowCount();
    map = map->GetNextSibling();
  }
  return numRows;
}

CellData* 
nsTableCellMap::GetDataAt(PRInt32 aRowIndex, 
                          PRInt32 aColIndex) const
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->GetDataAt(rowIndex, aColIndex);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  return nsnull;
}

void 
nsTableCellMap::AddColsAtEnd(PRUint32 aNumCols)
{
  PRBool added;
  // XXX We really should have a way to say "make this voidarray at least
  // N entries long" to avoid reallocating N times.  On the other hand, the
  // number of likely allocations here isn't TOO gigantic, and we may not
  // know about many of them at a time.
  for (PRUint32 numX = 1; numX <= aNumCols; numX++) {
    nsColInfo* colInfo = new nsColInfo();
    if (colInfo) {
      added = mCols.AppendElement(colInfo);
      if (!added) {
        delete colInfo;
        NS_WARNING("Could not AppendElement");
      }
    }
    if (mBCInfo) {
      BCData* bcData = new BCData();
      if (bcData) {
        added = mBCInfo->mBottomBorders.AppendElement(bcData);
        if (!added) {
          delete bcData;
          NS_WARNING("Could not AppendElement");
        }
      }
    }
  }
}

void 
nsTableCellMap::RemoveColsAtEnd()
{
  // Remove the cols at the end which don't have originating cells or cells spanning 
  // into them. Only do this if the col was created as eColAnonymousCell  
  PRInt32 numCols = GetColCount();
  PRInt32 lastGoodColIndex = mTableFrame.GetIndexOfLastRealCol();
  for (PRInt32 colX = numCols - 1; (colX >= 0) && (colX > lastGoodColIndex); colX--) {
    nsColInfo* colInfo = (nsColInfo*)mCols.ElementAt(colX);
    if (colInfo) {
      if ((colInfo->mNumCellsOrig <= 0) && (colInfo->mNumCellsSpan <= 0))  {

        delete colInfo;
        mCols.RemoveElementAt(colX);

        if (mBCInfo) { 
          PRInt32 count = mBCInfo->mBottomBorders.Count();
          if (colX < count) {
            BCData* bcData = (BCData*)mBCInfo->mBottomBorders.ElementAt(colX);
            if (bcData) {
              delete bcData;
            }
            mBCInfo->mBottomBorders.RemoveElementAt(colX);
          }
        }
      }
      else break; // only remove until we encounter the 1st valid one
    }
    else {
      NS_ERROR("null entry in column info array");
      mCols.RemoveElementAt(colX);
    }
  }
}

void 
nsTableCellMap::ClearCols()
{
  PRInt32 numCols = GetColCount();
  for (PRInt32 colX = numCols - 1; (colX >= 0);colX--) {
    nsColInfo* colInfo = (nsColInfo*)mCols.ElementAt(colX);  
    delete colInfo;
    mCols.RemoveElementAt(colX);
    if (mBCInfo) { 
      PRInt32 count = mBCInfo->mBottomBorders.Count();
      if (colX < count) {
        BCData* bcData = (BCData*)mBCInfo->mBottomBorders.ElementAt(colX);
        if (bcData) {
              delete bcData;
        }
        mBCInfo->mBottomBorders.RemoveElementAt(colX);
      }
    }
  }
}
void
nsTableCellMap::InsertRows(nsTableRowGroupFrame& aParent,
                           nsVoidArray&          aRows,
                           PRInt32               aFirstRowIndex,
                           PRBool                aConsiderSpans,
                           nsRect&               aDamageArea)
{
  PRInt32 numNewRows = aRows.Count();
  if ((numNewRows <= 0) || (aFirstRowIndex < 0)) ABORT0(); 

  PRInt32 rowIndex = aFirstRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    nsTableRowGroupFrame* rg = cellMap->GetRowGroup();
    if (rg == &aParent) {
      cellMap->InsertRows(*this, aRows, rowIndex, aConsiderSpans, aDamageArea);
      aDamageArea.y = aFirstRowIndex;
      aDamageArea.height = PR_MAX(0, GetRowCount() - aFirstRowIndex);
#ifdef DEBUG_TABLE_CELLMAP 
      Dump("after InsertRows");
#endif
      if (mBCInfo) {
        BCData* bcData;
        PRInt32 count = mBCInfo->mRightBorders.Count();
        if (aFirstRowIndex < count) {
          for (PRInt32 rowX = aFirstRowIndex; rowX < aFirstRowIndex + numNewRows; rowX++) {
            bcData = new BCData(); if (!bcData) ABORT0();
            mBCInfo->mRightBorders.InsertElementAt(bcData, rowX);
          }
        }
        else {
          GetRightMostBorder(aFirstRowIndex); // this will create missing entries
          for (PRInt32 rowX = aFirstRowIndex + 1; rowX < aFirstRowIndex + numNewRows; rowX++) {
            bcData = new BCData(); if (!bcData) ABORT0();
            mBCInfo->mRightBorders.AppendElement(bcData);
          }
        }
      }
      return;
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }

  NS_ERROR("Attempt to insert row into wrong map.");
}

void
nsTableCellMap::RemoveRows(PRInt32         aFirstRowIndex,
                           PRInt32         aNumRowsToRemove,
                           PRBool          aConsiderSpans,
                           nsRect&         aDamageArea)
{
  PRInt32 rowIndex = aFirstRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      cellMap->RemoveRows(*this, rowIndex, aNumRowsToRemove, aConsiderSpans, aDamageArea);
      nsTableRowGroupFrame* rg = cellMap->GetRowGroup();
      aDamageArea.y += (rg) ? rg->GetStartRowIndex() : 0;
      aDamageArea.height = PR_MAX(0, GetRowCount() - aFirstRowIndex);
      if (mBCInfo) {
        BCData* bcData;
        for (PRInt32 rowX = aFirstRowIndex + aNumRowsToRemove - 1; rowX >= aFirstRowIndex; rowX--) {
          if (rowX < mBCInfo->mRightBorders.Count()) {
            bcData = (BCData*)mBCInfo->mRightBorders.ElementAt(rowX);
            if (bcData) {
              delete bcData;
            }
            mBCInfo->mRightBorders.RemoveElementAt(rowX);
          }
        }
      }
      break;
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
#ifdef DEBUG_TABLE_CELLMAP
  Dump("after RemoveRows");
#endif
}



CellData* 
nsTableCellMap::AppendCell(nsTableCellFrame& aCellFrame,
                           PRInt32           aRowIndex,
                           PRBool            aRebuildIfNecessary,
                           nsRect&           aDamageArea)
{
  NS_ASSERTION(&aCellFrame == aCellFrame.GetFirstInFlow(), "invalid call on continuing frame");
  nsIFrame* rgFrame = aCellFrame.GetParent(); // get the row
  if (!rgFrame) return 0;
  rgFrame = rgFrame->GetParent();   // get the row group
  if (!rgFrame) return 0;

  CellData* result = nsnull;
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowGroup() == rgFrame) {
      result = cellMap->AppendCell(*this, &aCellFrame, rowIndex, aRebuildIfNecessary, aDamageArea);
      nsTableRowGroupFrame* rg = cellMap->GetRowGroup();
      aDamageArea.y += (rg) ? rg->GetStartRowIndex() : 0;
      break;
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
#ifdef DEBUG_TABLE_CELLMAP
  Dump("after AppendCell");
#endif
  return result;
}


void 
nsTableCellMap::InsertCells(nsVoidArray&          aCellFrames,
                            PRInt32               aRowIndex,
                            PRInt32               aColIndexBefore,
                            nsRect&               aDamageArea)
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      cellMap->InsertCells(*this, aCellFrames, rowIndex, aColIndexBefore, aDamageArea);
      nsTableRowGroupFrame* rg = cellMap->GetRowGroup();
      aDamageArea.y += (rg) ? rg->GetStartRowIndex() : 0;
      aDamageArea.width = PR_MAX(0, GetColCount() - aColIndexBefore - 1);
      break;
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
#ifdef DEBUG_TABLE_CELLMAP
  Dump("after InsertCells");
#endif
}


void 
nsTableCellMap::RemoveCell(nsTableCellFrame* aCellFrame,
                           PRInt32           aRowIndex,
                           nsRect&           aDamageArea)
{
  if (!aCellFrame) ABORT0();
  NS_ASSERTION(aCellFrame == (nsTableCellFrame *)aCellFrame->GetFirstInFlow(),
               "invalid call on continuing frame");
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      cellMap->RemoveCell(*this, aCellFrame, rowIndex, aDamageArea);
      nsTableRowGroupFrame* rg = cellMap->GetRowGroup();
      aDamageArea.y += (rg) ? rg->GetStartRowIndex() : 0;
      PRInt32 colIndex;
      aCellFrame->GetColIndex(colIndex);
      aDamageArea.width = PR_MAX(0, GetColCount() - colIndex - 1);
#ifdef DEBUG_TABLE_CELLMAP
      Dump("after RemoveCell");
#endif
      return;
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  // if we reach this point - the cell did not get removed, the caller of this routine 
  // will delete the cell and the cellmap will probably hold a reference to 
  // the deleted cell which will cause a subsequent crash when this cell is 
  // referenced later
  NS_ERROR("nsTableCellMap::RemoveCell - could not remove cell");
}

void
SetDamageArea(PRInt32 aXOrigin,
              PRInt32 aYOrigin,
              PRInt32 aWidth,
              PRInt32 aHeight,
              nsRect& aDamageArea)
{
  aDamageArea.x      = aXOrigin;
  aDamageArea.y      = aYOrigin;
  aDamageArea.width  = PR_MAX(1, aWidth);
  aDamageArea.height = PR_MAX(1, aHeight);
}

void 
nsTableCellMap::RebuildConsideringCells(nsCellMap*      aCellMap,
                                        nsVoidArray*    aCellFrames,
                                        PRInt32         aRowIndex,
                                        PRInt32         aColIndex,
                                        PRBool          aInsert,
                                        nsRect&         aDamageArea)
{
  PRInt32 numOrigCols = GetColCount();
  ClearCols();
  nsCellMap* cellMap = mFirstMap;
  PRInt32 rowCount = 0;
  while (cellMap) {
    if (cellMap == aCellMap) {
      cellMap->RebuildConsideringCells(*this, numOrigCols, aCellFrames, aRowIndex, aColIndex, aInsert, aDamageArea);
      
    }
    else {
      cellMap->RebuildConsideringCells(*this, numOrigCols, nsnull, -1, 0, PR_FALSE, aDamageArea);
    }
    rowCount += cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  SetDamageArea(0, 0, GetColCount(), rowCount, aDamageArea);
}

void 
nsTableCellMap::RebuildConsideringRows(nsCellMap*      aCellMap,
                                       PRInt32         aStartRowIndex,
                                       nsVoidArray*    aRowsToInsert,
                                       PRInt32         aNumRowsToRemove,
                                       nsRect&         aDamageArea)
{
  NS_PRECONDITION(!aRowsToInsert || aNumRowsToRemove == 0,
                  "Can't handle both removing and inserting rows at once");
  
  PRInt32 numOrigCols = GetColCount();
  ClearCols();
  nsCellMap* cellMap = mFirstMap;
  PRInt32 rowCount = 0;
  while (cellMap) {
    if (cellMap == aCellMap) {
      cellMap->RebuildConsideringRows(*this, aStartRowIndex, aRowsToInsert, aNumRowsToRemove, aDamageArea);
    }
    else {
      cellMap->RebuildConsideringCells(*this, numOrigCols, nsnull, -1, 0, PR_FALSE, aDamageArea);
    }
    rowCount += cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  SetDamageArea(0, 0, GetColCount(), rowCount, aDamageArea);
}

PRInt32 
nsTableCellMap::GetNumCellsOriginatingInCol(PRInt32 aColIndex) const
{
  PRInt32 colCount = mCols.Count();
  if ((aColIndex >= 0) && (aColIndex < colCount)) {
    return ((nsColInfo *)mCols.ElementAt(aColIndex))->mNumCellsOrig;
  }
  else {
    NS_ERROR("nsCellMap::GetNumCellsOriginatingInCol - bad col index");
    return 0;
  }
}

#ifdef NS_DEBUG
void 
nsTableCellMap::Dump(char* aString) const
{
  if (aString) 
    printf("%s \n", aString);
  printf("***** START TABLE CELL MAP DUMP ***** %p\n", (void*)this);
  // output col info
  PRInt32 colCount = mCols.Count();
  printf ("cols array orig/span-> %p", (void*)this);
  for (PRInt32 colX = 0; colX < colCount; colX++) {
    nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
    printf ("%d=%d/%d ", colX, colInfo->mNumCellsOrig, colInfo->mNumCellsSpan);
  }
  printf(" cols in cache %d\n", mTableFrame.GetColCache().Count());
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    cellMap->Dump(nsnull != mBCInfo);
    cellMap = cellMap->GetNextSibling();
  }
  if (nsnull != mBCInfo) {
    printf("***** bottom borders *****\n");
    nscoord       size;
    BCBorderOwner owner;
    PRUint8       side;
    PRBool        segStart;
    PRPackedBool  bevel;  
    PRInt32       colIndex;    
    PRInt32 numCols = mBCInfo->mBottomBorders.Count();
    for (PRInt32 i = 0; i <= 2; i++) {
     
      printf("\n          ");
      for (colIndex = 0; colIndex < numCols; colIndex++) {
        BCData* cd = (BCData*)mBCInfo->mBottomBorders.ElementAt(colIndex);;
        if (cd) {
          if (0 == i) {
            size = cd->GetTopEdge(owner, segStart);
            printf("t=%d%X%d ", size, owner, segStart);
          }
          else if (1 == i) {
            size = cd->GetLeftEdge(owner, segStart);
            printf("l=%d%X%d ", size, owner, segStart);
          }
          else {
            size = cd->GetCorner(side, bevel);
            printf("c=%d%X%d ", size, side, bevel);
          }
        }
      }
      BCData* cd = &mBCInfo->mLowerRightCorner;
      if (cd) {
        if (0 == i) {
           size = cd->GetTopEdge(owner, segStart);
           printf("t=%d%X%d ", size, owner, segStart);
        }
        else if (1 == i) {
          size = cd->GetLeftEdge(owner, segStart);
          printf("l=%d%X%d ", size, owner, segStart);
        }
        else {
          size = cd->GetCorner(side, bevel);
          printf("c=%d%X%d ", size, side, bevel);
        }
      }
    }
    printf("\n");
  }
  printf("***** END TABLE CELL MAP DUMP *****\n");
}
#endif

nsTableCellFrame* 
nsTableCellMap::GetCellInfoAt(PRInt32  aRowIndex, 
                              PRInt32  aColIndex, 
                              PRBool*  aOriginates, 
                              PRInt32* aColSpan) const
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      return cellMap->GetCellInfoAt(*this, rowIndex, aColIndex, aOriginates, aColSpan);
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  return nsnull;
}
  

PRBool nsTableCellMap::RowIsSpannedInto(PRInt32 aRowIndex,
                                        PRInt32 aNumEffCols) const
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      return cellMap->RowIsSpannedInto(rowIndex, aNumEffCols);
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  return PR_FALSE;
}

PRBool nsTableCellMap::RowHasSpanningCells(PRInt32 aRowIndex,
                                           PRInt32 aNumEffCols) const
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      return cellMap->RowHasSpanningCells(rowIndex, aNumEffCols);
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  return PR_FALSE;
}

PRBool nsTableCellMap::ColIsSpannedInto(PRInt32 aColIndex) const
{
  PRBool result = PR_FALSE;

  PRInt32 colCount = mCols.Count();
  if ((aColIndex >= 0) && (aColIndex < colCount)) {
    result = (PRBool) ((nsColInfo *)mCols.ElementAt(aColIndex))->mNumCellsSpan;
  }
  return result;
}

PRBool nsTableCellMap::ColHasSpanningCells(PRInt32 aColIndex) const
{
  NS_PRECONDITION (aColIndex < GetColCount(), "bad col index arg");
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->ColHasSpanningCells(aColIndex)) {
      return PR_TRUE;
    }
    cellMap = cellMap->GetNextSibling();
  }
  return PR_FALSE;
}

void nsTableCellMap::ExpandZeroColSpans()
{
  mTableFrame.SetNeedColSpanExpansion(PR_FALSE); // mark the work done
  mTableFrame.SetHasZeroColSpans(PR_FALSE); // reset the bit, if there is a
                                            // zerospan it will be set again.
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    cellMap->ExpandZeroColSpans(*this);
    cellMap = cellMap->GetNextSibling();
  }
}
BCData* 
nsTableCellMap::GetBCData(PRUint8     aSide, 
                          nsCellMap&  aCellMap,
                          PRUint32    aRowIndex, 
                          PRUint32    aColIndex,
                          PRBool      aIsLowerRight)
{
  if (!mBCInfo || aIsLowerRight) ABORT1(nsnull);

  BCCellData* cellData;
  BCData* bcData = nsnull;

  switch(aSide) {
  case NS_SIDE_BOTTOM:
    aRowIndex++;
  case NS_SIDE_TOP:
    cellData = (BCCellData*)aCellMap.GetDataAt(aRowIndex, aColIndex);
    if (cellData) {
      bcData = &cellData->mData;
    }
    else {
      NS_ASSERTION(aSide == NS_SIDE_BOTTOM, "program error");
      // try the next row group
      nsCellMap* cellMap = aCellMap.GetNextSibling();
      if (cellMap) {
        cellData = (BCCellData*)cellMap->GetDataAt(0, aColIndex);
        if (cellData) {
          bcData = &cellData->mData;
        }
        else {
          bcData = GetBottomMostBorder(aColIndex);
        }
      }
    }
    break;
  case NS_SIDE_RIGHT:
    aColIndex++;
  case NS_SIDE_LEFT:
    cellData = (BCCellData*)aCellMap.GetDataAt(aRowIndex, aColIndex);
    if (cellData) {
      bcData = &cellData->mData;
    }
    else {
      NS_ASSERTION(aSide == NS_SIDE_RIGHT, "program error");
      bcData = GetRightMostBorder(aRowIndex);
    }
    break;
  }
  return bcData;
}

// store the aSide border segment at coord = (aRowIndex, aColIndex). For top/left, store
// the info at coord. For bottom/left store it at the adjacent location so that it is 
// top/left at that location. If the new location is at the right or bottom edge of the 
// table, then store it one of the special arrays (right most borders, bottom most borders).  
void 
nsTableCellMap::SetBCBorderEdge(PRUint8       aSide, 
                                nsCellMap&    aCellMap,
                                PRUint32      aCellMapStart,
                                PRUint32      aRowIndex, 
                                PRUint32      aColIndex,
                                PRUint32      aLength,
                                BCBorderOwner aOwner,
                                nscoord       aSize,
                                PRBool        aChanged)
{
  if (!mBCInfo) ABORT0();
  
  BCCellData* cellData;
  PRInt32 lastIndex, xIndex, yIndex;
  PRInt32 xPos = aColIndex;
  PRInt32 yPos = aRowIndex;
  PRInt32 rgYPos = aRowIndex - aCellMapStart;
  PRBool changed;

  switch(aSide) {
  case NS_SIDE_BOTTOM:
    rgYPos++;
    yPos++;
  case NS_SIDE_TOP:
    lastIndex = xPos + aLength - 1;
    for (xIndex = xPos; xIndex <= lastIndex; xIndex++) {
      changed = aChanged && (xIndex == xPos);
      BCData* bcData = nsnull;
      cellData = (BCCellData*)aCellMap.GetDataAt(rgYPos, xIndex);
      if (!cellData) {
        PRInt32 numRgRows = aCellMap.GetRowCount();
        if (yPos < numRgRows) { // add a dead cell data
          nsRect damageArea;
          cellData = (BCCellData*)aCellMap.AppendCell(*this, nsnull, rgYPos, PR_FALSE, damageArea); if (!cellData) ABORT0();
        }
        else {
          NS_ASSERTION(aSide == NS_SIDE_BOTTOM, "program error");
          // try the next non empty row group
          nsCellMap* cellMap = aCellMap.GetNextSibling();
          while (cellMap && (0 == cellMap->GetRowCount())) {
            cellMap = cellMap->GetNextSibling();
          }
          if (cellMap) {
            cellData = (BCCellData*)cellMap->GetDataAt(0, xIndex);
            if (!cellData) { // add a dead cell
              nsRect damageArea;
              cellData = (BCCellData*)cellMap->AppendCell(*this, nsnull, 0, PR_FALSE, damageArea); 
            }
          }
          else { // must be at the end of the table
            bcData = GetBottomMostBorder(xIndex);
          }
        }
      }
      if (!bcData && cellData) {
        bcData = &cellData->mData;
      }
      if (bcData) {
        bcData->SetTopEdge(aOwner, aSize, changed);
      }
      else NS_ERROR("Cellmap: Top edge not found");
    }
    break;
  case NS_SIDE_RIGHT:
    xPos++;
  case NS_SIDE_LEFT:
    // since top, bottom borders were set, there should already be a cellData entry
    lastIndex = rgYPos + aLength - 1;
    for (yIndex = rgYPos; yIndex <= lastIndex; yIndex++) {
      changed = aChanged && (yIndex == rgYPos);
      cellData = (BCCellData*)aCellMap.GetDataAt(yIndex, xPos);
      if (cellData) {
        cellData->mData.SetLeftEdge(aOwner, aSize, changed);
      }
      else {
        NS_ASSERTION(aSide == NS_SIDE_RIGHT, "program error");
        BCData* bcData = GetRightMostBorder(yIndex + aCellMapStart);
        if (bcData) {
          bcData->SetLeftEdge(aOwner, aSize, changed);
        }
        else NS_ERROR("Cellmap: Left edge not found");
      }
    }
    break;
  }
}

// store corner info (aOwner, aSubSize, aBevel). For aCorner = eTopLeft, store the info at 
// (aRowIndex, aColIndex). For eTopRight, store it in the entry to the right where
// it would be top left. For eBottomRight, store it in the entry to the bottom. etc. 
void 
nsTableCellMap::SetBCBorderCorner(Corner      aCorner,
                                  nsCellMap&  aCellMap,
                                  PRUint32    aCellMapStart,
                                  PRUint32    aRowIndex, 
                                  PRUint32    aColIndex,
                                  PRUint8     aOwner,
                                  nscoord     aSubSize,
                                  PRBool      aBevel,
                                  PRBool      aIsBottomRight)
{
  if (!mBCInfo) ABORT0();

  if (aIsBottomRight) { 
    mBCInfo->mLowerRightCorner.SetCorner(aSubSize, aOwner, aBevel);
    return;
  }

  PRInt32 xPos = aColIndex;
  PRInt32 yPos = aRowIndex;
  PRInt32 rgYPos = aRowIndex - aCellMapStart;

  if (eTopRight == aCorner) {
    xPos++;
  }
  else if (eBottomRight == aCorner) {
    xPos++;
    rgYPos++;
    yPos++;
  }
  else if (eBottomLeft == aCorner) {
    rgYPos++;
    yPos++;
  }

  BCCellData* cellData = nsnull;
  BCData*     bcData   = nsnull;
  if (GetColCount() <= xPos) {
    NS_ASSERTION(xPos == GetColCount(), "program error");
    // at the right edge of the table as we checked the corner before
    NS_ASSERTION(!aIsBottomRight, "should be handled before");
    bcData = GetRightMostBorder(yPos);
  }
  else {
    cellData = (BCCellData*)aCellMap.GetDataAt(rgYPos, xPos);
    if (!cellData) {
      PRInt32 numRgRows = aCellMap.GetRowCount();
      if (yPos < numRgRows) { // add a dead cell data
        nsRect damageArea;
        cellData = (BCCellData*)aCellMap.AppendCell(*this, nsnull, rgYPos, PR_FALSE, damageArea); 
      }
      else {
        // try the next non empty row group
        nsCellMap* cellMap = aCellMap.GetNextSibling();
        while (cellMap && (0 == cellMap->GetRowCount())) {
          cellMap = cellMap->GetNextSibling();
        }
        if (cellMap) {
          cellData = (BCCellData*)cellMap->GetDataAt(0, xPos);
          if (!cellData) { // add a dead cell
            nsRect damageArea;
            cellData = (BCCellData*)cellMap->AppendCell(*this, nsnull, 0, PR_FALSE, damageArea); 
          }
        }
        else { // must be a the bottom of the table
          bcData = GetBottomMostBorder(xPos);
        }
      }
    }
  }
  if (!bcData && cellData) {
    bcData = &cellData->mData;
  }
  if (bcData) {
    bcData->SetCorner(aSubSize, aOwner, aBevel);
  }
  else NS_ERROR("program error: Corner not found");
}

nsCellMap::nsCellMap(nsTableRowGroupFrame& aRowGroup, PRBool aIsBC)
  : mRows(8), mContentRowCount(0), mRowGroupFrame(&aRowGroup),
    mNextSibling(nsnull), mIsBC(aIsBC),
    mPresContext(aRowGroup.GetPresContext())
{
  MOZ_COUNT_CTOR(nsCellMap);
  NS_ASSERTION(mPresContext, "Must have prescontext");
}

nsCellMap::~nsCellMap()
{
  MOZ_COUNT_DTOR(nsCellMap);
  
  PRUint32 mapRowCount = mRows.Length();
  for (PRUint32 rowX = 0; rowX < mapRowCount; rowX++) {
    CellDataArray &row = mRows[rowX];
    PRUint32 colCount = row.Length();
    for (PRUint32 colX = 0; colX < colCount; colX++) {
      DestroyCellData(row[colX]);
    }
  }
}

nsTableCellFrame* 
nsCellMap::GetCellFrame(PRInt32   aRowIndexIn,
                        PRInt32   aColIndexIn,
                        CellData& aData,
                        PRBool    aUseRowIfOverlap) const
{
  PRInt32 rowIndex = aRowIndexIn - aData.GetRowSpanOffset();
  PRInt32 colIndex = aColIndexIn - aData.GetColSpanOffset();
  if (aData.IsOverlap()) {
    if (aUseRowIfOverlap) {
      colIndex = aColIndexIn;
    }
    else {
      rowIndex = aRowIndexIn;
    }
  }

  CellData* data =
    mRows.SafeElementAt(rowIndex, sEmptyRow).SafeElementAt(colIndex);
  if (data) {
    return data->GetCellFrame();
  }
  return nsnull;
}

PRBool nsCellMap::Grow(nsTableCellMap& aMap,
                       PRInt32         aNumRows,
                       PRInt32         aRowIndex)
{
  NS_ASSERTION(aNumRows >= 1, "Why are we calling this?");

  // Get the number of cols we want to use for preallocating the row arrays.
  PRInt32 numCols = aMap.GetColCount();
  if (numCols == 0) {
    numCols = 4;
  }
  PRUint32 startRowIndex = (aRowIndex >= 0) ? aRowIndex : mRows.Length();
  NS_ASSERTION(startRowIndex <= mRows.Length(), "Missing grow call inbetween");

  return mRows.InsertElementsAt(startRowIndex, aNumRows, numCols) != nsnull;
}

void nsCellMap::GrowRow(CellDataArray& aRow,
                        PRInt32        aNumCols)
                     
{
  // Have to have the cast to get the template to do the right thing.
  aRow.InsertElementsAt(aRow.Length(), aNumCols, (CellData*)nsnull);
}

void
nsCellMap::InsertRows(nsTableCellMap& aMap,
                      nsVoidArray&    aRows,
                      PRInt32         aFirstRowIndex,
                      PRBool          aConsiderSpans,
                      nsRect&         aDamageArea)
{
  PRInt32 numCols = aMap.GetColCount();
  NS_ASSERTION(aFirstRowIndex >= 0, "nsCellMap::InsertRows called with negative rowIndex");
  if (PRUint32(aFirstRowIndex) > mRows.Length()) {
    // create (aFirstRowIndex - mRows.Length()) empty rows up to aFirstRowIndex
    PRInt32 numEmptyRows = aFirstRowIndex - mRows.Length();
    if (!Grow(aMap, numEmptyRows)) {
      return;
    }
  }
  // update mContentRowCount, since non-empty rows will be added
  mContentRowCount = PR_MAX(aFirstRowIndex, mContentRowCount);
 
  if (!aConsiderSpans) {
    ExpandWithRows(aMap, aRows, aFirstRowIndex, aDamageArea);
    return;
  }

  // if any cells span into or out of the row being inserted, then rebuild
  PRBool spansCauseRebuild = CellsSpanInOrOut(aFirstRowIndex,
                                              aFirstRowIndex, 0, numCols - 1);

  // if any of the new cells span out of the new rows being added, then rebuild
  // XXX it would be better to only rebuild the portion of the map that follows the new rows
  if (!spansCauseRebuild && (PRUint32(aFirstRowIndex) < mRows.Length())) {
    spansCauseRebuild = CellsSpanOut(aRows);
  }

  if (spansCauseRebuild) {
    aMap.RebuildConsideringRows(this, aFirstRowIndex, &aRows, 0, aDamageArea);
  }
  else {
    ExpandWithRows(aMap, aRows, aFirstRowIndex, aDamageArea);
  }
}

void
nsCellMap::RemoveRows(nsTableCellMap& aMap,
                      PRInt32         aFirstRowIndex,
                      PRInt32         aNumRowsToRemove,
                      PRBool          aConsiderSpans,
                      nsRect&         aDamageArea)
{
  PRInt32 numRows = mRows.Length();
  PRInt32 numCols = aMap.GetColCount();

  if (aFirstRowIndex >= numRows) {
    // reduce the content based row count based on the function arguments
    // as they are known to be real rows even if the cell map did not create
    // rows for them before.
    mContentRowCount -= aNumRowsToRemove;
    return;
  }
  if (!aConsiderSpans) {
    ShrinkWithoutRows(aMap, aFirstRowIndex, aNumRowsToRemove, aDamageArea);
    return;
  }
  PRInt32 endRowIndex = aFirstRowIndex + aNumRowsToRemove - 1;
  if (endRowIndex >= numRows) {
    NS_ERROR("nsCellMap::RemoveRows tried to remove too many rows");
    endRowIndex = numRows - 1;
  }
  PRBool spansCauseRebuild = CellsSpanInOrOut(aFirstRowIndex, endRowIndex,
                                              0, numCols - 1);

  if (spansCauseRebuild) {
    aMap.RebuildConsideringRows(this, aFirstRowIndex, nsnull, aNumRowsToRemove, aDamageArea);
  }
  else {
    ShrinkWithoutRows(aMap, aFirstRowIndex, aNumRowsToRemove, aDamageArea);
  }
}




CellData* 
nsCellMap::AppendCell(nsTableCellMap&   aMap,
                      nsTableCellFrame* aCellFrame, 
                      PRInt32           aRowIndex,
                      PRBool            aRebuildIfNecessary,
                      nsRect&           aDamageArea,
                      PRInt32*          aColToBeginSearch)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  PRInt32 origNumMapRows = mRows.Length();
  PRInt32 origNumCols = aMap.GetColCount();
  PRBool  zeroRowSpan = PR_FALSE;
  PRInt32 rowSpan = (aCellFrame) ? GetRowSpanForNewCell(aCellFrame, aRowIndex,
                                                        zeroRowSpan) : 1;
  // add new rows if necessary
  PRInt32 endRowIndex = aRowIndex + rowSpan - 1;
  if (endRowIndex >= origNumMapRows) {
    // XXXbz handle allocation failures?
    Grow(aMap, 1 + endRowIndex - origNumMapRows);
  }

  // get the first null or dead CellData in the desired row. It will equal origNumCols if there are none
  CellData* origData = nsnull;
  PRInt32 startColIndex = 0;
  if (aColToBeginSearch)
    startColIndex = *aColToBeginSearch;
  for (; startColIndex < origNumCols; startColIndex++) {
    CellData* data = GetDataAt(aRowIndex, startColIndex);
    if (!data) 
      break;
    if (data->IsDead()) {
      origData = data;
      break; 
    }
    if (data->IsZeroColSpan() ) {
      // appending a cell collapses zerospans.
      CollapseZeroColSpan(aMap, data, aRowIndex, startColIndex);
      // ask again for the data as it should be modified
      origData = GetDataAt(aRowIndex, startColIndex);
      NS_ASSERTION(origData->IsDead(),
                   "The cellposition should have been cleared");
      break;
    }
  }
  // We found the place to append the cell, when the next cell is appended 
  // the next search does not need to duplicate the search but can start 
  // just at the next cell.
  if (aColToBeginSearch)
    *aColToBeginSearch =  startColIndex + 1; 
  
  PRBool  zeroColSpan = PR_FALSE;
  PRInt32 colSpan = (aCellFrame) ?
                    GetColSpanForNewCell(*aCellFrame, zeroColSpan) : 1;
  if (zeroColSpan) {
    aMap.mTableFrame.SetHasZeroColSpans(PR_TRUE);
    aMap.mTableFrame.SetNeedColSpanExpansion(PR_TRUE);
  }

  // if the new cell could potentially span into other rows and collide with 
  // originating cells there, we will play it safe and just rebuild the map
  if (aRebuildIfNecessary && (aRowIndex < mContentRowCount - 1) && (rowSpan > 1)) {
    nsAutoVoidArray newCellArray;
    newCellArray.AppendElement(aCellFrame);
    aMap.RebuildConsideringCells(this, &newCellArray, aRowIndex, startColIndex, PR_TRUE, aDamageArea);
    return origData;
  }
  mContentRowCount = PR_MAX(mContentRowCount, aRowIndex + 1);

  // add new cols to the table map if necessary
  PRInt32 endColIndex = startColIndex + colSpan - 1;
  if (endColIndex >= origNumCols) {
    NS_ASSERTION(aCellFrame, "dead cells should not require new columns");
    aMap.AddColsAtEnd(1 + endColIndex - origNumCols);
  }

  // Setup CellData for this cell
  if (origData) {
    NS_ASSERTION(origData->IsDead(), "replacing a non dead cell is a memory leak");
    if (aCellFrame) { // do nothing to replace a dead cell with a dead cell
      origData->Init(aCellFrame);
      // we are replacing a dead cell, increase the number of cells 
      // originating at this column
      nsColInfo* colInfo = aMap.GetColInfoAt(startColIndex);
      NS_ASSERTION(colInfo, "access to a non existing column");
      if (colInfo) { 
        colInfo->mNumCellsOrig++;
      }
    }
  }
  else {
    origData = AllocCellData(aCellFrame);
    if (!origData) ABORT1(origData);
    SetDataAt(aMap, *origData, aRowIndex, startColIndex);
  }

  SetDamageArea(startColIndex, aRowIndex, 1 + endColIndex - startColIndex, 1 + endRowIndex - aRowIndex, aDamageArea); 

  if (!aCellFrame) {
    return origData;
  }

  // initialize the cell frame
  aCellFrame->SetColIndex(startColIndex);

  // Create CellData objects for the rows that this cell spans. Set
  // their mOrigCell to nsnull and their mSpanData to point to data.
  for (PRInt32 rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    // The row at rowX will need to have at least endColIndex columns
    mRows[rowX].SetCapacity(endColIndex);
    for (PRInt32 colX = startColIndex; colX <= endColIndex; colX++) {
      if ((rowX != aRowIndex) || (colX != startColIndex)) { // skip orig cell data done above
        CellData* cellData = GetDataAt(rowX, colX);
        if (cellData) {
          if (cellData->IsOrig()) {
            NS_ERROR("cannot overlap originating cell");
            continue;
          }
          if (rowX > aRowIndex) { // row spanning into cell
            if (cellData->IsRowSpan()) {
              // do nothing, this can be caused by rowspan which is overlapped
              // by a another cell with a rowspan and a colspan
            }
            else {
              cellData->SetRowSpanOffset(rowX - aRowIndex);
              if (zeroRowSpan) {
                cellData->SetZeroRowSpan(PR_TRUE);
              }
            }
          }
          if (colX > startColIndex) { // col spanning into cell
            if (!cellData->IsColSpan()) { 
              if (cellData->IsRowSpan()) {
                cellData->SetOverlap(PR_TRUE);
              }
              cellData->SetColSpanOffset(colX - startColIndex);
              if (zeroColSpan) {
                cellData->SetZeroColSpan(PR_TRUE);
              }
              
              nsColInfo* colInfo = aMap.GetColInfoAt(colX);
              colInfo->mNumCellsSpan++;
            }
          }
        }
        else { 
          cellData = AllocCellData(nsnull);
          if (!cellData) return origData;
          if (rowX > aRowIndex) {
            cellData->SetRowSpanOffset(rowX - aRowIndex);
            if (zeroRowSpan) {
              cellData->SetZeroRowSpan(PR_TRUE);
            }
          }
          if (colX > startColIndex) {
            cellData->SetColSpanOffset(colX - startColIndex);
            if (zeroColSpan) {
              cellData->SetZeroColSpan(PR_TRUE);
            }
          }
          SetDataAt(aMap, *cellData, rowX, colX);
        }
      }
    }
  }
#ifdef DEBUG_TABLE_CELLMAP
  printf("appended cell=%p row=%d \n", aCellFrame, aRowIndex);
  aMap.Dump();
#endif
  return origData;
}

void nsCellMap::CollapseZeroColSpan(nsTableCellMap& aMap,
                                    CellData*       aOrigData,
                                    PRInt32         aRowIndex,
                                    PRInt32         aColIndex)
{
  // if after a colspan = 0 cell another cell is appended in a row the html 4
  // spec is already violated. In principle one should then append the cell
  // after the last column but then the zero spanning cell would also have
  // to grow. The only plausible way to break this cycle is ignore the zero
  // colspan and reset the cell to colspan = 1.

  NS_ASSERTION(aOrigData && aOrigData->IsZeroColSpan(),
               "zero colspan should have been passed");
  // find the originating cellframe
  nsTableCellFrame* cell = GetCellFrame(aRowIndex, aColIndex, *aOrigData, PR_TRUE);
  NS_ASSERTION(cell, "originating cell not found");
  
  // find the clearing region
  PRInt32 startRowIndex = aRowIndex - aOrigData->GetRowSpanOffset();
  PRBool  zeroSpan;
  PRInt32 rowSpan = GetRowSpanForNewCell(cell, startRowIndex, zeroSpan);
  PRInt32 endRowIndex = startRowIndex + rowSpan;

  PRInt32 origColIndex = aColIndex - aOrigData->GetColSpanOffset();
  PRInt32 endColIndex = origColIndex +
                        GetEffectiveColSpan(aMap, startRowIndex,
                                            origColIndex, zeroSpan);
  for (PRInt32 colX = origColIndex +1; colX < endColIndex; colX++) {
    // Start the collapse just after the originating cell, since
    // we're basically making the originating cell act as if it
    // has colspan="1".
    nsColInfo* colInfo = aMap.GetColInfoAt(colX);
    colInfo->mNumCellsSpan -= rowSpan;
    
    for (PRInt32 rowX = startRowIndex; rowX < endRowIndex; rowX++)
    {
      CellData* data = mRows[rowX][colX];
      NS_ASSERTION(data->IsZeroColSpan(),
                   "Overwriting previous data - memory leak");
      data->Init(nsnull); // mark the cell as a dead cell.
    }
  }
}

PRBool nsCellMap::CellsSpanOut(nsVoidArray&    aRows) const
{ 
  PRInt32 numNewRows = aRows.Count();
  for (PRInt32 rowX = 0; rowX < numNewRows; rowX++) {
    nsIFrame* rowFrame = (nsIFrame *) aRows.ElementAt(rowX);
    nsIFrame* cellFrame = rowFrame->GetFirstChild(nsnull);
    while (cellFrame) {
      if (IS_TABLE_CELL(cellFrame->GetType())) {
        PRBool zeroSpan;
        PRInt32 rowSpan = GetRowSpanForNewCell((nsTableCellFrame*) cellFrame,
                                               rowX, zeroSpan);
        if (rowX + rowSpan > numNewRows) {
          return PR_TRUE;
        }
      }
      cellFrame = cellFrame->GetNextSibling();
    }
  }
  return PR_FALSE;
}
    
// return PR_TRUE if any cells have rows spans into or out of the region 
// defined by the row and col indices or any cells have colspans into the region
PRBool nsCellMap::CellsSpanInOrOut(PRInt32 aStartRowIndex,
                                   PRInt32 aEndRowIndex,
                                   PRInt32 aStartColIndex,
                                   PRInt32 aEndColIndex) const
{
  /*
   * this routine will watch the cells adjacent to the region or at the edge
   * they are marked with *. The routine will verify whether they span in or
   * are spanned out.
   *
   *                           startCol          endCol
   *             r1c1   r1c2   r1c3      r1c4    r1c5    r1rc6  r1c7
   *  startrow   r2c1   r2c2  *r2c3     *r2c4   *r2c5   *r2rc6  r2c7
   *  endrow     r3c1   r3c2  *r3c3      r3c4    r3c5   *r3rc6  r3c7
   *             r4c1   r4c2  *r4c3     *r4c4   *r4c5    r4rc6  r4c7
   *             r5c1   r5c2   r5c3      r5c4    r5c5    r5rc6  r5c7  
   */

  PRInt32 numRows = mRows.Length(); // use the cellmap rows to determine the 
                                    // current cellmap extent.
  for (PRInt32 colX = aStartColIndex; colX <= aEndColIndex; colX++) {
    CellData* cellData;
    if (aStartRowIndex > 0) {
      cellData = GetDataAt(aStartRowIndex, colX);
      if (cellData && (cellData->IsRowSpan())) {
        return PR_TRUE; // there is a row span into the region
      }
    }
    if (aEndRowIndex < numRows - 1) { // is there anything below aEndRowIndex
      cellData = GetDataAt(aEndRowIndex + 1, colX);
      if ((cellData) && (cellData->IsRowSpan())) {
        return PR_TRUE; // there is a row span out of the region
      }
    }
    else {
      cellData = GetDataAt(aEndRowIndex, colX);
      if ((cellData) && (cellData->IsRowSpan()) && (mContentRowCount < numRows)) {
        return PR_TRUE; // this cell might be the cause of a dead row
      }
    }
  }
  if (aStartColIndex > 0) {
    for (PRInt32 rowX = aStartRowIndex; rowX <= aEndRowIndex; rowX++) {
      CellData* cellData = GetDataAt(rowX, aStartColIndex);
      if (cellData && (cellData->IsColSpan())) {
        return PR_TRUE; // there is a col span into the region
      }
      cellData = GetDataAt(rowX, aEndColIndex + 1);
      if (cellData && (cellData->IsColSpan())) {
        return PR_TRUE; // there is a col span out of the region
      }
    }
  }
  return PR_FALSE;
}

void nsCellMap::InsertCells(nsTableCellMap& aMap,
                            nsVoidArray&    aCellFrames, 
                            PRInt32         aRowIndex,
                            PRInt32         aColIndexBefore,
                            nsRect&         aDamageArea)
{
  if (aCellFrames.Count() == 0) return;
  NS_ASSERTION(aColIndexBefore >= -1, "index out of range");
  PRInt32 numCols = aMap.GetColCount();
  if (aColIndexBefore >= numCols) {
    NS_ERROR("Inserting instead of appending cells indicates a serious cellmap error");
    aColIndexBefore = numCols - 1;
  }

  // get the starting col index of the 1st new cells 
  PRInt32 startColIndex;
  for (startColIndex = aColIndexBefore + 1; startColIndex < numCols; startColIndex++) {
    CellData* data = GetDataAt(aRowIndex, startColIndex);
    if (!data || data->IsOrig() || data->IsDead()) { // stop unless it is a span
      break; 
    }
  }

  // record whether inserted cells are going to cause complications due 
  // to existing row spans, col spans or table sizing. 
  PRBool spansCauseRebuild = PR_FALSE;

  // check that all cells have the same row span
  PRInt32 numNewCells = aCellFrames.Count();
  PRBool zeroRowSpan = PR_FALSE;
  PRInt32 rowSpan = 0;
  for (PRInt32 cellX = 0; cellX < numNewCells; cellX++) {
    nsTableCellFrame* cell = (nsTableCellFrame*) aCellFrames.ElementAt(cellX);
    PRInt32 rowSpan2 = GetRowSpanForNewCell(cell, aRowIndex, zeroRowSpan);
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
    if (mRows.Length() < PRUint32(aRowIndex + rowSpan)) {
      spansCauseRebuild = PR_TRUE;
    }
  }

  if (!spansCauseRebuild) {
    spansCauseRebuild = CellsSpanInOrOut(aRowIndex, aRowIndex + rowSpan - 1,
                                         startColIndex, numCols - 1);
  }

  if (spansCauseRebuild) {
    aMap.RebuildConsideringCells(this, &aCellFrames, aRowIndex, startColIndex, PR_TRUE, aDamageArea);
  }
  else {
    ExpandWithCells(aMap, aCellFrames, aRowIndex, startColIndex, rowSpan, zeroRowSpan, aDamageArea);
  }
}
 
void
nsCellMap::ExpandWithRows(nsTableCellMap& aMap,
                          nsVoidArray&    aRowFrames,
                          PRInt32         aStartRowIndexIn,
                          nsRect&         aDamageArea)
{
  PRInt32 startRowIndex = (aStartRowIndexIn >= 0) ? aStartRowIndexIn : 0;
  NS_ASSERTION(PRUint32(startRowIndex) <= mRows.Length(), "caller should have grown cellmap before");

  PRInt32 numNewRows  = aRowFrames.Count();
  mContentRowCount += numNewRows;

  PRInt32 endRowIndex = startRowIndex + numNewRows - 1;
  
  // shift the rows after startRowIndex down and insert empty rows that will
  // be filled via the AppendCell call below
  if (!Grow(aMap, numNewRows, startRowIndex)) {
    return;
  }
  

  PRInt32 newRowIndex = 0;
  for (PRInt32 rowX = startRowIndex; rowX <= endRowIndex; rowX++) {
    nsTableRowFrame* rFrame = (nsTableRowFrame *)aRowFrames.ElementAt(newRowIndex);
    // append cells 
    nsIFrame* cFrame = rFrame->GetFirstChild(nsnull);
    PRInt32 colIndex = 0;
    while (cFrame) {
      if (IS_TABLE_CELL(cFrame->GetType())) {
        AppendCell(aMap, (nsTableCellFrame *)cFrame, rowX, PR_FALSE, aDamageArea, &colIndex);
      }
      cFrame = cFrame->GetNextSibling();
    }
    newRowIndex++;
  }

  SetDamageArea(0, startRowIndex, aMap.GetColCount(), 1 + endRowIndex - startRowIndex, aDamageArea); 
}

void nsCellMap::ExpandWithCells(nsTableCellMap& aMap,
                                nsVoidArray&    aCellFrames,
                                PRInt32         aRowIndex,
                                PRInt32         aColIndex,
                                PRInt32         aRowSpan, // same for all cells
                                PRBool          aRowSpanIsZero,
                                nsRect&         aDamageArea)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  PRInt32 endRowIndex = aRowIndex + aRowSpan - 1;
  PRInt32 startColIndex = aColIndex;
  PRInt32 endColIndex = aColIndex;
  PRInt32 numCells = aCellFrames.Count();
  PRInt32 totalColSpan = 0;

  // add cellData entries for the space taken up by the new cells
  for (PRInt32 cellX = 0; cellX < numCells; cellX++) {
    nsTableCellFrame* cellFrame = (nsTableCellFrame*) aCellFrames.ElementAt(cellX);
    CellData* origData = AllocCellData(cellFrame); // the originating cell
    if (!origData) return;

    // set the starting and ending col index for the new cell
    PRBool zeroColSpan = PR_FALSE;
    PRInt32 colSpan = GetColSpanForNewCell(*cellFrame, zeroColSpan);
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
      CellDataArray& row = mRows[rowX];
      // Pre-allocate all the cells we'll need in this array, setting
      // them to null.
      // Have to have the cast to get the template to do the right thing.
      PRUint32 insertionIndex = row.Length();
      if (insertionIndex > aColIndex) {
        insertionIndex = aColIndex;
      }
      if (!row.InsertElementsAt(insertionIndex, endColIndex - insertionIndex + 1,
                                (CellData*)nsnull) &&
          rowX == aRowIndex) {
        // Failed to insert the slots, and this is the very first row.  That
        // means that we need to clean up |origData| before returning, since
        // the cellmap doesn't own it yet.
        DestroyCellData(origData);
        return;
      }
      
      for (PRInt32 colX = aColIndex; colX <= endColIndex; colX++) {
        CellData* data = origData;
        if ((rowX != aRowIndex) || (colX != startColIndex)) {
          data = AllocCellData(nsnull);
          if (!data) return;
          if (rowX > aRowIndex) {
            data->SetRowSpanOffset(rowX - aRowIndex);
            if (aRowSpanIsZero) {
              data->SetZeroRowSpan(PR_TRUE);
            }
          }
          if (colX > startColIndex) {
            data->SetColSpanOffset(colX - startColIndex);
            if (zeroColSpan) {
              data->SetZeroColSpan(PR_TRUE);
            }
          }
        }
        SetDataAt(aMap, *data, rowX, colX);
      }
    }
    cellFrame->SetColIndex(startColIndex);
  }
  PRInt32 damageHeight = (aRowSpanIsZero) ? aMap.GetColCount() - aRowIndex : aRowSpan;
  SetDamageArea(aColIndex, aRowIndex, 1 + endColIndex - aColIndex, damageHeight, aDamageArea); 

  PRInt32 rowX;

  // update the row and col info due to shifting
  for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    CellDataArray& row = mRows[rowX];
    PRUint32 numCols = row.Length();
    PRUint32 colX;
    for (colX = aColIndex + totalColSpan; colX < numCols; colX++) {
      CellData* data = row[colX];
      if (data) {
        // increase the origin and span counts beyond the spanned cols
        if (data->IsOrig()) {
          // a cell that gets moved needs adjustment as well as it new orignating col
          data->GetCellFrame()->SetColIndex(colX);
          nsColInfo* colInfo = aMap.GetColInfoAt(colX);
          colInfo->mNumCellsOrig++;
        }
        if (data->IsColSpan()) {
          nsColInfo* colInfo = aMap.GetColInfoAt(colX);
          colInfo->mNumCellsSpan++;
        }
        
        // decrease the origin and span counts within the spanned cols
        PRInt32 colX2 = colX - totalColSpan;
        nsColInfo* colInfo2 = aMap.GetColInfoAt(colX2);
        if (data->IsOrig()) {
          // the old originating col of a moved cell needs adjustment
          colInfo2->mNumCellsOrig--;
        }
        else {
          colInfo2->mNumCellsSpan--;
        }
      }
    }
  }
}

void nsCellMap::ShrinkWithoutRows(nsTableCellMap& aMap,
                                  PRInt32         aStartRowIndex,
                                  PRInt32         aNumRowsToRemove,
                                  nsRect&         aDamageArea)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  PRInt32 endRowIndex = aStartRowIndex + aNumRowsToRemove - 1;
  PRUint32 colCount = aMap.GetColCount();
  for (PRInt32 rowX = endRowIndex; rowX >= aStartRowIndex; --rowX) {
    CellDataArray& row = mRows[rowX];
    PRUint32 colX;
    for (colX = 0; colX < colCount; colX++) {
      CellData* data = row.SafeElementAt(colX);
      if (data) {
        // Adjust the column counts.
        if (data->IsOrig()) {
          // Decrement the column count.
          nsColInfo* colInfo = aMap.GetColInfoAt(colX);
          colInfo->mNumCellsOrig--;
        }
        // colspan=0 is only counted as a spanned cell in the 1st col it spans
        else if (data->IsColSpan()) {
          nsColInfo* colInfo = aMap.GetColInfoAt(colX);
          colInfo->mNumCellsSpan--;
        }
      }
    }

    PRUint32 rowLength = row.Length();
    // Delete our row information.
    for (colX = 0; colX < rowLength; colX++) {
      DestroyCellData(row[colX]);
    }

    mRows.RemoveElementAt(rowX);

    // Decrement our row and next available index counts.
    mContentRowCount--;
  }
  aMap.RemoveColsAtEnd();

  SetDamageArea(0, aStartRowIndex, aMap.GetColCount(), 0, aDamageArea); 
}

PRInt32 nsCellMap::GetColSpanForNewCell(nsTableCellFrame& aCellFrameToAdd, 
                                        PRBool&           aIsZeroColSpan) const
{
  aIsZeroColSpan = PR_FALSE;
  PRInt32 colSpan = aCellFrameToAdd.GetColSpan();
  if (0 == colSpan) {
    colSpan = 1; // set the min colspan it will be expanded later
    aIsZeroColSpan = PR_TRUE;
  }
  return colSpan;
}

PRInt32 nsCellMap::GetEffectiveColSpan(const nsTableCellMap& aMap,
                                       PRInt32         aRowIndex,
                                       PRInt32         aColIndex,
                                       PRBool&         aZeroColSpan) const
{
  PRInt32 numColsInTable = aMap.GetColCount();
  aZeroColSpan = PR_FALSE;
  PRInt32 colSpan = 1;
  if (PRUint32(aRowIndex) >= mRows.Length()) {
    return colSpan;
  }
  
  const CellDataArray& row = mRows[aRowIndex];
  PRInt32 colX;
  CellData* data;
  PRInt32 maxCols = numColsInTable;
  PRBool hitOverlap = PR_FALSE; // XXX this is not ever being set to PR_TRUE
  for (colX = aColIndex + 1; colX < maxCols; colX++) {
    data = row.SafeElementAt(colX);
    if (data) {
      // for an overlapping situation get the colspan from the originating cell and
      // use that as the max number of cols to iterate. Since this is rare, only 
      // pay the price of looking up the cell's colspan here.
      if (!hitOverlap && data->IsOverlap()) {
        CellData* origData = row.SafeElementAt(aColIndex);
        if (origData && origData->IsOrig()) {
          nsTableCellFrame* cellFrame = origData->GetCellFrame();
          if (cellFrame) {
            // possible change the number of colums to iterate
            maxCols = PR_MIN(aColIndex + cellFrame->GetColSpan(), maxCols);
            if (colX >= maxCols) 
              break;
          }
        }
      }
      if (data->IsColSpan()) {
        colSpan++;
        if (data->IsZeroColSpan()) {
          aZeroColSpan = PR_TRUE;
        }
      }
      else {
        break;
      }
    }
    else break;
  }
  return colSpan;
}

PRInt32 
nsCellMap::GetRowSpanForNewCell(nsTableCellFrame* aCellFrameToAdd,
                                PRInt32           aRowIndex,
                                PRBool&           aIsZeroRowSpan) const
{
  aIsZeroRowSpan = PR_FALSE;
  PRInt32 rowSpan = aCellFrameToAdd->GetRowSpan();
  if (0 == rowSpan) {
    // use a min value of 2 for a zero rowspan to make computations easier elsewhere 
    rowSpan = PR_MAX(2, mContentRowCount - aRowIndex);
    aIsZeroRowSpan = PR_TRUE;
  }
  return rowSpan;
}

PRBool nsCellMap::HasMoreThanOneCell(PRInt32 aRowIndex) const
{
  const CellDataArray& row = mRows.SafeElementAt(aRowIndex, sEmptyRow);
  PRUint32 maxColIndex = row.Length();
  PRUint32 count = 0;
  PRUint32 colIndex;
  for (colIndex = 0; colIndex < maxColIndex; colIndex++) {
    CellData* cellData = row[colIndex];
    if (cellData && (cellData->GetCellFrame() || cellData->IsRowSpan()))
      count++;
    if (count > 1)
      return PR_TRUE;
  }
  return PR_FALSE;
}

PRInt32
nsCellMap::GetNumCellsOriginatingInRow(PRInt32 aRowIndex) const
{
  const CellDataArray& row = mRows.SafeElementAt(aRowIndex, sEmptyRow);
  PRUint32 count = 0;
  PRUint32 maxColIndex = row.Length();
  PRUint32 colIndex;
  for (colIndex = 0; colIndex < maxColIndex; colIndex++) {
    CellData* cellData = row[colIndex];
    if (cellData && cellData->IsOrig())
      count++;
  }
  return count;
}

PRInt32 nsCellMap::GetRowSpan(PRInt32  aRowIndex,
                              PRInt32  aColIndex,
                              PRBool   aGetEffective) const
{
  PRInt32 rowSpan = 1;
  PRInt32 rowCount = (aGetEffective) ? mContentRowCount : mRows.Length();
  PRInt32 rowX;
  for (rowX = aRowIndex + 1; rowX < rowCount; rowX++) {
    CellData* data = GetDataAt(rowX, aColIndex);
    if (data) {
      if (data->IsRowSpan()) {
        rowSpan++;
      }
      else {
        break;
      }
    } 
    else break;
  }
  return rowSpan;
}

void nsCellMap::ShrinkWithoutCell(nsTableCellMap&   aMap,
                                  nsTableCellFrame& aCellFrame,
                                  PRInt32           aRowIndex,
                                  PRInt32           aColIndex,
                                  nsRect&           aDamageArea)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  PRUint32 colX, rowX;

  // get the rowspan and colspan from the cell map since the content may have changed
  PRBool zeroColSpan;
  PRUint32 numCols = aMap.GetColCount();
  PRInt32 rowSpan = GetRowSpan(aRowIndex, aColIndex, PR_FALSE);
  PRUint32 colSpan = GetEffectiveColSpan(aMap, aRowIndex, aColIndex, zeroColSpan);
  PRUint32 endRowIndex = aRowIndex + rowSpan - 1;
  PRUint32 endColIndex = aColIndex + colSpan - 1;

  SetDamageArea(aColIndex, aRowIndex, 1 + endColIndex - aColIndex, 1 + endRowIndex - aRowIndex, aDamageArea); 

  // adjust the col counts due to the deleted cell before removing it
  for (colX = aColIndex; colX <= endColIndex; colX++) {
    nsColInfo* colInfo = aMap.GetColInfoAt(colX);
    if (colX == PRUint32(aColIndex)) {
      colInfo->mNumCellsOrig--;
    }
    else  {
      colInfo->mNumCellsSpan--;
    }
  }

  // remove the deleted cell and cellData entries for it
  for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    CellDataArray& row = mRows[rowX];

    // endIndexForRow points at the first slot we don't want to clean up.  This
    // makes the aColIndex == 0 case work right with our unsigned int colX.
    NS_ASSERTION(endColIndex + 1 <= row.Length(), "span beyond the row size!");
    PRUint32 endIndexForRow = PR_MIN(endColIndex + 1, row.Length());

    // Since endIndexForRow <= row.Length(), enough to compare aColIndex to it.
    if (PRUint32(aColIndex) < endIndexForRow) {
      for (colX = endIndexForRow; colX > PRUint32(aColIndex); colX--) {
        DestroyCellData(row[colX-1]);
      }
      row.RemoveElementsAt(aColIndex, endIndexForRow - aColIndex);
    }
  }

  numCols = aMap.GetColCount();

  // update the row and col info due to shifting
  for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    CellDataArray& row = mRows[rowX];
    for (colX = aColIndex; colX < numCols - colSpan; colX++) {
      CellData* data = row.SafeElementAt(colX);
      if (data) {
        if (data->IsOrig()) {
          // a cell that gets moved to the left needs adjustment in its new location 
          data->GetCellFrame()->SetColIndex(colX);
          nsColInfo* colInfo = aMap.GetColInfoAt(colX);
          colInfo->mNumCellsOrig++;
          // a cell that gets moved to the left needs adjustment in its old location 
          colInfo = aMap.GetColInfoAt(colX + colSpan);
          if (colInfo) {
            colInfo->mNumCellsOrig--;
          }
        }
        
        else if (data->IsColSpan()) {
          // a cell that gets moved to the left needs adjustment
          // in its new location
          nsColInfo* colInfo = aMap.GetColInfoAt(colX);
          colInfo->mNumCellsSpan++;
          // a cell that gets moved to the left needs adjustment
          // in its old location
          colInfo = aMap.GetColInfoAt(colX + colSpan);
          if (colInfo) {
            colInfo->mNumCellsSpan--;
          }
        }
      }
    }
  }
  aMap.RemoveColsAtEnd();
}

void
nsCellMap::RebuildConsideringRows(nsTableCellMap& aMap,
                                  PRInt32         aStartRowIndex,
                                  nsVoidArray*    aRowsToInsert,
                                  PRInt32         aNumRowsToRemove,
                                  nsRect&         aDamageArea)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  // copy the old cell map into a new array
  PRUint32 numOrigRows = mRows.Length();
  nsTArray<CellDataArray> origRows;
  mRows.SwapElements(origRows);

  PRInt32 rowNumberChange;
  if (aRowsToInsert) {
    rowNumberChange = aRowsToInsert->Count();
  } else {
    rowNumberChange = -aNumRowsToRemove;
  }

  // adjust mContentRowCount based on the function arguments as they are known to
  // be real rows.
  mContentRowCount += rowNumberChange;
  NS_ASSERTION(mContentRowCount >= 0, "previous mContentRowCount was wrong");
  // mRows is empty now.  Grow it to the size we expect it to have.
  if (mContentRowCount) {
    if (!Grow(aMap, mContentRowCount)) {
      // Bail, I guess...  Not sure what else we can do here.
      return;
    }
  }

  // aStartRowIndex might be after all existing rows so we should limit the
  // copy to the amount of exisiting rows
  PRUint32 copyEndRowIndex = PR_MIN(numOrigRows, PRUint32(aStartRowIndex));

  // rowX keeps track of where we are in mRows while setting up the
  // new cellmap.
  PRUint32 rowX = 0;
  
  // put back the rows before the affected ones just as before.  Note that we
  // can't just copy the old rows in bit-for-bit, because they might be
  // spanning out into the rows we're adding/removing.
  for ( ; rowX < copyEndRowIndex; rowX++) {
    const CellDataArray& row = origRows[rowX];
    PRUint32 numCols = row.Length();
    for (PRUint32 colX = 0; colX < numCols; colX++) {
      // put in the original cell from the cell map
      const CellData* data = row.ElementAt(colX);
      if (data && data->IsOrig()) {
        AppendCell(aMap, data->GetCellFrame(), rowX, PR_FALSE, aDamageArea);
      }
    }
  }

  // Now handle the new rows being inserted, if any.
  PRUint32 copyStartRowIndex;
  rowX = aStartRowIndex;
  if (aRowsToInsert) {
    // add in the new cells and create rows if necessary
    PRInt32 numNewRows = aRowsToInsert->Count();
    for (PRInt32 newRowX = 0; newRowX < numNewRows; newRowX++) {
      nsTableRowFrame* rFrame = (nsTableRowFrame *)aRowsToInsert->ElementAt(newRowX);
      nsIFrame* cFrame = rFrame->GetFirstChild(nsnull);
      while (cFrame) {
        if (IS_TABLE_CELL(cFrame->GetType())) {
          AppendCell(aMap, (nsTableCellFrame *)cFrame, rowX, PR_FALSE, aDamageArea);
        }
        cFrame = cFrame->GetNextSibling();
      }
      rowX++;
    }
    copyStartRowIndex = aStartRowIndex;
  }
  else {
    copyStartRowIndex = aStartRowIndex + aNumRowsToRemove;
  }
  
  // put back the rows after the affected ones just as before.  Again, we can't
  // just copy the old bits because that would not handle the new rows spanning
  // out or our earlier old rows spanning through the damaged area.
  for (PRUint32 copyRowX = copyStartRowIndex; copyRowX < numOrigRows;
       copyRowX++) {
    const CellDataArray& row = origRows[copyRowX];
    PRUint32 numCols = row.Length();
    for (PRUint32 colX = 0; colX < numCols; colX++) {
      // put in the original cell from the cell map
      CellData* data = row.ElementAt(colX);
      if (data && data->IsOrig()) {
        AppendCell(aMap, data->GetCellFrame(), rowX, PR_FALSE, aDamageArea);
      }
    }
    rowX++;
  }

  // delete the old cell map.  Now rowX no longer has anything to do with mRows
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    CellDataArray& row = origRows[rowX];
    PRUint32 len = row.Length();
    for (PRUint32 colX = 0; colX < len; colX++) {
      DestroyCellData(row[colX]);
    }
  }
  
  SetDamageArea(0, 0, aMap.GetColCount(), GetRowCount(), aDamageArea); 
}

void nsCellMap::RebuildConsideringCells(nsTableCellMap& aMap,
                                        PRInt32         aNumOrigCols,
                                        nsVoidArray*    aCellFrames,
                                        PRInt32         aRowIndex,
                                        PRInt32         aColIndex,
                                        PRBool          aInsert,
                                        nsRect&         aDamageArea)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  // copy the old cell map into a new array
  PRUint32 numOrigRows  = mRows.Length();
  nsTArray<CellDataArray> origRows;
  mRows.SwapElements(origRows);

  PRInt32 numNewCells = (aCellFrames) ? aCellFrames->Count() : 0;
  
  // the new cells might extend the previous column number
  NS_ASSERTION(aNumOrigCols >= aColIndex, "Appending cells far beyond cellmap data?!");
  PRInt32 numCols = aInsert ? PR_MAX(aNumOrigCols, aColIndex + 1) : aNumOrigCols;  
  
  // build the new cell map.  Hard to say what, if anything, we can preallocate
  // here...  Should come back to that sometime, perhaps.
  PRUint32 rowX;
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    const CellDataArray& row = origRows[rowX];
    for (PRInt32 colX = 0; colX < numCols; colX++) {
      if ((rowX == aRowIndex) && (colX == aColIndex)) { 
        if (aInsert) { // put in the new cells
          for (PRInt32 cellX = 0; cellX < numNewCells; cellX++) {
            nsTableCellFrame* cell = (nsTableCellFrame*)aCellFrames->ElementAt(cellX);
            if (cell) {
              AppendCell(aMap, cell, rowX, PR_FALSE, aDamageArea);
            }
          }
        }
        else {
          continue; // do not put the deleted cell back
        }
      }
      // put in the original cell from the cell map
      CellData* data = row.SafeElementAt(colX);
      if (data && data->IsOrig()) {
        AppendCell(aMap, data->GetCellFrame(), rowX, PR_FALSE, aDamageArea);
      }
    }
  }
  if (aInsert && numOrigRows <= aRowIndex) { // append the new cells below the last original row
    NS_ASSERTION (numOrigRows == aRowIndex, "Appending cells far beyond the last row");
    for (PRInt32 cellX = 0; cellX < numNewCells; cellX++) {
      nsTableCellFrame* cell = (nsTableCellFrame*)aCellFrames->ElementAt(cellX);
      if (cell) {
        AppendCell(aMap, cell, aRowIndex, PR_FALSE, aDamageArea);
      }
    }
  } 

  // delete the old cell map
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    CellDataArray& row = origRows[rowX];
    PRUint32 len = row.Length();
    for (PRUint32 colX = 0; colX < len; colX++) {
      DestroyCellData(row.SafeElementAt(colX));
    }
  }
}

void nsCellMap::RemoveCell(nsTableCellMap&   aMap,
                           nsTableCellFrame* aCellFrame,
                           PRInt32           aRowIndex,
                           nsRect&           aDamageArea)
{
  PRUint32 numRows = mRows.Length();
  if (PRUint32(aRowIndex) >= numRows) {
    NS_ERROR("bad arg in nsCellMap::RemoveCell");
    return;
  }
  PRInt32 numCols = aMap.GetColCount();

  // Now aRowIndex is guaranteed OK.

  // get the starting col index of the cell to remove
  PRInt32 startColIndex;
  for (startColIndex = 0; startColIndex < numCols; startColIndex++) {
    CellData* data = mRows[aRowIndex].SafeElementAt(startColIndex);
    if (data && (data->IsOrig()) && (aCellFrame == data->GetCellFrame())) {
      break; // we found the col index
    }
  }

  PRInt32 rowSpan = GetRowSpan(aRowIndex, startColIndex, PR_FALSE);
  // record whether removing the cells is going to cause complications due 
  // to existing row spans, col spans or table sizing. 
  PRBool spansCauseRebuild = CellsSpanInOrOut(aRowIndex,
                                              aRowIndex + rowSpan - 1,
                                              startColIndex, numCols - 1);
  // XXX if the cell has a col span to the end of the map, and the end has no originating 
  // cells, we need to assume that this the only such cell, and rebuild so that there are 
  // no extraneous cols at the end. The same is true for removing rows.
  if (!aCellFrame->GetRowSpan() || !aCellFrame->GetColSpan())
    spansCauseRebuild = PR_TRUE;

  if (spansCauseRebuild) {
    aMap.RebuildConsideringCells(this, nsnull, aRowIndex, startColIndex, PR_FALSE, aDamageArea);
  }
  else {
    ShrinkWithoutCell(aMap, *aCellFrame, aRowIndex, startColIndex, aDamageArea);
  }
}

void nsCellMap::ExpandZeroColSpans(nsTableCellMap& aMap)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  PRUint32 numRows = mRows.Length();
  PRUint32 numCols = aMap.GetColCount();
  PRUint32 rowIndex, colIndex;
 
  for (rowIndex = 0; rowIndex < numRows; rowIndex++) {
    for (colIndex = 0; colIndex < numCols; colIndex++) {
      CellData* data = mRows[rowIndex].SafeElementAt(colIndex);
      if (!data || !data->IsOrig())
        continue;
      nsTableCellFrame* cell = data->GetCellFrame();
      NS_ASSERTION(cell, "There has to be a cell");
      PRInt32 cellRowSpan = cell->GetRowSpan();
      PRInt32 cellColSpan = cell->GetColSpan();
      PRBool rowZeroSpan = (0 == cell->GetRowSpan());
      PRBool colZeroSpan = (0 == cell->GetColSpan());
      if (colZeroSpan) {
        aMap.mTableFrame.SetHasZeroColSpans(PR_TRUE);
        // do the expansion
        NS_ASSERTION(numRows > 0, "Bogus numRows");
        NS_ASSERTION(numCols > 0, "Bogus numCols");
        PRUint32 endRowIndex =  rowZeroSpan ? numRows - 1 :
                                              rowIndex + cellRowSpan - 1;
        PRUint32 endColIndex =  colZeroSpan ? numCols - 1 :
                                              colIndex + cellColSpan - 1;
        PRUint32 colX, rowX;
        colX = colIndex + 1;
        while (colX <= endColIndex) {
          // look at columns from here to our colspan.  For each one, check
          // the rows from here to our rowspan to make sure there is no
          // obstacle to marking that column as a zerospanned column; if there
          // isn't, mark it so
          for (rowX = rowIndex; rowX <= endRowIndex; rowX++) {
            CellData* oldData = GetDataAt(rowX, colX);
            if (oldData) {
              if (oldData->IsOrig()) {
                break; // something is in the way
              }
              if (oldData->IsRowSpan()) {
                if ((rowX - rowIndex) != oldData->GetRowSpanOffset()) {
                  break;
                }
              }
              if (oldData->IsColSpan()) {
                if ((colX - colIndex) != oldData->GetColSpanOffset()) {
                  break;
                }
              }
            }
          }
          if (endRowIndex >= rowX)
            break;// we hit something
          for (rowX = rowIndex; rowX <= endRowIndex; rowX++) {
            CellData* newData = AllocCellData(nsnull);
            if (!newData) return;
            
            newData->SetColSpanOffset(colX - colIndex);
            newData->SetZeroColSpan(PR_TRUE);
            
            if (rowX > rowIndex) {
              newData->SetRowSpanOffset(rowX - rowIndex);
              if (rowZeroSpan)
                newData->SetZeroRowSpan(PR_TRUE);
            }
            SetDataAt(aMap, *newData, rowX, colX);
          }
          colX++;
        }  // while (colX <= endColIndex)
      } // if zerocolspan
    }
  }
}
#ifdef NS_DEBUG
void nsCellMap::Dump(PRBool aIsBorderCollapse) const
{
  printf("\n  ***** START GROUP CELL MAP DUMP ***** %p\n", (void*)this);
  nsTableRowGroupFrame* rg = GetRowGroup();
  const nsStyleDisplay* display = rg->GetStyleDisplay();
  switch (display->mDisplay) {
  case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
    printf("  thead ");
    break;
  case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
    printf("  tfoot ");
    break;
  case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
    printf("  tbody ");
    break;
  default:
    printf("HUH? wrong display type on rowgroup");
  }
  PRUint32 mapRowCount = mRows.Length();
  printf("mapRowCount=%u tableRowCount=%d\n", mapRowCount, mContentRowCount);
  

  PRUint32 rowIndex, colIndex;
  for (rowIndex = 0; rowIndex < mapRowCount; rowIndex++) {
    const CellDataArray& row = mRows[rowIndex];
    printf("  row %d : ", rowIndex);
    PRUint32 colCount = row.Length();
    for (colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = row[colIndex];
      if (cd) {
        if (cd->IsOrig()) {
          printf("C%d,%d  ", rowIndex, colIndex);
        } else {
          nsTableCellFrame* cell = nsnull;
          if (cd->IsRowSpan()) {
            cell = GetCellFrame(rowIndex, colIndex, *cd, PR_TRUE);
            printf("R ");
          }
          if (cd->IsColSpan()) {
            cell = GetCellFrame(rowIndex, colIndex, *cd, PR_FALSE);
            printf("C ");
          }
          if (!(cd->IsRowSpan() && cd->IsColSpan())) {
            printf("  ");
          } 
          printf("  ");  
        }
      } else {
        printf("----  ");
      }
    }
    if (aIsBorderCollapse) {
      nscoord       size;
      BCBorderOwner owner;
      PRUint8       side;
      PRBool        segStart;
      PRPackedBool  bevel;      
      for (PRInt32 i = 0; i <= 2; i++) {
        printf("\n          ");
        for (colIndex = 0; colIndex < colCount; colIndex++) {
          BCCellData* cd = (BCCellData *)row[colIndex];
          if (cd) {
            if (0 == i) {
              size = cd->mData.GetTopEdge(owner, segStart);
              printf("t=%d%d%d ", size, owner, segStart);
            }
            else if (1 == i) {
              size = cd->mData.GetLeftEdge(owner, segStart);
              printf("l=%d%d%d ", size, owner, segStart);
            }
            else {
              size = cd->mData.GetCorner(side, bevel);
              printf("c=%d%d%d ", size, side, bevel);
            }
          }
        }
      }
    }
    printf("\n");
  }

  // output info mapping Ci,j to cell address
  PRUint32 cellCount = 0;
  for (PRUint32 rIndex = 0; rIndex < mapRowCount; rIndex++) {
    const CellDataArray& row = mRows[rIndex];
    PRUint32 colCount = row.Length();
    printf("  ");
    for (colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = row[colIndex];
      if (cd) {
        if (cd->IsOrig()) {
          nsTableCellFrame* cellFrame = cd->GetCellFrame();
          PRInt32 cellFrameColIndex;
          cellFrame->GetColIndex(cellFrameColIndex);
          printf("C%d,%d=%p(%d)  ", rIndex, colIndex, (void*)cellFrame,
                 cellFrameColIndex);
          cellCount++;
        }
      }
    }
    printf("\n");
  }

  printf("  ***** END GROUP CELL MAP DUMP *****\n");
}
#endif

PRBool 
nsCellMap::IsZeroColSpan(PRInt32 aRowIndex,
                         PRInt32 aColIndex) const
{
  CellData* data =
    mRows.SafeElementAt(aRowIndex, sEmptyRow).SafeElementAt(aColIndex);
  return data && data->IsZeroColSpan();
}

CellData* 
nsCellMap::GetDataAt(PRInt32         aMapRowIndex,
                     PRInt32         aColIndex) const
{
  return mRows.SafeElementAt(aMapRowIndex, sEmptyRow).SafeElementAt(aColIndex);
}

// only called if the cell at aMapRowIndex, aColIndex is null or dead
// (the latter from ExpandZeroColSpans).
void nsCellMap::SetDataAt(nsTableCellMap& aMap,
                          CellData&       aNewCell, 
                          PRInt32         aMapRowIndex, 
                          PRInt32         aColIndex)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  if (PRUint32(aMapRowIndex) >= mRows.Length()) {
    NS_ERROR("SetDataAt called with row index > num rows");
    return;
  }

  CellDataArray& row = mRows[aMapRowIndex];

  // the table map may need cols added
  PRInt32 numColsToAdd = aColIndex + 1 - aMap.GetColCount();
  if (numColsToAdd > 0) {
    aMap.AddColsAtEnd(numColsToAdd);
  }
  // the row may need cols added
  numColsToAdd = aColIndex + 1 - row.Length();
  if (numColsToAdd > 0) {
    // XXXbz need to handle allocation failures.
    GrowRow(row, numColsToAdd);
  }

  DestroyCellData(row[aColIndex]);

  row.ReplaceElementsAt(aColIndex, 1, &aNewCell);
  // update the originating cell counts if cell originates in this row, col
  nsColInfo* colInfo = aMap.GetColInfoAt(aColIndex);
  if (colInfo) {
    if (aNewCell.IsOrig()) { 
      colInfo->mNumCellsOrig++;
    }
    else if (aNewCell.IsColSpan()) {
      colInfo->mNumCellsSpan++;
    }
  }
  else NS_ERROR("SetDataAt called with col index > table map num cols");
}

nsTableCellFrame* 
nsCellMap::GetCellInfoAt(const nsTableCellMap& aMap,
                         PRInt32               aRowX,
                         PRInt32               aColX,
                         PRBool*               aOriginates,
                         PRInt32*              aColSpan) const
{
  if (aOriginates) {
    *aOriginates = PR_FALSE;
  }
  CellData* data = GetDataAt(aRowX, aColX);
  nsTableCellFrame* cellFrame = nsnull;  
  if (data) {
    if (data->IsOrig()) {
      cellFrame = data->GetCellFrame();
      if (aOriginates)
        *aOriginates = PR_TRUE;
    }
    else {
      cellFrame = GetCellFrame(aRowX, aColX, *data, PR_TRUE);
    }
    if (cellFrame && aColSpan) {
      PRInt32 initialColIndex;
      cellFrame->GetColIndex(initialColIndex);
      PRBool zeroSpan;
      *aColSpan = GetEffectiveColSpan(aMap, aRowX, initialColIndex, zeroSpan);
    }
  }
  return cellFrame;
}
  

PRBool nsCellMap::RowIsSpannedInto(PRInt32         aRowIndex,
                                   PRInt32         aNumEffCols) const
{
  if ((0 > aRowIndex) || (aRowIndex >= mContentRowCount)) {
    return PR_FALSE;
  }
  for (PRInt32 colIndex = 0; colIndex < aNumEffCols; colIndex++) {
    CellData* cd = GetDataAt(aRowIndex, colIndex);
    if (cd) { // there's really a cell at (aRowIndex, colIndex)
      if (cd->IsSpan()) { // the cell at (aRowIndex, colIndex) is the result of a span
        if (cd->IsRowSpan() && GetCellFrame(aRowIndex, colIndex, *cd, PR_TRUE)) { // XXX why the last check
          return PR_TRUE;
        }
      }
    }
  }
  return PR_FALSE;
}

PRBool nsCellMap::RowHasSpanningCells(PRInt32 aRowIndex,
                                      PRInt32 aNumEffCols) const
{
  if ((0 > aRowIndex) || (aRowIndex >= mContentRowCount)) {
    return PR_FALSE;
  }
  if (aRowIndex != mContentRowCount - 1) {
    // aRowIndex is not the last row, so we check the next row after aRowIndex for spanners
    for (PRInt32 colIndex = 0; colIndex < aNumEffCols; colIndex++) {
      CellData* cd = GetDataAt(aRowIndex, colIndex);
      if (cd && (cd->IsOrig())) { // cell originates 
        CellData* cd2 = GetDataAt(aRowIndex + 1, colIndex);
        if (cd2 && cd2->IsRowSpan()) { // cd2 is spanned by a row
          if (cd->GetCellFrame() == GetCellFrame(aRowIndex + 1, colIndex, *cd2, PR_TRUE)) {
            return PR_TRUE;
          }
        }
      }
    }
  }
  return PR_FALSE;
}

PRBool nsCellMap::ColHasSpanningCells(PRInt32 aColIndex) const
{
  for (PRInt32 rowIndex = 0; rowIndex < mContentRowCount; rowIndex++) {
    CellData* cd = GetDataAt(rowIndex, aColIndex);
    if (cd && (cd->IsOrig())) { // cell originates 
      CellData* cd2 = GetDataAt(rowIndex, aColIndex +1);
      if (cd2 && cd2->IsColSpan()) { // cd2 is spanned by a col
        if (cd->GetCellFrame() == GetCellFrame(rowIndex , aColIndex + 1, *cd2, PR_FALSE)) {
          return PR_TRUE;
        }
      }
    }
  }
  return PR_FALSE;
}

void nsCellMap::DestroyCellData(CellData* aData)
{
  if (!aData) {
    return;
  }
  
  if (mIsBC) {
    BCCellData* bcData = NS_STATIC_CAST(BCCellData*, aData);
    bcData->~BCCellData();
    mPresContext->FreeToShell(sizeof(BCCellData), bcData);
  } else {
    aData->~CellData();
    mPresContext->FreeToShell(sizeof(CellData), aData);
  }
}

CellData* nsCellMap::AllocCellData(nsTableCellFrame* aOrigCell)
{
  if (mIsBC) {
    BCCellData* data = (BCCellData*)
      mPresContext->AllocateFromShell(sizeof(BCCellData));
    if (data) {
      new (data) BCCellData(aOrigCell);
    }
    return data;
  }

  CellData* data = (CellData*)
    mPresContext->AllocateFromShell(sizeof(CellData));
  if (data) {
    new (data) CellData(aOrigCell);
  }
  return data;
}

void
nsCellMapColumnIterator::AdvanceRowGroup()
{
  do {
    mCurMapStart += mCurMapContentRowCount;  
    mCurMap = mCurMap->GetNextSibling();
    if (!mCurMap) {
      // Set mCurMapContentRowCount and mCurMapRelevantRowCount to 0 in case
      // mCurMap has no next sibling.  This can happen if we just handled the
      // last originating cell.  Future calls will end up with mFoundCells ==
      // mOrigCells, but for this one mFoundCells was definitely not big enough
      // if we got here.
      mCurMapContentRowCount = 0;
      mCurMapRelevantRowCount = 0;
      break;
    }

    mCurMapContentRowCount = mCurMap->GetRowCount();
    PRUint32 rowArrayLength = mCurMap->mRows.Length();
    mCurMapRelevantRowCount = PR_MIN(mCurMapContentRowCount, rowArrayLength);
  } while (0 == mCurMapRelevantRowCount);

  NS_ASSERTION(mCurMapRelevantRowCount != 0 || !mCurMap,
               "How did that happen?");

  // Set mCurMapRow to 0, since cells can't span across table row groups.
  mCurMapRow = 0;
}

void
nsCellMapColumnIterator::IncrementRow(PRInt32 aIncrement)
{
  NS_PRECONDITION(aIncrement >= 0, "Bogus increment");
  NS_PRECONDITION(mCurMap, "Bogus mOrigCells?");
  if (aIncrement == 0) {
    AdvanceRowGroup();
  }
  else {
    mCurMapRow += aIncrement;
    if (mCurMapRow >= mCurMapRelevantRowCount) {
      AdvanceRowGroup();
    }
  }
}

nsTableCellFrame*
nsCellMapColumnIterator::GetNextFrame(PRInt32* aRow, PRInt32* aColSpan)
{
  // Fast-path for the case when we don't have anything left in the column and
  // we know it.
  if (mFoundCells == mOrigCells) {
    *aRow = 0;
    *aColSpan = 1;
    return nsnull;
  }

  while (1) {
    NS_ASSERTION(mCurMapRow < mCurMapRelevantRowCount, "Bogus mOrigCells?");
    // Safe to just get the row (which is faster than calling GetDataAt(), but
    // there may not be that many cells in it, so have to use SafeElementAt for
    // the mCol.
    const nsCellMap::CellDataArray& row = mCurMap->mRows[mCurMapRow];
    CellData* cellData = row.SafeElementAt(mCol);
    if (!cellData || cellData->IsDead()) {
      // Could hit this if there are fewer cells in this row than others, for
      // example.
      IncrementRow(1);
      continue;
    }

    if (cellData->IsColSpan()) {
      // Look up the originating data for this cell, advance by its rowspan.
      CellData* origData = row[mCol - cellData->GetColSpanOffset()];
      NS_ASSERTION(origData && origData->IsOrig() && origData->GetCellFrame(),
                   "Must have usable originating data here");
      IncrementRow(origData->GetCellFrame()->GetRowSpan());
      continue;
    }

    NS_ASSERTION(cellData->IsOrig(),
                 "Must have originating cellData by this point.  "
                 "See comment on mCurMapRow in header.");

    nsTableCellFrame* cellFrame = cellData->GetCellFrame();
    NS_ASSERTION(cellFrame, "Orig data without cellframe?");
    
    *aRow = mCurMapStart + mCurMapRow;
    PRBool ignoredZeroSpan;
    *aColSpan = mCurMap->GetEffectiveColSpan(*mMap, mCurMapRow, mCol,
                                             ignoredZeroSpan);

    IncrementRow(cellFrame->GetRowSpan());

    ++mFoundCells;

    NS_ASSERTION(cellData = mMap->GetDataAt(*aRow, mCol),
                 "Giving caller bogus row?");

    return cellFrame;
  }

  NS_NOTREACHED("Can't get here");
  return nsnull;
}
