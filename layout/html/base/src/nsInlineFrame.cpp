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
#include "nsFrameReflowState.h"
#include "nsInlineReflow.h"
#include "nsIHTMLReflow.h"
#include "nsLineLayout.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"

#include "nsIPresContext.h"
#include "nsIReflowCommand.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"


struct nsInlineReflowState : nsFrameReflowState {
  nsInlineReflowState(nsIPresContext& aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      const nsHTMLReflowMetrics& aMetrics);
  ~nsInlineReflowState();

  // Last child we have reflowed (so far)
  nsIFrame* mLastChild;
};

nsInlineReflowState::nsInlineReflowState(nsIPresContext& aPresContext,
                                         const nsHTMLReflowState& aReflowState,
                                         const nsHTMLReflowMetrics& aMetrics)
  : nsFrameReflowState(aPresContext, aReflowState, aMetrics),
    mLastChild(nsnull)
{
}

nsInlineReflowState::~nsInlineReflowState()
{
}

//----------------------------------------------------------------------

class nsInlineFrame : public nsHTMLContainerFrame
{
public:
  // nsIFrame
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aCX,
                                   nsIFrame* aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*& aContinuingFrame);
  NS_IMETHOD GetFrameName(nsString& aResult) const;

  // nsIHTMLReflow
  NS_IMETHOD FindTextRuns(nsLineLayout& aLineLayout);
  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  NS_IMETHOD AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace);
  NS_IMETHOD TrimTrailingWhiteSpace(nsIPresContext& aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth);
  NS_IMETHOD MoveInSpaceManager(nsIPresContext& aPresContext,
                                nsISpaceManager* aSpaceManager,
                                nscoord aDeltaX, nscoord aDeltaY);

  virtual PRIntn GetSkipSides() const;

  PRBool CalculateMargins(nsInlineReflowState& aState,
                          nsInlineReflow& aInlineReflow,
                          nscoord& aTopMarginResult,
                          nscoord& aBottomMarginResult);

  void SlideFrames(nsIFrame* aKid, nscoord aDY);

  nsresult InitialReflow(nsInlineReflowState& aState,
                         nsInlineReflow& aInlineReflow);

  nsresult FrameAppendedReflow(nsInlineReflowState& aState,
                               nsInlineReflow& aInlineReflow);

  nsReflowStatus FrameRemovedReflow(nsInlineReflowState& aState,
                                    nsInlineReflow& aInlineReflow);

  nsresult ChildIncrementalReflow(nsInlineReflowState& aState,
                                  nsInlineReflow& aInlineReflow);

  nsresult ResizeReflow(nsInlineReflowState& aState,
                        nsInlineReflow& aInlineReflow);

  void ComputeFinalSize(nsInlineReflowState& aState,
                        nsInlineReflow& aInlineReflow,
                        nsHTMLReflowMetrics& aMetrics);

  PRBool ReflowMapped(nsInlineReflowState& aState,
                      nsInlineReflow& aInlineReflow,
                      nsReflowStatus& aReflowStatus);

  PRBool ReflowFrame(nsInlineReflowState& aState,
                     nsInlineReflow& aInlineReflow,
                     nsIFrame* aFrame,
                     nsReflowStatus& aReflowStatus);

  nsReflowStatus PullUpChildren(nsInlineReflowState& aState,
                                nsInlineReflow& aInlineReflow);

  nsIFrame* PullOneChild(nsIPresContext& aPresContext,
                         nsInlineFrame* aNextInFlow,
                         nsIFrame* aLastChild,
                         nsReflowStatus& aReflowStatus);

  void PushKids(nsInlineReflowState& aState,
                nsInlineReflow& aInlineReflow,
                nsIFrame* aPushedChild);

  void DrainOverflowLists(nsIPresContext& aPresContext);

  nsresult AppendNewFrames(nsIPresContext& aPresContext, nsIFrame*);

  nsresult InsertNewFrame(nsIPresContext& aPresContext,
                          nsIFrame*       aNewFrame,
                          nsIFrame*       aPrevSibling);

  PRBool SafeToPull(nsIFrame* aFrame);

  friend nsresult NS_NewInlineFrame(nsIFrame*& aNewFrame);

  struct AdjustData {
    nsIFrame* frame;
    PRBool splittable;
    nsRect bounds;
  };

};

//----------------------------------------------------------------------

nsresult NS_NewInlineFrame(nsIFrame*& aNewFrame)
{
  aNewFrame = new nsInlineFrame;
  if (nsnull == aNewFrame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

PRIntn
nsInlineFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_LEFT;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_RIGHT;
  }
  return skip;
}

