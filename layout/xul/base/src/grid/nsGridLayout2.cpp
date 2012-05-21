/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

nsresult
NS_NewGridLayout2( nsIPresShell* aPresShell, nsBoxLayout** aNewLayout)
{
  *aNewLayout = new nsGridLayout2(aPresShell);
  NS_IF_ADDREF(*aNewLayout);

  return NS_OK;
  
} 

nsGridLayout2::nsGridLayout2(nsIPresShell* aPresShell):nsStackLayout()
{
}

// static
void
nsGridLayout2::AddOffset(nsBoxLayoutState& aState, nsIBox* aChild, nsSize& aSize)
{
  nsMargin offset;
  GetOffset(aState, aChild, offset);
  aSize.width += offset.left;
  aSize.height += offset.top;
}

NS_IMETHODIMP
nsGridLayout2::Layout(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  // XXX This should be set a better way!
  mGrid.SetBox(aBox);
  NS_ASSERTION(aBox->GetLayoutManager() == this, "setting incorrect box");

  nsresult rv = nsStackLayout::Layout(aBox, aBoxLayoutState);
#ifdef DEBUG_grid
  mGrid.PrintCellMap();
#endif
  return rv;
}

void
nsGridLayout2::IntrinsicWidthsDirty(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  nsStackLayout::IntrinsicWidthsDirty(aBox, aBoxLayoutState);
  // XXXldb We really don't need to do all the work that NeedsRebuild
  // does; we just need to mark intrinsic widths dirty on the
  // (row/column)(s/-groups).
  mGrid.NeedsRebuild(aBoxLayoutState);
}

nsGrid*
nsGridLayout2::GetGrid(nsIBox* aBox, PRInt32* aIndex, nsGridRowLayout* aRequestor)
{
  // XXX This should be set a better way!
  mGrid.SetBox(aBox);
  NS_ASSERTION(aBox->GetLayoutManager() == this, "setting incorrect box");
  return &mGrid;
}

void
nsGridLayout2::AddWidth(nsSize& aSize, nscoord aSize2, bool aIsHorizontal)
{
  nscoord& size = GET_WIDTH(aSize, aIsHorizontal);

  if (size != NS_INTRINSICSIZE) {
    if (aSize2 == NS_INTRINSICSIZE)
      size = NS_INTRINSICSIZE;
    else
      size += aSize2;
  }
}

nsSize
nsGridLayout2::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsSize minSize = nsStackLayout::GetMinSize(aBox, aState); 

  // if there are no <rows> tags that will sum up our columns,
  // sum up our columns here.
  nsSize total(0,0);
  nsIBox* rowsBox = mGrid.GetRowsBox();
  nsIBox* columnsBox = mGrid.GetColumnsBox();
  if (!rowsBox || !columnsBox) {
    if (!rowsBox) {
      // max height is the sum of our rows
      PRInt32 rows = mGrid.GetRowCount();
      for (PRInt32 i=0; i < rows; i++)
      {
        nscoord height = mGrid.GetMinRowHeight(aState, i, true); 
        AddWidth(total, height, false); // AddHeight
      }
    }

    if (!columnsBox) {
      // max height is the sum of our rows
      PRInt32 columns = mGrid.GetColumnCount();
      for (PRInt32 i=0; i < columns; i++)
      {
        nscoord width = mGrid.GetMinRowHeight(aState, i, false); 
        AddWidth(total, width, true); // AddWidth
      }
    }

    AddMargin(aBox, total);
    AddOffset(aState, aBox, total);
    AddLargestSize(minSize, total);
  }
  
  return minSize;
}

nsSize
nsGridLayout2::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsSize pref = nsStackLayout::GetPrefSize(aBox, aState); 

  // if there are no <rows> tags that will sum up our columns,
  // sum up our columns here.
  nsSize total(0,0);
  nsIBox* rowsBox = mGrid.GetRowsBox();
  nsIBox* columnsBox = mGrid.GetColumnsBox();
  if (!rowsBox || !columnsBox) {
    if (!rowsBox) {
      // max height is the sum of our rows
      PRInt32 rows = mGrid.GetRowCount();
      for (PRInt32 i=0; i < rows; i++)
      {
        nscoord height = mGrid.GetPrefRowHeight(aState, i, true); 
        AddWidth(total, height, false); // AddHeight
      }
    }

    if (!columnsBox) {
      // max height is the sum of our rows
      PRInt32 columns = mGrid.GetColumnCount();
      for (PRInt32 i=0; i < columns; i++)
      {
        nscoord width = mGrid.GetPrefRowHeight(aState, i, false);
        AddWidth(total, width, true); // AddWidth
      }
    }

    AddMargin(aBox, total);
    AddOffset(aState, aBox, total);
    AddLargestSize(pref, total);
  }

  return pref;
}

nsSize
nsGridLayout2::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsSize maxSize = nsStackLayout::GetMaxSize(aBox, aState); 

  // if there are no <rows> tags that will sum up our columns,
  // sum up our columns here.
  nsSize total(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  nsIBox* rowsBox = mGrid.GetRowsBox();
  nsIBox* columnsBox = mGrid.GetColumnsBox();
  if (!rowsBox || !columnsBox) {
    if (!rowsBox) {
      total.height = 0;
      // max height is the sum of our rows
      PRInt32 rows = mGrid.GetRowCount();
      for (PRInt32 i=0; i < rows; i++)
      {
        nscoord height = mGrid.GetMaxRowHeight(aState, i, true); 
        AddWidth(total, height, false); // AddHeight
      }
    }

    if (!columnsBox) {
      total.width = 0;
      // max height is the sum of our rows
      PRInt32 columns = mGrid.GetColumnCount();
      for (PRInt32 i=0; i < columns; i++)
      {
        nscoord width = mGrid.GetMaxRowHeight(aState, i, false);
        AddWidth(total, width, true); // AddWidth
      }
    }

    AddMargin(aBox, total);
    AddOffset(aState, aBox, total);
    AddSmallestSize(maxSize, total);
  }

  return maxSize;
}

PRInt32
nsGridLayout2::BuildRows(nsIBox* aBox, nsGridRow* aRows)
{
  if (aBox) {
    aRows[0].Init(aBox, true);
    return 1;
  }
  return 0;
}

nsMargin
nsGridLayout2::GetTotalMargin(nsIBox* aBox, bool aIsHorizontal)
{
  nsMargin margin(0,0,0,0);
  return margin;
}

void
nsGridLayout2::ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState,
                                nsIBox* aPrevBox,
                                const nsFrameList::Slice& aNewChildren)
{
  mGrid.NeedsRebuild(aState);
}

void
nsGridLayout2::ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState,
                                const nsFrameList::Slice& aNewChildren)
{
  mGrid.NeedsRebuild(aState);
}

void
nsGridLayout2::ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState,
                               nsIBox* aChildList)
{
  mGrid.NeedsRebuild(aState);
}

void
nsGridLayout2::ChildrenSet(nsIBox* aBox, nsBoxLayoutState& aState,
                           nsIBox* aChildList)
{
  mGrid.NeedsRebuild(aState);
}

NS_IMPL_ADDREF_INHERITED(nsGridLayout2, nsStackLayout)
NS_IMPL_RELEASE_INHERITED(nsGridLayout2, nsStackLayout)

NS_INTERFACE_MAP_BEGIN(nsGridLayout2)
  NS_INTERFACE_MAP_ENTRY(nsIGridPart)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGridPart)
NS_INTERFACE_MAP_END_INHERITING(nsStackLayout)
