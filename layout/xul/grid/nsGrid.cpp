/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsGrid.h"
#include "nsGridRowGroupLayout.h"
#include "nsIScrollableFrame.h"
#include "nsSprocketLayout.h"
#include "nsGridLayout2.h"
#include "nsGridRow.h"
#include "nsGridCell.h"
#include "mozilla/ReflowInput.h"

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
         <label value="hello"/>
         <label value="there"/>
      </row>
   </rows>
</grid>

example 3:

<grid>

<rows>
      <row>
         <label value="hello"/>
         <label value="there"/>
      </row>
   </rows>

   <columns>
      <column>
         <label value="Hey I'm in the column and I'm on top!"/>
      </column>
      <column/>
   </columns>

</grid>

Usually the columns are first and the rows are second, so the rows will be drawn
on top of the columns. You can reverse this by defining the rows first. Other
tags are then placed in the <row> or <column> tags causing the grid to
accommodate everyone. It does this by creating 3 things: A cellmap, a row list,
and a column list. The cellmap is a 2 dimensional array of nsGridCells. Each
cell contains 2 boxes.  One cell from the column list and one from the row list.
When a cell is asked for its size it returns that smallest size it can be to
accommodate the 2 cells. Row lists and Column lists use the same data structure:
nsGridRow. Essentially a row and column are the same except a row goes alone the
x axis and a column the y. To make things easier and save code everything is
written in terms of the x dimension. A flag is passed in called "isHorizontal"
that can flip the calculations to the y axis.

Usually the number of cells in a row match the number of columns, but not
always. It is possible to define 5 columns for a grid but have 10 cells in one
of the rows. In this case 5 extra columns will be added to the column list to
handle the situation. These are called extraColumns/Rows.
*/

using namespace mozilla;

nsGrid::nsGrid()
    : mBox(nullptr),
      mRowsBox(nullptr),
      mColumnsBox(nullptr),
      mNeedsRebuild(true),
      mRowCount(0),
      mColumnCount(0),
      mExtraRowCount(0),
      mExtraColumnCount(0),
      mMarkingDirty(false) {
  MOZ_COUNT_CTOR(nsGrid);
}

nsGrid::~nsGrid() {
  FreeMap();
  MOZ_COUNT_DTOR(nsGrid);
}

/*
 * This is called whenever something major happens in the grid. And example
 * might be when many cells or row are added. It sets a flag signaling that
 * all the grids caches information should be recalculated.
 */
void nsGrid::NeedsRebuild(nsBoxLayoutState& aState) {
  if (mNeedsRebuild) return;

  // iterate through columns and rows and dirty them
  mNeedsRebuild = true;

  // find the new row and column box. They could have
  // been changed.
  mRowsBox = nullptr;
  mColumnsBox = nullptr;
  FindRowsAndColumns(&mRowsBox, &mColumnsBox);

  // tell all the rows and columns they are dirty
  DirtyRows(mRowsBox, aState);
  DirtyRows(mColumnsBox, aState);
}

/**
 * If we are marked for rebuild. Then build everything
 */
void nsGrid::RebuildIfNeeded() {
  if (!mNeedsRebuild) return;

  mNeedsRebuild = false;

  // find the row and columns frames
  FindRowsAndColumns(&mRowsBox, &mColumnsBox);

  // count the rows and columns
  int32_t computedRowCount = 0;
  int32_t computedColumnCount = 0;
  int32_t rowCount = 0;
  int32_t columnCount = 0;

  CountRowsColumns(mRowsBox, rowCount, computedColumnCount);
  CountRowsColumns(mColumnsBox, columnCount, computedRowCount);

  // computedRowCount are the actual number of rows as determined by the
  // columns children.
  // computedColumnCount are the number of columns as determined by the number
  // of rows children.
  // We can use this information to see how many extra columns or rows we need.
  // This can happen if there are are more children in a row that number of
  // columns defined. Example:
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
  mRows = BuildRows(mRowsBox, rowCount, true);
  mColumns = BuildRows(mColumnsBox, columnCount, false);

  // build and populate the cell map
  mCellMap = BuildCellMap(rowCount, columnCount);

  mRowCount = rowCount;
  mColumnCount = columnCount;

  // populate the cell map from column and row children
  PopulateCellMap(mRows.get(), mColumns.get(), mRowCount, mColumnCount, true);
  PopulateCellMap(mColumns.get(), mRows.get(), mColumnCount, mRowCount, false);
}

