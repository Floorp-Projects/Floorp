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
  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext);

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD DidReflow(nsIPresContext&   aPresContext,
                       nsDidReflowStatus aStatus);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);

  NS_IMETHOD GetFrameName(nsString& aResult) const;

protected:
  virtual PRIntn GetSkipSides() const;

private:
  nsresult CreateScrollingView();
};

NS_IMETHODIMP
nsScrollFrame::Init(nsIPresContext&  aPresContext,
                    nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIStyleContext* aStyleContext)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent,
                                            aParent, aStyleContext);

  // Create the scrolling view
  CreateScrollingView();
  return rv;
}
  
NS_IMETHODIMP
nsScrollFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName,
                                                           aChildList);

#ifdef NS_DEBUG
  // Verify that the scrolled frame has a view
  nsIView*  scrolledView;

  mFirstChild->GetView(scrolledView);
  NS_ASSERTION(nsnull != scrolledView, "no view");
#endif

  // We need to allow the view's position to be different than the
  // frame's position
  nsFrameState  state;
  mFirstChild->GetFrameState(state);
  state &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
  mFirstChild->SetFrameState(state);

  return rv;
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
    view->Init(viewManager, mRect, parentView);

    // Insert the view into the view hierarchy
    viewManager->InsertChild(parentView, view, zIndex);

    // Set the view's opacity
    viewManager->SetViewOpacity(view, color->mOpacity);

    // Because we only paintg the border and we don't paint a background,
    // inform the view manager that we have transparent content
    viewManager->SetViewContentTransparency(view, PR_TRUE);

    // Get the nsIScrollableView interface
    nsIScrollableView* scrollingView;
    view->QueryInterface(kScrollViewIID, (void**)&scrollingView);

    // Create widgets for scrolling
    scrollingView->CreateScrollControls();

    // Set the scroll preference
    nsScrollPreference scrollPref = (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow)
                                    ? nsScrollPreference_kAlwaysScroll :
                                      nsScrollPreference_kAuto;
    scrollingView->SetScrollPreference(scrollPref);

    // Set the scrolling view's insets to whatever our border is
    nsMargin border;
    if (!spacing->GetBorder(border)) {
      border.SizeTo(0, 0, 0, 0);
    }
    scrollingView->SetControlInsets(border);

    // Remember our view
    SetView(view);
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

  // Special handling for incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    // See whether we're the target of the reflow command
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      nsIReflowCommand::ReflowType  type;

      // The only type of reflow command we expect to get is a style
      // change reflow command
      aReflowState.reflowCommand->GetType(type);
      NS_ASSERTION(nsIReflowCommand::StyleChanged == type, "unexpected reflow type");

      // Make a copy of the reflow state (with a different reflow reason) and
      // then recurse
      nsHTMLReflowState reflowState(aReflowState);
      reflowState.reason = eReflowReason_StyleChange;
      reflowState.reflowCommand = nsnull;
      return Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
    }

    // Get the next frame in the reflow chain, and verify that it's our
    // child frame
    aReflowState.reflowCommand->GetNext(nextFrame);
    NS_ASSERTION(nextFrame == mFirstChild, "unexpected reflow command next-frame");
  }

  // Calculate the amount of space needed for borders
  const nsStyleSpacing*  spacing = (const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin border;
  if (!spacing->GetBorder(border)) {
    border.SizeTo(0, 0, 0, 0);
  }

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
  if ((eHTMLFrameConstraint_Fixed == aReflowState.widthConstraint) ||
      (eHTMLFrameConstraint_FixedContent == aReflowState.widthConstraint)) {
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

  if ((eHTMLFrameConstraint_Fixed == aReflowState.heightConstraint) ||
      (eHTMLFrameConstraint_FixedContent == aReflowState.heightConstraint)) {
    // The reflow state height reflects space for the content area only, so don't
    // subtract for borders...
    scrollAreaSize.height = aReflowState.minHeight;

    if (eHTMLFrameConstraint_Fixed == aReflowState.heightConstraint) {
      scrollAreaSize.height -= border.top + border.bottom;

      // If scrollbars are always visible then subtract for the
      // height of the horizontal scrollbar
      if (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) {
        scrollAreaSize.height -= NSToCoordRound(sbHeight);
      }
    }
  }
  else {
    // Use the max height in the reflow state
    scrollAreaSize.height = aReflowState.maxSize.height;
    if (NS_UNCONSTRAINEDSIZE != scrollAreaSize.height) {
      // The height is constrained so subtract for borders
      scrollAreaSize.height -= border.top + border.bottom;

      // If scrollbars are always visible then subtract for the
      // height of the horizontal scrollbar
      if (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) {
        scrollAreaSize.height -= NSToCoordRound(sbHeight);
      }
    }
  }
  //@ Make me a subroutine...

  // Reflow the child and get its desired size. Let it be as high as it
  // wants
  nsSize              kidReflowSize(scrollAreaSize.width, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState   kidReflowState(aPresContext, mFirstChild, aReflowState,
                                     kidReflowSize);
  nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.maxElementSize);

  ReflowChild(mFirstChild, aPresContext, kidDesiredSize, kidReflowState, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
  // Make sure the scrolled frame fills the entire scroll area along a
  // fixed dimension
  if ((eHTMLFrameConstraint_Fixed == aReflowState.heightConstraint) ||
      (eHTMLFrameConstraint_FixedContent == aReflowState.heightConstraint)) {
    if (kidDesiredSize.height < scrollAreaSize.height) {
      kidDesiredSize.height = scrollAreaSize.height;

      // If there's an auto horizontal scrollbar and the scrollbar will be
      // visible then subtract for the space taken up by the scrollbar;
      // otherwise, we'll end up with a vertical scrollbar even if we don't
      // need one...
      if ((NS_STYLE_OVERFLOW_SCROLL != display->mOverflow) &&
          (kidDesiredSize.width > scrollAreaSize.width)) {
        kidDesiredSize.height -= NSToCoordRound(sbHeight);
      }
    }
    
    if (kidDesiredSize.height <= scrollAreaSize.height) {
      // If the scrollbars are auto and the scrolled frame is fully visible
      // vertically then the vertical scrollbar will be hidden so increase the
      // width of the scrolled frame
      if (display->mOverflow != NS_STYLE_OVERFLOW_SCROLL) {
        kidDesiredSize.width += NSToCoordRound(sbWidth);
      }
    }
  }
  if ((eHTMLFrameConstraint_Fixed == aReflowState.widthConstraint) ||
      (eHTMLFrameConstraint_FixedContent == aReflowState.widthConstraint)) {
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
  if ((eHTMLFrameConstraint_Fixed == aReflowState.widthConstraint) ||
      (eHTMLFrameConstraint_FixedContent == aReflowState.widthConstraint)) {
    aDesiredSize.width = scrollAreaSize.width;
  } else {
    aDesiredSize.width = kidDesiredSize.width;
  }
  aDesiredSize.width += border.left + border.right + NSToCoordRound(sbWidth);

  if ((eHTMLFrameConstraint_Fixed == aReflowState.heightConstraint) ||
      (eHTMLFrameConstraint_FixedContent == aReflowState.heightConstraint)) {
    aDesiredSize.height = scrollAreaSize.height;
  } else {
    aDesiredSize.height = kidDesiredSize.height;
  }
  aDesiredSize.height += border.top + border.bottom;
  // XXX This should really be "if we have a visible horizontal scrollbar"...
  if (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) {
    aDesiredSize.height += NSToCoordRound(sbHeight);
  }

  if (nsnull != aDesiredSize.maxElementSize) {
    nscoord maxWidth = aDesiredSize.maxElementSize->width;
    maxWidth += border.left + border.right + NSToCoordRound(sbWidth);
    nscoord maxHeight = aDesiredSize.maxElementSize->height;
    maxHeight += border.top + border.bottom;
    aDesiredSize.maxElementSize->width = maxWidth;
    aDesiredSize.maxElementSize->height = maxHeight;
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
nsScrollFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Scroll", aResult);
}

//----------------------------------------------------------------------

nsresult
NS_NewScrollFrame(nsIFrame*&  aResult)
{
  aResult = new nsScrollFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}
