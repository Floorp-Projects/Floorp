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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsGrid.h"
#include "nsGridRowGroupLayout.h"
#include "nsBox.h"
#include "nsIScrollableFrame.h"
#include "nsSprocketLayout.h"
#include "nsGridRow.h"
#include "nsGridCell.h"

/*
The grid control expands the idea of boxes from 1 dimension to 2 dimensions. 
It works by allowing the XUL to define a collection of rows and columns and then 
stacking them on top of each other. Here is and example.

Example 1:

<grid>
   <columns>
      <column/>
      <column/>
   </columns>

   <rows>
      <row/>
      <row/>
   </rows>
</grid>

example 2:

<grid>
   <columns>
      <column flex="1"/>
      <column flex="1"/>
   </columns>

   <rows>
      <row>
         <text value="hello"/>
         <text value="there"/>
      </row>
   </rows>
</grid>

example 3:

<grid>

<rows>
      <row>
         <text value="hello"/>
         <text value="there"/>
      </row>
   </rows>

   <columns>
      <column>
         <text value="Hey I'm in the column and I'm on top!"/>
      </column>
      <column/>
   </columns>

</grid>

The columns are first and the rows are second, so the rows will be drawn on top of the columns. 
You can reverse this by defining the rows first.
Other tags are then placed in the <row> or <column> tags causing the grid  to accommodate everyone.  
It does this by creating 3 things: A cellmap, a row list, and a column list. The cellmap is a 2 
dimensional array of nsGridCells. Each cell contains 2 boxes.  One cell from the column list 
and one from the row list. When a cell is asked for its size it returns that smallest size it can 
be to accommodate the 2 cells. Row lists and Column lists use the same data structure: nsGridRow. 
Essentially a row and column are the same except a row goes alone the x axis and a column the y. 
To make things easier and save code everything is written in terms of the x dimension. A flag is 
passed in called "isRow" that can flip the calculations to the y axis.

Usually the number of cells in a row match the number of columns, but not always. 
It is possible to define 5 columns for a grid but have 10 cells in one of the rows. 
In this case 5 extra columns will be added to the column list to handle the situation. 
These are called extraColumns/Rows.
*/

nsGrid::nsGrid():mBox(nsnull),
                 mColumns(nsnull), 
                 mRows(nsnull),
                 mRowBox(nsnull),
                 mColumnBox(nsnull),
                 mNeedsRebuild(PR_TRUE),
                 mCellMap(nsnull),
                 mMarkingDirty(PR_FALSE),
                 mColumnCount(0),
                 mRowCount(0),
                 mExtraRowCount(0),
                 mExtraColumnCount(0)
{
    MOZ_COUNT_CTOR(nsGrid);
}

nsGrid::~nsGrid()
{
    FreeMap();
    MOZ_COUNT_DTOR(nsGrid);
}

/*
 * This is called whenever something major happens in the grid. And example 
 * might be when many cells or row are added. It sets a flag signaling that 
 * all the grids caches information should be recalculated.
 */
void
nsGrid::NeedsRebuild(nsBoxLayoutState& aState)
{
  if (mNeedsRebuild)
    return;

  // iterate through columns and rows and dirty them
  mNeedsRebuild = PR_TRUE;

  // free the map
  FreeMap();

  // find the new row and column box. They could have 
  // been changed.
  FindRowsAndColumns(&mRowBox, &mColumnBox);

  // tell all the rows and columns they are dirty
  DirtyRows(mRowBox, aState);
  DirtyRows(mColumnBox, aState);
}


/**
 * Run through the rows in the given box and mark them dirty so they 
 * will get recalculated and get a layout.
 */
void 
nsGrid::DirtyRows(nsIBox* aRowBox, nsBoxLayoutState& aState)
{
  // make sure we prevent others from dirtying things.
  mMarkingDirty = PR_TRUE;

  // if the box is a grid part have it recursively hand it.
  if (aRowBox) {
    nsCOMPtr<nsIBoxLayout> layout;
    aRowBox->GetLayoutManager(getter_AddRefs(layout));
    if (layout) {
       nsCOMPtr<nsIGridPart> part( do_QueryInterface(layout) );
       if (part) 
          part->DirtyRows(aRowBox, aState);
    }
  }

  mMarkingDirty = PR_FALSE;
}

