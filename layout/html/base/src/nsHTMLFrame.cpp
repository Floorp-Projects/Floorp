/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsContainerFrame.h"
#include "nsIDocument.h"
#include "nsIReflowCommand.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsHTMLIIDs.h"
#include "nsPageFrame.h"
#include "nsIRenderingContext.h"
#include "nsGUIEvent.h"
#include "nsDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIViewManager.h"
#include "nsHTMLAtoms.h"
#include "nsIEventStateManager.h"
#include "nsIDeviceContext.h"

class RootFrame : public nsContainerFrame {
public:
  RootFrame(nsIContent* aContent);

  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);

  NS_IMETHOD Reflow(nsIPresContext&      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);
};

// Pseudo frame created by the root frame
class RootContentFrame : public nsContainerFrame {
public:
  RootContentFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Reflow(nsIPresContext&      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);
};

//----------------------------------------------------------------------

nsresult
NS_NewHTMLFrame(nsIContent* aContent, nsIFrame* aParentFrame,
                nsIFrame*& aResult)
{
  RootFrame* frame = new RootFrame(aContent);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

RootFrame::RootFrame(nsIContent* aContent)
  : nsContainerFrame(aContent, nsnull)
{
}

NS_IMETHODIMP
RootFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  // Construct the root content frame and set its style context
  mFirstChild = new RootContentFrame(mContent, this);
  mChildCount = 1;
  nsIStyleContext* pseudoStyleContext =
    aPresContext.ResolvePseudoStyleContextFor(nsHTMLAtoms::rootContentPseudo, this);
  mFirstChild->SetStyleContext(&aPresContext, pseudoStyleContext);
  NS_RELEASE(pseudoStyleContext);

  // Set the geometric and content parent for each of the child frames
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(frame)) {
    frame->SetGeometricParent(mFirstChild);
    frame->SetContentParent(mFirstChild);
  }

  // Queue up the frames for the root content frame
  return mFirstChild->Init(aPresContext, aChildList);
}

NS_IMETHODIMP
RootFrame::Reflow(nsIPresContext&      aPresContext,
                  nsReflowMetrics&     aDesiredSize,
                  const nsReflowState& aReflowState,
                  nsReflowStatus&      aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("RootFrame::Reflow");

#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  aStatus = NS_FRAME_COMPLETE;

  if (eReflowReason_Incremental == aReflowState.reason) {
    // We don't expect the target of the reflow command to be the root frame
#ifdef NS_DEBUG
    NS_ASSERTION(nsnull != aReflowState.reflowCommand, "no reflow command");

    nsIFrame* target;
    aReflowState.reflowCommand->GetTarget(target);
    NS_ASSERTION(target != this, "root frame is reflow command target");
#endif
  
    // Verify that the next frame in the reflow chain is our pseudo frame
    nsIFrame* next;
    aReflowState.reflowCommand->GetNext(next);
    NS_ASSERTION(next == mFirstChild, "unexpected next reflow command frame");
  }

  // Reflow our pseudo frame. It will choose whetever height its child frame
  // wants
  if (nsnull != mFirstChild) {
    nsReflowMetrics  desiredSize(nsnull);
    nsReflowState    kidReflowState(mFirstChild, aReflowState, aReflowState.maxSize);

    mFirstChild->WillReflow(aPresContext);
    aStatus = ReflowChild(mFirstChild, &aPresContext, desiredSize, kidReflowState);
  
    // Place and size the child
    nsRect  rect(0, 0, desiredSize.width, desiredSize.height);
    mFirstChild->SetRect(rect);
    mFirstChild->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);

    mLastContentOffset = ((RootContentFrame*)mFirstChild)->GetLastContentOffset();
  }

  // Return the max size as our desired size
  aDesiredSize.width = aReflowState.maxSize.width;
  aDesiredSize.height = aReflowState.maxSize.height;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif
  NS_FRAME_TRACE_REFLOW_OUT("RootFrame::Reflow", aStatus);
  return NS_OK;
}