void nsGrid::FreeMap() {
  mRows = nullptr;
  mColumns = nullptr;
  mCellMap = nullptr;
  mColumnCount = 0;
  mRowCount = 0;
  mExtraColumnCount = 0;
  mExtraRowCount = 0;
  mRowsBox = nullptr;
  mColumnsBox = nullptr;
}

/**
 * finds the first <rows> and <columns> tags in the <grid> tag
 */
void nsGrid::FindRowsAndColumns(nsIFrame** aRows, nsIFrame** aColumns) {
  *aRows = nullptr;
  *aColumns = nullptr;

  // find the boxes that contain our rows and columns
  nsIFrame* child = nullptr;
  // if we have <grid></grid> then mBox will be null (bug 125689)
  if (mBox) {
    child = nsIFrame::GetChildXULBox(mBox);
  }

  while (child) {
    nsIFrame* oldBox = child;
    nsIScrollableFrame* scrollFrame = do_QueryFrame(child);
    if (scrollFrame) {
      nsIFrame* scrolledFrame = scrollFrame->GetScrolledFrame();
      NS_ASSERTION(scrolledFrame, "Error no scroll frame!!");
      child = do_QueryFrame(scrolledFrame);
    }

    nsCOMPtr<nsIGridPart> monument = GetPartFromBox(child);
    if (monument) {
      nsGridRowGroupLayout* rowGroup = monument->CastToRowGroupLayout();
      if (rowGroup) {
        bool isHorizontal = !nsSprocketLayout::IsXULHorizontal(child);
        if (isHorizontal)
          *aRows = child;
        else
          *aColumns = child;

        if (*aRows && *aColumns) return;
      }
    }

    if (scrollFrame) {
      child = oldBox;
    }

    child = nsIFrame::GetNextXULBox(child);
  }
}

/**
 * Count the number of rows and columns in the given box. aRowCount well become
 * the actual number rows defined in the xul. aComputedColumnCount will become
 * the number of columns by counting the number of cells in each row.
 */
void nsGrid::CountRowsColumns(nsIFrame* aRowBox, int32_t& aRowCount,
                              int32_t& aComputedColumnCount) {
  aRowCount = 0;
  aComputedColumnCount = 0;
  // get the rowboxes layout manager. Then ask it to do the work for us
  if (aRowBox) {
    nsCOMPtr<nsIGridPart> monument = GetPartFromBox(aRowBox);
    if (monument)
      monument->CountRowsColumns(aRowBox, aRowCount, aComputedColumnCount);
  }
}

/**
 * Given the number of rows create nsGridRow objects for them and full them out.
 */
UniquePtr<nsGridRow[]> nsGrid::BuildRows(nsIFrame* aBox, int32_t aRowCount,
                                         bool aIsHorizontal) {
  // if no rows then return null
  if (aRowCount == 0) {
    return nullptr;
  }

  // create the array
  UniquePtr<nsGridRow[]> row;

  // only create new rows if we have to. Reuse old rows.
  if (aIsHorizontal) {
    if (aRowCount > mRowCount) {
      row = MakeUnique<nsGridRow[]>(aRowCount);
    } else {
      for (int32_t i = 0; i < mRowCount; i++) mRows[i].Init(nullptr, false);

      row = std::move(mRows);
    }
  } else {
    if (aRowCount > mColumnCount) {
      row = MakeUnique<nsGridRow[]>(aRowCount);
    } else {
      for (int32_t i = 0; i < mColumnCount; i++)
        mColumns[i].Init(nullptr, false);

      row = std::move(mColumns);
    }
  }

  // populate it if we can. If not it will contain only dynamic columns
  if (aBox) {
    nsCOMPtr<nsIGridPart> monument = GetPartFromBox(aBox);
    if (monument) {
      monument->BuildRows(aBox, row.get());
    }
  }

  return row;
}

