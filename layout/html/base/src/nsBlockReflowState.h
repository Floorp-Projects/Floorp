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
#include "nsBlockFrame.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIHTMLContent.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIAnchoredItems.h"
#include "nsPlaceholderFrame.h"
#include "nsIPtr.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLValue.h"
#include "nsReflowCommand.h"
#include "nsCSSLayout.h"

// XXX what do we do with catastrophic errors (rv < 0)? What is the
// state of the reflow world after such an error?

static NS_DEFINE_IID(kStyleDisplaySID, NS_STYLEDISPLAY_SID);
static NS_DEFINE_IID(kStylePositionSID, NS_STYLEPOSITION_SID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);

static NS_DEFINE_IID(kIAnchoredItemsIID, NS_IANCHOREDITEMS_IID);
static NS_DEFINE_IID(kIRunaroundIID, NS_IRUNAROUND_IID);
static NS_DEFINE_IID(kIFloaterContainerIID, NS_IFLOATERCONTAINER_IID);

NS_DEF_PTR(nsIContent);
NS_DEF_PTR(nsIStyleContext);

//----------------------------------------------------------------------


void nsBlockBandData::ComputeAvailSpaceRect()
{
  nsBandTrapezoid*  trapezoid = data;

  if (count > 1) {
    // If there's more than one trapezoid that means there are floaters
    PRInt32 i;

    // Stop when we get to space occupied by a right floater
    for (i = 0; i < count; i++) {
      nsBandTrapezoid*  trapezoid = &data[i];

      if (trapezoid->state != nsBandTrapezoid::smAvailable) {
        nsStyleDisplay* display;
      
        // XXX Handle the case of multiple frames
        trapezoid->frame->GetStyleData(kStyleDisplaySID, (nsStyleStruct*&)display);
        if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
          break;
        }
      }
    }

    if (i > 0) {
      trapezoid = &data[i - 1];
    }
  }

  if (nsBandTrapezoid::smAvailable == trapezoid->state) {
    // The trapezoid is available
    trapezoid->GetRect(availSpace);
  } else {
    nsStyleDisplay* display;

    // The trapezoid is occupied. That means there's no available space
    trapezoid->GetRect(availSpace);

    // XXX Handle the case of multiple frames
    trapezoid->frame->GetStyleData(kStyleDisplaySID, (nsStyleStruct*&)display);
    if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
      availSpace.x = availSpace.XMost();
    }
    availSpace.width = 0;
  }
}

//----------------------------------------------------------------------

nsBlockReflowState::nsBlockReflowState()
{
}

nsBlockReflowState::~nsBlockReflowState()
{
}

nsresult
nsBlockReflowState::Initialize(nsIPresContext* aPresContext,
                               nsISpaceManager* aSpaceManager,
                               const nsSize& aMaxSize,
                               nsSize* aMaxElementSize,
                               nsBlockFrame* aBlock)
{
  nsresult rv = NS_OK;

  mPresContext = aPresContext;
  mBlock = aBlock;
  mSpaceManager = aSpaceManager;
  mBlockIsPseudo = aBlock->IsPseudoFrame();
  mCurrentLine = nsnull;
  mPrevKidFrame = nsnull;

  mX = 0;
  mY = 0;
  mAvailSize = aMaxSize;
  mUnconstrainedWidth = PRBool(mAvailSize.width == NS_UNCONSTRAINEDSIZE);
  mUnconstrainedHeight = PRBool(mAvailSize.height == NS_UNCONSTRAINEDSIZE);
  mMaxElementSizePointer = aMaxElementSize;
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }
  mKidXMost = 0;

  mPrevPosBottomMargin = 0;
  mPrevNegBottomMargin = 0;

  mNextListOrdinal = -1;
  mFirstChildIsInsideBullet = PR_FALSE;

  return rv;
}

