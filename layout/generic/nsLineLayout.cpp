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
#include "nsLineLayout.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsBlockFrame.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsIPresContext.h"
#include "nsISpaceManager.h"
#include "nsIPtr.h"
#include "nsAbsoluteFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsCSSLayout.h"
#include "nsCRT.h"
#include "nsReflowCommand.h"
#include "nsIFontMetrics.h"
#include "nsHTMLFrame.h"
#include "nsScrollFrame.h"

#undef NOISY_REFLOW

// XXX zap mAscentNum

NS_DEF_PTR(nsIContent);
NS_DEF_PTR(nsIStyleContext);

nsLineData::nsLineData()
{
  mNextLine = nsnull;
  mPrevLine = nsnull;
  mFirstChild = nsnull;
  mChildCount = 0;
  mFirstContentOffset = 0;
  mLastContentOffset = 0;
  mLastContentIsComplete = PR_TRUE;
  mIsBlock = PR_FALSE;
  mBounds.SetRect(0, 0, 0, 0);
  mFloaters = nsnull;
}

nsLineData::~nsLineData()
{
  delete mFloaters;
}

void
nsLineData::UnlinkLine()
{
  nsLineData* prevLine = mPrevLine;
  nsLineData* nextLine = mNextLine;
  if (nsnull != nextLine) nextLine->mPrevLine = prevLine;
  if (nsnull != prevLine) prevLine->mNextLine = nextLine;
}

nsresult
nsLineData::Verify(PRBool aFinalCheck) const
{
  NS_ASSERTION(mNextLine != this, "bad line linkage");
  NS_ASSERTION(mPrevLine != this, "bad line linkage");
  if (nsnull != mPrevLine) {
    NS_ASSERTION(mPrevLine->mNextLine == this, "bad line linkage");
  }
  if (nsnull != mNextLine) {
    NS_ASSERTION(mNextLine->mPrevLine == this, "bad line linkage");
  }

  if (aFinalCheck) {
    NS_ASSERTION(0 != mChildCount, "empty line");
    NS_ASSERTION(nsnull != mFirstChild, "empty line");

    nsIFrame* nextLinesFirstChild = nsnull;
    if (nsnull != mNextLine) {
      nextLinesFirstChild = mNextLine->mFirstChild;
    }

    // Check that number of children are ok and that the index in parent
    // information agrees with the content offsets.
    PRInt32 offset = mFirstContentOffset;
    PRInt32 len = 0;
    nsIFrame* child = mFirstChild;
    while ((nsnull != child) && (child != nextLinesFirstChild)) {
      PRInt32 indexInParent;
      child->GetContentIndex(indexInParent);
      NS_ASSERTION(indexInParent == offset, "bad line offsets");
      len++;
      if (len != mChildCount) {
        offset++;
      }
      child->GetNextSibling(child);
    }
    NS_ASSERTION(offset == mLastContentOffset, "bad mLastContentOffset");
    NS_ASSERTION(len == mChildCount, "bad child count");
  }

  if (1 == mChildCount) {
    if (mIsBlock) {
      nsIFrame* child = mFirstChild;
      nsIStyleContext* sc;
      child->GetStyleContext(nsnull, sc);
      nsStyleDisplay* display = (nsStyleDisplay*)
        sc->GetData(eStyleStruct_Display);
      NS_ASSERTION((NS_STYLE_DISPLAY_BLOCK == display->mDisplay) ||
                   (NS_STYLE_DISPLAY_LIST_ITEM == display->mDisplay),
                   "bad mIsBlock state");
    }
  }

  // XXX verify content offsets and mLastContentIsComplete
  return NS_OK;
}

nsIFrame*
nsLineData::GetLastChild()
{
  nsIFrame* lastChild = mFirstChild;
  if (mChildCount > 1) {
    for (PRInt32 numKids = mChildCount - 1; --numKids >= 0; ) {
      nsIFrame* nextChild;
      lastChild->GetNextSibling(nextChild);
      lastChild = nextChild;
    }
  }
  return lastChild;
}

PRIntn
nsLineData::GetLineNumber() const
{
  PRIntn lineNumber = 0;
  nsLineData* prev = mPrevLine;
  while (nsnull != prev) {
    lineNumber++;
    prev = prev->mPrevLine;
  }
  return lineNumber;
}

void
nsLineData::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the first/last content offset
  fprintf(out, "line %d [%d,%d,%c] ",
          GetLineNumber(),
          mFirstContentOffset, mLastContentOffset,
          (mLastContentIsComplete ? 'T' : 'F'));

  // Output the bounds rect
  out << mBounds;

  // Output the children, one line at a time
  if (nsnull != mFirstChild) {
    fputs("<\n", out);
    aIndent++;

    nsIFrame* child = mFirstChild;
    for (PRInt32 numKids = mChildCount; --numKids >= 0; ) {
      child->List(out, aIndent);
      child->GetNextSibling(child);
    }

    aIndent--;
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs(">\n", out);
  } else {
    fputs("<>\n", out);
  }
}

//----------------------------------------------------------------------

nsLineLayout::nsLineLayout(nsBlockReflowState& aState)
  : mBlockReflowState(aState)
{
  mBlock = aState.mBlock;
  mSpaceManager = aState.mSpaceManager;
  mBlock->GetContent(mBlockContent);
  mPresContext = aState.mPresContext;
  mUnconstrainedWidth = aState.mUnconstrainedWidth;
  mUnconstrainedHeight = aState.mUnconstrainedHeight;
  mMaxElementSizePointer = aState.mMaxElementSizePointer;

  mAscents = mAscentBuf;
  mMaxAscents = sizeof(mAscentBuf) / sizeof(mAscentBuf[0]);
}

nsLineLayout::~nsLineLayout()
{
  NS_IF_RELEASE(mBlockContent);
  if (mAscents != mAscentBuf) {
    delete [] mAscents;
  }
}

nsresult
nsLineLayout::Initialize(nsBlockReflowState& aState, nsLineData* aLine)
{
  nsresult rv = NS_OK;

  SetReflowSpace(aState.mCurrentBand.availSpace);

  mState.mSkipLeadingWhiteSpace = PR_TRUE;
  mState.mColumn = 0;

  mState.mKidFrame = nsnull;
  mState.mPrevKidFrame = nsnull;
  mState.mKidIndex = aLine->mFirstContentOffset;
  mState.mKidFrameNum = 0;
  mState.mMaxElementSize.width = 0;
  mState.mMaxElementSize.height = 0;
  mState.mMaxAscent = nsnull;
  mState.mMaxDescent = nsnull;

  mSavedState.mKidFrame = nsnull;
  mBreakFrame = nsnull;
  mPendingBreak = NS_STYLE_CLEAR_NONE;

  mLine = aLine;
  mKidPrevInFlow = nsnull;
  mNewFrames = 0;
  mFramesReflowed = 0;

  mMarginApplied = PR_FALSE;

  mMustReflowMappedChildren = PR_FALSE;
  mY = aState.mY;
  mMaxHeight = aState.mAvailSize.height;
  mReflowDataChanged = PR_FALSE;

  mLineHeight = 0;


  return rv;
}

void
nsLineLayout::SetReflowSpace(nsRect& aAvailableSpaceRect)
{
  nscoord x0 = aAvailableSpaceRect.x;
  mLeftEdge = x0;
  mState.mX = x0;
  mRightEdge = x0 + aAvailableSpaceRect.width;

  mMaxWidth = mRightEdge - x0;
  mReflowDataChanged = PR_TRUE;
  mMustReflowMappedChildren = PR_TRUE;
}

