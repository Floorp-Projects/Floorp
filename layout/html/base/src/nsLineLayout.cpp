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
#include "nsCOMPtr.h"
#include "nsLineLayout.h"
#include "nsStyleConsts.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"

#ifdef DEBUG
#undef  NOISY_HORIZONTAL_ALIGN
#undef   REALLY_NOISY_HORIZONTAL_ALIGN
#undef  NOISY_VERTICAL_ALIGN
#undef   REALLY_NOISY_VERTICAL_ALIGN
#undef  NOISY_REFLOW
#undef   REALLY_NOISY_REFLOW
#undef  NOISY_PUSHING
#undef   REALLY_NOISY_PUSHING
#define DEBUG_ADD_TEXT
#else
#undef NOISY_HORIZONTAL_ALIGN
#undef  REALLY_NOISY_HORIZONTAL_ALIGN
#undef NOISY_VERTICAL_ALIGN
#undef  REALLY_NOISY_VERTICAL_ALIGN
#undef NOISY_REFLOW
#undef  REALLY_NOISY_REFLOW
#undef NOISY_PUSHING
#undef  REALLY_NOISY_PUSHING
#undef DEBUG_ADD_TEXT
#endif

nsTextRun::nsTextRun()
{
  mNext = nsnull;
}

nsTextRun::~nsTextRun()
{
}

void
nsTextRun::List(FILE* out, PRInt32 aIndent)
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  PRInt32 n = mArray.Count();
  fprintf(out, "%p: count=%d <", this, n);
  for (i = 0; i < n; i++) {
    nsIFrame* text = (nsIFrame*) mArray.ElementAt(i);
    nsAutoString tmp;
    text->GetFrameName(tmp);
    fputs(tmp, out);
    printf("@%p ", text);
  }
  fputs(">\n", out);
}

//----------------------------------------------------------------------

#define PLACED_LEFT  0x1
#define PLACED_RIGHT 0x2

#define NUM_SPAN_DATA (sizeof(mSpanDataBuf) / sizeof(mSpanDataBuf[0]))
#define NUM_FRAME_DATA (sizeof(mFrameDataBuf) / sizeof(mFrameDataBuf[0]))

nsLineLayout::nsLineLayout(nsIPresContext& aPresContext,
                           nsISpaceManager* aSpaceManager,
                           const nsHTMLReflowState* aOuterReflowState,
                           PRBool aComputeMaxElementSize)
  : mPresContext(aPresContext),
    mSpaceManager(aSpaceManager),
    mBlockReflowState(aOuterReflowState),
    mComputeMaxElementSize(aComputeMaxElementSize)
{
  // Stash away some style data that we need
  aOuterReflowState->frame->GetStyleData(eStyleStruct_Text,
                                         (const nsStyleStruct*&) mStyleText);
  mTextAlign = mStyleText->mTextAlign;
  switch (mStyleText->mWhiteSpace) {
  case NS_STYLE_WHITESPACE_PRE:
  case NS_STYLE_WHITESPACE_NOWRAP:
    mNoWrap = PR_TRUE;
    break;
  default:
    mNoWrap = PR_FALSE;
    break;
  }
  mDirection = aOuterReflowState->mStyleDisplay->mDirection;
  mMinLineHeight = nsHTMLReflowState::CalcLineHeight(mPresContext,
                                                     aOuterReflowState->frame);
  mBRFrame = nsnull;
  mLineNumber = 0;
  mColumn = 0;
  mEndsInWhiteSpace = PR_TRUE;
  mUnderstandsWhiteSpace = PR_FALSE;
  mFirstLetterStyleOK = PR_FALSE;
  mIsTopOfPage = PR_FALSE;
  mWasInWord = PR_FALSE;
  mCanBreakBeforeFrame = PR_FALSE;
  mUpdatedBand = PR_FALSE;
  mPlacedFloaters = 0;
  mTotalPlacedFrames = 0;
  mTopEdge = mBottomEdge = 0;
  mReflowTextRuns = nsnull;
  mTextRun = nsnull;

  // XXX Do this on demand to avoid extra setup time
  // Place the baked-in per-frame-data on the frame free list
  PerFrameData** pfdp = &mFrameFreeList;
  PerFrameData* pfd = mFrameDataBuf;
  PerFrameData* endpfd = pfd + NUM_FRAME_DATA;
  while (pfd < endpfd) {
#ifdef DEBUG
    nsCRT::memset(pfd, 0xEE, sizeof(*pfd));
#endif
    *pfdp = pfd;
    pfdp = &pfd->mNext;
    pfd++;
  }
  *pfdp = nsnull;

  // XXX Do this on demand to avoid extra setup time
  // Place the baked-in per-span-data on the span free list
  PerSpanData** psdp = &mSpanFreeList;
  PerSpanData* psd = mSpanDataBuf;
  PerSpanData* endpsd = psd + NUM_SPAN_DATA;
  while (psd < endpsd) {
#ifdef DEBUG
    nsCRT::memset(psd, 0xEE, sizeof(*psd));
#endif
    *psdp = psd;
    psdp = &psd->mNext;
    psd++;
  }
  *psdp = nsnull;

  mCurrentSpan = mRootSpan = mLastSpan = nsnull;
  mSpanDepth = 0;

  mTextRuns = nsnull;
  mTextRunP = &mTextRuns;
  mNewTextRun = nsnull;
}

nsLineLayout::nsLineLayout(nsIPresContext& aPresContext)
  : mPresContext(aPresContext)
{
  mTextRuns = nsnull;
  mTextRunP = &mTextRuns;
  mNewTextRun = nsnull;
  mRootSpan = nsnull;
  mSpanFreeList = nsnull;
  mFrameFreeList = nsnull;
}

nsLineLayout::~nsLineLayout()
{
  NS_ASSERTION(nsnull == mRootSpan, "bad line-layout user");
  nsTextRun::DeleteTextRuns(mTextRuns);

  // Free up all of the per-span-data items that were allocated on the heap
  PerSpanData* psd = mSpanFreeList;
  while (nsnull != psd) {
    PerSpanData* nextSpan = psd->mNext;
    if ((psd < &mSpanDataBuf[0]) || (psd >= &mSpanDataBuf[NUM_SPAN_DATA])) {
      delete psd;
    }
    psd = nextSpan;
  }

  // Free up all of the per-frame-data items that were allocated on the heap
  PerFrameData* pfd = mFrameFreeList;
  while (nsnull != pfd) {
    PerFrameData* nextFrame = pfd->mNext;
    if ((pfd < &mFrameDataBuf[0]) || (pfd >= &mFrameDataBuf[NUM_FRAME_DATA])) {
      delete pfd;
    }
    pfd = nextFrame;
  }
}

