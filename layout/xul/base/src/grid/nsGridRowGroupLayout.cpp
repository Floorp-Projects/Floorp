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


/*
 * The nsGridRowGroupLayout implements the <rows> or <columns> tag in a grid. 
 */

#include "nsGridRowGroupLayout.h"
#include "nsCOMPtr.h"
#include "nsIScrollableFrame.h"
#include "nsBoxLayoutState.h"
#include "nsGridLayout2.h"
#include "nsGridRow.h"

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
nsGridRowGroupLayout::ChildAddedOrRemoved(nsIBox* aBox, nsBoxLayoutState& aState)
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
nsGridRowGroupLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState)
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
nsGridRowGroupLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState)
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
nsGridRowGroupLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState)
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
nsGridRowGroupLayout::DirtyRows(nsIBox* aBox, nsBoxLayoutState& aState)
{
  if (aBox) {
    // mark us dirty
    // XXXldb We probably don't want to walk up the ancestor chain
    // calling MarkIntrinsicWidthsDirty for every row group.
    aState.PresShell()->FrameNeedsReflow(aBox, nsIPresShell::eTreeChange,
                                         NS_FRAME_IS_DIRTY);
    nsIBox* child = aBox->GetChildBox();

    while(child) {

      // walk into scrollframes
      nsIBox* deepChild = nsGrid::GetScrolledBox(child);

      // walk into other monuments
      nsIGridPart* monument = nsGrid::GetPartFromBox(deepChild);
      if (monument) 
        monument->DirtyRows(deepChild, aState);

      child = child->GetNextBox();
    }
  }
}


void
nsGridRowGroupLayout::CountRowsColumns(nsIBox* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount)
{
  if (aBox) {
    PRInt32 startCount = aRowCount;

    nsIBox* child = aBox->GetChildBox();

    while(child) {
      
      // first see if it is a scrollframe. If so walk down into it and get the scrolled child
      nsIBox* deepChild = nsGrid::GetScrolledBox(child);

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
nsGridRowGroupLayout::BuildRows(nsIBox* aBox, nsGridRow* aRows)
{ 
  PRInt32 rowCount = 0;

  if (aBox) {
    nsIBox* child = aBox->GetChildBox();

    while(child) {
      
      // first see if it is a scrollframe. If so walk down into it and get the scrolled child
      nsIBox* deepChild = nsGrid::GetScrolledBox(child);

      nsIGridPart* monument = nsGrid::GetPartFromBox(deepChild);
      if (monument) {
        rowCount += monument->BuildRows(deepChild, &aRows[rowCount]);
        child = child->GetNextBox();
        deepChild = child;
        continue;
      }

      aRows[rowCount].Init(child, PR_TRUE);

      child = child->GetNextBox();

      // if not a monument. Then count it. It will be a bogus row
      rowCount++;
    }
  }

  return rowCount;
}

nsMargin
nsGridRowGroupLayout::GetTotalMargin(nsIBox* aBox, bool aIsHorizontal)
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


