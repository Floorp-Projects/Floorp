/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Clark <buster@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   L. David Baron <dbaron@dbaron.org>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   IBM Corporation
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* state and methods used while laying out a single line of a block frame */

#define PL_ARENA_CONST_ALIGN_MASK (sizeof(void*)-1)
#include "plarena.h"

#include "mozilla/Util.h"
#include "nsCOMPtr.h"
#include "nsLineLayout.h"
#include "nsBlockFrame.h"
#include "nsInlineFrame.h"
#include "nsStyleConsts.h"
#include "nsHTMLContainerFrame.h"
#include "nsFloatManager.h"
#include "nsStyleContext.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsPlaceholderFrame.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIContent.h"
#include "nsTextFragment.h"
#include "nsBidiUtils.h"
#include "nsLayoutUtils.h"
#include "nsTextFrame.h"
#include "nsCSSRendering.h"

#ifdef DEBUG
#undef  NOISY_HORIZONTAL_ALIGN
#undef  NOISY_VERTICAL_ALIGN
#undef  REALLY_NOISY_VERTICAL_ALIGN
#undef  NOISY_REFLOW
#undef  REALLY_NOISY_REFLOW
#undef  NOISY_PUSHING
#undef  REALLY_NOISY_PUSHING
#undef  DEBUG_ADD_TEXT
#undef  NOISY_MAX_ELEMENT_SIZE
#undef  REALLY_NOISY_MAX_ELEMENT_SIZE
#undef  NOISY_CAN_PLACE_FRAME
#undef  NOISY_TRIM
#undef  REALLY_NOISY_TRIM
#endif

using namespace mozilla;

//----------------------------------------------------------------------

#define FIX_BUG_50257

nsLineLayout::nsLineLayout(nsPresContext* aPresContext,
                           nsFloatManager* aFloatManager,
                           const nsHTMLReflowState* aOuterReflowState,
                           const nsLineList::iterator* aLine)
  : mPresContext(aPresContext),
    mFloatManager(aFloatManager),
    mBlockReflowState(aOuterReflowState),
    mLastOptionalBreakContent(nsnull),
    mForceBreakContent(nsnull),
    mBlockRS(nsnull),/* XXX temporary */
    mLastOptionalBreakPriority(eNoBreak),
    mLastOptionalBreakContentOffset(-1),
    mForceBreakContentOffset(-1),
    mMinLineHeight(0),
    mTextIndent(0)
{
  NS_ASSERTION(aFloatManager || aOuterReflowState->frame->GetType() ==
                                  nsGkAtoms::letterFrame,
               "float manager should be present");
  MOZ_COUNT_CTOR(nsLineLayout);

  // Stash away some style data that we need
  mStyleText = aOuterReflowState->frame->GetStyleText();
  mTextAlign = mStyleText->mTextAlign;
  mLineNumber = 0;
  mFlags = 0; // default all flags to false except those that follow here...
  mTotalPlacedFrames = 0;
  mTopEdge = 0;
  mTrimmableWidth = 0;

  // Instead of always pre-initializing the free-lists for frames and
  // spans, we do it on demand so that situations that only use a few
  // frames and spans won't waste a lot of time in unneeded
  // initialization.
  PL_INIT_ARENA_POOL(&mArena, "nsLineLayout", 1024);
  mFrameFreeList = nsnull;
  mSpanFreeList = nsnull;

  mCurrentSpan = mRootSpan = nsnull;
  mSpanDepth = 0;

  if (aLine) {
    SetFlag(LL_GOTLINEBOX, PR_TRUE);
    mLineBox = *aLine;
  }
}

nsLineLayout::~nsLineLayout()
{
  MOZ_COUNT_DTOR(nsLineLayout);

  NS_ASSERTION(nsnull == mRootSpan, "bad line-layout user");

  // PL_FreeArenaPool takes our memory and puts in on a global free list so
  // that the next time an arena makes an allocation it will not have to go
  // all the way down to malloc.  This is desirable as this class is created
  // and destroyed in a tight loop.
  //
  // I looked at the code.  It is not technically necessary to call
  // PL_FinishArenaPool() after PL_FreeArenaPool(), but from an API
  // standpoint, I think we are susposed to.  It will be very fast anyway,
  // since PL_FreeArenaPool() has done all the work.
  PL_FreeArenaPool(&mArena);
  PL_FinishArenaPool(&mArena);
}

// Find out if the frame has a non-null prev-in-flow, i.e., whether it
// is a continuation.
inline PRBool
HasPrevInFlow(nsIFrame *aFrame)
{
  nsIFrame *prevInFlow = aFrame->GetPrevInFlow();
  return prevInFlow != nsnull;
}

void
nsLineLayout::BeginLineReflow(nscoord aX, nscoord aY,
                              nscoord aWidth, nscoord aHeight,
                              PRBool aImpactedByFloats,
                              PRBool aIsTopOfPage)
{
  NS_ASSERTION(nsnull == mRootSpan, "bad linelayout user");
  NS_WARN_IF_FALSE(aWidth != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
#ifdef DEBUG
  if ((aWidth != NS_UNCONSTRAINEDSIZE) && CRAZY_WIDTH(aWidth)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": Init: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
  }
  if ((aHeight != NS_UNCONSTRAINEDSIZE) && CRAZY_HEIGHT(aHeight)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": Init: bad caller: height WAS %d(0x%x)\n",
           aHeight, aHeight);
  }
#endif
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": BeginLineReflow: %d,%d,%d,%d impacted=%s %s\n",
         aX, aY, aWidth, aHeight,
         aImpactedByFloats?"true":"false",
         aIsTopOfPage ? "top-of-page" : "");
#endif
#ifdef DEBUG
  mSpansAllocated = mSpansFreed = mFramesAllocated = mFramesFreed = 0;
#endif

  SetFlag(LL_FIRSTLETTERSTYLEOK, PR_FALSE);
  SetFlag(LL_ISTOPOFPAGE, aIsTopOfPage);
  SetFlag(LL_IMPACTEDBYFLOATS, aImpactedByFloats);
  mTotalPlacedFrames = 0;
  SetFlag(LL_LINEISEMPTY, PR_TRUE);
  SetFlag(LL_LINEATSTART, PR_TRUE);
  SetFlag(LL_LINEENDSINBR, PR_FALSE);
  mSpanDepth = 0;
  mMaxTopBoxHeight = mMaxBottomBoxHeight = 0;

  if (GetFlag(LL_GOTLINEBOX)) {
    mLineBox->ClearHasBullet();
  }

  PerSpanData* psd;
  NewPerSpanData(&psd);
  mCurrentSpan = mRootSpan = psd;
  psd->mReflowState = mBlockReflowState;
  psd->mLeftEdge = aX;
  psd->mX = aX;
  psd->mRightEdge = aX + aWidth;

  mTopEdge = aY;

  psd->mNoWrap = !mStyleText->WhiteSpaceCanWrap();
  psd->mDirection = mBlockReflowState->mStyleVisibility->mDirection;
  psd->mChangedFrameDirection = PR_FALSE;

  // If this is the first line of a block then see if the text-indent
  // property amounts to anything.

  if (0 == mLineNumber && !HasPrevInFlow(mBlockReflowState->frame)) {
    const nsStyleCoord &textIndent = mStyleText->mTextIndent;
    nscoord pctBasis = 0;
    if (textIndent.HasPercent()) {
      pctBasis =
        nsHTMLReflowState::GetContainingBlockContentWidth(mBlockReflowState);

      if (GetFlag(LL_GOTLINEBOX)) {
        mLineBox->DisableResizeReflowOptimization();
      }
    }
    nscoord indent = nsRuleNode::ComputeCoordPercentCalc(textIndent, pctBasis);

    mTextIndent = indent;

    if (NS_STYLE_DIRECTION_RTL == psd->mDirection) {
      psd->mRightEdge -= indent;
    }
    else {
      psd->mX += indent;
    }
  }
}

void
nsLineLayout::EndLineReflow()
{
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": EndLineReflow: width=%d\n", mRootSpan->mX - mRootSpan->mLeftEdge);
#endif

  FreeSpan(mRootSpan);
  mCurrentSpan = mRootSpan = nsnull;

  NS_ASSERTION(mSpansAllocated == mSpansFreed, "leak");
  NS_ASSERTION(mFramesAllocated == mFramesFreed, "leak");

#if 0
  static PRInt32 maxSpansAllocated = NS_LINELAYOUT_NUM_SPANS;
  static PRInt32 maxFramesAllocated = NS_LINELAYOUT_NUM_FRAMES;
  if (mSpansAllocated > maxSpansAllocated) {
    printf("XXX: saw a line with %d spans\n", mSpansAllocated);
    maxSpansAllocated = mSpansAllocated;
  }
  if (mFramesAllocated > maxFramesAllocated) {
    printf("XXX: saw a line with %d frames\n", mFramesAllocated);
    maxFramesAllocated = mFramesAllocated;
  }
#endif
}

// XXX swtich to a single mAvailLineWidth that we adjust as each frame
// on the line is placed. Each span can still have a per-span mX that
// tracks where a child frame is going in its span; they don't need a
// per-span mLeftEdge?

void
nsLineLayout::UpdateBand(const nsRect& aNewAvailSpace,
                         nsIFrame* aFloatFrame)
{
#ifdef REALLY_NOISY_REFLOW
  printf("nsLL::UpdateBand %d, %d, %d, %d, frame=%p\n  will set mImpacted to PR_TRUE\n",
         aNewAvailSpace.x, aNewAvailSpace.y,
         aNewAvailSpace.width, aNewAvailSpace.height,
         aFloatFrame);
#endif
#ifdef DEBUG
  if ((aNewAvailSpace.width != NS_UNCONSTRAINEDSIZE) && CRAZY_WIDTH(aNewAvailSpace.width)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": UpdateBand: bad caller: width WAS %d(0x%x)\n",
           aNewAvailSpace.width, aNewAvailSpace.width);
  }
  if ((aNewAvailSpace.height != NS_UNCONSTRAINEDSIZE) && CRAZY_HEIGHT(aNewAvailSpace.height)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": UpdateBand: bad caller: height WAS %d(0x%x)\n",
           aNewAvailSpace.height, aNewAvailSpace.height);
  }
#endif

  // Compute the difference between last times width and the new width
  NS_WARN_IF_FALSE(mRootSpan->mRightEdge != NS_UNCONSTRAINEDSIZE &&
                   aNewAvailSpace.width != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
  // The root span's mLeftEdge moves to aX
  nscoord deltaX = aNewAvailSpace.x - mRootSpan->mLeftEdge;
  // The width of all spans changes by this much (the root span's
  // mRightEdge moves to aX + aWidth, its new width is aWidth)
  nscoord deltaWidth = aNewAvailSpace.width - (mRootSpan->mRightEdge - mRootSpan->mLeftEdge);
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": UpdateBand: %d,%d,%d,%d deltaWidth=%d deltaX=%d\n",
         aNewAvailSpace.x, aNewAvailSpace.y,
         aNewAvailSpace.width, aNewAvailSpace.height, deltaWidth, deltaX);
#endif

  // Update the root span position
  mRootSpan->mLeftEdge += deltaX;
  mRootSpan->mRightEdge += deltaX;
  mRootSpan->mX += deltaX;

  // Now update the right edges of the open spans to account for any
  // change in available space width
  for (PerSpanData* psd = mCurrentSpan; psd; psd = psd->mParent) {
    psd->mRightEdge += deltaWidth;
    psd->mContainsFloat = PR_TRUE;
    NS_ASSERTION(psd->mX - mTrimmableWidth <= psd->mRightEdge,
                 "We placed a float where there was no room!");
#ifdef NOISY_REFLOW
    printf("  span %p: oldRightEdge=%d newRightEdge=%d\n",
           psd, psd->mRightEdge - deltaRightEdge, psd->mRightEdge);
#endif
  }
  NS_ASSERTION(mRootSpan->mContainsFloat &&
               mRootSpan->mLeftEdge == aNewAvailSpace.x &&
               mRootSpan->mRightEdge == aNewAvailSpace.XMost(),
               "root span was updated incorrectly?");

  // Update frame bounds
  // Note: Only adjust the outermost frames (the ones that are direct
  // children of the block), not the ones in the child spans. The reason
  // is simple: the frames in the spans have coordinates local to their
  // parent therefore they are moved when their parent span is moved.
  if (deltaX != 0) {
    for (PerFrameData* pfd = mRootSpan->mFirstFrame; pfd; pfd = pfd->mNext) {
      pfd->mBounds.x += deltaX;
    }
  }

  mTopEdge = aNewAvailSpace.y;
  SetFlag(LL_IMPACTEDBYFLOATS, PR_TRUE);

  SetFlag(LL_LASTFLOATWASLETTERFRAME,
          nsGkAtoms::letterFrame == aFloatFrame->GetType());
}