/**
 * Given the number of rows and columns. Build a cellmap
 */
UniquePtr<nsGridCell[]> nsGrid::BuildCellMap(int32_t aRows, int32_t aColumns) {
  int32_t size = aRows * aColumns;
  int32_t oldsize = mRowCount * mColumnCount;
  if (size == 0) {
    return nullptr;
  }

  if (size > oldsize) {
    return MakeUnique<nsGridCell[]>(size);
  }

  // clear out cellmap
  for (int32_t i = 0; i < oldsize; i++) {
    mCellMap[i].SetBoxInRow(nullptr);
    mCellMap[i].SetBoxInColumn(nullptr);
  }
  return std::move(mCellMap);
}

/**
 * Run through all the cells in the rows and columns and populate then with 2
 * cells. One from the row and one from the column
 */
void nsGrid::PopulateCellMap(nsGridRow* aRows, nsGridRow* aColumns,
                             int32_t aRowCount, int32_t aColumnCount,
                             bool aIsHorizontal) {
  if (!aRows) return;

  // look through the columns
  int32_t j = 0;

  for (int32_t i = 0; i < aRowCount; i++) {
    nsIFrame* child = nullptr;
    nsGridRow* row = &aRows[i];

    // skip bogus rows. They have no cells
    if (row->mIsBogus) continue;

    child = row->mBox;
    if (child) {
      child = nsIFrame::GetChildXULBox(child);

      j = 0;

      while (child && j < aColumnCount) {
        // skip bogus column. They have no cells
        nsGridRow* column = &aColumns[j];
        if (column->mIsBogus) {
          j++;
          continue;
        }

        if (aIsHorizontal)
          GetCellAt(j, i)->SetBoxInRow(child);
        else
          GetCellAt(i, j)->SetBoxInColumn(child);

        child = nsIFrame::GetNextXULBox(child);

        j++;
      }
    }
  }
}

/**
 * Run through the rows in the given box and mark them dirty so they
 * will get recalculated and get a layout.
 */
void nsGrid::DirtyRows(nsIFrame* aRowBox, nsBoxLayoutState& aState) {
  // make sure we prevent others from dirtying things.
  mMarkingDirty = true;

  // if the box is a grid part have it recursively hand it.
  if (aRowBox) {
    nsCOMPtr<nsIGridPart> part = GetPartFromBox(aRowBox);
    if (part) part->DirtyRows(aRowBox, aState);
  }

  mMarkingDirty = false;
}

nsGridRow* nsGrid::GetColumnAt(int32_t aIndex, bool aIsHorizontal) {
  return GetRowAt(aIndex, !aIsHorizontal);
}

nsGridRow* nsGrid::GetRowAt(int32_t aIndex, bool aIsHorizontal) {
  RebuildIfNeeded();

  if (aIsHorizontal) {
    NS_ASSERTION(aIndex < mRowCount && aIndex >= 0, "Index out of range");
    return &mRows[aIndex];
  } else {
    NS_ASSERTION(aIndex < mColumnCount && aIndex >= 0, "Index out of range");
    return &mColumns[aIndex];
  }
}

nsGridCell* nsGrid::GetCellAt(int32_t aX, int32_t aY) {
  RebuildIfNeeded();

  NS_ASSERTION(aY < mRowCount && aY >= 0, "Index out of range");
  NS_ASSERTION(aX < mColumnCount && aX >= 0, "Index out of range");
  return &mCellMap[aY * mColumnCount + aX];
}

int32_t nsGrid::GetExtraColumnCount(bool aIsHorizontal) {
  return GetExtraRowCount(!aIsHorizontal);
}

