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

#include "nsObeliskLayout.h"
#include "nsTempleLayout.h"
#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsIScrollableFrame.h"

nsresult
NS_NewObeliskLayout( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout)
{
  aNewLayout = new nsObeliskLayout(aPresShell);

  return NS_OK;
  
} 

nsObeliskLayout::nsObeliskLayout(nsIPresShell* aPresShell):nsMonumentLayout(aPresShell), mOtherMonumentList(nsnull)
{
  mOtherMonumentList = nsnull;
}

nsObeliskLayout::~nsObeliskLayout()
{
  if (mOtherMonumentList)
    mOtherMonumentList->RemoveListener();
}

NS_IMETHODIMP
nsObeliskLayout::CastToObelisk(nsObeliskLayout** aObelisk)
{
  *aObelisk = this;
  return NS_OK;
}

void
nsObeliskLayout::UpdateMonuments(nsIBox* aBox, nsBoxLayoutState& aState)
{
  if (!mOtherMonumentList)
  {
     GetOtherMonuments(aBox, &mOtherMonumentList);
     if (mOtherMonumentList) {
        // if we fail to set the listener the null out our list.
        // this could happend if someone put more than 1 <columns> or <rows> tags in a grid. This is 
        // technically illegal. But at the moment we can't stop them from doing it.
       PRBool wasSet = mOtherMonumentList->SetListener(aBox, *this);
       NS_ASSERTION(wasSet, "Too many columns or rows!");

       // recover gracefully for the optimized bits.
       if (!wasSet) 
           mOtherMonumentList = nsnull;
     }
  }
}

NS_IMETHODIMP
nsObeliskLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  nsresult rv = nsMonumentLayout::GetPrefSize(aBox, aState, aSize); 

  UpdateMonuments(aBox, aState);

  nsBoxSizeList* node = mOtherMonumentList;

  PRBool isHorizontal = PR_FALSE;
  aBox->GetOrientation(isHorizontal);

  if (node) {
    // if the infos pref width is greater than aSize's use it.
    // if the infos min width is greater than aSize's use it.
    // if the infos max width is smaller than aSizes then set it.
    nsBoxSize size = node->GetBoxSize(aState, isHorizontal);

    nscoord s = size.pref;

    nsMargin bp(0,0,0,0);
    aBox->GetBorderAndPadding(bp);
    if (isHorizontal) {
      s += bp.top + bp.bottom;
    } else {
      s += bp.left + bp.right;
    }
 
    nscoord& s2 = GET_HEIGHT(aSize, isHorizontal);

    if (s > s2)
      s2 = s;
  }

  return rv;
}

