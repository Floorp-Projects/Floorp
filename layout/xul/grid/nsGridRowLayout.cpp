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

nsGridRowLayout::~nsGridRowLayout()
{
}

void
nsGridRowLayout::ChildrenInserted(nsIFrame* aBox, nsBoxLayoutState& aState,
                                  nsIFrame* aPrevBox,
                                  const nsFrameList::Slice& aNewChildren)
{
  ChildAddedOrRemoved(aBox, aState);
}

void
nsGridRowLayout::ChildrenAppended(nsIFrame* aBox, nsBoxLayoutState& aState,
                                  const nsFrameList::Slice& aNewChildren)
{
  ChildAddedOrRemoved(aBox, aState);
}

void
nsGridRowLayout::ChildrenRemoved(nsIFrame* aBox, nsBoxLayoutState& aState, nsIFrame* aChildList)
{
  ChildAddedOrRemoved(aBox, aState);
}

void
nsGridRowLayout::ChildrenSet(nsIFrame* aBox, nsBoxLayoutState& aState, nsIFrame* aChildList)
{
  ChildAddedOrRemoved(aBox, aState);
}

nsIGridPart*
nsGridRowLayout::GetParentGridPart(nsIFrame* aBox, nsIFrame** aParentBox)
{
  // go up and find our parent gridRow. Skip and non gridRow
  // parents.
  *aParentBox = nullptr;

  // walk up through any scrollboxes
  aBox = nsGrid::GetScrollBox(aBox);

  // get the parent
  if (aBox)
    aBox = nsBox::GetParentXULBox(aBox);

  if (aBox)
  {
    nsIGridPart* parentGridRow = nsGrid::GetPartFromBox(aBox);
    if (parentGridRow && parentGridRow->CanContain(this)) {
      *aParentBox = aBox;
      return parentGridRow;
    }
  }

  return nullptr;
}


nsGrid*
nsGridRowLayout::GetGrid(nsIFrame* aBox, int32_t* aIndex, nsGridRowLayout* aRequestor)
{

   if (aRequestor == nullptr)
   {
      nsIFrame* parentBox; // nsIFrame is implemented by nsIFrame and is not refcounted.
      nsIGridPart* parent = GetParentGridPart(aBox, &parentBox);
      if (parent)
         return parent->GetGrid(parentBox, aIndex, this);
      return nullptr;
   }

   int32_t index = -1;
   nsIFrame* child = nsBox::GetChildXULBox(aBox);
   int32_t count = 0;
   while(child)
   {
     // if there is a scrollframe walk inside it to its child
     nsIFrame* childBox = nsGrid::GetScrolledBox(child);

     nsBoxLayout* layout = childBox->GetXULLayoutManager();
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

     child = nsBox::GetNextXULBox(child);
   }

   // if we didn't find ourselves then the tree isn't properly formed yet
   // this could happen during initial construction so lets just
   // fail.
   if (index == -1) {
     *aIndex = -1;
     return nullptr;
   }

   (*aIndex) += index;

   nsIFrame* parentBox; // nsIFrame is implemented by nsIFrame and is not refcounted.
   nsIGridPart* parent = GetParentGridPart(aBox, &parentBox);
   if (parent)
     return parent->GetGrid(parentBox, aIndex, this);

   return nullptr;
}

nsMargin
nsGridRowLayout::GetTotalMargin(nsIFrame* aBox, bool aIsHorizontal)
{
  // get our parents margin
  nsMargin margin(0,0,0,0);
  nsIFrame* parent = nullptr;
  nsIGridPart* part = GetParentGridPart(aBox, &parent);
  if (part && parent) {
    // if we are the first or last child walk upward and add margins.

    // make sure we check for a scrollbox
    aBox = nsGrid::GetScrollBox(aBox);

    // see if we have a next to see if we are last
    nsIFrame* next = nsBox::GetNextXULBox(aBox);

    // get the parent first child to see if we are first
    nsIFrame* child = nsBox::GetChildXULBox(parent);

    margin = part->GetTotalMargin(parent, aIsHorizontal);

    // if first or last
    if (child == aBox || next == nullptr) {

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
       if (next != nullptr)
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
  aBox->GetXULMargin(ourMargin);
  margin += ourMargin;

  return margin;
}

NS_IMPL_ADDREF_INHERITED(nsGridRowLayout, nsBoxLayout)
NS_IMPL_RELEASE_INHERITED(nsGridRowLayout, nsBoxLayout)

NS_INTERFACE_MAP_BEGIN(nsGridRowLayout)
  NS_INTERFACE_MAP_ENTRY(nsIGridPart)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGridPart)
NS_INTERFACE_MAP_END_INHERITING(nsBoxLayout)
