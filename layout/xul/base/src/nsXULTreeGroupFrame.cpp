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
 *   Mike Pinkerton (pinkerton@netscape.com)
 */

#include "nsCOMPtr.h"
#include "nsXULTreeGroupFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsBoxLayoutState.h"
#include "nsISupportsArray.h"
#include "nsTreeItemDragCapturer.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMElement.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsXULAtoms.h"
#include "nsIViewManager.h"
#include "nsINameSpaceManager.h"
#include "nsXULTreeOuterGroupFrame.h"
#include "nsIDocument.h"
#include "nsIBindingManager.h"


//
// Prototypes
//
static void LocateIndentationFrame ( nsIPresContext* aPresContext, nsIFrame* aParentFrame,
                                        nsIFrame** aResult ) ;

//
// NS_NewXULTreeGroupFrame
//
// Creates a new TreeGroup frame
//
nsresult
NS_NewXULTreeGroupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                        nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULTreeGroupFrame* it = new (aPresShell) nsXULTreeGroupFrame(aPresShell, aIsRoot, aLayoutManager);
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
nsXULTreeGroupFrame::nsXULTreeGroupFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
:nsBoxFrame(aPresShell, aIsRoot, aLayoutManager), mFrameConstructor(nsnull), mPresContext(nsnull),
 mOuterFrame(nsnull), mAvailableHeight(10000), mTopFrame(nsnull), mBottomFrame(nsnull), mLinkupFrame(nsnull),
 mContentChain(nsnull), mYDropLoc(nsTreeItemDragCapturer::kNoDropLoc), mDropOnContainer(PR_FALSE),
 mOnScreenRowCount(-1)
{}

// Destructor
nsXULTreeGroupFrame::~nsXULTreeGroupFrame()
{
  nsCOMPtr<nsIContent> content;
  GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));

  // NOTE: the last Remove will delete the drag capturer
  if ( receiver ) {
    receiver->RemoveEventListener(NS_ConvertASCIItoUCS2("dragover"), mDragCapturer, PR_TRUE);
    receiver->RemoveEventListener(NS_ConvertASCIItoUCS2("dragexit"), mDragCapturer, PR_TRUE);
  }
  
}


//
// Init
//
// Setup event capturers for drag and drop. Our frame's lifetime is bounded by the
// lifetime of the content model, so we're guaranteed that the content node won't go away on us. As
// a result, our drag capturer can't go away before the frame is deleted. Since the content
// node holds owning references to our drag capturer, which we tear down in the dtor, there is no 
// need to hold an owning ref to it ourselves.
//
NS_IMETHODIMP
nsXULTreeGroupFrame::Init ( nsIPresContext* aPresContext, nsIContent* aContent,
                             nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow )
{
  nsresult retVal = nsBoxFrame::Init ( aPresContext, aContent, aParent, aContext, aPrevInFlow );
  
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(aContent));
  if ( NS_SUCCEEDED(retVal) && receiver ) {
    // register our drag over and exit capturers. These annotate the content object
    // with enough info to determine where the drop would happen so that JS down the 
    // line can do the right thing.
    nsresult rv1, rv2;
    mDragCapturer = new nsTreeItemDragCapturer(this, aPresContext);
    rv1 = receiver->AddEventListener(NS_ConvertASCIItoUCS2("dragover"), mDragCapturer, PR_TRUE);
    rv2 = receiver->AddEventListener(NS_ConvertASCIItoUCS2("dragexit"), mDragCapturer, PR_TRUE);
    NS_ASSERTION ( NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2), "Couldn't hookup drag capturer on tree" );
  }
  
  // it's ok to ignore the failure of the event listeners. Nothing bad will come of it.
  return retVal;
  
} // Init

NS_IMETHODIMP
nsXULTreeGroupFrame::Redraw(nsBoxLayoutState& aState,
                            const nsRect*   aDamageRect,
                            PRBool          aImmediate)
{
  return nsBoxFrame::Redraw(aState, aDamageRect, aImmediate);
}

