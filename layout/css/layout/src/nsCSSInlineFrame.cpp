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

// test: <b>...<br></b> : make sure that it works in pullup case and
// non-pullup case

// content appended INCREMENTAL reflow
// content inserted INCREMENTAL reflow
// content deleted INCREMENTAL reflow

nsCSSInlineReflowState::nsCSSInlineReflowState(nsCSSLineLayout& aLineLayout,
                                               nsCSSInlineFrame* aInlineFrame,
                                               nsIStyleContext* aInlineSC,
                                               const nsReflowState& aRS,
                                               PRBool aComputeMaxElementSize)
  : nsReflowState(aRS),
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
                        mNoWrap, aComputeMaxElementSize);
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
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsCSSInlineFrame::CreateContinuingFrame: newFrame=%p", cf));
  return NS_OK;
}

PRBool
nsCSSInlineFrame::DeleteNextInFlowsFor(nsIFrame* aChild)
{
  return DeleteChildsNextInFlow(aChild);
}

NS_IMETHODIMP
nsCSSInlineFrame::FindTextRuns(nsCSSLineLayout&  aLineLayout,
                               nsIReflowCommand* aReflowCommand)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
    ("enter nsCSSInlineFrame::FindTextRuns [%d,%d,%c]",
     mFirstContentOffset, mLastContentOffset, mLastContentIsComplete?'T':'F'));

  nsIFrame* frame;
  PRInt32 n;
  nsresult rv = NS_OK;

  // Gather up children from the overflow lists
  DrainOverflowLists();

  // Create new frames if necessary
  if (NS_FRAME_FIRST_REFLOW & mState) {
    if ((nsnull == mPrevInFlow) && (nsnull == mNextInFlow) &&
        (0 == mChildCount)) {
      rv = CreateNewFrames(aLineLayout.mPresContext);
      if (NS_OK != rv) {
        goto done;
      }
    }
  }
  else if (nsnull != aReflowCommand) {
    nsIFrame* target;
    aReflowCommand->GetTarget(target);
    if (this == target) {
      nsIReflowCommand::ReflowType type;
      aReflowCommand->GetType(type);
      if (nsIReflowCommand::FrameAppended == type) {
        rv = CreateNewFrames(aLineLayout.mPresContext);
        if (NS_OK != rv) {
          goto done;
        }
      }
    }
  }

  // Ask each child frame for its text runs
  frame = mFirstChild;
  n = mChildCount;
  while (--n >= 0) {
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

 done:;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSInlineFrame::FindTextRuns rv=%x [%d,%d,%c]",
      rv, mFirstContentOffset, mLastContentOffset,
      mLastContentIsComplete?'T':'F'));
  return rv;
}

