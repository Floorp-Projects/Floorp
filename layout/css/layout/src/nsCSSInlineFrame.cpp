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
#include "nsCSSInlineFrame.h"
#include "nsCSSLayout.h"

#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"

#include "nsHTMLIIDs.h"// XXX 
#include "nsHTMLBase.h"// XXX rename to nsCSSBase?

// XXX TODO:
// content appended INCREMENTAL reflow
// content inserted INCREMENTAL reflow
// content deleted INCREMENTAL reflow

nsCSSInlineReflowState::nsCSSInlineReflowState(nsCSSLineLayout& aLineLayout,
                                               nsCSSInlineFrame* aInlineFrame,
                                               nsIStyleContext* aInlineSC,
                                               const nsReflowState& aRS,
                                               nsSize* aMaxElementSize)
  : nsReflowState(aInlineFrame, *aRS.parentReflowState, aRS.maxSize),
    mInlineLayout(aLineLayout, aInlineFrame, aInlineSC)
{
  // While we skip around the reflow state that our parent gave us so
  // that the parentReflowState is linked properly, we don't want to
  // skip over it's reason.
  reason = aRS.reason;

  mPresContext = aLineLayout.mPresContext;
  mLastChild = nsnull;

  const nsStyleText* styleText = (const nsStyleText*)
    aInlineSC->GetStyleData(eStyleStruct_Text);
  mNoWrap = PRBool(NS_STYLE_WHITESPACE_NORMAL != styleText->mWhiteSpace);
  const nsStyleSpacing* styleSpacing = (const nsStyleSpacing*)
    aInlineSC->GetStyleData(eStyleStruct_Spacing);
  styleSpacing->CalcBorderPaddingFor(aInlineFrame, mBorderPadding);

  if (nsnull != aMaxElementSize) {
    aMaxElementSize->SizeTo(0, 0);
  }

  // If we're constrained adjust the available size so it excludes space
  // needed for border/padding
  nscoord width = maxSize.width;
  if (NS_UNCONSTRAINEDSIZE != width) {
    width -= mBorderPadding.left + mBorderPadding.right;
  }
  // XXX I don't think we need this!
  nscoord height = maxSize.height;
  if (NS_UNCONSTRAINEDSIZE != height) {
    height -= mBorderPadding.top + mBorderPadding.bottom;
  }

  PRIntn ss = mStyleSizeFlags =
    nsCSSLayout::GetStyleSize(mPresContext, *this, mStyleSize);
  if (0 != (ss & NS_SIZE_HAS_WIDTH)) {
    // When we are given a width, change the reflow behavior to
    // unconstrained.
    // XXX is this right?
    width = NS_UNCONSTRAINEDSIZE;
  }

  mInlineLayout.Init(this);
  mInlineLayout.Prepare(PRBool(NS_UNCONSTRAINEDSIZE == width),
                        mNoWrap, aMaxElementSize);
  mInlineLayout.SetReflowSpace(mBorderPadding.left, mBorderPadding.top,
                               width, height/* XXX height??? */);
}

nsCSSInlineReflowState::~nsCSSInlineReflowState()
{
}

//----------------------------------------------------------------------

