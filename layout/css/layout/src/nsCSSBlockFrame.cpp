/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsCSSBlockFrame.h"
#include "nsCSSLineLayout.h"
#include "nsCSSInlineLayout.h"
#include "nsCSSLayout.h"
#include "nsPlaceholderFrame.h"
#include "nsStyleConsts.h"
#include "nsHTMLIIDs.h"
#include "nsCSSRendering.h"

#include "nsIAnchoredItems.h"
#include "nsIFloaterContainer.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIReflowCommand.h"
#include "nsIRunaround.h"
#include "nsIStyleContext.h"
#include "nsIView.h"

#include "nsHTMLBase.h"// XXX rename to nsCSSBase?
#include "nsHTMLParts.h"// XXX html reflow command???
#include "nsHTMLAtoms.h"// XXX list ordinal hack
#include "nsHTMLValue.h"// XXX list ordinal hack
#include "nsIHTMLContent.h"// XXX list ordinal hack

// XXX mLastContentOffset, mFirstContentOffset, mLastContentIsComplete
// XXX pagination
// XXX prev-in-flow continuations

// XXX write verify code for FD's:
//   no contiguous IFD's
//   check that child-counts agree with mChildCount
//   verify that mFirstChild == first FD's first child

// XXX IsFirstChild
// XXX max-element-size
// XXX no-wrap
// XXX margin collapsing and empty InlineFrameData's
// XXX eliminate PutCachedData usage
// XXX floaters in inlines
// XXX Check handling of mUnconstrainedWidth

// XXX MULTICOL support; note that multicol will end up using the
// equivalent of pagination! Therefore we should probably make sure
// the pagination code isn't completely stupid.

// XXX better lists (bullet numbering)

// XXX page-breaks

// XXX out of memory checks are missing

// XXX push code can record the y coordinate where the push occurred
// and the width that the data was flowed against and also indicate
// which frames were reflowed and which weren't. Then drain code can just
// place the pulled up data directly when there are no floaters to
// worry about. something like that...

//----------------------------------------------------------------------
// XXX It's really important that blocks strip out extra whitespace;
// otherwise we will see ALOT of this, which will waste memory big time:
//
//   <fd(inline - empty height because of compressed \n)>
//   <fd(block)>
//   <fd(inline - empty height because of compressed \n)>
//   <fd(block)>
//   ...
//----------------------------------------------------------------------

// XXX I don't want mFirstChild, mChildCount, mOverflowList,
// mLastContentIsComplete in our base class!!!

struct LineData;

// XXX Death to pseudo-frames!!!!!
#define DTPF 1

// XXX temporary until PropagateContentOffsets can be written genericly
#define nsCSSBlockFrameSuper nsContainerFrame

class nsCSSBlockFrame : public nsCSSBlockFrameSuper,
                        public nsIRunaround,
                        public nsIFloaterContainer
{
public:
  nsCSSBlockFrame(nsIContent* aContent, nsIFrame* aParent);

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD ChildCount(PRInt32& aChildCount) const;
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const;
  NS_IMETHOD IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const;
  NS_IMETHOD FirstChild(nsIFrame*& aFirstChild) const;
  NS_IMETHOD NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const;
  NS_IMETHOD PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const;
  NS_IMETHOD LastChild(nsIFrame*& aLastChild) const;
  NS_IMETHOD DeleteChildsNextInFlow(nsIFrame* aNextInFlow);
  NS_IMETHOD IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);
  NS_IMETHOD ContentAppended(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer);
  NS_IMETHOD DidReflow(nsIPresContext& aPresContext,
                       nsDidReflowStatus aStatus);
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD ListTag(FILE* out) const;
  NS_IMETHOD VerifyTree() const;
  // XXX implement regular reflow method too!

  // nsIRunaround
  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsISpaceManager*     aSpaceManager,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsRect&              aDesiredRect,
                    nsReflowStatus&      aStatus);

  // nsIFloaterContainer
  virtual PRBool AddFloater(nsIPresContext*      aPresContext,
                            const nsReflowState& aPlaceholderReflowState,
                            nsIFrame*            aFloater,
                            nsPlaceholderFrame*  aPlaceholder);

protected:
  ~nsCSSBlockFrame();

#if 0
  PRInt32 GetFirstContentOffset() const;

  PRInt32 GetLastContentOffset() const;

  PRBool GetLastContentIsComplete() const;
#endif

  nsresult DrainOverflowLines();

  PRBool RemoveChild(LineData* aLines, nsIFrame* aChild);

  nsresult ProcessInitialReflow(nsIPresContext* aPresContext);

  //XXX void PropagateContentOffsets();

  //XXX virtual void WillDeleteNextInFlowFrame(nsIFrame* aNextInFlow);

  PRIntn GetSkipSides() const;

  PRBool IsPseudoFrame() const;

  nsresult FrameAppendedReflow(nsCSSBlockReflowState& aState);

  nsresult ChildIncrementalReflow(nsCSSBlockReflowState& aState);

  nsresult ResizeReflow(nsCSSBlockReflowState& aState);

  void ComputeFinalSize(nsCSSBlockReflowState& aState,
                        nsReflowMetrics&       aMetrics,
                        nsRect&                aDesiredRect);

  nsresult ReflowLinesAt(nsCSSBlockReflowState& aState, LineData* aLine);

  PRBool ReflowLine(nsCSSBlockReflowState& aState,
                    LineData*              aLine,
                    nsInlineReflowStatus&  aReflowResult);

  PRBool PlaceLine(nsCSSBlockReflowState& aState,
                   LineData*              aLine,
                   nsInlineReflowStatus   aReflowStatus);

  PRBool ReflowInlineFrame(nsCSSBlockReflowState& aState,
                           LineData*              aLine,
                           nsIFrame*              aFrame,
                           nsInlineReflowStatus&  aResult);

  nsresult SplitLine(nsCSSBlockReflowState& aState,
                     LineData*              aLine,
                     nsIFrame*              aFrame,
                     PRBool                 aLineWasComplete);

  PRBool ReflowBlockFrame(nsCSSBlockReflowState& aState,
                          LineData*              aLine,
                          nsIFrame*              aFrame,
                          nsInlineReflowStatus&  aResult);

  PRBool PullFrame(nsCSSBlockReflowState& aState,
                   LineData*              aToLine,
                   LineData*              aFromLine,
                   PRBool                 aUpdateGeometricParent,
                   nsInlineReflowStatus&  aResult);

  void PushLines(nsCSSBlockReflowState& aState);

  void ReflowFloater(nsIPresContext*        aPresContext,
                     nsCSSBlockReflowState& aState,
                     nsIFrame*              aFloaterFrame);

  PRBool IsLeftMostChild(nsIFrame* aFrame);

  void PaintChildren(nsIPresContext&      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect);

#ifdef NS_DEBUG
  PRBool IsChild(nsIFrame* aFrame);
#endif

  LineData* mLines;

  LineData* mOverflowLines;

  // placeholder frames for floaters to display at the top line
  nsVoidArray* mRunInFloaters;

  friend struct nsCSSBlockReflowState;
};

//----------------------------------------------------------------------

#define LINE_IS_DIRTY                 0x1
#define LINE_IS_BLOCK                 0x2
#define LINE_LAST_CONTENT_IS_COMPLETE 0x4

struct LineData {
  LineData(nsIFrame* aFrame, PRInt32 aCount) {
    mFirstChild = aFrame;
    mChildCount = aCount;
    mState = 0;
    mFloaters = nsnull;
    mNext = nsnull;
  }

  ~LineData();

  void List(FILE* out, PRInt32 aIndent) const;

  nsIFrame* LastChild() const;

  void SetLastContentIsComplete() {
    mState |= LINE_LAST_CONTENT_IS_COMPLETE;
  }

  void ClearLastContentIsComplete() {
    mState &= ~LINE_LAST_CONTENT_IS_COMPLETE;
  }

  void SetLastContentIsComplete(PRBool aValue) {
    if (aValue) {
      SetLastContentIsComplete();
    }
    else {
      ClearLastContentIsComplete();
    }
  }

  PRBool GetLastContentIsComplete() {
    return 0 != (mState & LINE_LAST_CONTENT_IS_COMPLETE);
  }

  PRBool IsBlock() const {
    return 0 != (mState & LINE_IS_BLOCK);
  }

  void SetIsBlock() {
    mState |= LINE_IS_BLOCK;
  }

  void ClearIsBlock() {
    mState &= ~LINE_IS_BLOCK;
  }

  void SetIsBlock(PRBool aValue) {
    if (aValue) {
      SetIsBlock();
    }
    else {
      ClearIsBlock();
    }
  }

  PRUint16 GetState() const { return mState; }

#ifdef NS_DEBUG
  void Verify();
#endif

  nsIFrame* mFirstChild;
  PRUint16 mChildCount;
  PRUint16 mState;
  nsRect mBounds;
  nsVoidArray* mFloaters;
  LineData* mNext;
};

LineData::~LineData()
{
  if (nsnull != mFloaters) {
    delete mFloaters;
  }
}

void
LineData::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fprintf(out, "%p: count=%d state=%x {%d,%d,%d,%d} <\n",
          this, mChildCount, GetState(),
          mBounds.x, mBounds.y, mBounds.width, mBounds.height);

  nsIFrame* frame = mFirstChild;
  PRInt32 n = mChildCount;
  while (--n >= 0) {
    frame->List(out, aIndent + 1);
    frame->GetNextSibling(frame);
  }

  for (i = aIndent; --i >= 0; ) fputs("  ", out);

  if (nsnull != mFloaters) {
    PRInt32 j, n = mFloaters->Count();
    fprintf(out, "> #floaters=%d<\n", n);
    for (j = 0; j < n; j++) {
      nsIFrame* frame = (nsIFrame*) mFloaters->ElementAt(j);
      frame->List(out, aIndent + 1);
    }
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
  }
  fputs(">\n", out);
}

nsIFrame*
LineData::LastChild() const
{
  nsIFrame* frame = mFirstChild;
  PRInt32 n = mChildCount - 1;
  while (--n >= 0) {
    frame->GetNextSibling(frame);
  }
  return frame;
}

#ifdef NS_DEBUG
static PRInt32
LengthOf(nsIFrame* aFrame)
{
  PRInt32 result = 0;
  while (nsnull != aFrame) {
    result++;
    aFrame->GetNextSibling(aFrame);
  }
  return result;
}

