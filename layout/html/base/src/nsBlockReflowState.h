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
#include "nsBlockFrame.h"
#include "nsLineLayout.h"
#include "nsInlineReflow.h"
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
#include "nsIFontMetrics.h"

#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLValue.h"
#include "nsIHTMLContent.h"

//#include "js/jsapi.h"
//#include "nsDOMEvent.h"
#define DOM_EVENT_INIT      0x0001

#include "prprf.h"

// 09-15-98: make sure that the outer container of the block (e.g. the
// body sets up the outer top margin feed in properly so that the top
// margin collapses properly with the body margin. Note that for the
// block that is reflowing a body's children the body will have a
// padding value that wants to be collapsed with the blocks
// first-child's top-margin (e.g. BODY P foo needs to collapse P's margin
// with the BODY's padding

// 09-17-98: I don't like keeping mInnerBottomMargin


// 09-18-98: I think I can get rid of the distinction in this code
// between blocks and inlines. The only real issue remaining is that
// real blocks (that implement nsIRunaround) require a slightly
// different coordinate system to begin reflow at (x/y/widht/height
// values to aInlineReflow.Init) than do regular frames that do not
// implement nsIRunaround. Factoring break-before/break-after should
// be easy and handle the other 90% difference.

// 09-18-98: floating block elements don't size quite right because we
// wrap them in a body frame and the body frame doesn't honor the css
// width/height properties (among others!). The body code needs
// updating.

//XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
//XXX below this line is dubious (the entire file? :-)

// XXX MULTICOL support; note that multicol will end up using the
// equivalent of pagination! Therefore we should probably make sure
// the pagination code isn't completely stupid.

// XXX page-breaks

// XXX out of memory checks are missing

// XXX Tuneup: if mNoWrap is true and we are given a ResizeReflow we
// can just return because there's nothing to do!; this is true in
// nsInlineFrame too!
// Except that noWrap is ignored if the containers width is too small
// (like a table cell with a fixed width.)

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
//XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

struct LineData;

#define nsBlockFrameSuper nsHTMLContainerFrame

class nsBlockFrame : public nsBlockFrameSuper,
                     public nsIRunaround,
                     public nsIFloaterContainer
{
public:
  nsBlockFrame(nsIContent* aContent, nsIFrame* aParent);
  ~nsBlockFrame();

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);
  NS_IMETHOD FirstChild(nsIFrame*& aFirstChild) const;
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);

  // XXX CONSTRUCTION
#if 0
  NS_IMETHOD ContentInserted(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInParent);
#endif
  NS_IMETHOD ContentDeleted(nsIPresShell*   aShell,
                            nsIPresContext* aPresContext,
                            nsIContent*     aContainer,
                            nsIContent*     aChild,
                            PRInt32         aIndexInParent);

  NS_IMETHOD List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const;
  NS_IMETHOD ListTag(FILE* out) const;
  NS_IMETHOD VerifyTree() const;

  // nsIRunaround
  NS_IMETHOD ReflowAround(nsIPresContext&      aPresContext,
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

#ifdef DO_SELECTION
  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus& aEventStatus);

  NS_IMETHOD  HandleDrag(nsIPresContext& aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);

  nsIFrame * FindHitFrame(nsBlockFrame * aBlockFrame, 
                          const nscoord aX, const nscoord aY,
                          const nsPoint & aPoint);

#endif

  virtual PRBool DeleteChildsNextInFlow(nsIPresContext& aPresContext,
                                        nsIFrame* aNextInFlow);

  PRBool DrainOverflowLines();

  PRBool RemoveChild(LineData* aLines, nsIFrame* aChild);

  nsresult ProcessInitialReflow(nsBlockReflowState& aState);

  PRIntn GetSkipSides() const;

  PRBool IsPseudoFrame() const;

  nsresult InitialReflow(nsBlockReflowState& aState);

  nsresult FrameAppendedReflow(nsBlockReflowState& aState);

  nsresult InsertNewFrame(nsBlockFrame* aParentFrame,
                          nsIFrame*        aNewFrame,
                          nsIFrame*        aPrevSibling);
  nsresult FrameInsertedReflow(nsBlockReflowState& aState);

  nsresult FrameDeletedReflow(nsBlockReflowState& aState);

  nsresult FindTextRuns(nsBlockReflowState& aState);

  nsresult ChildIncrementalReflow(nsBlockReflowState& aState);

  nsresult ResizeReflow(nsBlockReflowState& aState);

  void ComputeFinalSize(nsBlockReflowState& aState,
                        nsReflowMetrics&       aMetrics,
                        nsRect&                aDesiredRect);

  nsresult ReflowLinesAt(nsBlockReflowState& aState, LineData* aLine);

  PRBool ReflowLine(nsBlockReflowState& aState,
                    LineData* aLine,
                    nsInlineReflowStatus& aReflowResult);

  PRBool PlaceLine(nsBlockReflowState& aState,
                   LineData* aLine,
                   nsInlineReflowStatus aReflowStatus);

  void FindFloaters(LineData* aLine);

  void PrepareInlineReflow(nsBlockReflowState& aState, nsIFrame* aFrame);

  PRBool ReflowInlineFrame(nsBlockReflowState& aState,
                           LineData* aLine,
                           nsIFrame* aFrame,
                           nsInlineReflowStatus& aResult);

  nsresult SplitLine(nsBlockReflowState& aState,
                     LineData* aLine,
                     nsIFrame* aFrame,
                     PRBool aLineWasComplete);

  PRBool ReflowBlockFrame(nsBlockReflowState& aState,
                          LineData* aLine,
                          nsIFrame* aFrame,
                          nsInlineReflowStatus& aResult);

  PRBool PullFrame(nsBlockReflowState& aState,
                   LineData* aToLine,
                   LineData** aFromList,
                   PRBool aUpdateGeometricParent,
                   nsInlineReflowStatus&  aResult);

  void PushLines(nsBlockReflowState& aState);

  void ReflowFloater(nsIPresContext& aPresContext,
                     nsBlockReflowState& aState,
                     nsIFrame* aFloaterFrame);

  void PaintChildren(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);
  void PaintChild(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect,
                  nsIFrame* aFrame,
                  PRBool aClip);

  nsresult AppendNewFrames(nsIPresContext& aPresContext, nsIFrame*);

#ifdef NS_DEBUG
  PRBool IsChild(nsIFrame* aFrame);
#endif

  LineData* mLines;

  LineData* mOverflowLines;

  // Text run information
  nsTextRun* mTextRuns;

  // For list-item frames, this is the bullet frame.
  nsIFrame* mBullet;
};

//----------------------------------------------------------------------

#define LINE_IS_DIRTY                 0x1
#define LINE_IS_BLOCK                 0x2
#define LINE_LAST_CONTENT_IS_COMPLETE 0x4
#define LINE_NEED_DID_REFLOW          0x8

struct LineData {
  LineData(nsIFrame* aFrame, PRInt32 aCount, PRUint16 flags) {
    mFirstChild = aFrame;
    mChildCount = aCount;
    mState = LINE_IS_DIRTY | LINE_NEED_DID_REFLOW | flags;
    mFloaters = nsnull;
    mNext = nsnull;
    mInnerBottomMargin = 0;
    mOuterBottomMargin = 0;
    mBounds.SetRect(0,0,0,0);
  }

  ~LineData();

  void List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter = nsnull,
            PRBool aOutputMe=PR_TRUE) const;

  nsIFrame* LastChild() const;

  PRBool IsLastChild(nsIFrame* aFrame) const;

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
    return 0 != (LINE_LAST_CONTENT_IS_COMPLETE & mState);
  }

  PRBool IsBlock() const {
    return 0 != (LINE_IS_BLOCK & mState);
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

  void MarkDirty() {
    mState |= LINE_IS_DIRTY;
  }

  void ClearDirty() {
    mState &= ~LINE_IS_DIRTY;
  }

  void SetNeedDidReflow() {
    mState |= LINE_NEED_DID_REFLOW;
  }

  void ClearNeedDidReflow() {
    mState &= ~LINE_NEED_DID_REFLOW;
  }

  PRBool NeedsDidReflow() {
    return 0 != (LINE_NEED_DID_REFLOW & mState);
  }

  PRBool IsDirty() const {
    return 0 != (LINE_IS_DIRTY & mState);
  }

  PRUint16 GetState() const { return mState; }

  char* StateToString(char* aBuf, PRInt32 aBufSize) const;

  PRBool Contains(nsIFrame* aFrame) const;

#ifdef NS_DEBUG
  void Verify();
#endif

  nsIFrame* mFirstChild;
  PRUint16 mChildCount;
  PRUint16 mState;
  nsRect mBounds;
  nscoord mInnerBottomMargin;
  nscoord mOuterBottomMargin;
  nsVoidArray* mFloaters;
  LineData* mNext;
};

LineData::~LineData()
{
  if (nsnull != mFloaters) {
    delete mFloaters;
  }
}

static void
ListFloaters(FILE* out, nsVoidArray* aFloaters)
{
  PRInt32 i, n = aFloaters->Count();
  for (i = 0; i < n; i++) {
    nsIFrame* frame = (nsIFrame*) aFloaters->ElementAt(i);
    frame->ListTag(out);
    if (i < n - 1) fputs(" ", out);
  }
}

static void
ListTextRuns(FILE* out, PRInt32 aIndent, nsTextRun* aRuns)
{
  while (nsnull != aRuns) {
    aRuns->List(out, aIndent);
    aRuns = aRuns->mNext;
  }
}

char*
LineData::StateToString(char* aBuf, PRInt32 aBufSize) const
{
  PR_snprintf(aBuf, aBufSize, "%s,%s,%scomplete",
              (mState & LINE_IS_DIRTY) ? "dirty" : "clean",
              (mState & LINE_IS_BLOCK) ? "block" : "inline",
              (mState & LINE_LAST_CONTENT_IS_COMPLETE) ? "" : "!");
  return aBuf;
}

void
LineData::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter,
               PRBool aOutputMe) const
{
  PRInt32 i;

  if (aOutputMe) {
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    char cbuf[100];
    fprintf(out, "line %p: count=%d state=%s {%d,%d,%d,%d} ibm=%d obm=%d <\n",
            this, mChildCount, StateToString(cbuf, sizeof(cbuf)),
            mBounds.x, mBounds.y, mBounds.width, mBounds.height,
            mInnerBottomMargin, mOuterBottomMargin);
  }

  nsIFrame* frame = mFirstChild;
  PRInt32 n = mChildCount;
  while (--n >= 0) {
    frame->List(out, aIndent + 1, aFilter);
    frame->GetNextSibling(frame);
  }

  if (aOutputMe) {
    for (i = aIndent; --i >= 0; ) fputs("  ", out);

    if (nsnull != mFloaters) {
      fputs("> bcl-floaters=<", out);
      ListFloaters(out, mFloaters);
    }
    fputs(">\n", out);
  }
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

PRBool
LineData::IsLastChild(nsIFrame* aFrame) const
{
  nsIFrame* lastFrame = LastChild();
  return aFrame == lastFrame;
}

PRBool
LineData::Contains(nsIFrame* aFrame) const
{
  PRInt32 n = mChildCount;
  nsIFrame* frame = mFirstChild;
  while (--n >= 0) {
    if (frame == aFrame) {
      return PR_TRUE;
    }
    frame->GetNextSibling(frame);
  }
  return PR_FALSE;
}

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

#ifdef NS_DEBUG
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
DeleteLineList(nsIPresContext& aPresContext, LineData* aLine)
{
  if (nsnull != aLine) {
    // Delete our child frames before doing anything else. In particular
    // we do all of this before our base class releases it's hold on the
    // view.
    for (nsIFrame* child = aLine->mFirstChild; child; ) {
      nsIFrame* nextChild;
      child->GetNextSibling(nextChild);
      child->DeleteFrame(aPresContext);
      child = nextChild;
    }

    while (nsnull != aLine) {
      LineData* next = aLine->mNext;
      delete aLine;
      aLine = next;
    }
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

static LineData*
FindLineContaining(LineData* aLine, nsIFrame* aFrame)
{
  while (nsnull != aLine) {
    if (aLine->Contains(aFrame)) {
      return aLine;
    }
    aLine = aLine->mNext;
  }
  return nsnull;
}

//----------------------------------------------------------------------

void
nsBlockReflowState::BlockBandData::ComputeAvailSpaceRect()
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

    // XXX Better handle the case of multiple frames
    if (nsBandTrapezoid::Occupied == trapezoid->state) {
      trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                     (const nsStyleStruct*&)display);
      if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
        availSpace.x = availSpace.XMost();
      }
    }
    availSpace.width = 0;
  }
}

//----------------------------------------------------------------------

static nscoord
GetParentLeftPadding(const nsReflowState* aReflowState)
{
  nscoord leftPadding = 0;
  while (nsnull != aReflowState) {
    nsIFrame* frame = aReflowState->frame;
    const nsStyleDisplay* styleDisplay;
    frame->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&) styleDisplay);
    if (NS_STYLE_DISPLAY_BLOCK == styleDisplay->mDisplay) {
      const nsStyleSpacing* styleSpacing;
      frame->GetStyleData(eStyleStruct_Spacing,
                          (const nsStyleStruct*&) styleSpacing);
      nsMargin padding;
      styleSpacing->CalcPaddingFor(frame, padding);
      leftPadding = padding.left;
      break;
    }
    aReflowState = aReflowState->parentReflowState;
  }
  return leftPadding;
}

