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
#include "nsIReflowCommand.h"
#include "nsPlaceholderFrame.h"
#include "nsIPtr.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLValue.h"
#include "nsCSSLayout.h"
#include "nsIView.h"

// XXX what do we do with catastrophic errors (rv < 0)? What is the
// state of the reflow world after such an error?

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

    // Stop when we get to space occupied by a right floater, or when we've
    // looked at every trapezoid and none are right floaters
    for (i = 0; i < count; i++) {
      nsBandTrapezoid*  trapezoid = &data[i];

      if (trapezoid->state != nsBandTrapezoid::Available) {
        const nsStyleDisplay* display;
      
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 numFrames = trapezoid->frames->Count();

          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (PRInt32 i = 0; i < numFrames; i++) {
            nsIFrame* f = (nsIFrame*)trapezoid->frames->ElementAt(i);

            f->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
            if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
              goto foundRightFloater;
            }
          }

        } else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
          if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
            break;
          }
        }
      }
    }
  foundRightFloater:

    if (i > 0) {
      trapezoid = &data[i - 1];
    }
  }

  if (nsBandTrapezoid::Available == trapezoid->state) {
    // The trapezoid is available
    trapezoid->GetRect(availSpace);
  } else {
    const nsStyleDisplay* display;

    // The trapezoid is occupied. That means there's no available space
    trapezoid->GetRect(availSpace);

    // XXX Handle the case of multiple frames
    trapezoid->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
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
                               const nsReflowState& aReflowState,
                               nsSize* aMaxElementSize,
                               nsBlockFrame* aBlock)
{
  nsresult rv = NS_OK;

  mPresContext = aPresContext;
  mBlock = aBlock;
  mSpaceManager = aSpaceManager;
  mBlockIsPseudo = aBlock->IsPseudoFrame();
  mListPositionOutside = PR_FALSE;
  mCurrentLine = nsnull;
  mPrevKidFrame = nsnull;
  reflowState = &aReflowState;

  mX = 0;
  mY = 0;
  mStyleSizeFlags = 0;
  mAvailSize = reflowState->maxSize;
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
  mPrevMarginSynthetic = PR_FALSE;

  mNextListOrdinal = -1;

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
      const nsStyleSpacing* kidSpacing;
      nsIFrame*             kid = prevLine->mFirstChild;
  
      kid->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)kidSpacing);
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
  delete mRunInFloaters;
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
nsBlockFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_SPLITTABLE_NON_RECTANGULAR;
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
  nsIView* view;
  GetView(view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", view);
    NS_RELEASE(view);
  }

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
  NS_ASSERTION(0 == (mState & NS_FRAME_IN_REFLOW), "frame is in reflow");
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
nsBlockFrame::ReflowInlineChild(nsLineLayout&        aLineLayout,
                                nsIFrame*            aKidFrame,
                                nsReflowMetrics&     aDesiredSize,
                                const nsReflowState& aReflowState,
                                nsReflowStatus&      aStatus)
{
  NS_PRECONDITION(aReflowState.frame == aKidFrame, "bad reflow state");
#ifdef NS_DEBUG
  nsFrameState  kidFrameState;
  aKidFrame->GetFrameState(kidFrameState);
  NS_ASSERTION(kidFrameState & NS_FRAME_IN_REFLOW, "kid frame is not in reflow");
#endif

  nsIInlineFrame* iif;
  if (NS_OK == aKidFrame->QueryInterface(kIInlineFrameIID, (void**)&iif)) {
    iif->ReflowInline(aLineLayout, aDesiredSize, aReflowState, aStatus);
  }
  else {
    aKidFrame->Reflow(aLineLayout.mPresContext, aDesiredSize, aReflowState,
                      aStatus);
  }

  if (NS_FRAME_IS_COMPLETE(aStatus)) {
    nsIFrame* kidNextInFlow;
     
    aKidFrame->GetNextInFlow(kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsIFrame* parent;
       
      aKidFrame->GetGeometricParent(parent);
      DeleteNextInFlowsFor((nsContainerFrame*)parent, aKidFrame);
    }
  }
  return NS_OK;
}

