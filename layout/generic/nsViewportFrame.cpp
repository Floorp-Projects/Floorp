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
#include "nsContainerFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLIIDs.h"
#include "nsLayoutAtoms.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDeviceContext.h"
#include "nsIPresContext.h"
#include "nsIReflowCommand.h"
#include "nsIPresShell.h"

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);

/**
 * Viewport frame class.
 *
 * The viewport frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be one of the
 * following:
 * - scroll frame
 * - root frame
 *
 * The viewport frame has an additional named child list:
 * - "Fixed-list" which contains the fixed positioned frames
 *
 * @see nsLayoutAtoms::fixedList
 */
class ViewportFrame : public nsContainerFrame {
public:
  NS_IMETHOD Destroy(nsIPresContext& aPresContext);

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD AppendFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;

  NS_IMETHOD FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const;

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  
  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::viewportFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;

protected:
  nsresult IncrementalReflow(nsIPresContext&          aPresContext,
                             const nsHTMLReflowState& aReflowState);

  nsresult ReflowFixedFrame(nsIPresContext&          aPresContext,
                            const nsHTMLReflowState& aReflowState,
                            nsIFrame*                aKidFrame,
                            PRBool                   aInitialReflow,
                            nsReflowStatus&          aStatus) const;

  void ReflowFixedFrames(nsIPresContext&          aPresContext,
                         const nsHTMLReflowState& aReflowState) const;

  void CalculateFixedContainingBlockSize(nsIPresContext&          aPresContext,
                                         const nsHTMLReflowState& aReflowState,
                                         nscoord&                 aWidth,
                                         nscoord&                 aHeight) const;

private:
  nsFrameList mFixedFrames;  // additional named child list
};

//----------------------------------------------------------------------