nsGridRow* nsGrid::GetColumns()
{
  RebuildIfNeeded();

  return mColumns;
}

nsGridRow* nsGrid::GetRows()
{
  RebuildIfNeeded();

  return mRows;
}

nsGridRow*
nsGrid::GetColumnAt(PRInt32 aIndex, PRBool aIsRow)
{
  return GetRowAt(aIndex, !aIsRow);
}

nsGridRow*
nsGrid::GetRowAt(PRInt32 aIndex, PRBool aIsRow)
{
  RebuildIfNeeded();

  if (aIsRow) {
    NS_ASSERTION(aIndex < mRowCount || aIndex >= 0, "Index out of range");
    return &mRows[aIndex];
  } else {
    NS_ASSERTION(aIndex < mColumnCount || aIndex >= 0, "Index out of range");
    return &mColumns[aIndex];
  }
}

nsGridCell*
nsGrid::GetCellAt(PRInt32 aX, PRInt32 aY)
{
  RebuildIfNeeded();

  NS_ASSERTION(aY < mRowCount && aY >= 0, "Index out of range");
  NS_ASSERTION(aX < mColumnCount && aX >= 0, "Index out of range");
  return &mCellMap[aY*mColumnCount+aX];
}

PRInt32
nsGrid::GetExtraColumnCount(PRBool aIsRow)
{
  return GetExtraRowCount(!aIsRow);
}

PRInt32
nsGrid::GetExtraRowCount(PRBool aIsRow)
{
  RebuildIfNeeded();

  if (aIsRow)
    return mExtraRowCount;
  else
    return mExtraColumnCount;
}

/**
 * If we are marked for rebuild. Then build everything
 */
void
nsGrid::RebuildIfNeeded()
{
  if (!mNeedsRebuild)
    return;

  mNeedsRebuild = PR_FALSE;

  // find the row and columns frames
  FindRowsAndColumns(&mRowBox, &mColumnBox);

  // count the rows and columns
  PRInt32 computedRowCount = 0;
  PRInt32 computedColumnCount = 0;
  CountRowsColumns(mRowBox, mRowCount, computedColumnCount);
  CountRowsColumns(mColumnBox, mColumnCount, computedRowCount);

  // computedRowCount are the actual number of rows as determined by the 
  // columns children.
  // computedColumnCount are the number of columns as determined by the number
  // of rows children.
  // We can use this information to see how many extra columns or rows we need.
  // This can happen if there are are more children in a row that number of columns
  // defined. Example:
  //
  // <columns>
  //   <column/>
  // </columns>
  //
  // <rows>
  //   <row>
  //     <button/><button/>
  //   </row>
  // </rows>
  //
  // computedColumnCount = 2 // for the 2 buttons in the the row tag
  // computedRowCount = 0 // there is nothing in the  column tag
  // mColumnCount = 1 // one column defined
  // mRowCount = 1 // one row defined
  // 
  // So in this case we need to make 1 extra column.
  //

  if (computedColumnCount > mColumnCount) {
     mExtraColumnCount = computedColumnCount - mColumnCount;
     mColumnCount = computedColumnCount;
  }

  if (computedRowCount > mRowCount) {
     mExtraRowCount    = computedRowCount - mRowCount;
     mRowCount = computedRowCount;
  }

  // build and poplulate row and columns arrays
  BuildRows(mRowBox, mRowCount, &mRows, PR_TRUE);
  BuildRows(mColumnBox, mColumnCount, &mColumns, PR_FALSE);

  // build and populate the cell map
  BuildCellMap(mRowCount, mColumnCount, &mCellMap);

  // populate the cell map from column and row children
  PopulateCellMap(mRows, mColumns, mRowCount, mColumnCount, PR_TRUE);
  PopulateCellMap(mColumns, mRows, mColumnCount, mRowCount, PR_FALSE);
}

void
nsGrid::FreeMap()
{
  if (mRows) 
    delete[] mRows;

  if (mColumns)
    delete[] mColumns;

  if (mCellMap)
    delete[] mCellMap;

  mRows = nsnull;
  mColumns = nsnull;
  mCellMap = nsnull;
  mColumnCount = 0;
  mRowCount = 0;
  mExtraColumnCount = 0;
  mExtraRowCount = 0;
  mRowBox = nsnull;
  mColumnBox = nsnull;
}

