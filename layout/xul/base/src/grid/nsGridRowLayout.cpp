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
