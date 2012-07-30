/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsCellMap_h__
#define nsCellMap_h__

#include "nscore.h"
#include "celldata.h"
#include "nsTArray.h"
#include "nsTArray.h"
#include "nsRect.h"
#include "nsCOMPtr.h"
#include "nsAlgorithm.h"
#include "nsAutoPtr.h"

#undef DEBUG_TABLE_CELLMAP

class nsTableColFrame;
class nsTableCellFrame;
class nsTableRowFrame;
class nsTableRowGroupFrame;
class nsTableFrame;
class nsCellMap;
class nsPresContext;
class nsCellMapColumnIterator;

struct nsColInfo
{
  PRInt32 mNumCellsOrig; // number of cells originating in the col
  PRInt32 mNumCellsSpan; // number of cells spanning into the col via colspans (not rowspans)

  nsColInfo();
  nsColInfo(PRInt32 aNumCellsOrig,
            PRInt32 aNumCellsSpan);
};

enum Corner
{
  eTopLeft     = 0,
  eTopRight    = 1,
  eBottomRight = 2,
  eBottomLeft  = 3
};

struct BCInfo
{
  nsTArray<BCData> mRightBorders;
  nsTArray<BCData> mBottomBorders;
  BCData           mLowerRightCorner;
};

class nsTableCellMap
{
public:
  nsTableCellMap(nsTableFrame&   aTableFrame,
                 bool            aBorderCollapse);

  /** destructor
    * NOT VIRTUAL BECAUSE THIS CLASS SHOULD **NEVER** BE SUBCLASSED
    */
  ~nsTableCellMap();

  void RemoveGroupCellMap(nsTableRowGroupFrame* aRowGroup);

  void InsertGroupCellMap(nsTableRowGroupFrame*  aNewRowGroup,
                          nsTableRowGroupFrame*& aPrevRowGroup);

  /**
   * Get the nsCellMap for the given row group.  If aStartHint is non-null,
   * will start looking with that cellmap and only fall back to starting at the
   * beginning of the list if that doesn't find us the right nsCellMap.
   * Otherwise, just start at the beginning.
   *
   * aRowGroup must not be null.
   */
  nsCellMap* GetMapFor(const nsTableRowGroupFrame* aRowGroup,
                       nsCellMap* aStartHint) const;

  /** synchronize the cellmaps with the rowgroups again **/
  void Synchronize(nsTableFrame* aTableFrame);

  nsTableCellFrame* GetCellFrame(PRInt32   aRowIndex,
                                 PRInt32   aColIndex,
                                 CellData& aData,
                                 bool      aUseRowIfOverlap) const;

  /** return the CellData for the cell at (aRowIndex, aColIndex) */
  CellData* GetDataAt(PRInt32 aRowIndex,
                      PRInt32 aColIndex) const;

  // this function creates a col if needed
  nsColInfo* GetColInfoAt(PRInt32 aColIndex);

  /** append the cellFrame at the end of the row at aRowIndex and return the col index
    */
  CellData* AppendCell(nsTableCellFrame&     aCellFrame,
                       PRInt32               aRowIndex,
                       bool                  aRebuildIfNecessary,
                       nsIntRect&            aDamageArea);

  void InsertCells(nsTArray<nsTableCellFrame*>& aCellFrames,
                   PRInt32                      aRowIndex,
                   PRInt32                      aColIndexBefore,
                   nsIntRect&                   aDamageArea);

  void RemoveCell(nsTableCellFrame* aCellFrame,
                  PRInt32           aRowIndex,
                  nsIntRect&        aDamageArea);
  /** Remove the previously gathered column information */
  void ClearCols();
  void InsertRows(nsTableRowGroupFrame*       aRowGroup,
                  nsTArray<nsTableRowFrame*>& aRows,
                  PRInt32                     aFirstRowIndex,
                  bool                        aConsiderSpans,
                  nsIntRect&                  aDamageArea);

  void RemoveRows(PRInt32         aFirstRowIndex,
                  PRInt32         aNumRowsToRemove,
                  bool            aConsiderSpans,
                  nsIntRect&      aDamageArea);

  PRInt32 GetNumCellsOriginatingInRow(PRInt32 aRowIndex) const;
  PRInt32 GetNumCellsOriginatingInCol(PRInt32 aColIndex) const;