nsresult
nsBlockFrame::ReflowBlockChild(nsIFrame*            aKidFrame,
                               nsIPresContext*      aPresContext,
                               nsISpaceManager*     aSpaceManager,
                               nsReflowMetrics&     aDesiredSize,
                               nsReflowState&       aReflowState,
                               nsRect&              aDesiredRect,
                               nsReflowStatus&      aStatus)
{
  nsIRunaround*   reflowRunaround;

  NS_PRECONDITION(aReflowState.frame == aKidFrame, "bad reflow state");
#ifdef NS_DEBUG
  nsFrameState  kidFrameState;
  aKidFrame->GetFrameState(kidFrameState);
  NS_ASSERTION(kidFrameState & NS_FRAME_IN_REFLOW, "kid frame is not in reflow");
#endif

  // Get the band for this y-offset and see whether there are any floaters
  // that have changed the left/right edges.
  //
  // XXX In order to do this efficiently we should move all this code to
  // nsBlockFrame since it already has band data, and it's probably the only
  // one who calls this routine anyway
  nsBandData        bandData;
  nsBandTrapezoid   trapezoids[12];
  nsBandTrapezoid*  trapezoid = trapezoids;
  nsRect            availBand;

  bandData.trapezoids = trapezoids;
  bandData.size = 12;
  aSpaceManager->GetBandData(0, aReflowState.maxSize, bandData);

  if (bandData.count > 1) {
    // If there's more than one trapezoid that means there are floaters
    PRInt32 i;

    // Stop when we get to space occupied by a right floater, or when we've
    // looked at every trapezoid and none are right floaters
    for (i = 0; i < bandData.count; i++) {
      nsBandTrapezoid*  trapezoid = &trapezoids[i];

      if (trapezoid->state != nsBandTrapezoid::Available) {
        const nsStyleDisplay* display;
      
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 numFrames = trapezoid->frames->Count();

          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (PRInt32 i = 0; i < numFrames; i++) {
            nsIFrame* f = (nsIFrame*)trapezoid->frames->ElementAt(i);

            f->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
            if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
              goto foundRightFloater;
            }
          }

        } else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
          if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
            break;
          }
        }
      }
    }
  foundRightFloater:

    if (i > 0) {
      trapezoid = &trapezoids[i - 1];
    }
  }

  if (nsBandTrapezoid::Available == trapezoid->state) {
    // The trapezoid is available
    trapezoid->GetRect(availBand);
  } else {
    const nsStyleDisplay* display;

    // The trapezoid is occupied. That means there's no available space
    trapezoid->GetRect(availBand);

    // XXX Handle the case of multiple frames
    trapezoid->frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
    if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
      availBand.x = availBand.XMost();
    }
    availBand.width = 0;
  }

  // Does the child frame support interface nsIRunaround?
  if (NS_OK == aKidFrame->QueryInterface(kIRunaroundIID,
                                         (void**)&reflowRunaround)) {
    // Yes, the child frame wants to interact directly with the space
    // manager.
    reflowRunaround->Reflow(aPresContext, aSpaceManager, aDesiredSize, aReflowState,
                            aDesiredRect, aStatus);
  } else {
    // No, use interface nsIFrame instead.
    if (aReflowState.maxSize.width != NS_UNCONSTRAINEDSIZE) {
      if ((availBand.x > 0) || (availBand.XMost() < aReflowState.maxSize.width)) {
        // There are left/right floaters.
        aReflowState.maxSize.width = availBand.width;
      }
    }

    // XXX FIX ME
    aKidFrame->Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

    // Return the desired rect
    aDesiredRect.x = availBand.x;
    aDesiredRect.y = 0;
    aDesiredRect.width = aDesiredSize.width;
    aDesiredRect.height = aDesiredSize.height;
  }

  if (NS_FRAME_IS_COMPLETE(aStatus)) {
    nsIFrame* kidNextInFlow;
     
    aKidFrame->GetNextInFlow(kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsIFrame* parent;
      aKidFrame->GetGeometricParent(parent);
      DeleteNextInFlowsFor((nsContainerFrame*)parent, aKidFrame);
    }
  }

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

