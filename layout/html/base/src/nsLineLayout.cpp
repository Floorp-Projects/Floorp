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

#undef NOISY_REFLOW

static NS_DEFINE_IID(kStyleDisplaySID, NS_STYLEDISPLAY_SID);
static NS_DEFINE_IID(kStyleFontSID, NS_STYLEFONT_SID);
static NS_DEFINE_IID(kStylePositionSID, NS_STYLEPOSITION_SID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);
static NS_DEFINE_IID(kStyleTextSID, NS_STYLETEXT_SID);

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
}

nsLineData::~nsLineData()
{
}

void
nsLineData::UnlinkLine()
{
  nsLineData* prevLine = mPrevLine;
  nsLineData* nextLine = mNextLine;
  if (nsnull != nextLine) nextLine->mPrevLine = prevLine;
  if (nsnull != prevLine) prevLine->mNextLine = nextLine;
}

void
nsLineData::MoveLineBy(nscoord dx, nscoord dy)
{
  nsIFrame* kid = mFirstChild;
  nsPoint pt;
  for (PRInt32 i = mChildCount; --i >= 0; ) {
    kid->GetOrigin(pt);
    pt.x += dx;
    pt.y += dy;
    kid->MoveTo(pt.x, pt.y);
    kid->GetNextSibling(kid);
  }
  mBounds.MoveBy(dx, dy);
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
      child->GetIndexInParent(indexInParent);
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
{
  mBlock = aState.mBlock;
  mSpaceManager = aState.mSpaceManager;
  mBlock->GetContent(mBlockContent);
  mPresContext = aState.mPresContext;
#if 0
  // XXX Do we still need this?
  mBlockIsPseudo = aState.mBlockIsPseudo;
#endif
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
  mKidIndex = aLine->mFirstContentOffset;

  mReflowData.mMaxElementSize.width = 0;
  mReflowData.mMaxElementSize.height = 0;
  mReflowData.mMaxAscent = nsnull;
  mReflowData.mMaxDescent = nsnull;

  SetReflowSpace(aState.mCurrentBand.availSpace);
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
  mReflowDataChanged = PR_TRUE;
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
      kidSC->GetData(kStyleSpacingSID);
    kidAvailSize.width -= kidSpacing->mMargin.left + kidSpacing->mMargin.right;
  }

  // Reflow that child of the block having set the reflow type so that
  // the child knows whats going on.
  mReflowType = NS_LINE_LAYOUT_REFLOW_TYPE_WORD_WRAP;
  mReflowResult = NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE;
  nsSize maxElementSize;
  nsReflowMetrics kidSize;
  nsIFrame::ReflowStatus kidReflowStatus;
  nsSize* kidMaxElementSize = nsnull;
  if (nsnull != mMaxElementSizePointer) {
    kidMaxElementSize = &maxElementSize;
  }
  rv = mBlock->ReflowInlineChild(frame, mPresContext, kidSize,
                                 kidAvailSize, kidMaxElementSize,
                                 kidReflowStatus);

  return rv;
}

