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
#include "nsIAreaFrame.h"
#include "nsLayoutAtoms.h"
#include "nsIPresShell.h"

// Interface IDs
static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

/**
 * Root frame class.
 *
 * The root frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be an area
 * frame
 */
class RootFrame : public nsHTMLContainerFrame {
public:
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

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::rootFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
  NS_IMETHOD GetFrameName(nsString& aResult) const;

  // XXX Temporary hack...
  NS_IMETHOD SetRect(const nsRect& aRect);

protected:
  virtual PRIntn GetSkipSides() const;

private:
  nscoord mNaturalHeight;
};

//----------------------------------------------------------------------

nsresult
NS_NewRootFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  RootFrame* it = new RootFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

// XXX Temporary hack until we support the CSS2 'min-width', 'max-width',
// 'min-height', and 'max-height' properties. Then we can do this in a top-down
// fashion
NS_IMETHODIMP
RootFrame::SetRect(const nsRect& aRect)
{
  nsresult  rv = nsHTMLContainerFrame::SetRect(aRect);

  // If our height is larger than our natural height (the height we returned
  // as our desired height), then make sure the document element's frame is
  // increased as well. This happens because the scroll frame will make sure
  // that our height fills the entire scroll area.
  // Note: only do this if the document element's 'height' is 'auto'
  nsIFrame* kidFrame = mFrames.FirstChild();
  if (nsnull != kidFrame) {
    nsStylePosition*  kidPosition;
    kidFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)kidPosition);

    if (eStyleUnit_Auto == kidPosition->mHeight.GetUnit()) {
      nscoord   yDelta = aRect.height - mNaturalHeight;
      nsSize    kidSize;

      kidFrame->GetSize(kidSize);
      kidFrame->SizeTo(kidSize.width, kidSize.height + yDelta);
    }
  }

  return rv;
}

NS_IMETHODIMP
RootFrame::AppendFrames(nsIPresContext& aPresContext,
                        nsIPresShell&   aPresShell,
                        nsIAtom*        aListName,
                        nsIFrame*       aFrameList)
{
  nsresult  rv;

  NS_ASSERTION(!aListName, "unexpected child list name");
  NS_PRECONDITION(mFrames.IsEmpty(), "already have a child frame");
  if (aListName) {
    // We only support unnamed principal child list
    rv = NS_ERROR_INVALID_ARG;

  } else if (!mFrames.IsEmpty()) {
    // We only allow a single child frame
    rv = NS_ERROR_FAILURE;

  } else {
    // Insert the new frames
#ifdef NS_DEBUG
    nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
    mFrames.AppendFrame(nsnull, aFrameList);

    // Generate a reflow command to reflow the newly inserted frame
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this, nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }

  return rv;
}

NS_IMETHODIMP
RootFrame::InsertFrames(nsIPresContext& aPresContext,
                        nsIPresShell&   aPresShell,
                        nsIAtom*        aListName,
                        nsIFrame*       aPrevFrame,
                        nsIFrame*       aFrameList)
{
  nsresult  rv;

  // Because we only support a single child frame inserting is the same
  // as appending
  NS_PRECONDITION(!aPrevFrame, "unexpected previous sibling frame");
  if (aPrevFrame) {
    rv = NS_ERROR_UNEXPECTED;
  } else {
    rv = AppendFrames(aPresContext, aPresShell, aListName, aFrameList);
  }

  return rv;
}