nsresult NS_NewCSSInlineFrame(nsIFrame**  aInstancePtrResult,
                              nsIContent* aContent,
                              nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsCSSInlineFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsCSSInlineFrame::nsCSSInlineFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsCSSContainerFrame(aContent, aParent)
{
}

nsCSSInlineFrame::~nsCSSInlineFrame()
{
}

NS_IMETHODIMP
nsCSSInlineFrame::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
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
nsCSSInlineFrame::GetSkipSides() const
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

NS_IMETHODIMP
nsCSSInlineFrame::CreateContinuingFrame(nsIPresContext*  aCX,
                                        nsIFrame*        aParent,
                                        nsIStyleContext* aStyleContext,
                                        nsIFrame*&       aContinuingFrame)
{
  nsCSSInlineFrame* cf = new nsCSSInlineFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aCX, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSInlineFrame::FindTextRuns(nsCSSLineLayout& aLineLayout)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsCSSInlineFrame::InlineReflow(nsCSSLineLayout&     aLineLayout,
                               nsReflowMetrics&     aMetrics,
                               const nsReflowState& aReflowState)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
    ("enter nsCSSInlineFrame::InlineReflow maxSize=%d,%d reason=%d [%d,%d,%c]",
     aReflowState.maxSize.width, aReflowState.maxSize.height,
     aReflowState.reason,
     mFirstContentOffset, mLastContentOffset, mLastContentIsComplete?'T':'F'));

  // If this is the initial reflow, generate any synthetic content
  // that needs generating.
  if (eReflowReason_Initial == aReflowState.reason) {
    NS_ASSERTION(0 != (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
    nsresult rv = ProcessInitialReflow(aLineLayout.mPresContext);
    if (NS_OK != rv) {
      return rv;
    }
  }
  else {
    NS_ASSERTION(0 == (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }

  nsCSSInlineReflowState state(aLineLayout, this, mStyleContext,
                               aReflowState, aMetrics.maxElementSize);
  nsresult rv = NS_OK;
  if (eReflowReason_Initial == state.reason) {
    MoveOverflowToChildList();
    rv = FrameAppendedReflow(state);
    mState &= ~NS_FRAME_FIRST_REFLOW;
  }
  else if (eReflowReason_Incremental == state.reason) {
    NS_ASSERTION(nsnull == mOverflowList, "unexpected overflow list");
    nsIFrame* target;
    state.reflowCommand->GetTarget(target);
    if (this == target) {
      nsIReflowCommand::ReflowType type;
      state.reflowCommand->GetType(type);
      switch (type) {
      case nsIReflowCommand::FrameAppended:
        rv = FrameAppendedReflow(state);
        break;

      default:
        // XXX For now map the other incremental operations into full reflows
        rv = ResizeReflow(state);
        break;
      }
    }
    else {
      rv = ChildIncrementalReflow(state);
    }
  }
  else if (eReflowReason_Resize == state.reason) {
    MoveOverflowToChildList();
    rv = ResizeReflow(state);
  }
  ComputeFinalSize(state, aMetrics);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSInlineFrame::InlineReflow size=%d,%d rv=%x [%d,%d,%c]",
      aMetrics.width, aMetrics.height, rv,
      mFirstContentOffset, mLastContentOffset,
      mLastContentIsComplete?'T':'F'));
  return rv;
}

void
nsCSSInlineFrame::ComputeFinalSize(nsCSSInlineReflowState& aState,
                                   nsReflowMetrics& aMetrics)
{
  // Align our child frames. Note that inline frames "shrink wrap"
  // around their contents therefore we need to fixup the available
  // width in nsCSSInlineLayout so that it doesn't do any horizontal
  // alignment.
  nsRect bounds;
  aState.mInlineLayout.mAvailWidth =
    aState.mInlineLayout.mX - aState.mInlineLayout.mLeftEdge;
  aState.mInlineLayout.AlignFrames(mFirstChild, mChildCount, bounds);

  // Compute the final width. Use the style width when supplied.
  PRIntn ss = aState.mStyleSizeFlags;
  if (0 != (ss & NS_SIZE_HAS_WIDTH)) {
    // XXX check css2 spec: I think the spec dictates the size of the
    // entire box, not the content area
    aMetrics.width = aState.mBorderPadding.left + aState.mStyleSize.width +
      aState.mBorderPadding.right;
  }
  else {
    // Make sure that we collapse into nothingness if our content is
    // zero sized
    if (0 == bounds.width) {
      aMetrics.width = 0;
    }
    else {
      aMetrics.width = aState.mBorderPadding.left + bounds.width +
        aState.mBorderPadding.right;
    }
  }

  // Compute the final height. Use the style height when supplied.
#if XXX_fix_me
  if (0 != (ss & NS_SIZE_HAS_HEIGHT)) {
    aMetrics.height = aState.mBorderPadding.top + aState.mStyleSize.height +
      aState.mBorderPadding.bottom;
    // XXX Apportion extra height evenly to the ascent/descent, taking
    // into account roundoff properly. What about when the height is
    // smaller than the computed height? Does the css2 spec say
    // anything about this?
    aMetrics.ascent = ?;
    aMetrics.descent = ?;
  }
  else
#endif
  {
    // Make sure that we collapse into nothingness if our content is
    // zero sized
    if (0 == bounds.height) {
      aMetrics.ascent = 0;
      aMetrics.descent = 0;
      aMetrics.height = 0;
    }
    else {
      aMetrics.ascent = aState.mBorderPadding.top +
        aState.mInlineLayout.mMaxAscent;
      aMetrics.descent = aState.mInlineLayout.mMaxDescent +
        aState.mBorderPadding.bottom;
      aMetrics.height = aMetrics.ascent + aMetrics.descent;
    }
  }

  // Adjust max-element size if this frame's no-wrap flag is set.
  if ((nsnull != aMetrics.maxElementSize) && aState.mNoWrap) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }
}

nsInlineReflowStatus
nsCSSInlineFrame::FrameAppendedReflow(nsCSSInlineReflowState& aState)
{
  // XXX we can do better SOMEDAY

  // This frame is the last in-flow (because this is a frame-appended
  // reflow); therefore we know that all of the prior frames are
  // unaffected and need not be reflowed.

  // XXX What about the case where the last frame is text or contains
  // text and we have just appended more text?

  nsInlineReflowStatus rv = NS_INLINE_REFLOW_COMPLETE;
  if (0 != mChildCount) {
    rv = ReflowMapped(aState);
    if (IS_REFLOW_ERROR(rv)) {
      return rv;
    }
  }

  if (NS_INLINE_REFLOW_COMPLETE == (NS_INLINE_REFLOW_REFLOW_MASK & rv)) {
    // Note: There is nothing to pullup because this is a
    // frame-appended reflow (therefore we are the last-in-flow)
    NS_ASSERTION(nsnull == mNextInFlow, "bad frame-appended-reflow");

    // Reflow new children
    rv = ReflowUnmapped(aState);
  }
  return rv;
}

nsInlineReflowStatus
nsCSSInlineFrame::ChildIncrementalReflow(nsCSSInlineReflowState& aState)
{
  // XXX we can do better SOMEDAY
  return ResizeReflow(aState);
}

nsInlineReflowStatus
nsCSSInlineFrame::ResizeReflow(nsCSSInlineReflowState& aState)
{
  nsInlineReflowStatus rv = NS_INLINE_REFLOW_COMPLETE;
  if (0 != mChildCount) {
    rv = ReflowMapped(aState);
    if (IS_REFLOW_ERROR(rv)) {
      return rv;
    }
  }

  if (NS_INLINE_REFLOW_COMPLETE == (NS_INLINE_REFLOW_REFLOW_MASK & rv)) {
    // Try to fit some more children from our next in flow
    if (nsnull != mNextInFlow) {
      rv = PullUpChildren(aState);
    }
  }

  // XXX This is here temporarily until inline is reworked to
  // pre-fluff it's frame tree
  if (NS_INLINE_REFLOW_COMPLETE == (NS_INLINE_REFLOW_REFLOW_MASK & rv)) {
    // Reflow new children
    rv = ReflowUnmapped(aState);
  }

  return rv;
}

nsInlineReflowStatus
nsCSSInlineFrame::ReflowMapped(nsCSSInlineReflowState& aState)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsCSSInlineFrame::ReflowMapped: childCount=%d", mChildCount));

  nsresult rv;
  nsInlineReflowStatus reflowResult = NS_INLINE_REFLOW_NOT_COMPLETE;
  PRInt32 kidContentIndex = mFirstContentOffset;
  nsIFrame* prevChild = nsnull;
  nsIFrame* child = mFirstChild;
  while (nsnull != child) {
    nsInlineReflowStatus rs = aState.mInlineLayout.ReflowAndPlaceFrame(child);
    if (IS_REFLOW_ERROR(rs)) {
      reflowResult = rs;
      goto done;
    }
    switch (NS_INLINE_REFLOW_REFLOW_MASK & rs) {
    case NS_INLINE_REFLOW_COMPLETE:
      prevChild = child;
      child->GetNextSibling(child);
      kidContentIndex++;
      mLastContentIsComplete = PR_TRUE;
      break;

    case NS_INLINE_REFLOW_NOT_COMPLETE:
      // We must create the continuation now because our next-in-flow
      // won't know to create it.
      rv = MaybeCreateNextInFlow(aState, child);
      if (NS_OK != rv) {
        reflowResult = nsInlineReflowStatus(rv);
        goto done;
      }
      prevChild = child;
      child->GetNextSibling(child);
      PushKids(aState, prevChild, child);
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_FALSE;
      NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");
      goto done;

    case NS_INLINE_REFLOW_BREAK_AFTER:
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_TRUE;
      NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");

      // We might have children following this breaking child (for
      // example, somebody inserted an HTML BR in the middle of
      // us). If we do have more children they need to be pushed to a
      // next-in-flow.
      prevChild = child;
      child->GetNextSibling(child);
      if (nsnull != child) {
        PushKids(aState, prevChild, child);
      }
      else {
        reflowResult = NS_INLINE_REFLOW_COMPLETE;
      }
      goto done;

    case NS_INLINE_REFLOW_BREAK_BEFORE:
      mLastContentIsComplete = PR_TRUE;
      if (nsnull != prevChild) {
        mLastContentOffset = kidContentIndex - 1;
        PushKids(aState, prevChild, child);
      }
      else {
        // We are trying to push our only child. It must be a block
        // that must be at the start of a line and we must not be at
        // the start of a line.
        NS_NOTYETIMPLEMENTED("XXX");
      }
      goto done;
    }
  }
  if (kidContentIndex == 0) {
    mLastContentOffset = 0;
  } else {
    mLastContentOffset = kidContentIndex - 1;
  }
  NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");
  mLastContentIsComplete = PR_TRUE;
  aState.mLastChild = prevChild;
  reflowResult = NS_INLINE_REFLOW_COMPLETE;

done:;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSInlineFrame::ReflowMapped: childCount=%d reflowResult=%x",
      mChildCount, reflowResult));
  return reflowResult;
}