nsBlockReflowState::nsBlockReflowState(nsIPresContext& aPresContext,
                                       const nsReflowState& aReflowState,
                                       const nsReflowMetrics& aMetrics,
                                       nsISpaceManager* aSpaceManager)
  : nsFrameReflowState(aPresContext, aReflowState, aMetrics),
    mLineLayout(aPresContext, aSpaceManager)
{
  mInlineReflow = nsnull;
  mLineLayout.Init(this);

  mSpaceManager = aSpaceManager;

  // Save away the outer coordinate system origin for later
  mSpaceManager->GetTranslation(mSpaceManagerX, mSpaceManagerY);

  mPresContext = aPresContext;
  mBlock = (nsBlockFrame*) frame;
  mBlock->GetNextInFlow((nsIFrame*&)mNextInFlow);
  mKidXMost = 0;

  mY = 0;
  mUnconstrainedWidth = maxSize.width == NS_UNCONSTRAINEDSIZE;
  mUnconstrainedHeight = maxSize.height == NS_UNCONSTRAINEDSIZE;
#ifdef NS_DEBUG
  if (!mUnconstrainedWidth && (maxSize.width > 100000)) {
    mBlock->ListTag(stdout);
    printf(": bad parent: maxSize WAS %d,%d\n",
           maxSize.width, maxSize.height);
    maxSize.width = NS_UNCONSTRAINEDSIZE;
    mUnconstrainedWidth = PR_TRUE;
  }
  if (!mUnconstrainedHeight && (maxSize.height > 100000)) {
    mBlock->ListTag(stdout);
    printf(": bad parent: maxSize WAS %d,%d\n",
           maxSize.width, maxSize.height);
    maxSize.height = NS_UNCONSTRAINEDSIZE;
    mUnconstrainedHeight = PR_TRUE;
  }
#endif

  mTextAlign = mStyleText->mTextAlign;

// XXX get rid of this hack (if possible!)
  mBulletPadding = 0;
#if XXX_fix_me
  if (NS_STYLE_DISPLAY_LIST_ITEM == mStyleDisplay->mDisplay) {
    const nsStyleList* sl = (const nsStyleList*)
      aBlockSC->GetStyleData(eStyleStruct_List);
    if (NS_STYLE_LIST_STYLE_POSITION_OUTSIDE == sl->mListStylePosition) {
      mLineLayout.mListPositionOutside = PR_TRUE;
      mBulletPadding = GetParentLeftPadding(aReflowState.parentReflowState);
    }
  }

  nsMargin padding;
  mStyleSpacing->CalcPaddingFor(mBlock, padding);
  mLeftPadding = padding.left;
#endif

  nscoord lr = mBorderPadding.left + mBorderPadding.right;
  mY = mBorderPadding.top;

  // Get and apply the stylistic size. Note: do not limit the
  // height until we are done reflowing.
  PRIntn ss = nsCSSLayout::GetStyleSize(&aPresContext, aReflowState,
                                        mStyleSize);
  mStyleSizeFlags = ss;
  if (0 != (ss & NS_SIZE_HAS_WIDTH)) {
    // The CSS2 spec says that the width attribute defines the
    // width of the "content area" which does not include the
    // border padding. So we add those back in.
    mBorderArea.width = mStyleSize.width + lr;
    mContentArea.width = mStyleSize.width;
  }
  else {
    if (mUnconstrainedWidth) {
      mBorderArea.width = NS_UNCONSTRAINEDSIZE;
      mContentArea.width = NS_UNCONSTRAINEDSIZE;
    }
    else {
      mBorderArea.width = maxSize.width;
      mContentArea.width = maxSize.width - lr;
    }
  }

  mBorderArea.height = maxSize.height;
  mContentArea.height = maxSize.height;
  mBottomEdge = maxSize.height;
  if (!mUnconstrainedHeight) {
    mBottomEdge -= mBorderPadding.bottom;
  }

  // Now translate in by our border and padding
  mSpaceManager->Translate(mBorderPadding.left, mBorderPadding.top);

  mPrevChild = nsnull;
  mFreeList = nsnull;
  mPrevLine = nsnull;

  // Setup initial list ordinal value

  // XXX translate the starting value to a css style type and stop
  // doing this!
  mNextListOrdinal = -1;
  nsIContent* blockContent;
  mBlock->GetContent(blockContent);
  nsIAtom* tag;
  blockContent->GetTag(tag);
  if ((tag == nsHTMLAtoms::ul) || (tag == nsHTMLAtoms::ol) ||
      (tag == nsHTMLAtoms::menu) || (tag == nsHTMLAtoms::dir)) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        ((nsIHTMLContent*)blockContent)->GetAttribute(nsHTMLAtoms::start,
                                                      value)) {
      if (eHTMLUnit_Integer == value.GetUnit()) {
        mNextListOrdinal = value.GetIntValue();
      }
    }
  }
  NS_IF_RELEASE(tag);
  NS_RELEASE(blockContent);
}

nsBlockReflowState::~nsBlockReflowState()
{
  // Restore the coordinate system
  mSpaceManager->Translate(-mBorderPadding.left, -mBorderPadding.top);

  LineData* line = mFreeList;
  while (nsnull != line) {
    NS_ASSERTION((0 == line->mChildCount) && (nsnull == line->mFirstChild),
                 "bad free line");
    LineData* next = line->mNext;
    delete line;
    line = next;
  }
}

// Get the available reflow space for the current y coordinate. The
// available space is relative to our coordinate system (0,0) is our
// upper left corner.
void
nsBlockReflowState::GetAvailableSpace()
{
  nsISpaceManager* sm = mSpaceManager;

#ifdef NS_DEBUG
  // Verify that the caller setup the coordinate system properly
  nscoord wx, wy;
  sm->GetTranslation(wx, wy);
  nscoord cx = mSpaceManagerX + mBorderPadding.left;
  nscoord cy = mSpaceManagerY + mBorderPadding.top;
  NS_ASSERTION((wx == cx) && (wy == cy), "bad coord system");
#endif

  // Fill in band data for the specific Y coordinate. Because the
  // space manager is pre-translated into our content-area (so that
  // nested blocks/inlines will line up properly), we have to remove
  // the Y translation for find the band coordinates relative to our
  // upper left corner (0,0).

  // In addition, we need to use the entire available area
  // (represented by mBorderArea) so that the band data is completely
  // relative in size to our upper left corner.
  sm->GetBandData(mY - mBorderPadding.top, mBorderArea, mCurrentBand);

  // Compute the bounding rect of the available space, i.e. space
  // between any left and right floaters.
  mCurrentBand.ComputeAvailSpaceRect();

  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
     ("nsBlockReflowState::GetAvailableSpace: band={%d,%d,%d,%d} count=%d",
      mCurrentBand.availSpace.x, mCurrentBand.availSpace.y,
      mCurrentBand.availSpace.width, mCurrentBand.availSpace.height,
      mCurrentBand.count));
}

//----------------------------------------------------------------------

nsresult
NS_NewBlockFrame(nsIContent* aContent, nsIFrame* aParentFrame,
                 nsIFrame*& aNewFrame)
{
  aNewFrame = new nsBlockFrame(aContent, aParentFrame);
  if (nsnull == aNewFrame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsBlockFrame::nsBlockFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsBlockFrameSuper(aContent, aParent)
{
}

nsBlockFrame::~nsBlockFrame()
{
  nsTextRun::DeleteTextRuns(mTextRuns);
}

NS_IMETHODIMP
nsBlockFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  // XXX temporary
  if (aIID.Equals(kBlockFrameCID)) {
    *aInstancePtr = (void*) (this);
    return NS_OK;
  }
  if (aIID.Equals(kIRunaroundIID)) {
    *aInstancePtr = (void*) ((nsIRunaround*) this);
    return NS_OK;
  }
  if (aIID.Equals(kIFloaterContainerIID)) {
    *aInstancePtr = (void*) ((nsIFloaterContainer*) this);
    return NS_OK;
  }
  return nsBlockFrameSuper::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsBlockFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  return AppendNewFrames(aPresContext, aChildList);
}

NS_IMETHODIMP
nsBlockFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  DeleteLineList(aPresContext, mLines);
  DeleteLineList(aPresContext, mOverflowLines);

  nsBlockFrameSuper::DeleteFrame(aPresContext);

  return NS_OK;
}

