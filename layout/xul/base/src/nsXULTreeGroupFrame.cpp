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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsXULTreeGroupFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsBoxLayoutState.h"
#include "nsISupportsArray.h"

//
// NS_NewXULTreeGroupFrame
//
// Creates a new TreeGroup frame
//
nsresult
NS_NewXULTreeGroupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                        nsIBoxLayout* aLayoutManager, PRBool aIsHorizontal)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULTreeGroupFrame* it = new (aPresShell) nsXULTreeGroupFrame(aPresShell, aIsRoot, aLayoutManager, aIsHorizontal);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewXULTreeGroupFrame

NS_IMETHODIMP_(nsrefcnt) 
nsXULTreeGroupFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsXULTreeGroupFrame::Release(void)
{
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsXULTreeGroupFrame)
  NS_INTERFACE_MAP_ENTRY(nsIXULTreeSlice)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)

// Constructor
nsXULTreeGroupFrame::nsXULTreeGroupFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager, PRBool aIsHorizontal)
:nsBoxFrame(aPresShell, aIsRoot, aLayoutManager, aIsHorizontal), mFrameConstructor(nsnull), mPresContext(nsnull),
 mOuterFrame(nsnull), mAvailableHeight(10000), mTopFrame(nsnull), mBottomFrame(nsnull), mLinkupFrame(nsnull),
 mContentChain(nsnull)
{}

// Destructor
nsXULTreeGroupFrame::~nsXULTreeGroupFrame()
{
}

void nsXULTreeGroupFrame::LocateFrame(nsIFrame* aStartFrame, nsIFrame** aResult)
{
  if (aStartFrame == nsnull)
    *aResult = mFrames.FirstChild();
  else aStartFrame->GetNextSibling(aResult);
}

nsIFrame*
nsXULTreeGroupFrame::GetFirstFrame()
{
  LocateFrame(nsnull, &mTopFrame);
  return mTopFrame;
}

nsIFrame*
nsXULTreeGroupFrame::GetNextFrame(nsIFrame* aPrevFrame)
{
  nsIFrame* result;
  LocateFrame(aPrevFrame, &result);
  return result;
}

nsIFrame*
nsXULTreeGroupFrame::GetLastFrame()
{
  return mFrames.LastChild();
}

nsIBox* 
nsXULTreeGroupFrame::GetFirstTreeBox()
{
  // Clear ourselves out.
  mLinkupFrame = nsnull;
  mBottomFrame = mTopFrame;
  
  // If we have a frame and no content chain (e.g., unresolved/uncreated content)
  if (mTopFrame && !mContentChain) {
    nsCOMPtr<nsIBox> box(do_QueryInterface(mTopFrame));
    return box;
  }

  // See if we have any frame whatsoever.
  LocateFrame(nsnull, &mTopFrame);
  
  mBottomFrame = mTopFrame;

  nsCOMPtr<nsIContent> startContent;
  
  if (mContentChain) {
    nsCOMPtr<nsISupports> supports;
    mContentChain->GetElementAt(0, getter_AddRefs(supports));
    nsCOMPtr<nsIContent> chainContent = do_QueryInterface(supports);
    startContent = chainContent;

    if (mTopFrame) {  
      // We have a content chain. If the top frame is the same as our content
      // chain, we can go ahead and destroy our content chain and return the
      // top frame.
      nsCOMPtr<nsIContent> topContent;
      mTopFrame->GetContent(getter_AddRefs(topContent));
      if (chainContent.get() == topContent.get()) {
        // The two content nodes are the same.  Our content chain has
        // been synched up, and we can now remove our element and
        // pass the content chain inwards.
        InitSubContentChain((nsXULTreeGroupFrame*)mTopFrame);

        nsCOMPtr<nsIBox> box(do_QueryInterface(mTopFrame));
        return box;
      }
      else mLinkupFrame = mTopFrame; // We have some frames that we'll eventually catch up with.
                                     // Cache the pointer to the first of these frames, so
                                     // we'll know it when we hit it.
    }
  }
  else if (mTopFrame) {
    nsCOMPtr<nsIBox> box(do_QueryInterface(mTopFrame));
    return box;
  }

  // We don't have a top frame instantiated. Let's
  // try to make one.

  // If startContent is initialized, we have a content chain, and 
  // we're using that content node to make our frame.
  // Otherwise we have nothing, and we should just try to grab the first child.
  if (!startContent) {
    PRInt32 childCount;
    mContent->ChildCount(childCount);
    nsCOMPtr<nsIContent> childContent;
    if (childCount > 0) {
      mContent->ChildAt(0, *getter_AddRefs(childContent));
      startContent = childContent;
    }
  }

  if (startContent) {  
    PRBool isAppend = (mLinkupFrame == nsnull);

    mFrameConstructor->CreateTreeWidgetContent(mPresContext, this, nsnull, startContent,
                                               &mTopFrame, isAppend, PR_FALSE, 
                                               nsnull);
    
    mBottomFrame = mTopFrame;
    nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(mTopFrame));
    PRBool isGroup = PR_FALSE;
    if (slice)
      slice->IsGroupFrame(&isGroup);

    if (isGroup && mContentChain) {
      // We have just instantiated a row group, and we have a content chain. This
      // means we need to potentially pass a sub-content chain to the instantiated
      // frame, so that it can also sync up with its children.
      InitSubContentChain((nsXULTreeGroupFrame*)mTopFrame);
    }

    SetContentChain(nsnull);
    
    nsCOMPtr<nsIBox> box(do_QueryInterface(mTopFrame));
    return box;
  }
  return nsnull;

}

