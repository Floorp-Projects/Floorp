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
#ifndef nsCellMap_h__
#define nsCellMap_h__

#include "nscore.h"
#include "celldata.h"
#include "nsVoidArray.h"
#include "nsRect.h"

#undef DEBUG_TABLE_CELLMAP

class nsTableColFrame;
class nsTableCellFrame;
class nsTableRowGroupFrame;
class nsTableFrame;
class nsCellMap;

struct nsColInfo
{
  PRInt32 mNumCellsOrig; // number of cells originating in the col
  PRInt32 mNumCellsSpan; // number of cells spanning into the col via colspans (not rowspans)
                         // for simplicity, a colspan=0 cell is only counted as spanning the
                         // 1st col to the right of where it orginates
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
  nsVoidArray mRightBorders;
  nsVoidArray mBottomBorders;
  BCData      mLowerRightCorner;
};

class nsTableCellMap
{
public:
  nsTableCellMap(nsTableFrame&   aTableFrame,
                 PRBool          aBorderCollapse);

  /** destructor
    * NOT VIRTUAL BECAUSE THIS CLASS SHOULD **NEVER** BE SUBCLASSED  
    */
  ~nsTableCellMap();
  
  void RemoveGroupCellMap(nsTableRowGroupFrame* aRowGroup);

  void InsertGroupCellMap(nsTableRowGroupFrame&  aNewRowGroup,
                          nsTableRowGroupFrame*& aPrevRowGroup);

  nsCellMap* GetMapFor(nsTableRowGroupFrame& aRowGroup);

  /** synchronize the cellmaps with the rowgroups again **/
  void Synchronize(nsTableFrame* aTableFrame);

  nsTableCellFrame* GetCellFrame(PRInt32   aRowIndex,
                                 PRInt32   aColIndex,
                                 CellData& aData,
                                 PRBool    aUseRowIfOverlap) const;

  /** return the CellData for the cell at (aTableRowIndex, aTableColIndex) */
  CellData* GetDataAt(PRInt32 aRowIndex, 
                      PRInt32 aColIndex,
                      PRBool  aUpdateZeroSpan = PR_TRUE);

  nsColInfo* GetColInfoAt(PRInt32 aColIndex);

  /** append the cellFrame at the end of the row at aRowIndex and return the col index
    */
  CellData* AppendCell(nsTableCellFrame&     aCellFrame,
                       PRInt32               aRowIndex,
                       PRBool                aRebuildIfNecessary,
                       nsRect&               aDamageArea);

  void InsertCells(nsVoidArray&          aCellFrames,
                   PRInt32               aRowIndex,
                   PRInt32               aColIndexBefore,
                   nsRect&               aDamageArea);

  void RemoveCell(nsTableCellFrame* aCellFrame,
                  PRInt32           aRowIndex,
                  nsRect&           aDamageArea);
  /** Remove the previously gathered column information */
  void ClearCols();
  void InsertRows(nsTableRowGroupFrame& aRowGroup,
                  nsVoidArray&          aRows,
                  PRInt32               aFirstRowIndex,
                  PRBool                aConsiderSpans,
                  nsRect&               aDamageArea);

  void RemoveRows(PRInt32         aFirstRowIndex,
                  PRInt32         aNumRowsToRemove,
                  PRBool          aConsiderSpans,
                  nsRect&               aDamageArea);

  PRInt32 GetNumCellsOriginatingInRow(PRInt32 aRowIndex);
  PRInt32 GetNumCellsOriginatingInCol(PRInt32 aColIndex) const;

  /** indicate whether the row has more than one cell that either originates
    * or is spanned from the rows above
    */
  PRBool HasMoreThanOneCell(PRInt32 aRowIndex);

  PRInt32 GetEffectiveRowSpan(PRInt32 aRowIndex,
                              PRInt32 aColIndex);
  PRInt32 GetEffectiveColSpan(PRInt32 aRowIndex,
                              PRInt32 aColIndex);

  /** return the total number of columns in the table represented by this CellMap */
  PRInt32 GetColCount() const;

