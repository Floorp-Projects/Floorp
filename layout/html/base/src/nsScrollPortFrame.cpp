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
#include "nsScrollPortFrame.h"
#include "nsLayoutAtoms.h"
#include "nsIBox.h"

/*

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);

static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);


*/

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kScrollPortViewCID, NS_SCROLL_PORT_VIEW_CID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);

static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);

//----------------------------------------------------------------------

nsresult
NS_NewScrollPortFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsScrollPortFrame* it = new nsScrollPortFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsScrollPortFrame::nsScrollPortFrame()
{
    mIncremental = PR_FALSE;
    mNeedsRecalc = PR_TRUE;
}

NS_IMETHODIMP
nsScrollPortFrame::Init(nsIPresContext*  aPresContext,
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
nsScrollPortFrame::SetInitialChildList(nsIPresContext* aPresContext,
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
nsScrollPortFrame::AppendFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScrollPortFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aPrevFrame,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsScrollPortFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame)
{
  // Scroll frame doesn't support incremental changes
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollPortFrame::DidReflow(nsIPresContext*   aPresContext,
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
    if (NS_SUCCEEDED(view->QueryInterface(kScrollViewIID, (void**)&scrollingView))) {
      scrollingView->ComputeScrollOffsets(PR_TRUE);
    }
  }

  return rv;
}

nsresult
nsScrollPortFrame::CreateScrollingViewWidget(nsIView* aView, const nsStylePosition* aPosition)
{
  nsresult rv = NS_OK;
   // If it's fixed positioned, then create a widget 
  if (NS_STYLE_POSITION_FIXED == aPosition->mPosition) {
    rv = aView->CreateWidget(kWidgetCID);
  }

  return(rv);
}

nsresult
nsScrollPortFrame::GetScrollingParentView(nsIPresContext* aPresContext,
                                          nsIFrame* aParent,
                                          nsIView** aParentView)
{
  nsresult rv = aParent->GetView(aPresContext, aParentView);
  NS_ASSERTION(aParentView, "GetParentWithView failed");
  return(rv);
}

nsresult
nsScrollPortFrame::CreateScrollingView(nsIPresContext* aPresContext)
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
  nsresult rv = nsComponentManager::CreateInstance(kScrollPortViewCID, 
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

    scrollingView->SetScrollPreference(nsScrollPreference_kNeverScroll);

    // Have the scrolling view create its internal widgets
    scrollingView->CreateScrollControls(); 

    // Set the scrolling view's insets to whatever our border is
    nsMargin border;
    if (!spacing->GetBorder(border)) {
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


// Calculate the total amount of space needed for the child frame,
// including its child frames that stick outside its bounds and any
// absolutely positioned child frames.
// Updates the width/height members of the reflow metrics
nsresult
nsScrollPortFrame::CalculateChildTotalSize(nsIFrame*            aKidFrame,
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
nsScrollPortFrame::Reflow(nsIPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsScrollPortFrame::Reflow: maxSize=%d,%d",
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

  // Reflow the child and get its desired size. Let it be as high as it
  // wants

  const nsMargin& border = aReflowState.mComputedBorderPadding;

  // we only worry about our border. Out scrolled frame worries about the padding.
  // so lets remove the padding and add it to our computed size.
  nsMargin padding = aReflowState.mComputedPadding;

  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE)
     ((nscoord&)aReflowState.mComputedWidth) += padding.left + padding.right;
  
  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE)
     ((nscoord&)aReflowState.mComputedHeight) += padding.top + padding.bottom;


  ((nsMargin&)aReflowState.mComputedPadding) = nsMargin(0,0,0,0);
  ((nsMargin&)aReflowState.mComputedBorderPadding) -= padding;
 
  nscoord theHeight;
  nsIBox* box;
  nsresult result = kidFrame->QueryInterface(kIBoxIID, (void**)&box);
  if (NS_SUCCEEDED(result))
     theHeight = aReflowState.mComputedHeight;
  else 
     theHeight = NS_INTRINSICSIZE;
     
  nsSize              kidReflowSize(aReflowState.availableWidth, theHeight);
  nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                     kidFrame, kidReflowSize);
  nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.maxElementSize);

  if (mIncremental) {
      kidReflowState.reason = eReflowReason_Incremental; 
      mIncremental = PR_FALSE;
  }

  kidReflowState.mComputedWidth = aReflowState.mComputedWidth;
  kidReflowState.mComputedHeight = theHeight;
 
 
  ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
              border.left, border.top, NS_FRAME_NO_MOVE_VIEW, aStatus);

  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  CalculateChildTotalSize(kidFrame, kidDesiredSize);

  // Place and size the child.
  nscoord x = border.left;
  nscoord y = border.top;

  // Compute our desired size
  if (aReflowState.mComputedWidth == NS_INTRINSICSIZE)
     aDesiredSize.width = kidDesiredSize.width;
  else
     aDesiredSize.width = aReflowState.mComputedWidth;

  if (aDesiredSize.width > kidDesiredSize.width)
     kidDesiredSize.width = aDesiredSize.width;

  aDesiredSize.width += border.left + border.right;
  
  // Compute our desired size
  if (aReflowState.mComputedHeight == NS_INTRINSICSIZE)
     aDesiredSize.height = kidDesiredSize.height;
  else
     aDesiredSize.height = aReflowState.mComputedHeight;

  if (aDesiredSize.height > kidDesiredSize.height)
     kidDesiredSize.height = aDesiredSize.height;

  aDesiredSize.height += border.top + border.bottom;

  FinishReflowChild(kidFrame, aPresContext, kidDesiredSize, x, y, NS_FRAME_NO_MOVE_VIEW);
  //printf("width=%d, height=%d\n", kidDesiredSize.width, kidDesiredSize.height);


  if (nsnull != aDesiredSize.maxElementSize) {
    nscoord maxWidth = aDesiredSize.maxElementSize->width;
    maxWidth += aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;
    nscoord maxHeight = aDesiredSize.maxElementSize->height;
    maxHeight += aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;
    aDesiredSize.maxElementSize->width = maxWidth;
    aDesiredSize.maxElementSize->height = maxHeight;
  }

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  /*
  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE)
     ((nscoord&)aReflowState.mComputedWidth) -= (padding.left + padding.right);

  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE)
     ((nscoord&)aReflowState.mComputedHeight) -= (padding.top + padding.bottom);

  ((nsMargin&)aReflowState.mComputedPadding) = padding;
  ((nsMargin&)aReflowState.mComputedBorderPadding) += padding;
  */

  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
    ("exit nsScrollPortFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}

NS_IMETHODIMP
nsScrollPortFrame::Paint(nsIPresContext*      aPresContext,
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
  nsresult rv = nsContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                                 aWhichLayer);

  return rv;
}

