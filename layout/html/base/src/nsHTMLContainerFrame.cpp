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
#include "nsReflowCommand.h"
#include "nsIPtr.h"
#include "nsAbsoluteFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsIContentDelegate.h"
#include "nsIHTMLContent.h"
#include "nsHTMLParts.h"
#include "nsHTMLFrame.h"
#include "nsScrollFrame.h"
#include "nsIView.h"

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
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, mRect, *color);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, mRect, *spacing, skipSides);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);

  if (nsIFrame::GetShowFrameBorders()) {
    nsIView* view;
    GetView(view);
    if (nsnull != view) {
      aRenderingContext.SetColor(NS_RGB(0,0,255));
      NS_RELEASE(view);
    }
    else {
      aRenderingContext.SetColor(NS_RGB(255,0,0));
    }
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
  return NS_OK;
}

void nsHTMLContainerFrame::TriggerLink(nsIPresContext& aPresContext,
                                       const nsString& aBase,
                                       const nsString& aURLSpec,
                                       const nsString& aTargetSpec,
                                       PRBool aClick)
{
  nsILinkHandler* handler;
  if (NS_OK == aPresContext.GetLinkHandler(&handler)) {
    // Resolve url to an absolute url
    nsIURL* docURL = nsnull;
    nsIDocument* doc = nsnull;
    mContent->GetDocument(doc);
    if (nsnull != doc) {
      docURL = doc->GetDocumentURL();
      NS_RELEASE(doc);
    }

    nsAutoString absURLSpec;
    nsresult rv = NS_MakeAbsoluteURL(docURL, aBase, aURLSpec, absURLSpec);
    if (nsnull != docURL) {
      NS_RELEASE(docURL);
    }

    // Now pass on absolute url to the click handler
    if (aClick) {
      handler->OnLinkClick(this, absURLSpec, aTargetSpec);
    }
    else {
      handler->OnOverLink(this, absURLSpec, aTargetSpec);
    }
  }
}

NS_METHOD nsHTMLContainerFrame::HandleEvent(nsIPresContext& aPresContext,
                                            nsGUIEvent* aEvent,
                                            nsEventStatus& aEventStatus)
{
  aEventStatus = nsEventStatus_eIgnore; 
  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
    if (nsEventStatus_eIgnore ==
        nsContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus)) { 
      // If our child didn't take the click then since we are an
      // anchor, we take the click.
      nsIAtom* tag = mContent->GetTag();
      if (tag == nsHTMLAtoms::a) {
        nsAutoString base, href, target;
        mContent->GetAttribute("href", href);
        mContent->GetAttribute("target", target);
        TriggerLink(aPresContext, base, href, target, PR_TRUE);
        aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      NS_IF_RELEASE(tag);
    }
    break;

  case NS_MOUSE_RIGHT_BUTTON_DOWN:
    // XXX Bring up a contextual menu provided by the application
    break;

  case NS_MOUSE_MOVE:
    if (nsEventStatus_eIgnore ==
        nsContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus)) { 
      nsIAtom* tag = mContent->GetTag();
      if (tag == nsHTMLAtoms::a) {
        nsAutoString base, href, target;
        mContent->GetAttribute("href", href);
        mContent->GetAttribute("target", target);
        TriggerLink(aPresContext, base, href, target, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      NS_IF_RELEASE(tag);
    }
    break;

    // XXX this doesn't seem to do anything yet
  case NS_MOUSE_EXIT:
    if (nsEventStatus_eIgnore ==
        nsContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus)) { 
      nsIAtom* tag = mContent->GetTag();
      if (tag == nsHTMLAtoms::a) {
        nsAutoString empty;
        TriggerLink(aPresContext, empty, empty, empty, PR_FALSE);
        aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      NS_IF_RELEASE(tag);
    }
    break;

  default:
    nsContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
    break;
  }
  return NS_OK;
}