  /** indicate whether the row has more than one cell that either originates
    * or is spanned from the rows above
    */
  bool HasMoreThanOneCell(PRInt32 aRowIndex) const;

  PRInt32 GetEffectiveRowSpan(PRInt32 aRowIndex,
                              PRInt32 aColIndex) const;
  PRInt32 GetEffectiveColSpan(PRInt32 aRowIndex,
                              PRInt32 aColIndex) const;

  /** return the total number of columns in the table represented by this CellMap */
  PRInt32 GetColCount() const;

  /** return the actual number of rows in the table represented by this CellMap */
  PRInt32 GetRowCount() const;

  nsTableCellFrame* GetCellInfoAt(PRInt32  aRowX,
                                  PRInt32  aColX,
                                  bool*  aOriginates = nullptr,
                                  PRInt32* aColSpan = nullptr) const;

  /**
   * Returns the index at the given row and column coordinates.
   *
   * @see  nsITableLayout::GetIndexByRowAndColumn()
   *
   * @param aRow     [in] the row coordinate
   * @param aColumn  [in] the column coordinate
   * @returns             the index for the cell
   */
  PRInt32 GetIndexByRowAndColumn(PRInt32 aRow, PRInt32 aColumn) const;

  /**
   * Retrieves the row and column coordinates for the given index.
   *
   * @see  nsITableLayout::GetRowAndColumnByIndex()
   *
   * @param aIndex  [in] the index for which coordinates are to be retrieved
   * @param aRow    [out] the row coordinate to be returned
   * @param aColumn [out] the column coordinate to be returned
   */
  void GetRowAndColumnByIndex(PRInt32 aIndex,
                              PRInt32 *aRow, PRInt32 *aColumn) const;

  void AddColsAtEnd(PRUint32 aNumCols);
  void RemoveColsAtEnd();

  bool RowIsSpannedInto(PRInt32 aRowIndex, PRInt32 aNumEffCols) const;
  bool RowHasSpanningCells(PRInt32 aRowIndex, PRInt32 aNumEffCols) const;
  void RebuildConsideringCells(nsCellMap*                   aCellMap,
                               nsTArray<nsTableCellFrame*>* aCellFrames,
                               PRInt32                      aRowIndex,
                               PRInt32                      aColIndex,
                               bool                         aInsert,
                               nsIntRect&                   aDamageArea);

protected:
  /**
   * Rebuild due to rows being inserted or deleted with cells spanning
   * into or out of the rows.  This function can only handle insertion
   * or deletion but NOT both.  So either aRowsToInsert must be null
   * or aNumRowsToRemove must be 0.
   *
   * // XXXbz are both allowed to happen?  That'd be a no-op...
   */
  void RebuildConsideringRows(nsCellMap*                  aCellMap,
                              PRInt32                     aStartRowIndex,
                              nsTArray<nsTableRowFrame*>* aRowsToInsert,
                              PRInt32                     aNumRowsToRemove,
                              nsIntRect&                  aDamageArea);

public:
  void ExpandZeroColSpans();

  void ResetTopStart(PRUint8    aSide,
                     nsCellMap& aCellMap,
                     PRUint32   aYPos,
                     PRUint32   aXPos,
                     bool       aIsLowerRight = false);

  void SetBCBorderEdge(mozilla::css::Side aEdge,
                       nsCellMap&    aCellMap,
                       PRUint32      aCellMapStart,
                       PRUint32      aYPos,
                       PRUint32      aXPos,
                       PRUint32      aLength,
                       BCBorderOwner aOwner,
                       nscoord       aSize,
                       bool          aChanged);

  void SetBCBorderCorner(Corner      aCorner,
                         nsCellMap&  aCellMap,
                         PRUint32    aCellMapStart,
                         PRUint32    aYPos,
                         PRUint32    aXPos,
                         mozilla::css::Side aOwner,
                         nscoord     aSubSize,
                         bool        aBevel,
                         bool        aIsBottomRight = false);

  /** dump a representation of the cell map to stdout for debugging */
#ifdef DEBUG
  void Dump(char* aString = nullptr) const;
#endif

protected:
  BCData* GetRightMostBorder(PRInt32 aRowIndex);
  BCData* GetBottomMostBorder(PRInt32 aColIndex);

