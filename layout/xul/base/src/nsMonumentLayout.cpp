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

#include "nsMonumentLayout.h"
#include "nsBoxLayoutState.h"
#include "nsIBox.h"
#include "nsIScrollableFrame.h"
#include "nsBox.h"

// ----- Monument Iterator -----

nsLayoutIterator::nsLayoutIterator(nsIBox* aBox):mBox(nsnull),mStartBox(aBox),mParentCount(0)
{
}

void
nsLayoutIterator::Reset()
{
  mBox = nsnull;
}

PRBool
nsLayoutIterator::GetNextLayout(nsIBoxLayout** aLayout, PRBool aSearchChildren)
{
  if (mBox == nsnull) {
    mBox = mStartBox;
  } else {
    if (aSearchChildren) {
       mParents[mParentCount++] = mBox;
       NS_ASSERTION(mParentCount < PARENT_STACK_SIZE, "Stack overflow!!");
       mBox->GetChildBox(&mBox);
    } else {
       mBox->GetNextBox(&mBox);
    }
  }

  return DigDeep(aLayout, aSearchChildren);  
}

PRBool
nsLayoutIterator::DigDeep(nsIBoxLayout** aLayout, PRBool aSearchChildren)
{
  // if our box is null. See if we have any parents on the stack
  // if so pop them off and continue.
  if (!mBox) {
    if (mParentCount > 0) {
      mBox = mParents[--mParentCount];
      mBox->GetNextBox(&mBox);
      return DigDeep(aLayout, aSearchChildren);
    }

    *aLayout = nsnull;
    return PR_FALSE;
  }

  // if its a scrollframe. Then continue down into the scrolled frame
  nsresult rv = NS_OK;
  nsCOMPtr<nsIScrollableFrame> scrollFrame = do_QueryInterface(mBox, &rv);
  if (scrollFrame) {
     nsIFrame* scrolledFrame = nsnull;
     scrollFrame->GetScrolledFrame(nsnull, scrolledFrame);
     NS_ASSERTION(scrolledFrame,"Error no scroll frame!!");
     mParents[mParentCount++] = mBox;
     NS_ASSERTION(mParentCount < PARENT_STACK_SIZE, "Stack overflow!!");
     nsCOMPtr<nsIBox> b = do_QueryInterface(scrolledFrame);
     mBox = b;
  }

  // get the layout manager
  nsCOMPtr<nsIBoxLayout> layout;
  
  mBox->GetLayoutManager(getter_AddRefs(layout));

  // if we are supposed to search our children. And the layout manager
  // was null. Then dig into the children.
  if (aSearchChildren && !layout)
  {
       mParents[mParentCount++] = mBox;
       NS_ASSERTION(mParentCount < PARENT_STACK_SIZE, "Stack overflow!!");
       mBox->GetChildBox(&mBox);
       return DigDeep(aLayout, aSearchChildren);
  }

  *aLayout = layout;
  NS_IF_ADDREF(*aLayout);
  if (layout) 
    return PR_TRUE;
  else
    return PR_FALSE;
}


// ---- Monument Iterator -----

nsMonumentIterator::nsMonumentIterator(nsIBox* aBox):nsLayoutIterator(aBox)
{
}

PRBool
nsMonumentIterator::GetNextMonument(nsIMonument** aMonument, PRBool aSearchChildren)
{
  nsCOMPtr<nsIBoxLayout> layout;

  while(GetNextLayout(getter_AddRefs(layout), aSearchChildren)) {
    if (layout) 
    {
      nsresult rv = NS_OK;
      nsCOMPtr<nsIMonument> monument = do_QueryInterface(layout, &rv);
      *aMonument = monument;

      if (monument) {
        NS_IF_ADDREF(*aMonument);
        return PR_TRUE;
      }
    }
  }

  return PR_FALSE;
}

PRBool
nsMonumentIterator::GetNextObelisk(nsObeliskLayout** aObelisk, PRBool aSearchChildren)
{
  nsCOMPtr<nsIMonument> monument;
  PRBool searchChildren = aSearchChildren;

  while(GetNextMonument(getter_AddRefs(monument), searchChildren)) {

    searchChildren = aSearchChildren;
    if (monument) 
    {
      *aObelisk = nsnull;
      monument->CastToObelisk(aObelisk);

      if (*aObelisk) {
        return PR_TRUE;
      }

      // ok we found another grid. Don't enter it.
      nsGridLayout* grid = nsnull;
      monument->CastToGrid(&grid);
      if (grid) {
         searchChildren = PR_FALSE;
      }
    }
  }
  
  *aObelisk = nsnull;

  return PR_FALSE;
}

//static long _nodes = 0;
//static long _lists = 0;


//------ nsInfoListNodeImpl ----

nsBoxSizeListNodeImpl::~nsBoxSizeListNodeImpl()
{
    MOZ_COUNT_DTOR(nsBoxSizeListNodeImpl);
    //_nodes--;
    //printf("Nodes %d\n",_nodes);
}

void
nsBoxSizeListNodeImpl::Release(nsBoxLayoutState& aState)
{
  Destroy(aState);
}

void
nsBoxSizeListNodeImpl::Destroy(nsBoxLayoutState& aState)
{
  delete this;
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
  if (mBox)
    mBox->MarkDirty(aState);
}

void nsBoxSizeListNodeImpl::SetNext(nsBoxLayoutState& aState, nsBoxSizeList* aNext)
{
  mNext = aNext;
}

void nsBoxSizeListNodeImpl::SetAdjacent(nsBoxLayoutState& aState, nsBoxSizeList* aNext)
{
  mAdjacent = aNext;
}

void
nsBoxSizeListNodeImpl::Append(nsBoxLayoutState& aState, nsBoxSizeList* aChild)
{
  NS_ERROR("Attept at append to a leaf");
}