// Recover the block reflow state to what it should be if aLine is about
// to be reflowed. aLine should not be nsnull
nsresult nsBlockReflowState::RecoverState(nsLineData* aLine)
{
  NS_PRECONDITION(nsnull != aLine, "null parameter");
  nsLineData* prevLine = aLine->mPrevLine;

  if (nsnull != prevLine) {
    // Compute the running y-offset, and the available height
    mY = prevLine->mBounds.YMost();
    if (!mUnconstrainedHeight) {
      mAvailSize.height -= mY;
    }
  
    // Compute the kid x-most
    for (nsLineData* l = prevLine; nsnull != l; l = l->mPrevLine) {
      nscoord xmost = l->mBounds.XMost();
      if (xmost > mKidXMost) {
        mKidXMost = xmost;
      }
    }
  
    // If the previous line is a block, then factor in its bottom margin
    if (prevLine->mIsBlock) {
      nsStyleSpacing* kidSpacing;
      nsIFrame*       kid = prevLine->mFirstChild;
  
      kid->GetStyleData(kStyleSpacingSID, (nsStyleStruct*&)kidSpacing);
      nsMargin  kidMargin;
      kidSpacing->CalcMarginFor(kid, kidMargin);
      if (kidMargin.bottom < 0) {
        mPrevPosBottomMargin = 0;
        mPrevNegBottomMargin = -kidMargin.bottom;
      } else {
        mPrevPosBottomMargin = kidMargin.bottom;
        mPrevNegBottomMargin = 0;
      }
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
nsBlockFrame::NewFrame(nsIFrame** aInstancePtrResult,
                       nsIContent* aContent,
                       nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsBlockFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsBlockFrame::nsBlockFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

nsBlockFrame::~nsBlockFrame()
{
  DestroyLines();
}

void nsBlockFrame::DestroyLines()
{
  nsLineData* line = mLines;
  while (nsnull != line) {
    nsLineData* next = line->mNextLine;
    delete line;
    line = next;
  }
  mLines = nsnull;
}

NS_METHOD
nsBlockFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kBlockFrameCID)) {
    *aInstancePtr = (void*) (this);
    return NS_OK;
  }
  else if (aIID.Equals(kIRunaroundIID)) {
    *aInstancePtr = (void*) ((nsIRunaround*) this);
    return NS_OK;
  }
  else if (aIID.Equals(kIFloaterContainerIID)) {
    *aInstancePtr = (void*) ((nsIFloaterContainer*) this);
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

NS_METHOD
nsBlockFrame::IsSplittable(SplittableType& aIsSplittable) const
{
  aIsSplittable = SplittableNonRectangular;
  return NS_OK;
}

NS_METHOD
nsBlockFrame::CreateContinuingFrame(nsIPresContext*  aCX,
                                    nsIFrame*        aParent,
                                    nsIStyleContext* aStyleContext,
                                    nsIFrame*&       aContinuingFrame)
{
  nsBlockFrame* cf = new nsBlockFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aCX, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

NS_METHOD
nsBlockFrame::ListTag(FILE* out) const
{
  if ((nsnull != mGeometricParent) && IsPseudoFrame()) {
    fprintf(out, "*block<");
    nsIAtom* atom = mContent->GetTag();
    if (nsnull != atom) {
      nsAutoString tmp;
      atom->ToString(tmp);
      fputs(tmp, out);
    }
    PRInt32 contentIndex;
    GetContentIndex(contentIndex);
    fprintf(out, ">(%d)@%p", contentIndex, this);
  } else {
    nsHTMLContainerFrame::ListTag(out);
  }
  return NS_OK;
}

NS_METHOD
nsBlockFrame::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag
  ListTag(out);

  // Output the first/last content offset
  fprintf(out, "[%d,%d,%c] ", mFirstContentOffset, mLastContentOffset,
          (mLastContentIsComplete ? 'T' : 'F'));
  if (nsnull != mPrevInFlow) {
    fprintf(out, "prev-in-flow=%p ", mPrevInFlow);
  }
  if (nsnull != mNextInFlow) {
    fprintf(out, "next-in-flow=%p ", mNextInFlow);
  }

  // Output the rect
  out << mRect;

  // Output the children, one line at a time
  if (nsnull != mLines) {
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("<\n", out);
    aIndent++;

    nsLineData* line = mLines;
    while (nsnull != line) {
      line->List(out, aIndent);
      line = line->mNextLine;
    }

    aIndent--;
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs(">\n", out);
  } else {
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("<>\n", out);
  }

  return NS_OK;
}

NS_METHOD
nsBlockFrame::VerifyTree() const
{
#ifdef NS_DEBUG
  nsresult rv = nsHTMLContainerFrame::VerifyTree();
  if (NS_OK != rv) {
    return rv;
  }
  rv = VerifyLines(PR_TRUE);
  return rv;
#else
  return NS_OK;
#endif
}

#ifdef NS_DEBUG
nsresult
nsBlockFrame::VerifyLines(PRBool aFinalCheck) const
{
  nsresult rv = NS_OK;

  // Make sure that the list of children agrees with our child count.
  // If this is not the case then the child list and the line list are
  // not properly arranged.
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(mChildCount == len, "bad child list");

  // Verify that our lines are correctly setup
  PRInt32 offset = mFirstContentOffset;
  PRInt32 lineChildCount = 0;
  nsLineData* line = mLines;
  nsLineData* prevLine = nsnull;
  while (nsnull != line) {
    if (aFinalCheck) {
      NS_ASSERTION(((offset == line->mFirstContentOffset) &&
                    (line->mFirstContentOffset <= line->mLastContentOffset)),
                   "bad line mFirstContentOffset");
      NS_ASSERTION(line->mLastContentOffset <= mLastContentOffset,
                   "bad line mLastContentOffset");
      offset = line->mLastContentOffset;
      if (line->mLastContentIsComplete) {
        offset++;
      }
    }
    lineChildCount += line->mChildCount;
    rv = line->Verify(aFinalCheck);
    if (NS_OK != rv) {
      return rv;
    }
    prevLine = line;
    line = line->mNextLine;
  }
  if (aFinalCheck && (nsnull != prevLine)) {
    NS_ASSERTION(prevLine->mLastContentOffset == mLastContentOffset,
                 "bad mLastContentOffset");
    NS_ASSERTION(prevLine->mLastContentIsComplete == mLastContentIsComplete,
                 "bad mLastContentIsComplete");
  }
  NS_ASSERTION(lineChildCount == mChildCount, "bad line counts");

  return rv;
}
#endif

//----------------------------------------------------------------------

// Remove a next-in-flow from from this block's list of lines

// XXX problems here:
// 1. we always have to start from the first line: slow!
// 2. we can avoid this when the child is not the last child in a line

void
nsBlockFrame::WillDeleteNextInFlowFrame(nsIFrame* aNextInFlow)
{
  // When a reflow indicates completion it's possible that
  // next-in-flows were just removed. We have to remove them from any
  // nsLineData's that follow the current line.
  nsLineData* line = mLines;
  while (nsnull != line) {
    if (line->mFirstChild == aNextInFlow) {
      // Remove child from line.
      if (0 == --line->mChildCount) {
        line->mFirstChild = nsnull;
      }
      else {
        // Fixup the line
        nsIFrame* nextKid;
        aNextInFlow->GetNextSibling(nextKid);
        line->mFirstChild = nextKid;
        nextKid->GetContentIndex(line->mFirstContentOffset);
      }
      break;
    }
    line = line->mNextLine;
  }
}

nsresult
nsBlockFrame::ReflowInlineChild(nsIFrame*        aKidFrame,
                                nsIPresContext*  aPresContext,
                                nsReflowMetrics& aDesiredSize,
                                const nsSize&    aMaxSize,
                                nsSize*          aMaxElementSize,
                                nsReflowStatus&  aStatus)
{
  aStatus = ReflowChild(aKidFrame, aPresContext, aDesiredSize, aMaxSize,
                        aMaxElementSize);
  return NS_OK;
}

nsresult
nsBlockFrame::ReflowBlockChild(nsIFrame*        aKidFrame,
                               nsIPresContext*  aPresContext,
                               nsISpaceManager* aSpaceManager,
                               const nsSize&    aMaxSize,
                               nsRect&          aDesiredRect,
                               nsSize*          aMaxElementSize,
                               nsReflowStatus&  aStatus)
{
  aStatus = ReflowChild(aKidFrame, aPresContext, aSpaceManager,
                        aMaxSize, aDesiredRect, aMaxElementSize);
  return NS_OK;
}

nsLineData*
nsBlockFrame::CreateLineForOverflowList(nsIFrame* aOverflowList)
{
  nsLineData* newLine = new nsLineData();
  if (nsnull != newLine) {
    nsIFrame* kid = aOverflowList;
    newLine->mFirstChild = kid;
    kid->GetContentIndex(newLine->mFirstContentOffset);
    newLine->mLastContentOffset = -1;
    newLine->mLastContentIsComplete = PRPackedBool(0x255);
    PRInt32 kids = 0;
    while (nsnull != kid) {
      kids++;
      kid->GetNextSibling(kid);
    }
    newLine->mChildCount = kids;
  }
  return newLine;
}

void
nsBlockFrame::DrainOverflowList()
{
  nsBlockFrame* prevBlock = (nsBlockFrame*) mPrevInFlow;
  if (nsnull != prevBlock) {
    nsIFrame* overflowList = prevBlock->mOverflowList;
    if (nsnull != overflowList) {
      NS_ASSERTION(nsnull == mFirstChild, "bad overflow list");
      NS_ASSERTION(nsnull == mLines, "bad overflow list");

      // Create a line to hold the entire overflow list
      nsLineData* newLine = CreateLineForOverflowList(overflowList);

      // Place the children on our child list; this also reassigns
      // their geometric parent and updates our mChildCount.
      AppendChildren(overflowList);
      prevBlock->mOverflowList = nsnull;

      // The new line is the first line
      mLines = newLine;
    }
  }

  if (nsnull != mOverflowList) {
    NS_ASSERTION(nsnull != mFirstChild,
                 "overflow list but no mapped children");

    // Create a line to hold the overflow list
    nsLineData* newLine = CreateLineForOverflowList(mOverflowList);

    // Place the children on our child list; this also reassigns
    // their geometric parent and updates our mChildCount.
    AppendChildren(mOverflowList, PR_FALSE);
    mOverflowList = nsnull;

    // The new line is appended after our other lines
    nsLineData* prevLine = nsnull;
    nsLineData* line = mLines;
    while (nsnull != line) {
      prevLine = line;
      line = line->mNextLine;
    }
    if (nsnull == prevLine) {
      mLines = newLine;
    }
    else {
      prevLine->mNextLine = newLine;
      newLine->mPrevLine = prevLine;
    }
  }
#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_FALSE);
  }
#endif
}

nsresult
nsBlockFrame::PlaceLine(nsBlockReflowState&     aState,
                        nsLineLayout&           aLineLayout,
                        nsLineData*             aLine)
{
  // Before we place the line, make sure that it will fit in it's new
  // location. It always fits if the height isn't constrained or it's
  // the first line.
  if (!aState.mUnconstrainedHeight && (aLine != mLines)) {
    if (aState.mY + aLine->mBounds.height > aState.mAvailSize.height) {
      // The line will not fit
      return PushLines(aState, aLine);
    }
  }

  // Consume space and advance running values
  aState.mY += aLine->mBounds.height;
  if (nsnull != aState.mMaxElementSizePointer) {
    nsSize* maxSize = aState.mMaxElementSizePointer;
    if (aLineLayout.mReflowData.mMaxElementSize.width > maxSize->width) {
      maxSize->width = aLineLayout.mReflowData.mMaxElementSize.width;
    }
    if (aLineLayout.mReflowData.mMaxElementSize.height > maxSize->height) {
      maxSize->height = aLineLayout.mReflowData.mMaxElementSize.height;
    }
  }
  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
  }

  // Any below current line floaters to place?
  if (aState.mPendingFloaters.Count() > 0) {
    PlaceBelowCurrentLineFloaters(aState, aState.mY);
    // XXX Factor in the height of the floaters as well when considering
    // whether the line fits.
    // The default policy is that if there isn't room for the floaters then
    // both the line and the floaters are pushed to the next-in-flow...
  }

  if (aState.mY >= aState.mCurrentBand.availSpace.YMost()) {
    // The current y coordinate is now past our available space
    // rectangle. Get a new band of space.
    GetAvailableSpace(aState, aState.mY);
  }

  return NS_LINE_LAYOUT_COMPLETE;
}

// aY has borderpadding.top already factored in
// aResult is relative to left,aY
nsresult
nsBlockFrame::GetAvailableSpace(nsBlockReflowState& aState, nscoord aY)
{
  nsresult rv = NS_OK;

  nsISpaceManager* sm = aState.mSpaceManager;

  // Fill in band data for the specific Y coordinate
  sm->Translate(aState.mBorderPadding.left, 0);
  sm->GetBandData(aY, aState.mAvailSize, aState.mCurrentBand);
  sm->Translate(-aState.mBorderPadding.left, 0);

  // Compute the bounding rect of the available space, i.e. space
  // between any left and right floaters
  aState.mCurrentBand.ComputeAvailSpaceRect();

#if 0
  // XXX For now we assume that there are no height restrictions
  // (e.g. no "float to bottom of column/page")
  nsRect& availSpace = aState.mCurrentBand.availSpace;
  aResult.x = availSpace.x;
  aResult.y = availSpace.y;
  aResult.width = availSpace.width;
  aResult.height = aState.mAvailSize.height;
#else
  aState.mCurrentBand.availSpace.MoveBy(aState.mBorderPadding.left,
                                        aState.mY);
#endif

  return rv;
}

// Give aLine and any successive lines to the block's next-in-flow; if
// we don't have a next-in-flow then push all the children onto our
// overflow list.
nsresult
nsBlockFrame::PushLines(nsBlockReflowState& aState, nsLineData* aLine)
{
  PRInt32 i;

  // Split our child-list in two; revert our last content offset and
  // completion status to the previous line.
  nsLineData* prevLine = aLine->mPrevLine;
  NS_PRECONDITION(nsnull != prevLine, "pushing first line");
  nsIFrame* prevKidFrame = prevLine->mFirstChild;
  for (i = prevLine->mChildCount - 1; --i >= 0; ) {
    prevKidFrame->GetNextSibling(prevKidFrame);
  }
#ifdef NS_DEBUG
  nsIFrame* nextFrame;
  prevKidFrame->GetNextSibling(nextFrame);
  NS_ASSERTION(nextFrame == aLine->mFirstChild, "bad line list");
#endif
  prevKidFrame->SetNextSibling(nsnull);
  prevLine->mNextLine = nsnull;
  mLastContentOffset = prevLine->mLastContentOffset;
  mLastContentIsComplete = prevLine->mLastContentIsComplete;

  // Push children to our next-in-flow if we have, or to our overflow list
  nsBlockFrame* nextInFlow = (nsBlockFrame*) mNextInFlow;
  PRInt32 pushCount = 0;
  if (nsnull == nextInFlow) {
    // Place children on the overflow list
    mOverflowList = aLine->mFirstChild;

    // Destroy the lines
    nsLineData* line = aLine;
    while (nsnull != line) {
      pushCount += line->mChildCount;
      nsLineData* next = line->mNextLine;
      delete line;
      line = next;
    }
  }
  else {
    aLine->mPrevLine = nsnull;

    // Pass on the children to our next-in-flow
    nsLineData* line = aLine;
    prevLine = line;
    nsIFrame* lastKid = aLine->mFirstChild;
    nsIFrame* kid = lastKid;
    while (nsnull != line) {
      i = line->mChildCount;
      pushCount += i;
      NS_ASSERTION(kid == line->mFirstChild, "bad line list");
      while (--i >= 0) {
        kid->SetGeometricParent(nextInFlow);
        nsIFrame* contentParent;
        kid->GetContentParent(contentParent);
        if (this == contentParent) {
          kid->SetContentParent(nextInFlow);
        }
        lastKid = kid;
        kid->GetNextSibling(kid);
      }
      prevLine = line;
      line = line->mNextLine;
    }

    // Join the two line lists
    nsLineData* nextInFlowLine = nextInFlow->mLines;
    if (nsnull != nextInFlowLine) {
      lastKid->SetNextSibling(nextInFlowLine->mFirstChild);
      nextInFlowLine->mPrevLine = prevLine;
    }
    nsIFrame* firstKid = aLine->mFirstChild;
    prevLine->mNextLine = nextInFlowLine;
    nextInFlow->mLines = aLine;
    nextInFlow->mFirstChild = firstKid;
    nextInFlow->mChildCount += pushCount;
    firstKid->GetContentIndex(nextInFlow->mFirstContentOffset);

#ifdef NS_DEBUG
    if (GetVerifyTreeEnable()) {
      nextInFlow->VerifyLines(PR_FALSE);
    }
#endif
  }
  mChildCount -= pushCount;

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_TRUE);
  }