nsresult
nsLineLayout::NewPerSpanData(PerSpanData** aResult)
{
  PerSpanData* psd = mSpanFreeList;
  if (nsnull == psd) {
    void *mem;
    PL_ARENA_ALLOCATE(mem, &mArena, sizeof(PerSpanData));
    if (nsnull == mem) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    psd = reinterpret_cast<PerSpanData*>(mem);
  }
  else {
    mSpanFreeList = psd->mNextFreeSpan;
  }
  psd->mParent = nsnull;
  psd->mFrame = nsnull;
  psd->mFirstFrame = nsnull;
  psd->mLastFrame = nsnull;
  psd->mContainsFloat = PR_FALSE;
  psd->mZeroEffectiveSpanBox = PR_FALSE;
  psd->mHasNonemptyContent = PR_FALSE;

#ifdef DEBUG
  mSpansAllocated++;
#endif
  *aResult = psd;
  return NS_OK;
}

nsresult
nsLineLayout::BeginSpan(nsIFrame* aFrame,
                        const nsHTMLReflowState* aSpanReflowState,
                        nscoord aLeftEdge,
                        nscoord aRightEdge,
                        nscoord* aBaseline)
{
  NS_ASSERTION(aRightEdge != NS_UNCONSTRAINEDSIZE,
               "should no longer be using unconstrained sizes");
#ifdef NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth+1);
  nsFrame::ListTag(stdout, aFrame);
  printf(": BeginSpan leftEdge=%d rightEdge=%d\n", aLeftEdge, aRightEdge);
#endif

  PerSpanData* psd;
  nsresult rv = NewPerSpanData(&psd);
  if (NS_SUCCEEDED(rv)) {
    // Link up span frame's pfd to point to its child span data
    PerFrameData* pfd = mCurrentSpan->mLastFrame;
    NS_ASSERTION(pfd->mFrame == aFrame, "huh?");
    pfd->mSpan = psd;

    // Init new span
    psd->mFrame = pfd;
    psd->mParent = mCurrentSpan;
    psd->mReflowState = aSpanReflowState;
    psd->mLeftEdge = aLeftEdge;
    psd->mX = aLeftEdge;
    psd->mRightEdge = aRightEdge;
    psd->mBaseline = aBaseline;

    psd->mNoWrap =
      !aSpanReflowState->frame->GetStyleText()->WhiteSpaceCanWrap();
    psd->mDirection = aSpanReflowState->mStyleVisibility->mDirection;
    psd->mChangedFrameDirection = PR_FALSE;

    // Switch to new span
    mCurrentSpan = psd;
    mSpanDepth++;
  }
  return rv;
}

nscoord
nsLineLayout::EndSpan(nsIFrame* aFrame)
{
  NS_ASSERTION(mSpanDepth > 0, "end-span without begin-span");
#ifdef NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth);
  nsFrame::ListTag(stdout, aFrame);
  printf(": EndSpan width=%d\n", mCurrentSpan->mX - mCurrentSpan->mLeftEdge);
#endif
  PerSpanData* psd = mCurrentSpan;
  nscoord widthResult = psd->mLastFrame ? (psd->mX - psd->mLeftEdge) : 0;

  mSpanDepth--;
  mCurrentSpan->mReflowState = nsnull;  // no longer valid so null it out!
  mCurrentSpan = mCurrentSpan->mParent;
  return widthResult;
}

PRInt32
nsLineLayout::GetCurrentSpanCount() const
{
  NS_ASSERTION(mCurrentSpan == mRootSpan, "bad linelayout user");
  PRInt32 count = 0;
  PerFrameData* pfd = mRootSpan->mFirstFrame;
  while (nsnull != pfd) {
    count++;
    pfd = pfd->mNext;
  }
  return count;
}

void
nsLineLayout::SplitLineTo(PRInt32 aNewCount)
{
  NS_ASSERTION(mCurrentSpan == mRootSpan, "bad linelayout user");

#ifdef REALLY_NOISY_PUSHING
  printf("SplitLineTo %d (current count=%d); before:\n", aNewCount,
         GetCurrentSpanCount());
  DumpPerSpanData(mRootSpan, 1);
#endif
  PerSpanData* psd = mRootSpan;
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    if (--aNewCount == 0) {
      // Truncate list at pfd (we keep pfd, but anything following is freed)
      PerFrameData* next = pfd->mNext;
      pfd->mNext = nsnull;
      psd->mLastFrame = pfd;

      // Now release all of the frames following pfd
      pfd = next;
      while (nsnull != pfd) {
        next = pfd->mNext;
        pfd->mNext = mFrameFreeList;
        mFrameFreeList = pfd;
#ifdef DEBUG
        mFramesFreed++;
#endif
        if (nsnull != pfd->mSpan) {
          FreeSpan(pfd->mSpan);
        }
        pfd = next;
      }
      break;
    }
    pfd = pfd->mNext;
  }
#ifdef NOISY_PUSHING
  printf("SplitLineTo %d (current count=%d); after:\n", aNewCount,
         GetCurrentSpanCount());
  DumpPerSpanData(mRootSpan, 1);
#endif
}

void
nsLineLayout::PushFrame(nsIFrame* aFrame)
{
  PerSpanData* psd = mCurrentSpan;
  NS_ASSERTION(psd->mLastFrame->mFrame == aFrame, "pushing non-last frame");

#ifdef REALLY_NOISY_PUSHING
  nsFrame::IndentBy(stdout, mSpanDepth);
  printf("PushFrame %p, before:\n", psd);
  DumpPerSpanData(psd, 1);
#endif

  // Take the last frame off of the span's frame list
  PerFrameData* pfd = psd->mLastFrame;
  if (pfd == psd->mFirstFrame) {
    // We are pushing away the only frame...empty the list
    psd->mFirstFrame = nsnull;
    psd->mLastFrame = nsnull;
  }
  else {
    PerFrameData* prevFrame = pfd->mPrev;
    prevFrame->mNext = nsnull;
    psd->mLastFrame = prevFrame;
  }

  // Now free it, and if it has a span, free that too
  pfd->mNext = mFrameFreeList;
  mFrameFreeList = pfd;
#ifdef DEBUG
  mFramesFreed++;
#endif
  if (nsnull != pfd->mSpan) {
    FreeSpan(pfd->mSpan);
  }
#ifdef NOISY_PUSHING
  nsFrame::IndentBy(stdout, mSpanDepth);
  printf("PushFrame: %p after:\n", psd);
  DumpPerSpanData(psd, 1);
#endif
}

void
nsLineLayout::FreeSpan(PerSpanData* psd)
{
  // Free its frames
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    if (nsnull != pfd->mSpan) {
      FreeSpan(pfd->mSpan);
    }
    PerFrameData* next = pfd->mNext;
    pfd->mNext = mFrameFreeList;
    mFrameFreeList = pfd;
#ifdef DEBUG
    mFramesFreed++;
#endif
    pfd = next;
  }

  // Now put the span on the free list since it's free too
  psd->mNextFreeSpan = mSpanFreeList;
  mSpanFreeList = psd;
#ifdef DEBUG
  mSpansFreed++;
#endif
}

PRBool
nsLineLayout::IsZeroHeight()
{
  PerSpanData* psd = mCurrentSpan;
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    if (0 != pfd->mBounds.height) {
      return PR_FALSE;
    }
    pfd = pfd->mNext;
  }
  return PR_TRUE;
}

nsresult
nsLineLayout::NewPerFrameData(PerFrameData** aResult)
{
  PerFrameData* pfd = mFrameFreeList;
  if (nsnull == pfd) {
    void *mem;
    PL_ARENA_ALLOCATE(mem, &mArena, sizeof(PerFrameData));
    if (nsnull == mem) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    pfd = reinterpret_cast<PerFrameData*>(mem);
  }
  else {
    mFrameFreeList = pfd->mNext;
  }
  pfd->mSpan = nsnull;
  pfd->mNext = nsnull;
  pfd->mPrev = nsnull;
  pfd->mFrame = nsnull;
  pfd->mFlags = 0;  // all flags default to false

#ifdef DEBUG
  pfd->mVerticalAlign = 0xFF;
  mFramesAllocated++;
#endif
  *aResult = pfd;
  return NS_OK;
}

PRBool
nsLineLayout::LineIsBreakable() const
{
  // XXX mTotalPlacedFrames should go away and we should just use
  // LL_LINEISEMPTY here instead
  if ((0 != mTotalPlacedFrames) || GetFlag(LL_IMPACTEDBYFLOATS)) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

// Checks all four sides for percentage units.  This means it should
// only be used for things (margin, padding) where percentages on top
// and bottom depend on the *width* just like percentages on left and
// right.
static PRBool
HasPercentageUnitSide(const nsStyleSides& aSides)
{
  NS_FOR_CSS_SIDES(side) {
    if (aSides.Get(side).HasPercent())
      return PR_TRUE;
  }
  return PR_FALSE;
}

static PRBool
IsPercentageAware(const nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "null frame is not allowed");

  nsIAtom *fType = aFrame->GetType();
  if (fType == nsGkAtoms::textFrame) {
    // None of these things can ever be true for text frames.
    return PR_FALSE;
  }

  // Some of these things don't apply to non-replaced inline frames
  // (that is, fType == nsGkAtoms::inlineFrame || fType ==
  // nsGkAtoms::positionedInlineFrame), but we won't bother making
  // things unnecessarily complicated, since they'll probably be set
  // quite rarely.

  const nsStyleMargin* margin = aFrame->GetStyleMargin();
  if (HasPercentageUnitSide(margin->mMargin)) {
    return PR_TRUE;
  }

  const nsStylePadding* padding = aFrame->GetStylePadding();
  if (HasPercentageUnitSide(padding->mPadding)) {
    return PR_TRUE;
  }

  // Note that borders can't be aware of percentages

  const nsStylePosition* pos = aFrame->GetStylePosition();

  if ((pos->WidthDependsOnContainer() &&
       pos->mWidth.GetUnit() != eStyleUnit_Auto) ||
      pos->MaxWidthDependsOnContainer() ||
      pos->MinWidthDependsOnContainer() ||
      pos->OffsetHasPercent(NS_SIDE_RIGHT) ||
      pos->OffsetHasPercent(NS_SIDE_LEFT)) {
    return PR_TRUE;
  }

  if (eStyleUnit_Auto == pos->mWidth.GetUnit()) {
    // We need to check for frames that shrink-wrap when they're auto
    // width.
    const nsStyleDisplay* disp = aFrame->GetStyleDisplay();
    if (disp->mDisplay == NS_STYLE_DISPLAY_INLINE_BLOCK ||
        disp->mDisplay == NS_STYLE_DISPLAY_INLINE_TABLE ||
        fType == nsGkAtoms::HTMLButtonControlFrame ||
        fType == nsGkAtoms::gfxButtonControlFrame ||
        fType == nsGkAtoms::fieldSetFrame ||
        fType == nsGkAtoms::comboboxDisplayFrame) {
      return PR_TRUE;
    }

    // Per CSS 2.1, section 10.3.2:
    //   If 'height' and 'width' both have computed values of 'auto' and
    //   the element has an intrinsic ratio but no intrinsic height or
    //   width and the containing block's width does not itself depend
    //   on the replaced element's width, then the used value of 'width'
    //   is calculated from the constraint equation used for
    //   block-level, non-replaced elements in normal flow. 
    nsIFrame *f = const_cast<nsIFrame*>(aFrame);
    if (f->GetIntrinsicRatio() != nsSize(0, 0) &&
        // Some percents are treated like 'auto', so check != coord
        pos->mHeight.GetUnit() != eStyleUnit_Coord) {
      const nsIFrame::IntrinsicSize &intrinsicSize = f->GetIntrinsicSize();
      if (intrinsicSize.width.GetUnit() == eStyleUnit_None &&
          intrinsicSize.height.GetUnit() == eStyleUnit_None) {
        return PR_TRUE;
      }
    }
  }

  return PR_FALSE;
}

