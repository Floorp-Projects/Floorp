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
#include "nsReflowCommand.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIContentDelegate.h"
#include "nsIWidget.h"
#include "nsHTMLIIDs.h"
#include "nsPageFrame.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"
#include "nsIViewManager.h"
#include "nsHTMLAtoms.h"

class RootFrame : public nsContainerFrame {
public:
  RootFrame(nsIContent* aContent);

  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
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

  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);

protected:
  void CreateFirstChild(nsIPresContext* aPresContext);
};

//----------------------------------------------------------------------

RootFrame::RootFrame(nsIContent* aContent)
  : nsContainerFrame(aContent, nsnull)
{
}

NS_METHOD RootFrame::Reflow(nsIPresContext*      aPresContext,
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
    NS_ASSERTION(nsnull != aReflowState.reflowCommand, "no reflow command");
    NS_ASSERTION(aReflowState.reflowCommand->GetTarget() != this,
                 "root frame is reflow command target");
  
    // Verify that the next frame in the reflow chain is our pseudo frame
    nsIFrame* next = aReflowState.reflowCommand->GetNext();
    NS_ASSERTION(next == mFirstChild, "unexpected next reflow command frame");
  
  } else {
    // Do we have any children?
    if (nsnull == mFirstChild) {
      // No. Create a pseudo frame
      NS_ASSERTION(eReflowReason_Initial == aReflowState.reason, "unexpected reflow reason");
      mFirstChild = new RootContentFrame(mContent, this);
      mChildCount = 1;
      nsIStyleContext* style = aPresContext->ResolvePseudoStyleContextFor(nsHTMLAtoms::rootContentPseudo, this);
      mFirstChild->SetStyleContext(aPresContext,style);
      NS_RELEASE(style);
    }
  }

  // Reflow our pseudo frame. It will choose whetever height its child frame
  // wants
  nsReflowMetrics  desiredSize(nsnull);

  if (nsnull != mFirstChild) {
    mFirstChild->WillReflow(*aPresContext);
    aStatus = ReflowChild(mFirstChild, aPresContext, desiredSize, aReflowState);
  
    // Place and size the child
    nsRect  rect(0, 0, desiredSize.width, desiredSize.height);
    mFirstChild->SetRect(rect);
    mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);

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

