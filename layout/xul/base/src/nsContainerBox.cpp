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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//


#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsIStyleContext.h"
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
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsHTMLIIDs.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIReflowCommand.h"
#include "nsIContent.h"
#include "nsSpaceManager.h"
#include "nsHTMLParts.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsFrameNavigator.h"
#include "nsCSSRendering.h"
#include "nsISelfScrollingFrame.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"

#include "nsContainerBox.h"

nsContainerBox::nsContainerBox(nsIPresShell* aShell):nsBox(aShell)
{
  mFirstChild = nsnull;
  mLastChild = nsnull;
  mChildCount = 0;
}

nsContainerBox::~nsContainerBox()
{
}

void
nsContainerBox::GetBoxName(nsAutoString& aName)
{
  aName.AssignWithConversion("ContainerBox");
}

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
       aFrameList->GetNextSibling(&aFrameList);
       nsIBox* last = aLast;

       while(aFrameList) {

        if (NS_SUCCEEDED(aFrameList->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) 
            aLast = ibox;
        else
            aLast = new (shell) nsBoxToBlockAdaptor(shell, aFrameList);

         aLast->SetParentBox(this);

         last->SetNextBox(aLast);
         last = aLast;
         aFrameList->GetNextSibling(&aFrameList);       
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

PRInt32
nsContainerBox::GetIndexOf(nsIBox* aBox)
{
   // find the frame to remove
    nsIBox* child = mFirstChild;
    PRInt32 count = 0;
    while (child) 
    {       
      if (aBox == child) {
          return count;
      }

      child->GetNextBox(&child);
      count++;
    }

    return -1;
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
   //NS_ASSERTION(aPrevFrame == nsnull || prevBox,"Error! The previous frame given is not in our list!");

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

    if (mLayoutManager) {
      mLayoutManager->ChildrenInserted(this, aState, aPrev, first);
    }

}

void 
nsContainerBox::InitChildren(nsBoxLayoutState& aState, nsIFrame* aList)
{
    ClearChildren(aState);
    mChildCount += CreateBoxList(aState, aList, mFirstChild, mLastChild);

    if (mLayoutManager)
      mLayoutManager->ChildrenAppended(this, aState, mFirstChild);

}

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

void
nsContainerBox::SanityCheck(nsFrameList& aFrameList)
{
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
       child->GetNextSibling(&child);
       count++;
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

