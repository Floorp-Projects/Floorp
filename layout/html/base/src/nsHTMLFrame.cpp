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

// Interface IDs
static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

/**
 * Root frame class.
 *
 * The root frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be one of the
 * following:
 * - scroll frame
 * - area frame
 * - page sequence frame
 */
class RootFrame : public nsHTMLContainerFrame {
public:
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
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
NS_NewRootFrame(nsIFrame*& aResult)
{
  RootFrame* frame = new RootFrame;
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

NS_IMETHODIMP
RootFrame::SetInitialChildList(nsIPresContext& aPresContext,
                               nsIAtom*        aListName,
                               nsIFrame*       aChildList)
{
  mFrames.SetFrames(aChildList);
  return NS_OK;
}

// XXX Temporary hack until we support the CSS2 'min-width', 'max-width',
// 'min-height', and 'max-height' properties. Then we can do this in a top-down
// fashion
NS_IMETHODIMP
RootFrame::SetRect(const nsRect& aRect)
{
  nsresult  rv = nsHTMLContainerFrame::SetRect(aRect);

  // Make sure our child's frame is adjusted as well
  // Note: only do this if it's 'height' is 'auto'
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
RootFrame::Reflow(nsIPresContext&          aPresContext,
                  nsHTMLReflowMetrics&     aDesiredSize,
                  const nsHTMLReflowState& aReflowState,
                  nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("RootFrame::Reflow");
  NS_PRECONDITION(nsnull == aDesiredSize.maxElementSize, "unexpected request");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  PRBool  isChildInitialReflow = PR_FALSE;

  // Check for an incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    // See if we're the target frame
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      nsIReflowCommand::ReflowType  reflowType;
      nsIFrame*                     childFrame;

      // Get the reflow type
      aReflowState.reflowCommand->GetType(reflowType);

      if ((nsIReflowCommand::FrameAppended == reflowType) ||
          (nsIReflowCommand::FrameInserted == reflowType)) {

        NS_ASSERTION(mFrames.IsEmpty(), "only one child frame allowed");

        // Insert the frame into the child list
        aReflowState.reflowCommand->GetChildFrame(childFrame);
        mFrames.SetFrames(childFrame);

        // It's the child frame's initial reflow
        isChildInitialReflow = PR_TRUE;

      } else if (nsIReflowCommand::FrameRemoved == reflowType) {
        nsIFrame* deletedFrame;

        // Get the child frame we should delete
        aReflowState.reflowCommand->GetChildFrame(deletedFrame);
        NS_ASSERTION(deletedFrame == mFrames.FirstChild(), "not a child frame");

        // Remove it from the child list
        if (deletedFrame == mFrames.FirstChild()) {
          // Damage the area occupied by the deleted frame
          nsRect  damageRect;
          deletedFrame->GetRect(damageRect);
          Invalidate(damageRect, PR_FALSE);

          mFrames.DeleteFrame(aPresContext, deletedFrame);
        }
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
    // Return our desired size
    aDesiredSize.width = aDesiredSize.height = 0;
    aDesiredSize.ascent = aDesiredSize.descent = 0;

  } else {
    nsIFrame* kidFrame = mFrames.FirstChild();

    // We must pass in that the available height is unconstrained, because
    // constrained is only for when we're paginated...
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                     nsSize(aReflowState.availableWidth,
                                            NS_UNCONSTRAINEDSIZE));
    if (isChildInitialReflow) {
      kidReflowState.reason = eReflowReason_Initial;
      kidReflowState.reflowCommand = nsnull;
    }

    // Reflow the frame
    nsIHTMLReflow* htmlReflow;
    if (NS_OK == kidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                  aStatus);

      // Get the total size including any space needed for absolute positioned
      // elements
      // XXX It would be nice if this were part of the reflow metrics...
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

      // If our height is fixed, then make sure the child frame plus its top and
      // bottom margin is at least that high as well...
      if (NS_AUTOHEIGHT != aReflowState.computedHeight) {
        nscoord totalHeight = kidDesiredSize.height + kidReflowState.computedMargin.top +
          kidReflowState.computedMargin.bottom;

        if (totalHeight < aReflowState.computedHeight) {
          kidDesiredSize.height += aReflowState.computedHeight - totalHeight;
        }
      }
      nsRect  rect(kidReflowState.computedMargin.left, kidReflowState.computedMargin.top,
                   kidDesiredSize.width, kidDesiredSize.height);
      kidFrame->SetRect(rect);
    }

    // If this is a resize reflow then do a repaint
    if (eReflowReason_Resize == aReflowState.reason) {
      nsRect  damageRect(0, 0, aReflowState.availableWidth, aReflowState.availableHeight);
      Invalidate(damageRect, PR_FALSE);
    }

    // Return our desired size
    aDesiredSize.width = kidDesiredSize.width + kidReflowState.computedMargin.left +
      kidReflowState.computedMargin.right;
    aDesiredSize.height = kidDesiredSize.height + kidReflowState.computedMargin.top +
      kidReflowState.computedMargin.bottom;
    aDesiredSize.ascent = aDesiredSize.height;
    aDesiredSize.descent = 0;

    // Set NS_FRAME_OUTSIDE_CHILDREN flag, or reset it, as appropriate
    nsFrameState  kidState;
    kidFrame->GetFrameState(&kidState);
    if (NS_FRAME_OUTSIDE_CHILDREN & kidState) {
      nscoord kidXMost = kidReflowState.computedMargin.left +
                         kidDesiredSize.mCombinedArea.XMost();
      nscoord kidYMost = kidReflowState.computedMargin.top +
                         kidDesiredSize.mCombinedArea.YMost();

      if ((kidXMost > aDesiredSize.width) || (kidYMost > aDesiredSize.height)) {
        aDesiredSize.mCombinedArea.x = 0;
        aDesiredSize.mCombinedArea.y = 0;
        aDesiredSize.mCombinedArea.width = PR_MAX(aDesiredSize.width, kidXMost);
        aDesiredSize.mCombinedArea.height = PR_MAX(aDesiredSize.height, kidYMost);
        mState |= NS_FRAME_OUTSIDE_CHILDREN;

      } else {
        mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
      }

    } else {
      mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
    }

    // XXX Temporary hack. Remember this for later when our parent resizes us
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
