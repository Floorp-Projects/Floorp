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

#include "nsBoxLayoutState.h"
#include "nsIReflowCommand.h"
#include "nsBoxFrame.h"
#include "nsIStyleContext.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIPresContext.h"

nsBoxLayoutState::nsBoxLayoutState(nsIPresContext* aPresContext):mPresContext(aPresContext), mReflowState(nsnull)
{
}

nsBoxLayoutState::nsBoxLayoutState(const nsBoxLayoutState& aState)
{
  mPresContext = aState.mPresContext;
  mType        = aState.mType;
  mReflowState = aState.mReflowState;
}

nsBoxLayoutState::nsBoxLayoutState(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState):mReflowState(&aReflowState),mPresContext(aPresContext),mType(Dirty)
{
}

PRBool
nsBoxLayoutState::HandleReflow(nsIBox* aRootBox, PRBool aCoelesce)
{
      switch(mReflowState->reason)
      {
         case eReflowReason_Incremental: 
         {
            nsIReflowCommand::ReflowType  type;
            mReflowState->reflowCommand->GetType(type);

            // atempt to coelesce style changes and reflows
            // see if the target is a box
            /*
             PRBool isAdaptor = PR_FALSE;
             nsIBox* box = GetTargetBox(mReflowState->reflowCommand, isAdaptor);
             // if it is mark it dirty. Generating a dirty reflow targeted at us unless on is already
             // done.
             if (box && box != aRootBox) {
               if (type == nsIReflowCommand::StyleChanged) {
                  // could be a visiblity change need to dirty
                  // parent so it gets redrawn.
                  nsIBox* parent;
                  box->GetParentBox(&parent);
                  parent->MarkDirty(*this);
                  DirtyAllChildren(*this, box);
               } else
                  box->MarkDirty(*this);
               return PR_TRUE;
             }
             */

             // ok if the target was not a box. Then unwind it down 
            if (UnWind(mReflowState->reflowCommand, aRootBox, aCoelesce)) {
               mType = Dirty;
               return PR_TRUE;
            }
           
         } // fall into dirty

         case eReflowReason_Dirty: 
            mType = Dirty;
         break;

         // if the a resize reflow then it doesn't need to be reflowed. Only if the size is different
         // from the new size would we actually do a reflow
         case eReflowReason_Resize:
            mType = Resize;
         break;

         // if its an initial reflow we must place the child.
         // otherwise we might think it was already placed when it wasn't
         case eReflowReason_Initial:
            mType = Initial;
         break;

         default:
            mType = Dirty;
      }

      return PR_FALSE;
}


PRBool
nsBoxLayoutState::UnWind(nsIReflowCommand* aCommand, nsIBox* aBox, PRBool aCoelesce)
{
  // if incremental unwindow the chain
  nsIFrame* incrementalChild = nsnull;
  nsIBox* lastBox = nsnull;
  nsIFrame* target = nsnull;
  aCommand->GetTarget(target);
  nsIReflowCommand::ReflowType  type;
  mReflowState->reflowCommand->GetType(type);

  while(1)
  {
    aCommand->GetNext(incrementalChild, PR_FALSE);
    if (incrementalChild == nsnull)
      return PR_FALSE;

    // get the box for the given incrementalChild. If adaptor is true then
    // it is some wrapped HTML frame.
    PRBool isAdaptor = PR_FALSE;
    nsIBox* ibox = GetBoxForFrame(incrementalChild, isAdaptor);
    if (ibox) {
      nsFrameState state;
      incrementalChild->GetFrameState(&state);
      state &= ~NS_FRAME_HAS_DIRTY_CHILDREN;
      incrementalChild->SetFrameState(state);

      // ok we got a box is it the target?
      if (incrementalChild == target) {

        if (!aCoelesce) {
          nsFrameState state;
          nsIFrame* frame;
          aBox->GetFrame(&frame);
          frame->GetFrameState(&state);
          state |= NS_FRAME_HAS_DIRTY_CHILDREN;
          frame->SetFrameState(state);
        }
         // the target is a box?
         // mark it dirty generating a new reflow command targeted
         // at us and coelesce out this one.
         ibox->MarkDirty(*this);      

         if (type == nsIReflowCommand::StyleChanged) {
            // could be a visiblity change need to dirty
            // parent so it gets redrawn.
            nsIBox* parent;
            ibox->GetParentBox(&parent);
            parent->MarkDirty(*this);
            //DirtyAllChildren(*this, box);
         } 

         // yes we coelesed
         return aCoelesce ? PR_TRUE : PR_FALSE;
      }

      // was the child html?
      if (isAdaptor) {
        // the target is deep inside html we will have to honor this one.
        // mark us as dirty so we don't post
        // a dirty reflow
        nsFrameState state;
        nsIFrame* frame;
        aBox->GetFrame(&frame);
        frame->GetFrameState(&state);
        state |= NS_FRAME_HAS_DIRTY_CHILDREN;
        frame->SetFrameState(state);

        incrementalChild->GetFrameState(&state);
        state &= ~NS_FRAME_IS_DIRTY;
        incrementalChild->SetFrameState(state);

        // mark the adaptor dirty
        ibox->MarkDirty(*this);      
        
        // we are done and we did not coelesce
        return PR_FALSE;
      }

    } else {
      NS_ERROR("This should not happen! We should always get a box");
      break;
    }
  
    aCommand->GetNext(incrementalChild);
  }
   
  return PR_FALSE;
}

