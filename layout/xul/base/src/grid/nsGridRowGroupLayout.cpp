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


/*
 * The nsGridRowGroupLayout implements the <rows> or <columns> tag in a grid. 
 */

#include "nsGridRowGroupLayout.h"
#include "nsCOMPtr.h"
#include "nsIScrollableFrame.h"
#include "nsBoxLayoutState.h"
#include "nsGridLayout2.h"
#include "nsGridRow.h"
#include "nsHTMLReflowState.h"

already_AddRefed<nsBoxLayout> NS_NewGridRowGroupLayout()
{
  nsBoxLayout* layout = new nsGridRowGroupLayout();
  NS_IF_ADDREF(layout);
  return layout;
} 

nsGridRowGroupLayout::nsGridRowGroupLayout():nsGridRowLayout(), mRowCount(0)
{
}

nsGridRowGroupLayout::~nsGridRowGroupLayout()
{
}

void
nsGridRowGroupLayout::ChildAddedOrRemoved(nsIFrame* aBox, nsBoxLayoutState& aState)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  bool isHorizontal = IsHorizontal(aBox);

  if (grid)
    grid->RowAddedOrRemoved(aState, index, isHorizontal);
}

void
nsGridRowGroupLayout::AddWidth(nsSize& aSize, nscoord aSize2, bool aIsHorizontal)
{
  nscoord& size = GET_WIDTH(aSize, aIsHorizontal);

  if (size == NS_INTRINSICSIZE || aSize2 == NS_INTRINSICSIZE)
    size = NS_INTRINSICSIZE;
  else
    size += aSize2;
}

nsSize
nsGridRowGroupLayout::GetPrefSize(nsIFrame* aBox, nsBoxLayoutState& aState)
{ 
  nsSize vpref = nsGridRowLayout::GetPrefSize(aBox, aState); 


 /* It is possible that we could have some extra columns. This is when less columns in XUL were 
  * defined that needed. And example might be a grid with 3 defined columns but a row with 4 cells in 
  * it. We would need an extra column to make the grid work. But because that extra column does not 
  * have a box associated with it we must add its size in manually. Remember we could have extra rows
  * as well.
  */

  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);

  if (grid) 
  {
    // make sure we add in extra columns sizes as well
    bool isHorizontal = IsHorizontal(aBox);
    PRInt32 extraColumns = grid->GetExtraColumnCount(isHorizontal);
    PRInt32 start = grid->GetColumnCount(isHorizontal) - grid->GetExtraColumnCount(isHorizontal);
    for (PRInt32 i=0; i < extraColumns; i++)
    {
      nscoord pref =
        grid->GetPrefRowHeight(aState, i+start, !isHorizontal); // GetPrefColumnWidth

      AddWidth(vpref, pref, isHorizontal);
    }
  }

  return vpref;
}

nsSize
nsGridRowGroupLayout::GetMaxSize(nsIFrame* aBox, nsBoxLayoutState& aState)
{
 nsSize maxSize = nsGridRowLayout::GetMaxSize(aBox, aState); 

  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);

  if (grid) 
  {
    // make sure we add in extra columns sizes as well
    bool isHorizontal = IsHorizontal(aBox);
    PRInt32 extraColumns = grid->GetExtraColumnCount(isHorizontal);
    PRInt32 start = grid->GetColumnCount(isHorizontal) - grid->GetExtraColumnCount(isHorizontal);
    for (PRInt32 i=0; i < extraColumns; i++)
    {
      nscoord max =
        grid->GetMaxRowHeight(aState, i+start, !isHorizontal); // GetMaxColumnWidth

      AddWidth(maxSize, max, isHorizontal);
    }
  }

  return maxSize;
}

