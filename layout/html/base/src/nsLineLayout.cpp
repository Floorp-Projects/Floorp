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

#undef NOISY_REFLOW

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
  mHasBullet = PR_FALSE;
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
    if (mHasBullet) {
      // Skip bullet
      child->GetNextSibling(child);
      len++;
    }
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

  mLine = aLine;
  mKidPrevInFlow = nsnull;
  mNewFrames = 0;
  mFramesReflowed = 0;
  mKidIndex = aLine->mFirstContentOffset;

  mReflowData.mMaxElementSize.width = 0;
  mReflowData.mMaxElementSize.height = 0;
  mReflowData.mMaxAscent = nsnull;
  mReflowData.mMaxDescent = nsnull;
  mMarginApplied = PR_FALSE;

  SetReflowSpace(aState.mCurrentBand.availSpace);
  mMustReflowMappedChildren = PR_FALSE;
  mY = aState.mY;
  mMaxHeight = aState.mAvailSize.height;
  mReflowDataChanged = PR_FALSE;

  mLineHeight = 0;
  mAscentNum = 0;

  mKidFrame = nsnull;
  mPrevKidFrame = nsnull;

  mWordStart = nsnull;
  mWordStartParent = nsnull;
  mWordStartOffset = 0;

  mSkipLeadingWhiteSpace = PR_TRUE;
  mColumn = 0;

  return rv;
}

void
nsLineLayout::SetReflowSpace(nsRect& aAvailableSpaceRect)
{
  mReflowData.mX = aAvailableSpaceRect.x;
  mReflowData.mAvailWidth = aAvailableSpaceRect.width;
  mX0 = mReflowData.mX;
  mMaxWidth = mReflowData.mAvailWidth;
  mNewRightEdge = mX0 + mMaxWidth;
  mReflowDataChanged = PR_TRUE;
  mMustReflowMappedChildren = PR_TRUE;
}

