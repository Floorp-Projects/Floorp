/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsScrollFrame.h"
#include "nsLayoutAtoms.h"
#include "nsIWebShell.h"
#include "nsIBox.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollable.h"
#include "nsIStatefulFrame.h"
#include "nsBoxLayoutState.h"
#include "nsISupportsPrimitives.h"
#include "nsIPresState.h"

#undef NOISY_SECOND_REFLOW


static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);


//----------------------------------------------------------------------

nsresult
NS_NewScrollFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsScrollFrame* it = new (aPresShell) nsScrollFrame;
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
nsScrollFrame::Init(nsIPresContext*  aPresContext,
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

/**
* Set the view that we are scrolling within the scrolling view. 
*/
NS_IMETHODIMP
nsScrollFrame::SetScrolledFrame(nsIPresContext* aPresContext, nsIFrame *aScrolledFrame)
{
   // remove the first child and add in the new one
   nsIFrame* child = mFrames.FirstChild();
   mFrames.DestroyFrame(aPresContext, child);
   mFrames.InsertFrame(nsnull, nsnull, aScrolledFrame);
   return NS_OK;
}

/**
* Get the view that we are scrolling within the scrolling view. 
* @result child view
*/
NS_IMETHODIMP
nsScrollFrame::GetScrolledFrame(nsIPresContext* aPresContext, nsIFrame *&aScrolledFrame) const
{
   // our first and only child is the scrolled child
   nsIFrame* child = mFrames.FirstChild();
   aScrolledFrame = child;
   return NS_OK;  
}

NS_IMETHODIMP
nsScrollFrame::GetClipSize(   nsIPresContext* aPresContext, 
                              nscoord *aWidth, 
                              nscoord *aHeight) const
{
    nsIScrollableView* scrollingView;
    nsIView*           view;
    GetView(aPresContext, &view);
    if (NS_SUCCEEDED(view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView))) {
       const nsIView* clip = nsnull;
       scrollingView->GetClipView(&clip);
       clip->GetDimensions(aWidth, aHeight);
    } else {
       *aWidth = 0;
       *aHeight = 0;
    }

   return NS_OK;
}

/**
 * Query whether scroll bars should be displayed all the time, never or
 * only when necessary.
 * @return current scrollbar selection
 */
NS_IMETHODIMP
nsScrollFrame::GetScrollPreference(nsIPresContext* aPresContext, nsScrollPref* aScrollPreference) const
{
  nsIScrollableView* scrollingView;
  nsIView*           view;
  GetView(aPresContext, &view);
  if (NS_SUCCEEDED(view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView))) {
     nsScrollPreference pref;
     scrollingView->GetScrollPreference(pref);
     switch(pref) 
     {
       case nsScrollPreference_kAuto:
         *aScrollPreference = Auto;
         break;
       case nsScrollPreference_kNeverScroll:
         *aScrollPreference = NeverScroll;
         break;
       case nsScrollPreference_kAlwaysScroll:
         *aScrollPreference = AlwaysScroll;
         break;
       case nsScrollPreference_kAlwaysScrollHorizontal:
         *aScrollPreference = AlwaysScrollHorizontal;
         break;
       case nsScrollPreference_kAlwaysScrollVertical:
         *aScrollPreference = AlwaysScrollVertical;
         break;

     }

     return NS_OK;
  }

  NS_ERROR("No scrollview!!!");
  *aScrollPreference = NeverScroll;
  return NS_OK;
}

/**
* Gets the size of the area that lies inside the scrollbars but clips the scrolled frame
*/
NS_IMETHODIMP
nsScrollFrame::GetScrollbarSizes(nsIPresContext* aPresContext, 
                             nscoord *aVbarWidth, 
                             nscoord *aHbarHeight) const
{
  float             sbWidth, sbHeight;
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext->GetDeviceContext(getter_AddRefs(dc));

  dc->GetScrollBarDimensions(sbWidth, sbHeight);
  *aVbarWidth = NSToCoordRound(sbWidth);
  *aHbarHeight = NSToCoordRound(sbHeight);
  return NS_OK;
}