NS_IMETHODIMP
RootFrame::HandleEvent(nsIPresContext& aPresContext, 
                       nsGUIEvent* aEvent,
                       nsEventStatus& aEventStatus)
{
  mContent->HandleDOMEvent(aPresContext, (nsEvent*)aEvent, nsnull, DOM_EVENT_INIT, aEventStatus);

  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP ||
      aEvent->message == NS_MOUSE_MIDDLE_BUTTON_UP ||
      aEvent->message == NS_MOUSE_RIGHT_BUTTON_UP) {
    nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

#if 0
  if (aEventStatus != nsEventStatus_eConsumeNoDefault) {
    switch (aEvent->message) {
    case NS_MOUSE_MOVE:
    case NS_MOUSE_ENTER:
      {
        nsIFrame* target = this;
        PRInt32 cursor;
       
        GetCursorAndContentAt(aPresContext, aEvent->point, &target, &mContent, cursor);
        if (cursor == NS_STYLE_CURSOR_INHERIT) {
          cursor = NS_STYLE_CURSOR_DEFAULT;
        }
        nsCursor c;
        switch (cursor) {
        default:
        case NS_STYLE_CURSOR_DEFAULT:
          c = eCursor_standard;
          break;
        case NS_STYLE_CURSOR_HAND:
          c = eCursor_hyperlink;
          break;
        case NS_STYLE_CURSOR_IBEAM:
          c = eCursor_select;
          break;
        }
        nsIWidget* window;
        target->GetWindow(window);
        window->SetCursor(c);
        NS_RELEASE(window);
      }
      break;
    }
  }
#endif
  return NS_OK;
}

//----------------------------------------------------------------------

RootContentFrame::RootContentFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsContainerFrame(aContent, aParent)
{
  // Create a view
  nsIFrame* parent;
  nsIView*  view;

  GetParentWithView(parent);
  NS_ASSERTION(parent, "GetParentWithView failed");
  nsIView* parView;
   
  parent->GetView(parView);
  NS_ASSERTION(parView, "no parent with view");

  // Create a view
  static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
  static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

  nsresult result = nsRepository::CreateInstance(kViewCID, 
                                                 nsnull, 
                                                 kIViewIID, 
                                                 (void **)&view);
  if (NS_OK == result) {
    nsIView*        rootView = parView;
    nsIViewManager* viewManager;
    rootView->GetViewManager(viewManager);

    // Initialize the view
    NS_ASSERTION(nsnull != viewManager, "null view manager");

    view->Init(viewManager, mRect, rootView);
    viewManager->InsertChild(rootView, view, 0);

    NS_RELEASE(viewManager);

    // Remember our view
    SetView(view);
  }
}

// XXX Hack
#define PAGE_SPACING_TWIPS 100