  /** return the actual number of rows in the table represented by this CellMap */
  PRInt32 GetRowCount() const;

  nsTableCellFrame* GetCellInfoAt(PRInt32  aRowX, 
                                  PRInt32  aColX, 
                                  PRBool*  aOriginates = nsnull, 
                                  PRInt32* aColSpan = nsnull);

  void AddColsAtEnd(PRUint32 aNumCols);
  void RemoveColsAtEnd();

  PRBool RowIsSpannedInto(PRInt32 aRowIndex, PRInt32 aNumEffCols);
  PRBool RowHasSpanningCells(PRInt32 aRowIndex, PRInt32 aNumEffCols);
  void RebuildConsideringCells(nsCellMap*      aCellMap,
                               nsVoidArray*    aCellFrames,
                               PRInt32         aRowIndex,
                               PRInt32         aColIndex,
                               PRBool          aInsert,
                               nsRect&         aDamageArea);
  void RebuildConsideringRows(nsCellMap*      aCellMap,
                              PRInt32         aStartRowIndex,
                              nsVoidArray*    aRowsToInsert,
                              PRBool          aNumRowsToRemove,
                              nsRect&         aDamageArea);

  PRBool ColIsSpannedInto(PRInt32 aColIndex);
  PRBool ColHasSpanningCells(PRInt32 aColIndex);

  BCData* GetBCData(PRUint8     aSide, 
                    nsCellMap&  aCellMap,
                    PRUint32    aYPos, 
                    PRUint32    aXPos,
                    PRBool      aIsLowerRight = PR_FALSE);

  void SetBCBorderEdge(PRUint8       aEdge, 
                       nsCellMap&    aCellMap,
                       PRUint32      aCellMapStart,
                       PRUint32      aYPos, 
                       PRUint32      aXPos,
                       PRUint32      aLength,
                       BCBorderOwner aOwner,
                       nscoord       aSize,
                       PRBool        aChanged);

  void SetBCBorderCorner(Corner      aCorner,
                         nsCellMap&  aCellMap,
                         PRUint32    aCellMapStart,
                         PRUint32    aYPos, 
                         PRUint32    aXPos,
                         PRUint8     aOwner,
                         nscoord     aSubSize,
                         PRBool      aBevel,
                         PRBool      aIsBottomRight = PR_FALSE);

  /** dump a representation of the cell map to stdout for debugging */
#ifdef NS_DEBUG
  void Dump(char* aString = nsnull) const;
#endif

protected:
  BCData* GetRightMostBorder(PRInt32 aRowIndex);
  BCData* GetBottomMostBorder(PRInt32 aColIndex);

  friend class nsCellMap;
  friend class BCMapCellIterator;
  friend class BCMapBorderIterator;
/** Insert a row group cellmap after aPrevMap, if aPrefMap is null insert it
  * at the beginning, the ordering of the cellmap corresponds to the ordering of
  * rowgroups once OrderRowGroups has been called
  */
  void InsertGroupCellMap(nsCellMap* aPrevMap,
                          nsCellMap& aNewMap);
  void DeleteRightBottomBorders();

  nsTableFrame&   mTableFrame;
  nsAutoVoidArray mCols;
  nsCellMap*      mFirstMap;
  // border collapsing info
  BCInfo*         mBCInfo;
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
  * mRows is an array of rows.  a row cannot be null.
  * each row is an array of cells.  a cell can be null.
  */
class nsCellMap
{
public:
  /** constructor 
    * @param aRowGroupFrame the row group frame this is a cellmap for
    * @param aIsBC whether the table is doing border-collapse
    */
  nsCellMap(nsTableRowGroupFrame& aRowGroupFrame, PRBool aIsBC);

  /** destructor
    * NOT VIRTUAL BECAUSE THIS CLASS SHOULD **NEVER** BE SUBCLASSED  
    */
  ~nsCellMap();

  nsCellMap* GetNextSibling() const;
  void SetNextSibling(nsCellMap* aSibling);

  nsTableRowGroupFrame* GetRowGroup() const;

