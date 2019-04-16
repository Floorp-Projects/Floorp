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

#include "nsGridLayout2.h"
#include "nsGridRowGroupLayout.h"
#include "nsGridRow.h"
#include "nsBox.h"
#include "nsIScrollableFrame.h"
#include "nsSprocketLayout.h"
#include "mozilla/ReflowInput.h"

nsresult NS_NewGridLayout2(nsBoxLayout** aNewLayout) {
  *aNewLayout = new nsGridLayout2();
  NS_IF_ADDREF(*aNewLayout);

  return NS_OK;
}

// static
void nsGridLayout2::AddOffset(nsIFrame* aChild, nsSize& aSize) {
  nsMargin offset;
  GetOffset(aChild, offset);
  aSize.width += offset.left;
  aSize.height += offset.top;
}

NS_IMETHODIMP
nsGridLayout2::XULLayout(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) {
  // XXX This should be set a better way!
  mGrid.SetBox(aBox);
  NS_ASSERTION(aBox->GetXULLayoutManager() == this, "setting incorrect box");

  nsresult rv = nsStackLayout::XULLayout(aBox, aBoxLayoutState);
#ifdef DEBUG_grid
  mGrid.PrintCellMap();
#endif
  return rv;
}

void nsGridLayout2::IntrinsicISizesDirty(nsIFrame* aBox,
                                         nsBoxLayoutState& aBoxLayoutState) {
  nsStackLayout::IntrinsicISizesDirty(aBox, aBoxLayoutState);
  // XXXldb We really don't need to do all the work that NeedsRebuild
  // does; we just need to mark intrinsic widths dirty on the
  // (row/column)(s/-groups).
  mGrid.NeedsRebuild(aBoxLayoutState);
}

nsGrid* nsGridLayout2::GetGrid(nsIFrame* aBox, int32_t* aIndex,
                               nsGridRowLayout* aRequestor) {
  // XXX This should be set a better way!
  mGrid.SetBox(aBox);
  NS_ASSERTION(aBox->GetXULLayoutManager() == this, "setting incorrect box");
  return &mGrid;
}

void nsGridLayout2::AddWidth(nsSize& aSize, nscoord aSize2,
                             bool aIsHorizontal) {
  nscoord& size = GET_WIDTH(aSize, aIsHorizontal);

  if (size != NS_INTRINSICSIZE) {
    if (aSize2 == NS_INTRINSICSIZE)
      size = NS_INTRINSICSIZE;
    else
      size += aSize2;
  }
}

nsSize nsGridLayout2::GetXULMinSize(nsIFrame* aBox, nsBoxLayoutState& aState) {
  nsSize minSize = nsStackLayout::GetXULMinSize(aBox, aState);

  // if there are no <rows> tags that will sum up our columns,
  // sum up our columns here.
  nsSize total(0, 0);
  nsIFrame* rowsBox = mGrid.GetRowsBox();
  nsIFrame* columnsBox = mGrid.GetColumnsBox();
  if (!rowsBox || !columnsBox) {
    if (!rowsBox) {
      // max height is the sum of our rows
      int32_t rows = mGrid.GetRowCount();
      for (int32_t i = 0; i < rows; i++) {
        nscoord height = mGrid.GetMinRowHeight(aState, i, true);
        AddWidth(total, height, false);  // AddHeight
      }
    }

    if (!columnsBox) {
      // max height is the sum of our rows
      int32_t columns = mGrid.GetColumnCount();
      for (int32_t i = 0; i < columns; i++) {
        nscoord width = mGrid.GetMinRowHeight(aState, i, false);
        AddWidth(total, width, true);  // AddWidth
      }
    }

    AddMargin(aBox, total);
    AddOffset(aBox, total);
    AddLargestSize(minSize, total);
  }

  return minSize;
}

