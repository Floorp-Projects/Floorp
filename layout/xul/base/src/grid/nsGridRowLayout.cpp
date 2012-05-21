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

#include "nsGridRowLayout.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsBox.h"
#include "nsStackLayout.h"
#include "nsGrid.h"

nsGridRowLayout::nsGridRowLayout():nsSprocketLayout()
{
}

void
nsGridRowLayout::ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState,
                                  nsIBox* aPrevBox,
                                  const nsFrameList::Slice& aNewChildren)
{
  ChildAddedOrRemoved(aBox, aState);
}

void
nsGridRowLayout::ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState,
                                  const nsFrameList::Slice& aNewChildren)
{
  ChildAddedOrRemoved(aBox, aState);
}

void
nsGridRowLayout::ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)
{
  ChildAddedOrRemoved(aBox, aState);
}

void
nsGridRowLayout::ChildrenSet(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)
{
  ChildAddedOrRemoved(aBox, aState);
}

nsIGridPart*
nsGridRowLayout::GetParentGridPart(nsIBox* aBox, nsIBox** aParentBox)
{
  // go up and find our parent gridRow. Skip and non gridRow
  // parents.
  *aParentBox = nsnull;
  
  // walk up through any scrollboxes
  aBox = nsGrid::GetScrollBox(aBox);

  // get the parent
  if (aBox)
    aBox = aBox->GetParentBox();

  if (aBox)
  {
    nsIGridPart* parentGridRow = nsGrid::GetPartFromBox(aBox);
    if (parentGridRow && parentGridRow->CanContain(this)) {
      *aParentBox = aBox;
      return parentGridRow;
    }
  }

  return nsnull;
}


nsGrid*
nsGridRowLayout::GetGrid(nsIBox* aBox, PRInt32* aIndex, nsGridRowLayout* aRequestor)
{

   if (aRequestor == nsnull)
   {
      nsIBox* parentBox; // nsIBox is implemented by nsIFrame and is not refcounted.
      nsIGridPart* parent = GetParentGridPart(aBox, &parentBox);
      if (parent)
         return parent->GetGrid(parentBox, aIndex, this);
      return nsnull;
   }

   PRInt32 index = -1;
   nsIBox* child = aBox->GetChildBox();
   PRInt32 count = 0;
   while(child)
   {
     // if there is a scrollframe walk inside it to its child
     nsIBox* childBox = nsGrid::GetScrolledBox(child);

     nsBoxLayout* layout = childBox->GetLayoutManager();
     nsIGridPart* gridRow = nsGrid::GetPartFromBox(childBox);
     if (gridRow) 
     {
       if (layout == aRequestor) {
          index = count;
          break;
       }
       count += gridRow->GetRowCount();
     } else 
       count++;

     child = child->GetNextBox();
   }

   // if we didn't find ourselves then the tree isn't properly formed yet
   // this could happen during initial construction so lets just
   // fail.
   if (index == -1) {
     *aIndex = -1;
     return nsnull;
   }

   (*aIndex) += index;

   nsIBox* parentBox; // nsIBox is implemented by nsIFrame and is not refcounted.
   nsIGridPart* parent = GetParentGridPart(aBox, &parentBox);
   if (parent)
     return parent->GetGrid(parentBox, aIndex, this);

   return nsnull;
}

nsMargin
nsGridRowLayout::GetTotalMargin(nsIBox* aBox, bool aIsHorizontal)
{
  // get our parents margin
  nsMargin margin(0,0,0,0);
  nsIBox* parent = nsnull;
  nsIGridPart* part = GetParentGridPart(aBox, &parent);
  if (part && parent) {
    // if we are the first or last child walk upward and add margins.

    // make sure we check for a scrollbox
    aBox = nsGrid::GetScrollBox(aBox);

    // see if we have a next to see if we are last
    nsIBox* next = aBox->GetNextBox();

    // get the parent first child to see if we are first
    nsIBox* child = parent->GetChildBox();

    margin = part->GetTotalMargin(parent, aIsHorizontal);

    // if first or last
    if (child == aBox || next == nsnull) {

       // if it's not the first child remove the top margin
       // we don't need it.
       if (child != aBox)
       {
          if (aIsHorizontal)
              margin.top = 0;
          else 
              margin.left = 0;
       }

       // if it's not the last child remove the bottom margin
       // we don't need it.
       if (next != nsnull)
       {
          if (aIsHorizontal)
              margin.bottom = 0;
          else 
              margin.right = 0;
       }

    }
  }
    
  // add ours to it.
  nsMargin ourMargin;
  aBox->GetMargin(ourMargin);
  margin += ourMargin;

  return margin;
}

NS_IMPL_ADDREF_INHERITED(nsGridRowLayout, nsBoxLayout)
NS_IMPL_RELEASE_INHERITED(nsGridRowLayout, nsBoxLayout)

NS_INTERFACE_MAP_BEGIN(nsGridRowLayout)
  NS_INTERFACE_MAP_ENTRY(nsIGridPart)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGridPart)
NS_INTERFACE_MAP_END_INHERITING(nsBoxLayout)