void
nsBlockFrame::ClearFloaters(nsBlockReflowState& aState, PRUint8 aBreakType)
{
  for (;;) {
    if (aState.mCurrentBand.count <= 1) {
      // No floaters in this band therefore nothing to clear
      break;
    }

    // Find the Y coordinate to clear to
    nscoord clearYMost = aState.mY;
    nsRect tmp;
    PRInt32 i;
    for (i = 0; i < aState.mCurrentBand.count; i++) {
      const nsStyleDisplay* display;
      nsBandTrapezoid* trapezoid = &aState.mCurrentBand.data[i];
      if (trapezoid->state != nsBandTrapezoid::Available) {
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 fn, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (fn = 0; fn < numFrames; fn++) {
            nsIFrame* f = (nsIFrame*) trapezoid->frames->ElementAt(fn);
            f->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);

            switch (display->mFloats) {
            case NS_STYLE_FLOAT_LEFT:
              if ((NS_STYLE_CLEAR_LEFT == aBreakType) ||
                  (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
                trapezoid->GetRect(tmp);
                nscoord ym = tmp.YMost();
                if (ym > clearYMost) {
                  clearYMost = ym;
                }
              }
              break;
            case NS_STYLE_FLOAT_RIGHT:
              if ((NS_STYLE_CLEAR_RIGHT == aBreakType) ||
                  (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
                trapezoid->GetRect(tmp);
                nscoord ym = tmp.YMost();
                if (ym > clearYMost) {
                  clearYMost = ym;
                }
              }
              break;
            }
          }
        }
        else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                         (const nsStyleStruct*&)display);
          switch (display->mFloats) {
          case NS_STYLE_FLOAT_LEFT:
            if ((NS_STYLE_CLEAR_LEFT == aBreakType) ||
                (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
              trapezoid->GetRect(tmp);
              nscoord ym = tmp.YMost();
              if (ym > clearYMost) {
                clearYMost = ym;
              }
            }
            break;
          case NS_STYLE_FLOAT_RIGHT:
            if ((NS_STYLE_CLEAR_RIGHT == aBreakType) ||
                (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
              trapezoid->GetRect(tmp);
              nscoord ym = tmp.YMost();
              if (ym > clearYMost) {
                clearYMost = ym;
              }
            }
            break;
          }
        }
      }
    }

    if (clearYMost == aState.mY) {
      // Nothing to clear
      break;
    }

    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsBlockFrame::ClearFloaters: mY=%d clearYMost=%d\n",
                  aState.mY, clearYMost));

    aState.mY = clearYMost + 1;

    // Get a new band
    GetAvailableSpace(aState, aState.mY);
  }
}