#endif
  return NS_LINE_LAYOUT_NOT_COMPLETE;
}

nsresult
nsBlockFrame::ReflowMapped(nsBlockReflowState& aState)
{
  nsresult rv = NS_OK;

  // Get some space to start reflowing with
  GetAvailableSpace(aState, aState.mY);

  nsLineData* line = mLines;
  nsLineLayout lineLayout(aState);
  aState.mCurrentLine = &lineLayout;
  while (nsnull != line) {
    // Initialize the line layout for this line
    rv = lineLayout.Initialize(aState, line);
    if (NS_OK != rv) {
      goto done;
    }
    lineLayout.mPrevKidFrame = aState.mPrevKidFrame;

    // Reflow the line
    nsresult lineReflowStatus = lineLayout.ReflowLine();
    if (PRInt32(lineReflowStatus) < 0) {
      // Some kind of hard error
      rv = lineReflowStatus;
      goto done;
    }
    mChildCount += lineLayout.mNewFrames;

    // Now place it. It's possible it won't fit.
    rv = PlaceLine(aState, lineLayout, line);
    if (NS_LINE_LAYOUT_COMPLETE != rv) {
      goto done;
    }

    mLastContentOffset = line->mLastContentOffset;
    mLastContentIsComplete = PRBool(line->mLastContentIsComplete);
    line = line->mNextLine;
    aState.mPrevKidFrame = lineLayout.mPrevKidFrame;
  }

done:
  aState.mCurrentLine = nsnull;
#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_TRUE);
  }