void
nsLineLayout::BeginLineReflow(nscoord aX, nscoord aY,
                              nscoord aWidth, nscoord aHeight,
                              PRBool aIsTopOfPage)
{
  NS_ASSERTION(nsnull == mRootSpan, "bad linelayout user");
#ifdef DEBUG
  if ((aWidth > 200000) && (aWidth != NS_UNCONSTRAINEDSIZE)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": Init: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
    aWidth = NS_UNCONSTRAINEDSIZE;
  }
  if ((aHeight > 200000) && (aHeight != NS_UNCONSTRAINEDSIZE)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": Init: bad caller: height WAS %d(0x%x)\n",
           aHeight, aHeight);
    aHeight = NS_UNCONSTRAINEDSIZE;
  }
#endif
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": BeginLineReflow: %d,%d,%d,%d %s\n",
         aX, aY, aWidth, aHeight,
         aIsTopOfPage ? "top-of-page" : "");
#endif

  mBRFrame = nsnull;
  mColumn = 0;
  mEndsInWhiteSpace = PR_TRUE;
  mUnderstandsWhiteSpace = PR_FALSE;
  mFirstLetterStyleOK = PR_FALSE;
  mIsTopOfPage = aIsTopOfPage;
  mCanBreakBeforeFrame = PR_FALSE;
  mUpdatedBand = PR_FALSE;
  mPlacedFloaters = 0;
  mTotalPlacedFrames = 0;
  mSpanDepth = 0;
  mMaxTopBoxHeight = mMaxBottomBoxHeight = 0;

  ForgetWordFrames();

  PerSpanData* psd;
  NewPerSpanData(&psd);
  mCurrentSpan = mRootSpan = psd;
  psd->mReflowState = mBlockReflowState;
  psd->mLeftEdge = aX;
  psd->mX = aX;
  if (NS_UNCONSTRAINEDSIZE == aWidth) {
    psd->mRightEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    psd->mRightEdge = aX + aWidth;
  }

  mTopEdge = aY;
  if (NS_UNCONSTRAINEDSIZE == aHeight) {
    mBottomEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mBottomEdge = aY + aHeight;
  }
}

void
nsLineLayout::EndLineReflow()
{
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": EndLineReflow: width=%d\n", mRootSpan->mX - mRootSpan->mLeftEdge);
#endif

  PerSpanData* psd = mRootSpan;
  while (nsnull != psd) {
    // Put frames on free list
    PerFrameData* pfd = psd->mFirstFrame;
    while (nsnull != pfd) {
      PerFrameData* nextFrame = pfd->mNext;
      pfd->mNext = mFrameFreeList;
      mFrameFreeList = pfd;
      pfd = nextFrame;
    }

    // Put span on free list
    PerSpanData* nextSpan = psd->mNext;
    psd->mNext = mSpanFreeList;
    mSpanFreeList = psd;
    psd = nextSpan;
  }
  mCurrentSpan = mRootSpan = mLastSpan = nsnull;
}

void
nsLineLayout::UpdateBand(nscoord aX, nscoord aY,
                         nscoord aWidth, nscoord aHeight,
                         PRBool aPlacedLeftFloater)
{
  PerSpanData* psd = mRootSpan;
  NS_PRECONDITION(psd->mX == psd->mLeftEdge, "update-band called late");
#ifdef DEBUG
  if ((aWidth > 200000) && (aWidth != NS_UNCONSTRAINEDSIZE)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": UpdateBand: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
    aWidth = NS_UNCONSTRAINEDSIZE;
  }
  if ((aHeight > 200000) && (aHeight != NS_UNCONSTRAINEDSIZE)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": UpdateBand: bad caller: height WAS %d(0x%x)\n",
           aHeight, aHeight);
    aHeight = NS_UNCONSTRAINEDSIZE;
  }
#endif
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": UpdateBand: %d,%d,%d,%d %s\n",
         aX, aY, aWidth, aHeight,
         aPlacedLeftFloater ? "placed-left-floater" : "");
#endif

  psd->mLeftEdge = aX;
  psd->mX = aX;
  if (NS_UNCONSTRAINEDSIZE == aWidth) {
    psd->mRightEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    psd->mRightEdge = aX + aWidth;
  }
  mTopEdge = aY;
  if (NS_UNCONSTRAINEDSIZE == aHeight) {
    mBottomEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mBottomEdge = aY + aHeight;
  }
  mUpdatedBand = PR_TRUE;
  mPlacedFloaters |= (aPlacedLeftFloater ? PLACED_LEFT : PLACED_RIGHT);
}

void
nsLineLayout::UpdateFrames()
{
  if (NS_STYLE_DIRECTION_LTR == mDirection) {
    if (PLACED_LEFT & mPlacedFloaters) {
      // Note: Only adjust the outermost frames (the ones that are
      // direct children of the block), not the ones in the child
      // spans. The reason is simple: the frames in the spans have
      // coordinates local to their parent therefore they are moved
      // when their parent span is moved.
      PerSpanData* psd = mRootSpan;
      PerFrameData* pfd = psd->mFirstFrame;
      while (nsnull != pfd) {
        pfd->mBounds.x = psd->mX;
        pfd = pfd->mNext;
      }
    }
  }
  else if (PLACED_RIGHT & mPlacedFloaters) {
    // XXX handle DIR=right-to-left
  }
}

nsresult
nsLineLayout::NewPerSpanData(PerSpanData** aResult)
{
  PerSpanData* psd = mSpanFreeList;
  if (nsnull == psd) {
    psd = new PerSpanData;
    if (nsnull == psd) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    mSpanFreeList = psd->mNext;
  }
  psd->mParent = nsnull;
  psd->mFrame = nsnull;
  psd->mFirstFrame = nsnull;
  psd->mLastFrame = nsnull;
  psd->mNext = nsnull;

  // Link new span to the end of the span list
  if (nsnull == mLastSpan) {
    psd->mPrev = nsnull;
  }
  else {
    mLastSpan->mNext = psd;
    psd->mPrev = mLastSpan;
  }
  mLastSpan = psd;

  *aResult = psd;
  return NS_OK;
}

nsresult
nsLineLayout::BeginSpan(nsIFrame* aFrame,
                        const nsHTMLReflowState* aSpanReflowState,
                        nscoord aLeftEdge,
                        nscoord aRightEdge)
{
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

    // Switch to new span
    mCurrentSpan = psd;
    mSpanDepth++;
  }
  return rv;
}