  friend class nsCellMap;
  friend class BCMapCellIterator;
  friend class BCPaintBorderIterator;
  friend class nsCellMapColumnIterator;

/** Insert a row group cellmap after aPrevMap, if aPrefMap is null insert it
  * at the beginning, the ordering of the cellmap corresponds to the ordering of
  * rowgroups once OrderRowGroups has been called
  */
  void InsertGroupCellMap(nsCellMap* aPrevMap,
                          nsCellMap& aNewMap);
  void DeleteRightBottomBorders();

  nsTableFrame&               mTableFrame;
  nsAutoTArray<nsColInfo, 8>  mCols;
  nsCellMap*                  mFirstMap;
  // border collapsing info
  BCInfo*                     mBCInfo;
};

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
  * mRows is an array of rows.  Each row is an array of cells.  a cell
  * can be null.
  */
class nsCellMap
{
public:
  /** constructor
    * @param aRowGroupFrame the row group frame this is a cellmap for
    * @param aIsBC whether the table is doing border-collapse
    */
  nsCellMap(nsTableRowGroupFrame* aRowGroupFrame, bool aIsBC);

  /** destructor
    * NOT VIRTUAL BECAUSE THIS CLASS SHOULD **NEVER** BE SUBCLASSED
    */
  ~nsCellMap();

  static void Init();
  static void Shutdown();

  nsCellMap* GetNextSibling() const;
  void SetNextSibling(nsCellMap* aSibling);

  nsTableRowGroupFrame* GetRowGroup() const;

  nsTableCellFrame* GetCellFrame(PRInt32   aRowIndex,
                                 PRInt32   aColIndex,
                                 CellData& aData,
                                 bool      aUseRowSpanIfOverlap) const;

  /**
   * Returns highest cell index within the cell map.
   *
   * @param  aColCount  [in] the number of columns in the table
   */
  PRInt32 GetHighestIndex(PRInt32 aColCount);

  /**
   * Returns the index of the given row and column coordinates.
   *
   * @see  nsITableLayout::GetIndexByRowAndColumn()
   *
   * @param aColCount    [in] the number of columns in the table
   * @param aRow         [in] the row coordinate
   * @param aColumn      [in] the column coordinate
   */
  PRInt32 GetIndexByRowAndColumn(PRInt32 aColCount,
                                 PRInt32 aRow, PRInt32 aColumn) const;

  /**
   * Get the row and column coordinates at the given index.
   *
   * @see  nsITableLayout::GetRowAndColumnByIndex()
   *
   * @param aColCount  [in] the number of columns in the table
   * @param aIndex     [in] the index for which coordinates are to be retrieved
   * @param aRow       [out] the row coordinate to be returned
   * @param aColumn    [out] the column coordinate to be returned
   */
  void GetRowAndColumnByIndex(PRInt32 aColCount, PRInt32 aIndex,
                              PRInt32 *aRow, PRInt32 *aColumn) const;

  /** append the cellFrame at an empty or dead cell or finally at the end of
    * the row at aRowIndex and return a pointer to the celldata entry in the
    * cellmap
    *
    * @param aMap               - reference to the table cell map
    * @param aCellFrame         - a pointer to the cellframe which will be appended
    *                             to the row
    * @param aRowIndex          - to this row the celldata entry will be added
    * @param aRebuildIfNecessay - if a cell spans into a row below it might be
    *                             necesserary to rebuild the cellmap as this rowspan
    *                             might overlap another cell.
    * @param aDamageArea        - area in cellmap coordinates which have been updated.
    * @param aColToBeginSearch  - if not null contains the column number where
    *                             the search for a empty or dead cell in the
    *                             row should start
    * @return                   - a pointer to the celldata entry inserted into
    *                             the cellmap
    */
  CellData* AppendCell(nsTableCellMap&   aMap,
                       nsTableCellFrame* aCellFrame,
                       PRInt32           aRowIndex,
                       bool              aRebuildIfNecessary,
                       PRInt32           aRgFirstRowIndex,
                       nsIntRect&        aDamageArea,
                       PRInt32*          aBeginSearchAtCol = nullptr);

  /** Function to be called when a cell is added at a location which is spanned
    * to by a zero colspan.  We handle this situation by collapsing the zero
    * colspan, since there is really no good way to deal with it (trying to
    * increase the number of columns to hold the new cell would just mean the
    * zero colspan needs to expand).

    * @param aMap      - reference to the table cell map
    * @param aOrigData - zero colspanned cell that will be collapsed
    * @param aRowIndex - row where the first collision appears
    * @param aColIndex - column where the first collision appears
    **/
  void CollapseZeroColSpan(nsTableCellMap& aMap,
                           CellData*       aOrigData,
                           PRInt32         aRowIndex,
                           PRInt32         aColIndex);