void
LineData::Verify()
{
  nsIFrame* lastFrame = LastChild();
  if (nsnull != lastFrame) {
    nsIFrame* nextInFlow;
    lastFrame->GetNextInFlow(nextInFlow);
    if (GetLastContentIsComplete()) {
      NS_ASSERTION(nsnull == nextInFlow, "bad mState");
    }
    if (nsnull != mNext) {
      nsIFrame* nextSibling;
      lastFrame->GetNextSibling(nextSibling);
      NS_ASSERTION(mNext->mFirstChild == nextSibling, "bad line list");
    }
  }
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len >= mChildCount, "bad mChildCount");
}

static void
VerifyLines(LineData* aLine)
{
  while (nsnull != aLine) {
    aLine->Verify();
    aLine = aLine->mNext;
  }
}

static void
VerifyChildCount(LineData* aLines, PRBool aEmptyOK = PR_FALSE)
{
  if (nsnull != aLines) {
    PRInt32 childCount = LengthOf(aLines->mFirstChild);
    PRInt32 sum = 0;
    LineData* line = aLines;
    while (nsnull != line) {
      if (!aEmptyOK) {
        NS_ASSERTION(0 != line->mChildCount, "empty line left in line list");
      }
      sum += line->mChildCount;
      line = line->mNext;
    }
    if (sum != childCount) {
      printf("Bad sibling list/line mChildCount's\n");
      LineData* line = aLines;
      while (nsnull != line) {
        line->List(stdout, 1);
        if (nsnull != line->mNext) {
          nsIFrame* lastFrame = line->LastChild();
          if (nsnull != lastFrame) {
            nsIFrame* nextSibling;
            lastFrame->GetNextSibling(nextSibling);
            if (line->mNext->mFirstChild != nextSibling) {
              printf("  [list broken: nextSibling=%p mNext->mFirstChild=%p]\n",
                     nextSibling, line->mNext->mFirstChild);
            }
          }
        }
        line = line->mNext;
      }
      NS_ASSERTION(sum == childCount, "bad sibling list/line mChildCount's");
    }
  }
}
#endif

static void
DeleteLineList(LineData* aLine)
{
  while (nsnull != aLine) {
    LineData* next = aLine->mNext;
    delete aLine;
    aLine = next;
  }
}

static LineData*
LastLine(LineData* aLine)
{
  if (nsnull != aLine) {
    while (nsnull != aLine->mNext) {
      aLine = aLine->mNext;
    }
  }
  return aLine;
}

//----------------------------------------------------------------------

void nsCSSBlockReflowState::BlockBandData::ComputeAvailSpaceRect()
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
          PRInt32 j, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (j = 0; j < numFrames; j++) {
            nsIFrame* f = (nsIFrame*)trapezoid->frames->ElementAt(j);
            f->GetStyleData(eStyleStruct_Display,
                            (const nsStyleStruct*&)display);
            if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
              goto foundRightFloater;
            }
          }
        } else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                         (const nsStyleStruct*&)display);
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
    trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                   (const nsStyleStruct*&)display);
    if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
      availSpace.x = availSpace.XMost();
    }
    availSpace.width = 0;
  }
}

//----------------------------------------------------------------------

nsCSSBlockReflowState::nsCSSBlockReflowState(nsIPresContext* aPresContext,
                                             nsISpaceManager* aSpaceManager,
                                             nsCSSBlockFrame* aBlock,
                                             nsIStyleContext* aBlockSC,
                                             const nsReflowState& aReflowState,
                                             nsSize* aMaxElementSize)
  : nsReflowState(aBlock, *aReflowState.parentReflowState,
                  aReflowState.maxSize),
    mLineLayout(aPresContext, aSpaceManager),
    mInlineLayout(mLineLayout, aBlock, aBlockSC)
{
  mInlineLayout.Init(this);

  mPresContext = aPresContext;
  mSpaceManager = aSpaceManager;
  mBlock = aBlock;
  mBlockIsPseudo = aBlock->IsPseudoFrame();
  aBlock->GetNextInFlow((nsIFrame*&)mNextInFlow);
  mPrevPosBottomMargin = 0;
  mPrevNegBottomMargin = 0;
  mKidXMost = 0;

  mX = 0;
  mY = 0;
  mUnconstrainedWidth = aReflowState.maxSize.width == NS_UNCONSTRAINEDSIZE;
  mUnconstrainedHeight = aReflowState.maxSize.height == NS_UNCONSTRAINEDSIZE;
  mMaxElementSize = aMaxElementSize;
  if (nsnull != aMaxElementSize) {
    // XXX CAN'T pass our's to an inline child frame because it will
    // do the same thing...
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }

  // Set mNoWrap flag
  const nsStyleText* blockText = (const nsStyleText*)
    aBlockSC->GetStyleData(eStyleStruct_Text);
  switch (blockText->mWhiteSpace) {
  case NS_STYLE_WHITESPACE_PRE:
  case NS_STYLE_WHITESPACE_NOWRAP:
    mNoWrap = PR_TRUE;
    break;
  default:
    mNoWrap = PR_FALSE;
    break;
  }
  mTextAlign = blockText->mTextAlign;
  const nsStyleDisplay* styleDisplay = (const nsStyleDisplay*)
    aBlockSC->GetStyleData(eStyleStruct_Display);
  mDirection = styleDisplay->mDirection;

  // Apply border and padding adjustments for regular frames only
  nsRect blockRect;
  mBlock->GetRect(blockRect);
  if (!mBlockIsPseudo) {
    const nsStyleSpacing* blockSpacing = (const nsStyleSpacing*)
      aBlockSC->GetStyleData(eStyleStruct_Spacing);

    blockSpacing->CalcBorderPaddingFor(mBlock, mBorderPadding);
    mY = mBorderPadding.top;
    mX = mBorderPadding.left;

    if (mUnconstrainedWidth) {
      // If our width is unconstrained don't bother futzing with the
      // available width/height because they don't matter - we are
      // going to get reflowed again.
      mDeltaWidth = NS_UNCONSTRAINEDSIZE;
      mInnerSize.width = NS_UNCONSTRAINEDSIZE;
    }
    else {
      // When we are constrained we need to apply the width/height
      // style properties. When we have a width/height it applies to
      // the content width/height of our box. The content width/height
      // doesn't include the border+padding so we have to add that in
      // instead of subtracting it out of our maxsize.
      nscoord lr = mBorderPadding.left + mBorderPadding.right;

      // Get and apply the stylistic size. Note: do not limit the
      // height until we are done reflowing.
      PRIntn ss = nsCSSLayout::GetStyleSize(aPresContext, aReflowState,
                                            mStyleSize);
      mStyleSizeFlags = ss;
      if (0 != (ss & NS_SIZE_HAS_WIDTH)) {
        // CSS2 spec says that the width attribute defines the width
        // of the "content area" which does not include the border
        // padding. So we add those back in.
        mInnerSize.width = mStyleSize.width + lr;
      }
      else {
        mInnerSize.width = maxSize.width - lr;
      }
      mDeltaWidth = maxSize.width - blockRect.width;
    }
    if (mUnconstrainedHeight) {
      mInnerSize.height = maxSize.height;
      mBottomEdge = maxSize.height;
    }
    else {
      nscoord tb = mBorderPadding.top + mBorderPadding.bottom;
      mInnerSize.height = maxSize.height - tb;
      mBottomEdge = maxSize.height - mBorderPadding.bottom;
    }
  }
  else {
    mBorderPadding.SizeTo(0, 0, 0, 0);
    mDeltaWidth = maxSize.width - blockRect.width;
    mInnerSize = maxSize;
    mBottomEdge = maxSize.height;
  }

  mPrevChild = nsnull;
  mFreeList = nsnull;
  mPrevLine = nsnull;

  // Setup initial list ordinal value

  // XXX translate the starting value to a css style type and stop
  // doing this!
  mNextListOrdinal = -1;
  nsIContent* blockContent;
  mBlock->GetContent(blockContent);
  nsIAtom* tag = blockContent->GetTag();
  if ((tag == nsHTMLAtoms::ul) || (tag == nsHTMLAtoms::ol) ||
      (tag == nsHTMLAtoms::menu) || (tag == nsHTMLAtoms::dir)) {
    nsHTMLValue value;
    if (eContentAttr_HasValue ==
        ((nsIHTMLContent*)blockContent)->GetAttribute(nsHTMLAtoms::start,
                                                      value)) {
      if (eHTMLUnit_Integer == value.GetUnit()) {
        mNextListOrdinal = value.GetIntValue();
      }
    }
  }
  NS_RELEASE(blockContent);
}

nsCSSBlockReflowState::~nsCSSBlockReflowState()
{
  // XXX release free list
}

void
nsCSSBlockReflowState::GetAvailableSpace()
{
  nsresult rv = NS_OK;

  nsISpaceManager* sm = mSpaceManager;

  // Fill in band data for the specific Y coordinate
  sm->Translate(mBorderPadding.left, mY);
  sm->GetBandData(0, mInnerSize, mCurrentBand);
  sm->Translate(-mBorderPadding.left, -mY);

  // Compute the bounding rect of the available space, i.e. space
  // between any left and right floaters
  mCurrentBand.ComputeAvailSpaceRect();
  mCurrentBand.availSpace.MoveBy(mBorderPadding.left, mY);

  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
     ("nsCSSBlockReflowState::GetAvailableSpace: band={%d,%d,%d,%d}",
      mCurrentBand.availSpace.x, mCurrentBand.availSpace.y,
      mCurrentBand.availSpace.width, mCurrentBand.availSpace.height));
}

//----------------------------------------------------------------------

nsresult
NS_NewCSSBlockFrame(nsIFrame**  aInstancePtrResult,
                    nsIContent* aContent,
                    nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsCSSBlockFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsCSSBlockFrame::nsCSSBlockFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsCSSBlockFrameSuper(aContent, aParent)
{
}

nsCSSBlockFrame::~nsCSSBlockFrame()
{
  DeleteLineList(mLines);
  DeleteLineList(mOverflowLines);
  if (nsnull != mRunInFloaters) {
    delete mRunInFloaters;
  }
}

NS_IMETHODIMP
nsCSSBlockFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
#if 0
  if (aIID.Equals(kBlockFrameCID)) {
    *aInstancePtr = (void*) (this);
    return NS_OK;
  }
#endif
  if (aIID.Equals(kIRunaroundIID)) {
    *aInstancePtr = (void*) ((nsIRunaround*) this);
    return NS_OK;
  }
  if (aIID.Equals(kIFloaterContainerIID)) {
    *aInstancePtr = (void*) ((nsIFloaterContainer*) this);
    return NS_OK;
  }
  return nsCSSBlockFrameSuper::QueryInterface(aIID, aInstancePtr);
}

PRBool
nsCSSBlockFrame::IsPseudoFrame() const
{
  PRBool  result = PR_FALSE;

  if (nsnull != mGeometricParent) {
    nsIContent* parentContent;
     
    mGeometricParent->GetContent(parentContent);
    if (parentContent == mContent) {
      result = PR_TRUE;
    }
    NS_RELEASE(parentContent);
  }

  return result;
}

