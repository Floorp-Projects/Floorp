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

#include "nsHTMLBase.h"// XXX rename to nsCSSBase?

// XXX
static NS_DEFINE_IID(kIInlineReflowIID, NS_IINLINE_REFLOW_IID);

nsCSSInlineReflowState::nsCSSInlineReflowState(nsCSSLineLayout& aLineLayout,
                                               nsCSSInlineFrame* aInlineFrame,
                                               nsIStyleContext* aInlineSC,
                                               const nsReflowState& aRS,
                                               nsSize* aMaxElementSize)
  : nsReflowState(aInlineFrame, *aRS.parentReflowState, aRS.maxSize),
    mInlineLayout(aLineLayout, aInlineFrame, aInlineSC)
{
  mPresContext = aLineLayout.mPresContext;
  mLastChild = nsnull;
  mKidContentIndex = aInlineFrame->GetFirstContentOffset();

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

  if (eReflowReason_Incremental != aReflowState.reason) {
    MoveOverflowToChildList();
  }

  nsCSSInlineReflowState state(aLineLayout, this, mStyleContext,
                               aReflowState, aMetrics.maxElementSize);
  nsresult rv = NS_OK;
  if (eReflowReason_Initial == state.reason) {
    rv = FrameAppendedReflow(state);
  }
  else if (eReflowReason_Incremental == state.reason) {
    nsIFrame* target;
    state.reflowCommand->GetTarget(target);
    if (this == target) {
      nsIReflowCommand::ReflowType type;
      state.reflowCommand->GetType(type);
      switch (type) {
      case nsIReflowCommand::FrameAppended:
        //XXX RecoverState(state);
        rv = FrameAppendedReflow(state);
        break;

      default:
        NS_NOTYETIMPLEMENTED("XXX");
      }
    }
    else {
      rv = ChildIncrementalReflow(state);
    }
  }
  else if (eReflowReason_Resize == state.reason) {
    rv = ResizeReflow(state);
  }
  ComputeFinalSize(state, aMetrics);

#ifdef NS_DEBUG
  NS_ASSERTION(LengthOf(mFirstChild) == mChildCount, "bad child count");
#endif
  return rv;
}

void
nsCSSInlineFrame::ComputeFinalSize(nsCSSInlineReflowState& aState,
                                   nsReflowMetrics& aMetrics)
{
  // Compute default size
  nsRect bounds;
  aState.mInlineLayout.mAvailWidth =
    aState.mInlineLayout.mX - aState.mInlineLayout.mLeftEdge;
  nscoord lineHeight =
    aState.mInlineLayout.AlignFrames(mFirstChild, mChildCount, bounds);

  aMetrics.width = aState.mBorderPadding.left +
    (aState.mInlineLayout.mX - aState.mInlineLayout.mLeftEdge) +
    aState.mBorderPadding.right;
  aMetrics.ascent = aState.mBorderPadding.top +
    aState.mInlineLayout.mMaxAscent;
  aMetrics.descent = aState.mInlineLayout.mMaxDescent +
    aState.mBorderPadding.bottom;
  aMetrics.height = aMetrics.ascent + aMetrics.descent;

  // Apply width/height style properties
  PRIntn ss = aState.mStyleSizeFlags;
  if (0 != (ss & NS_SIZE_HAS_WIDTH)) {
    aMetrics.width = aState.mBorderPadding.left + aState.mStyleSize.width +
      aState.mBorderPadding.right;
  }
  if (0 != (ss & NS_SIZE_HAS_HEIGHT)) {
    aMetrics.height = aState.mBorderPadding.top + aState.mStyleSize.height +
      aState.mBorderPadding.bottom;
  }

  if ((nsnull != aMetrics.maxElementSize) && aState.mNoWrap) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }
}