nsresult
nsLineLayout::ReflowFrame(nsIFrame* aFrame,
                          nsReflowStatus& aReflowStatus,
                          nsHTMLReflowMetrics* aMetrics,
                          PRBool& aPushedFrame)
{
  // Initialize OUT parameter
  aPushedFrame = PR_FALSE;

  PerFrameData* pfd;
  nsresult rv = NewPerFrameData(&pfd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  PerSpanData* psd = mCurrentSpan;
  psd->AppendFrame(pfd);

#ifdef REALLY_NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth);
  printf("%p: Begin ReflowFrame pfd=%p ", psd, pfd);
  nsFrame::ListTag(stdout, aFrame);
  printf("\n");
#endif

  mTextJustificationNumSpaces = 0;
  mTextJustificationNumLetters = 0;

  // Stash copies of some of the computed state away for later
  // (vertical alignment, for example)
  pfd->mFrame = aFrame;

  // NOTE: While the x coordinate remains relative to the parent span,
  // the y coordinate is fixed at the top edge for the line. During
  // VerticalAlignFrames we will repair this so that the y coordinate
  // is properly set and relative to the appropriate span.
  pfd->mBounds.x = psd->mX;
  pfd->mBounds.y = mTopEdge;

  // We want to guarantee that we always make progress when
  // formatting. Therefore, if the object being placed on the line is
  // too big for the line, but it is the only thing on the line and is not
  // impacted by a float, then we go ahead and place it anyway. (If the line
  // is impacted by one or more floats, then it is safe to break because
  // we can move the line down below float(s).)
  //
  // Capture this state *before* we reflow the frame in case it clears
  // the state out. We need to know how to treat the current frame
  // when breaking.
  PRBool notSafeToBreak = LineIsEmpty() && !GetFlag(LL_IMPACTEDBYFLOATS);

  // Figure out whether we're talking about a textframe here
  nsIAtom* frameType = aFrame->GetType();
  PRBool isText = frameType == nsGkAtoms::textFrame;
  
  // Compute the available size for the frame. This available width
  // includes room for the side margins.
  // For now, set the available height to unconstrained always.
  nsSize availSize(mBlockReflowState->ComputedWidth(), NS_UNCONSTRAINEDSIZE);

  // Inline-ish and text-ish things don't compute their width;
  // everything else does.  We need to give them an available width that
  // reflects the space left on the line.
  NS_WARN_IF_FALSE(psd->mRightEdge != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
  nscoord availableSpaceOnLine = psd->mRightEdge - psd->mX;

  // Setup reflow state for reflowing the frame
  Maybe<nsHTMLReflowState> reflowStateHolder;
  if (!isText) {
    reflowStateHolder.construct(mPresContext, *psd->mReflowState,
                                aFrame, availSize);
    nsHTMLReflowState& reflowState = reflowStateHolder.ref();
    reflowState.mLineLayout = this;
    reflowState.mFlags.mIsTopOfPage = GetFlag(LL_ISTOPOFPAGE);
    if (reflowState.ComputedWidth() == NS_UNCONSTRAINEDSIZE)
      reflowState.availableWidth = availableSpaceOnLine;
    pfd->mMargin = reflowState.mComputedMargin;
    pfd->mBorderPadding = reflowState.mComputedBorderPadding;
    pfd->SetFlag(PFD_RELATIVEPOS,
                 (reflowState.mStyleDisplay->mPosition == NS_STYLE_POSITION_RELATIVE));
    if (pfd->GetFlag(PFD_RELATIVEPOS)) {
      pfd->mOffsets = reflowState.mComputedOffsets;
    }

    // Apply start margins (as appropriate) to the frame computing the
    // new starting x,y coordinates for the frame.
    ApplyStartMargin(pfd, reflowState);
  } else {
    pfd->mMargin.SizeTo(0, 0, 0, 0);
    pfd->mBorderPadding.SizeTo(0, 0, 0, 0);
    pfd->mOffsets.SizeTo(0, 0, 0, 0);
    // Text reflow doesn't look at the dirty bits on the frame being reflowed,
    // so no need to propagate NS_FRAME_IS_DIRTY from the parent.
  }

  // See if this frame depends on the width of its containing block.  If
  // so, disable resize reflow optimizations for the line.  (Note that,
  // to be conservative, we do this if we *try* to fit a frame on a
  // line, even if we don't succeed.)  (Note also that we can only make
  // this IsPercentageAware check *after* we've constructed our
  // nsHTMLReflowState, because that construction may be what forces aFrame
  // to lazily initialize its (possibly-percent-valued) intrinsic size.)
  if (GetFlag(LL_GOTLINEBOX) && IsPercentageAware(aFrame)) {
    mLineBox->DisableResizeReflowOptimization();
  }

  // Let frame know that are reflowing it. Note that we don't bother
  // positioning the frame yet, because we're probably going to end up
  // moving it when we do the vertical alignment
  aFrame->WillReflow(mPresContext);

  // Adjust spacemanager coordinate system for the frame.
  nsHTMLReflowMetrics metrics;
#ifdef DEBUG
  metrics.width = nscoord(0xdeadbeef);
  metrics.height = nscoord(0xdeadbeef);
#endif
  nscoord tx = pfd->mBounds.x;
  nscoord ty = pfd->mBounds.y;
  mFloatManager->Translate(tx, ty);

  PRInt32 savedOptionalBreakOffset;
  gfxBreakPriority savedOptionalBreakPriority;
  nsIContent* savedOptionalBreakContent =
    GetLastOptionalBreakPosition(&savedOptionalBreakOffset,
                                 &savedOptionalBreakPriority);

  if (!isText) {
    rv = aFrame->Reflow(mPresContext, metrics, reflowStateHolder.ref(),
                        aReflowStatus);
    if (NS_FAILED(rv)) {
      NS_WARNING( "Reflow of frame failed in nsLineLayout" );
      return rv;
    }
  } else {
    static_cast<nsTextFrame*>(aFrame)->
      ReflowText(*this, availableSpaceOnLine, psd->mReflowState->rendContext,
                 psd->mReflowState->mFlags.mBlinks, metrics, aReflowStatus);
  }
  
  pfd->mJustificationNumSpaces = mTextJustificationNumSpaces;
  pfd->mJustificationNumLetters = mTextJustificationNumLetters;

  // See if the frame is a placeholderFrame and if it is process
  // the float. At the same time, check if the frame has any non-collapsed-away
  // content.
  PRBool placedFloat = PR_FALSE;
  PRBool isEmpty;
  if (!frameType) {
    isEmpty = pfd->mFrame->IsEmpty();
  } else {
    if (nsGkAtoms::placeholderFrame == frameType) {
      isEmpty = PR_TRUE;
      pfd->SetFlag(PFD_SKIPWHENTRIMMINGWHITESPACE, PR_TRUE);
      nsIFrame* outOfFlowFrame = nsLayoutUtils::GetFloatFromPlaceholder(aFrame);
      if (outOfFlowFrame) {
        // Add mTrimmableWidth to the available width since if the line ends
        // here, the width of the inline content will be reduced by
        // mTrimmableWidth.
        nscoord availableWidth = psd->mRightEdge - (psd->mX - mTrimmableWidth);
        if (psd->mNoWrap) {
          // If we place floats after inline content where there's
          // no break opportunity, we don't know how much additional
          // width is required for the non-breaking content after the float,
          // so we can't know whether the float plus that content will fit
          // on the line. So for now, don't place floats after inline
          // content where there's no break opportunity. This is incorrect
          // but hopefully rare. Fixing it will require significant
          // restructuring of line layout.
          // We might as well allow zero-width floats to be placed, though.
          availableWidth = 0;
        }
        placedFloat = AddFloat(outOfFlowFrame, availableWidth);
        NS_ASSERTION(!(outOfFlowFrame->GetType() == nsGkAtoms::letterFrame &&
                       GetFirstLetterStyleOK()),
                    "FirstLetterStyle set on line with floating first letter");
      }
    }
    else if (isText) {
      // Note non-empty text-frames for inline frame compatibility hackery
      pfd->SetFlag(PFD_ISTEXTFRAME, PR_TRUE);
      nsTextFrame* textFrame = static_cast<nsTextFrame*>(pfd->mFrame);
      isEmpty = !textFrame->HasNoncollapsedCharacters();
      if (!isEmpty) {
        pfd->SetFlag(PFD_ISNONEMPTYTEXTFRAME, PR_TRUE);
        nsIContent* content = textFrame->GetContent();

        const nsTextFragment* frag = content->GetText();
        if (frag) {
          pfd->SetFlag(PFD_ISNONWHITESPACETEXTFRAME,
                       !content->TextIsOnlyWhitespace());
        }
      }
    }
    else if (nsGkAtoms::brFrame == frameType) {
      pfd->SetFlag(PFD_SKIPWHENTRIMMINGWHITESPACE, PR_TRUE);
      isEmpty = PR_FALSE;
    } else {
      if (nsGkAtoms::letterFrame==frameType) {
        pfd->SetFlag(PFD_ISLETTERFRAME, PR_TRUE);
      }
      if (pfd->mSpan) {
        isEmpty = !pfd->mSpan->mHasNonemptyContent && pfd->mFrame->IsSelfEmpty();
      } else {
        isEmpty = pfd->mFrame->IsEmpty();
      }
    }
  }

  mFloatManager->Translate(-tx, -ty);

  NS_ASSERTION(metrics.width>=0, "bad width");
  NS_ASSERTION(metrics.height>=0,"bad height");
  if (metrics.width<0) metrics.width=0;
  if (metrics.height<0) metrics.height=0;

#ifdef DEBUG
  // Note: break-before means ignore the reflow metrics since the
  // frame will be reflowed another time.
  if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    if (CRAZY_WIDTH(metrics.width) || CRAZY_HEIGHT(metrics.height)) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" metrics=%d,%d!\n", metrics.width, metrics.height);
    }
    if ((metrics.width == nscoord(0xdeadbeef)) ||
        (metrics.height == nscoord(0xdeadbeef))) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" didn't set w/h %d,%d!\n", metrics.width, metrics.height);
    }
  }
#endif

  // Unlike with non-inline reflow, the overflow area here does *not*
  // include the accumulation of the frame's bounds and its inline
  // descendants' bounds. Nor does it include the outline area; it's
  // just the union of the bounds of any absolute children. That is
  // added in later by nsLineLayout::ReflowInlineFrames.
  pfd->mOverflowAreas = metrics.mOverflowAreas;

  pfd->mBounds.width = metrics.width;
  pfd->mBounds.height = metrics.height;

  // Size the frame, but |RelativePositionFrames| will size the view.
  aFrame->SetSize(nsSize(metrics.width, metrics.height));

  // Tell the frame that we're done reflowing it
  aFrame->DidReflow(mPresContext,
                    isText ? nsnull : reflowStateHolder.addr(),
                    NS_FRAME_REFLOW_FINISHED);

  if (aMetrics) {
    *aMetrics = metrics;
  }

  if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    // If frame is complete and has a next-in-flow, we need to delete
    // them now. Do not do this when a break-before is signaled because
    // the frame is going to get reflowed again (and may end up wanting
    // a next-in-flow where it ends up).
    if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
      nsIFrame* kidNextInFlow = aFrame->GetNextInFlow();
      if (nsnull != kidNextInFlow) {
        // Remove all of the childs next-in-flows. Make sure that we ask
        // the right parent to do the removal (it's possible that the
        // parent is not this because we are executing pullup code)
        nsHTMLContainerFrame* parent = static_cast<nsHTMLContainerFrame*>
                                                  (kidNextInFlow->GetParent());
        parent->DeleteNextInFlowChild(mPresContext, kidNextInFlow, PR_TRUE);
      }
    }

    // Check whether this frame breaks up text runs. All frames break up text
    // runs (hence return false here) except for text frames and inline containers.
    PRBool continuingTextRun = aFrame->CanContinueTextRun();
    
    // Clear any residual mTrimmableWidth if this isn't a text frame
    if (!continuingTextRun && !pfd->GetFlag(PFD_SKIPWHENTRIMMINGWHITESPACE)) {
      mTrimmableWidth = 0;
    }

    // See if we can place the frame. If we can't fit it, then we
    // return now.
    PRBool optionalBreakAfterFits;
    NS_ASSERTION(isText ||
                 reflowStateHolder.ref().mStyleDisplay->mFloats ==
                   NS_STYLE_FLOAT_NONE,
                 "How'd we get a floated inline frame? "
                 "The frame ctor should've dealt with this.");
    // Direction is inherited, so using the psd direction is fine.
    // Get it off the reflow state instead of the frame to save style
    // data computation (especially for the text).
    PRUint8 direction =
      isText ? psd->mReflowState->mStyleVisibility->mDirection :
               reflowStateHolder.ref().mStyleVisibility->mDirection;
    if (CanPlaceFrame(pfd, direction, notSafeToBreak, continuingTextRun,
                      savedOptionalBreakContent != nsnull, metrics,
                      aReflowStatus, &optionalBreakAfterFits)) {
      if (!isEmpty) {
        psd->mHasNonemptyContent = PR_TRUE;
        SetFlag(LL_LINEISEMPTY, PR_FALSE);
        if (!pfd->mSpan) {
          // nonempty leaf content has been placed
          SetFlag(LL_LINEATSTART, PR_FALSE);
        }
      }

      // Place the frame, updating aBounds with the final size and
      // location.  Then apply the bottom+right margins (as
      // appropriate) to the frame.
      PlaceFrame(pfd, metrics);
      PerSpanData* span = pfd->mSpan;
      if (span) {
        // The frame we just finished reflowing is an inline
        // container.  It needs its child frames vertically aligned,
        // so do most of it now.
        VerticalAlignFrames(span);
      }
      
      if (!continuingTextRun) {
        if (!psd->mNoWrap && (!LineIsEmpty() || placedFloat)) {
          // record soft break opportunity after this content that can't be
          // part of a text run. This is not a text frame so we know
          // that offset PR_INT32_MAX means "after the content".
          if (NotifyOptionalBreakPosition(aFrame->GetContent(), PR_INT32_MAX, optionalBreakAfterFits, eNormalBreak)) {
            // If this returns true then we are being told to actually break here.
            aReflowStatus = NS_INLINE_LINE_BREAK_AFTER(aReflowStatus);
          }
        }
      }
    }
    else {
      PushFrame(aFrame);
      aPushedFrame = PR_TRUE;
      // Undo any saved break positions that the frame might have told us about,
      // since we didn't end up placing it
      RestoreSavedBreakPosition(savedOptionalBreakContent,
                                savedOptionalBreakOffset,
                                savedOptionalBreakPriority);
    }
  }
  else {
    PushFrame(aFrame);
  }
  