NS_IMETHODIMP
nsCSSInlineFrame::InlineReflow(nsCSSLineLayout&     aLineLayout,
                               nsReflowMetrics&     aMetrics,
                               const nsReflowState& aReflowState)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
    ("enter nsCSSInlineFrame::InlineReflow maxSize=%d,%d reason=%d kids=%d nif=%p [%d,%d,%c]",
     aReflowState.maxSize.width, aReflowState.maxSize.height,
     aReflowState.reason, mChildCount, mNextInFlow,
     mFirstContentOffset, mLastContentOffset, mLastContentIsComplete?'T':'F'));

  // If this is the initial reflow, generate any synthetic content
  // that needs generating.
  if (eReflowReason_Initial == aReflowState.reason) {
    NS_ASSERTION(0 != (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }
  else {
    NS_ASSERTION(0 == (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }

  nsCSSInlineReflowState state(aLineLayout, this, mStyleContext,
                               aReflowState,
                               PRBool(nsnull != aMetrics.maxElementSize));
  nsresult rv = NS_OK;
  if (eReflowReason_Initial == state.reason) {
    DrainOverflowLists();
    rv = InitialReflow(state);
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
      // Get next frame in reflow command chain
      state.reflowCommand->GetNext(state.mInlineLayout.mNextRCFrame);

      // Continue the reflow
      rv = ChildIncrementalReflow(state);
    }
  }
  else if (eReflowReason_Resize == state.reason) {
    DrainOverflowLists();
    rv = ResizeReflow(state);
  }
  ComputeFinalSize(state, aMetrics);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSInlineFrame::InlineReflow size=%d,%d rv=%x kids=%d nif=%p [%d,%d,%c]",
      aMetrics.width, aMetrics.height, rv, mChildCount, mNextInFlow,
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
  if (aState.mInlineLayout.mComputeMaxElementSize) {
    if (aState.mNoWrap) {
      aMetrics.maxElementSize->width = aMetrics.width;
      aMetrics.maxElementSize->height = aMetrics.height;
    }
    else {
      *aMetrics.maxElementSize = aState.mInlineLayout.mMaxElementSize;
    }
  }
}

nsInlineReflowStatus
nsCSSInlineFrame::InitialReflow(nsCSSInlineReflowState& aState)
{
  NS_PRECONDITION(nsnull == mNextInFlow, "bad frame-appended-reflow");
  NS_PRECONDITION(mLastContentIsComplete == PR_TRUE, "bad state");

  nsresult rv = ProcessInitialReflow(aState.mPresContext);
  if (NS_OK != rv) {
    return rv;
  }

#if XXX
  // Create any frames that need creating; note that they should have
  // been created during FindTextRuns which should have been called
  // before this, but we check anyway.
  if ((nsnull == mPrevInFlow) && (nsnull == mNextInFlow) &&
      (0 == mChildCount)) {
    nsresult rv = CreateNewFrames(aState.mPresContext);
    if (NS_OK != rv) {
      return rv;
    }
  }
#endif

  nsInlineReflowStatus rs = NS_FRAME_COMPLETE;
  if (0 != mChildCount) {
    if (!ReflowMapped(aState, rs)) {
      return rs;
    }
  }
  return rs;
}

nsInlineReflowStatus
nsCSSInlineFrame::FrameAppendedReflow(nsCSSInlineReflowState& aState)
{
  NS_PRECONDITION(nsnull == mNextInFlow, "bad frame-appended-reflow");
  NS_PRECONDITION(mLastContentIsComplete == PR_TRUE, "bad state");

#if XXX
  // Create any frames that need creating
  nsresult rv = CreateNewFrames(aState.mPresContext);
  if (NS_OK != rv) {
    return rv;
  }
#endif

  nsInlineReflowStatus rs = NS_FRAME_COMPLETE;
  if (0 != mChildCount) {
    if (!ReflowMapped(aState, rs)) {
      return rs;
    }
  }
  return rs;
}

nsresult
nsCSSInlineFrame::CreateNewFrames(nsIPresContext* aPresContext)
{
  // reason: initial:     (a) null nif, pif: create frames
  //                      (b) pullup: don't create frames
  // reason: incremental: (a) append targetted at us => create frames

  // Get the childPrevInFlow for our eventual first child if we are a
  // continuation and we have no children and the last child in our
  // prev-in-flow is incomplete. While we are at it, we also compute
  // our kidContentIndex.
  PRInt32 kidContentIndex;
  nsIFrame* childPrevInFlow = nsnull;
  nsIFrame* prevChild = nsnull;
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
    LastChild(prevChild);
  }

  nsresult rv;
  PRInt32 lastContentIndex;
  lastContentIndex = mContent->ChildCount();
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsCSSInlineFrame::CreateNewFrames: kidContentIndex=%d lastContentIndex=%d childPrevInFlow=%p",
      kidContentIndex, lastContentIndex, childPrevInFlow));
  while (kidContentIndex < lastContentIndex) {
    nsIContent* kid;
    kid = mContent->ChildAt(kidContentIndex);
    if (nsnull == kid) {
      // Our content container is bad
      break;
    }

    // Create child
    nsIFrame* child;
    rv = nsHTMLBase::CreateFrame(aPresContext, this, kid, childPrevInFlow,
                                 child);
    NS_RELEASE(kid);
    if (NS_OK != rv) {
      return rv;
    }
    if (nsnull == prevChild) {
      mFirstChild = child;
      mFirstContentOffset = kidContentIndex;
    }
    else {
      prevChild->SetNextSibling(child);
    }
    mChildCount++;
    kidContentIndex++;
    childPrevInFlow = nsnull;
    prevChild = child;
    NS_FRAME_TRACE(NS_FRAME_TRACE_NEW_FRAMES,
       ("nsCSSInlineFrame::CreateNewFrames: new-frame=%p prev-in-flow=%p",
        child, childPrevInFlow));
  }
  if (kidContentIndex == 0) {
    NS_ASSERTION(lastContentIndex == 0, "bad kid content index");
    mLastContentOffset = 0;
  } else {
    mLastContentOffset = kidContentIndex - 1;
  }
  NS_ASSERTION(mFirstContentOffset <= mLastContentOffset, "bad fco/lco");
  mLastContentIsComplete = PR_TRUE;

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSInlineFrame::CreateNewFrames"));
  return NS_OK;
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
  nsInlineReflowStatus rs = NS_FRAME_COMPLETE;
  if (0 != mChildCount) {
    if (!ReflowMapped(aState, rs)) {
      return rs;
    }
  }

  if (NS_FRAME_IS_COMPLETE(rs)) {
    // Try to fit some more children from our next in flow
    if (nsnull != mNextInFlow) {
      if (!PullUpChildren(aState, rs)) {
        return rs;
      }
    }
  }

  return rs;
}

