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
  NS_PRECONDITION(LengthOf(aChildList) == 1, "wrong number child frames");

  // Unless it's already a body frame, scrolled frames that are a container
  // need to be wrapped in a body frame.
  // XXX It would be nice to have a cleaner way to do this...
  nsIAbsoluteItems* absoluteItems;
  if (NS_FAILED(aChildList->QueryInterface(kIAbsoluteItemsIID, (void**)&absoluteItems))) {
    nsIFrame* wrapperFrame;
    if (CreateWrapperFrame(aPresContext, aChildList, wrapperFrame)) {
      aChildList = wrapperFrame;
    }
  }

  mFirstChild = aChildList;
  return NS_OK;
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
    
    // Send the DidReflow notification to the scrolled frame's view
    nsIHTMLReflow*  htmlReflow;
    
    mFirstChild->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
    htmlReflow->DidReflow(aPresContext, aStatus);

    // Size the scrolled frame's view. Leave its position alone
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
    const nsStylePosition* position = (const nsStylePosition*)
      mStyleContext->GetStyleData(eStyleStruct_Position);
    const nsStyleColor*    color = (const nsStyleColor*)
      mStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleSpacing*  spacing = (const nsStyleSpacing*)
      mStyleContext->GetStyleData(eStyleStruct_Spacing);
    const nsStyleDisplay*  display = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);

    // Get the z-index
    PRInt32 zIndex = 0;

    if (eStyleUnit_Integer == position->mZIndex.GetUnit()) {
      zIndex = position->mZIndex.GetIntValue();
    }

    // Initialize the scrolling view
    view->Init(viewManager, mRect, parentView, nsnull, nsnull, nsnull,
               nsnull, color->mOpacity);

    // Insert the view into the view hierarchy
    viewManager->InsertChild(parentView, view, zIndex);
  
    // If the background is transparent then inform the view manager
    PRBool  isTransparent = (NS_STYLE_BG_COLOR_TRANSPARENT & color->mBackgroundFlags);
    if (isTransparent) {
      viewManager->SetViewContentTransparency(view, PR_TRUE);
    }

    // Get the nsIScrollableView interface
    nsIScrollableView* scrollingView;
    view->QueryInterface(kScrollViewIID, (void**)&scrollingView);

    // Set the scroll prefrence
#if 0
    nsScrollPreference scrollPref = (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow)
                                    ? nsScrollPreference_kAlwaysScroll :
                                      nsScrollPreference_kAuto;
    scrollingView->SetScrollPreference(scrollPref);
