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

#include "nsGridRowLeafLayout.h"
#include "nsGridRowGroupLayout.h"
#include "nsGridRow.h"
#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsIScrollableFrame.h"
#include "nsBoxFrame.h"
#include "nsGridLayout2.h"

already_AddRefed<nsBoxLayout> NS_NewGridRowLeafLayout()
{
  nsBoxLayout* layout = new nsGridRowLeafLayout();
  NS_IF_ADDREF(layout);
  return layout;
} 

nsGridRowLeafLayout::nsGridRowLeafLayout():nsGridRowLayout()
{
}

nsGridRowLeafLayout::~nsGridRowLeafLayout()
{
}

nsSize
nsGridRowLeafLayout::GetPrefSize(nsIFrame* aBox, nsBoxLayoutState& aState)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  bool isHorizontal = IsHorizontal(aBox);

  // If we are not in a grid. Then we just work like a box. But if we are in a grid
  // ask the grid for our size.
  if (!grid) {
    return nsGridRowLayout::GetPrefSize(aBox, aState); 
  }
  else {
    return grid->GetPrefRowSize(aState, index, isHorizontal);
    //AddBorderAndPadding(aBox, pref);
  }
}

nsSize
nsGridRowLeafLayout::GetMinSize(nsIFrame* aBox, nsBoxLayoutState& aState)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  bool isHorizontal = IsHorizontal(aBox);

  if (!grid)
    return nsGridRowLayout::GetMinSize(aBox, aState); 
  else {
    nsSize minSize = grid->GetMinRowSize(aState, index, isHorizontal);
    AddBorderAndPadding(aBox, minSize);
    return minSize;
  }
}

nsSize
nsGridRowLeafLayout::GetMaxSize(nsIFrame* aBox, nsBoxLayoutState& aState)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  bool isHorizontal = IsHorizontal(aBox);

  if (!grid)
    return nsGridRowLayout::GetMaxSize(aBox, aState); 
  else {
    nsSize maxSize;
    maxSize = grid->GetMaxRowSize(aState, index, isHorizontal);
    AddBorderAndPadding(aBox, maxSize);
    return maxSize;
  }
}

/** If a child is added or removed or changes size
  */
void
nsGridRowLeafLayout::ChildAddedOrRemoved(nsIFrame* aBox, nsBoxLayoutState& aState)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  bool isHorizontal = IsHorizontal(aBox);

  if (grid)
    grid->CellAddedOrRemoved(aState, index, isHorizontal);
}

void
nsGridRowLeafLayout::PopulateBoxSizes(nsIFrame* aBox, nsBoxLayoutState& aState, nsBoxSize*& aBoxSizes, nscoord& aMinSize, nscoord& aMaxSize, PRInt32& aFlexes)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  bool isHorizontal = IsHorizontal(aBox);

  // Our base class SprocketLayout is giving us a chance to change the box sizes before layout
  // If we are a row lets change the sizes to match our columns. If we are a column then do the opposite
  // and make them match or rows.
  if (grid) {
    nsGridRow* column;
    PRInt32 count = grid->GetColumnCount(isHorizontal); 
    nsBoxSize* start = nullptr;
    nsBoxSize* last = nullptr;
    nsBoxSize* current = nullptr;
    nsIFrame* child = aBox->GetChildBox();
    for (int i=0; i < count; i++)
    {
      column = grid->GetColumnAt(i,isHorizontal); 

      // make sure the value was computed before we use it.
      // !isHorizontal is passed in to invert the behavior of these methods.
      nscoord pref =
        grid->GetPrefRowHeight(aState, i, !isHorizontal); // GetPrefColumnWidth
      nscoord min = 
        grid->GetMinRowHeight(aState, i, !isHorizontal);  // GetMinColumnWidth
      nscoord max = 
        grid->GetMaxRowHeight(aState, i, !isHorizontal);  // GetMaxColumnWidth
      nscoord flex =
        grid->GetRowFlex(aState, i, !isHorizontal);       // GetColumnFlex
      nscoord left  = 0;
      nscoord right  = 0;
      grid->GetRowOffsets(aState, i, left, right, !isHorizontal); // GetColumnOffsets
      nsIFrame* box = column->GetBox();
      bool collapsed = false;
      nscoord topMargin = column->mTopMargin;
      nscoord bottomMargin = column->mBottomMargin;

      if (box) 
        collapsed = box->IsCollapsed();

      pref = pref - (left + right);
      if (pref < 0)
        pref = 0;

      // if this is the first or last column. Take into account that
      // our row could have a border that could affect our left or right
      // padding from our columns. If the row has padding subtract it.
      // would should always be able to garentee that our margin is smaller
      // or equal to our left or right
      PRInt32 firstIndex = 0;
      PRInt32 lastIndex = 0;
      nsGridRow* firstRow = nullptr;
      nsGridRow* lastRow = nullptr;
      grid->GetFirstAndLastRow(aState, firstIndex, lastIndex, firstRow, lastRow, !isHorizontal);

      if (i == firstIndex || i == lastIndex) {
        nsMargin offset = GetTotalMargin(aBox, isHorizontal);

        nsMargin border(0,0,0,0);
        // can't call GetBorderPadding we will get into recursion
        aBox->GetBorder(border);
        offset += border;
        aBox->GetPadding(border);
        offset += border;

        // subtract from out left and right
        if (i == firstIndex) 
        {
          if (isHorizontal)
           left -= offset.left;
          else
           left -= offset.top;
        }

        if (i == lastIndex)
        {
          if (isHorizontal)
           right -= offset.right;
          else
           right -= offset.bottom;
        }
      }
    
      // initialize the box size here 
      max = NS_MAX(min, max);
      pref = nsBox::BoundsCheck(min, pref, max);
   
      current = new (aState) nsBoxSize();
      current->pref = pref;
      current->min = min;
      current->max = max;
      current->flex = flex;
      current->bogus = column->mIsBogus;
      current->left = left + topMargin;
      current->right = right + bottomMargin;
      current->collapsed = collapsed;

      if (!start) {
        start = current;
        last = start;
      } else {
        last->next = current;
        last = current;
      }

      if (child && !column->mIsBogus)
        child = child->GetNextBox();

    }
    aBoxSizes = start;
  }

  nsSprocketLayout::PopulateBoxSizes(aBox, aState, aBoxSizes, aMinSize, aMaxSize, aFlexes);
}