#endif
  return rv;
}

// XXX This is a short-term hack. It assumes that the caller has already recovered
// the state, and that some space has been retrieved from the space manager...
nsresult
nsBlockFrame::ReflowMappedFrom(nsBlockReflowState& aState, nsLineData* aLine)
{
  nsresult rv = NS_OK;

  nsLineLayout lineLayout(aState);
  aState.mCurrentLine = &lineLayout;
  while (nsnull != aLine) {
    // Initialize the line layout for this line
    rv = lineLayout.Initialize(aState, aLine);
    if (NS_OK != rv) {
      goto done;
    }
    lineLayout.mPrevKidFrame = aState.mPrevKidFrame;

    // Reflow the line
    nsresult lineReflowStatus = lineLayout.ReflowLine();
    if (PRInt32(lineReflowStatus) < 0) {
      // Some kind of hard error
      rv = lineReflowStatus;
      goto done;
    }
    mChildCount += lineLayout.mNewFrames;

    // Now place it. It's possible it won't fit.
    rv = PlaceLine(aState, lineLayout, aLine);
    if (NS_LINE_LAYOUT_COMPLETE != rv) {
      goto done;
    }

    mLastContentOffset = aLine->mLastContentOffset;
    mLastContentIsComplete = PRBool(aLine->mLastContentIsComplete);
    aLine = aLine->mNextLine;
    aState.mPrevKidFrame = lineLayout.mPrevKidFrame;
  }

done:
  aState.mCurrentLine = nsnull;
#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_TRUE);
  }
#endif
  return rv;
}