PRBool
nsBlockFrame::IsPseudoFrame() const
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
nsBlockFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_SPLITTABLE_NON_RECTANGULAR;
  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::CreateContinuingFrame(nsIPresContext&  aCX,
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
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsBlockFrame::CreateContinuingFrame: newFrame=%p", cf));
  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::ListTag(FILE* out) const
{
  if ((nsnull != mGeometricParent) && IsPseudoFrame()) {
    fprintf(out, "*block<");
    nsIAtom* atom;
    mContent->GetTag(atom);
    if (nsnull != atom) {
      nsAutoString tmp;
      atom->ToString(tmp);
      fputs(tmp, out);
    }
    PRInt32 contentIndex;
    GetContentIndex(contentIndex);
    fprintf(out, ">(%d)@%p", contentIndex, this);
  } else {
    nsBlockFrameSuper::ListTag(out);
  }
  return NS_OK;
}

NS_METHOD
nsBlockFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
{
  // if a filter is present, only output this frame if the filter says we should
  nsIAtom* tag;
  nsAutoString tagString;
  mContent->GetTag(tag);
  if (tag != nsnull) 
  {
    tag->ToString(tagString);
    NS_RELEASE(tag);
  }

  PRInt32 i;
  PRBool outputMe = (PRBool)((nsnull==aFilter) || ((PR_TRUE==aFilter->OutputTag(&tagString)) && (!IsPseudoFrame())));
  if (PR_TRUE==outputMe)
  {
    // Indent
    for (i = aIndent; --i >= 0; ) fputs("  ", out);

    // Output the tag
    ListTag(out);
    nsIView* view;
    GetView(view);
    if (nsnull != view) {
      fprintf(out, " [view=%p]", view);
    }

    // Output the flow linkage
    if (nsnull != mPrevInFlow) {
      fprintf(out, "prev-in-flow=%p ", mPrevInFlow);
    }
    if (nsnull != mNextInFlow) {
      fprintf(out, "next-in-flow=%p ", mNextInFlow);
    }

    // Output the rect and state
    out << mRect;
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
  }


  // Output the children, one line at a time
  if (nsnull != mLines) {
    if (PR_TRUE==outputMe)
      fputs("<\n", out);
    aIndent++;
    LineData* line = mLines;
    while (nsnull != line) {
      line->List(out, aIndent, aFilter, outputMe);
      line = line->mNext;
    }
    aIndent--;
    if (PR_TRUE==outputMe)
    {
      for (i = aIndent; --i >= 0; ) fputs("  ", out);
      fputs(">", out);
    }
  }
  else {
    if (PR_TRUE==outputMe)
      fputs("<>", out);
  }

  if (PR_TRUE==outputMe)
  {
    // Output the text-runs
    if (nsnull != mTextRuns) {
      fputs(" text-runs=<\n", out);
      ListTextRuns(out, aIndent + 1, mTextRuns);
      for (i = aIndent; --i >= 0; ) fputs("  ", out);
      fputs(">", out);
    }
    fputs("\n", out);
  }

  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

NS_IMETHODIMP
nsBlockFrame::FirstChild(nsIFrame*& aFirstChild) const
{
  aFirstChild = (nsnull != mLines) ? mLines->mFirstChild : nsnull;
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Reflow methods

NS_IMETHODIMP
nsBlockFrame::ReflowAround(nsIPresContext&      aPresContext,
                              nsISpaceManager*     aSpaceManager,
                              nsReflowMetrics&     aMetrics,
                              const nsReflowState& aReflowState,
                              nsRect&              aDesiredRect,
                              nsReflowStatus&      aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsBlockFrame::Reflow: maxSize=%d,%d reason=%d",
                  aReflowState.maxSize.width,
                  aReflowState.maxSize.height,
                  aReflowState.reason));

  // If this is the initial reflow, generate any synthetic content
  // that needs generating.
  if (eReflowReason_Initial == aReflowState.reason) {
    NS_ASSERTION(0 != (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }
  else {
    NS_ASSERTION(0 == (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }

  // Replace parent provided reflow state with our own significantly
  // more extensive version.
  nsBlockReflowState state(aPresContext, aReflowState, aMetrics,
                           aSpaceManager);

  // Prepare inline-reflow engine
  nsInlineReflow inlineReflow(state.mLineLayout, state, this);
  state.mInlineReflow = &inlineReflow;

  nsresult rv = NS_OK;
  if (eReflowReason_Initial == state.reason) {
    if (!DrainOverflowLines()) {
      rv = InitialReflow(state);
    }
    else {
      rv = ResizeReflow(state);
    }
    mState &= ~NS_FRAME_FIRST_REFLOW;
  }
  else if (eReflowReason_Incremental == state.reason) {
#if XXX
    // We can have an overflow here if our parent doesn't bother to
    // continue us
    DrainOverflowLines();
#endif
    nsIFrame* target;
    state.reflowCommand->GetTarget(target);
    if (this == target) {
      nsIReflowCommand::ReflowType type;
      state.reflowCommand->GetType(type);
      switch (type) {
      case nsIReflowCommand::FrameAppended:
        rv = FrameAppendedReflow(state);
        break;

      case nsIReflowCommand::FrameInserted:
        rv = FrameInsertedReflow(state);
        break;

      case nsIReflowCommand::FrameDeleted:
        rv = FrameDeletedReflow(state);
        break;

      default:
        NS_NOTYETIMPLEMENTED("XXX");
      }
    }
    else {
      // Get next frame in reflow command chain
      state.reflowCommand->GetNext(state.mNextRCFrame);

      // Now do the reflow
      rv = ChildIncrementalReflow(state);
    }
  }
  else if (eReflowReason_Resize == state.reason) {
    DrainOverflowLines();
    rv = ResizeReflow(state);
  }

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
                 ("exit nsBlockFrame::Reflow: size=%d,%d rv=%x",
                  aMetrics.width, aMetrics.height, rv));
  return NS_OK;
}

nsresult
nsBlockFrame::ProcessInitialReflow(nsBlockReflowState& aState)
{
  nsresult rv = NS_OK;

  // Create list bullet on the first reflow
  if ((nsnull == mPrevInFlow) &&
      (NS_STYLE_DISPLAY_LIST_ITEM == aState.mStyleDisplay->mDisplay) &&
      (nsnull == mBullet)) {
    // Create synthetic bullet content object. Note that we don't add
    // the content object to the content tree so that the DOM can't
    // find it.
    nsIHTMLContent* bullet;
    nsresult rv = NS_NewHTMLBullet(&bullet);
    if (NS_OK != rv) {
      return rv;
    }

    // Create bullet frame
    rv = NS_NewBulletFrame(bullet, this, mBullet);
    if (NS_OK != rv) {
      NS_RELEASE(bullet);
      return rv;
    }

    // Resolve style for the bullet frame
    nsIStyleContext* kidSC;
    kidSC = aState.mPresContext.ResolveStyleContextFor(bullet, this);
    mBullet->SetStyleContext(&aState.mPresContext, kidSC);
    NS_RELEASE(kidSC);
    NS_RELEASE(bullet);

    // If the list bullet frame should be positioned inside then add
    // it to the flow now.
    const nsStyleList* styleList;
    GetStyleData(eStyleStruct_List, (const nsStyleStruct*&) styleList);
    if (NS_STYLE_LIST_STYLE_POSITION_INSIDE == styleList->mListStylePosition) {
      InsertNewFrame(this, mBullet, nsnull);
    }
  }
  return NS_OK;
}

void
nsBlockFrame::ComputeFinalSize(nsBlockReflowState& aState,
                                  nsReflowMetrics&  aMetrics,
                                  nsRect&           aDesiredRect)
{
  aDesiredRect.x = 0;
  aDesiredRect.y = 0;

  // Compute final width
  PRIntn ss = aState.mStyleSizeFlags;
  if (NS_SIZE_HAS_WIDTH & ss) {
    // Use style defined width
    aDesiredRect.width = aState.mBorderPadding.left +
      aState.mStyleSize.width + aState.mBorderPadding.right;
  }
  else {
    // There are two options here. We either shrink wrap around our
    // contents or we fluff out to the maximum available width.
    nscoord contentWidth = aState.mKidXMost + aState.mBorderPadding.right;
    if (!aState.mUnconstrainedWidth) {
      // Fluff out to the max width if we aren't already that wide
      if (contentWidth < aState.maxSize.width) {
        contentWidth = aState.maxSize.width;
      }
    }
    aDesiredRect.width = contentWidth;
  }

  // Compute final height
  if (NS_SIZE_HAS_HEIGHT & ss) {
    // Use style defined height
    aDesiredRect.height = aState.mBorderPadding.top +
      aState.mStyleSize.height + aState.mBorderPadding.bottom;
  }
  else {
    // Shrink wrap our height around our contents.
    if (0 != aState.mBorderPadding.bottom) {
      aState.mY += aState.mBorderPadding.bottom;
      aMetrics.mCarriedOutBottomMargin = 0;
    }
    else {
      aMetrics.mCarriedOutBottomMargin = aState.mRunningMargin;
    }
    aDesiredRect.height = aState.mY;
  }

  // Special check for zero sized content: If our content is zero
  // sized then we collapse into nothingness.
  if ((NS_SIZE_HAS_BOTH != ss) &&
      ((0 == aState.mKidXMost - aState.mBorderPadding.left) ||
       (0 == aState.mY - aState.mBorderPadding.top))) {
    aDesiredRect.width = 0;
    aDesiredRect.height = 0;
    aMetrics.mCarriedOutBottomMargin = 0;
  }

  aMetrics.width = aDesiredRect.width;
  aMetrics.height = aDesiredRect.height;
  aMetrics.ascent = aDesiredRect.height;
  aMetrics.descent = 0;

  // XXX this needs reworking I suppose
  if (aState.mComputeMaxElementSize) {
    *aMetrics.maxElementSize = aState.mMaxElementSize;

    // Add in our border and padding to the max-element-size so that
    // we don't shrink too far.
    aMetrics.maxElementSize->width += aState.mBorderPadding.left +
      aState.mBorderPadding.right;
    aMetrics.maxElementSize->height += aState.mBorderPadding.top +
      aState.mBorderPadding.bottom;

    // Factor in any left and right floaters as well
    LineData* line = mLines;
    PRInt32 maxLeft = 0, maxRight = 0;
    while (nsnull != line) {
      if (nsnull != line->mFloaters) {
        nsRect r;
        nsMargin floaterMargin;
        PRInt32 leftSum = 0, rightSum = 0;
        PRInt32 n = line->mFloaters->Count();
        for (PRInt32 i = 0; i < n; i++) {
          nsPlaceholderFrame* placeholder = (nsPlaceholderFrame*)
            line->mFloaters->ElementAt(i);
          nsIFrame* floater = placeholder->GetAnchoredItem();
          floater->GetRect(r);
          const nsStyleDisplay* floaterDisplay;
          const nsStyleSpacing* floaterSpacing;
          floater->GetStyleData(eStyleStruct_Display,
                                (const nsStyleStruct*&)floaterDisplay);
          floater->GetStyleData(eStyleStruct_Spacing,
                                (const nsStyleStruct*&)floaterSpacing);
          floaterSpacing->CalcMarginFor(floater, floaterMargin);
          nscoord width = r.width + floaterMargin.left + floaterMargin.right;
          switch (floaterDisplay->mFloats) {
          default:
            NS_NOTYETIMPLEMENTED("Unsupported floater type");
            // FALL THROUGH

          case NS_STYLE_FLOAT_LEFT:
            leftSum += width;
            break;

          case NS_STYLE_FLOAT_RIGHT:
            rightSum += width;
            break;
          }
        }
        if (leftSum > maxLeft) maxLeft = leftSum;
        if (rightSum > maxRight) maxRight = rightSum;
      }
      line = line->mNext;
    }
    aMetrics.maxElementSize->width += maxLeft + maxRight;
  }
}

nsresult
nsBlockFrame::AppendNewFrames(nsIPresContext& aPresContext,
                              nsIFrame* aNewFrame)
{
  // Get our last line and then get its last child
  nsIFrame* lastFrame;
  LineData* lastLine = LastLine(mLines);
  if (nsnull != lastLine) {
    lastFrame = lastLine->LastChild();
  } else {
    lastFrame = nsnull;
  }

  // Add the new frames to the sibling list; wrap any frames that
  // require wrapping
  if (nsnull != lastFrame) {
    lastFrame->SetNextSibling(aNewFrame);
  }
  nsresult rv;
#if 0
  rv = WrapFrames(aPresContext, lastFrame, aNewFrame);
  if (NS_OK != rv) {
    return rv;
  }
#endif

  // Make sure that new inlines go onto the end of the lastLine when
  // the lastLine is mapping inline frames.
  PRInt32 pendingInlines = 0;
  if (nsnull != lastLine) {
    if (!lastLine->IsBlock()) {
      pendingInlines = 1;
    }
  }

  // Now create some lines for the new frames
  nsIFrame* prevFrame = lastFrame;
  for (nsIFrame* frame = aNewFrame; nsnull != frame; frame->GetNextSibling(frame)) {
    // See if the child is a block or non-block
    const nsStyleDisplay* kidDisplay;
    rv = frame->GetStyleData(eStyleStruct_Display,
                             (const nsStyleStruct*&) kidDisplay);
    if (NS_OK != rv) {
      return rv;
    }
    const nsStylePosition* kidPosition;
    rv = frame->GetStyleData(eStyleStruct_Position,
                             (const nsStyleStruct*&) kidPosition);
    if (NS_OK != rv) {
      return rv;
    }
    PRBool isBlock =
      nsLineLayout::TreatFrameAsBlock(kidDisplay, kidPosition);

// XXX yikes! this has to be here because otherwise we bust...why?
#if 1
    // See if the element wants to be floated
    if (NS_STYLE_FLOAT_NONE != kidDisplay->mFloats) {
      // Create a placeholder frame that will serve as the anchor point.
      nsPlaceholderFrame* placeholder =
        CreatePlaceholderFrame(aPresContext, frame);

      // Remove the floated element from the flow, and replace it with the
      // placeholder frame
      if (nsnull != prevFrame) {
        prevFrame->SetNextSibling(placeholder);
      }
      nsIFrame* nextSibling;
      frame->GetNextSibling(nextSibling);
      placeholder->SetNextSibling(nextSibling);
      frame->SetNextSibling(nsnull);

      // If the floated element can contain children then wrap it in a
      // BODY frame before floating it
      nsIContent* content;
      PRBool      isContainer;

      frame->GetContent(content);
      content->CanContainChildren(isContainer);
      if (isContainer) {
        // Wrap the floated element in a BODY frame.
        nsIFrame* wrapperFrame;
        NS_NewBodyFrame(content, this, wrapperFrame);
    
        // The body wrapper frame gets the original style context, and the floated
        // frame gets a pseudo style context
        nsIStyleContext*  kidStyle;
        frame->GetStyleContext(&aPresContext, kidStyle);
        wrapperFrame->SetStyleContext(&aPresContext, kidStyle);
        NS_RELEASE(kidStyle);

        nsIStyleContext*  pseudoStyle;
        pseudoStyle = aPresContext.ResolvePseudoStyleContextFor(nsHTMLAtoms::columnPseudo,
                                                                 wrapperFrame);
        frame->SetStyleContext(&aPresContext, pseudoStyle);
        NS_RELEASE(pseudoStyle);
    
        // Init the body frame
        wrapperFrame->Init(aPresContext, frame);

        // Bind the wrapper frame to the placeholder
        placeholder->SetAnchoredItem(wrapperFrame);
      }
      NS_RELEASE(content);

      // The placeholder frame is always inline
      frame = placeholder;
      isBlock = PR_FALSE;
    }
#endif

    // XXX CONSTRUCTION See if it wants to be absolutely positioned or scrolled
    // if overflows...
#if 0
    nsIFrame* kidFrame = nsnull;
    nsresult rv;
    if (NS_STYLE_POSITION_ABSOLUTE == kidPosition->mPosition) {
      rv = nsAbsoluteFrame::NewFrame(&kidFrame, aKid, aParentFrame);
      if (NS_OK == rv) {
        kidFrame->SetStyleContext(aPresContext, kidSC);
      }
    }
    else if ((NS_STYLE_OVERFLOW_SCROLL == kidDisplay->mOverflow) ||
             (NS_STYLE_OVERFLOW_AUTO == kidDisplay->mOverflow)) {
      rv = NS_NewScrollFrame(&kidFrame, aKid, aParentFrame);
      if (NS_OK == rv) {
        kidFrame->SetStyleContext(aPresContext, kidSC);
      }
    }
#endif

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
        lastLine->MarkDirty();
        pendingInlines = 0;
      }

      // Create a line for the block
      LineData* line = new LineData(frame, 1,
                                    (LINE_IS_BLOCK |
                                     LINE_LAST_CONTENT_IS_COMPLETE));
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
    else {
      // Queue up the inlines for reflow later on
      if (0 == pendingInlines) {
        LineData* line = new LineData(frame, 0, 0);
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

    // Remember the previous frame
    prevFrame = frame;

    // XXX CONSTRUCTION This needs to go somewhere...
#if 0
    if (NS_OK == rv) {
      // Wrap the frame in a view if necessary
      rv = CreateViewForFrame(aPresContext, kidFrame, kidSC, PR_FALSE);
    }
#endif
  }

  if (0 != pendingInlines) {
    // Set this to true in case we don't end up reflowing all of the
    // frames on the line (because they end up being pushed).
    lastLine->SetLastContentIsComplete();
    lastLine->MarkDirty();
  }

  return NS_OK;
}

nsresult
nsBlockFrame::InitialReflow(nsBlockReflowState& aState)
{
  // Create synthetic content
  nsresult rv = ProcessInitialReflow(aState);
  if (NS_OK != rv) {
    return rv;
  }

  // Generate text-run information
  rv = FindTextRuns(aState);
  if (NS_OK != rv) {
    return rv;
  }

  // Reflow everything
  aState.GetAvailableSpace();
  return ResizeReflow(aState);
}

nsresult
nsBlockFrame::FrameAppendedReflow(nsBlockReflowState& aState)
{
  nsresult  rv = NS_OK;

  // Get the first of the newly appended frames
  nsIFrame* firstAppendedFrame;
  aState.reflowCommand->GetChildFrame(firstAppendedFrame);

  // Add the new frames to the child list, and create new lines. Each
  // impacted line will be marked dirty
  AppendNewFrames(aState.mPresContext, firstAppendedFrame);

  // Generate text-run information
  rv = FindTextRuns(aState);
  if (NS_OK != rv) {
    return rv;
  }

  // Recover our reflow state. First find the lastCleanLine and the
  // firstDirtyLine which follows it. While we are looking, compute
  // the maximum xmost of each line.
  LineData* firstDirtyLine = mLines;
  LineData* lastCleanLine = nsnull;
  LineData* lastYLine = nsnull;
  while (nsnull != firstDirtyLine) {
    if (firstDirtyLine->IsDirty()) {
      break;
    }
    nscoord xmost = firstDirtyLine->mBounds.XMost();
    if (xmost > aState.mKidXMost) {
      aState.mKidXMost = xmost;
    }
    if (firstDirtyLine->mBounds.height > 0) {
      lastYLine = firstDirtyLine;
    }
    lastCleanLine = firstDirtyLine;
    firstDirtyLine = firstDirtyLine->mNext;
  }

  // Recover the starting Y coordinate and the previous bottom margin
  // value.
  if (nsnull != lastCleanLine) {
    // If the lastCleanLine is not a block but instead is a zero
    // height inline line then we need to backup to either a non-zero
    // height line.
    aState.mRunningMargin = 0;
    if (nsnull != lastYLine) {
      // XXX I have no idea if this is right any more!
      aState.mRunningMargin = lastYLine->mInnerBottomMargin +
        lastYLine->mOuterBottomMargin;
    }

    // Start the Y coordinate after the last clean line.
    aState.mY = lastCleanLine->mBounds.YMost();

    // Add in the outer margin to the Y coordinate (the inner margin
    // will be present in the lastCleanLine's YMost so don't add it
    // in again)
    aState.mY += lastCleanLine->mOuterBottomMargin;

    // XXX I'm not sure about the previous statement and floaters!!!

    // Place any floaters the line has
    if (nsnull != lastCleanLine->mFloaters) {
      aState.mCurrentLine = lastCleanLine;
      aState.PlaceFloaters(lastCleanLine->mFloaters);
    }
  }

  aState.GetAvailableSpace();
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsBlockReflowState::FrameAppendedReflow: y=%d firstDirtyLine=%p",
      aState.mY, firstDirtyLine));

  // Reflow lines from there forward
  aState.mPrevLine = lastCleanLine;
  return ReflowLinesAt(aState, firstDirtyLine);
}

// XXX keep the text-run data in the first-in-flow of the block
nsresult
nsBlockFrame::FindTextRuns(nsBlockReflowState& aState)
{
  // Destroy old run information first
  nsTextRun::DeleteTextRuns(mTextRuns);
  mTextRuns = nsnull;
  aState.mLineLayout.ResetTextRuns();

  // Ask each child that implements nsIInlineReflow to find its text runs
  LineData* line = mLines;
  while (nsnull != line) {
    if (!line->IsBlock()) {
      nsIFrame* frame = line->mFirstChild;
      PRInt32 n = line->mChildCount;
      while (--n >= 0) {
        nsIInlineReflow* inlineReflow;
        if (NS_OK == frame->QueryInterface(kIInlineReflowIID,
                                           (void**)&inlineReflow)) {
          nsresult rv = inlineReflow->FindTextRuns(aState.mLineLayout,
                                                   aState.reflowCommand);
          if (NS_OK != rv) {
            return rv;
          }
        }
        else {
          // A frame that doesn't implement nsIInlineReflow isn't text
          // therefore it will end an open text run.
          aState.mLineLayout.EndTextRun();
        }
        frame->GetNextSibling(frame);
      }
    }
    else {
      // A frame that doesn't implement nsIInlineReflow isn't text
      // therefore it will end an open text run.
      aState.mLineLayout.EndTextRun();
    }
    line = line->mNext;
  }
  aState.mLineLayout.EndTextRun();

  // Now take the text-runs away from the line layout engine.
  mTextRuns = aState.mLineLayout.TakeTextRuns();

  return NS_OK;
}

nsresult
nsBlockFrame::FrameInsertedReflow(nsBlockReflowState& aState)
{
  // Get the inserted frame
  nsIFrame* newFrame;
  aState.reflowCommand->GetChildFrame(newFrame);

  // Get the previous sibling frame
  nsIFrame* prevSibling;
  aState.reflowCommand->GetPrevSiblingFrame(prevSibling);

  // Insert the frame. This marks the line dirty...
  InsertNewFrame(this, newFrame, prevSibling);

  LineData* line = mLines;
  while (nsnull != line->mNext) {
    if (line->IsDirty()) {
      break;
    }
    line = line->mNext;
  }
  NS_ASSERTION(nsnull != line, "bad inserted reflow");
  //XXX return ReflowDirtyLines(aState, line);

  // XXX Correct implementation: reflow the dirty lines only; all
  // other lines can be moved; recover state before first dirty line.

  // XXX temporary
  aState.GetAvailableSpace();
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

nsresult
nsBlockFrame::FrameDeletedReflow(nsBlockReflowState& aState)
{
  if (nsnull == mLines) {
    return NS_OK;
  } 
  LineData* line = mLines;
  while (nsnull != line->mNext) {
    if (line->IsDirty()) {
      break;
    }
    line = line->mNext;
  }
  NS_ASSERTION(nsnull != line, "bad inserted reflow");
  //XXX return ReflowDirtyLines(aState, line);

  // XXX Correct implementation: reflow the dirty lines only; all
  // other lines can be moved; recover state before first dirty line.

  // XXX temporary
  aState.GetAvailableSpace();
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

// XXX Todo: some incremental reflows are passing through this block
// and into a child block; those cannot impact our text-runs. In that
// case skip the FindTextRuns work.

// XXX easy optimizations: find the line that contains the next child
// in the reflow-command path and mark it dirty and only reflow it;
// recover state before it, slide lines down after it.

nsresult
nsBlockFrame::ChildIncrementalReflow(nsBlockReflowState& aState)
{
  // Generate text-run information; this will also "fluff out" any
  // inline children's frame tree.
  nsresult rv = FindTextRuns(aState);
  if (NS_OK != rv) {
    return rv;
  }

  // XXX temporary
  aState.GetAvailableSpace();
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

nsresult
nsBlockFrame::ResizeReflow(nsBlockReflowState& aState)
{
  // Mark everything dirty
  LineData* line = mLines;
  while (nsnull != line) {
    line->MarkDirty();
    line = line->mNext;
  }

  // Reflow all of our lines
  aState.GetAvailableSpace();
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

nsresult
nsBlockFrame::ReflowLinesAt(nsBlockReflowState& aState, LineData* aLine)
{
  // Reflow the lines that are already ours
  while (nsnull != aLine) {
    nsInlineReflowStatus rs;
    if (!ReflowLine(aState, aLine, rs)) {
      if (NS_IS_REFLOW_ERROR(rs)) {
        return nsresult(rs);
      }
      return NS_FRAME_NOT_COMPLETE;
    }
    aState.mLineLayout.NextLine();
    aState.mPrevLine = aLine;
    aLine = aLine->mNext;
  }

  // Pull data from a next-in-flow if we can
  while (nsnull != aState.mNextInFlow) {
    // Grab first line from our next-in-flow
    aLine = aState.mNextInFlow->mLines;
    if (nsnull == aLine) {
      aState.mNextInFlow = (nsBlockFrame*) aState.mNextInFlow->mNextInFlow;
      continue;
    }
    // XXX See if the line is not dirty; if it's not maybe we can
    // avoid the pullup if it can't fit?
    aState.mNextInFlow->mLines = aLine->mNext;
    aLine->mNext = nsnull;
    if (0 == aLine->mChildCount) {
      // The line is empty. Try the next one.
      NS_ASSERTION(nsnull == aLine->mChildCount, "bad empty line");
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
        if (NS_IS_REFLOW_ERROR(rs)) {
          return nsresult(rs);
        }
        return NS_FRAME_NOT_COMPLETE;
      }
      aState.mLineLayout.NextLine();
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
nsBlockFrame::ReflowLine(nsBlockReflowState& aState,
                         LineData* aLine,
                         nsInlineReflowStatus& aReflowResult)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsBlockFrame::ReflowLine: line=%p", aLine));

  // Setup the line-layout for the new line
  aState.mLineLayout.Reset();
  aState.mCurrentLine = aLine;
  aState.mInlineReflowPrepared = PR_FALSE;

  // XXX temporary SLOW code
  if (nsnull != aLine->mFloaters) {
    delete aLine->mFloaters;
    aLine->mFloaters = nsnull;
  }

  aLine->ClearDirty();
  aLine->SetNeedDidReflow();

  // Reflow mapped frames in the line
  nsBlockFrame* nextInFlow;
  PRBool keepGoing = PR_FALSE;
  PRInt32 n = aLine->mChildCount;
  if (0 != n) {
    nsIFrame* frame = aLine->mFirstChild;
#ifdef NS_DEBUG
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&) display);
    const nsStylePosition* position;
    frame->GetStyleData(eStyleStruct_Position,
                        (const nsStyleStruct*&) position);
    PRBool isBlock = nsLineLayout::TreatFrameAsBlock(display, position);
    NS_ASSERTION(isBlock == aLine->IsBlock(), "bad line isBlock");
#endif 
   if (aLine->IsBlock()) {
      keepGoing = ReflowBlockFrame(aState, aLine, frame, aReflowResult);
      return keepGoing;
    }
    else {
      while (--n >= 0) {
        keepGoing = ReflowInlineFrame(aState, aLine, frame, aReflowResult);
        if (!keepGoing) {
          // It is possible that one or more of next lines are empty
          // (because of DeleteNextInFlowsFor). If so, delete them now
          // in case we are finished.
          LineData* nextLine = aLine->mNext;
          while ((nsnull != nextLine) && (0 == nextLine->mChildCount)) {
            // Discard empty lines immediately. Empty lines can happen
            // here because of DeleteNextInFlowsFor not being able to
            // delete lines.
            aLine->mNext = nextLine->mNext;
            NS_ASSERTION(nsnull == nextLine->mFirstChild, "bad empty line");
            nextLine->mNext = aState.mFreeList;
            aState.mFreeList = nextLine;
            nextLine = aLine->mNext;
          }
          goto done;
        }
        frame->GetNextSibling(frame);
      }
    }
  }

  // Pull frames from the next line until we can't
  while (nsnull != aLine->mNext) {
    keepGoing = PullFrame(aState, aLine, &aLine->mNext,
                          PR_FALSE, aReflowResult);
    if (!keepGoing) {
      goto done;
    }
  }

  // Pull frames from the next-in-flow(s) until we can't
  nextInFlow = aState.mNextInFlow;
  while (nsnull != nextInFlow) {
    LineData* line = nextInFlow->mLines;
    if (nsnull == line) {
      nextInFlow = (nsBlockFrame*) nextInFlow->mNextInFlow;
      aState.mNextInFlow = nextInFlow;
      continue;
    }
    keepGoing = PullFrame(aState, aLine, &nextInFlow->mLines,
                          PR_TRUE, aReflowResult);
    if (!keepGoing) {
      goto done;
    }
  }
  keepGoing = PR_TRUE;

done:;
  if (!aLine->IsBlock()) {
    return PlaceLine(aState, aLine, aReflowResult);
  }
  return keepGoing;
}

// block's vs. inlines's:

// 1. break-before/break-after (is handled by nsInlineReflow)
// 2. pull-up: can't pull a block onto a non-empty line
// 3. blocks require vertical margin handling
// 4. left/right margins have to be figured differently for blocks

// Assume that container is aware of block/inline differences of the
// child frame and handles them through different pathways OR that the
// nsInlineReflow does and reports back the different results

// Here is how we format B0[ I0[ I1[ text0 B1 text1] ] ]
// B0 reflows I0
//  I0 formats I1
//   I1 reflows text0
//    I1 hits B1 and notices it can't be placed
//    I1 pushes B1 to overflow list
//    I1 returns not-complete
//  I0 creates I1'
//  I0 returns not-complete
// B0 creates I0'
// B0 flushes out line
// B0 creates new line
// B0 puts I0' on new line
// B0 reflows I0'
//  I0' reflows I1'
//   I1' reflows B1
//   I1' returns not-complete
//  I0' creates I1''
//  I0' returns not-complete
// B0 creates I0''
// B0 flushes out line
// B0 creates new line
// B0 puts I0'' on new line
// B0 reflows I0''
//  I0'' reflows I1''
//   I1'' reflows text1
//   I1'' returns complete
//  I0'' returns complete
// B0 flushes out line
// B0 returns complete

// Here is how we format B0[ I0[ text0 I1[ B1 text1] ] ]

// This is harder; I1 doesn't have text0 before B1 to know that we
// have to stop reflowing I1 before reflowing B1; and worse yet, we
// only have to stop if I1 begins on the same line that contains text0
// (it might not depending on the size of text0 and the size given to
// I0 to reflow into).

// To solve this we need to know when we hit B1 whether or not a break
// is required. To answer this question we need to be able to walk
// backwards up the nsReflowState's and discover where B1 will
// actually end up. Can't use the X coordinate because of
// border/padding.

// Since B0 will define this what we do is put a counter into the
// nsLineLayout structure. As the reflow recurses down the tree we
// will bump the counter whenever a non-inline frame is seen and
// placed (one that has a non-zero area too). When I1 goes to place B1
// it will see that the count is 1 (because of text0) and know to
// return a "break-before" status (instead of a not-complete; we
// return not-complete if we have placed ANY children and break-before
// when we have placed NO children).

// XXX factor this some more so that nsInlineFrame can use it too.
// The post-reflow-success code that computes the final margin
// values and carries them forward, plus the code that factors in
// the max-element size.

// XXX inline frames need this too when they wrap up blocks, right??
// Otherwise blocks in inlines won't interact with floaters properly.
void
nsBlockFrame::PrepareInlineReflow(nsBlockReflowState& aState, nsIFrame* aFrame)
{
  // Setup initial coordinate system for reflowing the frame into
  nscoord x, availWidth, availHeight;
  nsIRunaround* runAround;
  if (NS_OK == aFrame->QueryInterface(kIRunaroundIID, (void**)&runAround)) {
    // XXX Child needs to apply OUR border-padding and IT's left
    // margin and right margin! How should this be done?
    x = aState.mBorderPadding.left;
    if (aState.mUnconstrainedWidth) {
      availWidth = NS_UNCONSTRAINEDSIZE;
    }
    else {
      availWidth = aState.mContentArea.width;
    }
    if (aState.mUnconstrainedHeight) {
      availHeight = NS_UNCONSTRAINEDSIZE;
    }
    else {
      /* XXX get the height right! */
      availHeight = aState.mContentArea.height - aState.mY;
    }
  }
  else {
    // If the child doesn't handle run-around then we give it the band
    // adjusted data. The band aligned data doesn't have our
    // border/padding applied to it so add that in.
    x = aState.mCurrentBand.availSpace.x + aState.mBorderPadding.left;
    availWidth = aState.mCurrentBand.availSpace.width;
    if (NS_UNCONSTRAINEDSIZE != availWidth) {
      availWidth -= aState.mBorderPadding.left + aState.mBorderPadding.right;
    }

    if (aState.mUnconstrainedHeight) {
      availHeight = NS_UNCONSTRAINEDSIZE;
    }
    else {
      /* XXX get the height right! */
      availHeight = aState.mCurrentBand.availSpace.height;
    }
  }
  aState.mInlineReflow->Init(x, aState.mY, availWidth, availHeight);
  aState.mInlineReflowPrepared = PR_TRUE;
//XXXaFrame->ListTag(stdout); printf(": prepare-inline-reflow: running-margin=%d\n", aState.mRunningMargin);
}

PRBool
nsBlockFrame::ReflowBlockFrame(nsBlockReflowState& aState,
                               LineData* aLine,
                               nsIFrame* aFrame,
                               nsInlineReflowStatus& aReflowResult)
{
  NS_PRECONDITION(0 == aState.mLineLayout.GetPlacedFrames(),
                  "non-empty line with a block");

  nsInlineReflow& ir = *aState.mInlineReflow;

  // Prepare the inline reflow engine
  PrepareInlineReflow(aState, aFrame);
  ir.SetIsFirstChild((aLine == mLines) &&
                     (aFrame == aLine->mFirstChild));

  // Reflow the block frame
  nsReflowStatus reflowStatus = ir.ReflowFrame(aFrame);
  if (NS_IS_REFLOW_ERROR(reflowStatus)) {
    aReflowResult = reflowStatus;
    return PR_FALSE;
  }
  aReflowResult = reflowStatus;
  if (NS_FRAME_IS_COMPLETE(reflowStatus)) {
    aLine->SetLastContentIsComplete();
  }
  else {
    aLine->ClearLastContentIsComplete();
  }

  // XXX block's can't (currently) return break before/after, right???
  // If we want to consolidate inline/block reflow then we need to
  // pretend that they can and make this class return break
  // before/after status based on the placed frame count in the
  // line...
  NS_ASSERTION(!NS_INLINE_IS_BREAK(reflowStatus), "bad block reflow status");

  // Align the frame
  ir.VerticalAlignFrames(aLine->mBounds);
  ir.HorizontalAlignFrames(aLine->mBounds);
  ir.RelativePositionFrames();

  // Try to place the frame. It might not fit after factoring in the
  // bottom margin.
  if (aLine->mBounds.height == 0) {
    // Leave aState.mPrevBottomMargin alone in this case so that it
    // can carry forward to the next non-empty block.
    aLine->mInnerBottomMargin = 0;
    aLine->mOuterBottomMargin = 0;
  }
  else {
    nscoord innerBottomMargin = ir.GetInnerBottomMargin();
    nscoord outerBottomMargin = ir.GetBottomMargin();
    nscoord newY = aLine->mBounds.YMost() + outerBottomMargin;
    // XXX running margin is advanced during reflow which means if the
    // frame doesn't fit the wrong value will be used in
    // ComputeFinalSize
    if ((mLines != aLine) && (newY > aState.mBottomEdge)) {
      // The frame doesn't fit inside our available space. Push the
      // line to the next-in-flow and return our incomplete status to
      // our parent.
      PushLines(aState);
      aReflowResult = NS_FRAME_NOT_COMPLETE;
      return PR_FALSE;
    }
    aState.mY = newY;
    aLine->mInnerBottomMargin = innerBottomMargin;
    aLine->mOuterBottomMargin = outerBottomMargin;
  }

  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
  }

  // Update max-element-size
  if (aState.mComputeMaxElementSize) {
    const nsSize& kidMaxElementSize = ir.GetMaxElementSize();
    if (kidMaxElementSize.width > aState.mMaxElementSize.width) {
      aState.mMaxElementSize.width = kidMaxElementSize.width;
    }
    if (kidMaxElementSize.height > aState.mMaxElementSize.height) {
      aState.mMaxElementSize.height = kidMaxElementSize.height;
    }
  }

  aState.mPrevChild = aFrame;

  // Refresh our available space in case a floater was placed by our
  // child.
  // XXX expensive!
  aState.GetAvailableSpace();

  if (NS_FRAME_IS_NOT_COMPLETE(reflowStatus)) {
    // Some of the block fit. We need to have the block frame
    // continued, so we make sure that it has a next-in-flow now.
    nsIFrame* nextInFlow;
    nsresult rv;
    rv = nsHTMLContainerFrame::CreateNextInFlow(aState.mPresContext,
                                                this,
                                                aFrame,
                                                nextInFlow);
    if (NS_OK != rv) {
      aReflowResult = nsInlineReflowStatus(rv);
      return PR_FALSE;
    }
    if (nsnull != nextInFlow) {
      // We made a next-in-flow for the block child frame. Create a
      // line to map the block childs next-in-flow.
      LineData* line = new LineData(nextInFlow, 1,
                                    (LINE_IS_BLOCK |
                                     LINE_LAST_CONTENT_IS_COMPLETE));
      if (nsnull == line) {
        aReflowResult = nsInlineReflowStatus(NS_ERROR_OUT_OF_MEMORY);
        return PR_FALSE;
      }
      line->mNext = aLine->mNext;
      aLine->mNext = line;
    }

    // Advance mPrevLine because we are keeping aLine (since some of
    // the child block frame fit). Then push any remaining lines to
    // our next-in-flow
    aState.mPrevLine = aLine;
    if (nsnull != aLine->mNext) {
      PushLines(aState);
    }
    aReflowResult = NS_INLINE_LINE_BREAK_AFTER(reflowStatus);
    return PR_FALSE;
  }
  return PR_TRUE;
}

/**
 * Reflow an inline frame. Returns PR_FALSE if the frame won't fit
 * on the line and the line should be flushed.
 */
PRBool
nsBlockFrame::ReflowInlineFrame(nsBlockReflowState& aState,
                                LineData* aLine,
                                nsIFrame* aFrame,
                                nsInlineReflowStatus& aReflowResult)
{
  nsresult rv;
  nsIFrame* nextInFlow;

  if (!aState.mInlineReflowPrepared) {
    PrepareInlineReflow(aState, aFrame);
  }

  PRBool isFirstChild = (aLine == mLines) &&
    (aFrame == aLine->mFirstChild);
  aState.mInlineReflow->SetIsFirstChild(isFirstChild);
  aReflowResult = aState.mInlineReflow->ReflowFrame(aFrame);
  if (NS_IS_REFLOW_ERROR(aReflowResult)) {
    return PR_FALSE;
  }

  PRBool lineWasComplete = aLine->GetLastContentIsComplete();
  if (!NS_INLINE_IS_BREAK(aReflowResult)) {
    aState.mPrevChild = aFrame;
    if (NS_FRAME_IS_COMPLETE(aReflowResult)) {
      aFrame->GetNextSibling(aFrame);
      aLine->SetLastContentIsComplete();
//XXXListTag(stdout); printf(": exit reflow-inline-frame #1 (reflowStatus=%x)\n", aReflowResult);
      return PR_TRUE;
    }

    // Create continuation frame (if necessary); add it to the end of
    // the current line so that it can be pushed to the next line
    // properly.
    aLine->ClearLastContentIsComplete();
    rv = nsHTMLContainerFrame::CreateNextInFlow(aState.mPresContext,
                                                this, aFrame, nextInFlow);
    if (NS_OK != rv) {
      aReflowResult = nsInlineReflowStatus(rv);
      return PR_FALSE;
    }
    if (nsnull != nextInFlow) {
      // Add new child to the line
      aLine->mChildCount++;
    }
    aFrame->GetNextSibling(aFrame);
  }
  else {
    if (NS_INLINE_IS_BREAK_AFTER(aReflowResult)) {
      aState.mPrevChild = aFrame;
      if (NS_FRAME_IS_COMPLETE(aReflowResult)) {
        aLine->SetLastContentIsComplete();
      }
      else {
        // Create continuation frame (if necessary); add it to the end of
        // the current line so that it can be pushed to the next line
        // properly.
        aLine->ClearLastContentIsComplete();
        rv = nsHTMLContainerFrame::CreateNextInFlow(aState.mPresContext,
                                                    this, aFrame,
                                                    nextInFlow);
        if (NS_OK != rv) {
          aReflowResult = nsInlineReflowStatus(rv);
          return PR_FALSE;
        }
        if (nsnull != nextInFlow) {
          // Add new child to the line
          aLine->mChildCount++;
        }
      }
      aFrame->GetNextSibling(aFrame);
    }
    else {
      NS_ASSERTION(aLine->GetLastContentIsComplete(), "bad mState");
    }
  }

  // Split line since we aren't going to keep going
  rv = SplitLine(aState, aLine, aFrame, lineWasComplete);
  if (NS_IS_REFLOW_ERROR(rv)) {
    aReflowResult = nsInlineReflowStatus(rv);
  }
//XXXListTag(stdout); printf(": exit reflow-inline-frame #2 (reflowStatus=%x)\n", aReflowResult);
  return PR_FALSE;
}

// XXX alloc lines using free-list in aState

// XXX refactor this since the split NEVER has to deal with blocks

nsresult
nsBlockFrame::SplitLine(nsBlockReflowState& aState,
                        LineData* aLine,
                        nsIFrame* aFrame,
                        PRBool aLineWasComplete)
{
  PRInt32 pushCount = aLine->mChildCount -
    aState.mInlineReflow->GetCurrentFrameNum();
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsBlockFrame::SplitLine: pushing %d frames",
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
        LineData* insertedLine = new LineData(aFrame, pushCount, 0);
        aLine->mNext = insertedLine;
        insertedLine->mNext = to;
        to = insertedLine;
      } else {
        to->mFirstChild = aFrame;
        to->mChildCount += pushCount;
      }
    } else {
      to = new LineData(aFrame, pushCount, 0);
      aLine->mNext = to;
    }
    if (nsnull == to) {
      return NS_ERROR_OUT_OF_MEMORY;
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
nsBlockFrame::PullFrame(nsBlockReflowState& aState,
                        LineData* aLine,
                        LineData** aFromList,
                        PRBool aUpdateGeometricParent,
                        nsInlineReflowStatus& aReflowResult)
{
  LineData* fromLine = *aFromList;
  NS_ASSERTION(nsnull != fromLine, "bad line to pull from");
  if (0 == fromLine->mChildCount) {
    // Discard empty lines immediately. Empty lines can happen here
    // because of DeleteChildsNextInFlow not being able to delete
    // lines.
    *aFromList = fromLine->mNext;
    NS_ASSERTION(nsnull == fromLine->mFirstChild, "bad empty line");
    fromLine->mNext = aState.mFreeList;
    aState.mFreeList = fromLine;
    return PR_TRUE;
  }

  // If our line is not empty and the child in aFromLine is a block
  // then we cannot pull up the frame into this line.
  if ((0 != aLine->mChildCount) && fromLine->IsBlock()) {
    aReflowResult = NS_INLINE_LINE_BREAK_BEFORE();
    return PR_FALSE;
  }

  // Take frame from fromLine
  nsIFrame* frame = fromLine->mFirstChild;
  if (0 == aLine->mChildCount++) {
    aLine->mFirstChild = frame;
    aLine->SetIsBlock(fromLine->IsBlock());
#ifdef NS_DEBUG
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&) display);
    const nsStylePosition* position;
    frame->GetStyleData(eStyleStruct_Position,
                        (const nsStyleStruct*&) position);
    PRBool isBlock = nsLineLayout::TreatFrameAsBlock(display, position);
    NS_ASSERTION(isBlock == aLine->IsBlock(), "bad line isBlock");
#endif
  }
  if (0 != --fromLine->mChildCount) {
    frame->GetNextSibling(fromLine->mFirstChild);
  }
  else {
    // Free up the fromLine now that it's empty
    *aFromList = fromLine->mNext;
    fromLine->mFirstChild = nsnull;
    fromLine->mNext = aState.mFreeList;
    aState.mFreeList = fromLine;
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

// XXX This is identical to the back end of the block reflow code, not
// counting the continuation of block frames part. Factor this!

PRBool
nsBlockFrame::PlaceLine(nsBlockReflowState& aState,
                        LineData* aLine,
                        nsInlineReflowStatus aReflowStatus)
{
  // Align the children. This also determines the actual height and
  // width of the line.
  nsInlineReflow& ir = *aState.mInlineReflow;
  ir.VerticalAlignFrames(aLine->mBounds);
  ir.HorizontalAlignFrames(aLine->mBounds);
  ir.RelativePositionFrames();

  // See if the line fit. If it doesn't we need to push it. Our first
  // line will always fit.

  // XXX This is a good place to check and see if we have
  // below-current-line floaters, and if we do make sure that they fit
  // too.

  // XXX don't forget to factor in the top/bottom margin when sharing
  // this with the block code
  nscoord newY = aState.mY + aLine->mBounds.height;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsBlockFrame::PlaceLine: newY=%d limit=%d lineHeight=%d",
      newY, aState.mBottomEdge, aLine->mBounds.height));
  if ((mLines != aLine) && (newY > aState.mBottomEdge)) {
    // Push this line and all of it's children and anything else that
    // follows to our next-in-flow
    PushLines(aState);
    return PR_FALSE;
  }

  // Now that we know the line is staying put, put in the outside
  // bullet if we have one.
  if ((nsnull == mPrevInFlow) && (aLine == mLines) && (nsnull != mBullet)) {
    const nsStyleList* list;
    GetStyleData(eStyleStruct_List, (const nsStyleStruct*&)list);
    if (NS_STYLE_LIST_STYLE_POSITION_OUTSIDE == list->mListStylePosition) {
      // Reflow the bullet now
      nsSize availSize;
      availSize.width = NS_UNCONSTRAINEDSIZE;
      availSize.height = NS_UNCONSTRAINEDSIZE;
      nsReflowState reflowState(mBullet, aState, availSize);
      nsReflowMetrics metrics(nsnull);
      nsIInlineReflow* iir;
      if (NS_OK == mBullet->QueryInterface(kIInlineReflowIID, (void**) &iir)) {
        mBullet->WillReflow(aState.mPresContext);
        iir->InlineReflow(aState.mLineLayout, metrics, reflowState);
        mBullet->DidReflow(aState.mPresContext, NS_FRAME_REFLOW_FINISHED);
      }

      // Place the bullet now
      nsMargin padding;
      aState.mStyleSpacing->CalcPaddingFor(this, padding);
      nscoord x = aState.mBorderPadding.left - (padding.left/2) -
        metrics.width;
      nscoord y = aState.mBorderPadding.top + ir.GetMaxAscent() -
        metrics.ascent;
      mBullet->SetRect(nsRect(x, y, metrics.width, metrics.height));
    }
  }

  // Update max-element-size
  if (aState.mComputeMaxElementSize) {
    nsSize& lineMaxElementSize = aState.mMaxElementSize;
    if (lineMaxElementSize.width > aState.mMaxElementSize.width) {
      aState.mMaxElementSize.width = lineMaxElementSize.width;
    }
    if (lineMaxElementSize.height > aState.mMaxElementSize.height) {
      aState.mMaxElementSize.height = lineMaxElementSize.height;
    }
  }

  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
  }
  aState.mY = newY;

  // Any below current line floaters to place?
  if (0 != aState.mPendingFloaters.Count()) {
    aState.PlaceFloaters(&aState.mPendingFloaters);
    aState.mPendingFloaters.Clear();

    // XXX Factor in the height of the floaters as well when considering
    // whether the line fits.
    // The default policy is that if there isn't room for the floaters then
    // both the line and the floaters are pushed to the next-in-flow...
  }

  // Based on the last child we reflowed reflow status, we may need to
  // clear past any floaters.
  if (NS_INLINE_IS_BREAK_AFTER(aReflowStatus)) {
    // Apply break to the line
    PRUint8 breakType = NS_INLINE_GET_BREAK_TYPE(aReflowStatus);
    switch (breakType) {
    default:
      break;
    case NS_STYLE_CLEAR_LEFT:
    case NS_STYLE_CLEAR_RIGHT:
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
         ("nsBlockFrame::PlaceLine: clearing floaters=%d",
          breakType));
      aState.ClearFloaters(breakType);
      break;
    }
    // XXX page breaks, etc, need to be passed upwards too!
  }

  // Update available space after placing line in case below current
  // line floaters were placed or in case we just used up the space in
  // the current band and are ready to move into a new band.
  aState.GetAvailableSpace();

  return PR_TRUE;
}

static nsresult
FindFloatersIn(nsIFrame* aFrame, nsVoidArray*& aArray)
{
  const nsStyleDisplay* display;
  aFrame->GetStyleData(eStyleStruct_Display,
                       (const nsStyleStruct*&) display);
  if (NS_STYLE_FLOAT_NONE != display->mFloats) {
    if (nsnull == aArray) {
      aArray = new nsVoidArray();
      if (nsnull == aArray) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    aArray->AppendElement(aFrame);
  }

  if (NS_STYLE_DISPLAY_INLINE == display->mDisplay) {
    nsIFrame* kid;
    aFrame->FirstChild(kid);
    while (nsnull != kid) {
      nsresult rv = FindFloatersIn(kid, aArray);
      if (NS_OK != rv) {
        return rv;
      }
      kid->GetNextSibling(kid);
    }
  }
  return NS_OK;
}

void
nsBlockFrame::FindFloaters(LineData* aLine)
{
  nsVoidArray* floaters = aLine->mFloaters;
  if (nsnull != floaters) {
    // Empty floater array before proceeding
    floaters->Clear();
  }

  nsIFrame* frame = aLine->mFirstChild;
  PRInt32 n = aLine->mChildCount;
  while (--n >= 0) {
    FindFloatersIn(frame, floaters);
    frame->GetNextSibling(frame);
  }

  aLine->mFloaters = floaters;

  // Get rid of floater array if we don't need it
  if (nsnull != floaters) {
    if (0 == floaters->Count()) {
      delete floaters;
      aLine->mFloaters = nsnull;
    }
  }
}

void
nsBlockFrame::PushLines(nsBlockReflowState& aState)
{
  NS_ASSERTION(nsnull != aState.mPrevLine, "bad push");

  LineData* lastLine = aState.mPrevLine;
  LineData* nextLine = lastLine->mNext;

  lastLine->mNext = nsnull;
  mOverflowLines = nextLine;

  // Break frame sibling list
  nsIFrame* lastFrame = lastLine->LastChild();
  lastFrame->SetNextSibling(nsnull);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsBlockFrame::PushLines: line=%p prevInFlow=%p nextInFlow=%p",
      mOverflowLines, mPrevInFlow, mNextInFlow));
#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyChildCount(mLines);
    VerifyChildCount(mOverflowLines, PR_TRUE);
  }