nsInlineReflowStatus
nsCSSInlineFrame::FrameAppendedReflow(nsCSSInlineReflowState& aState)
{
  // XXX Reflow existing children to recover our reflow state
  nsInlineReflowStatus rv = NS_INLINE_REFLOW_COMPLETE;
  if (0 != mChildCount) {
    rv = ReflowMapped(aState);
    if (IS_REFLOW_ERROR(rv)) {
      return rv;
    }
  }
  if (NS_INLINE_REFLOW_COMPLETE == (NS_INLINE_REFLOW_REFLOW_MASK & rv)) {
    // Reflow new children
    rv = ReflowUnmapped(aState);
    // Note: There is nothing to pullup because this is a
    // frame-appended reflow (therefore we are the last-in-flow)
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
  return rv;
}

// XXX: this code is lazy about setting mLastContentIsComplete; it
// can be made more efficient after very careful thought; the initial
// value for mLastContentIsComplete upon entry is undefined (both
// values are legal).

nsInlineReflowStatus
nsCSSInlineFrame::ReflowMapped(nsCSSInlineReflowState& aState)
{
  nsresult rv = NS_INLINE_REFLOW_NOT_COMPLETE;
  PRInt32 n = mChildCount;
  PRInt32 kidContentIndex = mFirstContentOffset;
  nsIFrame* prevChild = nsnull;
  nsIFrame* child = mFirstChild;
  while (--n >= 0) {
    nsInlineReflowStatus rs = aState.mInlineLayout.ReflowAndPlaceFrame(child);
    if (IS_REFLOW_ERROR(rs)) {
      rv = nsresult(rs);
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
      prevChild = child;
      child->GetNextSibling(child);
      PushKids(aState, prevChild, child);
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_FALSE;
      goto done;

    case NS_INLINE_REFLOW_BREAK_AFTER:
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_TRUE;

      // We might have children following this breaking child (for
      // example, somebody inserted an HTML BR in the middle of
      // us). If we do have more children they need to be pushed to a
      // next-in-flow.
      if (n != 0) {
        prevChild = child;
        child->GetNextSibling(child);
        PushKids(aState, prevChild, child);
      }
      else {
        rv = NS_INLINE_REFLOW_COMPLETE;
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
  NS_ASSERTION(kidContentIndex > 0, "bad kid content index");
  mLastContentOffset = kidContentIndex - 1;
  mLastContentIsComplete = PR_TRUE;
  aState.mKidContentIndex = kidContentIndex;
  aState.mLastChild = prevChild;
  rv = NS_INLINE_REFLOW_COMPLETE;

done:;
  return rv;
}

// XXX: this code is lazy about setting mLastContentIsComplete; it
// can be made more efficient after very careful thought; the initial
// value for mLastContentIsComplete upon entry is undefined (both
// values are legal).

nsInlineReflowStatus
nsCSSInlineFrame::ReflowUnmapped(nsCSSInlineReflowState& aState)
{
  NS_PRECONDITION(mLastContentIsComplete == PR_TRUE, "huh?");

  nsresult rv = NS_INLINE_REFLOW_NOT_COMPLETE;
  PRInt32 kidContentIndex = aState.mKidContentIndex;

  // Get the childPrevInFlow for our eventual first child if we are a
  // continuation and we have no children and the last child in our
  // prev-in-flow is incomplete.
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

    // Reflow the child
    nsInlineReflowStatus rs = aState.mInlineLayout.ReflowAndPlaceFrame(child);
    if (IS_REFLOW_ERROR(rs)) {
      rv = nsresult(rs);
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
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_FALSE;
      goto done;

    case NS_INLINE_REFLOW_BREAK_AFTER:
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_TRUE;
      if (++kidContentIndex == lastContentIndex) {
        rv = NS_INLINE_REFLOW_COMPLETE;
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
  NS_ASSERTION(kidContentIndex > 0, "bad kid content index");
  mLastContentOffset = kidContentIndex - 1;
  mLastContentIsComplete = PR_TRUE;
  rv = NS_INLINE_REFLOW_COMPLETE;

done:;
  return rv;
}

// XXX: this code is lazy about setting mLastContentIsComplete; it
// can be made more efficient after very careful thought; the initial
// value for mLastContentIsComplete upon entry is undefined (both
// values are legal).

nsInlineReflowStatus
nsCSSInlineFrame::PullUpChildren(nsCSSInlineReflowState& aState)
{
  nsresult rv = NS_INLINE_REFLOW_NOT_COMPLETE;
  PRInt32 kidContentIndex = aState.mKidContentIndex;
  PRInt32 lastContentIndex;
  lastContentIndex = mContent->ChildCount();
  nsCSSInlineFrame* nextInFlow = (nsCSSInlineFrame*) mNextInFlow;
  nsIFrame* prevChild = aState.mLastChild;
  while (nsnull != nextInFlow) {
    // Get child from our next-in-flow
    nsIFrame* child = PullUpOneChild(nextInFlow, prevChild);
    if (nsnull == child) {
      nextInFlow = (nsCSSInlineFrame*) nextInFlow->mNextInFlow;
      continue;
    }

    // Reflow it
    nsInlineReflowStatus rs = aState.mInlineLayout.ReflowAndPlaceFrame(child);
    if (IS_REFLOW_ERROR(rs)) {
      rv = nsresult(rs);
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
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_FALSE;
      goto done;

    case NS_INLINE_REFLOW_BREAK_AFTER:
      mLastContentOffset = kidContentIndex;
      mLastContentIsComplete = PR_TRUE;
      if (kidContentIndex == lastContentIndex) {
        // All of the children have been pulled up
        rv = NS_INLINE_REFLOW_COMPLETE;
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
  mLastContentIsComplete = PR_TRUE;
  rv = NS_INLINE_REFLOW_COMPLETE;

done:;
  return rv;
}

void
nsCSSInlineFrame::PushKids(nsCSSInlineReflowState& aState,
                           nsIFrame* aPrevChild, nsIFrame* aPushedChild)
{
  // Count how many children are being pushed
  PRInt32 pushCount = 0;
  nsIFrame* child = aPushedChild;
  while (nsnull != child) {
    child->GetNextSibling(child);
    pushCount++;
  }
  NS_ASSERTION(pushCount == mChildCount - aState.mInlineLayout.mFrameNum,
               "whoops");

  // Only ask container base class to push children if there is
  // something to push...
  if (0 != pushCount) {
    PushChildren(aPushedChild, aPrevChild, PR_TRUE/* XXX ignored! */);
    mChildCount -= pushCount;
  }
}