nsresult
nsBlockFrame::ReflowUnmapped(nsBlockReflowState& aState)
{
  nsresult rv = NS_OK;

  // If we have no children and we have a prev-in-flow then we need to
  // pick up where it left off. If we have children, e.g. we're being
  // resized, then our content offset will have already been set
  // correctly.
  nsIFrame* kidPrevInFlow = nsnull;
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    nsBlockFrame* prev = (nsBlockFrame*) mPrevInFlow;
    mFirstContentOffset = prev->NextChildOffset();// XXX Is this necessary?
    if (PR_FALSE == prev->mLastContentIsComplete) {
      // Our prev-in-flow's last child is not complete
      prev->LastChild(kidPrevInFlow);
    }
  }

  // Get to the last line where the new content may be added
  nsLineData* line = nsnull;
  nsLineData* prevLine = nsnull;
  if (nsnull != mLines) {
    line = mLines;
    while (nsnull != line->mNextLine) {
      line = line->mNextLine;
    }
    prevLine = line->mPrevLine;

    // If the last line is not complete then kidPrevInFlow should be
    // set to the last-line's last child.
    if (!line->mLastContentIsComplete) {
      kidPrevInFlow = line->GetLastChild();
    }
  }

  // Get some space to start reflowing with
  GetAvailableSpace(aState, aState.mY);

  // Now reflow the new content until we are out of new content or out
  // of vertical space.
  PRInt32 kidIndex = NextChildOffset();
  nsLineLayout lineLayout(aState);
  aState.mCurrentLine = &lineLayout;
  lineLayout.mKidPrevInFlow = kidPrevInFlow;
  PRInt32 contentChildCount = mContent->ChildCount();
  while (kidIndex < contentChildCount) {
    if (nsnull == line) {
      if (!MoreToReflow(aState)) {
        break;
      }
      line = new nsLineData();
      if (nsnull == line) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto done;
      }
      line->mFirstContentOffset = kidIndex;
    }

    // Initialize the line layout for this line
    rv = lineLayout.Initialize(aState, line);
    if (NS_OK != rv) {
      goto done;
    }
    lineLayout.mKidPrevInFlow = kidPrevInFlow;
    lineLayout.mPrevKidFrame = aState.mPrevKidFrame;

    // Reflow the line
    nsresult lineReflowStatus = lineLayout.ReflowLine();
    if (PRInt32(lineReflowStatus) < 0) {
      // Some kind of hard error
      rv = lineReflowStatus;
      goto done;
    }
    mChildCount += lineLayout.mNewFrames;

    // Add line to the block; do this before placing the line in case
    // PushLines is needed.
    if (nsnull == prevLine) {
      // For the first line, initialize mFirstContentOffset
      mFirstContentOffset = line->mFirstContentOffset;
      mFirstChild = line->mFirstChild;
      mLines = line;
    }
    else {
      prevLine->mNextLine = line;
      line->mPrevLine = prevLine;
    }

    // Now place it. It's possible it won't fit.
    rv = PlaceLine(aState, lineLayout, line);
    if (NS_LINE_LAYOUT_COMPLETE != rv) {
      goto done;
    }
    kidIndex = lineLayout.mKidIndex;
    kidPrevInFlow = lineLayout.mKidPrevInFlow;

    mLastContentOffset = line->mLastContentOffset;
    mLastContentIsComplete = PRBool(line->mLastContentIsComplete);
    prevLine = line;
    line = line->mNextLine;
    aState.mPrevKidFrame = lineLayout.mPrevKidFrame;
  }

done:
  aState.mCurrentLine = nsnull;
  if (aState.mBlockIsPseudo) {
    PropagateContentOffsets();
  }

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_TRUE);
  }
#endif
  return rv;
}

nsresult
nsBlockFrame::InitializeState(nsIPresContext*     aPresContext,
                              nsISpaceManager*    aSpaceManager,
                              const nsSize&       aMaxSize,
                              nsSize*             aMaxElementSize,
                              nsBlockReflowState& aState)
{
  nsresult rv;
  rv = aState.Initialize(aPresContext, aSpaceManager,
                         aMaxSize, aMaxElementSize, this);

  // Apply border and padding adjustments for regular frames only
  if (!aState.mBlockIsPseudo) {
    nsStyleSpacing* mySpacing = (nsStyleSpacing*)
      mStyleContext->GetData(kStyleSpacingSID);
    nsStylePosition* myPosition = (nsStylePosition*)
      mStyleContext->GetData(kStylePositionSID);

    mySpacing->CalcBorderPaddingFor(this, aState.mBorderPadding);
    aState.mY = aState.mBorderPadding.top;
    aState.mX = aState.mBorderPadding.left;

    if (aState.mUnconstrainedWidth) {
      // If our width is unconstrained don't bother futzing with the
      // available width/height because they don't matter - we are
      // going to get reflowed again.
      aState.mDeltaWidth = NS_UNCONSTRAINEDSIZE;
    }
    else {
      // When we are constrained we need to apply the width/height
      // style properties. When we have a width/height it applies to
      // the content width/height of our box. The content width/height
      // doesn't include the border+padding so we have to add that in
      // instead of subtracting it out of our maxsize.
      nscoord lr =
        aState.mBorderPadding.left + aState.mBorderPadding.right;

      // Get and apply the stylistic size. Note: do not limit the
      // height until we are done reflowing.
      PRIntn ss = aState.mStyleSizeFlags =
        nsCSSLayout::GetStyleSize(aPresContext, this, aState.mStyleSize);
      if (0 != (ss & NS_SIZE_HAS_WIDTH)) {
        aState.mAvailSize.width = aState.mStyleSize.width + lr;
      }
      else {
        aState.mAvailSize.width -= lr;
      }
      aState.mDeltaWidth = aState.mAvailSize.width - mRect.width;
    }
  }
  else {
    aState.mBorderPadding.SizeTo(0, 0, 0, 0);
    aState.mDeltaWidth = aState.mAvailSize.width - mRect.width;
  }

  // Setup initial list ordinal value
  nsIAtom* tag = mContent->GetTag();
  if ((tag == nsHTMLAtoms::ul) || (tag == nsHTMLAtoms::ol) ||
      (tag == nsHTMLAtoms::menu) || (tag == nsHTMLAtoms::dir)) {
    nsHTMLValue value;
    if (eContentAttr_HasValue ==
        ((nsIHTMLContent*)mContent)->GetAttribute(nsHTMLAtoms::start, value)) {
      if (eHTMLUnit_Integer == value.GetUnit()) {
        aState.mNextListOrdinal = value.GetIntValue();
      }
    }
  }
  NS_RELEASE(tag);

  return rv;
}

PRBool
nsBlockFrame::MoreToReflow(nsBlockReflowState& aState)
{
  PRBool rv = PR_FALSE;
  if (NextChildOffset() < mContent->ChildCount()) {
    rv = PR_TRUE;
  }
  return rv;
}

nsBlockReflowState*
nsBlockFrame::FindBlockReflowState(nsIPresContext* aPresContext,
                                   nsIFrame* aFrame)
{
  nsBlockReflowState* state = nsnull;
  if (nsnull != aFrame) {
    nsIFrame* parent;
    aFrame->GetGeometricParent(parent);
    while (nsnull != parent) {
      nsBlockFrame* block;
      nsresult rv = parent->QueryInterface(kBlockFrameCID, (void**) &block);
      if (NS_OK == rv) {
        nsIPresShell* shell = aPresContext->GetShell();
        state = (nsBlockReflowState*) shell->GetCachedData(block);
        NS_RELEASE(shell);
        break;
      }
      parent->GetGeometricParent(parent);
    }
  }
  return state;
}

