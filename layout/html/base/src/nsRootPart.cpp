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

  virtual ReflowStatus ResizeReflow(nsIPresContext*  aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize&    aMaxSize,
                                    nsSize*          aMaxElementSize);

  virtual ReflowStatus IncrementalReflow(nsIPresContext*  aPresContext,
                                         nsReflowMetrics& aDesiredSize,
                                         const nsSize&    aMaxSize,
                                         nsReflowCommand& aReflowCommand);

  virtual nsEventStatus HandleEvent(nsIPresContext& aPresContext, 
                             nsGUIEvent*     aEvent);
};

// Pseudo frame created by the root frame
class RootContentFrame : public nsContainerFrame {
public:
  RootContentFrame(nsIContent* aContent, nsIFrame* aParent);

  virtual ReflowStatus ResizeReflow(nsIPresContext*  aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize&    aMaxSize,
                                    nsSize*          aMaxElementSize);

  virtual ReflowStatus IncrementalReflow(nsIPresContext*  aPresContext,
                                         nsReflowMetrics& aDesiredSize,
                                         const nsSize&    aMaxSize,
                                         nsReflowCommand& aReflowCommand);

  virtual void Paint(nsIPresContext&      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect);

protected:
  void CreateFirstChild(nsIPresContext* aPresContext);
};

//----------------------------------------------------------------------

RootFrame::RootFrame(nsIContent* aContent)
  : nsContainerFrame(aContent, -1, nsnull)
{
}

nsIFrame::ReflowStatus RootFrame::ResizeReflow(nsIPresContext* aPresContext,
                                               nsReflowMetrics& aDesiredSize,
                                               const nsSize& aMaxSize,
                                               nsSize* aMaxElementSize)
{
#ifdef NS_DEBUG
  PreReflowCheck();
#endif
  nsIFrame::ReflowStatus  result = frComplete;

  // Do we have any children?
  if (nsnull == mFirstChild) {
    // No. Create a pseudo frame
    mFirstChild = new RootContentFrame(mContent, this);
    mChildCount = 1;
    nsIStyleContext* style = aPresContext->ResolveStyleContextFor(mContent, this);
    mFirstChild->SetStyleContext(style);
    NS_RELEASE(style);
  }

  // Resize the pseudo frame passing it our max size. It will choose whatever
  // height its child frame wants...
  if (nsnull != mFirstChild) {
    nsReflowMetrics  desiredSize;
    result = ReflowChild(mFirstChild, aPresContext, desiredSize, aMaxSize,
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
  PostReflowCheck(result);
#endif
  return result;
}

nsIFrame::ReflowStatus
RootFrame::IncrementalReflow(nsIPresContext* aPresContext,
                             nsReflowMetrics& aDesiredSize,
                             const nsSize& aMaxSize,
                             nsReflowCommand& aReflowCommand)
{
  // We don't expect the target of the reflow command to be the root frame
  NS_ASSERTION(aReflowCommand.GetTarget() != this, "root frame is reflow command target");

  nsReflowMetrics  desiredSize;
  nsIFrame*        child;
  ReflowStatus     status;

  // Dispatch the reflow command to our pseudo frame
  status = aReflowCommand.Next(desiredSize, aMaxSize, child);

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
  return status;
}

nsEventStatus RootFrame::HandleEvent(nsIPresContext& aPresContext, 
                              nsGUIEvent*     aEvent)
{
  switch (aEvent->message) {
  case NS_MOUSE_MOVE:
  case NS_MOUSE_ENTER:
    {
      nsIFrame* target = this;
      PRInt32 cursor = GetCursorAt(aPresContext, aEvent->point, &target);
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
      nsIWidget* window = target->GetWindow();
      window->SetCursor(c);
    }
    break;
  }

  return nsEventStatus_eIgnore;
}

//----------------------------------------------------------------------

RootContentFrame::RootContentFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsContainerFrame(aContent, aParent->GetIndexInParent(), aParent)
{
  // Create a view
  nsIFrame* parent = GetParentWithView();
  nsIView*  view;

  NS_ASSERTION(parent, "GetParentWithView failed");
  nsIView* parView = parent->GetView(); 
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

    view->Init(viewManager, GetRect(), rootView);
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
          mFirstChild = cd->CreateFrame(aPresContext, child, 0, this);
          if (nsnull != mFirstChild) {
            mChildCount = 1;
            mLastContentOffset = mFirstContentOffset;

            // Resolve style and set the style context
            nsIStyleContext* kidStyleContext =
              aPresContext->ResolveStyleContextFor(child, this);
            mFirstChild->SetStyleContext(kidStyleContext);
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

nsIFrame::ReflowStatus
RootContentFrame::ResizeReflow(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aDesiredSize,
                               const nsSize&    aMaxSize,
                               nsSize*          aMaxElementSize)
{
#ifdef NS_DEBUG
  PreReflowCheck();
#endif
  nsIFrame::ReflowStatus  result = frComplete;

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
        nsIFrame* nextInFlow = kidFrame->GetNextInFlow();

        if (frComplete == status) {
          NS_ASSERTION(nsnull == kidFrame->GetNextInFlow(), "bad child flow list");
        } else if (nsnull == nextInFlow) {
          // The page isn't complete and it doesn't have a next-in-flow so
          // create a continuing page
          nsIFrame*  continuingPage = kidFrame->CreateContinuingFrame(aPresContext, this);

          // Add it to our child list
          NS_ASSERTION(nsnull == kidFrame->GetNextSibling(), "unexpected sibling");
          kidFrame->SetNextSibling(continuingPage);
          mChildCount++;
        }

        // Get the next page
        kidFrame = kidFrame->GetNextSibling();
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
      result = ReflowChild(mFirstChild, aPresContext, aDesiredSize, maxSize,
                           aMaxElementSize);
      NS_ASSERTION(frComplete == result, "bad status");

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
  PostReflowCheck(result);
#endif
  return result;
}

// XXX Do something sensible for page mode...
nsIFrame::ReflowStatus
RootContentFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize&    aMaxSize,
                                    nsReflowCommand& aReflowCommand)
{
  // We don't expect the target of the reflow command to be the root
  // content frame
  NS_ASSERTION(aReflowCommand.GetTarget() != this, "root content frame is reflow command target");

  nsSize        maxSize(aMaxSize.width, NS_UNCONSTRAINEDSIZE);
  ReflowStatus  status;
  nsIFrame*     child;

  // Dispatch the reflow command to our pseudo frame. Allow it to be as high
  // as it wants
  status = aReflowCommand.Next(aDesiredSize, maxSize, child);

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

  return status;
}

void RootContentFrame::Paint(nsIPresContext&      aPresContext,
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
}

//----------------------------------------------------------------------

class RootPart : public nsHTMLContainer {
public:
  RootPart(nsIDocument* aDoc);

  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();
  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
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
                                PRInt32 aIndexInParent,
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
