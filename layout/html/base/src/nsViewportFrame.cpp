/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLIIDs.h"
#include "nsLayoutAtoms.h"
#include "nsIViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsIDeviceContext.h"
#include "nsIPresContext.h"
#include "nsIReflowCommand.h"
#include "nsIPresShell.h"


/**
 * Viewport frame class.
 *
 * The viewport frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be one of the
 * following:
 * - scroll frame
 * - root frame
 *
 * The viewport frame has an additional named child list:
 * - "Fixed-list" which contains the fixed positioned frames
 *
 * @see nsLayoutAtoms::fixedList
 */
class ViewportFrame : public nsContainerFrame {
public:
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);
  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;

  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  
  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::viewportFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
  nsresult IncrementalReflow(nsIPresContext*          aPresContext,
                             const nsHTMLReflowState& aReflowState);

  nsresult ReflowFixedFrame(nsIPresContext*          aPresContext,
                            const nsHTMLReflowState& aReflowState,
                            nsIFrame*                aKidFrame,
                            PRBool                   aInitialReflow,
                            nsReflowStatus&          aStatus) const;

  void ReflowFixedFrames(nsIPresContext*          aPresContext,
                         const nsHTMLReflowState& aReflowState) const;

  void CalculateFixedContainingBlockSize(nsIPresContext*          aPresContext,
                                         const nsHTMLReflowState& aReflowState,
                                         nscoord&                 aWidth,
                                         nscoord&                 aHeight) const;

private:
  nsFrameList mFixedFrames;  // additional named child list
};

//----------------------------------------------------------------------

