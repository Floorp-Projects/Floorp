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
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsIDocument.h"
#include "nsIReflowCommand.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsHTMLIIDs.h"
#include "nsPageFrame.h"
#include "nsIRenderingContext.h"
#include "nsGUIEvent.h"
#include "nsDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIViewManager.h"
#include "nsHTMLAtoms.h"
#include "nsIEventStateManager.h"
#include "nsIDeviceContext.h"
#include "nsIScrollableView.h"

// Interface IDs
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);

class RootFrame : public nsHTMLContainerFrame {
public:
  RootFrame(nsIContent* aContent);

  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);
  NS_IMETHOD IsPercentageBase(PRBool& aBase) const {
    aBase = PR_TRUE;
    return NS_OK;
  }

protected:
  virtual PRIntn GetSkipSides() const;
};

// Pseudo frame created by the root frame
class RootContentFrame : public nsContainerFrame {
public:
  RootContentFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD DidReflow(nsIPresContext& aPresContext,
                       nsDidReflowStatus aStatus);
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);
  NS_IMETHOD IsPercentageBase(PRBool& aBase) const {
    aBase = PR_TRUE;
    return NS_OK;
  }

  void ComputeChildMargins(nsMargin& aMargin);
};

//----------------------------------------------------------------------

nsresult
NS_NewHTMLFrame(nsIContent* aContent, nsIFrame* aParentFrame,
                nsIFrame*& aResult)
{
  RootFrame* frame = new RootFrame(aContent);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

RootFrame::RootFrame(nsIContent* aContent)
  : nsHTMLContainerFrame(aContent, nsnull)
{
}

NS_IMETHODIMP
RootFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
#if 0
  // Construct the root content frame and set its style context
  mFirstChild = new RootContentFrame(mContent, this);
  nsIStyleContext* pseudoStyleContext =
    aPresContext.ResolvePseudoStyleContextFor(mContent, 
                                              nsHTMLAtoms::rootContentPseudo, 
                                              mStyleContext);
  mFirstChild->SetStyleContext(&aPresContext, pseudoStyleContext);
  NS_RELEASE(pseudoStyleContext);

  // Set the geometric and content parent for each of the child frames
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(frame)) {
    frame->SetGeometricParent(mFirstChild);
    frame->SetContentParent(mFirstChild);
  }

  // Queue up the frames for the root content frame
  return mFirstChild->Init(aPresContext, aChildList);
#else
  mFirstChild = aChildList;
  return NS_OK;
#endif
}

NS_IMETHODIMP
RootFrame::Reflow(nsIPresContext&          aPresContext,
                  nsHTMLReflowMetrics&     aDesiredSize,
                  const nsHTMLReflowState& aReflowState,
                  nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("RootFrame::Reflow");

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
  }

  // Reflow our pseudo frame. It will choose whetever height its child frame
  // wants
  if (nsnull != mFirstChild) {
    // Compute how much space to reserve for our border and padding
    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    nsMargin borderPadding;
    spacing->CalcBorderPaddingFor(this, borderPadding);

    nsSize  kidMaxSize(aReflowState.maxSize);
    kidMaxSize.width -= borderPadding.left + borderPadding.right;
    kidMaxSize.height -= borderPadding.top + borderPadding.bottom;
    nsHTMLReflowMetrics desiredSize(nsnull);
    nsHTMLReflowState kidReflowState(aPresContext, mFirstChild, aReflowState,
                                     kidMaxSize);
    // XXX HACK
    kidReflowState.widthConstraint = eHTMLFrameConstraint_Fixed;
    kidReflowState.minWidth = kidMaxSize.width;
    kidReflowState.heightConstraint = eHTMLFrameConstraint_Fixed;
    kidReflowState.minHeight = kidMaxSize.height;
    nsIHTMLReflow* htmlReflow;

    if (NS_OK == mFirstChild->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      ReflowChild(mFirstChild, aPresContext, desiredSize, kidReflowState, aStatus);
    
      // Place and size the child
      nsRect  rect(borderPadding.left, borderPadding.top, desiredSize.width, desiredSize.height);
      mFirstChild->SetRect(rect);

      // XXX We should resolve the details of who/when DidReflow() notifications
      // are sent...
      htmlReflow->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
    }

    // If this is a resize reflow and we have non-zero border or padding
    // then do a repaint to make sure we repaint correctly.
    // XXX We could be smarter about only damaging the border/padding area
    // that was affected by the resize...
    if ((eReflowReason_Resize == aReflowState.reason) &&
        ((borderPadding.left != 0) || (borderPadding.top != 0) ||
         (borderPadding.right != 0) || (borderPadding.bottom) != 0)) {
      nsRect  damageRect(0, 0, aReflowState.maxSize.width, aReflowState.maxSize.height);
      Invalidate(damageRect, PR_FALSE);
    }
  }

  // Return the max size as our desired size
  aDesiredSize.width = aReflowState.maxSize.width;
  aDesiredSize.height = aReflowState.maxSize.height;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  NS_FRAME_TRACE_REFLOW_OUT("RootFrame::Reflow", aStatus);
  return NS_OK;
}

