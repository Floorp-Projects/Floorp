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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Eric D Vaughan <evaughan@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Dan Rosen <dr@netscape.com>
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

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIPresContext.h"
#include "nsIDeviceContext.h"
#include "nsPageFrame.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsIScrollableView.h"
#include "nsWidgetsCID.h"
#include "nsScrollBoxFrame.h"
#include "nsLayoutAtoms.h"
#include "nsIBox.h"
#include "nsBoxLayoutState.h"
#include "nsIBoxToBlockAdaptor.h"
#include "nsIScrollbarMediator.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIPresState.h"
#include "nsButtonBoxFrame.h"
#include "nsITimer.h"
#include "nsRepeatService.h"

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kScrollBoxViewCID, NS_SCROLL_PORT_VIEW_CID);


//----------------------------------------------------------------------

nsresult
NS_NewScrollBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsScrollBoxFrame* it = new (aPresShell) nsScrollBoxFrame (aPresShell);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsScrollBoxFrame::nsScrollBoxFrame(nsIPresShell* aShell):nsBoxFrame(aShell), mVerticalOverflow(PR_FALSE), mHorizontalOverflow(PR_FALSE),
     mRestoreRect(-1, -1, -1, -1),
     mLastPos(-1, -1)
{
}


PRBool
nsScrollBoxFrame::NeedsClipWidget()
{
  return PR_TRUE;
}

NS_IMETHODIMP
nsScrollBoxFrame::Init(nsIPresContext*  aPresContext,
                    nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsStyleContext*  aStyleContext,
                    nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent,
                                            aParent, aStyleContext,
                                            aPrevInFlow);

  // Create the scrolling view
  CreateScrollingView(aPresContext);
  return rv;
}
  
NS_IMETHODIMP
nsScrollBoxFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName,
                                                           aChildList);

  SetUpScrolledFrame(aPresContext);

  return rv;
}

void
nsScrollBoxFrame::SetUpScrolledFrame(nsIPresContext* aPresContext)
{
  NS_ASSERTION(mFrames.GetLength() <= 1, "ScrollBoxes can only have 1 child!");


  nsIFrame* frame = mFrames.FirstChild();

  if (!frame)
     return;

  // create a view if we don't already have one.
  nsHTMLContainerFrame::CreateViewForFrame(frame, nsnull, PR_TRUE);
}

NS_IMETHODIMP
nsScrollBoxFrame::AppendFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  nsresult rv = nsBoxFrame::AppendFrames(aPresContext, aPresShell, aListName, aFrameList);

  // make sure we only have 1 child.
  NS_ASSERTION(!mFrames.FirstChild()->GetNextSibling(), "Error ScrollBoxes can only have 1 child");

  SetUpScrolledFrame(aPresContext);

  return rv;
}

NS_IMETHODIMP
nsScrollBoxFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aPrevFrame,
                            nsIFrame*       aFrameList)
{
  nsresult rv = nsBoxFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame, aFrameList);

  // make sure we only have 1 child.
  NS_ASSERTION(!mFrames.FirstChild()->GetNextSibling(), "Error ScrollBoxes can only have 1 child");

  SetUpScrolledFrame(aPresContext);

  return rv;
}

NS_IMETHODIMP
nsScrollBoxFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame)
{
  nsresult rv = nsBoxFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);

  SetUpScrolledFrame(aPresContext);

  return rv;
}

nsresult
nsScrollBoxFrame::CreateScrollingViewWidget(nsIView* aView, const nsStyleDisplay* aDisplay)
{
  nsresult rv = NS_OK;
   // If it's fixed positioned, then create a widget 
  if (NS_STYLE_POSITION_FIXED == aDisplay->mPosition) {
    rv = aView->CreateWidget(kWidgetCID);
  }

  return(rv);
}

nsresult
nsScrollBoxFrame::GetScrollingParentView(nsIPresContext* aPresContext,
                                          nsIFrame* aParent,
                                          nsIView** aParentView)
{
  *aParentView = aParent->GetView();
  NS_ASSERTION(*aParentView, "GetParentWithView failed");
  return NS_OK;
}