void
nsLineLayout::EndSpan(nsIFrame* aFrame,
                      nsSize& aSizeResult,
                      nsSize* aMaxElementSize)
{
  NS_ASSERTION(mSpanDepth > 0, "end-span without begin-span");
#ifdef NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth);
  nsFrame::ListTag(stdout, aFrame);
  printf(": EndSpan width=%d\n", mCurrentSpan->mX - mCurrentSpan->mLeftEdge);
#endif
  PerSpanData* psd = mCurrentSpan;
  nscoord width = 0;
  nscoord maxHeight = 0;
  nscoord maxElementWidth = 0;
  nscoord maxElementHeight = 0;
  if (nsnull != psd->mLastFrame) {
    width = psd->mX - psd->mLeftEdge;
    PerFrameData* pfd = psd->mFirstFrame;
    while (nsnull != pfd) {
      if (pfd->mBounds.height > maxHeight) maxHeight = pfd->mBounds.height;

      // Compute max-element-size if necessary
      if (aMaxElementSize) {
        nscoord mw = pfd->mMaxElementSize.width +
          pfd->mMargin.left + pfd->mMargin.right;
        if (maxElementWidth < mw) {
          maxElementWidth = mw;
        }
        nscoord mh = pfd->mMaxElementSize.height +
          pfd->mMargin.top + pfd->mMargin.bottom;
        if (maxElementHeight < mh) {
          maxElementHeight = mh;
        }
      }
      pfd = pfd->mNext;
    }
  }
  aSizeResult.width = width;
  aSizeResult.height = maxHeight;
  if (aMaxElementSize) {
    aMaxElementSize->width = maxElementWidth;
    aMaxElementSize->height = maxElementHeight;
  }

  mSpanDepth--;
  mCurrentSpan->mReflowState = nsnull;  // no longer valid so null it out!
  mCurrentSpan = mCurrentSpan->mParent;
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
  // Take span out of the list
  if (nsnull != psd->mNext) psd->mNext->mPrev = psd->mPrev;
  if (nsnull != psd->mPrev) psd->mPrev->mNext = psd->mNext;

  // Free its frames
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    if (nsnull != pfd->mSpan) {
      FreeSpan(pfd->mSpan);
    }
    PerFrameData* next = pfd->mNext;
    pfd->mNext = mFrameFreeList;
    mFrameFreeList = pfd;
    pfd = next;
  }
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
    pfd = new PerFrameData;
    if (nsnull == pfd) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    mFrameFreeList = pfd->mNext;
  }
  pfd->mSpan = nsnull;
  pfd->mNext = nsnull;
  *aResult = pfd;
  return NS_OK;
}