nsresult
nsLineLayout::SetAscent(nscoord aAscent)
{
  PRInt32 kidFrameNum = mState.mKidFrameNum;
  if (kidFrameNum == mMaxAscents) {
    mMaxAscents *= 2;
    nscoord* newAscents = new nscoord[mMaxAscents];
    if (nsnull != newAscents) {
      nsCRT::memcpy(newAscents, mAscents, sizeof(nscoord) * kidFrameNum);
      if (mAscents != mAscentBuf) {
        delete [] mAscents;
      }
      mAscents = newAscents;
    } else {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  mAscents[kidFrameNum] = aAscent;
  return NS_OK;
}

void
nsLineLayout::AtSpace()
{
  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsLineLayout::AtSpace: kidFrame=%p kidIndex=%d mX=%d",
                mState.mKidFrame, mState.mKidIndex, mState.mX));
  mBreakFrame = nsnull;
  mSavedState.mKidFrame = nsnull;
}

void
nsLineLayout::AtWordStart(nsIFrame* aFrame, nscoord aX)
{
  if (nsnull == mBreakFrame) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsLineLayout::AtWordStart: aFrame=%p kidFrame=%p[%d] aX=%d mX=%d",
        aFrame,
        mState.mKidFrame, mState.mKidIndex,
        aX, mState.mX));

    mSavedState = mState;
    mBreakFrame = aFrame;
    mBreakX = aX;
  }
  else if (mBreakFrame == aFrame) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::AtWordStart: update aX=%d", aX));
    NS_ASSERTION((mSavedState.mKidFrame == mState.mKidFrame) &&
                 (mSavedState.mKidIndex == mState.mKidIndex) &&
                 (mSavedState.mKidFrameNum == mState.mKidFrameNum),
                 "bad break state");
    mBreakX = aX;
  }
}

PRBool
nsLineLayout::CanBreak()
{
  if (nsnull == mBreakFrame) {
    // There is no word to break at
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::CanBreak: no break frame"));
    return PR_FALSE;
  }

  if (mSavedState.mKidFrame == mLine->mFirstChild) {
    // The line's first frame contains the break position; we are not
    // allowed to break if the break would empty the line.
    if (0 == mBreakX) {
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsLineLayout::CanBreak: breakX=0"));
      return PR_FALSE;
    }
  }

  // Compute the break x coordinate in our coordinate system by adding
  // in the X coordinates of each of the frames between the break
  // frame and the containing block frame.
  nscoord breakX = mBreakX;
  nsIFrame* frame = mBreakFrame;
  for (;;) {
    nsRect r;
    frame->GetRect(r);
    breakX += r.x;
    nsIFrame* parent;
    frame->GetGeometricParent(parent);
    if (parent == mBlock) {
      break;
    }
    frame = parent;
  }

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsLineLayout::CanBreak: backup from=%p[%d]/%d to=%p[%d]/%d breakX=%d",
      mState.mKidFrame, mState.mKidIndex, mState.mKidFrameNum,
      mSavedState.mKidFrame, mSavedState.mKidIndex, mSavedState.mKidFrameNum,
      breakX));

  // Revert the line layout back to where it was when the break point
  // was found.
  mState = mSavedState;

  // Change the right edge to the breakX so that when we reflow the
  // child it will stop just before the break point.
  mRightEdge = breakX;

  // Forgot word break
  mSavedState.mKidFrame = nsnull;
  mBreakFrame = nsnull;

  return PR_TRUE;
}

/**
 * Attempt to avoid reflowing a child by seeing if it's been touched
 * since the last time it was reflowed.
 */
nsresult
nsLineLayout::ReflowMappedChild()
{
  nsIFrame* kidFrame = mState.mKidFrame;

  if (mMustReflowMappedChildren || PR_TRUE) {
/*
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowMappedChild: must reflow frame=%p[%d]",
                  kidFrame, mKidIndex));
*/
    return ReflowChild(nsnull, PR_FALSE);
  }

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsLineLayout::ReflowMappedChild: attempt frame=%p[%d]",
                kidFrame, mState.mKidIndex));

  // If the child is a container then we need to reflow it if there is
  // a change in width. Note that if it's an empty container then it
  // doesn't really matter how much space we give it.
  if (mBlockReflowState.mDeltaWidth != 0) {
    nsIFrame* f;
    kidFrame->FirstChild(f);
    if (nsnull != f) {
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsLineLayout::ReflowMappedChild: has children"));
      return ReflowChild(nsnull, PR_FALSE);
    }
  }

  // If we need the max-element size and we are splittable then we
  // have to reflow to get it.
  nsSplittableType splits;
  kidFrame->IsSplittable(splits);
#if 0
  if (nsnull != mMaxElementSizePointer) {
    if (NS_FRAME_IS_SPLITTABLE(splits)) {
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsLineLayout::ReflowMappedChild: need max-element-size"));
      return ReflowChild(nsnull, PR_FALSE);
    }
  }
#else
  // XXX For now, if the child is splittable we reflow it. The reason
  // is that the text whitespace compression needs to be consulted
  // here to properly handle reflow avoidance. To do that properly we
  // really need a first-rate protocol here (WillPlace?
  // CanAvoidReflow?) that gets the frame involved.
  if (NS_FRAME_IS_SPLITTABLE(splits)) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowMappedChild: splittable hack"));
    return ReflowChild(nsnull, PR_FALSE);
  }
