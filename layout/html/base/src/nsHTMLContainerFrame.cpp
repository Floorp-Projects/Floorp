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
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsViewsCID.h"
#include "nsIReflowCommand.h"
#include "nsHTMLIIDs.h"
#include "nsDOMEvent.h"

NS_DEF_PTR(nsIStyleContext);

nsHTMLContainerFrame::nsHTMLContainerFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsContainerFrame(aContent, aParent)
{
}

nsHTMLContainerFrame::~nsHTMLContainerFrame()
{
}

NS_IMETHODIMP
nsHTMLContainerFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
  // Probe for a JS onPaint event handler
  nsIHTMLContent* hc;
  if (NS_OK == mContent->QueryInterface(kIHTMLContentIID, (void**)&hc)) {
    nsHTMLValue val;
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        hc->GetAttribute(nsHTMLAtoms::onpaint, val)) {
      nsEventStatus es;
      nsresult rv;

      nsRect r(aDirtyRect);
      nsPaintEvent event;
      event.eventStructType = NS_PAINT_EVENT;
      event.message = NS_PAINT;
      event.point.x = 0;
      event.point.y = 0;
      event.time = 0;
      event.widget = nsnull;
      event.nativeMsg = nsnull;
      event.renderingContext = &aRenderingContext;
      event.rect = &r;

      rv = mContent->HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, es);
      if (NS_OK == rv) {
      }
    }
    NS_RELEASE(hc);
  }

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
    NS_NewBodyFrame(content, this, aWrapperFrame, NS_BODY_SHRINK_WRAP);/* XXX auto margins? */

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

// XXX pass in aFrame's style context instead
PRBool
nsHTMLContainerFrame::MoveFrameOutOfFlow(nsIPresContext&        aPresContext,
                                         nsIFrame*              aFrame,
                                         const nsStyleDisplay*  aDisplay,
                                         const nsStylePosition* aPosition,
                                         nsIFrame*&             aPlaceholderFrame)
{
  // Initialize OUT parameter
  aPlaceholderFrame = nsnull;

  // See if the element wants to be floated or absolutely positioned
  PRBool  isFloated =
    (NS_STYLE_FLOAT_LEFT == aDisplay->mFloats) ||
    (NS_STYLE_FLOAT_RIGHT == aDisplay->mFloats);
  PRBool  isAbsolute = NS_STYLE_POSITION_ABSOLUTE == aPosition->mPosition;

  if (isFloated || isAbsolute) {
    nsIFrame* nextSibling;

    // Set aFrame's next sibling to nsnull, and remember the current next
    // sibling pointer
    aFrame->GetNextSibling(nextSibling);
    aFrame->SetNextSibling(nsnull);

    nsIFrame* frameToWrapWithAView = aFrame;
    if (isFloated) {
      // Create a placeholder frame that will serve as the anchor point.
      nsPlaceholderFrame* placeholder =
        CreatePlaceholderFrame(aPresContext, aFrame);
  
      // See if we need to wrap the frame in a BODY frame
      nsIFrame*  wrapperFrame;
      if (CreateWrapperFrame(aPresContext, aFrame, wrapperFrame)) {
        // Bind the wrapper frame to the placeholder
        placeholder->SetAnchoredItem(wrapperFrame);
        frameToWrapWithAView = wrapperFrame;
      }
  
      aPlaceholderFrame = placeholder;

    } else {
      // Create a placeholder frame that will serve as the anchor point.
      nsAbsoluteFrame* placeholder =
        CreateAbsolutePlaceholderFrame(aPresContext, aFrame);
  
      // See if we need to wrap the frame in a BODY frame
      nsIFrame*  wrapperFrame;
      if (CreateWrapperFrame(aPresContext, aFrame, wrapperFrame)) {
        // Bind the wrapper frame to the placeholder
        placeholder->SetAbsoluteFrame(wrapperFrame);
        frameToWrapWithAView = wrapperFrame;
      }
  
      aPlaceholderFrame = placeholder;
    }

    // Wrap the frame in a view if necessary
    nsIStyleContext* kidSC;
    aFrame->GetStyleContext(&aPresContext, kidSC);
    nsresult rv = CreateViewForFrame(aPresContext, frameToWrapWithAView,
                                     kidSC, PR_FALSE);
    NS_RELEASE(kidSC);
    if (NS_OK != rv) {
      return rv;
    }

    // Set the placeholder's next sibling to what aFrame's next sibling was
    aPlaceholderFrame->SetNextSibling(nextSibling);
    return PR_TRUE;
  }

  return PR_FALSE;
}