NS_IMETHODIMP
nsCSSBlockFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_SPLITTABLE_NON_RECTANGULAR;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::CreateContinuingFrame(nsIPresContext*  aCX,
                                       nsIFrame*        aParent,
                                       nsIStyleContext* aStyleContext,
                                       nsIFrame*&       aContinuingFrame)
{
  nsCSSBlockFrame* cf = new nsCSSBlockFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aCX, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsCSSBlockFrame::CreateContinuingFrame: prev-in-flow=%p newFrame=%p",
      this, cf));
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::ListTag(FILE* out) const
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
    nsCSSBlockFrameSuper::ListTag(out);
  }
  return NS_OK;
}

NS_METHOD
nsCSSBlockFrame::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;

  // Indent
  for (i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag
  ListTag(out);
  nsIView* view;
  GetView(view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", view);
    NS_RELEASE(view);
  }

  // Output the first/last content offset
  fprintf(out, "[%d,%d,%c] ",
          GetFirstContentOffset(), GetLastContentOffset(),
          (GetLastContentIsComplete() ? 'T' : 'F'));
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
    LineData* line = mLines;
    while (nsnull != line) {
      line->List(out, aIndent);
      line = line->mNext;
    }
    aIndent--;

    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs(">\n", out);
  } else {
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("<>\n", out);
  }

  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