#endif

  nsFrameState state;
  kidFrame->GetFrameState(state);

  // XXX a better term for this is "dirty" and once we add a dirty
  // bit that's what we'll do here.

  // XXX this check will cause pass2 of table reflow to reflow
  // everything; tables will be even faster if we have a dirty bit
  // instead (that way we can avoid reflowing non-splittables on
  // pass2)
  if (0 != (state & NS_FRAME_IN_REFLOW)) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowMappedChild: frame is dirty"));
    return ReflowChild(nsnull, PR_FALSE);
  }

  if (NS_FRAME_IS_SPLITTABLE(splits)) {
    // XXX a next-in-flow propogated dirty-bit eliminates this code

    // The splittable frame has not yet been reflowed. This means
    // that, in theory, its state is well defined. However, if it has
    // a prev-in-flow and that frame has been touched then we need to
    // reflow this frame.
    nsIFrame* prevInFlow;
    kidFrame->GetPrevInFlow(prevInFlow);
    if (nsnull != prevInFlow) {
      nsFrameState prevState;
      prevInFlow->GetFrameState(prevState);
      if (0 != (prevState & NS_FRAME_IN_REFLOW)) {
        NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
           ("nsLineLayout::ReflowMappedChild: prev-in-flow frame is dirty"));
        return ReflowChild(nsnull, PR_FALSE);
      }
    }

    // If the child has a next-in-flow then never-mind, we need to
    // reflow it in case it has more/less space to reflow into.
    nsIFrame* nextInFlow;
    kidFrame->GetNextInFlow(nextInFlow);
    if (nsnull != nextInFlow) {
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
         ("nsLineLayout::ReflowMappedChild: frame has next-in-flow"));
      return ReflowChild(nsnull, PR_FALSE);
    }
  }

  // Success! We have (so far) avoided reflowing the child. However,
  // we do need to place it and advance our position state. Get the
  // size of the child and its reflow metrics for placing.
  nsIStyleContextPtr kidSC;
  nsresult rv = kidFrame->GetStyleContext(mPresContext, kidSC.AssignRef());
  if (NS_OK != rv) {
    return rv;
  }
  nsStyleDisplay* kidDisplay = (nsStyleDisplay*)
    kidSC->GetData(eStyleStruct_Display);
  if (NS_STYLE_FLOAT_NONE != kidDisplay->mFloats) {
    // XXX If it floats it needs to go through the normal path so that
    // PlaceFloater is invoked.
    return ReflowChild(nsnull, PR_FALSE);
  }
  nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
    kidSC->GetData(eStyleStruct_Spacing);
  PRBool isBlock = PR_FALSE;
  switch (kidDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
    if (kidFrame != mLine->mFirstChild) {
      // Block items must be at the start of a line, therefore we need
      // to break before the block item.
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
         ("nsLineLayout::ReflowMappedChild: block requires break-before"));
      return NS_LINE_LAYOUT_BREAK_BEFORE;
    }
    isBlock = PR_TRUE;
    break;
  }

  // Compute new total width of child using its current margin values
  // (they may have changed since the last time the child was reflowed)
  nsRect kidRect;
  kidFrame->GetRect(kidRect);
  nsMargin kidMargin;
  kidSpacing->CalcMarginFor(kidFrame, kidMargin);
  nscoord totalWidth;
  totalWidth = kidMargin.left + kidMargin.right + kidRect.width;

  // If the child intersects the area affected by the reflow then
  // we need to reflow it.
  nscoord x = mState.mX;
  if (x + kidMargin.left + kidRect.width > mRightEdge) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowMappedChild: failed edge test"));
    // XXX if !splittable then return NS_LINE_LAYOUT_BREAK_BEFORE
    return ReflowChild(nsnull, PR_FALSE);
  }

  // Make sure the child will fit. The child always fits if it's the
  // first child on the line.
  nscoord availWidth = mRightEdge - x;
  if (mUnconstrainedWidth ||
      (kidFrame == mLine->mFirstChild) ||
      (totalWidth <= availWidth)) {
    // By convention, mReflowResult is set during ResizeReflow,
    // IncrementalReflow AND GetReflowMetrics by those frames that are
    // line layout aware.
    mReflowResult = NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE;
    nsReflowMetrics kidMetrics(nsnull);
    kidFrame->GetReflowMetrics(mPresContext, kidMetrics);

    nsSize maxElementSize;
    nsSize* kidMaxElementSize = nsnull;
    if (nsnull != mMaxElementSizePointer) {
      kidMaxElementSize = &maxElementSize;
      maxElementSize.width = kidRect.width;
      maxElementSize.height = kidRect.height;
    }
    kidRect.x = x + kidMargin.left;
    kidRect.y = mY;

    if (NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE == mReflowResult) {
      mState.mSkipLeadingWhiteSpace = PR_FALSE;
    }

    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowMappedChild: fit size=%d,%d",
                  kidRect.width, kidRect.height));
    mLine->mIsBlock = isBlock;
    return PlaceChild(kidRect, kidMetrics, kidMaxElementSize, kidMargin,
                      NS_FRAME_COMPLETE);
  }

  // The child doesn't fit as is; if it's splittable then reflow it
  // otherwise return break-before status so that the non-splittable
  // child is pushed to the next line.
  if (NS_FRAME_IS_SPLITTABLE(splits)) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowMappedChild: can't directly fit"));
    return ReflowChild(nsnull, PR_FALSE);
  }
  return NS_LINE_LAYOUT_BREAK_BEFORE;
}

