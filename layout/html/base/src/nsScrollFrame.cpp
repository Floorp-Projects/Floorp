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
#include "nsCOMPtr.h"
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
#include "nsLayoutAtoms.h"
#include "nsIWebShell.h"

static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);

static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);

//----------------------------------------------------------------------

NS_IMETHODIMP
nsScrollFrame::Init(nsIPresContext&  aPresContext,
                    nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIStyleContext* aStyleContext,
                    nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent,
                                            aParent, aStyleContext,
                                            aPrevInFlow);

  // Create the scrolling view
  CreateScrollingView(aPresContext);
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
  frame->GetView(&scrolledView);
  NS_ASSERTION(nsnull != scrolledView, "no view");
#endif

  // We need to allow the view's position to be different than the
  // frame's position
  nsFrameState  state;
  frame->GetFrameState(&state);
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
    frame->GetView(&scrolledView);
    scrolledView->GetViewManager(vm);
    vm->ResizeView(scrolledView, size.width, size.height);
    NS_RELEASE(vm);

    // Have the scrolling view layout
    nsIScrollableView* scrollingView;
    nsIView*           view;
    GetView(&view);
    if (NS_SUCCEEDED(view->QueryInterface(kScrollViewIID, (void**)&scrollingView))) {
      scrollingView->ComputeScrollOffsets(PR_TRUE);
    }
  }

  return rv;
}

