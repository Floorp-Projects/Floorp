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
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsIReflowCommand.h"
#include "nsIDeviceContext.h"
#include "nsPageFrame.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsBodyFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsCSSRendering.h"
#include "nsIScrollableView.h"
#include "nsWidgetsCID.h"

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);

//----------------------------------------------------------------------

class nsScrollViewFrame : public nsHTMLContainerFrame {
public:
  nsScrollViewFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);
  
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  virtual PRIntn GetSkipSides() const;
};

nsScrollViewFrame::nsScrollViewFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

NS_IMETHODIMP
nsScrollViewFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{

  // Unless it's already a body frame, child frames that are containers
  // need to be wrapped in a body frame.
  // XXX Check for it already being a body frame...
  nsIFrame* wrapperFrame;
  if (CreateWrapperFrame(aPresContext, aChildList, wrapperFrame)) {
    mFirstChild = wrapperFrame;
  } else {
    mFirstChild = aChildList;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScrollViewFrame::Reflow(nsIPresContext&          aPresContext,
                          nsHTMLReflowMetrics&     aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsScrollViewFrame::Reflow: maxSize=%d,%d",
                      aReflowState.maxSize.width,
                      aReflowState.maxSize.height));

  // Scroll frame handles the border, and we handle the padding and background
  const nsStyleSpacing* spacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin padding;
  spacing->CalcPaddingFor(this, padding);

  // Allow the child frame to be as wide as our max width (minus scrollbar
  // width and padding), and as high as it wants to be.
  nsSize  kidMaxSize(aReflowState.maxSize.width, NS_UNCONSTRAINEDSIZE);

  if (NS_UNCONSTRAINEDSIZE != aReflowState.maxSize.width) {
    nsIDeviceContext* dc = aPresContext.GetDeviceContext();
    float             sbWidth, sbHeight;
  
    dc->GetScrollBarDimensions(sbWidth, sbHeight);
    kidMaxSize.width -= NSToCoordRound(sbWidth) + padding.left + padding.right;
    NS_RELEASE(dc);
  }
  
  // Reflow the child
  nsHTMLReflowMetrics kidMetrics(aDesiredSize.maxElementSize);
  nsHTMLReflowState   kidReflowState(aPresContext, mFirstChild, aReflowState, kidMaxSize);

  ReflowChild(mFirstChild, aPresContext, kidMetrics, kidReflowState, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
  // Place and size the child
  nsRect  rect(padding.left, padding.top, kidMetrics.width, kidMetrics.height);
  mFirstChild->SetRect(rect);

  // Determine our size
  aDesiredSize.width = kidMetrics.width + padding.left + padding.right;
  aDesiredSize.height = kidMetrics.height + padding.top + padding.bottom;

  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
    ("exit nsScrollViewFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}

PRIntn
nsScrollViewFrame::GetSkipSides() const
{
  // Scroll frame handles the border...
  return 0xF;
}

NS_IMETHODIMP
nsScrollViewFrame::ListTag(FILE* out) const
{
  fputs("*scrollviewframe<", out);
  nsIAtom* atom;
  mContent->GetTag(atom);
  if (nsnull != atom) {
    nsAutoString tmp;
    atom->ToString(tmp);
    fputs(tmp, out);
    NS_RELEASE(atom);
  }
  fprintf(out, ">(%d)@%p", ContentIndexInContainer(this), this);
  return NS_OK;
}

//----------------------------------------------------------------------

/**
 * The scroll frame creates and manages the scrolling view. It creates
 * a nsScrollViewFrame which handles padding and rendering of the
 * background.
 */
class nsScrollFrame : public nsHTMLContainerFrame {
public:
  nsScrollFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);

  NS_IMETHOD DidReflow(nsIPresContext&   aPresContext,
                       nsDidReflowStatus aStatus);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);

  NS_IMETHOD  ListTag(FILE* out = stdout) const;

protected:
  virtual PRIntn GetSkipSides() const;

private:
  nsresult CreateScrollingView();
};

nsScrollFrame::nsScrollFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

NS_IMETHODIMP
nsScrollFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  NS_PRECONDITION(nsnull != aChildList, "no child frame");

  // Create a scroll view frame
  mFirstChild = new nsScrollViewFrame(mContent, this);
  if (nsnull == mFirstChild) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // Have it use our style context
  mFirstChild->SetStyleContext(&aPresContext, mStyleContext);

  // Reset the child frame's geometric and content parent to be
  // the scroll view frame
#ifdef NS_DEBUG
  // Verify that there's only one child frame
  nsIFrame* nextSibling;
  aChildList->GetNextSibling(nextSibling);
  NS_ASSERTION(nsnull == nextSibling, "expected only one child");
#endif
  aChildList->SetGeometricParent(mFirstChild);
  aChildList->SetContentParent(mFirstChild);

  // Init the scroll view frame passing it the child list
  return mFirstChild->Init(aPresContext, aChildList);
}