nsresult
nsLineLayout::ReflowFrame(nsIFrame* aFrame,
                          nsIFrame** aNextRCFrame,
                          nsReflowStatus& aReflowStatus)
{
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

  // Compute the available size for the frame. This available width
  // includes room for the side margins and for the text-indent.
  nsSize availSize;
  if (NS_UNCONSTRAINEDSIZE == psd->mRightEdge) {
    availSize.width = NS_UNCONSTRAINEDSIZE;
  }
  else {
    availSize.width = psd->mRightEdge - psd->mX;
    if (mNoWrap) {
      // XXX Shouldn't this use NS_UNCONSTRAINEDSIZE?
      availSize.width = psd->mReflowState->availableWidth;
    }
  }
  if (NS_UNCONSTRAINEDSIZE == mBottomEdge) {
    availSize.height = NS_UNCONSTRAINEDSIZE;
  }
  else {
    availSize.height = mBottomEdge - mTopEdge;
  }

  // Get reflow reason set correctly. It's possible that a child was
  // created and then it was decided that it could not be reflowed
  // (for example, a block frame that isn't at the start of a
  // line). In this case the reason will be wrong so we need to check
  // the frame state.
  nsReflowReason reason = eReflowReason_Resize;
  nsFrameState state;
  aFrame->GetFrameState(&state);
  if (NS_FRAME_FIRST_REFLOW & state) {
    reason = eReflowReason_Initial;
  }
  else if (*aNextRCFrame == aFrame) {
    reason = eReflowReason_Incremental;
    // Make sure we only incrementally reflow once
    *aNextRCFrame = nsnull;
  }

  // Setup reflow state for reflowing the frame
  nsHTMLReflowState reflowState(mPresContext, *psd->mReflowState, aFrame,
                                availSize, reason);
  reflowState.lineLayout = this;
  reflowState.isTopOfPage = mIsTopOfPage;
  mUnderstandsWhiteSpace = PR_FALSE;
  mCanBreakBeforeFrame = mTotalPlacedFrames > 0;

  // Stash copies of some of the computed state away for later
  // (vertical alignment, for example)
  pfd->mFrame = aFrame;
  pfd->mMargin = reflowState.computedMargin;
  pfd->mBorderPadding = reflowState.mComputedBorderPadding;
  pfd->mFrameType = reflowState.frameType;
  pfd->mRelativePos =
    reflowState.mStylePosition->mPosition == NS_STYLE_POSITION_RELATIVE;
  if (pfd->mRelativePos) {
    pfd->mOffsets = reflowState.computedOffsets;
  }

  // Capture this state *before* we reflow the frame in case it clears
  // the state out. We need to know how to treat the current frame
  // when breaking.
  mWasInWord = InWord();

  // Apply left margins (as appropriate) to the frame computing the
  // new starting x,y coordinates for the frame.
  ApplyLeftMargin(pfd, reflowState);

  // Let frame know that are reflowing it
  nscoord x = pfd->mBounds.x;
  nscoord y = pfd->mBounds.y;
  nsIHTMLReflow* htmlReflow;

  aFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  htmlReflow->WillReflow(mPresContext);

  // Adjust spacemanager coordinate system for the frame. The
  // spacemanager coordinates are <b>inside</b> the current spans
  // border+padding, but the x/y coordinates are not (recall that
  // frame coordinates are relative to the parents origin and that the
  // parents border/padding is <b>inside</b> the parent
  // frame. Therefore we have to subtract out the parents
  // border+padding before translating.
  nsSize innerMaxElementSize;
  nsHTMLReflowMetrics metrics(mComputeMaxElementSize
                              ? &innerMaxElementSize
                              : nsnull);
#ifdef DEBUG
  metrics.width = nscoord(0xdeadbeef);
  metrics.height = nscoord(0xdeadbeef);
  metrics.ascent = nscoord(0xdeadbeef);
  metrics.descent = nscoord(0xdeadbeef);
  if (mComputeMaxElementSize) {
    metrics.maxElementSize->width = nscoord(0xdeadbeef);
    metrics.maxElementSize->height = nscoord(0xdeadbeef);
  }
#endif
  nscoord tx = x - psd->mReflowState->mComputedBorderPadding.left;
  nscoord ty = y - psd->mReflowState->mComputedBorderPadding.top;
  mSpaceManager->Translate(tx, ty);
  htmlReflow->Reflow(mPresContext, metrics, reflowState, aReflowStatus);
  mSpaceManager->Translate(-tx, -ty);

#ifdef DEBUG_kipp
  NS_ASSERTION((metrics.width > -200000) && (metrics.width < 200000), "oy");
  NS_ASSERTION((metrics.height > -200000) && (metrics.height < 200000), "oy");
#endif
#ifdef DEBUG
  if (mComputeMaxElementSize &&
      ((nscoord(0xdeadbeef) == metrics.maxElementSize->width) ||
       (nscoord(0xdeadbeef) == metrics.maxElementSize->height))) {
    printf("nsLineLayout: ");
    nsFrame::ListTag(stdout, aFrame);
    printf(" didn't set max-element-size!\n");
    metrics.maxElementSize->width = 0;
    metrics.maxElementSize->height = 0;
  }
  if ((metrics.width == nscoord(0xdeadbeef)) ||
      (metrics.height == nscoord(0xdeadbeef)) ||
      (metrics.ascent == nscoord(0xdeadbeef)) ||
      (metrics.descent == nscoord(0xdeadbeef))) {
    printf("nsBlockReflowContext: ");
    nsFrame::ListTag(stdout, aFrame);
    printf(" didn't set whad %d,%d,%d,%d!\n", metrics.width, metrics.height,
           metrics.ascent, metrics.descent);
  }
#endif

  aFrame->GetFrameState(&state);
  if (NS_FRAME_OUTSIDE_CHILDREN & state) {
    pfd->mCombinedArea = metrics.mCombinedArea;
  }
  else {
    pfd->mCombinedArea.x = 0;
    pfd->mCombinedArea.y = 0;
    pfd->mCombinedArea.width = metrics.width;
    pfd->mCombinedArea.height = metrics.height;
  }
  pfd->mBounds.width = metrics.width;
  pfd->mBounds.height = metrics.height;
  if (mComputeMaxElementSize) {
    pfd->mMaxElementSize = *metrics.maxElementSize;
  }

  // Now that frame has been reflowed at least one time make sure that
  // the NS_FRAME_FIRST_REFLOW bit is cleared so that never give it an
  // initial reflow reason again.
  if (eReflowReason_Initial == reason) {
    aFrame->GetFrameState(&state);
    aFrame->SetFrameState(state & ~NS_FRAME_FIRST_REFLOW);
  }

  if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    // If frame is complete and has a next-in-flow, we need to delete
    // them now. Do not do this when a break-before is signaled because
    // the frame is going to get reflowed again (and may end up wanting
    // a next-in-flow where it ends up).
    if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
      nsIFrame* kidNextInFlow;
      aFrame->GetNextInFlow(&kidNextInFlow);
      if (nsnull != kidNextInFlow) {
        // Remove all of the childs next-in-flows. Make sure that we ask
        // the right parent to do the removal (it's possible that the
        // parent is not this because we are executing pullup code)
        nsHTMLContainerFrame* parent;
        aFrame->GetParent((nsIFrame**) &parent);
        parent->DeleteChildsNextInFlow(mPresContext, aFrame);
      }
    }

    // See if we can place the frame. If we can't fit it, then we
    // return now.
    if (CanPlaceFrame(pfd, reflowState, metrics, aReflowStatus)) {
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
    }
    else {
      PushFrame(aFrame);
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
nsLineLayout::ApplyLeftMargin(PerFrameData* pfd,
                              nsHTMLReflowState& aReflowState)
{
  // If this is the first frame in the block, and its the first line
  // of a block then see if the text-indent property amounts to
  // anything.
  nscoord indent = 0;
  if (InBlockContext() && (0 == mLineNumber) &&
      (0 == mTotalPlacedFrames)) {
    nsStyleUnit unit = mStyleText->mTextIndent.GetUnit();
    if (eStyleUnit_Coord == unit) {
      indent = mStyleText->mTextIndent.GetCoordValue();
    }
    else if (eStyleUnit_Percent == unit) {
      nscoord width =
        nsHTMLReflowState::GetContainingBlockContentWidth(mBlockReflowState->parentReflowState);
      if (0 != width) {
        indent = nscoord(mStyleText->mTextIndent.GetPercentValue() * width);
      }
    }
  }

  // Adjust available width to account for the indent and the margins
  aReflowState.availableWidth -= indent + pfd->mMargin.left +
    pfd->mMargin.right;

  // NOTE: While the x coordinate remains relative to the parent span,
  // the y coordinate is fixed at the top edge for the line. During
  // VerticalAlignFrames we will repair this so that the y coordinate
  // is properly set and relative to the appropriate span.
  PerSpanData* psd = mCurrentSpan;
  pfd->mBounds.x = psd->mX + indent;
  pfd->mBounds.y = mTopEdge;

  // Compute left margin
  nsIFrame* prevInFlow;
  switch (aReflowState.mStyleDisplay->mFloats) {
  default:
    NS_NOTYETIMPLEMENTED("Unsupported floater type");
    // FALL THROUGH

  case NS_STYLE_FLOAT_LEFT:
  case NS_STYLE_FLOAT_RIGHT:
    // When something is floated, its margins are applied there
    // not here.
    break;

  case NS_STYLE_FLOAT_NONE:
    // Only apply left-margin on the first-in flow for inline frames
    pfd->mFrame->GetPrevInFlow(&prevInFlow);
    if (nsnull != prevInFlow) {
      // Zero this out so that when we compute the max-element-size
      // of the frame we will properly avoid adding in the left
      // margin.
      pfd->mMargin.left = 0;
    }
    pfd->mBounds.x += pfd->mMargin.left;
    break;
  }
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
                            const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus)
{
  // Compute right margin to use
  nscoord rightMargin = 0;
  if (0 != pfd->mBounds.width) {
    switch (aReflowState.mStyleDisplay->mFloats) {
    default:
      NS_NOTYETIMPLEMENTED("Unsupported floater type");
      // FALL THROUGH

    case NS_STYLE_FLOAT_LEFT:
    case NS_STYLE_FLOAT_RIGHT:
      // When something is floated, its margins are applied there
      // not here.
      break;

    case NS_STYLE_FLOAT_NONE:
      // Only apply right margin for the last-in-flow
      if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
        // Zero this out so that when we compute the
        // max-element-size of the frame we will properly avoid
        // adding in the right margin.
        pfd->mMargin.right = 0;
      }
      rightMargin = pfd->mMargin.right;
      break;
    }
  }
  pfd->mMargin.right = rightMargin;

  // Set outside to PR_TRUE if the result of the reflow leads to the
  // frame sticking outside of our available area.
  PerSpanData* psd = mCurrentSpan;
  PRBool outside = pfd->mBounds.XMost() + rightMargin > psd->mRightEdge;

  // There are several special conditions that exist which allow us to
  // ignore outside. If they are true then we can place frame and
  // return PR_TRUE.
  if (!mCanBreakBeforeFrame || mWasInWord || mNoWrap) {
    return PR_TRUE;
  }

  if (0 == pfd->mMargin.left + pfd->mBounds.width + rightMargin) {
    // Empty frames always fit right where they are
    return PR_TRUE;
  }

  if (pfd == mCurrentSpan->mFirstFrame) {
    return PR_TRUE;
  }

  if (outside) {
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
    return PR_FALSE;
  }
  return PR_TRUE;
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
  pfd->mAscent = aMetrics.ascent;
  pfd->mDescent = aMetrics.descent;
  pfd->mCarriedOutTopMargin = aMetrics.mCarriedOutTopMargin;
  pfd->mCarriedOutBottomMargin = aMetrics.mCarriedOutBottomMargin;

  // If the band was updated during the reflow of that frame then we
  // need to adjust any prior frames that were reflowed.
  if (mUpdatedBand && InBlockContext()) {
    UpdateFrames();
    mUpdatedBand = PR_FALSE;
  }

  // Advance to next X coordinate
  psd->mX = pfd->mBounds.XMost() + pfd->mMargin.right;

  // If the frame is a not aware of white-space and it takes up some
  // area, disable leading white-space compression for the next frame
  // to be reflowed.
  if (!mUnderstandsWhiteSpace && !emptyFrame) {
    mEndsInWhiteSpace = PR_FALSE;
  }

  // Compute the bottom margin to apply. Note that the margin only
  // applies if the frame ends up with a non-zero height.
  if (!emptyFrame) {
    // Inform line layout that we have placed a non-empty frame
    mTotalPlacedFrames++;
  }
}