/**
 * finds the first <rows> and <columns> tags in the <grid> tag
 */
void
nsGrid::FindRowsAndColumns(nsIBox** aRows, nsIBox** aColumns)
{
  *aRows = nsnull;
  *aColumns = nsnull;

  // find the boxes that contain our rows and columns
  nsIBox* child = nsnull;
  mBox->GetChildBox(&child);

  while(child)
  {
    nsIBox* oldBox = child;
    nsresult rv = NS_OK;
    nsCOMPtr<nsIScrollableFrame> scrollFrame = do_QueryInterface(child, &rv);
    if (scrollFrame) {
       nsIFrame* scrolledFrame = nsnull;
       scrollFrame->GetScrolledFrame(nsnull, scrolledFrame);
       NS_ASSERTION(scrolledFrame,"Error no scroll frame!!");
       nsCOMPtr<nsIBox> b = do_QueryInterface(scrolledFrame);
       child = b;
    }

    nsCOMPtr<nsIBoxLayout> layout;
    child->GetLayoutManager(getter_AddRefs(layout));

     nsCOMPtr<nsIGridPart> monument( do_QueryInterface(layout) );
     if (monument)
     {
       nsGridRowGroupLayout* rowGroup = nsnull;
       monument->CastToRowGroupLayout(&rowGroup);
       if (rowGroup) {
          PRBool isRow = !nsSprocketLayout::IsHorizontal(child);
          if (isRow)
            *aRows = child;
          else
            *aColumns = child;
         
          if (*aRows && *aColumns)
            return;
       }
     }

    if (scrollFrame) {
      child = oldBox;
    }

    child->GetNextBox(&child);
  }
}

/**
 * Count the number of rows and columns in the given box. aRowCount well become the actual number
 * rows defined in the xul. aComputedColumnCount will become the number of columns by counting the number
 * of cells in each row.
 */
void
nsGrid::CountRowsColumns(nsIBox* aRowBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount)
{
  // get the rowboxes layout manager. Then ask it to do the work for us
  if (aRowBox) {
    nsCOMPtr<nsIBoxLayout> layout;
    aRowBox->GetLayoutManager(getter_AddRefs(layout));
    if (layout) {
       nsCOMPtr<nsIGridPart> monument( do_QueryInterface(layout) );
       if (monument) 
          monument->CountRowsColumns(aRowBox, aRowCount, aComputedColumnCount);
    }
  }
}


/**
 * Given the number of rows create nsGridRow objects for them and full them out.
 */
void
nsGrid::BuildRows(nsIBox* aBox, PRBool aRowCount, nsGridRow** aRows, PRBool aIsRow)
{
  // if not rows then return null
  if (aRowCount == 0) {
    *aRows = nsnull;
    return;
  }

  // create the array
  PRInt32 count = 0;
  nsGridRow* row = new nsGridRow[aRowCount];

  // populate it if we can. If not it will contain only dynamic columns
  if (aBox)
  {
    nsCOMPtr<nsIBoxLayout> layout;
    aBox->GetLayoutManager(getter_AddRefs(layout));
    if (layout) {
      nsCOMPtr<nsIGridPart> monument( do_QueryInterface(layout) );
      if (monument) {
         PRInt32 count;
         monument->BuildRows(aBox, row, &count);
      }
    }
  }

  *aRows = row;
}


/**
 * Given the number of rows and columns. Build a cellmap
 */
void
nsGrid::BuildCellMap(PRInt32 aRows, PRInt32 aColumns, nsGridCell** aCells)
{
  PRInt32 size = aRows*aColumns;
  if (size == 0)
    (*aCells) = nsnull;
  else 
    (*aCells) = new nsGridCell[size];
}

/** 
 * Run through all the cells in the rows and columns and populate then with 2 cells. One from the row and one
 * from the column
 */