NS_METHOD RootFrame::HandleEvent(nsIPresContext& aPresContext, 
                                 nsGUIEvent* aEvent,
                                 nsEventStatus& aEventStatus)
{
  switch (aEvent->message) {
  case NS_MOUSE_MOVE:
  case NS_MOUSE_ENTER:
    {
      nsIFrame* target = this;
      PRInt32 cursor;
       
      GetCursorAt(aPresContext, aEvent->point, &target, cursor);
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

  aEventStatus = nsEventStatus_eIgnore;
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

  nsresult result = NSRepository::CreateInstance(kViewCID, 
                                                 nsnull, 
                                                 kIViewIID, 
                                                 (void **)&view);
  if (NS_OK == result) {
    nsIView*        rootView = parView;
    nsIViewManager* viewManager = rootView->GetViewManager();

    // Initialize the view
    NS_ASSERTION(nsnull != viewManager, "null view manager");

    view->Init(viewManager, mRect, rootView);
    viewManager->InsertChild(rootView, view, 0);

    NS_RELEASE(viewManager);

    // Remember our view
    SetView(view);
    NS_RELEASE(view);
  }
  NS_RELEASE(parView);
}

void RootContentFrame::CreateFirstChild(nsIPresContext* aPresContext)
{
  // Are we paginated?
  if (aPresContext->IsPaginated()) {
    // Yes. Create the first page frame
    mFirstChild = new nsPageFrame(mContent, this);
    mChildCount = 1;
    mLastContentOffset = mFirstContentOffset;

  } else {
    // Create a frame for our one and only content child
    if (mContent->ChildCount() > 0) {
      nsIContent* child = mContent->ChildAt(0);

      if (nsnull != child) {
        // Create a frame
        nsIContentDelegate* cd = child->GetDelegate(aPresContext);
        if (nsnull != cd) {
          nsIStyleContext* kidStyleContext =
            aPresContext->ResolveStyleContextFor(child, this);
          nsresult rv = cd->CreateFrame(aPresContext, child, this,
                                        kidStyleContext, mFirstChild);
          NS_RELEASE(kidStyleContext);
          if (NS_OK == rv) {
            mChildCount = 1;
            mLastContentOffset = mFirstContentOffset;
          }
          NS_RELEASE(cd);
        }
        NS_RELEASE(child);
      }
    }
  }
}

// XXX Hack
#define PAGE_SPACING 100

NS_METHOD RootContentFrame::Reflow(nsIPresContext*      aPresContext,
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
    NS_ASSERTION(aReflowState.reflowCommand->GetTarget() != this,
                 "root content frame is reflow command target");
  
    nsSize        maxSize(aReflowState.maxSize.width, NS_UNCONSTRAINEDSIZE);
    nsReflowState reflowState(aReflowState.reflowCommand, maxSize);
  
    // Verify the next frame in the reflow chain is our child frame
    nsIFrame* next = aReflowState.reflowCommand->GetNext();
    NS_ASSERTION(next == mFirstChild, "unexpected next reflow command frame");

    // Dispatch the reflow command to our child frame. Allow it to be as high
    // as it wants
    mFirstChild->WillReflow(*aPresContext);
    aStatus = ReflowChild(mFirstChild, aPresContext, aDesiredSize, reflowState);
  
    // Place and size the child. Make sure the child is at least as
    // tall as our max size (the containing window)
    if (aDesiredSize.height < aReflowState.maxSize.height) {
      aDesiredSize.height = aReflowState.maxSize.height;
    }
  
    nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
    mFirstChild->SetRect(rect);
    mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);

  } else {
    // Do we have any children?
    if (nsnull == mFirstChild) {
      // No, create the first child frame
      CreateFirstChild(aPresContext);
    }
  
    // Resize our frames
    if (nsnull != mFirstChild) {
      if (aPresContext->IsPaginated()) {
        nscoord         y = PAGE_SPACING;
        nsReflowMetrics kidSize(aDesiredSize.maxElementSize);
        nsSize          pageSize(aPresContext->GetPageWidth(),
                                 aPresContext->GetPageHeight());
        nsReflowState   reflowState(aReflowState.reason, pageSize);
  
        // Tile the pages vertically
        for (nsIFrame* kidFrame = mFirstChild; nsnull != kidFrame; ) {
          // Reflow the page
          nsReflowStatus  status;

          // Place and size the page. If the page is narrower than our max width then
          // center it horizontally
          nsIDeviceContext *dx = aPresContext->GetDeviceContext();
          PRInt32 extra = aReflowState.maxSize.width - kidSize.width -
                          NS_TO_INT_ROUND(dx->GetScrollBarWidth());
          NS_RELEASE(dx);
          nscoord         x = extra > 0 ? extra / 2 : 0;

          kidFrame->WillReflow(*aPresContext);
          kidFrame->MoveTo(x, y);
          status = ReflowChild(kidFrame, aPresContext, kidSize, reflowState);
  
          kidFrame->SetRect(nsRect(x, y, kidSize.width, kidSize.height));
          kidFrame->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
          y += kidSize.height;
  
          // Leave a slight gap between the pages
          y += PAGE_SPACING;
  
          // Is the page complete?
          nsIFrame* kidNextInFlow;
           
          kidFrame->GetNextInFlow(kidNextInFlow);
          if (NS_FRAME_IS_COMPLETE(status)) {
            NS_ASSERTION(nsnull == kidNextInFlow, "bad child flow list");
          } else if (nsnull == kidNextInFlow) {
            // The page isn't complete and it doesn't have a next-in-flow so
            // create a continuing page
            nsIStyleContext* kidSC;
            kidFrame->GetStyleContext(aPresContext, kidSC);
            nsIFrame*  continuingPage;
            nsresult rv = kidFrame->CreateContinuingFrame(aPresContext, this,
                                                          kidSC, continuingPage);
            NS_RELEASE(kidSC);
  
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
        aDesiredSize.width = aReflowState.maxSize.width;
        aDesiredSize.height = y;
  
      } else {
        // Allow the frame to be as wide as our max width, and as high
        // as it wants to be.
        nsSize        maxSize(aReflowState.maxSize.width, NS_UNCONSTRAINEDSIZE);
        nsReflowState reflowState(aReflowState.reason, maxSize);
  
        // Get the child's desired size. Our child's desired height is our
        // desired size
        mFirstChild->WillReflow(*aPresContext);
        aStatus = ReflowChild(mFirstChild, aPresContext, aDesiredSize, reflowState);
        NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
        // Place and size the child. Make sure the child is at least as
        // tall as our max size (the containing window)
        if (aDesiredSize.height < aReflowState.maxSize.height) {
          aDesiredSize.height = aReflowState.maxSize.height;
        }
  
        // Place and size the child
        nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
        mFirstChild->SetRect(rect);
        mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
      }
    }
  }

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif

  NS_FRAME_TRACE_REFLOW_OUT("RootContentFrame::Reflow", aStatus);
  return NS_OK;
}

NS_METHOD RootContentFrame::Paint(nsIPresContext&      aPresContext,
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

//----------------------------------------------------------------------

class RootPart : public nsHTMLContainer {
public:
  RootPart(nsIDocument* aDoc);

  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual nsresult CreateFrame(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*& aResult);

protected:
  virtual ~RootPart();
};

RootPart::RootPart(nsIDocument* aDoc)
{
  NS_PRECONDITION(nsnull != aDoc, "null ptr");
  mDocument = aDoc;
  mTag = NS_NewAtom("HTML");
}

RootPart::~RootPart()
{
}

nsrefcnt RootPart::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt RootPart::Release(void)
{
  if (--mRefCnt == 0) {
    delete this;
    return 0;
  }
  return mRefCnt;
}

nsresult
RootPart::CreateFrame(nsIPresContext* aPresContext,
                      nsIFrame* aParentFrame,
                      nsIStyleContext* aStyleContext,
                      nsIFrame*& aResult)
{
  RootFrame* frame = new RootFrame(this);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

nsresult
NS_NewRootPart(nsIHTMLContent** aInstancePtrResult,
               nsIDocument* aDocument)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* root = new RootPart(aDocument);
  if (nsnull == root) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aDocument->SetRootContent(root);
  return root->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