nsresult
nsBlockFrame::DoResizeReflow(nsBlockReflowState& aState,
                             const nsSize&       aMaxSize,
                             nsRect&             aDesiredRect,
                             nsReflowStatus&     aStatus)
{
  NS_FRAME_TRACE_MSG(("enter nsBlockFrame::DoResizeReflow: deltaWidth=%d",
                      aState.mDeltaWidth));

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_TRUE);
    PreReflowCheck();
  }
#endif

  nsresult rv = NS_OK;
  nsIPresShell* shell = aState.mPresContext->GetShell();
  shell->PutCachedData(this, &aState);

  // Check for an overflow list
  DrainOverflowList();

  if (nsnull != mLines) {
    rv = ReflowMapped(aState);
  }

  if (NS_OK == rv) {
    if ((nsnull != mLines) && (aState.mAvailSize.height <= 0)) {
      // We are out of space
    }
    if (MoreToReflow(aState)) {
      rv = ReflowUnmapped(aState);
    }
  }

  // Return our desired rect
  ComputeDesiredRect(aState, aMaxSize, aDesiredRect);

  // Set return status
  aStatus = NS_FRAME_COMPLETE;
  if (NS_LINE_LAYOUT_NOT_COMPLETE == rv) {
    rv = NS_OK;
    aStatus = NS_FRAME_NOT_COMPLETE;
  }
#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    // Verify that the line layout code pulled everything up when it
    // indicates a complete reflow.
    if (NS_FRAME_IS_COMPLETE(aStatus)) {
      nsBlockFrame* nextBlock = (nsBlockFrame*) mNextInFlow;
      while (nsnull != nextBlock) {
        NS_ASSERTION((nsnull == nextBlock->mLines) &&
                     (nextBlock->mChildCount == 0) &&
                     (nsnull == nextBlock->mFirstChild),
                     "bad completion status");
        nextBlock = (nsBlockFrame*) nextBlock->mNextInFlow;
      }

#if XXX
      // We better not be in the same parent frame as our prev-in-flow.
      // If we are it means that we were continued instead of pulling up
      // children.
      if (nsnull != mPrevInFlow) {
        nsIFrame* ourParent = mGeometricParent;
        nsIFrame* prevParent = ((nsBlockFrame*)mPrevInFlow)->mGeometricParent;
        NS_ASSERTION(ourParent != prevParent, "bad continuation");
      }
#endif
    }
  }
#endif

  // Now that reflow has finished, remove the cached pointer
  shell->RemoveCachedData(this);
  NS_RELEASE(shell);

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_TRUE);
    PostReflowCheck(aStatus);
  }
#endif

  NS_FRAME_TRACE_REFLOW_OUT("nsBlockFrame::DoResizeReflow", aStatus);
  return rv;
}

//----------------------------------------------------------------------

NS_METHOD
nsBlockFrame::ContentAppended(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer)
{
  return nsHTMLContainerFrame::ContentAppended(aShell, aPresContext, aContainer);
}

NS_METHOD
nsBlockFrame::ContentInserted(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer,
                              nsIContent*     aChild,
                              PRInt32         aIndexInParent)
{
  return nsHTMLContainerFrame::ContentInserted(aShell, aPresContext, aContainer,
                                               aChild, aIndexInParent);
}

NS_METHOD
nsBlockFrame::ContentReplaced(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer,
                              nsIContent*     aOldChild,
                              nsIContent*     aNewChild,
                              PRInt32         aIndexInParent)
{
  nsresult rv = NS_OK;
  return rv;
}

NS_METHOD
nsBlockFrame::ContentDeleted(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInParent)
{
  nsresult rv = NS_OK;
  return rv;
}

// XXX if there is nothing special going on here, then remove this
// implementation and let nsFrame do it.
NS_METHOD
nsBlockFrame::GetReflowMetrics(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aMetrics)
{
  nsresult rv = NS_OK;
  aMetrics.width = mRect.width;
  aMetrics.height = mRect.height;
  aMetrics.ascent = mRect.height;
  aMetrics.descent = 0;
  return rv;
}

//----------------------------------------------------------------------

NS_METHOD
nsBlockFrame::ResizeReflow(nsIPresContext*  aPresContext,
                           nsISpaceManager* aSpaceManager,
                           const nsSize&    aMaxSize,
                           nsRect&          aDesiredRect,
                           nsSize*          aMaxElementSize,
                           nsReflowStatus&  aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;
  nsBlockReflowState state;
  rv = InitializeState(aPresContext, aSpaceManager, aMaxSize,
                       aMaxElementSize, state);
  if (NS_OK == rv) {
    nsRect desiredRect;
    rv = DoResizeReflow(state, aMaxSize, aDesiredRect, aStatus);
  }
  return rv;
}

nsLineData* nsBlockFrame::FindLine(nsIFrame* aFrame)
{
  // Find the line that contains the aFrame
  nsLineData* line = mLines;
  while (nsnull != line) {
    nsIFrame* child = line->mFirstChild;
    for (PRInt32 count = line->mChildCount; count > 0; count--) {
      if (child == aFrame) {
        return line;
      }

      child->GetNextSibling(child);
    }

    // Move to the next line
    line = line->mNextLine;
  }

  return nsnull;
}

nsLineData* nsBlockFrame::LastLine()
{
  nsLineData* lastLine = mLines;

  // Get the last line
  if (nsnull != lastLine) {
    while (nsnull != lastLine->mNextLine) {
      lastLine = lastLine->mNextLine;
    }
  }

  return lastLine;
}

nsresult nsBlockFrame::IncrementalReflowAfter(nsBlockReflowState& aState,
                                              nsLineData*         aLine,
                                              nsresult            aReflowStatus,
                                              const nsRect&       aOldBounds)
{
  // Now just reflow all the lines that follow...
  // XXX Obviously this needs to be more efficient
  return ReflowMappedFrom(aState, aLine->mNextLine);
}

NS_METHOD
nsBlockFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                nsISpaceManager* aSpaceManager,
                                const nsSize&    aMaxSize,
                                nsRect&          aDesiredRect,
                                nsReflowCommand& aReflowCommand,
                                nsReflowStatus&  aStatus)
{
  NS_FRAME_TRACE_REFLOW_IN("nsBlockFrame::IncrementalReflow");
#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_TRUE);
    PreReflowCheck();
  }