PRBool
nsCSSInlineFrame::ReflowMapped(nsCSSInlineReflowState& aState,
                               nsInlineReflowStatus&   aReflowStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsCSSInlineFrame::ReflowMapped: childCount=%d", mChildCount));

  nsresult rv;

  PRBool keepGoing = PR_FALSE;
  PRInt32 kidContentIndex = mFirstContentOffset;
  nsIFrame* prevChild = nsnull;
  nsIFrame* child = mFirstChild;
  while (nsnull != child) {
    aReflowStatus = aState.mInlineLayout.ReflowAndPlaceFrame(child);
    if (NS_IS_REFLOW_ERROR(aReflowStatus)) {
      return PR_FALSE;
    }

    // Update our mLastContentIsComplete flag
    mLastContentIsComplete = NS_FRAME_IS_COMPLETE(aReflowStatus);
    if (!mLastContentIsComplete) {
      // Create a continuation frame for the child now
      rv = MaybeCreateNextInFlow(aState, child);/* XXX rename method */
      if (NS_OK != rv) {
        aReflowStatus = nsInlineReflowStatus(rv);
        return PR_FALSE;
      }
    }
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsCSSInlineFrame::ReflowMapped: frame=%p c=%c kidRS=%x",
      child, mLastContentIsComplete ? 'T' : 'F', aReflowStatus));

    // See if a break is needed and if one is needed, what kind of break
    if (!NS_INLINE_IS_BREAK(aReflowStatus)) {
      // Advance past the child we just reflowed.
      prevChild = child;
      child->GetNextSibling(child);

      // If child is complete then continue reflowing
      if (mLastContentIsComplete) {
        kidContentIndex++;
        continue;
      }

      // When our child is not complete we need to push any children
      // that follow to our next-in-flow.
      PushKids(aState, prevChild, child);
      mLastContentOffset = kidContentIndex;
      goto done;
    }
    else if (NS_INLINE_IS_BREAK_AFTER(aReflowStatus)) {
      // We need to break-after the child. We also need to return
      // the break-after state to our parent because it means our
      // parent needs to break-after us (and so on until the type of
      // break requested is satisfied).
      mLastContentOffset = kidContentIndex;

      // Advance past the child we are breaking after (if it was
      // incomplete then we already created its next-in-flow above)
      prevChild = child;
      child->GetNextSibling(child);
      if (nsnull != child) {
        // Push extra children to a next-in-flow (and therefore we
        // know we aren't complete)
        PushKids(aState, prevChild, child);
        aReflowStatus |= NS_FRAME_NOT_COMPLETE;
      }
      else if (mLastContentIsComplete) {
        // Determine our completion status. We might be complete IFF
        // the child we just reflowed happens to be the last
        // possible child we can contain.
        PRInt32 lastContentIndex;
        lastContentIndex = mContent->ChildCount();
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsCSSInlineFrame::ReflowMapped: HERE kidContentIndex=%d lastContentIndex=%d frame=%p c=%c kidRS=%x",
      kidContentIndex, lastContentIndex, child, mLastContentIsComplete ? 'T' : 'F', aReflowStatus));
        if (++kidContentIndex == lastContentIndex) {
          // We are complete. Yippee. :-)
          aReflowStatus &= ~NS_FRAME_NOT_COMPLETE;
        }
        else {
          // We are not complete. Bummer dude.
          aReflowStatus |= NS_FRAME_NOT_COMPLETE;
        }
      }
      goto done;
    } else {
      // We need to break-before the child. Unlink break-after, a
      // break-before may be changed into just incomplete if there are
      // children preceeding this child (because the other children we
      // have do not need to be broken-before).
      mLastContentIsComplete = PR_TRUE;
      if (nsnull == prevChild) {
        // We are breaking before our very first child
        // frame. Therefore we need return the break-before status as
        // it is. And by definition we are incomplete.
        aReflowStatus |= NS_FRAME_NOT_COMPLETE;
        mLastContentOffset = kidContentIndex;
      }
      else {
        // We have other children before the child that needs the
        // break-before. Therefore map the break-before from the child
        // into a break-after for us, preserving the break-type in the
        // process. This is important when the break type is not just
        // a simple line break.
        aReflowStatus = NS_FRAME_NOT_COMPLETE | NS_INLINE_BREAK |
          NS_INLINE_BREAK_AFTER | (NS_INLINE_BREAK_TYPE_MASK & aReflowStatus);
        mLastContentOffset = kidContentIndex - 1;
        PushKids(aState, prevChild, child);
      }
      goto done;
    }
  }
  if (kidContentIndex == 0) {
    mLastContentOffset = 0;
  } else {
    mLastContentOffset = kidContentIndex - 1;
  }
  mLastContentIsComplete = PR_TRUE;
  keepGoing = PR_TRUE;