nsSize nsGridLayout2::GetXULPrefSize(nsIFrame* aBox, nsBoxLayoutState& aState) {
  nsSize pref = nsStackLayout::GetXULPrefSize(aBox, aState);

  // if there are no <rows> tags that will sum up our columns,
  // sum up our columns here.
  nsSize total(0, 0);
  nsIFrame* rowsBox = mGrid.GetRowsBox();
  nsIFrame* columnsBox = mGrid.GetColumnsBox();
  if (!rowsBox || !columnsBox) {
    if (!rowsBox) {
      // max height is the sum of our rows
      int32_t rows = mGrid.GetRowCount();
      for (int32_t i = 0; i < rows; i++) {
        nscoord height = mGrid.GetPrefRowHeight(aState, i, true);
        AddWidth(total, height, false);  // AddHeight
      }
    }

    if (!columnsBox) {
      // max height is the sum of our rows
      int32_t columns = mGrid.GetColumnCount();
      for (int32_t i = 0; i < columns; i++) {
        nscoord width = mGrid.GetPrefRowHeight(aState, i, false);
        AddWidth(total, width, true);  // AddWidth
      }
    }

    AddMargin(aBox, total);
    AddOffset(aBox, total);
    AddLargestSize(pref, total);
  }

  return pref;
}

nsSize nsGridLayout2::GetXULMaxSize(nsIFrame* aBox, nsBoxLayoutState& aState) {
  nsSize maxSize = nsStackLayout::GetXULMaxSize(aBox, aState);

  // if there are no <rows> tags that will sum up our columns,
  // sum up our columns here.
  nsSize total(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  nsIFrame* rowsBox = mGrid.GetRowsBox();
  nsIFrame* columnsBox = mGrid.GetColumnsBox();
  if (!rowsBox || !columnsBox) {
    if (!rowsBox) {
      total.height = 0;
      // max height is the sum of our rows
      int32_t rows = mGrid.GetRowCount();
      for (int32_t i = 0; i < rows; i++) {
        nscoord height = mGrid.GetMaxRowHeight(aState, i, true);
        AddWidth(total, height, false);  // AddHeight
      }
    }

    if (!columnsBox) {
      total.width = 0;
      // max height is the sum of our rows
      int32_t columns = mGrid.GetColumnCount();
      for (int32_t i = 0; i < columns; i++) {
        nscoord width = mGrid.GetMaxRowHeight(aState, i, false);
        AddWidth(total, width, true);  // AddWidth
      }
    }

    AddMargin(aBox, total);
    AddOffset(aBox, total);
    AddSmallestSize(maxSize, total);
  }

  return maxSize;
}

int32_t nsGridLayout2::BuildRows(nsIFrame* aBox, nsGridRow* aRows) {
  if (aBox) {
    aRows[0].Init(aBox, true);
    return 1;
  }
  return 0;
}

nsMargin nsGridLayout2::GetTotalMargin(nsIFrame* aBox, bool aIsHorizontal) {
  nsMargin margin(0, 0, 0, 0);
  return margin;
}

void nsGridLayout2::ChildrenInserted(nsIFrame* aBox, nsBoxLayoutState& aState,
                                     nsIFrame* aPrevBox,
                                     const nsFrameList::Slice& aNewChildren) {
  mGrid.NeedsRebuild(aState);
}

void nsGridLayout2::ChildrenAppended(nsIFrame* aBox, nsBoxLayoutState& aState,
                                     const nsFrameList::Slice& aNewChildren) {
  mGrid.NeedsRebuild(aState);
}

void nsGridLayout2::ChildrenRemoved(nsIFrame* aBox, nsBoxLayoutState& aState,
                                    nsIFrame* aChildList) {
  mGrid.NeedsRebuild(aState);
}

void nsGridLayout2::ChildrenSet(nsIFrame* aBox, nsBoxLayoutState& aState,
                                nsIFrame* aChildList) {
  mGrid.NeedsRebuild(aState);
}

NS_IMPL_ADDREF_INHERITED(nsGridLayout2, nsStackLayout)
NS_IMPL_RELEASE_INHERITED(nsGridLayout2, nsStackLayout)

NS_INTERFACE_MAP_BEGIN(nsGridLayout2)
  NS_INTERFACE_MAP_ENTRY(nsIGridPart)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGridPart)
NS_INTERFACE_MAP_END_INHERITING(nsStackLayout)