NS_IMETHODIMP
nsCSSBlockFrame::ChildCount(PRInt32& aChildCount) const
{
  PRInt32 sum = 0;
  LineData* line = mLines;
  while (nsnull != line) {
    sum += line->mChildCount;
    line = line->mNext;
  }
  aChildCount = sum;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const
{
  LineData* line = mLines;
  if ((aIndex >= 0) && (nsnull != line)) {
    // First find the line that contains the aIndex
    while (nsnull != line) {
      PRInt32 n = line->mChildCount;
      if (aIndex < n) {
        // Then find the frame in the line
        nsIFrame* frame = mLines->mFirstChild;
        while (--n >= 0) {
          if (0 == aIndex) {
            aFrame = frame;
            return NS_OK;
          }
          aIndex--;
          frame->GetNextSibling(frame);
        }
        NS_NOTREACHED("line mChildCount wrong");
      }
      aIndex -= line->mChildCount;
      line = line->mNext;
    }
  }

  aFrame = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const
{
  aIndex = -1;
  if (nsnull != mLines) {
    PRInt32 index = 0;
    nsIFrame* frame = mLines->mFirstChild;
    NS_ASSERTION(nsnull != frame, "bad mLines");
    while (nsnull != frame) {
      if (frame == aChild) {
        aIndex = index;
        break;
      }
      index++;
      frame->GetNextSibling(frame);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::FirstChild(nsIFrame*& aFirstChild) const
{
  aFirstChild = (nsnull != mLines) ? mLines->mFirstChild : nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const
{
  NS_PRECONDITION(aChild != nsnull, "null pointer");
  aChild->GetNextSibling(aNextChild);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const
{
  NS_PRECONDITION(aChild != nsnull, "null pointer");
  if ((nsnull != mLines) && (mLines->mFirstChild != aChild)) {
    nsIFrame* frame = mLines->mFirstChild;
    while (nsnull != frame) {
      nsIFrame* nextFrame;
      frame->GetNextSibling(nextFrame);
      if (nextFrame == aChild) {
        aPrevChild = frame;
        return NS_OK;
      }
      frame = nextFrame;
    }
  }
  aPrevChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::LastChild(nsIFrame*& aLastChild) const
{
  LineData* line = LastLine(mLines);
  if (nsnull != line) {
    aLastChild = line->LastChild();
    return NS_OK;
  }
  aLastChild = nsnull;
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Reflow methods

#if XXX
PRBool
nsCSSBlockFrame::GetLastContentIsComplete() const
{
  PRBool result = PR_TRUE;
  LineData* line = LastLine(mLines);
  if (nsnull != line) {
    return line->GetLastContentIsComplete();
  }
  return result;
}

PRInt32
nsCSSBlockFrame::GetFirstContentOffset() const
{
  PRInt32 result = 0;
  LineData* line = mLines;
  if (nsnull != line) {
    line->mFirstChild->GetContentIndex(result);
  }
  return result;
}

PRInt32
nsCSSBlockFrame::GetLastContentOffset() const
{
  PRInt32 result = 0;
  LineData* line = LastNonEmptyLine(mLines);
  if (nsnull != line) {
    line->LastChild()->GetContentIndex(result);
  }
  return result;
}
#endif

NS_IMETHODIMP
nsCSSBlockFrame::Reflow(nsIPresContext*      aPresContext,
                        nsISpaceManager*     aSpaceManager,
                        nsReflowMetrics&     aMetrics,
                        const nsReflowState& aReflowState,
                        nsRect&              aDesiredRect,
                        nsReflowStatus&      aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsCSSBlockFrame::Reflow: maxSize=%d,%d reason=%d [%d,%d,%c]",
      aReflowState.maxSize.width,
      aReflowState.maxSize.height,
      aReflowState.reason,
      GetFirstContentOffset(), GetLastContentOffset(),
      GetLastContentIsComplete()?'T':'F'));

  // If this is the initial reflow, generate any synthetic content
  // that needs generating.
  if (eReflowReason_Initial == aReflowState.reason) {
    NS_ASSERTION(0 != (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
    nsresult rv = ProcessInitialReflow(aPresContext);
    if (NS_OK != rv) {
      return rv;
    }
  }
  else {
    NS_ASSERTION(0 == (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }

  // Replace parent provided reflow state with our own significantly
  // more extensive version.
  nsCSSBlockReflowState state(aPresContext, aSpaceManager,
                              this, mStyleContext,
                              aReflowState, aMetrics.maxElementSize);

  nsresult rv = NS_OK;
  if (eReflowReason_Initial == state.reason) {
    DrainOverflowLines();
    state.GetAvailableSpace();
    rv = FrameAppendedReflow(state);
    mState &= ~NS_FRAME_FIRST_REFLOW;
  }
  else if (eReflowReason_Incremental == state.reason) {
    NS_ASSERTION(nsnull == mOverflowLines, "bad overflow list");
    nsIFrame* target;
    state.reflowCommand->GetTarget(target);
    if (this == target) {
      nsIReflowCommand::ReflowType type;
      state.reflowCommand->GetType(type);
      switch (type) {
      case nsIReflowCommand::FrameAppended:
        state.GetAvailableSpace();
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
    DrainOverflowLines();
    state.GetAvailableSpace();
    rv = ResizeReflow(state);
  }

#ifdef DTPF
  // Update content offsets; we don't track them normally but we do
  // need them because we are a pseudo-frame
  if (nsnull != mLines) {
    nsIFrame* firstChild = mLines->mFirstChild;
    if (nsnull != firstChild) {
      firstChild->GetContentIndex(mFirstContentOffset);
    }
    LineData* line = mLines;
    while (nsnull != line->mNext) {
      NS_ASSERTION(line->mChildCount > 0, "empty line left on list");
      line = line->mNext;
    }
    line->LastChild()->GetContentIndex(mLastContentOffset);
    mLastContentIsComplete = line->GetLastContentIsComplete();
  }
  if (state.mBlockIsPseudo) {
    PropagateContentOffsets();
  }
#endif

  // Compute our final size
  ComputeFinalSize(state, aMetrics, aDesiredRect);

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyChildCount(mLines);
    VerifyLines(mLines);
  }
#endif
  aStatus = rv;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSBlockFrame::Reflow: size=%d,%d rv=%x [%d,%d,%c]",
      aMetrics.width, aMetrics.height, rv,
      GetFirstContentOffset(), GetLastContentOffset(),
      GetLastContentIsComplete()?'T':'F'));
  return NS_OK;
}

nsresult
nsCSSBlockFrame::ProcessInitialReflow(nsIPresContext* aPresContext)
{
  // XXX ick; html stuff. pfuui.

  if (nsnull == mPrevInFlow) {
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);
    NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
       ("nsCSSBlockFrame::ProcessInitialReflow: display=%d",
        display->mDisplay));
    if (NS_STYLE_DISPLAY_LIST_ITEM == display->mDisplay) {
      // This container is a list-item container. Therefore it needs a
      // bullet content object. However, it might have already had the
      // bullet crated. Check to see if the first child of the
      // container is a synthetic object; if it is, then don't make a
      // bullet (XXX what a hack!).
      nsIContent* firstContent = mContent->ChildAt(0);
      if (nsnull != firstContent) {
        PRBool is;
        firstContent->IsSynthetic(is);
        if (is) {
          return NS_OK;
        }
      }

      nsIHTMLContent* bullet;
      nsresult rv = NS_NewHTMLBullet(&bullet);
      if (NS_OK != rv) {
        return rv;
      }

      // Insert the bullet. Do not allow an incremental reflow operation
      // to occur.
      mContent->InsertChildAt(bullet, 0, PR_FALSE);
    }
  }

  return NS_OK;
}

#if XXX
// XXX It should be impossible to write this code because we use
// methods on nsContainerFrame that we have no right using.

// XXX can't work: our parent can be an nsContainerFrame -or- an
// nsCSSBlockFrame; we can't tell them apart and yet the need to be
// updated when we are a pseudo-frame

void
nsCSSBlockFrame::PropagateContentOffsets()
{
  NS_PRECONDITION(IsPseudoFrame(), "not a pseudo frame");
  nsIFrame* parent = mGeometricParent;
  if (nsnull != parent) {
    // If we're the first child frame then update our parent's content offset
    nsIFrame* firstChild;
    parent->FirstChild(firstChild);
    if (firstChild == this) {
      parent->SetFirstContentOffset(GetFirstContentOffset());
    }

    // If we're the last child frame then update our parent's content offset
    if (nsnull == mNextSibling) {
      parent->SetLastContentOffset(GetLastContentOffset());
      parent->SetLastContentIsComplete(GetLastContentIsComplete());
    }

    // If the parent is being used as a pseudo frame then we need to propagate
    // the content offsets upwards to its parent frame
    if (parent->IsPseudoFrame()) {
      parent->PropagateContentOffsets();
    }
  }
}
#endif

void
nsCSSBlockFrame::ComputeFinalSize(nsCSSBlockReflowState& aState,
                                  nsReflowMetrics&  aMetrics,
                                  nsRect&           aDesiredRect)
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
      if (aDesiredRect.width < aState.maxSize.width) {
        aDesiredRect.width = aState.maxSize.width;
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
      nscoord maxY = aState.maxSize.height;
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

  aMetrics.width = aDesiredRect.width;
  aMetrics.height = aDesiredRect.height;
  aMetrics.ascent = aMetrics.height;
  aMetrics.descent = 0;
}

// XXX move this somewhere else!!!
static PRBool
IsBlock(PRUint8 aDisplay)
{
  switch (aDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
  case NS_STYLE_DISPLAY_TABLE:
    return PR_TRUE;
  }
  return PR_FALSE;
}

// XXX this is not incremental; yuck. it's because of
// DrainOverflowLines; we don't know how to reflow the overflow lines
// and incrementally handle the new lines....time for dirty bits!

nsresult
nsCSSBlockFrame::FrameAppendedReflow(nsCSSBlockReflowState& aState)
{
  // Determine the starting kidContentIndex and the childPrevInFlow if
  // the last child is not yet complete.
  PRInt32 kidContentIndex = 0;
  nsIFrame* childPrevInFlow = nsnull;
  nsIFrame* lastFrame = nsnull;
  LineData* lastLine = nsnull;
  if ((nsnull == mLines) && (nsnull != mPrevInFlow)) {
    nsCSSBlockFrame* prev = (nsCSSBlockFrame*)mPrevInFlow;
    lastLine = LastLine(prev->mLines);
    if (nsnull != lastLine) {
      lastFrame = lastLine->LastChild();
      lastFrame->GetContentIndex(kidContentIndex);
      if (lastLine->GetLastContentIsComplete()) {
        kidContentIndex++;
      }
      else {
        childPrevInFlow = lastFrame;
      }
    }
    lastLine = nsnull;
    lastFrame = nsnull;
  }
  else {
    lastLine = LastLine(mLines);
    if (nsnull != lastLine) {
      lastFrame = lastLine->LastChild();
      lastFrame->GetContentIndex(kidContentIndex);
      if (lastLine->GetLastContentIsComplete()) {
        kidContentIndex++;
      }
      else {
        childPrevInFlow = lastFrame;
      }
    }
  }

  // Make sure that new inlines go onto the end of the lastLine when
  // the lastLine is mapping inline frames.
  PRInt32 pendingInlines = 0;
  if (nsnull != lastLine) {
    if (!lastLine->IsBlock()) {
      pendingInlines = 1;
    }
  }

  // Now create frames for all of the new content.
  nsresult rv;
  PRInt32 lastContentIndex;
  lastContentIndex = mContent->ChildCount();
  for (; kidContentIndex < lastContentIndex; kidContentIndex++) {
    nsIContent* kid;
    kid = mContent->ChildAt(kidContentIndex);
    if (nsnull == kid) {
      break;
    }

    // Create frame for our new child and add it to the sibling list
    nsIFrame* frame;
    rv = nsHTMLBase::CreateFrame(aState.mPresContext, this, kid,
                                 childPrevInFlow, frame);
    NS_RELEASE(kid);
    if (NS_OK != rv) {
      return rv;
    }
    if (nsnull != lastFrame) {
      lastFrame->SetNextSibling(frame);
    }
    lastFrame = frame;
    childPrevInFlow = nsnull;

    // See if the child is a block or non-block
    const nsStyleDisplay* kidDisplay;
    rv = frame->GetStyleData(eStyleStruct_Display,
                             (const nsStyleStruct*&) kidDisplay);
    if (NS_OK != rv) {
      return rv;
    }
    PRBool isBlock = IsBlock(kidDisplay->mDisplay);

    // If the child is an inline then add it to the lastLine (if it's
    // an inline line, otherwise make a new line). If the child is a
    // block then make a new line and put the child in that line.
    if (isBlock) {
      // If the previous line has pending inline data to be reflowed,
      // do so now.
      if (0 != pendingInlines) {
        // Set this to true in case we don't end up reflowing all of the
        // frames on the line (because they end up being pushed).
        lastLine->SetLastContentIsComplete();
        pendingInlines = 0;
      }

      // Create a line for the block
      LineData* line = new LineData(frame, 1);
      if (nsnull == line) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      if (nsnull == lastLine) {
        mLines = line;
      }
      else {
        lastLine->mNext = line;
      }
      lastLine = line;
      lastLine->SetIsBlock();
      lastLine->SetLastContentIsComplete();
    }
    else {
      // Queue up the inlines for reflow later on
      if (0 == pendingInlines) {
        LineData* line = new LineData(frame, 0);
        if (nsnull == line) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        if (nsnull == lastLine) {
          mLines = line;
        }
        else {
          lastLine->mNext = line;
        }
        lastLine = line;
      }
      lastLine->mChildCount++;
      pendingInlines++;
    }
  }

  if (0 != pendingInlines) {
    // Set this to true in case we don't end up reflowing all of the
    // frames on the line (because they end up being pushed).
    lastLine->SetLastContentIsComplete();
  }

  // XXX temporary; use dirty bits instead so we can skip over all of
  // the lines that weren't changed.
  return ResizeReflow(aState);
}

#if XXX
  // Recover our reflow state. When our last line is a block line then
  // we will include it in the recovered state because we don't need
  // to reflow it. When our last line is an inline line then we do not
  // include it in the recovered state because we will begin reflowing
  // there.
  LineData* line = mLines;
  if (nsnull != line) {
    LineData* end = lastLine;
    if (lastLine->IsBlock()) {
      end = nsnull;
    }

    // For each line up to, but not include lastLine, compute the
    // maximum xmost of each line.
    while (line != end) {
      nscoord xmost = line->mBounds.XMost();
      if (xmost > aState.mKidXMost) {
        aState.mKidXMost = xmost;
      }
      line = line->mNext;
    }

    // Start Y off just past the bottom of the line before the last line
    aState.mY = line->mBounds.YMost();

    // Recover previous bottom margin values
    if (line->IsBlock()) {
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        line->mFirstChild->GetStyleData(eStyleStruct_Spacing,
                                        (const nsStyleStruct*&) spacing);
      nsMargin margin;
      spacing->CalcMarginFor(line->mFirstChild, margin);
      if (margin.bottom < 0) {
        aState.mPrevPosBottomMargin = 0;
        aState.mPrevNegBottomMargin = -margin.bottom;
      }
      else {
        aState.mPrevPosBottomMargin = margin.bottom;
        aState.mPrevNegBottomMargin = 0;
      }
    }
    else {
      aState.mPrevPosBottomMargin = 0;
      aState.mPrevNegBottomMargin = 0;
    }
  }
#endif

nsresult
nsCSSBlockFrame::ChildIncrementalReflow(nsCSSBlockReflowState& aState)
{
  // XXX temporary
  aState.GetAvailableSpace();
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

nsresult
nsCSSBlockFrame::ResizeReflow(nsCSSBlockReflowState& aState)
{
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

nsresult
nsCSSBlockFrame::ReflowLinesAt(nsCSSBlockReflowState& aState, LineData* aLine)
{
  // Reflow the lines that are already ours
  while (nsnull != aLine) {
    nsInlineReflowStatus rs;
    if (!ReflowLine(aState, aLine, rs)) {
      if (IS_REFLOW_ERROR(rs)) {
        return nsresult(rs);
      }
      return NS_FRAME_NOT_COMPLETE;
    }
    aState.mPrevLine = aLine;
    aLine = aLine->mNext;
  }

  // Pull data from a next-in-flow if we can
  while (nsnull != aState.mNextInFlow) {
    // Grab first line from our next-in-flow
    aLine = aState.mNextInFlow->mLines;
    if (nsnull == aLine) {
      aState.mNextInFlow = (nsCSSBlockFrame*) aState.mNextInFlow->mNextInFlow;
      continue;
    }
    // XXX See if the line is not dirty; if it's not maybe we can
    // avoid the pullup if it can't fit?
    aState.mNextInFlow->mLines = aLine->mNext;
    aLine->mNext = nsnull;
    if (0 == aLine->mChildCount) {
      // The line is empty. Try the next one.
      NS_ASSERTION(nsnull == aLine->mChildCount, "bad line");
      aLine->mNext = aState.mFreeList;
      aState.mFreeList = aLine;
      continue;
    }

    // Make the children in the line ours.
    nsIFrame* frame = aLine->mFirstChild;
    nsIFrame* lastFrame = nsnull;
    PRInt32 n = aLine->mChildCount;
    while (--n >= 0) {
      nsIFrame* geometricParent;
      nsIFrame* contentParent;
      frame->GetGeometricParent(geometricParent);
      frame->GetContentParent(contentParent);
      if (contentParent == geometricParent) {
        frame->SetContentParent(this);
      }
      frame->SetGeometricParent(this);
      lastFrame = frame;
      frame->GetNextSibling(frame);
    }
    lastFrame->SetNextSibling(nsnull);

#ifdef NS_DEBUG
    LastChild(lastFrame);
    NS_ASSERTION(lastFrame == aState.mPrevChild, "bad aState.mPrevChild");
#endif

    // Add line to our line list
    if (nsnull == aState.mPrevLine) {
      NS_ASSERTION(nsnull == mLines, "bad aState.mPrevLine");
      mLines = aLine;
    }
    else {
      NS_ASSERTION(nsnull == aState.mPrevLine->mNext, "bad aState.mPrevLine");
      aState.mPrevLine->mNext = aLine;
      aState.mPrevChild->SetNextSibling(aLine->mFirstChild);
    }
#ifdef NS_DEBUG
    if (GetVerifyTreeEnable()) {
      VerifyChildCount(mLines);
    }
#endif

    // Now reflow it and any lines that it makes during it's reflow.
    while (nsnull != aLine) {
      nsInlineReflowStatus rs;
      if (!ReflowLine(aState, aLine, rs)) {
        if (IS_REFLOW_ERROR(rs)) {
          return nsresult(rs);
        }
        return NS_FRAME_NOT_COMPLETE;
      }
      aState.mPrevLine = aLine;
      aLine = aLine->mNext;
    }
  }

  return NS_FRAME_COMPLETE;
}

/**
 * Reflow a line. The line will either contain a single block frame
 * or contain 1 or more inline frames.
 */
PRBool
nsCSSBlockFrame::ReflowLine(nsCSSBlockReflowState& aState,
                            LineData*              aLine,
                            nsInlineReflowStatus&  aReflowResult)
{
  PRBool keepGoing = PR_FALSE;
  nsInlineReflowStatus rs;
  nsCSSBlockFrame* nextInFlow;
  aState.mInlineLayoutPrepared = PR_FALSE;

  // Reflow mapped frames in the line
  PRInt32 n = aLine->mChildCount;
  if (0 != n) {
    nsIFrame* frame = aLine->mFirstChild;
#ifdef NS_DEBUG
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&) display);
    PRBool isBlock = IsBlock(display->mDisplay);
    NS_ASSERTION(isBlock == aLine->IsBlock(), "bad line isBlock");
#endif 
   if (aLine->IsBlock()) {
      keepGoing = ReflowBlockFrame(aState, aLine, frame, aReflowResult);
      return keepGoing;
    }
    else {
      while (--n >= 0) {
        if (!ReflowInlineFrame(aState, aLine, frame, aReflowResult)) {
          goto done;
        }
        frame->GetNextSibling(frame);
      }
    }
  }

  // Pull frames from the next line until we can't
  while (nsnull != aLine->mNext) {
    LineData* line = aLine->mNext;
    keepGoing = PullFrame(aState, aLine, line, PR_FALSE, aReflowResult);
    if (0 == line->mChildCount) {
      aLine->mNext = line->mNext;
      line->mNext = aState.mFreeList;           // put on freelist
      aState.mFreeList = line;
    }
    if (!keepGoing) {
      goto done;
    }
  }

  // Pull frames from the next-in-flow(s) until we can't
  nextInFlow = aState.mNextInFlow;
  while (nsnull != nextInFlow) {
    LineData* line = nextInFlow->mLines;
    if (nsnull == line) {
      nextInFlow = (nsCSSBlockFrame*) nextInFlow->mNextInFlow;
      aState.mNextInFlow = nextInFlow;
      continue;
    }
    keepGoing = PullFrame(aState, aLine, line, PR_TRUE, aReflowResult);
    if (0 == line->mChildCount) {
      nextInFlow->mLines = line->mNext;
      line->mNext = aState.mFreeList;           // put on freelist
      aState.mFreeList = line;
    }
    if (!keepGoing) {
      goto done;
    }
  }
  keepGoing = PR_TRUE;
  rs = NS_INLINE_REFLOW_COMPLETE;

done:;
  if (!aLine->IsBlock()) {
    return PlaceLine(aState, aLine, aReflowResult);
  }
  return keepGoing;
}

PRBool
nsCSSBlockFrame::ReflowBlockFrame(nsCSSBlockReflowState& aState,
                                  LineData*              aLine,
                                  nsIFrame*              aFrame,
                                  nsInlineReflowStatus&  aReflowResult)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsCSSBlockFrame::ReflowBlockFrame: line=%p frame=%p y=%d",
      aLine, aFrame, aState.mY));

  // Get the block's margins
  nsMargin blockMargin;
  const nsStyleSpacing* blockSpacing;
  aFrame->GetStyleData(eStyleStruct_Spacing,
                       (const nsStyleStruct*&)blockSpacing);
  blockSpacing->CalcMarginFor(aFrame, blockMargin);

  // XXX ebina margins...
  nscoord posTopMargin, negTopMargin;
  if (blockMargin.top < 0) {
    negTopMargin = -blockMargin.top;
    posTopMargin = 0;
  }
  else {
    negTopMargin = 0;
    posTopMargin = blockMargin.top;
  }
  nscoord prevPos = aState.mPrevPosBottomMargin;
  nscoord prevNeg = aState.mPrevNegBottomMargin;
  nscoord maxPos = PR_MAX(prevPos, posTopMargin);
  nscoord maxNeg = PR_MAX(prevNeg, negTopMargin);
  nscoord topMargin = maxPos - maxNeg;

  // Remember bottom margin for next time
  if (blockMargin.bottom < 0) {
    aState.mPrevNegBottomMargin = -blockMargin.bottom;
    aState.mPrevPosBottomMargin = 0;
  }
  else {
    aState.mPrevNegBottomMargin = 0;
    aState.mPrevPosBottomMargin = blockMargin.bottom;
  }

  nscoord x = aState.mX + blockMargin.left;
  nscoord y = aState.mY + topMargin;
  aFrame->WillReflow(*aState.mPresContext);
  aFrame->MoveTo(x, y);

  // Compute the available space that the child block can reflow into
  nsSize availSize;
  if (aState.mUnconstrainedWidth) {
    // Never give a block an unconstrained width
    availSize.width = aState.maxSize.width;
  }
  else {
    availSize.width = aState.mInnerSize.width -
      (blockMargin.left + blockMargin.right);
  }
  availSize.height = aState.mBottomEdge - y;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsCSSBlockFrame::ReflowBlockFrame: availSize={%d,%d}",
      availSize.width, availSize.height));

  // Determine the reason for the reflow
  nsReflowReason reason = eReflowReason_Resize;
  nsFrameState state;
  aFrame->GetFrameState(state);
  if (NS_FRAME_FIRST_REFLOW & state) {
    reason = eReflowReason_Initial;
  }
#if XXX_fix_me
  else if (nsnull != aState.reflowCommand) {
    reason = eReflowReason_Incremental;
  }
#endif

  // Reflow the block frame. Use the run-around API if possible;
  // otherwise treat it as a rectangular lump and place it.
  nsresult rv;
  nsReflowMetrics metrics(aState.mMaxElementSize);/* XXX mMaxElementSize*/
  nsReflowStatus reflowStatus;
  nsIRunaround* runAround;
  if ((nsnull != aState.mSpaceManager) &&
      (NS_OK == aFrame->QueryInterface(kIRunaroundIID, (void**)&runAround))) {

    // Reflow the block
    nsReflowState reflowState(aFrame, aState, availSize);
    reflowState.reason = reason;
    nsRect r;
    rv = runAround->Reflow(aState.mPresContext, aState.mSpaceManager,
                           metrics, reflowState, r, reflowStatus);
    metrics.width = r.width;
    metrics.height = r.height;
    metrics.ascent = r.height;
    metrics.descent = 0;
  }
  else {
    nsReflowState reflowState(aFrame, aState, availSize);
    reflowState.reason = reason;
    rv = aFrame->Reflow(aState.mPresContext, metrics, reflowState,
                        reflowStatus);
  }
  if (IS_REFLOW_ERROR(rv)) {
    aReflowResult = nsInlineReflowStatus(rv);
    return PR_FALSE;
  }

  if (NS_FRAME_IS_COMPLETE(reflowStatus)) {
    nsIFrame* kidNextInFlow;
    aFrame->GetNextInFlow(kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsIFrame* parent;
      aFrame->GetGeometricParent(parent);
      parent->DeleteChildsNextInFlow(aFrame);
    }
    aLine->SetLastContentIsComplete();
    aReflowResult = NS_INLINE_REFLOW_COMPLETE;
  }
  else {
    aLine->ClearLastContentIsComplete();
    aReflowResult = NS_INLINE_REFLOW_NOT_COMPLETE;
  }

  // See if it fit
  nscoord newY = y + metrics.height;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsCSSBlockFrame::ReflowBlockFrame: metrics={%d,%d} newY=%d maxY=%d",
      metrics.width, metrics.height, newY, aState.mBottomEdge));
  if ((mLines != aLine) && (newY > aState.mBottomEdge)) {
    // None of the block fit. Push aLine and any other lines that follow
    PushLines(aState);
    aReflowResult = NS_INLINE_REFLOW_LINE_BREAK_BEFORE;
    return PR_FALSE;
  }

  // Save away bounds before other adjustments
  aLine->mBounds.x = x;
  aLine->mBounds.y = y;
  aLine->mBounds.width = metrics.width;
  aLine->mBounds.height = metrics.height;
  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
  }

  // Place child then align it and relatively position it
  aFrame->SetRect(aLine->mBounds);
  aState.mY = newY;
  if (!aState.mUnconstrainedWidth) {
    nsCSSLayout::HorizontallyPlaceChildren(aState.mPresContext,
                                           aState.mBlock,
                                           aState.mTextAlign,
                                           aState.mDirection,
                                           aFrame, 1,
                                           metrics.width,
                                           availSize.width);
  }
  nsCSSLayout::RelativePositionChildren(aState.mPresContext,
                                        aState.mBlock,
                                        aFrame, 1);
  aState.mPrevChild = aFrame;

  if (NS_FRAME_IS_NOT_COMPLETE(reflowStatus)) {
    // Some of the block fit. We need to have the block frame
    // continued, so we make sure that it has a next-in-flow now.
    nsIFrame* nextInFlow;
    rv = aState.mInlineLayout.MaybeCreateNextInFlow(aFrame, nextInFlow);
    if (NS_OK != rv) {
      aReflowResult = nsInlineReflowStatus(rv);
      return PR_FALSE;
    }
    if (nsnull != nextInFlow) {
      // We made a next-in-flow for the block child frame. Create a
      // line to map that block childs next-in-flow and then push that
      // line to our next-in-flow.
      LineData* line = new LineData(nextInFlow, 1);
      if (nsnull == line) {
        aReflowResult = nsInlineReflowStatus(NS_ERROR_OUT_OF_MEMORY);
        return PR_FALSE;
      }
      line->SetIsBlock();
      line->mNext = aLine->mNext;
      aLine->mNext = line;

      // Push the new line to the next-in-flow
      aState.mPrevLine = aLine;
      PushLines(aState);
    }
    aReflowResult = NS_INLINE_REFLOW_LINE_BREAK_AFTER;
    return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsCSSBlockFrame::ReflowInlineFrame(nsCSSBlockReflowState& aState,
                                   LineData*              aLine,
                                   nsIFrame*              aFrame,
                                   nsInlineReflowStatus&  aReflowResult)
{
  if (!aState.mInlineLayoutPrepared) {
    aState.mInlineLayout.Prepare(aState.mUnconstrainedWidth, aState.mNoWrap,
                                 aState.mMaxElementSize);
    aState.mInlineLayout.SetReflowSpace(aState.mCurrentBand.availSpace.x,
                                        aState.mY,
                                        aState.mCurrentBand.availSpace.width,
                                        aState.mCurrentBand.availSpace.height);
    aState.mInlineLayoutPrepared = PR_TRUE;
  }

  nsresult rv;
  nsIFrame* nextInFlow;
  aReflowResult = aState.mInlineLayout.ReflowAndPlaceFrame(aFrame);
  if (IS_REFLOW_ERROR(aReflowResult)) {
    return PR_FALSE;
  }
  PRBool lineWasComplete = aLine->GetLastContentIsComplete();
  switch (aReflowResult & NS_INLINE_REFLOW_REFLOW_MASK) {
  case NS_INLINE_REFLOW_COMPLETE:
    aState.mPrevChild = aFrame;
    aFrame->GetNextSibling(aFrame);
    aLine->SetLastContentIsComplete();
    break;

  case NS_INLINE_REFLOW_NOT_COMPLETE:
    // Create continuation frame (if necessary); add it to the end of
    // the current line so that it can be pushed to the next line
    // properly.
    aLine->ClearLastContentIsComplete();
    aState.mPrevChild = aFrame;
    rv = aState.mInlineLayout.MaybeCreateNextInFlow(aFrame, nextInFlow);
    if (NS_OK != rv) {
      aReflowResult = nsInlineReflowStatus(rv);
      return PR_FALSE;
    }
    if (nsnull != nextInFlow) {
      // Add new child to the line
      aLine->mChildCount++;
      aFrame = nextInFlow;
    }
    goto split_line;

  case NS_INLINE_REFLOW_BREAK_AFTER:
    aLine->SetLastContentIsComplete();
    aState.mPrevChild = aFrame;
    aFrame->GetNextSibling(aFrame);
    goto split_line;

  case NS_INLINE_REFLOW_BREAK_BEFORE:
    NS_ASSERTION(aLine->GetLastContentIsComplete(), "bad mState");

  split_line:
    rv = SplitLine(aState, aLine, aFrame, lineWasComplete);
    if (IS_REFLOW_ERROR(rv)) {
      aReflowResult = nsInlineReflowStatus(rv);
    }
    return PR_FALSE;
  }

  return PR_TRUE;
}

// XXX alloc lines using free-list in aState
nsresult
nsCSSBlockFrame::SplitLine(nsCSSBlockReflowState& aState,
                           LineData*              aLine,
                           nsIFrame*              aFrame,
                           PRBool                 aLineWasComplete)
{
  PRInt32 pushCount = aLine->mChildCount - aState.mInlineLayout.mFrameNum;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsCSSBlockFrame::SplitLine: pushing %d frames",
      pushCount));
  if (0 != pushCount) {
    NS_ASSERTION(nsnull != aFrame, "whoops");
    LineData* to = aLine->mNext;
    if (nsnull != to) {
      // Only push into the next line if it's empty; otherwise we can
      // end up pushing a frame which is continued into the same frame
      // as it's continuation. This causes all sorts of bad side
      // effects so we don't allow it.
      if (to->mChildCount != 0) {
        LineData* insertedLine = new LineData(aFrame, pushCount);
        aLine->mNext = insertedLine;
        insertedLine->mNext = to;
        to = insertedLine;
      } else {
        to->mFirstChild = aFrame;
        to->mChildCount += pushCount;
      }
    } else {
      to = new LineData(aFrame, pushCount);
      aLine->mNext = to;
    }
    if (nsnull == to) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (1 == pushCount) {
      const nsStyleDisplay* display;
      to->mFirstChild->GetStyleData(eStyleStruct_Display,
                                    (const nsStyleStruct*&) display);
      to->SetIsBlock(IsBlock(display->mDisplay));
    }
    to->SetLastContentIsComplete(aLineWasComplete);
    aLine->mChildCount -= pushCount;
#ifdef NS_DEBUG
    if (GetVerifyTreeEnable()) {
      aLine->Verify();
    }
#endif
    NS_ASSERTION(0 != aLine->mChildCount, "bad push");
  }
  return NS_OK;
}

