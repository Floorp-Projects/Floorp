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

NS_IMETHODIMP
nsObeliskLayout::CastToObelisk(nsObeliskLayout** aObelisk)
{
  *aObelisk = this;
  return NS_OK;
}

void
nsObeliskLayout::UpdateMonuments(nsIBox* aBox, nsBoxLayoutState& aState)
{
  if (!mOtherMonumentList || mOtherMonumentList->GetRefCount() == 1)
  {
    if (mOtherMonumentList) {
      mOtherMonumentList->Release(aState);
    }

     mOtherMonumentList = nsnull;
     GetOtherMonuments(aBox, &mOtherMonumentList);
     if (mOtherMonumentList)
        mOtherMonumentList->AddRef();
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

  // for each info
  while(node)
  {
    // if the infos pref width is greater than aSize's use it.
    // if the infos min width is greater than aSize's use it.
    // if the infos max width is smaller than aSizes then set it.
    nsBoxSize size = node->GetBoxSize(aState);

    nscoord s = size.pref;
    nscoord& s2 = GET_HEIGHT(aSize, isHorizontal);

    if (s > s2)
      s2 = s;

    node = node->GetNext();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsObeliskLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  nsresult rv = nsMonumentLayout::GetMinSize(aBox, aState, aSize); 

  UpdateMonuments(aBox, aState);

  nsBoxSizeList* node = mOtherMonumentList;

  PRBool isHorizontal = PR_FALSE;
  aBox->GetOrientation(isHorizontal);

  // for each info
  while(node)
  {
    // if the infos pref width is greater than aSize's use it.
    // if the infos min width is greater than aSize's use it.
    // if the infos max width is smaller than aSizes then set it.
    nsBoxSize size = node->GetBoxSize(aState);

    nscoord s = size.min;
    nscoord& s2 = GET_HEIGHT(aSize, isHorizontal);

    if (s > s2)
      s2 = s;

    node = node->GetNext();
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

  // for each info
  while(node)
  {
    // if the infos pref width is greater than aSize's use it.
    // if the infos min width is greater than aSize's use it.
    // if the infos max width is smaller than aSizes then set it.
    nsBoxSize size = node->GetBoxSize(aState);

    nscoord s = size.max;
    nscoord& s2 = GET_HEIGHT(aSize, isHorizontal);

    if (s > s2)
      s2 = s;

    node = node->GetNext();
  }

  return NS_OK;
}

void
nsObeliskLayout::ChildNeedsLayout(nsIBox* aBox, nsIBoxLayout* aChild)
{
  // if one of our cells has changed size and needs reflow
  // make sure we clean any cached information about it.
  nsIBoxLayout* layout;
  GetParentLayout(aBox, &layout);
  nsTempleLayout* parent = nsnull;
  nsIMonument* monument = nsnull;

  nsIBox* child = nsnull;
  aBox->GetChildBox(&child);
  PRInt32 count = 0;
  while(child)
  {
    nsIBoxLayout* layout = nsnull;
    child->GetLayoutManager(&layout);
    if (layout && NS_SUCCEEDED(layout->QueryInterface(NS_GET_IID(nsIMonument), (void**)&monument)) && monument) 
    {
      if (layout == aChild) {
        parent->EncriptionChanged(count);
        return;
      }
    }
    child->GetNextBox(&child);
    count++;
  }
}

void
nsObeliskLayout::ComputeChildSizes(nsIBox* aBox, 
                                   nsBoxLayoutState& aState, 
                                   nscoord& aGivenSize, 
                                   nsBoxSize* aBoxSizes, 
                                   nsComputedBoxSize* aComputedBoxSizes)
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
  }

  nsSprocketLayout::ComputeChildSizes(aBox, aState, aGivenSize, aBoxSizes, aComputedBoxSizes);  
}


