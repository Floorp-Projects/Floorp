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

nsresult
NS_NewScrollFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsScrollFrame* it = new nsScrollFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsScrollFrame::nsScrollFrame()
{
}

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

  // There must be one and only one child frame
  if (!frame) {
    return NS_ERROR_INVALID_ARG;
  } else if (mFrames.GetLength() > 1) {
    return NS_ERROR_UNEXPECTED;
  }

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
nsScrollFrame::AppendFrames(nsIPresContext& aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScrollFrame::InsertFrames(nsIPresContext& aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aPrevFrame,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScrollFrame::RemoveFrame(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame)
{
  // Scroll frame doesn't support incremental changes
  return NS_ERROR_NOT_IMPLEMENTED;
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
nsScrollFrame::CreateScrollingViewWidget(nsIView* aView, const nsStylePosition* aPosition)
{
  nsresult rv = NS_OK;
   // If it's fixed positioned, then create a widget 
  if (NS_STYLE_POSITION_FIXED == aPosition->mPosition) {
    rv = aView->CreateWidget(kWidgetCID);
  }

  return(rv);
}

nsresult
nsScrollFrame::GetScrollingParentView(nsIFrame* aParent, nsIView** aParentView)
{
  nsresult rv = aParent->GetView(aParentView);
  NS_ASSERTION(aParentView, "GetParentWithView failed");
  return(rv);
}

nsresult
nsScrollFrame::CreateScrollingView(nsIPresContext& aPresContext)
{
  nsIView*  view;

   //Get parent frame
  nsIFrame* parent;
  GetParentWithView(&parent);
  NS_ASSERTION(parent, "GetParentWithView failed");

  // Get parent view
  nsIView* parentView = nsnull;
  GetScrollingParentView(parent, &parentView);
 
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
    view->Init(viewManager, mRect, parentView, nsnull, display->mVisible ?
               nsViewVisibility_kShow : nsViewVisibility_kHide);

    // Insert the view into the view hierarchy
    viewManager->InsertChild(parentView, view, zIndex);

    // Set the view's opacity
    viewManager->SetViewOpacity(view, color->mOpacity);

    // Because we only paint the border and we don't paint a background,
    // inform the view manager that we have transparent content
    viewManager->SetViewContentTransparency(view, PR_TRUE);

    // If it's fixed positioned, then create a widget too
    CreateScrollingViewWidget(view, position);

    // Get the nsIScrollableView interface
    nsIScrollableView* scrollingView;
    view->QueryInterface(kScrollViewIID, (void**)&scrollingView);

    // Have the scrolling view create its internal widgets
    scrollingView->CreateScrollControls();

    // Set the scrolling view's scroll preference
    nsScrollPreference scrollPref = (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow)
                                    ? nsScrollPreference_kAlwaysScroll :
                                      nsScrollPreference_kAuto;

    // If this is a scroll frame for a viewport and its webshell 
    // has its scrolling set, use that value
    // XXX This is a huge hack, and we should not be checking the web shell's
    // scrolling preference...
    nsIFrame* parentFrame = nsnull;
    GetParent(&parentFrame);
    nsIAtom* frameType = nsnull;
    parent->GetFrameType(&frameType);
    if (nsLayoutAtoms::viewportFrame == frameType) {
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
    NS_IF_RELEASE(frameType);
    scrollingView->SetScrollPreference(scrollPref);

    // Set the scrolling view's insets to whatever our border is
    nsMargin border;
    if (!spacing->GetBorder(border)) {
      NS_NOTYETIMPLEMENTED("percentage border");
      border.SizeTo(0, 0, 0, 0);
    }
    scrollingView->SetControlInsets(border);

    // Remember our view
    SetView(view);
  }

  NS_RELEASE(viewManager);
  return rv;
}

// Returns the width of the vertical scrollbar and the height of
// the horizontal scrollbar in twips
static inline void
GetScrollbarDimensions(nsIPresContext& aPresContext,
                       nscoord&        aWidth,
                       nscoord&        aHeight)
{
  float             sbWidth, sbHeight;
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext.GetDeviceContext(getter_AddRefs(dc));

  dc->GetScrollBarDimensions(sbWidth, sbHeight);
  aWidth = NSToCoordRound(sbWidth);
  aHeight = NSToCoordRound(sbHeight);
}

// Calculates the size of the scroll area. This is the area inside of the
// border edge and inside of any vertical and horizontal scrollbar
// Also returns whether space was reserved for the vertical scrollbar.
nsresult
nsScrollFrame::CalculateScrollAreaSize(nsIPresContext&          aPresContext,
                                       const nsHTMLReflowState& aReflowState,
                                       nsMargin&                aBorder,
                                       nscoord                  aSBWidth,
                                       nscoord                  aSBHeight,
                                       nsSize*                  aScrollAreaSize,
                                       PRBool*                  aRoomForVerticalScrollbar)
{
  *aRoomForVerticalScrollbar = PR_FALSE;  // assume there's no vertical scrollbar

  // Compute the scroll area width
  aScrollAreaSize->width = aReflowState.mComputedWidth;

  // We need to add back the padding area that was subtracted off by the
  // reflow state code.
  // XXX This isn't the best way to handle this...
  PRBool unconstrainedWidth = (NS_UNCONSTRAINEDSIZE == aScrollAreaSize->width);
  if (!unconstrainedWidth) {
    aScrollAreaSize->width += aReflowState.mComputedPadding.left +
                              aReflowState.mComputedPadding.right;
  }

  // Compute the scroll area size
  if (NS_AUTOHEIGHT == aReflowState.mComputedHeight) {
    // We have an 'auto' height and so we should shrink wrap around the
    // scrolled frame. Let the scrolled frame be as high as the available
    // height minus our border thickness
    aScrollAreaSize->height = aReflowState.availableHeight;
    if (NS_UNCONSTRAINEDSIZE != aScrollAreaSize->height) {
      aScrollAreaSize->height -= aBorder.top + aBorder.bottom;
    }
  } else {
    // We have a fixed height, so use the computed height and add back any
    // padding that was subtracted off by the reflow state code
    aScrollAreaSize->height = aReflowState.mComputedHeight +
                              aReflowState.mComputedPadding.top +
                              aReflowState.mComputedPadding.bottom;
  }

  // See whether we have 'auto' scrollbars
  if (aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL) {
    // Always show both scrollbars, so subtract for the space taken up by the
    // vertical scrollbar
    if (!unconstrainedWidth) {
      aScrollAreaSize->width -= aSBWidth;
    }
  } else {
    // Predict whether we'll need a vertical scrollbar
    if (eReflowReason_Initial == aReflowState.reason) {
      // If it's the initial reflow, then just assume we'll need the scrollbar
      *aRoomForVerticalScrollbar = PR_TRUE;

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
        scrollingView->GetScrollbarVisibility(aRoomForVerticalScrollbar, &unused);
      }
    }

    // If we're assuming we need a vertical scrollbar, then leave room
    if (*aRoomForVerticalScrollbar && !unconstrainedWidth) {
      aScrollAreaSize->width -= aSBWidth;
    }
  }

  // If scrollbars are always visible, then subtract for the height of the
  // horizontal scrollbar
  if ((NS_STYLE_OVERFLOW_SCROLL == aReflowState.mStyleDisplay->mOverflow) &&
      !unconstrainedWidth) {
    aScrollAreaSize->height -= aSBHeight;
  }

  return NS_OK;
}

// Calculate the total amount of space needed for the child frame,
// including its child frames that stick outside its bounds and any
// absolutely positioned child frames.
// Updates the width/height members of the reflow metrics
nsresult
nsScrollFrame::CalculateChildTotalSize(nsIFrame*            aKidFrame,
                                       nsHTMLReflowMetrics& aKidReflowMetrics)
{
  // If the frame has child frames that stick outside its bounds, then take
  // them into account, too
  nsFrameState  kidState;
  aKidFrame->GetFrameState(&kidState);
  if (NS_FRAME_OUTSIDE_CHILDREN & kidState) {
    aKidReflowMetrics.width = aKidReflowMetrics.mCombinedArea.width;
    aKidReflowMetrics.height = aKidReflowMetrics.mCombinedArea.height;
  }

  // If it's an area frame, then get the total size which includes the
  // space taken up by absolutely positioned child elements
  nsIAreaFrame* areaFrame;
  if (NS_SUCCEEDED(aKidFrame->QueryInterface(kAreaFrameIID, (void**)&areaFrame))) {
    nscoord xMost, yMost;

    areaFrame->GetPositionedInfo(xMost, yMost);
    if (xMost > aKidReflowMetrics.width) {
      aKidReflowMetrics.width = xMost;
    }
    if (yMost > aKidReflowMetrics.height) {
      aKidReflowMetrics.height = yMost;
    }
  }

  return NS_OK;
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
    NS_NOTYETIMPLEMENTED("percentage border");
    border.SizeTo(0, 0, 0, 0);
  }

  // Get the scrollbar dimensions (width of the vertical scrollbar and the
  // height of the horizontal scrollbar) in twips
  nscoord   sbWidth, sbHeight;
  GetScrollbarDimensions(aPresContext, sbWidth, sbHeight);

  // Compute the scroll area size (area inside of the border edge and inside
  // of any vertical and horizontal scrollbars), and whether space was left
  // for the vertical scrollbar
  nsSize scrollAreaSize;
  PRBool roomForVerticalScrollbar;
  CalculateScrollAreaSize(aPresContext, aReflowState, border, sbWidth, sbHeight,
                          &scrollAreaSize, &roomForVerticalScrollbar);
  
  // Reflow the child and get its desired size. Let it be as high as it
  // wants
  PRBool              unconstrainedWidth = (NS_UNCONSTRAINEDSIZE == scrollAreaSize.width);
  nsSize              kidReflowSize(scrollAreaSize.width, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                     kidFrame, kidReflowSize);
  nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.maxElementSize);

  // Reset the computed width based on the scroll area size
  // XXX Eplain why we have to do this...
  if (!unconstrainedWidth) {
    kidReflowState.mComputedWidth = scrollAreaSize.width -
                                    aReflowState.mComputedPadding.left -
                                    aReflowState.mComputedPadding.right;
  }
  kidReflowState.mComputedHeight = NS_AUTOHEIGHT;

  ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
              aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  CalculateChildTotalSize(kidFrame, kidDesiredSize);

  // If we're 'auto' scrolling and not shrink-wrapping our height, then see
  // whether we correctly predicted whether a vertical scrollbar is needed
  if ((aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL) &&
      (NS_AUTOHEIGHT != aReflowState.mComputedHeight)) {

    PRBool  mustReflow = PR_FALSE;

    // There are two cases to consider
    if (roomForVerticalScrollbar) {
      if (kidDesiredSize.height <= scrollAreaSize.height) {
        // We left room for the vertical scrollbar, but it's not needed;
        // reflow with a larger computed width
        // XXX We need to be checking for horizontal scrolling...
        kidReflowState.availableWidth += sbWidth;
        kidReflowState.mComputedWidth += sbWidth;
        scrollAreaSize.width += sbWidth;
        mustReflow = PR_TRUE;
      }
    } else {
      if (kidDesiredSize.height > scrollAreaSize.height) {
        // We didn't leave room for the vertical scrollbar, but it turns
        // out we needed it
        kidReflowState.availableWidth -= sbWidth;
        kidReflowState.mComputedWidth -= sbWidth;
        scrollAreaSize.width -= sbWidth;
        mustReflow = PR_TRUE;
      }
    }

    // Go ahead and reflow the child a second time
    if (mustReflow) {
      kidReflowState.reason = eReflowReason_Resize;
      ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                  aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
      CalculateChildTotalSize(kidFrame, kidDesiredSize);
    }
  }
  
  // Make sure the height of the scrolled frame fills the entire scroll area,
  // unless we're shrink wrapping
  if (NS_AUTOHEIGHT != aReflowState.mComputedHeight) {
    if (kidDesiredSize.height < scrollAreaSize.height) {
      kidDesiredSize.height = scrollAreaSize.height;

      // If there's an auto horizontal scrollbar and the scrollbar will be
      // visible then subtract for the space taken up by the scrollbar;
      // otherwise, we'll end up with a vertical scrollbar even if we don't
      // need one...
      if ((NS_STYLE_OVERFLOW_SCROLL != aReflowState.mStyleDisplay->mOverflow) &&
          (kidDesiredSize.width > scrollAreaSize.width)) {
        kidDesiredSize.height -= sbHeight;
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
  if ((kidDesiredSize.height > scrollAreaSize.height) ||
      (aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL)) {
    aDesiredSize.width += sbWidth;
  }
  
  // For the height if we're shrink wrapping then use whatever is smaller between
  // the available height and the child's desired size; otherwise, use the scroll
  // area size
  if (NS_AUTOHEIGHT == aReflowState.mComputedHeight) {
    aDesiredSize.height = PR_MIN(aReflowState.availableHeight, kidDesiredSize.height);
  } else {
    aDesiredSize.height = scrollAreaSize.height;
  }
  aDesiredSize.height += border.top + border.bottom;
  // XXX This should really be "if we have a visible horizontal scrollbar"...
  if (NS_STYLE_OVERFLOW_SCROLL == aReflowState.mStyleDisplay->mOverflow) {
    aDesiredSize.height += sbHeight;
  }

  if (nsnull != aDesiredSize.maxElementSize) {
    nscoord maxWidth = aDesiredSize.maxElementSize->width;
    maxWidth += border.left + border.right + sbWidth;
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
      nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
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
