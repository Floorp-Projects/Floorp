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
#include "nsHTMLContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsCSSRendering.h"
#include "nsIScrollableView.h"
#include "nsWidgetsCID.h"
#include "nsIAreaFrame.h"
#include "nsScrollFrame.h"

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);

//----------------------------------------------------------------------

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

  nsIFrame* frame = mFrames.FirstChild();

#ifdef NS_DEBUG
  // Verify that the scrolled frame has a view
  nsIView*  scrolledView;
  frame->GetView(scrolledView);
  NS_ASSERTION(nsnull != scrolledView, "no view");
#endif

  // We need to allow the view's position to be different than the
  // frame's position
  nsFrameState  state;
  frame->GetFrameState(state);
  state &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
  frame->SetFrameState(state);

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
    nsIFrame* frame = mFrames.FirstChild();
    nsIHTMLReflow*  htmlReflow;

    frame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
    htmlReflow->DidReflow(aPresContext, aStatus);

    // Size the scrolled frame's view. Leave its position alone
    nsSize          size;
    nsIViewManager* vm;
    nsIView*        scrolledView;

    frame->GetSize(size);
    frame->GetView(scrolledView);
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
                      aReflowState.availableWidth,
                      aReflowState.availableHeight));

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
    NS_ASSERTION(nextFrame == mFrames.FirstChild(), "unexpected reflow command next-frame");
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

  nsMargin  padding;
  nsHTMLReflowState::ComputePaddingFor(this, (const nsHTMLReflowState*)aReflowState.parentReflowState,
                                       padding);

  // Compute the scroll area size. This is the area inside of our border edge
  // and inside of any vertical and horizontal scrollbars. We need to add
  // back the padding area that was subtracted off
  nsSize scrollAreaSize;
  scrollAreaSize.width = aReflowState.computedWidth + padding.left + padding.right;
  // Subtract for the width of the vertical scrollbar. We always do this
  // regardless of whether scrollbars are auto or always visible
  scrollAreaSize.width -= NSToCoordRound(sbWidth);
  if (NS_AUTOHEIGHT == aReflowState.computedHeight) {
    // We have an 'auto' height and so we should shrink wrap around the
    // scrolled frame. Let the scrolled frame be as high as the available
    // height
    scrollAreaSize.height = aReflowState.availableHeight;
    scrollAreaSize.height -= border.top + border.bottom;

    // XXX Check for min/max limits...
  } else {
    // We have a fixed height so use the computed height plus padding
    // that applies to the scrolled frame
    scrollAreaSize.height = aReflowState.computedHeight + padding.top +
      padding.bottom;
  }
  // If scrollbars are always visible then subtract for the height of the
  // horizontal scrollbar
  if (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) {
    scrollAreaSize.height -= NSToCoordRound(sbHeight);
  }

  // Reflow the child and get its desired size. Let it be as high as it
  // wants
  nsIFrame*           kidFrame = mFrames.FirstChild();
  nsSize              kidReflowSize(scrollAreaSize.width, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState   kidReflowState(aPresContext, kidFrame, aReflowState,
                                     kidReflowSize);
  nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.maxElementSize);

  // Recompute the computed width based on the scroll area size
  kidReflowState.computedWidth = scrollAreaSize.width - padding.left -
    padding.right;
  kidReflowState.computedHeight = NS_AUTOHEIGHT;

  ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
              aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

  // If it's an area frame then get the total size, which includes the
  // space taken up by absolutely positioned child elements
  nsIAreaFrame* areaFrame;
  if (NS_SUCCEEDED(kidFrame->QueryInterface(kAreaFrameIID, (void**)&areaFrame))) {
    nscoord xMost, yMost;

    areaFrame->GetPositionedInfo(xMost, yMost);
    if (xMost > kidDesiredSize.width) {
      kidDesiredSize.width = xMost;
    }
    if (yMost > kidDesiredSize.height) {
      kidDesiredSize.height = yMost;
    }
  }
  
  // Make sure the height of the scrolled frame fills the entire scroll area,
  // unless we're shrink wrapping
  if (NS_AUTOHEIGHT != aReflowState.computedHeight) {
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
  // Make sure the width of the scrolled frame fills the entire scroll area
  if (kidDesiredSize.width < scrollAreaSize.width) {
    kidDesiredSize.width = scrollAreaSize.width;
  }

  // Place and size the child.
  nsRect rect(border.left, border.top, kidDesiredSize.width, kidDesiredSize.height);
  kidFrame->SetRect(rect);

  // XXX Moved to root frame
#if 0
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
#endif

  // Compute our desired size
  aDesiredSize.width = scrollAreaSize.width;
  aDesiredSize.width += border.left + border.right + NSToCoordRound(sbWidth);
  
  // For the height if we're shrink wrapping then use the child's desired size;
  // otherwise, use the scroll area size
  if (NS_AUTOHEIGHT == aReflowState.computedHeight) {
    aDesiredSize.height = kidDesiredSize.height;
  } else {
    aDesiredSize.height = scrollAreaSize.height;
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
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer)
{
  if (eFramePaintLayer_Underlay == aWhichLayer) {
    // Only paint the border and background if we're visible
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);

    if (display->mVisible) {
      // Paint our border only (no background)
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, 0);
    }
  }

  // Paint our children
  return nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                                 aWhichLayer);
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

NS_IMETHODIMP 
nsScrollFrame::ReResolveStyleContext(nsIPresContext* aPresContext, 
                                     nsIStyleContext* aParentContext) 
{ 
  nsresult rv = nsHTMLContainerFrame::ReResolveStyleContext(aPresContext, aParentContext); 

  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  nsIView * view; 
  GetView(view); 
  if (view) { 
    view->SetVisibility(NS_STYLE_VISIBILITY_HIDDEN == disp->mVisible ?
nsViewVisibility_kHide:nsViewVisibility_kShow); 
  } 
  return rv; 
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
