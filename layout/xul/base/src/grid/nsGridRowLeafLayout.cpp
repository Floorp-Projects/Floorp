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

#include "nsGridRowLeafLayout.h"
#include "nsGridRowGroupLayout.h"
#include "nsGridRow.h"
#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsIScrollableFrame.h"
#include "nsBoxFrame.h"
#include "nsGridLayout2.h"

nsresult
NS_NewGridRowLeafLayout( nsIPresShell* aPresShell, nsIBoxLayout** aNewLayout)
{
  *aNewLayout = new nsGridRowLeafLayout(aPresShell);
  NS_IF_ADDREF(*aNewLayout);

  return NS_OK;
  
} 

nsGridRowLeafLayout::nsGridRowLeafLayout(nsIPresShell* aPresShell):nsGridRowLayout(aPresShell)
{
}

nsGridRowLeafLayout::~nsGridRowLeafLayout()
{
}

NS_IMETHODIMP
nsGridRowLeafLayout::CastToGridRowLeaf(nsGridRowLeafLayout** aGridRowLeaf)
{
  *aGridRowLeaf = this;
  return NS_OK;
}

NS_IMETHODIMP
nsGridRowLeafLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);
  PRInt32 isHorizontal = IsHorizontal(aBox);

  // If we are not in a grid. Then we just work like a box. But if we are in a grid
  // ask the grid for our size.
  if (!grid)
    return nsGridRowLayout::GetPrefSize(aBox, aState, aSize); 
  else {
    nsresult rv = grid->GetPrefRowSize(aState, index, aSize, isHorizontal);
    //AddBorderAndPadding(aBox, aSize);
    //AddInset(aBox, aSize);
    return rv;
  }
}

NS_IMETHODIMP
nsGridRowLeafLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);
  PRInt32 isHorizontal = IsHorizontal(aBox);

  if (!grid)
    return nsGridRowLayout::GetMinSize(aBox, aState, aSize); 
  else {
    nsresult rv = grid->GetMinRowSize(aState, index, aSize, isHorizontal);
    AddBorderAndPadding(aBox, aSize);
    AddInset(aBox, aSize);
    return rv;
  }
}

NS_IMETHODIMP
nsGridRowLeafLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);
  PRInt32 isHorizontal = IsHorizontal(aBox);

  if (!grid)
    return nsGridRowLayout::GetMaxSize(aBox, aState, aSize); 
  else {
    nsresult rv = grid->GetMaxRowSize(aState, index, aSize, isHorizontal);
    AddBorderAndPadding(aBox, aSize);
    AddInset(aBox, aSize);
    return rv;
  }
}

NS_IMETHODIMP
nsGridRowLeafLayout::ChildBecameDirty(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChild)
{
  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);
  PRInt32 isHorizontal = IsHorizontal(aBox);

  if (grid) {
    PRInt32 columnIndex = -1;
    aBox->GetIndexOf(aChild, &columnIndex);
    grid->RowChildIsDirty(aState, index, columnIndex, isHorizontal);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGridRowLeafLayout::BecameDirty(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);
  PRInt32 isHorizontal = IsHorizontal(aBox);

  if (grid)
    grid->RowIsDirty(aState, index, isHorizontal);

  return NS_OK;
}

/** If a child is added or removed or changes size
  */
NS_IMETHODIMP
nsGridRowLeafLayout::ChildAddedOrRemoved(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);
  PRInt32 isHorizontal = IsHorizontal(aBox);

  if (grid)
    grid->CellAddedOrRemoved(aState, index, isHorizontal);

  return NS_OK;
}