int32_t nsGrid::GetExtraRowCount(bool aIsHorizontal) {
  RebuildIfNeeded();

  if (aIsHorizontal)
    return mExtraRowCount;
  else
    return mExtraColumnCount;
}

/**
 * These methods return the preferred, min, max sizes for a given row index.
 * aIsHorizontal if aIsHorizontal is true. If you pass false you will get the
 * inverse. As if you called GetPrefColumnSize(aState, index, aPref)
 */
nsSize nsGrid::GetPrefRowSize(nsBoxLayoutState& aState, int32_t aRowIndex,
                              bool aIsHorizontal) {
  nsSize size(0, 0);
  if (!(aRowIndex >= 0 && aRowIndex < GetRowCount(aIsHorizontal))) return size;

  nscoord height = GetPrefRowHeight(aState, aRowIndex, aIsHorizontal);
  SetLargestSize(size, height, aIsHorizontal);

  return size;
}

nsSize nsGrid::GetMinRowSize(nsBoxLayoutState& aState, int32_t aRowIndex,
                             bool aIsHorizontal) {
  nsSize size(0, 0);
  if (!(aRowIndex >= 0 && aRowIndex < GetRowCount(aIsHorizontal))) return size;

  nscoord height = GetMinRowHeight(aState, aRowIndex, aIsHorizontal);
  SetLargestSize(size, height, aIsHorizontal);

  return size;
}

