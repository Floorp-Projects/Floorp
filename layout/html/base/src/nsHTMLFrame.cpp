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
NS_NewHTMLFrame(nsIFrame*& aResult)
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

  aStatus = NS_FRAME_COMPLETE;
  // XXX Copy the reflow state so that we can change the 
  // reason in cases where we're inserting and deleting
  // frames from the root. Troy may want to reconstruct
  // this routine.
  nsHTMLReflowState reflowState(aReflowState);

  if (eReflowReason_Incremental == reflowState.reason) {
    // See if we're the target frame
    nsIFrame* targetFrame;
    reflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      nsIReflowCommand::ReflowType  reflowType;
      nsIFrame*                     childFrame;

      // Get the reflow type
      reflowState.reflowCommand->GetType(reflowType);

      if ((nsIReflowCommand::FrameAppended == reflowType) ||
          (nsIReflowCommand::FrameInserted == reflowType)) {
        // Insert the frame into the child list
        reflowState.reflowCommand->GetChildFrame(childFrame);
        if (nsnull == mFirstChild) {
          mFirstChild = childFrame;
        } else {
          nsIFrame* lastChild = LastFrame(mFirstChild);
          lastChild->SetNextSibling(childFrame);
        }
        // XXX This is wrong. Only the new child should be reflowed 
        // with this reason. The rest should just be resized.
        reflowState.reason = eReflowReason_Initial;
      } else if (nsIReflowCommand::FrameRemoved == reflowType) {
        nsIFrame* deletedFrame;

        // Get the child frame we should delete
        reflowState.reflowCommand->GetChildFrame(deletedFrame);

        // Remove it from the child list
        if (deletedFrame == mFirstChild) {
          deletedFrame->GetNextSibling(mFirstChild);
        } else {
          nsIFrame* prevSibling = nsnull;
          nsIFrame* f;
          for (f = mFirstChild; nsnull != f; f->GetNextSibling(f)) {
            if (f == deletedFrame) {
              break;
            }

            prevSibling = f;
          }

          if (nsnull != f) {
            nsIFrame* nextSibling;

            f->GetNextSibling(nextSibling);
            NS_ASSERTION(nsnull != prevSibling, "null pointer");
            prevSibling->SetNextSibling(nextSibling);
          }
        }
        deletedFrame->SetNextSibling(nsnull);
          
        // Delete the frame
        deletedFrame->DeleteFrame(aPresContext);
        reflowState.reason = eReflowReason_Resize;
      }

    } else {
      nsIFrame* nextFrame;
      // Get the next frame in the reflow chain
      reflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame == mFirstChild, "unexpected next reflow command frame");
    }
  }

  // Reflow our child frame
  if (nsnull != mFirstChild) {
    // Compute how much space to reserve for our border and padding
    nsMargin borderPadding;
    nsHTMLReflowState::ComputeBorderPaddingFor(this, nsnull,
                                               borderPadding);

    // Compute the margins around the child frame
    nsMargin childMargins;
    nsHTMLReflowState::ComputeMarginFor(mFirstChild, &aReflowState,
                                        childMargins);

    nscoord top = borderPadding.top + childMargins.top;
    nscoord bottom = borderPadding.bottom + childMargins.bottom;
    nscoord left = borderPadding.left + childMargins.left;
    nscoord right = borderPadding.right + childMargins.right;

    nsSize  kidMaxSize(reflowState.maxSize);
    kidMaxSize.width -= left + right;
    kidMaxSize.height -= top + bottom;
    nsHTMLReflowMetrics desiredSize(nsnull);
    nsHTMLReflowState kidReflowState(aPresContext, mFirstChild, reflowState,
                                     kidMaxSize);
    // XXX HACK
    kidReflowState.widthConstraint = eHTMLFrameConstraint_Fixed;
    kidReflowState.minWidth = kidMaxSize.width;
    kidReflowState.heightConstraint = eHTMLFrameConstraint_Fixed;
    kidReflowState.minHeight = kidMaxSize.height;
    nsIHTMLReflow* htmlReflow;

    if (NS_OK == mFirstChild->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      ReflowChild(mFirstChild, aPresContext, desiredSize, kidReflowState, aStatus);
    
      // Place and size the child. Because we told the child it was
      // fixed make sure it's at least as big as we told it. This
      // handles the case where the child ignores the reflow state
      // constraints
      if (desiredSize.width < kidMaxSize.width) {
        desiredSize.width = kidMaxSize.width;
      }
      if (desiredSize.height < kidMaxSize.height) {
        desiredSize.height = kidMaxSize.height;
      }
      nsRect  rect(left, top, desiredSize.width, desiredSize.height);
      mFirstChild->SetRect(rect);

      // XXX We should resolve the details of who/when DidReflow()
      // notifications are sent...
      htmlReflow->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
    }

    // If this is a resize reflow and we have non-zero border or padding
    // then do a repaint to make sure we repaint correctly.
    // XXX We could be smarter about only damaging the border/padding area
    // that was affected by the resize...
    if ((eReflowReason_Resize == reflowState.reason) &&
        ((left != 0) || (top != 0) || (right != 0) || (bottom) != 0)) {
      nsRect  damageRect(0, 0, reflowState.maxSize.width, reflowState.maxSize.height);
      Invalidate(damageRect, PR_FALSE);
    }
  }

  // Return the max size as our desired size
  aDesiredSize.width = reflowState.maxSize.width;
  aDesiredSize.height = reflowState.maxSize.height;
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
