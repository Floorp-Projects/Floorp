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
#include "nsAbsoluteContainingBlock.h"
#include "nsContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsIAreaFrame.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsIViewManager.h"
#include "nsLayoutAtoms.h"
#include "nsIReflowCommand.h"
#include "nsIPresShell.h"
#include "nsHTMLParts.h"

static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);

nsresult
nsAbsoluteContainingBlock::FirstChild(const nsIFrame* aDelegatingFrame,
                                      nsIAtom*        aListName,
                                      nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(nsLayoutAtoms::absoluteList == aListName, "unexpected child list name");
  *aFirstChild = mAbsoluteFrames.FirstChild();
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::SetInitialChildList(nsIFrame*       aDelegatingFrame,
                                               nsIPresContext& aPresContext,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  NS_PRECONDITION(nsLayoutAtoms::absoluteList == aListName, "unexpected child list name");
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  mAbsoluteFrames.SetFrames(aChildList);
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::AppendFrames(nsIFrame*       aDelegatingFrame,
                                        nsIPresContext& aPresContext,
                                        nsIPresShell&   aPresShell,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aFrameList)
{
  nsresult  rv = NS_OK;

  // Append the frames to our list of absolutely positioned frames
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.AppendFrames(nsnull, aFrameList);

  // Generate a reflow command to reflow the dirty frames
  nsIReflowCommand* reflowCmd;
  rv = NS_NewHTMLReflowCommand(&reflowCmd, aDelegatingFrame, nsIReflowCommand::ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    reflowCmd->SetChildListName(nsLayoutAtoms::absoluteList);
    aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }

  return rv;
}

nsresult
nsAbsoluteContainingBlock::InsertFrames(nsIFrame*       aDelegatingFrame,
                                        nsIPresContext& aPresContext,
                                        nsIPresShell&   aPresShell,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aPrevFrame,
                                        nsIFrame*       aFrameList)
{
  nsresult  rv = NS_OK;

  // Insert the new frames
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);

  // Generate a reflow command to reflow the dirty frames
  nsIReflowCommand* reflowCmd;
  rv = NS_NewHTMLReflowCommand(&reflowCmd, aDelegatingFrame, nsIReflowCommand::ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    reflowCmd->SetChildListName(nsLayoutAtoms::absoluteList);
    aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }

  return rv;
}

nsresult
nsAbsoluteContainingBlock::RemoveFrame(nsIFrame*       aDelegatingFrame,
                                       nsIPresContext& aPresContext,
                                       nsIPresShell&   aPresShell,
                                       nsIAtom*        aListName,
                                       nsIFrame*       aOldFrame)
{
  PRBool result = mAbsoluteFrames.DestroyFrame(aPresContext, aOldFrame);
  NS_ASSERTION(result, "didn't find frame to delete");
  // Because positioned frames aren't part of a flow, there's no additional
  // work to do, e.g. reflowing sibling frames. And because positioned frames
  // have a view, we don't need to repaint
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::Reflow(nsIFrame*                aDelegatingFrame,
                                  nsIPresContext&          aPresContext,
                                  const nsHTMLReflowState& aReflowState)
{
  // Make a copy of the reflow state. If the reason is eReflowReason_Incremental,
  // then change it to eReflowReason_Resize
  nsHTMLReflowState reflowState(aReflowState);
  if (eReflowReason_Incremental == reflowState.reason) {
    reflowState.reason = eReflowReason_Resize;
  }

  nsIFrame* kidFrame;
  for (kidFrame = mAbsoluteFrames.FirstChild(); nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
    // Reflow the frame
    nsReflowStatus  kidStatus;
    ReflowAbsoluteFrame(aDelegatingFrame, aPresContext, reflowState, kidFrame,
                        PR_FALSE, kidStatus);
  }
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::IncrementalReflow(nsIFrame*                aDelegatingFrame,
                                             nsIPresContext&          aPresContext,
                                             const nsHTMLReflowState& aReflowState,
                                             PRBool&                  aWasHandled)
{
  // Initialize the OUT paremeter
  aWasHandled = PR_FALSE;

  // See if the reflow command is targeted at us
  nsIFrame* targetFrame;
  aReflowState.reflowCommand->GetTarget(targetFrame);

  if (aReflowState.frame == targetFrame) {
    nsIAtom*  listName;
    PRBool    isAbsoluteChild;

    // It's targeted at us. See if it's for the positioned child frames
    aReflowState.reflowCommand->GetChildListName(listName);
    isAbsoluteChild = nsLayoutAtoms::absoluteList == listName;
    NS_IF_RELEASE(listName);

    if (isAbsoluteChild) {
      nsIReflowCommand::ReflowType  type;

      // Get the type of reflow command
      aReflowState.reflowCommand->GetType(type);

      // The only type of reflow command we expect is that we have dirty
      // child frames to reflow
      NS_ASSERTION(nsIReflowCommand::ReflowDirty, "unexpected reflow type");

      // Walk the positioned frames and reflow the dirty frames
      for (nsIFrame* f = mAbsoluteFrames.FirstChild(); f; f->GetNextSibling(&f)) {
        nsFrameState  frameState;

        f->GetFrameState(&frameState);
        if (frameState & NS_FRAME_IS_DIRTY) {
          nsReflowStatus  status;
          ReflowAbsoluteFrame(aDelegatingFrame, aPresContext, aReflowState,
                              f, PR_TRUE, status);
        }
      }

      // Indicate we handled the reflow command
      aWasHandled = PR_TRUE;
    }

  } else {
    // Peek at the next frame in the reflow path
    nsIFrame* nextFrame;
    aReflowState.reflowCommand->GetNext(nextFrame, PR_FALSE);

    // See if it's one of our absolutely positioned child frames
    NS_ASSERTION(nsnull != nextFrame, "next frame in reflow command is null"); 
    if (mAbsoluteFrames.ContainsFrame(nextFrame)) {
      // Remove the next frame from the reflow path
      aReflowState.reflowCommand->GetNext(nextFrame, PR_TRUE);

      nsReflowStatus  kidStatus;
      ReflowAbsoluteFrame(aDelegatingFrame, aPresContext, aReflowState,
                          nextFrame, PR_FALSE, kidStatus);
      // We don't need to invalidate anything because the frame should
      // invalidate any area within its frame that needs repainting, and
      // because it has a view if it changes size the view manager will
      // damage the dirty area
      aWasHandled = PR_TRUE;
    }
  }

  return NS_OK;
}

void
nsAbsoluteContainingBlock::DestroyFrames(nsIFrame*       aDelegatingFrame,
                                         nsIPresContext& aPresContext)
{
  mAbsoluteFrames.DestroyFrames(aPresContext);
}

// XXX Optimize the case where it's a resize reflow and the absolutely
// positioned child has the exact same size and position and skip the
// reflow...
nsresult
nsAbsoluteContainingBlock::ReflowAbsoluteFrame(nsIFrame*                aDelegatingFrame,
                                               nsIPresContext&          aPresContext,
                                               const nsHTMLReflowState& aReflowState,
                                               nsIFrame*                aKidFrame,
                                               PRBool                   aInitialReflow,
                                               nsReflowStatus&          aStatus)
{
  nsresult  rv;
  nsMargin  border;

  // Get the border values
  aReflowState.mStyleSpacing->GetBorder(border);
  
  nsIHTMLReflow*  htmlReflow;
  rv = aKidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  if (NS_SUCCEEDED(rv)) {
    htmlReflow->WillReflow(aPresContext);

    nsSize              availSize(aReflowState.mComputedWidth, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowMetrics kidDesiredSize(nsnull);
    nsHTMLReflowState   kidReflowState(aPresContext, aReflowState, aKidFrame,
                                       availSize);

    // If it's the initial reflow, then override the reflow reason. This is
    // used when frames are inserted incrementally
    if (aInitialReflow) {
      kidReflowState.reason = eReflowReason_Initial;
    }

    rv = htmlReflow->Reflow(aPresContext, kidDesiredSize, kidReflowState, aStatus);

    // Because we don't know the size of a replaced element until after we reflow
    // it 'auto' margins must be computed now, and we need to take into account
    // min-max information
    if (NS_FRAME_IS_REPLACED(kidReflowState.mFrameType)) {
      // Factor in any minimum and maximum size information
      if (kidDesiredSize.width > kidReflowState.mComputedMaxWidth) {
        kidDesiredSize.width = kidReflowState.mComputedMaxWidth;
      } else if (kidDesiredSize.width < kidReflowState.mComputedMinWidth) {
        kidDesiredSize.width = kidReflowState.mComputedMinWidth;
      }
      if (kidDesiredSize.height > kidReflowState.mComputedMaxHeight) {
        kidDesiredSize.height = kidReflowState.mComputedMaxHeight;
      } else if (kidDesiredSize.height < kidReflowState.mComputedMinHeight) {
        kidDesiredSize.height = kidReflowState.mComputedMinHeight;
      }

      // Get the containing block width/height
      nscoord containingBlockWidth, containingBlockHeight;
      kidReflowState.ComputeContainingBlockRectangle(&aReflowState,
                                                     containingBlockWidth,
                                                     containingBlockHeight);

      // XXX This code belongs in nsHTMLReflowState...
      if ((NS_AUTOMARGIN == kidReflowState.mComputedMargin.left) ||
          (NS_AUTOMARGIN == kidReflowState.mComputedMargin.right)) {
        // Calculate the amount of space for margins
        nscoord availMarginSpace = containingBlockWidth -
          kidReflowState.mComputedOffsets.left - kidReflowState.mComputedBorderPadding.left -
          kidDesiredSize.width - kidReflowState.mComputedBorderPadding.right -
          kidReflowState.mComputedOffsets.right;

        if (NS_AUTOMARGIN == kidReflowState.mComputedMargin.left) {
          if (NS_AUTOMARGIN == kidReflowState.mComputedMargin.right) {
            // Both 'margin-left' and 'margin-right' are 'auto', so they get
            // equal values
            kidReflowState.mComputedMargin.left = availMarginSpace / 2;
            kidReflowState.mComputedMargin.right = availMarginSpace -
              kidReflowState.mComputedMargin.left;
          } else {
            // Just 'margin-left' is 'auto'
            kidReflowState.mComputedMargin.left = availMarginSpace -
              kidReflowState.mComputedMargin.right;
          }
        } else {
          // Just 'margin-right' is 'auto'
          kidReflowState.mComputedMargin.right = availMarginSpace -
            kidReflowState.mComputedMargin.left;
        }
      }
      if ((NS_AUTOMARGIN == kidReflowState.mComputedMargin.top) ||
          (NS_AUTOMARGIN == kidReflowState.mComputedMargin.bottom)) {
        // Calculate the amount of space for margins
        nscoord availMarginSpace = containingBlockHeight -
          kidReflowState.mComputedOffsets.top - kidReflowState.mComputedBorderPadding.top -
          kidDesiredSize.height - kidReflowState.mComputedBorderPadding.bottom -
          kidReflowState.mComputedOffsets.bottom;

        if (NS_AUTOMARGIN == kidReflowState.mComputedMargin.top) {
          if (NS_AUTOMARGIN == kidReflowState.mComputedMargin.bottom) {
            // Both 'margin-top' and 'margin-bottom' are 'auto', so they get
            // equal values
            kidReflowState.mComputedMargin.top = availMarginSpace / 2;
            kidReflowState.mComputedMargin.bottom = availMarginSpace -
              kidReflowState.mComputedMargin.top;
          } else {
            // Just 'margin-top' is 'auto'
            kidReflowState.mComputedMargin.top = availMarginSpace -
              kidReflowState.mComputedMargin.bottom;
          }
        } else {
          // Just 'margin-bottom' is 'auto'
          kidReflowState.mComputedMargin.bottom = availMarginSpace -
            kidReflowState.mComputedMargin.top;
        }
      }
    }

    // XXX If the child had a fixed height, then make sure it respected it...
    if (NS_AUTOHEIGHT != kidReflowState.mComputedHeight) {
      if (kidDesiredSize.height < kidReflowState.mComputedHeight) {
        kidDesiredSize.height = kidReflowState.mComputedHeight;
        kidDesiredSize.height += kidReflowState.mComputedBorderPadding.top +
                                 kidReflowState.mComputedBorderPadding.bottom;
      }
    }
    
    // Position the child relative to our padding edge
    nsRect  rect(border.left + kidReflowState.mComputedOffsets.left + kidReflowState.mComputedMargin.left,
                 border.top + kidReflowState.mComputedOffsets.top + kidReflowState.mComputedMargin.top,
                 kidDesiredSize.width, kidDesiredSize.height);
    aKidFrame->SetRect(rect);
  }

  return rv;
}

nsresult
nsAbsoluteContainingBlock::GetPositionedInfo(const nsIFrame* aDelegatingFrame,
                                             nscoord&        aXMost,
                                             nscoord&        aYMost) const
{
  aXMost = aYMost = 0;
  for (nsIFrame* f = mAbsoluteFrames.FirstChild(); nsnull != f; f->GetNextSibling(&f)) {
    // Get the frame's x-most and y-most. This is for its flowed content only
    nsRect  rect;
    f->GetRect(rect);

    if (rect.XMost() > aXMost) {
      aXMost = rect.XMost();
    }
    if (rect.YMost() > aYMost) {
      aYMost = rect.YMost();
    }

    // If the child frame is also an area frame, then take into account its child
    // absolutely positioned elements
    nsIAreaFrame* areaFrame;
    if (NS_SUCCEEDED(f->QueryInterface(kAreaFrameIID, (void**)&areaFrame))) {
      nscoord xMost, yMost;

      areaFrame->GetPositionedInfo(xMost, yMost);
      // Convert to our coordinate space
      xMost += rect.x;
      yMost += rect.y;

      if (xMost > aXMost) {
        aXMost = xMost;
      }
      if (yMost > aYMost) {
        aYMost = yMost;
      }
    }
  }

  return NS_OK;
}