nsresult
nsLineLayout::AddBulletFrame(nsIFrame* aFrame,
                             const nsHTMLReflowMetrics& aMetrics)
{
  NS_ASSERTION(mCurrentSpan == mRootSpan, "bad linelayout user");

  PerFrameData* pfd;
  nsresult rv = NewPerFrameData(&pfd);
  if (NS_SUCCEEDED(rv)) {
    mRootSpan->AppendFrame(pfd);
    pfd->mFrame = aFrame;
    pfd->mMargin.SizeTo(0, 0, 0, 0);
    pfd->mBorderPadding.SizeTo(0, 0, 0, 0);
    pfd->mFrameType = NS_CSS_FRAME_TYPE_INLINE|NS_FRAME_REPLACED_ELEMENT;
    pfd->mRelativePos = PR_FALSE;
    pfd->mAscent = aMetrics.ascent;
    pfd->mDescent = aMetrics.descent;
    aFrame->GetRect(pfd->mBounds);        // y value is irrelevant
    pfd->mCombinedArea = aMetrics.mCombinedArea;
    if (mComputeMaxElementSize) {
      pfd->mMaxElementSize.SizeTo(aMetrics.width, aMetrics.height);
    }
  }
  return rv;
}

#ifdef DEBUG
void
nsLineLayout::DumpPerSpanData(PerSpanData* psd, PRInt32 aIndent)
{
  nsFrame::IndentBy(stdout, aIndent);
  printf("%p: left=%d x=%d right=%d prev/next=%p/%p\n", psd, psd->mLeftEdge,
         psd->mX, psd->mRightEdge, psd->mPrev, psd->mNext);
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
nsLineLayout::VerticalAlignFrames(nsRect& aLineBoxResult,
                                  nsSize& aMaxElementSizeResult)
{
  // Synthesize a PerFrameData for the block frame
  PerFrameData rootPFD;
  rootPFD.mFrame = mBlockReflowState->frame;
  rootPFD.mFrameType = mBlockReflowState->frameType;
  rootPFD.mAscent = 0;
  rootPFD.mDescent = 0;
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

  // It's possible that the line-height isn't tall enough because of
  // the blocks minimum line-height.
  if (0 != lineHeight) {
    // If line contains nothing but empty boxes that have no height
    // then don't apply the min-line-height.
    //
    // Note: This is how we hide lines that contain nothing but
    // compressed whitespace.
    if (lineHeight < mMinLineHeight) {
      // Apply half of the extra space to the top of the line as top
      // leading
      nscoord extra = mMinLineHeight - lineHeight;
      baselineY += extra / 2;
      lineHeight = mMinLineHeight;
    }
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
  printf("  ==> lineHeight=%d baselineY=%d\n", lineHeight, baselineY);
#endif

  // Now position all of the frames in the root span. We will also
  // recurse over the child spans and place any top/bottom aligned
  // frames we find.
  // XXX PERFORMANCE: set a bit per-span to avoid the extra work
  // (propogate it upward too)
  PerFrameData* pfd = psd->mFirstFrame;
  nscoord maxElementWidth = 0;
  nscoord maxElementHeight = 0;
  while (nsnull != pfd) {
    // Compute max-element-size if necessary
    if (mComputeMaxElementSize) {
      nscoord mw = pfd->mMaxElementSize.width +
        pfd->mMargin.left + pfd->mMargin.right;
      if (maxElementWidth < mw) {
        maxElementWidth = mw;
      }
      nscoord mh = pfd->mMaxElementSize.height +
        pfd->mMargin.top + pfd->mMargin.bottom;
      if (maxElementHeight < mh) {
        maxElementHeight = mh;
      }
    }
    PerSpanData* span = pfd->mSpan;
    switch (pfd->mVerticalAlign) {
      case VALIGN_TOP:
        if (span) {
          pfd->mBounds.y = mTopEdge - pfd->mBorderPadding.top +
            span->mTopLeading;
        }
        else {
          pfd->mBounds.y = mTopEdge + pfd->mMargin.top;
        }
        break;
      case VALIGN_BOTTOM:
        if (span) {
          // Compute bottom leading
          pfd->mBounds.y = mTopEdge + lineHeight -
            pfd->mBounds.height + pfd->mBorderPadding.bottom -
            span->mBottomLeading;
        }
        else {
          pfd->mBounds.y = mTopEdge + lineHeight - pfd->mMargin.bottom -
            pfd->mBounds.height;
        }
        break;
      case VALIGN_OTHER:
        pfd->mBounds.y += baselineY;
        break;
    }
    pfd->mFrame->SetRect(pfd->mBounds);
#ifdef NOISY_VERTICAL_ALIGN
    printf("  ");
    nsFrame::ListTag(stdout, pfd->mFrame);
    printf(": y=%d\n", pfd->mBounds.y);
#endif
    if (span) {
      nscoord distanceFromTop = pfd->mBounds.y - mTopEdge;
      PlaceTopBottomFrames(span, distanceFromTop, lineHeight);
    }
    pfd = pfd->mNext;
  }

  // Fill in returned line-box and max-element-size data
  aLineBoxResult.x = psd->mLeftEdge;
  aLineBoxResult.y = mTopEdge;
  aLineBoxResult.width = psd->mX - psd->mLeftEdge;
  aLineBoxResult.height = lineHeight;
  aMaxElementSizeResult.width = maxElementWidth;
  aMaxElementSizeResult.height = maxElementHeight;

  // Undo root-span mFrame pointer to prevent brane damage later on...
  mRootSpan->mFrame = nsnull;
}

void
nsLineLayout::PlaceTopBottomFrames(PerSpanData* psd,
                                   nscoord aDistanceFromTop,
                                   nscoord aLineHeight)
{
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    PerSpanData* span = pfd->mSpan;
    switch (pfd->mVerticalAlign) {
      case VALIGN_TOP:
        if (span) {
          pfd->mBounds.y = -aDistanceFromTop - pfd->mBorderPadding.top +
            span->mTopLeading;
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
          pfd->mBounds.y = -aDistanceFromTop + aLineHeight -
            pfd->mBounds.height + pfd->mBorderPadding.bottom -
            span->mBottomLeading;
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
    pfd = pfd->mNext;
  }
}

// Vertically place frames within a given span. Note: this doesn't
// place top/bottom aligned frames as those have to wait until the
// entire line box height is known. This is called after the span
// frame has finished being reflowed so that we know its height.
void
nsLineLayout::VerticalAlignFrames(PerSpanData* psd)
{
  // Get parent frame info
  PerFrameData* parentPFD = psd->mFrame;
  nsIFrame* parentFrame = parentPFD->mFrame;

  // Get the parent frame's font for all of the frames in this span
  const nsStyleFont* parentFont;
  parentFrame->GetStyleData(eStyleStruct_Font,
                            (const nsStyleStruct*&)parentFont);
  nsIRenderingContext* rc = mBlockReflowState->rendContext;
  rc->SetFont(parentFont->mFont);
  nsIFontMetrics* fm;
  rc->GetFontMetrics(fm);


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
    minY = 0;
    maxY = 0;
#ifdef NOISY_VERTICAL_ALIGN
    nsFrame::ListTag(stdout, parentFrame);
    printf(": pass1 valign frames: topEdge=%d minLineHeight=%d\n",
           mTopEdge, mMinLineHeight);
#endif
  }
  else if (0 != parentPFD->mBounds.height) {
    // Compute the logical height for this span. Also compute the top
    // leading.
    nscoord logicalHeight =
      nsHTMLReflowState::CalcLineHeight(mPresContext, parentFrame);
    nscoord contentHeight = parentPFD->mBounds.height -
      parentPFD->mBorderPadding.top - parentPFD->mBorderPadding.bottom;
    nscoord leading = logicalHeight - contentHeight;
    psd->mTopLeading = leading / 2;
    psd->mBottomLeading = leading - psd->mTopLeading;
    psd->mLogicalHeight = logicalHeight;

    // The initial values for the min and max Y values are in the spans
    // coordinate space, and cover the logical height of the span. If
    // there are child frames in this span that stick out of this area
    // then the minY and maxY are updated by the amount of logical
    // height that is outside this range.
    minY = parentPFD->mBorderPadding.top - psd->mTopLeading;
    maxY = minY + psd->mLogicalHeight;

    // This is the distance from the top edge of the parents visual
    // box to the baseline.
    nscoord parentAscent;
    fm->GetMaxAscent(parentAscent);
    baselineY = parentAscent + parentPFD->mBorderPadding.top;
#ifdef NOISY_VERTICAL_ALIGN
    nsFrame::ListTag(stdout, parentFrame);
    printf(": baseLine=%d logicalHeight=%d topLeading=%d h=%d bp=%d,%d\n",
           baselineY, logicalHeight, psd->mTopLeading,
           parentPFD->mBounds.height,
           parentPFD->mBorderPadding.top, parentPFD->mBorderPadding.bottom);
#endif
  }
  else {
    // When a span container is zero height it means that all of its
    // kids are zero height as well.
    psd->mMinY = 0;
    psd->mMaxY = 0;
    psd->mTopLeading = 0;
    psd->mBottomLeading = 0;
    psd->mLogicalHeight = 0;
#ifdef NOISY_VERTICAL_ALIGN
    printf("  ==> [empty line]\n");
#endif
    return;
  }

  nscoord maxTopBoxHeight = 0;
  nscoord maxBottomBoxHeight = 0;
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    nsIFrame* frame = pfd->mFrame;

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
    const nsStyleText* textStyle;
    frame->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textStyle);
    nsStyleUnit verticalAlignUnit = textStyle->mVerticalAlign.GetUnit();
    if (eStyleUnit_Inherit == verticalAlignUnit) {
      printf("XXX: vertical-align: inherit not implemented for ");
      nsFrame::ListTag(stdout, frame);
      printf("\n");
    }
    PRUint8 verticalAlignEnum;
    nscoord parentAscent, parentDescent, parentXHeight;
    nscoord parentSuperscript, parentSubscript;
    nscoord coordOffset, percentOffset, elementLineHeight;
    nscoord revisedBaselineY;
    switch (verticalAlignUnit) {
      case eStyleUnit_Enumerated:
      default:
        if (eStyleUnit_Enumerated == verticalAlignUnit) {
          verticalAlignEnum = textStyle->mVerticalAlign.GetIntValue();
        }
        else {
          verticalAlignEnum = NS_STYLE_VERTICAL_ALIGN_BASELINE;
        }
        switch (verticalAlignEnum) {
          default:
          case NS_STYLE_VERTICAL_ALIGN_BASELINE:
            // The elements baseline is aligned with the baseline of
            // the parent.
            if (frameSpan) {
              // XXX explain
              pfd->mBounds.y = baselineY - pfd->mAscent;
            }
            else {
              // For non-span elements the borders, padding and
              // margins are significant. Use the visual box height
              // and the bottom margin as the distance off of the
              // baseline.
              pfd->mBounds.y = baselineY - pfd->mAscent - pfd->mMargin.bottom;
            }
            pfd->mVerticalAlign = VALIGN_OTHER;
            break;

          case NS_STYLE_VERTICAL_ALIGN_SUB:
            // Lower the baseline of the box to the subscript offset
            // of the parent's box. This is identical to the baseline
            // alignment except for the addition of the subscript
            // offset to the baseline Y.
            fm->GetSubscriptOffset(parentSubscript);
            revisedBaselineY = baselineY + parentSubscript;
            if (frameSpan) {
              pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
            }
            else {
              pfd->mBounds.y = revisedBaselineY - pfd->mAscent -
                pfd->mMargin.bottom;
            }
            pfd->mVerticalAlign = VALIGN_OTHER;
            break;

          case NS_STYLE_VERTICAL_ALIGN_SUPER:
            // Raise the baseline of the box to the superscript offset
            // of the parent's box. This is identical to the baseline
            // alignment except for the subtraction of the superscript
            // offset to the baseline Y.
            fm->GetSuperscriptOffset(parentSuperscript);
            revisedBaselineY = baselineY - parentSuperscript;
            if (frameSpan) {
              pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
            }
            else {
              pfd->mBounds.y = revisedBaselineY - pfd->mAscent -
                pfd->mMargin.bottom;
            }
            pfd->mVerticalAlign = VALIGN_OTHER;
            break;

          case NS_STYLE_VERTICAL_ALIGN_TOP:
            pfd->mVerticalAlign = VALIGN_TOP;
            if (logicalHeight > maxTopBoxHeight) {
              maxTopBoxHeight = logicalHeight;
            }
            break;

          case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
            pfd->mVerticalAlign = VALIGN_BOTTOM;
            if (logicalHeight > maxBottomBoxHeight) {
              maxBottomBoxHeight = logicalHeight;
            }
            break;

          case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
            // Align the midpoint of the frame with 1/2 the parents
            // x-height above the baseline.
            fm->GetXHeight(parentXHeight);
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

          case NS_STYLE_VERTICAL_ALIGN_TEXT_TOP:
            // The top of the logical box is aligned with the top of
            // the parent elements text.
            fm->GetMaxAscent(parentAscent);
            if (frameSpan) {
              pfd->mBounds.y = baselineY - parentAscent -
                pfd->mBorderPadding.top + frameSpan->mTopLeading;
            }
            else {
              pfd->mBounds.y = baselineY - parentAscent + pfd->mMargin.top;
            }
            pfd->mVerticalAlign = VALIGN_OTHER;
            break;

          case NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM:
            // The bottom of the logical box is aligned with the
            // bottom of the parent elements text.
            fm->GetMaxDescent(parentDescent);
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
        break;

      case eStyleUnit_Coord:
        // According to the CSS2 spec (10.8.1), a positive value
        // "raises" the box by the given distance while a negative value
        // "lowers" the box by the given distance (with zero being the
        // baseline). Since Y coordinates increase towards the bottom of
        // the screen we reverse the sign.
        coordOffset = textStyle->mVerticalAlign.GetCoordValue();
        revisedBaselineY = baselineY - coordOffset;
        if (frameSpan) {
          pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
        }
        else {
          pfd->mBounds.y = revisedBaselineY - pfd->mAscent -
            pfd->mMargin.bottom;
        }
        pfd->mVerticalAlign = VALIGN_OTHER;
        break;

      case eStyleUnit_Percent:
        // Similar to a length value (eStyleUnit_Coord) except that the
        // percentage is a function of the elements line-height value.
        elementLineHeight =
          nsHTMLReflowState::CalcLineHeight(mPresContext, frame);
        percentOffset = nscoord(
          textStyle->mVerticalAlign.GetPercentValue() * elementLineHeight
          );
        revisedBaselineY = baselineY - percentOffset;
        if (frameSpan) {
          pfd->mBounds.y = revisedBaselineY - pfd->mAscent;
        }
        else {
          pfd->mBounds.y = revisedBaselineY - pfd->mAscent -
            pfd->mMargin.bottom;
        }
        pfd->mVerticalAlign = VALIGN_OTHER;
        break;
    }

    // Update minY/maxY for frames that we just placed
    if (pfd->mVerticalAlign == VALIGN_OTHER) {
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
      if (yTop < minY) minY = yTop;
      if (yBottom > maxY) maxY = yBottom;
#ifdef NOISY_VERTICAL_ALIGN
      printf("  ");
      nsFrame::ListTag(stdout, frame);
      printf(": raw: a=%d d=%d h=%d bp=%d,%d logical: h=%d leading=%d y=%d minY=%d maxY=%d\n",
             pfd->mAscent, pfd->mDescent, pfd->mBounds.height,
             pfd->mBorderPadding.top, pfd->mBorderPadding.bottom,
             logicalHeight,
             pfd->mSpan ? topLeading : 0,
             pfd->mBounds.y, minY, maxY);
#endif
      if (psd != mRootSpan) {
        frame->SetRect(pfd->mBounds);
      }
    }
    pfd = pfd->mNext;
  }
  NS_RELEASE(fm);
  psd->mMinY = minY;
  psd->mMaxY = maxY;
#ifdef NOISY_VERTICAL_ALIGN
  printf("  ==> minY=%d maxY=%d delta=%d maxBoxHeight=%d\n",
         minY, maxY, maxY - minY, maxBoxHeight);
#endif
  if (maxTopBoxHeight > mMaxTopBoxHeight) {
    mMaxTopBoxHeight = maxTopBoxHeight;
  }
  if (maxBottomBoxHeight > mMaxBottomBoxHeight) {
    mMaxBottomBoxHeight = maxBottomBoxHeight;
  }
}

void
nsLineLayout::TrimTrailingWhiteSpace(nsRect& aLineBounds)
{
}

void
nsLineLayout::HorizontalAlignFrames(nsRect& aLineBounds, PRBool aAllowJustify)
{
  PerSpanData* psd = mRootSpan;
  nscoord availWidth = psd->mRightEdge;
  if (NS_UNCONSTRAINEDSIZE == availWidth) {
    // Don't bother horizontal aligning on pass1 table reflow
#ifdef REALLY_NOISY_HORIZONTAL_ALIGN
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": skipping horizontal alignment in pass1 table reflow\n");
#endif
    return;
  }
  availWidth -= psd->mLeftEdge;
  nscoord remainingWidth = availWidth - aLineBounds.width;
#ifdef REALLY_NOISY_HORIZONTAL_ALIGN
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": availWidth=%d lineWidth=%d delta=%d\n",
           availWidth, aLineBounds.width, remainingWidth);