// Return values: <0 for error
// 0 == NS_LINE_LAYOUT
nsresult
nsLineLayout::ReflowChild(nsReflowCommand* aReflowCommand,
                          PRBool aNewChild)
{
  nsIFrame* kidFrame = mState.mKidFrame;

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsLineLayout::ReflowChild: attempt frame=%p[%d] mX=%d availWidth=%d",
      kidFrame, mState.mKidIndex,
      mState.mX, mRightEdge - mState.mX));

  // Get kid frame's style context
  nsIStyleContextPtr kidSC;
  nsresult rv = kidFrame->GetStyleContext(mPresContext, kidSC.AssignRef());
  if (NS_OK != rv) {
    return rv;
  }

  // See if frame belongs in the line.
  // XXX absolute positioning
  // XXX floating frames
  // XXX break-before
  nsStyleDisplay * kidDisplay = (nsStyleDisplay*)
    kidSC->GetData(eStyleStruct_Display);
  PRBool isBlock = PR_FALSE;
  PRBool isFirstChild = PRBool(kidFrame == mLine->mFirstChild);
  switch (kidDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_NONE:
    // Make sure the frame remains zero sized.
    kidFrame->WillReflow(*mPresContext);
    kidFrame->SetRect(nsRect(mState.mX, mY, 0, 0));
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowChild: display=none"));
    return NS_LINE_LAYOUT_COMPLETE;

  case NS_STYLE_DISPLAY_INLINE:
    break;

  default:
    isBlock = PR_TRUE;
    if (!isFirstChild) {
      // XXX Make sure child is dirty for next time
      kidFrame->WillReflow(*mPresContext);
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsLineLayout::ReflowChild: block requires break-before"));
      return NS_LINE_LAYOUT_BREAK_BEFORE;
    }
    break;
  }

  // Get the available size to reflow the child into
  PRBool didBreak = PR_FALSE;
 reflow_it_again_sam:
  nscoord availWidth = mRightEdge - mState.mX;
  nsSize kidAvailSize;
  kidAvailSize.width = availWidth;
  kidAvailSize.height = mMaxHeight;
  nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
    kidSC->GetData(eStyleStruct_Spacing);
  nsMargin kidMargin;
  kidSpacing->CalcMarginFor(kidFrame, kidMargin);
  if (!mUnconstrainedWidth) {
    kidAvailSize.width -= kidMargin.left + kidMargin.right;
    if (!isFirstChild && (kidAvailSize.width <= 0)) {
      // No room.
      if (!didBreak && CanBreak()) {
        kidFrame = mState.mKidFrame;
        didBreak = PR_TRUE;
        goto reflow_it_again_sam;
      }

      // XXX Make sure child is dirty for next time
      kidFrame->WillReflow(*mPresContext);
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsLineLayout::ReflowChild: !fit"));
      return NS_LINE_LAYOUT_BREAK_BEFORE;
    }
  }

  // Reflow the child
  mFramesReflowed++;
  nsRect kidRect;
  nsSize maxElementSize;
  nsSize* kidMaxElementSize = nsnull;
  nsReflowStatus kidReflowStatus;
  if (nsnull != mMaxElementSizePointer) {
    kidMaxElementSize = &maxElementSize;
  }
  nsReflowMetrics kidMetrics(kidMaxElementSize);

  // Get reflow reason set correctly. It's possible that we created a
  // child and then decided that we cannot reflow it (for example, a
  // block frame that isn't at the start of a line). In this case the
  // reason will be wrong so we need to check the frame state.
  nsReflowReason  kidReason = eReflowReason_Resize;
  if (nsnull != aReflowCommand) {
    kidReason = eReflowReason_Incremental;
  }
  else if (aNewChild) {
    kidReason = eReflowReason_Initial;
  }
  else {
    nsFrameState state;
    kidFrame->GetFrameState(state);
    if (NS_FRAME_FIRST_REFLOW & state) {
      kidReason = eReflowReason_Initial;
    }
  }

  nsReflowState   kidReflowState(kidFrame, *mBlockReflowState.reflowState,
                                 kidAvailSize, kidReason);
  kidReflowState.reflowCommand = aReflowCommand;
  mReflowResult = NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE;
  nscoord dx = mState.mX + kidMargin.left;
  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsLineLayout::ReflowChild: reflowing frame into %d,%d",
                kidAvailSize.width, kidAvailSize.height));
  if (isBlock) {
    // Calculate top margin by collapsing with previous bottom margin
    nscoord negTopMargin;
    nscoord posTopMargin;
    nsMargin kidMargin;
    kidSpacing->CalcMarginFor(kidFrame, kidMargin);
    if (kidMargin.top < 0) {
      negTopMargin = -kidMargin.top;
      posTopMargin = 0;
    } else {
      negTopMargin = 0;
      posTopMargin = kidMargin.top;
    }

    // XXX if (nav4_compatability)
    // Calculate the previous margin's positive and negative value.
    // If the current top margin is not defined by css then just use
    // what is cached in the block reflow state. Otherwise if the top
    // margin is defined by css then wipe out any synthetic margin.
    nscoord prevPos = mBlockReflowState.mPrevPosBottomMargin;
    nscoord prevNeg = mBlockReflowState.mPrevNegBottomMargin;
    if (eStyleUnit_Null != kidSpacing->mMargin.GetTopUnit()) {
      if (mBlockReflowState.mPrevMarginSynthetic) {
        prevPos = 0;
        prevNeg = 0;
      }
    }
    else {
      // When our top margin is not specified by css...act like
      // ebina's engine.
      if ((nsnull != mLine->mPrevLine) && !mLine->mPrevLine->mIsBlock) {
        // Supply a default top margin
        nsIStyleContext* blockSC;
        mBlock->GetStyleContext(mPresContext, blockSC);
        nsStyleFont* styleFont = (nsStyleFont*)
          blockSC->GetData(eStyleStruct_Font);
        nsIFontMetrics* fm = mPresContext->GetMetricsFor(styleFont->mFont);
        mBlockReflowState.mPrevNegBottomMargin = 0;
        mBlockReflowState.mPrevPosBottomMargin = fm->GetHeight();
        mBlockReflowState.mPrevMarginSynthetic = PR_TRUE;
        NS_RELEASE(fm);
        NS_RELEASE(blockSC);
      }
    }

    nscoord maxPos = PR_MAX(prevPos, posTopMargin);
    nscoord maxNeg = PR_MAX(prevNeg, negTopMargin);
    nscoord topMargin = maxPos - maxNeg;

    // This is no longer a break point
    mSavedState.mKidFrame = nsnull;
    mBreakFrame = nsnull;

    mY += topMargin;
    mBlockReflowState.mY += topMargin;
    // XXX tell block what topMargin ended up being so that it can
    // undo it if it ends up pushing the line.

    mSpaceManager->Translate(dx, mY);
    kidFrame->WillReflow(*mPresContext);
    kidFrame->MoveTo(dx, mY);
    rv = mBlock->ReflowBlockChild(kidFrame, mPresContext,
                                  mSpaceManager, kidMetrics, kidReflowState,
                                  kidRect, kidReflowStatus);
    mSpaceManager->Translate(-dx, -mY);

    if ((0 == kidRect.width) && (0 == kidRect.height)) {
      // When a block child collapses into nothingness we don't apply
      // it's margins.
      mY -= topMargin;
      mBlockReflowState.mY -= topMargin;
    }
    else {
      // XXX if (nav4_compatability)
      // If the block frame we just reflowed has no bottom margin as
      // specified by css, then we supply our own.
      if (eStyleUnit_Null == kidSpacing->mMargin.GetBottomUnit()) {
        // ebina's engine uses the height of the font for the bottom margin.
        nsIStyleContext* blockSC;
        mBlock->GetStyleContext(mPresContext, blockSC);
        nsStyleFont* styleFont = (nsStyleFont*)
          blockSC->GetData(eStyleStruct_Font);
        nsIFontMetrics* fm = mPresContext->GetMetricsFor(styleFont->mFont);
        mBlockReflowState.mPrevNegBottomMargin = 0;
        mBlockReflowState.mPrevPosBottomMargin = fm->GetHeight();
        mBlockReflowState.mPrevMarginSynthetic = PR_TRUE;
        NS_RELEASE(fm);
        NS_RELEASE(blockSC);
      }
      else {
        // Save away bottom margin information for later
        if (kidMargin.bottom < 0) {
          mBlockReflowState.mPrevNegBottomMargin = -kidMargin.bottom;
          mBlockReflowState.mPrevPosBottomMargin = 0;
        } else {
          mBlockReflowState.mPrevNegBottomMargin = 0;
          mBlockReflowState.mPrevPosBottomMargin = kidMargin.bottom;
        }
        mBlockReflowState.mPrevMarginSynthetic = PR_FALSE;
      }
    }

    kidRect.x = dx;
    kidRect.y = mY;
    kidMetrics.width = kidRect.width;
    kidMetrics.height = kidRect.height;
    kidMetrics.ascent = kidRect.height;
    kidMetrics.descent = 0;
  }
  else {
    // Apply bottom margin speculatively before reflowing the child
    nscoord bottomMargin = mBlockReflowState.mPrevPosBottomMargin -
      mBlockReflowState.mPrevNegBottomMargin;
    if (!mMarginApplied) {
      // Before we place the first inline child on this line apply
      // the previous block's bottom margin.
      mY += bottomMargin;
      mBlockReflowState.mY += bottomMargin;
    }

    kidFrame->WillReflow(*mPresContext);
    kidFrame->MoveTo(dx, mY);
    rv = mBlock->ReflowInlineChild(kidFrame, mPresContext, kidMetrics,
                                   kidReflowState, kidReflowStatus);

    // See if speculative application of the margin should stick
    if (!mMarginApplied) {
      if (0 == kidMetrics.height) {
        // No, undo margin application when we get a zero height child.
        mY -= bottomMargin;
        mBlockReflowState.mY -= bottomMargin;
      }
      else {
        // Yes, keep the margin application.
        mMarginApplied = PR_TRUE;
        mBlockReflowState.mPrevPosBottomMargin = 0;
        mBlockReflowState.mPrevNegBottomMargin = 0;
        // XXX tell block what bottomMargin ended up being so that it can
        // undo it if it ends up pushing the line.
      }
    }

    kidRect.x = dx;
    kidRect.y = mY;
    kidRect.width = kidMetrics.width;
    kidRect.height = kidMetrics.height;
  }
  if (NS_OK != rv) return rv;

  // See if the child fit
  if (kidMetrics.width > kidAvailSize.width) {
    if (!isFirstChild) {
      if (!didBreak && CanBreak()) {
        kidFrame = mState.mKidFrame;
        didBreak = PR_TRUE;
        goto reflow_it_again_sam;
      }
      else {
        // We are out of room.
        // XXX mKidPrevInFlow
        NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                     ("nsLineLayout::ReflowChild: !fit size=%d,%d",
                      kidRect.width, kidRect.height));
        return NS_LINE_LAYOUT_BREAK_BEFORE;
      }
    }
  }