NS_IMETHODIMP
nsScrollFrame::DidReflow(nsIPresContext&   aPresContext,
                         nsDidReflowStatus aStatus)
{
  nsresult  rv = NS_OK;

  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    // Let the default nsFrame implementation clear the state flags
    // and size and position our view
    rv = nsFrame::DidReflow(aPresContext, aStatus);
    
    // Send the DidReflow notification to the scroll view frame
    nsIHTMLReflow*  htmlReflow;
    
    mFirstChild->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
    htmlReflow->DidReflow(aPresContext, aStatus);

    // Size the scroll view frame's view. Don't change its position
    nsSize          size;
    nsIViewManager* vm;
    nsIView*        scrolledView;

    mFirstChild->GetSize(size);
    mFirstChild->GetView(scrolledView);
    scrolledView->GetViewManager(vm);
    vm->ResizeView(scrolledView, size.width, size.height);
    NS_RELEASE(vm);
  }

  return rv;
}

nsresult
nsScrollFrame::CreateScrollingView()
{
  nsIView*  view;

  // Get parent view
  nsIFrame* parent;
  GetParentWithView(parent);
  NS_ASSERTION(parent, "GetParentWithView failed");
  nsIView* parentView;
  parent->GetView(parentView);
  NS_ASSERTION(parentView, "GetParentWithView failed");

  // Get the view manager
  nsIViewManager* viewManager;
  parentView->GetViewManager(viewManager);

  // Create the scrolling view
  nsresult rv = nsRepository::CreateInstance(kScrollingViewCID, 
                                             nsnull, 
                                             kIViewIID, 
                                             (void **)&view);

  if (NS_OK == rv) {
    // Initialize the scrolling view
    // XXX Check for opacity...
    //view->Init(viewManager, mRect, parentView, &kWidgetCID, nsnull, nsnull);
    view->Init(viewManager, mRect, parentView, nsnull, nsnull, nsnull);
  
    // Insert the view into the view hierarchy
    viewManager->InsertChild(parentView, view, 0);
  
    // If the background is transparent then inform the view manager
    const nsStyleColor* color = (const nsStyleColor*)
      mStyleContext->GetStyleData(eStyleStruct_Color);

    PRBool  isTransparent = (NS_STYLE_BG_COLOR_TRANSPARENT & color->mBackgroundFlags);
    if (isTransparent) {
      viewManager->SetViewContentTransparency(view, PR_TRUE);
    }

    // Get the nsIScrollableView interface
    nsIScrollableView* scrollingView;
    view->QueryInterface(kScrollViewIID, (void**)&scrollingView);

    // Set the scrolling view's insets to whatever our border is
    const nsStyleSpacing* spacing = (const nsStyleSpacing*)
      mStyleContext->GetStyleData(eStyleStruct_Spacing);
    nsMargin border;
    spacing->CalcBorderFor(this, border);
    scrollingView->SetControlInsets(border);

    // Remember our view
    SetView(view);

    // Create a view for the scroll view frame
    nsIView*  scrolledView;
    rv = nsRepository::CreateInstance(kViewCID, nsnull, kIViewIID, (void **)&scrolledView);
    if (NS_OK == rv) {
      // Bind the view to the frame
      mFirstChild->SetView(scrolledView);
  
      // Initialize the view
      scrolledView->Init(viewManager, nsRect(0, 0, 0, 0), parentView);
  
      // Set it as the scrolling view's scrolled view
      scrollingView->SetScrolledView(scrolledView);
  
      // If the background is transparent then inform the view manager
      if (isTransparent) {
        viewManager->SetViewContentTransparency(scrolledView, PR_TRUE);
      }
  
      // We need to allow the view's position to be different than the
      // frame's position
      nsFrameState  state;
      mFirstChild->GetFrameState(state);
      state &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
      mFirstChild->SetFrameState(state);
    }
  }

  NS_RELEASE(viewManager);
  return rv;
}