PRIntn
RootFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
RootFrame::HandleEvent(nsIPresContext& aPresContext, 
                       nsGUIEvent* aEvent,
                       nsEventStatus& aEventStatus)
{
  mContent->HandleDOMEvent(aPresContext, (nsEvent*)aEvent, nsnull, DOM_EVENT_INIT, aEventStatus);

  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP ||
      aEvent->message == NS_MOUSE_MIDDLE_BUTTON_UP ||
      aEvent->message == NS_MOUSE_RIGHT_BUTTON_UP) {
    nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

#if 0
  if (aEventStatus != nsEventStatus_eConsumeNoDefault) {
    switch (aEvent->message) {
    case NS_MOUSE_MOVE:
    case NS_MOUSE_ENTER:
      {
        nsIFrame* target = this;
        PRInt32 cursor;
       
        GetCursorAndContentAt(aPresContext, aEvent->point, &target, &mContent, cursor);
        if (cursor == NS_STYLE_CURSOR_INHERIT) {
          cursor = NS_STYLE_CURSOR_DEFAULT;
        }
        nsCursor c;
        switch (cursor) {
        default:
        case NS_STYLE_CURSOR_DEFAULT:
          c = eCursor_standard;
          break;
        case NS_STYLE_CURSOR_POINTER:
          c = eCursor_hyperlink;
          break;
        case NS_STYLE_CURSOR_TEXT:
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
#endif
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

  nsresult result = nsRepository::CreateInstance(kViewCID, 
                                                 nsnull, 
                                                 kIViewIID, 
                                                 (void **)&view);
  if (NS_OK == result) {
    nsIView*        rootView = parView;
    nsIViewManager* viewManager;
    rootView->GetViewManager(viewManager);

    // Initialize the view
    NS_ASSERTION(nsnull != viewManager, "null view manager");

    view->Init(viewManager, mRect, rootView);
    //we don't insert it here, instead SetScrolledView() does it...
    NS_RELEASE(viewManager);

    // We expect the root view to be a scrolling view
    nsIScrollableView* scrollView;
    if (NS_SUCCEEDED(rootView->QueryInterface(kScrollViewIID, (void**)&scrollView))) {
      scrollView->SetScrolledView(view);
    }

    // Remember our view
    SetView(view);

    // Don't allow our view's position to be changed. It's controlled by the
    // scrollview
    mState &= ~NS_FRAME_SYNC_FRAME_AND_VIEW;
  }
}

NS_IMETHODIMP
RootContentFrame::DidReflow(nsIPresContext& aPresContext,
                            nsDidReflowStatus aStatus)
{
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    // Size the view. Don't position it...
    nsIView*  view;
    nsIViewManager  *vm;

    GetView(view);
    view->GetViewManager(vm);
    vm->ResizeView(view, mRect.width, mRect.height);
    NS_RELEASE(vm);
  }
  return nsContainerFrame::DidReflow(aPresContext, aStatus);
}

// Determine the margins to place around the child frame. Note that
// this applies to the frame in the page-frame when paginating, not
// to the page-frame.
void
RootContentFrame::ComputeChildMargins(nsMargin& aMargin)
{
  const nsStyleSpacing* spacing = nsnull;
  mFirstChild->GetStyleData(eStyleStruct_Spacing,
                            (const nsStyleStruct*&) spacing);
  if (nsnull != spacing) {
    spacing->CalcMarginFor(mFirstChild, aMargin);

    // We cannot allow negative margins
    if (aMargin.top < 0) aMargin.top = 0;
    if (aMargin.right < 0) aMargin.right = 0;
    if (aMargin.bottom < 0) aMargin.bottom = 0;
    if (aMargin.left < 0) aMargin.left = 0;
  }
  else {
    aMargin.left = 0;
    aMargin.right = 0;
    aMargin.top = 0;
    aMargin.bottom = 0;
  }
}

// XXX Hack
#define PAGE_SPACING_TWIPS 100

NS_IMETHODIMP
RootContentFrame::Reflow(nsIPresContext&          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("RootContentFrame::Reflow");

  aStatus = NS_FRAME_COMPLETE;

  // XXX Need a copy of this margin and border handling code in nsPageFrame

  // Calculate margin around our child
  nsMargin margin;
  ComputeChildMargins(margin);

  // Calculate our border and padding. Note that we use our parents
  // style context since we have a pseudo-style.
  nsMargin borderPadding(0, 0, 0, 0);
  const nsStyleSpacing* spacing;
  mGeometricParent->GetStyleData(eStyleStruct_Spacing,
                                 (const nsStyleStruct*&) spacing);
  if (nsnull != spacing) {
    spacing->CalcBorderPaddingFor(this, borderPadding);
  }

  // Calculate the inner area available for reflowing the child
  nscoord top = margin.top + borderPadding.top;
  nscoord right = margin.right + borderPadding.right;
  nscoord bottom = margin.bottom + borderPadding.bottom;
  nscoord left = margin.left + borderPadding.left;
  nscoord availWidth = aReflowState.maxSize.width - left - right;

  // If this is not a paginated view then (maybe) subtract out space
  // reserved for the scroll bar. We check our child and see if it
  // wants to be scrolled (overflow: auto/scroll) and if so then we
  // reserve some space for the scrollbar.
  nscoord sbarWidth = 0;
  if (!aPresContext.IsPaginated()) {
    const nsStyleDisplay* display;
    mFirstChild->GetStyleData(eStyleStruct_Display,
                              (const nsStyleStruct*&) display);
    if ((NS_STYLE_OVERFLOW_AUTO == display->mOverflow) ||
        (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow)) {
      nsIDeviceContext* dc = aPresContext.GetDeviceContext();
      float sbWidth, sbHeight;
      dc->GetScrollBarDimensions(sbWidth, sbHeight);
      sbarWidth = NSToCoordRound(sbWidth);
      availWidth -= sbarWidth;
      NS_RELEASE(dc);
    }
  }

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

    nsSize            maxSize(availWidth, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowState kidReflowState(aPresContext, next, aReflowState,
                                     maxSize);
  
    // Dispatch the reflow command to our child frame. Allow it to be as high
    // as it wants
    ReflowChild(mFirstChild, aPresContext, aDesiredSize, kidReflowState, aStatus);
  
    // Place and size the child
    nsRect  rect(left, top, aDesiredSize.width, aDesiredSize.height);
    mFirstChild->SetRect(rect);

    // Compute our desired size
    aDesiredSize.width += left + right + sbarWidth;
    aDesiredSize.height += top + bottom;
    if (aDesiredSize.height < aReflowState.maxSize.height) {
      aDesiredSize.height = aReflowState.maxSize.height;
    }
  } else {
    nsReflowReason  reflowReason = aReflowState.reason;

    // Resize our frames
    if (nsnull != mFirstChild) {
      if (aPresContext.IsPaginated()) {
        nscoord             y = PAGE_SPACING_TWIPS;
        nsHTMLReflowMetrics kidSize(aDesiredSize.maxElementSize);

        // Compute the size of each page and the x coordinate within
        // ourselves that the pages will be placed at.
        nsSize          pageSize(aPresContext.GetPageWidth(),
                                 aPresContext.GetPageHeight());
        nsIDeviceContext *dx = aPresContext.GetDeviceContext();
        float             sbWidth, sbHeight;
        dx->GetScrollBarDimensions(sbWidth, sbHeight);
        PRInt32 extra = aReflowState.maxSize.width - PAGE_SPACING_TWIPS*2 -
          pageSize.width - NSToCoordRound(sbWidth);
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
          nsHTMLReflowState kidReflowState(aPresContext, kidFrame,
                                           aReflowState, pageSize,
                                           reflowReason);
          nsReflowStatus  status;

          // Place and size the page. If the page is narrower than our
          // max width then center it horizontally
          ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, status);
          kidFrame->SetRect(nsRect(x, y, kidSize.width, kidSize.height));
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
            kidFrame->GetStyleContext(kidSC);
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
        nsSize            maxSize(availWidth, NS_UNCONSTRAINEDSIZE);
        nsHTMLReflowState kidReflowState(aPresContext, mFirstChild,
                                         aReflowState, maxSize,
                                         reflowReason);
  
        // Get the child's desired size. Our child's desired height is our
        // desired size
        ReflowChild(mFirstChild, aPresContext, aDesiredSize, kidReflowState, aStatus);
        NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
        // Place and size the child
        nsRect  rect(left, top, aDesiredSize.width, aDesiredSize.height);
        mFirstChild->SetRect(rect);

        // Compute our desired size
        aDesiredSize.width += left + right + sbarWidth;
        aDesiredSize.height += top + bottom;
        if (aDesiredSize.height < aReflowState.maxSize.height) {
          aDesiredSize.height = aReflowState.maxSize.height;
        }

        // Do the necessary repainting
        if (eReflowReason_Initial == reflowReason) {
          // Repaint the visible area
          Invalidate(nsRect(0, 0, aReflowState.maxSize.width, aReflowState.maxSize.height));
        } else if (eReflowReason_Resize == aReflowState.reason) {
          // Repaint the entire frame
          Invalidate(nsRect(0, 0, aReflowState.maxSize.width, aDesiredSize.height));
        }
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

  // We are always a pseudo-frame; make sure our content offset is
  // properly pushed upwards
  nsContainerFrame* parent = (nsContainerFrame*) mGeometricParent;

  NS_FRAME_TRACE_REFLOW_OUT("RootContentFrame::Reflow", aStatus);
  return NS_OK;
}

NS_IMETHODIMP
RootContentFrame::Paint(nsIPresContext&      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect)
{
  // If we're paginated then fill the dirty rect with white
  if (aPresContext.IsPaginated()) {
    // Cross hatching would be nicer...
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.FillRect(aDirtyRect);
  }
  else {
    // Get our parents color from the style sheet; our parent can't
    // paint (special hackery because it's the root frame), but we
    // can. If our parent's color is transparent then use our first
    // child's color instead. This is done so that you override the
    // HTML background using css but still allow for the BODY
    // background/bgcolor attribute to affect the HTML element when
    // css is not involved.
    PRBool renderAll = PR_TRUE;
    const nsStyleColor* color = nsnull;
    mGeometricParent->GetStyleData(eStyleStruct_Color,
                                   (const nsStyleStruct*&) color);
    if (nsnull != color) {
      // If we have no bg color and we have no bg image then...
      if ((NS_STYLE_BG_COLOR_TRANSPARENT|NS_STYLE_BG_IMAGE_NONE) ==
          ((NS_STYLE_BG_COLOR_TRANSPARENT|NS_STYLE_BG_IMAGE_NONE) &
           color->mBackgroundFlags)) {
        // Use the color from our child to render just the margins and
        // padding area.
        mFirstChild->GetStyleData(eStyleStruct_Color,
                                  (const nsStyleStruct*&) color);
        renderAll = PR_FALSE;
      }
    }

    const nsStyleSpacing* spacing = nsnull;
    mGeometricParent->GetStyleData(eStyleStruct_Spacing,
                                   (const nsStyleStruct*&) spacing);
    nsMargin borderPadding(0, 0, 0, 0);
    nsMargin border(0, 0, 0, 0);
    if (nsnull != spacing) {
      // Paint border
      nsRect r(0, 0, mRect.width, mRect.height);
      spacing->CalcBorderFor(this, border);
      spacing->CalcBorderPaddingFor(this, borderPadding);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext,
                                  this, aDirtyRect,
                                  r, *spacing, 0);
    }
    if (nsnull != color) {
      if (renderAll) {
        // Paint the entire background
        nscoord w = mRect.width - border.left - border.right;
        nscoord h = mRect.height - border.top - border.bottom;
        nsRect r(border.left, border.top, w, h);
        if ((w > 0) && (h > 0)) {
          nsCSSRendering::PaintBackground(aPresContext, aRenderingContext,
                                          this, aDirtyRect, r, *color, 0, 0);
        }
      }
      else {
        // Paint our padding area and our childs margin area
        nsMargin padding(0, 0, 0, 0);
        if (nsnull != spacing) {
          spacing->CalcPaddingFor(this, padding);
        }

        nsMargin margin;
        ComputeChildMargins(margin);

        // If this is not a paginated view then (maybe) subtract out space
        // reserved for the scroll bar. We check our child and see if it
        // wants to be scrolled (overflow: auto/scroll) and if so then we
        // reserve some space for the scrollbar.
        nscoord sbarWidth = 0;
        {
          const nsStyleDisplay* display;
          mFirstChild->GetStyleData(eStyleStruct_Display,
                                    (const nsStyleStruct*&) display);
          if ((NS_STYLE_OVERFLOW_AUTO == display->mOverflow) ||
              (NS_STYLE_OVERFLOW_SCROLL == display->mOverflow)) {
            nsIDeviceContext* dc = aPresContext.GetDeviceContext();
            float sbWidth, sbHeight;
            dc->GetScrollBarDimensions(sbWidth, sbHeight);
            sbarWidth = NSToCoordRound(sbWidth);
            NS_RELEASE(dc);
          }
        }

        nsRect childBounds;
        mFirstChild->GetRect(childBounds);

        // Paint the top padding+margin area
        nsRect r(border.left, border.top,
                 mRect.width - border.left - border.right,
                 padding.top + margin.top);
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext,
                                        this, aDirtyRect, r, *color,
                                        childBounds.x, childBounds.y);

        // Paint the left padding+margin area
        r.y = childBounds.y;
        r.width = padding.left + margin.left;
        r.height = childBounds.YMost() - padding.top - margin.top;
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext,
                                        this, aDirtyRect, r, *color,
                                        childBounds.x, childBounds.y);

        // Paint the right padding+margin area
        r.x = mRect.width - border.right - padding.right - margin.right -
          sbarWidth;
        r.width = padding.right + margin.right + sbarWidth;
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext,
                                        this, aDirtyRect, r, *color,
                                        childBounds.x, childBounds.y);

        // Paint the bottom padding+margin+extra area
        r.x = border.left;
        r.y = childBounds.YMost();
        if (r.y < mRect.height) {
          r.width = mRect.width - border.left - border.right;
          r.height = mRect.height - r.y;
          nsCSSRendering::PaintBackground(aPresContext, aRenderingContext,
                                          this, aDirtyRect, r, *color,
                                          childBounds.x, childBounds.y);
        }
      }
    }
  }

  // Now paint our children
  return nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
}

NS_IMETHODIMP
RootContentFrame::HandleEvent(nsIPresContext& aPresContext, 
                              nsGUIEvent* aEvent,
                              nsEventStatus& aEventStatus)
{
#if 0
  mContent->HandleDOMEvent(aPresContext, (nsEvent*)aEvent, nsnull, DOM_EVENT_INIT, aEventStatus);
#else
  nsContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
#endif
  
  if (aEventStatus != nsEventStatus_eConsumeNoDefault) {
    switch (aEvent->message) {
    case NS_MOUSE_MOVE:
    case NS_MOUSE_ENTER:
      {
        nsIFrame* target = this;
        nsIContent* mTargetContent = mContent;
        PRInt32 cursor;
       
        GetCursorAndContentAt(aPresContext, aEvent->point, &target, &mTargetContent, cursor);
        if (cursor == NS_STYLE_CURSOR_INHERIT) {
          cursor = NS_STYLE_CURSOR_DEFAULT;
        }
        nsCursor c;
        switch (cursor) {
        default:
        case NS_STYLE_CURSOR_DEFAULT:
          c = eCursor_standard;
          break;
        case NS_STYLE_CURSOR_POINTER:
          c = eCursor_hyperlink;
          break;
        case NS_STYLE_CURSOR_TEXT:
          c = eCursor_select;
          break;
        }
        nsIWidget* window;
        target->GetWindow(window);
        window->SetCursor(c);
        NS_RELEASE(window);

        //If the content object under the cursor has changed, fire a mouseover/out
        nsIEventStateManager *mStateManager;
        nsIContent *mLastContent;
        if (NS_OK == aPresContext.GetEventStateManager(&mStateManager)) {
          mStateManager->GetLastMouseOverContent(&mLastContent);
          if (mLastContent != mTargetContent) {
            if (nsnull != mLastContent) {
              //fire mouseout
              nsEventStatus mStatus = nsEventStatus_eIgnore;
              nsMouseEvent mEvent;
              mEvent.eventStructType = NS_MOUSE_EVENT;
              mEvent.message = NS_MOUSE_EXIT;
              mLastContent->HandleDOMEvent(aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus); 
            }
            //fire mouseover
            nsEventStatus mStatus = nsEventStatus_eIgnore;
            nsMouseEvent mEvent;
            mEvent.eventStructType = NS_MOUSE_EVENT;
            mEvent.message = NS_MOUSE_ENTER;
            mTargetContent->HandleDOMEvent(aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus);
            mStateManager->SetLastMouseOverContent(mTargetContent);
          }
          NS_RELEASE(mStateManager);
          NS_IF_RELEASE(mLastContent);
        }
      }
      break;
    case NS_MOUSE_EXIT:
      //Don't know if this is actually hooked up.
      {
        //Fire of mouseout to the last content object.
        nsIEventStateManager *mStateManager;
        nsIContent *mLastContent;
        if (NS_OK == aPresContext.GetEventStateManager(&mStateManager)) {
          mStateManager->GetLastMouseOverContent(&mLastContent);
          if (nsnull != mLastContent) {
            //fire mouseout
            nsEventStatus mStatus = nsEventStatus_eIgnore;
            nsMouseEvent mEvent;
            mEvent.eventStructType = NS_MOUSE_EVENT;
            mEvent.message = NS_MOUSE_EXIT;
            mLastContent->HandleDOMEvent(aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus);
            mStateManager->SetLastMouseOverContent(nsnull);

            NS_RELEASE(mLastContent);
          }
          NS_RELEASE(mStateManager);
        }
      }
      break;
    case NS_MOUSE_LEFT_BUTTON_UP:
      {
        nsIEventStateManager *mStateManager;
        if (NS_OK == aPresContext.GetEventStateManager(&mStateManager)) {
          mStateManager->SetActiveLink(nsnull);
          NS_RELEASE(mStateManager);
        }
      }
      break;
    }
  }

  return NS_OK;
}