nsInlineReflowStatus
nsCSSInlineFrame::ReflowUnmapped(nsCSSInlineReflowState& aState)
{
  NS_PRECONDITION(mLastContentIsComplete == PR_TRUE, "huh?");

  nsresult rv;
  nsInlineReflowStatus reflowResult = NS_INLINE_REFLOW_NOT_COMPLETE;

  // Get the childPrevInFlow for our eventual first child if we are a
  // continuation and we have no children and the last child in our
  // prev-in-flow is incomplete. While we are at it, we also compute
  // our kidContentIndex.
  PRInt32 kidContentIndex;
  nsIFrame* childPrevInFlow = nsnull;
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    nsCSSInlineFrame* prev = (nsCSSInlineFrame*)mPrevInFlow;
    NS_ASSERTION(prev->mLastContentOffset >= prev->mFirstContentOffset,
                 "bad prevInFlow");

    kidContentIndex = prev->NextChildOffset();
    if (!prev->mLastContentIsComplete) {
      // Our prev-in-flow's last child is not complete
      prev->LastChild(childPrevInFlow);
    }
  }
  else {
    kidContentIndex = NextChildOffset();
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsCSSInlineFrame::ReflowUnmapped: kci=%d childPrevInFlow=%p",
      kidContentIndex, childPrevInFlow));

  nsIFrame* prevChild = aState.mLastChild;
  PRInt32 lastContentIndex;
  lastContentIndex = mContent->ChildCount();
  while (kidContentIndex < lastContentIndex) {
    nsIContent* kid;
    kid = mContent->ChildAt(kidContentIndex);
    if (nsnull == kid) {
      // Our content container is bad
      break;
    }

    // Create child
    nsIFrame* child;
    rv = nsHTMLBase::CreateFrame(aState.mPresContext, this, kid,
                                 childPrevInFlow, child);
    NS_RELEASE(kid);
    if (NS_OK != rv) {
      reflowResult = nsInlineReflowStatus(rv);
      goto done;
    }
    if (nsnull == prevChild) {
      mFirstChild = child;
      mFirstContentOffset = kidContentIndex;
    }
    else {
      prevChild->SetNextSibling(child);
    }
    mChildCount++;
    NS_FRAME_TRACE(NS_FRAME_TRACE_NEW_FRAMES,
       ("nsCSSInlineFrame::ReflowUnmapped: new-frame=%p prev-in-flow=%p",
        child, childPrevInFlow));

    // Reflow the child
    nsInlineReflowStatus rs = aState.mInlineLayout.ReflowAndPlaceFrame(child);
    if (IS_REFLOW_ERROR(rs)) {
      reflowResult = rs;
      goto done;
    }
    switch (NS_INLINE_REFLOW_REFLOW_MASK & rs) {
    case NS_INLINE_REFLOW_COMPLETE:
      prevChild = child;
      childPrevInFlow = nsnull;
      kidContentIndex++;
      mLastContentIsComplete = PR_TRUE;
      break;

    case NS_INLINE_REFLOW_NOT_COMPLETE:
      // Note: We don't need to create a next-in-flow for the child
      // because we know that we will be continued and our
      // continuation will re-enter the reflow-unmapped logic and pick
      // up from the prev-in-flows last child and continue it.
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_FALSE;
      NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");
      goto done;

    case NS_INLINE_REFLOW_BREAK_AFTER:
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_TRUE;
      NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");
      if (++kidContentIndex == lastContentIndex) {
        reflowResult = NS_INLINE_REFLOW_COMPLETE;
      }
      goto done;

    case NS_INLINE_REFLOW_BREAK_BEFORE:
      // We created the child but it turns out we can't keep it
      mLastContentIsComplete = PR_TRUE;
      if (nsnull != prevChild) {
        mLastContentOffset = kidContentIndex - 1;
        PushKids(aState, prevChild, child);
      }
      else {
        // We are trying to push our only child. It must be a block
        // that must be at the start of a line and we must not be at
        // the start of a line.
        NS_NOTYETIMPLEMENTED("XXX");
      }
      goto done;
    }
  }
  if (kidContentIndex == 0) {
    NS_ASSERTION(lastContentIndex == 0, "bad kid content index");
    mLastContentOffset = 0;
  } else {
    mLastContentOffset = kidContentIndex - 1;
  }
  NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");
  mLastContentIsComplete = PR_TRUE;
  reflowResult = NS_INLINE_REFLOW_COMPLETE;