  void InsertCells(nsTableCellMap&              aMap,
                   nsTArray<nsTableCellFrame*>& aCellFrames,
                   PRInt32                      aRowIndex,
                   PRInt32                      aColIndexBefore,
                   PRInt32                      aRgFirstRowIndex,
                   nsIntRect&                   aDamageArea);

  void RemoveCell(nsTableCellMap&   aMap,
                  nsTableCellFrame* aCellFrame,
                  PRInt32           aRowIndex,
                  PRInt32           aRgFirstRowIndex,
                  nsIntRect&        aDamageArea);

  void InsertRows(nsTableCellMap&             aMap,
                  nsTArray<nsTableRowFrame*>& aRows,
                  PRInt32                     aFirstRowIndex,
                  bool                        aConsiderSpans,
                  PRInt32                     aRgFirstRowIndex,
                  nsIntRect&                  aDamageArea);

  void RemoveRows(nsTableCellMap& aMap,
                  PRInt32         aFirstRowIndex,
                  PRInt32         aNumRowsToRemove,
                  bool            aConsiderSpans,
                  PRInt32         aRgFirstRowIndex,
                  nsIntRect&      aDamageArea);

  PRInt32 GetNumCellsOriginatingInRow(PRInt32 aRowIndex) const;
  PRInt32 GetNumCellsOriginatingInCol(PRInt32 aColIndex) const;

  /** return the number of rows in the table represented by this CellMap */
  PRInt32 GetRowCount(bool aConsiderDeadRowSpanRows = false) const;

  nsTableCellFrame* GetCellInfoAt(const nsTableCellMap& aMap,
                                  PRInt32          aRowX,
                                  PRInt32          aColX,
                                  bool*          aOriginates = nullptr,
                                  PRInt32*         aColSpan = nullptr) const;

  bool RowIsSpannedInto(PRInt32 aRowIndex,
                          PRInt32 aNumEffCols) const;

  bool RowHasSpanningCells(PRInt32 aRowIndex,
                             PRInt32 aNumEffCols) const;

  void ExpandZeroColSpans(nsTableCellMap& aMap);

  /** indicate whether the row has more than one cell that either originates
   * or is spanned from the rows above
   */
  bool HasMoreThanOneCell(PRInt32 aRowIndex) const;

  /* Get the rowspan for a cell starting at aRowIndex and aColIndex.
   * If aGetEffective is true the size will not exceed the last content based
   * row. Cells can have a specified rowspan that extends below the last
   * content based row. This is legitimate considering incr. reflow where the
   * content rows will arive later.
   */
  PRInt32 GetRowSpan(PRInt32 aRowIndex,
                     PRInt32 aColIndex,
                     bool    aGetEffective) const;

  PRInt32 GetEffectiveColSpan(const nsTableCellMap& aMap,
                              PRInt32     aRowIndex,
                              PRInt32     aColIndex,
                              bool&     aIsZeroColSpan) const;

  typedef nsTArray<CellData*> CellDataArray;

  /** dump a representation of the cell map to stdout for debugging */
#ifdef DEBUG
  void Dump(bool aIsBorderCollapse) const;
#endif

protected:
  friend class nsTableCellMap;
  friend class BCMapCellIterator;
  friend class BCPaintBorderIterator;
  friend class nsTableFrame;
  friend class nsCellMapColumnIterator;

  /**
   * Increase the number of rows in this cellmap by aNumRows.  Put the
   * new rows at aRowIndex.  If aRowIndex is -1, put them at the end.
   */
  bool Grow(nsTableCellMap& aMap,
              PRInt32         aNumRows,
              PRInt32         aRowIndex = -1);

  void GrowRow(CellDataArray& aRow,
               PRInt32        aNumCols);

  /** assign aCellData to the cell at (aRow,aColumn) */
  void SetDataAt(nsTableCellMap& aMap,
                 CellData&       aCellData,
                 PRInt32         aMapRowIndex,
                 PRInt32         aColIndex);

  CellData* GetDataAt(PRInt32         aMapRowIndex,
                      PRInt32         aColIndex) const;

  PRInt32 GetNumCellsIn(PRInt32 aColIndex) const;