NS_IMETHODIMP
RootContentFrame::Reflow(nsIPresContext&      aPresContext,
                         nsReflowMetrics&     aDesiredSize,
                         const nsReflowState& aReflowState,
                         nsReflowStatus&      aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("RootContentFrame::Reflow");
#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  aStatus = NS_FRAME_COMPLETE;

  // XXX Incremental reflow code doesn't handle page mode at all...
  if (eReflowReason_Incremental == aReflowState.reason) {
    // We don't expect the target of the reflow command to be the root
    // content frame
#ifdef NS_DEBUG
    nsIFrame* target;
    aReflowState.reflowCommand->GetTarget(target);
    NS_ASSERTION(target != this, "root content frame is reflow command target");
#endif
  
    // Verify the next frame in the reflow chain is our child frame
    nsIFrame* next;
    aReflowState.reflowCommand->GetNext(next);
    NS_ASSERTION(next == mFirstChild, "unexpected next reflow command frame");

    nsSize        maxSize(aReflowState.maxSize.width, NS_UNCONSTRAINEDSIZE);
    nsReflowState kidReflowState(next, aReflowState, maxSize);
  
    // Dispatch the reflow command to our child frame. Allow it to be as high
    // as it wants
    mFirstChild->WillReflow(aPresContext);
    aStatus = ReflowChild(mFirstChild, &aPresContext, aDesiredSize, kidReflowState);
  
    // Place and size the child. Make sure the child is at least as
    // tall as our max size (the containing window)
    if (aDesiredSize.height < aReflowState.maxSize.height) {
      aDesiredSize.height = aReflowState.maxSize.height;
    }
  
    nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
    mFirstChild->SetRect(rect);

  } else {
    nsReflowReason  reflowReason = aReflowState.reason;

    // Resize our frames
    if (nsnull != mFirstChild) {
      if (aPresContext.IsPaginated()) {
        nscoord         y = PAGE_SPACING_TWIPS;
        nsReflowMetrics kidSize(aDesiredSize.maxElementSize);

        // Compute the size of each page and the x coordinate within
        // ourselves that the pages will be placed at.
        nsSize          pageSize(aPresContext.GetPageWidth(),
                                 aPresContext.GetPageHeight());
        nsIDeviceContext *dx = aPresContext.GetDeviceContext();
        float             sbWidth, sbHeight;
        dx->GetScrollBarDimensions(sbWidth, sbHeight);
        PRInt32 extra = aReflowState.maxSize.width - PAGE_SPACING_TWIPS*2 -
          pageSize.width - NSToCoordRound(sbWidth);
        NS_RELEASE(dx);

        // Note: nscoord is an unsigned type so don't combine these
        // two statements or the extra will be promoted to unsigned
        // and the >0 won't work!
        nscoord x = PAGE_SPACING_TWIPS;
        if (extra > 0) {
          x += extra / 2;
        }
  
        // Tile the pages vertically
        for (nsIFrame* kidFrame = mFirstChild; nsnull != kidFrame; ) {
          // Reflow the page
          nsReflowState   kidReflowState(kidFrame, aReflowState, pageSize,
                                         reflowReason);
          nsReflowStatus  status;

          // Place and size the page. If the page is narrower than our
          // max width then center it horizontally
          kidFrame->WillReflow(aPresContext);
          kidFrame->MoveTo(x, y);
          status = ReflowChild(kidFrame, &aPresContext, kidSize,
                               kidReflowState);
          kidFrame->SetRect(nsRect(x, y, kidSize.width, kidSize.height));
          y += kidSize.height;
  
          // Leave a slight gap between the pages
          y += PAGE_SPACING_TWIPS;
  
          // Is the page complete?
          nsIFrame* kidNextInFlow;
           
          kidFrame->GetNextInFlow(kidNextInFlow);
          if (NS_FRAME_IS_COMPLETE(status)) {
            NS_ASSERTION(nsnull == kidNextInFlow, "bad child flow list");
          } else if (nsnull == kidNextInFlow) {
            // The page isn't complete and it doesn't have a next-in-flow so
            // create a continuing page
            nsIStyleContext* kidSC;
            kidFrame->GetStyleContext(&aPresContext, kidSC);
            nsIFrame*  continuingPage;
            nsresult rv = kidFrame->CreateContinuingFrame(aPresContext, this,
                                                          kidSC, continuingPage);
            NS_RELEASE(kidSC);
            reflowReason = eReflowReason_Initial;
  
            // Add it to our child list
#ifdef NS_DEBUG
            nsIFrame* kidNextSibling;
            kidFrame->GetNextSibling(kidNextSibling);
            NS_ASSERTION(nsnull == kidNextSibling, "unexpected sibling");
#endif
            kidFrame->SetNextSibling(continuingPage);
            mChildCount++;
          }
  
          // Get the next page
          kidFrame->GetNextSibling(kidFrame);
        }
  
        // Return our desired size
        aDesiredSize.height = y;
        if (aDesiredSize.height < aReflowState.maxSize.height) {
          aDesiredSize.height = aReflowState.maxSize.height;
        }
        aDesiredSize.width = PAGE_SPACING_TWIPS*2 + pageSize.width;
        if (aDesiredSize.width < aReflowState.maxSize.width) {
          aDesiredSize.width = aReflowState.maxSize.width;
        }
  
      } else {
        // Allow the frame to be as wide as our max width, and as high
        // as it wants to be.
        nsSize        maxSize(aReflowState.maxSize.width, NS_UNCONSTRAINEDSIZE);
        nsReflowState kidReflowState(mFirstChild, aReflowState, maxSize, reflowReason);
  
        // Get the child's desired size. Our child's desired height is our
        // desired size
        mFirstChild->WillReflow(aPresContext);
        aStatus = ReflowChild(mFirstChild, &aPresContext, aDesiredSize, kidReflowState);
        NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
        // Place and size the child. Make sure the child is at least as
        // tall as our max size (the containing window)
        if (aDesiredSize.height < aReflowState.maxSize.height) {
          aDesiredSize.height = aReflowState.maxSize.height;
        }
  
        // Place and size the child
        nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
        mFirstChild->SetRect(rect);

        // Do the necessary repainting
        if (eReflowReason_Initial == reflowReason) {
          // Repaint the visible area
          Invalidate(nsRect(0, 0, aReflowState.maxSize.width, aReflowState.maxSize.height));
        } else if (eReflowReason_Resize == aReflowState.reason) {
          // Repaint the entire frame
          Invalidate(nsRect(0, 0, aReflowState.maxSize.width, aDesiredSize.height));
        }
      }
    }
    else {
      aDesiredSize.width = aReflowState.maxSize.width;
      aDesiredSize.height = aReflowState.maxSize.height;
      aDesiredSize.ascent = aDesiredSize.height;
      aDesiredSize.descent = 0;
      if (nsnull != aDesiredSize.maxElementSize) {
        aDesiredSize.maxElementSize->width = 0;
        aDesiredSize.maxElementSize->height = 0;
      }
    }
  }

  // We are always a pseudo-frame; make sure our content offset is
  // properly pushed upwards
  nsContainerFrame* parent = (nsContainerFrame*) mGeometricParent;
  parent->PropagateContentOffsets(this, mFirstContentOffset,
                                  mLastContentOffset, mLastContentIsComplete);

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif
  NS_FRAME_TRACE_REFLOW_OUT("RootContentFrame::Reflow", aStatus);
  return NS_OK;
}

