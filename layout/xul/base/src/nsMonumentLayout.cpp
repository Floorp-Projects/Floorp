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

#include "nsMonumentLayout.h"
#include "nsBoxLayoutState.h"
#include "nsIBox.h"
#include "nsBox.h"

//------ nsInfoListNodeImpl ----

void
nsBoxSizeListNodeImpl::Release(nsBoxLayoutState& aState)
{
   mRefCount--;
   if (mRefCount == 0) {
     Clear(aState);
     if (mNext)
       mNext->Release(aState);
     delete this;
   }
}

void
nsBoxSizeListNodeImpl::Desecrate(nsBoxLayoutState& aState)
{
  if (mParent)
    mParent->Desecrate(aState);
}

void
nsBoxSizeListNodeImpl::MarkDirty(nsBoxLayoutState& aState)
{
  mBox->MarkDirty(aState);
}

void nsBoxSizeListNodeImpl::SetNext(nsBoxLayoutState& aState, nsBoxSizeList* aNext)
{
  if (mNext) {
    mNext->Release(aState);
  }

  mNext = aNext;
  if (mNext)
    aNext->AddRef();
}

void
nsBoxSizeListNodeImpl::Append(nsBoxLayoutState& aState, nsBoxSizeList* aChild)
{
  NS_ERROR("Attept at append to a leaf");
}

nsBoxSizeListNodeImpl::nsBoxSizeListNodeImpl(nsIBox* aBox):mNext(nsnull), 
                                                           mParent(nsnull), 
                                                           mRefCount(0),
                                                           mBox(aBox),
                                                           mIsSet(PR_FALSE)
{
}


nsBoxSizeList* 
nsBoxSizeListNodeImpl::GetAt(PRInt32 aIndex)
{
   nsBoxSizeList* node = this;
   PRInt32 count = 0;
   while(node)
   {
     if (count == aIndex)
        return node;

     node = node->GetNext();
     count++;
   }

   return nsnull;
}

nsBoxSizeList* 
nsBoxSizeListNodeImpl::Get(nsIBox* aBox)
{
   nsBoxSizeList* node = this;
   while(node)
   {
     if (node->GetBox() == aBox)
        return node;

     node = node->GetNext();
   }

   return nsnull;
}

//------ nsInfoListImpl2 ----

nsBoxSizeListImpl::nsBoxSizeListImpl(nsIBox* aBox):nsBoxSizeListNodeImpl(aBox),mListenerBox(nsnull), mListener(nsnull), mFirst(nsnull),mLast(nsnull),mCount(0)
{
}

void
nsBoxSizeListImpl::Release(nsBoxLayoutState& aState)
{
   if (mRefCount == 1) {
     if (mListener)
       mListener->WillBeDestroyed(mListenerBox, aState, *this);
   }

   nsBoxSizeListNodeImpl::Release(aState);
}

void
nsBoxSizeListImpl::Clear(nsBoxLayoutState& aState)
{
  nsBoxSizeList* list = mFirst;
  while(list)
  {
    nsBoxSizeList* toRelease = list;
    list = list->GetNext();
    toRelease->Release(aState);
  }

  mFirst = nsnull;
  mLast = nsnull;
}

void
nsBoxSizeListImpl::Append(nsBoxLayoutState& aState, nsBoxSizeList* aChild)
{
  if (!mFirst) 
      mFirst = aChild;
  else 
      mLast->SetNext(aState, aChild);

  mLast = aChild;
  aChild->SetParent(this);
  aChild->AddRef();
}

void
nsBoxSizeListImpl::Desecrate(nsBoxLayoutState& aState)
{
  if (mIsSet) {
    mIsSet = PR_FALSE;
    if (mListener)
       mListener->Desecrated(mListenerBox, aState, *this);

    nsBoxSizeListNodeImpl::Desecrate(aState);
  }
}

void
nsBoxSizeListImpl::MarkDirty(nsBoxLayoutState& aState)
{
   nsBoxSizeList* child = mFirst;
   while(child)
   {
     child->MarkDirty(aState);
     child = child->GetNext();
   }

   nsBoxSizeListNodeImpl::MarkDirty(aState);
}

PRBool
nsBoxSizeListImpl::SetListener(nsIBox* aBox, nsBoxSizeListener& aListener)
{
  if (mListener)
    return PR_FALSE;

  mListener = &aListener;
  mListenerBox = aBox;
  return PR_TRUE;
}