nsresult
nsScrollBoxFrame::CreateScrollingView(nsIPresContext* aPresContext)
{
  nsIView*  view;

   //Get parent frame
  nsIFrame* parent = GetAncestorWithView();
  NS_ASSERTION(parent, "GetParentWithView failed");

  // Get parent view
  nsIView* parentView = nsnull;
  GetScrollingParentView(aPresContext, parent, &parentView);
 
  // Get the view manager
  nsIViewManager* viewManager = parentView->GetViewManager();

  // Create the scrolling view
  nsresult rv = CallCreateInstance(kScrollBoxViewCID, &view);

  if (NS_SUCCEEDED(rv)) {
    // Initialize the scrolling view
    view->Init(viewManager, mRect, parentView);

    SyncFrameViewProperties(aPresContext, this, mStyleContext, view);

    // Insert the view into the view hierarchy
    // XXX Put view last in document order until we know how to do better
    viewManager->InsertChild(parentView, view, nsnull, PR_TRUE);

    // If it's fixed positioned, then create a widget too
    CreateScrollingViewWidget(view, GetStyleDisplay());

    // Get the nsIScrollableView interface
    nsIScrollableView* scrollingView;
    CallQueryInterface(view, &scrollingView);

    scrollingView->SetScrollPreference(nsScrollPreference_kNeverScroll);

    // Have the scrolling view create its internal widgets
    if (NeedsClipWidget()) {
      scrollingView->CreateScrollControls(); 
    }

    // Remember our view
    SetView(view);
  }
  return rv;
}

NS_IMETHODIMP
nsScrollBoxFrame::GetMargin(nsMargin& aMargin)
{
   aMargin.SizeTo(0,0,0,0);
   return NS_OK;
}

NS_IMETHODIMP
nsScrollBoxFrame::GetPadding(nsMargin& aMargin)
{
   aMargin.SizeTo(0,0,0,0);
   return NS_OK;
}

NS_IMETHODIMP
nsScrollBoxFrame::GetBorder(nsMargin& aMargin)
{
   aMargin.SizeTo(0,0,0,0);
   return NS_OK;
}