nsIBox* 
nsXULTreeGroupFrame::GetNextTreeBox(nsIBox* aBox)
{
  // We're ultra-cool. We build our frames on the fly.
  nsIFrame* result;
  nsIFrame* frame;
  aBox->GetFrame(&frame);
  LocateFrame(frame, &result);
  if (result && (result == mLinkupFrame)) {
    // We haven't really found a result. We've only found a result if
    // the linkup frame is really the next frame following the
    // previous frame.
    nsCOMPtr<nsIContent> prevContent;
    frame->GetContent(getter_AddRefs(prevContent));
    nsCOMPtr<nsIContent> linkupContent;
    mLinkupFrame->GetContent(getter_AddRefs(linkupContent));
    PRInt32 i, j;
    mContent->IndexOf(prevContent, i);
    mContent->IndexOf(linkupContent, j);
    if (i+1==j) {
      // We have found a match and successfully linked back up with our
      // old frame. 
      mBottomFrame = mLinkupFrame;
      mLinkupFrame = nsnull;
		  nsCOMPtr<nsIBox> box(do_QueryInterface(result));
      return box;
    }
    else result = nsnull; // No true linkup. We need to make a frame.
  }

  if (!result) {
    // No result found. See if there's a content node that wants a frame.
    PRInt32 i, childCount;
    nsCOMPtr<nsIContent> prevContent;
    frame->GetContent(getter_AddRefs(prevContent));
    nsCOMPtr<nsIContent> parentContent;
    mContent->IndexOf(prevContent, i);
    mContent->ChildCount(childCount);
    if (i+1 < childCount) {
      
      // There is a content node that wants a frame.
      nsCOMPtr<nsIContent> nextContent;
      mContent->ChildAt(i+1, *getter_AddRefs(nextContent));
      nsIFrame* prevFrame = nsnull; // Default is to append
      PRBool isAppend = PR_TRUE;
      if (mLinkupFrame) {
        // This will be an insertion, since we have frames on the end.
        aBox->GetFrame(&prevFrame);
        isAppend = PR_FALSE;
      }

      mFrameConstructor->CreateTreeWidgetContent(mPresContext, this, prevFrame, nextContent,
                                                 &result, isAppend, PR_FALSE,
                                                 nsnull);
    }
  }

  mBottomFrame = result;

  nsCOMPtr<nsIBox> box(do_QueryInterface(result));
  return box;
}