done:;
  aState.mLastChild = prevChild;
  NS_POSTCONDITION(mFirstContentOffset <= mLastContentOffset,
                   "bad content offsets");
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSInlineFrame::ReflowMapped: childCount=%d rs=%x",
      mChildCount, aReflowStatus));
  return keepGoing;
}

PRBool
nsCSSInlineFrame::PullUpChildren(nsCSSInlineReflowState& aState,
                                 nsInlineReflowStatus&   aReflowStatus)
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

  PRBool keepGoing = PR_FALSE;
  nsresult rv;
  aReflowStatus = NS_FRAME_COMPLETE;
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
    aReflowStatus = aState.mInlineLayout.ReflowAndPlaceFrame(child);
    if (NS_IS_REFLOW_ERROR(aReflowStatus)) {
      goto done;
    }

    // XXX This code is identical to ReflowMapped; maybe they should
    // be one routine?

    // Update our mLastContentIsComplete flag
    mLastContentIsComplete = NS_FRAME_IS_COMPLETE(aReflowStatus);
    if (!mLastContentIsComplete) {
      // Create a continuation frame for the child now
      rv = MaybeCreateNextInFlow(aState, child);/* XXX rename method */
      if (NS_OK != rv) {
        aReflowStatus = nsInlineReflowStatus(rv);
        return PR_FALSE;
      }
    }

    // See if a break is needed and if one is needed, what kind of break
    if (!NS_INLINE_IS_BREAK(aReflowStatus)) {
      // Advance past the child we just reflowed.
      prevChild = child;
      child->GetNextSibling(child);

      // If child is complete then continue reflowing
      if (mLastContentIsComplete) {
        kidContentIndex++;
        continue;
      }

      // When our child is not complete we need to push any children
      // that follow to our next-in-flow.
      PushKids(aState, prevChild, child);
      mLastContentOffset = kidContentIndex;
      goto done;
    }
    else if (NS_INLINE_IS_BREAK_AFTER(aReflowStatus)) {
      // We need to break-after the child. We also need to return
      // the break-after state to our parent because it means our
      // parent needs to break-after us (and so on until the type of
      // break requested is satisfied).
      mLastContentOffset = kidContentIndex;

      // Advance past the child we are breaking after (if it was
      // incomplete then we already created its next-in-flow above)
      prevChild = child;
      child->GetNextSibling(child);
      if (nsnull != child) {
        // Push extra children to a next-in-flow (and therefore we
        // know we aren't complete)
        PushKids(aState, prevChild, child);
        aReflowStatus |= NS_FRAME_NOT_COMPLETE;
      }
      else if (mLastContentIsComplete) {
        // Determine our completion status. We might be complete IFF
        // the child we just reflowed happens to be the last
        // possible child we can contain.
        PRInt32 lastContentIndex;
        lastContentIndex = mContent->ChildCount();
        if (++kidContentIndex == lastContentIndex) {
          // We are complete. Yippee. :-)
          aReflowStatus &= ~NS_FRAME_NOT_COMPLETE;
        }
        else {
          // We are not complete. Bummer dude.
          aReflowStatus |= NS_FRAME_NOT_COMPLETE;
        }
      }
      goto done;
    } else {
      // We need to break-before the child. Unlink break-after, a
      // break-before may be changed into just incomplete if there are
      // children preceeding this child (because the other children we
      // have do not need to be broken-before).
      if (nsnull == prevChild) {
        // We are breaking before our very first child
        // frame. Therefore we need return the break-before status as
        // it is. And by definition we are incomplete.
        aReflowStatus |= NS_FRAME_NOT_COMPLETE;
        mLastContentOffset = kidContentIndex;
      }
      else {
        // We have other children before the child that needs the
        // break-before. Therefore map the break-before from the child
        // into a break-after for us, preserving the break-type in the
        // process. This is important when the break type is not just
        // a simple line break.
        aReflowStatus = NS_FRAME_NOT_COMPLETE | NS_INLINE_BREAK |
          NS_INLINE_BREAK_AFTER | (NS_INLINE_BREAK_TYPE_MASK & aReflowStatus);
        mLastContentOffset = kidContentIndex - 1;
        PushKids(aState, prevChild, child);
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
  keepGoing = PR_TRUE;

done:;
// XXX Why bother? Assuming our offsets are correct then our
// next-in-flow will pick up where we left off.
#if XXX
  nextInFlow = (nsCSSInlineFrame*) mNextInFlow;
  if ((nsnull != nextInFlow) && (nsnull == nextInFlow->mFirstChild)) {
    if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus)) {
      AdjustOffsetOfEmptyNextInFlows();
    }
  }
