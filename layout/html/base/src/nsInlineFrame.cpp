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
#include "nsLineLayout.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"

#include "nsIPresContext.h"
#include "nsIReflowCommand.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"


struct nsInlineReflowState : nsFrameReflowState {
  nsInlineReflowState(nsIPresContext& aPresContext,
                      const nsReflowState& aReflowState,
                      const nsReflowMetrics& aMetrics);
  ~nsInlineReflowState();

  // Last child we have reflowed (so far)
  nsIFrame* mLastChild;
};

nsInlineReflowState::nsInlineReflowState(nsIPresContext& aPresContext,
                                         const nsReflowState& aReflowState,
                                         const nsReflowMetrics& aMetrics)
  : nsFrameReflowState(aPresContext, aReflowState, aMetrics)
{

  mLastChild = nsnull;
}

nsInlineReflowState::~nsInlineReflowState()
{
}

//----------------------------------------------------------------------

class nsInlineFrame : public nsHTMLContainerFrame,
                      public nsIInlineReflow
{
public:
  nsInlineFrame(nsIContent* aContent, nsIFrame* aParent);
  virtual ~nsInlineFrame();

  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);
  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aCX,
                                   nsIFrame* aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*& aContinuingFrame);
  NS_IMETHOD AttributeChanged(nsIPresShell* aShell,
                              nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              nsIAtom* aAttribute);

  // nsIInlineReflow
  NS_IMETHOD FindTextRuns(nsLineLayout& aLineLayout,
                          nsIReflowCommand* aReflowCommand);
  NS_IMETHOD InlineReflow(nsLineLayout& aLineLayout,
                          nsReflowMetrics& aDesiredSize,
                          const nsReflowState& aReflowState);

  virtual PRIntn GetSkipSides() const;

  nsresult InitialReflow(nsInlineReflowState& aState,
                         nsInlineReflow& aInlineReflow);

  nsresult FrameAppendedReflow(nsInlineReflowState& aState,
                               nsInlineReflow& aInlineReflow);

  nsresult ChildIncrementalReflow(nsInlineReflowState& aState,
                                  nsInlineReflow& aInlineReflow);

  nsresult ResizeReflow(nsInlineReflowState& aState,
                        nsInlineReflow& aInlineReflow);

  void ComputeFinalSize(nsInlineReflowState& aState,
                        nsInlineReflow& aInlineReflow,
                        nsReflowMetrics& aMetrics);

  PRBool ReflowMapped(nsInlineReflowState& aState,
                      nsInlineReflow& aInlineReflow,
                      nsInlineReflowStatus& aReflowStatus);

  PRBool ReflowFrame(nsInlineReflowState& aState,
                     nsInlineReflow& aInlineReflow,
                     nsIFrame* aFrame,
                     nsInlineReflowStatus& aReflowStatus);

  nsInlineReflowStatus PullUpChildren(nsInlineReflowState& aState,
                                      nsInlineReflow& aInlineReflow);

  nsIFrame* PullOneChild(nsInlineFrame* aNextInFlow,
                         nsIFrame* aLastChild);

  void PushKids(nsInlineReflowState& aState,
                nsInlineReflow& aInlineReflow,
                nsIFrame* aPushedChild);

  void DrainOverflowLists();

  nsresult AppendNewFrames(nsIPresContext& aPresContext, nsIFrame*);

  void InsertNewFrame(nsIFrame* aNewFrame, nsIFrame* aPrevSibling);

  friend nsresult NS_NewInlineFrame(nsIContent* aContent,
                                    nsIFrame* aParentFrame,
                                    nsIFrame*& aNewFrame);
};

//----------------------------------------------------------------------

nsresult NS_NewInlineFrame(nsIContent* aContent, nsIFrame* aParentFrame,
                           nsIFrame*& aNewFrame)
{
  aNewFrame = new nsInlineFrame(aContent, aParentFrame);
  if (nsnull == aNewFrame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsInlineFrame::nsInlineFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

nsInlineFrame::~nsInlineFrame()
{
}

NS_IMETHODIMP
nsInlineFrame::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIInlineReflowIID)) {
    *aInstancePtrResult = (void*) ((nsIInlineReflow*)this);
    return NS_OK;
  }
  return nsFrame::QueryInterface(aIID, aInstancePtrResult);
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
  nsIFrame* lastFrame;
  if (nsnull == mFirstChild) {
    lastFrame = nsnull;
    mFirstChild = aNewFrame;
  } else {
    lastFrame = LastFrame(mFirstChild);
    lastFrame->SetNextSibling(aNewFrame);
  }
  return WrapFrames(aPresContext, lastFrame, aNewFrame);
}

NS_IMETHODIMP
nsInlineFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  NS_PRECONDITION(nsnull == mFirstChild, "already initialized");
  return AppendNewFrames(aPresContext, aChildList);  
}