NS_IMETHODIMP
nsObeliskLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  //nsresult rv = nsMonumentLayout::GetMinSize(aBox, aState, aSize); 

   PRBool isHorizontal = PR_FALSE;
   aBox->GetOrientation(isHorizontal);

   aSize.width = 0;
   aSize.height = 0;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box

   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);

   // our flexes are determined by the other temple. So in getting out min size we need to 
   // iterator over our temples obelisks.

   nsTempleLayout* temple = nsnull;
   nsIBox* aTempleBox = nsnull;
   GetOtherTemple(aBox, &temple, &aTempleBox);
   NS_IF_RELEASE(temple);

   nsMonumentIterator it(aTempleBox);
   
   while (child) 
   {  
       // ignore collapsed children
      //PRBool isCollapsed = PR_FALSE;
      //aBox->IsCollapsed(aState, isCollapsed);

      //if (!isCollapsed)
      //{
        nsSize min(0,0);
        nsSize pref(0,0);
        nscoord flex = 0;

        child->GetMinSize(aState, min);        

        // get the next obelisk and use its flex.
        nsObeliskLayout* obelisk;
        it.GetNextObelisk(&obelisk, PR_TRUE);
        nsIBox* obeliskBox = nsnull;
        it.GetBox(&obeliskBox);

        if (obeliskBox) {
           obeliskBox->GetFlex(aState, flex);
        } else {
           child->GetFlex(aState, flex);
        }

        // if the child is not flexible then
        // its min size is its pref size.
        if (flex == 0)  {
            child->GetPrefSize(aState, pref);
            if (isHorizontal)
               min.width = pref.width;
            else
               min.height = pref.height;
        }

        AddMargin(child, min);
        AddLargestSize(aSize, min, isHorizontal);
      //}

      child->GetNextBox(&child);
   }

  UpdateMonuments(aBox, aState);

  nsBoxSizeList* node = mOtherMonumentList;

  if (node) {
    // if the infos pref width is greater than aSize's use it.
    // if the infos min width is greater than aSize's use it.
    // if the infos max width is smaller than aSizes then set it.
    nsBoxSize size = node->GetBoxSize(aState, isHorizontal);

    nscoord s = size.min;

    nsMargin bp(0,0,0,0);
    aBox->GetBorderAndPadding(bp);
    if (isHorizontal) {
      s += bp.top + bp.bottom;
    } else {
      s += bp.left + bp.right;
    }

    nscoord& s2 = GET_HEIGHT(aSize, isHorizontal);

    if (s > s2)
      s2 = s;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsObeliskLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  nsresult rv = nsMonumentLayout::GetMaxSize(aBox, aState, aSize); 

  UpdateMonuments(aBox, aState);

  nsBoxSizeList* node = mOtherMonumentList;

  PRBool isHorizontal = PR_FALSE;
  aBox->GetOrientation(isHorizontal);

  if (node) {
   // if the infos pref width is greater than aSize's use it.
    // if the infos min width is greater than aSize's use it.
    // if the infos max width is smaller than aSizes then set it.
    nsBoxSize size = node->GetBoxSize(aState, isHorizontal);

    nscoord s = size.max;
    nscoord& s2 = GET_HEIGHT(aSize, isHorizontal);

    if (s > s2)
      s2 = s;

  }

  return rv;
}