#if 0
  else if (!NS_FRAME_IS_COMPLETE(kidReflowStatus)) {
    // The child seems to have fit. However, it's not complete. See if
    // we need to split at the break frame or not.
    if ((nsnull != mBreakFrame) && (mBreakFrame != mState.mKidFrame)) {
      // We know that we have a break frame.

      // We also know that the break frame is not the frame we just
      // reflow (therefore it's either a previous child or it's a
      // frame inside one of our children).

      // Because the break frame is set we know that the current
      // "word" is not complete. Therefore we must break.

      // XXX This is not quite sufficient for the first-word case: if
      // an inline-frame child decides to stop reflowing because it
      // runs out of space, yet we haven't finished the word (and it
      // might finish the word) the inline-frame will have prematurely
      // stopped!

      if (!didBreak && CanBreak()) {
        kidFrame = mState.mKidFrame;
        didBreak = PR_TRUE;
        goto reflow_it_again_sam;
      }
    }
  }
#endif

  // Non-aware children that take up space act like words; which means
  // that space immediately following them must not be skipped over.
  if (NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE == mReflowResult) {
    if ((kidRect.width != 0) && (kidRect.height != 0)) {
      mState.mSkipLeadingWhiteSpace = PR_FALSE;
    }
  }

  // Now place the child
  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsLineLayout::ReflowChild: fit size=%d,%d",
                kidRect.width, kidRect.height));
  mLine->mIsBlock = isBlock;
  return PlaceChild(kidRect, kidMetrics, kidMaxElementSize,
                    kidMargin, kidReflowStatus);
}

nsresult
nsLineLayout::PlaceChild(nsRect& kidRect,
                         const nsReflowMetrics& kidMetrics,
                         const nsSize* kidMaxElementSize,
                         const nsMargin& kidMargin,
                         nsReflowStatus kidReflowStatus)
{
  nscoord horizontalMargins = 0;

  // Special case to position outside list bullets.
  // XXX RTL bullets
  PRBool isBullet = PR_FALSE;
  if (mBlockReflowState.mListPositionOutside) {
    PRBool isFirstChild = PRBool(mState.mKidFrame == mLine->mFirstChild);
    PRBool isFirstLine = PRBool(nsnull == mLine->mPrevLine);
    if (isFirstChild && isFirstLine) {
      nsIFrame* blockPrevInFlow;
      mBlock->GetPrevInFlow(blockPrevInFlow);
      PRBool isFirstInFlow = PRBool(nsnull == blockPrevInFlow);
      if (isFirstInFlow) {
        isBullet = PR_TRUE;
        // We are placing the first child of the block therefore this
        // is the bullet that is being reflowed. The bullet is placed
        // in the padding area of this block. Don't worry about
        // getting the Y coordinate of the bullet right (vertical
        // alignment will take care of that).

        // Compute gap between bullet and inner rect left edge
        nsIStyleContext* blockCX;
        mBlock->GetStyleContext(mPresContext, blockCX);
        nsStyleFont* font =
          (nsStyleFont*)blockCX->GetData(eStyleStruct_Font);
        NS_RELEASE(blockCX);
        nsIFontMetrics* fm = mPresContext->GetMetricsFor(font->mFont);
        nscoord kidAscent = fm->GetMaxAscent();
        nscoord dx = fm->GetHeight() / 2;  // from old layout engine
        NS_RELEASE(fm);

        // XXX RTL bullets
        kidRect.x = mState.mX - kidRect.width - dx;
        mState.mKidFrame->SetRect(kidRect);
      }
    }
  }
  if (!isBullet) {
    // Place normal in-flow child
    mState.mKidFrame->SetRect(kidRect);

    // Advance
    // XXX RTL
    horizontalMargins = kidMargin.left + kidMargin.right;
    nscoord totalWidth = kidMetrics.width + horizontalMargins;
    mState.mX += totalWidth;
  }

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsLineLayout::PlaceChild: frame=%p[%d] {%d, %d, %d, %d}",
                mState.mKidFrame,
                mState.mKidIndex,
                kidRect.x, kidRect.y, kidRect.width, kidRect.height));

  if (nsnull != mMaxElementSizePointer) {
    // XXX I'm not certain that this is doing the right thing; rethink this
    nscoord elementWidth = kidMaxElementSize->width + horizontalMargins;
    if (elementWidth > mState.mMaxElementSize.width) {
      mState.mMaxElementSize.width = elementWidth;
    }
    if (kidMetrics.height > mState.mMaxElementSize.height) {
      mState.mMaxElementSize.height = kidMetrics.height;
    }
  }
  if (kidMetrics.ascent > mState.mMaxAscent) {
    mState.mMaxAscent = kidMetrics.ascent;
  }
  if (kidMetrics.descent > mState.mMaxDescent) {
    mState.mMaxDescent = kidMetrics.descent;
  }
  SetAscent(mLine->mIsBlock ? 0 : kidMetrics.ascent);

  // Set completion status
  nsresult rv = NS_LINE_LAYOUT_COMPLETE;
  mLine->mLastContentOffset = mState.mKidIndex;
  if (NS_FRAME_IS_COMPLETE(kidReflowStatus)) {
    mLine->mLastContentIsComplete = PR_TRUE;
    if (mLine->mIsBlock ||
        (NS_LINE_LAYOUT_REFLOW_RESULT_BREAK_AFTER == mReflowResult) ||
        (NS_STYLE_CLEAR_NONE != mPendingBreak)) {
      rv = NS_LINE_LAYOUT_BREAK_AFTER;
      if (NS_STYLE_CLEAR_LINE == mPendingBreak) {
        mPendingBreak = NS_STYLE_CLEAR_NONE;
      }
    }
    mKidPrevInFlow = nsnull;
  }
  else {
    mLine->mLastContentIsComplete = PR_FALSE;
    rv = NS_LINE_LAYOUT_NOT_COMPLETE;
    mKidPrevInFlow = mState.mKidFrame;
  }

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsLineLayout::PlaceChild: rv=%d", rv));
  return rv;
}

nsresult
nsLineLayout::IncrementalReflowFromChild(nsReflowCommand* aReflowCommand,
                                         nsIFrame*        aChildFrame)
{
  nsresult reflowStatus = NS_LINE_LAYOUT_COMPLETE;

  mLine->mBounds.x = mState.mX;
  mLine->mBounds.y = mY;
  mState.mKidFrame = mLine->mFirstChild;
  mState.mKidFrameNum = 0;
  while (mState.mKidFrameNum < mLine->mChildCount) {
    nsresult childReflowStatus;
    if (mState.mKidFrame == aChildFrame) {
      childReflowStatus = ReflowChild(aReflowCommand, PR_FALSE);
    } else {
      childReflowStatus = ReflowMappedChild();
    }

    if (childReflowStatus < 0) {
      reflowStatus = childReflowStatus;
      goto done;
    }

    switch (childReflowStatus) {
    default:
    case NS_LINE_LAYOUT_COMPLETE:
      mState.mPrevKidFrame = mState.mKidFrame;
      mState.mKidFrame->GetNextSibling(mState.mKidFrame);
      mState.mKidIndex++;
      mState.mKidFrameNum++;
      break;

    case NS_LINE_LAYOUT_NOT_COMPLETE:
      reflowStatus = childReflowStatus;
      mState.mPrevKidFrame = mState.mKidFrame;
      mState.mKidFrame->GetNextSibling(mState.mKidFrame);
      mState.mKidFrameNum++;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_BEFORE:
      reflowStatus = childReflowStatus;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_AFTER:
      reflowStatus = childReflowStatus;
      mState.mPrevKidFrame = mState.mKidFrame;
      mState.mKidFrame->GetNextSibling(mState.mKidFrame);
      mState.mKidIndex++;
      mState.mKidFrameNum++;

    split_line:
      reflowStatus = SplitLine(childReflowStatus);
      goto done;
    }
  }

done:
  // Perform alignment operations
  AlignChildren();

  // Set final bounds of the line
  mLine->mBounds.height = mLineHeight;
  mLine->mBounds.width = mState.mX - mLine->mBounds.x;

  NS_ASSERTION(((reflowStatus < 0) ||
                (reflowStatus == NS_LINE_LAYOUT_COMPLETE) ||
                (reflowStatus == NS_LINE_LAYOUT_NOT_COMPLETE) ||
                (reflowStatus == NS_LINE_LAYOUT_BREAK_BEFORE) ||
                (reflowStatus == NS_LINE_LAYOUT_BREAK_AFTER)),
               "bad return status from ReflowMapped");
  return reflowStatus;
}