PRBool
nsCSSBlockFrame::PullFrame(nsCSSBlockReflowState& aState,
                           LineData*              aLine,
                           LineData*              aFromLine,
                           PRBool                 aUpdateGeometricParent,
                           nsInlineReflowStatus&  aReflowResult)
{
  if (0 == aFromLine->mChildCount) {
    NS_ASSERTION(nsnull == aFromLine->mFirstChild, "bad line");
    return PR_TRUE;
  }

  // If our line is not empty and the child in aFromLine is a block
  // then we cannot pull up the frame into this line.
  if ((0 != aLine->mChildCount) && aFromLine->IsBlock()) {
    aReflowResult = NS_INLINE_REFLOW_LINE_BREAK_BEFORE;
    return PR_FALSE;
  }

  // Take frame from aFromLine
  nsIFrame* frame = aFromLine->mFirstChild;
  if (0 == aLine->mChildCount++) {
    aLine->mFirstChild = frame;
    aLine->SetIsBlock(aFromLine->IsBlock());
#ifdef NS_DEBUG
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&) display);
    PRBool isBlock = IsBlock(display->mDisplay);
    NS_ASSERTION(isBlock == aLine->IsBlock(), "bad line isBlock");
#endif
  }
  if (0 != --aFromLine->mChildCount) {
    frame->GetNextSibling(aFromLine->mFirstChild);
  }
  else {
    aFromLine->mFirstChild = nsnull;
  }

  // Change geometric parents
  if (aUpdateGeometricParent) {
    nsIFrame* geometricParent;
    nsIFrame* contentParent;
    frame->GetGeometricParent(geometricParent);
    frame->GetContentParent(contentParent);
    if (contentParent == geometricParent) {
      frame->SetContentParent(this);
    }
    frame->SetGeometricParent(this);

    // The frame is being pulled from a next-in-flow; therefore we
    // need to add it to our sibling list.
    if (nsnull != aState.mPrevChild) {
      aState.mPrevChild->SetNextSibling(frame);
    }
    frame->SetNextSibling(nsnull);
  }

  // Reflow the frame
  if (aLine->IsBlock()) {
    return ReflowBlockFrame(aState, aLine, frame, aReflowResult);
  }
  else {
    return ReflowInlineFrame(aState, aLine, frame, aReflowResult);
  }
}