// Return values: <0 for error
// 0 == NS_LINE_LAYOUT
nsresult
nsLineLayout::ReflowChild()
{
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
    kidSC->GetData(kStyleDisplaySID);
  PRBool isBlock = PR_FALSE;
  PRBool isFirstChild = PRBool(mKidFrame == mLine->mFirstChild);
  switch (kidDisplay->mDisplay) {
  case NS_STYLE_DISPLAY_NONE:
    // Make sure the frame remains zero sized.
    mKidFrame->SetRect(nsRect(mReflowData.mX, mY, 0, 0));
    return NS_LINE_LAYOUT_COMPLETE;
  case NS_STYLE_DISPLAY_INLINE:
    break;
  default:
    isBlock = PR_TRUE;
    if (!isFirstChild) {
      return NS_LINE_LAYOUT_BREAK_BEFORE;
    }
    break;
  }

  // Get the available size to reflow the child into
  nsSize kidAvailSize;
  kidAvailSize.width = mReflowData.mAvailWidth;
  kidAvailSize.height = mMaxHeight;
  nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
    kidSC->GetData(kStyleSpacingSID);
  if (!mUnconstrainedWidth) {
    kidAvailSize.width -= kidSpacing->mMargin.left + kidSpacing->mMargin.right;
    if (!isFirstChild && (kidAvailSize.width <= 0)) {
      // No room.
      return NS_LINE_LAYOUT_BREAK_BEFORE;
    }
  }

  // Reflow the child
  nsRect kidRect;
  nsSize maxElementSize;
  nsReflowMetrics kidSize;
  nsSize* kidMaxElementSize = nsnull;
  nsIFrame::ReflowStatus kidReflowStatus;
  if (nsnull != mMaxElementSizePointer) {
    kidMaxElementSize = &maxElementSize;
  }
  mReflowResult = NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE;
  nscoord dx = mReflowData.mX + kidSpacing->mMargin.left;
  if (isBlock) {
    mSpaceManager->Translate(dx, 0);
    rv = mBlock->ReflowBlockChild(mKidFrame, mPresContext,
                                  mSpaceManager, kidAvailSize, kidRect,
                                  kidMaxElementSize, kidReflowStatus);
    mSpaceManager->Translate(-dx, 0);
    kidRect.x = dx;
    kidRect.y = mY;
    kidSize.width = kidRect.width;
    kidSize.height = kidRect.height;
    kidSize.ascent = kidRect.height;
    kidSize.descent = 0;
  }
  else {
    rv = mBlock->ReflowInlineChild(mKidFrame, mPresContext,
                                   kidSize, kidAvailSize, kidMaxElementSize,
                                   kidReflowStatus);
    kidRect.x = dx;
    kidRect.y = mY;
    kidRect.width = kidSize.width;
    kidRect.height = kidSize.height;
  }
  if (NS_OK != rv) return rv;

  // See if the child fit
  if (kidSize.width > kidAvailSize.width) {
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
        return NS_LINE_LAYOUT_BREAK_BEFORE;
      }
    }
  }

  // For non-aware children they act like words which means that space
  // immediately following them must not be skipped over.
  if (NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE == mReflowResult) {
    mSkipLeadingWhiteSpace = PR_FALSE;
  }

  // Place child
  mKidFrame->SetRect(kidRect);

  // Advance
  // XXX RTL
  nscoord horizontalMargins = kidSpacing->mMargin.left +
    kidSpacing->mMargin.right;
  nscoord totalWidth = kidSize.width + horizontalMargins;
  mReflowData.mX += totalWidth;
  if (!mUnconstrainedWidth) {
    mReflowData.mAvailWidth -= totalWidth;
  }
  if (nsnull != mMaxElementSizePointer) {
    nscoord elementWidth = maxElementSize.width + horizontalMargins;
    if (elementWidth > mReflowData.mMaxElementSize.width) {
      mReflowData.mMaxElementSize.width = elementWidth;
    }
    if (kidSize.height > mReflowData.mMaxElementSize.height) {
      mReflowData.mMaxElementSize.height = kidSize.height;
    }
  }
  if (kidSize.ascent > mReflowData.mMaxAscent) {
    mReflowData.mMaxAscent = kidSize.ascent;
  }
  if (kidSize.descent > mReflowData.mMaxDescent) {
    mReflowData.mMaxDescent = kidSize.descent;
  }
  AddAscent(isBlock ? 0 : kidSize.ascent);
  mLine->mIsBlock = isBlock;

  // Set completion status
  mLine->mLastContentOffset = mKidIndex;
  if (nsIFrame::frComplete == kidReflowStatus) {
    mLine->mLastContentIsComplete = PR_TRUE;
    if (isBlock ||
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

  return rv;
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
      mPrevKidFrame->CreateContinuingFrame(mPresContext, mBlock, nextInFlow);
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
    mKidFrame->GetIndexInParent(kidIndexInParent);
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
    from->Verify();
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
    // XXX Code to avoid reflowing a child goes here

    nsresult childReflowStatus = ReflowChild();
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
        kidSC->GetData(kStyleDisplaySID);
      if (IsBlock(kidDisplay)) {
        if ((nsnull != mLine->mPrevLine) || (0 != mLine->mChildCount)) {
          goto done;
        }
      }

      // Make pulled child part of this line
#ifdef NOISY_REFLOW
      printf("Before Pullup:\n");
      line->List(stdout, 1);
#endif
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
        line->mFirstChild->GetIndexInParent(indexInParent);
        line->mFirstContentOffset = indexInParent;
#ifdef NS_DEBUG
        line->Verify();
#endif
      }

      // Try to reflow it like any other mapped child
      nsresult childReflowStatus = ReflowChild();
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
  nsresult rv = NS_OK;

  // XXX what if kidSC ends up null?
  nsIStyleContextPtr kidSC =
    mPresContext->ResolveStyleContextFor(aKid, mBlock);
  nsStylePosition* kidPosition = (nsStylePosition*)
    kidSC->GetData(kStylePositionSID);
  nsStyleDisplay* kidDisplay = (nsStyleDisplay*)
    kidSC->GetData(kStyleDisplaySID);

  // Check whether it wants to floated or absolutely positioned
  // XXX don't lose error status from frame ctor's!
  PRBool isBlock = PR_FALSE;
  nsIFrame* kidFrame;
  if (NS_STYLE_POSITION_ABSOLUTE == kidPosition->mPosition) {
    AbsoluteFrame::NewFrame(&kidFrame, aKid, mKidIndex, mBlock);
    kidFrame->SetStyleContext(mPresContext, kidSC);
  } else if (kidDisplay->mFloats != NS_STYLE_FLOAT_NONE) {
    PlaceholderFrame::NewFrame(&kidFrame, aKid, mKidIndex, mBlock);
    kidFrame->SetStyleContext(mPresContext, kidSC);
  } else if (nsnull == mKidPrevInFlow) {
    // Create initial frame for the child

    // XXX refactor to just let delegate create frame (unless style is
    // none) and then after that if it's a block frame and the child
    // count is non-zero we return the break-before value.

    nsIContentDelegate* kidDel;
    switch (kidDisplay->mDisplay) {
    case NS_STYLE_DISPLAY_BLOCK:
    case NS_STYLE_DISPLAY_LIST_ITEM:
#if 0
      // XXX Do we still need this? Now that the body code is changed it
      // causes a problem...
      if (mBlockIsPseudo) {
        // Don't create the frame! It doesn't belong in us.

        // XXX the unfortunate thing here is that we waste the style
        // lookup!

        return NS_LINE_LAYOUT_PSEUDO_BREAK_BEFORE_BLOCK;
      }
#endif
      kidDel = aKid->GetDelegate(mPresContext);
      kidFrame = kidDel->CreateFrame(mPresContext, aKid, mKidIndex, mBlock);
      NS_RELEASE(kidDel);
      isBlock = PR_TRUE;
      break;

    case NS_STYLE_DISPLAY_INLINE:
      // XXX pass in kidSC to speed things up *alot*!
      // XXX fix CreateFrame API to return an nsresult!
      kidDel = aKid->GetDelegate(mPresContext);
      kidFrame = kidDel->CreateFrame(mPresContext, aKid, mKidIndex, mBlock);
      NS_RELEASE(kidDel);
      break;

    default:/* XXX bzzt! */
      nsFrame::NewFrame(&kidFrame, aKid, mKidIndex, mBlock);
      break;
    }
    kidFrame->SetStyleContext(mPresContext, kidSC);
  } else {
    // Since kid has a prev-in-flow, use that to create the next
    // frame.
    mKidPrevInFlow->CreateContinuingFrame(mPresContext, mBlock, kidFrame);
    NS_ASSERTION(0 == mLine->mChildCount, "bad continuation");
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
    if (NS_LINE_LAYOUT_PSEUDO_BREAK_BEFORE_BLOCK == rv) {
      // We have found a child that should not be a part of the
      // block. Therefore we are finished!
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
      goto split_line;
    }

    // Reflow new child frame
    childReflowStatus = ReflowChild();
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
#ifdef NOISY_REFLOW
  printf("Before ReflowLine:\n");
  mLine->List(stdout, 1);
#endif

  nsresult rv = NS_LINE_LAYOUT_COMPLETE;

  mLine->mBounds.x = mReflowData.mX;
  mLine->mBounds.y = mY;

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
  if (mLine->mIsBlock) {
    mLineHeight = mReflowData.mMaxAscent + mReflowData.mMaxDescent;
  }
  else {
    AlignChildren();
  }

  // Set final bounds of the line
  mLine->mBounds.height = mLineHeight;
  mLine->mBounds.width = mReflowData.mX - mLine->mBounds.x;

#ifdef NOISY_REFLOW
  printf("After ReflowLine:\n");
  mLine->List(stdout, 1);
#endif
#ifdef NS_DEBUG
  mLine->Verify();
#endif
  return rv;
}

nsresult
nsLineLayout::AlignChildren()
{
  NS_PRECONDITION(mLine->mChildCount == mAscentNum, "bad line reflow");

  nsresult rv = NS_OK;

  nsIStyleContextPtr blockSC;
  mBlock->GetStyleContext(mPresContext, blockSC.AssignRef());
  nsStyleFont* blockFont = (nsStyleFont*)
    blockSC->GetData(kStyleFontSID);
  nsStyleText* blockText = (nsStyleText*)
    blockSC->GetData(kStyleTextSID);

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

  return rv;
}
