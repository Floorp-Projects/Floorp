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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/**
 
  Author:
  Eric D Vaughan

**/

#ifndef nsGrid_h___
#define nsGrid_h___

#include "nsStackLayout.h"
#include "nsIGridPart.h"
#include "nsCOMPtr.h"
#include "nsIBox.h"

class nsGridRowGroupLayout;
class nsGridRowLayout;
class nsBoxLayoutState;
class nsGridCell;

class nsGrid
{
public:
  nsGrid();
  ~nsGrid();

  virtual void NeedsRebuild(nsBoxLayoutState& aBoxLayoutState);
  virtual nsGridRow* GetColumnAt(PRInt32 aIndex, PRBool aIsRow);
  virtual nsGridRow* GetRowAt(PRInt32 aIndex, PRBool aIsRow);
  virtual nsGridCell* GetCellAt(PRInt32 aX, PRInt32 aY);
  virtual void RebuildIfNeeded();

  virtual nsresult GetPrefRowSize(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, nsSize& aSize, PRBool aIsRow);
  virtual nsresult GetMinRowSize(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, nsSize& aSize, PRBool aIsRow);
  virtual nsresult GetMaxRowSize(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, nsSize& aSize, PRBool aIsRow);
  virtual nsresult GetRowFlex(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, nscoord& aSize, PRBool aIsRow);

  virtual nsresult GetPrefRowHeight(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, nscoord& aHeight, PRBool aIsRow);
  virtual nsresult GetMinRowHeight(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, nscoord& aHeight, PRBool aIsRow);
  virtual nsresult GetMaxRowHeight(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, nscoord& aHeight, PRBool aIsRow);

  virtual void RowChildIsDirty(nsBoxLayoutState& aBoxLayoutState, PRInt32 aRowIndex, PRInt32 aColumnIndex, PRBool aIsRow);
  virtual void RowIsDirty(nsBoxLayoutState& aBoxLayoutState, PRInt32 aIndex, PRBool aIsRow);
  virtual void RowAddedOrRemoved(nsBoxLayoutState& aBoxLayoutState, PRInt32 aIndex, PRBool aIsRow);
  virtual void CellAddedOrRemoved(nsBoxLayoutState& aBoxLayoutState, PRInt32 aIndex, PRBool aIsRow);
  virtual void DirtyRows(nsIBox* aRowBox, nsBoxLayoutState& aState);
#ifdef DEBUG_grid
  virtual void PrintCellMap();
#endif
  virtual PRInt32 GetExtraColumnCount(PRBool aIsRow);
  virtual PRInt32 GetExtraRowCount(PRBool aIsRow);

// accessors
  virtual void SetBox(nsIBox* aBox) { mBox = aBox; }
  virtual nsIBox* GetBox() { return mBox; }
  virtual nsGridRow* GetColumns();
  virtual nsGridRow* GetRows();
  virtual PRInt32 GetRowCount(PRInt32 aIsRow);
  virtual PRInt32 GetColumnCount(PRInt32 aIsRow);

protected:
  virtual void FreeMap();
  virtual void FindRowsAndColumns(nsIBox** aRows, nsIBox** aColumns);
  virtual void BuildRows(nsIBox* aBox, PRBool aSize, nsGridRow** aColumnsRows, PRBool aIsRow);
  virtual void BuildCellMap(PRInt32 aRows, PRInt32 aColumns, nsGridCell** aCells);
  virtual void PopulateCellMap(nsGridRow* aRows, nsGridRow* aColumns, PRInt32 aRowCount, PRInt32 aColumnCount, PRBool aIsRow);
  virtual void CountRowsColumns(nsIBox* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount);
  virtual void SetLargestSize(nsSize& aSize, nscoord aHeight, PRBool aIsRow);
  virtual void SetSmallestSize(nsSize& aSize, nscoord aHeight, PRBool aIsRow);

private:

  // the box that implement the <grid> tag
  nsIBox* mBox;

  // an array of row object
  nsGridRow* mRows;

  // an array of columns objects.
  nsGridRow* mColumns;

  // the first in the <grid> that implements the <rows> tag.
  nsIBox* mRowBox;

  // the first in the <grid> that implements the <columns> tag.
  nsIBox* mColumnBox;

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