void
nsGrid::PopulateCellMap(nsGridRow* aRows, nsGridRow* aColumns, PRInt32 aRowCount, PRInt32 aColumnCount, PRBool aIsRow)
{
  if (!aRows)
    return;

   // look through the columns
  nscoord j = 0;

  for(PRInt32 i=0; i < aRowCount; i++) 
  {
     nsIBox* child = nsnull;
     nsGridRow* row = &aRows[i];

     // skip bogus rows. They have no cells
     if (row->mIsBogus) 
       continue;

     child = row->mBox;
     if (child) {
       child->GetChildBox(&child);

       j = 0;

       while(child && j < aColumnCount)
       {
         // skip bogus column. They have no cells
         nsGridRow* column = &aColumns[j];
         if (column->mIsBogus) 
         {
           j++;
           continue;
         }

         if (aIsRow)
           GetCellAt(j,i)->SetBoxInRow(child);
         else
           GetCellAt(i,j)->SetBoxInColumn(child);

         child->GetNextBox(&child);

         j++;
       }
     }
  }
}

/**
 * These methods return the preferred, min, max sizes for a given row index.
 * aIsRow if aIsRow is PR_TRUE. If you pass PR_FALSE you will get the inverse.
 * As if you called GetPrefColumnSize(aState, index, aPref)
 */
nsresult
nsGrid::GetPrefRowSize(nsBoxLayoutState& aState, PRInt32 aRowIndex, nsSize& aSize, PRBool aIsRow)
{ 
  //NS_ASSERTION(aRowIndex >=0 && aRowIndex < GetRowCount(aIsRow), "Row index out of range!");
  if (!(aRowIndex >=0 && aRowIndex < GetRowCount(aIsRow)))
    return NS_OK;

  nscoord height = 0;
  GetPrefRowHeight(aState, aRowIndex, height, aIsRow);
  SetLargestSize(aSize, height, aIsRow);

  return NS_OK;
}

nsresult
nsGrid::GetMinRowSize(nsBoxLayoutState& aState, PRInt32 aRowIndex, nsSize& aSize, PRBool aIsRow)
{ 
  if (!(aRowIndex >=0 && aRowIndex < GetRowCount(aIsRow)))
    return NS_OK;

  nscoord height = 0;
  GetMinRowHeight(aState, aRowIndex, height, aIsRow);
  SetLargestSize(aSize, height, aIsRow);

  return NS_OK;
}

nsresult
nsGrid::GetMaxRowSize(nsBoxLayoutState& aState, PRInt32 aRowIndex, nsSize& aSize, PRBool aIsRow)
{ 
  if (!(aRowIndex >=0 && aRowIndex < GetRowCount(aIsRow)))
    return NS_OK;

  nscoord height = 0;
  GetMaxRowHeight(aState, aRowIndex, height, aIsRow);
  SetSmallestSize(aSize, height, aIsRow);

  return NS_OK;
}

/**
 * These methods return the preferred, min, max coord for a given row index if
 * aIsRow is PR_TRUE. If you pass PR_FALSE you will get the inverse.
 * As if you called GetPrefColumnHeight(aState, index, aPref).
 */
nsresult
nsGrid::GetPrefRowHeight(nsBoxLayoutState& aState, PRInt32 aIndex, nscoord& aSize, PRBool aIsRow)
{
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsRow);

  if (row->IsPrefSet()) 
  {
    aSize = row->mPref;
    return NS_OK;
  }

  nsIBox* box = row->mBox;

  // set in CSS?
  if (box) {
    nsSize cssSize;
    cssSize.width = -1;
    cssSize.height = -1;
    nsIBox::AddCSSPrefSize(aState, box, cssSize);
    // TBD subtract our borders and padding place them in 
    // left and right padding

    row->mPref = GET_HEIGHT(cssSize, aIsRow);

    // yep do nothing.
    if (row->mPref != -1)
    {
      aSize = row->mPref;
      return NS_OK;
    }
  }

  nsSize size(0,0);

  nsGridCell* child;

  PRInt32 count = GetColumnCount(aIsRow); 

  PRBool isCollapsed = PR_FALSE;

  for (PRInt32 i=0; i < count; i++)
  {  
    if (aIsRow)
     child = GetCellAt(i,aIndex);
    else
     child = GetCellAt(aIndex,i);

    // ignore collapsed children
    child->IsCollapsed(aState, isCollapsed);

    if (!isCollapsed)
    {
      nsSize childSize(0,0);

      child->GetPrefSize(aState, childSize);

      nsSprocketLayout::AddLargestSize(size, childSize, aIsRow);
    }
  }

  row->mPref = GET_HEIGHT(size, aIsRow);

  aSize = row->mPref;

  return NS_OK;
}