nsresult
nsBlockFrame::PlaceLine(nsBlockReflowState& aState,
                        nsLineLayout&       aLineLayout,
                        nsLineData*         aLine)
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

  // When the line is finally placed then we know that the child
  // frames have a stable location. This means that we can DidReflow
  // them marking them done. Note that when we are paginated this
  // optimization cannot be done because this may be a nested block
  // that will be pushed to a new page by the containing block and the
  // nested block can't know that during its reflow.
  // XXX and actually, this only always works for the block which is a
  // child of the body.
  if (!aState.mPresContext->IsPaginated()) {
    nsIAtom* tag = mContent->GetTag();
    if (nsHTMLAtoms::body == tag) {
      nsIFrame* child = aLine->mFirstChild;
      for (PRInt32 i = aLine->mChildCount; --i >= 0; ) {
        nsFrameState state;
        child->GetFrameState(state);
        if (NS_FRAME_IN_REFLOW & state) {
          child->DidReflow(*aState.mPresContext, NS_FRAME_REFLOW_FINISHED);
        }
        child->GetNextSibling(child);
      }

      // Paint this portion of ourselves
      if (!aLine->mBounds.IsEmpty()) {
        Invalidate(aLine->mBounds);
      }
    }
    NS_IF_RELEASE(tag);
  }

  // Consume space and advance running values
  aState.mY += aLine->mBounds.height;
  if (nsnull != aState.mMaxElementSizePointer) {
    nsSize* maxSize = aState.mMaxElementSizePointer;
    if (aLineLayout.mState.mMaxElementSize.width > maxSize->width) {
      maxSize->width = aLineLayout.mState.mMaxElementSize.width;
    }
    if (aLineLayout.mState.mMaxElementSize.height > maxSize->height) {
      maxSize->height = aLineLayout.mState.mMaxElementSize.height;
    }
  }
  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
  }

  // Process any pending break operations
  switch (aLineLayout.mPendingBreak) {
  default:
    break;
  case NS_STYLE_CLEAR_LEFT:
  case NS_STYLE_CLEAR_RIGHT:
  case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
    ClearFloaters(aState, aLineLayout.mPendingBreak);
    break;
  }
  // XXX for now clear the pending break; this is where support for
  // page breaks or column breaks could be partially handled.
  aLineLayout.mPendingBreak = NS_STYLE_CLEAR_NONE;

  // Any below current line floaters to place?
  // XXX We really want to know whether this is the initial reflow (reflow
  // unmapped) or a subsequent reflow in which case we only need to offset
  // the existing floaters...
  if (aState.mPendingFloaters.Count() > 0) {
    if (nsnull == aLine->mFloaters) {
      aLine->mFloaters = new nsVoidArray;
    }
    aLine->mFloaters->operator=(aState.mPendingFloaters);
    aState.mPendingFloaters.Clear();
  }

  if (nsnull != aLine->mFloaters) {
    PlaceBelowCurrentLineFloaters(aState, aLine->mFloaters, aState.mY);
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

  aState.mCurrentBand.availSpace.MoveBy(aState.mBorderPadding.left,
                                        aState.mY);

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

  // See if we have any run-in floaters to place
  if (nsnull != mRunInFloaters) {
    PlaceBelowCurrentLineFloaters(aState, mRunInFloaters, aState.mY);
  }

  // Get some space to start reflowing with
  GetAvailableSpace(aState, aState.mY);

  nsLineData* line = mLines;
  nsLineLayout lineLayout(aState);
  aState.mCurrentLine = &lineLayout;
  while (nsnull != line) {
    // Initialize the line layout for this line
    rv = lineLayout.Initialize(aState, line);/* XXX move out of loop */
    if (NS_OK != rv) {
      goto done;
    }
    lineLayout.mState.mPrevKidFrame = aState.mPrevKidFrame;

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
    aState.mPrevKidFrame = lineLayout.mState.mPrevKidFrame;
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
    lineLayout.mState.mPrevKidFrame = aState.mPrevKidFrame;

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
    aState.mPrevKidFrame = lineLayout.mState.mPrevKidFrame;
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
    lineLayout.mState.mPrevKidFrame = aState.mPrevKidFrame;

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
    kidIndex = lineLayout.mState.mKidIndex;
    kidPrevInFlow = lineLayout.mKidPrevInFlow;

    mLastContentOffset = line->mLastContentOffset;
    mLastContentIsComplete = PRBool(line->mLastContentIsComplete);
    prevLine = line;
    line = line->mNextLine;
    aState.mPrevKidFrame = lineLayout.mState.mPrevKidFrame;
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
nsBlockFrame::InitializeState(nsIPresContext*      aPresContext,
                              nsISpaceManager*     aSpaceManager,
                              const nsReflowState& aReflowState,
                              nsSize*              aMaxElementSize,
                              nsBlockReflowState&  aState)
{
  nsresult rv;
  rv = aState.Initialize(aPresContext, aSpaceManager,
                         aReflowState, aMaxElementSize, this);

  // Apply border and padding adjustments for regular frames only
  if (!aState.mBlockIsPseudo) {
    const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)
      mStyleContext->GetStyleData(eStyleStruct_Spacing);
    const nsStylePosition* myPosition = (const nsStylePosition*)
      mStyleContext->GetStyleData(eStyleStruct_Position);

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
        nsCSSLayout::GetStyleSize(aPresContext, aReflowState,
                                  aState.mStyleSize);
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

  // Setup list flags in block reflow state if this block is a list
  // item.
  const nsStyleDisplay* myDisplay = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
  if (NS_STYLE_DISPLAY_LIST_ITEM == myDisplay->mDisplay) {
    const nsStyleList* myList = (const nsStyleList*)
      mStyleContext->GetStyleData(eStyleStruct_List);
    if (NS_STYLE_LIST_STYLE_POSITION_OUTSIDE == myList->mListStylePosition) {
      aState.mListPositionOutside = PR_TRUE;
    }
  }

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
nsBlockFrame::Reflow(nsIPresContext*      aPresContext,
                     nsISpaceManager*     aSpaceManager,
                     nsReflowMetrics&     aDesiredSize,
                     const nsReflowState& aReflowState,
                     nsRect&              aDesiredRect,
                     nsReflowStatus&      aStatus)
{
#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_TRUE);
    PreReflowCheck();
  }
#endif

  nsBlockReflowState state;
  nsresult           rv = NS_OK;

  if (eReflowReason_Initial == aReflowState.reason) {
//XXX remove this code and uncomment the assertion when the table code plays nice
#ifdef NS_DEBUG
    if (0 == (NS_FRAME_FIRST_REFLOW & mState)) {
      printf("XXX: table code isn't setting the reflow reason properly! [nsBlockFrame.cpp]\n");
    }
#endif
//XXX    NS_ASSERTION(0 != (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
    rv = ProcessInitialReflow(aPresContext);
    if (NS_OK != rv) {
      return rv;
    }
  }
  else {
//XXX remove this code and uncomment the assertion when the table code plays nice
#ifdef NS_DEBUG
    if (0 != (NS_FRAME_FIRST_REFLOW & mState)) {
      printf("XXX: table code isn't setting the reflow reason properly! [nsBlockFrame.cpp]\n");
    }
#endif
//XXX    NS_ASSERTION(0 == (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }

  aStatus = NS_FRAME_COMPLETE;
  rv = InitializeState(aPresContext, aSpaceManager, aReflowState,
                       aDesiredSize.maxElementSize, state);

  NS_FRAME_TRACE_MSG(("enter nsBlockFrame::Reflow: reason=%d deltaWidth=%d",
                      aReflowState.reason, state.mDeltaWidth));

  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIPresShell* shell = state.mPresContext->GetShell();
    shell->PutCachedData(this, &state);

    // Is the reflow command target at us?
    nsIFrame* target;
    aReflowState.reflowCommand->GetTarget(target);
    if (this == target) {
      nsIReflowCommand::ReflowType  type;
      aReflowState.reflowCommand->GetType(type);

      if (nsIReflowCommand::FrameAppended == type) {
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
      } else {
        NS_NOTYETIMPLEMENTED("unexpected reflow command");
      }
   } else {
      // The command is passing through us. Get the next frame in the
      // reflow chain
      nsIFrame* nextFrame;
      aReflowState.reflowCommand->GetNext(nextFrame);
  
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
      rv = lineLayout.IncrementalReflowFromChild(aReflowState.reflowCommand,
                                                 nextFrame);
  
      // Now place the line. It's possible it won't fit
      rv = PlaceLine(state, lineLayout, line);
      // XXX The way NS_LINE_LAYOUT_COMPLETE is being used is very confusing...
      if (NS_LINE_LAYOUT_COMPLETE == rv) {
        mLastContentOffset = line->mLastContentOffset;
        mLastContentIsComplete = PRBool(line->mLastContentIsComplete);
        state.mPrevKidFrame = lineLayout.mState.mPrevKidFrame;
  
        // Now figure out what to do with the frames that follow
        rv = IncrementalReflowAfter(state, line, rv, oldBounds);
      }
    }
  
    // Return our desired rect
    ComputeDesiredRect(state, aReflowState.maxSize, aDesiredRect);
  
    // Set return status
    aStatus = NS_FRAME_COMPLETE;
    if (NS_LINE_LAYOUT_NOT_COMPLETE == rv) {
      rv = NS_OK;
      aStatus = NS_FRAME_NOT_COMPLETE;
    }
  
    // Now that reflow has finished, remove the cached pointer
    shell->RemoveCachedData(this);
    NS_RELEASE(shell);
  } else {
    nsresult rv = NS_OK;
    nsIPresShell* shell = aPresContext->GetShell();
    shell->PutCachedData(this, &state);

    // Check for an overflow list
    DrainOverflowList();

    if (nsnull != mLines) {
      rv = ReflowMapped(state);
    }

    if (NS_OK == rv) {
      if ((nsnull != mLines) && (state.mAvailSize.height <= 0)) {
        // We are out of space
      }
      if (MoreToReflow(state)) {
        rv = ReflowUnmapped(state);
      }
    }

#ifdef NS_DEBUG
    if (0 != mContent->ChildCount()) {
      NS_ASSERTION(nsnull != mLines, "reflowed zero children");
    }
#endif

    // Return our desired rect
    ComputeDesiredRect(state, aReflowState.maxSize, aDesiredRect);

    // Set return status
    aStatus = NS_FRAME_COMPLETE;
    if (NS_LINE_LAYOUT_NOT_COMPLETE == rv) {
      rv = NS_OK;
      aStatus = NS_FRAME_NOT_COMPLETE;
    }

    // Now that reflow has finished, remove the cached pointer
    shell->RemoveCachedData(this);
    NS_RELEASE(shell);
  }

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyLines(PR_TRUE);
    PostReflowCheck(aStatus);
  }
#endif
  NS_FRAME_TRACE_MSG(
    ("exit nsBlockFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredRect.width, aDesiredRect.height));
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

void nsBlockFrame::ComputeDesiredRect(nsBlockReflowState& aState,
                                      const nsSize&       aMaxSize,
                                      nsRect&             aDesiredRect)
{
  aDesiredRect.x = 0;
  aDesiredRect.y = 0;

  // Special check for zero sized content: If our content is zero
  // sized then we collapse into nothingness.
  if ((0 == aState.mKidXMost - aState.mBorderPadding.left) &&
      (0 == aState.mY - aState.mBorderPadding.top)) {
    aDesiredRect.width = 0;
    aDesiredRect.height = 0;
  }
  else {
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
}

//----------------------------------------------------------------------

PRBool
nsBlockFrame::AddFloater(nsIPresContext*     aPresContext,
                         nsIFrame*           aFloater,
                         nsPlaceholderFrame* aPlaceholder)
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

      // Determine whether we place it at the top or we place it below the
      // current line
      if (IsLeftMostChild(aPlaceholder)) {
        if (nsnull == mRunInFloaters) {
          mRunInFloaters = new nsVoidArray;
        }
        mRunInFloaters->AppendElement(aPlaceholder);
        PlaceFloater(aPresContext, aFloater, aPlaceholder, *state);
      } else {
        // Add the placeholder to our to-do list
        state->mPendingFloaters.AppendElement(aPlaceholder);
      }
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

// XXX Deprecated
void
nsBlockFrame::PlaceFloater(nsIPresContext*     aPresContext,
                           nsIFrame*           aFloater,
                           nsPlaceholderFrame* aPlaceholder)
{
#if 0
  nsIPresShell* shell = aPresContext->GetShell();
  nsBlockReflowState* state = (nsBlockReflowState*) shell->GetCachedData(this);
  NS_RELEASE(shell);
  if (nsnull != state) {
    PlaceFloater(aPresContext, aFloater, aPlaceholder, *state);
  }
#endif
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

// Used when placing run-in floaters (floaters displayed at the top of the
// block as supposed to below the current line)
void
nsBlockFrame::PlaceFloater(nsIPresContext*     aPresContext,
                           nsIFrame*           aFloater,
                           nsPlaceholderFrame* aPlaceholder,
                           nsBlockReflowState& aState)
{
  nsISpaceManager* sm = aState.mSpaceManager;

  // Get the type of floater
  const nsStyleDisplay* floaterDisplay;
  aFloater->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)floaterDisplay);

  // Commit some space in the space manager, and adjust our current
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
}

// XXX It's unclear what coordinate space aY is in. Is it relative to the
// upper-left origin of the containing block, or relative to aState.mY?
void
nsBlockFrame::PlaceBelowCurrentLineFloaters(nsBlockReflowState& aState,
                                            nsVoidArray* aFloaterList,
                                            nscoord aY)
{
  NS_PRECONDITION(aFloaterList->Count() > 0, "no floaters");

  nsISpaceManager* sm = aState.mSpaceManager;
  nsBlockBandData* bd = &aState.mCurrentBand;

  // XXX Factor this code with PlaceFloater()...
  PRInt32 numFloaters = aFloaterList->Count();
  for (PRInt32 i = 0; i < numFloaters; i++) {
    nsPlaceholderFrame* placeholderFrame = (nsPlaceholderFrame*)aFloaterList->ElementAt(i);
    nsIFrame* floater = placeholderFrame->GetAnchoredItem();
    nsRect region;

    // Get the band of available space
    // XXX This is inefficient to do this inside the loop...
    GetAvailableSpace(aState, aY);

    // Get the type of floater
    const nsStyleDisplay* sd;
    floater->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)sd);
  
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
    // XXX Temporary incremental hack
    sm->RemoveRegion(floater);
    sm->AddRectRegion(floater, region);

    // Set the origin of the floater in world coordinates
    nscoord worldX, worldY;
    sm->GetTranslation(worldX, worldY);
    floater->MoveTo(region.x + worldX, region.y + worldY);
    sm->Translate(-aState.mBorderPadding.left, 0);
  }

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
