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
#include "nsPresContext.h"
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

nsScrollBoxFrame::nsScrollBoxFrame(nsIPresShell* aShell):nsBoxFrame(aShell), mVerticalOverflow(PR_FALSE), mHorizontalOverflow(PR_FALSE)
{
}


PRBool
nsScrollBoxFrame::NeedsClipWidget()
{
  return PR_TRUE;
}

NS_IMETHODIMP
nsScrollBoxFrame::Init(nsPresContext*  aPresContext,
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
nsScrollBoxFrame::SetInitialChildList(nsPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName,
                                                           aChildList);

  SetUpScrolledFrame(aPresContext);

  return rv;
}

void
nsScrollBoxFrame::SetUpScrolledFrame(nsPresContext* aPresContext)
{
  NS_ASSERTION(mFrames.GetLength() <= 1, "ScrollBoxes can only have 1 child!");


  nsIFrame* frame = mFrames.FirstChild();

  if (!frame)
     return;

  // create a view if we don't already have one.
  nsHTMLContainerFrame::CreateViewForFrame(frame, nsnull, PR_TRUE);
}

NS_IMETHODIMP
nsScrollBoxFrame::AppendFrames(nsPresContext* aPresContext,
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
nsScrollBoxFrame::InsertFrames(nsPresContext* aPresContext,
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
nsScrollBoxFrame::RemoveFrame(nsPresContext* aPresContext,
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
nsScrollBoxFrame::GetScrollingParentView(nsPresContext* aPresContext,
                                          nsIFrame* aParent,
                                          nsIView** aParentView)
{
  *aParentView = aParent->GetView();
  NS_ASSERTION(*aParentView, "GetParentWithView failed");
  return NS_OK;
}

nsIView*
nsScrollBoxFrame::GetMouseCapturer() const
{
  nsIScrollableView* scrollingView;
#ifdef DEBUG
  nsresult result =
#endif
    CallQueryInterface(GetView(), &scrollingView);
  NS_ASSERTION(NS_SUCCEEDED(result),
               "Scrollbox does not contain a scrolling view?");
  nsIView* view;
  scrollingView->GetScrolledView(view);
  NS_ASSERTION(view, "Scrolling view does not have a scrolled view");
  return view;
}

nsresult
nsScrollBoxFrame::CreateScrollingView(nsPresContext* aPresContext)
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

  nsPresContext* presContext = aState.PresContext();

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

  childRect = kid->GetRect();

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
     nsRect r(0, 0, childRect.width, childRect.height);
     nsContainerFrame::SyncFrameViewAfterReflow(presContext, kid,
       kid->GetView(), &r, NS_FRAME_NO_MOVE_VIEW);
  }

  nsIView* view = GetView();
  nsIScrollableView* scrollingView;
  if (NS_SUCCEEDED(CallQueryInterface(view, &scrollingView))) {
    scrollingView->ComputeScrollOffsets(PR_TRUE);
  }

  childRect = kid->GetRect();

  // first see what changed
  PRBool vertChanged = PR_FALSE;
  PRBool horizChanged = PR_FALSE;

  if (mVerticalOverflow && childRect.height <= mRect.height) {
    mVerticalOverflow = PR_FALSE;
    vertChanged = PR_TRUE;
  } else if (childRect.height > mRect.height) {
    if (!mVerticalOverflow) {
       mVerticalOverflow = PR_TRUE;
    }
    vertChanged = PR_TRUE;
  }

  if (mHorizontalOverflow && childRect.width <= mRect.width) {
    mHorizontalOverflow = PR_FALSE;
    horizChanged = PR_TRUE;
  } else if (childRect.width > mRect.width) {
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
nsScrollBoxFrame::Paint(nsPresContext*      aPresContext,
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


NS_INTERFACE_MAP_BEGIN(nsScrollBoxFrame)
  NS_INTERFACE_MAP_ENTRY(nsIBox)
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

  NS_IMETHOD Destroy(nsPresContext* aPresContext);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD Init(nsPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsStyleContext*  aContext,
              nsIFrame*        aPrevInFlow);

  NS_DECL_NSITIMERCALLBACK
  nsPresContext* mPresContext;
  
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
nsAutoRepeatBoxFrame::Init(nsPresContext*  aPresContext,
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
nsAutoRepeatBoxFrame::HandleEvent(nsPresContext* aPresContext, 
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
nsAutoRepeatBoxFrame::Destroy(nsPresContext* aPresContext)
{
  // Ensure our repeat service isn't going... it's possible that a scrollbar can disappear out
  // from under you while you're in the process of scrolling.
  nsRepeatService::GetInstance()->Stop();
  return nsButtonBoxFrame::Destroy(aPresContext);
}
