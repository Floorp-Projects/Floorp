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
                                    aDirtyRect, rect, *color, 0, 0);
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

nsAbsoluteFrame*
nsHTMLContainerFrame::CreateAbsolutePlaceholderFrame(nsIPresContext& aPresContext,
                                                     nsIFrame*       aAbsoluteFrame)
{
  nsIContent* content;
  aAbsoluteFrame->GetContent(content);

  nsAbsoluteFrame* placeholder;
  nsAbsoluteFrame::NewFrame((nsIFrame**)&placeholder, content, this, aAbsoluteFrame);
  NS_IF_RELEASE(content);

  // Let the placeholder share the same style context as the floated element
  nsIStyleContext*  kidSC;
  aAbsoluteFrame->GetStyleContext(&aPresContext, kidSC);
  placeholder->SetStyleContext(&aPresContext, kidSC);
  NS_RELEASE(kidSC);
  
  return placeholder;
}

PRBool
nsHTMLContainerFrame::CreateWrapperFrame(nsIPresContext& aPresContext,
                                         nsIFrame*       aFrame,
                                         nsIFrame*&      aWrapperFrame)
{
  // If the floated element can contain children then wrap it in a
  // BODY frame before floating it
  nsIContent* content;
  PRBool      isContainer;

  aFrame->GetContent(content);
  content->CanContainChildren(isContainer);
  if (isContainer) {
    // Wrap the floated element in a BODY frame.
    NS_NewBodyFrame(content, this, aWrapperFrame, PR_FALSE);

    // The body wrapper frame gets the original style context, and the floated
    // frame gets a pseudo style context
    nsIStyleContext*  kidStyle;
    aFrame->GetStyleContext(&aPresContext, kidStyle);
    aWrapperFrame->SetStyleContext(&aPresContext, kidStyle);
    NS_RELEASE(kidStyle);

    nsIStyleContext*  pseudoStyle;
    pseudoStyle = aPresContext.ResolvePseudoStyleContextFor(nsHTMLAtoms::columnPseudo,
                                                            aWrapperFrame);
    aFrame->SetStyleContext(&aPresContext, pseudoStyle);
    NS_RELEASE(pseudoStyle);

    // Init the body frame
    aWrapperFrame->Init(aPresContext, aFrame);
  }

  NS_RELEASE(content);
  return isContainer;
}