//----------------------------------------------------------------------

nsresult
nsLineLayout::SplitLine(PRInt32 aChildReflowStatus)
{
  PRInt32 pushCount = mLine->mChildCount - mState.mKidFrameNum;
  nsresult rv = NS_LINE_LAYOUT_COMPLETE;

  if (NS_LINE_LAYOUT_NOT_COMPLETE == aChildReflowStatus) {
    // When a line is not complete it indicates that the last child on
    // the line reflowed and took some space but wasn't given enough
    // space to complete. Sometimes when this happens we will need to
    // create a next-in-flow for the child.
    nsIFrame* nextInFlow;
    mState.mPrevKidFrame->GetNextInFlow(nextInFlow);
    if (nsnull == nextInFlow) {
      // Create a continuation frame for the child frame and insert it
      // into our lines child list.
      nsIFrame* nextFrame;
      mState.mPrevKidFrame->GetNextSibling(nextFrame);
      nsIStyleContext* kidSC;
      mState.mPrevKidFrame->GetStyleContext(mPresContext, kidSC);
      mState.mPrevKidFrame->CreateContinuingFrame(mPresContext, mBlock, kidSC,
                                                  nextInFlow);
      NS_RELEASE(kidSC);
      if (nsnull == nextInFlow) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mState.mPrevKidFrame->SetNextSibling(nextInFlow);
      nextInFlow->SetNextSibling(nextFrame);
      mNewFrames++;

      // Add new child to our line
      mLine->mChildCount++;

      // Set mKidFrame to the new next-in-flow so that we will
      // push it when we push children. Increment the number of
      // remaining kids now that there is one more.
      mState.mKidFrame = nextInFlow;
      pushCount++;
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsLineLayout::SplitLine: created next in flow %p",
                    nextInFlow));
    }
  }

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsLineLayout::SplitLine: pushing %d frames",
                pushCount));

  if (0 != pushCount) {
    NS_ASSERTION(nsnull != mState.mKidFrame, "whoops");
    nsLineData* from = mLine;
    nsLineData* to = mLine->mNextLine;
    if (nsnull != to) {
      // Only push into the next line if it's empty; otherwise we can
      // end up pushing a frame which is continued into the same frame
      // as it's continuation. This causes all sorts of side effects
      // so we don't allow it.
      if (to->mChildCount != 0) {
        nsLineData* insertedLine = new nsLineData();
        from->mNextLine = insertedLine;
        to->mPrevLine = insertedLine;
        insertedLine->mPrevLine = from;
        insertedLine->mNextLine = to;
        to = insertedLine;
        to->mLastContentOffset = from->mLastContentOffset;
        to->mLastContentIsComplete = from->mLastContentIsComplete;
      }
    } else {
      to = new nsLineData();
      to->mPrevLine = from;
      from->mNextLine = to;
    }
    if (nsnull == to) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    PRInt32 kidIndexInParent;
    mState.mKidFrame->GetContentIndex(kidIndexInParent);
    to->mFirstChild = mState.mKidFrame;
    to->mChildCount += pushCount;
    to->mFirstContentOffset = kidIndexInParent;

    // The to-line is going to be reflowed therefore it's last content
    // offset and completion status don't matter. In fact, it's expensive
    // to compute them so don't bother.
#ifdef NS_DEBUG
    to->mLastContentOffset = -1;
    to->mLastContentIsComplete = PRPackedBool(0x255);
#endif

    from->mChildCount -= pushCount;
    NS_ASSERTION(0 != from->mChildCount, "bad push");
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      from->Verify();
    }
#endif
#ifdef NOISY_REFLOW
    printf("After push, from-line (%d):\n", pushCount);
    from->List(stdout, 1);
    printf("After push, to-line:\n");
    to->List(stdout, 1);
#endif
  }

  return aChildReflowStatus;
}

//----------------------------------------------------------------------

nsresult
nsLineLayout::ReflowMapped()
{
  nsresult reflowStatus = NS_LINE_LAYOUT_COMPLETE;

  mState.mKidFrame = mLine->mFirstChild;
  mState.mKidFrameNum = 0;
  while (mState.mKidFrameNum < mLine->mChildCount) {
    nsresult childReflowStatus = ReflowMappedChild();
    if (childReflowStatus < 0) {
      reflowStatus = childReflowStatus;
      goto done;
    }
    switch (childReflowStatus) {
    default:
    case NS_LINE_LAYOUT_COMPLETE:
      mState.mPrevKidFrame = mState.mKidFrame;
      mState.mKidFrame->GetNextSibling(mState.mKidFrame);
      mState.mKidIndex++;
      mState.mKidFrameNum++;
      break;

    case NS_LINE_LAYOUT_NOT_COMPLETE:
      reflowStatus = childReflowStatus;
      mState.mPrevKidFrame = mState.mKidFrame;
      mState.mKidFrame->GetNextSibling(mState.mKidFrame);
      mState.mKidFrameNum++;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_BEFORE:
      reflowStatus = childReflowStatus;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_AFTER:
      reflowStatus = childReflowStatus;
      mState.mPrevKidFrame = mState.mKidFrame;
      mState.mKidFrame->GetNextSibling(mState.mKidFrame);
      mState.mKidIndex++;
      mState.mKidFrameNum++;

    split_line:
      reflowStatus = SplitLine(childReflowStatus);
      goto done;
    }
  }

done:
  NS_ASSERTION(((reflowStatus < 0) ||
                (reflowStatus == NS_LINE_LAYOUT_COMPLETE) ||
                (reflowStatus == NS_LINE_LAYOUT_NOT_COMPLETE) ||
                (reflowStatus == NS_LINE_LAYOUT_BREAK_BEFORE) ||
                (reflowStatus == NS_LINE_LAYOUT_BREAK_AFTER)),
               "bad return status from ReflowMapped");
  return reflowStatus;
}

//----------------------------------------------------------------------

static PRBool
IsBlock(nsStyleDisplay* aDisplay)
{
  switch (aDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
    return PR_TRUE;
  }
  return PR_FALSE;
}

// XXX fix this code to look at the available width and if it's too
// small for the next child then skip the pullup (and return
// BREAK_AFTER status).