#ifdef REALLY_NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth);
  printf("End ReflowFrame ");
  nsFrame::ListTag(stdout, aFrame);
  printf(" status=%x\n", aReflowStatus);
#endif

  return rv;
}

void
nsLineLayout::ApplyStartMargin(PerFrameData* pfd,
                               nsHTMLReflowState& aReflowState)
{
  NS_ASSERTION(aReflowState.mStyleDisplay->mFloats == NS_STYLE_FLOAT_NONE,
               "How'd we get a floated inline frame? "
               "The frame ctor should've dealt with this.");

  // XXXwaterson probably not the right way to get this; e.g., embeddings, etc.
  PRBool ltr = (NS_STYLE_DIRECTION_LTR == aReflowState.mStyleVisibility->mDirection);

  // Only apply start-margin on the first-in flow for inline frames,
  // and make sure to not apply it to any inline other than the first
  // in an ib split.  Note that the ib special sibling annotations
  // only live on the first continuation, but we don't want to apply
  // the start margin for later continuations anyway.
  if (pfd->mFrame->GetPrevContinuation() ||
      nsLayoutUtils::FrameIsNonFirstInIBSplit(pfd->mFrame)) {
    // Zero this out so that when we compute the max-element-width of
    // the frame we will properly avoid adding in the starting margin.
    if (ltr)
      pfd->mMargin.left = 0;
    else
      pfd->mMargin.right = 0;
  }
  else {
    pfd->mBounds.x += ltr ? pfd->mMargin.left : pfd->mMargin.right;

    NS_WARN_IF_FALSE(NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth,
                     "have unconstrained width; this should only result from "
                     "very large sizes, not attempts at intrinsic width "
                     "calculation");
    if (NS_UNCONSTRAINEDSIZE == aReflowState.ComputedWidth()) {
      // For inline-ish and text-ish things (which don't compute widths
      // in the reflow state), adjust available width to account for the
      // left margin. The right margin will be accounted for when we
      // finish flowing the frame.
      aReflowState.availableWidth -= ltr ? pfd->mMargin.left : pfd->mMargin.right;
    }
  }
}

nscoord
nsLineLayout::GetCurrentFrameXDistanceFromBlock()
{
  PerSpanData* psd;
  nscoord x = 0;
  for (psd = mCurrentSpan; psd; psd = psd->mParent) {
    x += psd->mX;
  }
  return x;
}

/**
 * See if the frame can be placed now that we know it's desired size.
 * We can always place the frame if the line is empty. Note that we
 * know that the reflow-status is not a break-before because if it was
 * ReflowFrame above would have returned false, preventing this method
 * from being called. The logic in this method assumes that.
 *
 * Note that there is no check against the Y coordinate because we
 * assume that the caller will take care of that.
 */
PRBool
nsLineLayout::CanPlaceFrame(PerFrameData* pfd,
                            PRUint8 aFrameDirection,
                            PRBool aNotSafeToBreak,
                            PRBool aFrameCanContinueTextRun,
                            PRBool aCanRollBackBeforeFrame,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus,
                            PRBool* aOptionalBreakAfterFits)
{
  NS_PRECONDITION(pfd && pfd->mFrame, "bad args, null pointers for frame data");
  
  *aOptionalBreakAfterFits = PR_TRUE;
  // Compute right margin to use
  if (0 != pfd->mBounds.width) {
    // XXXwaterson this is probably not exactly right; e.g., embeddings, etc.
    PRBool ltr = (NS_STYLE_DIRECTION_LTR == aFrameDirection);

    /*
     * We want to only apply the end margin if we're the last continuation and
     * either not in an {ib} split or the last inline in it.  In all other
     * cases we want to zero it out.  That means zeroing it out if any of these
     * conditions hold:
     * 1) The frame is not complete (in this case it will get a next-in-flow)
     * 2) The frame is complete but has a non-fluid continuation on its
     *    continuation chain.  Note that if it has a fluid continuation, that
     *    continuation will get destroyed later, so we don't want to drop the
     *    end-margin in that case.
     * 3) The frame is in an {ib} split and is not the last part.
     *
     * However, none of that applies if this is a letter frame (XXXbz why?)
     */
    if ((NS_FRAME_IS_NOT_COMPLETE(aStatus) ||
         pfd->mFrame->GetLastInFlow()->GetNextContinuation() ||
         nsLayoutUtils::FrameIsNonLastInIBSplit(pfd->mFrame))
        && !pfd->GetFlag(PFD_ISLETTERFRAME)) {
      if (ltr)
        pfd->mMargin.right = 0;
      else
        pfd->mMargin.left = 0;
    }
  }
  else {
    // Don't apply margin to empty frames.
    pfd->mMargin.left = pfd->mMargin.right = 0;
  }

  PerSpanData* psd = mCurrentSpan;
  if (psd->mNoWrap) {
    // When wrapping is off, everything fits.
    return PR_TRUE;
  }

  PRBool ltr = NS_STYLE_DIRECTION_LTR == aFrameDirection;
  nscoord endMargin = ltr ? pfd->mMargin.right : pfd->mMargin.left;

#ifdef NOISY_CAN_PLACE_FRAME
  if (nsnull != psd->mFrame) {
    nsFrame::ListTag(stdout, psd->mFrame->mFrame);
  }
  else {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
  } 
  printf(": aNotSafeToBreak=%s frame=", aNotSafeToBreak ? "true" : "false");
  nsFrame::ListTag(stdout, pfd->mFrame);
  printf(" frameWidth=%d\n", pfd->mBounds.XMost() + endMargin - psd->mX);
#endif

  // Set outside to PR_TRUE if the result of the reflow leads to the
  // frame sticking outside of our available area.
  PRBool outside = pfd->mBounds.XMost() - mTrimmableWidth + endMargin > psd->mRightEdge;
  if (!outside) {
    // If it fits, it fits
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> inside\n");
#endif
    return PR_TRUE;
  }
  *aOptionalBreakAfterFits = PR_FALSE;

  // When it doesn't fit, check for a few special conditions where we
  // allow it to fit anyway.
  if (0 == pfd->mMargin.left + pfd->mBounds.width + pfd->mMargin.right) {
    // Empty frames always fit right where they are
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> empty frame fits\n");
#endif
    return PR_TRUE;
  }

#ifdef FIX_BUG_50257
  // another special case:  always place a BR
  if (nsGkAtoms::brFrame == pfd->mFrame->GetType()) {
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> BR frame fits\n");
#endif
    return PR_TRUE;
  }
#endif

  if (aNotSafeToBreak) {
    // There are no frames on the line that take up width and the line is
    // not impacted by floats, so we must allow the current frame to be
    // placed on the line
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> not-safe and not-impacted fits: ");
    while (nsnull != psd) {
      printf("<psd=%p x=%d left=%d> ", psd, psd->mX, psd->mLeftEdge);
      psd = psd->mParent;
    }
    printf("\n");
#endif
    return PR_TRUE;
  }
 
  // Special check for span frames
  if (pfd->mSpan && pfd->mSpan->mContainsFloat) {
    // If the span either directly or indirectly contains a float then
    // it fits. Why? It's kind of complicated, but here goes:
    //
    // 1. CanPlaceFrame is used for all frame placements on a line,
    // and in a span. This includes recursively placement of frames
    // inside of spans, and the span itself. Because the logic always
    // checks for room before proceeding (the code above here), the
    // only things on a line will be those things that "fit".
    //
    // 2. Before a float is placed on a line, the line has to be empty
    // (otherwise it's a "below current line" float and will be placed
    // after the line).
    //
    // Therefore, if the span directly or indirectly has a float
    // then it means that at the time of the placement of the float
    // the line was empty. Because of #1, only the frames that fit can
    // be added after that point, therefore we can assume that the
    // current span being placed has fit.
    //
    // So how do we get here and have a span that should already fit
    // and yet doesn't: Simple: span's that have the no-wrap attribute
    // set on them and contain a float and are placed where they
    // don't naturally fit.
    return PR_TRUE;
 }

  if (aFrameCanContinueTextRun) {
    // Let it fit, but we reserve the right to roll back.
    // Note that we usually won't get here because a text frame will break
    // itself to avoid exceeding the available width.
    // We'll only get here for text frames that couldn't break early enough.
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> placing overflowing textrun, requesting backup\n");
#endif

    // We will want to try backup.
    SetFlag(LL_NEEDBACKUP, PR_TRUE);
    return PR_TRUE;
  }

#ifdef NOISY_CAN_PLACE_FRAME
  printf("   ==> didn't fit\n");
#endif
  aStatus = NS_INLINE_LINE_BREAK_BEFORE();
  return PR_FALSE;
}

/**
 * Place the frame. Update running counters.
 */
void
nsLineLayout::PlaceFrame(PerFrameData* pfd, nsHTMLReflowMetrics& aMetrics)
{
  // If frame is zero width then do not apply its left and right margins.
  PerSpanData* psd = mCurrentSpan;
  PRBool emptyFrame = PR_FALSE;
  if ((0 == pfd->mBounds.width) && (0 == pfd->mBounds.height)) {
    pfd->mBounds.x = psd->mX;
    pfd->mBounds.y = mTopEdge;
    emptyFrame = PR_TRUE;
  }

  // Record ascent and update max-ascent and max-descent values
  if (aMetrics.ascent == nsHTMLReflowMetrics::ASK_FOR_BASELINE)
    pfd->mAscent = pfd->mFrame->GetBaseline();
  else
    pfd->mAscent = aMetrics.ascent;

  PRBool ltr = (NS_STYLE_DIRECTION_LTR == pfd->mFrame->GetStyleVisibility()->mDirection);
  // Advance to next X coordinate
  psd->mX = pfd->mBounds.XMost() + (ltr ? pfd->mMargin.right : pfd->mMargin.left);

  // Count the number of non-empty frames on the line...
  if (!emptyFrame) {
    mTotalPlacedFrames++;
  }
}

nsresult
nsLineLayout::AddBulletFrame(nsIFrame* aFrame,
                             const nsHTMLReflowMetrics& aMetrics)
{
  NS_ASSERTION(mCurrentSpan == mRootSpan, "bad linelayout user");
  NS_ASSERTION(GetFlag(LL_GOTLINEBOX), "must have line box");


  nsIFrame *blockFrame = mBlockReflowState->frame;
  NS_ASSERTION(blockFrame->IsFrameOfType(nsIFrame::eBlockFrame),
               "must be for block");
  if (!static_cast<nsBlockFrame*>(blockFrame)->BulletIsEmpty()) {
    SetFlag(LL_HASBULLET, PR_TRUE);
    mLineBox->SetHasBullet();
  }

  PerFrameData* pfd;
  nsresult rv = NewPerFrameData(&pfd);
  if (NS_SUCCEEDED(rv)) {
    mRootSpan->AppendFrame(pfd);
    pfd->mFrame = aFrame;
    pfd->mMargin.SizeTo(0, 0, 0, 0);
    pfd->mBorderPadding.SizeTo(0, 0, 0, 0);
    pfd->mFlags = 0;  // all flags default to false
    pfd->SetFlag(PFD_ISBULLET, PR_TRUE);
    if (aMetrics.ascent == nsHTMLReflowMetrics::ASK_FOR_BASELINE)
      pfd->mAscent = aFrame->GetBaseline();
    else
      pfd->mAscent = aMetrics.ascent;

    // Note: y value will be updated during vertical alignment
    pfd->mBounds = aFrame->GetRect();
    pfd->mOverflowAreas = aMetrics.mOverflowAreas;
  }
  return rv;
}