#endif

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSInlineFrame::PullUpChildren: childCount=%d rs=%x",
      mChildCount, aReflowStatus));
  return keepGoing;
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
    // content offset.
    kidFrame->GetNextSibling(aNextInFlow->mFirstChild);
    aNextInFlow->mChildCount--;
    if (nsnull != aNextInFlow->mFirstChild) {
      aNextInFlow->SetFirstContentOffset(aNextInFlow->mFirstChild);
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
                           nsIFrame*               aPrevChild,
                           nsIFrame*               aPushedChild)
{
  NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");

  // Count how many children are being pushed
  PRInt32 pushCount = mChildCount - aState.mInlineLayout.mFrameNum;

  // Only ask container base class to push children if there is
  // something to push...
  if (0 != pushCount) {
    NS_FRAME_TRACE(NS_FRAME_TRACE_PUSH_PULL,
       ("nsCSSInlineFrame::PushKids: pushing %d children", pushCount));
    // Break sibling list
    aPrevChild->SetNextSibling(nsnull);
    mChildCount -= pushCount;

    // Place overflow frames on our overflow list; our next-in-flow
    // will pick them up when it is reflowed
    mOverflowList = aPushedChild;
  }
#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "child count is wrong");
#endif
}

void
nsCSSInlineFrame::DrainOverflowLists()
{
  // Our prev-in-flows overflow list goes before my children and must
  // be re-parented.
  if (nsnull != mPrevInFlow) {
    nsCSSInlineFrame* prevInFlow = (nsCSSInlineFrame*) mPrevInFlow;
    if (nsnull != prevInFlow->mOverflowList) {
      nsIFrame* frame = prevInFlow->mOverflowList;
      nsIFrame* lastFrame = nsnull;
      PRInt32 count = 0;
      while (nsnull != frame) {
        // Reparent the frame
        frame->SetGeometricParent(this);
        nsIFrame* contentParent;
        frame->GetContentParent(contentParent);
        if (prevInFlow == contentParent) {
          frame->SetContentParent(this);
        }

        // Advance through the list
        count++;
        lastFrame = frame;
        frame->GetNextSibling(frame);
      }

      // Join the two frame lists together and update our child count
#if XXX
      if (nsnull == mFirstChild) {
        // We are a continuation of our prev-in-flow; we assume that
        // the last child was complete.
        mLastContentIsComplete = PR_TRUE;
      }
#endif
      nsIFrame* newFirstChild = prevInFlow->mOverflowList;
      newFirstChild->GetContentIndex(mFirstContentOffset);
      lastFrame->SetNextSibling(mFirstChild);
      mFirstChild = newFirstChild;
      prevInFlow->mOverflowList = nsnull;
      mChildCount += count;
    }
  }

  // Our overflow list goes to the end of our child list
  if (nsnull != mOverflowList) {
    // Append the overflow list to the end of our child list
    nsIFrame* lastFrame;
    LastChild(lastFrame);
    if (nsnull == lastFrame) {
      mFirstChild = mOverflowList;
      mFirstChild->GetContentIndex(mFirstContentOffset);
    }
    else {
      lastFrame->SetNextSibling(mOverflowList);
    }

    // Count how many frames are on the overflow list and then update
    // our count
    nsIFrame* frame = mOverflowList;
    PRInt32 count = 0;
    while (nsnull != frame) {
      count++;
      frame->GetNextSibling(frame);
    }
    mChildCount += count;
    mOverflowList = nsnull;
#if XXX
    // We get here when we pushed some children to a potential
    // next-in-flow and then our parent decided to reflow us instead
    // of continuing us.
    NS_ASSERTION(nsnull == mNextInFlow, "huh?");
    mLastContentIsComplete = PR_TRUE;
#endif
  }

  // XXX What do we set mLastContentIsComplete to?

#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "child count is wrong");
#endif
}