NS_IMETHODIMP
nsXULTreeGroupFrame::TreeInsertFrames(nsIFrame* aPrevFrame, nsIFrame* aFrameList)
{
  // insert the frames to our info list
  nsBoxLayoutState state(mPresContext);
  Insert(state, aPrevFrame, aFrameList);

  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGroupFrame::TreeAppendFrames(nsIFrame* aFrameList)
{
  // append them after
  nsBoxLayoutState state(mPresContext);
  Append(state,aFrameList);

  mFrames.AppendFrames(nsnull, aFrameList);
  return NS_OK;
}

void 
nsXULTreeGroupFrame::OnContentInserted(nsIPresContext* aPresContext, nsIFrame* aNextSibling, PRInt32 aIndex)
{
  nsIFrame* currFrame = aNextSibling;

  // this will probably never happen - 
  // if we haven't created the topframe, it doesn't matter if
  // content was inserted
  if (mTopFrame == nsnull) return;

  // if we're inserting content at the top of visible content,
  // then ignore it because it would go off-screen
  // except of course in the case of the first row, where we're
  // actually adding visible content
  if(aNextSibling == mTopFrame) {
    if (aIndex == 0)
      // it's the first row, blow away mTopFrame so it can be
      // crecreated later
      mTopFrame = nsnull;
    else
      // it's not visible, nothing to do
      return;
  }

  while (currFrame) {
    nsIFrame* nextFrame;
    currFrame->GetNextSibling(&nextFrame);
    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, currFrame, nsnull);
     
    nsBoxLayoutState state(aPresContext);
    Remove(state, currFrame);
    mFrames.DestroyFrame(aPresContext, currFrame);
    currFrame = nextFrame;
  }
  nsBoxLayoutState state(aPresContext);
  MarkDirtyChildren(state);
}

void nsXULTreeGroupFrame::OnContentRemoved(nsIPresContext* aPresContext, 
                                           nsIFrame* aChildFrame,
                                           PRInt32 aIndex)
{
  // if we're removing the top row, the new top row is the next row
  if (mTopFrame && mTopFrame == aChildFrame)
    mTopFrame->GetNextSibling(&mTopFrame);

  // if we're removing the last frame in this rowgroup and if we have
  // a scrollbar, we have to yank in some rows from above
 /* if (!mTopFrame && mScrollbar && mCurrentIndex > 0) {  
    // sync up the scrollbar, now that we've scrolled one row
     mCurrentIndex--;
     nsAutoString indexStr;
     PRInt32 pixelIndex = mCurrentIndex * SCROLL_FACTOR;
     indexStr.AppendInt(pixelIndex);
    
     nsCOMPtr<nsIContent> scrollbarContent;
     mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
     scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::curpos,
                                     indexStr, PR_TRUE);

     // Now force the reflow to happen immediately, because we need to
     // deal with cleaning out the content chain.
     nsCOMPtr<nsIPresShell> shell;
     aPresContext->GetShell(getter_AddRefs(shell));
     shell->FlushPendingNotifications();

     return; // All frames got deleted anyway by the pos change.
  }
  */

  // Go ahead and delete the frame.
  nsBoxLayoutState state(aPresContext);
  if (aChildFrame) {
    Remove(state, aChildFrame);
    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, aChildFrame, nsnull);
    mFrames.DestroyFrame(aPresContext, aChildFrame);

  }
  MarkDirtyChildren(state);
}

PRBool nsXULTreeGroupFrame::ContinueReflow(nscoord height) 
{ 
  if (height <= 0) {
    nsIFrame* lastChild = GetLastFrame();
    nsIFrame* startingPoint = mBottomFrame;
    if (startingPoint == nsnull) {
      // We just want to delete everything but the first item.
      startingPoint = GetFirstFrame();
    }

    if (lastChild != startingPoint) {
      // We have some hangers on (probably caused by shrinking the size of the window).
      // Nuke them.
      nsIFrame* currFrame;
      startingPoint->GetNextSibling(&currFrame);
      while (currFrame) {
        nsIFrame* nextFrame;
        currFrame->GetNextSibling(&nextFrame);
        mFrameConstructor->RemoveMappingsForFrameSubtree(mPresContext, currFrame, nsnull);
        
        nsBoxLayoutState state(mPresContext);
        Remove(state, currFrame);

        mFrames.DestroyFrame(mPresContext, currFrame);

        currFrame = nextFrame;
      }
    }
    return PR_FALSE;
  }
  else
    return PR_TRUE;
}


