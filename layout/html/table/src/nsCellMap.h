/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsCellMap_h__
#define nsCellMap_h__

#include "nscore.h"
#include "celldata.h"
#include "nsVoidArray.h"
class nsTableColFrame;
class nsTableCellFrame;
class nsIPresContext;
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

class nsTableCellMap
{
public:
  nsTableCellMap(nsIPresContext* aPresContext, nsTableFrame& aTableFrame);

  /** destructor
    * NOT VIRTUAL BECAUSE THIS CLASS SHOULD **NEVER** BE SUBCLASSED  
	  */
  ~nsTableCellMap();
  
  void RemoveGroupCellMap(nsTableRowGroupFrame* aRowGroup);
  void InsertGroupCellMap(nsTableRowGroupFrame&  aNewRowGroup,
                          nsTableRowGroupFrame*& aPrevRowGroup);

  nsTableCellFrame* GetCellFrame(PRInt32   aRowIndex,
                                 PRInt32   aColIndex,
                                 CellData& aData,
                                 PRBool    aUseRowIfOverlap) const;

  /** return the CellData for the cell at (aTableRowIndex, aTableColIndex) */
  CellData* GetCellAt(PRInt32 aRowIndex, 
                      PRInt32 aColIndex);

  nsColInfo* GetColInfoAt(PRInt32 aColIndex);

  /** append the cellFrame at the end of the row at aRowIndex and return the col index
    */
  PRInt32 AppendCell(nsTableCellFrame&     aCellFrame,
                     PRInt32               aRowIndex,
                     PRBool                aRebuildIfNecessary);

  void InsertCells(nsVoidArray&          aCellFrames,
                   PRInt32               aRowIndex,
                   PRInt32               aColIndexBefore);

  void RemoveCell(nsTableCellFrame* aCellFrame,
                  PRInt32           aRowIndex);

  void InsertRows(nsIPresContext*       aPresContext,
                  nsTableRowGroupFrame& aRowGroup,
                  nsVoidArray&          aRows,
                  PRInt32               aFirstRowIndex,
                  PRBool                aConsiderSpans);

  void RemoveRows(nsIPresContext* aPresContext,
                  PRInt32         aFirstRowIndex,
                  PRInt32         aNumRowsToRemove,
                  PRBool          aConsiderSpans);

  PRInt32 GetNumCellsOriginatingInRow(PRInt32 aRowIndex);
  PRInt32 GetNumCellsOriginatingInCol(PRInt32 aColIndex) const;

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

  PRBool RowIsSpannedInto(PRInt32 aRowIndex);
  PRBool RowHasSpanningCells(PRInt32 aRowIndex);
  PRBool ColIsSpannedInto(PRInt32 aColIndex);
  PRBool ColHasSpanningCells(PRInt32 aColIndex);

  /** dump a representation of the cell map to stdout for debugging */
#ifdef NS_DEBUG
  void Dump() const;
#endif

#ifdef DEBUG
  void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
  friend class nsCellMap;
  void InsertGroupCellMap(nsCellMap* aPrevMap,
                          nsCellMap& aNewMap);

  nsTableFrame& mTableFrame;
  nsAutoVoidArray mCols;
  nsCellMap*    mFirstMap;
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
    * @param aNumRows - initial number of rows
	  * @param aNumColumns - initial number of columns
	  */
  nsCellMap(nsTableRowGroupFrame& aRowGroupFrame);

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

  /** return the CellData for the cell at (aTableRowIndex, aTableColIndex) */
  CellData* GetCellAt(nsTableCellMap& aMap,
                      PRInt32         aRowIndex, 
                      PRInt32         aColIndex);

  /** append the cellFrame at the end of the row at aRowIndex and return the col index
    */
  PRInt32 AppendCell(nsTableCellMap&   aMap,
                     nsTableCellFrame& aCellFrame, 
                     PRInt32           aRowIndex,
                     PRBool            aRebuildIfNecessary);

  void InsertCells(nsTableCellMap& aMap,
                   nsVoidArray&    aCellFrames,
                   PRInt32         aRowIndex,
                   PRInt32         aColIndexBefore);

  void RemoveCell(nsTableCellMap&   aMap,
                  nsTableCellFrame* aCellFrame,
                  PRInt32           aRowIndex);

  void RemoveCol(PRInt32 aColIndex);

  void InsertRows(nsIPresContext* aPresContext,
                  nsTableCellMap& aMap,
                  nsVoidArray&    aRows,
                  PRInt32         aFirstRowIndex,
                  PRBool          aConsiderSpans);

