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
#include "nsIEventStateManager.h"

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
  if (nsnull != mFirstChild) {
    nsReflowMetrics  desiredSize(nsnull);
    nsReflowState    kidReflowState(mFirstChild, aReflowState, aReflowState.maxSize);

    mFirstChild->WillReflow(*aPresContext);
    aStatus = ReflowChild(mFirstChild, aPresContext, desiredSize, kidReflowState);
  
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
  nsIEventStateManager *mManager;

  if (NS_OK == aPresContext.GetEventStateManager(&mManager)) {
    mManager->SetEventTarget((nsISupports*)mContent);
    NS_RELEASE(mManager);
  }
  mContent->HandleDOMEvent(aPresContext, aEvent, nsnull, aEventStatus);
  
  if (aEventStatus != nsEventStatus_eConsumeNoDefault) {
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
  }

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
    // Create a frame for the body child
    PRInt32 i, n;
    n = mContent->ChildCount();
    for (i = 0; i < n; i++) {
      nsIContent* child = mContent->ChildAt(i);
      if (nsnull != child) {
        nsIAtom* tag;
        tag = child->GetTag();
        if (nsHTMLAtoms::body == tag) {
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
              mFirstContentOffset = i;
              mLastContentOffset = i;
            }
            NS_RELEASE(cd);
          }
        }
        NS_IF_RELEASE(tag);
        NS_RELEASE(child);
      }
    }
  }
}

// XXX Hack
#define PAGE_SPACING_TWIPS 100

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
    mFirstChild->WillReflow(*aPresContext);
    aStatus = ReflowChild(mFirstChild, aPresContext, aDesiredSize, kidReflowState);
  
    // Place and size the child. Make sure the child is at least as
    // tall as our max size (the containing window)
    if (aDesiredSize.height < aReflowState.maxSize.height) {
      aDesiredSize.height = aReflowState.maxSize.height;
    }
  
    nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
    mFirstChild->SetRect(rect);
    mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);

  } else {
    nsReflowReason  reflowReason = aReflowState.reason;

    // Do we have any children?
    if (nsnull == mFirstChild) {
      // No, create the first child frame
      reflowReason = eReflowReason_Initial;
      CreateFirstChild(aPresContext);
    }
  
    // Resize our frames
    if (nsnull != mFirstChild) {
      if (aPresContext->IsPaginated()) {
        nscoord         y = PAGE_SPACING_TWIPS;
        nsReflowMetrics kidSize(aDesiredSize.maxElementSize);

        // Compute the size of each page and the x coordinate within
        // ourselves that the pages will be placed at.
        nsSize          pageSize(aPresContext->GetPageWidth(),
                                 aPresContext->GetPageHeight());
        nsIDeviceContext *dx = aPresContext->GetDeviceContext();
        PRInt32 extra = aReflowState.maxSize.width - PAGE_SPACING_TWIPS*2 -
          pageSize.width - NS_TO_INT_ROUND(dx->GetScrollBarWidth());
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
          kidFrame->WillReflow(*aPresContext);
          kidFrame->MoveTo(x, y);
          status = ReflowChild(kidFrame, aPresContext, kidSize,
                               kidReflowState);
          kidFrame->SetRect(nsRect(x, y, kidSize.width, kidSize.height));
          kidFrame->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
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
            kidFrame->GetStyleContext(aPresContext, kidSC);
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
        mFirstChild->WillReflow(*aPresContext);
        aStatus = ReflowChild(mFirstChild, aPresContext, aDesiredSize, kidReflowState);
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
  PropagateContentOffsets();
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

  virtual nsresult CreateFrame(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*& aResult);

  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext, 
                               nsGUIEvent* aEvent, 
                               nsIDOMEvent* aDOMEvent,
                               nsEventStatus& aEventStatus);

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);

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
RootPart::HandleDOMEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent, 
                         nsIDOMEvent* aDOMEvent,
                         nsEventStatus& aEventStatus)
{
  return mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aEventStatus);
}

nsresult 
RootPart::GetScriptObject(nsIScriptContext *aContext, 
                                  void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptElement(aContext, this, mDocument, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
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
