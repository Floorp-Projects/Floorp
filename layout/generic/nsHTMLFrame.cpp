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

  NS_IMETHOD ListTag(FILE* out) const;

protected:
  virtual PRIntn GetSkipSides() const;
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
        // Insert the frame into the child list
        aReflowState.reflowCommand->GetChildFrame(childFrame);
        if (nsnull == mFirstChild) {
          mFirstChild = childFrame;
        } else {
          nsIFrame* lastChild = LastFrame(mFirstChild);
          lastChild->SetNextSibling(childFrame);
        }

      } else if (nsIReflowCommand::FrameRemoved == reflowType) {
        nsIFrame* deletedFrame;

        // Get the child frame we should delete
        aReflowState.reflowCommand->GetChildFrame(deletedFrame);

        // Remove it from the child list
        if (deletedFrame == mFirstChild) {
          deletedFrame->GetNextSibling(mFirstChild);
        } else {
          nsIFrame* prevSibling = nsnull;
          for (nsIFrame* f = mFirstChild; nsnull != f; f->GetNextSibling(f)) {
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
      }

    } else {
      nsIFrame* nextFrame;
      // Get the next frame in the reflow chain
      aReflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame == mFirstChild, "unexpected next reflow command frame");
    }
  }

  // Reflow our child frame
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
  if (nsnull != mContent) {
    mContent->HandleDOMEvent(aPresContext, (nsEvent*)aEvent, nsnull, DOM_EVENT_INIT, aEventStatus);
  }

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

// Output the frame's tag
NS_IMETHODIMP
RootFrame::ListTag(FILE* out) const
{
  if (nsnull == mContent) {
    fprintf(out, "*Root(-1)@%p", this);
  } 
  else {
    return nsFrame::ListTag(out);
  }
  return NS_OK;
}


