/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGrid_h___
#define nsGrid_h___

#include "nsStackLayout.h"
#include "nsIGridPart.h"
#include "nsCOMPtr.h"
#include "mozilla/UniquePtr.h"

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

  nsGridRow* GetColumnAt(int32_t aIndex, bool aIsHorizontal = true);
  nsGridRow* GetRowAt(int32_t aIndex, bool aIsHorizontal = true);
  nsGridCell* GetCellAt(int32_t aX, int32_t aY);

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

  nsSize GetPrefRowSize(nsBoxLayoutState& aBoxLayoutState, int32_t aRowIndex, bool aIsHorizontal = true);
  nsSize GetMinRowSize(nsBoxLayoutState& aBoxLayoutState, int32_t aRowIndex, bool aIsHorizontal = true);
  nsSize GetMaxRowSize(nsBoxLayoutState& aBoxLayoutState, int32_t aRowIndex, bool aIsHorizontal = true);
  nscoord GetRowFlex(int32_t aRowIndex, bool aIsHorizontal = true);

  nscoord GetPrefRowHeight(nsBoxLayoutState& aBoxLayoutState, int32_t aRowIndex, bool aIsHorizontal = true);
  nscoord GetMinRowHeight(nsBoxLayoutState& aBoxLayoutState, int32_t aRowIndex, bool aIsHorizontal = true);
  nscoord GetMaxRowHeight(nsBoxLayoutState& aBoxLayoutState, int32_t aRowIndex, bool aIsHorizontal = true);
  void GetRowOffsets(int32_t aIndex, nscoord& aTop, nscoord& aBottom, bool aIsHorizontal = true);

  void RowAddedOrRemoved(nsBoxLayoutState& aBoxLayoutState, int32_t aIndex, bool aIsHorizontal = true);
  void CellAddedOrRemoved(nsBoxLayoutState& aBoxLayoutState, int32_t aIndex, bool aIsHorizontal = true);
  void DirtyRows(nsIFrame* aRowBox, nsBoxLayoutState& aState);
#ifdef DEBUG_grid
  void PrintCellMap();
#endif
  int32_t GetExtraColumnCount(bool aIsHorizontal = true);
  int32_t GetExtraRowCount(bool aIsHorizontal = true);

// accessors
  void SetBox(nsIFrame* aBox) { mBox = aBox; }
  nsIFrame* GetBox() { return mBox; }
  nsIFrame* GetRowsBox() { return mRowsBox; }
  nsIFrame* GetColumnsBox() { return mColumnsBox; }
  int32_t GetRowCount(int32_t aIsHorizontal = true);
  int32_t GetColumnCount(int32_t aIsHorizontal = true);

  static nsIFrame* GetScrolledBox(nsIFrame* aChild);
  static nsIFrame* GetScrollBox(nsIFrame* aChild);
  static nsIGridPart* GetPartFromBox(nsIFrame* aBox);
  void GetFirstAndLastRow(int32_t& aFirstIndex,
                          int32_t& aLastIndex,
                          nsGridRow*& aFirstRow,
                          nsGridRow*& aLastRow,
                          bool aIsHorizontal);

private:

  nsMargin GetBoxTotalMargin(nsIFrame* aBox, bool aIsHorizontal = true);

  void FreeMap();
  void FindRowsAndColumns(nsIFrame** aRows, nsIFrame** aColumns);
  mozilla::UniquePtr<nsGridRow[]> BuildRows(nsIFrame* aBox, int32_t aSize,
                                            bool aIsHorizontal = true);
  mozilla::UniquePtr<nsGridCell[]> BuildCellMap(int32_t aRows, int32_t aColumns);
  void PopulateCellMap(nsGridRow* aRows, nsGridRow* aColumns, int32_t aRowCount, int32_t aColumnCount, bool aIsHorizontal = true);
  void CountRowsColumns(nsIFrame* aBox, int32_t& aRowCount, int32_t& aComputedColumnCount);
  void SetLargestSize(nsSize& aSize, nscoord aHeight, bool aIsHorizontal = true);
  void SetSmallestSize(nsSize& aSize, nscoord aHeight, bool aIsHorizontal = true);
  bool IsGrid(nsIFrame* aBox);

  // the box that implement the <grid> tag
  nsIFrame* mBox;

  // an array of row object
  mozilla::UniquePtr<nsGridRow[]> mRows;

  // an array of columns objects.
  mozilla::UniquePtr<nsGridRow[]> mColumns;

  // the first in the <grid> that implements the <rows> tag.
  nsIFrame* mRowsBox;

  // the first in the <grid> that implements the <columns> tag.
  nsIFrame* mColumnsBox;

  // a flag that is false tells us to rebuild the who grid
  bool mNeedsRebuild;

  // number of rows and columns as defined by the XUL
  int32_t mRowCount;
  int32_t mColumnCount;

  // number of rows and columns that are implied but not
  // explicitly defined int he XUL
  int32_t mExtraRowCount;
  int32_t mExtraColumnCount;

  // x,y array of cells in the rows and columns
  mozilla::UniquePtr<nsGridCell[]> mCellMap;

  // a flag that when true suppresses all other MarkDirties. This
  // prevents lots of extra work being done.
  bool mMarkingDirty;
};

#endif

