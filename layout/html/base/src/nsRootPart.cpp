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

class RootFrame : public nsContainerFrame {
public:
  RootFrame(nsIContent* aContent);

  NS_IMETHOD ResizeReflow(nsIPresContext*  aPresContext,
                          nsReflowMetrics& aDesiredSize,
                          const nsSize&    aMaxSize,
                          nsSize*          aMaxElementSize,
                          ReflowStatus&    aStatus);

  NS_IMETHOD IncrementalReflow(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aDesiredSize,
                               const nsSize&    aMaxSize,
                               nsReflowCommand& aReflowCommand,
                               ReflowStatus&    aStatus);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);
};

// Pseudo frame created by the root frame
class RootContentFrame : public nsContainerFrame {
public:
  RootContentFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD ResizeReflow(nsIPresContext*  aPresContext,
                          nsReflowMetrics& aDesiredSize,
                          const nsSize&    aMaxSize,
                          nsSize*          aMaxElementSize,
                          ReflowStatus&    aStatus);

  NS_IMETHOD IncrementalReflow(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aDesiredSize,
                               const nsSize&    aMaxSize,
                               nsReflowCommand& aReflowCommand,
                               ReflowStatus&    aStatus);

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

NS_METHOD RootFrame::ResizeReflow(nsIPresContext* aPresContext,
                                  nsReflowMetrics& aDesiredSize,
                                  const nsSize& aMaxSize,
                                  nsSize* aMaxElementSize,
                                  ReflowStatus& aStatus)
{
#ifdef NS_DEBUG
  PreReflowCheck();
#endif
  aStatus = frComplete;

  // Do we have any children?
  if (nsnull == mFirstChild) {
    // No. Create a pseudo frame
    mFirstChild = new RootContentFrame(mContent, this);
    mChildCount = 1;
    nsIStyleContext* style = aPresContext->ResolveStyleContextFor(mContent, this);
    mFirstChild->SetStyleContext(aPresContext,style);
    NS_RELEASE(style);
  }

  // Resize the pseudo frame passing it our max size. It will choose whatever
  // height its child frame wants...
  if (nsnull != mFirstChild) {
    nsReflowMetrics  desiredSize;

    aStatus = ReflowChild(mFirstChild, aPresContext, desiredSize, aMaxSize,
                         aMaxElementSize);

    // Place and size the child
    nsRect  rect(0, 0, desiredSize.width, desiredSize.height);
    mFirstChild->SetRect(rect);
  }
  mLastContentOffset = ((RootContentFrame*)mFirstChild)->GetLastContentOffset();

  // Return the max size as our desired size
  aDesiredSize.width = aMaxSize.width;
  aDesiredSize.height = aMaxSize.height;
  aDesiredSize.ascent = aMaxSize.height;
  aDesiredSize.descent = 0;

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif
  return NS_OK;
}

NS_METHOD RootFrame::IncrementalReflow(nsIPresContext* aPresContext,
                                       nsReflowMetrics& aDesiredSize,
                                       const nsSize& aMaxSize,
                                       nsReflowCommand& aReflowCommand,
                                       ReflowStatus& aStatus)
{
  // We don't expect the target of the reflow command to be the root frame
  NS_ASSERTION(aReflowCommand.GetTarget() != this, "root frame is reflow command target");

  nsReflowMetrics  desiredSize;
  nsIFrame*        child;

  // Dispatch the reflow command to our pseudo frame
  aStatus = aReflowCommand.Next(desiredSize, aMaxSize, child);

  // Place and size the child
  if (nsnull != child) {
    NS_ASSERTION(child == mFirstChild, "unexpected reflow command frame");

    nsRect  rect(0, 0, desiredSize.width, desiredSize.height);
    child->SetRect(rect);
  }

  // Return the max size as our desired size
  aDesiredSize.width = aMaxSize.width;
  aDesiredSize.height = aMaxSize.height;
  aDesiredSize.ascent = aMaxSize.height;
  aDesiredSize.descent = 0;
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
    mFirstChild = new PageFrame(mContent, this);
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
          mFirstChild = cd->CreateFrame(aPresContext, child, this);
          if (nsnull != mFirstChild) {
            mChildCount = 1;
            mLastContentOffset = mFirstContentOffset;

            // Resolve style and set the style context
            nsIStyleContext* kidStyleContext =
              aPresContext->ResolveStyleContextFor(child, this);
            mFirstChild->SetStyleContext(aPresContext,kidStyleContext);
            NS_RELEASE(kidStyleContext);
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

NS_METHOD RootContentFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                         nsReflowMetrics& aDesiredSize,
                                         const nsSize&    aMaxSize,
                                         nsSize*          aMaxElementSize,
                                         ReflowStatus&    aStatus)
{
#ifdef NS_DEBUG
  PreReflowCheck();
#endif
  aStatus = frComplete;

  // Do we have any children?
  if (nsnull == mFirstChild) {
    // No, create the first child frame
    CreateFirstChild(aPresContext);
  }

  // Resize our frames
  if (nsnull != mFirstChild) {
    if (aPresContext->IsPaginated()) {
      nscoord         y = PAGE_SPACING;
      nsReflowMetrics kidSize;
      nsSize          pageSize(aPresContext->GetPageWidth(),
                               aPresContext->GetPageHeight());

      // Tile the pages vertically
      for (nsIFrame* kidFrame = mFirstChild; nsnull != kidFrame; ) {
        // Reflow the page
        ReflowStatus  status = ReflowChild(kidFrame, aPresContext, kidSize,
                                           pageSize, aMaxElementSize);

        // Place and size the page. If the page is narrower than our max width then
        // center it horizontally
        nsIDeviceContext *dx = aPresContext->GetDeviceContext();
        PRInt32 extra = aMaxSize.width - kidSize.width - NS_TO_INT_ROUND(dx->GetScrollBarWidth());
        NS_RELEASE(dx);
        kidFrame->SetRect(nsRect(extra > 0 ? extra / 2 : 0, y, kidSize.width, kidSize.height));
        y += kidSize.height;

        // Leave a slight gap between the pages
        y += PAGE_SPACING;

        // Is the page complete?
        nsIFrame* kidNextInFlow;
         
        kidFrame->GetNextInFlow(kidNextInFlow);
        if (frComplete == status) {
          NS_ASSERTION(nsnull == kidNextInFlow, "bad child flow list");
        } else if (nsnull == kidNextInFlow) {
          // The page isn't complete and it doesn't have a next-in-flow so
          // create a continuing page
          nsIFrame*  continuingPage;
           
          kidFrame->CreateContinuingFrame(aPresContext, this, continuingPage);

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
      aDesiredSize.width = aMaxSize.width;
      aDesiredSize.height = y;

    } else {
      // Allow the frame to be as wide as our max width, and as high
      // as it wants to be.
      nsSize  maxSize(aMaxSize.width, NS_UNCONSTRAINEDSIZE);

      // Get the child's desired size. Our child's desired height is our
      // desired size
      aStatus = ReflowChild(mFirstChild, aPresContext, aDesiredSize, maxSize,
                            aMaxElementSize);
      NS_ASSERTION(frComplete == aStatus, "bad status");

      // Place and size the child. Make sure the child is at least as
      // tall as our max size (the containing window)
      if (aDesiredSize.height < aMaxSize.height) {
        aDesiredSize.height = aMaxSize.height;
      }

      // Place and size the child
      nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
      mFirstChild->SetRect(rect);
    }
  }

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif
  return NS_OK;
}

// XXX Do something sensible for page mode...
NS_METHOD RootContentFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                              nsReflowMetrics& aDesiredSize,
                                              const nsSize&    aMaxSize,
                                              nsReflowCommand& aReflowCommand,
                                              ReflowStatus&    aStatus)
{
  // We don't expect the target of the reflow command to be the root
  // content frame
  NS_ASSERTION(aReflowCommand.GetTarget() != this, "root content frame is reflow command target");

  nsSize        maxSize(aMaxSize.width, NS_UNCONSTRAINEDSIZE);
  nsIFrame*     child;

  // Dispatch the reflow command to our pseudo frame. Allow it to be as high
  // as it wants
  aStatus = aReflowCommand.Next(aDesiredSize, maxSize, child);

  // Place and size the child. Make sure the child is at least as
  // tall as our max size (the containing window)
  if (nsnull != child) {
    NS_ASSERTION(child == mFirstChild, "unexpected reflow command frame");
    if (aDesiredSize.height < aMaxSize.height) {
      aDesiredSize.height = aMaxSize.height;
    }

    nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
    child->SetRect(rect);
  }

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
  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                nsIFrame* aParentFrame);

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

nsIFrame* RootPart::CreateFrame(nsIPresContext* aPresContext,
                                nsIFrame* aParentFrame)
{
  nsIFrame* rv = new RootFrame(this);
  return rv;
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