#endif
  if (remainingWidth > 0) {
    nscoord dx = 0;
    switch (mTextAlign) {
      case NS_STYLE_TEXT_ALIGN_DEFAULT:
        if (NS_STYLE_DIRECTION_LTR == mDirection) {
          // default alignment for left-to-right is left so do nothing
          break;
        }
        // Fall through to align right case for default alignment
        // used when the direction is right-to-left.

      case NS_STYLE_TEXT_ALIGN_RIGHT:
        dx = remainingWidth;
        break;

      case NS_STYLE_TEXT_ALIGN_LEFT:
        break;

      case NS_STYLE_TEXT_ALIGN_JUSTIFY:
        // If this is not the last line then go ahead and justify the
        // frames in the line. If it is the last line then if the
        // direction is right-to-left then we right-align the frames.
        if (aAllowJustify) {
          break;
        }
        else if (NS_STYLE_DIRECTION_RTL == mDirection) {
          // right align the frames
          dx = remainingWidth;;
        }
        break;

      case NS_STYLE_TEXT_ALIGN_CENTER:
        dx = remainingWidth / 2;
        break;
    }
    if (0 != dx) {
      PerFrameData* pfd = psd->mFirstFrame;
      while (nsnull != pfd) {
        pfd->mBounds.x += dx;
        pfd->mFrame->SetRect(pfd->mBounds);
        pfd = pfd->mNext;
      }
    }
  }
}