done:;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSInlineFrame::ReflowUnmapped: reflowResult=%x", reflowResult));
  return reflowResult;
}

nsInlineReflowStatus
nsCSSInlineFrame::PullUpChildren(nsCSSInlineReflowState& aState)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsCSSInlineFrame::PullUpChildren: childCount=%d",
      mChildCount));

  // Get correct kidContentIndex
  PRInt32 kidContentIndex;
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    nsCSSInlineFrame* prev = (nsCSSInlineFrame*)mPrevInFlow;
    NS_ASSERTION(prev->mLastContentOffset >= prev->mFirstContentOffset,
                 "bad prevInFlow");
    kidContentIndex = prev->NextChildOffset();
  }
  else {
    kidContentIndex = NextChildOffset();
  }

  nsresult rv;
  nsInlineReflowStatus reflowResult = NS_INLINE_REFLOW_NOT_COMPLETE;
  nsCSSInlineFrame* nextInFlow = (nsCSSInlineFrame*) mNextInFlow;
  nsIFrame* prevChild = aState.mLastChild;
#ifdef NS_DEBUG
  if (nsnull == prevChild) {
    NS_ASSERTION(nsnull == mFirstChild, "bad prevChild");
    NS_ASSERTION(0 == mChildCount, "bad child count");
  }
  else {
    NS_ASSERTION(nsnull != mFirstChild, "bad prevChild");
    NS_ASSERTION(0 != mChildCount, "bad child count");
  }