PRIntn
nsScrollPortFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsScrollPortFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::scrollFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsScrollPortFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Scroll", aResult);
}
#endif

/**
 * Goes though each child asking for its size to determine our size. Returns our box size minus our border.
 * This method is defined in nsIBox interface.
 */
NS_IMETHODIMP
nsScrollPortFrame::GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
   nsresult rv;

   aSize.Clear();
 
   if (mNeedsRecalc) {
       nsIFrame* childFrame = mFrames.FirstChild(); 
        // get the size of the child. This is the min, max, preferred, and spring constant
        // it does not include its border.
        rv = GetChildBoxInfo(aPresContext, aReflowState, childFrame, mSpring);
        NS_ASSERTION(rv == NS_OK,"failed to child box info");
        if (NS_FAILED(rv))
         return rv;

        // add in the child's margin and border/padding if there is one.
        const nsStyleSpacing* spacing;
        rv = childFrame->GetStyleData(eStyleStruct_Spacing,
                                      (const nsStyleStruct*&) spacing);

        NS_ASSERTION(rv == NS_OK,"failed to get spacing info");
        if (NS_FAILED(rv))
           return rv;

        nsMargin margin(0,0,0,0);
        spacing->GetMargin(margin);
        nsSize m(margin.left+margin.right,margin.top+margin.bottom);
        mSpring.minSize += m;
        mSpring.prefSize += m;
        if (mSpring.maxSize.width != NS_INTRINSICSIZE)
           mSpring.maxSize.width += m.width;

        if (mSpring.maxSize.height != NS_INTRINSICSIZE)
           mSpring.maxSize.height += m.height;

        spacing->GetBorderPadding(margin);
        nsSize b(margin.left+margin.right,margin.top+margin.bottom);
        mSpring.minSize += b;
        mSpring.prefSize += b;
        if (mSpring.maxSize.width != NS_INTRINSICSIZE)
           mSpring.maxSize.width += b.width;

        if (mSpring.maxSize.height != NS_INTRINSICSIZE)
           mSpring.maxSize.height += b.height;
      

      // ok we don't need to calc this guy again
      mNeedsRecalc = PR_FALSE;
    } 

  aSize = mSpring;

  return rv;
}