  void ExpandWithRows(nsTableCellMap&             aMap,
                      nsTArray<nsTableRowFrame*>& aRowFrames,
                      PRInt32                     aStartRowIndex,
                      PRInt32                     aRgFirstRowIndex,
                      nsIntRect&                  aDamageArea);

  void ExpandWithCells(nsTableCellMap&              aMap,
                       nsTArray<nsTableCellFrame*>& aCellFrames,
                       PRInt32                      aRowIndex,
                       PRInt32                      aColIndex,
                       PRInt32                      aRowSpan,
                       bool                         aRowSpanIsZero,
                       PRInt32                      aRgFirstRowIndex,
                       nsIntRect&                   aDamageArea);

  void ShrinkWithoutRows(nsTableCellMap& aMap,
                         PRInt32         aFirstRowIndex,
                         PRInt32         aNumRowsToRemove,
                         PRInt32         aRgFirstRowIndex,
                         nsIntRect&      aDamageArea);

  void ShrinkWithoutCell(nsTableCellMap&   aMap,
                         nsTableCellFrame& aCellFrame,
                         PRInt32           aRowIndex,
                         PRInt32           aColIndex,
                         PRInt32           aRgFirstRowIndex,
                         nsIntRect&        aDamageArea);

  /**
   * Rebuild due to rows being inserted or deleted with cells spanning
   * into or out of the rows.  This function can only handle insertion
   * or deletion but NOT both.  So either aRowsToInsert must be null
   * or aNumRowsToRemove must be 0.
   *
   * // XXXbz are both allowed to happen?  That'd be a no-op...
   */
  void RebuildConsideringRows(nsTableCellMap&             aMap,
                              PRInt32                     aStartRowIndex,
                              nsTArray<nsTableRowFrame*>* aRowsToInsert,
                              PRInt32                     aNumRowsToRemove);

  void RebuildConsideringCells(nsTableCellMap&              aMap,
                               PRInt32                      aNumOrigCols,
                               nsTArray<nsTableCellFrame*>* aCellFrames,
                               PRInt32                      aRowIndex,
                               PRInt32                      aColIndex,
                               bool                         aInsert);

  bool CellsSpanOut(nsTArray<nsTableRowFrame*>& aNewRows) const;

  /** If a cell spans out of the area defined by aStartRowIndex, aEndRowIndex
    * and aStartColIndex, aEndColIndex the cellmap changes are more severe so
    * the corresponding routines needs to be called. This is also necessary if
    * cells outside spans into this region.
    * @aStartRowIndex       - y start index
    * @aEndRowIndex         - y end index
    * @param aStartColIndex - x start index
    * @param aEndColIndex   - x end index
    * @return               - true if a cell span crosses the border of the
                              region
    */
  bool CellsSpanInOrOut(PRInt32 aStartRowIndex,
                          PRInt32 aEndRowIndex,
                          PRInt32 aStartColIndex,
                          PRInt32 aEndColIndex) const;

  void ExpandForZeroSpan(nsTableCellFrame* aCellFrame,
                         PRInt32           aNumColsInTable);

  bool CreateEmptyRow(PRInt32 aRowIndex,
                        PRInt32 aNumCols);

  PRInt32 GetRowSpanForNewCell(nsTableCellFrame* aCellFrameToAdd,
                               PRInt32           aRowIndex,
                               bool&           aIsZeroRowSpan) const;

  PRInt32 GetColSpanForNewCell(nsTableCellFrame& aCellFrameToAdd,
                               bool&           aIsZeroColSpan) const;

  // Destroy a CellData struct.  This will handle the case of aData
  // actually being a BCCellData properly.
  void DestroyCellData(CellData* aData);
  // Allocate a CellData struct.  This will handle needing to create a
  // BCCellData properly.
  // @param aOrigCell the originating cell to pass to the celldata constructor
  CellData* AllocCellData(nsTableCellFrame* aOrigCell);

  /** an array containing, for each row, the CellDatas for the cells
    * in that row.  It can be larger than mContentRowCount due to row spans
    * extending beyond the table */
  // XXXbz once we have auto TArrays, we should probably use them here.
  nsTArray<CellDataArray> mRows;

  /** the number of rows in the table (content) which is not indentical to the
    * number of rows in the cell map due to row spans extending beyond the end
    * of thetable (dead rows) or empty tr tags
    */
  PRInt32 mContentRowCount;