NS_IMETHODIMP
nsScrollFrame::GetScrollPosition(nsIPresContext* aPresContext, nscoord &aX, nscoord& aY) const
{
    nsIScrollableView* scrollingView;
    nsIView*           view;
    GetView(aPresContext, &view);
    nsresult rv = view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView);
    NS_ASSERTION(NS_SUCCEEDED(rv), "No scrolling view");
    return scrollingView->GetScrollPosition(aX, aY); 
}

NS_IMETHODIMP
nsScrollFrame::ScrollTo(nsIPresContext* aPresContext, nscoord aX, nscoord aY, PRUint32 aFlags)
{
    nsIScrollableView* scrollingView;
    nsIView*           view;
    GetView(aPresContext, &view);

    nsresult rv = view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView);
    NS_ASSERTION(NS_SUCCEEDED(rv), "No scrolling view");
    return scrollingView->ScrollTo(aX, aY, aFlags); 

}


/**
* Get information about whether the vertical and horizontal scrollbars
* are currently visible
*/ 
NS_IMETHODIMP
nsScrollFrame::GetScrollbarVisibility(nsIPresContext* aPresContext, 
                                      PRBool *aVerticalVisible,
                                      PRBool *aHorizontalVisible) const
{
    nsIScrollableView* scrollingView;
    nsIView*           view;
    GetView(aPresContext, &view);
    if (NS_SUCCEEDED(view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView))) {
      scrollingView->GetScrollbarVisibility(aVerticalVisible, aHorizontalVisible);
    } else {
        aVerticalVisible = PR_FALSE;
        aHorizontalVisible = PR_FALSE;
    }

   return NS_OK;
}

NS_IMETHODIMP
nsScrollFrame::SetScrollbarVisibility(nsIPresContext* aPresContext, 
                                      PRBool aVerticalVisible,
                                      PRBool aHorizontalVisible)
{
  NS_ASSERTION(0,"Write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsScrollFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(NS_GET_IID(nsIScrollableFrame))) {                                         
    *aInstancePtr = (void*)(nsIScrollableFrame*) this;                                        
    return NS_OK;                                                        
  }   
  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*)(nsIStatefulFrame*) this;
    return NS_OK;
  }


  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);                                  
}
  
NS_IMETHODIMP
nsScrollFrame::SetInitialChildList(nsIPresContext* aPresContext,
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
  frame->GetView(aPresContext, &scrolledView);
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
nsScrollFrame::AppendFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScrollFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aPrevFrame,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScrollFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame)
{
  // Scroll frame doesn't support incremental changes
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollFrame::DidReflow(nsIPresContext*   aPresContext,
                         nsDidReflowStatus aStatus)
{
  nsresult  rv = NS_OK;

  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    // Let the default nsFrame implementation clear the state flags
    // and size and position our view
    rv = nsFrame::DidReflow(aPresContext, aStatus);
    
    // Have the scrolling view layout
    nsIScrollableView* scrollingView;
    nsIView*           view;
    GetView(aPresContext, &view);
    if (NS_SUCCEEDED(view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView))) {
      scrollingView->ComputeScrollOffsets(PR_TRUE);
    }
  }

  return rv;
}

nsresult
nsScrollFrame::CreateScrollingViewWidget(nsIView* aView, const nsStyleDisplay* aDisplay)
{
  nsresult rv = NS_OK;
   // If it's fixed positioned, then create a widget 
  if (NS_STYLE_POSITION_FIXED == aDisplay->mPosition) {
    rv = aView->CreateWidget(kWidgetCID);
  }

  return(rv);
}

nsresult
nsScrollFrame::GetScrollingParentView(nsIPresContext* aPresContext,
                                      nsIFrame*       aParent,
                                      nsIView**       aParentView)
{
  nsresult rv = aParent->GetView(aPresContext, aParentView);
  NS_ASSERTION(aParentView, "GetParentWithView failed");
  return(rv);
}