#endif
  while (nsnull != nextInFlow) {
    // Get child from our next-in-flow
    nsIFrame* child = PullOneChild(nextInFlow, prevChild);
    if (nsnull == child) {
      nextInFlow = (nsCSSInlineFrame*) nextInFlow->mNextInFlow;
      continue;
    }
    NS_FRAME_TRACE(NS_FRAME_TRACE_PUSH_PULL,
       ("nsCSSInlineFrame::PullUpChildren: trying to pullup %p", child));

    // Reflow it
    nsInlineReflowStatus rs = aState.mInlineLayout.ReflowAndPlaceFrame(child);
    if (IS_REFLOW_ERROR(rs)) {
      reflowResult = rs;
      goto done;
    }
    switch (NS_INLINE_REFLOW_REFLOW_MASK & rs) {
    case NS_INLINE_REFLOW_COMPLETE:
      prevChild = child;
      child->GetNextSibling(child);
      kidContentIndex++;
      mLastContentIsComplete = PR_TRUE;
      break;

    case NS_INLINE_REFLOW_NOT_COMPLETE:
      // We must create the continuation now because our next-in-flow
      // won't know to create it.
      rv = MaybeCreateNextInFlow(aState, child);
      if (NS_OK != rv) {
        reflowResult = nsInlineReflowStatus(rv);
        goto done;
      }
      prevChild = child;
      child->GetNextSibling(child);
      PushKids(aState, prevChild, child);
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_FALSE;
      NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");
      goto done;

    case NS_INLINE_REFLOW_BREAK_AFTER:
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_TRUE;
      NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");
      {
        PRInt32 lastContentIndex;
        lastContentIndex = mContent->ChildCount();
        if (kidContentIndex == lastContentIndex) {
          // All of the children have been pulled up
          reflowResult = NS_INLINE_REFLOW_COMPLETE;
        }
      }
      goto done;

    case NS_INLINE_REFLOW_BREAK_BEFORE:
      mLastContentIsComplete = PR_TRUE;
      if (nsnull != prevChild) {
        mLastContentOffset = kidContentIndex - 1;
        NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");
        PushKids(aState, prevChild, child);
      }
      else {
        // We are trying to push our only child. It must be a block
        // that must be at the start of a line and we must not be at
        // the start of a line.
        NS_NOTYETIMPLEMENTED("XXX");
      }
      goto done;
    }
  }
  if (kidContentIndex == 0) {
    mLastContentOffset = 0;
  } else {
    mLastContentOffset = kidContentIndex - 1;
  }
  NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");
  mLastContentIsComplete = PR_TRUE;
  reflowResult = NS_INLINE_REFLOW_COMPLETE;

