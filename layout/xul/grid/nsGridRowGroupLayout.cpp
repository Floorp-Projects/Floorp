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

/*
 * The nsGridRowGroupLayout implements the <rows> or <columns> tag in a grid.
 */

#include "nsGridRowGroupLayout.h"
#include "nsCOMPtr.h"
#include "nsIScrollableFrame.h"
#include "nsBox.h"
#include "nsBoxLayoutState.h"
#include "nsGridLayout2.h"
#include "nsGridRow.h"
#include "mozilla/ReflowInput.h"

already_AddRefed<nsBoxLayout> NS_NewGridRowGroupLayout() {
  RefPtr<nsBoxLayout> layout = new nsGridRowGroupLayout();
  return layout.forget();
}

nsGridRowGroupLayout::nsGridRowGroupLayout()
    : nsGridRowLayout(), mRowCount(0) {}

nsGridRowGroupLayout::~nsGridRowGroupLayout() {}

void nsGridRowGroupLayout::ChildAddedOrRemoved(nsIFrame* aBox,
                                               nsBoxLayoutState& aState) {
  int32_t index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  bool isHorizontal = IsXULHorizontal(aBox);

  if (grid) grid->RowAddedOrRemoved(aState, index, isHorizontal);
}

void nsGridRowGroupLayout::AddWidth(nsSize& aSize, nscoord aSize2,
                                    bool aIsHorizontal) {
  nscoord& size = GET_WIDTH(aSize, aIsHorizontal);

  if (size == NS_INTRINSICSIZE || aSize2 == NS_INTRINSICSIZE)
    size = NS_INTRINSICSIZE;
  else
    size += aSize2;
}

nsSize nsGridRowGroupLayout::GetXULPrefSize(nsIFrame* aBox,
                                            nsBoxLayoutState& aState) {
  nsSize vpref = nsGridRowLayout::GetXULPrefSize(aBox, aState);

  /* It is possible that we could have some extra columns. This is when less
   * columns in XUL were defined that needed. And example might be a grid with 3
   * defined columns but a row with 4 cells in it. We would need an extra column
   * to make the grid work. But because that extra column does not have a box
   * associated with it we must add its size in manually. Remember we could have
   * extra rows as well.
   */

  int32_t index = 0;
  nsGrid* grid = GetGrid(aBox, &index);

  if (grid) {
    // make sure we add in extra columns sizes as well
    bool isHorizontal = IsXULHorizontal(aBox);
    int32_t extraColumns = grid->GetExtraColumnCount(isHorizontal);
    int32_t start = grid->GetColumnCount(isHorizontal) -
                    grid->GetExtraColumnCount(isHorizontal);
    for (int32_t i = 0; i < extraColumns; i++) {
      nscoord pref = grid->GetPrefRowHeight(
          aState, i + start, !isHorizontal);  // GetPrefColumnWidth

      AddWidth(vpref, pref, isHorizontal);
    }
  }

  return vpref;
}

nsSize nsGridRowGroupLayout::GetXULMaxSize(nsIFrame* aBox,
                                           nsBoxLayoutState& aState) {
  nsSize maxSize = nsGridRowLayout::GetXULMaxSize(aBox, aState);

  int32_t index = 0;
  nsGrid* grid = GetGrid(aBox, &index);

  if (grid) {
    // make sure we add in extra columns sizes as well
    bool isHorizontal = IsXULHorizontal(aBox);
    int32_t extraColumns = grid->GetExtraColumnCount(isHorizontal);
    int32_t start = grid->GetColumnCount(isHorizontal) -
                    grid->GetExtraColumnCount(isHorizontal);
    for (int32_t i = 0; i < extraColumns; i++) {
      nscoord max = grid->GetMaxRowHeight(aState, i + start,
                                          !isHorizontal);  // GetMaxColumnWidth

      AddWidth(maxSize, max, isHorizontal);
    }
  }

  return maxSize;
}

