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

  NS_IMETHOD GetFrameName(nsString& aResult) const;

protected:
  virtual PRIntn GetSkipSides() const;
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
  mFirstChild = aChildList;
  return NS_OK;
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

        NS_ASSERTION(nsnull == mFirstChild, "only one child frame allowed");

        // Insert the frame into the child list
        aReflowState.reflowCommand->GetChildFrame(childFrame);
        mFirstChild = childFrame;

        // It's the child frame's initial reflow
        isChildInitialReflow = PR_TRUE;

      } else if (nsIReflowCommand::FrameRemoved == reflowType) {
        nsIFrame* deletedFrame;

        // Get the child frame we should delete
        aReflowState.reflowCommand->GetChildFrame(deletedFrame);
        NS_ASSERTION(deletedFrame == mFirstChild, "not a child frame");

        // Remove it from the child list
        if (deletedFrame == mFirstChild) {
          mFirstChild = nsnull;

          // Damage the area occupied by the deleted frame
          nsRect  damageRect;
          deletedFrame->GetRect(damageRect);
          Invalidate(damageRect, PR_FALSE);

          // Delete the frame
          deletedFrame->DeleteFrame(aPresContext);
        }
      }

    } else {
      nsIFrame* nextFrame;
      // Get the next frame in the reflow chain
      aReflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame == mFirstChild, "unexpected next reflow command frame");
    }
  }

  // Reflow our one and only child frame
  if (nsnull != mFirstChild) {
    // Note: the root frame does not have border or padding...

    // Compute the margins around the child frame
    nsMargin childMargin;
    nsHTMLReflowState::ComputeMarginFor(mFirstChild, &aReflowState, childMargin);

    // Compute the child frame's available space
    nsSize  kidMaxSize(aReflowState.maxSize.width - childMargin.left - childMargin.right,
                       aReflowState.maxSize.height - childMargin.top - childMargin.bottom);
    nsHTMLReflowMetrics desiredSize(nsnull);
    // We must pass in that the available height is unconstrained, because
    // constrained is only for when we're paginated...
    nsHTMLReflowState kidReflowState(aPresContext, mFirstChild, aReflowState,
                                     nsSize(kidMaxSize.width, NS_UNCONSTRAINEDSIZE));
    if (isChildInitialReflow) {
      kidReflowState.reason = eReflowReason_Initial;
      kidReflowState.reflowCommand = nsnull;
    }

    // For a height that's 'auto', make the frame as big as the available space
    if (!kidReflowState.HaveFixedContentHeight()) {
      kidReflowState.computedHeight = kidMaxSize.height;

      // Computed height is for the content area so reduce it by the amount of
      // space taken up by border and padding
      nsMargin  borderPadding;
      kidReflowState.ComputeBorderPaddingFor(mFirstChild, &aReflowState, borderPadding);
      kidReflowState.computedHeight -= borderPadding.top + borderPadding.bottom;
    }

    // Reflow the frame
    nsIHTMLReflow* htmlReflow;
    if (NS_OK == mFirstChild->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      ReflowChild(mFirstChild, aPresContext, desiredSize, kidReflowState, aStatus);
    
      nsRect  rect(childMargin.left, childMargin.top, desiredSize.width, desiredSize.height);
      mFirstChild->SetRect(rect);

      // XXX We should resolve the details of who/when DidReflow()
      // notifications are sent...
      htmlReflow->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
    }

    // If this is a resize reflow then do a repaint
    if (eReflowReason_Resize == aReflowState.reason) {
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
RootFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Root", aResult);
}