NS_IMETHODIMP
nsScrollBoxFrame::DoLayout(nsBoxLayoutState& aState)
{
  PRUint32 oldflags = aState.LayoutFlags();

  nsRect clientRect(0,0,0,0);
  GetClientRect(clientRect);
  nsIBox* kid = nsnull;
  GetChildBox(&kid);
  nsRect childRect(clientRect);
  nsMargin margin(0,0,0,0);
  kid->GetMargin(margin);
  childRect.Deflate(margin);

  nsIPresContext* presContext = aState.PresContext();

  // see if our child is html. If it is then
  // never include the overflow. The child will be the size
  // given but its view will include the overflow size.
  nsCOMPtr<nsIBoxToBlockAdaptor> adaptor = do_QueryInterface(kid);
  if (adaptor)
    adaptor->SetIncludeOverflow(PR_FALSE);

  PRInt32 flags = NS_FRAME_NO_MOVE_VIEW;

  // do we have an adaptor? No then we can't use
  // min size the child technically can get as small as it wants
  // to.
  if (!adaptor) {
    nsSize min(0,0);
    kid->GetMinSize(aState, min);

    if (min.height > childRect.height)
       childRect.height = min.height;

    if (min.width > childRect.width)
       childRect.width = min.width;
  } else { 
    // don't size the view if we have an adaptor
    flags |=  NS_FRAME_NO_SIZE_VIEW;
  }

  aState.SetLayoutFlags(flags);
  kid->SetBounds(aState, childRect);
  kid->Layout(aState);

  kid->GetBounds(childRect);

  clientRect.Inflate(margin);

    // now size the view to the size including our overflow.
  if (adaptor) {
     nsSize overflow(0,0);
     adaptor->GetOverflow(overflow);
     childRect.width = overflow.width;
     childRect.height = overflow.height;
  }

  if (childRect.width < clientRect.width || childRect.height < clientRect.height)
  {
    if (childRect.width < clientRect.width)
       childRect.width = clientRect.width;

    if (childRect.height < clientRect.height)
       childRect.height = clientRect.height;

    clientRect.Deflate(margin);

    // remove overflow area when we update the bounds,
    // because we've already accounted for it
    kid->SetBounds(aState, childRect, PR_TRUE);
  }

  aState.SetLayoutFlags(oldflags);

  SyncLayout(aState);

  if (adaptor) {
     nsIFrame* frame;
     kid->GetFrame(&frame);

     nsRect r(0, 0, childRect.width, childRect.height);
     nsContainerFrame::SyncFrameViewAfterReflow(presContext, frame,
       frame->GetView(), &r, NS_FRAME_NO_MOVE_VIEW);
  }

  nsIView* view = GetView();
  nsIScrollableView* scrollingView;
  if (NS_SUCCEEDED(CallQueryInterface(view, &scrollingView))) {
    scrollingView->ComputeScrollOffsets(PR_TRUE);
  }

  nsRect scrollPort;
  GetBounds(scrollPort);

  kid->GetBounds(childRect);

  // first see what changed
  PRBool vertChanged = PR_FALSE;
  PRBool horizChanged = PR_FALSE;

  if (mVerticalOverflow && childRect.height <= scrollPort.height) {
    mVerticalOverflow = PR_FALSE;
    vertChanged = PR_TRUE;
  } else if (childRect.height > scrollPort.height) {
    if (!mVerticalOverflow) {
       mVerticalOverflow = PR_TRUE;
    }
    vertChanged = PR_TRUE;
  }

  if (mHorizontalOverflow && childRect.width <= scrollPort.width) {
    mHorizontalOverflow = PR_FALSE;
    horizChanged = PR_TRUE;
  } else if (childRect.width > scrollPort.width) {
    if (!mHorizontalOverflow) {
      mHorizontalOverflow = PR_TRUE;
    }
    horizChanged = PR_TRUE;
  }

  nsCOMPtr<nsIPresShell> shell = aState.PresShell();

  // if either changed
  if (vertChanged || horizChanged) 
  {
    // are there 2 events or 1?
    if (vertChanged && horizChanged) {
      if (mVerticalOverflow == mHorizontalOverflow)
      {
        // both either overflowed or underflowed. 1 event
        PostScrollPortEvent(shell, mVerticalOverflow, nsScrollPortEvent::both);
      } else {
        // one overflowed and one underflowed
        PostScrollPortEvent(shell, mVerticalOverflow, nsScrollPortEvent::vertical);
        PostScrollPortEvent(shell, mHorizontalOverflow, nsScrollPortEvent::horizontal);
      }
    } else if (vertChanged) // only one changed either vert or horiz
       PostScrollPortEvent(shell, mVerticalOverflow, nsScrollPortEvent::vertical);
    else
       PostScrollPortEvent(shell, mHorizontalOverflow, nsScrollPortEvent::horizontal);
  }

  /**
   * this code is resposible for restoring the scroll position back to some
   * saved positon. if the user has not moved the scroll position manually
   * we keep scrolling down until we get to our orignally position. keep in
   * mind that content could incrementally be coming in. we only want to stop
   * when we reach our new position.
   */

  if ((mRestoreRect.y != -1) &&
      (mLastPos.x != -1) &&
      (mLastPos.y != -1)) {

    // make sure our scroll position did not change for where we last put
    // it. if it does then the user must have moved it, and we no longer
    // need to restore.
    if (!view)
      return NS_OK; // don't freak out if we have no view

    nsIScrollableView* scrollingView;
    CallQueryInterface(view, &scrollingView);
    if (scrollingView) {
      nscoord x = 0;
      nscoord y = 0;
      scrollingView->GetScrollPosition(x, y);

      // if we didn't move, we still need to restore
      if ((x == mLastPos.x) && (y == mLastPos.y)) {
        nsRect childRect(0, 0, 0, 0);
        nsIView* child = nsnull;
        nsresult rv = scrollingView->GetScrolledView(child);
        if (NS_SUCCEEDED(rv) && child)
          childRect = child->GetBounds();

        PRInt32 cx, cy, x, y;
        scrollingView->GetScrollPosition(cx,cy);

        x = (int)
          (((float)childRect.width / mRestoreRect.width) * mRestoreRect.x);
        y = (int)
          (((float)childRect.height / mRestoreRect.height) * mRestoreRect.y);

        // if our position is greater than the scroll position, scroll.
        // remember that we could be incrementally loading so we may enter
        // and scroll many times.
        if ((y > cy) || (x > cx)) {
          scrollingView->ScrollTo(x, y, 0);
          // scrollpostion goes from twips to pixels. this fixes any roundoff
          // problems.
          scrollingView->GetScrollPosition(mLastPos.x, mLastPos.y);
        }
        else {
          // if we reached the position then stop
          mRestoreRect.y = -1; 
          mLastPos.x = -1;
          mLastPos.y = -1;
        }
      }
      else {
        // user moved the position, so we won't need to restore
        mLastPos.x = -1;
        mLastPos.y = -1;
      }
    }
  }

  return NS_OK;
}