nsresult
nsInlineFrame::AppendNewFrames(nsIPresContext& aPresContext,
                               nsIFrame* aNewFrame)
{
  // Walk the list of new frames, and see if we need to move any of them
  // out of the flow
  nsIFrame* prevFrame = nsnull;
  for (nsIFrame* frame = aNewFrame; nsnull != frame; frame->GetNextSibling(frame)) {
    const nsStyleDisplay*  kidDisplay;
    const nsStylePosition* kidPosition;
    frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)kidDisplay);
    frame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&) kidPosition);

    // XXX encapsulte the move-out-of-flow and CreateViewForFrame
    nsIFrame* placeholder;
    if (MoveFrameOutOfFlow(aPresContext, frame, kidDisplay, kidPosition, placeholder)) {
      // Reset the previous frame's next sibling pointer
      if (nsnull == prevFrame) {
        aNewFrame = placeholder;
      } else {
        prevFrame->SetNextSibling(placeholder);
      }
      frame = placeholder;
    }
    else {
      // Wrap the frame in a view if necessary
      nsIStyleContext* kidSC;
      frame->GetStyleContext(kidSC);
      nsresult rv = CreateViewForFrame(aPresContext, frame, kidSC, PR_FALSE);
      NS_RELEASE(kidSC);
      if (NS_OK != rv) {
        return rv;
      }
    }

    prevFrame = frame;
  }

  // Append the new frames to the child list
  nsIFrame* lastFrame;
  if (nsnull == mFirstChild) {
    lastFrame = nsnull;
    mFirstChild = aNewFrame;
  } else {
    lastFrame = LastFrame(mFirstChild);
    lastFrame->SetNextSibling(aNewFrame);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  NS_PRECONDITION(nsnull == mFirstChild, "already initialized");
  nsresult rv = AppendNewFrames(aPresContext, aChildList);  
  if (NS_OK != rv) {
    return rv;
  }
  return rv;
}

