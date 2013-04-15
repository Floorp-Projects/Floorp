/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "nsCellMap.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableRowGroupFrame.h"
#include <algorithm>


static void
SetDamageArea(int32_t aXOrigin,
              int32_t aYOrigin,
              int32_t aWidth,
              int32_t aHeight,
              nsIntRect& aDamageArea)
{
  NS_ASSERTION(aXOrigin >= 0, "negative col index");
  NS_ASSERTION(aYOrigin >= 0, "negative row index");
  NS_ASSERTION(aWidth >= 0, "negative horizontal damage");
  NS_ASSERTION(aHeight >= 0, "negative vertical damage");
  aDamageArea.x      = aXOrigin;
  aDamageArea.y      = aYOrigin;
  aDamageArea.width  = aWidth;
  aDamageArea.height = aHeight;
}
 
// Empty static array used for SafeElementAt() calls on mRows.
static nsCellMap::CellDataArray * sEmptyRow;

// CellData

CellData::CellData(nsTableCellFrame* aOrigCell)
{
  MOZ_COUNT_CTOR(CellData);
  MOZ_STATIC_ASSERT(sizeof(mOrigCell) == sizeof(mBits),
                    "mOrigCell and mBits must be the same size");
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
                               bool            aBorderCollapse)
:mTableFrame(aTableFrame), mFirstMap(nullptr), mBCInfo(nullptr)
{
  MOZ_COUNT_CTOR(nsTableCellMap);

  nsTableFrame::RowGroupArray orderedRowGroups;
  aTableFrame.OrderRowGroups(orderedRowGroups);

  nsTableRowGroupFrame* prior = nullptr;
  for (uint32_t rgX = 0; rgX < orderedRowGroups.Length(); rgX++) {
    nsTableRowGroupFrame* rgFrame = orderedRowGroups[rgX];
    InsertGroupCellMap(rgFrame, prior);
    prior = rgFrame;
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

  if (mBCInfo) {
    DeleteRightBottomBorders();
    delete mBCInfo;
  }
}

// Get the bcData holding the border segments of the right edge of the table
BCData*
nsTableCellMap::GetRightMostBorder(int32_t aRowIndex)
{
  if (!mBCInfo) ABORT1(nullptr);

  int32_t numRows = mBCInfo->mRightBorders.Length();
  if (aRowIndex < numRows) {
    return &mBCInfo->mRightBorders.ElementAt(aRowIndex);
  }

  if (!mBCInfo->mRightBorders.SetLength(aRowIndex+1))
    ABORT1(nullptr);
  return &mBCInfo->mRightBorders.ElementAt(aRowIndex);
}

// Get the bcData holding the border segments of the bottom edge of the table
BCData*
nsTableCellMap::GetBottomMostBorder(int32_t aColIndex)
{
  if (!mBCInfo) ABORT1(nullptr);

  int32_t numCols = mBCInfo->mBottomBorders.Length();
  if (aColIndex < numCols) {
    return &mBCInfo->mBottomBorders.ElementAt(aColIndex);
  }

  if (!mBCInfo->mBottomBorders.SetLength(aColIndex+1))
    ABORT1(nullptr);
  return &mBCInfo->mBottomBorders.ElementAt(aColIndex);
}

// delete the borders corresponding to the right and bottom edges of the table
void
nsTableCellMap::DeleteRightBottomBorders()
{
  if (mBCInfo) {
    mBCInfo->mBottomBorders.Clear();
    mBCInfo->mRightBorders.Clear();
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

void nsTableCellMap::InsertGroupCellMap(nsTableRowGroupFrame*  aNewGroup,
                                        nsTableRowGroupFrame*& aPrevGroup)
{
  nsCellMap* newMap = new nsCellMap(aNewGroup, mBCInfo != nullptr);
  nsCellMap* prevMap = nullptr;
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
      aPrevGroup = (prevMap) ? prevMap->GetRowGroup() : nullptr;
    }
    else {
      aPrevGroup = nullptr;
    }
  }
  InsertGroupCellMap(prevMap, *newMap);
}

void nsTableCellMap::RemoveGroupCellMap(nsTableRowGroupFrame* aGroup)
{
  nsCellMap* map = mFirstMap;
  nsCellMap* prior = nullptr;
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

  return nullptr;
}

nsCellMap*
nsTableCellMap::GetMapFor(const nsTableRowGroupFrame* aRowGroup,
                          nsCellMap* aStartHint) const
{
  NS_PRECONDITION(aRowGroup, "Must have a rowgroup");
  NS_ASSERTION(!aRowGroup->GetPrevInFlow(), "GetMapFor called with continuation");
  if (aStartHint) {
    nsCellMap* map = FindMapFor(aRowGroup, aStartHint, nullptr);
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
    nsTableFrame* fifTable = static_cast<nsTableFrame*>(mTableFrame.GetFirstInFlow());

    const nsStyleDisplay* display = aRowGroup->StyleDisplay();
    nsTableRowGroupFrame* rgOrig =
      (NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == display->mDisplay) ?
      fifTable->GetTHead() : fifTable->GetTFoot();
    // find the row group cell map using the original header/footer
    if (rgOrig && rgOrig != aRowGroup) {
      return GetMapFor(rgOrig, aStartHint);
    }
  }

  return nullptr;
}

void
nsTableCellMap::Synchronize(nsTableFrame* aTableFrame)
{
  nsTableFrame::RowGroupArray orderedRowGroups;
  nsAutoTArray<nsCellMap*, 8> maps;

  aTableFrame->OrderRowGroups(orderedRowGroups);
  if (!orderedRowGroups.Length()) {
    return;
  }

  // XXXbz this fails if orderedRowGroups is missing some row groups
  // (due to OOM when appending to the array, e.g. -- we leak maps in
  // that case).

  // Scope |map| outside the loop so we can use it as a hint.
  nsCellMap* map = nullptr;
  for (uint32_t rgX = 0; rgX < orderedRowGroups.Length(); rgX++) {
    nsTableRowGroupFrame* rgFrame = orderedRowGroups[rgX];
    map = GetMapFor((nsTableRowGroupFrame*)rgFrame->GetFirstInFlow(), map);
    if (map) {
      if (!maps.AppendElement(map)) {
        delete map;
        map = nullptr;
        NS_WARNING("Could not AppendElement");
        break;
      }
    }
  }
  if (maps.IsEmpty()) {
    MOZ_ASSERT(!mFirstMap);
    return;
  }

  int32_t mapIndex = maps.Length() - 1;  // Might end up -1
  nsCellMap* nextMap = maps.ElementAt(mapIndex);
  nextMap->SetNextSibling(nullptr);
  for (mapIndex-- ; mapIndex >= 0; mapIndex--) {
    nsCellMap* map = maps.ElementAt(mapIndex);
    map->SetNextSibling(nextMap);
    nextMap = map;
  }
  mFirstMap = nextMap;
}

bool
nsTableCellMap::HasMoreThanOneCell(int32_t aRowIndex) const
{
  int32_t rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->HasMoreThanOneCell(rowIndex);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  return false;
}

int32_t
nsTableCellMap::GetNumCellsOriginatingInRow(int32_t aRowIndex) const
{
  int32_t rowIndex = aRowIndex;
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
int32_t
nsTableCellMap::GetEffectiveRowSpan(int32_t aRowIndex,
                                    int32_t aColIndex) const
{
  int32_t rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->GetRowSpan(rowIndex, aColIndex, true);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  NS_NOTREACHED("Bogus row index?");
  return 0;
}

int32_t
nsTableCellMap::GetEffectiveColSpan(int32_t aRowIndex,
                                    int32_t aColIndex) const
{
  int32_t rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      bool zeroColSpan;
      return map->GetEffectiveColSpan(*this, rowIndex, aColIndex, zeroColSpan);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  NS_NOTREACHED("Bogus row index?");
  return 0;
}

nsTableCellFrame*
nsTableCellMap::GetCellFrame(int32_t   aRowIndex,
                             int32_t   aColIndex,
                             CellData& aData,
                             bool      aUseRowIfOverlap) const
{
  int32_t rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->GetCellFrame(rowIndex, aColIndex, aData, aUseRowIfOverlap);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  return nullptr;
}

nsColInfo*
nsTableCellMap::GetColInfoAt(int32_t aColIndex)
{
  int32_t numColsToAdd = aColIndex + 1 - mCols.Length();
  if (numColsToAdd > 0) {
    AddColsAtEnd(numColsToAdd);  // XXX this could fail to add cols in theory
  }
  return &mCols.ElementAt(aColIndex);
}

int32_t
nsTableCellMap::GetRowCount() const
{
  int32_t numRows = 0;
  nsCellMap* map = mFirstMap;
  while (map) {
    numRows += map->GetRowCount();
    map = map->GetNextSibling();
  }
  return numRows;
}

CellData*
nsTableCellMap::GetDataAt(int32_t aRowIndex,
                          int32_t aColIndex) const
{
  int32_t rowIndex = aRowIndex;
  nsCellMap* map = mFirstMap;
  while (map) {
    if (map->GetRowCount() > rowIndex) {
      return map->GetDataAt(rowIndex, aColIndex);
    }
    rowIndex -= map->GetRowCount();
    map = map->GetNextSibling();
  }
  return nullptr;
}

void
nsTableCellMap::AddColsAtEnd(uint32_t aNumCols)
{
  if (!mCols.AppendElements(aNumCols)) {
    NS_WARNING("Could not AppendElement");
  }
  if (mBCInfo) {
    if (!mBCInfo->mBottomBorders.AppendElements(aNumCols)) {
      NS_WARNING("Could not AppendElement");
    }
  }
}

void
nsTableCellMap::RemoveColsAtEnd()
{
  // Remove the cols at the end which don't have originating cells or cells spanning
  // into them. Only do this if the col was created as eColAnonymousCell
  int32_t numCols = GetColCount();
  int32_t lastGoodColIndex = mTableFrame.GetIndexOfLastRealCol();
  for (int32_t colX = numCols - 1; (colX >= 0) && (colX > lastGoodColIndex); colX--) {
    nsColInfo& colInfo = mCols.ElementAt(colX);
    if ((colInfo.mNumCellsOrig <= 0) && (colInfo.mNumCellsSpan <= 0))  {
      mCols.RemoveElementAt(colX);

      if (mBCInfo) {
        int32_t count = mBCInfo->mBottomBorders.Length();
        if (colX < count) {
          mBCInfo->mBottomBorders.RemoveElementAt(colX);
        }
      }
    }
    else break; // only remove until we encounter the 1st valid one
  }
}

void
nsTableCellMap::ClearCols()
{
  mCols.Clear();
  if (mBCInfo)
    mBCInfo->mBottomBorders.Clear();
}
void
nsTableCellMap::InsertRows(nsTableRowGroupFrame*       aParent,
                           nsTArray<nsTableRowFrame*>& aRows,
                           int32_t                     aFirstRowIndex,
                           bool                        aConsiderSpans,
                           nsIntRect&                  aDamageArea)
{
  int32_t numNewRows = aRows.Length();
  if ((numNewRows <= 0) || (aFirstRowIndex < 0)) ABORT0();

  int32_t rowIndex = aFirstRowIndex;
  int32_t rgStartRowIndex = 0;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    nsTableRowGroupFrame* rg = cellMap->GetRowGroup();
    if (rg == aParent) {
      cellMap->InsertRows(*this, aRows, rowIndex, aConsiderSpans,
                          rgStartRowIndex, aDamageArea);
#ifdef DEBUG_TABLE_CELLMAP
      Dump("after InsertRows");
#endif
      if (mBCInfo) {
        int32_t count = mBCInfo->mRightBorders.Length();
        if (aFirstRowIndex < count) {
          for (int32_t rowX = aFirstRowIndex; rowX < aFirstRowIndex + numNewRows; rowX++) {
            if (!mBCInfo->mRightBorders.InsertElementAt(rowX))
              ABORT0();
          }
        }
        else {
          GetRightMostBorder(aFirstRowIndex); // this will create missing entries
          for (int32_t rowX = aFirstRowIndex + 1; rowX < aFirstRowIndex + numNewRows; rowX++) {
            if (!mBCInfo->mRightBorders.AppendElement())
              ABORT0();
          }
        }
      }
      return;
    }
    int32_t rowCount = cellMap->GetRowCount();
    rgStartRowIndex += rowCount;
    rowIndex -= rowCount;
    cellMap = cellMap->GetNextSibling();
  }

  NS_ERROR("Attempt to insert row into wrong map.");
}

void
nsTableCellMap::RemoveRows(int32_t         aFirstRowIndex,
                           int32_t         aNumRowsToRemove,
                           bool            aConsiderSpans,
                           nsIntRect&      aDamageArea)
{
  int32_t rowIndex = aFirstRowIndex;
  int32_t rgStartRowIndex = 0;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    int32_t rowCount = cellMap->GetRowCount();
    if (rowCount > rowIndex) {
      cellMap->RemoveRows(*this, rowIndex, aNumRowsToRemove, aConsiderSpans,
                          rgStartRowIndex, aDamageArea);
      if (mBCInfo) {
        for (int32_t rowX = aFirstRowIndex + aNumRowsToRemove - 1; rowX >= aFirstRowIndex; rowX--) {
          if (uint32_t(rowX) < mBCInfo->mRightBorders.Length()) {
            mBCInfo->mRightBorders.RemoveElementAt(rowX);
          }
        }
      }
      break;
    }
    rgStartRowIndex += rowCount;
    rowIndex -= rowCount;
    cellMap = cellMap->GetNextSibling();
  }
#ifdef DEBUG_TABLE_CELLMAP
  Dump("after RemoveRows");
#endif
}



