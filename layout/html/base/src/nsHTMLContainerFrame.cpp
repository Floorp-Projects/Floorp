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
#include "nsHTMLContainerFrame.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsIWidget.h"
#include "nsILinkHandler.h"
#include "nsHTMLValue.h"
#include "nsGUIEvent.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsIPtr.h"
#include "nsAbsoluteFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsIHTMLContent.h"
#include "nsHTMLParts.h"
#include "nsScrollFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsViewsCID.h"
#include "nsIReflowCommand.h"

NS_DEF_PTR(nsIStyleContext);

nsHTMLContainerFrame::nsHTMLContainerFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsContainerFrame(aContent, aParent)
{
}

nsHTMLContainerFrame::~nsHTMLContainerFrame()
{
}

NS_METHOD nsHTMLContainerFrame::Paint(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect)
{
  // Paint our background and border
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible && mRect.width && mRect.height) {
    PRIntn skipSides = GetSkipSides();
    const nsStyleColor* color =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);

    nsRect  rect(0, 0, mRect.width, mRect.height);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *color);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, *spacing, skipSides);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);

  if (nsIFrame::GetShowFrameBorders()) {
    nsIView* view;
    GetView(view);
    if (nsnull != view) {
      aRenderingContext.SetColor(NS_RGB(0,0,255));
    }
    else {
      aRenderingContext.SetColor(NS_RGB(255,0,0));
    }
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
  return NS_OK;
}