#endif
}

PRBool
nsBlockFrame::DrainOverflowLines()
{
  PRBool drained = PR_FALSE;

  // First grab the prev-in-flows overflow lines
  nsBlockFrame* prevBlock = (nsBlockFrame*) mPrevInFlow;
  if (nsnull != prevBlock) {
    LineData* line = prevBlock->mOverflowLines;
    if (nsnull != line) {
      drained = PR_TRUE;
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
         ("nsBlockFrame::DrainOverflowLines: line=%p prevInFlow=%p",
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
       ("nsBlockFrame::DrainOverflowLines: from me, line=%p",
        mOverflowLines));
    LineData* lastLine = LastLine(mLines);
    if (nsnull == lastLine) {
      mLines = mOverflowLines;
    }
    else {
      lastLine->mNext = mOverflowLines;
      nsIFrame* lastFrame = lastLine->LastChild();
      lastFrame->SetNextSibling(mOverflowLines->mFirstChild);

      // Update our last-content-index now that we have a new last child
      lastLine = LastLine(mOverflowLines);
    }
    mOverflowLines = nsnull;
    drained = PR_TRUE;
  }

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyChildCount(mLines, PR_TRUE);
  }
#endif
  return drained;
}

nsresult
nsBlockFrame::InsertNewFrame(nsBlockFrame* aParentFrame,
                             nsIFrame* aNewFrame,
                             nsIFrame* aPrevSibling)
{
  const nsStyleDisplay* display;
  aNewFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) display);
  const nsStylePosition* position;
  aNewFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&) position);
  PRUint16 newFrameIsBlock = nsLineLayout::TreatFrameAsBlock(display, position)
                               ? LINE_IS_BLOCK : 0;

  // Insert/append the frame into flows line list at the right spot
  LineData* newLine;
  LineData* line = aParentFrame->mLines;
  if (nsnull == aPrevSibling) {
    // Insert new frame into the sibling list
    aNewFrame->SetNextSibling(line->mFirstChild);

    if (line->IsBlock() || newFrameIsBlock) {
      // Create a new line
      newLine = new LineData(aNewFrame, 1, LINE_LAST_CONTENT_IS_COMPLETE |
                             newFrameIsBlock);
      if (nsnull == newLine) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      newLine->mNext = aParentFrame->mLines;
      aParentFrame->mLines = newLine;
    } else {
      // Insert frame at the front of the line
      line->mFirstChild = aNewFrame;
      line->mChildCount++;
      line->MarkDirty();
    }
  }
  else {
    // Find line containing the previous sibling to the new frame
    line = FindLineContaining(line, aPrevSibling);
    NS_ASSERTION(nsnull != line, "no line contains the previous sibling");
    if (nsnull != line) {
      if (line->IsBlock()) {
        // Create a new line just after line
        newLine = new LineData(aNewFrame, 1, LINE_LAST_CONTENT_IS_COMPLETE |
                               newFrameIsBlock);
        if (nsnull == newLine) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        newLine->mNext = line->mNext;
        line->mNext = newLine;
      }
      else if (newFrameIsBlock) {
        // Split line in two, if necessary. We can't allow a block to
        // end up in an inline line.
        if (line->IsLastChild(aPrevSibling)) {
          // The new frame goes after prevSibling and prevSibling is
          // the last frame on the line. Therefore we don't need to
          // split the line, just create a new line.
          newLine = new LineData(aNewFrame, 1, LINE_LAST_CONTENT_IS_COMPLETE |
                                 newFrameIsBlock);
          if (nsnull == newLine) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          newLine->mNext = line->mNext;
          line->mNext = newLine;
        }
        else {
          // The new frame goes after prevSibling and prevSibling is
          // somewhere in the line, but not at the end. Split the line
          // just after prevSibling.
          PRInt32 i, n = line->mChildCount;
          nsIFrame* frame = line->mFirstChild;
          for (i = 0; i < n; i++) {
            if (frame == aPrevSibling) {
              nsIFrame* nextSibling;
              aPrevSibling->GetNextSibling(nextSibling);

              // Create new line to hold the remaining frames
              NS_ASSERTION(n - i - 1 > 0, "bad line count");
              newLine = new LineData(nextSibling, n - i - 1,
                               line->mState & LINE_LAST_CONTENT_IS_COMPLETE);
              if (nsnull == newLine) {
                return NS_ERROR_OUT_OF_MEMORY;
              }
              newLine->mNext = line->mNext;
              line->mNext = newLine;
              line->MarkDirty();
              line->SetLastContentIsComplete();
              line->mChildCount = i + 1;
              break;
            }
            frame->GetNextSibling(frame);
          }

          // Now create a new line to hold the block
          newLine = new LineData(aNewFrame, 1,
                             newFrameIsBlock | LINE_LAST_CONTENT_IS_COMPLETE);
          if (nsnull == newLine) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          newLine->mNext = line->mNext;
          line->mNext = newLine;
        }
      }
      else {
        // Insert frame into the line.
        //XXX NS_ASSERTION(line->GetLastContentIsComplete(), "bad line LCIC");
        line->mChildCount++;
        line->MarkDirty();
      }
    }

    // Insert new frame into the sibling list; note: this must be done
    // after the above logic because the above logic depends on the
    // sibling list being in the "before insertion" state.
    nsIFrame* nextSibling;
    aPrevSibling->GetNextSibling(nextSibling);
    aNewFrame->SetNextSibling(nextSibling);
    aPrevSibling->SetNextSibling(aNewFrame);
  }

  return NS_OK;
}