nsresult
nsScrollPortFrame::GetChildBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsIFrame* aFrame, nsBoxInfo& aSize)
{
  aSize.Clear();

  // see if the frame has IBox interface

  // if it does ask it for its BoxSize and we are done
  nsIBox* ibox;
  if (NS_SUCCEEDED(aFrame->QueryInterface(nsIBox::GetIID(), (void**)&ibox)) && ibox) {
     ibox->GetBoxInfo(aPresContext, aReflowState, aSize); 
     return NS_OK;
  }   

 // start the preferred size as intrinsic
  aSize.prefSize.width = NS_INTRINSICSIZE;
  aSize.prefSize.height = NS_INTRINSICSIZE;

    nsSize              kidReflowSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
    nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                     aFrame, kidReflowSize);
    nsHTMLReflowMetrics kidDesiredSize(nsnull);

    kidReflowState.reason = eReflowReason_Resize; 

    kidReflowState.mComputedWidth = NS_INTRINSICSIZE;
    kidReflowState.mComputedHeight = NS_INTRINSICSIZE;
 
    nsReflowStatus status = NS_FRAME_COMPLETE;

    ReflowChild(aFrame, aPresContext, kidDesiredSize, kidReflowState,
                0, 0, NS_FRAME_NO_MOVE_FRAME, status);
    aFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);

    NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "bad status");

    const nsStyleSpacing* spacing;
    nsresult rv = aFrame->GetStyleData(eStyleStruct_Spacing,
                   (const nsStyleStruct*&) spacing);

    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get spacing");
    if (NS_FAILED(rv))
        return rv;

    nsMargin border(0,0,0,0);;
    spacing->GetBorderPadding(border);
    

    // remove border
    kidDesiredSize.width -= (border.left + border.right);
    kidDesiredSize.height -= (border.top + border.bottom);
    
    // get the size returned and the it as the preferredsize.
    aSize.prefSize.width = kidDesiredSize.width;
    aSize.prefSize.height = kidDesiredSize.height;
 
  return NS_OK;
}

NS_IMETHODIMP 
nsScrollPortFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIBoxIID)) {                                         
    *aInstancePtr = (void*)(nsIBox*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);                                     
}

NS_IMETHODIMP
nsScrollPortFrame::Dirty(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsIFrame*& incrementalChild)
{
  mIncremental = PR_FALSE;
  incrementalChild = nsnull;

  nsIFrame* frame;
  aReflowState.reflowCommand->GetNext(frame);

  nsIFrame* childFrame = mFrames.FirstChild(); 
    
  nsIBox* ibox;
  if (NS_SUCCEEDED(childFrame->QueryInterface(nsIBox::GetIID(), (void**)&ibox)) && ibox)
      ibox->Dirty(aPresContext, aReflowState, incrementalChild);
  else {
      incrementalChild = frame;
  // if we found a leaf that is not a box. Then mark it as being incremental. So if we are ever
  // flowed incremental then we will flow our child incrementally.
      mIncremental = PR_TRUE;
  }

  return NS_OK;
}

