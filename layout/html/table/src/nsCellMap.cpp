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

#include "nsLayoutAtoms.h"
#include "nsVoidArray.h"
#include "nsCellMap.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableRowGroupFrame.h"


// colspan=0 gets a minimum number of cols initially to make computations easier
#define MIN_NUM_COLS_FOR_ZERO_COLSPAN 2

// CellData 

MOZ_DECL_CTOR_COUNTER(CellData)

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

MOZ_DECL_CTOR_COUNTER(nsCellMap)

// nsTableCellMap

nsTableCellMap::nsTableCellMap(nsTableFrame&   aTableFrame,
                               PRBool          aBorderCollapse)
:mTableFrame(aTableFrame), mFirstMap(nsnull), mBCInfo(nsnull)
{
  MOZ_COUNT_CTOR(nsTableCellMap);

  nsAutoVoidArray orderedRowGroups;
  PRUint32 numRowGroups;
  aTableFrame.OrderRowGroups(orderedRowGroups, numRowGroups);
  NS_ASSERTION(orderedRowGroups.Count() == (PRInt32) numRowGroups,"problem in OrderRowGroups");

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
  nsCellMap* newMap = new nsCellMap(aNewGroup);
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

nsCellMap* 
nsTableCellMap::GetMapFor(nsTableRowGroupFrame& aRowGroup)
{
  NS_ASSERTION(!aRowGroup.GetPrevInFlow(), "GetMapFor called with continuation");
  for (nsCellMap* map = mFirstMap; map; map = map->GetNextSibling()) {
    if (&aRowGroup == map->GetRowGroup()) {
      return map;
    }
  }
  // if aRowGroup is a repeated header or footer find the header or footer it was repeated from
  if (aRowGroup.IsRepeatable()) {
    nsTableFrame* fifTable = NS_STATIC_CAST(nsTableFrame*, mTableFrame.GetFirstInFlow());

    nsAutoVoidArray rowGroups;
    PRUint32 numRowGroups;
    nsTableRowGroupFrame *thead, *tfoot;
    nsIFrame *ignore;
    // find the original header/footer 
    fifTable->OrderRowGroups(rowGroups, numRowGroups, &ignore, &thead, &tfoot);

    const nsStyleDisplay* display = aRowGroup.GetStyleDisplay();
    nsTableRowGroupFrame* rgOrig = 
      (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == display->mDisplay) ? thead : tfoot; 
    // find the row group cell map using the original header/footer
    if (rgOrig) {
      for (nsCellMap* map = mFirstMap; map; map = map->GetNextSibling()) {
        if (rgOrig == map->GetRowGroup()) {
          return map;
        }
      }
    }
  }

  return nsnull;
}

PRBool
nsTableCellMap::HasMoreThanOneCell(PRInt32 aRowIndex)
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->HasMoreThanOneCell(*this, rowIndex);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  return PR_FALSE;
}

PRInt32 
nsTableCellMap::GetEffectiveRowSpan(PRInt32 aRowIndex,
                                    PRInt32 aColIndex)
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      PRBool zeroRowSpan;
      return map->GetRowSpan(*this, rowIndex, aColIndex, PR_TRUE, zeroRowSpan);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  return nsnull;
}

PRInt32 
nsTableCellMap::GetEffectiveColSpan(PRInt32 aRowIndex,
                                    PRInt32 aColIndex)
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
  return nsnull;
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
                          PRInt32 aColIndex,
                          PRBool  aUpdateZeroSpan)
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->GetDataAt(*this, rowIndex, aColIndex, aUpdateZeroSpan);
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
      NS_ASSERTION(0, "null entry in column info array");
      mCols.RemoveElementAt(colX);
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

  NS_ASSERTION(PR_FALSE, "Attempt to insert row into wrong map.");
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

PRInt32
nsTableCellMap::GetNumCellsOriginatingInRow(PRInt32 aRowIndex)
{
  PRInt32 originCount = 0;

  CellData* cellData;
  PRInt32   colIndex = 0;

  do {
	  cellData = GetDataAt(aRowIndex, colIndex);
	  if (cellData && cellData->GetCellFrame())
		  originCount++;
    colIndex++;
  } while(cellData);

  return originCount;
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

PRInt32 
nsTableCellMap::GetNumCellsOriginatingInCol(PRInt32 aColIndex) const
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
void 
nsTableCellMap::Dump(char* aString) const
{
  if (aString) 
    printf("%s \n", aString);
  printf("***** START TABLE CELL MAP DUMP ***** %p\n", this);
  // output col info
  PRInt32 colCount = mCols.Count();
  printf ("cols array orig/span-> %p", this);
  for (PRInt32 colX = 0; colX < colCount; colX++) {
    nsColInfo* colInfo = (nsColInfo *)mCols.ElementAt(colX);
    printf ("%d=%d/%d ", colX, colInfo->mNumCellsOrig, colInfo->mNumCellsSpan);
  }
  printf(" cols in cache %d", mTableFrame.GetColCache().Count());
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
                              PRInt32* aColSpan)
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
  

PRBool nsTableCellMap::RowIsSpannedInto(PRInt32 aRowIndex)
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      return cellMap->RowIsSpannedInto(*this, rowIndex);
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  return PR_FALSE;
}

PRBool nsTableCellMap::RowHasSpanningCells(PRInt32 aRowIndex)
{
  PRInt32 rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      return cellMap->RowHasSpanningCells(*this, rowIndex);
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  return PR_FALSE;
}

PRBool nsTableCellMap::ColIsSpannedInto(PRInt32 aColIndex)
{
  PRBool result = PR_FALSE;

  PRInt32 colCount = mCols.Count();
  if ((aColIndex >= 0) && (aColIndex < colCount)) {
    result = (PRBool) ((nsColInfo *)mCols.ElementAt(aColIndex))->mNumCellsSpan;
  }
  return result;
}