  nsTableCellFrame* GetCellFrame(PRInt32   aRowIndex,
                                 PRInt32   aColIndex,
                                 CellData& aData,
                                 PRBool    aUseRowSpanIfOverlap) const;

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
                       PRBool            aRebuildIfNecessary,
                       nsRect&           aDamageArea,
                       PRInt32*          aBeginSearchAtCol = nsnull);

  void InsertCells(nsTableCellMap& aMap,
                   nsVoidArray&    aCellFrames,
                   PRInt32         aRowIndex,
                   PRInt32         aColIndexBefore,
                   nsRect&         aDamageArea);

  void RemoveCell(nsTableCellMap&   aMap,
                  nsTableCellFrame* aCellFrame,
                  PRInt32           aRowIndex,
                  nsRect&           aDamageArea);

  void InsertRows(nsTableCellMap& aMap,
                  nsVoidArray&    aRows,
                  PRInt32         aFirstRowIndex,
                  PRBool          aConsiderSpans,
                  nsRect&         aDamageArea);

  void RemoveRows(nsTableCellMap& aMap,
                  PRInt32         aFirstRowIndex,
                  PRInt32         aNumRowsToRemove,
                  PRBool          aConsiderSpans,
                  nsRect&         aDamageArea);

  PRInt32 GetNumCellsOriginatingInRow(PRInt32 aRowIndex) const;
  PRInt32 GetNumCellsOriginatingInCol(PRInt32 aColIndex) const;

  /** return the number of rows in the table represented by this CellMap */
  PRInt32 GetRowCount(PRBool aConsiderDeadRowSpanRows = PR_FALSE) const;

  nsTableCellFrame* GetCellInfoAt(nsTableCellMap& aMap,
                                  PRInt32         aRowX, 
                                  PRInt32         aColX,
                                  PRBool*         aOriginates = nsnull, 
                                  PRInt32*        aColSpan = nsnull);

  PRBool RowIsSpannedInto(nsTableCellMap& aMap,
                          PRInt32         aRowIndex,
                          PRInt32         aNumEffCols);

  PRBool RowHasSpanningCells(nsTableCellMap& aMap,
                             PRInt32         aRowIndex,
                             PRInt32         aNumEffCols);

  PRBool ColHasSpanningCells(nsTableCellMap& aMap,
                             PRInt32         aColIndex);

  /** indicate whether the row has more than one cell that either originates
   * or is spanned from the rows above
   */
  PRBool HasMoreThanOneCell(nsTableCellMap& aMap,
                            PRInt32         aRowIndex);

  PRInt32 GetRowSpan(nsTableCellMap& aMap,
                     PRInt32         aRowIndex,
                     PRInt32         aColIndex,
                     PRBool          aGetEffective,
                     PRBool&         aIsZeroRowSpan);

  PRInt32 GetEffectiveColSpan(nsTableCellMap& aMap,
                              PRInt32         aRowIndex,
                              PRInt32         aColIndex,
                              PRBool&         aIsZeroColSpan);

  /** dump a representation of the cell map to stdout for debugging */
#ifdef NS_DEBUG
  void Dump(PRBool aIsBorderCollapse) const;
#endif

protected:
  friend class nsTableCellMap;
  friend class BCMapCellIterator;
  friend class BCMapBorderIterator;
  friend class nsTableFrame;

  PRBool Grow(nsTableCellMap& aMap,
              PRInt32         aNumRows,
              PRInt32         aRowIndex = -1); 

  void GrowRow(nsVoidArray& aRow,
               PRInt32      aNumCols);

  /** assign aCellData to the cell at (aRow,aColumn) */
  void SetDataAt(nsTableCellMap& aMap,
                 CellData&       aCellData, 
                 PRInt32         aMapRowIndex, 
                 PRInt32         aColIndex,
                 PRBool          aCountZeroSpanAsSpan);

  CellData* GetDataAt(nsTableCellMap& aMap,
                      PRInt32         aMapRowIndex, 
                      PRInt32         aColIndex,
                      PRBool          aUpdateZeroSpan);

  PRInt32 GetNumCellsIn(PRInt32 aColIndex) const;