CellData*
nsTableCellMap::AppendCell(nsTableCellFrame& aCellFrame,
                           int32_t           aRowIndex,
                           bool              aRebuildIfNecessary,
                           nsIntRect&        aDamageArea)
{
  NS_ASSERTION(&aCellFrame == aCellFrame.GetFirstInFlow(), "invalid call on continuing frame");
  nsIFrame* rgFrame = aCellFrame.GetParent(); // get the row
  if (!rgFrame) return 0;
  rgFrame = rgFrame->GetParent();   // get the row group
  if (!rgFrame) return 0;

  CellData* result = nullptr;
  int32_t rowIndex = aRowIndex;
  int32_t rgStartRowIndex = 0;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowGroup() == rgFrame) {
      result = cellMap->AppendCell(*this, &aCellFrame, rowIndex,
                                   aRebuildIfNecessary, rgStartRowIndex,
                                   aDamageArea);
      break;
    }
    int32_t rowCount = cellMap->GetRowCount();
    rgStartRowIndex += rowCount;
    rowIndex -= rowCount;
    cellMap = cellMap->GetNextSibling();
  }
#ifdef DEBUG_TABLE_CELLMAP
  Dump("after AppendCell");
#endif
  return result;
}


void
nsTableCellMap::InsertCells(nsTArray<nsTableCellFrame*>& aCellFrames,
                            int32_t                      aRowIndex,
                            int32_t                      aColIndexBefore,
                            nsIntRect&                   aDamageArea)
{
  int32_t rowIndex = aRowIndex;
  int32_t rgStartRowIndex = 0;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    int32_t rowCount = cellMap->GetRowCount();
    if (rowCount > rowIndex) {
      cellMap->InsertCells(*this, aCellFrames, rowIndex, aColIndexBefore,
                           rgStartRowIndex, aDamageArea);
      break;
    }
    rgStartRowIndex += rowCount;
    rowIndex -= rowCount;
    cellMap = cellMap->GetNextSibling();
  }
#ifdef DEBUG_TABLE_CELLMAP
  Dump("after InsertCells");
#endif
}


void
nsTableCellMap::RemoveCell(nsTableCellFrame* aCellFrame,
                           int32_t           aRowIndex,
                           nsIntRect&        aDamageArea)
{
  if (!aCellFrame) ABORT0();
  NS_ASSERTION(aCellFrame == (nsTableCellFrame *)aCellFrame->GetFirstInFlow(),
               "invalid call on continuing frame");
  int32_t rowIndex = aRowIndex;
  int32_t rgStartRowIndex = 0;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    int32_t rowCount = cellMap->GetRowCount();
    if (rowCount > rowIndex) {
      cellMap->RemoveCell(*this, aCellFrame, rowIndex, rgStartRowIndex,
                          aDamageArea);
#ifdef DEBUG_TABLE_CELLMAP
      Dump("after RemoveCell");
#endif
      return;
    }
    rgStartRowIndex += rowCount;
    rowIndex -= rowCount;
    cellMap = cellMap->GetNextSibling();
  }
  // if we reach this point - the cell did not get removed, the caller of this routine
  // will delete the cell and the cellmap will probably hold a reference to
  // the deleted cell which will cause a subsequent crash when this cell is
  // referenced later
  NS_ERROR("nsTableCellMap::RemoveCell - could not remove cell");
}

void
nsTableCellMap::RebuildConsideringCells(nsCellMap*                   aCellMap,
                                        nsTArray<nsTableCellFrame*>* aCellFrames,
                                        int32_t                      aRowIndex,
                                        int32_t                      aColIndex,
                                        bool                         aInsert,
                                        nsIntRect&                   aDamageArea)
{
  int32_t numOrigCols = GetColCount();
  ClearCols();
  nsCellMap* cellMap = mFirstMap;
  int32_t rowCount = 0;
  while (cellMap) {
    if (cellMap == aCellMap) {
      cellMap->RebuildConsideringCells(*this, numOrigCols, aCellFrames,
                                       aRowIndex, aColIndex, aInsert);
    }
    else {
      cellMap->RebuildConsideringCells(*this, numOrigCols, nullptr, -1, 0,
                                       false);
    }
    rowCount += cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  SetDamageArea(0, 0, GetColCount(), rowCount, aDamageArea);
}

void
nsTableCellMap::RebuildConsideringRows(nsCellMap*                  aCellMap,
                                       int32_t                     aStartRowIndex,
                                       nsTArray<nsTableRowFrame*>* aRowsToInsert,
                                       int32_t                     aNumRowsToRemove,
                                       nsIntRect&                  aDamageArea)
{
  NS_PRECONDITION(!aRowsToInsert || aNumRowsToRemove == 0,
                  "Can't handle both removing and inserting rows at once");

  int32_t numOrigCols = GetColCount();
  ClearCols();
  nsCellMap* cellMap = mFirstMap;
  int32_t rowCount = 0;
  while (cellMap) {
    if (cellMap == aCellMap) {
      cellMap->RebuildConsideringRows(*this, aStartRowIndex, aRowsToInsert,
                                      aNumRowsToRemove);
    }
    else {
      cellMap->RebuildConsideringCells(*this, numOrigCols, nullptr, -1, 0,
                                       false);
    }
    rowCount += cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  SetDamageArea(0, 0, GetColCount(), rowCount, aDamageArea);
}

int32_t
nsTableCellMap::GetNumCellsOriginatingInCol(int32_t aColIndex) const
{
  int32_t colCount = mCols.Length();
  if ((aColIndex >= 0) && (aColIndex < colCount)) {
    return mCols.ElementAt(aColIndex).mNumCellsOrig;
  }
  else {
    NS_ERROR("nsCellMap::GetNumCellsOriginatingInCol - bad col index");
    return 0;
  }
}

#ifdef DEBUG
void
nsTableCellMap::Dump(char* aString) const
{
  if (aString)
    printf("%s \n", aString);
  printf("***** START TABLE CELL MAP DUMP ***** %p\n", (void*)this);
  // output col info
  int32_t colCount = mCols.Length();
  printf ("cols array orig/span-> %p", (void*)this);
  for (int32_t colX = 0; colX < colCount; colX++) {
    const nsColInfo& colInfo = mCols.ElementAt(colX);
    printf ("%d=%d/%d ", colX, colInfo.mNumCellsOrig, colInfo.mNumCellsSpan);
  }
  printf(" cols in cache %d\n", mTableFrame.GetColCache().Length());
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    cellMap->Dump(nullptr != mBCInfo);
    cellMap = cellMap->GetNextSibling();
  }
  if (nullptr != mBCInfo) {
    printf("***** bottom borders *****\n");
    nscoord       size;
    BCBorderOwner owner;
    mozilla::css::Side side;
    bool          segStart;
    bool          bevel;
    int32_t       colIndex;
    int32_t numCols = mBCInfo->mBottomBorders.Length();
    for (int32_t i = 0; i <= 2; i++) {

      printf("\n          ");
      for (colIndex = 0; colIndex < numCols; colIndex++) {
        BCData& cd = mBCInfo->mBottomBorders.ElementAt(colIndex);
        if (0 == i) {
          size = cd.GetTopEdge(owner, segStart);
          printf("t=%d%X%d ", int32_t(size), owner, segStart);
        }
        else if (1 == i) {
          size = cd.GetLeftEdge(owner, segStart);
          printf("l=%d%X%d ", int32_t(size), owner, segStart);
        }
        else {
          size = cd.GetCorner(side, bevel);
          printf("c=%d%X%d ", int32_t(size), side, bevel);
        }
      }
      BCData& cd = mBCInfo->mLowerRightCorner;
      if (0 == i) {
         size = cd.GetTopEdge(owner, segStart);
         printf("t=%d%X%d ", int32_t(size), owner, segStart);
      }
      else if (1 == i) {
        size = cd.GetLeftEdge(owner, segStart);
        printf("l=%d%X%d ", int32_t(size), owner, segStart);
      }
      else {
        size = cd.GetCorner(side, bevel);
        printf("c=%d%X%d ", int32_t(size), side, bevel);
      }
    }
    printf("\n");
  }
  printf("***** END TABLE CELL MAP DUMP *****\n");
}
#endif

nsTableCellFrame*
nsTableCellMap::GetCellInfoAt(int32_t  aRowIndex,
                              int32_t  aColIndex,
                              bool*  aOriginates,
                              int32_t* aColSpan) const
{
  int32_t rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      return cellMap->GetCellInfoAt(*this, rowIndex, aColIndex, aOriginates, aColSpan);
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  return nullptr;
}

int32_t
nsTableCellMap::GetIndexByRowAndColumn(int32_t aRow, int32_t aColumn) const
{
  int32_t index = 0;

  int32_t colCount = mCols.Length();
  int32_t rowIndex = aRow;

  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    int32_t rowCount = cellMap->GetRowCount();
    if (rowIndex >= rowCount) {
      // If the rowCount is less than the rowIndex, this means that the index is
      // not within the current map. If so, get the index of the last cell in
      // the last row.
      rowIndex -= rowCount;

      int32_t cellMapIdx = cellMap->GetHighestIndex(colCount);
      if (cellMapIdx != -1)
        index += cellMapIdx + 1;

    } else {
      // Index is in valid range for this cellmap, so get the index of rowIndex
      // and aColumn.
      int32_t cellMapIdx = cellMap->GetIndexByRowAndColumn(colCount, rowIndex,
                                                           aColumn);
      if (cellMapIdx == -1)
        return -1; // no cell at the given row and column.

      index += cellMapIdx;
      return index;  // no need to look through further maps here
    }

    cellMap = cellMap->GetNextSibling();
  }

  return -1;
}