/*
void
nsBoxLayoutState::UnWind(nsIReflowCommand* aCommand, nsIBox* aBox)
{
    
  nsFrameState state;
  nsIFrame* frame;
  aBox->GetFrame(&frame);
  frame->GetFrameState(&state);
  state |= NS_FRAME_HAS_DIRTY_CHILDREN;
  frame->SetFrameState(state);

  // if incremental unwindow the chain
  nsIFrame* incrementalChild = nsnull;

  while(1)
  {
    aCommand->GetNext(incrementalChild, PR_FALSE);
    if (incrementalChild == nsnull)
      break;

    nsIFrame* target = nsnull;
    aCommand->GetTarget(target);

    PRBool isAdaptor = PR_FALSE;
    nsIBox* ibox = GetBoxForFrame(incrementalChild, isAdaptor);
    if (ibox) {
      if (incrementalChild == target || isAdaptor) {
         ibox->MarkDirty(*this);
         if (isAdaptor)
           break;
      } else 
         ibox->MarkDirtyChildren(*this);
      
    } else {
      break;
    }
  
    aCommand->GetNext(incrementalChild);
  }
  
}
*/

nsIBox*
nsBoxLayoutState::GetTargetBox(nsIReflowCommand* mCommand, PRBool& aIsAdaptor)
{
  nsIFrame* target = nsnull;
  mReflowState->reflowCommand->GetTarget(target);
  return GetBoxForFrame(target, aIsAdaptor);
}

nsIBox*
nsBoxLayoutState::GetBoxForFrame(nsIFrame* aFrame, PRBool& aIsAdaptor)
{
  if (aFrame == nsnull)
    return nsnull;

  nsIBox* ibox = nsnull;
  if (NS_FAILED(aFrame->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox))) {
    aIsAdaptor = PR_TRUE;

    // if we hit a non box. Find the box in out last container
    // and clear its cache.
    nsIFrame* parent = nsnull;
    aFrame->GetParent(&parent);
    nsIBox* parentBox = nsnull;
    if (NS_FAILED(parent->QueryInterface(NS_GET_IID(nsIBox), (void**)&parentBox))) 
       return nsnull;

    if (parentBox) {
      nsIBox* start = nsnull;
      parentBox->GetChildBox(&start);
      while (start) {
        nsIFrame* frame = nsnull;
        start->GetFrame(&frame);
        if (frame == aFrame) {
          ibox = start;
          break;
        }

        start->GetNextBox(&start);
      }
    }
  } 

  return ibox;
}

void
nsBoxLayoutState::DirtyAllChildren(nsBoxLayoutState& aState, nsIBox* aBox)
{
    aBox->MarkDirty(aState);

    nsIBox* first = nsnull;
    aBox->GetChildBox(&first);
    if (first)
       aBox->MarkDirtyChildren(aState);

    while(first)
    {
      DirtyAllChildren(aState, first);
      first->GetNextBox(&first);
    }
}

void* 
nsBoxLayoutState::Allocate(size_t sz, nsIPresShell* aPresShell)
{
  // Check the recycle list first.
  void* result = nsnull;
  aPresShell->AllocateFrame(sz, &result);
  
  if (result) {
    nsCRT::zero(result, sz);
  }

  return result;
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsBoxLayoutState::Free(void* aPtr, size_t sz)
{
  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.

  // Stash the size of the object in the first four bytes of the
  // freed up memory.  The Destroy method can then use this information
  // to recycle the object.
  size_t* szPtr = (size_t*)aPtr;
  *szPtr = sz;
}

void
nsBoxLayoutState::RecycleFreedMemory(nsIPresShell* aShell, void* aMem)
{
  size_t* sz = (size_t*)aMem;
  aShell->FreeFrame(*sz, aMem);
}

nsresult
nsBoxLayoutState::GetPresShell(nsIPresShell** aShell)
{
  return mPresContext->GetShell(aShell); 
}

nsresult
nsBoxLayoutState::PushStackMemory()
{
  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));
  return shell->PushStackMemory();
}

nsresult
nsBoxLayoutState::PopStackMemory()
{
  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));

  return shell->PopStackMemory();
}

nsresult
nsBoxLayoutState::AllocateStackMemory(size_t aSize, void** aResult)
{
  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));

  return shell->AllocateStackMemory(aSize, aResult);
}
 