NS_IMETHODIMP
nsInlineFrame::CreateContinuingFrame(nsIPresContext&  aCX,
                                     nsIFrame*        aParent,
                                     nsIStyleContext* aStyleContext,
                                     nsIFrame*&       aContinuingFrame)
{
  nsInlineFrame* cf = new nsInlineFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aCX, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsInlineFrame::CreateContinuingFrame: newFrame=%p", cf));
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::AttributeChanged(nsIPresShell* aShell,
                                nsIPresContext* aPresContext,
                                nsIContent* aChild,
                                nsIAtom* aAttribute)
{
  nsresult result = nsHTMLContainerFrame::AttributeChanged(aShell, 
                                                           aPresContext,
                                                           aChild, 
                                                           aAttribute);
  if (NS_OK != result) {
    return result;
  }
  if (nsHTMLAtoms::color == aAttribute) {
    ApplyStyleChangeToTree(*aPresContext, this);
    ApplyRenderingChangeToTree(*aPresContext, this);
  }
  else if (nsHTMLAtoms::face == aAttribute) {
    ApplyStyleChangeToTree(*aPresContext, this);
    ApplyReflowChangeToTree(*aPresContext, this);
    ApplyRenderingChangeToTree(*aPresContext, this);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::FindTextRuns(nsLineLayout&  aLineLayout,
                            nsIReflowCommand* aReflowCommand)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsInlineFrame::FindTextRuns"));

  // Gather up children from the overflow lists
  nsresult rv = NS_OK;
  DrainOverflowLists();

  // Ask each child frame for its text runs
  nsIFrame* frame = mFirstChild;
  while (nsnull != frame) {
    nsIInlineReflow* inlineReflow;
    if (NS_OK == frame->QueryInterface(kIInlineReflowIID,
                                       (void**)&inlineReflow)) {
      rv = inlineReflow->FindTextRuns(aLineLayout, aReflowCommand);
      if (NS_OK != rv) {
        return rv;
      }
    }
    else {
      // A frame that doesn't implement nsIInlineReflow isn't text
      // therefore it will end an open text run.
      aLineLayout.EndTextRun();
    }
    frame->GetNextSibling(frame);
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsInlineFrame::FindTextRuns rv=%x", rv));
  return rv;
}

void
nsInlineFrame::InsertNewFrame(nsIFrame* aNewFrame, nsIFrame* aPrevSibling)
{
  nsIFrame* nextSibling;

  if (nsnull == aPrevSibling) {
    nextSibling = mFirstChild;
    mFirstChild = aNewFrame;
  } else {
    aPrevSibling->GetNextSibling(nextSibling);
    aPrevSibling->SetNextSibling(aNewFrame);
  }
  aNewFrame->SetNextSibling(nextSibling);
}

NS_IMETHODIMP
nsInlineFrame::InlineReflow(nsLineLayout&        aLineLayout,
                            nsReflowMetrics&     aMetrics,
                            const nsReflowState& aReflowState)
{
//XXX ListTag(stdout); printf(": enter (runningMargin=%d)\n", aMetrics.mCarriedInTopMargin);
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
  nsInlineReflowState state(aLineLayout.mPresContext, aReflowState, aMetrics);

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
  nsInlineReflow inlineReflow(aLineLayout, state, this);
  inlineReflow.Init(state.mBorderPadding.left, state.mBorderPadding.top,
                    width, height);

  // Now translate in by our border and padding
  aLineLayout.mSpaceManager->Translate(state.mBorderPadding.left,
                                       state.mBorderPadding.top);

  // Based on the type of reflow, switch out to the appropriate
  // routine.
  nsresult rv = NS_OK;
  if (eReflowReason_Initial == state.reason) {
    DrainOverflowLists();
    rv = InitialReflow(state, inlineReflow);
    mState &= ~NS_FRAME_FIRST_REFLOW;
  }
  else if (eReflowReason_Incremental == state.reason) {
    // XXX For now we drain our overflow list in case our parent
    // reflowed our prev-in-flow and our prev-in-flow pushed some
    // children forward to us (e.g. a speculative pullup from us that
    // failed)
    DrainOverflowLists();

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
        rv = FrameAppendedReflow(state, inlineReflow);
        break;

      case nsIReflowCommand::FrameInserted:
        // Link the new frame into the child list
        state.reflowCommand->GetChildFrame(newFrame);
        state.reflowCommand->GetPrevSiblingFrame(prevSibling);
        InsertNewFrame(newFrame, prevSibling);
        // fall thru...

      default:
        // XXX For now map the other incremental operations into full reflows
        rv = ResizeReflow(state, inlineReflow);
        break;
      }
    }
    else {
      // Get next frame in reflow command chain
      state.reflowCommand->GetNext(state.mNextRCFrame);

      // Continue the reflow
      rv = ChildIncrementalReflow(state, inlineReflow);
    }
  }
  else if (eReflowReason_Resize == state.reason) {
    DrainOverflowLists();
    rv = ResizeReflow(state, inlineReflow);
  }
  ComputeFinalSize(state, inlineReflow, aMetrics);

  // Now translate in by our border and padding
  aLineLayout.mSpaceManager->Translate(-state.mBorderPadding.left,
                                       -state.mBorderPadding.top);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsInlineFrame::InlineReflow size=%d,%d rv=%x nif=%p",
      aMetrics.width, aMetrics.height, rv, mNextInFlow));