  void RemoveRows(nsIPresContext* aPresContext,
                  nsTableCellMap& aMap,
                  PRInt32         aFirstRowIndex,
                  PRInt32         aNumRowsToRemove,
                  PRBool          aConsiderSpans);

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
                          PRInt32 aRowIndex);

  PRBool RowHasSpanningCells(nsTableCellMap& aMap,
                             PRInt32         aRowIndex);

  PRBool ColHasSpanningCells(nsTableCellMap& aMap,
                             PRInt32         aColIndex);

  /** dump a representation of the cell map to stdout for debugging */
#ifdef NS_DEBUG
  void Dump() const;
#endif

#ifdef DEBUG
  void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
  friend class nsTableCellMap;

  PRBool Grow(nsTableCellMap& aMap,
              PRInt32         aNumRows,
              PRInt32         aRowIndex = -1); 

  void GrowRow(nsVoidArray& aRow,
               PRInt32      aNumCols);

  /** assign aCellData to the cell at (aRow,aColumn) */
  void SetMapCellAt(nsTableCellMap& aMap,
                    CellData&       aCellData, 
                    PRInt32         aMapRowIndex, 
                    PRInt32         aColIndex,
                    PRBool          aCountZeroSpanAsSpan);

  CellData* GetMapCellAt(nsTableCellMap& aMap,
                         PRInt32         aMapRowIndex, 
                         PRInt32         aColIndex,
                         PRBool          aUpdateZeroSpan);

  PRInt32 GetNumCellsIn(PRInt32 aColIndex) const;

  void ExpandWithRows(nsIPresContext* aPresContext,
                      nsTableCellMap& aMap,
                      nsVoidArray&    aRowFrames,
                      PRInt32         aStartRowIndex);

  void ExpandWithCells(nsTableCellMap& aMap,
                       nsVoidArray&    aCellFrames,
                       PRInt32         aRowIndex,
                       PRInt32         aColIndex,
                       PRInt32         aRowSpan,
                       PRBool          aRowSpanIsZero);

  void ShrinkWithoutRows(nsTableCellMap& aMap,
                         PRInt32         aFirstRowIndex,
                         PRInt32         aNumRowsToRemove);

  void ShrinkWithoutCell(nsTableCellMap&   aMap,
                         nsTableCellFrame& aCellFrame,
                         PRInt32           aRowIndex,
                         PRInt32           aColIndex);

  void RebuildConsideringRows(nsIPresContext* aPresContext,
                              nsTableCellMap& aMap,
                              PRInt32         aStartRowIndex,
                              nsVoidArray*    aRowsToInsert,
                              PRInt32         aNumRowsToRemove = 0);

  void RebuildConsideringCells(nsTableCellMap& aMap,
                               nsVoidArray*    aCellFrames,
                               PRInt32         aRowIndex,
                               PRInt32         aColIndex,
                               PRBool          aInsert);

  PRBool CellsSpanOut(nsIPresContext* aPresContext, 
                      nsVoidArray&    aNewRows);

  PRBool CellsSpanInOrOut(nsTableCellMap& aMap,
                          PRInt32         aStartRowIndex, 
                          PRInt32         aEndRowIndex,
                          PRInt32         aStartColIndex, 
                          PRInt32         aEndColIndex);

  void ExpandForZeroSpan(nsTableCellFrame* aCellFrame,
                         PRInt32           aNumColsInTable);

  PRBool CreateEmptyRow(PRInt32 aRowIndex,
                        PRInt32 aNumCols);

  PRInt32 GetRowSpan(nsTableCellMap& aMap,
                     PRInt32         aRowIndex,
                     PRInt32         aColIndex,
                     PRBool          aGetEffective,
                     PRBool&         aIsZeroRowSpan);

  PRInt32 GetRowSpanForNewCell(nsTableCellFrame& aCellFrameToAdd, 
                               PRInt32           aRowIndex,
                               PRBool&           aIsZeroRowSpan);

  PRInt32 GetEffectiveColSpan(nsTableCellMap& aMap,
                              PRInt32         aRowIndex,
                              PRInt32         aColIndex,
                              PRBool&         aIsZeroColSpan);

  PRInt32 GetColSpanForNewCell(nsTableCellFrame& aCellFrameToAdd, 
                               PRInt32           aColIndex,
                               PRInt32           aNumColsInTable,
                               PRBool&           aIsZeroColSpan);
    
  void AdjustForZeroSpan(nsTableCellMap& aMap,
                         PRInt32         aRowIndex,
                         PRInt32         aColIndex);

  PRBool IsZeroColSpan(PRInt32 aRowIndex,
                       PRInt32 aColIndex) const;

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