PRBool
nsCSSBlockFrame::PlaceLine(nsCSSBlockReflowState& aState,
                           LineData*              aLine,
                           nsInlineReflowStatus   aReflowStatus)
{
  // Align the children. This also determines the actual height and
  // width of the line.
  aState.mInlineLayout.AlignFrames(aLine->mFirstChild, aLine->mChildCount,
                                   aLine->mBounds);

  // See if the line fit. If it doesn't we need to push it. Our first
  // line will always fit.

  // XXX This is a good place to check and see if we have
  // below-current-line floaters, and if we do make sure that they fit
  // too.
  nscoord newY = aState.mY + aLine->mBounds.height;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsCSSBlockFrame::PlaceLine: newY=%d limit=%d lineHeight=%d",
      newY, aState.mBottomEdge, aLine->mBounds.height));
  if ((mLines != aLine) && (newY > aState.mBottomEdge)) {
    // Push this line and all of it's children and anything else that
    // follows to our next-in-flow
    PushLines(aState);
    return PR_FALSE;
  }

  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
  }
  aState.mY = newY;

  // Based on the last child we reflowed reflow status, we may need to
  // clear past any floaters.
  PRBool updateReflowSpace = PR_FALSE;
  if (NS_INLINE_REFLOW_BREAK_AFTER ==
      (aReflowStatus & NS_INLINE_REFLOW_REFLOW_MASK)) {
    // Apply break to the line
    PRUint8 breakType = NS_INLINE_REFLOW_GET_BREAK_TYPE(aReflowStatus);
    switch (breakType) {
    default:
      break;
    case NS_STYLE_CLEAR_LEFT:
    case NS_STYLE_CLEAR_RIGHT:
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      aState.ClearFloaters(breakType);
      updateReflowSpace = PR_TRUE;
      break;
    }
    // XXX page breaks, etc, need to be passed upwards too!
  }

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
    aState.PlaceBelowCurrentLineFloaters(aLine->mFloaters);
    // XXX Factor in the height of the floaters as well when considering
    // whether the line fits.
    // The default policy is that if there isn't room for the floaters then
    // both the line and the floaters are pushed to the next-in-flow...
  }

  if (aState.mY >= aState.mCurrentBand.availSpace.YMost()) {
    // The current y coordinate is now past our available space
    // rectangle. Get a new band of space.
    aState.GetAvailableSpace();
    updateReflowSpace = PR_TRUE;
  }

  if (updateReflowSpace) {
    aState.mInlineLayout.SetReflowSpace(aState.mCurrentBand.availSpace.x,
                                        aState.mY,
                                        aState.mCurrentBand.availSpace.width,
                                        aState.mCurrentBand.availSpace.height);
  }
  return PR_TRUE;
}

void
nsCSSBlockFrame::PushLines(nsCSSBlockReflowState& aState)
{
  NS_ASSERTION(nsnull != aState.mPrevLine, "bad push");

  LineData* lastLine = aState.mPrevLine;
  LineData* nextLine = lastLine->mNext;

#if XXX
  // Strip out empty lines
  LineData** lp = &nextLine;
  for (;;) {
    LineData* line = *lp;
    if (nsnull == line) {
      break;
    }
    if (0 == line->mChildCount) {
      NS_ASSERTION(nsnull == line->mFirstChild, "bad line");
      *lp = line->mNext;
      line->mNext = aState.mFreeList;
      aState.mFreeList = line;
      continue;
    }
    lp = &line->mNext;
  }
  if (nsnull == nextLine) {
    // Nothing to push: they were all empty
    return;
  }
#endif
  lastLine->mNext = nsnull;
  mOverflowLines = nextLine;

  // Break frame sibling list
  nsIFrame* lastFrame = lastLine->LastChild();
  lastFrame->SetNextSibling(nsnull);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsCSSBlockFrame::PushLines: line=%p prevInFlow=%p nextInFlow=%p",
      mOverflowLines, mPrevInFlow, mNextInFlow));
#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyChildCount(mLines);
    VerifyChildCount(mOverflowLines, PR_TRUE);
  }
#endif
}

