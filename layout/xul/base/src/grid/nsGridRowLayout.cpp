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

#include "nsGridRowLayout.h"
#include "nsBoxLayoutState.h"
#include "nsIBox.h"
#include "nsIScrollableFrame.h"
#include "nsBox.h"
#include "nsStackLayout.h"

nsGridRowLayout::nsGridRowLayout(nsIPresShell* aPresShell):nsSprocketLayout()
{
}

NS_IMETHODIMP
nsGridRowLayout::ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aPrevBox, nsIBox* aChildList)
{
  return ChildAddedOrRemoved(aBox, aState);
}

NS_IMETHODIMP
nsGridRowLayout::ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)
{
  return ChildAddedOrRemoved(aBox, aState);
}

NS_IMETHODIMP
nsGridRowLayout::ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)
{
  return ChildAddedOrRemoved(aBox, aState);
}

NS_IMETHODIMP
nsGridRowLayout::ChildrenSet(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)
{
  return ChildAddedOrRemoved(aBox, aState);
}

NS_IMETHODIMP
nsGridRowLayout::GetParentGridPart(nsIBox* aBox, nsCOMPtr<nsIBox>& aParentBox, nsIGridPart** aParentGridPart)
{
  // go up and find our parent gridRow. Skip and non gridRow
  // parents.
  nsCOMPtr<nsIBoxLayout> layout;
  nsCOMPtr<nsIGridPart> parentGridRow;
  nsresult rv = NS_OK;
  *aParentGridPart = nsnull;
  aBox->GetParentBox(&aBox);
  
  while (aBox) {
    aBox->GetLayoutManager(getter_AddRefs(layout));
    parentGridRow = do_QueryInterface(layout, &rv);
    if (NS_SUCCEEDED(rv) && parentGridRow) {
      aParentBox = aBox;
      *aParentGridPart = parentGridRow.get();
      NS_IF_ADDREF(*aParentGridPart);
      return rv;
    }
    aBox->GetParentBox(&aBox);
  }

  aParentBox = nsnull;
  *aParentGridPart = nsnull;
  return rv;
}


NS_IMETHODIMP
nsGridRowLayout::GetGrid(nsIBox* aBox, nsGrid** aList, PRInt32* aIndex, nsGridRowLayout* aRequestor)
{

   if (aRequestor == nsnull)
   {
      nsCOMPtr<nsIGridPart> parent;
      nsCOMPtr<nsIBox> parentBox;
      GetParentGridPart(aBox, parentBox, getter_AddRefs(parent));
      if (parent)
         return parent->GetGrid(parentBox, aList, aIndex, this);
      else
         return NS_OK;
   }

   nsresult rv = NS_OK;

   PRInt32 index = -1;
   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   PRInt32 count = 0;
   while(child)
   {
     // if there is a scrollframe walk inside it to its child
     nsCOMPtr<nsIBox> childBox = child;
     nsCOMPtr<nsIScrollableFrame> scrollFrame = do_QueryInterface(child, &rv);
     if (scrollFrame) {
       nsIFrame* childFrame = nsnull;
       scrollFrame->GetScrolledFrame(nsnull, childFrame);
       if (!childFrame)
         return NS_ERROR_FAILURE;
       childBox = do_QueryInterface(childFrame);
     }

     nsCOMPtr<nsIBoxLayout> layout;
     childBox->GetLayoutManager(getter_AddRefs(layout));
     
     // find our requester
     nsCOMPtr<nsIGridPart> gridRow = do_QueryInterface(layout, &rv);
     if (NS_SUCCEEDED(rv) && gridRow) 
     {
       if (layout == aRequestor) {
          index = count;
          break;
       }
     }

     // add the index. 
     count++;
     child->GetNextBox(&child);
   }

   // if we didn't find ourselves then the tree isn't properly formed yet
   // this could happen during initial construction so lets just
   // fail.
   if (index == -1) {
     *aList = nsnull;
     *aIndex = -1;
     return NS_OK;
   }

   (*aIndex) += index;

   nsCOMPtr<nsIGridPart> parent;
   nsCOMPtr<nsIBox> parentBox;
   GetParentGridPart(aBox, parentBox, getter_AddRefs(parent));

   if (parent)
      parent->GetGrid(parentBox, aList, aIndex, this);

   return NS_OK;
}

NS_IMETHODIMP
nsGridRowLayout::CastToRowGroupLayout(nsGridRowGroupLayout** aRowGroup)
{
  (*aRowGroup) = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGridRowLayout::CastToGridLayout(nsGridLayout2** aGridLayout)
{
  (*aGridLayout) = nsnull;
  return NS_OK;
}

NS_IMPL_ADDREF_INHERITED(nsGridRowLayout, nsBoxLayout);
NS_IMPL_RELEASE_INHERITED(nsGridRowLayout, nsBoxLayout);

NS_INTERFACE_MAP_BEGIN(nsGridRowLayout)
  NS_INTERFACE_MAP_ENTRY(nsIGridPart)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGridPart)
NS_INTERFACE_MAP_END_INHERITING(nsBoxLayout)
