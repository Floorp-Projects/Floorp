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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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


#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsHTMLContainerFrame.h"
#include "nsBoxToBlockAdaptor.h"

#include "nsBoxLayoutState.h"
#include "nsBoxFrame.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsUnitConversion.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIContent.h"
#include "nsHTMLParts.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsFrameNavigator.h"
#include "nsCSSRendering.h"
#include "nsIServiceManager.h"

#include "nsContainerBox.h"

nsContainerBox::nsContainerBox(nsIPresShell* aShell):nsBox(aShell)
{
  mFirstChild = nsnull;
  mLastChild = nsnull;
  mChildCount = 0;
  mOrderBoxes = PR_FALSE;
}

nsContainerBox::~nsContainerBox()
{
}

void
nsContainerBox::Destroy(nsBoxLayoutState& aState)
{
  SetLayoutManager(nsnull);
  ClearChildren(aState);
}

#ifdef DEBUG_LAYOUT
void
nsContainerBox::GetBoxName(nsAutoString& aName)
{
  aName.AssignLiteral("ContainerBox");
}
#endif

NS_IMETHODIMP 
nsContainerBox::GetChildBox(nsIBox** aBox)
{
    *aBox = mFirstChild;
    return NS_OK;
}

PRInt32 
nsContainerBox::GetChildCount()
{
   return mChildCount;
}