PRBool
nsHTMLContainerFrame::MoveFrameOutOfFlow(nsIPresContext&        aPresContext,
                                         nsIFrame*              aFrame,
                                         const nsStyleDisplay*  aDisplay,
                                         const nsStylePosition* aPosition,
                                         nsIFrame*&             aPlaceholderFrame)
{
  aPlaceholderFrame = nsnull;

  nsIFrame* nextSibling;

  // See if the element wants to be floated or absolutely positioned
  if (NS_STYLE_FLOAT_NONE != aDisplay->mFloats) {
    aFrame->GetNextSibling(nextSibling);
    aFrame->SetNextSibling(nsnull);

    // Create a placeholder frame that will serve as the anchor point.
    nsPlaceholderFrame* placeholder =
      CreatePlaceholderFrame(aPresContext, aFrame);

    // See if we need to wrap the frame in a BODY frame
    nsIFrame*  wrapperFrame;
    if (CreateWrapperFrame(aPresContext, aFrame, wrapperFrame)) {
      // Bind the wrapper frame to the placeholder
      placeholder->SetAnchoredItem(wrapperFrame);
    }

    aPlaceholderFrame = placeholder;

  } else if (NS_STYLE_POSITION_ABSOLUTE == aPosition->mPosition) {
    aFrame->GetNextSibling(nextSibling);
    aFrame->SetNextSibling(nsnull);

    // Create a placeholder frame that will serve as the anchor point.
    nsAbsoluteFrame* placeholder =
      CreateAbsolutePlaceholderFrame(aPresContext, aFrame);

    // See if we need to wrap the frame in a BODY frame
    nsIFrame*  wrapperFrame;
    if (CreateWrapperFrame(aPresContext, aFrame, wrapperFrame)) {
      // Bind the wrapper frame to the placeholder
      placeholder->SetAbsoluteFrame(wrapperFrame);
    }

    aPlaceholderFrame = placeholder;
  }

  if (nsnull == aPlaceholderFrame) {
    return PR_FALSE;
  } else {
    aPlaceholderFrame->SetNextSibling(nextSibling);
    return PR_TRUE;
  }
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
          rv = NS_NewBodyFrame(content, this, wrapperFrame, PR_FALSE);
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

NS_IMETHODIMP
nsHTMLContainerFrame::AttributeChanged(nsIPresShell* aShell,
                                       nsIPresContext* aPresContext,
                                       nsIContent* aChild,
                                       nsIAtom* aAttribute)
{
  if (nsHTMLAtoms::style == aAttribute) {
    ApplyStyleChangeToTree(*aPresContext, this);
    StyleChangeReflow(*aPresContext, this);
  }

  return NS_OK;
}

static void
RemapStyleInTree(nsIPresContext* aPresContext,
                 nsIFrame* aFrame)
{
  nsIStyleContext*  sc;
  aFrame->GetStyleContext(nsnull, sc);
  if (nsnull != sc) {
    sc->RemapStyle(aPresContext);

    // Update the children too...
    nsIFrame* kid;
    aFrame->FirstChild(kid);
    while (nsnull != kid) {
      RemapStyleInTree(aPresContext, kid);
      kid->GetNextSibling(kid);
    }
  }
}

void
nsHTMLContainerFrame::ApplyStyleChangeToTree(nsIPresContext& aPresContext,
                                             nsIFrame* aFrame)
{
  nsIContent* content;
  nsIFrame* geometricParent;
  aFrame->GetGeometricParent(geometricParent);
  aFrame->GetContent(content);

  if (nsnull != content) {
    PRBool  onlyRemap = PR_FALSE;
    nsIStyleContext* oldSC;
    aFrame->GetStyleContext(nsnull, oldSC);
    nsIStyleContext* newSC =
      aPresContext.ResolveStyleContextFor(content, geometricParent);
    if (newSC == oldSC) {
      // Force cached style data to be recomputed
      newSC->RemapStyle(&aPresContext);
      onlyRemap = PR_TRUE;
    }
    else {
      // Switch to new style context
      aFrame->SetStyleContext(&aPresContext, newSC);
    }
    NS_IF_RELEASE(oldSC);
    NS_RELEASE(newSC);
    NS_RELEASE(content);

    // Update the children too...
    nsIFrame* kid;
    aFrame->FirstChild(kid);
    if (onlyRemap) {
      while (nsnull != kid) {
        RemapStyleInTree(&aPresContext, kid);
        kid->GetNextSibling(kid);
      }
    }
    else {
      while (nsnull != kid) {
        ApplyStyleChangeToTree(aPresContext, kid);
        kid->GetNextSibling(kid);
      }
    }
  }
}

void
nsHTMLContainerFrame::ApplyRenderingChangeToTree(nsIPresContext& aPresContext,
                                                 nsIFrame* aFrame)
{
  nsIViewManager* viewManager = nsnull;

  // Trigger rendering updates by damaging this frame and any
  // continuations of this frame.
  while (nsnull != aFrame) {
    // Get the frame's bounding rect
    nsRect r;
    aFrame->GetRect(r);
    r.x = 0;
    r.y = 0;

    // Get view if this frame has one and trigger an update. If the
    // frame doesn't have a view, find the nearest containing view
    // (adjusting r's coordinate system to reflect the nesting) and
    // update there.
    nsIView* view;
    aFrame->GetView(view);
    if (nsnull != view) {
    } else {
      nsPoint offset;
      aFrame->GetOffsetFromView(offset, view);
      NS_ASSERTION(nsnull != view, "no view");
      r += offset;
    }
    if (nsnull == viewManager) {
      view->GetViewManager(viewManager);
    }
    viewManager->UpdateView(view, r, NS_VMREFRESH_NO_SYNC);

    aFrame->GetNextInFlow(aFrame);
  }

  if (nsnull != viewManager) {
    viewManager->Composite();
    NS_RELEASE(viewManager);
  }
}

void
nsHTMLContainerFrame::StyleChangeReflow(nsIPresContext& aPresContext,
                                        nsIFrame* aFrame)
{
  nsIPresShell* shell;
  shell = aPresContext.GetShell();
    
  nsIReflowCommand* reflowCmd;
  nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                        nsIReflowCommand::StyleChanged);
  if (NS_SUCCEEDED(rv)) {
    shell->AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }

  NS_RELEASE(shell);
}
