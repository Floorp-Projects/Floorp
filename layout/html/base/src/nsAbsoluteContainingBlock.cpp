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

static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);

void
nsAbsoluteContainingBlock::DeleteFrames(nsIPresContext& aPresContext)
{
  mAbsoluteFrames.DeleteFrames(aPresContext);
}

nsresult
nsAbsoluteContainingBlock::FirstChild(nsIAtom*   aListName,
                                      nsIFrame** aFirstChild) const
{
  NS_PRECONDITION(nsLayoutAtoms::absoluteList == aListName, "unexpected child list name");
  *aFirstChild = mAbsoluteFrames.FirstChild();
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::SetInitialChildList(nsIPresContext& aPresContext,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  NS_PRECONDITION(nsLayoutAtoms::absoluteList == aListName, "unexpected child list name");
  mAbsoluteFrames.SetFrames(aChildList);
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::Reflow(nsIPresContext&          aPresContext,
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
    ReflowAbsoluteFrame(aPresContext, reflowState, kidFrame, PR_FALSE,
                        kidStatus);
  }
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::IncrementalReflow(nsIPresContext&          aPresContext,
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

    // It's targeted at us. See if the child frame is absolutely positioned
    aReflowState.reflowCommand->GetChildListName(listName);
    isAbsoluteChild = nsLayoutAtoms::absoluteList == listName;
    NS_IF_RELEASE(listName);

    if (isAbsoluteChild) {
      nsIReflowCommand::ReflowType  type;
      nsIFrame*                     newFrames;
      PRInt32                       numFrames = 0;

      // Get the type of reflow command
      aReflowState.reflowCommand->GetType(type);

      // Handle each specific type
      if (nsIReflowCommand::FrameAppended == type) {
        // Add the frames to our list of absolutely position frames
        aReflowState.reflowCommand->GetChildFrame(newFrames);
        NS_ASSERTION(nsnull != newFrames, "null child list");
        numFrames = nsContainerFrame::LengthOf(newFrames);
        mAbsoluteFrames.AppendFrames(nsnull, newFrames);

      } else if (nsIReflowCommand::FrameRemoved == type) {
        // Get the new frame
        nsIFrame* childFrame;
        aReflowState.reflowCommand->GetChildFrame(childFrame);

        PRBool result = mAbsoluteFrames.DeleteFrame(aPresContext, childFrame);
        NS_ASSERTION(result, "didn't find frame to delete");

      } else if (nsIReflowCommand::FrameInserted == type) {
        // Get the previous sibling
        nsIFrame* prevSibling;
        aReflowState.reflowCommand->GetPrevSiblingFrame(prevSibling);

        // Insert the new frames
        aReflowState.reflowCommand->GetChildFrame(newFrames);
        NS_ASSERTION(nsnull != newFrames, "null child list");
        numFrames = nsContainerFrame::LengthOf(newFrames);
        mAbsoluteFrames.InsertFrames(nsnull, prevSibling, newFrames);

      } else {
        NS_ASSERTION(PR_FALSE, "unexpected reflow type");
      }

      // For inserted and appended reflow commands we need to reflow the
      // newly added frames
      if ((nsIReflowCommand::FrameAppended == type) ||
          (nsIReflowCommand::FrameInserted == type)) {

        while (numFrames-- > 0) {
          nsReflowStatus  status;

          ReflowAbsoluteFrame(aPresContext, aReflowState, newFrames, PR_TRUE, status);
          newFrames->GetNextSibling(&newFrames);
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
      ReflowAbsoluteFrame(aPresContext, aReflowState, nextFrame, PR_FALSE, kidStatus);
      // XXX Make sure the frame is repainted. For the time being, since we
      // have no idea what actually changed repaint it all...
      nsIView*  view;
      nextFrame->GetView(&view);
      if (nsnull != view) {
        nsIViewManager* viewMgr;
        view->GetViewManager(viewMgr);
        if (nsnull != viewMgr) {
          viewMgr->UpdateView(view, (nsIRegion*)nsnull, NS_VMREFRESH_NO_SYNC);
          NS_RELEASE(viewMgr);
        }
      }
      aWasHandled = PR_TRUE;
    }
  }

  return NS_OK;
}

// XXX Optimize the case where it's a resize reflow and the absolutely
// positioned child has the exact same size and position and skip the
// reflow...
nsresult
nsAbsoluteContainingBlock::ReflowAbsoluteFrame(nsIPresContext&          aPresContext,
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

    nsSize              availSize(aReflowState.computedWidth, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowMetrics kidDesiredSize(nsnull);
    nsHTMLReflowState   kidReflowState(aPresContext, aReflowState, aKidFrame,
                                       availSize);

    // If it's the initial reflow, then override the reflow reason. This is
    // used when frames are inserted incrementally
    if (aInitialReflow) {
      kidReflowState.reason = eReflowReason_Initial;
    }

    rv = htmlReflow->Reflow(aPresContext, kidDesiredSize, kidReflowState, aStatus);

    // XXX If the child had a fixed height, then make sure it respected it...
    if (NS_AUTOHEIGHT != kidReflowState.computedHeight) {
      if (kidDesiredSize.height < kidReflowState.computedHeight) {
        kidDesiredSize.height = kidReflowState.computedHeight;
        kidDesiredSize.height += kidReflowState.mComputedBorderPadding.top +
                                 kidReflowState.mComputedBorderPadding.bottom;
      }
    }
    
    // Position the child relative to our padding edge
    nsRect  rect(border.left + kidReflowState.computedOffsets.left + kidReflowState.computedMargin.left,
                 border.top + kidReflowState.computedOffsets.top + kidReflowState.computedMargin.top,
                 kidDesiredSize.width, kidDesiredSize.height);
    aKidFrame->SetRect(rect);
  }

  return rv;
}

nsresult
nsAbsoluteContainingBlock::GetPositionedInfo(nscoord& aXMost, nscoord& aYMost) const
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