//XXX handle replace reflow command
NS_METHOD nsHTMLContainerFrame::AddFrame(const nsHTMLReflowState& aReflowState,
                                         nsIFrame *               aAddedFrame)
{
  nsresult rv=NS_OK;
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowCommand->GetType(type);
  // we have a generic frame that gets inserted but doesn't effect reflow
  // hook it up then ignore it
  if (nsIReflowCommand::FrameAppended==type)
  { // frameAppended reflow -- find the last child and make aInsertedFrame its next sibling
    nsIFrame *lastChild=mFirstChild;
    nsIFrame *nextChild=mFirstChild;
    while (nsnull!=nextChild)
    {
      lastChild=nextChild;
      nextChild->GetNextSibling(nextChild);
    }
    if (nsnull==lastChild)
      mFirstChild = aAddedFrame;
    else
      lastChild->SetNextSibling(aAddedFrame);
  }
  else if (nsIReflowCommand::FrameInserted==type) 
  { // frameInserted reflow -- hook up aInsertedFrame as prevSibling's next sibling, 
    // and be sure to hook in aInsertedFrame's nextSibling (from prevSibling)
    nsIFrame *prevSibling=nsnull;
    rv = aReflowState.reflowCommand->GetPrevSiblingFrame(prevSibling);
    if (NS_SUCCEEDED(rv) && (nsnull!=prevSibling))
    {
      nsIFrame *nextSibling=nsnull;
      prevSibling->GetNextSibling(nextSibling);
      prevSibling->SetNextSibling(aAddedFrame);
      aAddedFrame->SetNextSibling(nextSibling);
    }
    else
    {
      nsIFrame *nextSibling = mFirstChild;
      mFirstChild = aAddedFrame;
      aAddedFrame->SetNextSibling(nextSibling);
    }
  }
  else
  {
    NS_ASSERTION(PR_FALSE, "bad reflow type");
    rv = NS_ERROR_UNEXPECTED;
  }
  return rv;
}
  /** */
NS_METHOD nsHTMLContainerFrame::RemoveFrame(nsIFrame * aRemovedFrame)
{
  nsIFrame *prevChild=nsnull;
  nsIFrame *nextChild=mFirstChild;
  while (nextChild!=aRemovedFrame)
  {
    prevChild=nextChild;
    nextChild->GetNextSibling(nextChild);
  }  
  nextChild=nsnull;
  aRemovedFrame->GetNextSibling(nextChild);
  if (nsnull==prevChild)  // objectFrame was first child
    mFirstChild = nextChild;
  else
    prevChild->SetNextSibling(nextChild);
  return NS_OK;;
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

nsresult
nsHTMLContainerFrame::CreateViewForFrame(nsIPresContext& aPresContext,
                                         nsIFrame* aFrame,
                                         nsIStyleContext* aStyleContext,
                                         PRBool aForce)
{
  nsIView* view;
  aFrame->GetView(view);
  // If we don't yet have a view; see if we need a view
  if (nsnull == view) {
    // Get my nsStyleColor
    const nsStyleColor* myColor = (const nsStyleColor*)
      aStyleContext->GetStyleData(eStyleStruct_Color);

    // See if the opacity is not the same as the geometric parent
    // frames opacity.
    if (!aForce) {
      nsIFrame* parent;
      aFrame->GetGeometricParent(parent);
      if (nsnull != parent) {
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
        // If the background color is transparent then mark the view as having
        // transparent content.
        // XXX We could try and be smarter about this and check whether there's
        // a background image. If there is a background image and the image is
        // fully opaque then we don't need to mark the view as having transparent
        // content...
        if (NS_STYLE_BG_COLOR_TRANSPARENT & myColor->mBackgroundFlags) {
          viewManager->SetViewContentTransparency(view, PR_TRUE);
        }

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


