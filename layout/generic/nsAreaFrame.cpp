/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsAreaFrame.h"
#include "nsBlockBandData.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIView.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLValue.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"
#include "nsISizeOfHandler.h"

#undef NOISY_MAX_ELEMENT_SIZE
#undef NOISY_SPACEMANAGER
#undef NOISY_FINAL_SIZE

nsresult
NS_NewAreaFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsAreaFrame* it = new (aPresShell) nsAreaFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
}

nsAreaFrame::nsAreaFrame()
{
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_IMETHODIMP
nsAreaFrame::Destroy(nsIPresContext* aPresContext)
{
  mAbsoluteContainer.DestroyFrames(this, aPresContext);
  return nsBlockFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsAreaFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.SetInitialChildList(this, aPresContext, aListName, aChildList);
  } else {
    rv = nsBlockFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.AppendFrames(this, aPresContext, aPresShell, aListName,
                                         aFrameList);
  } else {
    rv = nsBlockFrame::AppendFrames(aPresContext, aPresShell, aListName,
                                    aFrameList);
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.InsertFrames(this, aPresContext, aPresShell, aListName,
                                         aPrevFrame, aFrameList);
  } else {
    rv = nsBlockFrame::InsertFrames(aPresContext, aPresShell, aListName,
                                    aPrevFrame, aFrameList);
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.RemoveFrame(this, aPresContext, aPresShell, aListName, aOldFrame);
  } else {
    rv = nsBlockFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
  }

  return rv;
}

NS_IMETHODIMP
nsAreaFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  if (aIndex <= NS_BLOCK_FRAME_LAST_LIST_INDEX) {
    return nsBlockFrame::GetAdditionalChildListName(aIndex, aListName);
  }
  
  *aListName = nsnull;
  if (NS_AREA_FRAME_ABSOLUTE_LIST_INDEX == aIndex) {
    *aListName = nsLayoutAtoms::absoluteList;
    NS_ADDREF(*aListName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAreaFrame::FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  if (aListName == nsLayoutAtoms::absoluteList) {
    return mAbsoluteContainer.FirstChild(this, aListName, aFirstChild);
  }

  return nsBlockFrame::FirstChild(aPresContext, aListName, aFirstChild);
}

static void
CalculateContainingBlock(const nsHTMLReflowState& aReflowState,
                         nscoord                  aFrameWidth,
                         nscoord                  aFrameHeight,
                         nscoord&                 aContainingBlockWidth,
                         nscoord&                 aContainingBlockHeight)
{
  aContainingBlockWidth = -1;  // have reflow state calculate
  aContainingBlockHeight = -1; // have reflow state calculate

  // The issue there is that for a 'height' of 'auto' the reflow state code
  // won't know how to calculate the containing block height because it's
  // calculated bottom up. We don't really want to do this for the initial
  // containing block so that's why we have the check for if the element
  // is absolutely or relatively positioned
  if (aReflowState.mStylePosition->IsAbsolutelyPositioned() ||
      (NS_STYLE_POSITION_RELATIVE == aReflowState.mStylePosition->mPosition)) {
    aContainingBlockWidth = aFrameWidth;
    aContainingBlockHeight = aFrameHeight;

    // Containing block is relative to the padding edge
    nsMargin  border;
    if (!aReflowState.mStyleSpacing->GetBorder(border)) {
      NS_NOTYETIMPLEMENTED("percentage border");
    }
    aContainingBlockWidth -= border.left + border.right;
    aContainingBlockHeight -= border.top + border.bottom;
  }
}

NS_IMETHODIMP
nsAreaFrame::Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsAreaFrame", aReflowState.reason);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsAreaFrame::Reflow: maxSize=%d,%d reason=%d",
                  aReflowState.availableWidth,
                  aReflowState.availableHeight,
                  aReflowState.reason));

  nsresult  rv = NS_OK;

  // See if it's an incremental reflow command
  if (eReflowReason_Incremental == aReflowState.reason) {
    // Give the absolute positioning code a chance to handle it
    nscoord containingBlockWidth;
    nscoord containingBlockHeight;
    PRBool  handled;
    nsRect  childBounds;

    CalculateContainingBlock(aReflowState, mRect.width, mRect.height,
                             containingBlockWidth, containingBlockHeight);
    
    mAbsoluteContainer.IncrementalReflow(this, aPresContext, aReflowState,
                                         containingBlockWidth, containingBlockHeight,
                                         handled, childBounds);

    // If the incremental reflow command was handled by the absolute positioning
    // code, then we're all done
    if (handled) {
      // Just return our current size as our desired size.
      // XXX We need to know the overflow area for the flowed content, and
      // we don't have a way to get that currently so for the time being pretend
      // a resize reflow occured
#if 0
      aMetrics.width = mRect.width;
      aMetrics.height = mRect.height;
      aMetrics.ascent = mRect.height;
      aMetrics.descent = 0;
  
      // Whether or not we're complete hasn't changed
      aStatus = (nsnull != mNextInFlow) ? NS_FRAME_NOT_COMPLETE : NS_FRAME_COMPLETE;
#else
      nsHTMLReflowState reflowState(aReflowState);
      reflowState.reason = eReflowReason_Resize;
      reflowState.reflowCommand = nsnull;
      rv = nsBlockFrame::Reflow(aPresContext, aMetrics, reflowState, aStatus);
#endif
      
      // Factor the absolutely positioned child bounds into the overflow area
      aMetrics.mOverflowArea.UnionRect(aMetrics.mOverflowArea, childBounds);

      // Make sure the NS_FRAME_OUTSIDE_CHILDREN flag is set correctly
      if ((aMetrics.mOverflowArea.x < 0) ||
          (aMetrics.mOverflowArea.y < 0) ||
          (aMetrics.mOverflowArea.XMost() > aMetrics.width) ||
          (aMetrics.mOverflowArea.YMost() > aMetrics.height)) {
        mState |= NS_FRAME_OUTSIDE_CHILDREN;
      } else {
        mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
      }
      return rv;
    }
  }

  // Let the block frame do its reflow first
  rv = nsBlockFrame::Reflow(aPresContext, aMetrics, aReflowState, aStatus);

  // Let the absolutely positioned container reflow any absolutely positioned
  // child frames that need to be reflowed, e.g., elements with a percentage
  // based width/height
  if (NS_SUCCEEDED(rv)) {
    nscoord containingBlockWidth;
    nscoord containingBlockHeight;
    nsRect  childBounds;

    CalculateContainingBlock(aReflowState, aMetrics.width, aMetrics.height,
                             containingBlockWidth, containingBlockHeight);

    rv = mAbsoluteContainer.Reflow(this, aPresContext, aReflowState,
                                   containingBlockWidth, containingBlockHeight,
                                   childBounds);

    // Factor the absolutely positioned child bounds into the overflow area
    aMetrics.mOverflowArea.UnionRect(aMetrics.mOverflowArea, childBounds);

    // Make sure the NS_FRAME_OUTSIDE_CHILDREN flag is set correctly
    if ((aMetrics.mOverflowArea.x < 0) ||
        (aMetrics.mOverflowArea.y < 0) ||
        (aMetrics.mOverflowArea.XMost() > aMetrics.width) ||
        (aMetrics.mOverflowArea.YMost() > aMetrics.height)) {
      mState |= NS_FRAME_OUTSIDE_CHILDREN;
    } else {
      mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
    }
  }