void nsXULTreeGroupFrame::DestroyRows(PRInt32& aRowsToLose) 
{
  // We need to destroy frames until our row count has been properly
  // reduced.  A reflow will then pick up and create the new frames.
  nsIFrame* childFrame = GetFirstFrame();
  while (childFrame && aRowsToLose > 0) {
    nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(childFrame));
    if (!slice) continue;
    PRBool isRowGroup;
    slice->IsGroupFrame(&isRowGroup);
    if (isRowGroup)
    {
      nsXULTreeGroupFrame* childGroup = (nsXULTreeGroupFrame*)childFrame;
      childGroup->DestroyRows(aRowsToLose);
      if (aRowsToLose == 0) {
        nsIBox* box = nsnull;
        childGroup->GetChildBox(&box);
        if (box) // Still have kids. Just return.
          return;
      }
    }
    else
    {
      // Lost a row.
      aRowsToLose--;
    }
    
    nsIFrame* nextFrame = GetNextFrame(childFrame);
    mFrameConstructor->RemoveMappingsForFrameSubtree(mPresContext, childFrame, nsnull);
    nsBoxLayoutState state(mPresContext);
    Remove(state, childFrame);
    mFrames.DestroyFrame(mPresContext, childFrame);
    mTopFrame = childFrame = nextFrame;
  }
}

void nsXULTreeGroupFrame::ReverseDestroyRows(PRInt32& aRowsToLose) 
{
  // We need to destroy frames until our row count has been properly
  // reduced.  A reflow will then pick up and create the new frames.
  nsIFrame* childFrame = GetLastFrame();
  while (childFrame && aRowsToLose > 0) {
    nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(childFrame));
    if (!slice) continue;
    PRBool isRowGroup;
    slice->IsGroupFrame(&isRowGroup);
    if (isRowGroup)
    {
      nsXULTreeGroupFrame* childGroup = (nsXULTreeGroupFrame*)childFrame;
      childGroup->ReverseDestroyRows(aRowsToLose);
      if (aRowsToLose == 0) {
        nsIBox* box = nsnull;
        childGroup->GetChildBox(&box);
        if (box) // Still have kids. Just return.
          return;
      }
    }
    else
    {
      // Lost a row.
      aRowsToLose--;
    }
    
    nsIFrame* prevFrame;
    prevFrame = mFrames.GetPrevSiblingFor(childFrame);
    mFrameConstructor->RemoveMappingsForFrameSubtree(mPresContext, childFrame, nsnull);
    nsBoxLayoutState state(mPresContext);
    Remove(state, childFrame);
    mFrames.DestroyFrame(mPresContext, childFrame);
    mBottomFrame = childFrame = prevFrame;
  }
}


void 
nsXULTreeGroupFrame::GetFirstRowContent(nsIContent** aResult)
{
  *aResult = nsnull;
  nsIFrame* kid = GetFirstFrame();
  while (kid) {
    nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(kid));
    if (!slice)
      continue;
    PRBool isRowGroup;
    slice->IsGroupFrame(&isRowGroup);
    if (isRowGroup) {
      ((nsXULTreeGroupFrame*)kid)->GetFirstRowContent(aResult);
      if (*aResult)
        return;
    }
    else {
      kid->GetContent(aResult); // The ADDREF happens here.
      return;
    }
    kid = GetNextFrame(kid);
  }
}

void nsXULTreeGroupFrame::SetContentChain(nsISupportsArray* aContentChain)
{
  NS_IF_RELEASE(mContentChain);
  mContentChain = aContentChain;
  NS_IF_ADDREF(mContentChain);
}

void nsXULTreeGroupFrame::InitSubContentChain(nsXULTreeGroupFrame* aRowGroupFrame)
{
  if (mContentChain) {
    mContentChain->RemoveElementAt(0);
    PRUint32 chainSize;
    mContentChain->Count(&chainSize);
    if (chainSize > 0 && aRowGroupFrame) {
      aRowGroupFrame->SetContentChain(mContentChain);
    }
    // The chain is dead. Long live the chain.
    SetContentChain(nsnull);
  }
}