nsSize nsGridRowGroupLayout::GetXULMinSize(nsIFrame* aBox,
                                           nsBoxLayoutState& aState) {
  nsSize minSize = nsGridRowLayout::GetXULMinSize(aBox, aState);

  int32_t index = 0;
  nsGrid* grid = GetGrid(aBox, &index);

  if (grid) {
    // make sure we add in extra columns sizes as well
    bool isHorizontal = IsXULHorizontal(aBox);
    int32_t extraColumns = grid->GetExtraColumnCount(isHorizontal);
    int32_t start = grid->GetColumnCount(isHorizontal) -
                    grid->GetExtraColumnCount(isHorizontal);
    for (int32_t i = 0; i < extraColumns; i++) {
      nscoord min = grid->GetMinRowHeight(aState, i + start,
                                          !isHorizontal);  // GetMinColumnWidth
      AddWidth(minSize, min, isHorizontal);
    }
  }

  return minSize;
}

/*
 * Run down through our children dirtying them recursively.
 */
void nsGridRowGroupLayout::DirtyRows(nsIFrame* aBox, nsBoxLayoutState& aState) {
  if (aBox) {
    // mark us dirty
    // XXXldb We probably don't want to walk up the ancestor chain
    // calling MarkIntrinsicISizesDirty for every row group.
    aState.PresShell()->FrameNeedsReflow(aBox, IntrinsicDirty::TreeChange,
                                         NS_FRAME_IS_DIRTY);
    nsIFrame* child = nsBox::GetChildXULBox(aBox);

    while (child) {
      // walk into scrollframes
      nsIFrame* deepChild = nsGrid::GetScrolledBox(child);

      // walk into other monuments
      nsIGridPart* monument = nsGrid::GetPartFromBox(deepChild);
      if (monument) monument->DirtyRows(deepChild, aState);

      child = nsBox::GetNextXULBox(child);
    }
  }
}

void nsGridRowGroupLayout::CountRowsColumns(nsIFrame* aBox, int32_t& aRowCount,
                                            int32_t& aComputedColumnCount) {
  if (aBox) {
    int32_t startCount = aRowCount;

    nsIFrame* child = nsBox::GetChildXULBox(aBox);

    while (child) {
      // first see if it is a scrollframe. If so walk down into it and get the
      // scrolled child
      nsIFrame* deepChild = nsGrid::GetScrolledBox(child);

      nsIGridPart* monument = nsGrid::GetPartFromBox(deepChild);
      if (monument) {
        monument->CountRowsColumns(deepChild, aRowCount, aComputedColumnCount);
        child = nsBox::GetNextXULBox(child);
        deepChild = child;
        continue;
      }

      child = nsBox::GetNextXULBox(child);

      // if not a monument. Then count it. It will be a bogus row
      aRowCount++;
    }

    mRowCount = aRowCount - startCount;
  }
}

/**
 * Fill out the given row structure recursively
 */
int32_t nsGridRowGroupLayout::BuildRows(nsIFrame* aBox, nsGridRow* aRows) {
  int32_t rowCount = 0;

  if (aBox) {
    nsIFrame* child = nsBox::GetChildXULBox(aBox);

    while (child) {
      // first see if it is a scrollframe. If so walk down into it and get the
      // scrolled child
      nsIFrame* deepChild = nsGrid::GetScrolledBox(child);

      nsIGridPart* monument = nsGrid::GetPartFromBox(deepChild);
      if (monument) {
        rowCount += monument->BuildRows(deepChild, &aRows[rowCount]);
        child = nsBox::GetNextXULBox(child);
        deepChild = child;
        continue;
      }

      aRows[rowCount].Init(child, true);

      child = nsBox::GetNextXULBox(child);

      // if not a monument. Then count it. It will be a bogus row
      rowCount++;
    }
  }

  return rowCount;
}

nsMargin nsGridRowGroupLayout::GetTotalMargin(nsIFrame* aBox,
                                              bool aIsHorizontal) {
  // group have border and padding added to the total margin

  nsMargin margin = nsGridRowLayout::GetTotalMargin(aBox, aIsHorizontal);

  // make sure we have the scrollframe on the outside if it has one.
  // that's where the border is.
  aBox = nsGrid::GetScrollBox(aBox);

  // add our border/padding to it
  nsMargin borderPadding(0, 0, 0, 0);
  aBox->GetXULBorderAndPadding(borderPadding);
  margin += borderPadding;

  return margin;
}
