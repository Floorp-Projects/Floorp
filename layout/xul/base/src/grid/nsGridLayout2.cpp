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
        nscoord height = mGrid.GetMinRowHeight(aState, i, PR_TRUE); 
        AddWidth(total, height, PR_FALSE); // AddHeight
      }
    }

    if (!columnsBox) {
      // max height is the sum of our rows
      PRInt32 columns = mGrid.GetColumnCount();
      for (PRInt32 i=0; i < columns; i++)
      {
        nscoord width = mGrid.GetMinRowHeight(aState, i, PR_FALSE); 
        AddWidth(total, width, PR_TRUE); // AddWidth
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
        nscoord height = mGrid.GetPrefRowHeight(aState, i, PR_TRUE); 
        AddWidth(total, height, PR_FALSE); // AddHeight
      }
    }

    if (!columnsBox) {
      // max height is the sum of our rows
      PRInt32 columns = mGrid.GetColumnCount();
      for (PRInt32 i=0; i < columns; i++)
      {
        nscoord width = mGrid.GetPrefRowHeight(aState, i, PR_FALSE);
        AddWidth(total, width, PR_TRUE); // AddWidth
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
        nscoord height = mGrid.GetMaxRowHeight(aState, i, PR_TRUE); 
        AddWidth(total, height, PR_FALSE); // AddHeight
      }
    }

    if (!columnsBox) {
      total.width = 0;
      // max height is the sum of our rows
      PRInt32 columns = mGrid.GetColumnCount();
      for (PRInt32 i=0; i < columns; i++)
      {
        nscoord width = mGrid.GetMaxRowHeight(aState, i, PR_FALSE);
        AddWidth(total, width, PR_TRUE); // AddWidth
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
    aRows[0].Init(aBox, PR_TRUE);
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