nsresult
nsCSSBlockFrame::DrainOverflowLines()
{
  // First grab the prev-in-flows overflow lines
  nsCSSBlockFrame* prevBlock = (nsCSSBlockFrame*) mPrevInFlow;
  if (nsnull != prevBlock) {
    LineData* line = prevBlock->mOverflowLines;
    if (nsnull != line) {
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
         ("nsCSSBlockFrame::DrainOverflowLines: line=%p prevInFlow=%p",
          line, prevBlock));
      prevBlock->mOverflowLines = nsnull;

      // Make all the frames on the mOverflowLines list mine
      nsIFrame* lastFrame = nsnull;
      nsIFrame* frame = line->mFirstChild;
      while (nsnull != frame) {
        nsIFrame* geometricParent;
        nsIFrame* contentParent;
        frame->GetGeometricParent(geometricParent);
        frame->GetContentParent(contentParent);
        if (contentParent == geometricParent) {
          frame->SetContentParent(this);
        }
        frame->SetGeometricParent(this);
        lastFrame = frame;
        frame->GetNextSibling(frame);
      }

      // Join the line lists
      if (nsnull == mLines) {
        mLines = line;
      }
      else {
        // Join the sibling lists together
        lastFrame->SetNextSibling(mLines->mFirstChild);

        // Place overflow lines at the front of our line list
        LineData* lastLine = LastLine(line);
        lastLine->mNext = mLines;
        mLines = line;
      }
    }
  }

  // Now grab our own overflow lines
  if (nsnull != mOverflowLines) {
    // This can happen when we reflow and not everything fits and then
    // we are told to reflow again before a next-in-flow is created
    // and reflows.
    NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
       ("nsCSSBlockFrame::DrainOverflowLines: from me, line=%p",
        mOverflowLines));
    LineData* lastLine = LastLine(mLines);
    if (nsnull == lastLine) {
      mLines = mOverflowLines;
    }
    else {
      lastLine->mNext = mOverflowLines;
      nsIFrame* lastFrame = lastLine->LastChild();
      lastFrame->SetNextSibling(mOverflowLines->mFirstChild);
    }
    mOverflowLines = nsnull;
  }

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyChildCount(mLines, PR_TRUE);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::ContentAppended(nsIPresShell*   aShell,
                                 nsIPresContext* aPresContext,
                                 nsIContent*     aContainer)
{
  // Get the last-in-flow
  nsCSSBlockFrame* lastInFlow = (nsCSSBlockFrame*)GetLastInFlow();

  // Generate a reflow command for the frame
  nsIReflowCommand* cmd;
  nsresult          result;
                                                  
  result = NS_NewHTMLReflowCommand(&cmd, lastInFlow,
                                   nsIReflowCommand::FrameAppended);
  if (NS_OK == result) {
    aShell->AppendReflowCommand(cmd);
    NS_RELEASE(cmd);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::DeleteChildsNextInFlow(nsIFrame* aChild)
{
  NS_PRECONDITION(IsChild(aChild), "bad geometric parent");

  nsIFrame*         nextInFlow;
  nsCSSBlockFrame*  parent;
   
  aChild->GetNextInFlow(nextInFlow);
  NS_PRECONDITION(nsnull != nextInFlow, "null next-in-flow");
  nextInFlow->GetGeometricParent((nsIFrame*&)parent);

  // If the next-in-flow has a next-in-flow then delete it, too (and
  // delete it first).
  nsIFrame* nextNextInFlow;
  nextInFlow->GetNextInFlow(nextNextInFlow);
  if (nsnull != nextNextInFlow) {
    parent->DeleteChildsNextInFlow(nextInFlow);
  }

#ifdef NS_DEBUG
  PRInt32   childCount;
  nsIFrame* firstChild;
  nextInFlow->ChildCount(childCount);
  nextInFlow->FirstChild(firstChild);
  NS_ASSERTION((0 == childCount) && (nsnull == firstChild),
               "deleting !empty next-in-flow");
#endif

  // Disconnect the next-in-flow from the flow list
  nextInFlow->BreakFromPrevFlow();

  // Remove nextInFlow from the parents line list. Also remove it from
  // the sibling list.
  if (RemoveChild(parent->mLines, nextInFlow)) {
    NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
       ("nsCSSBlockFrame::DeleteChildsNextInFlow: frame=%p (from mLines)",
        nextInFlow));
    goto done;
  }

  // If we get here then we didn't find the child on the line list. If
  // it's not there then it has to be on the overflow lines list.
  if (nsnull != mOverflowLines) {
    if (RemoveChild(parent->mOverflowLines, nextInFlow)) {
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
         ("nsCSSBlockFrame::DeleteChildsNextInFlow: frame=%p (from overflow)",
          nextInFlow));
      goto done;
    }
  }
  NS_NOTREACHED("can't find next-in-flow in overflow list");

 done:;
  // If the parent is us then we will finish reflowing and update the
  // content offsets of our parents when we are a pseudo-frame; if the
  // parent is not us then it's a next-in-flow which means it will get
  // reflowed by our parent and fix its content offsets. So there.
#if XXX
  if (parent->IsPseudoFrame()) {
    parent->PropagateContentOffsets();
  }
#endif

  // Delete the next-in-flow frame and adjust its parents child count
  nextInFlow->DeleteFrame();

#ifdef NS_DEBUG
  aChild->GetNextInFlow(nextInFlow);
  NS_POSTCONDITION(nsnull == nextInFlow, "non null next-in-flow");
#endif
  return NS_OK;
}

PRBool
nsCSSBlockFrame::RemoveChild(LineData* aLines, nsIFrame* aChild)
{
  LineData* line = aLines;
  nsIFrame* prevChild = nsnull;
  while (nsnull != line) {
    nsIFrame* child = line->mFirstChild;
    PRInt32 n = line->mChildCount;
    while (--n >= 0) {
      nsIFrame* nextChild;
      child->GetNextSibling(nextChild);
      if (child == aChild) {
        // Continuations HAVE to be at the start of a line
        NS_ASSERTION(child == line->mFirstChild, "bad continuation");
        line->mFirstChild = nextChild;
        if (0 == --line->mChildCount) {
          line->mFirstChild = nsnull;
        }
        if (nsnull != prevChild) {
          // When nextInFlow and it's continuation are in the same
          // container then we remove the nextInFlow from the sibling
          // list.
          prevChild->SetNextSibling(nextChild);
        }
        return PR_TRUE;
      }
      prevChild = child;
      child = nextChild;
    }
    line = line->mNext;
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsCSSBlockFrame::DidReflow(nsIPresContext& aPresContext,
                           nsDidReflowStatus aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsCSSBlockFrame::DidReflow: status=%d",
      aStatus));

  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    nsFrameState state;
    nsIFrame* kid = (nsnull == mLines) ? nsnull : mLines->mFirstChild;
    while (nsnull != kid) {
      kid->GetFrameState(state);
      if (0 != (state & NS_FRAME_IN_REFLOW)) {
        kid->DidReflow(aPresContext, aStatus);
      }
      kid->GetNextSibling(kid);
    }
  }

  NS_FRAME_TRACE_OUT("nsCSSBlockFrame::DidReflow");

  // Let nsFrame position and size our view (if we have one), and clear
  // the NS_FRAME_IN_REFLOW bit
  return nsFrame::DidReflow(aPresContext, aStatus);
}

////////////////////////////////////////////////////////////////////////
// Floater support

void
nsCSSBlockFrame::ReflowFloater(nsIPresContext*        aPresContext,
                               nsCSSBlockReflowState& aState,
                               nsIFrame*              aFloaterFrame)
{
//XXX fix_me: use the values already stored in the nsCSSBlockReflowState
  // Compute the available space for the floater. Use the default
  // 'auto' width and height values
  nsSize kidAvailSize(0, NS_UNCONSTRAINEDSIZE);
  nsSize styleSize;
  PRIntn styleSizeFlags =
    nsCSSLayout::GetStyleSize(aPresContext, aState, styleSize);

  // XXX The width and height are for the content area only. Add in space for
  // border and padding
  if (styleSizeFlags & NS_SIZE_HAS_WIDTH) {
    kidAvailSize.width = styleSize.width;
  }
  if (styleSizeFlags & NS_SIZE_HAS_HEIGHT) {
    kidAvailSize.height = styleSize.height;
  }

  // Resize reflow the anchored item into the available space
  // XXX Check for complete?
  nsReflowMetrics desiredSize(nsnull);
  nsReflowState   reflowState(aFloaterFrame, aState, kidAvailSize,
                              eReflowReason_Initial);
  nsReflowStatus  status;

  aFloaterFrame->WillReflow(*aPresContext);
  aFloaterFrame->Reflow(aPresContext, desiredSize, reflowState, status);
  aFloaterFrame->SizeTo(desiredSize.width, desiredSize.height);

  aFloaterFrame->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
}