#ifdef DEBUG
void
nsLineLayout::DumpPerSpanData(PerSpanData* psd, PRInt32 aIndent)
{
  nsFrame::IndentBy(stdout, aIndent);
  printf("%p: left=%d x=%d right=%d\n", static_cast<void*>(psd),
         psd->mLeftEdge, psd->mX, psd->mRightEdge);
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    nsFrame::IndentBy(stdout, aIndent+1);
    nsFrame::ListTag(stdout, pfd->mFrame);
    printf(" %d,%d,%d,%d\n", pfd->mBounds.x, pfd->mBounds.y,
           pfd->mBounds.width, pfd->mBounds.height);
    if (pfd->mSpan) {
      DumpPerSpanData(pfd->mSpan, aIndent + 1);
    }
    pfd = pfd->mNext;
  }
}
#endif

#define VALIGN_OTHER  0
#define VALIGN_TOP    1
#define VALIGN_BOTTOM 2

void
nsLineLayout::VerticalAlignLine()
{
  // Synthesize a PerFrameData for the block frame
  PerFrameData rootPFD;
  rootPFD.mFrame = mBlockReflowState->frame;
  rootPFD.mAscent = 0;
  mRootSpan->mFrame = &rootPFD;

  // Partially place the children of the block frame. The baseline for
  // this operation is set to zero so that the y coordinates for all
  // of the placed children will be relative to there.
  PerSpanData* psd = mRootSpan;
  VerticalAlignFrames(psd);

  // Compute the line-height. The line-height will be the larger of:
  //
  // [1] maxY - minY (the distance between the highest childs top edge
  // and the lowest childs bottom edge)
  //
  // [2] the maximum logical box height (since not every frame may have
  // participated in #1; for example: top/bottom aligned frames)
  //
  // [3] the minimum line height (line-height property set on the
  // block frame)
  nscoord lineHeight = psd->mMaxY - psd->mMinY;

  // Now that the line-height is computed, we need to know where the
  // baseline is in the line. Position baseline so that mMinY is just
  // inside the top of the line box.
  nscoord baselineY;
  if (psd->mMinY < 0) {
    baselineY = mTopEdge - psd->mMinY;
  }
  else {
    baselineY = mTopEdge;
  }

  // It's also possible that the line-height isn't tall enough because
  // of top/bottom aligned elements that were not accounted for in
  // min/max Y.
  //
  // The CSS2 spec doesn't really say what happens when to the
  // baseline in this situations. What we do is if the largest top
  // aligned box height is greater than the line-height then we leave
  // the baseline alone. If the largest bottom aligned box is greater
  // than the line-height then we slide the baseline down by the extra
  // amount.
  //
  // Navigator 4 gives precedence to the first top/bottom aligned
  // object.  We just let bottom aligned objects win.
  if (lineHeight < mMaxBottomBoxHeight) {
    // When the line is shorter than the maximum top aligned box
    nscoord extra = mMaxBottomBoxHeight - lineHeight;
    baselineY += extra;
    lineHeight = mMaxBottomBoxHeight;
  }
  if (lineHeight < mMaxTopBoxHeight) {
    lineHeight = mMaxTopBoxHeight;
  }
#ifdef NOISY_VERTICAL_ALIGN
  printf("  [line]==> lineHeight=%d baselineY=%d\n", lineHeight, baselineY);
#endif

  // Now position all of the frames in the root span. We will also
  // recurse over the child spans and place any top/bottom aligned
  // frames we find.
  // XXX PERFORMANCE: set a bit per-span to avoid the extra work
  // (propagate it upward too)
  for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
    if (pfd->mVerticalAlign == VALIGN_OTHER) {
      pfd->mBounds.y += baselineY;
      pfd->mFrame->SetRect(pfd->mBounds);
    }
  }
  PlaceTopBottomFrames(psd, -mTopEdge, lineHeight);

  // If the frame being reflowed has text decorations, we simulate the
  // propagation of those decorations to a line-level element by storing the
  // offset in a frame property on any child frames that are vertically-aligned
  // somewhere other than the baseline. This property is then used by
  // nsTextFrame::GetTextDecorations when the same conditions are met.
  if (rootPFD.mFrame->GetStyleContext()->HasTextDecorationLines()) {
    for (const PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
      const nsIFrame *const f = pfd->mFrame;
      const nsStyleCoord& vAlign =
          f->GetStyleContext()->GetStyleTextReset()->mVerticalAlign;

      if (vAlign.GetUnit() != eStyleUnit_Enumerated ||
          vAlign.GetIntValue() != NS_STYLE_VERTICAL_ALIGN_BASELINE) {
        const nscoord offset = baselineY - (pfd->mBounds.y);
        f->Properties().Set(nsIFrame::LineBaselineOffset(),
                            NS_INT32_TO_PTR(offset));
      }
    }
  }

  // Fill in returned line-box and max-element-width data
  mLineBox->mBounds.x = psd->mLeftEdge;
  mLineBox->mBounds.y = mTopEdge;
  mLineBox->mBounds.width = psd->mX - psd->mLeftEdge;
  mLineBox->mBounds.height = lineHeight;
  mFinalLineHeight = lineHeight;
  mLineBox->SetAscent(baselineY - mTopEdge);
#ifdef NOISY_VERTICAL_ALIGN
  printf(
    "  [line]==> bounds{x,y,w,h}={%d,%d,%d,%d} lh=%d a=%d\n",
    mLineBox->mBounds.x, mLineBox->mBounds.y,
    mLineBox->mBounds.width, mLineBox->mBounds.height,
    mFinalLineHeight, mLineBox->GetAscent());
#endif

  // Undo root-span mFrame pointer to prevent brane damage later on...
  mRootSpan->mFrame = nsnull;
}

void
nsLineLayout::PlaceTopBottomFrames(PerSpanData* psd,
                                   nscoord aDistanceFromTop,
                                   nscoord aLineHeight)
{
  for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
    PerSpanData* span = pfd->mSpan;
#ifdef DEBUG
    NS_ASSERTION(0xFF != pfd->mVerticalAlign, "umr");
#endif
    switch (pfd->mVerticalAlign) {
      case VALIGN_TOP:
        if (span) {
          pfd->mBounds.y = -aDistanceFromTop - span->mMinY;
        }
        else {
          pfd->mBounds.y = -aDistanceFromTop + pfd->mMargin.top;
        }
        pfd->mFrame->SetRect(pfd->mBounds);
#ifdef NOISY_VERTICAL_ALIGN
        printf("    ");
        nsFrame::ListTag(stdout, pfd->mFrame);
        printf(": y=%d dTop=%d [bp.top=%d topLeading=%d]\n",
               pfd->mBounds.y, aDistanceFromTop,
               span ? pfd->mBorderPadding.top : 0,
               span ? span->mTopLeading : 0);
#endif
        break;
      case VALIGN_BOTTOM:
        if (span) {
          // Compute bottom leading
          pfd->mBounds.y = -aDistanceFromTop + aLineHeight - span->mMaxY;
        }
        else {
          pfd->mBounds.y = -aDistanceFromTop + aLineHeight -
            pfd->mMargin.bottom - pfd->mBounds.height;
        }
        pfd->mFrame->SetRect(pfd->mBounds);
#ifdef NOISY_VERTICAL_ALIGN
        printf("    ");
        nsFrame::ListTag(stdout, pfd->mFrame);
        printf(": y=%d\n", pfd->mBounds.y);
#endif
        break;
    }
    if (span) {
      nscoord distanceFromTop = aDistanceFromTop + pfd->mBounds.y;
      PlaceTopBottomFrames(span, distanceFromTop, aLineHeight);
    }
  }
}

#define VERTICAL_ALIGN_FRAMES_NO_MINIMUM nscoord_MAX
#define VERTICAL_ALIGN_FRAMES_NO_MAXIMUM nscoord_MIN