// XXX CONSTRUCTION
#if 0
// XXX we assume that the insertion is really an assertion and never an append
// XXX what about zero lines case
NS_IMETHODIMP
nsBlockFrame::ContentInserted(nsIPresShell*   aShell,
                                 nsIPresContext* aPresContext,
                                 nsIContent*     aContainer,
                                 nsIContent*     aChild,
                                 PRInt32         aIndexInParent)
{
  // Find the frame that precedes this frame
  nsIFrame* prevSibling = nsnull;
  if (aIndexInParent > 0) {
    nsIContent* precedingContent;
    aContainer->ChildAt(aIndexInParent - 1, precedingContent);
    prevSibling = aShell->FindFrameWithContent(precedingContent);
    NS_ASSERTION(nsnull != prevSibling, "no frame for preceding content");
    NS_RELEASE(precedingContent);

    // The frame may have a next-in-flow. Get the last-in-flow; we do
    // it the hard way because we can't assume that prevSibling is a
    // subclass of nsSplittableFrame.
    nsIFrame* nextInFlow;
    do {
      prevSibling->GetNextInFlow(nextInFlow);
      if (nsnull != nextInFlow) {
        prevSibling = nextInFlow;
      }
    } while (nsnull != nextInFlow);
  }

  // Get the parent of the previous sibling (which will be the proper
  // next-in-flow for the child). We expect it to be this frame or one
  // of our next-in-flow(s).
  nsBlockFrame* flow = this;
  if (nsnull != prevSibling) {
    prevSibling->GetGeometricParent((nsIFrame*&)flow);
  }

  // Now that we have the right flow block we can create the new
  // frame; test and see if the inserted frame is a block or not.
  // XXX create-frame could return that fact
  nsIFrame* newFrame;
  nsresult rv = nsHTMLContainerFrame::CreateFrame(aPresContext, flow, aChild,
                                        nsnull, newFrame);
  if (NS_OK != rv) {
    return rv;
  }

  InsertNewFrame(flow, newFrame, prevSibling);

  // Generate a reflow command
  nsIReflowCommand* cmd;
  rv = NS_NewHTMLReflowCommand(&cmd, flow, nsIReflowCommand::FrameInserted);
  if (NS_OK != rv) {
    return rv;
  }
  aShell->AppendReflowCommand(cmd);
  NS_RELEASE(cmd);

  return NS_OK;
}
#endif