PRBool
nsCSSBlockFrame::AddFloater(nsIPresContext*      aPresContext,
                            const nsReflowState& aReflowState,
                            nsIFrame*            aFloater,
                            nsPlaceholderFrame*  aPlaceholder)
{
  // Walk up reflow state chain, looking for ourself
  const nsReflowState* rs = &aReflowState;
  while (nsnull != rs) {
    if (rs->frame == this) {
      break;
    }
    rs = rs->parentReflowState;
  }
  if (nsnull == rs) {
    // Never mind
    return PR_FALSE;
  }
  nsCSSBlockReflowState* state = (nsCSSBlockReflowState*) rs;

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

    // Reflow the floater
    ReflowFloater(aPresContext, *state, aFloater);

    // Determine whether we place it at the top or we place it below the
    // current line
    if (IsLeftMostChild(aPlaceholder)) {
      if (nsnull == mRunInFloaters) {
        mRunInFloaters = new nsVoidArray;
      }
      mRunInFloaters->AppendElement(aPlaceholder);
      state->PlaceCurrentLineFloater(aFloater);
    } else {
      // Add the placeholder to our to-do list
      state->mPendingFloaters.AppendElement(aPlaceholder);
    }
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsCSSBlockFrame::IsLeftMostChild(nsIFrame* aFrame)
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

// Used when placing run-in floaters (floaters displayed at the top of
// the block as supposed to below the current line)
void
nsCSSBlockReflowState::PlaceCurrentLineFloater(nsIFrame* aFloater)
{
  nsISpaceManager* sm = mSpaceManager;

  // Get the type of floater
  const nsStyleDisplay* floaterDisplay;
  aFloater->GetStyleData(eStyleStruct_Display,
                         (const nsStyleStruct*&)floaterDisplay);
  const nsStyleSpacing* floaterSpacing;
  aFloater->GetStyleData(eStyleStruct_Spacing,
                         (const nsStyleStruct*&)floaterSpacing);

  // Commit some space in the space manager, and adjust our current
  // band of available space.
  nsRect region;
  aFloater->GetRect(region);
  region.y = mY;
  switch (floaterDisplay->mFloats) {
  default:
    NS_NOTYETIMPLEMENTED("Unsupported floater type");
    // FALL THROUGH

  case NS_STYLE_FLOAT_LEFT:
    region.x = mX;
    break;

  case NS_STYLE_FLOAT_RIGHT:
    region.x = mCurrentBand.availSpace.XMost() - region.width;
    if (region.x < 0) {
      region.x = 0;
    }
  }

  // Factor in the floaters margins
  nsMargin floaterMargin;
  floaterSpacing->CalcMarginFor(aFloater, floaterMargin);
  region.width += floaterMargin.left + floaterMargin.right;
  region.height += floaterMargin.top + floaterMargin.bottom;
  sm->AddRectRegion(aFloater, region);

  // Set the origin of the floater in world coordinates
  nscoord worldX, worldY;
  sm->GetTranslation(worldX, worldY);
  aFloater->MoveTo(region.x + worldX + floaterMargin.left,
                   region.y + worldY + floaterMargin.top);

  // Update the band of available space to reflect space taken up by
  // the floater
  GetAvailableSpace();

  // XXX if the floater is a child of an inline frame then this won't
  // work because we won't update the correct nsCSSInlineLayout.
  mInlineLayout.SetReflowSpace(mCurrentBand.availSpace.x,
                               mY,
                               mCurrentBand.availSpace.width,
                               mCurrentBand.availSpace.height);
}

void
nsCSSBlockReflowState::PlaceBelowCurrentLineFloaters(nsVoidArray* aFloaterList)
{
  NS_PRECONDITION(aFloaterList->Count() > 0, "no floaters");

  nsISpaceManager* sm = mSpaceManager;

  // XXX Factor this code with PlaceFloater()...
  nsRect region;
  PRInt32 numFloaters = aFloaterList->Count();
  for (PRInt32 i = 0; i < numFloaters; i++) {
    nsPlaceholderFrame* placeholderFrame = (nsPlaceholderFrame*)
      aFloaterList->ElementAt(i);
    nsIFrame* floater = placeholderFrame->GetAnchoredItem();

    // Get the band of available space
    // XXX This is inefficient to do this inside the loop...
    GetAvailableSpace();

    // Get the type of floater
    const nsStyleDisplay* floaterDisplay;
    const nsStyleSpacing* floaterSpacing;
    floater->GetStyleData(eStyleStruct_Display,
                          (const nsStyleStruct*&)floaterDisplay);
    floater->GetStyleData(eStyleStruct_Spacing,
                          (const nsStyleStruct*&)floaterSpacing);

    floater->GetRect(region);
    region.y = mCurrentBand.availSpace.y;
    switch (floaterDisplay->mFloats) {
    default:
      NS_NOTYETIMPLEMENTED("Unsupported floater type");
      // FALL THROUGH

    case NS_STYLE_FLOAT_LEFT:
      region.x = mCurrentBand.availSpace.x;
      break;

    case NS_STYLE_FLOAT_RIGHT:
      region.x = mCurrentBand.availSpace.XMost() - region.width;
      if (region.x < 0) {
        region.x = 0;
      }
    }

    // XXX Temporary incremental hack (kipp asks: why is this
    // temporary or an incremental hack?)

    // Factor in the floaters margins
    nsMargin floaterMargin;
    floaterSpacing->CalcMarginFor(floater, floaterMargin);
    region.width += floaterMargin.left + floaterMargin.right;
    region.height += floaterMargin.top + floaterMargin.bottom;
    sm->RemoveRegion(floater);/* XXX temporary code */
    sm->AddRectRegion(floater, region);

    // Set the origin of the floater in world coordinates
    nscoord worldX, worldY;
    sm->GetTranslation(worldX, worldY);
    floater->MoveTo(region.x + worldX + floaterMargin.left,
                    region.y + worldY + floaterMargin.top);
  }

  // Pass on updated available space to the current line
  GetAvailableSpace();
  mInlineLayout.SetReflowSpace(mCurrentBand.availSpace.x, mY,
                               mCurrentBand.availSpace.width,
                               mCurrentBand.availSpace.height);
}

void
nsCSSBlockReflowState::ClearFloaters(PRUint8 aBreakType)
{
  for (;;) {
    if (mCurrentBand.count <= 1) {
      // No floaters in this band therefore nothing to clear
      break;
    }

    // Find the Y coordinate to clear to
    nscoord clearYMost = mY;
    nsRect tmp;
    PRInt32 i;
    for (i = 0; i < mCurrentBand.count; i++) {
      const nsStyleDisplay* display;
      nsBandTrapezoid* trapezoid = &mCurrentBand.data[i];
      if (trapezoid->state != nsBandTrapezoid::Available) {
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 fn, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (fn = 0; fn < numFrames; fn++) {
            nsIFrame* f = (nsIFrame*) trapezoid->frames->ElementAt(fn);
            f->GetStyleData(eStyleStruct_Display,
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

    if (clearYMost == mY) {
      // Nothing to clear
      break;
    }

    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsCSSBlockReflowState::ClearFloaters: mY=%d clearYMost=%d",
        mY, clearYMost));

    mY = clearYMost + 1;

    // Get a new band
    GetAvailableSpace();
  }
}

//////////////////////////////////////////////////////////////////////
// Painting, event handling

PRIntn
nsCSSBlockFrame::GetSkipSides() const
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

NS_IMETHODIMP
nsCSSBlockFrame::Paint(nsIPresContext&      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect)
{
  // Paint our background and border
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible && mRect.width && mRect.height) {
    PRIntn skipSides = GetSkipSides();
    const nsStyleColor* color =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, mRect, *color);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, mRect, *spacing, skipSides);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);

  if (nsIFrame::GetShowFrameBorders()) {
    nsIView* view;
    GetView(view);
    if (nsnull != view) {
      aRenderingContext.SetColor(NS_RGB(0,0,255));
      NS_RELEASE(view);
    }
    else {
      aRenderingContext.SetColor(NS_RGB(255,0,0));
    }
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
  return NS_OK;
}

// aDirtyRect is in our coordinate system
// child rect's are also in our coordinate system

// XXX take advantage of line information to skip entire lines

void
nsCSSBlockFrame::PaintChildren(nsIPresContext&      aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               const nsRect&        aDirtyRect)
{
  // Set clip rect so that children don't leak out of us
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  PRBool hidden = PR_FALSE;
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(nsRect(0, 0, mRect.width, mRect.height),
                                  nsClipCombine_kIntersect);
    hidden = PR_TRUE;
  }

  // XXX reminder: use the coordinates in the dirty rect to figure out
  // which set of children are impacted and only do the intersection
  // work for them. In addition, stop when we no longer overlap.

  nsIFrame* kid = (nsnull == mLines) ? nsnull : mLines->mFirstChild;
  while (nsnull != kid) {
    nsIView *pView;
     
    kid->GetView(pView);
    if (nsnull == pView) {
      nsRect kidRect;
      kid->GetRect(kidRect);
      nsRect damageArea;
      PRBool overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
      if (overlap) {
        // Translate damage area into kid's coordinate system
        nsRect kidDamageArea(damageArea.x - kidRect.x,
                             damageArea.y - kidRect.y,
                             damageArea.width, damageArea.height);
        aRenderingContext.PushState();
        aRenderingContext.Translate(kidRect.x, kidRect.y);
        kid->Paint(aPresContext, aRenderingContext, kidDamageArea);
        if (nsIFrame::GetShowFrameBorders()) {
          nsIView* view;
          GetView(view);
          if (nsnull != view) {
            aRenderingContext.SetColor(NS_RGB(0,0,255));
            NS_RELEASE(view);
          }
          else {
            aRenderingContext.SetColor(NS_RGB(255,0,0));
          }
          aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
        }
        aRenderingContext.PopState();
      }
    }
    else {
      NS_RELEASE(pView);
    }
    kid->GetNextSibling(kid);
  }

  if (hidden) {
    aRenderingContext.PopState();
  }
}

//////////////////////////////////////////////////////////////////////
// Debugging

#ifdef NS_DEBUG
static PRBool
InLineList(LineData* aLines, nsIFrame* aFrame)
{
  while (nsnull != aLines) {
    nsIFrame* frame = aLines->mFirstChild;
    PRInt32 n = aLines->mChildCount;
    while (--n >= 0) {
      if (frame == aFrame) {
        return PR_TRUE;
      }
      frame->GetNextSibling(frame);
    }
    aLines = aLines->mNext;
  }
  return PR_FALSE;
}

static PRBool
InSiblingList(LineData* aLine, nsIFrame* aFrame)
{
  if (nsnull != aLine) {
    nsIFrame* frame = aLine->mFirstChild;
    while (nsnull != frame) {
      if (frame == aFrame) {
        return PR_TRUE;
      }
      frame->GetNextSibling(frame);
    }
  }
  return PR_FALSE;
}

PRBool
nsCSSBlockFrame::IsChild(nsIFrame* aFrame)
{
  nsIFrame* parent;
  aFrame->GetGeometricParent(parent);
  if (parent != (nsIFrame*)this) {
    return PR_FALSE;
  }
  if (InLineList(mLines, aFrame) && InSiblingList(mLines, aFrame)) {
    return PR_TRUE;
  }
  if (InLineList(mOverflowLines, aFrame) &&
      InSiblingList(mOverflowLines, aFrame)) {
    return PR_TRUE;
  }
  return PR_FALSE;
}
#endif

#define VERIFY_ASSERT(_expr, _msg)      \
  if (!(_expr)) {                       \
    DumpTree();                         \
  }                                     \
  NS_ASSERTION(_expr, _msg)

NS_IMETHODIMP
nsCSSBlockFrame::VerifyTree() const
{
#ifdef NS_DEBUG
  NS_ASSERTION(0 == (mState & NS_FRAME_IN_REFLOW), "frame is in reflow");

  VERIFY_ASSERT(nsnull == mOverflowLines, "bad overflow list");

  // Verify that child count is the same as the line's mChildCount's
  VerifyChildCount(mLines);

  // Verify child content offsets and index-in-parents
  PRInt32 offset = GetFirstContentOffset();
  nsIFrame* child = (nsnull == mLines) ? nsnull : mLines->mFirstChild;
  while (nsnull != child) {
    // Make sure child is ours
    nsIFrame* parent;
    child->GetGeometricParent(parent);
    VERIFY_ASSERT(parent == (nsIFrame*)this, "bad parent");

    // Make sure that the child's tree is valid
    child->VerifyTree();

    // Make sure child's index-in-parent is correct
    PRInt32 indexInParent;
    child->GetContentIndex(indexInParent);
    VERIFY_ASSERT(offset == indexInParent, "bad child offset");

    nsIFrame* nextInFlow;
    child->GetNextInFlow(nextInFlow);
    if (nsnull == nextInFlow) {
      offset++;
    }
    child->GetNextSibling(child);
  }

  // Verify that our last content offset is correct
  if (0 != mChildCount) {
    if (GetLastContentIsComplete()) {
      VERIFY_ASSERT(offset == GetLastContentOffset() + 1, "bad last offset");
    } else {
      VERIFY_ASSERT(offset == GetLastContentOffset(), "bad last offset");
    }
  }

#if XXX
  // Make sure that our flow blocks offsets are all correct
  if (nsnull == mPrevInFlow) {
    PRInt32 nextOffset = NextChildOffset();
    nsContainerFrame* nextInFlow = (nsContainerFrame*) mNextInFlow;
    while (nsnull != nextInFlow) {
      VERIFY_ASSERT(0 != nextInFlow->mChildCount, "empty next-in-flow");
      VERIFY_ASSERT(nextInFlow->GetFirstContentOffset() == nextOffset,
                    "bad next-in-flow first offset");
      nextOffset = nextInFlow->NextChildOffset();
      nextInFlow = (nsContainerFrame*) nextInFlow->mNextInFlow;
    }
  }
#endif
#endif
  return NS_OK;
}