PRInt32 
nsContainerBox::CreateBoxList(nsBoxLayoutState& aState, nsIFrame* aFrameList, nsIBox*& aFirst, nsIBox*& aLast)
{
  nsCOMPtr<nsIPresShell> shell;
  aState.GetPresShell(getter_AddRefs(shell));

  PRInt32 count = 0;
  if (aFrameList) {
    nsIBox* ibox = nsnull;
    if (NS_SUCCEEDED(aFrameList->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) 
        aFirst = ibox;
    else
        aFirst = new (shell) nsBoxToBlockAdaptor(shell, aFrameList);

       aFirst->SetParentBox(this);

       count++;
       aLast = aFirst;
       aFrameList = aFrameList->GetNextSibling();
       nsIBox* last = aLast;

       while(aFrameList) {

        if (NS_SUCCEEDED(aFrameList->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) 
            aLast = ibox;
        else
            aLast = new (shell) nsBoxToBlockAdaptor(shell, aFrameList);

         aLast->SetParentBox(this);

         // check if this box is in a different ordinal group, and requires sorting
         PRUint32 ordinal;
         aLast->GetOrdinal(aState, ordinal);
         if (ordinal != DEFAULT_ORDINAL_GROUP)
           mOrderBoxes = PR_TRUE;
         
         last->SetNextBox(aLast);
         last = aLast;
         aFrameList = aFrameList->GetNextSibling();
         count++;
       }
    }

    return count;
}

nsIBox* 
nsContainerBox::GetPrevious(nsIFrame* aFrame)
{
    if (aFrame == nsnull)
        return nsnull;

   // find the frame to remove
    nsIBox* box = mFirstChild;
    nsIBox* prev = nsnull;
    while (box) 
    {        
      nsIFrame* frame = nsnull;
      box->GetFrame(&frame);
      if (frame == aFrame) {
          return prev;
      }

      prev = box;
      box->GetNextBox(&box);
    }

    return nsnull;
}

nsIBox* 
nsContainerBox::GetBox(nsIFrame* aFrame)
{
    if (aFrame == nsnull)
        return nsnull;

      // recycle adaptors
   // nsIBox* ibox = nsnull;

   // if (NS_SUCCEEDED(aFrame->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox)
   //    return ibox;

   // find the frame to remove
    nsIBox* box = mFirstChild;
    while (box) 
    {    
      nsIFrame* frame = nsnull;
      box->GetFrame(&frame);

      if (frame == aFrame) {
          return box;
      }

      box->GetNextBox(&box);
    }

    return nsnull;
}

nsIBox* 
nsContainerBox::GetBoxAt(PRInt32 aIndex)
{
   // find the frame to remove
    nsIBox* child = mFirstChild;
    PRInt32 count = 0;
    while (child) 
    {        
      if (count == aIndex) {
          return child;
      }

      child->GetNextBox(&child);
      count++;
    }

    return nsnull;
}

void
nsContainerBox::Remove(nsBoxLayoutState& aState, nsIFrame* aFrame)
{
    // get the info before the frame
    nsIBox* prevBox = GetPrevious(aFrame);
    RemoveAfter(aState, prevBox);        
}

void
nsContainerBox::Insert(nsBoxLayoutState& aState, nsIFrame* aPrevFrame, nsIFrame* aFrameList)
{
   nsIBox* prevBox = GetBox(aPrevFrame);
   NS_ASSERTION(aPrevFrame == nsnull || prevBox,"Error! The previous frame given is not in our list!");

   // find the frame before this one
   // if no previous frame then we are inserting in front
   if (prevBox == nsnull) {
       // prepend them
       Prepend(aState, aFrameList);
   } else {
       // insert insert after previous info
       InsertAfter(aState, prevBox, aFrameList);
   }
}

void
nsContainerBox::RemoveAfter(nsBoxLayoutState& aState, nsIBox* aPrevious)
{
    nsIBox* toDelete = nsnull;

    if (aPrevious == nsnull)
    {
        NS_ASSERTION(mFirstChild,"Can't find first child");
        toDelete = mFirstChild;
        if (mLastChild == mFirstChild) {
            nsIBox* next = nsnull;
            mFirstChild->GetNextBox(&next);
            mLastChild = next;
        }
        mFirstChild->GetNextBox(&mFirstChild);
    } else {
        aPrevious->GetNextBox(&toDelete);
        NS_ASSERTION(toDelete,"Can't find child to delete");

        nsIBox* next = nsnull;
        toDelete->GetNextBox(&next);
        aPrevious->SetNextBox(next);

        if (mLastChild == toDelete)
            mLastChild = aPrevious;
    }

    // recycle adaptors
    nsIBoxToBlockAdaptor* adaptor = nsnull;

    if (NS_SUCCEEDED(toDelete->QueryInterface(NS_GET_IID(nsIBoxToBlockAdaptor), (void**)&adaptor)) && adaptor) {
       nsCOMPtr<nsIPresShell> shell;
       aState.GetPresShell(getter_AddRefs(shell));
       adaptor->Recycle(shell);
    }

    mChildCount--;

    if (mLayoutManager)
      mLayoutManager->ChildrenRemoved(this, aState, toDelete);
}

void
nsContainerBox::ClearChildren(nsBoxLayoutState& aState)
{
   if (mFirstChild && mLayoutManager)
      mLayoutManager->ChildrenRemoved(this, aState, mFirstChild);

   nsIBox* box = mFirstChild;
   while(box) {
      nsIBox* it = box;
      box->GetNextBox(&box);
      // recycle adaptors
      nsIBoxToBlockAdaptor* adaptor = nsnull;

      if (NS_SUCCEEDED(it->QueryInterface(NS_GET_IID(nsIBoxToBlockAdaptor), (void**)&adaptor)) && adaptor) {
         nsCOMPtr<nsIPresShell> shell;
         aState.GetPresShell(getter_AddRefs(shell));
         adaptor->Recycle(shell);
      }
   }

   mFirstChild= nsnull;
   mLastChild= nsnull;
   mChildCount = 0;
}

void
nsContainerBox::Prepend(nsBoxLayoutState& aState, nsIFrame* aList)
{
    nsIBox* first;
    nsIBox* last;
    mChildCount += CreateBoxList(aState, aList, first, last);
    if (!mFirstChild)
        mFirstChild= mLastChild= first;
    else {
        last->SetNextBox(mFirstChild);
        mFirstChild= first;
    }
    
    CheckBoxOrder(aState);
    
    if (mLayoutManager)
      mLayoutManager->ChildrenInserted(this, aState, nsnull, first);

}

void
nsContainerBox::Append(nsBoxLayoutState& aState, nsIFrame* aList)
{
    nsIBox* first;
    nsIBox* last;
    mChildCount += CreateBoxList(aState, aList, first, last);
    if (!mFirstChild) 
        mFirstChild= first;
    else 
        mLastChild->SetNextBox(first);
    
    mLastChild= last;

    CheckBoxOrder(aState);
    
    if (mLayoutManager)
      mLayoutManager->ChildrenAppended(this, aState, first);
}

void 
nsContainerBox::InsertAfter(nsBoxLayoutState& aState, nsIBox* aPrev, nsIFrame* aList)
{
    nsIBox* first = nsnull;
    nsIBox* last = nsnull;
    mChildCount += CreateBoxList(aState, aList, first, last);
    nsIBox* next = nsnull;
    aPrev->GetNextBox(&next);
    last->SetNextBox(next);
    aPrev->SetNextBox(first);
    if (aPrev == mLastChild)
        mLastChild = last;

    CheckBoxOrder(aState);
        
    if (mLayoutManager) {
      mLayoutManager->ChildrenInserted(this, aState, aPrev, first);
    }

}

void 
nsContainerBox::InitChildren(nsBoxLayoutState& aState, nsIFrame* aList)
{
    ClearChildren(aState);
    mChildCount += CreateBoxList(aState, aList, mFirstChild, mLastChild);
    CheckBoxOrder(aState);

    if (mLayoutManager)
      mLayoutManager->ChildrenSet(this, aState, mFirstChild);

}

#ifdef DEBUG_LAYOUT
void
nsContainerBox::SetDebugOnChildList(nsBoxLayoutState& aState, nsIBox* aChild, PRBool aDebug)
{
    nsIBox* child = nsnull;
     GetChildBox(&child);
     while (child)
     {
        child->SetDebug(aState, aDebug);
        child->GetNextBox(&child);
     }
}
#endif

void
nsContainerBox::SanityCheck(nsFrameList& aFrameList)
{
#ifdef DEBUG
    // make sure the length match
    PRInt32 length = aFrameList.GetLength();
    NS_ASSERTION(length == mChildCount,"nsBox::ERROR!! Box info list count does not match frame count!!");

    // make sure last makes sense
    nsIBox* next = nsnull;
    NS_ASSERTION(mLastChild == nsnull || (NS_SUCCEEDED(mLastChild->GetNextBox(&next)) && next == nsnull),"nsBox::ERROR!! The last child is not really the last!!!");
    nsIFrame* child = aFrameList.FirstChild();
    nsIBox* box = mFirstChild;
    PRInt32 count = 0;
    while(box)
    {
       NS_ASSERTION(count <= mChildCount,"too many children!!!");
       nsIFrame* frame = nsnull;
       nsIBox* parent = nsnull;
       NS_ASSERTION(NS_SUCCEEDED(box->GetFrame(&frame)) && frame == child,"nsBox::ERROR!! box info list and child info lists don't match!!!"); 
       NS_ASSERTION(NS_SUCCEEDED(box->GetParentBox(&parent)) && parent == this,"nsBox::ERROR!! parent's don't match!!!"); 
       box->GetNextBox(&box);
       child = child->GetNextSibling();
       count++;
    }
#endif
}

void 
nsContainerBox::CheckBoxOrder(nsBoxLayoutState& aState)
{
  if (mOrderBoxes) {
    nsIBox** boxes = new nsIBox*[mChildCount];
    
    // turn linked list into array for quick sort
    nsIBox* box = mFirstChild;
    nsIBox** boxPtr = boxes;
    while (box) {
      *boxPtr = box;
      box->GetNextBox(&box);
      ++boxPtr;
    }

    // sort the array by ordinal group, selection sort
    PRInt32 i, j, min;
    PRUint32 minOrd, jOrd;
    for(i = 0; i < mChildCount; i++) {
      min = i;
      boxes[min]->GetOrdinal(aState, minOrd);
      for(j = i + 1; j < mChildCount; j++) {
        boxes[j]->GetOrdinal(aState, jOrd);
        if (jOrd < minOrd) {
          min = j;
          minOrd = jOrd;
        }
      }
      box = boxes[min];
      boxes[min] = boxes[i];
      boxes[i] = box;
    }
    
    // turn the array back into linked list, with first and last cached
    mFirstChild = boxes[0];
    mLastChild = boxes[mChildCount-1];
    for (i = 0; i < mChildCount; ++i) {
      if (i <= mChildCount-2)
        boxes[i]->SetNextBox(boxes[i+1]);
      else
        boxes[i]->SetNextBox(nsnull);
    }
    delete [] boxes;
  }
}

NS_IMETHODIMP
nsContainerBox::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  nsresult rv = NS_OK;

  aSize.width = 0;
  aSize.height = 0;

  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed)
    return NS_OK;

  // if the size was not completely redefined in CSS then ask our children
  if (!nsIBox::AddCSSPrefSize(aState, this, aSize)) 
  {
    aSize.width = 0;
    aSize.height = 0;

    if (mLayoutManager) {
      rv = mLayoutManager->GetPrefSize(this, aState, aSize);
      nsIBox::AddCSSPrefSize(aState, this, aSize);
    } else 
      rv = nsBox::GetPrefSize(aState, aSize);
  }

  nsSize minSize(0,0);
  nsSize maxSize(0,0);
  GetMinSize(aState, minSize);
  GetMaxSize(aState, maxSize);

  BoundsCheck(minSize, aSize, maxSize);

  return rv;
}

