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
#include "nsReflowPath.h"
#include "nsBoxFrame.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIContent.h"
#include "nsIPresContext.h"

nsBoxLayoutState::nsBoxLayoutState(nsIPresContext* aPresContext):mPresContext(aPresContext), 
                                                                 mReflowState(nsnull), 
                                                                 mType(Dirty),
                                                                 mMaxElementWidth(nsnull),
                                                                 mScrolledBlockSizeConstraint(-1,-1),
                                                                 mLayoutFlags(0),
                                                                 mPaintingDisabled(PR_FALSE)
{
  NS_ASSERTION(mPresContext, "PresContext must be non-null");
}

nsBoxLayoutState::nsBoxLayoutState(const nsBoxLayoutState& aState)
{
  mPresContext = aState.mPresContext;
  mType        = aState.mType;
  mReflowState = aState.mReflowState;
  mMaxElementWidth = aState.mMaxElementWidth;
  mScrolledBlockSizeConstraint = aState.mScrolledBlockSizeConstraint;
  mLayoutFlags = aState.mLayoutFlags;
  mPaintingDisabled = aState.mPaintingDisabled;

  NS_ASSERTION(mPresContext, "PresContext must be non-null");
}

nsBoxLayoutState::nsBoxLayoutState(nsIPresShell* aShell):mReflowState(nsnull), 
                                                         mType(Dirty),
                                                         mMaxElementWidth(nsnull),
                                                         mScrolledBlockSizeConstraint(-1,-1),
                                                         mLayoutFlags(0),
                                                         mPaintingDisabled(PR_FALSE)
{
   aShell->GetPresContext(getter_AddRefs(mPresContext));
   NS_ASSERTION(mPresContext, "PresContext must be non-null");
}

nsBoxLayoutState::nsBoxLayoutState(nsIPresContext* aPresContext, 
                                   const nsHTMLReflowState& aReflowState, 
                                   nsHTMLReflowMetrics& aDesiredSize):mPresContext(aPresContext),
                                                                      mReflowState(&aReflowState),                                                                    
                                                                      mType(Dirty),
                                                                      mScrolledBlockSizeConstraint(-1,-1),
                                                                      mLayoutFlags(0),
                                                                      mPaintingDisabled(PR_FALSE)

                                                                                        

{
  mMaxElementWidth = &aDesiredSize.mMaxElementWidth;
  NS_ASSERTION(mPresContext, "PresContext must be non-null");
}

void
nsBoxLayoutState::HandleReflow(nsIBox* aRootBox)
{
      switch(mReflowState->reason)
      {
         case eReflowReason_Incremental: 
         {
           // XXXwaterson according to evaughan's prior comment here,
           // the target ought not to be a box frame. This makes some
           // of the logic in `Unwind' confusing.
           Unwind(mReflowState->path, aRootBox);
           mType = Dirty;
           break;  
         }

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

         case eReflowReason_StyleChange:
           // printf("STYLE CHANGE REFLOW. Blowing away all box caches!!\n");
            aRootBox->MarkChildrenStyleChange();
            // fall through to dirty

         default:
            mType = Dirty;
      }
}


void
nsBoxLayoutState::Unwind(nsReflowPath* aReflowPath, nsIBox* aRootBox)
{
  // If incremental, unwind the reflow path, updating dirty bits
  // appropriately. We recursively descend through the reflow tree,
  // clearing the NS_FRAME_HAS_DIRTY_CHILDREN bit on each frame until
  // we reach a target frame. When we reach a target frame, we set the
  // NS_FRAME_HAS_DIRTY_CHILDREN bit on the root box's frame, and then
  // call the target box's MarkDirty method. This will reset the
  // NS_FRAME_HAS_DIRTY_CHILDREN bit on each box on the path back to
  // root, as well as initialize each box correctly for a dirty
  // layout.
  nsReflowPath::iterator iter = aReflowPath->FirstChild();
  nsReflowPath::iterator end = aReflowPath->EndChildren();

  for ( ; iter != end; ++iter) {
    // Get the box for the given frame.
    PRBool isAdaptor = PR_FALSE;
    nsIBox* ibox = GetBoxForFrame(*iter, isAdaptor);
    if (! ibox) {
      NS_ERROR("This should not happen! We should always get a box");
      continue;
    }

    // Clear the dirty-children bit. This will be re-set by MarkDirty
    // once we reach a target.
    (*iter)->RemoveStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);

    if (isAdaptor) {
      // It's nested HTML. Mark the root box's frame with
      // NS_FRAME_HAS_DIRTY_CHILDREN so MarkDirty won't walk off the
      // top of the box hierarchy and schedule another reflow command.
      nsIFrame* frame;
      aRootBox->GetFrame(&frame);

      frame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);

      // Clear the frame's dirty bit so that MarkDirty doesn't
      // optimize the layout away.
      (*iter)->RemoveStateBits(NS_FRAME_IS_DIRTY);

      // Mark the adaptor dirty.
      ibox->MarkDirty(*this);      
        
      // We are done and we did not coelesce.
      continue;
    }

    // Is the box frame the target?
    // XXXwaterson according to the evaughan's previous comments in
    // HandleReflow, it ought to never be. Yet here we are.
    nsHTMLReflowCommand *command = iter.get()->mReflowCommand;
    if (command) {
      // Mark the root box's frame with NS_FRAME_HAS_DIRTY_CHILDREN so
      // that MarkDirty won't walk off the top of the box hierarchy
      // and schedule another reflow command.
      nsIFrame* frame;
      aRootBox->GetFrame(&frame);

      frame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);

      // The target is a box. Mark it dirty, generating a new reflow
      // command targeted at us and coelesce out this one.
      nsReflowType type;
      command->GetType(type);

      if (type == eReflowType_StyleChanged) {
        ibox->MarkStyleChange(*this);

        // could be a visiblity change. Like collapse so we need to dirty
        // parent so it gets redrawn. But be carefull we
        // don't want to just mark dirty that would notify the
        // box and it would notify its layout manager. This would 
        // be really bad for grid because it would blow away
        // all is cached infomation for is colums and rows. Because the
        // our parent is most likely a rows or columns and it will think
        // its child is getting bigger or something.
        nsIBox* parent;
        ibox->GetParentBox(&parent);
        if (parent) {
          nsIFrame* parentFrame;
          parent->GetFrame(&parentFrame);
          parentFrame->AddStateBits(NS_FRAME_IS_DIRTY);
        }

      }
      else
        ibox->MarkDirty(*this);      
    }

    // Recursively unwind the reflow path.
    Unwind(iter.get(), aRootBox);
  }
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
    nsIFrame* parent = aFrame->GetParent();
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

/*
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
*/

void* 
nsBoxLayoutState::Allocate(size_t sz, nsIPresShell* aPresShell)
{
  // Check the recycle list first.
  void* result = aPresShell->AllocateFrame(sz);
  
  if (result) {
    memset(result, 0, sz);
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