nsBoxSizeListNodeImpl::nsBoxSizeListNodeImpl(nsIBox* aBox):mNext(nsnull), 
                                                           mParent(nsnull), 
                                                           mAdjacent(nsnull),
                                                           mBox(aBox),
                                                           mRefCount(0),
                                                           mIsSet(PR_FALSE)
{
    MOZ_COUNT_CTOR(nsBoxSizeListNodeImpl);
 // _nodes++;
 // printf("Created. Nodes %d\n",_nodes);
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

     node = node->GetAdjacent();
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

nsBoxSizeListImpl::nsBoxSizeListImpl(nsIBox* aBox):nsBoxSizeListNodeImpl(aBox),
                                                   mFirst(nsnull),
                                                   mLast(nsnull),
                                                   mCount(0),
                                                   mListener(nsnull), 
                                                   mListenerBox(nsnull)
{
    MOZ_COUNT_CTOR(nsBoxSizeListImpl);
 //   _lists++;
 //   printf("Lists %d\n",_lists);
}

        
nsBoxSizeListImpl::~nsBoxSizeListImpl()
{
    MOZ_COUNT_DTOR(nsBoxSizeListImpl);
 //   _lists--;
 //   printf("Lists %d\n",_lists);
}

/* Ownership model nsMonumentLayout

    nsTempleLayout owns 
       mMonuments (nsBoxSizeListImpl)

    nsBoxSizeListImpl owns 
       mAdjacent (nsBoxSizeListImpl)
       mFirst (nsBoxSizeList) Now mFirst is a list of
       nsBoxSizeListImpl or nsBoxSizeListNodeImpl. 
       It only owns nsBoxSizeNodeImpl not the
       nsBoxSizeListImpl.
*/
void
nsBoxSizeListImpl::Destroy(nsBoxLayoutState& aState)
{
  // notify the listener that we are going away
  if (mListener) {
    mListener->WillBeDestroyed(mListenerBox, aState, *this);
  }

  // tell each of our children to release. If you ask
  // a node to release it will delete itself. If you
  // ask a list to release it will do nothing because
  // Lists are not owned by other lists they are owned
  // by Temples.
  nsBoxSizeList* list = mFirst;
  while(list)
  {
    nsBoxSizeList* toRelease = list;
    list = list->GetNext();
    toRelease->Release(aState);
  }

  // now tell each or our adacent children to be destroyed.
  if (mAdjacent)
    mAdjacent->Destroy(aState);

  delete this;
}

void
nsBoxSizeListImpl::Release(nsBoxLayoutState& aState)
{
  // do nothing. We can only be destroyed by our owner
  // by calling Destroy.
  mParent = nsnull;
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
  mLast->SetNext(aState, nsnull);
  aChild->SetParent(this);
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
nsBoxSizeListImpl::GetBoxSize(nsBoxLayoutState& aState, PRBool aIsHorizontal)
{
  if (!mIsSet) {

    mIsSet = PR_TRUE;

    mBoxSize.Clear();
    
    nsBoxSizeList* node = mFirst;

    while(node) {
      nsBoxSize size = node->GetBoxSize(aState, aIsHorizontal);

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
nsBoxSizeListNodeImpl::GetBoxSize(nsBoxLayoutState& aState, PRBool aIsHorizontal)
{
  nsBoxSize size;

  nsSize pref(0,0);
  nsSize min(0,0);
  nsSize max(NS_INTRINSICSIZE,NS_INTRINSICSIZE);
  nscoord ascent = 0;
  nscoord flex = 0;

  if (!mBox)
    return size;

  mBox->GetPrefSize(aState, pref);
  mBox->GetMinSize(aState, min);
  mBox->GetMaxSize(aState, max);
  mBox->GetAscent(aState, ascent);
  mBox->GetFlex(aState, flex);
  nsBox::AddMargin(mBox, pref);

  size.Add(min, pref, max, ascent, flex, !aIsHorizontal); 

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
nsMonumentLayout::CastToGrid(nsGridLayout** aGrid)
{
  *aGrid = nsnull;
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
  if (parent)
     return parent->GetOtherMonumentsAt(parentBox, 0, aList, this);
  else
     return NS_OK;
}

/**
 * Get the monuments in the other temple at the give monument index
 */
NS_IMETHODIMP
nsMonumentLayout::GetOtherMonumentsAt(nsIBox* aBox, PRInt32 aIndexOfObelisk, nsBoxSizeList** aList, nsMonumentLayout* aRequestor)
{
   nsresult rv = NS_OK;

   PRInt32 index = -1;
   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   PRInt32 count = 0;
   while(child)
   {
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

   if (parent)
      parent->GetOtherMonumentsAt(parentBox, aIndexOfObelisk, aList, this);

   return NS_OK;
}

NS_IMETHODIMP
nsMonumentLayout::GetOtherTemple(nsIBox* aBox, nsTempleLayout** aTemple, nsIBox** aTempleBox, nsMonumentLayout* aRequestor)
{
   nsCOMPtr<nsIMonument> parent;
   nsCOMPtr<nsIBox> parentBox;
   GetParentMonument(aBox, parentBox, getter_AddRefs(parent));
   if(parent)
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
     list = list->GetAdjacent();
     count++;
  }

  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsMonumentLayout::BuildBoxSizeList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aFirst, nsBoxSize*& aLast, PRBool aIsHorizontal)
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

   aFirst->Add(min, pref, max, ascent, flex, aIsHorizontal); 
   aFirst->Add(borderPadding,aIsHorizontal);
   aFirst->Add(margin,aIsHorizontal);
   
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
      last->SetAdjacent(aState, newOne);
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