NS_IMETHODIMP
RootContentFrame::Paint(nsIPresContext&      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect)
{
  // If we're paginated then fill the dirty rect with white
  if (aPresContext.IsPaginated()) {
    // Cross hatching would be nicer...
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.FillRect(aDirtyRect);
  }

  nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

NS_IMETHODIMP
RootContentFrame::HandleEvent(nsIPresContext& aPresContext, 
                              nsGUIEvent* aEvent,
                              nsEventStatus& aEventStatus)
{
#if 0
  mContent->HandleDOMEvent(aPresContext, (nsEvent*)aEvent, nsnull, DOM_EVENT_INIT, aEventStatus);
#else
  nsContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
#endif
  
  if (aEventStatus != nsEventStatus_eConsumeNoDefault) {
    switch (aEvent->message) {
    case NS_MOUSE_MOVE:
    case NS_MOUSE_ENTER:
      {
        nsIFrame* target = this;
        nsIContent* mTargetContent = mContent;
        PRInt32 cursor;
       
        GetCursorAndContentAt(aPresContext, aEvent->point, &target, &mTargetContent, cursor);
        if (cursor == NS_STYLE_CURSOR_INHERIT) {
          cursor = NS_STYLE_CURSOR_DEFAULT;
        }
        nsCursor c;
        switch (cursor) {
        default:
        case NS_STYLE_CURSOR_DEFAULT:
          c = eCursor_standard;
          break;
        case NS_STYLE_CURSOR_HAND:
          c = eCursor_hyperlink;
          break;
        case NS_STYLE_CURSOR_IBEAM:
          c = eCursor_select;
          break;
        }
        nsIWidget* window;
        target->GetWindow(window);
        window->SetCursor(c);
        NS_RELEASE(window);

        //If the content object under the cursor has changed, fire a mouseover/out
        nsIEventStateManager *mStateManager;
        nsIContent *mLastContent;
        if (NS_OK == aPresContext.GetEventStateManager(&mStateManager)) {
          mStateManager->GetLastMouseOverContent(&mLastContent);
          if (mLastContent != mTargetContent) {
            if (nsnull != mLastContent) {
              //fire mouseout
              nsEventStatus mStatus = nsEventStatus_eIgnore;
              nsMouseEvent mEvent;
              mEvent.eventStructType = NS_MOUSE_EVENT;
              mEvent.message = NS_MOUSE_EXIT;
              mLastContent->HandleDOMEvent(aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus); 
            }
            //fire mouseover
            nsEventStatus mStatus = nsEventStatus_eIgnore;
            nsMouseEvent mEvent;
            mEvent.eventStructType = NS_MOUSE_EVENT;
            mEvent.message = NS_MOUSE_ENTER;
            mTargetContent->HandleDOMEvent(aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus);
            mStateManager->SetLastMouseOverContent(mTargetContent);
          }
          NS_RELEASE(mStateManager);
          NS_IF_RELEASE(mLastContent);
        }
      }
      break;
    case NS_MOUSE_EXIT:
      //Don't know if this is actually hooked up.
      {
        //Fire of mouseout to the last content object.
        nsIEventStateManager *mStateManager;
        nsIContent *mLastContent;
        if (NS_OK == aPresContext.GetEventStateManager(&mStateManager)) {
          mStateManager->GetLastMouseOverContent(&mLastContent);
          if (nsnull != mLastContent) {
            //fire mouseout
            nsEventStatus mStatus = nsEventStatus_eIgnore;
            nsMouseEvent mEvent;
            mEvent.eventStructType = NS_MOUSE_EVENT;
            mEvent.message = NS_MOUSE_EXIT;
            mLastContent->HandleDOMEvent(aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus);
            mStateManager->SetLastMouseOverContent(nsnull);

            NS_RELEASE(mLastContent);
          }
          NS_RELEASE(mStateManager);
        }
      }
      break;
    case NS_MOUSE_LEFT_BUTTON_UP:
      {
        nsIEventStateManager *mStateManager;
        if (NS_OK == aPresContext.GetEventStateManager(&mStateManager)) {
          mStateManager->SetActiveLink(nsnull);
          NS_RELEASE(mStateManager);
        }
      }
      break;
    }
  }

  return NS_OK;
}