NS_IMETHODIMP
nsInlineFrame::CreateContinuingFrame(nsIPresContext&  aCX,
                                     nsIFrame*        aParent,
                                     nsIStyleContext* aStyleContext,
                                     nsIFrame*&       aContinuingFrame)
{
  nsInlineFrame* cf = new nsInlineFrame;
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  cf->Init(aCX, mContent, aParent, aStyleContext);
  cf->AppendToFlow(this);
  aContinuingFrame = cf;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsInlineFrame::CreateContinuingFrame: newFrame=%p", cf));
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::FindTextRuns(nsLineLayout& aLineLayout)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsInlineFrame::FindTextRuns"));

  // Gather up children from the overflow lists
  nsresult rv = NS_OK;
  DrainOverflowLists(aLineLayout.mPresContext);

  // Ask each child frame for its text runs
  nsIFrame* frame = mFirstChild;
  while (nsnull != frame) {
    nsIHTMLReflow* hr;
    if (NS_OK == frame->QueryInterface(kIHTMLReflowIID, (void**)&hr)) {
      rv = hr->FindTextRuns(aLineLayout);
      if (NS_OK != rv) {
        return rv;
      }
    }
    else {
      // A frame that doesn't implement nsIHTMLReflow isn't text
      // therefore it will end an open text run.
      aLineLayout.EndTextRun();
    }
    frame->GetNextSibling(frame);
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsInlineFrame::FindTextRuns rv=%x", rv));
  return rv;
}

// XXX This code is *almost* identical to the code in
// nsInlineReflow::JustifyFrames; factor it somehow
NS_IMETHODIMP
nsInlineFrame::AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace)
{
  if (0 >= aExtraSpace) {
    aUsedSpace = 0;
    return NS_OK;
  }

  const int NUM_AD = 50;
  AdjustData adjustMem[NUM_AD];
  AdjustData* ad0 = adjustMem;
  AdjustData* end = ad0 + NUM_AD;
  AdjustData* ad = ad0;
  PRInt32 numAD = NUM_AD;

  // Gather up raw data for justification
  nsIFrame* frame = mFirstChild;
  PRInt32 fixed = 0;
  PRInt32 total = 0;
  nscoord fixedWidth = 0;
  for (; nsnull != frame; ad++, total++) {
    // Grow temporary storage if we have to
    if (ad == end) {
      AdjustData* newAD = new AdjustData[numAD + numAD];
      if (nsnull == newAD) {
        if (ad0 != adjustMem) {
          delete [] ad0;
        }
        aUsedSpace = 0;
        return NS_OK;
      }
      nsCRT::memcpy(newAD, ad0, sizeof(AdjustData) * numAD);
      ad = newAD + (ad - ad0);
      if (ad0 != adjustMem) {
        delete [] ad0;
      }
      ad0 = newAD;
      end = ad0 + numAD;
      numAD = numAD + numAD;
    }

    // Record info about the frame
    ad->frame = frame;
    frame->GetRect(ad->bounds);
    nsSplittableType isSplittable = NS_FRAME_NOT_SPLITTABLE;
    frame->IsSplittable(isSplittable);
    if ((0 == ad->bounds.width) ||
        NS_FRAME_IS_NOT_SPLITTABLE(isSplittable)) {
      ad->splittable = PR_FALSE;
      fixed++;
      fixedWidth += ad->bounds.width;
    }
    else {
      ad->splittable = PR_TRUE;
    }

    // Advance to the next frame
    frame->GetNextSibling(frame);
  }

  nscoord totalUsed = 0;
  nscoord variableWidth = mRect.width - fixedWidth;
  if (variableWidth > 0) {
    // Each variable width frame gets a portion of the available extra
    // space that is proportional to the space it takes in the
    // line. The extra space is given to the frame by updating its
    // position and size. The frame is responsible for adjusting the
    // position of its contents on its own (during rendering).
    PRInt32 i, splittable = total - fixed;
    nscoord extraSpace = aExtraSpace;
    nscoord remainingExtra = extraSpace;
    nscoord dx = 0;
    float lineWidth = float(mRect.width);
    ad = ad0;
    for (i = 0; i < total; i++, ad++) {
      nsIFrame* frame = ad->frame;
      nsIHTMLReflow* ihr;
      if (NS_OK == frame->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
        nsRect r;
        if (ad->splittable && (ad->bounds.width > 0)) {
          float pctOfLine = float(ad->bounds.width) / lineWidth;
          nscoord extra = nscoord(pctOfLine * extraSpace);
          if (--splittable == 0) {
            extra = remainingExtra;
          }
          if (0 != extra) {
            nscoord used;
            ihr->AdjustFrameSize(extra, used);
            if (used < extra) {
//              frame->ListTag(); printf(": extra=%d used=%d\n", extra, used);
            }
            totalUsed += used;
            frame->GetRect(r);
            r.x += dx;
            frame->SetRect(r);
            dx += used;
            remainingExtra -= used;
          }
          else if (0 != dx) {
            frame->GetRect(r);
            r.x += dx;
            frame->SetRect(r);
          }
        }
        else if (0 != dx) {
          frame->GetRect(r);
          r.x += dx;
          frame->SetRect(r);
        }
      }
    }
  }
  mRect.width += totalUsed;
  aUsedSpace = totalUsed;

  if (ad0 != adjustMem) {
    delete [] ad0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::TrimTrailingWhiteSpace(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRC,
                                      nscoord& aDeltaWidth)
{
  nsIFrame* lastFrame = LastFrame(mFirstChild);
  if (nsnull == lastFrame) {
    aDeltaWidth = 0;
  }
  else {
    nsIHTMLReflow* ihr;
    if (NS_OK == lastFrame->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
      ihr->TrimTrailingWhiteSpace(aPresContext, aRC, aDeltaWidth);
      if (0 != aDeltaWidth) {
        mRect.width -= aDeltaWidth;
      }
    }
    else {
      aDeltaWidth = 0;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::MoveInSpaceManager(nsIPresContext& aPresContext,
                                  nsISpaceManager* aSpaceManager,
                                  nscoord aDeltaX, nscoord aDeltaY)
{
  nsIFrame* kid = mFirstChild;
  while (nsnull != kid) {
    nsIHTMLReflow* ihr;
    if (NS_OK == kid->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
      ihr->MoveInSpaceManager(aPresContext, aSpaceManager, aDeltaX, aDeltaY);
    }
    kid->GetNextSibling(kid);
  }
  return NS_OK;
}

nsresult
nsInlineFrame::InsertNewFrame(nsIPresContext& aPresContext,
                              nsIFrame*       aNewFrame,
                              nsIFrame*       aPrevSibling)
{
  const nsStyleDisplay* kidDisplay;
  aNewFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)kidDisplay);
  const nsStylePosition* kidPosition;
  aNewFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)kidPosition);

  // See if we need to move the frame outside of the flow, and insert a
  // placeholder frame in its place
  nsIFrame* placeholder;
  if (MoveFrameOutOfFlow(aPresContext, aNewFrame, kidDisplay, kidPosition, placeholder)) {
    // Add the placeholder frame to the flow
    aNewFrame = placeholder;
  }
  else {
    // Wrap the frame in a view if necessary
    nsIStyleContext* kidSC;
    aNewFrame->GetStyleContext(kidSC);
    nsresult rv = CreateViewForFrame(aPresContext, aNewFrame, kidSC, PR_FALSE);
    NS_RELEASE(kidSC);
    if (NS_OK != rv) {
      return rv;
    }
  }

  // Add the new frame to the child list
  nsIFrame* nextSibling;
  if (nsnull == aPrevSibling) {
    nextSibling = mFirstChild;
    mFirstChild = aNewFrame;
  } else {
    aPrevSibling->GetNextSibling(nextSibling);
    aPrevSibling->SetNextSibling(aNewFrame);
  }
  aNewFrame->SetNextSibling(nextSibling);

  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::Reflow(nsIPresContext& aPresContext,
                      nsHTMLReflowMetrics& aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
    ("enter nsInlineFrame::InlineReflow maxSize=%d,%d reason=%d nif=%p",
     aReflowState.maxSize.width, aReflowState.maxSize.height,
     aReflowState.reason, mNextInFlow));

  // If this is the initial reflow, generate any synthetic content
  // that needs generating.
  if (eReflowReason_Initial == aReflowState.reason) {
    NS_ASSERTION(0 != (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }
  else {
    NS_ASSERTION(0 == (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }

  // Prepare for reflow
  nsInlineReflowState state(aPresContext, aReflowState, aMetrics);

  // If we're constrained adjust the available size so it excludes space
  // needed for border/padding
  nscoord width = state.maxSize.width;
  if (NS_UNCONSTRAINEDSIZE != width) {
    width -= state.mBorderPadding.left + state.mBorderPadding.right;
  }
  nscoord height = state.maxSize.height;
  if (NS_UNCONSTRAINEDSIZE != height) {
    height -= state.mBorderPadding.top + state.mBorderPadding.bottom;
  }
  const nsStyleDisplay* display;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);

  // Prepare inline reflow engine
  NS_ASSERTION(nsnull != aReflowState.lineLayout, "no line layout");
  nsInlineReflow inlineReflow(*aReflowState.lineLayout, state, this, PR_FALSE);
  inlineReflow.Init(state.mBorderPadding.left, state.mBorderPadding.top,
                    width, height);
  aReflowState.lineLayout->PushInline(&inlineReflow);

// ListTag(stdout); printf(": enter isMarginRoot=%c\n", state.mIsMarginRoot?'T':'F');
  // Now translate in by our border and padding
  NS_ASSERTION(nsnull != aReflowState.spaceManager, "no space manager");
  aReflowState.spaceManager->Translate(state.mBorderPadding.left,
                                       state.mBorderPadding.top);

  // Based on the type of reflow, switch out to the appropriate
  // routine.
  if (eReflowReason_Initial == state.reason) {
    DrainOverflowLists(aPresContext);
    aStatus = InitialReflow(state, inlineReflow);
    mState &= ~NS_FRAME_FIRST_REFLOW;
  }
  else if (eReflowReason_Incremental == state.reason) {
    // XXX For now we drain our overflow list in case our parent
    // reflowed our prev-in-flow and our prev-in-flow pushed some
    // children forward to us (e.g. a speculative pullup from us that
    // failed)
    DrainOverflowLists(aPresContext);

    NS_ASSERTION(nsnull == mOverflowList, "unexpected overflow list");
    nsIFrame* target;
    state.reflowCommand->GetTarget(target);
    if (this == target) {
      nsIReflowCommand::ReflowType type;
      state.reflowCommand->GetType(type);
      nsIFrame* newFrame;
      nsIFrame* prevSibling;
      switch (type) {
      case nsIReflowCommand::FrameAppended:
        aStatus = FrameAppendedReflow(state, inlineReflow);
        break;

      case nsIReflowCommand::FrameInserted:
        // Link the new frame into the child list
        state.reflowCommand->GetChildFrame(newFrame);
        state.reflowCommand->GetPrevSiblingFrame(prevSibling);
        InsertNewFrame(state.mPresContext, newFrame, prevSibling);
        // XXX For now map into full reflow...
        aStatus = ResizeReflow(state, inlineReflow);
        break;

      case nsIReflowCommand::FrameRemoved:
        aStatus = FrameRemovedReflow(state, inlineReflow);
        break;

      default:
        // XXX For now map the other incremental operations into full reflows
        aStatus = ResizeReflow(state, inlineReflow);
        break;
      }
    }
    else {
      // Get next frame in reflow command chain
      state.reflowCommand->GetNext(state.mNextRCFrame);

      // Continue the reflow
      aStatus = ChildIncrementalReflow(state, inlineReflow);
    }
  }
  else if ((eReflowReason_Resize == state.reason) ||
           (eReflowReason_StyleChange == state.reason)) {
    DrainOverflowLists(aPresContext);
    aStatus = ResizeReflow(state, inlineReflow);
  }
  ComputeFinalSize(state, inlineReflow, aMetrics);

  // Now translate in by our border and padding
  aReflowState.lineLayout->PopInline();
  aReflowState.spaceManager->Translate(-state.mBorderPadding.left,
                                       -state.mBorderPadding.top);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsInlineFrame::InlineReflow size=%d,%d status=%x nif=%p",
      aMetrics.width, aMetrics.height, aStatus, mNextInFlow));
  return NS_OK;
}

// XXX factor this (into nsFrameReflowState?) so that both block and
// inline can use most of the same logic

PRBool
nsInlineFrame::CalculateMargins(nsInlineReflowState& aState,
                                nsInlineReflow& aInlineReflow,
                                nscoord& aTopMarginResult,
                                nscoord& aBottomMarginResult)
{
  PRBool haveCarriedMargins = PR_FALSE;

  nscoord childsCarriedOutTopMargin = aInlineReflow.GetCarriedOutTopMargin();
  nscoord childsCarriedOutBottomMargin = aInlineReflow.GetCarriedOutBottomMargin();

  // If the inline-reflow is wrapped up around a block or we have some
  // carried out margin information then we perform margin collapsing
  // (pretending we are a block...)
  if (aInlineReflow.GetIsBlock() ||
      (0 != childsCarriedOutTopMargin) ||
      (0 != childsCarriedOutBottomMargin)) {
    // Compute the collapsed top margin value from the childs margin and
    // its carried out top margin.
    nscoord childsTopMargin = aInlineReflow.GetTopMargin();
    nscoord collapsedTopMargin =
      nsInlineReflow::MaxMargin(childsCarriedOutTopMargin, childsTopMargin);

    // If this frame is a root for margins then we will apply the
    // collapsed top margin value ourselves. Otherwise, we pass it out
    // to our parent to apply.
    if (!aState.mIsMarginRoot) {
      // We are not a root for margin collapsing and this is our first
      // non-empty-line (with a block child presumably). Keep the
      // collapsed margin value around to pass out to our parent. We
      // don't apply the margin (our parent will) so zero it out.
      aState.mCollapsedTopMargin = collapsedTopMargin;
      collapsedTopMargin = 0;
    }
    aTopMarginResult = collapsedTopMargin;

    // Compute the collapsed bottom margin value. This collapsed value
    // will end up in aState.mPrevBottomMargin if the child frame ends
    // up being placed in this block frame.
    nscoord childsBottomMargin = aInlineReflow.GetBottomMargin();
    nscoord collapsedBottomMargin =
      nsInlineReflow::MaxMargin(childsCarriedOutBottomMargin,
                                childsBottomMargin);
    aBottomMarginResult = collapsedBottomMargin;
    haveCarriedMargins = PR_TRUE;
  }
  else {
    aTopMarginResult = 0;
    aBottomMarginResult = 0;
    haveCarriedMargins = PR_FALSE;
  }
  return haveCarriedMargins;
}

void
nsInlineFrame::ComputeFinalSize(nsInlineReflowState& aState,
                                nsInlineReflow& aInlineReflow,
                                nsHTMLReflowMetrics& aMetrics)
{
  // Align our child frames
  nsRect bounds;
  nscoord maxAscent, maxDescent;
  aInlineReflow.VerticalAlignFrames(bounds, maxAscent, maxDescent);
  aInlineReflow.RelativePositionFrames(aMetrics.mCombinedArea);

  // Make sure that we collapse into nothingness if our content is
  // zero sized
  if (0 == bounds.width) {
    aMetrics.width = 0;
  }
  else {
    aMetrics.width = aState.mBorderPadding.left + bounds.width +
      aState.mBorderPadding.right;
  }

  // Compute the final height. Make sure that we collapse into
  // nothingness if our content is zero sized
  if (0 == bounds.height) {
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
    aMetrics.height = 0;
  }
  else {
// XXX tip of the iceburg: when an inline is wrapping up a block frame
// it needs to go the whole hog: provide a proper spacemanager
// coordinate system; apply horizontal alignment (in case the block
// has a width set), etc. Of course the block code knows how to do
// this already...
    if (aInlineReflow.GetIsBlock()) {
      // Make sure the blocks top and bottom margins get applied; the
      // top margin will be in bounds.y; the bottom margin we get from
      // the inline reflow.
      nscoord newY = aState.mBorderPadding.top + bounds.YMost() +
        aState.mBorderPadding.bottom;
      aMetrics.height = newY;
      aMetrics.ascent = newY;
      aMetrics.descent = 0;
    }
    else {
      aMetrics.ascent = aState.mBorderPadding.top + maxAscent;
      aMetrics.descent = maxDescent + aState.mBorderPadding.bottom;
      aMetrics.height = aMetrics.ascent + aMetrics.descent;
    }
  }

  // Return top and bottom margin information
  if (aState.mIsMarginRoot) {
    aMetrics.mCarriedOutTopMargin = 0;
    aMetrics.mCarriedOutBottomMargin = 0;
    aMetrics.mCarriedOutMarginFlags = 0;
  }
  else {
    aMetrics.mCarriedOutTopMargin = aState.mCollapsedTopMargin;
    aMetrics.mCarriedOutBottomMargin = aState.mPrevBottomMargin;
    if (aInlineReflow.GetIsBlock()) {
      aMetrics.mCarriedOutMarginFlags = aInlineReflow.GetMarginFlags();
    }
    else {
      aMetrics.mCarriedOutMarginFlags = 0;
    }
  }

  if (aState.mComputeMaxElementSize) {
    // Adjust max-element size if this frame's no-wrap flag is set.
    nscoord maxWidth;
    nscoord maxHeight;
    if (aState.mNoWrap) {
      maxWidth = aMetrics.width;
      maxHeight = aMetrics.height;
    }
    else {
      const nsSize& maxSize = aInlineReflow.GetMaxElementSize();
      maxWidth = maxSize.width;
      maxHeight = maxSize.height;
    }

    // Add in our border and padding to the max-element-size so that
    // we don't shrink too far.
    maxWidth += aState.mBorderPadding.left + aState.mBorderPadding.right;
    maxHeight += aState.mBorderPadding.top + aState.mBorderPadding.bottom;

    aMetrics.maxElementSize->width = maxWidth;
    aMetrics.maxElementSize->height = maxHeight;
  }

  // See if the combined area of our children exceeds the bounds of
  // our frame. If it does, then set the NS_FRAME_OUTSIDE_CHILDREN
  // flag.

  // XXX take into account the overflow->clip property!
  nsRect& combinedArea = aMetrics.mCombinedArea;
  if ((combinedArea.x < 0) || (combinedArea.y < 0) ||
      (combinedArea.XMost() > aMetrics.width) ||
      (combinedArea.YMost() > aMetrics.height)) {
    mState |= NS_FRAME_OUTSIDE_CHILDREN;
  }
  else {
    mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
  }
}

nsReflowStatus
nsInlineFrame::InitialReflow(nsInlineReflowState& aState,
                             nsInlineReflow& aInlineReflow)
{
  NS_PRECONDITION(nsnull == mNextInFlow, "bad frame-appended-reflow");

  nsReflowStatus rs = NS_FRAME_COMPLETE;
  if (nsnull != mFirstChild) {
    if (!ReflowMapped(aState, aInlineReflow, rs)) {
      return rs;
    }
  }
  return rs;
}

nsReflowStatus
nsInlineFrame::FrameAppendedReflow(nsInlineReflowState& aState,
                                   nsInlineReflow& aInlineReflow)
{
  NS_PRECONDITION(nsnull == mNextInFlow, "bad frame-appended-reflow");

  // Get the first of the newly appended frames
  nsIFrame* firstAppendedFrame;
  aState.reflowCommand->GetChildFrame(firstAppendedFrame);

  // Add the new frames
  AppendNewFrames(aState.mPresContext, firstAppendedFrame);

  nsReflowStatus rs = NS_FRAME_COMPLETE;
  if (nsnull != mFirstChild) {
    if (!ReflowMapped(aState, aInlineReflow, rs)) {
      return rs;
    }
  }
  return rs;
}

nsReflowStatus
nsInlineFrame::FrameRemovedReflow(nsInlineReflowState& aState,
                                  nsInlineReflow& aInlineReflow)
{
  // Get the deleted frame
  nsIFrame* deletedFrame;
  aState.reflowCommand->GetChildFrame(deletedFrame);

  // Find the previous sibling frame
  nsIFrame* prevSibling = nsnull;
  for (nsIFrame* f = mFirstChild; f != deletedFrame; f->GetNextSibling(f)) {
    if (nsnull == f) {
      // We didn't find the deleted frame in our child list
      NS_WARNING("Can't find deleted frame");
      return NS_OK;
    }

    prevSibling = f;
  }

  // Take the frame away; Note that we also have to take away any
  // continuations so we loop here until deadFrame is nsnull.
  nsInlineFrame*  flow = this;
  while (nsnull != deletedFrame) {
    // Remove frame from sibling list
    nsIFrame* nextSib;
    deletedFrame->GetNextSibling(nextSib);
    if (nsnull != prevSibling) {
      prevSibling->SetNextSibling(nextSib);
    }
    else {
      flow->mFirstChild = nextSib;
    }

    // Break frame out of its flow and then destroy it
    nsIFrame* nextInFlow;
    deletedFrame->GetNextInFlow(nextInFlow);
    deletedFrame->BreakFromNextFlow();
    deletedFrame->DeleteFrame(aState.mPresContext);
    deletedFrame = nextInFlow;

    if (nsnull != deletedFrame) {
      // Get the parent of deadFrame's continuation
      deletedFrame->GetGeometricParent((nsIFrame*&) flow);

      // When we move to a next-in-flow then the deadFrame will be the
      // first child of the new parent. Therefore we know that
      // prevSibling will be null.
      prevSibling = nsnull;
    }
  }

  // XXX For now map into full reflow...
  return ResizeReflow(aState, aInlineReflow);
}

nsReflowStatus
nsInlineFrame::ChildIncrementalReflow(nsInlineReflowState& aState,
                                      nsInlineReflow& aInlineReflow)
{
  // XXX we can do better SOMEDAY
  return ResizeReflow(aState, aInlineReflow);
}

nsReflowStatus
nsInlineFrame::ResizeReflow(nsInlineReflowState& aState,
                            nsInlineReflow& aInlineReflow)
{
  nsReflowStatus rs = NS_FRAME_COMPLETE;
  if (nsnull != mFirstChild) {
    if (!ReflowMapped(aState, aInlineReflow, rs)) {
      return rs;
    }
  }

  // Try to fit some more children from our next in flow
  if (nsnull != mNextInFlow) {
    rs = PullUpChildren(aState, aInlineReflow);
  }
  return rs;
}

PRBool
nsInlineFrame::ReflowMapped(nsInlineReflowState& aState,
                            nsInlineReflow& aInlineReflow,
                            nsReflowStatus& aReflowStatus)
{
  PRBool keepGoing = PR_TRUE;
  aState.mLastChild = nsnull;
  nsIFrame* child = mFirstChild;
  while (nsnull != child) {
    keepGoing = ReflowFrame(aState, aInlineReflow, child, aReflowStatus);
    if (!keepGoing) {
      break;
    }
    aState.mLastChild = child;
    child->GetNextSibling(child);
  }
  return keepGoing;
}

nsReflowStatus
nsInlineFrame::PullUpChildren(nsInlineReflowState& aState,
                              nsInlineReflow& aInlineReflow)
{
  nsReflowStatus reflowStatus = NS_FRAME_COMPLETE;
  nsInlineFrame* nextInFlow = (nsInlineFrame*) mNextInFlow;
  while (nsnull != nextInFlow) {
    // Get child from our next-in-flow
    nsIFrame* child = PullOneChild(aState.mPresContext,
                                   nextInFlow, aState.mLastChild,
                                   reflowStatus);
    if (nsnull == child) {
      if (NS_FRAME_NOT_COMPLETE == reflowStatus) {
        break;
      }
      nextInFlow = (nsInlineFrame*) nextInFlow->mNextInFlow;
      continue;
    }

    // Reflow the new child; if it turns out we can't keep it then
    // ReflowFrame will push it back down to the next-in-flow.
    if (!ReflowFrame(aState, aInlineReflow, child, reflowStatus)) {
      break;
    }
    aState.mLastChild = child;
  }
  return reflowStatus;
}

void
nsInlineFrame::SlideFrames(nsIFrame* aKid, nscoord aDY)
{
  while (nsnull != aKid) {
    nsRect r;
    aKid->GetRect(r);
    r.y += aDY;
    aKid->SetRect(r);
    aKid->GetNextSibling(aKid);
  }
}

PRBool
nsInlineFrame::ReflowFrame(nsInlineReflowState& aState,
                           nsInlineReflow& aInlineReflow,
                           nsIFrame* aFrame,
                           nsReflowStatus& aReflowStatus)
{
#if XXX
  PRBool firstLetterOK = aState.lineLayout->GetFirstLetterStyleOK();
#else
  // XXX Until the CSS2 spec lawyers wrangle this out, we don't allow
  // for first letter style to apply anywhere except for text which is
  // a direct descendant of a block frame.
  aState.lineLayout->SetFirstLetterStyleOK(PR_FALSE);
#endif
  PRBool justDidFirstLetter = PR_FALSE;
  aInlineReflow.SetIsFirstChild(aFrame == mFirstChild);
  aReflowStatus = aInlineReflow.ReflowFrame(aFrame);
  if (NS_IS_REFLOW_ERROR(aReflowStatus)) {
    return PR_FALSE;
  }
#if XXX
  if (!aState.lineLayout->GetFirstLetterStyleOK() && firstLetterOK) {
    justDidFirstLetter = PR_TRUE;
  }
#endif

  // XXX temporary code until blocks can return break-after naturally
  if (aInlineReflow.GetIsBlock()) {
    if (!NS_INLINE_IS_BREAK(aReflowStatus) &&
        NS_FRAME_IS_COMPLETE(aReflowStatus)) {
      // Force reflow status to be what the block code should have returned
      aReflowStatus = NS_INLINE_LINE_BREAK_AFTER(NS_FRAME_COMPLETE);
    }
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("nsInlineFrame::ReflowMapped: frame=%p reflowStatus=%x",
                  aFrame, aReflowStatus));

  if (NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    // When breaking before a frame there is no point in creating the
    // continuation because it may not be needed when the frame is
    // reflowed in it's new location.
    if (aFrame != mFirstChild) {
      // We have other children before the child that needs the
      // break-before. Therefore map the break-before from the child
      // into a break-after for us, preserving the break-type in the
      // process. This is important when the break type is not just a
      // simple line break.
      aReflowStatus = NS_FRAME_NOT_COMPLETE | NS_INLINE_BREAK |
        NS_INLINE_BREAK_AFTER | (NS_INLINE_BREAK_TYPE_MASK & aReflowStatus);
      PushKids(aState, aInlineReflow, aFrame);
    }
    return PR_FALSE;
  }

  // Apply collapsing block margins. However, we only do this if a
  // carried out margin was present.
  nscoord topMargin, bottomMargin;
  if (CalculateMargins(aState, aInlineReflow, topMargin, bottomMargin)) {
    if (0 != topMargin) {
      SlideFrames(mFirstChild, topMargin);
    }
    // Record bottom margin value for returning as our carried out
    // bottom margin.
    aState.mPrevBottomMargin = bottomMargin;
  }

  if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus)) {
    // We may be breaking after a frame here (e.g. a child inline
    // frame that contains a BR tag and more content after the BR
    // tag). We will propagate that upward so that this frame gets
    // continued so that it can map the childs remaining content.

    // Create continuation frame for the child frame when it's not
    // complete.
    nsIFrame* newFrame;
    nsresult rv;
    rv = CreateNextInFlow(aState.mPresContext, this, aFrame, newFrame);
    if (NS_OK != rv) {
      // XXX RETURN ERROR STATUS...
      return PR_FALSE;
    }

    // Advance to next frame
    aState.mLastChild = aFrame;
    aFrame->GetNextSibling(aFrame);

    if (!justDidFirstLetter) {
      // Push remaining frames, if any. There may be no more frames if
      // the continuation frame already existed and it belongs to
      // <b>our</b> next-in-flow.
      if (nsnull != aFrame) {
        PushKids(aState, aInlineReflow, aFrame);
      }
      return PR_FALSE;
    }
  }

  if (NS_INLINE_IS_BREAK_AFTER(aReflowStatus)) {
    // If we are breaking after a child that's complete, we may still
    // be incomplete if we have more children that need
    // reflowing. Check for this after advancing to the next frame.
    aState.mLastChild = aFrame;
    aFrame->GetNextSibling(aFrame);
    if (nsnull != aFrame) {
      PushKids(aState, aInlineReflow, aFrame);
      aReflowStatus |= NS_FRAME_NOT_COMPLETE;
    }
    else {
      // We did the easy check for our completion status, now do the
      // hard one. See if any of our next-in-flows have any frames
      // left in them.
      nsInlineFrame* nextInFlow = (nsInlineFrame*) mNextInFlow;
      while (nsnull != nextInFlow) {
        if ((nsnull != nextInFlow->mFirstChild) ||
            (nsnull != nextInFlow->mOverflowList)) {
          // One of our next-in-flows has a child remaining. Therefore
          // we are not complete and must let our parent know so that
          // our parent doesn't accidently remove our next-in-flows!
          aReflowStatus |= NS_FRAME_NOT_COMPLETE;
          break;
        }
        nextInFlow = (nsInlineFrame*) nextInFlow->mNextInFlow;
      }
    }
    return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool
nsInlineFrame::SafeToPull(nsIFrame* aFrame)
{
  const nsStyleDisplay* disp;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) disp);
  const nsStylePosition* pos;
  aFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&) pos);
  if ((nsnull != disp) && (nsnull != pos)) {
    if (nsLineLayout::TreatFrameAsBlock(disp, pos)) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

nsIFrame*
nsInlineFrame::PullOneChild(nsIPresContext& aPresContext,
                            nsInlineFrame* aNextInFlow,
                            nsIFrame* aLastChild,
                            nsReflowStatus& aReflowStatus)
{
  NS_PRECONDITION(nsnull != aNextInFlow, "null ptr");

  // Get first available frame from the next-in-flow; if it's out of
  // frames check it's overflow list. Don't pull-up a block frame
  // unless we have zero children.
  nsIFrame* kidFrame = aNextInFlow->mFirstChild;
  if (nsnull == kidFrame) {
    if (nsnull == aNextInFlow->mOverflowList) {
      return nsnull;
    }
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
       ("nsInlineFrame::PullOneChild: from overflow list, frame=%p",
        kidFrame));
    kidFrame = aNextInFlow->mOverflowList;
    if ((nsnull != aLastChild) && !SafeToPull(kidFrame)) {
      aReflowStatus = NS_FRAME_NOT_COMPLETE;
      return nsnull;
    }
    kidFrame->GetNextSibling(aNextInFlow->mOverflowList);
  }
  else {
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("nsInlineFrame::PullOneChild: frame=%p",
                  kidFrame));
    if ((nsnull != aLastChild) && !SafeToPull(kidFrame)) {
      aReflowStatus = NS_FRAME_NOT_COMPLETE;
      return nsnull;
    }
    // Take the frame away from the next-in-flow. Update it's first
    // content offset.
    kidFrame->GetNextSibling(aNextInFlow->mFirstChild);
  }

  // Update the frames style context
  nsIFrame* oldParent;
  kidFrame->GetGeometricParent(oldParent);
  UpdateStyleContexts(aPresContext, kidFrame, oldParent, this);

  // Now give the frame to this container
  kidFrame->SetGeometricParent(this);
  nsIFrame* contentParent;
  kidFrame->GetContentParent(contentParent);
  if (aNextInFlow == contentParent) {
    kidFrame->SetContentParent(this);
  }

  // Add the frame on our list
  if (nsnull == aLastChild) {
    mFirstChild = kidFrame;
  } else {
    aLastChild->SetNextSibling(kidFrame);
  }
  kidFrame->SetNextSibling(nsnull);

  aReflowStatus = NS_FRAME_COMPLETE;
  return kidFrame;
}