  // the row group that corresponds to this map
  nsTableRowGroupFrame* mRowGroupFrame;

  // the next row group cell map
  nsCellMap* mNextSibling;

  // Whether this is a BC cellmap or not
  bool mIsBC;

  // Prescontext to deallocate and allocate celldata
  nsRefPtr<nsPresContext> mPresContext;
};

/**
 * A class for iterating the cells in a given column.  Must be given a
 * non-null nsTableCellMap and a column number valid for that cellmap.
 */
class nsCellMapColumnIterator
{
public:
  nsCellMapColumnIterator(const nsTableCellMap* aMap, PRInt32 aCol) :
    mMap(aMap), mCurMap(aMap->mFirstMap), mCurMapStart(0),
    mCurMapRow(0), mCol(aCol), mFoundCells(0)
  {
    NS_PRECONDITION(aMap, "Must have map");
    NS_PRECONDITION(mCol < aMap->GetColCount(), "Invalid column");
    mOrigCells = aMap->GetNumCellsOriginatingInCol(mCol);
    if (mCurMap) {
      mCurMapContentRowCount = mCurMap->GetRowCount();
      PRUint32 rowArrayLength = mCurMap->mRows.Length();
      mCurMapRelevantRowCount = NS_MIN(mCurMapContentRowCount, rowArrayLength);
      if (mCurMapRelevantRowCount == 0 && mOrigCells > 0) {
        // This row group is useless; advance!
        AdvanceRowGroup();
      }
    }
#ifdef DEBUG
    else {
      NS_ASSERTION(mOrigCells == 0, "Why no rowgroups?");
    }
#endif
  }

  nsTableCellFrame* GetNextFrame(PRInt32* aRow, PRInt32* aColSpan);

private:
  void AdvanceRowGroup();

  // Advance the row; aIncrement is considered to be a cell's rowspan,
  // so if 0 is passed in we'll advance to the next rowgroup.
  void IncrementRow(PRInt32 aIncrement);

  const nsTableCellMap* mMap;
  const nsCellMap* mCurMap;

  // mCurMapStart is the row in the entire nsTableCellMap where
  // mCurMap starts.  This is used to compute row indices to pass to
  // nsTableCellMap::GetDataAt, so must be a _content_ row index.
  PRUint32 mCurMapStart;

  // In steady-state mCurMapRow is the row in our current nsCellMap
  // that we'll use the next time GetNextFrame() is called.  Due to
  // the way we skip over rowspans, the entry in mCurMapRow and mCol
  // is either null, dead, originating, or a colspan.  In particular,
  // it cannot be a rowspan or overlap entry.
  PRUint32 mCurMapRow;
  const PRInt32 mCol;
  PRUint32 mOrigCells;
  PRUint32 mFoundCells;

  // The number of content rows in mCurMap.  This may be bigger than the number
  // of "relevant" rows, or it might be smaller.
  PRUint32 mCurMapContentRowCount;

  // The number of "relevant" rows in mCurMap.  That is, the number of rows
  // which might have an originating cell in them.  Once mCurMapRow reaches
  // mCurMapRelevantRowCount, we should move to the next map.
  PRUint32 mCurMapRelevantRowCount;
};


/* ----- inline methods ----- */
inline PRInt32 nsTableCellMap::GetColCount() const
{
  return mCols.Length();
}

inline nsCellMap* nsCellMap::GetNextSibling() const
{
  return mNextSibling;
}

inline void nsCellMap::SetNextSibling(nsCellMap* aSibling)
{
  mNextSibling = aSibling;
}

inline nsTableRowGroupFrame* nsCellMap::GetRowGroup() const
{
  return mRowGroupFrame;
}

inline PRInt32 nsCellMap::GetRowCount(bool aConsiderDeadRowSpanRows) const
{
  PRInt32 rowCount = (aConsiderDeadRowSpanRows) ? mRows.Length() : mContentRowCount;
  return rowCount;
}

// nsColInfo

inline nsColInfo::nsColInfo()
 :mNumCellsOrig(0), mNumCellsSpan(0)
{}

inline nsColInfo::nsColInfo(PRInt32 aNumCellsOrig,
                            PRInt32 aNumCellsSpan)
 :mNumCellsOrig(aNumCellsOrig), mNumCellsSpan(aNumCellsSpan)
{}


#endif