//XXX ListTag(stdout); printf(": exit (runningMargin=%d)\n", aMetrics.mCarriedOutBottomMargin);
  return rv;
}

// XXX factor this (into nsFrameReflowState?) so that both block and
// inline can use most of the same logic

void
nsInlineFrame::ComputeFinalSize(nsInlineReflowState& aState,
                                nsInlineReflow& aInlineReflow,
                                nsReflowMetrics& aMetrics)
{
  // Align our child frames. Note that inline frames "shrink wrap"
  // around their contents therefore we need to fixup the available
  // width in nsInlineLayout so that it doesn't do any horizontal
  // alignment.
  nsRect bounds;
  aInlineReflow.VerticalAlignFrames(bounds);
  aInlineReflow.RelativePositionFrames();

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
    aMetrics.mCarriedOutBottomMargin = 0;
  }
  else {
// XXX tip of the iceburg: when an inline is wrapping up a block frame
// it needs to go the whole hog: provide a proper spacemanager
// coordinate system; apply horizontal alignment (in case the block
// has a width set), etc. Of course the block code knows how to do
// this already...
//XXXprintf("bounds={%d,%d,%d,%d}\n", 
//XXX       bounds.x, bounds.y, bounds.width, bounds.height);
    if (aInlineReflow.GetIsBlock()) {
      // Make sure the blocks top and bottom margins get applied; the
      // top margin will be in bounds.y; the bottom margin we get from
      // the inline reflow.
      nscoord bottomMargin = aInlineReflow.GetBottomMargin();
      nscoord newY = aState.mBorderPadding.top + bounds.YMost()
        + bottomMargin + aState.mBorderPadding.bottom;
//XXXprintf("newY=%d bounds={%d,%d,%d,%d} bottomMargin=%d\n", newY,
//XXX       bounds.x, bounds.y, bounds.width, bounds.height, bottomMargin);
      aMetrics.height = newY;
      aMetrics.ascent = newY;
      aMetrics.descent = 0;
    }
    else {
      aMetrics.ascent = aState.mBorderPadding.top +
        aInlineReflow.GetMaxAscent();
      aMetrics.descent = aInlineReflow.GetMaxDescent() +
        aState.mBorderPadding.bottom;
      aMetrics.height = aMetrics.ascent + aMetrics.descent;
    }

    // Carry out to the caller the running bottom margin value
    if (0 != aState.mBorderPadding.bottom) {
      // Don't carry out a child margin when we have a border
      aMetrics.mCarriedOutBottomMargin = 0;
    }
    else {
      // Carry out our the total child bottom margin value
      aMetrics.mCarriedOutBottomMargin = aState.mRunningMargin;
    }
  }

  if (aState.mComputeMaxElementSize) {
    // Adjust max-element size if this frame's no-wrap flag is set.
    if (aState.mNoWrap) {
      aMetrics.maxElementSize->width = aMetrics.width;
      aMetrics.maxElementSize->height = aMetrics.height;
    }
    else {
      *aMetrics.maxElementSize = aState.mMaxElementSize;
    }

    // Add in our border and padding to the max-element-size so that
    // we don't shrink too far.
    aMetrics.maxElementSize->width += aState.mBorderPadding.left +
      aState.mBorderPadding.right;
    aMetrics.maxElementSize->height += aState.mBorderPadding.top +
      aState.mBorderPadding.bottom;
  }
}

nsInlineReflowStatus
nsInlineFrame::InitialReflow(nsInlineReflowState& aState,
                             nsInlineReflow& aInlineReflow)
{
  NS_PRECONDITION(nsnull == mNextInFlow, "bad frame-appended-reflow");

  nsInlineReflowStatus rs = NS_FRAME_COMPLETE;
  if (nsnull != mFirstChild) {
    if (!ReflowMapped(aState, aInlineReflow, rs)) {
      return rs;
    }
  }
  return rs;
}