nsresult
nsLineLayout::PullUpChildren()
{
  nsresult reflowStatus = NS_LINE_LAYOUT_COMPLETE;
  nsIFrame* prevKidFrame = mState.mPrevKidFrame;

  nsBlockFrame* currentBlock = mBlock;
  nsLineData* line = mLine->mNextLine;
  while (nsnull != currentBlock) {

    // Pull children from the next line
    while (nsnull != line) {
      // Get first child from next line
      mState.mKidFrame = line->mFirstChild;
      if (nsnull == mState.mKidFrame) {
        NS_ASSERTION(0 == line->mChildCount, "bad line list");
        nsLineData* nextLine = line->mNextLine;
        nsLineData* prevLine = line->mPrevLine;
        if (nsnull != prevLine) prevLine->mNextLine = nextLine;
        if (nsnull != nextLine) nextLine->mPrevLine = prevLine;
        delete line;/* XXX free-list in block-reflow-state? */
        line = nextLine;
        continue;
      }

      // XXX Avoid the pullup work if the child cannot already fit
      // (e.g. it's not splittable and can't fit)

      // XXX change this to use the next-line's mIsBlock when possible
      // (make sure push code set it's properly for this to work: only
      // from reflow-unmapped)

      // If the child is a block element then if this is not the first
      // line in the block or if it's the first line and it's not the
      // first child in the line then we cannot pull-up the child.
      nsresult rv;
      nsIStyleContextPtr kidSC;
      rv = mState.mKidFrame->GetStyleContext(mPresContext, kidSC.AssignRef());
      if (NS_OK != rv) {
        return rv;
      }
      nsStyleDisplay* kidDisplay = (nsStyleDisplay*)
        kidSC->GetData(eStyleStruct_Display);
      if (IsBlock(kidDisplay)) {
        if ((nsnull != mLine->mPrevLine) || (0 != mLine->mChildCount)) {
          goto done;
        }
      }

      // Make pulled child part of this line
      NS_FRAME_LOG(NS_FRAME_TRACE_PUSH_PULL,
                   ("nsLineLayout::PullUpChildren: trying to pull frame=%p",
                    mState.mKidFrame));
      mLine->mChildCount++;
      if (0 == --line->mChildCount) {
        // Remove empty lines from the list
        nsLineData* nextLine = line->mNextLine;
        nsLineData* prevLine = line->mPrevLine;
        if (nsnull != prevLine) prevLine->mNextLine = nextLine;
        if (nsnull != nextLine) nextLine->mPrevLine = prevLine;
        delete line;/* XXX free-list in block-reflow-state? */
        line = nextLine;
      }
      else {
        // Repair the first content offset of the line. The first
        // child of the line's index-in-parent should be the line's
        // new first content offset.
        mState.mKidFrame->GetNextSibling(line->mFirstChild);
        PRInt32 indexInParent;
        line->mFirstChild->GetContentIndex(indexInParent);
        line->mFirstContentOffset = indexInParent;
#ifdef NS_DEBUG
        if (nsIFrame::GetVerifyTreeEnable()) {
          line->Verify();
        }
#endif
      }

      // Try to reflow it like any other mapped child
      nsresult childReflowStatus = ReflowMappedChild();
      if (childReflowStatus < 0) {
        reflowStatus = childReflowStatus;
        goto done;
      }
      switch (childReflowStatus) {
      default:
      case NS_LINE_LAYOUT_COMPLETE:
        mState.mPrevKidFrame = mState.mKidFrame;
        mState.mKidFrame = nsnull;
        mState.mKidIndex++;
        mState.mKidFrameNum++;
        break;

      case NS_LINE_LAYOUT_NOT_COMPLETE:
        reflowStatus = childReflowStatus;
        mState.mPrevKidFrame = mState.mKidFrame;
        mState.mKidFrame = nsnull;
        mState.mKidFrameNum++;
        goto split_line;

      case NS_LINE_LAYOUT_BREAK_BEFORE:
        reflowStatus = childReflowStatus;
        goto split_line;

      case NS_LINE_LAYOUT_BREAK_AFTER:
        reflowStatus = childReflowStatus;
        mState.mPrevKidFrame = mState.mKidFrame;
        mState.mKidFrame = nsnull;
        mState.mKidIndex++;
        mState.mKidFrameNum++;

      split_line:
        reflowStatus = SplitLine(childReflowStatus);
        goto done;
      }
    }

    // Grab the block's next in flow
    nsIFrame* nextInFlow;
    currentBlock->GetNextInFlow(nextInFlow);
    currentBlock = (nsBlockFrame*)nextInFlow;
    if (nsnull != currentBlock) {
      line = currentBlock->GetFirstLine();
    }
  }

done:
  NS_ASSERTION(((reflowStatus < 0) ||
                (reflowStatus == NS_LINE_LAYOUT_COMPLETE) ||
                (reflowStatus == NS_LINE_LAYOUT_NOT_COMPLETE) ||
                (reflowStatus == NS_LINE_LAYOUT_BREAK_BEFORE) ||
                (reflowStatus == NS_LINE_LAYOUT_BREAK_AFTER)),
               "bad return status from PullUpChildren");
  return reflowStatus;
}

//----------------------------------------------------------------------

nsresult
nsLineLayout::CreateFrameFor(nsIContent* aKid)
{
  nsIStyleContextPtr kidSC =
    mPresContext->ResolveStyleContextFor(aKid, mBlock);  // XXX bad API
  if (nsnull == kidSC) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsStylePosition* kidPosition = (nsStylePosition*)
    kidSC->GetData(eStyleStruct_Position);
  nsStyleDisplay* kidDisplay = (nsStyleDisplay*)
    kidSC->GetData(eStyleStruct_Display);

  // Check whether it wants to floated or absolutely positioned
  PRBool isBlock = PR_FALSE;
  nsIFrame* kidFrame;
  nsresult rv;
  if (NS_STYLE_POSITION_ABSOLUTE == kidPosition->mPosition) {
    rv = nsAbsoluteFrame::NewFrame(&kidFrame, aKid, mBlock);
    if (NS_OK == rv) {
      kidFrame->SetStyleContext(mPresContext, kidSC);
    }
  }
  else if (NS_STYLE_FLOAT_NONE != kidDisplay->mFloats) {
    rv = nsPlaceholderFrame::NewFrame(&kidFrame, aKid, mBlock);
    if (NS_OK == rv) {
      kidFrame->SetStyleContext(mPresContext, kidSC);
    }
  }
  else if ((NS_STYLE_OVERFLOW_SCROLL == kidDisplay->mOverflow) ||
           (NS_STYLE_OVERFLOW_AUTO == kidDisplay->mOverflow)) {
    rv = NS_NewScrollFrame(&kidFrame, aKid, mBlock);
    if (NS_OK == rv) {
      kidFrame->SetStyleContext(mPresContext, kidSC);
    }
  }
  else if (nsnull == mKidPrevInFlow) {
    // Create initial frame for the child
    nsIContentDelegate* kidDel;
    switch (kidDisplay->mDisplay) {
    case NS_STYLE_DISPLAY_NONE:
      rv = nsFrame::NewFrame(&kidFrame, aKid, mBlock);
      if (NS_OK == rv) {
        kidFrame->SetStyleContext(mPresContext, kidSC);
      }
      break;

    case NS_STYLE_DISPLAY_BLOCK:
    case NS_STYLE_DISPLAY_LIST_ITEM:
      isBlock = PR_TRUE;
      // FALL THROUGH
    default:
      kidDel = aKid->GetDelegate(mPresContext);
      rv = kidDel->CreateFrame(mPresContext, aKid, mBlock, kidSC, kidFrame);
      NS_RELEASE(kidDel);
      break;
    }
  } else {
    // Since kid has a prev-in-flow, use that to create the next
    // frame.
    rv = mKidPrevInFlow->CreateContinuingFrame(mPresContext, mBlock, kidSC,
                                               kidFrame);
    NS_ASSERTION(0 == mLine->mChildCount, "bad continuation");
  }
  if (NS_OK != rv) {
    return rv;
  }
  if (NS_OK == rv) {
    // Wrap the frame in a view if necessary
    rv = nsHTMLFrame::CreateViewForFrame(mPresContext, kidFrame, kidSC,
                                         PR_FALSE);
    if (NS_OK != rv) {
      return rv;
    }
  }

  mState.mKidFrame = kidFrame;
  mNewFrames++;

  if (isBlock && (0 != mLine->mChildCount)) {
    // When we are not at the start of a line we need to break
    // before a block element.
    return NS_LINE_LAYOUT_BREAK_BEFORE;
  }

  return rv;
}