NS_IMETHODIMP
nsBlockFrame::ContentDeleted(nsIPresShell*   aShell,
                                nsIPresContext* aPresContext,
                                nsIContent*     aContainer,
                                nsIContent*     aChild,
                                PRInt32         aIndexInParent)
{
  // Find the frame that precedes the frame to destroy and the frame
  // to destroy (the first-in-flow if the frame is continued). We also
  // find which of our next-in-flows contain the dead frame.
  nsBlockFrame* flow;
  nsIFrame* deadFrame;
  nsIFrame* prevSibling;
  if (aIndexInParent > 0) {
    nsIContent* precedingContent;
    aContainer->ChildAt(aIndexInParent - 1, precedingContent);
    prevSibling = aShell->FindFrameWithContent(precedingContent);
    NS_RELEASE(precedingContent);

    // The frame may have a next-in-flow. Get the last-in-flow; we do
    // it the hard way because we can't assume that prevSibling is a
    // subclass of nsSplittableFrame.
    nsIFrame* nextInFlow;
    do {
      prevSibling->GetNextInFlow(nextInFlow);
      if (nsnull != nextInFlow) {
        prevSibling = nextInFlow;
      }
    } while (nsnull != nextInFlow);

    // Get the dead frame (maybe)
    prevSibling->GetGeometricParent((nsIFrame*&)flow);
    prevSibling->GetNextSibling(deadFrame);
    if (nsnull == deadFrame) {
      // The deadFrame must be prevSibling's parent's next-in-flows
      // first frame. Therefore it doesn't have a prevSibling.
      flow = (nsBlockFrame*) flow->mNextInFlow;
      if (nsnull != flow) {
        deadFrame = flow->mLines->mFirstChild;
      }
      prevSibling = nsnull;
    }
  }
  else {
    prevSibling = nsnull;
    flow = this;
    deadFrame = mLines->mFirstChild;
  }
  NS_ASSERTION(nsnull != deadFrame, "yikes! couldn't find frame");
  if (nsnull == deadFrame) {
    return NS_OK;
  }

  // Generate a reflow command for the appropriate flow frame
  nsIReflowCommand* cmd;
  nsresult rv = NS_NewHTMLReflowCommand(&cmd, flow,
                                        nsIReflowCommand::FrameDeleted);
  if (NS_OK != rv) {
    return rv;
  }
  aShell->AppendReflowCommand(cmd);
  NS_RELEASE(cmd);

  // Find line that contains deadFrame; we also find the pointer to
  // the line.
  LineData** linep = &flow->mLines;
  LineData* line = flow->mLines;
  while (nsnull != line) {
    if (line->Contains(deadFrame)) {
      break;
    }
    linep = &line->mNext;
    line = line->mNext;
  }

  // Remove frame and its continuations
  PRBool pseudos = flow->IsPseudoFrame();
  while (nsnull != deadFrame) {
    while ((nsnull != line) && (nsnull != deadFrame)) {
#ifdef NS_DEBUG
      nsIFrame* parent;
      deadFrame->GetGeometricParent(parent);
      NS_ASSERTION(flow == parent, "messed up delete code");
#endif
      NS_FRAME_TRACE(NS_FRAME_TRACE_CHILD_REFLOW,
         ("nsBlockFrame::ContentDeleted: deadFrame=%p",
          deadFrame));

      // Remove deadFrame from the line
      if (line->mFirstChild == deadFrame) {
        nsIFrame* nextFrame;
        deadFrame->GetNextSibling(nextFrame);
        line->mFirstChild = nextFrame;
      }
      else {
        nsIFrame* lastFrame = line->LastChild();
        if (lastFrame == deadFrame) {
          line->SetLastContentIsComplete();
        }
      }

      // Take deadFrame out of the sibling list
      if (nsnull != prevSibling) {
        nsIFrame* nextFrame;
        deadFrame->GetNextSibling(nextFrame);
        prevSibling->SetNextSibling(nextFrame);
      }

      // Destroy frame; capture its next-in-flow first in case we need
      // to destroy that too.
      nsIFrame* nextInFlow;
      deadFrame->GetNextInFlow(nextInFlow);
      if (nsnull != nextInFlow) {
        deadFrame->BreakFromNextFlow();
      }
      deadFrame->DeleteFrame(*aPresContext);
      deadFrame = nextInFlow;

      // If line is empty, remove it now
      LineData* next = line->mNext;
      if (0 == --line->mChildCount) {
        *linep = next;
        line->mNext = nsnull;
        delete line;
      }
      else {
        linep = &line->mNext;
      }
      line = next;
    }

    // Advance to next flow block if the frame has more continuations
    if (nsnull != deadFrame) {
      flow = (nsBlockFrame*) flow->mNextInFlow;
      NS_ASSERTION(nsnull != flow, "whoops, continuation without a parent");
      line = flow->mLines;
      prevSibling = nsnull;
    }
  }

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    // Verify that the above delete code actually deleted the frames!
    flow = this;
    while (nsnull != flow) {
      nsIFrame* frame = flow->mLines->mFirstChild;
      while (nsnull != frame) {
        nsIContent* content;
        frame->GetContent(content);
        NS_ASSERTION(content != aChild, "delete failed");
        NS_RELEASE(content);
        frame->GetNextSibling(frame);
      }
      flow = (nsBlockFrame*) flow->mNextInFlow;
    }
  }