PRBool nsTableCellMap::ColHasSpanningCells(PRInt32 aColIndex)
{
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->ColHasSpanningCells(*this, aColIndex)) {
      return PR_TRUE;
    }
    cellMap = cellMap->GetNextSibling();
  }
  return PR_FALSE;
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
    cellData = (BCCellData*)aCellMap.GetDataAt(*this, aRowIndex, aColIndex, PR_FALSE);
    if (cellData) {
      bcData = &cellData->mData;
    }
    else {
      NS_ASSERTION(aSide == NS_SIDE_BOTTOM, "program error");
      // try the next row group
      nsCellMap* cellMap = aCellMap.GetNextSibling();
      if (cellMap) {
        cellData = (BCCellData*)cellMap->GetDataAt(*this, 0, aColIndex, PR_FALSE);
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
    cellData = (BCCellData*)aCellMap.GetDataAt(*this, aRowIndex, aColIndex, PR_FALSE);
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
  //if (aRowIndex >= 80) {
  //  NS_ASSERTION(PR_FALSE, "hello");
  //}

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
      cellData = (BCCellData*)aCellMap.GetDataAt(*this, rgYPos, xIndex, PR_FALSE);
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
            cellData = (BCCellData*)cellMap->GetDataAt(*this, 0, xIndex, PR_FALSE);
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
      else NS_ASSERTION(PR_FALSE, "program error");
    }
    break;
  case NS_SIDE_RIGHT:
    xPos++;
  case NS_SIDE_LEFT:
    // since top, bottom borders were set, there should already be a cellData entry
    lastIndex = rgYPos + aLength - 1;
    for (yIndex = rgYPos; yIndex <= lastIndex; yIndex++) {
      changed = aChanged && (yIndex == rgYPos);
      cellData = (BCCellData*)aCellMap.GetDataAt(*this, yIndex, xPos, PR_FALSE);
      if (cellData) {
        cellData->mData.SetLeftEdge(aOwner, aSize, changed);
      }
      else {
        NS_ASSERTION(aSide == NS_SIDE_RIGHT, "program error");
        BCData* bcData = GetRightMostBorder(yIndex + aCellMapStart);
        if (bcData) {
          bcData->SetLeftEdge(aOwner, aSize, changed);
        }
        else NS_ASSERTION(PR_FALSE, "program error");
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
    if (aIsBottomRight) { // at the bottom right corner
      bcData = &mBCInfo->mLowerRightCorner;
    }
    else  {               // at the right edge of the table
      bcData = GetRightMostBorder(yPos);
    }
  }
  else {
    cellData = (BCCellData*)aCellMap.GetDataAt(*this, rgYPos, xPos, PR_FALSE);
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
          cellData = (BCCellData*)cellMap->GetDataAt(*this, 0, xPos, PR_FALSE);
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
  else NS_ASSERTION(PR_FALSE, "program error");
}

nsCellMap::nsCellMap(nsTableRowGroupFrame& aRowGroup)
  : mRowCount(0), mRowGroupFrame(&aRowGroup), mNextSibling(nsnull)
{
  MOZ_COUNT_CTOR(nsCellMap);
}

nsCellMap::~nsCellMap()
{
  MOZ_COUNT_DTOR(nsCellMap);
  
  PRInt32 mapRowCount = mRows.Count();
  for (PRInt32 rowX = 0; rowX < mapRowCount; rowX++) {
    nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(rowX));
    PRInt32 colCount = row->Count();
    for (PRInt32 colX = 0; colX < colCount; colX++) {
      CellData* data = (CellData *)(row->ElementAt(colX));
      if (data) {
        delete data;
      } 
    }
    delete row;
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

  nsVoidArray* row = (nsVoidArray*) mRows.SafeElementAt(rowIndex);
  if (row) {
    CellData* data = (CellData*)row->SafeElementAt(colIndex);
    if (data) {
      return data->GetCellFrame();
    }
  }
  return nsnull;
}

PRBool nsCellMap::Grow(nsTableCellMap& aMap,
                       PRInt32         aNumRows,
                       PRInt32         aRowIndex)
{
  PRInt32 numCols = aMap.GetColCount();
  PRInt32 startRowIndex = (aRowIndex >= 0) ? aRowIndex : mRows.Count();
  PRInt32 endRowIndex = startRowIndex + aNumRows - 1;
  // XXX We really should have a way to say "make this voidarray at least
  // N entries long" to avoid reallocating N times.  On the other hand, the
  // number of likely allocations here isn't TOO gigantic, and we may not
  // know about many of them at a time.
  for (PRInt32 rowX = startRowIndex; rowX <= endRowIndex; rowX++) {
    nsVoidArray* row;
    row = (0 == numCols) ? new nsVoidArray(4) : new nsVoidArray(numCols);
    if (row) {
      mRows.InsertElementAt(row, rowX);
    }
    else return PR_FALSE;
  }
  return PR_TRUE;
}

void nsCellMap::GrowRow(nsVoidArray& aRow,
                        PRInt32      aNumCols)
                     
{
  for (PRInt32 colX = 0; colX < aNumCols; colX++) {
    aRow.AppendElement(nsnull);
  }
}

void
nsCellMap::InsertRows(nsTableCellMap& aMap,
                      nsVoidArray&    aRows,
                      PRInt32         aFirstRowIndex,
                      PRBool          aConsiderSpans,
                      nsRect&         aDamageArea)
{
  PRInt32 numCols = aMap.GetColCount();

  if (aFirstRowIndex > mRowCount) {
    // create (aFirstRowIndex - mRowCount) empty rows up to aFirstRowIndex
    PRInt32 numEmptyRows = aFirstRowIndex - mRowCount;
    if (!Grow(aMap, numEmptyRows, mRowCount)) {
      return;
    }
    // update mRowCount, since non-empty rows will be added
    mRowCount += numEmptyRows;
  }

  if (!aConsiderSpans) {
    ExpandWithRows(aMap, aRows, aFirstRowIndex, aDamageArea);
    return;
  }

  // if any cells span into or out of the row being inserted, then rebuild
  PRBool spansCauseRebuild = CellsSpanInOrOut(aMap, aFirstRowIndex, 
                                              aFirstRowIndex, 0, numCols - 1);

  // if any of the new cells span out of the new rows being added, then rebuild
  // XXX it would be better to only rebuild the portion of the map that follows the new rows
  if (!spansCauseRebuild && (aFirstRowIndex < mRows.Count())) {
    spansCauseRebuild = CellsSpanOut(aRows);
  }

  if (spansCauseRebuild) {
    RebuildConsideringRows(aMap, aFirstRowIndex, &aRows, 0, aDamageArea);
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
  PRInt32 numRows = mRows.Count();
  PRInt32 numCols = aMap.GetColCount();

  if (aFirstRowIndex >= numRows) {
    return;
  }
  if (!aConsiderSpans) {
    ShrinkWithoutRows(aMap, aFirstRowIndex, aNumRowsToRemove, aDamageArea);
    return;
  }
  PRInt32 endRowIndex = aFirstRowIndex + aNumRowsToRemove - 1;
  if (endRowIndex >= numRows) {
    NS_ASSERTION(PR_FALSE, "nsCellMap::RemoveRows tried to remove too many rows");
    endRowIndex = numRows - 1;
  }
  PRBool spansCauseRebuild = CellsSpanInOrOut(aMap, aFirstRowIndex, endRowIndex,
                                              0, numCols - 1);

  if (spansCauseRebuild) {
    RebuildConsideringRows(aMap, aFirstRowIndex, nsnull, aNumRowsToRemove, aDamageArea);
  }
  else {
    ShrinkWithoutRows(aMap, aFirstRowIndex, aNumRowsToRemove, aDamageArea);
  }
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


CellData* 
nsCellMap::AppendCell(nsTableCellMap&   aMap,
                      nsTableCellFrame* aCellFrame, 
                      PRInt32           aRowIndex,
                      PRBool            aRebuildIfNecessary,
                      nsRect&           aDamageArea,
                      PRInt32*          aColToBeginSearch)
{
  PRInt32 origNumMapRows = mRows.Count();
  PRInt32 origNumCols = aMap.GetColCount();
  PRBool  zeroRowSpan;
  PRInt32 rowSpan = (aCellFrame) ? GetRowSpanForNewCell(*aCellFrame, aRowIndex, zeroRowSpan) : 1;
  // add new rows if necessary
  PRInt32 endRowIndex = aRowIndex + rowSpan - 1;
  if (endRowIndex >= origNumMapRows) {
    Grow(aMap, 1 + endRowIndex - origNumMapRows);
  }

  // get the first null or dead CellData in the desired row. It will equal origNumCols if there are none
  CellData* origData = nsnull;
  PRInt32 startColIndex = 0;
  if (aColToBeginSearch)
    startColIndex = *aColToBeginSearch;
  for (; startColIndex < origNumCols; startColIndex++) {
    CellData* data = GetDataAt(aMap, aRowIndex, startColIndex, PR_TRUE);
    if (!data) 
      break;
    if (data->IsDead()) {
      origData = data;
      break; 
    }
  }
  // We found the place to append the cell, when the next cell is appended 
  // the next search does not need to duplicate the search but can start 
  // just at the next cell.
  if (aColToBeginSearch)
    *aColToBeginSearch =  startColIndex + 1; 
  
  PRBool  zeroColSpan;
  PRInt32 colSpan = (aCellFrame) ? GetColSpanForNewCell(*aCellFrame, startColIndex, origNumCols, zeroColSpan) : 1;

  // if the new cell could potentially span into other rows and collide with 
  // originating cells there, we will play it safe and just rebuild the map
  if (aRebuildIfNecessary && (aRowIndex < mRowCount - 1) && (rowSpan > 1)) {
    nsAutoVoidArray newCellArray;
    newCellArray.AppendElement(aCellFrame);
    RebuildConsideringCells(aMap, &newCellArray, aRowIndex, startColIndex, PR_TRUE, aDamageArea);
    return origData;
  }
  mRowCount = PR_MAX(mRowCount, aRowIndex + 1);

  // add new cols to the table map if necessary
  PRInt32 endColIndex = startColIndex + colSpan - 1;
  if (endColIndex >= origNumCols) {
    aMap.AddColsAtEnd(1 + endColIndex - origNumCols);
  }

  // Setup CellData for this cell
  if (origData) {
    NS_ASSERTION(origData->IsDead(), "replacing a non dead cell is a memory leak");
    origData->Init(aCellFrame);
    // we are replacing a dead cell, increase the number of cells 
    // originating at this column
    nsColInfo* colInfo = aMap.GetColInfoAt(startColIndex);
    NS_ASSERTION(colInfo, "access to a non existing column");
    if (colInfo) { 
      colInfo->mNumCellsOrig++;
    }
  }
  else {
    origData = (aMap.mBCInfo) ? new BCCellData(aCellFrame) : new CellData(aCellFrame); if (!origData) ABORT1(origData);
    SetDataAt(aMap, *origData, aRowIndex, startColIndex, PR_TRUE);
  }

  SetDamageArea(startColIndex, aRowIndex, 1 + endColIndex - startColIndex, 1 + endRowIndex - aRowIndex, aDamageArea); 

  if (!aCellFrame) {
    return origData;
  }

  // initialize the cell frame
  aCellFrame->InitCellFrame(startColIndex);

  // Create CellData objects for the rows that this cell spans. Set
  // their mOrigCell to nsnull and their mSpanData to point to data.
  for (PRInt32 rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    for (PRInt32 colX = startColIndex; colX <= endColIndex; colX++) {
      if ((rowX != aRowIndex) || (colX != startColIndex)) { // skip orig cell data done above
        CellData* cellData = GetDataAt(aMap, rowX, colX, PR_FALSE);
        if (cellData) {
          if (cellData->IsOrig()) {
            NS_ASSERTION(PR_FALSE, "cannot overlap originating cell");
            continue;
          }
          if (rowX > aRowIndex) { // row spanning into cell
            if (cellData->IsRowSpan()) {
              NS_ASSERTION(PR_FALSE, "invalid overlap");
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
              // only count the 1st spanned col of a zero col span
              if (!zeroColSpan || (colX == startColIndex + 1)) {
                nsColInfo* colInfo = aMap.GetColInfoAt(colX);
                colInfo->mNumCellsSpan++;
              }
            }
          }
        }
        else { 
          cellData = (aMap.mBCInfo) ? new BCCellData(nsnull) : new CellData(nsnull);
          if (!cellData) return origData;
          if (rowX > aRowIndex) {
            cellData->SetRowSpanOffset(rowX - aRowIndex);
          }
          if (zeroRowSpan) {
            cellData->SetZeroRowSpan(PR_TRUE);
          }
          if (colX > startColIndex) {
            cellData->SetColSpanOffset(colX - startColIndex);
          }
          if (zeroColSpan) {
            cellData->SetZeroColSpan(PR_TRUE);
          }
          // only count the 1st spanned col of a zero col span
          SetDataAt(aMap, *cellData, rowX, colX, (colX == startColIndex + 1));
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

PRBool nsCellMap::CellsSpanOut(nsVoidArray&    aRows)
{ 
  PRInt32 numNewRows = aRows.Count();
  for (PRInt32 rowX = 0; rowX < numNewRows; rowX++) {
    nsIFrame* rowFrame = (nsIFrame *) aRows.ElementAt(rowX);
    nsIFrame* cellFrame = rowFrame->GetFirstChild(nsnull);
    while (cellFrame) {
      if (IS_TABLE_CELL(cellFrame->GetType())) {
        PRBool zeroSpan;
        PRInt32 rowSpan = GetRowSpanForNewCell((nsTableCellFrame &)*cellFrame, rowX, zeroSpan);
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
PRBool nsCellMap::CellsSpanInOrOut(nsTableCellMap& aMap,
                                   PRInt32         aStartRowIndex, 
                                   PRInt32         aEndRowIndex,
                                   PRInt32         aStartColIndex, 
                                   PRInt32         aEndColIndex)
{
  for (PRInt32 colX = aStartColIndex; colX <= aEndColIndex; colX++) {
    CellData* cellData;
    if (aStartRowIndex > 0) {
      cellData = GetDataAt(aMap, aStartRowIndex, colX, PR_TRUE);
      if (cellData && (cellData->IsRowSpan())) {
        return PR_TRUE; // there is a row span into the region
      }
    }
    if (aEndRowIndex < mRowCount - 1) {
      cellData = GetDataAt(aMap, aEndRowIndex + 1, colX, PR_TRUE);
      if ((cellData) && (cellData->IsRowSpan())) {
        return PR_TRUE; // there is a row span out of the region
      }
    }
  }
  if (aStartColIndex > 0) {
    for (PRInt32 rowX = aStartRowIndex; rowX <= aEndRowIndex; rowX++) {
      CellData* cellData = GetDataAt(aMap, rowX, aStartColIndex, PR_TRUE);
      if (cellData && (cellData->IsColSpan())) {
        return PR_TRUE; // there is a col span into the region
      }
      nsVoidArray* row = (nsVoidArray *)(mRows.SafeElementAt(rowX));
      if (row) {
        cellData = (CellData *)(row->SafeElementAt(aEndColIndex + 1));
        if (cellData && (cellData->IsColSpan())) {
          return PR_TRUE; // there is a col span out of the region 
        }
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
  PRInt32 numCols = aMap.GetColCount();
  if (aColIndexBefore >= numCols) {
    NS_ASSERTION(PR_FALSE, "bad arg in nsCellMap::InsertCellAt");
    return;
  }

  // get the starting col index of the 1st new cells 
  PRInt32 startColIndex;
  for (startColIndex = aColIndexBefore + 1; startColIndex < numCols; startColIndex++) {
    CellData* data = GetDataAt(aMap, aRowIndex, startColIndex, PR_TRUE);
    if (!data || data->IsOrig()) { // stop unless it is a span
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
    PRInt32 rowSpan2 = GetRowSpanForNewCell(*cell, aRowIndex, zeroRowSpan);
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
    spansCauseRebuild = CellsSpanInOrOut(aMap, aRowIndex, aRowIndex + rowSpan - 1, 
                                         startColIndex, numCols - 1);
  }

  if (spansCauseRebuild) {
    RebuildConsideringCells(aMap, &aCellFrames, aRowIndex, startColIndex, PR_TRUE, aDamageArea);
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
  PRInt32 numNewRows  = aRowFrames.Count();
  PRInt32 endRowIndex = startRowIndex + numNewRows - 1;

  // create the new rows first
  if (!Grow(aMap, numNewRows, startRowIndex)) {
    return;
  }
  mRowCount += numNewRows;

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
  PRInt32 endRowIndex = aRowIndex + aRowSpan - 1;
  PRInt32 startColIndex = aColIndex;
  PRInt32 endColIndex = aColIndex;
  PRInt32 numCells = aCellFrames.Count();
  PRInt32 totalColSpan = 0;

  // add cellData entries for the space taken up by the new cells
  for (PRInt32 cellX = 0; cellX < numCells; cellX++) {
    nsTableCellFrame* cellFrame = (nsTableCellFrame*) aCellFrames.ElementAt(cellX);
    CellData* origData = (aMap.mBCInfo) ? new BCCellData(cellFrame) : new CellData(cellFrame); // the originating cell
    if (!origData) return;

    // set the starting and ending col index for the new cell
    PRBool zeroColSpan = PR_FALSE;
    PRInt32 colSpan = GetColSpanForNewCell(*cellFrame, aColIndex, aMap.GetColCount(), zeroColSpan);
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
          data = (aMap.mBCInfo) ? new BCCellData(nsnull) : new CellData(nsnull);
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
        // only count 1st spanned col of colspan=0
        SetDataAt(aMap, *data, rowX, colX, (colX == aColIndex + 1)); 
      }
    }
    cellFrame->InitCellFrame(startColIndex); 
  }
  PRInt32 damageHeight = (aRowSpanIsZero) ? aMap.GetColCount() - aRowIndex : aRowSpan;
  SetDamageArea(aColIndex, aRowIndex, 1 + endColIndex - aColIndex, damageHeight, aDamageArea); 

  PRInt32 rowX;

  // update the row and col info due to shifting
  for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
    PRInt32 numCols = row->Count();
    PRInt32 colX;
    for (colX = aColIndex + totalColSpan; colX < numCols; colX++) {
      CellData* data = (CellData*) row->ElementAt(colX);
      if (data) {
        // increase the origin and span counts beyond the spanned cols
        if (data->IsOrig()) {
          // a cell that gets moved needs adjustment as well as it new orignating col
          data->GetCellFrame()->SetColIndex(colX);
          nsColInfo* colInfo = aMap.GetColInfoAt(colX);
          colInfo->mNumCellsOrig++;
        }
        // if the colspan is 0 only count the 1st spanned col
        PRBool countAsSpan = PR_FALSE;
        if (data->IsColSpan()) {
          if ( (!data->IsZeroColSpan()) ||
               ((data->IsZeroColSpan()) && (colX > aColIndex + totalColSpan) &&
                (!IsZeroColSpan(rowX, colX - 1)))) {
            nsColInfo* colInfo = aMap.GetColInfoAt(colX);
            colInfo->mNumCellsSpan++;
            countAsSpan = PR_TRUE;
          }
        }
        
        // decrease the origin and span counts within the spanned cols
        PRInt32 colX2 = colX - totalColSpan;
        nsColInfo* colInfo2 = aMap.GetColInfoAt(colX2);
        if (data->IsOrig()) {
          // the old originating col of a moved cell needs adjustment
          colInfo2->mNumCellsOrig--;
        }
        else if (countAsSpan) {
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
  PRInt32 endRowIndex = aStartRowIndex + aNumRowsToRemove - 1;
  PRInt32 colCount = aMap.GetColCount();
  for (PRInt32 rowX = endRowIndex; rowX >= aStartRowIndex; --rowX) {
    nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(rowX));
    PRInt32 colX;
    for (colX = 0; colX < colCount; colX++) {
      CellData* data = (CellData *) row->SafeElementAt(colX);
      if (data) {
        // Adjust the column counts.
        if (data->IsOrig()) {
          // Decrement the column count.
          nsColInfo* colInfo = aMap.GetColInfoAt(colX);
          colInfo->mNumCellsOrig--;
        }
        // colspan=0 is only counted as a spanned cell in the 1st col it spans
        else if (data->IsColSpan()) {
          if ( (!data->IsZeroColSpan()) ||
               ((data->IsZeroColSpan()) && (rowX == aStartRowIndex) && (!IsZeroColSpan(rowX, colX - 1)))) {
            nsColInfo* colInfo = aMap.GetColInfoAt(colX);
            colInfo->mNumCellsSpan--;
          }
        }
      }
    }

    PRInt32 rowLength = row->Count();
    // Delete our row information.
    for (colX = 0; colX < rowLength; colX++) {
      CellData* data = (CellData *)(row->ElementAt(colX));
      if (data) {
        delete data;
      }
    }

    mRows.RemoveElementAt(rowX);
    delete row;

    // Decrement our row and next available index counts.
    mRowCount--;
  }
  aMap.RemoveColsAtEnd();

  SetDamageArea(0, aStartRowIndex, aMap.GetColCount(), 0, aDamageArea); 
}

PRInt32 nsCellMap::GetColSpanForNewCell(nsTableCellFrame& aCellFrameToAdd, 
                                        PRInt32           aColIndex,
                                        PRInt32           aNumColsInTable,
                                        PRBool&           aIsZeroColSpan)
{
  aIsZeroColSpan = PR_FALSE;
  PRInt32 colSpan = aCellFrameToAdd.GetColSpan();
  if (0 == colSpan) {
    // use a min value for a zero colspan to make computations easier elsewhere 
    colSpan = PR_MAX(MIN_NUM_COLS_FOR_ZERO_COLSPAN, aNumColsInTable - aColIndex);
    aIsZeroColSpan = PR_TRUE;
  }
  return colSpan;
}

PRInt32 nsCellMap::GetEffectiveColSpan(nsTableCellMap& aMap,
                                       PRInt32         aRowIndex,
                                       PRInt32         aColIndex,
                                       PRBool&         aZeroColSpan)
{
  PRInt32 numColsInTable = aMap.GetColCount();
  aZeroColSpan = PR_FALSE;
  PRInt32 colSpan = 1;
  nsVoidArray* row = (nsVoidArray *)(mRows.SafeElementAt(aRowIndex));
  if (row) {
    PRInt32 colX;
    CellData* data;
    PRInt32 maxCols = numColsInTable;
    PRBool hitOverlap = PR_FALSE; // XXX this is not ever being set to PR_TRUE
    for (colX = aColIndex + 1; colX < maxCols; colX++) {
      data = GetDataAt(aMap, aRowIndex, colX, PR_TRUE);
      if (data) {
        // for an overlapping situation get the colspan from the originating cell and
        // use that as the max number of cols to iterate. Since this is rare, only 
        // pay the price of looking up the cell's colspan here.
        if (!hitOverlap && data->IsOverlap()) {
          CellData* origData = GetDataAt(aMap, aRowIndex, aColIndex, PR_TRUE);
          if (origData->IsOrig()) {
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
  }
  return colSpan;
}

PRInt32 
nsCellMap::GetRowSpanForNewCell(nsTableCellFrame& aCellFrameToAdd, 
                                PRInt32           aRowIndex,
                                PRBool&           aIsZeroRowSpan)
{
  aIsZeroRowSpan = PR_FALSE;
  PRInt32 rowSpan = aCellFrameToAdd.GetRowSpan();
  if (0 == rowSpan) {
    // use a min value of 2 for a zero rowspan to make computations easier elsewhere 
    rowSpan = PR_MAX(2, mRows.Count() - aRowIndex);
    aIsZeroRowSpan = PR_TRUE;
  }
  return rowSpan;
}

PRBool nsCellMap::HasMoreThanOneCell(nsTableCellMap& aMap,
                                     PRInt32       aRowIndex)
{
  nsVoidArray* row = (nsVoidArray *)(mRows.SafeElementAt(aRowIndex));
  if (row) {
    PRInt32 maxColIndex = row->Count();
    PRInt32 count = 0;
    PRInt32 colIndex;
    for (colIndex = 0; colIndex < maxColIndex; colIndex++) {
      CellData* cellData = GetDataAt(aMap, aRowIndex, colIndex, PR_FALSE);
      if (cellData && (cellData->GetCellFrame() || cellData->IsRowSpan()))
        count++;
      if (count > 1)
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

PRInt32 nsCellMap::GetRowSpan(nsTableCellMap& aMap,
                              PRInt32         aRowIndex,
                              PRInt32         aColIndex,
                              PRBool          aGetEffective,
                              PRBool&         aZeroRowSpan)
{
  aZeroRowSpan = PR_FALSE;
  PRInt32 rowSpan = 1;
  PRInt32 rowCount = (aGetEffective) ? mRowCount : mRows.Count();
  PRInt32 rowX;
  for (rowX = aRowIndex + 1; rowX < rowCount; rowX++) {
    CellData* data = GetDataAt(aMap, rowX, aColIndex, PR_TRUE);
    if (data) {
      if (data->IsRowSpan()) {
        rowSpan++;
        if (data->IsZeroRowSpan()) {
          aZeroRowSpan = PR_TRUE;
        }
      }
      else {
        break;
      }
    } 
    else break;
  }
  if (aZeroRowSpan && (rowX < rowCount)) {
    rowSpan += rowCount - rowX;
  }
  return rowSpan;
}

void nsCellMap::ShrinkWithoutCell(nsTableCellMap&   aMap,
                                  nsTableCellFrame& aCellFrame,
                                  PRInt32           aRowIndex,
                                  PRInt32           aColIndex,
                                  nsRect&           aDamageArea)
{
  PRInt32 colX, rowX;

  // get the rowspan and colspan from the cell map since the content may have changed
  PRBool  zeroRowSpan, zeroColSpan;
  PRInt32 numCols = aMap.GetColCount();
  PRInt32 rowSpan = GetRowSpan(aMap, aRowIndex, aColIndex, PR_FALSE, zeroRowSpan);
  PRInt32 colSpan = GetEffectiveColSpan(aMap, aRowIndex, aColIndex, zeroColSpan);
  PRInt32 endRowIndex = aRowIndex + rowSpan - 1;
  PRInt32 endColIndex = aColIndex + colSpan - 1;

  SetDamageArea(aColIndex, aRowIndex, 1 + endColIndex - aColIndex, 1 + endRowIndex - aRowIndex, aDamageArea); 

  // adjust the col counts due to the deleted cell before removing it
  for (colX = aColIndex; colX <= endColIndex; colX++) {
    nsColInfo* colInfo = aMap.GetColInfoAt(colX);
    if (colX == aColIndex) {
      colInfo->mNumCellsOrig--;
    }
    // a colspan=0 cell is only counted as a spanner in the 1st col it spans
    else if (!zeroColSpan || (zeroColSpan && (colX == aColIndex + 1))) {
      colInfo->mNumCellsSpan--;
    }
  }

  // remove the deleted cell and cellData entries for it
  for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
    for (colX = endColIndex; colX >= aColIndex; colX--) {
      CellData* doomedData = (CellData*) row->ElementAt(colX);
      delete doomedData;
      row->RemoveElementAt(colX);
    }
  }

  numCols = aMap.GetColCount();

  // update the row and col info due to shifting
  for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
    PRInt32 rowCount = row->Count();
    for (colX = aColIndex; colX < numCols - colSpan; colX++) {
      CellData* data = (colX < rowCount) ? (CellData*)row->ElementAt(colX) : nsnull;
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
        // a colspan=0 cell is only counted as a spanner in the 1st col it spans
        else if (data->IsColSpan()) {
          if ( (!data->IsZeroColSpan()) ||
               ((data->IsZeroColSpan()) && (rowX == aRowIndex) && (!IsZeroColSpan(rowX, colX - 1)))) {
            // a cell that gets moved to the left needs adjustment in its new location 
            nsColInfo* colInfo = aMap.GetColInfoAt(colX);
            colInfo->mNumCellsSpan++;
            // a cell that gets moved to the left needs adjustment in its old location 
            colInfo = aMap.GetColInfoAt(colX + colSpan);
            if (colInfo) {
              colInfo->mNumCellsSpan--;
            }
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
                                  PRBool          aNumRowsToRemove,
                                  nsRect&         aDamageArea)
{
  // copy the old cell map into a new array
  PRInt32 numOrigRows = mRows.Count();
  PRInt32 numOrigCols = aMap.GetColCount();
  void** origRows = new void*[numOrigRows];
  if (!origRows) return;
  PRInt32 rowX, colX;
  // copy the orig rows
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowX);
    origRows[rowX] = row;
  }
  for (colX = 0; colX < numOrigCols; colX++) {
    nsColInfo* colInfo = aMap.GetColInfoAt(colX);
    colInfo->mNumCellsOrig = 0;
  }

  mRows.Clear();
  mRowCount = 0;
  if (aRowsToInsert) { 
    Grow(aMap, numOrigRows); 
  }

  // put back the rows before the affected ones just as before
  for (rowX = 0; rowX < aStartRowIndex; rowX++) {
    nsVoidArray* row = (nsVoidArray *)origRows[rowX];
    PRInt32 numCols = row->Count();
    for (colX = 0; colX < numCols; colX++) {
      // put in the original cell from the cell map
      CellData* data = (CellData*) row->ElementAt(colX);
      if (data && data->IsOrig()) {
        AppendCell(aMap, data->GetCellFrame(), rowX, PR_FALSE, aDamageArea);
      }
    }
  }
  PRInt32 copyStartRowIndex;
  if (aRowsToInsert) {
    // add in the new cells and create rows if necessary
    PRInt32 numNewRows = aRowsToInsert->Count();
    rowX = aStartRowIndex;
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
      if (data && data->IsOrig()) {
        AppendCell(aMap, data->GetCellFrame(), rowX, PR_FALSE, aDamageArea);
      }
    }
    rowX++;
  }

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

  SetDamageArea(0, 0, aMap.GetColCount(), GetRowCount(), aDamageArea); 
}

void nsCellMap::RebuildConsideringCells(nsTableCellMap& aMap,
                                        nsVoidArray*    aCellFrames,
                                        PRInt32         aRowIndex,
                                        PRInt32         aColIndex,
                                        PRBool          aInsert,
                                        nsRect&         aDamageArea)
{
  // copy the old cell map into a new array
  PRInt32 mRowCountOrig = mRowCount;
  PRInt32 numOrigRows   = mRows.Count();
  PRInt32 numOrigCols   = aMap.GetColCount();
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
  
  Grow(aMap, numOrigRows);

  PRInt32 numNewCells = (aCellFrames) ? aCellFrames->Count() : 0;
  // build the new cell map 
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    nsVoidArray* row = (nsVoidArray *)origRows[rowX];
    for (PRInt32 colX = 0; colX < numOrigCols; colX++) {
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
      CellData* data = (CellData*) row->SafeElementAt(colX);
      if (data && data->IsOrig()) {
        AppendCell(aMap, data->GetCellFrame(), rowX, PR_FALSE, aDamageArea);
      }
    }
  }

  // For for cell deletion, since the row is not being deleted, 
  // keep mRowCount the same as before. 
  mRowCount = PR_MAX(mRowCount, mRowCountOrig);

  // delete the old cell map
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    nsVoidArray* row = (nsVoidArray *)origRows[rowX];
    PRInt32 len = row->Count();
    for (PRInt32 colX = 0; colX < len; colX++) {
      CellData* data = (CellData*) row->SafeElementAt(colX);
      if(data)
        delete data;
    }
    delete row;
  }
  delete [] origRows;

  SetDamageArea(0, 0, aMap.GetColCount(), GetRowCount(), aDamageArea); 
}

void nsCellMap::RemoveCell(nsTableCellMap&   aMap,
                           nsTableCellFrame* aCellFrame,
                           PRInt32           aRowIndex,
                           nsRect&           aDamageArea)
{
  PRInt32 numRows = mRows.Count();
  if ((aRowIndex < 0) || (aRowIndex >= numRows)) {
    NS_ASSERTION(PR_FALSE, "bad arg in nsCellMap::RemoveCell");
    return;
  }
  PRInt32 numCols = aMap.GetColCount();

  // get the starting col index of the cell to remove
  PRInt32 startColIndex;
  for (startColIndex = 0; startColIndex < numCols; startColIndex++) {
    CellData* data = GetDataAt(aMap, aRowIndex, startColIndex, PR_FALSE);
    if (data && (data->IsOrig()) && (aCellFrame == data->GetCellFrame())) {
      break; // we found the col index
    }
  }

  PRBool isZeroRowSpan;
  PRInt32 rowSpan = GetRowSpan(aMap, aRowIndex, startColIndex, PR_FALSE, isZeroRowSpan);
  // record whether removing the cells is going to cause complications due 
  // to existing row spans, col spans or table sizing. 
  PRBool spansCauseRebuild = CellsSpanInOrOut(aMap, aRowIndex, aRowIndex + rowSpan - 1, 
                                              startColIndex, numCols - 1);
  // XXX if the cell has a col span to the end of the map, and the end has no originating 
  // cells, we need to assume that this the only such cell, and rebuild so that there are 
  // no extraneous cols at the end. The same is true for removing rows.
  if (!aCellFrame->GetRowSpan() || !aCellFrame->GetColSpan())
    spansCauseRebuild = PR_TRUE;

  if (spansCauseRebuild) {
    RebuildConsideringCells(aMap, nsnull, aRowIndex, startColIndex, PR_FALSE, aDamageArea);
  }
  else {
    ShrinkWithoutCell(aMap, *aCellFrame, aRowIndex, startColIndex, aDamageArea);
  }
}

#ifdef NS_DEBUG
void nsCellMap::Dump(PRBool aIsBorderCollapse) const
{
  printf("\n  ***** START GROUP CELL MAP DUMP ***** %p\n", this);
  PRInt32 mapRowCount = mRows.Count();
  printf("  mapRowCount=%d tableRowCount=%d \n", mapRowCount, mRowCount);

  PRInt32 rowIndex, colIndex;
  for (rowIndex = 0; rowIndex < mapRowCount; rowIndex++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rowIndex);
    printf("  row %d : ", rowIndex);
    PRInt32 colCount = row->Count();
    for (colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = (CellData *)row->ElementAt(colIndex);
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
          BCCellData* cd = (BCCellData *)row->ElementAt(colIndex);
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
  PRInt32 cellCount = 0;
  for (PRInt32 rIndex = 0; rIndex < mapRowCount; rIndex++) {
    nsVoidArray* row = (nsVoidArray *)mRows.ElementAt(rIndex);
    PRInt32 colCount = row->Count();
    printf("  ");
    for (colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = (CellData *)row->ElementAt(colIndex);
      if (cd) {
        if (cd->IsOrig()) {
          nsTableCellFrame* cellFrame = cd->GetCellFrame();
          PRInt32 cellFrameColIndex;
          cellFrame->GetColIndex(cellFrameColIndex);
          printf("C%d,%d=%p(%d)  ", rIndex, colIndex, cellFrame, cellFrameColIndex);
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
  nsVoidArray* row = (nsVoidArray*)mRows.SafeElementAt(aRowIndex);
  if (row) {
    CellData* data = (CellData*)row->SafeElementAt(aColIndex);
    if (data && data->IsZeroColSpan()) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void 
nsCellMap::AdjustForZeroSpan(nsTableCellMap& aMap,
                             PRInt32         aRowIndex,
                             PRInt32         aColIndex)
{
  PRInt32 numColsInTable = aMap.GetColCount();
  CellData* data = GetDataAt(aMap, aRowIndex, aColIndex, PR_FALSE);
  if (!data) return;

  nsTableCellFrame* cell = (data->IsOrig()) ? data->GetCellFrame() : nsnull;
  if (!cell) return;

  PRInt32 cellRowSpan = cell->GetRowSpan();
  PRInt32 cellColSpan = cell->GetColSpan();

  PRInt32 endRowIndex = (0 == cell->GetRowSpan()) ? mRows.Count() - 1 : aRowIndex + cellRowSpan - 1;
  PRInt32 endColIndex = (0 == cell->GetColSpan()) ? numColsInTable - 1 : aColIndex + cellColSpan - 1;
  // if there is both a rowspan=0 and colspan=0 then only expand the cols to a minimum
  if ((0 == cellRowSpan) && (0 == cellColSpan)) {
    endColIndex = aColIndex + MIN_NUM_COLS_FOR_ZERO_COLSPAN - 1;
  }

  // Create span CellData objects filling out the rows to the end of the
  // map if the rowspan is 0, and/or filling out the cols to the end of 
  // table if the colspan is 0. If there is both a rowspan=0 and colspan=0
  // then only fill out the cols to a minimum value.
  for (PRInt32 colX = aColIndex; colX <= endColIndex; colX++) {
    PRInt32 rowX;
    // check to see if there is any cells originating after the cols 
    PRBool cellsOrig = PR_FALSE;
    if (colX >= aColIndex + MIN_NUM_COLS_FOR_ZERO_COLSPAN - 1) {
      for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
        CellData* cellData = GetDataAt(aMap, rowX, colX, PR_FALSE);
        if (cellData && cellData->IsOrig()) {
          cellsOrig = PR_TRUE;
          break; // there are cells in this col, so don't consider it
        }
      }
    }
    if (cellsOrig) break;
    for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
      if ((colX > aColIndex) || (rowX > aRowIndex)) {
        CellData* oldData = GetDataAt(aMap, rowX, colX, PR_FALSE); 
        if (!oldData) {
          CellData* newData = (aMap.mBCInfo) ? new BCCellData(nsnull) : new CellData(nsnull);
          if (!newData) return;
          if (colX > aColIndex) {
            newData->SetColSpanOffset(colX - aColIndex);
            newData->SetZeroColSpan(PR_TRUE);
          }
          if (rowX > aRowIndex) {
            newData->SetRowSpanOffset(rowX - aRowIndex);
            newData->SetZeroRowSpan(PR_TRUE);
          }
          // colspan=0 is only counted as spanning the 1st col to the right of its origin
          SetDataAt(aMap, *newData, rowX, colX, (colX == aColIndex + 1));
        }
      }
    }
  }
}

CellData* 
nsCellMap::GetDataAt(nsTableCellMap& aMap,
                     PRInt32         aMapRowIndex, 
                     PRInt32         aColIndex,
                     PRBool          aUpdateZeroSpan)
{
  PRInt32 numColsInTable = aMap.GetColCount();
  if ((aMapRowIndex < 0) || (aMapRowIndex >= mRows.Count())) {
    return nsnull;
  }

  nsVoidArray* row = (nsVoidArray *)(mRows.ElementAt(aMapRowIndex));
  if (!row) return nsnull;

  CellData* data = (CellData *)(row->SafeElementAt(aColIndex));
  if (!data && aUpdateZeroSpan) {
    PRBool didZeroExpand = PR_FALSE;
    // check for special zero row span
    PRInt32 prevRowX = aMapRowIndex - 1;
    // find the last non null data in the same col
    for ( ; prevRowX > 0; prevRowX--) {
      nsVoidArray* prevRow = (nsVoidArray *)(mRows.ElementAt(prevRowX));
      CellData* prevData = (CellData *)(prevRow->SafeElementAt(aColIndex));
      if (prevData) {
        if (prevData->IsZeroRowSpan()) {
          PRInt32 rowIndex = prevRowX - prevData->GetRowSpanOffset();
          PRInt32 colIndex = 0;
          // if there is a colspan and no overlap then the rowspan offset 
          // and colspan offset point to the same cell
          if ((prevData->IsColSpan()) && (!prevData->IsOverlap())) {
            colIndex = prevData->GetColSpanOffset();
          }
          AdjustForZeroSpan(aMap, rowIndex, colIndex);
          didZeroExpand = PR_TRUE;
        }
        break;
      }
    }

    // check for special zero col span
    if (!didZeroExpand && (aColIndex > 0) && (aColIndex < numColsInTable)) { 
      PRInt32 prevColX = aColIndex - 1;
      // find the last non null data in the same row
      for ( ; prevColX > 0; prevColX--) {
        CellData* prevData = GetDataAt(aMap, aMapRowIndex, prevColX, PR_FALSE);
        if (prevData) {
          if (prevData->IsZeroColSpan()) {
            PRInt32 colIndex = prevColX - prevData->GetColSpanOffset();
            // if there were also a rowspan, it would have been handled above
            AdjustForZeroSpan(aMap, aMapRowIndex, colIndex);
            didZeroExpand = PR_TRUE;
          }
          break;
        }
      }
    }

    // if zero span adjustments were made the data may be available now
    if (!data && didZeroExpand) {
      data = GetDataAt(aMap, aMapRowIndex, aColIndex, PR_FALSE);
    }
  }
  return data;
}

// only called if the cell at aMapRowIndex, aColIndex is null
void nsCellMap::SetDataAt(nsTableCellMap& aMap,
                          CellData&       aNewCell, 
                          PRInt32         aMapRowIndex, 
                          PRInt32         aColIndex,
                          PRBool          aCountZeroSpanAsSpan)
{
  nsVoidArray* row = (nsVoidArray *)(mRows.SafeElementAt(aMapRowIndex));
  if (row) {
    // the table map may need cols added
    PRInt32 numColsToAdd = aColIndex + 1 - aMap.GetColCount();
    if (numColsToAdd > 0) {
      aMap.AddColsAtEnd(numColsToAdd);
    }
    // the row may need cols added
    numColsToAdd = aColIndex + 1 - row->Count();
    if (numColsToAdd > 0) {
      GrowRow(*row, numColsToAdd);
    }

    CellData* doomedData = (CellData*)row->ElementAt(aColIndex);
    delete doomedData;

    row->ReplaceElementAt(&aNewCell, aColIndex);
    // update the originating cell counts if cell originates in this row, col
    nsColInfo* colInfo = aMap.GetColInfoAt(aColIndex);
    if (colInfo) {
      if (aNewCell.IsOrig()) { 
        colInfo->mNumCellsOrig++;
      }
      else if ((aNewCell.IsColSpan()) && 
               (!aNewCell.IsZeroColSpan() || aCountZeroSpanAsSpan)) {
        colInfo->mNumCellsSpan++;
      }
    }
    else NS_ASSERTION(PR_FALSE, "SetDataAt called with col index > table map num cols");
  }
  else NS_ASSERTION(PR_FALSE, "SetDataAt called with row index > num rows");
}

nsTableCellFrame* 
nsCellMap::GetCellInfoAt(nsTableCellMap& aMap,
                         PRInt32         aRowX, 
                         PRInt32         aColX, 
                         PRBool*         aOriginates, 
                         PRInt32*        aColSpan)
{
  if (aOriginates) {
    *aOriginates = PR_FALSE;
  }
  CellData* data = GetDataAt(aMap, aRowX, aColX, PR_TRUE);
  nsTableCellFrame* cellFrame = nsnull;  
  if (data) {
    if (data->IsOrig()) {
      cellFrame = data->GetCellFrame();
      if (aOriginates)
        *aOriginates = PR_TRUE;
      if (cellFrame && aColSpan) {
        PRInt32 initialColIndex;
        cellFrame->GetColIndex(initialColIndex);
        PRBool zeroSpan;
        *aColSpan = GetEffectiveColSpan(aMap, aRowX, initialColIndex, zeroSpan);
      }
    }
    else {
      cellFrame = GetCellFrame(aRowX, aColX, *data, PR_TRUE);
      if (aColSpan)
        *aColSpan = 0;
    }
  }
  return cellFrame;
}
  

PRBool nsCellMap::RowIsSpannedInto(nsTableCellMap& aMap,
                                   PRInt32         aRowIndex)
{
  PRInt32 numColsInTable = aMap.GetColCount();
  if ((0 > aRowIndex) || (aRowIndex >= mRowCount)) {
    return PR_FALSE;
  }
  for (PRInt32 colIndex = 0; colIndex < numColsInTable; colIndex++) {
    CellData* cd = GetDataAt(aMap, aRowIndex, colIndex, PR_TRUE);
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

PRBool nsCellMap::RowHasSpanningCells(nsTableCellMap& aMap,
                                      PRInt32         aRowIndex)
{
  PRInt32 numColsInTable = aMap.GetColCount();
  if ((0 > aRowIndex) || (aRowIndex >= mRowCount)) {
    return PR_FALSE;
  }
  if (aRowIndex != mRowCount - 1) {
    // aRowIndex is not the last row, so we check the next row after aRowIndex for spanners
    for (PRInt32 colIndex = 0; colIndex < numColsInTable; colIndex++) {
      CellData* cd = GetDataAt(aMap, aRowIndex, colIndex, PR_TRUE);
      if (cd && (cd->IsOrig())) { // cell originates 
        CellData* cd2 = GetDataAt(aMap, aRowIndex + 1, colIndex, PR_TRUE);
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

PRBool nsCellMap::ColHasSpanningCells(nsTableCellMap& aMap,
                                      PRInt32         aColIndex)
{
  PRInt32 numColsInTable = aMap.GetColCount();
  NS_PRECONDITION (aColIndex < numColsInTable, "bad col index arg");
  if ((0 > aColIndex) || (aColIndex >= numColsInTable - 1)) 
    return PR_FALSE;
 
  for (PRInt32 rowIndex = 0; rowIndex < mRowCount; rowIndex++) {
    CellData* cd = GetDataAt(aMap, rowIndex, aColIndex, PR_TRUE);
    if (cd && (cd->IsOrig())) { // cell originates 
      CellData* cd2 = GetDataAt(aMap, rowIndex, aColIndex +1, PR_TRUE);
      if (cd2 && cd2->IsColSpan()) { // cd2 is spanned by a col
        if (cd->GetCellFrame() == GetCellFrame(rowIndex , aColIndex + 1, *cd2, PR_FALSE)) {
          return PR_TRUE;
        }
      }
    }
  }
  return PR_FALSE;
}