#endif
  
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;
  nsBlockReflowState state;
  rv = InitializeState(aPresContext, aSpaceManager, aMaxSize,
                       nsnull, state);

  nsIPresShell* shell = state.mPresContext->GetShell();
  shell->PutCachedData(this, &state);

  // Is the reflow command target at us?
  if (this == aReflowCommand.GetTarget()) {
    if (aReflowCommand.GetType() == nsReflowCommand::FrameAppended) {
      nsLineData* lastLine = LastLine();

      // Restore the state
      if (nsnull != lastLine) {
        state.RecoverState(lastLine);
      }

      // Reflow unmapped children
      PRInt32 kidIndex = NextChildOffset();
      PRInt32 contentChildCount = mContent->ChildCount();
      if (kidIndex == contentChildCount) {
        // There is nothing to do here
        if (nsnull != lastLine) {
          state.mY = lastLine->mBounds.YMost();
        }
      }
      else {
        rv = ReflowUnmapped(state);
      }
#if 0
    } else if (aReflowCommand.GetType() == nsReflowCommand::ContentChanged) {
      // Restore our state as if the child that changed is the next frame to reflow
      nsLineData* line = FindLine(aReflowCommand.GetChildFrame());
      state.RecoverState(line);

      // Get some available space to start reflowing with
      GetAvailableSpace(state, state.mY);
  
      // Reflow the affected line, and all the lines that follow...
      // XXX Obviously this needs to be more efficient
      rv = ReflowMappedFrom(state, line);
#endif
    } else {
      NS_NOTYETIMPLEMENTED("unexpected reflow command");
    }
 } else {
    // The command is passing through us. Get the next frame in the reflow chain
    nsIFrame* nextFrame = aReflowCommand.GetNext();

    // Restore our state as if nextFrame is the next frame to reflow
    nsLineData* line = FindLine(nextFrame);
    state.RecoverState(line);

    // Get some available space to start reflowing with
    GetAvailableSpace(state, state.mY);

    // Reflow the affected line
    nsLineLayout  lineLayout(state);

    state.mCurrentLine = &lineLayout;
    lineLayout.Initialize(state, line);

    // Have the line handle the incremental reflow
    nsRect  oldBounds = line->mBounds;
    rv = lineLayout.IncrementalReflowFromChild(aReflowCommand, nextFrame);

    // Now place the line. It's possible it won't fit
    rv = PlaceLine(state, lineLayout, line);
    // XXX The way NS_LINE_LAYOUT_COMPLETE is being used is very confusing...
    if (NS_LINE_LAYOUT_COMPLETE == rv) {
      mLastContentOffset = line->mLastContentOffset;
      mLastContentIsComplete = PRBool(line->mLastContentIsComplete);
      state.mPrevKidFrame = lineLayout.mPrevKidFrame;

      // Now figure out what to do with the frames that follow
      rv = IncrementalReflowAfter(state, line, rv, oldBounds);
    }
  }

  // Return our desired rect
  ComputeDesiredRect(state, aMaxSize, aDesiredRect);

  // Set return status
  aStatus = NS_FRAME_COMPLETE;
  if (NS_LINE_LAYOUT_NOT_COMPLETE == rv) {
    rv = NS_OK;
    aStatus = NS_FRAME_NOT_COMPLETE;
  }

  // Now that reflow has finished, remove the cached pointer
  shell->RemoveCachedData(this);
  NS_RELEASE(shell);

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_TRUE);
    PostReflowCheck(aStatus);
  }
#endif
  NS_FRAME_TRACE_REFLOW_OUT("nsBlockFrame::IncrementalReflow", aStatus);
  return rv;
}

void nsBlockFrame::ComputeDesiredRect(nsBlockReflowState& aState,
                                      const nsSize&       aMaxSize,
                                      nsRect&             aDesiredRect)
{
  aDesiredRect.x = 0;
  aDesiredRect.y = 0;
  aDesiredRect.width = aState.mKidXMost + aState.mBorderPadding.right;
  if (!aState.mUnconstrainedWidth) {
    // Make sure we're at least as wide as the max size we were given
    nscoord maxWidth = aState.mAvailSize.width + aState.mBorderPadding.left +
      aState.mBorderPadding.right;
    if (aDesiredRect.width < maxWidth) {
      aDesiredRect.width = maxWidth;
    }
  }
  aState.mY += aState.mBorderPadding.bottom;
  nscoord lastBottomMargin = aState.mPrevPosBottomMargin -
    aState.mPrevNegBottomMargin;
  if (!aState.mUnconstrainedHeight && (lastBottomMargin > 0)) {
    // It's possible that we don't have room for the last bottom
    // margin (the last bottom margin is the margin following a block
    // element that we contain; it isn't applied immediately because
    // of the margin collapsing logic). This can happen when we are
    // reflowed in a limited amount of space because we don't know in
    // advance what the last bottom margin will be.
    nscoord maxY = aMaxSize.height;
    if (aState.mY + lastBottomMargin > maxY) {
      lastBottomMargin = maxY - aState.mY;
      if (lastBottomMargin < 0) {
        lastBottomMargin = 0;
      }
    }
  }
  aState.mY += lastBottomMargin;
  aDesiredRect.height = aState.mY;

  if (!aState.mBlockIsPseudo) {
    // Clamp the desired rect height when style height applies
    PRIntn ss = aState.mStyleSizeFlags;
    if (0 != (ss & NS_SIZE_HAS_HEIGHT)) {
      aDesiredRect.height = aState.mBorderPadding.top +
        aState.mStyleSize.height + aState.mBorderPadding.bottom;
    }
  }
}

//----------------------------------------------------------------------