nsresult
nsGrid::GetMinRowHeight(nsBoxLayoutState& aState, PRInt32 aIndex, nscoord& aSize, PRBool aIsRow)
{
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsRow);

  if (row->IsMinSet()) 
  {
    aSize = row->mMin;
    return NS_OK;
  }

  nsIBox* box = row->mBox;

  // set in CSS?
  if (box) {
    nsSize cssSize;
    cssSize.width = -1;
    cssSize.height = -1;
    nsIBox::AddCSSMinSize(aState, box, cssSize);
    // TBD subtract our borders and padding place them in 
    // left and right padding

    row->mMin = GET_HEIGHT(cssSize, aIsRow);

    // yep do nothing.
    if (row->mMin != -1)
    {
      aSize = row->mMin;
      return NS_OK;
    }
  }

  nsSize size(0,0);

  nsGridCell* child;

  PRInt32 count = GetColumnCount(aIsRow); 

  PRBool isCollapsed = PR_FALSE;

  for (PRInt32 i=0; i < count; i++)
  {  
    if (aIsRow)
     child = GetCellAt(i,aIndex);
    else
     child = GetCellAt(aIndex,i);

    // ignore collapsed children
    child->IsCollapsed(aState, isCollapsed);

    if (!isCollapsed)
    {
      nsSize childSize(0,0);

      child->GetMinSize(aState, childSize);

      nsSprocketLayout::AddLargestSize(size, childSize, aIsRow);
    }
  }

  row->mMin = GET_HEIGHT(size, aIsRow);

  aSize = row->mMin;

  return NS_OK;
}

nsresult
nsGrid::GetMaxRowHeight(nsBoxLayoutState& aState, PRInt32 aIndex, nscoord& aSize, PRBool aIsRow)
{
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsRow);

  if (row->IsMaxSet()) 
  {
    aSize = row->mMax;
    return NS_OK;
  }

  nsIBox* box = row->mBox;

  // set in CSS?
  if (box) {
    nsSize cssSize;
    cssSize.width = -1;
    cssSize.height = -1;
    nsIBox::AddCSSMaxSize(aState, box, cssSize);
    // TBD subtract our borders and padding place them in 
    // left and right padding

    row->mMax = GET_HEIGHT(cssSize, aIsRow);

    // yep do nothing.
    if (row->mMax != -1)
    {
      aSize = row->mMax;
      return NS_OK;
    }
  }

  nsSize size(NS_INTRINSICSIZE,NS_INTRINSICSIZE);

  nsGridCell* child;

  PRInt32 count = GetColumnCount(aIsRow); 

  PRBool isCollapsed = PR_FALSE;

  for (PRInt32 i=0; i < count; i++)
  {  
    if (aIsRow)
     child = GetCellAt(i,aIndex);
    else
     child = GetCellAt(aIndex,i);

    // ignore collapsed children
    child->IsCollapsed(aState, isCollapsed);

    if (!isCollapsed)
    {
      nsSize childSize(0,0);

      child->GetMaxSize(aState, childSize);

      nsSprocketLayout::AddLargestSize(size, childSize, aIsRow);
    }
  }

  row->mMax = GET_HEIGHT(size, aIsRow);

  aSize = row->mMax;

  return NS_OK;
}

nsresult
nsGrid::GetRowFlex(nsBoxLayoutState& aState, PRInt32 aIndex, nscoord& aFlex, PRBool aIsRow)
{
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsRow);

  if (row->IsFlexSet()) 
  {
    aFlex = row->mFlex;
    return NS_OK;
  }

  nsIBox* box = row->mBox;
  row->mFlex = 0;

  if (box) {
    box->GetFlex(aState, row->mFlex);
    nsIBox::AddCSSFlex(aState, box, row->mFlex);
  }
 
  aFlex = row->mFlex;

  return NS_OK;
}

void
nsGrid::SetLargestSize(nsSize& aSize, nscoord aHeight, PRBool aIsRow)
{
  if (aIsRow) {
    if (aSize.height < aHeight)
      aSize.height = aHeight;
  } else {
    if (aSize.width < aHeight)
      aSize.width = aHeight;
  }
}