NS_IMETHODIMP
nsContainerBox::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  nsresult rv = NS_OK;

  aSize.width = 0;
  aSize.height = 0;

  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed)
    return NS_OK;

  // if the size was not completely redefined in CSS then ask our children
  if (!nsIBox::AddCSSMinSize(aState, this, aSize)) 
  {
    aSize.width = 0;
    aSize.height = 0;

    if (mLayoutManager) {
        rv = mLayoutManager->GetMinSize(this, aState, aSize);
        nsIBox::AddCSSMinSize(aState, this, aSize);
    } else {
        rv = nsBox::GetMinSize(aState, aSize);        
    }  
  }

  return rv;
}

NS_IMETHODIMP
nsContainerBox::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  nsresult rv = NS_OK;

  aSize.width = NS_INTRINSICSIZE;
  aSize.height = NS_INTRINSICSIZE;

  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed)
    return NS_OK;

  // if the size was not completely redefined in CSS then ask our children
  if (!nsIBox::AddCSSMaxSize(aState, this, aSize)) 
  {
    aSize.width = NS_INTRINSICSIZE;
    aSize.height = NS_INTRINSICSIZE;

    if (mLayoutManager) {
        rv = mLayoutManager->GetMaxSize(this, aState, aSize);
        nsIBox::AddCSSMaxSize(aState, this, aSize);
    } else {
        rv = nsBox::GetMaxSize(aState, aSize);        
    }
  }

  return rv;
}