nsresult
nsScrollFrame::CreateScrollingView(nsIPresContext* aPresContext)
{
  nsIView*  view;

   //Get parent frame
  nsIFrame* parent;
  GetParentWithView(aPresContext, &parent);
  NS_ASSERTION(parent, "GetParentWithView failed");

  // Get parent view
  nsIView* parentView = nsnull;
  GetScrollingParentView(aPresContext, parent, &parentView);
 
  // Get the view manager
  nsIViewManager* viewManager;
  parentView->GetViewManager(viewManager);

  // Create the scrolling view
  nsresult rv = nsComponentManager::CreateInstance(kScrollingViewCID, 
                                             nsnull, 
                                             NS_GET_IID(nsIView), 
                                             (void **)&view);

  if (NS_OK == rv) {
    const nsStylePosition* position = (const nsStylePosition*)
      mStyleContext->GetStyleData(eStyleStruct_Position);
    const nsStyleBorder*  borderStyle = (const nsStyleBorder*)
      mStyleContext->GetStyleData(eStyleStruct_Border);
    const nsStyleDisplay*  display = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    
    // Get the z-index
    PRInt32 zIndex = 0;

    if (eStyleUnit_Integer == position->mZIndex.GetUnit()) {
      zIndex = position->mZIndex.GetIntValue();
    }

    // Initialize the scrolling view
    view->Init(viewManager, mRect, parentView, 
               vis->IsVisible() ?
                nsViewVisibility_kShow : 
                nsViewVisibility_kHide);

    // Insert the view into the view hierarchy
    viewManager->InsertChild(parentView, view, zIndex);

    // Set the view's opacity
    viewManager->SetViewOpacity(view, vis->mOpacity);

    // Because we only paint the border and we don't paint a background,
    // inform the view manager that we have transparent content
    viewManager->SetViewContentTransparency(view, PR_TRUE);

    // If it's fixed positioned, then create a widget too
    CreateScrollingViewWidget(view, display);

    // Get the nsIScrollableView interface
    nsIScrollableView* scrollingView;
    view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView);

    // Have the scrolling view create its internal widgets
    scrollingView->CreateScrollControls();

    // Set the scrolling view's scroll preference
    nsScrollPreference scrollPref;
    
    switch (display->mOverflow)
    {
       case NS_STYLE_OVERFLOW_SCROLL:
       scrollPref = nsScrollPreference_kAlwaysScroll;
       break;

       case NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL:
       scrollPref = nsScrollPreference_kAlwaysScrollHorizontal;
       break;

       case NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL:
       scrollPref = nsScrollPreference_kAlwaysScrollVertical;
       break;

       default:
       scrollPref = nsScrollPreference_kAuto;
    }

    // If this is a scroll frame for a viewport and its webshell 
    // has its scrolling set, use that value
    // XXX This is a huge hack, and we should not be checking the web shell's
    // scrolling preference...
    // This is needed to preserve iframe/frameset scrolling prefs between doc loads.
    nsIFrame* parentFrame = nsnull;
    GetParent(&parentFrame);
    nsCOMPtr<nsIAtom> frameType;
    parent->GetFrameType(getter_AddRefs(frameType));
    if (nsLayoutAtoms::viewportFrame == frameType.get()) {
      nsCOMPtr<nsISupports> container;
      rv = aPresContext->GetContainer(getter_AddRefs(container));
      if (NS_SUCCEEDED(rv) && container) {
        nsCOMPtr<nsIScrollable> scrollableContainer = do_QueryInterface(container, &rv);
        if (NS_SUCCEEDED(rv) && scrollableContainer) {
          PRInt32 scrolling = -1; // -1 indicates not set

          // XXX We should get prefs for X and Y and deal with these independently!
          scrollableContainer->GetCurrentScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,&scrolling);
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
    if (!borderStyle->GetBorder(border)) {
      NS_NOTYETIMPLEMENTED("percentage border");
      border.SizeTo(0, 0, 0, 0);
    }
    scrollingView->SetControlInsets(border);

    // Remember our view
    SetView(aPresContext, view);
  }

  NS_RELEASE(viewManager);
  return rv;
}

// Calculates the size of the scroll area. This is the area inside of the
// border edge and inside of any vertical and horizontal scrollbar
// Also returns whether space was reserved for the vertical scrollbar.
nsresult
nsScrollFrame::CalculateScrollAreaSize(nsIPresContext*          aPresContext,
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
  if (aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL) {
    // Always show both scrollbars, so subtract for the space taken up by the
    // vertical scrollbar
    if (!unconstrainedWidth) {
      aScrollAreaSize->width -= aSBWidth;
    }
  } else {
    if (NS_UNCONSTRAINEDSIZE == aScrollAreaSize->height) {
      // We'll never need a vertical scroller when we have an
      // unbounded amount of height to reflow into
    }
    else {
      // Predict whether we'll need a vertical scrollbar
      if (eReflowReason_Initial == aReflowState.reason) {
        // If it's the initial reflow, then just assume we'll need the
        // scrollbar
        *aRoomForVerticalScrollbar = PR_TRUE;

      } else {
        // Just assume the current scenario.  Note: an important but
        // subtle point is that for incremental reflow we must give
        // the frame being reflowed the same amount of available
        // width; otherwise, it's not only just an incremental reflow
        // but also
        nsIScrollableView* scrollingView;
        nsIView*           view;
        GetView(aPresContext, &view);
        if (NS_SUCCEEDED(view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView))) {
          PRBool  unused;
          scrollingView->GetScrollbarVisibility(aRoomForVerticalScrollbar, &unused);
        }
      }
    }

    // If we're assuming we need a vertical scrollbar, then leave room
    if (*aRoomForVerticalScrollbar && !unconstrainedWidth) {
      aScrollAreaSize->width -= aSBWidth;
    }
  }

  // If scrollbars are always visible, then subtract for the height of the
  // horizontal scrollbar
  if ((NS_STYLE_OVERFLOW_SCROLL == aReflowState.mStyleDisplay->mOverflow || aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL) &&
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
    aKidReflowMetrics.width = aKidReflowMetrics.mOverflowArea.width;
    aKidReflowMetrics.height = aKidReflowMetrics.mOverflowArea.height;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScrollFrame::Reflow(nsIPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsScrollFrame", aReflowState.reason);
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsScrollFrame::Reflow: maxSize=%d,%d",
                      aReflowState.availableWidth,
                      aReflowState.availableHeight));

  nsIFrame* kidFrame = mFrames.FirstChild();
  nsIFrame* targetFrame;
  nsIFrame* nextFrame;

  nsRect oldKidBounds;
  kidFrame->GetRect(oldKidBounds);

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
  if (!aReflowState.mStyleBorder->GetBorder(border)) {
    NS_NOTYETIMPLEMENTED("percentage border");
    border.SizeTo(0, 0, 0, 0);
  }

  // Get the scrollbar dimensions (width of the vertical scrollbar and the
  // height of the horizontal scrollbar) in twips
  nscoord   sbWidth = 0, sbHeight = 0;
  PRBool    getScrollBarDimensions = PR_TRUE;

  // XXX: Begin Temporary Hack.
  //
  //      The style system needs to be modified so we can
  //      independently hide the horizontal scrollbar, the
  //      vertical scrollbar, or both, but still be able to
  //      scroll the view.
  //
  //      An example of this would be a Gfx textfield, which
  //      has no scrollbars, but must be able to scroll as the
  //      cursor moves through the text it contains.
  //
  //      Right now the only way for us to create a scrolling
  //      frame with no scrollbars is to create one with scrollbars,
  //      then hide them by setting the scroll preference in the
  //      scrolling view to nsScrollPreference_kNeverScroll. Doing
  //      this makes the scroll frame's style info and the scrolling
  //      view's scroll preference out of sync, allowing the view
  //      size calculations below to incorrectly include the scrollbar
  //      dimensions in the size calculation.
  //
  //      This hack gets around the problem, but goes against the
  //      defined flow of control between the scroll frame and it's
  //      view, so this hack should be removed when the style system
  //      adds the support described above.
  //
  //      troy@netscape.com allowed me to check this hack in so that
  //      we can get Gfx textfields working without waiting for the
  //      support from the style system.
  //
  //      For more info contact: kin@netscape.com
  //
  nsIView   *view = 0;

  GetView(aPresContext, &view);

  if (view) {
    nsresult rv = NS_OK;
    nsIScrollableView *scrollableView = 0;
    
    rv = view->QueryInterface(NS_GET_IID(nsIScrollableView), (void **)&scrollableView);

    if (NS_SUCCEEDED(rv) && scrollableView) {
      nsScrollPreference scrollPref = nsScrollPreference_kAuto;
      rv = scrollableView->GetScrollPreference(scrollPref);
      if (NS_SUCCEEDED(rv) && scrollPref == nsScrollPreference_kNeverScroll)
        getScrollBarDimensions = PR_FALSE;
    }
  }
  // XXX: End Temporary Hack.

  if (getScrollBarDimensions) {
    GetScrollbarSizes(aPresContext, &sbWidth, &sbHeight);
  }

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

  nscoord theHeight;
  nsIBox* box;
  nsresult result = kidFrame->QueryInterface(NS_GET_IID(nsIBox), (void**)&box);
  if (NS_SUCCEEDED(result)) {
     nsBoxLayoutState state(aPresContext, aReflowState, aDesiredSize);
     nsSize min;
     box->GetMinSize(state, min);
     if (min.height > scrollAreaSize.height) 
        theHeight = min.height;
     else
        theHeight = scrollAreaSize.height;
  } else 
     theHeight = NS_INTRINSICSIZE;

  nsSize              kidReflowSize(scrollAreaSize.width, theHeight);
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
  kidReflowState.mComputedHeight = theHeight;

  ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
              border.left, border.top, NS_FRAME_NO_MOVE_VIEW, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  CalculateChildTotalSize(kidFrame, kidDesiredSize);
  if (NS_UNCONSTRAINEDSIZE == scrollAreaSize.width) 
    scrollAreaSize.width = kidDesiredSize.width;

  if (NS_UNCONSTRAINEDSIZE == scrollAreaSize.height) 
    scrollAreaSize.height = kidDesiredSize.height;

  // If we're 'auto' scrolling and not shrink-wrapping our height, then see
  // whether we correctly predicted whether a vertical scrollbar is needed
#ifdef NOISY_SECOND_REFLOW
  ListTag(stdout);
  printf(": childTotalSize=%d,%d scrollArea=%d,%d computedHeight=%d\n",
         kidDesiredSize.width, kidDesiredSize.height,
         scrollAreaSize.width, scrollAreaSize.height,
         aReflowState.mComputedHeight);
#endif
  if ((aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL) &&
      (NS_AUTOHEIGHT != aReflowState.mComputedHeight)) {

    PRBool  mustReflow = PR_FALSE;

    if (aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL)
    {
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
  #ifdef NOISY_SECOND_REFLOW
          ListTag(stdout);
          printf(": kid-height=%d < scrollArea-height=%d\n",
                 kidDesiredSize.height, scrollAreaSize.height);
  #endif
        }
      } else {
        if (kidDesiredSize.height > scrollAreaSize.height) {
          // We didn't leave room for the vertical scrollbar, but it turns
          // out we needed it
          kidReflowState.availableWidth -= sbWidth;
          kidReflowState.mComputedWidth -= sbWidth;
          scrollAreaSize.width -= sbWidth;
          mustReflow = PR_TRUE;
  #ifdef NOISY_SECOND_REFLOW
          ListTag(stdout);
          printf(": kid-height=%d > scrollArea-height=%d\n",
                 kidDesiredSize.height, scrollAreaSize.height);
  #endif
        }
      }
    }

    // Go ahead and reflow the child a second time
    if (mustReflow) {
      // If this is an incremental reflow, then we need to make sure
      // that we repaint after resizing the display width.
      // The child frame won't know to do that, because we're telling
      // it it's a resize reflow. Therefore, we need to do it
      if (eReflowReason_Incremental == aReflowState.reason) {
        nsRect  kidRect;

        kidFrame->GetRect(kidRect);
        Invalidate(aPresContext, kidRect, PR_FALSE);
      }

      // Reflow the child frame with a reflow reason of reflow
      kidReflowState.reason = eReflowReason_Resize;
      ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                  border.left, border.top, NS_FRAME_NO_MOVE_VIEW, aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
      CalculateChildTotalSize(kidFrame, kidDesiredSize);
    }
  }

  nsRect newKidBounds;
  kidFrame->GetRect(newKidBounds);

  // may need to update fixed position children of the viewport,
  // if the client area changed size
  if ((oldKidBounds.width != newKidBounds.width
      || oldKidBounds.height != newKidBounds.height)
      && eReflowReason_Incremental == aReflowState.reason) {
    nsIFrame* parentFrame;
    GetParent(&parentFrame);
    if (parentFrame) {
      nsCOMPtr<nsIAtom> parentFrameType;
      parentFrame->GetFrameType(getter_AddRefs(parentFrameType));
      if (parentFrameType.get() == nsLayoutAtoms::viewportFrame) {
        // Usually there are no fixed children, so don't do anything unless there's
        // at least one fixed child
        nsIFrame* child;
        if (NS_SUCCEEDED(parentFrame->FirstChild(aPresContext,
          nsLayoutAtoms::fixedList, &child)) && child) {
          nsCOMPtr<nsIPresShell> presShell;
          aPresContext->GetShell(getter_AddRefs(presShell));

          // force a reflow of the fixed children
          nsFrame::CreateAndPostReflowCommand(presShell, parentFrame,
            nsIReflowCommand::UserDefined, nsnull, nsnull, nsLayoutAtoms::fixedList);
        }
      }
    }
  }
  
  if (aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL)
  {

    // Make sure the height of the scrolled frame fills the entire scroll area,
    // unless we're shrink wrapping
    if (NS_AUTOHEIGHT != aReflowState.mComputedHeight) {
      if (kidDesiredSize.height < scrollAreaSize.height) {
        kidDesiredSize.height = scrollAreaSize.height;

        // If there's an auto horizontal scrollbar and the scrollbar will be
        // visible then subtract for the space taken up by the scrollbar;
        // otherwise, we'll end up with a vertical scrollbar even if we don't
        // need one...
        if ((NS_STYLE_OVERFLOW_SCROLL != aReflowState.mStyleDisplay->mOverflow && aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL) &&
            (kidDesiredSize.width > scrollAreaSize.width)) {
          kidDesiredSize.height -= sbHeight;
        }
      }
    }
  }

  // Make sure the width of the scrolled frame fills the entire scroll area
  if (kidDesiredSize.width < scrollAreaSize.width) {
    kidDesiredSize.width = scrollAreaSize.width;
  }

  // Place and size the child.
  FinishReflowChild(kidFrame, aPresContext, kidDesiredSize, border.left,
                    border.top, NS_FRAME_NO_MOVE_VIEW);

  // Compute our desired size
  aDesiredSize.width = scrollAreaSize.width;

  NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aDesiredSize.width, "Bad desired size!!!");

  aDesiredSize.width += border.left + border.right;

  if ((kidDesiredSize.height > scrollAreaSize.height) ||
      (aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL)) {
    aDesiredSize.width += sbWidth;
  }
  
  // For the height if we're shrink wrapping then use whatever is
  // smaller between the available height and the child's desired
  // size; otherwise, use the scroll area size
  if (NS_AUTOHEIGHT == aReflowState.mComputedHeight) {
    aDesiredSize.height = PR_MIN(aReflowState.availableHeight, kidDesiredSize.height);
  } else {
    aDesiredSize.height = scrollAreaSize.height;
  }
  aDesiredSize.height += border.top + border.bottom;
  // XXX This should really be "if we have a visible horizontal scrollbar"...
  if (NS_STYLE_OVERFLOW_SCROLL == aReflowState.mStyleDisplay->mOverflow || aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL) {
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
nsScrollFrame::Paint(nsIPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer,
                     PRUint32             aFlags)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // Only paint the border and background if we're visible
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
  
    if (vis->IsVisible()) {
      // Paint our border only (no background)
      const nsStyleBorder* border = (const nsStyleBorder*)
        mStyleContext->GetStyleData(eStyleStruct_Border);
      const nsStyleOutline* outline = (const nsStyleOutline*)
        mStyleContext->GetStyleData(eStyleStruct_Outline);


      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *border, mStyleContext, 0);
      nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *border, *outline, mStyleContext, 0);
    }
  }

  // Paint our children
  nsresult rv = nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                                 aWhichLayer);
  if (NS_FAILED(rv)) return rv;
  
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