// Vertically place frames within a given span. Note: this doesn't
// place top/bottom aligned frames as those have to wait until the
// entire line box height is known. This is called after the span
// frame has finished being reflowed so that we know its height.
void
nsLineLayout::VerticalAlignFrames(PerSpanData* psd)
{
  // Get parent frame info
  PerFrameData* spanFramePFD = psd->mFrame;
  nsIFrame* spanFrame = spanFramePFD->mFrame;

  // Get the parent frame's font for all of the frames in this span
  nsStyleContext* styleContext = spanFrame->GetStyleContext();
  nsRenderingContext* rc = mBlockReflowState->rendContext;
  nsLayoutUtils::SetFontFromStyle(mBlockReflowState->rendContext, styleContext);
  nsFontMetrics* fm = rc->FontMetrics();

  PRBool preMode = mStyleText->WhiteSpaceIsSignificant();

  // See if the span is an empty continuation. It's an empty continuation iff:
  // - it has a prev-in-flow
  // - it has no next in flow
  // - it's zero sized
  PRBool emptyContinuation = psd != mRootSpan &&
    spanFrame->GetPrevInFlow() && !spanFrame->GetNextInFlow() &&
    (0 == spanFramePFD->mBounds.width) && (0 == spanFramePFD->mBounds.height);

#ifdef NOISY_VERTICAL_ALIGN
  printf("[%sSpan]", (psd == mRootSpan)?"Root":"");
  nsFrame::ListTag(stdout, spanFrame);
  printf(": preMode=%s strictMode=%s w/h=%d,%d emptyContinuation=%s",
         preMode ? "yes" : "no",
         mPresContext->CompatibilityMode() != eCompatibility_NavQuirks ? "yes" : "no",
         spanFramePFD->mBounds.width, spanFramePFD->mBounds.height,
         emptyContinuation ? "yes" : "no");
  if (psd != mRootSpan) {
    printf(" bp=%d,%d,%d,%d margin=%d,%d,%d,%d",
           spanFramePFD->mBorderPadding.top,
           spanFramePFD->mBorderPadding.right,
           spanFramePFD->mBorderPadding.bottom,
           spanFramePFD->mBorderPadding.left,
           spanFramePFD->mMargin.top,
           spanFramePFD->mMargin.right,
           spanFramePFD->mMargin.bottom,
           spanFramePFD->mMargin.left);
  }
  printf("\n");
#endif

  // Compute the span's mZeroEffectiveSpanBox flag. What we are trying
  // to determine is how we should treat the span: should it act
  // "normally" according to css2 or should it effectively
  // "disappear".
  //
  // In general, if the document being processed is in full standards
  // mode then it should act normally (with one exception). The
  // exception case is when a span is continued and yet the span is
  // empty (e.g. compressed whitespace). For this kind of span we treat
  // it as if it were not there so that it doesn't impact the
  // line-height.
  //
  // In almost standards mode or quirks mode, we should sometimes make
  // it disappear. The cases that matter are those where the span
  // contains no real text elements that would provide an ascent and
  // descent and height. However, if css style elements have been
  // applied to the span (border/padding/margin) so that it's clear the
  // document author is intending css2 behavior then we act as if strict
  // mode is set.
  //
  // This code works correctly for preMode, because a blank line
  // in PRE mode is encoded as a text node with a LF in it, since
  // text nodes with only whitespace are considered in preMode.
  //
  // Much of this logic is shared with the various implementations of
  // nsIFrame::IsEmpty since they need to duplicate the way it makes
  // some lines empty.  However, nsIFrame::IsEmpty can't be reused here
  // since this code sets zeroEffectiveSpanBox even when there are
  // non-empty children.
  PRBool zeroEffectiveSpanBox = PR_FALSE;
  // XXXldb If we really have empty continuations, then all these other
  // checks don't make sense for them.
  // XXXldb This should probably just use nsIFrame::IsSelfEmpty, assuming that
  // it agrees with this code.  (If it doesn't agree, it probably should.)
  if ((emptyContinuation ||
       mPresContext->CompatibilityMode() != eCompatibility_FullStandards) &&
      ((psd == mRootSpan) ||
       ((0 == spanFramePFD->mBorderPadding.top) &&
        (0 == spanFramePFD->mBorderPadding.right) &&
        (0 == spanFramePFD->mBorderPadding.bottom) &&
        (0 == spanFramePFD->mBorderPadding.left) &&
        (0 == spanFramePFD->mMargin.top) &&
        (0 == spanFramePFD->mMargin.right) &&
        (0 == spanFramePFD->mMargin.bottom) &&
        (0 == spanFramePFD->mMargin.left)))) {
    // This code handles an issue with compatibility with non-css
    // conformant browsers. In particular, there are some cases
    // where the font-size and line-height for a span must be
    // ignored and instead the span must *act* as if it were zero
    // sized. In general, if the span contains any non-compressed
    // text then we don't use this logic.
    // However, this is not propagated outwards, since (in compatibility
    // mode) we don't want big line heights for things like
    // <p><font size="-1">Text</font></p>

    // We shouldn't include any whitespace that collapses, unless we're
    // preformatted (in which case it shouldn't, but the width=0 test is
    // perhaps incorrect).  This includes whitespace at the beginning of
    // a line and whitespace preceded (?) by other whitespace.
    // See bug 134580 and bug 155333.
    zeroEffectiveSpanBox = PR_TRUE;
    for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
      if (pfd->GetFlag(PFD_ISTEXTFRAME) &&
          (pfd->GetFlag(PFD_ISNONWHITESPACETEXTFRAME) || preMode ||
           pfd->mBounds.width != 0)) {
        zeroEffectiveSpanBox = PR_FALSE;
        break;
      }
    }
  }
  psd->mZeroEffectiveSpanBox = zeroEffectiveSpanBox;

  // Setup baselineY, minY, and maxY
  nscoord baselineY, minY, maxY;
  if (psd == mRootSpan) {
    // Use a zero baselineY since we don't yet know where the baseline
    // will be (until we know how tall the line is; then we will
    // know). In addition, use extreme values for the minY and maxY
    // values so that only the child frames will impact their values
    // (since these are children of the block, there is no span box to
    // provide initial values).
    baselineY = 0;
    minY = VERTICAL_ALIGN_FRAMES_NO_MINIMUM;
    maxY = VERTICAL_ALIGN_FRAMES_NO_MAXIMUM;
#ifdef NOISY_VERTICAL_ALIGN
    printf("[RootSpan]");
    nsFrame::ListTag(stdout, spanFrame);
    printf(": pass1 valign frames: topEdge=%d minLineHeight=%d zeroEffectiveSpanBox=%s\n",
           mTopEdge, mMinLineHeight,
           zeroEffectiveSpanBox ? "yes" : "no");
#endif
  }
  else {
    // Compute the logical height for this span. The logical height
    // is based on the line-height value, not the font-size. Also
    // compute the top leading.
    nscoord logicalHeight = nsHTMLReflowState::
      CalcLineHeight(spanFrame->GetStyleContext(),
                     mBlockReflowState->ComputedHeight());
    nscoord contentHeight = spanFramePFD->mBounds.height -
      spanFramePFD->mBorderPadding.top - spanFramePFD->mBorderPadding.bottom;

    // Special-case for a ::first-letter frame, set the line height to
    // the frame height if the user has left line-height == normal 
    if (spanFramePFD->GetFlag(PFD_ISLETTERFRAME) &&
        !spanFrame->GetPrevInFlow() &&
        spanFrame->GetStyleText()->mLineHeight.GetUnit() == eStyleUnit_Normal) {
      logicalHeight = spanFramePFD->mBounds.height;
    }

    nscoord leading = logicalHeight - contentHeight;
    psd->mTopLeading = leading / 2;
    psd->mBottomLeading = leading - psd->mTopLeading;
    psd->mLogicalHeight = logicalHeight;

    if (zeroEffectiveSpanBox) {
      // When the span-box is to be ignored, zero out the initial
      // values so that the span doesn't impact the final line
      // height. The contents of the span can impact the final line
      // height.

      // Note that things are readjusted for this span after its children
      // are reflowed
      minY = VERTICAL_ALIGN_FRAMES_NO_MINIMUM;
      maxY = VERTICAL_ALIGN_FRAMES_NO_MAXIMUM;
    }
    else {

      // The initial values for the min and max Y values are in the spans
      // coordinate space, and cover the logical height of the span. If
      // there are child frames in this span that stick out of this area
      // then the minY and maxY are updated by the amount of logical
      // height that is outside this range.
      minY = spanFramePFD->mBorderPadding.top - psd->mTopLeading;
      maxY = minY + psd->mLogicalHeight;
    }

    // This is the distance from the top edge of the parents visual
    // box to the baseline. The span already computed this for us,
    // so just use it.
    *psd->mBaseline = baselineY = spanFramePFD->mAscent;


#ifdef NOISY_VERTICAL_ALIGN
    printf("[%sSpan]", (psd == mRootSpan)?"Root":"");
    nsFrame::ListTag(stdout, spanFrame);
    printf(": baseLine=%d logicalHeight=%d topLeading=%d h=%d bp=%d,%d zeroEffectiveSpanBox=%s\n",
           baselineY, psd->mLogicalHeight, psd->mTopLeading,
           spanFramePFD->mBounds.height,
           spanFramePFD->mBorderPadding.top, spanFramePFD->mBorderPadding.bottom,
           zeroEffectiveSpanBox ? "yes" : "no");
#endif
  }

  nscoord maxTopBoxHeight = 0;
  nscoord maxBottomBoxHeight = 0;
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    nsIFrame* frame = pfd->mFrame;

    // sanity check (see bug 105168, non-reproducible crashes from null frame)
    NS_ASSERTION(frame, "null frame in PerFrameData - something is very very bad");
    if (!frame) {
      return;
    }

    // Compute the logical height of the frame
    nscoord logicalHeight;
    nscoord topLeading;
    PerSpanData* frameSpan = pfd->mSpan;
    if (frameSpan) {
      // For span frames the logical-height and top-leading was
      // pre-computed when the span was reflowed.
      logicalHeight = frameSpan->mLogicalHeight;
      topLeading = frameSpan->mTopLeading;
    }
    else {
      // For other elements the logical height is the same as the
      // frames height plus its margins.
      logicalHeight = pfd->mBounds.height + pfd->mMargin.top +
        pfd->mMargin.bottom;
      topLeading = 0;
    }

    // Get vertical-align property
    const nsStyleCoord& verticalAlign =
      frame->GetStyleTextReset()->mVerticalAlign;
#ifdef NOISY_VERTICAL_ALIGN
    printf("  [frame]");
    nsFrame::ListTag(stdout, frame);
    printf(": verticalAlignUnit=%d (enum == %d)\n",
           verticalAlignUnit,
           ((eStyleUnit_Enumerated == verticalAlign.GetUnit())
            ? verticalAlign.GetIntValue()
            : -1));