void
nsScrollBoxFrame::PostScrollPortEvent(nsIPresShell* aShell, PRBool aOverflow, nsScrollPortEvent::orientType aType)
{
  if (!mContent)
    return;

  nsScrollPortEvent* event = new nsScrollPortEvent(aOverflow ?
                                                   NS_SCROLLPORT_OVERFLOW :
                                                   NS_SCROLLPORT_UNDERFLOW);
  event->orient = aType;
  aShell->PostDOMEvent(mContent, event);
}


NS_IMETHODIMP
nsScrollBoxFrame::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  aAscent = 0;
  nsIBox* child = nsnull;
  GetChildBox(&child);
  nsresult rv = child->GetAscent(aState, aAscent);
  nsMargin m(0,0,0,0);
  GetBorderAndPadding(m);
  aAscent += m.top;
  GetMargin(m);
  aAscent += m.top;
  GetInset(m);
  aAscent += m.top;

  return rv;
}

NS_IMETHODIMP
nsScrollBoxFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsIBox* child = nsnull;
  GetChildBox(&child);
  nsresult rv = child->GetPrefSize(aBoxLayoutState, aSize);
  AddMargin(child, aSize);
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSPrefSize(aBoxLayoutState, this, aSize);
  return rv;
}

NS_IMETHODIMP
nsScrollBoxFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  aSize.width = 0;
  aSize.height = 0;
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMinSize(aBoxLayoutState, this, aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsScrollBoxFrame::GetMaxSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  nsIBox* child = nsnull;
  GetChildBox(&child);
  nsresult rv = child->GetMaxSize(aBoxLayoutState, aSize);
  AddMargin(child, aSize);
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMaxSize(aBoxLayoutState, this, aSize);
  return rv;
}

NS_IMETHODIMP
nsScrollBoxFrame::Paint(nsIPresContext*      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect,
                        nsFramePaintLayer    aWhichLayer,
                        PRUint32             aFlags)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // Only paint the border and background if we're visible

    if (GetStyleVisibility()->IsVisibleOrCollapsed()) {
      // Paint our border only (no background)
      const nsStyleBorder* border = GetStyleBorder();

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *border, mStyleContext, 0);
    }
  }

  // Paint our children
  nsBoxFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  // Call nsFrame::Paint to draw selection border when appropriate
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

nsresult 
nsScrollBoxFrame::GetContentOf(nsIContent** aContent)
{
  *aContent = GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

nsIAtom*
nsScrollBoxFrame::GetType() const
{
  return nsLayoutAtoms::scrollFrame;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsScrollBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ScrollBox"), aResult);
}
#endif

NS_IMETHODIMP_(nsrefcnt) 
nsScrollBoxFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsScrollBoxFrame::Release(void)
{
    return NS_OK;
}

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsScrollBoxFrame::SaveState(nsIPresContext* aPresContext,
                            nsIPresState** aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsCOMPtr<nsIScrollbarMediator> mediator;
  nsIFrame* first = mFrames.FirstChild();
  mediator = do_QueryInterface(first);
  if (mediator) {
    // Child manages its own scrolling. Bail.
    return NS_OK;
  }

  nsCOMPtr<nsIPresState> state;
  nsresult res = NS_OK;

  nsIView* view = GetView();
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

  PRInt32 x,y;
  nsIScrollableView* scrollingView;
  res = CallQueryInterface(view, &scrollingView);
  NS_ENSURE_SUCCESS(res, res);
  scrollingView->GetScrollPosition(x,y);

  // Don't save scroll position if we are at (0,0)
  if (x || y) {

    nsIView* child = nsnull;
    scrollingView->GetScrolledView(child);
    NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);

    nsRect childRect = child->GetBounds();

    res = NS_NewPresState(getter_AddRefs(state));
    NS_ENSURE_SUCCESS(res, res);

    nsCOMPtr<nsISupportsPRInt32> xoffset;
    nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
      nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(xoffset));
    if (xoffset) {
      res = xoffset->SetData(x);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_LITERAL_STRING("x-offset"), xoffset);
    }

    nsCOMPtr<nsISupportsPRInt32> yoffset;
    nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
      nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(yoffset));
    if (yoffset) {
      res = yoffset->SetData(y);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_LITERAL_STRING("y-offset"), yoffset);
    }

    nsCOMPtr<nsISupportsPRInt32> width;
    nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
      nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(width));
    if (width) {
      res = width->SetData(childRect.width);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_LITERAL_STRING("width"), width);
    }

    nsCOMPtr<nsISupportsPRInt32> height;
    nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
      nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(height));
    if (height) {
      res = height->SetData(childRect.height);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_LITERAL_STRING("height"), height);
    }
    *aState = state;
    NS_ADDREF(*aState);
  }
  return res;
}