NS_IMETHODIMP
nsScrollFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}

PRIntn
nsScrollFrame::GetSkipSides() const
{
  return 0;
}

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsScrollFrame::SaveState(nsIPresContext* aPresContext,
                         nsIPresState** aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  nsCOMPtr<nsIPresState> state;
  nsresult res = NS_OK;

  nsIView* view;
  GetView(aPresContext, &view);
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

  PRInt32 x,y;
  nsIScrollableView* scrollingView;
  res = view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView);
  NS_ENSURE_SUCCESS(res, res);
  scrollingView->GetScrollPosition(x,y);

  // Don't save scroll position if we are at (0,0)
  if (x || y) {

    nsIView* child = nsnull;
    scrollingView->GetScrolledView(child);
    NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);

    nsRect childRect(0,0,0,0);
    child->GetBounds(childRect);

    res = NS_NewPresState(getter_AddRefs(state));
    NS_ENSURE_SUCCESS(res, res);

    nsCOMPtr<nsISupportsPRInt32> xoffset;
    nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
      nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(xoffset));
    if (xoffset) {
      res = xoffset->SetData(x);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_ConvertASCIItoUCS2("x-offset"), xoffset);
    }

    nsCOMPtr<nsISupportsPRInt32> yoffset;
    nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
      nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(yoffset));
    if (yoffset) {
      res = yoffset->SetData(y);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_ConvertASCIItoUCS2("y-offset"), yoffset);
    }

    nsCOMPtr<nsISupportsPRInt32> width;
    nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
      nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(width));
    if (width) {
      res = width->SetData(childRect.width);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_ConvertASCIItoUCS2("width"), width);
    }

    nsCOMPtr<nsISupportsPRInt32> height;
    nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID,
      nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(height));
    if (height) {
      res = height->SetData(childRect.height);
      NS_ENSURE_SUCCESS(res, res);
      state->SetStatePropertyAsSupports(NS_ConvertASCIItoUCS2("height"), height);
    }
    *aState = state;
    NS_ADDREF(*aState);
  }
  return res;
}

