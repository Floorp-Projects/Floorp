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


/*
 * The nsGridRowGroupLayout implements the <rows> or <columns> tag in a grid. 
 */

#include "nsGridRowGroupLayout.h"
#include "nsIBox.h"
#include "nsCOMPtr.h"
#include "nsIScrollableFrame.h"
#include "nsBoxLayoutState.h"
#include "nsGridLayout2.h"
#include "nsGridRow.h"

nsresult
NS_NewGridRowGroupLayout( nsIPresShell* aPresShell, nsIBoxLayout** aNewLayout)
{
  *aNewLayout = new nsGridRowGroupLayout(aPresShell);
  NS_IF_ADDREF(*aNewLayout);

  return NS_OK;
  
} 

nsGridRowGroupLayout::nsGridRowGroupLayout(nsIPresShell* aPresShell):nsGridRowLayout(aPresShell), mRowCount(0)
{
}

nsGridRowGroupLayout::~nsGridRowGroupLayout()
{
}

NS_IMETHODIMP
nsGridRowGroupLayout::ChildAddedOrRemoved(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);
  PRInt32 isHorizontal = IsHorizontal(aBox);

  if (grid)
    grid->RowAddedOrRemoved(aState, index, isHorizontal);

  return NS_OK;
}

void
nsGridRowGroupLayout::AddWidth(nsSize& aSize, nscoord aSize2, PRBool aIsHorizontal)
{
  nscoord& size = GET_WIDTH(aSize, aIsHorizontal);

  if (size == NS_INTRINSICSIZE || aSize2 == NS_INTRINSICSIZE)
    size = NS_INTRINSICSIZE;
  else
    size += aSize2;
}