nsresult
nsLineLayout::AddAscent(nscoord aAscent)
{
  if (mAscentNum == mMaxAscents) {
    mMaxAscents *= 2;
    nscoord* newAscents = new nscoord[mMaxAscents];
    if (nsnull != newAscents) {
      nsCRT::memcpy(newAscents, mAscents, sizeof(nscoord) * mAscentNum);
      if (mAscents != mAscentBuf) {
        delete [] mAscents;
      }
      mAscents = newAscents;
    } else {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  mAscents[mAscentNum++] = aAscent;
  return NS_OK;
}

nsIFrame*
nsLineLayout::GetWordStartParent()
{
  if (nsnull == mWordStartParent) {
    nsIFrame* frame = mWordStart;
    for (;;) {
      nsIFrame* parent;
      frame->GetGeometricParent(parent);
      if (nsnull == parent) {
        break;
      }
      if (mBlock == parent) {
        break;
      }
      frame = parent;
    }
    mWordStartParent = frame;
  }
  return mWordStartParent;
}

nsresult
nsLineLayout::WordBreakReflow()
{
  // Restore line layout state to just before the word start.
  mReflowData = mWordStartReflowData;

  // Walk up from the frame that contains the start of the word to the
  // child of the block that contains the word.
  nsIFrame* frame = GetWordStartParent();

  // Compute the available space to reflow the child. Note that since
  // we are reflowing this child for the second time we know that the
  // child will fit before we begin.
  nsresult rv;
  nsSize kidAvailSize;
  kidAvailSize.width = mReflowData.mAvailWidth;
  kidAvailSize.height = mMaxHeight;
  if (!mUnconstrainedWidth) {
    nsIStyleContextPtr kidSC;
    rv = frame->GetStyleContext(mPresContext, kidSC.AssignRef());
    if (NS_OK != rv) {
      return rv;
    }
    nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
      kidSC->GetData(eStyleStruct_Spacing);
    nsMargin kidMargin;
    kidSpacing->CalcMarginFor(frame, kidMargin);
    kidAvailSize.width -= kidMargin.left + kidMargin.right;
  }

  // Reflow that child of the block having set the reflow type so that
  // the child knows whats going on.
  mReflowType = NS_LINE_LAYOUT_REFLOW_TYPE_WORD_WRAP;
  mReflowResult = NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE;
  nsSize maxElementSize;
  nsReflowStatus kidReflowStatus;
  nsSize* kidMaxElementSize = nsnull;
  if (nsnull != mMaxElementSizePointer) {
    kidMaxElementSize = &maxElementSize;
  }
  nsReflowMetrics kidSize(kidMaxElementSize);
  nsReflowState   kidReflowState(eReflowReason_Resize, kidAvailSize);
  rv = mBlock->ReflowInlineChild(frame, mPresContext, kidSize, kidReflowState,
                                 kidReflowStatus);

  return rv;
}

/**
 * Attempt to avoid reflowing a child by seeing if it's been touched
 * since the last time it was reflowed.
 */
nsresult
nsLineLayout::ReflowMappedChild()
{
  if (mMustReflowMappedChildren || PR_TRUE) {
/*
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowMappedChild: must reflow frame=%p[%d]",
                  mKidFrame, mKidIndex));
*/
    return ReflowChild(nsnull);
  }

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsLineLayout::ReflowMappedChild: attempt frame=%p[%d]",
                mKidFrame, mKidIndex));

  // If the child is a container then we need to reflow it if there is
  // a change in width. Note that if it's an empty container then it
  // doesn't really matter how much space we give it.
  if (mBlockReflowState.mDeltaWidth != 0) {
    nsIFrame* f;
    mKidFrame->FirstChild(f);
    if (nsnull != f) {
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsLineLayout::ReflowMappedChild: has children"));
      return ReflowChild(nsnull);
    }
  }

  // If we need the max-element size and we are splittable then we
  // have to reflow to get it.
  nsSplittableType splits;
  mKidFrame->IsSplittable(splits);
#if 0
  if (nsnull != mMaxElementSizePointer) {
    if (NS_FRAME_IS_SPLITTABLE(splits)) {
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsLineLayout::ReflowMappedChild: need max-element-size"));
      return ReflowChild(nsnull);
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
    return ReflowChild(nsnull);
  }
#endif

  nsFrameState state;
  mKidFrame->GetFrameState(state);

  // XXX a better term for this is "dirty" and once we add a dirty
  // bit that's what we'll do here.

  // XXX this check will cause pass2 of table reflow to reflow
  // everything; tables will be even faster if we have a dirty bit
  // instead (that way we can avoid reflowing non-splittables on
  // pass2)
  if (0 != (state & NS_FRAME_IN_REFLOW)) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowMappedChild: frame is dirty"));
    return ReflowChild(nsnull);
  }

  if (NS_FRAME_IS_SPLITTABLE(splits)) {
    // XXX a next-in-flow propogated dirty-bit eliminates this code

    // The splittable frame has not yet been reflowed. This means
    // that, in theory, its state is well defined. However, if it has
    // a prev-in-flow and that frame has been touched then we need to
    // reflow this frame.
    nsIFrame* prevInFlow;
    mKidFrame->GetPrevInFlow(prevInFlow);
    if (nsnull != prevInFlow) {
      nsFrameState prevState;
      prevInFlow->GetFrameState(prevState);
      if (0 != (prevState & NS_FRAME_IN_REFLOW)) {
        NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
           ("nsLineLayout::ReflowMappedChild: prev-in-flow frame is dirty"));
        return ReflowChild(nsnull);
      }
    }

    // If the child has a next-in-flow then never-mind, we need to
    // reflow it in case it has more/less space to reflow into.
    nsIFrame* nextInFlow;
    mKidFrame->GetNextInFlow(nextInFlow);
    if (nsnull != nextInFlow) {
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
         ("nsLineLayout::ReflowMappedChild: frame has next-in-flow"));
      return ReflowChild(nsnull);
    }
  }

  // Success! We have (so far) avoided reflowing the child. However,
  // we do need to place it and advance our position state. Get the
  // size of the child and its reflow metrics for placing.
  nsIStyleContextPtr kidSC;
  nsresult rv = mKidFrame->GetStyleContext(mPresContext, kidSC.AssignRef());
  if (NS_OK != rv) {
    return rv;
  }
  nsStyleDisplay* kidDisplay = (nsStyleDisplay*)
    kidSC->GetData(eStyleStruct_Display);
  if (NS_STYLE_FLOAT_NONE != kidDisplay->mFloats) {
    // XXX If it floats it needs to go through the normal path so that
    // PlaceFloater is invoked.
    return ReflowChild(nsnull);
  }
  nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
    kidSC->GetData(eStyleStruct_Spacing);
  PRBool isBlock = PR_FALSE;
  switch (kidDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
    if (mKidFrame != mLine->mFirstChild) {
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
  mKidFrame->GetRect(kidRect);
  nsMargin kidMargin;
  kidSpacing->CalcMarginFor(mKidFrame, kidMargin);
  nscoord totalWidth;
  totalWidth = kidMargin.left + kidMargin.right + kidRect.width;

  // If the child intersects the area affected by the reflow then
  // we need to reflow it.
  if (mReflowData.mX + kidMargin.left + kidRect.width > mNewRightEdge) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowMappedChild: failed edge test"));
    // XXX if !splittable then return NS_LINE_LAYOUT_BREAK_BEFORE
    return ReflowChild(nsnull);
  }

  // Make sure the child will fit. The child always fits if it's the
  // first child on the line.
  if (mUnconstrainedWidth ||
      (mKidFrame == mLine->mFirstChild) ||
      (totalWidth <= mReflowData.mAvailWidth)) {
    // By convention, mReflowResult is set during ResizeReflow,
    // IncrementalReflow AND GetReflowMetrics by those frames that are
    // line layout aware.
    mReflowResult = NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE;
    nsReflowMetrics kidMetrics(nsnull);
    mKidFrame->GetReflowMetrics(mPresContext, kidMetrics);

    nsSize maxElementSize;
    nsSize* kidMaxElementSize = nsnull;
    if (nsnull != mMaxElementSizePointer) {
      kidMaxElementSize = &maxElementSize;
      maxElementSize.width = kidRect.width;
      maxElementSize.height = kidRect.height;
    }
    kidRect.x = mReflowData.mX + kidMargin.left;
    kidRect.y = mY;

    if (NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE == mReflowResult) {
      mSkipLeadingWhiteSpace = PR_FALSE;
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
    return ReflowChild(nsnull);
  }
  return NS_LINE_LAYOUT_BREAK_BEFORE;
}

// Return values: <0 for error
// 0 == NS_LINE_LAYOUT
nsresult
nsLineLayout::ReflowChild(nsReflowCommand* aReflowCommand)
{
  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsLineLayout::ReflowChild: attempt frame=%p[%d] mX=%d mAvailWidth=%d",
      mKidFrame, mKidIndex,
      mReflowData.mX, mReflowData.mAvailWidth));

  // Get kid frame's style context
  nsIStyleContextPtr kidSC;
  nsresult rv = mKidFrame->GetStyleContext(mPresContext, kidSC.AssignRef());
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
  PRBool isFirstChild = PRBool(mKidFrame == mLine->mFirstChild);
  switch (kidDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_NONE:
    // Make sure the frame remains zero sized.
    mKidFrame->WillReflow(*mPresContext);
    mKidFrame->SetRect(nsRect(mReflowData.mX, mY, 0, 0));
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsLineLayout::ReflowChild: display=none"));
    return NS_LINE_LAYOUT_COMPLETE;

  case NS_STYLE_DISPLAY_INLINE:
    break;

  default:
    isBlock = PR_TRUE;
    if (!isFirstChild) {
      // XXX Make sure child is dirty for next time
      mKidFrame->WillReflow(*mPresContext);
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsLineLayout::ReflowChild: block requires break-before"));
      return NS_LINE_LAYOUT_BREAK_BEFORE;
    }
    break;
  }

  // Get the available size to reflow the child into
  nsSize kidAvailSize;
  kidAvailSize.width = mReflowData.mAvailWidth;
  kidAvailSize.height = mMaxHeight;
  nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
    kidSC->GetData(eStyleStruct_Spacing);
  nsMargin kidMargin;
  kidSpacing->CalcMarginFor(mKidFrame, kidMargin);
  if (!mUnconstrainedWidth) {
    kidAvailSize.width -= kidMargin.left + kidMargin.right;
    if (!isFirstChild && (kidAvailSize.width <= 0)) {
      // No room.

      // XXX Make sure child is dirty for next time
      mKidFrame->WillReflow(*mPresContext);
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
  nsReflowState   kidReflowState(aReflowCommand ? eReflowReason_Incremental :
                                 eReflowReason_Resize, kidAvailSize);
  kidReflowState.reflowCommand = aReflowCommand;
  mReflowResult = NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE;
  nscoord dx = mReflowData.mX + kidMargin.left;
  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsLineLayout::ReflowChild: reflowing frame=%p[%d] into %d,%d",
                mKidFrame, mKidIndex,
                kidAvailSize.width, kidAvailSize.height));
  if (isBlock) {
    // Calculate top margin by collapsing with previous bottom margin
    nscoord negTopMargin;
    nscoord posTopMargin;
    nsMargin kidMargin;
    kidSpacing->CalcMarginFor(mKidFrame, kidMargin);
    if (kidMargin.top < 0) {
      negTopMargin = -kidMargin.top;
      posTopMargin = 0;
    } else {
      negTopMargin = 0;
      posTopMargin = kidMargin.top;
    }
    nscoord maxPos =
      PR_MAX(mBlockReflowState.mPrevPosBottomMargin, posTopMargin);
    nscoord maxNeg =
      PR_MAX(mBlockReflowState.mPrevNegBottomMargin, negTopMargin);
    nscoord topMargin = maxPos - maxNeg;

    // Save away bottom margin information for later
    if (kidMargin.bottom < 0) {
      mBlockReflowState.mPrevNegBottomMargin = -kidMargin.bottom;
      mBlockReflowState.mPrevPosBottomMargin = 0;
    } else {
      mBlockReflowState.mPrevNegBottomMargin = 0;
      mBlockReflowState.mPrevPosBottomMargin = kidMargin.bottom;
    }

    mY += topMargin;
    mBlockReflowState.mY += topMargin;
    // XXX tell block what topMargin ended up being so that it can
    // undo it if it ends up pushing the line.

    mSpaceManager->Translate(dx, mY);
    mKidFrame->WillReflow(*mPresContext);
    rv = mBlock->ReflowBlockChild(mKidFrame, mPresContext,
                                  mSpaceManager, kidMetrics, kidReflowState,
                                  kidRect, kidReflowStatus);
    mSpaceManager->Translate(-dx, -mY);
    kidRect.x = dx;
    kidRect.y = mY;
    kidMetrics.width = kidRect.width;
    kidMetrics.height = kidRect.height;
    kidMetrics.ascent = kidRect.height;
    kidMetrics.descent = 0;
  }
  else {
    // Reflow the inline child
    mKidFrame->WillReflow(*mPresContext);
    rv = mBlock->ReflowInlineChild(mKidFrame, mPresContext, kidMetrics,
                                   kidReflowState, kidReflowStatus);
    // After we reflow the inline child we will know whether or not it
    // has any height/width. If it doesn't have any height/width then
    // we do not yet apply any previous block bottom margin.
    if ((0 != kidMetrics.height) && !mMarginApplied) {
      // Before we place the first inline child on this line apply
      // the previous block's bottom margin.
      nscoord bottomMargin = mBlockReflowState.mPrevPosBottomMargin -
        mBlockReflowState.mPrevNegBottomMargin;
      mY += bottomMargin;
      mBlockReflowState.mY += bottomMargin;
      // XXX tell block what bottomMargin ended up being so that it can
      // undo it if it ends up pushing the line.
      mMarginApplied = PR_TRUE;
      mBlockReflowState.mPrevPosBottomMargin = 0;
      mBlockReflowState.mPrevNegBottomMargin = 0;
    }
    kidRect.x = dx;
    kidRect.y = mY;
    kidRect.width = kidMetrics.width;
    kidRect.height = kidMetrics.height;
  }
  if (NS_OK != rv) return rv;

  // See if the child fit
  if (kidMetrics.width > kidAvailSize.width) {
    // The child took up too much space. This condition is ignored if
    // the child is the first child (by definition the first child
    // always fits) or we have a word start and the word start is the
    // first child.
    if (!isFirstChild) {
      // It's not our first child.
      if (nsnull != mWordStart) {
        // We have a word to break at
        if (GetWordStartParent() != mLine->mFirstChild) {
          // The word is not our first child
          WordBreakReflow();
          // XXX mKidPrevInFlow
          return NS_LINE_LAYOUT_BREAK_BEFORE;
        }
      }
      else {
        // There is no word to break at and it's not our first child.
        // We are out of room.
        // XXX mKidPrevInFlow
        NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                     ("nsLineLayout::ReflowChild: !fit size=%d,%d",
                      kidRect.width, kidRect.height));
        return NS_LINE_LAYOUT_BREAK_BEFORE;
      }
    }
  }

  // For non-aware children they act like words which means that space
  // immediately following them must not be skipped over.
  if (NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE == mReflowResult) {
    mSkipLeadingWhiteSpace = PR_FALSE;
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
nsLineLayout::PlaceChild(const nsRect& kidRect,
                         const nsReflowMetrics& kidMetrics,
                         const nsSize* kidMaxElementSize,
                         const nsMargin& kidMargin,
                         nsReflowStatus kidReflowStatus)
{
  // Place child
  mKidFrame->SetRect(kidRect);

  // Advance
  // XXX RTL
  nscoord horizontalMargins = kidMargin.left +
    kidMargin.right;
  nscoord totalWidth = kidMetrics.width + horizontalMargins;
  mReflowData.mX += totalWidth;
  if (!mUnconstrainedWidth) {
    mReflowData.mAvailWidth -= totalWidth;
  }
  if (nsnull != mMaxElementSizePointer) {
    // XXX I'm not certain that this is doing the right thing; rethink this
    nscoord elementWidth = kidMaxElementSize->width + horizontalMargins;
    if (elementWidth > mReflowData.mMaxElementSize.width) {
      mReflowData.mMaxElementSize.width = elementWidth;
    }
    if (kidMetrics.height > mReflowData.mMaxElementSize.height) {
      mReflowData.mMaxElementSize.height = kidMetrics.height;
    }
  }
  if (kidMetrics.ascent > mReflowData.mMaxAscent) {
    mReflowData.mMaxAscent = kidMetrics.ascent;
  }
  if (kidMetrics.descent > mReflowData.mMaxDescent) {
    mReflowData.mMaxDescent = kidMetrics.descent;
  }
  AddAscent(mLine->mIsBlock ? 0 : kidMetrics.ascent);

  // Set completion status
  nsresult rv = NS_LINE_LAYOUT_COMPLETE;
  mLine->mLastContentOffset = mKidIndex;
  if (NS_FRAME_IS_COMPLETE(kidReflowStatus)) {
    mLine->mLastContentIsComplete = PR_TRUE;
    if (mLine->mIsBlock ||
        (NS_LINE_LAYOUT_REFLOW_RESULT_BREAK_AFTER == mReflowResult)) {
      rv = NS_LINE_LAYOUT_BREAK_AFTER;
    }
    mKidPrevInFlow = nsnull;
  }
  else {
    mLine->mLastContentIsComplete = PR_FALSE;
    rv = NS_LINE_LAYOUT_NOT_COMPLETE;
    mKidPrevInFlow = mKidFrame;
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

  mLine->mBounds.x = mReflowData.mX;
  mLine->mBounds.y = mY;
  mKidFrame = mLine->mFirstChild;
  PRInt32 kidNum = 0;
  while (kidNum < mLine->mChildCount) {
    nsresult childReflowStatus;
    if (mKidFrame == aChildFrame) {
      childReflowStatus = ReflowChild(aReflowCommand);
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
      mPrevKidFrame = mKidFrame;
      mKidFrame->GetNextSibling(mKidFrame);
      mKidIndex++;
      kidNum++;
      break;

    case NS_LINE_LAYOUT_NOT_COMPLETE:
      reflowStatus = childReflowStatus;
      mPrevKidFrame = mKidFrame;
      mKidFrame->GetNextSibling(mKidFrame);
      kidNum++;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_BEFORE:
      reflowStatus = childReflowStatus;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_AFTER:
      reflowStatus = childReflowStatus;
      mPrevKidFrame = mKidFrame;
      mKidFrame->GetNextSibling(mKidFrame);
      mKidIndex++;
      kidNum++;

    split_line:
      reflowStatus = SplitLine(childReflowStatus, mLine->mChildCount - kidNum);
      goto done;
    }
  }

done:
  // Perform alignment operations
  AlignChildren();

  // Set final bounds of the line
  mLine->mBounds.height = mLineHeight;
  mLine->mBounds.width = mReflowData.mX - mLine->mBounds.x;

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
nsLineLayout::SplitLine(PRInt32 aChildReflowStatus, PRInt32 aRemainingKids)
{
  nsresult rv = NS_LINE_LAYOUT_COMPLETE;

  if (NS_LINE_LAYOUT_NOT_COMPLETE == aChildReflowStatus) {
    // When a line is not complete it indicates that the last child on
    // the line reflowed and took some space but wasn't given enough
    // space to complete. Sometimes when this happens we will need to
    // create a next-in-flow for the child.
    nsIFrame* nextInFlow;
    mPrevKidFrame->GetNextInFlow(nextInFlow);
    if (nsnull == nextInFlow) {
      // Create a continuation frame for the child frame and insert it
      // into our lines child list.
      nsIFrame* nextFrame;
      mPrevKidFrame->GetNextSibling(nextFrame);
      nsIStyleContext* kidSC;
      mPrevKidFrame->GetStyleContext(mPresContext, kidSC);
      mPrevKidFrame->CreateContinuingFrame(mPresContext, mBlock, kidSC,
                                           nextInFlow);
      NS_RELEASE(kidSC);
      if (nsnull == nextInFlow) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mPrevKidFrame->SetNextSibling(nextInFlow);
      nextInFlow->SetNextSibling(nextFrame);
      mNewFrames++;

      // Add new child to our line
      mLine->mChildCount++;

      // Set mKidFrame to the new next-in-flow so that we will
      // push it when we push children. Increment the number of
      // remaining kids now that there is one more.
      mKidFrame = nextInFlow;
      aRemainingKids++;
    }
  }

  if (0 != aRemainingKids) {
    NS_ASSERTION(nsnull != mKidFrame, "whoops");
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
    mKidFrame->GetContentIndex(kidIndexInParent);
    to->mFirstChild = mKidFrame;
    to->mChildCount += aRemainingKids;
    to->mFirstContentOffset = kidIndexInParent;

    // The to-line is going to be reflowed therefore it's last content
    // offset and completion status don't matter. In fact, it's expensive
    // to compute them so don't bother.
#ifdef NS_DEBUG
    to->mLastContentOffset = -1;
    to->mLastContentIsComplete = PRPackedBool(0x255);
#endif

    from->mChildCount -= aRemainingKids;
    NS_ASSERTION(0 != from->mChildCount, "bad push");
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      from->Verify();
    }
#endif
#ifdef NOISY_REFLOW
    printf("After push, from-line (%d):\n", aRemainingKids);
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

  mKidFrame = mLine->mFirstChild;
  PRInt32 kidNum = 0;
  while (kidNum < mLine->mChildCount) {
    nsresult childReflowStatus = ReflowMappedChild();
    if (childReflowStatus < 0) {
      reflowStatus = childReflowStatus;
      goto done;
    }
    switch (childReflowStatus) {
    default:
    case NS_LINE_LAYOUT_COMPLETE:
      mPrevKidFrame = mKidFrame;
      mKidFrame->GetNextSibling(mKidFrame);
      mKidIndex++;
      kidNum++;
      break;

    case NS_LINE_LAYOUT_NOT_COMPLETE:
      reflowStatus = childReflowStatus;
      mPrevKidFrame = mKidFrame;
      mKidFrame->GetNextSibling(mKidFrame);
      kidNum++;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_BEFORE:
      reflowStatus = childReflowStatus;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_AFTER:
      reflowStatus = childReflowStatus;
      mPrevKidFrame = mKidFrame;
      mKidFrame->GetNextSibling(mKidFrame);
      mKidIndex++;
      kidNum++;

    split_line:
      reflowStatus = SplitLine(childReflowStatus, mLine->mChildCount - kidNum);
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
  nsIFrame* prevKidFrame = mPrevKidFrame;

  nsBlockFrame* currentBlock = mBlock;
  nsLineData* line = mLine->mNextLine;
  while (nsnull != currentBlock) {

    // Pull children from the next line
    while (nsnull != line) {
      // Get first child from next line
      mKidFrame = line->mFirstChild;
      if (nsnull == mKidFrame) {
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

      // If the child is a block element then if this is not the first
      // line in the block or if it's the first line and it's not the
      // first child in the line then we cannot pull-up the child.
      nsresult rv;
      nsIStyleContextPtr kidSC;
      rv = mKidFrame->GetStyleContext(mPresContext, kidSC.AssignRef());
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
                    mKidFrame));
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
        mKidFrame->GetNextSibling(line->mFirstChild);
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
      PRInt32 pushCount;
      switch (childReflowStatus) {
      default:
      case NS_LINE_LAYOUT_COMPLETE:
        mPrevKidFrame = mKidFrame;
        mKidFrame = nsnull;
        mKidIndex++;
        break;

      case NS_LINE_LAYOUT_NOT_COMPLETE:
        reflowStatus = childReflowStatus;
        mPrevKidFrame = mKidFrame;
        mKidFrame = nsnull;
        pushCount = 0;
        goto split_line;

      case NS_LINE_LAYOUT_BREAK_BEFORE:
        reflowStatus = childReflowStatus;
        pushCount = 1;
        goto split_line;

      case NS_LINE_LAYOUT_BREAK_AFTER:
        reflowStatus = childReflowStatus;
        mPrevKidFrame = mKidFrame;
        mKidFrame = nsnull;
        mKidIndex++;
        pushCount = 0;

      split_line:
        reflowStatus = SplitLine(childReflowStatus, pushCount);
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
    rv = AbsoluteFrame::NewFrame(&kidFrame, aKid, mBlock);
    if (NS_OK == rv) {
      kidFrame->SetStyleContext(mPresContext, kidSC);
    }
  } else if (NS_STYLE_FLOAT_NONE != kidDisplay->mFloats) {
    rv = PlaceholderFrame::NewFrame(&kidFrame, aKid, mBlock);
    if (NS_OK == rv) {
      kidFrame->SetStyleContext(mPresContext, kidSC);
    }
  } else if (nsnull == mKidPrevInFlow) {
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

  mKidFrame = kidFrame;
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
    nsIContentPtr kid = mBlockContent->ChildAt(mKidIndex);
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
    if (nsnull != mPrevKidFrame) {
      mPrevKidFrame->SetNextSibling(mKidFrame);
    }
    if (0 == mLine->mChildCount) {
      mLine->mFirstChild = mKidFrame;
    }
    mLine->mChildCount++;

    nsresult childReflowStatus;
    PRInt32 pushCount;
    if (rv == NS_LINE_LAYOUT_BREAK_BEFORE) {
      // If we break before a frame is even supposed to layout then we
      // need to split the line.
      childReflowStatus = rv;
      pushCount = 1;

      // XXX Mark new frame dirty so it gets reflow later on
      mKidFrame->WillReflow(*mPresContext);
      goto split_line;
    }

    // Reflow new child frame
    childReflowStatus = ReflowChild(nsnull);
    if (childReflowStatus < 0) {
      reflowStatus = childReflowStatus;
      goto done;
    }
    switch (childReflowStatus) {
    default:
    case NS_LINE_LAYOUT_COMPLETE:
      mPrevKidFrame = mKidFrame;
      mKidFrame = nsnull;
      mKidIndex++;
      break;

    case NS_LINE_LAYOUT_NOT_COMPLETE:
      reflowStatus = childReflowStatus;
      mPrevKidFrame = mKidFrame;
      mKidFrame = nsnull;
      pushCount = 0;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_BEFORE:
      reflowStatus = childReflowStatus;
      pushCount = 1;
      goto split_line;

    case NS_LINE_LAYOUT_BREAK_AFTER:
      reflowStatus = childReflowStatus;
      mPrevKidFrame = mKidFrame;
      mKidFrame = nsnull;
      mKidIndex++;
      pushCount = 0;

    split_line:
      reflowStatus = SplitLine(childReflowStatus, pushCount);
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

  mLine->mBounds.x = mReflowData.mX;
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
  mLine->mBounds.width = mReflowData.mX - mLine->mBounds.x;

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
  NS_PRECONDITION(mLine->mChildCount == mAscentNum, "bad line reflow");

  // Block lines don't require (or allow!) alignment
  if (mLine->mIsBlock) {
    mLineHeight = mReflowData.mMaxAscent + mReflowData.mMaxDescent;
    return;
  }

  // Avoid alignment when we didn't actually reflow any frames and we
  // also didn't change the number of frames we had (which means the
  // pullup code didn't pull anything up). When this happens it means
  // that nothing changed which means that we can avoid the alignment
  // work.
  if ((0 == mFramesReflowed) && (mOldChildCount == mLine->mChildCount)) {
    mLineHeight = mLine->mBounds.height;
    return;
  }

  nsIStyleContextPtr blockSC;
  mBlock->GetStyleContext(mPresContext, blockSC.AssignRef());
  nsStyleFont* blockFont = (nsStyleFont*)
    blockSC->GetData(eStyleStruct_Font);
  nsStyleText* blockText = (nsStyleText*)
    blockSC->GetData(eStyleStruct_Text);

  // First vertically align the children on the line; this will
  // compute the actual line height for us.
  mLineHeight =
    nsCSSLayout::VerticallyAlignChildren(mPresContext, mBlock, blockFont,
                                         mY,
                                         mLine->mFirstChild,
                                         mLine->mChildCount,
                                         mAscents, mReflowData.mMaxAscent); 

  // Now horizontally place the children
  nsCSSLayout::HorizontallyPlaceChildren(mPresContext, mBlock, blockText,
                                         mLine->mFirstChild,
                                         mLine->mChildCount,
                                         mReflowData.mX - mX0,
                                         mMaxWidth);

  // Last, apply relative positioning
  nsCSSLayout::RelativePositionChildren(mPresContext, mBlock,
                                        mLine->mFirstChild,
                                        mLine->mChildCount);
}
