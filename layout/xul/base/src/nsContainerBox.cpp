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
#include "nsGenericHTMLElement.h"
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
  mInsertionPoint = nsnull;
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
nsContainerBox::CreateBoxList(nsIPresShell* aPresShell, nsIFrame* aFrameList, nsIBox*& aFirst, nsIBox*& aLast)
{
  PRInt32 count = 0;
  if (aFrameList) {
    nsIBox* ibox = nsnull;
    if (NS_SUCCEEDED(aFrameList->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) 
        aFirst = ibox;
    else
        aFirst = new (aPresShell) nsBoxToBlockAdaptor(aPresShell, aFrameList);

       aFirst->SetParentBox(this);

       count++;
       aLast = aFirst;
       aFrameList->GetNextSibling(&aFrameList);
       nsIBox* last = aLast;

       while(aFrameList) {

        if (NS_SUCCEEDED(aFrameList->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) 
            aLast = ibox;
        else
            aLast = new (aPresShell) nsBoxToBlockAdaptor(aPresShell, aFrameList);

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
nsContainerBox::Remove(nsIPresShell* aShell, nsIFrame* aFrame)
{
    // get the info before the frame
    nsIBox* prevBox = GetPrevious(aFrame);
    RemoveAfter(aShell, prevBox);        
}

void
nsContainerBox::Insert(nsIPresShell* aShell, nsIFrame* aPrevFrame, nsIFrame* aFrameList)
{
   nsIBox* prevBox = GetBox(aPrevFrame);
   //NS_ASSERTION(aPrevFrame == nsnull || prevBox,"Error! The previous frame given is not in our list!");

   // find the frame before this one
   // if no previous frame then we are inserting in front
   if (prevBox == nsnull) {
       // prepend them
       Prepend(aShell, aFrameList);
   } else {
       // insert insert after previous info
       InsertAfter(aShell, prevBox, aFrameList);
   }
}

void
nsContainerBox::RemoveAfter(nsIPresShell* aPresShell, nsIBox* aPrevious)
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

    if (NS_SUCCEEDED(toDelete->QueryInterface(NS_GET_IID(nsIBoxToBlockAdaptor), (void**)&adaptor)) && adaptor) 
       adaptor->Recycle(aPresShell);

    mChildCount--;
}

void
nsContainerBox::ClearChildren(nsIPresShell* aShell)
{
   nsIBox* box = mFirstChild;
   while(box) {
      nsIBox* it = box;
      box->GetNextBox(&box);
      // recycle adaptors
      nsIBoxToBlockAdaptor* adaptor = nsnull;

      if (NS_SUCCEEDED(it->QueryInterface(NS_GET_IID(nsIBoxToBlockAdaptor), (void**)&adaptor)) && adaptor) 
         adaptor->Recycle(aShell);
   }

   mFirstChild= nsnull;
   mLastChild= nsnull;
   mChildCount = 0;
}

void
nsContainerBox::Prepend(nsIPresShell* aPresShell, nsIFrame* aList)
{
    nsIBox* first;
    nsIBox* last;
    mChildCount += CreateBoxList(aPresShell, aList, first, last);
    if (!mFirstChild)
        mFirstChild= mLastChild= first;
    else {
        last->SetNextBox(mFirstChild);
        mFirstChild= first;
    }
}

void
nsContainerBox::Append(nsIPresShell* aPresShell, nsIFrame* aList)
{
    nsIBox* first;
    nsIBox* last;
    mChildCount += CreateBoxList(aPresShell, aList, first, last);
    if (!mFirstChild) 
        mFirstChild= first;
    else 
        mLastChild->SetNextBox(first);
    
    mLastChild= last;
}

void 
nsContainerBox::InsertAfter(nsIPresShell* aPresShell, nsIBox* aPrev, nsIFrame* aList)
{
    nsIBox* first = nsnull;
    nsIBox* last = nsnull;
    mChildCount += CreateBoxList(aPresShell, aList, first, last);
    nsIBox* next = nsnull;
    aPrev->GetNextBox(&next);
    last->SetNextBox(next);
    aPrev->SetNextBox(first);
    if (aPrev == mLastChild)
        mLastChild = last;
}

void 
nsContainerBox::InitChildren(nsIPresShell* aPresShell, nsIFrame* aList)
{
    ClearChildren(aPresShell);
    mChildCount += CreateBoxList(aPresShell, aList, mFirstChild, mLastChild);
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
nsContainerBox::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsresult rv = NS_OK;

  aSize.width = 0;
  aSize.height = 0;

  // if the size was not completely redefined in CSS then ask our children
  if (!nsIBox::AddCSSPrefSize(aBoxLayoutState, this, aSize)) 
  {
    aSize.width = 0;
    aSize.height = 0;

    if (mLayoutManager) {
      rv = mLayoutManager->GetPrefSize(this, aBoxLayoutState, aSize);
      nsIBox::AddCSSPrefSize(aBoxLayoutState, this, aSize);
    } else 
      rv = nsBox::GetPrefSize(aBoxLayoutState, aSize);
  }

  nsSize minSize(0,0);
  nsSize maxSize(0,0);
  GetMinSize(aBoxLayoutState, minSize);
  GetMaxSize(aBoxLayoutState, maxSize);

  BoundsCheck(minSize, aSize, maxSize);

  return rv;
}

NS_IMETHODIMP
nsContainerBox::GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsresult rv = NS_OK;

  aSize.width = 0;
  aSize.height = 0;

  // if the size was not completely redefined in CSS then ask our children
  if (!nsIBox::AddCSSMinSize(aBoxLayoutState, this, aSize)) 
  {
    aSize.width = 0;
    aSize.height = 0;

    if (mLayoutManager) {
        rv = mLayoutManager->GetMinSize(this, aBoxLayoutState, aSize);
        nsIBox::AddCSSMinSize(aBoxLayoutState, this, aSize);
    } else {
        rv = nsBox::GetMinSize(aBoxLayoutState, aSize);        
    }  
  }

  return rv;
}

NS_IMETHODIMP
nsContainerBox::GetMaxSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsresult rv = NS_OK;

  aSize.width = NS_INTRINSICSIZE;
  aSize.height = NS_INTRINSICSIZE;

  // if the size was not completely redefined in CSS then ask our children
  if (!nsIBox::AddCSSMaxSize(aBoxLayoutState, this, aSize)) 
  {
    aSize.width = NS_INTRINSICSIZE;
    aSize.height = NS_INTRINSICSIZE;

    if (mLayoutManager) {
        rv = mLayoutManager->GetMaxSize(this, aBoxLayoutState, aSize);
        nsIBox::AddCSSMaxSize(aBoxLayoutState, this, aSize);
    } else {
        rv = nsBox::GetMaxSize(aBoxLayoutState, aSize);        
    }
  }

  return rv;
}


NS_IMETHODIMP
nsContainerBox::GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent)
{
  nsresult rv = NS_OK;

  aAscent = 0;
  if (mLayoutManager)
    rv = mLayoutManager->GetAscent(this, aBoxLayoutState, aAscent);
  else
    rv = nsBox::GetAscent(aBoxLayoutState, aAscent);

  return rv;
}

NS_IMETHODIMP
nsContainerBox::Layout(nsBoxLayoutState& aBoxLayoutState)
{
  nsresult rv = NS_OK;

  if (mLayoutManager)
    rv = mLayoutManager->Layout(this, aBoxLayoutState);

  nsBox::Layout(aBoxLayoutState);

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

// XXX Eventually these will move into nsIFrame.
// These methods are used for XBL <children>.
NS_IMETHODIMP 
nsContainerBox::GetInsertionPoint(nsIFrame** aFrame)
{
  *aFrame = mInsertionPoint;
  return NS_OK;
}

NS_IMETHODIMP
nsContainerBox::SetInsertionPoint(nsIFrame* aFrame)
{
  mInsertionPoint = aFrame;
  return NS_OK;
}





