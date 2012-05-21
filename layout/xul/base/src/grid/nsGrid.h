/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGrid_h___
#define nsGrid_h___

#include "nsStackLayout.h"
#include "nsIGridPart.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"

class nsGridRowGroupLayout;
class nsGridRowLayout;
class nsBoxLayoutState;
class nsGridCell;

//#define DEBUG_grid 1

/**
 * The grid data structure, i.e., the grid cellmap.
 */
class nsGrid
{
public:
  nsGrid();
  ~nsGrid();

  nsGridRow* GetColumnAt(PRInt32 aIndex, bool aIsHorizontal = true);
  nsGridRow* GetRowAt(PRInt32 aIndex, bool aIsHorizontal = true);
  nsGridCell* GetCellAt(PRInt32 aX, PRInt32 aY);

  void NeedsRebuild(nsBoxLayoutState& aBoxLayoutState);
  void RebuildIfNeeded();

  // For all the methods taking an aIsHorizontal parameter:
  // * When aIsHorizontal is true, the words "rows" and (for
  //   GetColumnCount) "columns" refer to their normal meanings.
  // * When aIsHorizontal is false, the meanings are flipped.
  // FIXME:  Maybe eliminate GetColumnCount and change aIsHorizontal to
  // aIsRows?  (Calling it horizontal doesn't really make sense because
  // row groups and columns have vertical orientation, whereas column
  // groups and rows are horizontal.)

  nsSize GetPrefRowSize(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, bool aIsHorizontal = true);
  nsSize GetMinRowSize(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, bool aIsHorizontal = true);
  nsSize GetMaxRowSize(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, bool aIsHorizontal = true);
  nscoord GetRowFlex(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, bool aIsHorizontal = true);

  nscoord GetPrefRowHeight(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, bool aIsHorizontal = true);
  nscoord GetMinRowHeight(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, bool aIsHorizontal = true);
  nscoord GetMaxRowHeight(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, bool aIsHorizontal = true);
  void GetRowOffsets(nsBoxLayoutState& aState, PRInt32 aIndex, nscoord& aTop, nscoord& aBottom, bool aIsHorizontal = true);

  void RowAddedOrRemoved(nsBoxLayoutState& aBoxLayoutState, PRInt32 aIndex, bool aIsHorizontal = true);
  void CellAddedOrRemoved(nsBoxLayoutState& aBoxLayoutState, PRInt32 aIndex, bool aIsHorizontal = true);
  void DirtyRows(nsIBox* aRowBox, nsBoxLayoutState& aState);
#ifdef DEBUG_grid
  void PrintCellMap();
#endif
  PRInt32 GetExtraColumnCount(bool aIsHorizontal = true);
  PRInt32 GetExtraRowCount(bool aIsHorizontal = true);

// accessors
  void SetBox(nsIBox* aBox) { mBox = aBox; }
  nsIBox* GetBox() { return mBox; }
  nsIBox* GetRowsBox() { return mRowsBox; }
  nsIBox* GetColumnsBox() { return mColumnsBox; }
  PRInt32 GetRowCount(PRInt32 aIsHorizontal = true);
  PRInt32 GetColumnCount(PRInt32 aIsHorizontal = true);

  static nsIBox* GetScrolledBox(nsIBox* aChild);
  static nsIBox* GetScrollBox(nsIBox* aChild);
  static nsIGridPart* GetPartFromBox(nsIBox* aBox);
  void GetFirstAndLastRow(nsBoxLayoutState& aState, 
                          PRInt32& aFirstIndex, 
                          PRInt32& aLastIndex, 
                          nsGridRow*& aFirstRow,
                          nsGridRow*& aLastRow,
                          bool aIsHorizontal);

private:

  nsMargin GetBoxTotalMargin(nsIBox* aBox, bool aIsHorizontal = true);

  void FreeMap();
  void FindRowsAndColumns(nsIBox** aRows, nsIBox** aColumns);
  void BuildRows(nsIBox* aBox, PRInt32 aSize, nsGridRow** aColumnsRows, bool aIsHorizontal = true);
  nsGridCell* BuildCellMap(PRInt32 aRows, PRInt32 aColumns);
  void PopulateCellMap(nsGridRow* aRows, nsGridRow* aColumns, PRInt32 aRowCount, PRInt32 aColumnCount, bool aIsHorizontal = true);
  void CountRowsColumns(nsIBox* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount);
  void SetLargestSize(nsSize& aSize, nscoord aHeight, bool aIsHorizontal = true);
  void SetSmallestSize(nsSize& aSize, nscoord aHeight, bool aIsHorizontal = true);
  bool IsGrid(nsIBox* aBox);

  // the box that implement the <grid> tag
  nsIBox* mBox;

  // an array of row object
  nsGridRow* mRows;

  // an array of columns objects.
  nsGridRow* mColumns;

  // the first in the <grid> that implements the <rows> tag.
  nsIBox* mRowsBox;

  // the first in the <grid> that implements the <columns> tag.
  nsIBox* mColumnsBox;

  // a flag that is false tells us to rebuild the who grid
  bool mNeedsRebuild;

  // number of rows and columns as defined by the XUL
  PRInt32 mRowCount;
  PRInt32 mColumnCount;

  // number of rows and columns that are implied but not 
  // explicitly defined int he XUL
  PRInt32 mExtraRowCount;
  PRInt32 mExtraColumnCount;

  // x,y array of cells in the rows and columns
  nsGridCell* mCellMap;

  // a flag that when true suppresses all other MarkDirties. This
  // prevents lots of extra work being done.
  bool mMarkingDirty;
};

#endif