nsSize nsGrid::GetMaxRowSize(nsBoxLayoutState& aState, int32_t aRowIndex,
                             bool aIsHorizontal) {
  nsSize size(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  if (!(aRowIndex >= 0 && aRowIndex < GetRowCount(aIsHorizontal))) return size;

  nscoord height = GetMaxRowHeight(aState, aRowIndex, aIsHorizontal);
  SetSmallestSize(size, height, aIsHorizontal);

  return size;
}

// static
nsIGridPart* nsGrid::GetPartFromBox(nsIFrame* aBox) {
  if (!aBox) return nullptr;

  nsBoxLayout* layout = aBox->GetXULLayoutManager();
  return layout ? layout->AsGridPart() : nullptr;
}

nsMargin nsGrid::GetBoxTotalMargin(nsIFrame* aBox, bool aIsHorizontal) {
  nsMargin margin(0, 0, 0, 0);
  // walk the boxes parent chain getting the border/padding/margin of our parent
  // rows

  // first get the layour manager
  nsIGridPart* part = GetPartFromBox(aBox);
  if (part) margin = part->GetTotalMargin(aBox, aIsHorizontal);

  return margin;
}

/**
 * The first and last rows can be affected by <rows> tags with borders or margin
 * gets first and last rows and their indexes.
 * If it fails because there are no rows then:
 * FirstRow is nullptr
 * LastRow is nullptr
 * aFirstIndex = -1
 * aLastIndex = -1
 */
void nsGrid::GetFirstAndLastRow(int32_t& aFirstIndex, int32_t& aLastIndex,
                                nsGridRow*& aFirstRow, nsGridRow*& aLastRow,
                                bool aIsHorizontal) {
  aFirstRow = nullptr;
  aLastRow = nullptr;
  aFirstIndex = -1;
  aLastIndex = -1;

  int32_t count = GetRowCount(aIsHorizontal);

  if (count == 0) return;

  // We could have collapsed columns either before or after our index.
  // they should not count. So if we are the 5th row and the first 4 are
  // collaped we become the first row. Or if we are the 9th row and
  // 10 up to the last row are collapsed we then become the last.

  // see if we are first
  int32_t i;
  for (i = 0; i < count; i++) {
    nsGridRow* row = GetRowAt(i, aIsHorizontal);
    if (!row->IsXULCollapsed()) {
      aFirstIndex = i;
      aFirstRow = row;
      break;
    }
  }

  // see if we are last
  for (i = count - 1; i >= 0; i--) {
    nsGridRow* row = GetRowAt(i, aIsHorizontal);
    if (!row->IsXULCollapsed()) {
      aLastIndex = i;
      aLastRow = row;
      break;
    }
  }
}

/**
 * A row can have a top and bottom offset. Usually this is just the top and
 * bottom border/padding. However if the row is the first or last it could be
 * affected by the fact a column or columns could have a top or bottom margin.
 */
void nsGrid::GetRowOffsets(int32_t aIndex, nscoord& aTop, nscoord& aBottom,
                           bool aIsHorizontal) {
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsHorizontal);

  if (row->IsOffsetSet()) {
    aTop = row->mTop;
    aBottom = row->mBottom;
    return;
  }

  // first get the rows top and bottom border and padding
  nsIFrame* box = row->GetBox();

  // add up all the padding
  nsMargin margin(0, 0, 0, 0);
  nsMargin border(0, 0, 0, 0);
  nsMargin padding(0, 0, 0, 0);
  nsMargin totalBorderPadding(0, 0, 0, 0);
  nsMargin totalMargin(0, 0, 0, 0);

  // if there is a box and it's not bogus take its
  // borders padding into account
  if (box && !row->mIsBogus) {
    if (!box->IsXULCollapsed()) {
      // get real border and padding. GetXULBorderAndPadding
      // is redefined on nsGridRowLeafFrame. If we called it here
      // we would be in finite recurson.
      box->GetXULBorder(border);
      box->GetXULPadding(padding);

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

  // if we are the first or last row take into account the top and bottom
  // borders of each columns.

  // If we are the first row then get the largest top border/padding in
  // our columns. If that's larger than the rows top border/padding use it.

  // If we are the last row then get the largest bottom border/padding in
  // our columns. If that's larger than the rows bottom border/padding use it.
  int32_t firstIndex = 0;
  int32_t lastIndex = 0;
  nsGridRow* firstRow = nullptr;
  nsGridRow* lastRow = nullptr;
  GetFirstAndLastRow(firstIndex, lastIndex, firstRow, lastRow, aIsHorizontal);

  if (aIndex == firstIndex || aIndex == lastIndex) {
    nscoord maxTop = 0;
    nscoord maxBottom = 0;

    // run through the columns. Look at each column
    // pick the largest top border or bottom border
    int32_t count = GetColumnCount(aIsHorizontal);

    for (int32_t i = 0; i < count; i++) {
      nsMargin totalChildBorderPadding(0, 0, 0, 0);

      nsGridRow* column = GetColumnAt(i, aIsHorizontal);
      nsIFrame* box = column->GetBox();

      if (box) {
        // ignore collapsed children
        if (!box->IsXULCollapsed()) {
          // include the margin of the columns. To the row
          // at this point border/padding and margins all added
          // up to more needed space.
          margin = GetBoxTotalMargin(box, !aIsHorizontal);
          // get real border and padding. GetXULBorderAndPadding
          // is redefined on nsGridRowLeafFrame. If we called it here
          // we would be in finite recurson.
          box->GetXULBorder(border);
          box->GetXULPadding(padding);
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
          if (top > maxTop) maxTop = top;
        }

        // pick the largest bottom margin
        if (aIndex == lastIndex) {
          if (aIsHorizontal) {
            bottom = totalChildBorderPadding.bottom;
          } else {
            bottom = totalChildBorderPadding.right;
          }
          if (bottom > maxBottom) maxBottom = bottom;
        }
      }

      // If the biggest top border/padding the columns is larger than this rows
      // top border/padding the use it.
      if (aIndex == firstIndex) {
        if (maxTop > (row->mTop + row->mTopMargin))
          row->mTop = maxTop - row->mTopMargin;
      }

      // If the biggest bottom border/padding the columns is larger than this
      // rows bottom border/padding the use it.
      if (aIndex == lastIndex) {
        if (maxBottom > (row->mBottom + row->mBottomMargin))
          row->mBottom = maxBottom - row->mBottomMargin;
      }
    }
  }

  aTop = row->mTop;
  aBottom = row->mBottom;
}

/**
 * These methods return the preferred, min, max coord for a given row index if
 * aIsHorizontal is true. If you pass false you will get the inverse.
 * As if you called GetPrefColumnHeight(aState, index, aPref).
 */
nscoord nsGrid::GetPrefRowHeight(nsBoxLayoutState& aState, int32_t aIndex,
                                 bool aIsHorizontal) {
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsHorizontal);

  if (row->IsXULCollapsed()) return 0;

  if (row->IsPrefSet()) return row->mPref;

  nsIFrame* box = row->mBox;

  // set in CSS?
  if (box) {
    bool widthSet, heightSet;
    nsSize cssSize(-1, -1);
    nsIFrame::AddXULPrefSize(box, cssSize, widthSet, heightSet);

    row->mPref = GET_HEIGHT(cssSize, aIsHorizontal);

    // yep do nothing.
    if (row->mPref != -1) return row->mPref;
  }

  // get the offsets so they are cached.
  nscoord top;
  nscoord bottom;
  GetRowOffsets(aIndex, top, bottom, aIsHorizontal);

  // is the row bogus? If so then just ask it for its size
  // it should not be affected by cells in the grid.
  if (row->mIsBogus) {
    nsSize size(0, 0);
    if (box) {
      size = box->GetXULPrefSize(aState);
      nsIFrame::AddXULMargin(box, size);
      nsGridLayout2::AddOffset(box, size);
    }

    row->mPref = GET_HEIGHT(size, aIsHorizontal);
    return row->mPref;
  }

  nsSize size(0, 0);

  nsGridCell* child;

  int32_t count = GetColumnCount(aIsHorizontal);

  for (int32_t i = 0; i < count; i++) {
    if (aIsHorizontal)
      child = GetCellAt(i, aIndex);
    else
      child = GetCellAt(aIndex, i);

    // ignore collapsed children
    if (!child->IsXULCollapsed()) {
      nsSize childSize = child->GetXULPrefSize(aState);

      nsSprocketLayout::AddLargestSize(size, childSize, aIsHorizontal);
    }
  }

  row->mPref = GET_HEIGHT(size, aIsHorizontal) + top + bottom;

  return row->mPref;
}

nscoord nsGrid::GetMinRowHeight(nsBoxLayoutState& aState, int32_t aIndex,
                                bool aIsHorizontal) {
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsHorizontal);

  if (row->IsXULCollapsed()) return 0;

  if (row->IsMinSet()) return row->mMin;

  nsIFrame* box = row->mBox;

  // set in CSS?
  if (box) {
    bool widthSet, heightSet;
    nsSize cssSize(-1, -1);
    nsIFrame::AddXULMinSize(box, cssSize, widthSet, heightSet);

    row->mMin = GET_HEIGHT(cssSize, aIsHorizontal);

    // yep do nothing.
    if (row->mMin != -1) return row->mMin;
  }

  // get the offsets so they are cached.
  nscoord top;
  nscoord bottom;
  GetRowOffsets(aIndex, top, bottom, aIsHorizontal);

  // is the row bogus? If so then just ask it for its size
  // it should not be affected by cells in the grid.
  if (row->mIsBogus) {
    nsSize size(0, 0);
    if (box) {
      size = box->GetXULPrefSize(aState);
      nsIFrame::AddXULMargin(box, size);
      nsGridLayout2::AddOffset(box, size);
    }

    row->mMin = GET_HEIGHT(size, aIsHorizontal) + top + bottom;
    return row->mMin;
  }

  nsSize size(0, 0);

  nsGridCell* child;

  int32_t count = GetColumnCount(aIsHorizontal);

  for (int32_t i = 0; i < count; i++) {
    if (aIsHorizontal)
      child = GetCellAt(i, aIndex);
    else
      child = GetCellAt(aIndex, i);

    // ignore collapsed children
    if (!child->IsXULCollapsed()) {
      nsSize childSize = child->GetXULMinSize(aState);

      nsSprocketLayout::AddLargestSize(size, childSize, aIsHorizontal);
    }
  }

  row->mMin = GET_HEIGHT(size, aIsHorizontal);

  return row->mMin;
}