nsresult
nsScrollFrame::CreateScrollingView(nsIPresContext& aPresContext)
{
  nsIView*  view;

  // Get parent view
  nsIFrame* parent;
  GetParentWithView(&parent);
  NS_ASSERTION(parent, "GetParentWithView failed");
  nsIView* parentView;
  parent->GetView(&parentView);
  NS_ASSERTION(parentView, "GetParentWithView failed");

  // Get the view manager
  nsIViewManager* viewManager;
  parentView->GetViewManager(viewManager);

  // Create the scrolling view
  nsresult rv = nsComponentManager::CreateInstance(kScrollingViewCID, 
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

    // XXX If it's fixed positioned, then create a widget too
    if (NS_STYLE_POSITION_FIXED == position->mPosition) {
      view->CreateWidget(kWidgetCID);
    }

    // Get the nsIScrollableView interface
    nsIScrollableView* scrollingView;
    view->QueryInterface(kScrollViewIID, (void**)&scrollingView);

    // Create widgets for scrolling
    scrollingView->CreateScrollControls();

    // Set the scroll preference
    nsScrollPreference scrollPref = (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow)
                                    ? nsScrollPreference_kAlwaysScroll :
                                      nsScrollPreference_kAuto;
    // if this is a scroll frame for a viewport and its webshell 
    // has its scrolling set, use that value
    nsIFrame* parentFrame = nsnull;
    GetParent(&parentFrame);
    //nsCOMPtr<nsIAtom> frameType;
    nsIAtom* frameType = nsnull;
    //parent->GetFrameType(getter_AddRefs(frameType));
    parent->GetFrameType(&frameType);
    if (nsLayoutAtoms::viewportFrame == frameType) {
      NS_RELEASE(frameType);
      nsCOMPtr<nsISupports> container;
      rv = aPresContext.GetContainer(getter_AddRefs(container));
      if (NS_SUCCEEDED(rv) && container) {
        nsCOMPtr<nsIWebShell> webShell;
        rv = container->QueryInterface(kIWebShellIID, getter_AddRefs(webShell));
        if (NS_SUCCEEDED(rv)) {
          PRInt32 scrolling = -1; // -1 indicates not set
          webShell->GetScrolling(scrolling);
          if (-1 != scrolling) {
            if (NS_STYLE_OVERFLOW_SCROLL == scrolling) {
              scrollPref = nsScrollPreference_kAlwaysScroll;
            } else if (NS_STYLE_OVERFLOW_AUTO == scrolling) {
              scrollPref = nsScrollPreference_kAuto;
            }
          }
        }
      }
    }
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

  nsIFrame* kidFrame = mFrames.FirstChild();
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
    NS_ASSERTION(nextFrame == kidFrame, "unexpected reflow command next-frame");
  }

  // Calculate the amount of space needed for borders
  nsMargin border;
  if (!aReflowState.mStyleSpacing->GetBorder(border)) {
    border.SizeTo(0, 0, 0, 0);
  }

  // Get the scrollbar dimensions
  float             sbWidth, sbHeight;
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext.GetDeviceContext(getter_AddRefs(dc));

  dc->GetScrollBarDimensions(sbWidth, sbHeight);

  // Compute the scroll area size. This is the area inside of our border edge
  // and inside of any vertical and horizontal scrollbars. We need to add
  // back the padding area that was subtracted off
  // XXX This isn't the best way to handle this...
  nsSize scrollAreaSize;
  PRBool roomForVerticalScrollbar = PR_FALSE;  // if we allocated room for vertical scrollbar

  scrollAreaSize.width = aReflowState.computedWidth;
  PRBool unconstrainedWidth = NS_UNCONSTRAINEDSIZE == scrollAreaSize.width;
  if (!unconstrainedWidth) {
    scrollAreaSize.width += aReflowState.mComputedPadding.left +
                            aReflowState.mComputedPadding.right;
  }

  if (NS_AUTOHEIGHT == aReflowState.computedHeight) {
    // We have an 'auto' height and so we should shrink wrap around the
    // scrolled frame. Let the scrolled frame be as high as the available
    // height
    scrollAreaSize.height = aReflowState.availableHeight;
    if (NS_UNCONSTRAINEDSIZE != scrollAreaSize.height) {
      scrollAreaSize.height -= border.top + border.bottom;
    }
  } else {
    // We have a fixed height so use the computed height plus padding
    // that applies to the scrolled frame
    scrollAreaSize.height = aReflowState.computedHeight +
                            aReflowState.mComputedPadding.top +
                            aReflowState.mComputedPadding.bottom;
  }

  // See whether we have 'auto' scrollbars
  // XXX What about the case where we're shrink-wrapping our height?
  if (aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL) {
    // Always show scrollbars, so subtract for the space talen up by the
    // vertical scrollbar
    if (!unconstrainedWidth) {
      scrollAreaSize.width -= NSToCoordRound(sbWidth);
    }
  } else {
    // Predict whether we'll need a vertical scrollbar
    if (eReflowReason_Initial == aReflowState.reason) {
      roomForVerticalScrollbar = PR_TRUE;

    } else {
      // Just assume the current scenario.
      // Note: an important but subtle point is that for incremental reflow
      // we must give the frame being reflowed the same amount of available
      // width; otherwise, it's not only just an incremental reflow but also
      nsIScrollableView* scrollingView;
      nsIView*           view;
      GetView(&view);
      if (NS_SUCCEEDED(view->QueryInterface(kScrollViewIID, (void**)&scrollingView))) {
        PRBool  unused;
        scrollingView->GetScrollbarVisibility(&roomForVerticalScrollbar, &unused);
      }
    }

    if (roomForVerticalScrollbar && !unconstrainedWidth) {
      scrollAreaSize.width -= NSToCoordRound(sbWidth);
    }
  }

  // If scrollbars are always visible, then subtract for the height of the
  // horizontal scrollbar
  if ((NS_STYLE_OVERFLOW_SCROLL == aReflowState.mStyleDisplay->mOverflow) &&
      !unconstrainedWidth) {
    scrollAreaSize.height -= NSToCoordRound(sbHeight);
  }

  // Reflow the child and get its desired size. Let it be as high as it
  // wants
  nsSize  kidReflowSize(scrollAreaSize.width, NS_UNCONSTRAINEDSIZE);

  if (!unconstrainedWidth) {
    kidReflowSize.width -= aReflowState.mComputedPadding.left +
                           aReflowState.mComputedPadding.right;
  }
  nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                     kidFrame, kidReflowSize);
  nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.maxElementSize);

  // Reset the computed width based on the scroll area size
  if (!unconstrainedWidth) {
    kidReflowState.computedWidth = scrollAreaSize.width -
                                   aReflowState.mComputedPadding.left -
                                   aReflowState.mComputedPadding.right;
  }
  kidReflowState.computedHeight = NS_AUTOHEIGHT;

  ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
              aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

  // If the frame has child frames that stick outside its bounds, then take
  // them into account, too
  nsFrameState  kidState;
  kidFrame->GetFrameState(&kidState);
  if (NS_FRAME_OUTSIDE_CHILDREN & kidState) {
    kidDesiredSize.width = kidDesiredSize.mCombinedArea.width;
    kidDesiredSize.height = kidDesiredSize.mCombinedArea.height;
  }

  // If it's an area frame, then get the total size which includes the
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

  // If we're 'auto' scrolling and not shrink-wrapping our height, then see
  // whether we correctly predicted whether a vertical scrollbar is needed
  if ((aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL) &&
      (NS_AUTOHEIGHT != aReflowState.computedHeight)) {

    PRBool  mustReflow = PR_FALSE;

    // There are two cases to consider
    if (roomForVerticalScrollbar) {
      if (kidDesiredSize.height <= scrollAreaSize.height) {
        // We left room for the vertical scrollbar, but it's not needed;
        // reflow with a larger computed width
        // XXX We need to be checking for horizontal scrolling...
        kidReflowState.availableWidth += NSToCoordRound(sbWidth);
        kidReflowState.computedWidth += NSToCoordRound(sbWidth);
        scrollAreaSize.width += NSToCoordRound(sbWidth);
        mustReflow = PR_TRUE;
      }
    } else {
      if (kidDesiredSize.height > scrollAreaSize.height) {
        // We didn't leave room for the vertical scrollbar, but it turns
        // out we needed it
        kidReflowState.availableWidth -= NSToCoordRound(sbWidth);
        kidReflowState.computedWidth -= NSToCoordRound(sbWidth);
        scrollAreaSize.width -= NSToCoordRound(sbWidth);
        mustReflow = PR_TRUE;
      }
    }

    if (mustReflow) {
      kidReflowState.reason = eReflowReason_Resize;
      ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                  aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

      // If the frame has child frames that stick outside its bounds, then take
      // them into account, too
      nsFrameState  kidState;
      kidFrame->GetFrameState(&kidState);
      if (NS_FRAME_OUTSIDE_CHILDREN & kidState) {
        kidDesiredSize.width = kidDesiredSize.mCombinedArea.width;
        kidDesiredSize.height = kidDesiredSize.mCombinedArea.height;
      }

      // If it's an area frame, then get the total size which includes the
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
      if ((NS_STYLE_OVERFLOW_SCROLL != aReflowState.mStyleDisplay->mOverflow) &&
          (kidDesiredSize.width > scrollAreaSize.width)) {
        kidDesiredSize.height -= NSToCoordRound(sbHeight);
      }
    }
  }
  // Make sure the width of the scrolled frame fills the entire scroll area
  if (kidDesiredSize.width < scrollAreaSize.width) {
    kidDesiredSize.width = scrollAreaSize.width;
  }

  // Place and size the child.
  nscoord x = border.left;
  nscoord y = border.top;
  nsRect rect(x, y, kidDesiredSize.width, kidDesiredSize.height);
  kidFrame->SetRect(rect);

  // Compute our desired size
  aDesiredSize.width = scrollAreaSize.width;
  aDesiredSize.width += border.left + border.right;
  if (kidDesiredSize.height > scrollAreaSize.height) {
    aDesiredSize.width += NSToCoordRound(sbWidth);
  }
  
  // For the height if we're shrink wrapping then use whatever is smaller between
  // the available height and the child's desired size; otherwise, use the scroll
  // area size
  if (NS_AUTOHEIGHT == aReflowState.computedHeight) {
    aDesiredSize.height = PR_MIN(aReflowState.availableHeight, kidDesiredSize.height);
  } else {
    aDesiredSize.height = scrollAreaSize.height;
  }
  aDesiredSize.height += border.top + border.bottom;
  // XXX This should really be "if we have a visible horizontal scrollbar"...
  if (NS_STYLE_OVERFLOW_SCROLL == aReflowState.mStyleDisplay->mOverflow) {
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
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // Only paint the border and background if we're visible
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);

    if (display->mVisible) {
      // Paint our border only (no background)
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, mStyleContext, 0);
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
nsScrollFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::scrollFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
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