#endif

  return rv;
}

PRBool
nsBlockFrame::DeleteChildsNextInFlow(nsIPresContext& aPresContext,
                                     nsIFrame* aChild)
{
  NS_PRECONDITION(IsChild(aChild), "bad geometric parent");

  nsIFrame* nextInFlow;
  nsBlockFrame* parent;
   
  aChild->GetNextInFlow(nextInFlow);
  NS_PRECONDITION(nsnull != nextInFlow, "null next-in-flow");
  nextInFlow->GetGeometricParent((nsIFrame*&)parent);

  // If the next-in-flow has a next-in-flow then delete it, too (and
  // delete it first).
  nsIFrame* nextNextInFlow;
  nextInFlow->GetNextInFlow(nextNextInFlow);
  if (nsnull != nextNextInFlow) {
    parent->DeleteChildsNextInFlow(aPresContext, nextInFlow);
  }

#ifdef NS_DEBUG
  PRInt32   childCount;
  nsIFrame* firstChild;
  nextInFlow->FirstChild(firstChild);
  childCount = LengthOf(firstChild);
  NS_ASSERTION((0 == childCount) && (nsnull == firstChild),
               "deleting !empty next-in-flow");
#endif

  // Disconnect the next-in-flow from the flow list
  nextInFlow->BreakFromPrevFlow();

  // Remove nextInFlow from the parents line list. Also remove it from
  // the sibling list.
  if (RemoveChild(parent->mLines, nextInFlow)) {
    NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
       ("nsBlockFrame::DeleteNextInFlowsFor: frame=%p (from mLines)",
        nextInFlow));
    goto done;
  }

  // If we get here then we didn't find the child on the line list. If
  // it's not there then it has to be on the overflow lines list.
  if (nsnull != mOverflowLines) {
    if (RemoveChild(parent->mOverflowLines, nextInFlow)) {
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
         ("nsBlockFrame::DeleteNextInFlowsFor: frame=%p (from overflow)",
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

  // Delete the next-in-flow frame and adjust its parents child count
  nextInFlow->DeleteFrame(aPresContext);

#ifdef NS_DEBUG
  aChild->GetNextInFlow(nextInFlow);
  NS_POSTCONDITION(nsnull == nextInFlow, "non null next-in-flow");
#endif
  return PR_TRUE;
}

PRBool
nsBlockFrame::RemoveChild(LineData* aLines, nsIFrame* aChild)
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
        NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
           ("nsBlockFrame::RemoveChild: line=%p frame=%p",
            line, aChild));
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

////////////////////////////////////////////////////////////////////////
// Floater support

void
nsBlockFrame::ReflowFloater(nsIPresContext& aPresContext,
                            nsBlockReflowState& aState,
                            nsIFrame* aFloaterFrame)
{
  // Prepare the reflow state for the floater frame. Note that initially
  // it's maxSize will be 0,0 until we compute it (we need the reflowState
  // for nsLayout::GetStyleSize so we have to do this first)
  nsSize kidAvailSize(0, 0);
  nsReflowState reflowState(aFloaterFrame, aState, kidAvailSize,
                            eReflowReason_Initial);

  // Compute the available space for the floater. Use the default
  // 'auto' width and height values
  nsSize styleSize;
  PRIntn styleSizeFlags =
    nsCSSLayout::GetStyleSize(&aPresContext, reflowState, styleSize);

  // XXX The width and height are for the content area only. Add in space for
  // border and padding
  if (styleSizeFlags & NS_SIZE_HAS_WIDTH) {
    kidAvailSize.width = styleSize.width;
  }
  else {
    // If we are floating something and we don't know the width then
    // find a maximum width for it to reflow into.

    // XXX if the child is a block (instead of a table, say) then this
    // will do the wrong thing. A better choice would be
    // NS_UNCONSTRAINEDSIZE, but that has special meaning to tables.
    const nsReflowState* rsp = &aState;
    kidAvailSize.width = 0;
    while (nsnull != rsp) {
      if ((0 != rsp->maxSize.width) &&
          (NS_UNCONSTRAINEDSIZE != rsp->maxSize.width)) {
        kidAvailSize.width = rsp->maxSize.width;
        break;
      }
      rsp = rsp->parentReflowState;
    }
    NS_ASSERTION(0 != kidAvailSize.width, "no width for block found");
  }
  if (styleSizeFlags & NS_SIZE_HAS_HEIGHT) {
    kidAvailSize.height = styleSize.height;
  }
  else {
    kidAvailSize.height = NS_UNCONSTRAINEDSIZE;
  }
  reflowState.maxSize = kidAvailSize;

  // Resize reflow the anchored item into the available space
  // XXX Check for complete?
  nsReflowMetrics desiredSize(nsnull);
  nsReflowStatus  status;
  aFloaterFrame->WillReflow(aPresContext);
  aFloaterFrame->Reflow(aPresContext, desiredSize, reflowState, status);
  aFloaterFrame->SizeTo(desiredSize.width, desiredSize.height);
}

PRBool
nsBlockFrame::AddFloater(nsIPresContext* aPresContext,
                         const nsReflowState& aReflowState,
                         nsIFrame* aFloater,
                         nsPlaceholderFrame* aPlaceholder)
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
  nsBlockReflowState* state = (nsBlockReflowState*) rs;

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

    // Reflow the floater (the first time we do it here; later on it's
    // done during the reflow of the line that contains the floater)
    ReflowFloater(*aPresContext, *state, aFloater);
    return PR_TRUE;
  }

  return PR_FALSE;
}

// This is called by the line layout's AddFloater method when a
// place-holder frame is reflowed in a line. If the floater is a
// left-most child (it's x coordinate is at the line's left margin)
// then the floater is place immediately, otherwise the floater
// placement is deferred until the line has been reflowed.
void
nsBlockReflowState::AddFloater(nsPlaceholderFrame* aPlaceholder)
{
  // Update the current line's floater array
  NS_ASSERTION(nsnull != mCurrentLine, "null ptr");
  if (nsnull == mCurrentLine->mFloaters) {
    mCurrentLine->mFloaters = new nsVoidArray();
  }
  mCurrentLine->mFloaters->AppendElement(aPlaceholder);

  // Now place the floater immediately if possible. Otherwise stash it
  // away in mPendingFloaters and place it later.
  if (IsLeftMostChild(aPlaceholder)) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsBlockReflowState::AddFloater: IsLeftMostChild, placeHolder=%p",
        aPlaceholder));

    // Because we are in the middle of reflowing a placeholder frame
    // within a line (and possibly nested in an inline frame or two
    // that's a child of our block) we need to restore the space
    // manager's translation to the space that the block resides in
    // before placing the floater.
    nscoord ox, oy;
    mSpaceManager->GetTranslation(ox, oy);
    nscoord dx = ox - (mSpaceManagerX + mBorderPadding.left);
    nscoord dy = oy - (mSpaceManagerY + mBorderPadding.top);
    mSpaceManager->Translate(-dx, -dy);
    PlaceFloater(aPlaceholder);

    // Pass on updated available space to the current inline reflow engine
    GetAvailableSpace();
    mInlineReflow->UpdateBand(mCurrentBand.availSpace.x + mBorderPadding.left,
                              mY,
// XXX how do I know what to use here??? mContentArea.width or this?
                              mCurrentBand.availSpace.width -
                              mBorderPadding.left - mBorderPadding.right,
                              mCurrentBand.availSpace.height);

    // Restore coordinate system
    mSpaceManager->Translate(dx, dy);
  }
  else {
    // This floater will be placed after the line is done (it is a
    // below current line floater).
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsBlockReflowState::AddFloater: pending, placeHolder=%p",
        aPlaceholder));
    mPendingFloaters.AppendElement(aPlaceholder);
  }
}

// XXX Inline frame layout and block layout need to be more
// coordinated; IsFirstChild in the inline code is doing much the same
// thing as below; firstness should be well known.
PRBool
nsBlockReflowState::IsLeftMostChild(nsIFrame* aFrame)
{
  for (;;) {
    nsIFrame* parent;
    aFrame->GetGeometricParent(parent);
    if (parent == mBlock) {
      nsIFrame* child = mCurrentLine->mFirstChild;
      PRInt32 n = mCurrentLine->mChildCount;
      while ((nsnull != child) && (aFrame != child) && (--n >= 0)) {
        nsSize  size;

        // Is the child zero-sized?
        child->GetSize(size);
        if (size.width > 0) {
          // We found a non-zero sized child frame that precedes aFrame
          return PR_FALSE;
        }
        child->GetNextSibling(child);
      }
      break;
    }
    else {
      // See if there are any non-zero sized child frames that precede
      // aFrame in the child list
      nsIFrame* child;
      parent->FirstChild(child);
      while ((nsnull != child) && (aFrame != child)) {
        nsSize  size;

        // Is the child zero-sized?
        child->GetSize(size);
        if (size.width > 0) {
          // We found a non-zero sized child frame that precedes aFrame
          return PR_FALSE;
        }
        child->GetNextSibling(child);
      }
    }
  
    // aFrame is the left-most non-zero sized frame in its geometric parent.
    // Walk up one level and check that its parent is left-most as well
    aFrame = parent;
  }
  return PR_TRUE;
}

void
nsBlockReflowState::PlaceFloater(nsPlaceholderFrame* aPlaceholder)
{
  nsISpaceManager* sm = mSpaceManager;
  nsIFrame* floater = aPlaceholder->GetAnchoredItem();

  // Remove floaters old placement from the space manager
  sm->RemoveRegion(floater);

  // Reflow the floater if it's targetted for a reflow
  if (nsnull != reflowCommand) {
    nsIFrame* target;
    reflowCommand->GetTarget(target);
    if (floater == target) {
      mBlock->ReflowFloater(mPresContext, *this, floater);
    }
  }

  // Get the band of available space
  GetAvailableSpace();

  // Get the type of floater
  const nsStyleDisplay* floaterDisplay;
  const nsStyleSpacing* floaterSpacing;
  floater->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&)floaterDisplay);
  floater->GetStyleData(eStyleStruct_Spacing,
                        (const nsStyleStruct*&)floaterSpacing);

  // Get the floaters bounding box and margin information
  nsRect region;
  floater->GetRect(region);
  nsMargin floaterMargin;
  floaterSpacing->CalcMarginFor(floater, floaterMargin);

  // Compute the size of the region that will impact the space manager
  region.y = mY;
  switch (floaterDisplay->mFloats) {
  default:
    NS_NOTYETIMPLEMENTED("Unsupported floater type");
    // FALL THROUGH

  case NS_STYLE_FLOAT_LEFT:
    region.x = mCurrentBand.availSpace.x;
    region.width += mBulletPadding;/* XXX */
    break;

  case NS_STYLE_FLOAT_RIGHT:
    region.x = mCurrentBand.availSpace.XMost() - region.width;
    region.x -= floaterMargin.right;
    if (region.x < 0) {
      region.x = 0;
    }
  }

  // Factor in the floaters margins
  region.width += floaterMargin.left + floaterMargin.right;
  region.height += floaterMargin.top + floaterMargin.bottom;
  sm->AddRectRegion(floater, region);

  // Set the origin of the floater in world coordinates
  nscoord worldX = mSpaceManagerX;
  nscoord worldY = mSpaceManagerY;
  if (NS_STYLE_FLOAT_RIGHT == floaterDisplay->mFloats) {
    floater->MoveTo(region.x + worldX + floaterMargin.left,
                    region.y + worldY + floaterMargin.top);
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsBlockReflowState::PlaceFloater: right, placeHolder=%p xy=%d,%d worldxy=%d,%d",
        aPlaceholder, region.x + worldX + floaterMargin.left,
        region.y + worldY + floaterMargin.top,
        worldX, worldY));
  }
  else {
    floater->MoveTo(region.x + worldX + floaterMargin.left + mBulletPadding,/* XXX */
                    region.y + worldY + floaterMargin.top);
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsBlockReflowState::PlaceFloater: left, placeHolder=%p xy=%d,%d worldxy=%d,%d",
        aPlaceholder, region.x + worldX + floaterMargin.left + mBulletPadding,/* XXX */
        region.y + worldY + floaterMargin.top,
        worldX, worldY));
  }
}