void
nsBoxSizeListImpl::RemoveListener()
{
  mListener = nsnull;
  mListenerBox = nsnull;
}

nsBoxSize
nsBoxSizeListImpl::GetBoxSize(nsBoxLayoutState& aState)
{
  if (!mIsSet) {

    mIsSet = PR_TRUE;

    mBoxSize.Clear();
    
    nsBoxSizeList* node = mFirst;

    while(node) {
      nsBoxSize size = node->GetBoxSize(aState);

      if (size.pref > mBoxSize.pref)
         mBoxSize.pref = size.pref;

      if (size.min > mBoxSize.min)
         mBoxSize.min = size.min;

      if (mBoxSize.max != NS_INTRINSICSIZE)
        if (size.max == NS_INTRINSICSIZE)
            mBoxSize.max = size.max;
        else 
            mBoxSize.max += size.max;
        
      mBoxSize.flex = size.flex;
      mBoxSize.ascent = size.ascent;

      node = node->GetNext();
    }
  }

  return mBoxSize;
}

nsBoxSize
nsBoxSizeListNodeImpl::GetBoxSize(nsBoxLayoutState& aState)
{
  nsBoxSize size;
  PRBool isHorizontal = PR_FALSE;
  mBox->GetOrientation(isHorizontal);

  nsSize pref(0,0);
  nsSize min(0,0);
  nsSize max(NS_INTRINSICSIZE,NS_INTRINSICSIZE);
  nscoord ascent = 0;
  nscoord flex = 0;

  mBox->GetPrefSize(aState, pref);
  mBox->GetMinSize(aState, min);
  mBox->GetMaxSize(aState, max);
  mBox->GetAscent(aState, ascent);
  mBox->GetFlex(aState, flex);
  nsBox::AddMargin(mBox, pref);

  size.Add(min, pref, max, ascent, flex, isHorizontal); 

  return size;
}

// ------ nsMonumentLayout ------

nsMonumentLayout::nsMonumentLayout(nsIPresShell* aPresShell):nsSprocketLayout()
{
}