NS_IMETHODIMP
nsContainerBox::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  nsresult rv = NS_OK;

  aAscent = 0;

  PRBool collapsed = PR_FALSE;
  IsCollapsed(aState, collapsed);
  if (collapsed)
    return NS_OK;

  if (mLayoutManager)
    rv = mLayoutManager->GetAscent(this, aState, aAscent);
  else
    rv = nsBox::GetAscent(aState, aAscent);

  return rv;
}

NS_IMETHODIMP
nsContainerBox::DoLayout(nsBoxLayoutState& aState)
{
  PRUint32 oldFlags = 0;
  aState.GetLayoutFlags(oldFlags);
  aState.SetLayoutFlags(0);

  nsresult rv = NS_OK;
  if (mLayoutManager)
    rv = mLayoutManager->Layout(this, aState);

  aState.SetLayoutFlags(oldFlags);

  return rv;
}

NS_IMETHODIMP
nsContainerBox::SetLayoutManager(nsIBoxLayout* aLayout)
{
  mLayoutManager = aLayout;
  return NS_OK;
}

NS_IMETHODIMP
nsContainerBox::GetLayoutManager(nsIBoxLayout** aLayout)
{
  *aLayout = mLayoutManager;
  NS_IF_ADDREF(*aLayout);
  return NS_OK;
}