void
nsGridRowLeafLayout::ComputeChildSizes(nsIFrame* aBox,
                           nsBoxLayoutState& aState, 
                           nscoord& aGivenSize, 
                           nsBoxSize* aBoxSizes, 
                           nsComputedBoxSize*& aComputedBoxSizes)
{ 
  // see if we are in a scrollable frame. If we are then there could be scrollbars present
  // if so we need to subtract them out to make sure our columns line up.
  if (aBox) {
    bool isHorizontal = aBox->IsHorizontal();

    // go up the parent chain looking for scrollframes
    nscoord diff = 0;
    nsIFrame* parentBox;
    (void)GetParentGridPart(aBox, &parentBox);
    while (parentBox) {
      nsIFrame* scrollbox = nsGrid::GetScrollBox(parentBox);
      nsIScrollableFrame *scrollable = do_QueryFrame(scrollbox);
      if (scrollable) {
        // Don't call GetActualScrollbarSizes here because it's not safe
        // to call that while we're reflowing the contents of the scrollframe,
        // which we are here.
        nsMargin scrollbarSizes = scrollable->GetDesiredScrollbarSizes(&aState);
        PRUint32 visible = scrollable->GetScrollbarVisibility();

        if (isHorizontal && (visible & nsIScrollableFrame::VERTICAL)) {
          diff += scrollbarSizes.left + scrollbarSizes.right;
        } else if (!isHorizontal && (visible & nsIScrollableFrame::HORIZONTAL)) {
          diff += scrollbarSizes.top + scrollbarSizes.bottom;
        }
      }

      (void)GetParentGridPart(parentBox, &parentBox);
    }

    if (diff > 0) {
      aGivenSize += diff;

      nsSprocketLayout::ComputeChildSizes(aBox, aState, aGivenSize, aBoxSizes, aComputedBoxSizes);

      aGivenSize -= diff;

      nsComputedBoxSize* s    = aComputedBoxSizes;
      nsComputedBoxSize* last = aComputedBoxSizes;
      while(s)
      {
        last = s;
        s = s->next;
      }
  
      if (last) 
        last->size -= diff;                         

      return;
    }
  }
      
  nsSprocketLayout::ComputeChildSizes(aBox, aState, aGivenSize, aBoxSizes, aComputedBoxSizes);

}

NS_IMETHODIMP
nsGridRowLeafLayout::Layout(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  return nsGridRowLayout::Layout(aBox, aBoxLayoutState);
}

void
nsGridRowLeafLayout::DirtyRows(nsIFrame* aBox, nsBoxLayoutState& aState)
{
  if (aBox) {
    // mark us dirty
    // XXXldb We probably don't want to walk up the ancestor chain
    // calling MarkIntrinsicWidthsDirty for every row.
    aState.PresShell()->FrameNeedsReflow(aBox, nsIPresShell::eTreeChange,
                                         NS_FRAME_IS_DIRTY);
  }
}

void
nsGridRowLeafLayout::CountRowsColumns(nsIFrame* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount)
{
  if (aBox) {
    nsIFrame* child = aBox->GetChildBox();

    // count the children
    PRInt32 columnCount = 0;
    while(child) {
      child = child->GetNextBox();
      columnCount++;
    }

    // if our count is greater than the current column count
    if (columnCount > aComputedColumnCount) 
      aComputedColumnCount = columnCount;

    aRowCount++;
  }
}

PRInt32
nsGridRowLeafLayout::BuildRows(nsIFrame* aBox, nsGridRow* aRows)
{
  if (aBox) {
      aRows[0].Init(aBox, false);
      return 1;
  }

  return 0;
}