void nsXULTreeGroupFrame::LocateFrame(nsIFrame* aStartFrame, nsIFrame** aResult)
{
  if (aStartFrame == nsnull)
    *aResult = mFrames.FirstChild();
  else aStartFrame->GetNextSibling(aResult);
}

NS_IMETHODIMP
nsXULTreeGroupFrame::NeedsRecalc()
{
  mOnScreenRowCount = -1;
  return nsBoxFrame::NeedsRecalc();
}

NS_IMETHODIMP
nsXULTreeGroupFrame::GetOnScreenRowCount(PRInt32* aCount)
{
  if (mOnScreenRowCount == -1)
  {
    mOnScreenRowCount = 0;
    nsIBox* box = nsnull;
    GetChildBox(&box);
    while(box) {
      PRInt32 count = 0;
      nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(box));
      slice->GetOnScreenRowCount(&count);
      mOnScreenRowCount += count;
      box->GetNextBox(&box);
    }
  }

  *aCount = mOnScreenRowCount;
  return NS_OK;
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
nsXULTreeGroupFrame::GetFirstTreeBox(PRBool* aCreated)
{

  if (aCreated)
     *aCreated = PR_FALSE;

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
    nsCOMPtr<nsIContent> content(mContent);
    nsCOMPtr<nsIContent> bindingParent;
    mContent->GetBindingParent(getter_AddRefs(bindingParent));
    if (bindingParent) {
      nsCOMPtr<nsIDocument> doc;
      bindingParent->GetDocument(*getter_AddRefs(doc));
      nsCOMPtr<nsIBindingManager> bindingManager;
      doc->GetBindingManager(getter_AddRefs(bindingManager));
      nsCOMPtr<nsIAtom> tag;
      PRInt32 namespaceID;
      bindingManager->ResolveTag(bindingParent, &namespaceID, getter_AddRefs(tag));
      if (tag.get() == nsXULAtoms::tree)
        content = bindingParent;
    }

    PRInt32 childCount;
    content->ChildCount(childCount);
    nsCOMPtr<nsIContent> childContent;
    if (childCount > 0) {
      content->ChildAt(0, *getter_AddRefs(childContent));
      startContent = childContent;
    }
  }

  if (startContent) {  
    PRBool isAppend = (mLinkupFrame == nsnull);

    mFrameConstructor->CreateTreeWidgetContent(mPresContext, this, nsnull, startContent,
                                               &mTopFrame, isAppend, PR_FALSE, 
                                               nsnull);

    if (aCreated)
      *aCreated = PR_TRUE;

    //if (mTopFrame)
    //  mOuterFrame->PostReflowCallback();

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
nsXULTreeGroupFrame::GetNextTreeBox(nsIBox* aBox, PRBool* aCreated)
{
  if (aCreated)
      *aCreated = PR_FALSE;

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
    prevContent->GetParent(*getter_AddRefs(parentContent));
    parentContent->IndexOf(prevContent, i);
    parentContent->ChildCount(childCount);
    if (i+1 < childCount) {
      
      // There is a content node that wants a frame.
      nsCOMPtr<nsIContent> nextContent;
      parentContent->ChildAt(i+1, *getter_AddRefs(nextContent));
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

      if (aCreated)
         *aCreated = PR_TRUE;

      //if (result)
      //  mOuterFrame->PostReflowCallback();
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
  MarkDirtyChildren(state);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeGroupFrame::TreeAppendFrames(nsIFrame* aFrameList)
{
  // append them after
  nsBoxLayoutState state(mPresContext);
  Append(state,aFrameList);
  mFrames.AppendFrames(nsnull, aFrameList);
  MarkDirtyChildren(state);

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

  // if we're inserting the item before the first visible content,
  // then ignore it because it will end up off-screen
  // (except of course in the case of the first row, where we're
  // actually adding visible content)
  if(aNextSibling == mTopFrame) {
    if (aIndex > 0) // We aren't at the front, so we have to be offscreen.
      return;       // Just bail.
    
    nsCOMPtr<nsIContent> content;
    aNextSibling->GetContent(getter_AddRefs(content));
    PRInt32 siblingIndex;
    mContent->IndexOf(content, siblingIndex);
    
    if (siblingIndex == 1 && mOuterFrame->GetYPosition() == 0)
      // We just inserted an item in front of the first of our children
      // and we're at the top, such that we have to show the row.
      // This item is our new visible top row.
      mTopFrame = nsnull;
    else
      // The newly inserted row is offscreen.  We can just bail.
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
                                           PRInt32 aIndex, PRInt32& aOnScreenRowCount)
{
  // if we're removing the top row, the new top row is the next row
  if (mTopFrame && mTopFrame == aChildFrame)
    mTopFrame->GetNextSibling(&mTopFrame);

  // Go ahead and delete the frame.
  nsBoxLayoutState state(aPresContext);
  if (aChildFrame) {
    nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(aChildFrame));
    if (slice)
      slice->GetOnScreenRowCount(&aOnScreenRowCount);

    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, aChildFrame, nsnull);

    Remove(state, aChildFrame);
    mFrames.DestroyFrame(aPresContext, aChildFrame);
  }

  MarkDirtyChildren(state);
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  shell->FlushPendingNotifications(PR_FALSE);
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
      nsBoxLayoutState state(mPresContext);

      while (currFrame) {
        nsIFrame* nextFrame;
        currFrame->GetNextSibling(&nextFrame);
        mFrameConstructor->RemoveMappingsForFrameSubtree(mPresContext, currFrame, nsnull);
        
        Remove(state, currFrame);

        mFrames.DestroyFrame(mPresContext, currFrame);

        currFrame = nextFrame;
      }

      MarkDirtyChildren(state);
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
    MarkDirtyChildren(state);

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
    MarkDirtyChildren(state);

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


#ifdef XP_MAC
#pragma mark -
#endif


//
// [ Drag And Drop Methods ]
//
// These will hopefully all go away (and their 20K of extra code) when we convert over
// to using the style system to do drop feedback.
//


//
// ForceDrawFrame
//
// Force this frame to drop and give me twenty.
// NOTE: try to consolodate with the version in the toolbar code.
//
void
nsXULTreeGroupFrame :: ForceDrawFrame(nsIPresContext* aPresContext, nsIFrame * aFrame)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(aPresContext, pnt, &view);
  aFrame->GetRect(rect);
  rect.x = pnt.x;
  rect.y = pnt.y;
  if (view) {
    nsCOMPtr<nsIViewManager> viewMgr;
    view->GetViewManager(*getter_AddRefs(viewMgr));
    if (viewMgr)
      viewMgr->UpdateView(view, rect, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_IMMEDIATE);
  }

} // ForceDrawFrame


//
// AttributeChanged
//
// Track several attributes set by the d&d drop feedback tracking mechanism. The first
// is the "dd-triggerrepaint" attribute so JS can trigger a repaint when it
// needs up update the drop feedback. The second is the x (or y, if bar is vertical) 
// coordinate of where the drop feedback bar should be drawn.
//
NS_IMETHODIMP
nsXULTreeGroupFrame::AttributeChanged(nsIPresContext* aPresContext, nsIContent* aChild,
                                      PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                                      PRInt32 aModType, PRInt32 aHint)
{
  nsresult rv = NS_OK;
  PRInt32 ignore;
  
  if ( aAttribute == nsXULAtoms::ddTriggerRepaint )
    ForceDrawFrame ( aPresContext, this );
  else if ( aAttribute == nsXULAtoms::ddTriggerRepaintRestore ) {
    // Repaint the entire tree with no special attributes set.
    ForceDrawFrame ( aPresContext, NS_REINTERPRET_CAST(nsIFrame*, mOuterFrame) );
  }
  else if ( aAttribute == nsXULAtoms::ddDropLocationCoord ) {
    nsAutoString attribute;
    aChild->GetAttr ( kNameSpaceID_None, aAttribute, attribute );
    mYDropLoc = attribute.ToInteger(&ignore);
  }
  else if ( aAttribute == nsXULAtoms::ddDropOn ) {
    nsAutoString attribute;
    aChild->GetAttr ( kNameSpaceID_None, aAttribute, attribute );
    attribute.ToLowerCase();
    mDropOnContainer = attribute.EqualsWithConversion("true");
  }
  else
    rv = nsBoxFrame::AttributeChanged ( aPresContext, aChild, aNameSpaceID, aAttribute, aModType, aHint );

  return rv;
  
} // AttributeChanged


//
// Paint
//
// Used to draw the drop feedback based on attributes set by the drag event capturer
//
NS_IMETHODIMP
nsXULTreeGroupFrame :: Paint ( nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect, nsFramePaintLayer aWhichLayer, PRUint32 aFlags)
{
  nsresult res = NS_OK;
  res = nsBoxFrame::Paint ( aPresContext, aRenderingContext, aDirtyRect, aWhichLayer );
  
  if ( (aWhichLayer == eFramePaintLayer_Content) &&
        (mYDropLoc != nsTreeItemDragCapturer::kNoDropLoc || mDropOnContainer) )
    PaintDropFeedback ( aPresContext, aRenderingContext, PR_FALSE );

  return res;
  
} // Paint


//
// PaintDropFeedback
//
// Depending on the various flags, dispatch to the appropriate method for drawing
// drop feedback.
//
void
nsXULTreeGroupFrame :: PaintDropFeedback ( nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
                                             PRBool aPaintSorted )
{
  // lookup the drop marker color. default to black if not found.
  nscolor color = GetColorFromStyleContext ( aPresContext, nsXULAtoms::ddDropMarker, NS_RGB(0,0,0) );
  
  // find the twips-to-pixels conversion. We have decided not to cache this for
  // space reasons.
  float p2t = 20.0;
  nsCOMPtr<nsIDeviceContext> dc;
  aRenderingContext.GetDeviceContext ( *getter_AddRefs(dc) );
  if ( dc )
    dc->GetDevUnitsToTwips ( p2t );
  
  if ( aPaintSorted )
    PaintSortedDropFeedback ( color, aRenderingContext, p2t );
  else if ( !mOuterFrame->IsTreeSorted() ) {
    // draw different drop feedback depending on if we are dropping on the 
    // container or above/below it
    if ( !mDropOnContainer )
      PaintInBetweenDropFeedback ( color, aRenderingContext, aPresContext, p2t );
    else
      PaintOnContainerDropFeedback ( color, aRenderingContext, aPresContext, p2t );
  } // else tree not sorted

} // PaintDropFeedback


//
// PaintSortedDropFeedback
//
// Draws the drop feedback for when the tree is sorted, so line-item drop feedback is
// not appliable.
//
void
nsXULTreeGroupFrame :: PaintSortedDropFeedback ( nscolor inColor, nsIRenderingContext& inRenderingContext,
                                                   float & inPixelsToTwips )
{
  // two pixels wide
  const PRInt32 borderWidth = NSToIntRound(2 * inPixelsToTwips);
  
  nsRect top ( 0, 0, mRect.width, borderWidth );
  nsRect left ( 0, 0, borderWidth, mRect.height );
  nsRect right ( mRect.width - borderWidth, 0, borderWidth, mRect.height );
  nsRect bottom ( 0, mRect.height - borderWidth, mRect.width, borderWidth );
  
  inRenderingContext.SetColor(inColor);
  inRenderingContext.FillRect ( top );
  inRenderingContext.FillRect ( left );
  inRenderingContext.FillRect ( bottom );
  inRenderingContext.FillRect ( right );
 
} // PaintSortedDropFeedback


//
// PaintOnContainerDropFeedback
//
// Draws the drop feedback for when the row is a container and it is open. We let
// CSS handle the operation of painting the row bg so it's skinnable.
//
void
nsXULTreeGroupFrame :: PaintOnContainerDropFeedback ( nscolor inColor, nsIRenderingContext& inRenderingContext,
                                                         nsIPresContext* inPresContext, float & inPixelsToTwips )
{
  if ( IsOpenContainer() ) {
    PRInt32 horizIndent = 0;
    nsIFrame* firstChild = nsnull;
    FindFirstChildTreeItemFrame ( inPresContext, &firstChild );
    if ( firstChild )
      horizIndent = FindIndentation(inPresContext, firstChild);

    inRenderingContext.SetColor(inColor);
    nsRect dividingLine ( horizIndent, mRect.height - NSToIntRound(2 * inPixelsToTwips), 
                            NSToIntRound(50 * inPixelsToTwips), NSToIntRound(2 * inPixelsToTwips) );
    inRenderingContext.DrawRect ( dividingLine );
  }

} // PaintOnContainerDropFeedback


//
// PaintInBetweenDropFeedback
//
// Draw the feedback for when the drop is to go in between two nodes, but only if the tree
// allows that.
//
void
nsXULTreeGroupFrame :: PaintInBetweenDropFeedback ( nscolor inColor, nsIRenderingContext& inRenderingContext,
                                                      nsIPresContext* inPresContext, float & inPixelsToTwips )
{
  if ( mOuterFrame->CanDropBetweenRows() ) {
    // the normal case is that we can just look at this frame to find the indentation we need. However,
    // when we're an _open container_ and are being asked to draw the line _after_, we need to use the
    // indentation of our first child instead. ick.
    PRInt32 horizIndent = 0;
    if ( IsOpenContainer() && mYDropLoc > 0 ) {
      nsIFrame* firstChild = nsnull;
      FindFirstChildTreeItemFrame ( inPresContext, &firstChild );
      if ( firstChild )
        horizIndent = FindIndentation(inPresContext, firstChild);
    } // if open container and drop after
    else
      horizIndent = FindIndentation(inPresContext, this);
    
    inRenderingContext.SetColor(inColor);
    nsRect dividingLine ( horizIndent, mYDropLoc, 
                            NSToIntRound(50 * inPixelsToTwips), NSToIntRound(2 * inPixelsToTwips) );
    inRenderingContext.FillRect(dividingLine);
  }
  
} // PaintInBetweenDropFeedback


//
// FindIndentation
//
// Compute horizontal offset for dividing line by finding a treeindentation tag
// and using its right coordinate.
//
// NOTE: We assume this indentation tag is in the first column.
// NOTE: We aren't caching this value because of space reasons....
//  
PRInt32
nsXULTreeGroupFrame :: FindIndentation ( nsIPresContext* inPresContext, nsIFrame* inStartFrame ) const
{
  PRInt32 indentInTwips = 0;
  
  if ( !inStartFrame ) return 0;
  nsIFrame* treeRowFrame;
  inStartFrame->FirstChild ( inPresContext, nsnull, &treeRowFrame );
  if ( !treeRowFrame ) return 0;
  nsIFrame* treeCellFrame;
  treeRowFrame->FirstChild ( inPresContext, nsnull, &treeCellFrame );
  if ( !treeCellFrame ) return 0;

  nsIFrame* treeIndentFrame = nsnull;
  LocateIndentationFrame ( inPresContext, treeCellFrame, &treeIndentFrame );
  if ( treeIndentFrame ) {
    nsRect treeIndentBounds;
    treeIndentFrame->GetRect ( treeIndentBounds );
    indentInTwips = treeIndentBounds.x + treeIndentBounds.width;  
  }
  return indentInTwips;

} // FindIndentation


//
// LocateIndentationFrame
//
// Recursively locate the <treeindentation> tag
//
void
LocateIndentationFrame ( nsIPresContext* aPresContext, nsIFrame* aParentFrame,
                            nsIFrame** aResult)
{
  // Check ourselves.
  *aResult = nsnull;
  nsCOMPtr<nsIContent> content;
  aParentFrame->GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIAtom> tagName;
  content->GetTag ( *getter_AddRefs(tagName) );
  if ( tagName.get() == nsXULAtoms::treeindentation ) {
    *aResult = aParentFrame;
    return;
  }

  // Check our kids.
  nsIFrame* currFrame;
  aParentFrame->FirstChild(aPresContext, nsnull, &currFrame);
  while (currFrame) {
    LocateIndentationFrame(aPresContext, currFrame, aResult);
    if (*aResult)
      return;
    currFrame->GetNextSibling(&currFrame);
  }
}


//
// FindFirstChildTreeItemFrame
//
// Locates the first <treeitem> in our child list. Assumes that we are a container and that 
// the children are visible.
//
void
nsXULTreeGroupFrame :: FindFirstChildTreeItemFrame ( nsIPresContext* inPresContext, 
                                                      nsIFrame** outChild ) const
{
  *outChild = nsnull;
  
  // first find the <treechildren> tag in our child list.
  nsIFrame* currChildFrame = nsnull;
  FirstChild ( inPresContext, nsnull, &currChildFrame );
  while ( currChildFrame ) {
    nsCOMPtr<nsIContent> content;
    currChildFrame->GetContent ( getter_AddRefs(content) );
    nsCOMPtr<nsIAtom> tagName;
    content->GetTag ( *getter_AddRefs(tagName) );
    if ( tagName.get() == nsXULAtoms::treechildren )
      break;
    currChildFrame->GetNextSibling ( &currChildFrame );
  } // foreach child of the treeItem
  //NS_ASSERTION ( currChildFrame, "Can't find <treechildren>" );
  
  // |currChildFrame| now holds the correct frame if we found it
  if ( currChildFrame )
    currChildFrame->FirstChild ( inPresContext, nsnull, outChild );

} // FindFirstChildTreeItemFrame


//
// IsOpenContainer
//
// Determine if a node is both a container and open
//
PRBool
nsXULTreeGroupFrame :: IsOpenContainer ( ) const
{
  PRBool isOpenContainer = PR_FALSE;
  
  nsCOMPtr<nsIDOMElement> me ( do_QueryInterface(mContent) );
  if ( me ) {
    nsAutoString isContainer, isOpen;
    me->GetAttribute(NS_ConvertASCIItoUCS2("container"), isContainer);
    me->GetAttribute(NS_ConvertASCIItoUCS2("open"), isOpen);
    isOpenContainer = (isContainer.EqualsWithConversion("true") && isOpen.EqualsWithConversion("true"));
  }

  return isOpenContainer;
  
} // IsOpenContainer


//
// GetColorFromStyleContext
//
// A little helper to root out a color from the current style context. Returns
// the given default color if we can't find it, for whatever reason.
//
nscolor
nsXULTreeGroupFrame :: GetColorFromStyleContext ( nsIPresContext* inPresContext, nsIAtom* inAtom, 
                                                    nscolor inDefaultColor )
{
  nscolor retColor = inDefaultColor;
  
  // go looking for the psuedo-style. We have decided not to cache this for space reasons.
  nsCOMPtr<nsIStyleContext> markerStyle;
  inPresContext->ProbePseudoStyleContextFor(mContent, inAtom, mStyleContext,
                                              PR_FALSE, getter_AddRefs(markerStyle));

  // dig out the color we want.
  if ( markerStyle ) {
    const nsStyleColor* styleColor = 
               NS_STATIC_CAST(const nsStyleColor*, markerStyle->GetStyleData(eStyleStruct_Color));
    retColor = styleColor->mColor;
  }

  return retColor;
  
} // GetColorFromStyleContext


