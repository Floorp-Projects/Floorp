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
nsGridRowLeafLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  PRBool isHorizontal = IsHorizontal(aBox);

  // If we are not in a grid. Then we just work like a box. But if we are in a grid
  // ask the grid for our size.
  if (!grid)
    return nsGridRowLayout::GetPrefSize(aBox, aState, aSize); 
  else {
    aSize = grid->GetPrefRowSize(aState, index, isHorizontal);
    //AddBorderAndPadding(aBox, aSize);
    return NS_OK;
  }
}

NS_IMETHODIMP
nsGridRowLeafLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  PRBool isHorizontal = IsHorizontal(aBox);

  if (!grid)
    return nsGridRowLayout::GetMinSize(aBox, aState, aSize); 
  else {
    aSize = grid->GetMinRowSize(aState, index, isHorizontal);
    AddBorderAndPadding(aBox, aSize);
    return NS_OK;
  }
}

NS_IMETHODIMP
nsGridRowLeafLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  PRBool isHorizontal = IsHorizontal(aBox);

  if (!grid)
    return nsGridRowLayout::GetMaxSize(aBox, aState, aSize); 
  else {
    aSize = grid->GetMaxRowSize(aState, index, isHorizontal);
    AddBorderAndPadding(aBox, aSize);
    return NS_OK;
  }
}

/** If a child is added or removed or changes size
  */
void
nsGridRowLeafLayout::ChildAddedOrRemoved(nsIBox* aBox, nsBoxLayoutState& aState)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  PRBool isHorizontal = IsHorizontal(aBox);

  if (grid)
    grid->CellAddedOrRemoved(aState, index, isHorizontal);
}

void
nsGridRowLeafLayout::PopulateBoxSizes(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aBoxSizes, nsComputedBoxSize*& aComputedBoxSizes, nscoord& aMinSize, nscoord& aMaxSize, PRInt32& aFlexes)
{
  PRInt32 index = 0;
  nsGrid* grid = GetGrid(aBox, &index);
  PRBool isHorizontal = IsHorizontal(aBox);

  // Our base class SprocketLayout is giving us a chance to change the box sizes before layout
  // If we are a row lets change the sizes to match our columns. If we are a column then do the opposite
  // and make them match or rows.
  if (grid) {
   nsGridRow* column;
   PRInt32 count = grid->GetColumnCount(isHorizontal); 
   nsBoxSize* start = nsnull;
   nsBoxSize* last = nsnull;
   nsBoxSize* current = nsnull;
   nsIBox* child = aBox->GetChildBox();
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
     nsIBox* box = column->GetBox();
     PRBool collapsed = PR_FALSE;
     nscoord topMargin = column->mTopMargin;
     nscoord bottomMargin = column->mBottomMargin;

     if (box) 
       collapsed = box->IsCollapsed(aState);

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
      nsGridRow* firstRow = nsnull;
      nsGridRow* lastRow = nsnull;
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
     nsBox::BoundsCheck(min, pref, max);
   
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

     if (child)
       child = child->GetNextBox();

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
     aBox = aBox->GetParentBox();
     nsIBox* scrollbox = nsGrid::GetScrollBox(aBox);
       
       nsCOMPtr<nsIScrollableFrame> scrollable = do_QueryInterface(scrollbox);
       if (scrollable) {
          nsMargin scrollbarSizes = scrollable->GetActualScrollbarSizes();

          nsRect ourRect(scrollbox->GetRect());
          nsMargin padding(0,0,0,0);
          scrollbox->GetBorderAndPadding(padding);
          ourRect.Deflate(padding);

          nscoord diff;
          if (aBox->IsHorizontal()) {
            diff = scrollbarSizes.left + scrollbarSizes.right;
          } else {
            diff = scrollbarSizes.top + scrollbarSizes.bottom;
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

void
nsGridRowLeafLayout::DirtyRows(nsIBox* aBox, nsBoxLayoutState& aState)
{
  if (aBox) {
    // mark us dirty
    aBox->AddStateBits(NS_FRAME_IS_DIRTY);
    // XXXldb We probably don't want to walk up the ancestor chain
    // calling MarkIntrinsicWidthsDirty for every row.
    aState.PresShell()->FrameNeedsReflow(aBox, nsIPresShell::eTreeChange);
  }
}

void
nsGridRowLeafLayout::CountRowsColumns(nsIBox* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount)
{
  if (aBox) {
    nsIBox* child = aBox->GetChildBox();

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
nsGridRowLeafLayout::BuildRows(nsIBox* aBox, nsGridRow* aRows)
{ 
  if (aBox) {
      aRows[0].Init(aBox, PR_FALSE);
      return 1;
  }

  return 0;
}