PRBool
nsBlockFrame::AddFloater(nsIPresContext*   aPresContext,
                         nsIFrame*         aFloater,
                         PlaceholderFrame* aPlaceholder)
{
  nsIPresShell* shell = aPresContext->GetShell();
  nsBlockReflowState* state = (nsBlockReflowState*) shell->GetCachedData(this);
  NS_RELEASE(shell);

  if (nsnull != state) {
    // Get the frame associated with the space manager, and get its
    // nsIAnchoredItems interface
    nsIFrame* frame = state->mSpaceManager->GetFrame();
    nsIAnchoredItems* anchoredItems = nsnull;

    frame->QueryInterface(kIAnchoredItemsIID, (void**)&anchoredItems);
    NS_ASSERTION(nsnull != anchoredItems, "no anchored items interface");
    if (nsnull != anchoredItems) {
      anchoredItems->AddAnchoredItem(aFloater,
                                     nsIAnchoredItems::anHTMLFloater,
                                     this);
      PlaceFloater(aPresContext, aFloater, aPlaceholder, *state);
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

void
nsBlockFrame::PlaceFloater(nsIPresContext*   aPresContext,
                           nsIFrame*         aFloater,
                           PlaceholderFrame* aPlaceholder)
{
  nsIPresShell* shell = aPresContext->GetShell();
  nsBlockReflowState* state = (nsBlockReflowState*) shell->GetCachedData(this);
  NS_RELEASE(shell);
  if (nsnull != state) {
    PlaceFloater(aPresContext, aFloater, aPlaceholder, *state);
  }
}

PRBool
nsBlockFrame::IsLeftMostChild(nsIFrame* aFrame)
{
  do {
    nsIFrame* parent;
    aFrame->GetGeometricParent(parent);
  
    // See if there are any non-zero sized child frames that precede
    // aFrame in the child list
    nsIFrame* child;
    parent->FirstChild(child);
    while ((nsnull != child) && (aFrame != child)) {
      nsSize  size;

      // Is the child zero-sized?
      child->GetSize(size);
      if ((size.width > 0) || (size.height > 0)) {
        // We found a non-zero sized child frame that precedes aFrame
        return PR_FALSE;
      }
      child->GetNextSibling(child);
    }
  
    // aFrame is the left-most non-zero sized frame in its geometric parent.
    // Walk up one level and check that its parent is left-most as well
    aFrame = parent;
  } while (aFrame != this);
  return PR_TRUE;
}

void
nsBlockFrame::PlaceFloater(nsIPresContext*     aPresContext,
                           nsIFrame*           aFloater,
                           PlaceholderFrame*   aPlaceholder,
                           nsBlockReflowState& aState)
{
  // If the floater is the left-most non-zero size child frame then insert
  // it before the current line; otherwise add it to the below-current-line
  // todo list, and we'll handle it when we flush out the line
  if (IsLeftMostChild(aPlaceholder)) {
    nsISpaceManager* sm = aState.mSpaceManager;

    // Get the type of floater
    nsStyleDisplay* floaterDisplay;
    aFloater->GetStyleData(kStyleDisplaySID, (nsStyleStruct*&)floaterDisplay);

    // Commit some space in the space manager and adjust our current
    // band of available space.
    nsRect region;
    aFloater->GetRect(region);
    region.y = aState.mY;
    if (NS_STYLE_FLOAT_LEFT == floaterDisplay->mFloats) {
      region.x = aState.mX;
    } else {
      NS_ASSERTION(NS_STYLE_FLOAT_RIGHT == floaterDisplay->mFloats,
                   "bad float type");
      region.x = aState.mCurrentBand.availSpace.XMost() - region.width;
    }

    // XXX Don't forget the floater's margins...
    sm->Translate(aState.mBorderPadding.left, 0);
    sm->AddRectRegion(aFloater, region);

    // Set the origin of the floater in world coordinates
    nscoord worldX, worldY;
    sm->GetTranslation(worldX, worldY);
    aFloater->MoveTo(region.x + worldX, region.y + worldY);
    sm->Translate(-aState.mBorderPadding.left, 0);

    // Update the band of available space to reflect space taken up by
    // the floater
    GetAvailableSpace(aState, aState.mY);
    if (nsnull != aState.mCurrentLine) {
      nsLineLayout& lineLayout = *aState.mCurrentLine;
      lineLayout.SetReflowSpace(aState.mCurrentBand.availSpace);
    }
  } else {
    // Add the floater to our to-do list
    aState.mPendingFloaters.AppendElement(aFloater);
  }
}

// XXX It's unclear what coordinate space aY is in. Is it relative to the
// upper-left origin of the containing block, or relative to aState.mY?
void
nsBlockFrame::PlaceBelowCurrentLineFloaters(nsBlockReflowState& aState,
                                            nscoord aY)
{
  NS_PRECONDITION(aState.mPendingFloaters.Count() > 0, "no floaters");

  nsISpaceManager* sm = aState.mSpaceManager;
  nsBlockBandData* bd = &aState.mCurrentBand;

  // XXX Factor this code with PlaceFloater()...
  PRInt32 numFloaters = aState.mPendingFloaters.Count();
  for (PRInt32 i = 0; i < numFloaters; i++) {
    nsIFrame* floater = (nsIFrame*) aState.mPendingFloaters[i];
    nsRect region;

    // Get the band of available space
    // XXX This is inefficient to do this inside the loop...
    GetAvailableSpace(aState, aY);

    // Get the type of floater
    nsStyleDisplay* sd;
    floater->GetStyleData(kStyleDisplaySID, (nsStyleStruct*&)sd);
  
    floater->GetRect(region);
    // XXX GetAvailableSpace() is translating availSpace by aState.mY...
    region.y = bd->availSpace.y - aState.mY;
    if (NS_STYLE_FLOAT_LEFT == sd->mFloats) {
      region.x = bd->availSpace.x;
    } else {
      NS_ASSERTION(NS_STYLE_FLOAT_RIGHT == sd->mFloats, "bad float type");
      region.x = bd->availSpace.XMost() - region.width;
    }

    // XXX Don't forget the floater's margins...
    sm->Translate(aState.mBorderPadding.left, 0);
    sm->AddRectRegion(floater, region);

    // Set the origin of the floater in world coordinates
    nscoord worldX, worldY;
    sm->GetTranslation(worldX, worldY);
    floater->MoveTo(region.x + worldX, region.y + worldY);
    sm->Translate(-aState.mBorderPadding.left, 0);
  }

  aState.mPendingFloaters.Clear();

  // Pass on updated available space to the current line
  if (nsnull != aState.mCurrentLine) {
    nsLineLayout& lineLayout = *aState.mCurrentLine;
    lineLayout.SetReflowSpace(aState.mCurrentBand.availSpace);
  }
}

//----------------------------------------------------------------------

nsLineData*
nsBlockFrame::GetFirstLine()
{
  return mLines;
}

PRIntn
nsBlockFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}