done:;
  nextInFlow = (nsCSSInlineFrame*) mNextInFlow;
  if ((nsnull != nextInFlow) && (nsnull == nextInFlow->mFirstChild)) {
    if (NS_INLINE_REFLOW_COMPLETE != reflowResult) {
      // XXX This is only necessary if we want to avoid setting the
      // mFirstContentOffset when we get our first child. I think it's
      // a bad trade.
      AdjustOffsetOfEmptyNextInFlows();
    }
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSInlineFrame::PullUpChildren: childCount=%d reflowResult=%x",
      mChildCount, reflowResult));
  return reflowResult;
}

nsIFrame*
nsCSSInlineFrame::PullOneChild(nsCSSInlineFrame* aNextInFlow,
                               nsIFrame*         aLastChild)
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
       ("nsCSSInlineFrame::PullOneChild: from overflow list, frame=%p",
        kidFrame));
    kidFrame = aNextInFlow->mOverflowList;
    kidFrame->GetNextSibling(aNextInFlow->mOverflowList);
  }
  else {
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
       ("nsCSSInlineFrame::PullOneChild: frame=%p (%d kids left)",
        kidFrame, aNextInFlow->mChildCount - 1));

    // Take the frame away from the next-in-flow. Update it's first
    // content offset and propagate upward the offset if the
    // next-in-flow is a pseudo-frame.
    kidFrame->GetNextSibling(aNextInFlow->mFirstChild);
    aNextInFlow->mChildCount--;
    if (nsnull != aNextInFlow->mFirstChild) {
      aNextInFlow->SetFirstContentOffset(aNextInFlow->mFirstChild);
      if (aNextInFlow->IsPseudoFrame()) {
        aNextInFlow->PropagateContentOffsets();
      }
    }
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
    SetFirstContentOffset(kidFrame);
  } else {
    NS_ASSERTION(IsLastChild(aLastChild), "bad last child");
    aLastChild->SetNextSibling(kidFrame);
  }
  kidFrame->SetNextSibling(nsnull);
  mChildCount++;

  return kidFrame;
}

nsresult
nsCSSInlineFrame::MaybeCreateNextInFlow(nsCSSInlineReflowState& aState,
                                        nsIFrame*               aFrame)
{
  nsIFrame* nextInFlow;
  nsresult rv = aState.mInlineLayout.MaybeCreateNextInFlow(aFrame, nextInFlow);
  if (NS_OK != rv) {
    return rv;
  }
  if (nsnull != nextInFlow) {
    mChildCount++;
  }
  return NS_OK;
}

void
nsCSSInlineFrame::PushKids(nsCSSInlineReflowState& aState,
                           nsIFrame* aPrevChild, nsIFrame* aPushedChild)
{
  // Count how many children are being pushed
  PRInt32 pushCount = mChildCount - aState.mInlineLayout.mFrameNum;

  // Only ask container base class to push children if there is
  // something to push...
  if (0 != pushCount) {
    NS_FRAME_TRACE(NS_FRAME_TRACE_PUSH_PULL,
       ("nsCSSInlineFrame::PushKids: pushing %d children", pushCount));
    PushChildren(aPushedChild, aPrevChild, PR_TRUE/* XXX ignored! */);
    mChildCount -= pushCount;
  }
#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "child count is wrong");
#endif
}