/**
 * Place below-current-line floaters.
 */
void
nsBlockReflowState::PlaceFloaters(nsVoidArray* aFloaters)
{
  NS_PRECONDITION(aFloaters->Count() > 0, "no floaters");

  PRInt32 numFloaters = aFloaters->Count();
  for (PRInt32 i = 0; i < numFloaters; i++) {
    nsPlaceholderFrame* placeholderFrame = (nsPlaceholderFrame*)
      aFloaters->ElementAt(i);
    if (IsLeftMostChild(placeholderFrame)) {
      // Left-most children are placed during the line's reflow
      continue;
    }
    PlaceFloater(placeholderFrame);
  }

  // Update available spcae now that the floaters have been placed
  GetAvailableSpace();
}

void
nsBlockReflowState::ClearFloaters(PRUint8 aBreakType)
{
  for (;;) {
    PRBool haveFloater = PR_FALSE;
    // Find the Y coordinate to clear to. Note that the band trapezoid
    // coordinates are relative to the last translation. Since we
    // never translate by Y before getting a band, we can use absolute
    // comparisons against mY.
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsBlockReflowState::ClearFloaters: mY=%d trapCount=%d",
        mY, mCurrentBand.count));
    nscoord clearYMost = mY;
    nsRect tmp;
    PRInt32 i;
    for (i = 0; i < mCurrentBand.count; i++) {
      const nsStyleDisplay* display;
      nsBandTrapezoid* trapezoid = &mCurrentBand.data[i];
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
         ("nsBlockReflowState::ClearFloaters: trap=%d state=%d",
          i, trapezoid->state));
      if (nsBandTrapezoid::Available != trapezoid->state) {
        haveFloater = PR_TRUE;
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 fn, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (fn = 0; fn < numFrames; fn++) {
            nsIFrame* frame = (nsIFrame*) trapezoid->frames->ElementAt(fn);
            frame->GetStyleData(eStyleStruct_Display,
                                (const nsStyleStruct*&)display);
            NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsBlockReflowState::ClearFloaters: frame[%d]=%p floats=%d",
                fn, frame, display->mFloats));
            switch (display->mFloats) {
            case NS_STYLE_FLOAT_LEFT:
              if ((NS_STYLE_CLEAR_LEFT == aBreakType) ||
                  (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
                trapezoid->GetRect(tmp);
                nscoord ym = tmp.YMost();
                if (ym > clearYMost) {
                  clearYMost = ym;
                  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsBlockReflowState::ClearFloaters: left clearYMost=%d",
                    clearYMost));
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
                  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                  ("nsBlockReflowState::ClearFloaters: right clearYMost=%d",
                   clearYMost));
                }
              }
              break;
            }
          }
        }
        else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                         (const nsStyleStruct*&)display);
          NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
             ("nsBlockReflowState::ClearFloaters: frame=%p floats=%d",
              trapezoid->frame, display->mFloats));
          switch (display->mFloats) {
          case NS_STYLE_FLOAT_LEFT:
            if ((NS_STYLE_CLEAR_LEFT == aBreakType) ||
                (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
              trapezoid->GetRect(tmp);
              nscoord ym = tmp.YMost();
NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
   ("nsBlockReflowState::ClearFloaters: left? ym=%d clearYMost=%d",
    ym, clearYMost));
              if (ym > clearYMost) {
                clearYMost = ym;
                NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                  ("nsBlockReflowState::ClearFloaters: left clearYMost=%d",
                   clearYMost));
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
                NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                  ("nsBlockReflowState::ClearFloaters: right clearYMost=%d",
                   clearYMost));
              }
            }
            break;
          }
        }
      }
    }
    if (!haveFloater) {
      break;
    }

    if (clearYMost == mY) {
      // Nothing to clear
      break;
    }

    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsBlockReflowState::ClearFloaters: mY=%d clearYMost=%d",
        mY, clearYMost));

    mY = clearYMost + 1;

    // Get a new band
    GetAvailableSpace();
  }
}

//////////////////////////////////////////////////////////////////////
// Painting, event handling

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

NS_IMETHODIMP
nsBlockFrame::Paint(nsIPresContext&      aPresContext,
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

    nsRect  rect(0, 0, mRect.width, mRect.height);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *color);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, *spacing, skipSides);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);

  if (nsIFrame::GetShowFrameBorders()) {
    nsIView* view;
    GetView(view);
    if (nsnull != view) {
      aRenderingContext.SetColor(NS_RGB(0,0,255));
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
void
nsBlockFrame::PaintChildren(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
  // Set clip rect so that children don't leak out of us
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
  PRBool hidden = PR_FALSE;
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(nsRect(0, 0, mRect.width, mRect.height),
                                  nsClipCombine_kIntersect);
    hidden = PR_TRUE;
  }

  if ((nsnull == mPrevInFlow) && (nsnull != mBullet)) {
    const nsStyleList* list;
    GetStyleData(eStyleStruct_List, (const nsStyleStruct*&)list);
    if (NS_STYLE_LIST_STYLE_POSITION_OUTSIDE == list->mListStylePosition) {
      // Paint the bullet too
      PaintChild(aPresContext, aRenderingContext, aDirtyRect, mBullet,
                 PR_FALSE);
    }
  }

  // Iterate the lines looking for lines that intersect the dirty rect
  for (LineData* line = mLines; nsnull != line; line = line->mNext) {
    // Stop when we get to a line that's below the dirty rect
    if (line->mBounds.y >= aDirtyRect.YMost()) {
      break;
    }

    // If the line overlaps the dirty rect then iterate the child frames
    // and paint those frames that intersect the dirty rect
    if (line->mBounds.YMost() > aDirtyRect.y) {
      nsIFrame* kid = line->mFirstChild;
      for (PRUint16 i = 0; i < line->mChildCount; i++) {
        PaintChild(aPresContext, aRenderingContext, aDirtyRect, kid, PR_TRUE);
        kid->GetNextSibling(kid);
      }
    }
  }

  if (hidden) {
    aRenderingContext.PopState();
  }
}

void
nsBlockFrame::PaintChild(nsIPresContext& aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect,
                         nsIFrame* aFrame,
                         PRBool aClip)
{
  nsIView *pView;
  aFrame->GetView(pView);
  if (nsnull == pView) {
    nsRect kidRect;
    aFrame->GetRect(kidRect);
    nsRect damageArea;
#ifdef NS_DEBUG
    PRBool overlap = PR_FALSE;
    if (nsIFrame::GetShowFrameBorders() &&
        ((kidRect.width == 0) || (kidRect.height == 0))) {
      nscoord xmost = aDirtyRect.XMost();
      nscoord ymost = aDirtyRect.YMost();
      if ((aDirtyRect.x <= kidRect.x) && (kidRect.x < xmost) &&
          (aDirtyRect.y <= kidRect.y) && (kidRect.y < ymost)) {
        overlap = PR_TRUE;
      }
    }
    else {
      overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
    }
#else
    PRBool overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
#endif
    if (overlap || !aClip) {
      // Translate damage area into kid's coordinate system
      nsRect kidDamageArea(damageArea.x - kidRect.x,
                           damageArea.y - kidRect.y,
                           damageArea.width, damageArea.height);
      aRenderingContext.PushState();
      aRenderingContext.Translate(kidRect.x, kidRect.y);
      aFrame->Paint(aPresContext, aRenderingContext, kidDamageArea);
#ifdef NS_DEBUG
      if (nsIFrame::GetShowFrameBorders() &&
          (0 != kidRect.width) && (0 != kidRect.height)) {
        nsIView* view;
        GetView(view);
        if (nsnull != view) {
          aRenderingContext.SetColor(NS_RGB(0,0,255));
        }
        else {
          aRenderingContext.SetColor(NS_RGB(255,0,0));
        }
        aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
      }
#endif
      aRenderingContext.PopState();
    }
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
nsBlockFrame::IsChild(nsIFrame* aFrame)
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
nsBlockFrame::VerifyTree() const
{
  // XXX rewrite this
  return NS_OK;
}

#ifdef DO_SELECTION
nsIFrame * nsBlockFrame::FindHitFrame(nsBlockFrame * aBlockFrame, 
                                         const nscoord aX,
                                         const nscoord aY,
                                         const nsPoint & aPoint)
{
  nsPoint mousePoint(aPoint.x-aX, aPoint.y-aY);

  nsIFrame * contentFrame = nsnull;
  LineData * line         = aBlockFrame->mLines;
  if (nsnull != line) {
    // First find the line that contains the aIndex
    while (nsnull != line && contentFrame == nsnull) {
      nsIFrame* frame = line->mFirstChild;
      PRInt32 n = line->mChildCount;
      while (--n >= 0) {
        nsRect bounds;
        frame->GetRect(bounds);
        if (bounds.Contains(mousePoint)) {
           nsBlockFrame * blockFrame;
          if (NS_OK == frame->QueryInterface(kBlockFrameCID, (void**)&blockFrame)) {
            frame = FindHitFrame(blockFrame, bounds.x, bounds.y, aPoint);
            //NS_RELEASE(blockFrame);
            return frame;
          } else {
            return frame;
          }
        }
        frame->GetNextSibling(frame);
      }
      line = line->mNext;
    }
  }
  return aBlockFrame;
}

NS_METHOD nsBlockFrame::HandleEvent(nsIPresContext& aPresContext,
                                       nsGUIEvent* aEvent,
                                       nsEventStatus& aEventStatus)
{
  if (0) {
    nsHTMLContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

  //return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
    aEventStatus = nsEventStatus_eIgnore;
  
  //if (nsnull != mContent && (aEvent->message != NS_MOUSE_LEFT_BUTTON_UP ||
  //    (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP && !mDoingSelection))) {
  if (nsnull != mContent) {
    mContent->HandleDOMEvent(aPresContext, (nsEvent*)aEvent, nsnull, DOM_EVENT_INIT, aEventStatus);
  }

  if (DisplaySelection(aPresContext) == PR_FALSE) {
    if (aEvent->message != NS_MOUSE_LEFT_BUTTON_DOWN) {
      return NS_OK;
    }
  }

  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    int x = 0;
  }

  //nsRect bounds;
  //GetRect(bounds);
  //nsIFrame * contentFrame = FindHitFrame(this, bounds.x, bounds.y, aEvent->point);
  nsIFrame * contentFrame = FindHitFrame(this, 0,0, aEvent->point);

  if (contentFrame == nsnull) {
    return NS_OK;
  }

  if(nsEventStatus_eConsumeNoDefault != aEventStatus) {
    if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    } else if (aEvent->message == NS_MOUSE_MOVE && mDoingSelection ||
               aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      // no-op
    } else {
      return NS_OK;
    }

    if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      if (mDoingSelection) {
        contentFrame->HandleRelease(aPresContext, aEvent, aEventStatus);
      }
    } else if (aEvent->message == NS_MOUSE_MOVE) {
      mDidDrag = PR_TRUE;
      contentFrame->HandleDrag(aPresContext, aEvent, aEventStatus);
    } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
      contentFrame->HandlePress(aPresContext, aEvent, aEventStatus);
    }
  }

  return NS_OK;
}

nsIFrame * gNearByFrame = nsnull;

NS_METHOD nsBlockFrame::HandleDrag(nsIPresContext& aPresContext, 
                                      nsGUIEvent*     aEvent,
                                      nsEventStatus&  aEventStatus)
{
  if (DisplaySelection(aPresContext) == PR_FALSE)
  {
    aEventStatus = nsEventStatus_eIgnore;
    return NS_OK;
  }

  // Keep old start and end
  //nsIContent * startContent = mSelectionRange->GetStartContent(); // ref counted
  //nsIContent * endContent   = mSelectionRange->GetEndContent();   // ref counted

  mDidDrag = PR_TRUE;

  nsIFrame * contentFrame = nsnull;
  LineData* line = mLines;
  if (nsnull != line) {
    // First find the line that contains the aIndex
    while (nsnull != line && contentFrame == nsnull) {
      nsIFrame* frame = line->mFirstChild;
      PRInt32 n = line->mChildCount;
      while (--n >= 0) {
        nsRect bounds;
        frame->GetRect(bounds);
        if (aEvent->point.y >= bounds.y && aEvent->point.y < bounds.y+bounds.height) {
          contentFrame = frame;
          if (frame != gNearByFrame) {
            if (gNearByFrame != nsnull) {
              int x = 0;
            }
            aEvent->point.x = bounds.x+bounds.width-50;
            gNearByFrame = frame;
            return contentFrame->HandleDrag(aPresContext, aEvent, aEventStatus);
          } else {
            return NS_OK;
          }
          //break;
        }
        frame->GetNextSibling(frame);
      }
      line = line->mNext;
    }
  }
  return NS_OK;
}
#endif
