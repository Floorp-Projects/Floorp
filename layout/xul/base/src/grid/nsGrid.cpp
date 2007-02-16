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

Usually the columns are first and the rows are second, so the rows will be drawn on top of the columns. 
You can reverse this by defining the rows first.
Other tags are then placed in the <row> or <column> tags causing the grid to accommodate everyone.  
It does this by creating 3 things: A cellmap, a row list, and a column list. The cellmap is a 2 
dimensional array of nsGridCells. Each cell contains 2 boxes.  One cell from the column list 
and one from the row list. When a cell is asked for its size it returns that smallest size it can 
be to accommodate the 2 cells. Row lists and Column lists use the same data structure: nsGridRow. 
Essentially a row and column are the same except a row goes alone the x axis and a column the y. 
To make things easier and save code everything is written in terms of the x dimension. A flag is 
passed in called "isHorizontal" that can flip the calculations to the y axis.

Usually the number of cells in a row match the number of columns, but not always. 
It is possible to define 5 columns for a grid but have 10 cells in one of the rows. 
In this case 5 extra columns will be added to the column list to handle the situation. 
These are called extraColumns/Rows.
*/

nsGrid::nsGrid():mBox(nsnull),
                 mRows(nsnull),
                 mColumns(nsnull), 
                 mRowsBox(nsnull),
                 mColumnsBox(nsnull),
                 mNeedsRebuild(PR_TRUE),
                 mRowCount(0),
                 mColumnCount(0),
                 mExtraRowCount(0),
                 mExtraColumnCount(0),
                 mCellMap(nsnull),
                 mMarkingDirty(PR_FALSE)
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

  // find the new row and column box. They could have 
  // been changed.
  mRowsBox = nsnull;
  mColumnsBox = nsnull;
  FindRowsAndColumns(&mRowsBox, &mColumnsBox);

  // tell all the rows and columns they are dirty
  DirtyRows(mRowsBox, aState);
  DirtyRows(mColumnsBox, aState);
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
  FindRowsAndColumns(&mRowsBox, &mColumnsBox);

  // count the rows and columns
  PRInt32 computedRowCount = 0;
  PRInt32 computedColumnCount = 0;
  PRInt32 rowCount = 0;
  PRInt32 columnCount = 0;

  CountRowsColumns(mRowsBox, rowCount, computedColumnCount);
  CountRowsColumns(mColumnsBox, columnCount, computedRowCount);

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
  // computedColumnCount = 2 // for the 2 buttons in the row tag
  // computedRowCount = 0 // there is nothing in the  column tag
  // mColumnCount = 1 // one column defined
  // mRowCount = 1 // one row defined
  // 
  // So in this case we need to make 1 extra column.
  //

  // Make sure to update mExtraColumnCount no matter what, since it might
  // happen that we now have as many columns as are defined, and we wouldn't
  // want to have a positive mExtraColumnCount hanging about in that case!
  mExtraColumnCount = computedColumnCount - columnCount;
  if (computedColumnCount > columnCount) {
     columnCount = computedColumnCount;
  }

  // Same for rows.
  mExtraRowCount = computedRowCount - rowCount;
  if (computedRowCount > rowCount) {
     rowCount = computedRowCount;
  }

  // build and poplulate row and columns arrays
  BuildRows(mRowsBox, rowCount, &mRows, PR_TRUE);
  BuildRows(mColumnsBox, columnCount, &mColumns, PR_FALSE);

  // build and populate the cell map
  mCellMap = BuildCellMap(rowCount, columnCount);

  mRowCount = rowCount;
  mColumnCount = columnCount;

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
  mRowsBox = nsnull;
  mColumnsBox = nsnull;
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
  // if we have <grid></grid> then mBox will be null (bug 125689)
  if (mBox)
    mBox->GetChildBox(&child);

  while(child)
  {
    nsIBox* oldBox = child;
    nsresult rv = NS_OK;
    nsCOMPtr<nsIScrollableFrame> scrollFrame = do_QueryInterface(child, &rv);
    if (scrollFrame) {
       nsIFrame* scrolledFrame = scrollFrame->GetScrolledFrame();
       NS_ASSERTION(scrolledFrame,"Error no scroll frame!!");
       if (NS_FAILED(CallQueryInterface(scrolledFrame, &child)))
         child = nsnull;
    }

    nsCOMPtr<nsIBoxLayout> layout;
    child->GetLayoutManager(getter_AddRefs(layout));

     nsCOMPtr<nsIGridPart> monument( do_QueryInterface(layout) );
     if (monument)
     {
       nsGridRowGroupLayout* rowGroup = monument->CastToRowGroupLayout();
       if (rowGroup) {
          PRBool isHorizontal = !nsSprocketLayout::IsHorizontal(child);
          if (isHorizontal)
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
nsGrid::BuildRows(nsIBox* aBox, PRInt32 aRowCount, nsGridRow** aRows, PRBool aIsHorizontal)
{
  // if no rows then return null
  if (aRowCount == 0) {

    // make sure we free up the memory.
    if (*aRows)
      delete[] (*aRows);

    *aRows = nsnull;
    return;
  }

  // create the array
  nsGridRow* row;
  
  // only create new rows if we have to. Reuse old rows.
  if (aIsHorizontal)
  { 
    if (aRowCount > mRowCount) {
       delete[] mRows;
       row = new nsGridRow[aRowCount];
    } else {
      for (PRInt32 i=0; i < mRowCount; i++)
        mRows[i].Init(nsnull, PR_FALSE);

      row = mRows;
    }
  } else {
    if (aRowCount > mColumnCount) {
       delete[] mColumns;
       row = new nsGridRow[aRowCount];
    } else {
       for (PRInt32 i=0; i < mColumnCount; i++)
         mColumns[i].Init(nsnull, PR_FALSE);

       row = mColumns;
    }
  }

  // populate it if we can. If not it will contain only dynamic columns
  if (aBox)
  {
    nsCOMPtr<nsIBoxLayout> layout;
    aBox->GetLayoutManager(getter_AddRefs(layout));
    if (layout) {
      nsCOMPtr<nsIGridPart> monument( do_QueryInterface(layout) );
      if (monument) {
         monument->BuildRows(aBox, row);
      }
    }
  }

  *aRows = row;
}


/**
 * Given the number of rows and columns. Build a cellmap
 */
nsGridCell*
nsGrid::BuildCellMap(PRInt32 aRows, PRInt32 aColumns)
{
  PRInt32 size = aRows*aColumns;
  PRInt32 oldsize = mRowCount*mColumnCount;
  if (size == 0) {
    delete[] mCellMap;
  }
  else {
    if (size > oldsize) {
      delete[] mCellMap;
      return new nsGridCell[size];
    } else {
      // clear out cellmap
      for (PRInt32 i=0; i < oldsize; i++)
      {
        mCellMap[i].SetBoxInRow(nsnull);
        mCellMap[i].SetBoxInColumn(nsnull);
      }
      return mCellMap;
    }
  }
  return nsnull;
}

/** 
 * Run through all the cells in the rows and columns and populate then with 2 cells. One from the row and one
 * from the column
 */
void
nsGrid::PopulateCellMap(nsGridRow* aRows, nsGridRow* aColumns, PRInt32 aRowCount, PRInt32 aColumnCount, PRBool aIsHorizontal)
{
  if (!aRows)
    return;

   // look through the columns
  PRInt32 j = 0;

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

         if (aIsHorizontal)
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
nsGrid::GetColumnAt(PRInt32 aIndex, PRBool aIsHorizontal)
{
  return GetRowAt(aIndex, !aIsHorizontal);
}

nsGridRow*
nsGrid::GetRowAt(PRInt32 aIndex, PRBool aIsHorizontal)
{
  RebuildIfNeeded();

  if (aIsHorizontal) {
    NS_ASSERTION(aIndex < mRowCount && aIndex >= 0, "Index out of range");
    return &mRows[aIndex];
  } else {
    NS_ASSERTION(aIndex < mColumnCount && aIndex >= 0, "Index out of range");
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
nsGrid::GetExtraColumnCount(PRBool aIsHorizontal)
{
  return GetExtraRowCount(!aIsHorizontal);
}

PRInt32
nsGrid::GetExtraRowCount(PRBool aIsHorizontal)
{
  RebuildIfNeeded();

  if (aIsHorizontal)
    return mExtraRowCount;
  else
    return mExtraColumnCount;
}


/**
 * These methods return the preferred, min, max sizes for a given row index.
 * aIsHorizontal if aIsHorizontal is PR_TRUE. If you pass PR_FALSE you will get the inverse.
 * As if you called GetPrefColumnSize(aState, index, aPref)
 */
nsSize
nsGrid::GetPrefRowSize(nsBoxLayoutState& aState, PRInt32 aRowIndex, PRBool aIsHorizontal)
{ 
  NS_ASSERTION(aRowIndex >=0 && aRowIndex < GetRowCount(aIsHorizontal), "Row index out of range!");

  nsSize size(0,0);
  if (!(aRowIndex >=0 && aRowIndex < GetRowCount(aIsHorizontal)))
    return size;

  nscoord height = GetPrefRowHeight(aState, aRowIndex, aIsHorizontal);
  SetLargestSize(size, height, aIsHorizontal);

  return size;
}

nsSize
nsGrid::GetMinRowSize(nsBoxLayoutState& aState, PRInt32 aRowIndex, PRBool aIsHorizontal)
{ 
  NS_ASSERTION(aRowIndex >=0 && aRowIndex < GetRowCount(aIsHorizontal), "Row index out of range!");

  nsSize size(0,0);
  if (!(aRowIndex >=0 && aRowIndex < GetRowCount(aIsHorizontal)))
    return size;

  nscoord height = GetMinRowHeight(aState, aRowIndex, aIsHorizontal);
  SetLargestSize(size, height, aIsHorizontal);

  return size;
}

nsSize
nsGrid::GetMaxRowSize(nsBoxLayoutState& aState, PRInt32 aRowIndex, PRBool aIsHorizontal)
{ 
  NS_ASSERTION(aRowIndex >=0 && aRowIndex < GetRowCount(aIsHorizontal), "Row index out of range!");

  nsSize size(NS_INTRINSICSIZE,NS_INTRINSICSIZE);
  if (!(aRowIndex >=0 && aRowIndex < GetRowCount(aIsHorizontal)))
    return size;

  nscoord height = GetMaxRowHeight(aState, aRowIndex, aIsHorizontal);
  SetSmallestSize(size, height, aIsHorizontal);

  return size;
}

void 
nsGrid::GetPartFromBox(nsIBox* aBox, nsIGridPart** aPart)
{
  *aPart = nsnull;

  if (aBox) {
    nsCOMPtr<nsIBoxLayout> layout;
    aBox->GetLayoutManager(getter_AddRefs(layout));
    if (layout) {
       nsCOMPtr<nsIGridPart> part( do_QueryInterface(layout) );
       if (part) { 
          *aPart = part.get();
          NS_IF_ADDREF(*aPart);
       }
    }
  }
}

nsMargin
nsGrid::GetBoxTotalMargin(nsIBox* aBox, PRBool aIsHorizontal)
{
  nsMargin margin(0,0,0,0);
  // walk the boxes parent chain getting the border/padding/margin of our parent rows
  
  // first get the layour manager
  nsCOMPtr<nsIGridPart> part;
  GetPartFromBox(aBox, getter_AddRefs(part));
  if (part)
    margin = part->GetTotalMargin(aBox, aIsHorizontal);

  return margin;
}

/**
 * The first and last rows can be affected by <rows> tags with borders or margin
 * gets first and last rows and their indexes.
 * If it fails because there are no rows then:
 * FirstRow is nsnull
 * LastRow is nsnull
 * aFirstIndex = -1
 * aLastIndex = -1
 */
void
nsGrid::GetFirstAndLastRow(nsBoxLayoutState& aState, 
                          PRInt32& aFirstIndex, 
                          PRInt32& aLastIndex, 
                          nsGridRow*& aFirstRow,
                          nsGridRow*& aLastRow,
                          PRBool aIsHorizontal)
{
  aFirstRow = nsnull;
  aLastRow = nsnull;
  aFirstIndex = -1;
  aLastIndex = -1;

  PRInt32 count = GetRowCount(aIsHorizontal);

  if (count == 0)
    return;


  // We could have collapsed columns either before or after our index.
  // they should not count. So if we are the 5th row and the first 4 are
  // collaped we become the first row. Or if we are the 9th row and
  // 10 up to the last row are collapsed we then become the last.

  // see if we are first
  PRInt32 i;
  for (i=0; i < count; i++)
  {
     nsGridRow* row = GetRowAt(i,aIsHorizontal);
     if (!row->IsCollapsed(aState)) {
       aFirstIndex = i;
       aFirstRow = row;
       break;
     }
  }

  // see if we are last
  for (i=count-1; i >= 0; i--)
  {
     nsGridRow* row = GetRowAt(i,aIsHorizontal);
     if (!row->IsCollapsed(aState)) {
       aLastIndex = i;
       aLastRow = row;
       break;
     }

  }
}

/**
 * A row can have a top and bottom offset. Usually this is just the top and bottom border/padding.
 * However if the row is the first or last it could be affected by the fact a column or columns could
 * have a top or bottom margin. 
 */
void
nsGrid::GetRowOffsets(nsBoxLayoutState& aState, PRInt32 aIndex, nscoord& aTop, nscoord& aBottom, PRBool aIsHorizontal)
{

  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsHorizontal);

  if (row->IsOffsetSet()) 
  {
    aTop    = row->mTop;
    aBottom = row->mBottom;
    return;
  }

  // first get the rows top and bottom border and padding
  nsIBox* box = row->GetBox();

  // add up all the padding
  nsMargin margin(0,0,0,0);
  nsMargin inset(0,0,0,0);
  nsMargin border(0,0,0,0);
  nsMargin padding(0,0,0,0);
  nsMargin totalBorderPadding(0,0,0,0);
  nsMargin totalMargin(0,0,0,0);

  // if there is a box and it's not bogus take its
  // borders padding and insets into account
  if (box && !row->mIsBogus)
  {
    if (!box->IsCollapsed(aState))
    {
       box->GetInset(inset);

       // get real border and padding. GetBorderAndPadding
       // is redefined on nsGridRowLeafFrame. If we called it here
       // we would be in finite recurson.
       box->GetBorder(border);
       box->GetPadding(padding);

       totalBorderPadding += inset; 
       totalBorderPadding += border;
       totalBorderPadding += padding;
     }

     // if we are the first or last row
     // take into account <rows> tags around us
     // that could have borders or margins.
     // fortunately they only affect the first
     // and last row inside the <rows> tag

     totalMargin = GetBoxTotalMargin(box, aIsHorizontal);
  }

  if (aIsHorizontal) {
    row->mTop = totalBorderPadding.top;
    row->mBottom = totalBorderPadding.bottom;
    row->mTopMargin = totalMargin.top;
    row->mBottomMargin = totalMargin.bottom;
  } else {
    row->mTop = totalBorderPadding.left;
    row->mBottom = totalBorderPadding.right;
    row->mTopMargin = totalMargin.left;
    row->mBottomMargin = totalMargin.right;
  }

  // if we are the first or last row take into account the top and bottom borders
  // of each columns. 

  // If we are the first row then get the largest top border/padding in 
  // our columns. If that's larger than the rows top border/padding use it.

  // If we are the last row then get the largest bottom border/padding in 
  // our columns. If that's larger than the rows bottom border/padding use it.
  PRInt32 firstIndex = 0;
  PRInt32 lastIndex = 0;
  nsGridRow* firstRow = nsnull;
  nsGridRow* lastRow = nsnull;
  GetFirstAndLastRow(aState, firstIndex, lastIndex, firstRow, lastRow, aIsHorizontal);

  if (aIndex == firstIndex || aIndex == lastIndex) {
    nscoord maxTop = 0;
    nscoord maxBottom = 0;

    // run through the columns. Look at each column
    // pick the largest top border or bottom border
    PRInt32 count = GetColumnCount(aIsHorizontal); 

    for (PRInt32 i=0; i < count; i++)
    {  
      nsMargin totalChildBorderPadding(0,0,0,0);

      nsGridRow* column = GetColumnAt(i,aIsHorizontal);
      nsIBox* box = column->GetBox();

      if (box) 
      {
        // ignore collapsed children
        if (!box->IsCollapsed(aState))
        {
           // include the margin of the columns. To the row
           // at this point border/padding and margins all added
           // up to more needed space.
           margin = GetBoxTotalMargin(box, !aIsHorizontal);
           box->GetInset(inset);
           // get real border and padding. GetBorderAndPadding
           // is redefined on nsGridRowLeafFrame. If we called it here
           // we would be in finite recurson.
           box->GetBorder(border);
           box->GetPadding(padding);
           totalChildBorderPadding += inset; 
           totalChildBorderPadding += border;
           totalChildBorderPadding += padding;
           totalChildBorderPadding += margin;
        }

        nscoord top;
        nscoord bottom;

        // pick the largest top margin
        if (aIndex == firstIndex) {
          if (aIsHorizontal) {
            top = totalChildBorderPadding.top;
          } else {
            top = totalChildBorderPadding.left;
          }
          if (top > maxTop)
            maxTop = top;
        } 

        // pick the largest bottom margin
        if (aIndex == lastIndex) {
          if (aIsHorizontal) {
            bottom = totalChildBorderPadding.bottom;
          } else {
            bottom = totalChildBorderPadding.right;
          }
          if (bottom > maxBottom)
             maxBottom = bottom;
        }

      }
    
      // If the biggest top border/padding the columns is larger than this rows top border/padding
      // the use it.
      if (aIndex == firstIndex) {
        if (maxTop > (row->mTop + row->mTopMargin))
          row->mTop = maxTop - row->mTopMargin;
      }

      // If the biggest bottom border/padding the columns is larger than this rows bottom border/padding
      // the use it.
      if (aIndex == lastIndex) {
        if (maxBottom > (row->mBottom + row->mBottomMargin))
          row->mBottom = maxBottom - row->mBottomMargin;
      }
    }
  }
  
  aTop    = row->mTop;
  aBottom = row->mBottom;
}

/**
 * These methods return the preferred, min, max coord for a given row index if
 * aIsHorizontal is PR_TRUE. If you pass PR_FALSE you will get the inverse.
 * As if you called GetPrefColumnHeight(aState, index, aPref).
 */
nscoord
nsGrid::GetPrefRowHeight(nsBoxLayoutState& aState, PRInt32 aIndex, PRBool aIsHorizontal)
{
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsHorizontal);

  if (row->IsCollapsed(aState))
    return 0;

  if (row->IsPrefSet()) 
    return row->mPref;

  nsIBox* box = row->mBox;

  // set in CSS?
  if (box) 
  {
    nsSize cssSize(-1, -1);
    nsIBox::AddCSSPrefSize(aState, box, cssSize);

    row->mPref = GET_HEIGHT(cssSize, aIsHorizontal);

    // yep do nothing.
    if (row->mPref != -1)
      return row->mPref;
  }

  // get the offsets so they are cached.
  nscoord top;
  nscoord bottom;
  GetRowOffsets(aState, aIndex, top, bottom, aIsHorizontal);

  // is the row bogus? If so then just ask it for its size
  // it should not be affected by cells in the grid. 
  if (row->mIsBogus)
  {
     nsSize size(0,0);
     if (box) 
     {
       size = box->GetPrefSize(aState);
       nsBox::AddMargin(box, size);
       nsStackLayout::AddOffset(aState, box, size);
     }

     row->mPref = GET_HEIGHT(size, aIsHorizontal);
     return row->mPref;
  }

  nsSize size(0,0);

  nsGridCell* child;

  PRInt32 count = GetColumnCount(aIsHorizontal); 

  for (PRInt32 i=0; i < count; i++)
  {  
    if (aIsHorizontal)
     child = GetCellAt(i,aIndex);
    else
     child = GetCellAt(aIndex,i);

    // ignore collapsed children
    if (!child->IsCollapsed(aState))
    {
      nsSize childSize = child->GetPrefSize(aState);

      nsSprocketLayout::AddLargestSize(size, childSize, aIsHorizontal);
    }
  }

  row->mPref = GET_HEIGHT(size, aIsHorizontal) + top + bottom;

  return row->mPref;
}

nscoord
nsGrid::GetMinRowHeight(nsBoxLayoutState& aState, PRInt32 aIndex, PRBool aIsHorizontal)
{
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsHorizontal);

  if (row->IsCollapsed(aState))
    return 0;

  if (row->IsMinSet()) 
    return row->mMin;

  nsIBox* box = row->mBox;

  // set in CSS?
  if (box) {
    nsSize cssSize(-1, -1);
    nsIBox::AddCSSMinSize(aState, box, cssSize);

    row->mMin = GET_HEIGHT(cssSize, aIsHorizontal);

    // yep do nothing.
    if (row->mMin != -1)
      return row->mMin;
  }

  // get the offsets so they are cached.
  nscoord top;
  nscoord bottom;
  GetRowOffsets(aState, aIndex, top, bottom, aIsHorizontal);

  // is the row bogus? If so then just ask it for its size
  // it should not be affected by cells in the grid. 
  if (row->mIsBogus)
  {
     nsSize size(0,0);
     if (box) {
       size = box->GetPrefSize(aState);
       nsBox::AddMargin(box, size);
       nsStackLayout::AddOffset(aState, box, size);
     }

     row->mMin = GET_HEIGHT(size, aIsHorizontal) + top + bottom;
     return row->mMin;
  }

  nsSize size(0,0);

  nsGridCell* child;

  PRInt32 count = GetColumnCount(aIsHorizontal); 

  for (PRInt32 i=0; i < count; i++)
  {  
    if (aIsHorizontal)
     child = GetCellAt(i,aIndex);
    else
     child = GetCellAt(aIndex,i);

    // ignore collapsed children
    if (!child->IsCollapsed(aState))
    {
      nsSize childSize = child->GetMinSize(aState);

      nsSprocketLayout::AddLargestSize(size, childSize, aIsHorizontal);
    }
  }

  row->mMin = GET_HEIGHT(size, aIsHorizontal);

  return row->mMin;
}

nscoord
nsGrid::GetMaxRowHeight(nsBoxLayoutState& aState, PRInt32 aIndex, PRBool aIsHorizontal)
{
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsHorizontal);

  if (row->IsCollapsed(aState))
    return 0;

  if (row->IsMaxSet()) 
    return row->mMax;

  nsIBox* box = row->mBox;

  // set in CSS?
  if (box) {
    nsSize cssSize;
    cssSize.width = -1;
    cssSize.height = -1;
    nsIBox::AddCSSMaxSize(aState, box, cssSize);
    
    row->mMax = GET_HEIGHT(cssSize, aIsHorizontal);

    // yep do nothing.
    if (row->mMax != -1)
      return row->mMax;
  }

  // get the offsets so they are cached.
  nscoord top;
  nscoord bottom;
  GetRowOffsets(aState, aIndex, top, bottom, aIsHorizontal);

  // is the row bogus? If so then just ask it for its size
  // it should not be affected by cells in the grid. 
  if (row->mIsBogus)
  {
     nsSize size(NS_INTRINSICSIZE,NS_INTRINSICSIZE);
     if (box) {
       size = box->GetPrefSize(aState);
       nsBox::AddMargin(box, size);
       nsStackLayout::AddOffset(aState, box, size);
     }

     row->mMax = GET_HEIGHT(size, aIsHorizontal);
     return row->mMax;
  }

  nsSize size(NS_INTRINSICSIZE,NS_INTRINSICSIZE);

  nsGridCell* child;

  PRInt32 count = GetColumnCount(aIsHorizontal); 

  for (PRInt32 i=0; i < count; i++)
  {  
    if (aIsHorizontal)
     child = GetCellAt(i,aIndex);
    else
     child = GetCellAt(aIndex,i);

    // ignore collapsed children
    if (!child->IsCollapsed(aState))
    {
      nsSize childSize = child->GetMaxSize(aState);
      nsSize min = child->GetMinSize(aState);
      nsBox::BoundsCheckMinMax(min, childSize);

      nsSprocketLayout::AddLargestSize(size, childSize, aIsHorizontal);
    }
  }

  row->mMax = GET_HEIGHT(size, aIsHorizontal) + top + bottom;

  return row->mMax;
}

PRBool
nsGrid::IsGrid(nsIBox* aBox)
{
  if (!aBox)
    return PR_FALSE;

  nsCOMPtr<nsIGridPart> part;
  GetPartFromBox(aBox, getter_AddRefs(part));
  if (!part)
    return PR_FALSE;

  nsGridLayout2* grid = part->CastToGridLayout();

  if (grid)
    return PR_TRUE;

  return PR_FALSE;
}

/**
 * This get the flexibilty of the row at aIndex. It's not trivial. There are a few
 * things we need to look at. Specifically we need to see if any <rows> or <columns>
 * tags are around us. Their flexibilty will affect ours.
 */
nscoord
nsGrid::GetRowFlex(nsBoxLayoutState& aState, PRInt32 aIndex, PRBool aIsHorizontal)
{
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsHorizontal);

  if (row->IsFlexSet()) 
    return row->mFlex;

  nsIBox* box = row->mBox;
  row->mFlex = 0;

  if (box) {

    // We need our flex but a inflexible row could be around us. If so
    // neither are we. However if its the row tag just inside the grid it won't 
    // affect us. We need to do this for this case:
    // <grid> 
    //   <rows> 
    //     <rows> // this is not flexible. So our children should not be flexible
    //        <row flex="1"/>
    //        <row flex="1"/>
    //     </rows>
    //        <row/>
    //   </rows>
    // </grid>
    //
    // or..
    //
    // <grid> 
    //  <rows>
    //   <rows> // this is not flexible. So our children should not be flexible
    //     <rows flex="1"> 
    //        <row flex="1"/>
    //        <row flex="1"/>
    //     </rows>
    //        <row/>
    //   </rows>
    //  </row>
    // </grid>


    // So here is how it looks
    //
    // <grid>     
    //   <rows>   // parentsParent
    //     <rows> // parent
    //        <row flex="1"/> 
    //        <row flex="1"/>
    //     </rows>
    //        <row/>
    //   </rows>
    // </grid>

    // so the answer is simple: 1) Walk our parent chain. 2) If we find
    // someone who is not flexible and they aren't the rows immediately in
    // the grid. 3) Then we are not flexible

    nsIBox* parent=nsnull;
    nsIBox* parentsParent=nsnull;

    box = GetScrollBox(box);
    box->GetParentBox(&parent);
    
    while(parent)
    {
      parent = GetScrollBox(parent);
      parent->GetParentBox(&parentsParent);

      // if our parents parent is not a grid
      // the get its flex. If its 0 then we are
      // not flexible.
      if (parentsParent) {
        if (!IsGrid(parentsParent)) {
          nscoord flex = parent->GetFlex(aState);
          nsIBox::AddCSSFlex(aState, parent, flex);
          if (flex == 0) {
            row->mFlex = 0;
            return row->mFlex;
          }
        } else 
          break;
      }

      parent = parentsParent;
    }
    
    // get the row flex.
    row->mFlex = box->GetFlex(aState);
    nsIBox::AddCSSFlex(aState, box, row->mFlex);
  }

  return row->mFlex;
}

void
nsGrid::SetLargestSize(nsSize& aSize, nscoord aHeight, PRBool aIsHorizontal)
{
  if (aIsHorizontal) {
    if (aSize.height < aHeight)
      aSize.height = aHeight;
  } else {
    if (aSize.width < aHeight)
      aSize.width = aHeight;
  }
}

void
nsGrid::SetSmallestSize(nsSize& aSize, nscoord aHeight, PRBool aIsHorizontal)
{
  if (aIsHorizontal) {
    if (aSize.height > aHeight)
      aSize.height = aHeight;
  } else {
    if (aSize.width < aHeight)
      aSize.width = aHeight;
  }
}

PRInt32 
nsGrid::GetRowCount(PRInt32 aIsHorizontal)
{
  RebuildIfNeeded();

  if (aIsHorizontal)
    return mRowCount;
  else
    return mColumnCount;
}

PRInt32 
nsGrid::GetColumnCount(PRInt32 aIsHorizontal)
{
  return GetRowCount(!aIsHorizontal);
}

/*
 * A cell in the given row or columns at the given index has had a child added or removed
 */
void 
nsGrid::CellAddedOrRemoved(nsBoxLayoutState& aState, PRInt32 aIndex, PRBool aIsHorizontal)
{
  // TBD see if the cell will fit in our current row. If it will
  // just add it in. 
  // but for now rebuild everything.
  if (mMarkingDirty)
    return;

  NeedsRebuild(aState);
}

/**
 * A row or columns at the given index had been added or removed
 */
void 
nsGrid::RowAddedOrRemoved(nsBoxLayoutState& aState, PRInt32 aIndex, PRBool aIsHorizontal)
{
  // TBD see if we have extra room in the table and just add the new row in
  // for now rebuild the world
  if (mMarkingDirty)
    return;

  NeedsRebuild(aState);
}

/*
 * Scrollframes are tranparent. If this is given a scrollframe is will return the
 * frame inside. If there is no scrollframe it does nothing.
 */
nsIBox*
nsGrid::GetScrolledBox(nsIBox* aChild)
{
  // first see if it is a scrollframe. If so walk down into it and get the scrolled child
      nsCOMPtr<nsIScrollableFrame> scrollFrame = do_QueryInterface(aChild);
      if (scrollFrame) {
         nsIFrame* scrolledFrame = scrollFrame->GetScrolledFrame();
         NS_ASSERTION(scrolledFrame,"Error no scroll frame!!");
         return scrolledFrame;
      }

      return aChild;
}

/*
 * Scrollframes are tranparent. If this is given a child in a scrollframe is will return the
 * scrollframe ourside it. If there is no scrollframe it does nothing.
 */
nsIBox*
nsGrid::GetScrollBox(nsIBox* aChild)
{
  if (!aChild)
    return nsnull;

  // get parent
  nsIBox* parent = nsnull;
  nsCOMPtr<nsIBoxLayout> layout;
  nsCOMPtr<nsIGridPart> parentGridRow;

  aChild->GetParentBox(&parent);

  // walk up until we find a scrollframe or a part
  // if it's a scrollframe return it.
  // if it's a parent then the child passed does not
  // have a scroll frame immediately wrapped around it.
  while (parent) {
    nsCOMPtr<nsIScrollableFrame> scrollFrame = do_QueryInterface(parent);
    // scrollframe? Yep return it.
    if (scrollFrame)
      return parent;

    parent->GetLayoutManager(getter_AddRefs(layout));
    parentGridRow = do_QueryInterface(layout);
    // if a part then just return the child
    if (parentGridRow) 
      break;

    parent->GetParentBox(&parent);
  }

  return aChild;
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
  
}
#endif