#endif

    if (verticalAlign.GetUnit() == eStyleUnit_Enumerated) {
      switch (verticalAlign.GetIntValue()) {
        default:
        case NS_STYLE_VERTICAL_ALIGN_BASELINE:
        {
          // The element's baseline is aligned with the baseline of
          // the parent.
          pfd->mBounds.y = baselineY - pfd->mAscent;
          pfd->mVerticalAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_SUB:
        {
          // Lower the baseline of the box to the subscript offset
          // of the parent's box. This is identical to the baseline
          // alignment except for the addition of the subscript
          // offset to the baseline Y.
          nscoord parentSubscript = fm->SubscriptOffset();
          nscoord revisedBaselineY = baselineY + parentSubscript;
          pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
          pfd->mVerticalAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_SUPER:
        {
          // Raise the baseline of the box to the superscript offset
          // of the parent's box. This is identical to the baseline
          // alignment except for the subtraction of the superscript
          // offset to the baseline Y.
          nscoord parentSuperscript = fm->SuperscriptOffset();
          nscoord revisedBaselineY = baselineY - parentSuperscript;
          pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
          pfd->mVerticalAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_TOP:
        {
          pfd->mVerticalAlign = VALIGN_TOP;
          nscoord subtreeHeight = logicalHeight;
          if (frameSpan) {
            subtreeHeight = frameSpan->mMaxY - frameSpan->mMinY;
            NS_ASSERTION(subtreeHeight >= logicalHeight,
                         "unexpected subtree height");
          }
          if (subtreeHeight > maxTopBoxHeight) {
            maxTopBoxHeight = subtreeHeight;
          }
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
        {
          pfd->mVerticalAlign = VALIGN_BOTTOM;
          nscoord subtreeHeight = logicalHeight;
          if (frameSpan) {
            subtreeHeight = frameSpan->mMaxY - frameSpan->mMinY;
            NS_ASSERTION(subtreeHeight >= logicalHeight,
                         "unexpected subtree height");
          }
          if (subtreeHeight > maxBottomBoxHeight) {
            maxBottomBoxHeight = subtreeHeight;
          }
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
        {
          // Align the midpoint of the frame with 1/2 the parents
          // x-height above the baseline.
          nscoord parentXHeight = fm->XHeight();
          if (frameSpan) {
            pfd->mBounds.y = baselineY -
              (parentXHeight + pfd->mBounds.height)/2;
          }
          else {
            pfd->mBounds.y = baselineY - (parentXHeight + logicalHeight)/2 +
              pfd->mMargin.top;
          }
          pfd->mVerticalAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_TEXT_TOP:
        {
          // The top of the logical box is aligned with the top of
          // the parent element's text.
          nscoord parentAscent = fm->MaxAscent();
          if (frameSpan) {
            pfd->mBounds.y = baselineY - parentAscent -
              pfd->mBorderPadding.top + frameSpan->mTopLeading;
          }
          else {
            pfd->mBounds.y = baselineY - parentAscent + pfd->mMargin.top;
          }
          pfd->mVerticalAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM:
        {
          // The bottom of the logical box is aligned with the
          // bottom of the parent elements text.
          nscoord parentDescent = fm->MaxDescent();
          if (frameSpan) {
            pfd->mBounds.y = baselineY + parentDescent -
              pfd->mBounds.height + pfd->mBorderPadding.bottom -
              frameSpan->mBottomLeading;
          }
          else {
            pfd->mBounds.y = baselineY + parentDescent -
              pfd->mBounds.height - pfd->mMargin.bottom;
          }
          pfd->mVerticalAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE:
        {
          // Align the midpoint of the frame with the baseline of the parent.
          if (frameSpan) {
            pfd->mBounds.y = baselineY - pfd->mBounds.height/2;
          }
          else {
            pfd->mBounds.y = baselineY - logicalHeight/2 + pfd->mMargin.top;
          }
          pfd->mVerticalAlign = VALIGN_OTHER;
          break;
        }
      }
    } else {
      // We have either a coord, a percent, or a calc().
      nscoord pctBasis = 0;
      if (verticalAlign.HasPercent()) {
        // Percentages are like lengths, except treated as a percentage
        // of the elements line-height value.
        pctBasis = nsHTMLReflowState::CalcLineHeight(
          frame->GetStyleContext(), mBlockReflowState->ComputedHeight());
      }
      nscoord offset =
        nsRuleNode::ComputeCoordPercentCalc(verticalAlign, pctBasis);
      // According to the CSS2 spec (10.8.1), a positive value
      // "raises" the box by the given distance while a negative value
      // "lowers" the box by the given distance (with zero being the
      // baseline). Since Y coordinates increase towards the bottom of
      // the screen we reverse the sign.
      nscoord revisedBaselineY = baselineY - offset;
      pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
      pfd->mVerticalAlign = VALIGN_OTHER;
    }

    // Update minY/maxY for frames that we just placed. Do not factor
    // text into the equation.
    if (pfd->mVerticalAlign == VALIGN_OTHER) {
      // Text frames do not contribute to the min/max Y values for the
      // line (instead their parent frame's font-size contributes).
      // XXXrbs -- relax this restriction because it causes text frames
      //           to jam together when 'font-size-adjust' is enabled
      //           and layout is using dynamic font heights (bug 20394)
      //        -- Note #1: With this code enabled and with the fact that we are not
      //           using Em[Ascent|Descent] as nsDimensions for text metrics in
      //           GFX mean that the discussion in bug 13072 cannot hold.
      //        -- Note #2: We still don't want empty-text frames to interfere.
      //           For example in quirks mode, avoiding empty text frames prevents
      //           "tall" lines around elements like <hr> since the rules of <hr>
      //           in quirks.css have pseudo text contents with LF in them.
#if 0
      if (!pfd->GetFlag(PFD_ISTEXTFRAME)) {
#else
      // Only consider non empty text frames when line-height=normal
      PRBool canUpdate = !pfd->GetFlag(PFD_ISTEXTFRAME);
      if (!canUpdate && pfd->GetFlag(PFD_ISNONWHITESPACETEXTFRAME)) {
        canUpdate =
          frame->GetStyleText()->mLineHeight.GetUnit() == eStyleUnit_Normal;
      }
      if (canUpdate) {
#endif
        nscoord yTop, yBottom;
        if (frameSpan) {
          // For spans that were are now placing, use their position
          // plus their already computed min-Y and max-Y values for
          // computing yTop and yBottom.
          yTop = pfd->mBounds.y + frameSpan->mMinY;
          yBottom = pfd->mBounds.y + frameSpan->mMaxY;
        }
        else {
          yTop = pfd->mBounds.y - pfd->mMargin.top;
          yBottom = yTop + logicalHeight;
        }
        if (!preMode &&
            mPresContext->CompatibilityMode() != eCompatibility_FullStandards &&
            !logicalHeight) {
          // Check if it's a BR frame that is not alone on its line (it
          // is given a height of zero to indicate this), and if so reset
          // yTop and yBottom so that BR frames don't influence the line.
          if (nsGkAtoms::brFrame == frame->GetType()) {
            yTop = VERTICAL_ALIGN_FRAMES_NO_MINIMUM;
            yBottom = VERTICAL_ALIGN_FRAMES_NO_MAXIMUM;
          }
        }
        if (yTop < minY) minY = yTop;
        if (yBottom > maxY) maxY = yBottom;
#ifdef NOISY_VERTICAL_ALIGN
        printf("     [frame]raw: a=%d h=%d bp=%d,%d logical: h=%d leading=%d y=%d minY=%d maxY=%d\n",
               pfd->mAscent, pfd->mBounds.height,
               pfd->mBorderPadding.top, pfd->mBorderPadding.bottom,
               logicalHeight,
               pfd->mSpan ? topLeading : 0,
               pfd->mBounds.y, minY, maxY);
#endif
      }
      if (psd != mRootSpan) {
        frame->SetRect(pfd->mBounds);
      }
    }
    pfd = pfd->mNext;
  }

  // Factor in the minimum line-height when handling the root-span for
  // the block.
  if (psd == mRootSpan) {
    // We should factor in the block element's minimum line-height (as
    // defined in section 10.8.1 of the css2 spec) assuming that
    // mZeroEffectiveSpanBox is not set on the root span.  This only happens
    // in some cases in quirks mode:
    //  (1) if the root span contains non-whitespace text directly (this
    //      is handled by mZeroEffectiveSpanBox
    //  (2) if this line has a bullet
    //  (3) if this is the last line of an LI, DT, or DD element
    //      (The last line before a block also counts, but not before a
    //      BR) (NN4/IE5 quirk)

    // (1) and (2) above
    PRBool applyMinLH = !psd->mZeroEffectiveSpanBox || GetFlag(LL_HASBULLET);
    PRBool isLastLine = (!mLineBox->IsLineWrapped() && !GetFlag(LL_LINEENDSINBR));
    if (!applyMinLH && isLastLine) {
      nsIContent* blockContent = mRootSpan->mFrame->mFrame->GetContent();
      if (blockContent) {
        nsIAtom *blockTagAtom = blockContent->Tag();
        // (3) above, if the last line of LI, DT, or DD
        if (blockTagAtom == nsGkAtoms::li ||
            blockTagAtom == nsGkAtoms::dt ||
            blockTagAtom == nsGkAtoms::dd) {
          applyMinLH = PR_TRUE;
        }
      }
    }
    if (applyMinLH) {
      if (psd->mHasNonemptyContent || preMode || GetFlag(LL_HASBULLET)) {
#ifdef NOISY_VERTICAL_ALIGN
        printf("  [span]==> adjusting min/maxY: currentValues: %d,%d", minY, maxY);
#endif
        nscoord minimumLineHeight = mMinLineHeight;
        nscoord yTop =
          -nsLayoutUtils::GetCenteredFontBaseline(fm, minimumLineHeight);
        nscoord yBottom = yTop + minimumLineHeight;

        if (yTop < minY) minY = yTop;
        if (yBottom > maxY) maxY = yBottom;

#ifdef NOISY_VERTICAL_ALIGN
        printf(" new values: %d,%d\n", minY, maxY);
#endif
#ifdef NOISY_VERTICAL_ALIGN
        printf("            Used mMinLineHeight: %d, fontHeight: %d, fontAscent: %d\n", mMinLineHeight, fontHeight, fontAscent);
#endif
      }
      else {
        // XXX issues:
        // [1] BR's on empty lines stop working
        // [2] May not honor css2's notion of handling empty elements
        // [3] blank lines in a pre-section ("\n") (handled with preMode)

        // XXX Are there other problems with this?
#ifdef NOISY_VERTICAL_ALIGN
        printf("  [span]==> zapping min/maxY: currentValues: %d,%d newValues: 0,0\n",
               minY, maxY);
#endif
        minY = maxY = 0;
      }
    }
  }

  if ((minY == VERTICAL_ALIGN_FRAMES_NO_MINIMUM) ||
      (maxY == VERTICAL_ALIGN_FRAMES_NO_MAXIMUM)) {
    minY = maxY = baselineY;
  }

  if ((psd != mRootSpan) && (psd->mZeroEffectiveSpanBox)) {
#ifdef NOISY_VERTICAL_ALIGN
    printf("   [span]adjusting for zeroEffectiveSpanBox\n");
    printf("     Original: minY=%d, maxY=%d, height=%d, ascent=%d, logicalHeight=%d, topLeading=%d, bottomLeading=%d\n",
           minY, maxY, spanFramePFD->mBounds.height,
           spanFramePFD->mAscent,
           psd->mLogicalHeight, psd->mTopLeading, psd->mBottomLeading);
#endif
    nscoord goodMinY = spanFramePFD->mBorderPadding.top - psd->mTopLeading;
    nscoord goodMaxY = goodMinY + psd->mLogicalHeight;
    if (minY > goodMinY) {
      nscoord adjust = minY - goodMinY; // positive

      // shrink the logical extents
      psd->mLogicalHeight -= adjust;
      psd->mTopLeading -= adjust;
    }
    if (maxY < goodMaxY) {
      nscoord adjust = goodMaxY - maxY;
      psd->mLogicalHeight -= adjust;
      psd->mBottomLeading -= adjust;
    }
    if (minY > 0) {

      // shrink the content by moving its top down.  This is tricky, since
      // the top is the 0 for many coordinates, so what we do is
      // move everything else up.
      spanFramePFD->mAscent -= minY; // move the baseline up
      spanFramePFD->mBounds.height -= minY; // move the bottom up
      psd->mTopLeading += minY;
      *psd->mBaseline -= minY;

      pfd = psd->mFirstFrame;
      while (nsnull != pfd) {
        pfd->mBounds.y -= minY; // move all the children back up
        pfd->mFrame->SetRect(pfd->mBounds);
        pfd = pfd->mNext;
      }
      maxY -= minY; // since minY is in the frame's own coordinate system
      minY = 0;
    }
    if (maxY < spanFramePFD->mBounds.height) {
      nscoord adjust = spanFramePFD->mBounds.height - maxY;
      spanFramePFD->mBounds.height -= adjust; // move the bottom up
      psd->mBottomLeading += adjust;
    }
#ifdef NOISY_VERTICAL_ALIGN
    printf("     New: minY=%d, maxY=%d, height=%d, ascent=%d, logicalHeight=%d, topLeading=%d, bottomLeading=%d\n",
           minY, maxY, spanFramePFD->mBounds.height,
           spanFramePFD->mAscent,
           psd->mLogicalHeight, psd->mTopLeading, psd->mBottomLeading);
#endif
  }

  psd->mMinY = minY;
  psd->mMaxY = maxY;
#ifdef NOISY_VERTICAL_ALIGN
  printf("  [span]==> minY=%d maxY=%d delta=%d maxTopBoxHeight=%d maxBottomBoxHeight=%d\n",
         minY, maxY, maxY - minY, maxTopBoxHeight, maxBottomBoxHeight);
#endif
  if (maxTopBoxHeight > mMaxTopBoxHeight) {
    mMaxTopBoxHeight = maxTopBoxHeight;
  }
  if (maxBottomBoxHeight > mMaxBottomBoxHeight) {
    mMaxBottomBoxHeight = maxBottomBoxHeight;
  }
}

static void SlideSpanFrameRect(nsIFrame* aFrame, nscoord aDeltaWidth)
{
  nsRect r = aFrame->GetRect();
  r.x -= aDeltaWidth;
  aFrame->SetRect(r);
}

PRBool
nsLineLayout::TrimTrailingWhiteSpaceIn(PerSpanData* psd,
                                       nscoord* aDeltaWidth)
{
#ifndef IBMBIDI
// XXX what about NS_STYLE_DIRECTION_RTL?
  if (NS_STYLE_DIRECTION_RTL == psd->mDirection) {
    *aDeltaWidth = 0;
    return PR_TRUE;
  }
#endif

  PerFrameData* pfd = psd->mFirstFrame;
  if (!pfd) {
    *aDeltaWidth = 0;
    return PR_FALSE;
  }
  pfd = pfd->Last();
  while (nsnull != pfd) {
#ifdef REALLY_NOISY_TRIM
    nsFrame::ListTag(stdout, (psd == mRootSpan
                              ? mBlockReflowState->frame
                              : psd->mFrame->mFrame));
    printf(": attempting trim of ");
    nsFrame::ListTag(stdout, pfd->mFrame);
    printf("\n");
#endif
    PerSpanData* childSpan = pfd->mSpan;
    if (childSpan) {
      // Maybe the child span has the trailing white-space in it?
      if (TrimTrailingWhiteSpaceIn(childSpan, aDeltaWidth)) {
        nscoord deltaWidth = *aDeltaWidth;
        if (deltaWidth) {
          // Adjust the child spans frame size
          pfd->mBounds.width -= deltaWidth;
          if (psd != mRootSpan) {
            // When the child span is not a direct child of the block
            // we need to update the child spans frame rectangle
            // because it most likely will not be done again. Spans
            // that are direct children of the block will be updated
            // later, however, because the VerticalAlignFrames method
            // will be run after this method.
            nsIFrame* f = pfd->mFrame;
            nsRect r = f->GetRect();
            r.width -= deltaWidth;
            f->SetRect(r);
          }

          // Adjust the right edge of the span that contains the child span
          psd->mX -= deltaWidth;

          // Slide any frames that follow the child span over by the
          // right amount. The only thing that can follow the child
          // span is empty stuff, so we are just making things
          // sensible (keeping the combined area honest).
          while (pfd->mNext) {
            pfd = pfd->mNext;
            pfd->mBounds.x -= deltaWidth;
            if (psd != mRootSpan) {
              // When the child span is not a direct child of the block
              // we need to update the child spans frame rectangle
              // because it most likely will not be done again. Spans
              // that are direct children of the block will be updated
              // later, however, because the VerticalAlignFrames method
              // will be run after this method.
              SlideSpanFrameRect(pfd->mFrame, deltaWidth);
            }
          }
        }
        return PR_TRUE;
      }
    }
    else if (!pfd->GetFlag(PFD_ISTEXTFRAME) &&
             !pfd->GetFlag(PFD_SKIPWHENTRIMMINGWHITESPACE)) {
      // If we hit a frame on the end that's not text and not a placeholder,
      // then there is no trailing whitespace to trim. Stop the search.
      *aDeltaWidth = 0;
      return PR_TRUE;
    }
    else if (pfd->GetFlag(PFD_ISTEXTFRAME)) {
      // Call TrimTrailingWhiteSpace even on empty textframes because they
      // might have a soft hyphen which should now appear, changing the frame's
      // width
      nsTextFrame::TrimOutput trimOutput = static_cast<nsTextFrame*>(pfd->mFrame)->
          TrimTrailingWhiteSpace(mBlockReflowState->rendContext);
#ifdef NOISY_TRIM
      nsFrame::ListTag(stdout, (psd == mRootSpan
                                ? mBlockReflowState->frame
                                : psd->mFrame->mFrame));
      printf(": trim of ");
      nsFrame::ListTag(stdout, pfd->mFrame);
      printf(" returned %d\n", trimOutput.mDeltaWidth);
#endif
      if (trimOutput.mLastCharIsJustifiable && pfd->mJustificationNumSpaces > 0) {
        pfd->mJustificationNumSpaces--;
      }
      
      if (trimOutput.mChanged) {
        pfd->SetFlag(PFD_RECOMPUTEOVERFLOW, PR_TRUE);
      }

      if (trimOutput.mDeltaWidth) {
        pfd->mBounds.width -= trimOutput.mDeltaWidth;

        // See if the text frame has already been placed in its parent
        if (psd != mRootSpan) {
          // The frame was already placed during psd's
          // reflow. Update the frames rectangle now.
          pfd->mFrame->SetRect(pfd->mBounds);
        }

        // Adjust containing span's right edge
        psd->mX -= trimOutput.mDeltaWidth;

        // Slide any frames that follow the text frame over by the
        // right amount. The only thing that can follow the text
        // frame is empty stuff, so we are just making things
        // sensible (keeping the combined area honest).
        while (pfd->mNext) {
          pfd = pfd->mNext;
          pfd->mBounds.x -= trimOutput.mDeltaWidth;
          if (psd != mRootSpan) {
            // When the child span is not a direct child of the block
            // we need to update the child spans frame rectangle
            // because it most likely will not be done again. Spans
            // that are direct children of the block will be updated
            // later, however, because the VerticalAlignFrames method
            // will be run after this method.
            SlideSpanFrameRect(pfd->mFrame, trimOutput.mDeltaWidth);
          }
        }
      }

      if (pfd->GetFlag(PFD_ISNONEMPTYTEXTFRAME) || trimOutput.mChanged) {
        // Pass up to caller so they can shrink their span
        *aDeltaWidth = trimOutput.mDeltaWidth;
        return PR_TRUE;
      }
    }
    pfd = pfd->mPrev;
  }

  *aDeltaWidth = 0;
  return PR_FALSE;
}

PRBool
nsLineLayout::TrimTrailingWhiteSpace()
{
  PerSpanData* psd = mRootSpan;
  nscoord deltaWidth;
  TrimTrailingWhiteSpaceIn(psd, &deltaWidth);
  return 0 != deltaWidth;
}

void
nsLineLayout::ComputeJustificationWeights(PerSpanData* aPSD,
                                          PRInt32* aNumSpaces,
                                          PRInt32* aNumLetters)
{
  NS_ASSERTION(aPSD, "null arg");
  NS_ASSERTION(aNumSpaces, "null arg");
  NS_ASSERTION(aNumLetters, "null arg");
  PRInt32 numSpaces = 0;
  PRInt32 numLetters = 0;

  for (PerFrameData* pfd = aPSD->mFirstFrame; pfd != nsnull; pfd = pfd->mNext) {

    if (PR_TRUE == pfd->GetFlag(PFD_ISTEXTFRAME)) {
      numSpaces += pfd->mJustificationNumSpaces;
      numLetters += pfd->mJustificationNumLetters;
    }
    else if (pfd->mSpan != nsnull) {
      PRInt32 spanSpaces;
      PRInt32 spanLetters;

      ComputeJustificationWeights(pfd->mSpan, &spanSpaces, &spanLetters);

      numSpaces += spanSpaces;
      numLetters += spanLetters;
    }
  }

  *aNumSpaces = numSpaces;
  *aNumLetters = numLetters;
}

nscoord 
nsLineLayout::ApplyFrameJustification(PerSpanData* aPSD, FrameJustificationState* aState)
{
  NS_ASSERTION(aPSD, "null arg");
  NS_ASSERTION(aState, "null arg");

  nscoord deltaX = 0;
  for (PerFrameData* pfd = aPSD->mFirstFrame; pfd != nsnull; pfd = pfd->mNext) {
    // Don't reposition bullets (and other frames that occur out of X-order?)
    if (!pfd->GetFlag(PFD_ISBULLET)) {
      nscoord dw = 0;
      
      pfd->mBounds.x += deltaX;
      
      if (PR_TRUE == pfd->GetFlag(PFD_ISTEXTFRAME)) {
        if (aState->mTotalWidthForSpaces > 0 &&
            aState->mTotalNumSpaces > 0) {
          aState->mNumSpacesProcessed += pfd->mJustificationNumSpaces;

          nscoord newAllocatedWidthForSpaces =
            (aState->mTotalWidthForSpaces*aState->mNumSpacesProcessed)
              /aState->mTotalNumSpaces;
          
          dw += newAllocatedWidthForSpaces - aState->mWidthForSpacesProcessed;

          aState->mWidthForSpacesProcessed = newAllocatedWidthForSpaces;
        }

        if (aState->mTotalWidthForLetters > 0 &&
            aState->mTotalNumLetters > 0) {
          aState->mNumLettersProcessed += pfd->mJustificationNumLetters;

          nscoord newAllocatedWidthForLetters =
            (aState->mTotalWidthForLetters*aState->mNumLettersProcessed)
              /aState->mTotalNumLetters;
          
          dw += newAllocatedWidthForLetters - aState->mWidthForLettersProcessed;

          aState->mWidthForLettersProcessed = newAllocatedWidthForLetters;
        }
        
        if (dw) {
          pfd->SetFlag(PFD_RECOMPUTEOVERFLOW, PR_TRUE);
        }
      }
      else {
        if (nsnull != pfd->mSpan) {
          dw += ApplyFrameJustification(pfd->mSpan, aState);
        }
      }
      
      pfd->mBounds.width += dw;

      deltaX += dw;
      pfd->mFrame->SetRect(pfd->mBounds);
    }
  }
  return deltaX;
}

void
nsLineLayout::HorizontalAlignFrames(nsRect& aLineBounds,
                                    PRBool aAllowJustify)
{
  PerSpanData* psd = mRootSpan;
  NS_WARN_IF_FALSE(psd->mRightEdge != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
  nscoord availWidth = psd->mRightEdge - psd->mLeftEdge;
  nscoord remainingWidth = availWidth - aLineBounds.width;
#ifdef NOISY_HORIZONTAL_ALIGN
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": availWidth=%d lineWidth=%d delta=%d\n",
           availWidth, aLineBounds.width, remainingWidth);
#endif
  nscoord dx = 0;

  if (remainingWidth > 0) {
    switch (mTextAlign) {
      case NS_STYLE_TEXT_ALIGN_JUSTIFY:
        // If this is not the last line then go ahead and justify the
        // frames in the line.
        if (aAllowJustify) {
          PRInt32 numSpaces;
          PRInt32 numLetters;
            
          ComputeJustificationWeights(psd, &numSpaces, &numLetters);

          if (numSpaces > 0) {
            FrameJustificationState state =
              { numSpaces, numLetters, remainingWidth, 0, 0, 0, 0, 0 };

            // Apply the justification, and make sure to update our linebox
            // width to account for it.
            aLineBounds.width += ApplyFrameJustification(psd, &state);
            remainingWidth = availWidth - aLineBounds.width;
            break;
          }
        }
        // Fall through to the default case if we were told not to
        // justify anything or could not justify to fill the space.

      case NS_STYLE_TEXT_ALIGN_DEFAULT:
        if (NS_STYLE_DIRECTION_LTR == psd->mDirection) {
          // default alignment for left-to-right is left so do nothing
          break;
        }
        // Fall through to align right case for default alignment
        // used when the direction is right-to-left.

      case NS_STYLE_TEXT_ALIGN_RIGHT:
      case NS_STYLE_TEXT_ALIGN_MOZ_RIGHT:
        dx = remainingWidth;
        break;

      case NS_STYLE_TEXT_ALIGN_END:
        if (NS_STYLE_DIRECTION_LTR == psd->mDirection) {
          // Do what we do for ALIGN_RIGHT
          dx = remainingWidth;
          break;
        }
        // Fall through to align left case for end alignment
        // used when the direction is right-to-left.

      case NS_STYLE_TEXT_ALIGN_LEFT:
      case NS_STYLE_TEXT_ALIGN_MOZ_LEFT:
        break;

      case NS_STYLE_TEXT_ALIGN_CENTER:
      case NS_STYLE_TEXT_ALIGN_MOZ_CENTER:
        dx = remainingWidth / 2;
        break;
    }
  }
  else if (remainingWidth < 0) {
    if (NS_STYLE_DIRECTION_RTL == psd->mDirection) {
      dx = remainingWidth;
      psd->mX += dx;
      psd->mLeftEdge += dx;
    }
  }

  if (NS_STYLE_DIRECTION_RTL == psd->mDirection &&
      !psd->mChangedFrameDirection) {
    if (psd->mLastFrame->GetFlag(PFD_ISBULLET) ) {
      PerFrameData* bulletPfd = psd->mLastFrame;
      bulletPfd->mBounds.x -= remainingWidth;
      bulletPfd->mFrame->SetRect(bulletPfd->mBounds);
    }
    psd->mChangedFrameDirection = PR_TRUE;
  }

  if (dx) {
    for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
      pfd->mBounds.x += dx;
      pfd->mFrame->SetRect(pfd->mBounds);
    }
    aLineBounds.x += dx;
  }
}

void
nsLineLayout::RelativePositionFrames(nsOverflowAreas& aOverflowAreas)
{
  RelativePositionFrames(mRootSpan, aOverflowAreas);
}

void
nsLineLayout::RelativePositionFrames(PerSpanData* psd, nsOverflowAreas& aOverflowAreas)
{
  nsOverflowAreas overflowAreas;
  if (nsnull != psd->mFrame) {
    // The span's overflow areas come in three parts:
    // -- this frame's width and height
    // -- pfd->mOverflowAreas, which is the area of a bullet or the union
    // of a relatively positioned frame's absolute children
    // -- the bounds of all inline descendants
    // The former two parts are computed right here, we gather the descendants
    // below.
    // At this point psd->mFrame->mBounds might be out of date since
    // bidi reordering can move and resize the frames. So use the frame's
    // rect instead of mBounds.
    nsRect adjustedBounds(nsPoint(0, 0), psd->mFrame->mFrame->GetSize());

    overflowAreas.ScrollableOverflow().UnionRect(
      psd->mFrame->mOverflowAreas.ScrollableOverflow(), adjustedBounds);

    // Text-shadow overflow
    if (mPresContext->CompatibilityMode() != eCompatibility_NavQuirks) {
      nsRect shadowRect = nsLayoutUtils::GetTextShadowRectsUnion(adjustedBounds,
                                                                 psd->mFrame->mFrame);
      adjustedBounds.UnionRect(adjustedBounds, shadowRect);
    }

    // Text shadow is only part of visual overflow and not scrollable overflow.
    overflowAreas.VisualOverflow().UnionRect(
      psd->mFrame->mOverflowAreas.VisualOverflow(), adjustedBounds);
  }
  else {
    // The minimum combined area for the frames that are direct
    // children of the block starts at the upper left corner of the
    // line and is sized to match the size of the line's bounding box
    // (the same size as the values returned from VerticalAlignFrames)
    overflowAreas.VisualOverflow().x = psd->mLeftEdge;
    // If this turns out to be negative, the rect will be treated as empty.
    // Which is just fine.
    overflowAreas.VisualOverflow().width =
      psd->mX - overflowAreas.VisualOverflow().x;
    overflowAreas.VisualOverflow().y = mTopEdge;
    overflowAreas.VisualOverflow().height = mFinalLineHeight;

    overflowAreas.ScrollableOverflow() = overflowAreas.VisualOverflow();
  }

  for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
    nsIFrame* frame = pfd->mFrame;
    nsPoint origin = frame->GetPosition();

    // Adjust the origin of the frame
    if (pfd->GetFlag(PFD_RELATIVEPOS)) {
      // right and bottom are handled by
      // nsHTMLReflowState::ComputeRelativeOffsets
      nsPoint change(pfd->mOffsets.left, pfd->mOffsets.top);
      origin += change;
      frame->SetPosition(origin);
    }

    // We must position the view correctly before positioning its
    // descendants so that widgets are positioned properly (since only
    // some views have widgets).
    if (frame->HasView())
      nsContainerFrame::SyncFrameViewAfterReflow(mPresContext, frame,
        frame->GetView(), pfd->mOverflowAreas.VisualOverflow(),
        NS_FRAME_NO_SIZE_VIEW);

    // Note: the combined area of a child is in its coordinate
    // system. We adjust the childs combined area into our coordinate
    // system before computing the aggregated value by adding in
    // <b>x</b> and <b>y</b> which were computed above.
    nsOverflowAreas r;
    if (pfd->mSpan) {
      // Compute a new combined area for the child span before
      // aggregating it into our combined area.
      RelativePositionFrames(pfd->mSpan, r);
    } else {
      r = pfd->mOverflowAreas;
      if (pfd->GetFlag(PFD_ISTEXTFRAME)) {
        // We need to recompute overflow areas in two cases:
        // (1) When PFD_RECOMPUTEOVERFLOW is set due to trimming
        // (2) When there are text decorations, since we can't recompute the
        //     overflow area until Reflow and VerticalAlignLine have finished
        if (pfd->GetFlag(PFD_RECOMPUTEOVERFLOW) ||
            frame->GetStyleContext()->HasTextDecorationLines()) {
          nsTextFrame* f = static_cast<nsTextFrame*>(frame);
          r = f->RecomputeOverflow();
        }
        frame->FinishAndStoreOverflow(r, frame->GetSize());
      }

      // If we have something that's not an inline but with a complex frame
      // hierarchy inside that contains views, they need to be
      // positioned.
      // All descendant views must be repositioned even if this frame
      // does have a view in case this frame's view does not have a
      // widget and some of the descendant views do have widgets --
      // otherwise the widgets won't be repositioned.
      nsContainerFrame::PositionChildViews(frame);
    }

    // Do this here (rather than along with setting the overflow rect
    // below) so we get leaf frames as well.  No need to worry
    // about the root span, since it doesn't have a frame.
    if (frame->HasView())
      nsContainerFrame::SyncFrameViewAfterReflow(mPresContext, frame,
                                                 frame->GetView(),
                                                 r.VisualOverflow(),
                                                 NS_FRAME_NO_MOVE_VIEW);

    overflowAreas.UnionWith(r + origin);
  }

  // If we just computed a spans combined area, we need to update its
  // overflow rect...
  if (psd->mFrame) {
    PerFrameData* spanPFD = psd->mFrame;
    nsIFrame* frame = spanPFD->mFrame;
    frame->FinishAndStoreOverflow(overflowAreas, frame->GetSize());
  }
  aOverflowAreas = overflowAreas;
}