void
nsGridRowLeafLayout::PopulateBoxSizes(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aBoxSizes, nsComputedBoxSize*& aComputedBoxSizes, nscoord& aMinSize, nscoord& aMaxSize, PRInt32& aFlexes)
{
  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  GetGrid(aBox, &grid, &index);
  PRInt32 isHorizontal = IsHorizontal(aBox);

  // Our base class SprocketLayout is giving us a chance to change the box sizes before layout
  // If we are a row lets change the sizes to match our columns. If we are a column then do the opposite
  // and make them match or rows.
  if (grid) {
   nsGridRow* column;
   PRInt32 count = grid->GetColumnCount(isHorizontal); 
   nsBoxSize* start = nsnull;
   nsBoxSize* last = nsnull;
   nsBoxSize* current = nsnull;
   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   for (int i=0; i < count; i++)
   {
     column = grid->GetColumnAt(i,isHorizontal); 

     // make sure the value was computed before we use it.
     nscoord pref = 0;
     nscoord min  = 0;
     nscoord max  = 0;
     nscoord flex  = 0;
     nscoord left  = 0;
     nscoord right  = 0;

     current = new (aState) nsBoxSize();

     // !isHorizontal is passed in to invert the behavor of these methods.
     grid->GetPrefRowHeight(aState, i, pref, !isHorizontal); // GetPrefColumnWidth
     grid->GetMinRowHeight(aState, i, min, !isHorizontal);   // GetMinColumnWidth
     grid->GetMaxRowHeight(aState, i, max, !isHorizontal);   // GetMaxColumnWidth
     grid->GetRowFlex(aState, i, flex, !isHorizontal);       // GetColumnFlex
     grid->GetRowOffsets(aState, i, left, right, !isHorizontal); // GetColumnOffsets

     pref = pref - (left + right);
     if (pref < 0)
       pref = 0;

     // if this is the first or last column. Take into account that
     // our row could have a border that could affect our left or right
     // padding from our columns. If the row has padding subtract it.
     // would should always be able to garentee that our margin is smaller
     // or equal to our left or right
      if (i == 0 || i == count-1) {
        nsMargin offset(0,0,0,0);
        GetTotalMargin(aBox, offset, isHorizontal);
        // subtract from out left and right
        if (i == 0) 
        {
          if (isHorizontal)
           left -= offset.left;
          else
           left -= offset.top;
        }

        if (i == count-1)
        {
          if (isHorizontal)
           right -= offset.right;
          else
           right -= offset.bottom;
        }
      }
      
     // initialize the box size here 
     nsBox::BoundsCheck(min, pref, max);


     current->pref = pref;
     current->min = min;
     current->max = max;
     current->flex = flex;
     current->bogus = column->mIsBogus;
     current->left = left + column->mTopMargin;
     current->right = right + column->mBottomMargin;

     if (!start) {
        start = current;
        last = start;
     } else {
        last->next = current;
        last = current;
     }

     if (child)
       child->GetNextBox(&child);

   }
   aBoxSizes = start;
  }

  nsSprocketLayout::PopulateBoxSizes(aBox, aState, aBoxSizes, aComputedBoxSizes, aMinSize, aMaxSize, aFlexes);
}

void
nsGridRowLeafLayout::ComputeChildSizes(nsIBox* aBox,
                           nsBoxLayoutState& aState, 
                           nscoord& aGivenSize, 
                           nsBoxSize* aBoxSizes, 
                           nsComputedBoxSize*& aComputedBoxSizes)
{ 
  // see if we are in a scrollable frame. If we are then there could be scrollbars present
  // if so we need to subtract them out to make sure our columns line up.
  if (aBox) {

     // go up the parent chain looking for scrollframes
     PRBool isHorizontal = PR_FALSE;
     aBox->GetOrientation(isHorizontal);

     nsIBox* scrollbox = nsnull;
     aBox->GetParentBox(&aBox);
     scrollbox = nsGrid::GetScrollBox(aBox);
       
       nsCOMPtr<nsIScrollableFrame> scrollable = do_QueryInterface(scrollbox);
       if (scrollable) {

          // get the clip rect and compare its size to the scrollframe.
          // we just need to subtract out the difference. 
          nsSize clipSize(0,0);
          scrollable->GetClipSize(nsnull, &clipSize.width, &clipSize.height);

          nscoord diff = 0;

          nsRect ourRect;
          nsMargin padding(0,0,0,0);
          scrollbox->GetBounds(ourRect);
          scrollbox->GetBorderAndPadding(padding);
          ourRect.Deflate(padding);
          scrollbox->GetInset(padding);
          ourRect.Deflate(padding);

          if (isHorizontal) {
            diff = ourRect.width - clipSize.width;
          } else {
            diff = ourRect.height - clipSize.height;
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
          }
       }
  }
      
  nsSprocketLayout::ComputeChildSizes(aBox, aState, aGivenSize, aBoxSizes, aComputedBoxSizes);

}

NS_IMETHODIMP
nsGridRowLeafLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  return nsGridRowLayout::Layout(aBox, aBoxLayoutState);
}

NS_IMETHODIMP
nsGridRowLeafLayout::DirtyRows(nsIBox* aBox, nsBoxLayoutState& aState)
{
  if (aBox) {
    // mark us dirty
    aBox->MarkDirty(aState);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGridRowLeafLayout::CountRowsColumns(nsIBox* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount)
{
  if (aBox) {
    nsIBox* child = nsnull;
    aBox->GetChildBox(&child);

    // count the children
    PRInt32 columnCount = 0;
    while(child) {
      child->GetNextBox(&child);
      columnCount++;
    }

    // if our count is greater than the current column count
    if (columnCount > aComputedColumnCount) 
      aComputedColumnCount = columnCount;

    aRowCount++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGridRowLeafLayout::BuildRows(nsIBox* aBox, nsGridRow* aRows, PRInt32* aCount)
{ 
  if (aBox) {
      aRows[0].Init(aBox, PR_FALSE);
      *aCount = 1;
      return NS_OK;
  }

  *aCount = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsGridRowLeafLayout::GetRowCount(PRInt32& aRowCount)
{
  aRowCount = 1;
  return NS_OK;
}