nsSize
nsGridRowGroupLayout::GetMinSize(nsIFrame* aBox, nsBoxLayoutState& aState)
{
  nsSize minSize = nsGridRowLayout::GetMinSize(aBox, aState); 

  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);

  if (grid) 
  {
    // make sure we add in extra columns sizes as well
    bool isHorizontal = IsHorizontal(aBox);
    PRInt32 extraColumns = grid->GetExtraColumnCount(isHorizontal);
    PRInt32 start = grid->GetColumnCount(isHorizontal) - grid->GetExtraColumnCount(isHorizontal);
    for (PRInt32 i=0; i < extraColumns; i++)
    {
      nscoord min = 
        grid->GetMinRowHeight(aState, i+start, !isHorizontal); // GetMinColumnWidth
      AddWidth(minSize, min, isHorizontal);
    }
  }

  return minSize;
}

/*
 * Run down through our children dirtying them recursively.
 */
void
nsGridRowGroupLayout::DirtyRows(nsIFrame* aBox, nsBoxLayoutState& aState)
{
  if (aBox) {
    // mark us dirty
    // XXXldb We probably don't want to walk up the ancestor chain
    // calling MarkIntrinsicWidthsDirty for every row group.
    aState.PresShell()->FrameNeedsReflow(aBox, nsIPresShell::eTreeChange,
                                         NS_FRAME_IS_DIRTY);
    nsIFrame* child = aBox->GetChildBox();

    while(child) {

      // walk into scrollframes
      nsIFrame* deepChild = nsGrid::GetScrolledBox(child);

      // walk into other monuments
      nsIGridPart* monument = nsGrid::GetPartFromBox(deepChild);
      if (monument) 
        monument->DirtyRows(deepChild, aState);

      child = child->GetNextBox();
    }
  }
}


void
nsGridRowGroupLayout::CountRowsColumns(nsIFrame* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount)
{
  if (aBox) {
    PRInt32 startCount = aRowCount;

    nsIFrame* child = aBox->GetChildBox();

    while(child) {
      
      // first see if it is a scrollframe. If so walk down into it and get the scrolled child
      nsIFrame* deepChild = nsGrid::GetScrolledBox(child);

      nsIGridPart* monument = nsGrid::GetPartFromBox(deepChild);
      if (monument) {
        monument->CountRowsColumns(deepChild, aRowCount, aComputedColumnCount);
        child = child->GetNextBox();
        deepChild = child;
        continue;
      }

      child = child->GetNextBox();

      // if not a monument. Then count it. It will be a bogus row
      aRowCount++;
    }

    mRowCount = aRowCount - startCount;
  }
}


/**
 * Fill out the given row structure recursively
 */
PRInt32 
nsGridRowGroupLayout::BuildRows(nsIFrame* aBox, nsGridRow* aRows)
{ 
  PRInt32 rowCount = 0;

  if (aBox) {
    nsIFrame* child = aBox->GetChildBox();

    while(child) {
      
      // first see if it is a scrollframe. If so walk down into it and get the scrolled child
      nsIFrame* deepChild = nsGrid::GetScrolledBox(child);

      nsIGridPart* monument = nsGrid::GetPartFromBox(deepChild);
      if (monument) {
        rowCount += monument->BuildRows(deepChild, &aRows[rowCount]);
        child = child->GetNextBox();
        deepChild = child;
        continue;
      }

      aRows[rowCount].Init(child, true);

      child = child->GetNextBox();

      // if not a monument. Then count it. It will be a bogus row
      rowCount++;
    }
  }

  return rowCount;
}

nsMargin
nsGridRowGroupLayout::GetTotalMargin(nsIFrame* aBox, bool aIsHorizontal)
{
  // group have border and padding added to the total margin

  nsMargin margin = nsGridRowLayout::GetTotalMargin(aBox, aIsHorizontal);
  
  // make sure we have the scrollframe on the outside if it has one.
  // that's where the border is.
  aBox = nsGrid::GetScrollBox(aBox);

  // add our border/padding to it
  nsMargin borderPadding(0,0,0,0);
  aBox->GetBorderAndPadding(borderPadding);
  margin += borderPadding;

  return margin;
}