void
nsLineLayout::RelativePositionFrames(nsRect& aCombinedArea)
{
  RelativePositionFrames(mRootSpan, aCombinedArea);
}

void
nsLineLayout::RelativePositionFrames(PerSpanData* psd, nsRect& aCombinedArea)
{
  nsPoint origin;
  nsRect spanCombinedArea;
  PerFrameData* pfd;

  nscoord x0, y0, x1, y1;
  if (nsnull != psd->mFrame) {
    // The minimum combined area for the frames in a span covers the
    // entire span frame.
    pfd = psd->mFrame;
    x0 = 0;
    y0 = 0;
    x1 = pfd->mBounds.width;
    y1 = pfd->mBounds.height;
  }
  else {
    // The minimum combined area for the frames that are direct
    // children of the block starts at the upper left corner of the
    // line but has no width or height.
    x1 = x0 = psd->mLeftEdge;
    y1 = y0 = mTopEdge;
  }

  pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    nscoord x = pfd->mBounds.x;
    nscoord y = pfd->mBounds.y;

    // Adjust the origin of the frame
    if (pfd->mRelativePos) {
      nsIFrame* frame = pfd->mFrame;
      frame->GetOrigin(origin);
      // XXX what about right and bottom?
      nscoord dx = pfd->mOffsets.left;
      nscoord dy = pfd->mOffsets.top;
      frame->MoveTo(origin.x + dx, origin.y + dy);
      x += dx;
      y += dy;
    }

    // Note: the combined area of a child is in its coordinate
    // system. We adjust the childs combined area into our coordinate
    // system before computing the aggregated value by adding in
    // <b>x</b> and <b>y</b> which were computed above.
    nsRect* r = &pfd->mCombinedArea;
    if (pfd->mSpan) {
      // Compute a new combined area for the child span before
      // aggregating it into our combined area.
      r = &spanCombinedArea;
      RelativePositionFrames(pfd->mSpan, spanCombinedArea);
    }

    nscoord xl = x + r->x;
    nscoord xr = x + r->XMost();
    if (xl < x0) x0 = xl;
    if (xr > x1) x1 = xr;
    nscoord yt = y + r->y;
    nscoord yb = y + r->YMost();
    if (yt < y0) y0 = yt;
    if (yb > y1) y1 = yb;

    pfd = pfd->mNext;
  }

  // Compute aggregated combined area
  aCombinedArea.x = x0;
  aCombinedArea.y = y0;
  aCombinedArea.width = x1 - x0;
  aCombinedArea.height = y1 - y0;

  // If we just computed a spans combined area, we need to update its
  // NS_FRAME_OUTSIDE_CHILDREN bit..
  if (nsnull != psd->mFrame) {
    pfd = psd->mFrame;
    nsIFrame* frame = pfd->mFrame;
    nsFrameState oldState;
    frame->GetFrameState(&oldState);
    nsFrameState newState = oldState & ~NS_FRAME_OUTSIDE_CHILDREN;
    if ((x0 < 0) || (y0 < 0) ||
        (x1 > pfd->mBounds.width) || (y1 > pfd->mBounds.height)) {
      newState |= NS_FRAME_OUTSIDE_CHILDREN;
    }
    if (newState != oldState) {
      frame->SetFrameState(newState);
    }
  }
}