void
nsGrid::SetSmallestSize(nsSize& aSize, nscoord aHeight, PRBool aIsRow)
{
  if (aIsRow) {
    if (aSize.height > aHeight)
      aSize.height = aHeight;
  } else {
    if (aSize.width < aHeight)
      aSize.width = aHeight;
  }
}

PRInt32 
nsGrid::GetRowCount(PRInt32 aIsRow)
{
  RebuildIfNeeded();

  if (aIsRow)
    return mRowCount;
  else
    return mColumnCount;
}

PRInt32 
nsGrid::GetColumnCount(PRInt32 aIsRow)
{
  return GetRowCount(!aIsRow);
}

/**
 * This is called if a child in a row became dirty. This happens if the child gets bigger or smaller
 * in some way.
 */
void 
nsGrid::RowChildIsDirty(nsBoxLayoutState& aState, PRInt32 aRowIndex, PRInt32 aColumnIndex, PRBool aIsRow)
{ 
  // if we are already dirty do nothing.
  if (mNeedsRebuild || mMarkingDirty)
    return;

  mMarkingDirty = PR_TRUE;

  // index out of range. Rebuild it all
  if (aRowIndex >= GetRowCount(aIsRow) || aColumnIndex >= GetColumnCount(aIsRow))
  {
     NeedsRebuild(aState);
     return;
  }

  // dirty our 2 outer nsGridRowGroups. (one for columns and one for rows)
  // we need to do this because the rows and column we are given may be Extra ones
  // and may not have any Box associated with them. If you dirtied them then
  // corresponding nsGridRowGroup around them would never get dirty. So lets just
  // do it manually here.

  if (mRowBox)
    mRowBox->MarkDirty(aState);

  if (mColumnBox)
    mColumnBox->MarkDirty(aState);

  // dirty just our row and column that we were given
  nsGridRow* row = GetRowAt(aRowIndex, aIsRow);
  row->MarkDirty(aState);

  nsGridRow* column = GetColumnAt(aColumnIndex, aIsRow);
  column->MarkDirty(aState);


  mMarkingDirty = PR_FALSE;
}

/**
 * The row became dirty. This happens if the row's borders change or children inside it
 * force it to change size
 */
void 
nsGrid::RowIsDirty(nsBoxLayoutState& aState, PRInt32 aIndex, PRBool aIsRow)
{
  if (mMarkingDirty)
    return;

  NeedsRebuild(aState);
}

/*
 * a Cell in the given row or columns at the given index has had a child added or removed
 */
void 
nsGrid::CellAddedOrRemoved(nsBoxLayoutState& aState, PRInt32 aIndex, PRBool aIsRow)
{
  // see if the cell will fit in our current row. If it will
  // just add it. If not rebuild everything.
  if (mMarkingDirty)
    return;

  NeedsRebuild(aState);
}

/**
 * A row or columns at the given index had been added or removed
 */
void 
nsGrid::RowAddedOrRemoved(nsBoxLayoutState& aState, PRInt32 aIndex, PRBool aIsRow)
{
  // see if we have extra room in the table and just add the new row in
  if (mMarkingDirty)
    return;

  NeedsRebuild(aState);
}

#ifdef DEBUG_grid
void
nsGrid::PrintCellMap()
{
  
  printf("-----Columns------\n");
  for (int x=0; x < mColumnCount; x++) 
  {
   
    nsGridRow* column = GetColumnAt(x);
    printf("%d(pf=%d, mn=%d, mx=%d) ", x, column->mPref, column->mMin, column->mMax);
  }

  printf("\n-----Rows------\n");
  for (x=0; x < mRowCount; x++) 
  {
    nsGridRow* column = GetRowAt(x);
    printf("%d(pf=%d, mn=%d, mx=%d) ", x, column->mPref, column->mMin, column->mMax);
  }

  printf("\n");

  /*
  printf("-----CellMap------\n");
  for (int y=0; y < mRowCount; y++)
  {
    for (int x=0; x < mColumnCount; x++) 
    {
      nsGridCell* cell = GetCellAt(x,y);
      //printf("(%d)@%p[@%p,@%p] ", y*mColumnCount+x, cell, cell->GetBoxInRow(), cell->GetBoxInColumn());
      printf("p=%d, ", y*mColumnCount+x, cell, cell->GetBoxInRow(), cell->GetBoxInColumn());
    }
    printf("\n");
  }
  */
}
#endif