void
nsInlineFrame::PushKids(nsInlineReflowState& aState,
                        nsInlineReflow& aInlineReflow,
                        nsIFrame* aPushedChild)
{
#ifdef NS_DEBUG
  NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");
  NS_ASSERTION(nsnull != aPushedChild, "bad pushkids call");
  NS_ASSERTION(nsnull != aState.mLastChild, "bad pushkids call");
  nsIFrame* nextSib;
  aState.mLastChild->GetNextSibling(nextSib);
  NS_ASSERTION(nextSib == aPushedChild, "bad pushkids call");
#endif

  // Break sibling list
  aState.mLastChild->SetNextSibling(nsnull);

  // Place overflow frames on our overflow list; our next-in-flow
  // will pick them up when it is reflowed
  mOverflowList = aPushedChild;

  // Count how many frames are remaining and set the
  // aInlineReflow's frame count accordingly.
  PRInt32 count = 0;
  nsIFrame* kid = mFirstChild;
  while (nsnull != kid) {
    count++;
    kid->GetNextSibling(kid);
  }
  aInlineReflow.ChangeFrameCount(count);
}

void
nsInlineFrame::DrainOverflowLists(nsIPresContext& aPresContext)
{
  // Our prev-in-flows overflow list goes before my children and must
  // be re-parented.
  if (nsnull != mPrevInFlow) {
    nsInlineFrame* prevInFlow = (nsInlineFrame*) mPrevInFlow;
    if (nsnull != prevInFlow->mOverflowList) {
      nsIFrame* frame = prevInFlow->mOverflowList;
      nsIFrame* lastFrame = nsnull;
      while (nsnull != frame) {
        // Update the frames style context
        nsIFrame* oldParent;
        frame->GetGeometricParent(oldParent);
        UpdateStyleContexts(aPresContext, frame, oldParent, this);

        // Reparent the frame
        frame->SetGeometricParent(this);
        nsIFrame* contentParent;
        frame->GetContentParent(contentParent);
        if (prevInFlow == contentParent) {
          frame->SetContentParent(this);
        }

        // Advance through the list
        lastFrame = frame;
        frame->GetNextSibling(frame);
      }

      // Join the two frame lists together and update our child count
      nsIFrame* newFirstChild = prevInFlow->mOverflowList;
      lastFrame->SetNextSibling(mFirstChild);
      mFirstChild = newFirstChild;
      prevInFlow->mOverflowList = nsnull;
    }
  }

  // Our overflow list goes to the end of our child list
  if (nsnull != mOverflowList) {
    // Append the overflow list to the end of our child list
    nsIFrame* lastFrame = LastFrame(mFirstChild);
    if (nsnull == lastFrame) {
      mFirstChild = mOverflowList;
    }
    else {
      lastFrame->SetNextSibling(mOverflowList);
    }
    mOverflowList = nsnull;
  }
}

NS_IMETHODIMP
nsInlineFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Inline", aResult);
}
