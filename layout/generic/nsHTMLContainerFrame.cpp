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
  // Do not paint ourselves if we are a pseudo-frame
  if (PR_FALSE == IsPseudoFrame()) {  // this trip isn't really necessary
    nsStyleDisplay* disp =
      (nsStyleDisplay*)mStyleContext->GetData(eStyleStruct_Display);

    if (disp->mVisible) {
      PRIntn skipSides = GetSkipSides();
      nsStyleColor* color =
        (nsStyleColor*)mStyleContext->GetData(eStyleStruct_Color);
      nsStyleSpacing* spacing =
        (nsStyleSpacing*)mStyleContext->GetData(eStyleStruct_Spacing);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, mRect, *color);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, mRect, *spacing, skipSides);
    }
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);

  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
  return NS_OK;
}

void nsHTMLContainerFrame::TriggerLink(nsIPresContext& aPresContext,
                                       const nsString& aBase,
                                       const nsString& aURLSpec,
                                       const nsString& aTargetSpec)
{
  nsILinkHandler* handler;
  if (NS_OK == aPresContext.GetLinkHandler(&handler)) {
    // Resolve url to an absolute url
    nsIURL* docURL = nsnull;
    nsIDocument* doc = mContent->GetDocument();
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
    handler->OnLinkClick(this, absURLSpec, aTargetSpec);
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
        TriggerLink(aPresContext, base, href, target);
        aEventStatus = nsEventStatus_eConsumeNoDefault; 
      }
      NS_IF_RELEASE(tag);
    }
    break;

  case NS_MOUSE_RIGHT_BUTTON_DOWN:
    // XXX Bring up a contextual menu provided by the application
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
  nsStyleColor* styleColor = (nsStyleColor*)
    mStyleContext->GetData(eStyleStruct_Color);
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
  nsStylePosition*    position = (nsStylePosition*)
    aStyleContext->GetData(eStyleStruct_Position);
  nsStyleDisplay*     display = (nsStyleDisplay*)
    aStyleContext->GetData(eStyleStruct_Display);
  nsIFrame*           frame = nsnull;

  // See whether it wants any special handling
  nsresult rv;
  if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
    rv = AbsoluteFrame::NewFrame(&frame, aContent, this);
    if (NS_OK == rv) {
      frame->SetStyleContext(aPresContext, aStyleContext);
    }
  } else if (display->mFloats != NS_STYLE_FLOAT_NONE) {
    rv = PlaceholderFrame::NewFrame(&frame, aContent, this);
    if (NS_OK == rv) {
      frame->SetStyleContext(aPresContext, aStyleContext);
    }
  } else if (NS_STYLE_DISPLAY_NONE == display->mDisplay) {
    rv = nsFrame::NewFrame(&frame, aContent, this);
    if (NS_OK == rv) {
      frame->SetStyleContext(aPresContext, aStyleContext);
    }
  } else {
    // Ask the content delegate to create the frame
    nsIContentDelegate* delegate = aContent->GetDelegate(aPresContext);
    rv = delegate->CreateFrame(aPresContext, aContent, this,
                               aStyleContext, frame);
    NS_RELEASE(delegate);
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

#if 0
nsIFrame::ReflowStatus
nsHTMLContainerFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                        nsReflowMetrics& aDesiredSize,
                                        const nsSize&    aMaxSize,
                                        nsISpaceManager* aSpaceManager,
                                        nsReflowCommand& aReflowCommand)
{
  // 0. Get to the correct flow block for this frame that applies to
  // the effected frame. If the reflow command is not for us or it's
  // deleted or changed then "child" is the child being
  // effected. Otherwise child is the child before the effected
  // content child (or null if the effected child is our first child)
  <T>* flow = FindFlowBlock(aReflowCommand, &child);

  // 1. Recover reflow state
  <T>State state;
  RecoverState(aPresContext, ..., state);

  // 2. Put state into presentation shell cache so child frames can find
  // it.

  if (aReflowCommand.GetTarget() == this) {
    // Apply reflow command to the flow frame; one of it's immediate
    // children has changed state.
    ReflowStatus status;
    switch (aReflowCommand.GetType()) {
    case nsReflowCommand::rcContentAppended:
      status = ReflowAppended(aPresContext, aDesiredSize, aMaxSize,
                              aSpaceManager, state, flow);
      break;
    case nsReflowCommand::rcContentInserted:
      status = ReflowInserted(aPresContext, aDesiredSize, aMaxSize,
                              aSpaceManager, state, flow);
      break;
    case nsReflowCommand::rcContentDeleted:
      status = ReflowDeleted(aPresContext, aDesiredSize, aMaxSize,
                             aSpaceManager, state, flow);
      break;
    case nsReflowCommand::rcContentChanged:
      status = ReflowChanged(aPresContext, aDesiredSize, aMaxSize,
                             aSpaceManager, state, flow);
      break;
    case nsReflowCommand::rcUserDefined:
      switch (
      break;
    default:
      // Ignore all other reflow commands
      status = NS_FRAME_COMPLETE;
      break;
    }
  } else {
    // Reflow command applies to one of our children. We need to
    // figure out which child because it's going to change size most
    // likely and we need to be prepared to deal with it.
    nsIFrame* kid = nsnull;
    status = aReflowCommand.Next(aDesiredSize, aMaxSize, aSpaceManager, kid);

    // Execute the ReflowChanged post-processing code that deals with
    // the child frame's size change; next-in-flows, overflow-lists,
    // etc.
  }

  // 4. Remove state from presentation shell cache

  return status;
}

nsIFrame::ReflowStatus
nsHTMLContainerFrame::ReflowAppended(nsIPresContext*  aPresContext,
                                     nsReflowMetrics& aDesiredSize,
                                     const nsSize&    aMaxSize,
                                     nsISpaceManager* aSpaceManager,
                                     nsReflowCommand& aReflowCommand)
{
  // 1. compute state up to but not including new content w/o reflowing
  // everything that's already been flowed

  // 2. if this container uses pseudo-frames then 2b, else 2a

  //   2a. start a reflow-unmapped of the new children

  //   2b. reflow-mapped the last-child if it's a pseudo as it might
  //   pickup the new children; smarter containers can avoid this if
  //   they can determine that the last-child won't pickup the new
  //   children up. Once reflowing the last-child is complete then if
  //   the status is frComplete then and only then try reflowing any
  //   remaining unmapped children

  //   2c. For inline and column code the result of a reflow mapped
  //   cannot impact any previously reflowed children. For block this
  //   is not true so block needs to reconstruct the line and then
  //   proceed as if the line were being reflowed for the first time
  //   (skipping over the existing children, of course). The line still
  //   needs to be flushed out, vertically aligned, etc, which may cause
  //   it to not fit.

  // 3. we may end up pushing kids to next-in-flow or stopping before
  // all children have been mapped because we are out of room. parent
  // needs to look at our return status and create a next-in-flow for
  // us; the currently implemented reflow-unmapped code will do the
  // right thing in that a child that is being appended and reflowed
  // will get it's continuations created by us; if we run out of room
  // we return an incomplete status to our parent and it does the same
  // to us.
}

// use a custom reflow command when we push or create an overflow list; 

nsIFrame::ReflowStatus
nsHTMLContainerFrame::ReflowInserted(nsIPresContext*  aPresContext,
                                     nsReflowMetrics& aDesiredSize,
                                     const nsSize&    aMaxSize,
                                     nsISpaceManager* aSpaceManager,
                                     nsReflowCommand& aReflowCommand)
{
  // 1. compute state up to but not including new content w/o reflowing
  // everything that's already been flowed

  // 2. Content insertion implies a new child of this container; the
  // content inserted may need special attention (see
  // ContentAppended). The same rules apply. However, if the
  // pseudo-frame doesn't pullup the child then we need to
  // ResizeReflow the addition (We cannot go through the
  // reflow-unmapped path because the child frame(s) will need to be
  // inserted into the list).

  // 2a if reflow results in a push then go through push reflow
  // command path else go through deal-with-size-change
}

nsIFrame::ReflowStatus
nsHTMLContainerFrame::ReflowDeleted(nsIPresContext*  aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize&    aMaxSize,
                                    nsISpaceManager* aSpaceManager,
                                    nsReflowCommand& aReflowCommand)
{
  // 1. compute state up to but not including deleted content w/o reflowing
  // everything that's already been flowed

  // 2. remove all of the child frames that belong to the deleted
  // child; this includes next-in-flows. mark all of our next-in-flows
  // that are impacted by this as dirty (or generate another reflow
  // command)

  // 3. Run reflow-mapped code starting at the deleted child point;
  // return the usual status to the parent.


  // Generate reflow commands for my next-in-flows if they are
  // impacted by deleting my child's next-in-flows
}

// Meta point about ReflowChanged; we will factor out the change so
// that only stylistic changes that actually require a reflow end up
// at this frame. The style system will differentiate between
// rendering-only changes and reflow changes.

nsIFrame::ReflowStatus
nsHTMLContainerFrame::ReflowChanged(nsIPresContext*  aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize&    aMaxSize,
                                    nsISpaceManager* aSpaceManager,
                                    nsReflowCommand& aReflowCommand)
{
  // 1. compute state up to but not including deleted content w/o reflowing
  // everything that's already been flowed

  // 2. delete the frame that corresponds to the changed child (and
  // it's next-in-flows, etc.)

  // 3. run the ReflowInserted code on the content
}
#endif