nsInlineReflowStatus
nsInlineFrame::FrameAppendedReflow(nsInlineReflowState& aState,
                                   nsInlineReflow& aInlineReflow)
{
  NS_PRECONDITION(nsnull == mNextInFlow, "bad frame-appended-reflow");

  // Get the first of the newly appended frames
  nsIFrame* firstAppendedFrame;
  aState.reflowCommand->GetChildFrame(firstAppendedFrame);

  // Add the new frames
  AppendNewFrames(aState.mPresContext, firstAppendedFrame);

  nsInlineReflowStatus rs = NS_FRAME_COMPLETE;
  if (nsnull != mFirstChild) {
    if (!ReflowMapped(aState, aInlineReflow, rs)) {
      return rs;
    }
  }
  return rs;
}

nsInlineReflowStatus
nsInlineFrame::ChildIncrementalReflow(nsInlineReflowState& aState,
                                      nsInlineReflow& aInlineReflow)
{
  // XXX we can do better SOMEDAY
  return ResizeReflow(aState, aInlineReflow);
}

nsInlineReflowStatus
nsInlineFrame::ResizeReflow(nsInlineReflowState& aState,
                            nsInlineReflow& aInlineReflow)
{
  nsInlineReflowStatus rs = NS_FRAME_COMPLETE;
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
                            nsInlineReflowStatus& aReflowStatus)
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

nsInlineReflowStatus
nsInlineFrame::PullUpChildren(nsInlineReflowState& aState,
                              nsInlineReflow& aInlineReflow)
{
  nsInlineReflowStatus reflowStatus = NS_FRAME_COMPLETE;
  nsInlineFrame* nextInFlow = (nsInlineFrame*) mNextInFlow;
  while (nsnull != nextInFlow) {
    // Get child from our next-in-flow
    nsIFrame* child = PullOneChild(nextInFlow, aState.mLastChild);
    if (nsnull == child) {
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

PRBool
nsInlineFrame::ReflowFrame(nsInlineReflowState& aState,
                           nsInlineReflow& aInlineReflow,
                           nsIFrame* aFrame,
                           nsInlineReflowStatus& aReflowStatus)
{
  aInlineReflow.SetIsFirstChild(aFrame == mFirstChild);
  aReflowStatus = aInlineReflow.ReflowFrame(aFrame);
  if (NS_IS_REFLOW_ERROR(aReflowStatus)) {
    return PR_FALSE;
  }

  // XXX temporary code until blocks can return break-after naturally
  if (aInlineReflow.GetIsBlock() &&
      !NS_INLINE_IS_BREAK(aReflowStatus) &&
      NS_FRAME_IS_COMPLETE(aReflowStatus)) {
    // Force reflow status to be what the block code should have returned
    aReflowStatus = NS_INLINE_LINE_BREAK_AFTER(NS_FRAME_COMPLETE);
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
  else if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus)) {
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
      aReflowStatus = nsInlineReflowStatus(rv);
      return PR_FALSE;
    }

    // Advance to next frame
    aState.mLastChild = aFrame;
    aFrame->GetNextSibling(aFrame);

    // Push remaining frames, if any. There may be no more frames if
    // the continuation frame already existed and it belongs to
    // <b>our</b> next-in-flow.
    if (nsnull != aFrame) {
      PushKids(aState, aInlineReflow, aFrame);
    }
    return PR_FALSE;
  }
  else if (NS_INLINE_IS_BREAK_AFTER(aReflowStatus)) {
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
        if (nsnull != nextInFlow->mFirstChild) {
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

nsIFrame*
nsInlineFrame::PullOneChild(nsInlineFrame* aNextInFlow,
                               nsIFrame*   aLastChild)
{
  NS_PRECONDITION(nsnull != aNextInFlow, "null ptr");

  // Get first available frame from the next-in-flow; if it's out of
  // frames check it's overflow list.
  nsIFrame* kidFrame = aNextInFlow->mFirstChild;
  if (nsnull == kidFrame) {
    if (nsnull == aNextInFlow->mOverflowList) {
      return nsnull;
    }
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
       ("nsInlineFrame::PullOneChild: from overflow list, frame=%p",
        kidFrame));
    kidFrame = aNextInFlow->mOverflowList;
    kidFrame->GetNextSibling(aNextInFlow->mOverflowList);
  }
  else {
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("nsInlineFrame::PullOneChild: frame=%p",
                  kidFrame));

    // Take the frame away from the next-in-flow. Update it's first
    // content offset.
    kidFrame->GetNextSibling(aNextInFlow->mFirstChild);
  }

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
}

void
nsInlineFrame::DrainOverflowLists()
{
  // Our prev-in-flows overflow list goes before my children and must
  // be re-parented.
  if (nsnull != mPrevInFlow) {
    nsInlineFrame* prevInFlow = (nsInlineFrame*) mPrevInFlow;
    if (nsnull != prevInFlow->mOverflowList) {
      nsIFrame* frame = prevInFlow->mOverflowList;
      nsIFrame* lastFrame = nsnull;
      while (nsnull != frame) {
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