//XXX incremental reflow pass through
NS_IMETHODIMP
nsScrollFrame::Reflow(nsIPresContext&          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsScrollFrame::Reflow: maxSize=%d,%d",
                      aReflowState.maxSize.width,
                      aReflowState.maxSize.height));

  // If it's out initial reflow then create a scrolling view
  if (eReflowReason_Initial == aReflowState.reason) {
    CreateScrollingView();
  }

  const nsStyleSpacing* spacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin border;
  spacing->CalcBorderFor(this, border);
  nscoord lr = border.left + border.right;
  nscoord tb = border.top + border.bottom;

  // Compute the scroll view frame's max size taking into account our
  // borders
  nsSize kidMaxSize;
  if (aReflowState.HaveConstrainedWidth()) {
    // This value reflects space for the content area only, so don't
    // subtract for borders...
    kidMaxSize.width = aReflowState.minWidth;
  }
  else {
    kidMaxSize.width = aReflowState.maxSize.width;
    if (NS_UNCONSTRAINEDSIZE != kidMaxSize.width) {
      kidMaxSize.width -= lr;
    }
  }
  if (aReflowState.HaveConstrainedHeight()) {
    // This value reflects space for the content area only, so don't
    // subtract for borders...
    kidMaxSize.height = aReflowState.minHeight;
  }
  else {
    kidMaxSize.height = aReflowState.maxSize.height;
    if (NS_UNCONSTRAINEDSIZE != kidMaxSize.height) {
      kidMaxSize.height -= tb;
    }
  }

  // Reflow the child and get its desired size. Let it be as high as it
  // wants
  nsHTMLReflowState  kidReflowState(aPresContext, mFirstChild, aReflowState,
                                    nsSize(kidMaxSize.width, NS_UNCONSTRAINEDSIZE));

  // XXX Don't use aDesiredSize...
  ReflowChild(mFirstChild, aPresContext, aDesiredSize, kidReflowState, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
  // Place and size the child.
  // XXX Make sure it's at least as big as we are...
  nsRect rect(border.left, border.top, aDesiredSize.width, aDesiredSize.height);
  mFirstChild->SetRect(rect);

  // Compute our desired size
  aDesiredSize.width = kidMaxSize.width + lr;
  if (NS_UNCONSTRAINEDSIZE == kidMaxSize.height) {
    // Use the scroll view's desired height plus any borders
    aDesiredSize.height += tb;
  } else {
    // XXX This isn't correct. If our height is fixed, then use the fixed height;
    // otherwise use the MIN of the constrained height and the scroll view's height
    // plus borders...
    aDesiredSize.height = kidMaxSize.height + tb;
  }
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
    ("exit nsScrollFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}

NS_IMETHODIMP
nsScrollFrame::Paint(nsIPresContext&      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect)
{
  // Paint our border only (no background)
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);

  nsRect  rect(0, 0, mRect.width, mRect.height);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, rect, *spacing, 0);

  // Paint our children
  return nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
}

PRIntn
nsScrollFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsScrollFrame::ListTag(FILE* out) const
{
  fputs("*scrollframe<", out);
  nsIAtom* atom;
  mContent->GetTag(atom);
  if (nsnull != atom) {
    nsAutoString tmp;
    atom->ToString(tmp);
    fputs(tmp, out);
    NS_RELEASE(atom);
  }
  fprintf(out, ">(%d)@%p", ContentIndexInContainer(this), this);
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
NS_NewScrollFrame(nsIContent* aContent,
                  nsIFrame*   aParentFrame,
                  nsIFrame*&  aResult)
{
  aResult = new nsScrollFrame(aContent, aParentFrame);
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}