nsresult
nsContainerBox::LayoutChildAt(nsBoxLayoutState& aState, nsIBox* aBox, const nsRect& aRect)
{
  // get the current rect
  nsRect oldRect(0,0,0,0);
  aBox->GetBounds(oldRect);
  aBox->SetBounds(aState, aRect);

  PRBool dirty = PR_FALSE;
  PRBool dirtyChildren = PR_FALSE;
  aBox->IsDirty(dirty);
  aBox->HasDirtyChildren(dirtyChildren);

  PRBool layout = PR_TRUE;
  if (!(dirty || dirtyChildren) && aState.GetLayoutReason() != nsBoxLayoutState::Initial) 
     layout = PR_FALSE;
  
  if (layout || (oldRect.width != aRect.width || oldRect.height != aRect.height))  {
    return aBox->Layout(aState);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContainerBox::RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIBox* aChild)
{
  mOrderBoxes = PR_TRUE;
  
  PRUint32 ord;
  aChild->GetOrdinal(aState, ord);
  
  PRUint32 ordCmp;
  nsIBox* box = mFirstChild;
  nsIBox* newPrevSib = mFirstChild;
  
  box->GetOrdinal(aState, ordCmp);
  if (ord < ordCmp) {
    // new ordinal is lower than the lowest current ordinal, so it will not
    // have a previous sibling
    newPrevSib = nsnull;
  } else {  
    // search for the box after which we will insert this box
    while (box) {
      box->GetOrdinal(aState, ordCmp);
      if (newPrevSib && ordCmp > ord)
        break;
      
      newPrevSib = box;
      box->GetNextBox(&box);
    }
  }
    
  // look for the previous sibling of |aChild|
  nsIBox* oldPrevSib = mFirstChild;
  while (oldPrevSib) {
    nsIBox* me;
    oldPrevSib->GetNextBox(&me);
    if (aChild == me) {
      break;
    }
    oldPrevSib = me;
  }
  
  // if we are moving |mFirstChild|, we'll have to update the |mFirstChild| 
  // value later on
  PRBool firstChildMoved = PR_FALSE;
  if (aChild == mFirstChild)
    firstChildMoved = PR_TRUE;

  nsIBox* newNextSib;
  if (newPrevSib) {
    // insert |aChild| between |newPrevSib| and its next sibling
    newPrevSib->GetNextBox(&newNextSib);
    newPrevSib->SetNextBox(aChild);
  } else {
    // no |newPrevSib| found, so this box will become |mFirstChild|
    newNextSib = mFirstChild;
    mFirstChild = aChild;
  }
  
  // link up our new next sibling
  nsIBox* oldNextSib;
  aChild->GetNextBox(&oldNextSib);
  aChild->SetNextBox(newNextSib);

  // link |oldPrevSib| with |oldNextSib| to fill the gap left behind
  if (oldPrevSib)
    oldPrevSib->SetNextBox(oldNextSib);
  
  // if |newPrevSib| was the last child, then aChild is the new last child
  if (newPrevSib == mLastChild)
    mLastChild = aChild;
  
  if (firstChildMoved)
    mFirstChild = oldNextSib;

  return NS_OK;
}

NS_IMETHODIMP
nsContainerBox::GetIndexOf(nsIBox* aBox, PRInt32* aIndex)
{
    nsIBox* child = mFirstChild;
    PRInt32 count = 0;
    while (child) 
    {       
      if (aBox == child) {
          *aIndex = count;
          return NS_OK;
      }

      child->GetNextBox(&child);
      count++;
    }

    *aIndex = -1;

    return NS_OK;
}