NS_IMETHODIMP
nsObeliskLayout::ChildBecameDirty(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChild)
{
  // if one of our cells has changed size and needs reflow
  // make sure we clean any cached information about it.
  nsCOMPtr<nsIMonument> parent;
  nsCOMPtr<nsIBox> parentBox;
  GetParentMonument(aBox, parentBox, getter_AddRefs(parent));
  if (!parent)
    return NS_OK;
  
  nsIBox* child = nsnull;
  aBox->GetChildBox(&child);
  PRInt32 count = 0;
  nsCOMPtr<nsIBoxLayout> layout;
  while(child)
  {
    if (child == aChild) {
      parent->EnscriptionChanged(aState, count);
      return NS_OK;
    }

    child->GetNextBox(&child);
    count++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsObeliskLayout::BecameDirty(nsIBox* aBox, nsBoxLayoutState& aState)
{
  /*
  nsCOMPtr<nsIMonument> parent;
  nsCOMPtr<nsIBox> parentBox;
  GetParentMonument(aBox, parentBox, getter_AddRefs(parent));
  parent->DesecrateMonuments(aBox, aState);
  */

  UpdateMonuments(aBox, aState);
  if (mOtherMonumentList)
     mOtherMonumentList->MarkDirty(aState);

  return NS_OK;
}

void
nsObeliskLayout::PopulateBoxSizes(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aBoxSizes, nsComputedBoxSize*& aComputedBoxSizes, nscoord& aMinSize, nscoord& aMaxSize, PRInt32& aFlexes)
{
  nsTempleLayout* temple = nsnull;
  nsIBox* aTempleBox = nsnull;
  GetOtherTemple(aBox, &temple, &aTempleBox);
  if (temple) {
     // substitute our sizes for the other temples obelisk sizes.
     PRBool isHorizontal = PR_FALSE;
     aTempleBox->GetOrientation(isHorizontal);
     nsBoxSize* first = nsnull;
     nsBoxSize* last = nsnull;
     temple->BuildBoxSizeList(aTempleBox, aState, first, last, isHorizontal);
     aBoxSizes = first;
  }

  nsSprocketLayout::PopulateBoxSizes(aBox, aState, aBoxSizes, aComputedBoxSizes, aMinSize, aMaxSize, aFlexes);
  NS_IF_RELEASE(temple);
}

void
nsObeliskLayout::ComputeChildSizes(nsIBox* aBox,
                           nsBoxLayoutState& aState, 
                           nscoord& aGivenSize, 
                           nsBoxSize* aBoxSizes, 
                           nsComputedBoxSize*& aComputedBoxSizes)
{  
  nsCOMPtr<nsIBoxLayout> layout;
  nsCOMPtr<nsIMonument> parentMonument;
  nsCOMPtr<nsIScrollableFrame> scrollable;

  nsresult rv = NS_OK;
  aBox->GetParentBox(&aBox);
  nscoord size = aGivenSize;
  
  while (aBox) {
    aBox->GetLayoutManager(getter_AddRefs(layout));
    parentMonument = do_QueryInterface(layout, &rv);
    if (NS_SUCCEEDED(rv) && parentMonument) {
      // we have a parent monument good. Go up until we hit the grid.
      nsGridLayout* grid;
      parentMonument->CastToGrid(&grid);
      if (grid) {
       if (size > aGivenSize) {
          nscoord diff = size - aGivenSize;
          aGivenSize += diff;
          nsSprocketLayout::ComputeChildSizes(aBox, aState, aGivenSize, aBoxSizes, aComputedBoxSizes);
          nsComputedBoxSize* s = aComputedBoxSizes;
          nsComputedBoxSize* last = aComputedBoxSizes;
          while(s)
          {
            last = s;
            s = s->next;
          }
    
          last->size -= diff;
          aGivenSize -= diff;
       } else {
          nsSprocketLayout::ComputeChildSizes(aBox, aState, aGivenSize, aBoxSizes, aComputedBoxSizes);
       }
       return;
      }
    } else {
       scrollable = do_QueryInterface(aBox, &rv);
       if (NS_SUCCEEDED(rv)) {
         // oops we are in a scrollable. Did it change our size?
         // if so remove the excess space.
         nsRect r;
         aBox->GetBounds(r);
         PRBool isHorizontal = PR_FALSE;
         aBox->GetOrientation(isHorizontal);

         if (size < GET_WIDTH(r, isHorizontal)) {
            if (isHorizontal) {
               size = r.width;
            } else {
               size = r.height;
            }
         }
       }
    }
    aBox->GetParentBox(&aBox);
  }

  // Not in GRID!!! do the default.
  nsSprocketLayout::ComputeChildSizes(aBox, aState, aGivenSize, aBoxSizes, aComputedBoxSizes);
}

void 
nsObeliskLayout::WillBeDestroyed(nsIBox* aBox, nsBoxLayoutState& aState,  nsBoxSizeList& aList)
{
   Desecrated(aBox, aState, aList);
   mOtherMonumentList = nsnull;
}

void 
nsObeliskLayout::Desecrated(nsIBox* aBox, nsBoxLayoutState& aState,  nsBoxSizeList& aList)
{
  NS_ASSERTION(&aList == mOtherMonumentList,"Wrong list!!");
  if (mOtherMonumentList) {
    nsCOMPtr<nsIBoxLayout> layout;
    aBox->GetLayoutManager(getter_AddRefs(layout));
    aBox->SetLayoutManager(nsnull);
    aBox->MarkDirtyChildren(aState);
    aBox->SetLayoutManager(layout);
  }
}

NS_IMETHODIMP
nsObeliskLayout::ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aPrevBox, nsIBox* aChildList)
{
  nsCOMPtr<nsIMonument> parent;
  nsCOMPtr<nsIBox> parentBox;
  GetParentMonument(aBox, parentBox, getter_AddRefs(parent));
  if (parent) 
     return parent->DesecrateMonuments(aBox, aState);
  else
     return NS_OK;
}

NS_IMETHODIMP
nsObeliskLayout::ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)
{
  nsCOMPtr<nsIMonument> parent;
  nsCOMPtr<nsIBox> parentBox;
  GetParentMonument(aBox, parentBox, getter_AddRefs(parent));
  if (parent) 
     return parent->DesecrateMonuments(aBox, aState);
  else
     return NS_OK;
}

NS_IMETHODIMP
nsObeliskLayout::ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)
{
  nsCOMPtr<nsIMonument> parent;
  nsCOMPtr<nsIBox> parentBox;
  GetParentMonument(aBox, parentBox, getter_AddRefs(parent));
  if (parent) 
     return parent->DesecrateMonuments(aBox, aState);
  else
     return NS_OK;
}