NS_METHOD nsHTMLContainerFrame::GetCursorAt(nsIPresContext& aPresContext,
                                            const nsPoint& aPoint,
                                            nsIFrame** aFrame,
                                            PRInt32& aCursor)
{
  // Get my cursor
  const nsStyleColor* styleColor = (const nsStyleColor*)
    mStyleContext->GetStyleData(eStyleStruct_Color);
  PRInt32 myCursor = styleColor->mCursor;

  // Get child's cursor, if any
  nsresult rv = nsContainerFrame::GetCursorAt(aPresContext, aPoint, aFrame,
                                              aCursor);
  if (NS_OK != rv) return rv;
  if (aCursor != NS_STYLE_CURSOR_INHERIT) {
    nsIAtom* tag = mContent->GetTag();
    if (nsHTMLAtoms::a == tag) {
      // Anchor tags override their child cursors in some cases.
      if ((NS_STYLE_CURSOR_IBEAM == aCursor) &&
          (NS_STYLE_CURSOR_INHERIT != myCursor)) {
        aCursor = myCursor;
      }
    }
    NS_RELEASE(tag);
    return rv;
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

NS_METHOD nsHTMLContainerFrame::ContentAppended(nsIPresShell*   aShell,
                                                nsIPresContext* aPresContext,
                                                nsIContent*     aContainer)
{
  // Get the last-in-flow
  nsHTMLContainerFrame* lastInFlow = (nsHTMLContainerFrame*)GetLastInFlow();

  // Generate a reflow command for the frame
  nsReflowCommand* cmd = new nsReflowCommand(aPresContext, lastInFlow,
                                             nsReflowCommand::FrameAppended);
  aShell->AppendReflowCommand(cmd);
  return NS_OK;
}

static PRBool
HasSameMapping(nsIFrame* aFrame, nsIContent* aContent)
{
  nsIContent* content;
  PRBool      result;

  aFrame->GetContent(content);
  result = content == aContent;
  NS_RELEASE(content);
  return result;
}

enum ContentChangeType  {ContentInserted, ContentDeleted};

static void AdjustIndexInParents(nsIFrame*         aContainerFrame,
                                 nsIContent*       aContainer,
                                 PRInt32           aIndexInParent,
                                 ContentChangeType aChange)
{
  // Walk each child and check if its index-in-parent needs to be
  // adjusted
  nsIFrame* childFrame;
  for (aContainerFrame->FirstChild(childFrame);
       nsnull != childFrame;
       childFrame->GetNextSibling(childFrame))
  {
    // Is the child a pseudo-frame?
    if (HasSameMapping(childFrame, aContainer)) {
      // Don't change the pseudo frame's index-in-parent, but go change the
      // index-in-parent of its children
      AdjustIndexInParents(childFrame, aContainer, aIndexInParent, aChange);
    } else {
      PRInt32 index;

      childFrame->GetContentIndex(index);

#if 0
      if (::ContentInserted == aChange) {
        if (index >= aIndexInParent) {
          childFrame->SetIndexInParent(index + 1);
        }
      } else {
        if (index > aIndexInParent) {
          childFrame->SetIndexInParent(index - 1);
        }
      }
#endif
    }
  }
}

nsresult
nsHTMLContainerFrame::CreateFrameFor(nsIPresContext*  aPresContext,
                                     nsIContent*      aContent,
                                     nsIStyleContext* aStyleContext,
                                     nsIFrame*&       aResult)
{
  // Get the style data for the frame
  const nsStylePosition*  position = (const nsStylePosition*)
    aStyleContext->GetStyleData(eStyleStruct_Position);
  const nsStyleDisplay*   display = (const nsStyleDisplay*)
    aStyleContext->GetStyleData(eStyleStruct_Display);
  nsIFrame*               frame = nsnull;

  // See whether it wants any special handling
  nsresult rv;
  if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
    rv = nsAbsoluteFrame::NewFrame(&frame, aContent, this);
    if (NS_OK == rv) {
      frame->SetStyleContext(aPresContext, aStyleContext);
    }
  }
  else if (display->mFloats != NS_STYLE_FLOAT_NONE) {
    rv = nsPlaceholderFrame::NewFrame(&frame, aContent, this);
    if (NS_OK == rv) {
      frame->SetStyleContext(aPresContext, aStyleContext);
    }
  }
  else if ((NS_STYLE_OVERFLOW_SCROLL == display->mOverflow) ||
           (NS_STYLE_OVERFLOW_AUTO == display->mOverflow)) {
    rv = NS_NewScrollFrame(&frame, aContent, this);
    if (NS_OK == rv) {
      frame->SetStyleContext(aPresContext, aStyleContext);
    }
  }
  else if (NS_STYLE_DISPLAY_NONE == display->mDisplay) {
    rv = nsFrame::NewFrame(&frame, aContent, this);
    if (NS_OK == rv) {
      frame->SetStyleContext(aPresContext, aStyleContext);
    }
  }
  else {
    // Ask the content delegate to create the frame
    nsIContentDelegate* delegate = aContent->GetDelegate(aPresContext);
    rv = delegate->CreateFrame(aPresContext, aContent, this,
                               aStyleContext, frame);
    NS_RELEASE(delegate);
  }
  if (NS_OK == rv) {
    // Wrap the frame in a view if necessary
    rv = nsHTMLFrame::CreateViewForFrame(aPresContext, frame, aStyleContext,
                                         PR_FALSE);
  }
  aResult = frame;
  return rv;
}

NS_METHOD nsHTMLContainerFrame::ContentInserted(nsIPresShell*   aShell,
                                                nsIPresContext* aPresContext,
                                                nsIContent*     aContainer,
                                                nsIContent*     aChild,
                                                PRInt32         aIndexInParent)
{
  // Adjust the index-in-parent of each frame that follows the child that was
  // inserted
  PRBool                isPseudoFrame = IsPseudoFrame();
  nsHTMLContainerFrame* frame = this;

  while (nsnull != frame) {
    // Do a quick check to see whether any of the child frames need their
    // index-in-parent adjusted. Note that this assumes a linear mapping of
    // content to frame
    if (mLastContentOffset >= aIndexInParent) {
      AdjustIndexInParents(frame, aContainer, aIndexInParent, ::ContentInserted);
  
      // If the frame is being used as a pseudo-frame then make sure to propagate
      // the content offsets back up the tree
      if (isPseudoFrame) {
        frame->PropagateContentOffsets();
      }
    }

    frame->GetNextInFlow((nsIFrame*&)frame);
  }

  // Find the frame that precedes this frame
  nsIFrame* prevSibling = nsnull;

  if (aIndexInParent > 0) {
    nsIContent* precedingContent = aContainer->ChildAt(aIndexInParent - 1);
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
  }

  // Get the geometric parent. We expect it to be this frame or one of its
  // next-in-flow(s). It could be a pseudo-frame, but then it better also be
  // a nsHTMLContainerFrame...
  nsHTMLContainerFrame* parent = this;
  if (nsnull != prevSibling) {
    prevSibling->GetGeometricParent((nsIFrame*&)parent);
  }

  // Create the new frame
  nsIStyleContext* kidSC;
  kidSC = aPresContext->ResolveStyleContextFor(aChild, parent);
  nsIFrame* newFrame;
  nsresult rv = parent->CreateFrameFor(aPresContext, aChild, kidSC, newFrame);
  if (NS_OK != rv) {
    return rv;
  }

  // Insert the frame
  if (nsnull == prevSibling) {
    // If there's no preceding frame, then this is the first content child
    NS_ASSERTION(0 == aIndexInParent, "unexpected index-in-parent");
    NS_ASSERTION(0 == parent->mFirstContentOffset, "unexpected first content offset");
    newFrame->SetNextSibling(parent->mFirstChild);
    parent->mFirstChild = newFrame;

  } else {
    nsIFrame* nextSibling;

    // Link the new frame into
    prevSibling->GetNextSibling(nextSibling);
    newFrame->SetNextSibling(nextSibling);
    prevSibling->SetNextSibling(newFrame);

    if (nsnull == nextSibling) {
      // The new frame is the last child frame
      parent->SetLastContentOffset(newFrame);

      if (parent->IsPseudoFrame()) {
        parent->PropagateContentOffsets();
      }
    }
  }
  parent->mChildCount++;

  // Generate a reflow command
  nsReflowCommand* cmd = new nsReflowCommand(aPresContext, parent,
                                             nsReflowCommand::FrameAppended,
                                             newFrame);
  aShell->AppendReflowCommand(cmd);

  return NS_OK;
}

nsresult
nsHTMLContainerFrame::ProcessInitialReflow(nsIPresContext* aPresContext)
{
  const nsStyleDisplay* display = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
               ("nsHTMLContainerFrame::ProcessInitialReflow: display=%d",
                display->mDisplay));
  if (NS_STYLE_DISPLAY_LIST_ITEM == display->mDisplay) {
    // This container is a list-item container. Therefore it needs a
    // bullet content object.
    nsIHTMLContent* bullet;
    nsresult rv = NS_NewHTMLBullet(&bullet);
    if (NS_OK != rv) {
      return rv;
    }

    // Insert the bullet. Do not allow an incremental reflow operation
    // to occur.
    mContent->InsertChildAt(bullet, 0, PR_FALSE);
  }

  return NS_OK;
}