NS_IMETHODIMP
nsGridRowGroupLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{ 
  nsresult rv = nsGridRowLayout::GetPrefSize(aBox, aState, aSize); 


 /* It is possible that we could have some extra columns. This is when less columns in XUL were 
  * defined that needed. And example might be a grid with 3 defined columns but a row with 4 cells in 
  * it. We would need an extra column to make the grid work. But because that extra column does not 
  * have a box associated with it we must add its size in manually. Remember we could have extra rows
  * as well.
  */

  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);

  if (grid) 
  {
    // make sure we add in extra columns sizes as well
    PRInt32 isHorizontal = IsHorizontal(aBox);
    PRInt32 extraColumns = grid->GetExtraColumnCount(isHorizontal);
    PRInt32 start = grid->GetColumnCount(isHorizontal) - grid->GetExtraColumnCount(isHorizontal);
    for (PRInt32 i=0; i < extraColumns; i++)
    {
      nsGridRow* column = grid->GetColumnAt(i+start, isHorizontal);
      nscoord size = 0;
      grid->GetPrefRowHeight(aState, i+start, size, !isHorizontal); // GetPrefColumnWidth

      AddWidth(aSize, size, isHorizontal);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsGridRowGroupLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
 nsresult rv = nsGridRowLayout::GetMaxSize(aBox, aState, aSize); 

  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);

  if (grid) 
  {
    // make sure we add in extra columns sizes as well
    PRInt32 isHorizontal = IsHorizontal(aBox);
    PRInt32 extraColumns = grid->GetExtraColumnCount(isHorizontal);
    PRInt32 start = grid->GetColumnCount(isHorizontal) - grid->GetExtraColumnCount(isHorizontal);
    for (PRInt32 i=0; i < extraColumns; i++)
    {
      nsGridRow* column = grid->GetColumnAt(i+start, isHorizontal);
      nscoord size = 0;
      grid->GetMaxRowHeight(aState, i+start, size, !isHorizontal); // GetMaxColumnWidth

      AddWidth(aSize, size, isHorizontal);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsGridRowGroupLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
 nsresult rv = nsGridRowLayout::GetMinSize(aBox, aState, aSize); 

  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);

  if (grid) 
  {
    // make sure we add in extra columns sizes as well
    PRInt32 isHorizontal = IsHorizontal(aBox);
    PRInt32 extraColumns = grid->GetExtraColumnCount(isHorizontal);
    PRInt32 start = grid->GetColumnCount(isHorizontal) - grid->GetExtraColumnCount(isHorizontal);
    for (PRInt32 i=0; i < extraColumns; i++)
    {
      nsGridRow* column = grid->GetColumnAt(i+start, isHorizontal);
      nscoord size = 0;
      grid->GetMinRowHeight(aState, i+start, size, !isHorizontal); // GetMinColumnWidth

      AddWidth(aSize, size, isHorizontal);
    }
  }

  return rv;
}

/*
 * Run down through our children dirtying them recursively.
 */
NS_IMETHODIMP
nsGridRowGroupLayout::DirtyRows(nsIBox* aBox, nsBoxLayoutState& aState)
{
  if (aBox) {
    // mark us dirty
    aBox->MarkDirty(aState);
    nsIBox* child = nsnull;
    aBox->GetChildBox(&child); 
    nsIBox* deepChild = child;
    PRInt32 rowCount = 0;

    while(child) {

      // walk into scrollframes
      deepChild = nsGrid::GetScrolledBox(child);

      // walk into other monuments
      nsCOMPtr<nsIBoxLayout> layout;
      deepChild->GetLayoutManager(getter_AddRefs(layout));
      if (layout) {
        nsCOMPtr<nsIGridPart> monument( do_QueryInterface(layout) );
        if (monument) 
          monument->DirtyRows(deepChild, aState);
      }

      child->GetNextBox(&child);
      deepChild = child;
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsGridRowGroupLayout::CountRowsColumns(nsIBox* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount)
{
  if (aBox) {
    PRInt32 startCount = aRowCount;

    nsIBox* child = nsnull;
    aBox->GetChildBox(&child); 
    nsIBox* deepChild = child;
    PRInt32 rowCount = 0;

    while(child) {
      
      // first see if it is a scrollframe. If so walk down into it and get the scrolled child
      deepChild = nsGrid::GetScrolledBox(child);

      nsCOMPtr<nsIBoxLayout> layout;
      deepChild->GetLayoutManager(getter_AddRefs(layout));
      if (layout) {
        nsCOMPtr<nsIGridPart> monument( do_QueryInterface(layout) );
        if (monument) {
          monument->CountRowsColumns(deepChild, aRowCount, aComputedColumnCount);
          child->GetNextBox(&child);
          deepChild = child;
          continue;
        }
      }

      child->GetNextBox(&child);
      deepChild = child;

      // if not a monument. Then count it. It will be a bogus row
      aRowCount++;
    }

    mRowCount = aRowCount - startCount;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGridRowGroupLayout::GetRowCount(PRInt32& aRowCount)
{
  aRowCount = mRowCount;
  return NS_OK;
}


/**
 * Fill out the given row structure recursively
 */
NS_IMETHODIMP
nsGridRowGroupLayout::BuildRows(nsIBox* aBox, nsGridRow* aRows, PRInt32* aCount)
{ 
  PRInt32 rowCount = 0;

  if (aBox) {
    nsIBox* child = nsnull;
    aBox->GetChildBox(&child); 
    nsIBox* deepChild = child;

    while(child) {
      
      // first see if it is a scrollframe. If so walk down into it and get the scrolled child
      deepChild = nsGrid::GetScrolledBox(child);

      nsCOMPtr<nsIBoxLayout> layout;
      deepChild->GetLayoutManager(getter_AddRefs(layout));
      if (layout) {
        nsCOMPtr<nsIGridPart> monument( do_QueryInterface(layout) );
        if (monument) {
          PRInt32 count = 0;
          monument->BuildRows(deepChild, &aRows[rowCount], &count);
          rowCount += count;
          child->GetNextBox(&child);
          deepChild = child;
          continue;
        }
      }

      aRows[rowCount].Init(child, PR_TRUE);

      child->GetNextBox(&child);
      deepChild = child;

      // if not a monument. Then count it. It will be a bogus row
      rowCount++;
    }
  }

  *aCount = rowCount;

  return NS_OK;
}

NS_IMETHODIMP
nsGridRowGroupLayout::CastToRowGroupLayout(nsGridRowGroupLayout** aRowGroup)
{
  (*aRowGroup) = this;
  return NS_OK;
}

NS_IMETHODIMP
nsGridRowGroupLayout::GetTotalMargin(nsIBox* aBox, nsMargin& aMargin, PRBool aIsHorizontal)
{
  // group have border and padding added to the total margin

  nsresult rv = nsGridRowLayout::GetTotalMargin(aBox, aMargin, aIsHorizontal);
  
  // make sure we have the scrollframe on the outside if it has one.
  // thats where the border it.
  aBox = nsGrid::GetScrollBox(aBox);

  // add our border/padding to it
  nsMargin borderPadding(0,0,0,0);
  aBox->GetBorderAndPadding(borderPadding);

  aMargin += borderPadding;
  aBox->GetInset(borderPadding);

  aMargin += borderPadding;

  return rv;
}


