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
#include "nsScrollFrame.h"
#include "nsIPresContext.h"
#include "nsIContentDelegate.h"
#include "nsIStyleContext.h"
#include "nsIReflowCommand.h"
#include "nsIDeviceContext.h"
#include "nsPageFrame.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsBodyFrame.h"
#include "nsHTMLBase.h"
#include "nsCSSLayout.h"

#include "nsBodyFrame.h"

class nsScrollBodyFrame : public nsContainerFrame {
public:
  nsScrollBodyFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);

  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  void CreateFirstChild(nsIPresContext* aPresContext);
};

nsScrollBodyFrame::nsScrollBodyFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsContainerFrame(aContent, aParent)
{
  nsHTMLBase::CreateViewForFrame(nsnull, this, nsnull, PR_TRUE);
}

void
nsScrollBodyFrame::CreateFirstChild(nsIPresContext* aPresContext)
{
  // Are we paginated?
  if (aPresContext->IsPaginated()) {
    // Yes. Create the first page frame
    mFirstChild = new nsPageFrame(mContent, this);/* XXX mContent won't work here */
    mChildCount = 1;
    mLastContentOffset = mFirstContentOffset;
  } else {
    if (nsnull != mContent) {
#if 0
      // Create a frame for the child we are wrapping up
      nsIContentDelegate* cd = mContent->GetDelegate(aPresContext);
      if (nsnull != cd) {
        nsIStyleContext* kidStyleContext =
          aPresContext->ResolveStyleContextFor(mContent, this);
        nsresult rv = cd->CreateFrame(aPresContext, mContent, this,
                                      kidStyleContext, mFirstChild);
        NS_RELEASE(kidStyleContext);
        if (NS_OK == rv) {
          mChildCount = 1;
          mLastContentOffset = mFirstContentOffset;
        }
        NS_RELEASE(cd);
      }
#else
      nsBodyFrame::NewFrame(&mFirstChild, mContent, this);
      mChildCount = 1;
      mLastContentOffset = mFirstContentOffset;
#endif
    }
  }
}

// XXX Hack
#define PAGE_SPACING 100