nsresult
NS_NewViewportFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  ViewportFrame* it = new (aPresShell) ViewportFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::Destroy(nsIPresContext* aPresContext)
{
  mFixedFrames.DestroyFrames(aPresContext);
  return nsContainerFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
ViewportFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv;

  // See which child list to add the frames to
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  if (nsLayoutAtoms::fixedList == aListName) {
    mFixedFrames.SetFrames(aChildList);
    rv = NS_OK;

  } else {
    rv = nsContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint, 
                                   nsFramePaintLayer aWhichLayer,
                                   nsIFrame**     aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}

NS_IMETHODIMP
ViewportFrame::AppendFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  nsresult  rv;
  
  NS_PRECONDITION(nsLayoutAtoms::fixedList == aListName, "unexpected child list");
  if (aListName != nsLayoutAtoms::fixedList) {
    // We only expect incremental changes for our fixed frames
    rv = NS_ERROR_INVALID_ARG;

  } else {
    // Add the frames to our list of fixed position frames
  #ifdef NS_DEBUG
    nsFrame::VerifyDirtyBitSet(aFrameList);
  #endif
    mFixedFrames.AppendFrames(nsnull, aFrameList);
    
    // Generate a reflow command to reflow the dirty frames
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this, nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      reflowCmd->SetChildListName(nsLayoutAtoms::fixedList);
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList)
{
  nsresult  rv;

  NS_PRECONDITION(nsLayoutAtoms::fixedList == aListName, "unexpected child list");
  if (aListName != nsLayoutAtoms::fixedList) {
    // We only expect incremental changes for our fixed frames
    rv = NS_ERROR_INVALID_ARG;

  } else {
    // Insert the new frames
#ifdef NS_DEBUG
    nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
    mFixedFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  
    // Generate a reflow command to reflow the dirty frames
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this, nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      reflowCmd->SetChildListName(nsLayoutAtoms::fixedList);
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame)
{
  nsresult  rv;

  NS_PRECONDITION(nsLayoutAtoms::fixedList == aListName, "unexpected child list");
  if (aListName != nsLayoutAtoms::fixedList) {
    // We only expect incremental changes for our fixed frames
    rv = NS_ERROR_INVALID_ARG;

  } else {
    PRBool result = mFixedFrames.DestroyFrame(aPresContext, aOldFrame);
    NS_ASSERTION(result, "didn't find frame to delete");
    // Because fixed frames aren't part of a flow, there's no additional
    // work to do, e.g. reflowing sibling frames. And because fixed frames
    // have a view, we don't need to repaint
    rv = result ? NS_OK : NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                          nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  *aListName = nsnull;

  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;

  } else if (0 == aIndex) {
    *aListName = nsLayoutAtoms::fixedList;
    NS_ADDREF(*aListName);
  }

  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::FirstChild(nsIPresContext* aPresContext,
                          nsIAtom*        aListName,
                          nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  if (aListName == nsLayoutAtoms::fixedList) {
    *aFirstChild = mFixedFrames.FirstChild();
    return NS_OK;
  }

  return nsContainerFrame::FirstChild(aPresContext, aListName, aFirstChild);
}

void
ViewportFrame::CalculateFixedContainingBlockSize(nsIPresContext*          aPresContext,
                                                 const nsHTMLReflowState& aReflowState,
                                                 nscoord&                 aWidth,
                                                 nscoord&                 aHeight) const
{
  // Initialize the values to the computed width/height in our reflow
  // state struct
  aWidth = aReflowState.mComputedWidth;
  aHeight = aReflowState.mComputedHeight;

  // Get our prinicpal child frame and see if we're scrollable
  nsIFrame* kidFrame = mFrames.FirstChild();
  nsCOMPtr<nsIScrollableFrame> scrollingFrame(do_QueryInterface(kidFrame));

  if (scrollingFrame) {
    nscoord sbWidth = 0, sbHeight = 0;
    PRBool sbHVisible = PR_FALSE, sbVVisible = PR_FALSE;
    scrollingFrame->GetScrollbarSizes(aPresContext, &sbWidth, &sbHeight);
    scrollingFrame->GetScrollbarVisibility(aPresContext, &sbVVisible, &sbHVisible);
    if (sbVVisible) {
      aWidth -= sbWidth;
    }
    if (sbHVisible) {
      aHeight -= sbHeight;
    }
  }
}

/**
 * NOTE: nsAbsoluteContainingBlock::ReflowAbsoluteFrame is rather
 * similar to this function, so changes made here may also need to be
 * made there.
 */
nsresult
ViewportFrame::ReflowFixedFrame(nsIPresContext*          aPresContext,
                                const nsHTMLReflowState& aReflowState,
                                nsIFrame*                aKidFrame,
                                PRBool                   aInitialReflow,
                                nsReflowStatus&          aStatus) const
{
  // Reflow the frame
  nsresult  rv;

  nsHTMLReflowMetrics kidDesiredSize(nsnull);
  nsSize              availSize(aReflowState.availableWidth, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState   kidReflowState(aPresContext, aReflowState, aKidFrame,
                                     availSize);
    
  // If it's the initial reflow, then override the reflow reason. This is
  // used when frames are inserted incrementally
  if (aInitialReflow) {
    kidReflowState.reason = eReflowReason_Initial;
  }
    
  // Send the WillReflow() notification and position the frame
  aKidFrame->WillReflow(aPresContext);
  aKidFrame->MoveTo(aPresContext, 
                    kidReflowState.mComputedOffsets.left + kidReflowState.mComputedMargin.left,
                    kidReflowState.mComputedOffsets.top + kidReflowState.mComputedMargin.top);
  
  // Position its view
  nsContainerFrame::PositionFrameView(aPresContext, aKidFrame);

  // Do the reflow
  rv = aKidFrame->Reflow(aPresContext, kidDesiredSize, kidReflowState, aStatus);

  if (NS_AUTOOFFSET == kidReflowState.mComputedOffsets.left)
    kidReflowState.mComputedOffsets.left = aReflowState.mComputedWidth -
        kidReflowState.mComputedOffsets.right -
        kidReflowState.mComputedMargin.right -
        kidReflowState.mComputedBorderPadding.right -
        kidDesiredSize.width -
        kidReflowState.mComputedBorderPadding.left -
        kidReflowState.mComputedMargin.left;
  if (NS_AUTOOFFSET == kidReflowState.mComputedOffsets.top)
    kidReflowState.mComputedOffsets.top = aReflowState.mComputedHeight -
        kidReflowState.mComputedOffsets.bottom -
        kidReflowState.mComputedMargin.bottom -
        kidReflowState.mComputedBorderPadding.bottom -
        kidDesiredSize.height -
        kidReflowState.mComputedBorderPadding.top -
        kidReflowState.mComputedMargin.top;

  // XXX If the child had a fixed height, then make sure it respected it...
  if (NS_AUTOHEIGHT != kidReflowState.mComputedHeight) {
    if (kidDesiredSize.height < kidReflowState.mComputedHeight) {
      kidDesiredSize.height = kidReflowState.mComputedHeight;
      kidDesiredSize.height += kidReflowState.mComputedBorderPadding.top +
                               kidReflowState.mComputedBorderPadding.bottom;
    }
  }

  // Position the child
  nsRect  rect(kidReflowState.mComputedOffsets.left + kidReflowState.mComputedMargin.left,
               kidReflowState.mComputedOffsets.top + kidReflowState.mComputedMargin.top,
               kidDesiredSize.width, kidDesiredSize.height);
  aKidFrame->SetRect(aPresContext, rect);

  // Size and position the view and set its opacity, visibility, content
  // transparency, and clip
  nsIView* kidView;
  aKidFrame->GetView(aPresContext, &kidView);
  nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, aKidFrame, kidView,
                                             &kidDesiredSize.mOverflowArea);
  aKidFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
  return rv;
}

// Called by Reflow() to reflow all of the fixed positioned child frames.
// This is only done for 'initial', 'resize', and 'style change' reflow commands
void
ViewportFrame::ReflowFixedFrames(nsIPresContext*          aPresContext,
                                 const nsHTMLReflowState& aReflowState) const
{
  // Calculate how much room is available for the fixed items. That means
  // determining if the viewport is scrollable and whether the vertical and/or
  // horizontal scrollbars are visible
  nscoord width, height;
  CalculateFixedContainingBlockSize(aPresContext, aReflowState, width, height);

  // Make a copy of the reflow state and change the computed width and height
  // to reflect the available space for the fixed items
  // XXX Find a cleaner way to do this...
  nsHTMLReflowState reflowState(aReflowState);
  reflowState.mComputedWidth = width;
  reflowState.mComputedHeight = height;

  nsIFrame* kidFrame;
  for (kidFrame = mFixedFrames.FirstChild(); nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
    // Reflow the frame using our reflow reason
    nsReflowStatus  kidStatus;
    ReflowFixedFrame(aPresContext, reflowState, kidFrame, PR_FALSE, kidStatus);
  }
}

/**
 * Called by Reflow() to handle the case where it's an incremental reflow
 * of a fixed child frame
 */
nsresult
ViewportFrame::IncrementalReflow(nsIPresContext*          aPresContext,
                                 const nsHTMLReflowState& aReflowState)
{
  nsIReflowCommand::ReflowType  type;

  // Get the type of reflow command
  aReflowState.reflowCommand->GetType(type);

  // The only type of reflow command we expect is that we have dirty
  // child frames to reflow
  NS_ASSERTION(nsIReflowCommand::ReflowDirty, "unexpected reflow type");
  
  // Calculate how much room is available for the fixed items. That means
  // determining if the viewport is scrollable and whether the vertical and/or
  // horizontal scrollbars are visible
  nscoord width, height;
  CalculateFixedContainingBlockSize(aPresContext, aReflowState, width, height);
  
  // Make a copy of the reflow state and change the computed width and height
  // to reflect the available space for the fixed items
  // XXX Find a cleaner way to do this...
  nsHTMLReflowState reflowState(aReflowState);
  reflowState.mComputedWidth = width;
  reflowState.mComputedHeight = height;
  
  // Walk the fixed child frames and reflow the dirty frames
  for (nsIFrame* f = mFixedFrames.FirstChild(); f; f->GetNextSibling(&f)) {
    nsFrameState  frameState;

    f->GetFrameState(&frameState);
    if (frameState & NS_FRAME_IS_DIRTY) {
      nsReflowStatus  status;

      // Note: the only reason the frame would be dirty would be if it had
      // just been inserted or appended
      NS_ASSERTION(frameState & NS_FRAME_FIRST_REFLOW, "unexpected frame state");
      ReflowFixedFrame(aPresContext, reflowState, f, PR_TRUE, status);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::Reflow(nsIPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("ViewportFrame", aReflowState.reason);
  DISPLAY_REFLOW(this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("ViewportFrame::Reflow");
  NS_PRECONDITION(nsnull == aDesiredSize.maxElementSize, "unexpected request");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  nsIFrame* nextFrame = nsnull;
  PRBool    isHandled = PR_FALSE;
  
  nsIReflowCommand::ReflowType reflowType = nsIReflowCommand::ContentChanged;
  if (aReflowState.reflowCommand) {
    aReflowState.reflowCommand->GetType(reflowType);
  }
  if (reflowType == nsIReflowCommand::UserDefined) {
    // Reflow the fixed frames to account for changed scrolled area size
    ReflowFixedFrames(aPresContext, aReflowState);
    isHandled = PR_TRUE;

    // Otherwise check for an incremental reflow
  } else if (eReflowReason_Incremental == aReflowState.reason) {
    // See if we're the target frame
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      nsIAtom*  listName;
      PRBool    isFixedChild;
      
      // It's targeted at us. It better be for the fixed child list
      aReflowState.reflowCommand->GetChildListName(listName);
      isFixedChild = nsLayoutAtoms::fixedList == listName;
      NS_IF_RELEASE(listName);
      NS_ASSERTION(isFixedChild, "unexpected child list");
      
      if (isFixedChild) {
        // Handle the incremental reflow command
        IncrementalReflow(aPresContext, aReflowState);
        isHandled = PR_TRUE;
      }

    } else {
      // Get the next frame in the reflow chain
      aReflowState.reflowCommand->GetNext(nextFrame);
    }
  }

  nsRect kidRect(0,0,aReflowState.availableWidth,aReflowState.availableHeight);

  if (!isHandled) {
    if ((eReflowReason_Incremental == aReflowState.reason) &&
        (mFixedFrames.ContainsFrame(nextFrame))) {
      // Reflow the 'fixed' frame that's the next frame in the reflow path
      nsReflowStatus  kidStatus;
      ReflowFixedFrame(aPresContext, aReflowState, nextFrame, PR_FALSE,
                       kidStatus);

    } else {
      // Reflow our one and only principal child frame
      if (mFrames.NotEmpty()) {
        nsIFrame*           kidFrame = mFrames.FirstChild();
        nsHTMLReflowMetrics kidDesiredSize(nsnull);
        nsSize              availableSpace(aReflowState.availableWidth,
                                           aReflowState.availableHeight);
        nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                           kidFrame, availableSpace);

        // Reflow the frame
        kidReflowState.mComputedHeight = aReflowState.availableHeight;
        ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                    0, 0, 0, aStatus);
        kidRect.width = kidDesiredSize.width;
        kidRect.height = kidDesiredSize.height;

        FinishReflowChild(kidFrame, aPresContext, kidDesiredSize, 0, 0, 0);
      }

      // If it's a 'initial', 'resize', or 'style change' reflow command (anything
      // but incremental), then reflow all the fixed positioned child frames
      if (eReflowReason_Incremental != aReflowState.reason) {
        ReflowFixedFrames(aPresContext, aReflowState);
      }
    }
  }

  // If we were flowed initially at both an unconstrained width and height, 
  // this is a hint that we should return our child's intrinsic size.
  if ((eReflowReason_Initial == aReflowState.reason ||
       eReflowReason_Resize == aReflowState.reason) &&
      aReflowState.availableWidth == NS_UNCONSTRAINEDSIZE &&
      aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE) {
    aDesiredSize.width = kidRect.width;
    aDesiredSize.height = kidRect.height;
    aDesiredSize.ascent = kidRect.height;
    aDesiredSize.descent = 0;
  }
  else {
    // Return the max size as our desired size
    aDesiredSize.width = aReflowState.availableWidth;
    aDesiredSize.height = aReflowState.availableHeight;
    aDesiredSize.ascent = aReflowState.availableHeight;
    aDesiredSize.descent = 0;
  }

  // If this is an initial reflow, resize reflow, or style change reflow
  // then do a repaint
  if ((eReflowReason_Initial == aReflowState.reason) ||
      (eReflowReason_Resize == aReflowState.reason) ||
      (eReflowReason_StyleChange == aReflowState.reason)) {
    nsRect  damageRect(0, 0, aDesiredSize.width, aDesiredSize.height);
    if (!damageRect.IsEmpty()) {
      Invalidate(aPresContext, damageRect, PR_FALSE);
    }
  }

  NS_FRAME_TRACE_REFLOW_OUT("ViewportFrame::Reflow", aStatus);
  return NS_OK;
}

NS_IMETHODIMP
ViewportFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::viewportFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
ViewportFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Viewport"), aResult);
}

NS_IMETHODIMP
ViewportFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = sizeof(*this);
  return NS_OK;
}
#endif