  void ExpandWithRows(nsTableCellMap& aMap,
                      nsVoidArray&    aRowFrames,
                      PRInt32         aStartRowIndex,
                      nsRect&         aDamageArea);

  void ExpandWithCells(nsTableCellMap& aMap,
                       nsVoidArray&    aCellFrames,
                       PRInt32         aRowIndex,
                       PRInt32         aColIndex,
                       PRInt32         aRowSpan,
                       PRBool          aRowSpanIsZero,
                       nsRect&         aDamageArea);

  void ShrinkWithoutRows(nsTableCellMap& aMap,
                         PRInt32         aFirstRowIndex,
                         PRInt32         aNumRowsToRemove,
                         nsRect&         aDamageArea);

  void ShrinkWithoutCell(nsTableCellMap&   aMap,
                         nsTableCellFrame& aCellFrame,
                         PRInt32           aRowIndex,
                         PRInt32           aColIndex,
                         nsRect&           aDamageArea);

  void RebuildConsideringRows(nsTableCellMap& aMap,
                              PRInt32         aNumOrigCols,
                              PRInt32         aStartRowIndex,
                              nsVoidArray*    aRowsToInsert,
                              PRInt32         aNumRowsToRemove,
                              nsRect&         aDamageArea);

  void RebuildConsideringCells(nsTableCellMap& aMap,
                               PRInt32         aNumOrigCols,
                               nsVoidArray*    aCellFrames,
                               PRInt32         aRowIndex,
                               PRInt32         aColIndex,
                               PRBool          aInsert,
                               nsRect&         aDamageArea);

  PRBool CellsSpanOut(nsVoidArray&    aNewRows);

  PRBool CellsSpanInOrOut(nsTableCellMap& aMap,
                          PRInt32         aStartRowIndex, 
                          PRInt32         aEndRowIndex,
                          PRInt32         aStartColIndex, 
                          PRInt32         aEndColIndex);

  void ExpandForZeroSpan(nsTableCellFrame* aCellFrame,
                         PRInt32           aNumColsInTable);

  PRBool CreateEmptyRow(PRInt32 aRowIndex,
                        PRInt32 aNumCols);

  PRInt32 GetRowSpanForNewCell(nsTableCellFrame& aCellFrameToAdd, 
                               PRInt32           aRowIndex,
                               PRBool&           aIsZeroRowSpan);

  PRInt32 GetColSpanForNewCell(nsTableCellFrame& aCellFrameToAdd, 
                               PRInt32           aColIndex,
                               PRInt32           aNumColsInTable,
                               PRBool&           aIsZeroColSpan);
    
  void AdjustForZeroSpan(nsTableCellMap& aMap,
                         PRInt32         aRowIndex,
                         PRInt32         aColIndex);

  PRBool IsZeroColSpan(PRInt32 aRowIndex,
                       PRInt32 aColIndex) const;

  // Destroy a CellData struct.  This will handle the case of aData
  // actually being a BCCellData properly.
  void DestroyCellData(CellData* aData);
  // Allocate a CellData struct.  This will handle needing to create a
  // BCCellData properly.
  // @param aOrigCell the originating cell to pass to the celldata constructor
  CellData* AllocCellData(nsTableCellFrame* aOrigCell);

  /** an array containing col array. It can be larger than mRowCount due to
    * row spans extending beyond the table */
  nsAutoVoidArray mRows; 

  /** the number of rows in the table which is <= the number of rows in the cell map
    * due to row spans extending beyond the end of the table (dead rows) */
  PRInt32 mRowCount;

  // the row group that corresponds to this map
  nsTableRowGroupFrame* mRowGroupFrame;

  // the next row group cell map
  nsCellMap* mNextSibling;

  // Whether this is a BC cellmap or not
  PRBool mIsBC;
};

/* ----- inline methods ----- */
inline PRInt32 nsTableCellMap::GetColCount() const
{
  return mCols.Count();
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

inline PRInt32 nsCellMap::GetRowCount(PRBool aConsiderDeadRowSpanRows) const
{ 
  PRInt32 rowCount = (aConsiderDeadRowSpanRows) ? mRows.Count() : mRowCount;
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