#endif

    // Set the scrolling view's insets to whatever our border is
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
      scrolledView->Init(viewManager, nsRect(0, 0, 0, 0), parentView, nsnull,
                         nsnull, nsnull, nsnull, color->mOpacity);
  
      // If the background is transparent then inform the view manager
      if (isTransparent) {
        viewManager->SetViewContentTransparency(scrolledView, PR_TRUE);
      }
  
      // Set it as the scrolling view's scrolled view
      scrollingView->SetScrolledView(scrolledView);

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

  nsIFrame* targetFrame;
  nsIFrame* nextFrame;

  // Special handling for initial reflow and incremental reflow
  switch (aReflowState.reason) {
  case eReflowReason_Initial:
    // Create the scrolling view and the scrolled view
    CreateScrollingView();
    break;

  case eReflowReason_Incremental:
#ifdef NS_DEBUG
    // We should never be the target of the reflow command
    aReflowState.reflowCommand->GetTarget(targetFrame);
    NS_ASSERTION(targetFrame != this, "bad reflow command target-frame");
#endif

    // Get the next frame in the reflow chain, and verify that it's our
    // child frame
    aReflowState.reflowCommand->GetNext(nextFrame);
    NS_ASSERTION(nextFrame == mFirstChild, "unexpected reflow command next-frame");
    break;
  }

  // Calculate the amount of space needed for borders
  const nsStyleSpacing* spacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin border;
  spacing->CalcBorderFor(this, border);

  // See whether the scrollbars are always visible or auto
  const nsStyleDisplay*  display = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);

  // Get the scrollbar dimensions
  float             sbWidth, sbHeight;
  nsIDeviceContext* dc = aPresContext.GetDeviceContext();

  dc->GetScrollBarDimensions(sbWidth, sbHeight);
  NS_RELEASE(dc);

  //@ Make me a subroutine...

  // Compute the scroll area size. This is the area inside of our border edge
  // and inside of any vertical and horizontal scrollbars
  nsSize scrollAreaSize;
  if (aReflowState.HaveConstrainedWidth()) {
    // The reflow state width reflects space for the content area only, so don't
    // subtract for borders...
    scrollAreaSize.width = aReflowState.minWidth;

    if (eHTMLFrameConstraint_Fixed == aReflowState.widthConstraint) {
      scrollAreaSize.width -= border.left + border.right;
      scrollAreaSize.width -= NSToCoordRound(sbWidth);
    }
  }
  else {
    // Use the max width in the reflow state
    scrollAreaSize.width = aReflowState.maxSize.width;
    if (NS_UNCONSTRAINEDSIZE != scrollAreaSize.width) {
      // The width is constrained so subtract for borders
      scrollAreaSize.width -= border.left + border.right;

      // Subtract for the width of the vertical scrollbar. We always do this
      // regardless of whether scrollbars are auto or always visible
      scrollAreaSize.width -= NSToCoordRound(sbWidth);
    }
  }

  if (aReflowState.HaveConstrainedHeight()) {
    // The reflow state height reflects space for the content area only, so don't
    // subtract for borders...
    scrollAreaSize.height = aReflowState.minHeight;

    if (eHTMLFrameConstraint_Fixed == aReflowState.heightConstraint) {
      scrollAreaSize.height -= border.top + border.bottom;

#if 0
      // If scrollbars are always visible then subtract for the
      // height of the horizontal scrollbar
      if (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) {
        scrollAreaSize.height -= NSToCoordRound(sbHeight);
      }
#endif
    }
  }
  else {
    // Use the max height in the reflow state
    scrollAreaSize.height = aReflowState.maxSize.height;
    if (NS_UNCONSTRAINEDSIZE != scrollAreaSize.height) {
      // The height is constrained so subtract for borders
      scrollAreaSize.height -= border.top + border.bottom;

#if 0
      // If scrollbars are always visible then subtract for the
      // height of the horizontal scrollbar
      if (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) {
        scrollAreaSize.height -= NSToCoordRound(sbHeight);
      }
#endif
    }
  }
  //@ Make me a subroutine...

  // Reflow the child and get its desired size. Let it be as high as it
  // wants
  nsSize              kidReflowSize(scrollAreaSize.width, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState   kidReflowState(aPresContext, mFirstChild, aReflowState,
                                     kidReflowSize);
  nsHTMLReflowMetrics kidDesiredSize(nsnull);

  ReflowChild(mFirstChild, aPresContext, kidDesiredSize, kidReflowState, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
  // Make sure the scrolled frame fills the entire scroll area along a
  // fixed dimension
  if ((eHTMLFrameConstraint_Fixed == aReflowState.heightConstraint) || aReflowState.HaveConstrainedHeight()) {
    if (kidDesiredSize.height < scrollAreaSize.height) {
      kidDesiredSize.height = scrollAreaSize.height;
    }
    
    if (kidDesiredSize.height <= scrollAreaSize.height) {
      // If the scrollbars are auto and the scrolled frame is fully visible
      // vertically then the vertical scrollbar will be hidden so increase the
      // width of the scrolled frame
#if 0
      if (NS_STYLE_OVERFLOW_AUTO == display->mOverflow) {
#endif
        kidDesiredSize.width += NSToCoordRound(sbWidth);
#if 0
      }
#endif
    }
  }
  if ((eHTMLFrameConstraint_Fixed == aReflowState.widthConstraint) || aReflowState.HaveConstrainedWidth()) {
    if (kidDesiredSize.width < scrollAreaSize.width) {
      kidDesiredSize.width = scrollAreaSize.width;
    }
  }

  // Place and size the child.
  nsRect rect(border.left, border.top, kidDesiredSize.width, kidDesiredSize.height);
  mFirstChild->SetRect(rect);

  // If this is a resize reflow then repaint the scrolled frame
  if (eReflowReason_Resize == aReflowState.reason) {
    nsIView*        scrolledView;
    nsIViewManager* viewManager;
    nsRect          damageRect(0, 0, kidDesiredSize.width, kidDesiredSize.height);

    mFirstChild->GetView(scrolledView);
    scrolledView->GetViewManager(viewManager);
    viewManager->UpdateView(scrolledView, damageRect, NS_VMREFRESH_NO_SYNC);
    NS_RELEASE(viewManager);
  }

  // Compute our desired size. If our size was fixed then use the fixed size;
  // otherwise, shrink wrap around the scrolled frame
  if ((eHTMLFrameConstraint_Fixed == aReflowState.widthConstraint) || aReflowState.HaveConstrainedWidth()) {
    aDesiredSize.width = scrollAreaSize.width;
  } else {
    aDesiredSize.width = kidDesiredSize.width;
  }
  aDesiredSize.width += border.left + border.right + NSToCoordRound(sbWidth);

  if ((eHTMLFrameConstraint_Fixed == aReflowState.heightConstraint) || aReflowState.HaveConstrainedHeight()) {
    aDesiredSize.height = scrollAreaSize.height;
  } else {
    aDesiredSize.height = kidDesiredSize.height;
  }
  aDesiredSize.height += border.top + border.bottom;
#if 0
  // XXX This should really be "if we have a visible horizontal scrollbar"...
  if (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) {
    aDesiredSize.height += NSToCoordRound(sbHeight);
  }
#endif

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