void
nsTableCellMap::GetRowAndColumnByIndex(int32_t aIndex,
                                       int32_t *aRow, int32_t *aColumn) const
{
  *aRow = -1;
  *aColumn = -1;

  int32_t colCount = mCols.Length();

  int32_t previousRows = 0;
  int32_t index = aIndex;

  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    int32_t rowCount = cellMap->GetRowCount();
    // Determine the highest possible index in this map to see
    // if wanted index is in here.
    int32_t cellMapIdx = cellMap->GetHighestIndex(colCount);
    if (cellMapIdx == -1) {
      // The index is not within this map, increase the total row index
      // accordingly.
      previousRows += rowCount;
    } else {
      if (index > cellMapIdx) {
        // The index is not within this map, so decrease it by the cellMapIdx
        // determined index and increase the total row index accordingly.
        index -= cellMapIdx + 1;
        previousRows += rowCount;
      } else {
        cellMap->GetRowAndColumnByIndex(colCount, index, aRow, aColumn);
        // If there were previous indexes, take them into account.
        *aRow += previousRows;
        return; // no need to look any further.
      }
    }

    cellMap = cellMap->GetNextSibling();
  }
}

bool nsTableCellMap::RowIsSpannedInto(int32_t aRowIndex,
                                        int32_t aNumEffCols) const
{
  int32_t rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      return cellMap->RowIsSpannedInto(rowIndex, aNumEffCols);
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  return false;
}

bool nsTableCellMap::RowHasSpanningCells(int32_t aRowIndex,
                                           int32_t aNumEffCols) const
{
  int32_t rowIndex = aRowIndex;
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    if (cellMap->GetRowCount() > rowIndex) {
      return cellMap->RowHasSpanningCells(rowIndex, aNumEffCols);
    }
    rowIndex -= cellMap->GetRowCount();
    cellMap = cellMap->GetNextSibling();
  }
  return false;
}

void nsTableCellMap::ExpandZeroColSpans()
{
  mTableFrame.SetNeedColSpanExpansion(false); // mark the work done
  mTableFrame.SetHasZeroColSpans(false); // reset the bit, if there is a
                                            // zerospan it will be set again.
  nsCellMap* cellMap = mFirstMap;
  while (cellMap) {
    cellMap->ExpandZeroColSpans(*this);
    cellMap = cellMap->GetNextSibling();
  }
}