NS_IMETHODIMP
nsScrollBodyFrame::Reflow(nsIPresContext*      aPresContext,
                          nsReflowMetrics&     aDesiredSize,
                          const nsReflowState& aReflowState,
                          nsReflowStatus&      aStatus)
{
  NS_FRAME_TRACE_MSG(("enter nsScrollBodyFrame::Reflow: maxSize=%d,%d",
                      aReflowState.maxSize.width,
                      aReflowState.maxSize.height));

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
  
        // Tile the pages vertically
        for (nsIFrame* kidFrame = mFirstChild; nsnull != kidFrame; ) {
          // Reflow the page
          nsReflowState   kidReflowState(kidFrame, aReflowState, pageSize);
          nsReflowStatus  status;

          // Place and size the page. If the page is narrower than our
          // max width then center it horizontally
          nsIDeviceContext *dx = aPresContext->GetDeviceContext();
          PRInt32 extra = aReflowState.maxSize.width - kidSize.width -
                          NS_TO_INT_ROUND(dx->GetScrollBarWidth());
          NS_RELEASE(dx);
          nscoord         x = extra > 0 ? extra / 2 : 0;

          kidFrame->WillReflow(*aPresContext);
          kidFrame->MoveTo(x, y);
          status = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState);
  
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
        nsReflowState kidReflowState(mFirstChild, aReflowState, maxSize);
  
        // Get the child's desired size. Our child's desired height is our
        // desired size
        mFirstChild->WillReflow(*aPresContext);
        aStatus = ReflowChild(mFirstChild, aPresContext, aDesiredSize, kidReflowState);
        NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
#if 0
        // Place and size the child. Make sure the child is at least as
        // tall as our max size (the containing window)
        if (aDesiredSize.height < aReflowState.maxSize.height) {
          aDesiredSize.height = aReflowState.maxSize.height;
        }
#endif
  
        // Place and size the child
        nsRect  rect(0, 0, aDesiredSize.width, aDesiredSize.height);
        mFirstChild->SetRect(rect);
        mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
      }
    }
    else {
      aDesiredSize.width = aReflowState.maxSize.width;
      aDesiredSize.height = 1;
      aDesiredSize.ascent = aDesiredSize.height;
      aDesiredSize.descent = 0;
      if (nsnull != aDesiredSize.maxElementSize) {
        aDesiredSize.maxElementSize->width = 0;
        aDesiredSize.maxElementSize->height = 0;
      }
    }
  }

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif

  NS_FRAME_TRACE_MSG(
    ("exit nsScrollBodyFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}

NS_IMETHODIMP
nsScrollBodyFrame::Paint(nsIPresContext&      aPresContext,
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
nsScrollBodyFrame::ListTag(FILE* out) const
{
  fputs("*scrollbodyframe<", out);
  nsIAtom* atom = mContent->GetTag();
  if (nsnull != atom) {
    nsAutoString tmp;
    atom->ToString(tmp);
    fputs(tmp, out);
  }
  PRInt32 contentIndex;
  GetContentIndex(contentIndex);
  fprintf(out, ">(%d)@%p", contentIndex, this);
  return NS_OK;
}

//----------------------------------------------------------------------

class nsScrollInnerFrame : public nsContainerFrame {
public:
  nsScrollInnerFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
};

nsScrollInnerFrame::nsScrollInnerFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsContainerFrame(aContent, aParent)
{
}

NS_IMETHODIMP
nsScrollInnerFrame::Reflow(nsIPresContext*      aPresContext,
                           nsReflowMetrics&     aDesiredSize,
                           const nsReflowState& aReflowState,
                           nsReflowStatus&      aStatus)
{
  NS_FRAME_TRACE_MSG(("enter nsScrollInnerFrame::Reflow: maxSize=%d,%d",
                      aReflowState.maxSize.width,
                      aReflowState.maxSize.height));

  nsIView* view;
  GetView(view);
  if (nsnull == view) {
    static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
    static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

    // Get parent view
    nsIFrame* parent;
    GetParentWithView(parent);
    NS_ASSERTION(parent, "GetParentWithView failed");
    nsIView* parentView;
    parent->GetView(parentView);
    NS_ASSERTION(parentView, "GetParentWithView failed");

    nsIViewManager* viewManager = parentView->GetViewManager();

    nsresult rv = NSRepository::CreateInstance(kScrollingViewCID, 
                                               nsnull, 
                                               kIViewIID, 
                                               (void **)&view);
    if ((NS_OK != rv) || (NS_OK != view->Init(viewManager, 
                                              mRect,
                                              parentView))) {
      NS_RELEASE(parentView);
      NS_RELEASE(viewManager);
      return rv;
    }

    // Insert new view as a child of the parent view
    viewManager->InsertChild(parentView, view, 0);
    NS_RELEASE(viewManager);

    NS_RELEASE(parentView);
    SetView(view);
  }
  if (nsnull == view) {
    return NS_OK;
  }
  NS_RELEASE(view);

  if (nsnull == mFirstChild) {
    mFirstChild = new nsScrollBodyFrame(mContent, this);
    mChildCount = 1;
  }

  // Allow the child frame to be as wide as our max width (minus a
  // scroll bar width), and as high as it wants to be.
  nsSize        maxSize;
  nsIDeviceContext* dc = aPresContext->GetDeviceContext();
  maxSize.width = aReflowState.maxSize.width -
    NS_TO_INT_ROUND(dc->GetScrollBarWidth());
  NS_RELEASE(dc);
  maxSize.height = NS_UNCONSTRAINEDSIZE;
  
  // Reflow the child
  nsReflowMetrics kidMetrics(aDesiredSize.maxElementSize);
  nsReflowState kidReflowState(mFirstChild, aReflowState, maxSize);
  mFirstChild->WillReflow(*aPresContext);
  aStatus = ReflowChild(mFirstChild, aPresContext, kidMetrics,
                        kidReflowState);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
  // Place and size the child
  nsRect  rect(0, 0, kidMetrics.width, kidMetrics.height);
  mFirstChild->SetRect(rect);
  mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);

  // Determine our size. Our width is our maxWidth and our height is
  // either our child's height or our maxHeight if our maxHeight is
  // constrained.
  aDesiredSize.width = aReflowState.maxSize.width;
  if (NS_UNCONSTRAINEDSIZE == aReflowState.maxSize.height) {
    aDesiredSize.height  = kidMetrics.height;
  }
  else {
    aDesiredSize.height  = aReflowState.maxSize.height;
  }

  NS_FRAME_TRACE_MSG(
    ("exit nsScrollInnerFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}

NS_IMETHODIMP
nsScrollInnerFrame::ListTag(FILE* out) const
{
  fputs("*scrollinnerframe<", out);
  nsIAtom* atom = mContent->GetTag();
  if (nsnull != atom) {
    nsAutoString tmp;
    atom->ToString(tmp);
    fputs(tmp, out);
  }
  PRInt32 contentIndex;
  GetContentIndex(contentIndex);
  fprintf(out, ">(%d)@%p", contentIndex, this);
  return NS_OK;
}

//----------------------------------------------------------------------

class nsScrollOuterFrame : public nsHTMLContainerFrame {
public:
  nsScrollOuterFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  virtual PRIntn GetSkipSides() const;
};

nsScrollOuterFrame::nsScrollOuterFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

//XXX incremental reflow pass through
NS_IMETHODIMP
nsScrollOuterFrame::Reflow(nsIPresContext*      aPresContext,
                           nsReflowMetrics&     aDesiredSize,
                           const nsReflowState& aReflowState,
                           nsReflowStatus&      aStatus)
{
  NS_FRAME_TRACE_MSG(("enter nsScrollOuterFrame::Reflow: maxSize=%d,%d",
                      aReflowState.maxSize.width,
                      aReflowState.maxSize.height));

  if (nsnull == mFirstChild) {
    mFirstChild = new nsScrollInnerFrame(mContent, this);
    mChildCount = 1;
  }

  const nsStyleSpacing* spacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);
  nscoord lr = borderPadding.left + borderPadding.right;
  nscoord tb = borderPadding.top + borderPadding.bottom;

  // Get style size and determine how much area is available for the
  // child (the scroll inner frame) to layout into.
  nsSize maxSize, styleSize;
  PRIntn sf = nsCSSLayout::GetStyleSize(aPresContext, aReflowState,
                                        styleSize);
  if (NS_SIZE_HAS_WIDTH & sf) {
    maxSize.width = styleSize.width - lr;
  }
  else {
    maxSize.width = aReflowState.maxSize.width;
  }
  if (NS_SIZE_HAS_HEIGHT & sf) {
    maxSize.height = styleSize.height - tb;
  }
  else {
    maxSize.height = NS_UNCONSTRAINEDSIZE;
  }
  
  // Reflow the child and get its desired size
  nsReflowState kidReflowState(mFirstChild, aReflowState, maxSize);
  mFirstChild->WillReflow(*aPresContext);
  aStatus = ReflowChild(mFirstChild, aPresContext, aDesiredSize,
                        kidReflowState);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
  // Place and size the child
  nsRect rect(borderPadding.left, borderPadding.top,
              aDesiredSize.width, aDesiredSize.height);
  mFirstChild->SetRect(rect);
  mFirstChild->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);

  // The scroll outer frame either shrink wraps around it's single
  // child OR uses the style width/height.
  if (NS_SIZE_HAS_WIDTH & sf) {
    aDesiredSize.width = styleSize.width;
  }
  else {
    aDesiredSize.width += lr;
  }
  if (NS_SIZE_HAS_HEIGHT & sf) {
    aDesiredSize.height = styleSize.height;
  }
  else {
    aDesiredSize.height += tb;
  }
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  NS_FRAME_TRACE_MSG(
    ("exit nsScrollOuterFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}

NS_IMETHODIMP
nsScrollOuterFrame::ListTag(FILE* out) const
{
  fputs("*scrollouterframe<", out);
  nsIAtom* atom = mContent->GetTag();
  if (nsnull != atom) {
    nsAutoString tmp;
    atom->ToString(tmp);
    fputs(tmp, out);
  }
  PRInt32 contentIndex;
  GetContentIndex(contentIndex);
  fprintf(out, ">(%d)@%p", contentIndex, this);
  return NS_OK;
}

PRIntn
nsScrollOuterFrame::GetSkipSides() const
{
  return 0;
}

//----------------------------------------------------------------------

nsresult
NS_NewScrollFrame(nsIFrame**  aInstancePtrResult,
                  nsIContent* aContent,
                  nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsScrollOuterFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}