nscoord nsGrid::GetMaxRowHeight(nsBoxLayoutState& aState, int32_t aIndex,
                                bool aIsHorizontal) {
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsHorizontal);

  if (row->IsXULCollapsed()) return 0;

  if (row->IsMaxSet()) return row->mMax;

  nsIFrame* box = row->mBox;

  // set in CSS?
  if (box) {
    bool widthSet, heightSet;
    nsSize cssSize(-1, -1);
    nsIFrame::AddXULMaxSize(box, cssSize, widthSet, heightSet);

    row->mMax = GET_HEIGHT(cssSize, aIsHorizontal);

    // yep do nothing.
    if (row->mMax != -1) return row->mMax;
  }

  // get the offsets so they are cached.
  nscoord top;
  nscoord bottom;
  GetRowOffsets(aIndex, top, bottom, aIsHorizontal);

  // is the row bogus? If so then just ask it for its size
  // it should not be affected by cells in the grid.
  if (row->mIsBogus) {
    nsSize size(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
    if (box) {
      size = box->GetXULPrefSize(aState);
      nsIFrame::AddXULMargin(box, size);
      nsGridLayout2::AddOffset(box, size);
    }

    row->mMax = GET_HEIGHT(size, aIsHorizontal);
    return row->mMax;
  }

  nsSize size(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  nsGridCell* child;

  int32_t count = GetColumnCount(aIsHorizontal);

  for (int32_t i = 0; i < count; i++) {
    if (aIsHorizontal)
      child = GetCellAt(i, aIndex);
    else
      child = GetCellAt(aIndex, i);

    // ignore collapsed children
    if (!child->IsXULCollapsed()) {
      nsSize min = child->GetXULMinSize(aState);
      nsSize childSize =
          nsIFrame::XULBoundsCheckMinMax(min, child->GetXULMaxSize(aState));
      nsSprocketLayout::AddLargestSize(size, childSize, aIsHorizontal);
    }
  }

  row->mMax = GET_HEIGHT(size, aIsHorizontal) + top + bottom;

  return row->mMax;
}

bool nsGrid::IsGrid(nsIFrame* aBox) {
  nsIGridPart* part = GetPartFromBox(aBox);
  if (!part) return false;

  nsGridLayout2* grid = part->CastToGridLayout();

  if (grid) return true;

  return false;
}

/**
 * This get the flexibilty of the row at aIndex. It's not trivial. There are a
 * few things we need to look at. Specifically we need to see if any <rows> or
 * <columns> tags are around us. Their flexibilty will affect ours.
 */
nscoord nsGrid::GetRowFlex(int32_t aIndex, bool aIsHorizontal) {
  RebuildIfNeeded();

  nsGridRow* row = GetRowAt(aIndex, aIsHorizontal);

  if (row->IsFlexSet()) return row->mFlex;

  nsIFrame* box = row->mBox;
  row->mFlex = 0;

  if (box) {
    // We need our flex but a inflexible row could be around us. If so
    // neither are we. However if its the row tag just inside the grid it won't
    // affect us. We need to do this for this case:
    // <grid>
    //   <rows>
    //     <rows> // this is not flexible.
    //            // So our children should not be flexible
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

    box = GetScrollBox(box);
    nsIFrame* parent = nsIFrame::GetParentXULBox(box);
    nsIFrame* parentsParent = nullptr;

    while (parent) {
      parent = GetScrollBox(parent);
      parentsParent = nsIFrame::GetParentXULBox(parent);

      // if our parents parent is not a grid
      // the get its flex. If its 0 then we are
      // not flexible.
      if (parentsParent) {
        if (!IsGrid(parentsParent)) {
          nscoord flex = parent->GetXULFlex();
          nsIFrame::AddXULFlex(parent, flex);
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
    row->mFlex = box->GetXULFlex();
    nsIFrame::AddXULFlex(box, row->mFlex);
  }

  return row->mFlex;
}

void nsGrid::SetLargestSize(nsSize& aSize, nscoord aHeight,
                            bool aIsHorizontal) {
  if (aIsHorizontal) {
    if (aSize.height < aHeight) aSize.height = aHeight;
  } else {
    if (aSize.width < aHeight) aSize.width = aHeight;
  }
}

void nsGrid::SetSmallestSize(nsSize& aSize, nscoord aHeight,
                             bool aIsHorizontal) {
  if (aIsHorizontal) {
    if (aSize.height > aHeight) aSize.height = aHeight;
  } else {
    if (aSize.width < aHeight) aSize.width = aHeight;
  }
}

int32_t nsGrid::GetRowCount(int32_t aIsHorizontal) {
  RebuildIfNeeded();

  if (aIsHorizontal)
    return mRowCount;
  else
    return mColumnCount;
}

int32_t nsGrid::GetColumnCount(int32_t aIsHorizontal) {
  return GetRowCount(!aIsHorizontal);
}

/*
 * A cell in the given row or columns at the given index has had a child added
 * or removed
 */
void nsGrid::CellAddedOrRemoved(nsBoxLayoutState& aState, int32_t aIndex,
                                bool aIsHorizontal) {
  // TBD see if the cell will fit in our current row. If it will
  // just add it in.
  // but for now rebuild everything.
  if (mMarkingDirty) return;

  NeedsRebuild(aState);
}

/**
 * A row or columns at the given index had been added or removed
 */
void nsGrid::RowAddedOrRemoved(nsBoxLayoutState& aState, int32_t aIndex,
                               bool aIsHorizontal) {
  // TBD see if we have extra room in the table and just add the new row in
  // for now rebuild the world
  if (mMarkingDirty) return;

  NeedsRebuild(aState);
}

/*
 * Scrollframes are tranparent. If this is given a scrollframe is will return
 * the frame inside. If there is no scrollframe it does nothing.
 */
nsIFrame* nsGrid::GetScrolledBox(nsIFrame* aChild) {
  // first see if it is a scrollframe. If so walk down into it and get the
  // scrolled child
  nsIScrollableFrame* scrollFrame = do_QueryFrame(aChild);
  if (scrollFrame) {
    nsIFrame* scrolledFrame = scrollFrame->GetScrolledFrame();
    NS_ASSERTION(scrolledFrame, "Error no scroll frame!!");
    return scrolledFrame;
  }

  return aChild;
}

/*
 * Scrollframes are tranparent. If this is given a child in a scrollframe is
 * will return the scrollframe ourside it. If there is no scrollframe it does
 * nothing.
 */
nsIFrame* nsGrid::GetScrollBox(nsIFrame* aChild) {
  if (!aChild) return nullptr;

  // get parent
  nsIFrame* parent = nsIFrame::GetParentXULBox(aChild);

  // walk up until we find a scrollframe or a part
  // if it's a scrollframe return it.
  // if it's a parent then the child passed does not
  // have a scroll frame immediately wrapped around it.
  while (parent) {
    nsIScrollableFrame* scrollFrame = do_QueryFrame(parent);
    // scrollframe? Yep return it.
    if (scrollFrame) return parent;

    nsCOMPtr<nsIGridPart> parentGridRow = GetPartFromBox(parent);
    // if a part then just return the child
    if (parentGridRow) break;

    parent = nsIFrame::GetParentXULBox(parent);
  }

  return aChild;
}

#ifdef DEBUG_grid
void nsGrid::PrintCellMap() {
  printf("-----Columns------\n");
  for (int x = 0; x < mColumnCount; x++) {
    nsGridRow* column = GetColumnAt(x);
    printf("%d(pf=%d, mn=%d, mx=%d) ", x, column->mPref, column->mMin,
           column->mMax);
  }

  printf("\n-----Rows------\n");
  for (x = 0; x < mRowCount; x++) {
    nsGridRow* column = GetRowAt(x);
    printf("%d(pf=%d, mn=%d, mx=%d) ", x, column->mPref, column->mMin,
           column->mMax);
  }

  printf("\n");
}
#endif