//-----------------------------------------------------------
NS_IMETHODIMP
nsScrollFrame::RestoreState(nsIPresContext* aPresContext,
                                 nsIPresState* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsCOMPtr<nsISupportsPRInt32> xoffset;
  nsCOMPtr<nsISupportsPRInt32> yoffset;
  nsCOMPtr<nsISupportsPRInt32> width;
  nsCOMPtr<nsISupportsPRInt32> height;
  aState->GetStatePropertyAsSupports(NS_ConvertASCIItoUCS2("x-offset"), getter_AddRefs(xoffset));
  aState->GetStatePropertyAsSupports(NS_ConvertASCIItoUCS2("y-offset"), getter_AddRefs(yoffset));
  aState->GetStatePropertyAsSupports(NS_ConvertASCIItoUCS2("width"), getter_AddRefs(width));
  aState->GetStatePropertyAsSupports(NS_ConvertASCIItoUCS2("height"), getter_AddRefs(height));

  nsresult res = NS_ERROR_NULL_POINTER;
  if (xoffset && yoffset) {
    PRInt32 x,y,w,h;
    res = xoffset->GetData(&x);
    if (NS_SUCCEEDED(res))
      res = yoffset->GetData(&y);
    if (NS_SUCCEEDED(res))
      res = width->GetData(&w);
    if (NS_SUCCEEDED(res))
      res = height->GetData(&h);

    if (NS_SUCCEEDED(res)) {

      nsIScrollableView* scrollingView;
      nsIView*           view;
      GetView(aPresContext, &view);
      if (NS_SUCCEEDED(view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView))) {

        nsIView* child = nsnull;
        nsRect childRect(0,0,0,0);
        if (NS_SUCCEEDED(scrollingView->GetScrolledView(child)) && child) {
          child->GetBounds(childRect);
        }
        x = (int)(((float)childRect.width / w) * x);
        y = (int)(((float)childRect.height / h) * y);

        scrollingView->ScrollTo(x,y,0);
      }
    }
  }

  return res;
}

NS_IMETHODIMP
nsScrollFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::scrollFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsScrollFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Scroll", aResult);
}

NS_IMETHODIMP
nsScrollFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = sizeof(*this);
  return NS_OK;
}
#endif
