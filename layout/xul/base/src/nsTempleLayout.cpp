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

#include "nsTempleLayout.h"
#include "nsIBox.h"
#include "nsCOMPtr.h"

nsresult
NS_NewTempleLayout( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout)
{
  aNewLayout = new nsTempleLayout(aPresShell);

  return NS_OK;
  
} 

nsTempleLayout::nsTempleLayout(nsIPresShell* aPresShell):nsMonumentLayout(aPresShell), mMonuments(nsnull)
{
}

NS_IMETHODIMP
nsTempleLayout::CastToTemple(nsTempleLayout** aTemple)
{
  *aTemple = this;
  return NS_OK;
}

NS_IMETHODIMP
nsTempleLayout::EnscriptionChanged(nsBoxLayoutState& aState, PRInt32 aIndex)
{
  // if a cell changes size. 
  if (mMonuments) {
     nsBoxSizeList* size = mMonuments->GetAt(aIndex);
     if (size)
        size->Desecrate(aState);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTempleLayout::GetMonumentList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSizeList** aList)
{
  if (mMonuments) { 
     *aList = mMonuments;
     return NS_OK;
  }

  *aList = nsnull;

  // run through our children.
  // ask each child for its monument list
  // append the list to our list
  nsIBox* box = nsnull;
  aBox->GetChildBox(&box);
  nsBoxSizeList* current = nsnull;
  nsCOMPtr<nsIBoxLayout> layout;
  while(box) {

    box->GetLayoutManager(getter_AddRefs(layout));

    nsresult rv = NS_OK;
    nsCOMPtr<nsIMonument> monument = do_QueryInterface(layout, &rv);

    if (monument) {

      if (!mMonuments) {
          mMonuments = new nsBoxSizeListImpl(box);
          mMonuments->AddRef();
        }

      current = mMonuments;
      nsBoxSizeList* node = nsnull;
      monument->GetMonumentList(box, aState, &node);

      if (node)
          node->AddRef();

      while(node)
      {
        current->Append(aState, node);
        node->Release(aState);

        nsBoxSizeList* tmp = node->GetNext();
        if (tmp)
          tmp->AddRef();

        node->SetNext(aState, nsnull);
        node = tmp;

        if (node && !current->GetNext()) {
          nsBoxSizeList* newOne = new nsBoxSizeListImpl(box);
          current->SetNext(aState, newOne);
          current = newOne;
        } else {
          current = current->GetNext();
        }
      }
    }
    
    box->GetNextBox(&box);
  }

  *aList = mMonuments;
  return NS_OK;
}

NS_IMETHODIMP
nsTempleLayout::BuildBoxSizeList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aFirst, nsBoxSize*& aLast)
{
  nsIBox* box = nsnull;
  aBox->GetChildBox(&box);

  aFirst = nsnull;
  aLast = nsnull;

  nsBoxSize* first;
  nsBoxSize* last;
  PRInt32 count = 0;
  nsCOMPtr<nsIBoxLayout> layout;
  while(box) {
    nsIMonument* monument = nsnull;
    box->GetLayoutManager(getter_AddRefs(layout));

    if (layout) {
      layout->QueryInterface(NS_GET_IID(nsIMonument), (void**)&monument);
      if (monument) 
           monument->BuildBoxSizeList(box, aState, first, last);
      else {
           nsMonumentLayout::BuildBoxSizeList(box, aState, first, last);
           first->bogus = PR_TRUE;
      }

      if (count == 0)
        aFirst = first;
      else 
        (aLast)->next = first;
      aLast = last;
    }
    
    box->GetNextBox(&box);
    count++;
  }

  /*
  nsMargin borderPadding(0,0,0,0);
  aBox->GetBorderAndPadding(borderPadding);
  nsMargin margin(0,0,0,0);
  aBox->GetMargin(margin);

  nsMargin leftMargin(borderPadding.left + margin.left, borderPadding.top + margin.top, 0, 0);
  nsMargin rightMargin(0,0, borderPadding.right + margin.right, borderPadding.bottom + margin.bottom);

  PRBool isHorizontal = PR_FALSE;
  aBox->GetOrientation(isHorizontal);

  (aFirst)->Add(leftMargin,isHorizontal);
  (aLast)->Add(rightMargin,isHorizontal);
  */

  return NS_OK;
}

NS_IMETHODIMP
nsTempleLayout::ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aPrevBox, nsIBox* aChildList)
{
  DesecrateMonuments(aBox, aState);
  return NS_OK;
}

NS_IMETHODIMP
nsTempleLayout::ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)
{
  DesecrateMonuments(aBox, aState);
  return NS_OK;
}

NS_IMETHODIMP
nsTempleLayout::ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)
{
  DesecrateMonuments(aBox, aState);
  return NS_OK;
}

NS_IMETHODIMP
nsTempleLayout::DesecrateMonuments(nsIBox* aBox, nsBoxLayoutState& aState)
{
  if (mMonuments) {
    nsBoxSizeList* tmp = mMonuments;
    mMonuments = nsnull;
    tmp->Release(aState);
  }

  return NS_OK;
}

