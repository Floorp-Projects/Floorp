/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
  //nscoord totalWidth = 0;

  if (node) {
   // if the infos pref width is greater than aSize's use it.
    // if the infos min width is greater than aSize's use it.
    // if the infos max width is smaller than aSizes then set it.
    nsBoxSize size = node->GetBoxSize(aState);

    nscoord s = size.pref;
    nscoord& s2 = GET_HEIGHT(aSize, isHorizontal);

    if (s > s2)
      s2 = s;

  }

  return rv;
}

NS_IMETHODIMP
nsObeliskLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  nsresult rv = nsMonumentLayout::GetMinSize(aBox, aState, aSize); 

  UpdateMonuments(aBox, aState);

  nsBoxSizeList* node = mOtherMonumentList;

  PRBool isHorizontal = PR_FALSE;
  aBox->GetOrientation(isHorizontal);

  if (node) {
    // if the infos pref width is greater than aSize's use it.
    // if the infos min width is greater than aSize's use it.
    // if the infos max width is smaller than aSizes then set it.
    nsBoxSize size = node->GetBoxSize(aState);

    nscoord s = size.min;
    nscoord& s2 = GET_HEIGHT(aSize, isHorizontal);

    if (s > s2)
      s2 = s;
  }

  return rv;
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
    nsBoxSize size = node->GetBoxSize(aState);

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
     nsBoxSize* first = nsnull;
     nsBoxSize* last = nsnull;
     temple->BuildBoxSizeList(aTempleBox, aState, first, last);
     aBoxSizes = first;
  }/* else { */
    nsSprocketLayout::PopulateBoxSizes(aBox, aState, aBoxSizes, aComputedBoxSizes, aMinSize, aMaxSize, aFlexes);
   // return;
 // }

  /*
  aMinSize = 0;
  aMaxSize = NS_INTRINSICSIZE;

  PRBool isHorizontal = PR_FALSE;
  aBox->GetOrientation(isHorizontal);
  aFlexes = 0;

  nsIBox* child = nsnull;
  aBox->GetChildBox(&child);

  if (child && !aBoxSizes)
    aBoxSizes = new (aState) nsBoxSize();

  nsBoxSize* childSize = aBoxSizes;

  while(child)
  {
    nscoord flex = 0;
    child->GetFlex(aState, flex);
    
    if (flex > 0) 
       aFlexes++;

    nsSize pref(0,0);
    nsSize min(0,0);
    nsSize max(0,0);

    nscoord ascent = 0;
    child->GetPrefSize(aState, pref);
    child->GetAscent(aState, ascent);
    nsMargin margin;
    child->GetMargin(margin);
    child->GetMinSize(aState, min);
    child->GetMaxSize(aState, max);
    nsBox::BoundsCheck(min, pref, max);

    AddMargin(child, pref);
    AddMargin(child, min);
    AddMargin(child, max);

      if (!isHorizontal) {
        if (min.width > aMinSize)
          aMinSize = min.width;

        if (max.width < aMaxSize)
          aMaxSize = max.width;

      } else {
        if (min.height > aMinSize)
          aMinSize = min.height;

        if (max.height < aMaxSize)
          aMaxSize = max.height;
      }


    child->GetNextBox(&child);
    if (child && !childSize->next)
    {
      childSize->next = new (aState) nsBoxSize();
      childSize = childSize->next;
    }
  }
  */

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
    aBox->MarkDirty(aState);
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