void
nsLineLayout::ForgetWordFrame(nsIFrame* aFrame)
{
  NS_ASSERTION((void*)aFrame == mWordFrames[0], "forget-word-frame");
  if (0 != mWordFrames.Count()) {
    mWordFrames.RemoveElementAt(0);
  }
}

nsIFrame*
nsLineLayout::FindNextText(nsIFrame* aFrame)
{
  // Only the first-in-flows are present in the text run list so
  // backup from the argument frame to its first-in-flow.
  for (;;) {
    nsIFrame* prevInFlow;
    aFrame->GetPrevInFlow(&prevInFlow);
    if (nsnull == prevInFlow) {
      break;
    }
    aFrame = prevInFlow;
  }

  // Now look for the frame that follows aFrame's first-in-flow
  nsTextRun* run = mReflowTextRuns;
  while (nsnull != run) {
    PRInt32 ix = run->mArray.IndexOf(aFrame);
    if (ix >= 0) {
      if (ix < run->mArray.Count() - 1) {
        return (nsIFrame*) run->mArray[ix + 1];
      }
    }
    run = run->mNext;
  }
  return nsnull;
}

nsresult
nsLineLayout::AddText(nsIFrame* aTextFrame)
{
  if (nsnull == mNewTextRun) {
    mNewTextRun = new nsTextRun();
    if (nsnull == mNewTextRun) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    *mTextRunP = mNewTextRun;
    mTextRunP = &mNewTextRun->mNext;
  }
  mNewTextRun->mArray.AppendElement(aTextFrame);
#ifdef DEBUG_ADD_TEXT
  PRInt32 n = mNewTextRun->mArray.Count();
  for (PRInt32 i = 0; i < n - 1; i++) {
    NS_ASSERTION(mNewTextRun->mArray[i] != (void*)aTextFrame, "yikes");
  }
#endif
  return NS_OK;/* XXX */
}

void
nsLineLayout::EndTextRun()
{
  mNewTextRun = nsnull;
}

nsTextRun*
nsLineLayout::TakeTextRuns()
{
  nsTextRun* result = mTextRuns;
  mTextRuns = nsnull;
  mTextRunP = &mTextRuns;
  mNewTextRun = nsnull;
  return result;
}

PRBool
nsLineLayout::TreatFrameAsBlock(nsIFrame* aFrame)
{
  const nsStyleDisplay* display;
  const nsStylePosition* position;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) display);
  aFrame->GetStyleData(eStyleStruct_Position,(const nsStyleStruct*&) position);
  if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
    return PR_FALSE;
  }
  if (NS_STYLE_FLOAT_NONE != display->mFloats) {
    return PR_FALSE;
  }
  switch (display->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
  case NS_STYLE_DISPLAY_RUN_IN:
  case NS_STYLE_DISPLAY_COMPACT:
  case NS_STYLE_DISPLAY_TABLE:
    return PR_TRUE;
  }
  return PR_FALSE;
}
