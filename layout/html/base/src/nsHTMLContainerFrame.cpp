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
#include "nsIContentDelegate.h"
#include "nsIHTMLContent.h"
#include "nsHTMLParts.h"
#include "nsHTMLBase.h"
#include "nsScrollFrame.h"
#include "nsIView.h"
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

NS_METHOD nsHTMLContainerFrame::HandleEvent(nsIPresContext& aPresContext,
                                            nsGUIEvent* aEvent,
                                            nsEventStatus& aEventStatus)
{
  return nsContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
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
  nsIReflowCommand* cmd;
  nsresult          result;
                                                  
  result = NS_NewHTMLReflowCommand(&cmd, lastInFlow, nsIReflowCommand::FrameAppended);
  if (NS_OK == result) {
    aShell->AppendReflowCommand(cmd);
    NS_RELEASE(cmd);
  }
  
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
  
#if XXX
      // If the frame is being used as a pseudo-frame then make sure to propagate
      // the content offsets back up the tree
      if (isPseudoFrame) {
        frame->PropagateContentOffsets();
      }
#endif
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
  nsIFrame* newFrame;
  nsresult rv = nsHTMLBase::CreateFrame(aPresContext, this, aChild, nsnull,
                                        newFrame);
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

#if XXX
      if (parent->IsPseudoFrame()) {
        parent->PropagateContentOffsets();
      }
#endif
    }
  }
  parent->mChildCount++;

  // Generate a reflow command
  nsIReflowCommand* cmd;
                                               
  rv = NS_NewHTMLReflowCommand(&cmd, parent, nsIReflowCommand::FrameAppended,
                               newFrame);
  if (NS_OK == rv) {
    aShell->AppendReflowCommand(cmd);
    NS_RELEASE(cmd);
  }

  return rv;
}

nsresult
nsHTMLContainerFrame::ProcessInitialReflow(nsIPresContext* aPresContext)
{
  if (nsnull == mPrevInFlow) {
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
  }

  return NS_OK;
}