//-----------------------------------------------------------
NS_IMETHODIMP
nsScrollBoxFrame::RestoreState(nsIPresContext* aPresContext,
                                 nsIPresState* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsCOMPtr<nsISupportsPRInt32> xoffset;
  nsCOMPtr<nsISupportsPRInt32> yoffset;
  nsCOMPtr<nsISupportsPRInt32> width;
  nsCOMPtr<nsISupportsPRInt32> height;
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("x-offset"), getter_AddRefs(xoffset));
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("y-offset"), getter_AddRefs(yoffset));
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("width"), getter_AddRefs(width));
  aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("height"), getter_AddRefs(height));

  nsresult res = NS_ERROR_NULL_POINTER;
  if (xoffset && yoffset) {
    PRInt32 x,y,w,h;
    res = xoffset->GetData(&x);
    if (NS_SUCCEEDED(res))
      res = yoffset->GetData(&y);
    if (NS_SUCCEEDED(res))
      res = width->GetData(&w);
    if (NS_SUCCEEDED(res))
      res = height->GetData(&h);

    mLastPos.x = -1;
    mLastPos.y = -1;
    mRestoreRect.SetRect(-1, -1, -1, -1);

    // don't do it now, store it later and do it in layout.
    if (NS_SUCCEEDED(res)) {
      mRestoreRect.SetRect(x, y, w, h);
      nsIView* view = GetView();
      if (!view)
        return NS_ERROR_FAILURE;

      nsIScrollableView* scrollingView;
      CallQueryInterface(view, &scrollingView);
      if (scrollingView)
        scrollingView->GetScrollPosition(mLastPos.x, mLastPos.y);
    }
  }

  return res;
}


NS_INTERFACE_MAP_BEGIN(nsScrollBoxFrame)
  NS_INTERFACE_MAP_ENTRY(nsIBox)
  NS_INTERFACE_MAP_ENTRY(nsIStatefulFrame)
#ifdef NS_DEBUG
  NS_INTERFACE_MAP_ENTRY(nsIFrameDebug)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIBox)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)


class nsAutoRepeatBoxFrame : public nsButtonBoxFrame, 
                             public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsAutoRepeatBoxFrame(nsIPresShell* aPresShell);
  friend nsresult NS_NewAutoRepeatBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsStyleContext*  aContext,
              nsIFrame*        aPrevInFlow);

  NS_DECL_NSITIMERCALLBACK
  nsIPresContext* mPresContext;
  
};

nsresult
NS_NewAutoRepeatBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsAutoRepeatBoxFrame* it = new (aPresShell) nsAutoRepeatBoxFrame (aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewScrollBarButtonFrame


nsAutoRepeatBoxFrame::nsAutoRepeatBoxFrame(nsIPresShell* aPresShell)
:nsButtonBoxFrame(aPresShell)
{
}

NS_IMETHODIMP
nsAutoRepeatBoxFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsStyleContext*  aContext,
              nsIFrame*        aPrevInFlow)
{
  mPresContext = aPresContext;
  return nsButtonBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
}

NS_INTERFACE_MAP_BEGIN(nsAutoRepeatBoxFrame)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_INHERITING(nsButtonBoxFrame)

NS_IMPL_ADDREF_INHERITED(nsAutoRepeatBoxFrame, nsButtonBoxFrame)
NS_IMPL_RELEASE_INHERITED(nsAutoRepeatBoxFrame, nsButtonBoxFrame)

NS_IMETHODIMP
nsAutoRepeatBoxFrame::HandleEvent(nsIPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{  
  switch(aEvent->message)
  {
    case NS_MOUSE_ENTER:
    case NS_MOUSE_ENTER_SYNTH:
       nsRepeatService::GetInstance()->Start(this);
    break;

    case NS_MOUSE_EXIT:
    case NS_MOUSE_EXIT_SYNTH:
       nsRepeatService::GetInstance()->Stop();
    break;
  }
     
  return nsButtonBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

NS_IMETHODIMP
nsAutoRepeatBoxFrame::Notify(nsITimer *timer)
{
  MouseClicked(mPresContext, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsAutoRepeatBoxFrame::Destroy(nsIPresContext* aPresContext)
{
  // Ensure our repeat service isn't going... it's possible that a scrollbar can disappear out
  // from under you while you're in the process of scrolling.
  nsRepeatService::GetInstance()->Stop();
  return nsButtonBoxFrame::Destroy(aPresContext);
}