#ifdef NOISY_MAX_ELEMENT_SIZE
  ListTag(stdout);
  printf(": maxElementSize=%d,%d desiredSize=%d,%d\n",
         aMetrics.maxElementSize ? aMetrics.maxElementSize->width : 0,
         aMetrics.maxElementSize ? aMetrics.maxElementSize->height : 0,
         aMetrics.width, aMetrics.height);
#endif

  return rv;
}
  
NS_IMETHODIMP
nsAreaFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
  if (aChild) {
    // See if the child is absolutely positioned
    nsFrameState  childState;
    aChild->GetFrameState(&childState);
    if (childState & NS_FRAME_OUT_OF_FLOW) {
      const nsStylePosition*  position;
      aChild->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);

      if (position->IsAbsolutelyPositioned()) {
        // Generate a reflow command to reflow our dirty absolutely
        // positioned child frames.
        // XXX Note that we don't currently try and coalesce the reflow commands,
        // although we should. We can't use the NS_FRAME_HAS_DIRTY_CHILDREN
        // flag, because that's used to indicate whether in-flow children are
        // dirty...
        nsIReflowCommand* reflowCmd;
        nsresult          rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                                       nsIReflowCommand::ReflowDirty);
        if (NS_SUCCEEDED(rv)) {
          reflowCmd->SetChildListName(nsLayoutAtoms::absoluteList);
          aPresShell->AppendReflowCommand(reflowCmd);
          NS_RELEASE(reflowCmd);
        }
        return rv;
      }
    }
  }

  return nsBlockFrame::ReflowDirtyChild(aPresShell, aChild);
}

NS_IMETHODIMP
nsAreaFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::areaFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Diagnostics

#ifdef NS_DEBUG
NS_IMETHODIMP
nsAreaFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Area", aResult);
}

NS_IMETHODIMP
nsAreaFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsBlockFrame::SizeOf(aHandler, aResult);
  *aResult += sizeof(*this) - sizeof(nsBlockFrame);

  return NS_OK;
}
#endif
