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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Eric D Vaughan (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

  nsGridRow* GetColumnAt(PRInt32 aIndex, PRBool aIsHorizontal = PR_TRUE);
  nsGridRow* GetRowAt(PRInt32 aIndex, PRBool aIsHorizontal = PR_TRUE);
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

  nsSize GetPrefRowSize(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, PRBool aIsHorizontal = PR_TRUE);
  nsSize GetMinRowSize(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, PRBool aIsHorizontal = PR_TRUE);
  nsSize GetMaxRowSize(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, PRBool aIsHorizontal = PR_TRUE);
  nscoord GetRowFlex(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, PRBool aIsHorizontal = PR_TRUE);

  nscoord GetPrefRowHeight(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, PRBool aIsHorizontal = PR_TRUE);
  nscoord GetMinRowHeight(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, PRBool aIsHorizontal = PR_TRUE);
  nscoord GetMaxRowHeight(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, PRBool aIsHorizontal = PR_TRUE);
  void GetRowOffsets(nsBoxLayoutState& aState, PRInt32 aIndex, nscoord& aTop, nscoord& aBottom, PRBool aIsHorizontal = PR_TRUE);

  void RowAddedOrRemoved(nsBoxLayoutState& aBoxLayoutState, PRInt32 aIndex, PRBool aIsHorizontal = PR_TRUE);
  void CellAddedOrRemoved(nsBoxLayoutState& aBoxLayoutState, PRInt32 aIndex, PRBool aIsHorizontal = PR_TRUE);
  void DirtyRows(nsIBox* aRowBox, nsBoxLayoutState& aState);
#ifdef DEBUG_grid
  void PrintCellMap();
#endif
  PRInt32 GetExtraColumnCount(PRBool aIsHorizontal = PR_TRUE);
  PRInt32 GetExtraRowCount(PRBool aIsHorizontal = PR_TRUE);

// accessors
  void SetBox(nsIBox* aBox) { mBox = aBox; }
  nsIBox* GetBox() { return mBox; }
  nsIBox* GetRowsBox() { return mRowsBox; }
  nsIBox* GetColumnsBox() { return mColumnsBox; }
  nsGridRow* GetColumns();
  nsGridRow* GetRows();
  PRInt32 GetRowCount(PRInt32 aIsHorizontal = PR_TRUE);
  PRInt32 GetColumnCount(PRInt32 aIsHorizontal = PR_TRUE);

  static nsIBox* GetScrolledBox(nsIBox* aChild);
  static nsIBox* GetScrollBox(nsIBox* aChild);
  void GetFirstAndLastRow(nsBoxLayoutState& aState, 
                          PRInt32& aFirstIndex, 
                          PRInt32& aLastIndex, 
                          nsGridRow*& aFirstRow,
                          nsGridRow*& aLastRow,
                          PRBool aIsHorizontal);

private:
  void GetPartFromBox(nsIBox* aBox, nsIGridPart** aPart);
  nsMargin GetBoxTotalMargin(nsIBox* aBox, PRBool aIsHorizontal = PR_TRUE);

  void FreeMap();
  void FindRowsAndColumns(nsIBox** aRows, nsIBox** aColumns);
  void BuildRows(nsIBox* aBox, PRInt32 aSize, nsGridRow** aColumnsRows, PRBool aIsHorizontal = PR_TRUE);
  nsGridCell* BuildCellMap(PRInt32 aRows, PRInt32 aColumns);
  void PopulateCellMap(nsGridRow* aRows, nsGridRow* aColumns, PRInt32 aRowCount, PRInt32 aColumnCount, PRBool aIsHorizontal = PR_TRUE);
  void CountRowsColumns(nsIBox* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount);
  void SetLargestSize(nsSize& aSize, nscoord aHeight, PRBool aIsHorizontal = PR_TRUE);
  void SetSmallestSize(nsSize& aSize, nscoord aHeight, PRBool aIsHorizontal = PR_TRUE);
  PRBool IsGrid(nsIBox* aBox);

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
  PRBool mNeedsRebuild;

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
  PRBool mMarkingDirty;
};

#endif