NS_METHOD nsHTMLContainerFrame::HandleEvent(nsIPresContext& aPresContext,
                                            nsGUIEvent* aEvent,
                                            nsEventStatus& aEventStatus)
{
  return nsContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

NS_METHOD nsHTMLContainerFrame::GetCursorAndContentAt(nsIPresContext& aPresContext,
                                            const nsPoint& aPoint,
                                            nsIFrame** aFrame,
                                            nsIContent** aContent,
                                            PRInt32& aCursor)
{
  // Set content here, child will override if found.
  *aContent = mContent;
  
  // Get my cursor
  const nsStyleColor* styleColor = (const nsStyleColor*)
    mStyleContext->GetStyleData(eStyleStruct_Color);
  PRInt32 myCursor = styleColor->mCursor;

  // Get child's cursor, if any
  nsContainerFrame::GetCursorAndContentAt(aPresContext, aPoint, aFrame, aContent, aCursor);
  if (aCursor != NS_STYLE_CURSOR_INHERIT) {
    nsIAtom* tag;
    mContent->GetTag(tag);
    if (nsHTMLAtoms::a == tag) {
      // Anchor tags override their child cursors in some cases.
      if ((NS_STYLE_CURSOR_IBEAM == aCursor) &&
          (NS_STYLE_CURSOR_INHERIT != myCursor)) {
        aCursor = myCursor;
      }
    }
    NS_RELEASE(tag);
    return NS_OK;
  }

  if (NS_STYLE_CURSOR_INHERIT != myCursor) {
    // If this container has a particular cursor, use it, otherwise
    // let the child decide.
    *aFrame = this;
    aCursor = myCursor;
    return NS_OK;
  }

  // No specific cursor for us
  aCursor = NS_STYLE_CURSOR_INHERIT;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainerFrame::ContentDeleted(nsIPresShell*   aShell,
                                     nsIPresContext* aPresContext,
                                     nsIContent*     aContainer,
                                     nsIContent*     aChild,
                                     PRInt32         aIndexInParent)
{
  // Find the frame that precedes the deletion point
  nsHTMLContainerFrame* flow;
  nsIFrame* deadFrame;
  nsIFrame* prevSibling;
  if (aIndexInParent > 0) {
    nsIContent* precedingContent;
    aContainer->ChildAt(aIndexInParent - 1, precedingContent);
    prevSibling = aShell->FindFrameWithContent(precedingContent);
    NS_RELEASE(precedingContent);

    // The frame may have a next-in-flow. Get the last-in-flow
    nsIFrame* nextInFlow;
    do {
      prevSibling->GetNextInFlow(nextInFlow);
      if (nsnull != nextInFlow) {
        prevSibling = nextInFlow;
      }
    } while (nsnull != nextInFlow);

    // Get the dead frame (maybe)
    prevSibling->GetGeometricParent((nsIFrame*&)flow);
    prevSibling->GetNextSibling(deadFrame);
    if (nsnull == deadFrame) {
      // The deadFrame must be prevSibling's parent's next-in-flows
      // first frame. Therefore it doesn't have a prevSibling.
      flow = (nsHTMLContainerFrame*) flow->mNextInFlow;
      if (nsnull != flow) {
        deadFrame = flow->mFirstChild;
      }
      prevSibling = nsnull;
    }
  }
  else {
    prevSibling = nsnull;
    flow = this;
    deadFrame = flow->mFirstChild;
  }
  NS_ASSERTION(nsnull != deadFrame, "yikes! couldn't find frame");
  if (nsnull == deadFrame) {
    return NS_OK;
  }

  // Generate a reflow command
  nsIReflowCommand* cmd;
  nsresult rv = NS_NewHTMLReflowCommand(&cmd, flow,
                                        nsIReflowCommand::FrameDeleted);
  if (NS_OK != rv) {
    return rv;
  }
  aShell->AppendReflowCommand(cmd);
  NS_RELEASE(cmd);

  // Take the frame away; Note that we also have to take away any
  // continuations so we loop here until deadFrame is nsnull.
  while (nsnull != deadFrame) {
    // Remove frame from sibling list
    nsIFrame* nextSib;
    deadFrame->GetNextSibling(nextSib);
    if (nsnull != prevSibling) {
      prevSibling->SetNextSibling(nextSib);
    }
    else {
      flow->mFirstChild = nextSib;
    }

    // Break frame out of its flow and then destroy it
    nsIFrame* nextInFlow;
    deadFrame->GetNextInFlow(nextInFlow);
    deadFrame->BreakFromNextFlow();
    deadFrame->DeleteFrame(*aPresContext);
    deadFrame = nextInFlow;

    if (nsnull != deadFrame) {
      // Get the parent of deadFrame's continuation
      deadFrame->GetGeometricParent((nsIFrame*&) flow);

      // When we move to a next-in-flow then the deadFrame will be the
      // first child of the new parent. Therefore we know that
      // prevSibling will be null.
      prevSibling = nsnull;
    }
  }

  return rv;
}

nsPlaceholderFrame*
nsHTMLContainerFrame::CreatePlaceholderFrame(nsIPresContext& aPresContext,
                                             nsIFrame*       aFloatedFrame)
{
  nsIContent* content;
  aFloatedFrame->GetContent(content);

  nsPlaceholderFrame* placeholder;
  nsPlaceholderFrame::NewFrame((nsIFrame**)&placeholder, content, this, aFloatedFrame);
  NS_IF_RELEASE(content);

  // Let the placeholder share the same style context as the floated element
  nsIStyleContext*  kidSC;
  aFloatedFrame->GetStyleContext(&aPresContext, kidSC);
  placeholder->SetStyleContext(&aPresContext, kidSC);
  NS_RELEASE(kidSC);
  
  return placeholder;
}

/**
 * Create a next-in-flow for aFrame. Will return the newly created
 * frame in aNextInFlowResult <b>if and only if</b> a new frame is
 * created; otherwise nsnull is returned in aNextInFlowResult.
 */
nsresult
nsHTMLContainerFrame::CreateNextInFlow(nsIPresContext& aPresContext,
                                       nsIFrame* aOuterFrame,
                                       nsIFrame* aFrame,
                                       nsIFrame*& aNextInFlowResult)
{
  aNextInFlowResult = nsnull;

  nsIFrame* nextInFlow;
  aFrame->GetNextInFlow(nextInFlow);
  if (nsnull == nextInFlow) {
    // Create a continuation frame for the child frame and insert it
    // into our lines child list.
    nsIFrame* nextFrame;
    aFrame->GetNextSibling(nextFrame);
    nsIStyleContext* kidSC;
    aFrame->GetStyleContext(&aPresContext, kidSC);
    aFrame->CreateContinuingFrame(aPresContext, aOuterFrame,
                                  kidSC, nextInFlow);
    NS_RELEASE(kidSC);
    if (nsnull == nextInFlow) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aFrame->SetNextSibling(nextInFlow);
    nextInFlow->SetNextSibling(nextFrame);

    NS_FRAME_LOG(NS_FRAME_TRACE_NEW_FRAMES,
       ("nsInlineReflow::MaybeCreateNextInFlow: frame=%p nextInFlow=%p",
        aFrame, nextInFlow));

    aNextInFlowResult = nextInFlow;
  }
  return NS_OK;
}

// Walk the new frames and check if there are any elements that need
// wrapping. For floating frames we create a placeholder frame to wrap
// around the original frame. In addition, if the floating object
// needs a body frame, provide that too.

// For placeholder frames we...

// For frames that require scrolling we...

nsresult
nsHTMLContainerFrame::WrapFrames(nsIPresContext& aPresContext,
                                 nsIFrame* aPrevFrame, nsIFrame* aNewFrame)
{
  nsresult rv = NS_OK;

  nsIFrame* frame = aNewFrame;
  while (nsnull != frame) {
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);

    // See if the element wants to be floated
    if (NS_STYLE_FLOAT_NONE != display->mFloats) {
      // Create a placeholder frame that will serve as the anchor point.
      nsPlaceholderFrame* placeholder =
        CreatePlaceholderFrame(aPresContext, frame);
      if (nsnull == placeholder) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      // Remove the floated element from the flow, and replace it with
      // the placeholder frame
      if (nsnull == aPrevFrame) {
        mFirstChild = placeholder;
      } else {
        aPrevFrame->SetNextSibling(placeholder);
      }
      nsIFrame* nextSibling;
      frame->GetNextSibling(nextSibling);
      placeholder->SetNextSibling(nextSibling);
      frame->SetNextSibling(nsnull);

      // If the floated element can contain children then wrap it in a
      // BODY frame before floating it
      nsIContent* content;
      PRBool isContainer;
      frame->GetContent(content);
      if (nsnull != content) {
        content->CanContainChildren(isContainer);
        if (isContainer) {
          // Wrap the floated element in a BODY frame.
          nsIFrame* wrapperFrame;
          rv = NS_NewBodyFrame(content, this, wrapperFrame);
          if (NS_OK != rv) {
            return rv;
          }
    
          // The body wrapper frame gets the original style context,
          // and the floated frame gets a pseudo style context
          nsIStyleContext*  kidStyle;
          frame->GetStyleContext(&aPresContext, kidStyle);
          wrapperFrame->SetStyleContext(&aPresContext, kidStyle);
          NS_RELEASE(kidStyle);

          nsIStyleContext* pseudoStyle;
          pseudoStyle = aPresContext.ResolvePseudoStyleContextFor(nsHTMLAtoms::columnPseudo,
                                                                  wrapperFrame);
          frame->SetStyleContext(&aPresContext, pseudoStyle);
          NS_RELEASE(pseudoStyle);
    
          // Init the body frame
          wrapperFrame->Init(aPresContext, frame);

          // Bind the wrapper frame to the placeholder
          placeholder->SetAnchoredItem(wrapperFrame);
        }
        NS_RELEASE(content);
      }

      frame = placeholder;
    }

    // Remember the previous frame
    aPrevFrame = frame;
    frame->GetNextSibling(frame);
  }

  return rv;
}

nsresult
nsHTMLContainerFrame::CreateViewForFrame(nsIPresContext& aPresContext,
                                         nsIFrame* aFrame,
                                         nsIStyleContext* aStyleContext,
                                         PRBool aForce)
{
  nsIView* view;
  aFrame->GetView(view);
  if (nsnull == view) {
    // We don't yet have a view; see if we need a view

    // See if the opacity is not the same as the geometric parent
    // frames opacity.
    if (!aForce) {
      nsIFrame* parent;
      aFrame->GetGeometricParent(parent);
      if (nsnull != parent) {
        // Get my nsStyleColor
        const nsStyleColor* myColor = (const nsStyleColor*)
          aStyleContext->GetStyleData(eStyleStruct_Color);

        // Get parent's nsStyleColor
        const nsStyleColor* parentColor;
        parent->GetStyleData(eStyleStruct_Color,
                             (const nsStyleStruct*&)parentColor);

        // If the opacities are different then I need a view
        if (myColor->mOpacity != parentColor->mOpacity) {
          NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
            ("nsHTMLContainerFrame::CreateViewForFrame: frame=%p opacity=%g parentOpacity=%g",
             aFrame, myColor->mOpacity, parentColor->mOpacity));
          aForce = PR_TRUE;
        }
      }
    }

    // See if the frame is being relatively positioned
    if (!aForce) {
      const nsStylePosition* position = (const nsStylePosition*)
        aStyleContext->GetStyleData(eStyleStruct_Position);
      if (NS_STYLE_POSITION_RELATIVE == position->mPosition) {
        NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
          ("nsHTMLContainerFrame::CreateViewForFrame: frame=%p relatively positioned",
           aFrame));
        aForce = PR_TRUE;
      }
    }

    if (aForce) {
      // Create a view
      nsIFrame* parent;
      nsIView*  view;

      aFrame->GetParentWithView(parent);
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

        nsRect bounds;
        aFrame->GetRect(bounds);
        view->Init(viewManager, bounds, rootView);
        viewManager->InsertChild(rootView, view, 0);

        NS_RELEASE(viewManager);
      }

      // Remember our view
      aFrame->SetView(view);

      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
        ("nsHTMLContainerFrame::CreateViewForFrame: frame=%p view=%p",
         aFrame));
      return result;
    }
  }
  return NS_OK;
}