NS_IMETHODIMP
nsMonumentLayout::CastToTemple(nsTempleLayout** aTemple)
{
  *aTemple = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMonumentLayout::CastToObelisk(nsObeliskLayout** aObelisk)
{
  *aObelisk = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMonumentLayout::GetParentMonument(nsIBox* aBox, nsCOMPtr<nsIBox>& aParentBox, nsIMonument** aParentMonument)
{
  // go up and find our parent monument. Skip and non monument
  // parents.
  nsCOMPtr<nsIBoxLayout> layout;
  nsCOMPtr<nsIMonument> parentMonument;
  nsresult rv = NS_OK;
  *aParentMonument = nsnull;
  aBox->GetParentBox(&aBox);
  
  while (aBox) {
    aBox->GetLayoutManager(getter_AddRefs(layout));
    parentMonument = do_QueryInterface(layout, &rv);
    if (NS_SUCCEEDED(rv) && parentMonument) {
      aParentBox = aBox;
      *aParentMonument = parentMonument.get();
      NS_IF_ADDREF(*aParentMonument);
      return rv;
    }
    aBox->GetParentBox(&aBox);
  }

  aParentBox = nsnull;
  *aParentMonument = nsnull;
  return rv;
}

NS_IMETHODIMP
nsMonumentLayout::GetOtherMonuments(nsIBox* aBox, nsBoxSizeList** aList)
{
  nsCOMPtr<nsIMonument> parent;
  nsCOMPtr<nsIBox> parentBox;
  GetParentMonument(aBox, parentBox, getter_AddRefs(parent));
  return parent->GetOtherMonumentsAt(parentBox, 0, aList, this);
}

/**
 * Get the monuments in the other temple at the give monument index
 */
NS_IMETHODIMP
nsMonumentLayout::GetOtherMonumentsAt(nsIBox* aBox, PRInt32 aIndexOfObelisk, nsBoxSizeList** aList, nsMonumentLayout* aRequestor)
{
   PRInt32 index = -1;
   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   PRInt32 count = 0;
   while(child)
   {
     nsIBoxLayout* layout = nsnull;
     child->GetLayoutManager(&layout);
     nsresult rv = NS_OK;
     // only all monuments
     nsCOMPtr<nsIMonument> monument = do_QueryInterface(layout, &rv);
     if (NS_SUCCEEDED(rv) && monument) 
     {
       if (layout == aRequestor) {
          index = count;
          break;
       }
       count++;
     }
     child->GetNextBox(&child);
   }

   NS_ASSERTION(index != -1,"Error can't find requestor!!");
   aIndexOfObelisk += index;

   nsCOMPtr<nsIMonument> parent;
   nsCOMPtr<nsIBox> parentBox;
   GetParentMonument(aBox, parentBox, getter_AddRefs(parent));

   parent->GetOtherMonumentsAt(parentBox, aIndexOfObelisk, aList, this);
   return NS_OK;
}

NS_IMETHODIMP
nsMonumentLayout::GetOtherTemple(nsIBox* aBox, nsTempleLayout** aTemple, nsIBox** aTempleBox, nsMonumentLayout* aRequestor)
{
   nsCOMPtr<nsIMonument> parent;
   nsCOMPtr<nsIBox> parentBox;
   GetParentMonument(aBox, parentBox, getter_AddRefs(parent));
   parent->GetOtherTemple(parentBox, aTemple, aTempleBox, this);
   return NS_OK;
}

NS_IMETHODIMP
nsMonumentLayout::GetMonumentsAt(nsIBox* aBox, PRInt32 aMonumentIndex, nsBoxSizeList** aList)
{
  nsBoxLayoutState state((nsIPresContext*)nsnull);

  nsBoxSizeList* list = nsnull;

  GetMonumentList(aBox, state, &list);

  // create an info list for the given column.
  PRInt32 count = 0;
  while(list) {
     if (count == aMonumentIndex)
     {
        *aList = list;
        return NS_OK;
     }
     list = list->GetNext();
     count++;
  }

  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsMonumentLayout::BuildBoxSizeList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aFirst, nsBoxSize*& aLast)
{
   aFirst = aLast = new (aState) nsBoxSize();

   nsSize pref(0,0);
   nsSize min(0,0);
   nsSize max(NS_INTRINSICSIZE,NS_INTRINSICSIZE);
   nscoord flex = 0;
   nscoord ascent = 0;

   aBox->GetPrefSize(aState, pref);
   aBox->GetMinSize(aState, min);
   aBox->GetMaxSize(aState, max);
   aBox->GetAscent(aState, ascent);
   aBox->GetFlex(aState, flex);
   nsBox::BoundsCheck(min, pref, max);
   nsMargin borderPadding(0,0,0,0);
   aBox->GetBorderAndPadding(borderPadding);

   nsMargin margin(0,0,0,0);
   aBox->GetMargin(margin);

   PRBool isHorizontal = PR_FALSE;
   aBox->GetOrientation(isHorizontal);

   (aFirst)->Add(min, pref, max, ascent, flex, !isHorizontal); 
   (aFirst)->Add(borderPadding,!isHorizontal);
   (aFirst)->Add(margin,!isHorizontal);

   return NS_OK;
}

NS_IMETHODIMP
nsMonumentLayout::GetMonumentList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSizeList** aList)
{
  nsIBox* child = nsnull;
  aBox->GetChildBox(&child);
  nsBoxSizeList* last = nsnull;
  while(child)
  {
    nsBoxSizeList* newOne = new nsBoxSizeListNodeImpl(child);
    if (*aList == nsnull) 
      *aList = last = newOne;
    else {
      last->SetNext(aState, newOne);
      last = newOne;
    }
    child->GetNextBox(&child);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMonumentLayout::EnscriptionChanged(nsBoxLayoutState& aState, PRInt32 aIndex)
{
  NS_ERROR("Should Never be Called!");
  return NS_OK;
}

NS_IMETHODIMP
nsMonumentLayout::DesecrateMonuments(nsIBox* aBox, nsBoxLayoutState& aState)
{
  NS_ERROR("Should Never be Called!");
  return NS_OK;
}

PRInt32
nsMonumentLayout::GetIndexOfChild(nsIBox* aBox, nsIBox* aChild)
{
   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   PRInt32 count = 0;
   while(child)
   {
    if (child == aChild) {
        return count;
     }

     child->GetNextBox(&child);
     count++;
   }

   return -1;
}

NS_IMPL_ADDREF_INHERITED(nsMonumentLayout, nsBoxLayout);
NS_IMPL_RELEASE_INHERITED(nsMonumentLayout, nsBoxLayout);

NS_INTERFACE_MAP_BEGIN(nsMonumentLayout)
  NS_INTERFACE_MAP_ENTRY(nsIMonument)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMonument)
NS_INTERFACE_MAP_END_INHERITING(nsBoxLayout)