nsresult
nsLineLayout::ReflowUnmapped()
{
  nsresult reflowStatus = NS_LINE_LAYOUT_COMPLETE;
  for (;;) {
    nsIContentPtr kid = mBlockContent->ChildAt(mState.mKidIndex);
    if (kid.IsNull()) {
      break;
    }

    // Create a frame for the new content
    nsresult rv = CreateFrameFor(kid);
    if (rv < 0) {
      reflowStatus = rv;
      goto done;
    }

    // Add frame to our list
    if (nsnull != mState.mPrevKidFrame) {
      mState.mPrevKidFrame->SetNextSibling(mState.mKidFrame);
    }
    if (0 == mLine->mChildCount) {
      mLine->mFirstChild = mState.mKidFrame;
      mBlock->SetFirstChild(mState.mKidFrame);
    }
    mLine->mChildCount++;

    nsresult childReflowStatus;
    if (rv == NS_LINE_LAYOUT_BREAK_BEFORE) {
      // If we break before a frame is even supposed to layout then we
      // need to split the line.
      childReflowStatus = rv;

      // XXX Mark new frame dirty so it gets reflow later on
      mState.mKidFrame->WillReflow(*mPresContext);
      goto split_line;
    }

    // Reflow new child frame
    childReflowStatus = ReflowChild(nsnull, PR_TRUE);
    if (childReflowStatus < 0) {
      reflowStatus = childReflowStatus;
      goto done;
    }
    switch (childReflowStatus) {
    default:
    case NS_LINE_LAYOUT_COMPLETE:
      mState.mPrevKidFrame = mState.mKidFrame;
      mState.mKidFrame = nsnull;
      mState.mKidFrameNum++;
      mState.mKidIndex++;
      break;

    case NS_LINE_LAYOUT_NOT_COMPLETE:
      reflowStatus = childReflowStatus;
      mState.mPrevKidFrame = mState.mKidFrame;
      mState.mKidFrame = nsnull;
      mState.mKidFrameNum++;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_BEFORE:
      reflowStatus = childReflowStatus;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_AFTER:
      reflowStatus = childReflowStatus;
      mState.mPrevKidFrame = mState.mKidFrame;
      mState.mKidFrame = nsnull;
      mState.mKidFrameNum++;
      mState.mKidIndex++;

    split_line:
      reflowStatus = SplitLine(childReflowStatus);
      goto done;
    }
  }
  NS_ASSERTION(nsnull == mLine->mNextLine, "bad line list");

done:
  NS_ASSERTION(((reflowStatus < 0) ||
                (reflowStatus == NS_LINE_LAYOUT_COMPLETE) ||
                (reflowStatus == NS_LINE_LAYOUT_NOT_COMPLETE) ||
                (reflowStatus == NS_LINE_LAYOUT_BREAK_BEFORE) ||
                (reflowStatus == NS_LINE_LAYOUT_BREAK_AFTER)),
               "bad return status from ReflowUnmapped");
  return reflowStatus;
}

//----------------------------------------------------------------------

nsresult
nsLineLayout::ReflowLine()
{
  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
     ("enter nsLineLayout::ReflowLine: childCount=%d {%d, %d, %d, %d}",
      mLine->mChildCount,
      mLine->mBounds.x, mLine->mBounds.y,
      mLine->mBounds.width, mLine->mBounds.height));

  nsresult rv = NS_LINE_LAYOUT_COMPLETE;

  mLine->mBounds.x = mState.mX;
  mLine->mBounds.y = mY;
  mOldChildCount = mLine->mChildCount;

  // Reflow the mapped frames
  if (0 != mLine->mChildCount) {
    rv = ReflowMapped();
    if (rv < 0) return rv;
  }

  // Pull-up any frames from the next line
  if (NS_LINE_LAYOUT_COMPLETE == rv) {
    if (nsnull != mLine->mNextLine) {
      rv = PullUpChildren();
      if (rv < 0) return rv;
    }

    // Try reflowing any unmapped children
    if (NS_LINE_LAYOUT_COMPLETE == rv) {
      if (nsnull == mLine->mNextLine) {
        rv = ReflowUnmapped();
        if (rv < 0) return rv;
      }
    }
  }

  // Perform alignment operations
  AlignChildren();

  // Set final bounds of the line
  mLine->mBounds.height = mLineHeight;
  mLine->mBounds.width = mState.mX - mLine->mBounds.x;

#ifdef NS_DEBUG
  if (nsIFrame::GetVerifyTreeEnable()) {
    mLine->Verify();
  }
#endif
  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
     ("exit nsLineLayout::ReflowLine: childCount=%d {%d, %d, %d, %d}",
      mLine->mChildCount,
      mLine->mBounds.x, mLine->mBounds.y,
      mLine->mBounds.width, mLine->mBounds.height));
  return rv;
}

void
nsLineLayout::AlignChildren()
{
  NS_PRECONDITION(mLine->mChildCount == mState.mKidFrameNum, "bad line reflow");

  // Block lines don't require (or allow!) alignment
  if (mLine->mIsBlock) {
    mLineHeight = mState.mMaxAscent + mState.mMaxDescent;
  }
  else {
    // Avoid alignment when we didn't actually reflow any frames and we
    // also didn't change the number of frames we had (which means the
    // pullup code didn't pull anything up). When this happens it means
    // that nothing changed which means that we can avoid the alignment
    // work.
    if ((0 == mFramesReflowed) && (mOldChildCount == mLine->mChildCount)) {
      mLineHeight = mLine->mBounds.height;
    }
  }

  nsIStyleContextPtr blockSC;
  mBlock->GetStyleContext(mPresContext, blockSC.AssignRef());
  nsStyleFont* blockFont = (nsStyleFont*)
    blockSC->GetData(eStyleStruct_Font);
  nsStyleText* blockText = (nsStyleText*)
    blockSC->GetData(eStyleStruct_Text);
  nsStyleDisplay* blockDisplay = (nsStyleDisplay*)
    blockSC->GetData(eStyleStruct_Display);

  // First vertically align the children on the line; this will
  // compute the actual line height for us.
  if (!mLine->mIsBlock) {
    mLineHeight =
      nsCSSLayout::VerticallyAlignChildren(mPresContext, mBlock, blockFont,
                                           mY,
                                           mLine->mFirstChild,
                                           mLine->mChildCount,
                                           mAscents, mState.mMaxAscent); 
  }

  // Now horizontally place the children
  nsCSSLayout::HorizontallyPlaceChildren(mPresContext, mBlock,
                                         blockText->mTextAlign,
                                         blockDisplay->mDirection,
                                         mLine->mFirstChild,
                                         mLine->mChildCount,
                                         mState.mX - mLeftEdge,
                                         mMaxWidth);

  // Last, apply relative positioning
  nsCSSLayout::RelativePositionChildren(mPresContext, mBlock,
                                        mLine->mFirstChild,
                                        mLine->mChildCount);
}