void
nsTableCellMap::ResetTopStart(uint8_t    aSide,
                              nsCellMap& aCellMap,
                              uint32_t   aRowIndex,
                              uint32_t   aColIndex,
                              bool       aIsLowerRight)
{
  if (!mBCInfo || aIsLowerRight) ABORT0();

  BCCellData* cellData;
  BCData* bcData = nullptr;

  switch(aSide) {
  case NS_SIDE_BOTTOM:
    aRowIndex++;
    // FALLTHROUGH
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
    // FALLTHROUGH
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
  if (bcData) {
    bcData->SetTopStart(false);
  }
}

// store the aSide border segment at coord = (aRowIndex, aColIndex). For top/left, store
// the info at coord. For bottom/left store it at the adjacent location so that it is
// top/left at that location. If the new location is at the right or bottom edge of the
// table, then store it one of the special arrays (right most borders, bottom most borders).
void
nsTableCellMap::SetBCBorderEdge(mozilla::css::Side aSide,
                                nsCellMap&    aCellMap,
                                uint32_t      aCellMapStart,
                                uint32_t      aRowIndex,
                                uint32_t      aColIndex,
                                uint32_t      aLength,
                                BCBorderOwner aOwner,
                                nscoord       aSize,
                                bool          aChanged)
{
  if (!mBCInfo) ABORT0();

  BCCellData* cellData;
  int32_t lastIndex, xIndex, yIndex;
  int32_t xPos = aColIndex;
  int32_t yPos = aRowIndex;
  int32_t rgYPos = aRowIndex - aCellMapStart;
  bool changed;

  switch(aSide) {
  case NS_SIDE_BOTTOM:
    rgYPos++;
    yPos++;
  case NS_SIDE_TOP:
    lastIndex = xPos + aLength - 1;
    for (xIndex = xPos; xIndex <= lastIndex; xIndex++) {
      changed = aChanged && (xIndex == xPos);
      BCData* bcData = nullptr;
      cellData = (BCCellData*)aCellMap.GetDataAt(rgYPos, xIndex);
      if (!cellData) {
        int32_t numRgRows = aCellMap.GetRowCount();
        if (yPos < numRgRows) { // add a dead cell data
          nsIntRect damageArea;
          cellData = (BCCellData*)aCellMap.AppendCell(*this, nullptr, rgYPos,
                                                       false, 0, damageArea);
          if (!cellData) ABORT0();
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
              nsIntRect damageArea;
              cellData = (BCCellData*)cellMap->AppendCell(*this, nullptr, 0,
                                                           false, 0,
                                                           damageArea);
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
                                  uint32_t    aCellMapStart,
                                  uint32_t    aRowIndex,
                                  uint32_t    aColIndex,
                                  mozilla::css::Side aOwner,
                                  nscoord     aSubSize,
                                  bool        aBevel,
                                  bool        aIsBottomRight)
{
  if (!mBCInfo) ABORT0();

  if (aIsBottomRight) {
    mBCInfo->mLowerRightCorner.SetCorner(aSubSize, aOwner, aBevel);
    return;
  }

  int32_t xPos = aColIndex;
  int32_t yPos = aRowIndex;
  int32_t rgYPos = aRowIndex - aCellMapStart;

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

  BCCellData* cellData = nullptr;
  BCData*     bcData   = nullptr;
  if (GetColCount() <= xPos) {
    NS_ASSERTION(xPos == GetColCount(), "program error");
    // at the right edge of the table as we checked the corner before
    NS_ASSERTION(!aIsBottomRight, "should be handled before");
    bcData = GetRightMostBorder(yPos);
  }
  else {
    cellData = (BCCellData*)aCellMap.GetDataAt(rgYPos, xPos);
    if (!cellData) {
      int32_t numRgRows = aCellMap.GetRowCount();
      if (yPos < numRgRows) { // add a dead cell data
        nsIntRect damageArea;
        cellData = (BCCellData*)aCellMap.AppendCell(*this, nullptr, rgYPos,
                                                     false, 0, damageArea);
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
            nsIntRect damageArea;
            cellData = (BCCellData*)cellMap->AppendCell(*this, nullptr, 0,
                                                         false, 0, damageArea);
          }
        }
        else { // must be at the bottom of the table
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

nsCellMap::nsCellMap(nsTableRowGroupFrame* aRowGroup, bool aIsBC)
  : mRows(8), mContentRowCount(0), mRowGroupFrame(aRowGroup),
    mNextSibling(nullptr), mIsBC(aIsBC),
    mPresContext(aRowGroup->PresContext())
{
  MOZ_COUNT_CTOR(nsCellMap);
  NS_ASSERTION(mPresContext, "Must have prescontext");
}

nsCellMap::~nsCellMap()
{
  MOZ_COUNT_DTOR(nsCellMap);

  uint32_t mapRowCount = mRows.Length();
  for (uint32_t rowX = 0; rowX < mapRowCount; rowX++) {
    CellDataArray &row = mRows[rowX];
    uint32_t colCount = row.Length();
    for (uint32_t colX = 0; colX < colCount; colX++) {
      DestroyCellData(row[colX]);
    }
  }
}

/* static */
void
nsCellMap::Init()
{
  NS_ABORT_IF_FALSE(!sEmptyRow, "How did that happen?");
  sEmptyRow = new nsCellMap::CellDataArray();
}

/* static */
void
nsCellMap::Shutdown()
{
  delete sEmptyRow;
  sEmptyRow = nullptr;
}

nsTableCellFrame*
nsCellMap::GetCellFrame(int32_t   aRowIndexIn,
                        int32_t   aColIndexIn,
                        CellData& aData,
                        bool      aUseRowIfOverlap) const
{
  int32_t rowIndex = aRowIndexIn - aData.GetRowSpanOffset();
  int32_t colIndex = aColIndexIn - aData.GetColSpanOffset();
  if (aData.IsOverlap()) {
    if (aUseRowIfOverlap) {
      colIndex = aColIndexIn;
    }
    else {
      rowIndex = aRowIndexIn;
    }
  }

  CellData* data =
    mRows.SafeElementAt(rowIndex, *sEmptyRow).SafeElementAt(colIndex);
  if (data) {
    return data->GetCellFrame();
  }
  return nullptr;
}

int32_t
nsCellMap::GetHighestIndex(int32_t aColCount)
{
  int32_t index = -1;
  int32_t rowCount = mRows.Length();
  for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    const CellDataArray& row = mRows[rowIdx];

    for (int32_t colIdx = 0; colIdx < aColCount; colIdx++) {
      CellData* data = row.SafeElementAt(colIdx);
      // No data means row doesn't have more cells.
      if (!data)
        break;

      if (data->IsOrig())
        index++;
    }
  }

  return index;
}

int32_t
nsCellMap::GetIndexByRowAndColumn(int32_t aColCount,
                                  int32_t aRow, int32_t aColumn) const
{
  if (uint32_t(aRow) >= mRows.Length())
    return -1;

  int32_t index = -1;
  int32_t lastColsIdx = aColCount - 1;

  // Find row index of the cell where row span is started.
  const CellDataArray& row = mRows[aRow];
  CellData* data = row.SafeElementAt(aColumn);
  int32_t origRow = data ? aRow - data->GetRowSpanOffset() : aRow;

  // Calculate cell index.
  for (int32_t rowIdx = 0; rowIdx <= origRow; rowIdx++) {
    const CellDataArray& row = mRows[rowIdx];
    int32_t colCount = (rowIdx == origRow) ? aColumn : lastColsIdx;

    for (int32_t colIdx = 0; colIdx <= colCount; colIdx++) {
      data = row.SafeElementAt(colIdx);
      // No data means row doesn't have more cells.
      if (!data)
        break;

      if (data->IsOrig())
        index++;
    }
  }

  // Given row and column don't point to the cell.
  if (!data)
    return -1;

  return index;
}

void
nsCellMap::GetRowAndColumnByIndex(int32_t aColCount, int32_t aIndex,
                                  int32_t *aRow, int32_t *aColumn) const
{
  *aRow = -1;
  *aColumn = -1;

  int32_t index = aIndex;
  int32_t rowCount = mRows.Length();

  for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    const CellDataArray& row = mRows[rowIdx];

    for (int32_t colIdx = 0; colIdx < aColCount; colIdx++) {
      CellData* data = row.SafeElementAt(colIdx);

      // The row doesn't have more cells.
      if (!data)
        break;

      if (data->IsOrig())
        index--;

      if (index < 0) {
        *aRow = rowIdx;
        *aColumn = colIdx;
        return;
      }
    }
  }
}

bool nsCellMap::Grow(nsTableCellMap& aMap,
                       int32_t         aNumRows,
                       int32_t         aRowIndex)
{
  NS_ASSERTION(aNumRows >= 1, "Why are we calling this?");

  // Get the number of cols we want to use for preallocating the row arrays.
  int32_t numCols = aMap.GetColCount();
  if (numCols == 0) {
    numCols = 4;
  }
  uint32_t startRowIndex = (aRowIndex >= 0) ? aRowIndex : mRows.Length();
  NS_ASSERTION(startRowIndex <= mRows.Length(), "Missing grow call inbetween");

  return mRows.InsertElementsAt(startRowIndex, aNumRows, numCols) != nullptr;
}

void nsCellMap::GrowRow(CellDataArray& aRow,
                        int32_t        aNumCols)

{
  // Have to have the cast to get the template to do the right thing.
  aRow.InsertElementsAt(aRow.Length(), aNumCols, (CellData*)nullptr);
}

void
nsCellMap::InsertRows(nsTableCellMap&             aMap,
                      nsTArray<nsTableRowFrame*>& aRows,
                      int32_t                     aFirstRowIndex,
                      bool                        aConsiderSpans,
                      int32_t                     aRgFirstRowIndex,
                      nsIntRect&                  aDamageArea)
{
  int32_t numCols = aMap.GetColCount();
  NS_ASSERTION(aFirstRowIndex >= 0, "nsCellMap::InsertRows called with negative rowIndex");
  if (uint32_t(aFirstRowIndex) > mRows.Length()) {
    // create (aFirstRowIndex - mRows.Length()) empty rows up to aFirstRowIndex
    int32_t numEmptyRows = aFirstRowIndex - mRows.Length();
    if (!Grow(aMap, numEmptyRows)) {
      return;
    }
  }

  if (!aConsiderSpans) {
    // update mContentRowCount, since non-empty rows will be added
    mContentRowCount = std::max(aFirstRowIndex, mContentRowCount);
    ExpandWithRows(aMap, aRows, aFirstRowIndex, aRgFirstRowIndex, aDamageArea);
    return;
  }

  // if any cells span into or out of the row being inserted, then rebuild
  bool spansCauseRebuild = CellsSpanInOrOut(aFirstRowIndex,
                                              aFirstRowIndex, 0, numCols - 1);

  // update mContentRowCount, since non-empty rows will be added
  mContentRowCount = std::max(aFirstRowIndex, mContentRowCount);

  // if any of the new cells span out of the new rows being added, then rebuild
  // XXX it would be better to only rebuild the portion of the map that follows the new rows
  if (!spansCauseRebuild && (uint32_t(aFirstRowIndex) < mRows.Length())) {
    spansCauseRebuild = CellsSpanOut(aRows);
  }
  if (spansCauseRebuild) {
    aMap.RebuildConsideringRows(this, aFirstRowIndex, &aRows, 0, aDamageArea);
  }
  else {
    ExpandWithRows(aMap, aRows, aFirstRowIndex, aRgFirstRowIndex, aDamageArea);
  }
}

void
nsCellMap::RemoveRows(nsTableCellMap& aMap,
                      int32_t         aFirstRowIndex,
                      int32_t         aNumRowsToRemove,
                      bool            aConsiderSpans,
                      int32_t         aRgFirstRowIndex,
                      nsIntRect&      aDamageArea)
{
  int32_t numRows = mRows.Length();
  int32_t numCols = aMap.GetColCount();

  if (aFirstRowIndex >= numRows) {
    // reduce the content based row count based on the function arguments
    // as they are known to be real rows even if the cell map did not create
    // rows for them before.
    mContentRowCount -= aNumRowsToRemove;
    return;
  }
  if (!aConsiderSpans) {
    ShrinkWithoutRows(aMap, aFirstRowIndex, aNumRowsToRemove, aRgFirstRowIndex,
                      aDamageArea);
    return;
  }
  int32_t endRowIndex = aFirstRowIndex + aNumRowsToRemove - 1;
  if (endRowIndex >= numRows) {
    NS_ERROR("nsCellMap::RemoveRows tried to remove too many rows");
    endRowIndex = numRows - 1;
  }
  bool spansCauseRebuild = CellsSpanInOrOut(aFirstRowIndex, endRowIndex,
                                              0, numCols - 1);
  if (spansCauseRebuild) {
    aMap.RebuildConsideringRows(this, aFirstRowIndex, nullptr, aNumRowsToRemove,
                                aDamageArea);
  }
  else {
    ShrinkWithoutRows(aMap, aFirstRowIndex, aNumRowsToRemove, aRgFirstRowIndex,
                      aDamageArea);
  }
}




CellData*
nsCellMap::AppendCell(nsTableCellMap&   aMap,
                      nsTableCellFrame* aCellFrame,
                      int32_t           aRowIndex,
                      bool              aRebuildIfNecessary,
                      int32_t           aRgFirstRowIndex,
                      nsIntRect&        aDamageArea,
                      int32_t*          aColToBeginSearch)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  int32_t origNumMapRows = mRows.Length();
  int32_t origNumCols = aMap.GetColCount();
  bool    zeroRowSpan = false;
  int32_t rowSpan = (aCellFrame) ? GetRowSpanForNewCell(aCellFrame, aRowIndex,
                                                        zeroRowSpan) : 1;
  // add new rows if necessary
  int32_t endRowIndex = aRowIndex + rowSpan - 1;
  if (endRowIndex >= origNumMapRows) {
    // XXXbz handle allocation failures?
    Grow(aMap, 1 + endRowIndex - origNumMapRows);
  }

  // get the first null or dead CellData in the desired row. It will equal origNumCols if there are none
  CellData* origData = nullptr;
  int32_t startColIndex = 0;
  if (aColToBeginSearch)
    startColIndex = *aColToBeginSearch;
  for (; startColIndex < origNumCols; startColIndex++) {
    CellData* data = GetDataAt(aRowIndex, startColIndex);
    if (!data)
      break;
    // The border collapse code relies on having multiple dead cell data entries
    // in a row.
    if (data->IsDead() && aCellFrame) {
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

  bool    zeroColSpan = false;
  int32_t colSpan = (aCellFrame) ?
                    GetColSpanForNewCell(*aCellFrame, zeroColSpan) : 1;
  if (zeroColSpan) {
    aMap.mTableFrame.SetHasZeroColSpans(true);
    aMap.mTableFrame.SetNeedColSpanExpansion(true);
  }

  // if the new cell could potentially span into other rows and collide with
  // originating cells there, we will play it safe and just rebuild the map
  if (aRebuildIfNecessary && (aRowIndex < mContentRowCount - 1) && (rowSpan > 1)) {
    nsAutoTArray<nsTableCellFrame*, 1> newCellArray;
    newCellArray.AppendElement(aCellFrame);
    aMap.RebuildConsideringCells(this, &newCellArray, aRowIndex, startColIndex, true, aDamageArea);
    return origData;
  }
  mContentRowCount = std::max(mContentRowCount, aRowIndex + 1);

  // add new cols to the table map if necessary
  int32_t endColIndex = startColIndex + colSpan - 1;
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

  if (aRebuildIfNecessary) {
    //the caller depends on the damageArea
    // The special case for zeroRowSpan is to adjust for the '2' in
    // GetRowSpanForNewCell.
    uint32_t height = zeroRowSpan ? endRowIndex - aRowIndex  :
                                    1 + endRowIndex - aRowIndex;
    SetDamageArea(startColIndex, aRgFirstRowIndex + aRowIndex,
                  1 + endColIndex - startColIndex, height, aDamageArea);
  }

  if (!aCellFrame) {
    return origData;
  }

  // initialize the cell frame
  aCellFrame->SetColIndex(startColIndex);

  // Create CellData objects for the rows that this cell spans. Set
  // their mOrigCell to nullptr and their mSpanData to point to data.
  for (int32_t rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    // The row at rowX will need to have at least endColIndex columns
    mRows[rowX].SetCapacity(endColIndex);
    for (int32_t colX = startColIndex; colX <= endColIndex; colX++) {
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
                cellData->SetZeroRowSpan(true);
              }
            }
          }
          if (colX > startColIndex) { // col spanning into cell
            if (!cellData->IsColSpan()) {
              if (cellData->IsRowSpan()) {
                cellData->SetOverlap(true);
              }
              cellData->SetColSpanOffset(colX - startColIndex);
              if (zeroColSpan) {
                cellData->SetZeroColSpan(true);
              }

              nsColInfo* colInfo = aMap.GetColInfoAt(colX);
              colInfo->mNumCellsSpan++;
            }
          }
        }
        else {
          cellData = AllocCellData(nullptr);
          if (!cellData) return origData;
          if (rowX > aRowIndex) {
            cellData->SetRowSpanOffset(rowX - aRowIndex);
            if (zeroRowSpan) {
              cellData->SetZeroRowSpan(true);
            }
          }
          if (colX > startColIndex) {
            cellData->SetColSpanOffset(colX - startColIndex);
            if (zeroColSpan) {
              cellData->SetZeroColSpan(true);
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
                                    int32_t         aRowIndex,
                                    int32_t         aColIndex)
{
  // if after a colspan = 0 cell another cell is appended in a row the html 4
  // spec is already violated. In principle one should then append the cell
  // after the last column but then the zero spanning cell would also have
  // to grow. The only plausible way to break this cycle is ignore the zero
  // colspan and reset the cell to colspan = 1.

  NS_ASSERTION(aOrigData && aOrigData->IsZeroColSpan(),
               "zero colspan should have been passed");
  // find the originating cellframe
  nsTableCellFrame* cell = GetCellFrame(aRowIndex, aColIndex, *aOrigData, true);
  NS_ASSERTION(cell, "originating cell not found");

  // find the clearing region
  int32_t startRowIndex = aRowIndex - aOrigData->GetRowSpanOffset();
  bool    zeroSpan;
  int32_t rowSpan = GetRowSpanForNewCell(cell, startRowIndex, zeroSpan);
  int32_t endRowIndex = startRowIndex + rowSpan;

  int32_t origColIndex = aColIndex - aOrigData->GetColSpanOffset();
  int32_t endColIndex = origColIndex +
                        GetEffectiveColSpan(aMap, startRowIndex,
                                            origColIndex, zeroSpan);
  for (int32_t colX = origColIndex +1; colX < endColIndex; colX++) {
    // Start the collapse just after the originating cell, since
    // we're basically making the originating cell act as if it
    // has colspan="1".
    nsColInfo* colInfo = aMap.GetColInfoAt(colX);
    colInfo->mNumCellsSpan -= rowSpan;

    for (int32_t rowX = startRowIndex; rowX < endRowIndex; rowX++)
    {
      CellData* data = mRows[rowX][colX];
      NS_ASSERTION(data->IsZeroColSpan(),
                   "Overwriting previous data - memory leak");
      data->Init(nullptr); // mark the cell as a dead cell.
    }
  }
}

bool nsCellMap::CellsSpanOut(nsTArray<nsTableRowFrame*>& aRows) const
{
  int32_t numNewRows = aRows.Length();
  for (int32_t rowX = 0; rowX < numNewRows; rowX++) {
    nsIFrame* rowFrame = (nsIFrame *) aRows.ElementAt(rowX);
    nsIFrame* childFrame = rowFrame->GetFirstPrincipalChild();
    while (childFrame) {
      nsTableCellFrame *cellFrame = do_QueryFrame(childFrame);
      if (cellFrame) {
        bool zeroSpan;
        int32_t rowSpan = GetRowSpanForNewCell(cellFrame, rowX, zeroSpan);
        if (zeroSpan || rowX + rowSpan > numNewRows) {
          return true;
        }
      }
      childFrame = childFrame->GetNextSibling();
    }
  }
  return false;
}

// return true if any cells have rows spans into or out of the region
// defined by the row and col indices or any cells have colspans into the region
bool nsCellMap::CellsSpanInOrOut(int32_t aStartRowIndex,
                                   int32_t aEndRowIndex,
                                   int32_t aStartColIndex,
                                   int32_t aEndColIndex) const
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

  int32_t numRows = mRows.Length(); // use the cellmap rows to determine the
                                    // current cellmap extent.
  for (int32_t colX = aStartColIndex; colX <= aEndColIndex; colX++) {
    CellData* cellData;
    if (aStartRowIndex > 0) {
      cellData = GetDataAt(aStartRowIndex, colX);
      if (cellData && (cellData->IsRowSpan())) {
        return true; // there is a row span into the region
      }
      if ((aStartRowIndex >= mContentRowCount) &&  (mContentRowCount > 0)) {
        cellData = GetDataAt(mContentRowCount - 1, colX);
        if (cellData && cellData->IsZeroRowSpan()) {
          return true;  // When we expand the zerospan it'll span into our row
        }
      }
    }
    if (aEndRowIndex < numRows - 1) { // is there anything below aEndRowIndex
      cellData = GetDataAt(aEndRowIndex + 1, colX);
      if ((cellData) && (cellData->IsRowSpan())) {
        return true; // there is a row span out of the region
      }
    }
    else {
      cellData = GetDataAt(aEndRowIndex, colX);
      if ((cellData) && (cellData->IsRowSpan()) && (mContentRowCount < numRows)) {
        return true; // this cell might be the cause of a dead row
      }
    }
  }
  if (aStartColIndex > 0) {
    for (int32_t rowX = aStartRowIndex; rowX <= aEndRowIndex; rowX++) {
      CellData* cellData = GetDataAt(rowX, aStartColIndex);
      if (cellData && (cellData->IsColSpan())) {
        return true; // there is a col span into the region
      }
      cellData = GetDataAt(rowX, aEndColIndex + 1);
      if (cellData && (cellData->IsColSpan())) {
        return true; // there is a col span out of the region
      }
    }
  }
  return false;
}

void nsCellMap::InsertCells(nsTableCellMap&              aMap,
                            nsTArray<nsTableCellFrame*>& aCellFrames,
                            int32_t                      aRowIndex,
                            int32_t                      aColIndexBefore,
                            int32_t                      aRgFirstRowIndex,
                            nsIntRect&                   aDamageArea)
{
  if (aCellFrames.Length() == 0) return;
  NS_ASSERTION(aColIndexBefore >= -1, "index out of range");
  int32_t numCols = aMap.GetColCount();
  if (aColIndexBefore >= numCols) {
    NS_ERROR("Inserting instead of appending cells indicates a serious cellmap error");
    aColIndexBefore = numCols - 1;
  }

  // get the starting col index of the 1st new cells
  int32_t startColIndex;
  for (startColIndex = aColIndexBefore + 1; startColIndex < numCols; startColIndex++) {
    CellData* data = GetDataAt(aRowIndex, startColIndex);
    if (!data || data->IsOrig() || data->IsDead()) {
      // // Not a span.  Stop.
      break;
    }
    if (data->IsZeroColSpan()) {
      // Zero colspans collapse.  Stop in this case too.
      CollapseZeroColSpan(aMap, data, aRowIndex, startColIndex);
      break;
    }
  }

  // record whether inserted cells are going to cause complications due
  // to existing row spans, col spans or table sizing.
  bool spansCauseRebuild = false;

  // check that all cells have the same row span
  int32_t numNewCells = aCellFrames.Length();
  bool zeroRowSpan = false;
  int32_t rowSpan = 0;
  for (int32_t cellX = 0; cellX < numNewCells; cellX++) {
    nsTableCellFrame* cell = aCellFrames.ElementAt(cellX);
    int32_t rowSpan2 = GetRowSpanForNewCell(cell, aRowIndex, zeroRowSpan);
    if (rowSpan == 0) {
      rowSpan = rowSpan2;
    }
    else if (rowSpan != rowSpan2) {
      spansCauseRebuild = true;
      break;
    }
  }

  // check if the new cells will cause the table to add more rows
  if (!spansCauseRebuild) {
    if (mRows.Length() < uint32_t(aRowIndex + rowSpan)) {
      spansCauseRebuild = true;
    }
  }

  if (!spansCauseRebuild) {
    spansCauseRebuild = CellsSpanInOrOut(aRowIndex, aRowIndex + rowSpan - 1,
                                         startColIndex, numCols - 1);
  }
  if (spansCauseRebuild) {
    aMap.RebuildConsideringCells(this, &aCellFrames, aRowIndex, startColIndex,
                                 true, aDamageArea);
  }
  else {
    ExpandWithCells(aMap, aCellFrames, aRowIndex, startColIndex, rowSpan,
                    zeroRowSpan, aRgFirstRowIndex, aDamageArea);
  }
}

void
nsCellMap::ExpandWithRows(nsTableCellMap&             aMap,
                          nsTArray<nsTableRowFrame*>& aRowFrames,
                          int32_t                     aStartRowIndexIn,
                          int32_t                     aRgFirstRowIndex,
                          nsIntRect&                  aDamageArea)
{
  int32_t startRowIndex = (aStartRowIndexIn >= 0) ? aStartRowIndexIn : 0;
  NS_ASSERTION(uint32_t(startRowIndex) <= mRows.Length(), "caller should have grown cellmap before");

  int32_t numNewRows  = aRowFrames.Length();
  mContentRowCount += numNewRows;

  int32_t endRowIndex = startRowIndex + numNewRows - 1;

  // shift the rows after startRowIndex down and insert empty rows that will
  // be filled via the AppendCell call below
  if (!Grow(aMap, numNewRows, startRowIndex)) {
    return;
  }


  int32_t newRowIndex = 0;
  for (int32_t rowX = startRowIndex; rowX <= endRowIndex; rowX++) {
    nsTableRowFrame* rFrame = aRowFrames.ElementAt(newRowIndex);
    // append cells
    nsIFrame* cFrame = rFrame->GetFirstPrincipalChild();
    int32_t colIndex = 0;
    while (cFrame) {
      nsTableCellFrame *cellFrame = do_QueryFrame(cFrame);
      if (cellFrame) {
        AppendCell(aMap, cellFrame, rowX, false, aRgFirstRowIndex, aDamageArea,
                   &colIndex);
      }
      cFrame = cFrame->GetNextSibling();
    }
    newRowIndex++;
  }
  // mark all following rows damaged, they might contain a previously set
  // damage area which we can not shift.
  int32_t firstDamagedRow = aRgFirstRowIndex + startRowIndex;
  SetDamageArea(0, firstDamagedRow, aMap.GetColCount(),
                aMap.GetRowCount() - firstDamagedRow, aDamageArea);
}

void nsCellMap::ExpandWithCells(nsTableCellMap&              aMap,
                                nsTArray<nsTableCellFrame*>& aCellFrames,
                                int32_t                      aRowIndex,
                                int32_t                      aColIndex,
                                int32_t                      aRowSpan, // same for all cells
                                bool                         aRowSpanIsZero,
                                int32_t                      aRgFirstRowIndex,
                                nsIntRect&                   aDamageArea)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  int32_t endRowIndex = aRowIndex + aRowSpan - 1;
  int32_t startColIndex = aColIndex;
  int32_t endColIndex = aColIndex;
  int32_t numCells = aCellFrames.Length();
  int32_t totalColSpan = 0;

  // add cellData entries for the space taken up by the new cells
  for (int32_t cellX = 0; cellX < numCells; cellX++) {
    nsTableCellFrame* cellFrame = aCellFrames.ElementAt(cellX);
    CellData* origData = AllocCellData(cellFrame); // the originating cell
    if (!origData) return;

    // set the starting and ending col index for the new cell
    bool zeroColSpan = false;
    int32_t colSpan = GetColSpanForNewCell(*cellFrame, zeroColSpan);
    if (zeroColSpan) {
      aMap.mTableFrame.SetHasZeroColSpans(true);
      aMap.mTableFrame.SetNeedColSpanExpansion(true);
    }
    totalColSpan += colSpan;
    if (cellX == 0) {
      endColIndex = aColIndex + colSpan - 1;
    }
    else {
      startColIndex = endColIndex + 1;
      endColIndex   = startColIndex + colSpan - 1;
    }

    // add the originating cell data and any cell data corresponding to row/col spans
    for (int32_t rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
      CellDataArray& row = mRows[rowX];
      // Pre-allocate all the cells we'll need in this array, setting
      // them to null.
      // Have to have the cast to get the template to do the right thing.
      int32_t insertionIndex = row.Length();
      if (insertionIndex > startColIndex) {
        insertionIndex = startColIndex;
      }
      if (!row.InsertElementsAt(insertionIndex, endColIndex - insertionIndex + 1,
                                (CellData*)nullptr) &&
          rowX == aRowIndex) {
        // Failed to insert the slots, and this is the very first row.  That
        // means that we need to clean up |origData| before returning, since
        // the cellmap doesn't own it yet.
        DestroyCellData(origData);
        return;
      }

      for (int32_t colX = startColIndex; colX <= endColIndex; colX++) {
        CellData* data = origData;
        if ((rowX != aRowIndex) || (colX != startColIndex)) {
          data = AllocCellData(nullptr);
          if (!data) return;
          if (rowX > aRowIndex) {
            data->SetRowSpanOffset(rowX - aRowIndex);
            if (aRowSpanIsZero) {
              data->SetZeroRowSpan(true);
            }
          }
          if (colX > startColIndex) {
            data->SetColSpanOffset(colX - startColIndex);
            if (zeroColSpan) {
              data->SetZeroColSpan(true);
            }
          }
        }
        SetDataAt(aMap, *data, rowX, colX);
      }
    }
    cellFrame->SetColIndex(startColIndex);
  }
  int32_t damageHeight = std::min(GetRowGroup()->GetRowCount() - aRowIndex,
                                aRowSpan);
  SetDamageArea(aColIndex, aRgFirstRowIndex + aRowIndex,
                1 + endColIndex - aColIndex, damageHeight, aDamageArea);

  int32_t rowX;

  // update the row and col info due to shifting
  for (rowX = aRowIndex; rowX <= endRowIndex; rowX++) {
    CellDataArray& row = mRows[rowX];
    uint32_t numCols = row.Length();
    uint32_t colX;
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
        int32_t colX2 = colX - totalColSpan;
        nsColInfo* colInfo2 = aMap.GetColInfoAt(colX2);
        if (data->IsOrig()) {
          // the old originating col of a moved cell needs adjustment
          colInfo2->mNumCellsOrig--;
        }
        if (data->IsColSpan()) {
          colInfo2->mNumCellsSpan--;
        }
      }
    }
  }
}

void nsCellMap::ShrinkWithoutRows(nsTableCellMap& aMap,
                                  int32_t         aStartRowIndex,
                                  int32_t         aNumRowsToRemove,
                                  int32_t         aRgFirstRowIndex,
                                  nsIntRect&      aDamageArea)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  int32_t endRowIndex = aStartRowIndex + aNumRowsToRemove - 1;
  uint32_t colCount = aMap.GetColCount();
  for (int32_t rowX = endRowIndex; rowX >= aStartRowIndex; --rowX) {
    CellDataArray& row = mRows[rowX];
    uint32_t colX;
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

    uint32_t rowLength = row.Length();
    // Delete our row information.
    for (colX = 0; colX < rowLength; colX++) {
      DestroyCellData(row[colX]);
    }

    mRows.RemoveElementAt(rowX);

    // Decrement our row and next available index counts.
    mContentRowCount--;
  }
  aMap.RemoveColsAtEnd();
  // mark all following rows damaged, they might contain a previously set
  // damage area which we can not shift.
  int32_t firstDamagedRow = aRgFirstRowIndex + aStartRowIndex;
  SetDamageArea(0, firstDamagedRow, aMap.GetColCount(),
                aMap.GetRowCount() - firstDamagedRow, aDamageArea);
}

int32_t nsCellMap::GetColSpanForNewCell(nsTableCellFrame& aCellFrameToAdd,
                                        bool&           aIsZeroColSpan) const
{
  aIsZeroColSpan = false;
  int32_t colSpan = aCellFrameToAdd.GetColSpan();
  if (0 == colSpan) {
    colSpan = 1; // set the min colspan it will be expanded later
    aIsZeroColSpan = true;
  }
  return colSpan;
}

int32_t nsCellMap::GetEffectiveColSpan(const nsTableCellMap& aMap,
                                       int32_t         aRowIndex,
                                       int32_t         aColIndex,
                                       bool&         aZeroColSpan) const
{
  int32_t numColsInTable = aMap.GetColCount();
  aZeroColSpan = false;
  int32_t colSpan = 1;
  if (uint32_t(aRowIndex) >= mRows.Length()) {
    return colSpan;
  }

  const CellDataArray& row = mRows[aRowIndex];
  int32_t colX;
  CellData* data;
  int32_t maxCols = numColsInTable;
  bool hitOverlap = false; // XXX this is not ever being set to true
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
            maxCols = std::min(aColIndex + cellFrame->GetColSpan(), maxCols);
            if (colX >= maxCols)
              break;
          }
        }
      }
      if (data->IsColSpan()) {
        colSpan++;
        if (data->IsZeroColSpan()) {
          aZeroColSpan = true;
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

int32_t
nsCellMap::GetRowSpanForNewCell(nsTableCellFrame* aCellFrameToAdd,
                                int32_t           aRowIndex,
                                bool&           aIsZeroRowSpan) const
{
  aIsZeroRowSpan = false;
  int32_t rowSpan = aCellFrameToAdd->GetRowSpan();
  if (0 == rowSpan) {
    // Use a min value of 2 for a zero rowspan to make computations easier
    // elsewhere. Zero rowspans are only content dependent!
    rowSpan = std::max(2, mContentRowCount - aRowIndex);
    aIsZeroRowSpan = true;
  }
  return rowSpan;
}

bool nsCellMap::HasMoreThanOneCell(int32_t aRowIndex) const
{
  const CellDataArray& row = mRows.SafeElementAt(aRowIndex, *sEmptyRow);
  uint32_t maxColIndex = row.Length();
  uint32_t count = 0;
  uint32_t colIndex;
  for (colIndex = 0; colIndex < maxColIndex; colIndex++) {
    CellData* cellData = row[colIndex];
    if (cellData && (cellData->GetCellFrame() || cellData->IsRowSpan()))
      count++;
    if (count > 1)
      return true;
  }
  return false;
}

int32_t
nsCellMap::GetNumCellsOriginatingInRow(int32_t aRowIndex) const
{
  const CellDataArray& row = mRows.SafeElementAt(aRowIndex, *sEmptyRow);
  uint32_t count = 0;
  uint32_t maxColIndex = row.Length();
  uint32_t colIndex;
  for (colIndex = 0; colIndex < maxColIndex; colIndex++) {
    CellData* cellData = row[colIndex];
    if (cellData && cellData->IsOrig())
      count++;
  }
  return count;
}

int32_t nsCellMap::GetRowSpan(int32_t  aRowIndex,
                              int32_t  aColIndex,
                              bool     aGetEffective) const
{
  int32_t rowSpan = 1;
  int32_t rowCount = (aGetEffective) ? mContentRowCount : mRows.Length();
  int32_t rowX;
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
                                  int32_t           aRowIndex,
                                  int32_t           aColIndex,
                                  int32_t           aRgFirstRowIndex,
                                  nsIntRect&        aDamageArea)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  uint32_t colX, rowX;

  // get the rowspan and colspan from the cell map since the content may have changed
  bool zeroColSpan;
  uint32_t numCols = aMap.GetColCount();
  int32_t rowSpan = GetRowSpan(aRowIndex, aColIndex, true);
  uint32_t colSpan = GetEffectiveColSpan(aMap, aRowIndex, aColIndex, zeroColSpan);
  uint32_t endRowIndex = aRowIndex + rowSpan - 1;
  uint32_t endColIndex = aColIndex + colSpan - 1;

  if (aMap.mTableFrame.HasZeroColSpans()) {
    aMap.mTableFrame.SetNeedColSpanExpansion(true);
  }

  // adjust the col counts due to the deleted cell before removing it
  for (colX = aColIndex; colX <= endColIndex; colX++) {
    nsColInfo* colInfo = aMap.GetColInfoAt(colX);
    if (colX == uint32_t(aColIndex)) {
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
    uint32_t endIndexForRow = std::min(endColIndex + 1, row.Length());

    // Since endIndexForRow <= row.Length(), enough to compare aColIndex to it.
    if (uint32_t(aColIndex) < endIndexForRow) {
      for (colX = endIndexForRow; colX > uint32_t(aColIndex); colX--) {
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
  SetDamageArea(aColIndex, aRgFirstRowIndex + aRowIndex,
                std::max(0, aMap.GetColCount() - aColIndex - 1),
                1 + endRowIndex - aRowIndex, aDamageArea);
}

void
nsCellMap::RebuildConsideringRows(nsTableCellMap&             aMap,
                                  int32_t                     aStartRowIndex,
                                  nsTArray<nsTableRowFrame*>* aRowsToInsert,
                                  int32_t                     aNumRowsToRemove)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  // copy the old cell map into a new array
  uint32_t numOrigRows = mRows.Length();
  nsTArray<CellDataArray> origRows;
  mRows.SwapElements(origRows);

  int32_t rowNumberChange;
  if (aRowsToInsert) {
    rowNumberChange = aRowsToInsert->Length();
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
  uint32_t copyEndRowIndex = std::min(numOrigRows, uint32_t(aStartRowIndex));

  // rowX keeps track of where we are in mRows while setting up the
  // new cellmap.
  uint32_t rowX = 0;
  nsIntRect damageArea;
  // put back the rows before the affected ones just as before.  Note that we
  // can't just copy the old rows in bit-for-bit, because they might be
  // spanning out into the rows we're adding/removing.
  for ( ; rowX < copyEndRowIndex; rowX++) {
    const CellDataArray& row = origRows[rowX];
    uint32_t numCols = row.Length();
    for (uint32_t colX = 0; colX < numCols; colX++) {
      // put in the original cell from the cell map
      const CellData* data = row.ElementAt(colX);
      if (data && data->IsOrig()) {
        AppendCell(aMap, data->GetCellFrame(), rowX, false, 0, damageArea);
      }
    }
  }

  // Now handle the new rows being inserted, if any.
  uint32_t copyStartRowIndex;
  rowX = aStartRowIndex;
  if (aRowsToInsert) {
    // add in the new cells and create rows if necessary
    int32_t numNewRows = aRowsToInsert->Length();
    for (int32_t newRowX = 0; newRowX < numNewRows; newRowX++) {
      nsTableRowFrame* rFrame = aRowsToInsert->ElementAt(newRowX);
      nsIFrame* cFrame = rFrame->GetFirstPrincipalChild();
      while (cFrame) {
        nsTableCellFrame *cellFrame = do_QueryFrame(cFrame);
        if (cellFrame) {
          AppendCell(aMap, cellFrame, rowX, false, 0, damageArea);
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
  for (uint32_t copyRowX = copyStartRowIndex; copyRowX < numOrigRows;
       copyRowX++) {
    const CellDataArray& row = origRows[copyRowX];
    uint32_t numCols = row.Length();
    for (uint32_t colX = 0; colX < numCols; colX++) {
      // put in the original cell from the cell map
      CellData* data = row.ElementAt(colX);
      if (data && data->IsOrig()) {
        AppendCell(aMap, data->GetCellFrame(), rowX, false, 0, damageArea);
      }
    }
    rowX++;
  }

  // delete the old cell map.  Now rowX no longer has anything to do with mRows
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    CellDataArray& row = origRows[rowX];
    uint32_t len = row.Length();
    for (uint32_t colX = 0; colX < len; colX++) {
      DestroyCellData(row[colX]);
    }
  }
}

void
nsCellMap::RebuildConsideringCells(nsTableCellMap&              aMap,
                                   int32_t                      aNumOrigCols,
                                   nsTArray<nsTableCellFrame*>* aCellFrames,
                                   int32_t                      aRowIndex,
                                   int32_t                      aColIndex,
                                   bool                         aInsert)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  // copy the old cell map into a new array
  int32_t numOrigRows  = mRows.Length();
  nsTArray<CellDataArray> origRows;
  mRows.SwapElements(origRows);

  int32_t numNewCells = (aCellFrames) ? aCellFrames->Length() : 0;

  // the new cells might extend the previous column number
  NS_ASSERTION(aNumOrigCols >= aColIndex, "Appending cells far beyond cellmap data?!");
  int32_t numCols = aInsert ? std::max(aNumOrigCols, aColIndex + 1) : aNumOrigCols;

  // build the new cell map.  Hard to say what, if anything, we can preallocate
  // here...  Should come back to that sometime, perhaps.
  int32_t rowX;
  nsIntRect damageArea;
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    const CellDataArray& row = origRows[rowX];
    for (int32_t colX = 0; colX < numCols; colX++) {
      if ((rowX == aRowIndex) && (colX == aColIndex)) {
        if (aInsert) { // put in the new cells
          for (int32_t cellX = 0; cellX < numNewCells; cellX++) {
            nsTableCellFrame* cell = aCellFrames->ElementAt(cellX);
            if (cell) {
              AppendCell(aMap, cell, rowX, false, 0, damageArea);
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
        AppendCell(aMap, data->GetCellFrame(), rowX, false, 0, damageArea);
      }
    }
  }
  if (aInsert && numOrigRows <= aRowIndex) { // append the new cells below the last original row
    NS_ASSERTION (numOrigRows == aRowIndex, "Appending cells far beyond the last row");
    for (int32_t cellX = 0; cellX < numNewCells; cellX++) {
      nsTableCellFrame* cell = aCellFrames->ElementAt(cellX);
      if (cell) {
        AppendCell(aMap, cell, aRowIndex, false, 0, damageArea);
      }
    }
  }

  // delete the old cell map
  for (rowX = 0; rowX < numOrigRows; rowX++) {
    CellDataArray& row = origRows[rowX];
    uint32_t len = row.Length();
    for (uint32_t colX = 0; colX < len; colX++) {
      DestroyCellData(row.SafeElementAt(colX));
    }
  }
  // expand the cellmap to cover empty content rows
  if (mRows.Length() < uint32_t(mContentRowCount)) {
    Grow(aMap, mContentRowCount - mRows.Length());
  }

}

void nsCellMap::RemoveCell(nsTableCellMap&   aMap,
                           nsTableCellFrame* aCellFrame,
                           int32_t           aRowIndex,
                           int32_t           aRgFirstRowIndex,
                           nsIntRect&        aDamageArea)
{
  uint32_t numRows = mRows.Length();
  if (uint32_t(aRowIndex) >= numRows) {
    NS_ERROR("bad arg in nsCellMap::RemoveCell");
    return;
  }
  int32_t numCols = aMap.GetColCount();

  // Now aRowIndex is guaranteed OK.

  // get the starting col index of the cell to remove
  int32_t startColIndex;
  for (startColIndex = 0; startColIndex < numCols; startColIndex++) {
    CellData* data = mRows[aRowIndex].SafeElementAt(startColIndex);
    if (data && (data->IsOrig()) && (aCellFrame == data->GetCellFrame())) {
      break; // we found the col index
    }
  }

  int32_t rowSpan = GetRowSpan(aRowIndex, startColIndex, false);
  // record whether removing the cells is going to cause complications due
  // to existing row spans, col spans or table sizing.
  bool spansCauseRebuild = CellsSpanInOrOut(aRowIndex,
                                              aRowIndex + rowSpan - 1,
                                              startColIndex, numCols - 1);
  // XXX if the cell has a col span to the end of the map, and the end has no originating
  // cells, we need to assume that this the only such cell, and rebuild so that there are
  // no extraneous cols at the end. The same is true for removing rows.
  if (!aCellFrame->GetRowSpan() || !aCellFrame->GetColSpan())
    spansCauseRebuild = true;

  if (spansCauseRebuild) {
    aMap.RebuildConsideringCells(this, nullptr, aRowIndex, startColIndex, false,
                                 aDamageArea);
  }
  else {
    ShrinkWithoutCell(aMap, *aCellFrame, aRowIndex, startColIndex,
                      aRgFirstRowIndex, aDamageArea);
  }
}

void nsCellMap::ExpandZeroColSpans(nsTableCellMap& aMap)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  uint32_t numRows = mRows.Length();
  uint32_t numCols = aMap.GetColCount();
  uint32_t rowIndex, colIndex;

  for (rowIndex = 0; rowIndex < numRows; rowIndex++) {
    for (colIndex = 0; colIndex < numCols; colIndex++) {
      CellData* data = mRows[rowIndex].SafeElementAt(colIndex);
      if (!data || !data->IsOrig())
        continue;
      nsTableCellFrame* cell = data->GetCellFrame();
      NS_ASSERTION(cell, "There has to be a cell");
      int32_t cellRowSpan = cell->GetRowSpan();
      int32_t cellColSpan = cell->GetColSpan();
      bool rowZeroSpan = (0 == cell->GetRowSpan());
      bool colZeroSpan = (0 == cell->GetColSpan());
      if (colZeroSpan) {
        aMap.mTableFrame.SetHasZeroColSpans(true);
        // do the expansion
        NS_ASSERTION(numRows > 0, "Bogus numRows");
        NS_ASSERTION(numCols > 0, "Bogus numCols");
        uint32_t endRowIndex =  rowZeroSpan ? numRows - 1 :
                                              rowIndex + cellRowSpan - 1;
        uint32_t endColIndex =  colZeroSpan ? numCols - 1 :
                                              colIndex + cellColSpan - 1;
        uint32_t colX, rowX;
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
            CellData* newData = AllocCellData(nullptr);
            if (!newData) return;

            newData->SetColSpanOffset(colX - colIndex);
            newData->SetZeroColSpan(true);

            if (rowX > rowIndex) {
              newData->SetRowSpanOffset(rowX - rowIndex);
              if (rowZeroSpan)
                newData->SetZeroRowSpan(true);
            }
            SetDataAt(aMap, *newData, rowX, colX);
          }
          colX++;
        }  // while (colX <= endColIndex)
      } // if zerocolspan
    }
  }
}
#ifdef DEBUG
void nsCellMap::Dump(bool aIsBorderCollapse) const
{
  printf("\n  ***** START GROUP CELL MAP DUMP ***** %p\n", (void*)this);
  nsTableRowGroupFrame* rg = GetRowGroup();
  const nsStyleDisplay* display = rg->StyleDisplay();
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
  uint32_t mapRowCount = mRows.Length();
  printf("mapRowCount=%u tableRowCount=%d\n", mapRowCount, mContentRowCount);


  uint32_t rowIndex, colIndex;
  for (rowIndex = 0; rowIndex < mapRowCount; rowIndex++) {
    const CellDataArray& row = mRows[rowIndex];
    printf("  row %d : ", rowIndex);
    uint32_t colCount = row.Length();
    for (colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = row[colIndex];
      if (cd) {
        if (cd->IsOrig()) {
          printf("C%d,%d  ", rowIndex, colIndex);
        } else {
          if (cd->IsRowSpan()) {
            printf("R ");
          }
          if (cd->IsColSpan()) {
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
      mozilla::css::Side side;
      bool          segStart;
      bool          bevel;
      for (int32_t i = 0; i <= 2; i++) {
        printf("\n          ");
        for (colIndex = 0; colIndex < colCount; colIndex++) {
          BCCellData* cd = (BCCellData *)row[colIndex];
          if (cd) {
            if (0 == i) {
              size = cd->mData.GetTopEdge(owner, segStart);
              printf("t=%d%d%d ", int32_t(size), owner, segStart);
            }
            else if (1 == i) {
              size = cd->mData.GetLeftEdge(owner, segStart);
              printf("l=%d%d%d ", int32_t(size), owner, segStart);
            }
            else {
              size = cd->mData.GetCorner(side, bevel);
              printf("c=%d%d%d ", int32_t(size), side, bevel);
            }
          }
        }
      }
    }
    printf("\n");
  }

  // output info mapping Ci,j to cell address
  uint32_t cellCount = 0;
  for (uint32_t rIndex = 0; rIndex < mapRowCount; rIndex++) {
    const CellDataArray& row = mRows[rIndex];
    uint32_t colCount = row.Length();
    printf("  ");
    for (colIndex = 0; colIndex < colCount; colIndex++) {
      CellData* cd = row[colIndex];
      if (cd) {
        if (cd->IsOrig()) {
          nsTableCellFrame* cellFrame = cd->GetCellFrame();
          int32_t cellFrameColIndex;
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

CellData*
nsCellMap::GetDataAt(int32_t         aMapRowIndex,
                     int32_t         aColIndex) const
{
  return
    mRows.SafeElementAt(aMapRowIndex, *sEmptyRow).SafeElementAt(aColIndex);
}

// only called if the cell at aMapRowIndex, aColIndex is null or dead
// (the latter from ExpandZeroColSpans).
void nsCellMap::SetDataAt(nsTableCellMap& aMap,
                          CellData&       aNewCell,
                          int32_t         aMapRowIndex,
                          int32_t         aColIndex)
{
  NS_ASSERTION(!!aMap.mBCInfo == mIsBC, "BC state mismatch");
  if (uint32_t(aMapRowIndex) >= mRows.Length()) {
    NS_ERROR("SetDataAt called with row index > num rows");
    return;
  }

  CellDataArray& row = mRows[aMapRowIndex];

  // the table map may need cols added
  int32_t numColsToAdd = aColIndex + 1 - aMap.GetColCount();
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
                         int32_t               aRowX,
                         int32_t               aColX,
                         bool*               aOriginates,
                         int32_t*              aColSpan) const
{
  if (aOriginates) {
    *aOriginates = false;
  }
  CellData* data = GetDataAt(aRowX, aColX);
  nsTableCellFrame* cellFrame = nullptr;
  if (data) {
    if (data->IsOrig()) {
      cellFrame = data->GetCellFrame();
      if (aOriginates)
        *aOriginates = true;
    }
    else {
      cellFrame = GetCellFrame(aRowX, aColX, *data, true);
    }
    if (cellFrame && aColSpan) {
      int32_t initialColIndex;
      cellFrame->GetColIndex(initialColIndex);
      bool zeroSpan;
      *aColSpan = GetEffectiveColSpan(aMap, aRowX, initialColIndex, zeroSpan);
    }
  }
  return cellFrame;
}


bool nsCellMap::RowIsSpannedInto(int32_t         aRowIndex,
                                   int32_t         aNumEffCols) const
{
  if ((0 > aRowIndex) || (aRowIndex >= mContentRowCount)) {
    return false;
  }
  for (int32_t colIndex = 0; colIndex < aNumEffCols; colIndex++) {
    CellData* cd = GetDataAt(aRowIndex, colIndex);
    if (cd) { // there's really a cell at (aRowIndex, colIndex)
      if (cd->IsSpan()) { // the cell at (aRowIndex, colIndex) is the result of a span
        if (cd->IsRowSpan() && GetCellFrame(aRowIndex, colIndex, *cd, true)) { // XXX why the last check
          return true;
        }
      }
    }
  }
  return false;
}

bool nsCellMap::RowHasSpanningCells(int32_t aRowIndex,
                                      int32_t aNumEffCols) const
{
  if ((0 > aRowIndex) || (aRowIndex >= mContentRowCount)) {
    return false;
  }
  if (aRowIndex != mContentRowCount - 1) {
    // aRowIndex is not the last row, so we check the next row after aRowIndex for spanners
    for (int32_t colIndex = 0; colIndex < aNumEffCols; colIndex++) {
      CellData* cd = GetDataAt(aRowIndex, colIndex);
      if (cd && (cd->IsOrig())) { // cell originates
        CellData* cd2 = GetDataAt(aRowIndex + 1, colIndex);
        if (cd2 && cd2->IsRowSpan()) { // cd2 is spanned by a row
          if (cd->GetCellFrame() == GetCellFrame(aRowIndex + 1, colIndex, *cd2, true)) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void nsCellMap::DestroyCellData(CellData* aData)
{
  if (!aData) {
    return;
  }

  if (mIsBC) {
    BCCellData* bcData = static_cast<BCCellData*>(aData);
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
    uint32_t rowArrayLength = mCurMap->mRows.Length();
    mCurMapRelevantRowCount = std::min(mCurMapContentRowCount, rowArrayLength);
  } while (0 == mCurMapRelevantRowCount);

  NS_ASSERTION(mCurMapRelevantRowCount != 0 || !mCurMap,
               "How did that happen?");

  // Set mCurMapRow to 0, since cells can't span across table row groups.
  mCurMapRow = 0;
}

void
nsCellMapColumnIterator::IncrementRow(int32_t aIncrement)
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
nsCellMapColumnIterator::GetNextFrame(int32_t* aRow, int32_t* aColSpan)
{
  // Fast-path for the case when we don't have anything left in the column and
  // we know it.
  if (mFoundCells == mOrigCells) {
    *aRow = 0;
    *aColSpan = 1;
    return nullptr;
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
      // Look up the originating data for this cell, advance by its relative rowspan.
      int32_t rowspanOffset = cellData->GetRowSpanOffset();
      nsTableCellFrame* cellFrame = mCurMap->GetCellFrame(mCurMapRow, mCol, *cellData, false);
      NS_ASSERTION(cellFrame,"Must have usable originating data here");
      int32_t rowSpan = cellFrame->GetRowSpan();
      if (rowSpan == 0) {
        AdvanceRowGroup();
      }
      else {
        IncrementRow(rowSpan - rowspanOffset);
      }
      continue;
    }

    NS_ASSERTION(cellData->IsOrig(),
                 "Must have originating cellData by this point.  "
                 "See comment on mCurMapRow in header.");

    nsTableCellFrame* cellFrame = cellData->GetCellFrame();
    NS_ASSERTION(cellFrame, "Orig data without cellframe?");

    *aRow = mCurMapStart + mCurMapRow;
    bool ignoredZeroSpan;
    *aColSpan = mCurMap->GetEffectiveColSpan(*mMap, mCurMapRow, mCol,
                                             ignoredZeroSpan);

    IncrementRow(cellFrame->GetRowSpan());

    ++mFoundCells;

    NS_ABORT_IF_FALSE(cellData == mMap->GetDataAt(*aRow, mCol),
                      "Giving caller bogus row?");

    return cellFrame;
  }

  NS_NOTREACHED("Can't get here");
  return nullptr;
}