nsresult
NS_NewViewportFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  ViewportFrame* it = new ViewportFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::Destroy(nsIPresContext& aPresContext)
{
  mFixedFrames.DestroyFrames(aPresContext);
  return nsContainerFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
ViewportFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv;

  // See which child list to add the frames to
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  if (nsLayoutAtoms::fixedList == aListName) {
    mFixedFrames.SetFrames(aChildList);
    rv = NS_OK;

  } else {
    rv = nsContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::AppendFrames(nsIPresContext& aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  nsresult  rv;
  
  NS_PRECONDITION(nsLayoutAtoms::fixedList == aListName, "unexpected child list");
  if (aListName != nsLayoutAtoms::fixedList) {
    // We only expect incremental changes for our fixed frames
    rv = NS_ERROR_INVALID_ARG;

  } else {
    // Add the frames to our list of fixed position frames
  #ifdef NS_DEBUG
    nsFrame::VerifyDirtyBitSet(aFrameList);
  #endif
    mFixedFrames.AppendFrames(nsnull, aFrameList);
    
    // Generate a reflow command to reflow the dirty frames
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this, nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      reflowCmd->SetChildListName(nsLayoutAtoms::fixedList);
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList)
{
  nsresult  rv;

  NS_PRECONDITION(nsLayoutAtoms::fixedList == aListName, "unexpected child list");
  if (aListName != nsLayoutAtoms::fixedList) {
    // We only expect incremental changes for our fixed frames
    rv = NS_ERROR_INVALID_ARG;

  } else {
    // Insert the new frames
#ifdef NS_DEBUG
    nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
    mFixedFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  
    // Generate a reflow command to reflow the dirty frames
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this, nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      reflowCmd->SetChildListName(nsLayoutAtoms::fixedList);
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame)
{
  nsresult  rv;

  NS_PRECONDITION(nsLayoutAtoms::fixedList == aListName, "unexpected child list");
  if (aListName != nsLayoutAtoms::fixedList) {
    // We only expect incremental changes for our fixed frames
    rv = NS_ERROR_INVALID_ARG;

  } else {
    PRBool result = mFixedFrames.DestroyFrame(aPresContext, aOldFrame);
    NS_ASSERTION(result, "didn't find frame to delete");
    // Because fixed frames aren't part of a flow, there's no additional
    // work to do, e.g. reflowing sibling frames. And because fixed frames
    // have a view, we don't need to repaint
    rv = result ? NS_OK : NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                          nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  *aListName = nsnull;

  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;

  } else if (0 == aIndex) {
    *aListName = nsLayoutAtoms::fixedList;
    NS_ADDREF(*aListName);
  }

  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  if (aListName == nsLayoutAtoms::fixedList) {
    *aFirstChild = mFixedFrames.FirstChild();
    return NS_OK;
  }

  return nsContainerFrame::FirstChild(aListName, aFirstChild);
}

void
ViewportFrame::CalculateFixedContainingBlockSize(nsIPresContext&          aPresContext,
                                                 const nsHTMLReflowState& aReflowState,
                                                 nscoord&                 aWidth,
                                                 nscoord&                 aHeight) const
{
  // Initialize the values to the computed width/height in our reflow
  // state struct
  aWidth = aReflowState.mComputedWidth;
  aHeight = aReflowState.mComputedHeight;

  // Get our prinicpal child frame and see if we're scrollable
  nsIFrame* kidFrame = mFrames.FirstChild();
  nsIView*  kidView;

  kidFrame->GetView(&kidView);
  if (nsnull != kidView) {
    nsIScrollableView* scrollingView;
    
    if (NS_SUCCEEDED(kidView->QueryInterface(kScrollViewIID, (void**)&scrollingView))) {
      // Get the scrollbar dimensions
      float             sbWidth, sbHeight;
      nsCOMPtr<nsIDeviceContext> dc;
      aPresContext.GetDeviceContext(getter_AddRefs(dc));

      dc->GetScrollBarDimensions(sbWidth, sbHeight);
      
      // See if the scrollbars are visible
      PRBool  vertSBVisible, horzSBVisible;
      
      scrollingView->GetScrollbarVisibility(&vertSBVisible, &horzSBVisible);
      if (vertSBVisible) {
        aWidth -= NSToCoordRound(sbWidth);
      }
      if (horzSBVisible) {
        aHeight -= NSToCoordRound(sbHeight);
      }
    }
  }
}

nsresult
ViewportFrame::ReflowFixedFrame(nsIPresContext&          aPresContext,
                                const nsHTMLReflowState& aReflowState,
                                nsIFrame*                aKidFrame,
                                PRBool                   aInitialReflow,
                                nsReflowStatus&          aStatus) const
{
  // Reflow the frame
  nsIHTMLReflow* htmlReflow;
  nsresult       rv;

  rv = aKidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  if (NS_SUCCEEDED(rv)) {
    htmlReflow->WillReflow(aPresContext);
    
    nsHTMLReflowMetrics kidDesiredSize(nsnull);
    nsSize              availSize(aReflowState.availableWidth, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowState   kidReflowState(aPresContext, aReflowState, aKidFrame,
                                       availSize);
    
    // If it's the initial reflow, then override the reflow reason. This is
    // used when frames are inserted incrementally
    if (aInitialReflow) {
      kidReflowState.reason = eReflowReason_Initial;
    }
    
    htmlReflow->Reflow(aPresContext, kidDesiredSize, kidReflowState, aStatus);

    // XXX If the child had a fixed height, then make sure it respected it...
    if (NS_AUTOHEIGHT != kidReflowState.mComputedHeight) {
      if (kidDesiredSize.height < kidReflowState.mComputedHeight) {
        kidDesiredSize.height = kidReflowState.mComputedHeight;
        kidDesiredSize.height += kidReflowState.mComputedBorderPadding.top +
                                 kidReflowState.mComputedBorderPadding.bottom;
      }
    }

    // Position the child
    nsRect  rect(kidReflowState.mComputedOffsets.left + kidReflowState.mComputedMargin.left,
                 kidReflowState.mComputedOffsets.top + kidReflowState.mComputedMargin.top,
                 kidDesiredSize.width, kidDesiredSize.height);
    aKidFrame->SetRect(rect);
    htmlReflow->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
  }

  return rv;
}

// Called by Reflow() to reflow all of the fixed positioned child frames.
// This is only done for 'initial', 'resize', and 'style change' reflow commands
void
ViewportFrame::ReflowFixedFrames(nsIPresContext&          aPresContext,
                                 const nsHTMLReflowState& aReflowState) const
{
  NS_PRECONDITION(eReflowReason_Incremental != aReflowState.reason,
                  "unexpected reflow reason");

  // Calculate how much room is available for the fixed items. That means
  // determining if the viewport is scrollable and whether the vertical and/or
  // horizontal scrollbars are visible
  nscoord width, height;
  CalculateFixedContainingBlockSize(aPresContext, aReflowState, width, height);

  // Make a copy of the reflow state and change the computed width and height
  // to reflect the available space for the fixed items
  // XXX Find a cleaner way to do this...
  nsHTMLReflowState reflowState(aReflowState);
  reflowState.mComputedWidth = width;
  reflowState.mComputedHeight = height;

  nsIFrame* kidFrame;
  for (kidFrame = mFixedFrames.FirstChild(); nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
    // Reflow the frame using our reflow reason
    nsReflowStatus  kidStatus;
    ReflowFixedFrame(aPresContext, reflowState, kidFrame, PR_FALSE, kidStatus);
  }
}

/**
 * Called by Reflow() to handle the case where it's an incremental reflow
 * of a fixed child frame
 */
nsresult
ViewportFrame::IncrementalReflow(nsIPresContext&          aPresContext,
                                 const nsHTMLReflowState& aReflowState)
{
  nsIReflowCommand::ReflowType  type;

  // Get the type of reflow command
  aReflowState.reflowCommand->GetType(type);

  // The only type of reflow command we expect is that we have dirty
  // child frames to reflow
  NS_ASSERTION(nsIReflowCommand::ReflowDirty, "unexpected reflow type");
  
  // Calculate how much room is available for the fixed items. That means
  // determining if the viewport is scrollable and whether the vertical and/or
  // horizontal scrollbars are visible
  nscoord width, height;
  CalculateFixedContainingBlockSize(aPresContext, aReflowState, width, height);
  
  // Make a copy of the reflow state and change the computed width and height
  // to reflect the available space for the fixed items
  // XXX Find a cleaner way to do this...
  nsHTMLReflowState reflowState(aReflowState);
  reflowState.mComputedWidth = width;
  reflowState.mComputedHeight = height;
  
  // Walk the fixed child frames and reflow the dirty frames
  for (nsIFrame* f = mFixedFrames.FirstChild(); f; f->GetNextSibling(&f)) {
    nsFrameState  frameState;

    f->GetFrameState(&frameState);
    if (frameState & NS_FRAME_IS_DIRTY) {
      nsReflowStatus  status;

      // Note: the only reason the frame would be dirty would be if it had
      // just been inserted or appended
      NS_ASSERTION(frameState & NS_FRAME_FIRST_REFLOW, "unexpected frame state");
      ReflowFixedFrame(aPresContext, reflowState, f, PR_TRUE, status);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::Reflow(nsIPresContext&          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("ViewportFrame::Reflow");
  NS_PRECONDITION(nsnull == aDesiredSize.maxElementSize, "unexpected request");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  nsIFrame* nextFrame = nsnull;
  PRBool    isHandled = PR_FALSE;
  
  // Check for an incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    // See if we're the target frame
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      nsIAtom*  listName;
      PRBool    isFixedChild;
      
      // It's targeted at us. It better be for the fixed child list
      aReflowState.reflowCommand->GetChildListName(listName);
      isFixedChild = nsLayoutAtoms::fixedList == listName;
      NS_IF_RELEASE(listName);
      NS_ASSERTION(isFixedChild, "unexpected child list");
      
      if (isFixedChild) {
        // Handle the incremental reflow command
        IncrementalReflow(aPresContext, aReflowState);
        isHandled = PR_TRUE;
      }

    } else {
      // Get the next frame in the reflow chain
      aReflowState.reflowCommand->GetNext(nextFrame);
    }
  }

  nsRect kidRect(0,0,aReflowState.availableWidth,aReflowState.availableHeight);

  if (!isHandled) {
    if ((eReflowReason_Incremental == aReflowState.reason) &&
        (mFixedFrames.ContainsFrame(nextFrame))) {
      // Reflow the 'fixed' frame that's the next frame in the reflow path
      nsReflowStatus  kidStatus;
      ReflowFixedFrame(aPresContext, aReflowState, nextFrame, PR_FALSE,
                       kidStatus);

    } else {
      // Reflow our one and only principal child frame
      if (mFrames.NotEmpty()) {
        nsIFrame*           kidFrame = mFrames.FirstChild();
        nsHTMLReflowMetrics kidDesiredSize(nsnull);
        nsSize              availableSpace(aReflowState.availableWidth,
                                           aReflowState.availableHeight);
        nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                           kidFrame, availableSpace);

        // Reflow the frame
        nsIHTMLReflow* htmlReflow;
        if (NS_OK == kidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
          kidReflowState.mComputedHeight = aReflowState.availableHeight;
          ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                      aStatus);

          nsRect  rect(0, 0, kidDesiredSize.width, kidDesiredSize.height);
          kidFrame->SetRect(rect);
          kidRect = rect;

          // XXX We should resolve the details of who/when DidReflow()
          // notifications are sent...
          htmlReflow->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
        }
      }

      // If it's a 'initial', 'resize', or 'style change' reflow command (anything
      // but incremental), then reflow all the fixed positioned child frames
      if (eReflowReason_Incremental != aReflowState.reason) {
        ReflowFixedFrames(aPresContext, aReflowState);
      }
    }
  }

  // If we were flowed initially at both an unconstrained width and height, 
  // this is a hint that we should return our child's intrinsic size.
  if (eReflowReason_Initial == aReflowState.reason &&
      aReflowState.availableWidth == NS_UNCONSTRAINEDSIZE &&
      aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE) {
    aDesiredSize.width = kidRect.width;
    aDesiredSize.height = kidRect.height;
    aDesiredSize.ascent = kidRect.height;
    aDesiredSize.descent = 0;
  }
  else {
    // Return the max size as our desired size
    aDesiredSize.width = aReflowState.availableWidth;
    aDesiredSize.height = aReflowState.availableHeight;
    aDesiredSize.ascent = aReflowState.availableHeight;
    aDesiredSize.descent = 0;
  }

  // If this is a resize reflow or the initial reflow, then do a repaint
  if ((eReflowReason_Initial == aReflowState.reason) ||
      (eReflowReason_Resize == aReflowState.reason)) {
    nsRect  damageRect(0, 0, aDesiredSize.width, aDesiredSize.height);
    Invalidate(damageRect, PR_FALSE);
  }

  NS_FRAME_TRACE_REFLOW_OUT("ViewportFrame::Reflow", aStatus);
  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::viewportFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Viewport", aResult);
}

NS_IMETHODIMP
ViewportFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = sizeof(*this);
  return NS_OK;
}