NS_IMETHODIMP
RootFrame::RemoveFrame(nsIPresContext& aPresContext,
                       nsIPresShell&   aPresShell,
                       nsIAtom*        aListName,
                       nsIFrame*       aOldFrame)
{
  nsresult  rv;

  NS_ASSERTION(!aListName, "unexpected child list name");
  if (aListName) {
    // We only support the unnamed principal child list
    rv = NS_ERROR_INVALID_ARG;
  
  } else if (aOldFrame == mFrames.FirstChild()) {
    // It's our one and only child frame
    // Damage the area occupied by the deleted frame
    nsRect  damageRect;
    aOldFrame->GetRect(damageRect);
    Invalidate(damageRect, PR_FALSE);

    // Remove the frame and destroy it
    mFrames.DestroyFrame(aPresContext, aOldFrame);

    // Generate a reflow command so we get reflowed
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this, nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }

  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
RootFrame::Reflow(nsIPresContext&          aPresContext,
                  nsHTMLReflowMetrics&     aDesiredSize,
                  const nsHTMLReflowState& aReflowState,
                  nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("RootFrame::Reflow");
  NS_PRECONDITION(nsnull == aDesiredSize.maxElementSize, "unexpected request");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  PRBool  isStyleChange = PR_FALSE;
  PRBool  isDirtyChildReflow = PR_FALSE;

  // Check for an incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    // See if we're the target frame
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      // Get the reflow type
      nsIReflowCommand::ReflowType  reflowType;
      aReflowState.reflowCommand->GetType(reflowType);

      switch (reflowType) {
      case nsIReflowCommand::ReflowDirty:
        isDirtyChildReflow = PR_TRUE;
        break;

      case nsIReflowCommand::StyleChanged:
        // Remember it's a style change so we can set the reflow reason below
        isStyleChange = PR_TRUE;
        break;

      default:
        NS_ASSERTION(PR_FALSE, "unexpected reflow command type");
      }

    } else {
      nsIFrame* nextFrame;
      // Get the next frame in the reflow chain
      aReflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame == mFrames.FirstChild(), "unexpected next reflow command frame");
    }
  }

  // Reflow our one and only child frame
  nsHTMLReflowMetrics kidDesiredSize(nsnull);
  if (mFrames.IsEmpty()) {
    // We have no child frame, so return an empty size
    aDesiredSize.width = aDesiredSize.height = 0;
    aDesiredSize.ascent = aDesiredSize.descent = 0;

  } else {
    nsIFrame* kidFrame = mFrames.FirstChild();

    // We must specify an unconstrained available height, because constrained
    // is only for when we're paginated...
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                     nsSize(aReflowState.availableWidth,
                                            NS_UNCONSTRAINEDSIZE));
    if (isDirtyChildReflow) {
      // Note: the only reason the frame would be dirty would be if it had
      // just been inserted or appended
      kidReflowState.reason = eReflowReason_Initial;
      kidReflowState.reflowCommand = nsnull;
    } else if (isStyleChange) {
      kidReflowState.reason = eReflowReason_StyleChange;
      kidReflowState.reflowCommand = nsnull;
    }

    // Reflow the frame
    nsIHTMLReflow* htmlReflow;
    if (NS_OK == kidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                  aStatus);

      // The document element's background should cover the entire canvas, so
      // take into account the combined area and any space taken up by
      // absolutely positioned elements
      nsMargin      border;
      nsFrameState  kidState;

      if (!kidReflowState.mStyleSpacing->GetBorder(border)) {
        NS_NOTYETIMPLEMENTED("percentage border");
      }
      kidFrame->GetFrameState(&kidState);

      // First check the combined area
      if (NS_FRAME_OUTSIDE_CHILDREN & kidState) {
        // The background covers the content area and padding area, so check
        // for children sticking outside the child frame's padding edge
        nscoord paddingEdgeX = kidDesiredSize.width - border.right;
        nscoord paddingEdgeY = kidDesiredSize.height - border.bottom;

        if (kidDesiredSize.mCombinedArea.XMost() > paddingEdgeX) {
          kidDesiredSize.width = kidDesiredSize.mCombinedArea.XMost() +
                                 border.right;
        }
        if (kidDesiredSize.mCombinedArea.YMost() > paddingEdgeY) {
          kidDesiredSize.height = kidDesiredSize.mCombinedArea.YMost() +
                                  border.bottom;
        }
      }

      // XXX It would be nice if this were also part of the reflow metrics...
      nsIAreaFrame* areaFrame;
      if (NS_SUCCEEDED(kidFrame->QueryInterface(kAreaFrameIID, (void**)&areaFrame))) {
        // Get the x-most and y-most of the absolutely positioned children
        nscoord positionedXMost, positionedYMost;
        areaFrame->GetPositionedInfo(positionedXMost, positionedYMost);
        
        // The background covers the content area and padding area, so check
        // for children sticking outside the padding edge
        nscoord paddingEdgeX = kidDesiredSize.width - border.right;
        nscoord paddingEdgeY = kidDesiredSize.height - border.bottom;

        if (positionedXMost > paddingEdgeX) {
          kidDesiredSize.width = positionedXMost + border.right;
        }
        if (positionedYMost > paddingEdgeY) {
          kidDesiredSize.height = positionedYMost + border.bottom;
        }
      }

      // If our height is fixed, then make sure the child frame plus its top and
      // bottom margin is at least that high as well...
      if (NS_AUTOHEIGHT != aReflowState.mComputedHeight) {
        nscoord totalHeight = kidDesiredSize.height + kidReflowState.mComputedMargin.top +
          kidReflowState.mComputedMargin.bottom;

        if (totalHeight < aReflowState.mComputedHeight) {
          kidDesiredSize.height += aReflowState.mComputedHeight - totalHeight;
        }
      }

      // Position and size the child frame
      nsRect  rect(kidReflowState.mComputedMargin.left, kidReflowState.mComputedMargin.top,
                   kidDesiredSize.width, kidDesiredSize.height);
      kidFrame->SetRect(rect);

      // If the child frame was just inserted, then we're responsible for making sure
      // it repaints
      if (isDirtyChildReflow) {
        // Damage the area occupied by the deleted frame
        Invalidate(rect, PR_FALSE);
      }
    }

    // Return our desired size
    aDesiredSize.width = kidDesiredSize.width + kidReflowState.mComputedMargin.left +
      kidReflowState.mComputedMargin.right;
    aDesiredSize.height = kidDesiredSize.height + kidReflowState.mComputedMargin.top +
      kidReflowState.mComputedMargin.bottom;
    aDesiredSize.ascent = aDesiredSize.height;
    aDesiredSize.descent = 0;
    // XXX Don't completely ignore NS_FRAME_OUTSIDE_CHILDREN for child frames
    // that stick out on the left or top edges...

    // XXX Temporary hack. Remember this for later when our parent resizes us.
    // See SetRect()
    mNaturalHeight = aDesiredSize.height;
  }

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
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP ||
      aEvent->message == NS_MOUSE_MIDDLE_BUTTON_UP ||
      aEvent->message == NS_MOUSE_RIGHT_BUTTON_UP) {
    nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP
RootFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::rootFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
RootFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Root", aResult);
}
