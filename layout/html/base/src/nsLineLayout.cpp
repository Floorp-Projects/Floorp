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
#define PL_ARENA_CONST_ALIGN_MASK (sizeof(void*)-1)
#include "plarena.h"

#include "nsCOMPtr.h"
#include "nsLineLayout.h"
#include "nsBlockFrame.h"
#include "nsInlineFrame.h"
#include "nsStyleConsts.h"
#include "nsHTMLContainerFrame.h"
#include "nsSpaceManager.h"
#include "nsStyleContext.h"
#include "nsPresContext.h"
#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"
#include "nsLayoutAtoms.h"
#include "nsPlaceholderFrame.h"
#include "nsReflowPath.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLAtoms.h"
#include "nsTextFragment.h"
#include "nsBidiUtils.h"

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

//----------------------------------------------------------------------

#define FIX_BUG_50257

#define PLACED_LEFT  0x1
#define PLACED_RIGHT 0x2

#define HACK_MEW
//#undef HACK_MEW
#ifdef HACK_MEW
static nscoord AccumulateImageSizes(nsPresContext& aPresContext, nsIFrame& aFrame)
{
  nscoord sizes = 0;

  // see if aFrame is an image frame first
  if (aFrame.GetType() == nsLayoutAtoms::imageFrame) {
    sizes += aFrame.GetSize().width;
  } else {
    // see if there are children to process
    // XXX: process alternate child lists?
    nsIFrame* child = aFrame.GetFirstChild(nsnull);
    while (child) {
      // recurse: note that we already know we are in a child frame, so no need to track further
      sizes += AccumulateImageSizes(aPresContext, *child);
      // now next sibling
      child = child->GetNextSibling();
    }
  }

  return sizes;
}

static PRBool InUnconstrainedTableCell(const nsHTMLReflowState& aBlockReflowState)
{
  PRBool result = PR_FALSE;

  // get the parent reflow state
  const nsHTMLReflowState* parentReflowState = aBlockReflowState.parentReflowState;
  if (parentReflowState) {
    // check if the frame is a tablecell
    NS_ASSERTION(parentReflowState->mStyleDisplay, "null styleDisplay in parentReflowState unexpected");
    if (parentReflowState->mStyleDisplay &&
        parentReflowState->mStyleDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL) {
      // see if width is unconstrained or percent
      NS_ASSERTION(parentReflowState->mStylePosition, "null stylePosition in parentReflowState unexpected");
      if(parentReflowState->mStylePosition) {
        switch(parentReflowState->mStylePosition->mWidth.GetUnit()) {
          case eStyleUnit_Auto :
          case eStyleUnit_Null :
            result = PR_TRUE;
            break;
          default:
            result = PR_FALSE;
            break;
        }
      }
    }
  }
  return result;
}

#endif

MOZ_DECL_CTOR_COUNTER(nsLineLayout)

nsLineLayout::nsLineLayout(nsPresContext* aPresContext,
                           nsSpaceManager* aSpaceManager,
                           const nsHTMLReflowState* aOuterReflowState,
                           PRBool aComputeMaxElementWidth)
  : mPresContext(aPresContext),
    mSpaceManager(aSpaceManager),
    mBlockReflowState(aOuterReflowState),
    mBlockRS(nsnull),/* XXX temporary */
    mMinLineHeight(0),
    mComputeMaxElementWidth(aComputeMaxElementWidth),
    mTextIndent(0),
    mWordFrames(0)
{
  MOZ_COUNT_CTOR(nsLineLayout);

  // Stash away some style data that we need
  mStyleText = aOuterReflowState->frame->GetStyleText();
  mTextAlign = mStyleText->mTextAlign;
  mLineNumber = 0;
  mColumn = 0;
  mFlags = 0; // default all flags to false except those that follow here...
  SetFlag(LL_ENDSINWHITESPACE, PR_TRUE);
  mPlacedFloats = 0;
  mTotalPlacedFrames = 0;
  mTopEdge = mBottomEdge = 0;

  // Instead of always pre-initializing the free-lists for frames and
  // spans, we do it on demand so that situations that only use a few
  // frames and spans won't waste alot of time in unneeded
  // initialization.
  PL_INIT_ARENA_POOL(&mArena, "nsLineLayout", 1024);
  mFrameFreeList = nsnull;
  mSpanFreeList = nsnull;

  mCurrentSpan = mRootSpan = nsnull;
  mSpanDepth = 0;

  mCompatMode = mPresContext->CompatibilityMode();
}

nsLineLayout::~nsLineLayout()
{
  MOZ_COUNT_DTOR(nsLineLayout);

  NS_ASSERTION(nsnull == mRootSpan, "bad line-layout user");

  delete mWordFrames; // operator delete for this class just returns

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

void*
nsLineLayout::ArenaDeque::operator new(size_t aSize, PLArenaPool &aPool)
{
  void *mem;

  PL_ARENA_ALLOCATE(mem, &aPool, aSize);
  return mem;
}

PRBool nsLineLayout::AllocateDeque()
{
  mWordFrames = new(mArena) ArenaDeque;

  return mWordFrames != nsnull;
}

// Find out if the frame has a non-null prev-in-flow, i.e., whether it
// is a continuation.
inline PRBool
HasPrevInFlow(nsIFrame *aFrame)
{
  nsIFrame *prevInFlow;
  aFrame->GetPrevInFlow(&prevInFlow);
  return prevInFlow != nsnull;
}

void
nsLineLayout::BeginLineReflow(nscoord aX, nscoord aY,
                              nscoord aWidth, nscoord aHeight,
                              PRBool aImpactedByFloats,
                              PRBool aIsTopOfPage)
{
  NS_ASSERTION(nsnull == mRootSpan, "bad linelayout user");
#ifdef DEBUG
  if ((aWidth != NS_UNCONSTRAINEDSIZE) && CRAZY_WIDTH(aWidth)) {
    NS_NOTREACHED("bad width");
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": Init: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
  }
  if ((aHeight != NS_UNCONSTRAINEDSIZE) && CRAZY_HEIGHT(aHeight)) {
    NS_NOTREACHED("bad height");
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

  mColumn = 0;
  
  SetFlag(LL_ENDSINWHITESPACE, PR_TRUE);
  SetFlag(LL_UNDERSTANDSNWHITESPACE, PR_FALSE);
  SetFlag(LL_FIRSTLETTERSTYLEOK, PR_FALSE);
  SetFlag(LL_ISTOPOFPAGE, aIsTopOfPage);
  SetFlag(LL_UPDATEDBAND, PR_FALSE);
  mPlacedFloats = 0;
  SetFlag(LL_IMPACTEDBYFLOATS, aImpactedByFloats);
  mTotalPlacedFrames = 0;
  SetFlag(LL_CANPLACEFLOAT, PR_TRUE);
  SetFlag(LL_LINEENDSINBR, PR_FALSE);
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

  switch (mStyleText->mWhiteSpace) {
  case NS_STYLE_WHITESPACE_PRE:
  case NS_STYLE_WHITESPACE_NOWRAP:
    psd->mNoWrap = PR_TRUE;
    break;
  default:
    psd->mNoWrap = PR_FALSE;
    break;
  }
  psd->mDirection = mBlockReflowState->mStyleVisibility->mDirection;
  psd->mChangedFrameDirection = PR_FALSE;

  // If this is the first line of a block then see if the text-indent
  // property amounts to anything.
  
  if (0 == mLineNumber && !HasPrevInFlow(mBlockReflowState->frame)) {
    nscoord indent = 0;
    nsStyleUnit unit = mStyleText->mTextIndent.GetUnit();
    if (eStyleUnit_Coord == unit) {
      indent = mStyleText->mTextIndent.GetCoordValue();
    }
    else if (eStyleUnit_Percent == unit) {
      nscoord width =
        nsHTMLReflowState::GetContainingBlockContentWidth(mBlockReflowState);
      if ((0 != width) && (NS_UNCONSTRAINEDSIZE != width)) {
        indent = nscoord(mStyleText->mTextIndent.GetPercentValue() * width);
      }
    }

    mTextIndent = indent;

    if (NS_STYLE_DIRECTION_RTL == psd->mDirection) {
      if (NS_UNCONSTRAINEDSIZE != psd->mRightEdge) {
        psd->mRightEdge -= indent;
      }
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
nsLineLayout::UpdateBand(nscoord aX, nscoord aY,
                         nscoord aWidth, nscoord aHeight,
                         PRBool aPlacedLeftFloat,
                         nsIFrame* aFloatFrame)
{
#ifdef REALLY_NOISY_REFLOW
  printf("nsLL::UpdateBand %d, %d, %d, %d, frame=%p placedLeft=%s\n  will set mImpacted to PR_TRUE\n",
         aX, aY, aWidth, aHeight, aFloatFrame, aPlacedLeftFloat?"true":"false");
#endif
  PerSpanData* psd = mRootSpan;
  NS_PRECONDITION(psd->mX == psd->mLeftEdge, "update-band called late");
#ifdef DEBUG
  if ((aWidth != NS_UNCONSTRAINEDSIZE) && CRAZY_WIDTH(aWidth)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": UpdateBand: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
  }
  if ((aHeight != NS_UNCONSTRAINEDSIZE) && CRAZY_HEIGHT(aHeight)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": UpdateBand: bad caller: height WAS %d(0x%x)\n",
           aHeight, aHeight);
  }
#endif

  // Compute the difference between last times width and the new width
  nscoord deltaWidth = 0;
  if (NS_UNCONSTRAINEDSIZE != psd->mRightEdge) {
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aWidth, "switched constraints");
    nscoord oldWidth = psd->mRightEdge - psd->mLeftEdge;
    deltaWidth = aWidth - oldWidth;
  }
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": UpdateBand: %d,%d,%d,%d deltaWidth=%d %s float\n",
         aX, aY, aWidth, aHeight, deltaWidth,
         aPlacedLeftFloat ? "left" : "right");
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
  SetFlag(LL_UPDATEDBAND, PR_TRUE);
  mPlacedFloats |= (aPlacedLeftFloat ? PLACED_LEFT : PLACED_RIGHT);
  SetFlag(LL_IMPACTEDBYFLOATS, PR_TRUE);

  SetFlag(LL_LASTFLOATWASLETTERFRAME,
          nsLayoutAtoms::letterFrame == aFloatFrame->GetType());

  // Now update all of the open spans...
  mRootSpan->mContainsFloat = PR_TRUE;              // make sure mRootSpan gets updated too
  psd = mCurrentSpan;
  while (psd != mRootSpan) {
    NS_ASSERTION(nsnull != psd, "null ptr");
    if (nsnull == psd) {
      break;
    }
    NS_ASSERTION(psd->mX == psd->mLeftEdge, "bad float placement");
    if (NS_UNCONSTRAINEDSIZE == aWidth) {
      psd->mRightEdge = NS_UNCONSTRAINEDSIZE;
    }
    else {
      psd->mRightEdge += deltaWidth;
    }
    psd->mContainsFloat = PR_TRUE;
#ifdef NOISY_REFLOW
    printf("  span %p: oldRightEdge=%d newRightEdge=%d\n",
           psd, psd->mRightEdge - deltaWidth, psd->mRightEdge);
#endif
    psd = psd->mParent;
  }
}

// Note: Only adjust the outermost frames (the ones that are direct
// children of the block), not the ones in the child spans. The reason
// is simple: the frames in the spans have coordinates local to their
// parent therefore they are moved when their parent span is moved.
void
nsLineLayout::UpdateFrames()
{
  NS_ASSERTION(nsnull != mRootSpan, "UpdateFrames with no active spans");

  PerSpanData* psd = mRootSpan;
  if (PLACED_LEFT & mPlacedFloats) {
    PerFrameData* pfd = psd->mFirstFrame;
    while (nsnull != pfd) {
      pfd->mBounds.x = psd->mX;
      pfd = pfd->mNext;
    }
  }
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
    psd = NS_REINTERPRET_CAST(PerSpanData*, mem);
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

    const nsStyleText* styleText = aSpanReflowState->frame->GetStyleText();
    switch (styleText->mWhiteSpace) {
    case NS_STYLE_WHITESPACE_PRE:
    case NS_STYLE_WHITESPACE_NOWRAP:
        psd->mNoWrap = PR_TRUE;
        break;
    default:
        psd->mNoWrap = PR_FALSE;
        break;
    }
    psd->mDirection = aSpanReflowState->mStyleVisibility->mDirection;
    psd->mChangedFrameDirection = PR_FALSE;

    // Switch to new span
    mCurrentSpan = psd;
    mSpanDepth++;
  }
  return rv;
}

void
nsLineLayout::EndSpan(nsIFrame* aFrame,
                      nsSize& aSizeResult,
                      nscoord* aMaxElementWidth)
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
  if (nsnull != psd->mLastFrame) {
    width = psd->mX - psd->mLeftEdge;
    PerFrameData* pfd = psd->mFirstFrame;
    while (nsnull != pfd) {
      /* there's one oddball case we need to guard against
       * if we're reflowed with NS_UNCONSTRAINEDSIZE
       * then the last frame will not contribute to the max element size height
       * if it is a text frame that only contains whitespace
       */
      if (NS_UNCONSTRAINEDSIZE != psd->mRightEdge ||  // it's not an unconstrained reflow
          pfd->mNext ||                               // or it's not the last frame in the span
          !pfd->GetFlag(PFD_ISTEXTFRAME) ||           // or it's not a text frame
          pfd->GetFlag(PFD_ISNONWHITESPACETEXTFRAME)  // or it contains something other than whitespace
         ) {
        if (pfd->mBounds.height > maxHeight) maxHeight = pfd->mBounds.height;

        // Compute max-element-width if necessary
        if (aMaxElementWidth) {
          nscoord mw = pfd->mMaxElementWidth +
            pfd->mMargin.left + pfd->mMargin.right;
          if (maxElementWidth < mw) {
            maxElementWidth = mw;
          }
        }
      }
      pfd = pfd->mNext;
    }
  }
  aSizeResult.width = width;
  aSizeResult.height = maxHeight;
  if (aMaxElementWidth) {
    if (psd->mNoWrap) {
      // When we have a non-breakable span, it's max-element-width
      // width is its entire width.
      *aMaxElementWidth = width;
    }
    else {
      *aMaxElementWidth = maxElementWidth;
    }
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

  // Now put the span on the free list since its free too
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
    pfd = NS_REINTERPRET_CAST(PerFrameData*, mem);
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
nsLineLayout::CanPlaceFloatNow() const
{
  return GetFlag(LL_CANPLACEFLOAT);
}

PRBool
nsLineLayout::LineIsBreakable() const
{
  if ((0 != mTotalPlacedFrames) || GetFlag(LL_IMPACTEDBYFLOATS)) {
    return PR_TRUE;
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

  // Compute the available size for the frame. This available width
  // includes room for the side margins.
  nsSize availSize;
  if (NS_UNCONSTRAINEDSIZE == psd->mRightEdge) {
    availSize.width = NS_UNCONSTRAINEDSIZE;
  }
  else {
    availSize.width = psd->mRightEdge - psd->mX;
    if (psd->mNoWrap) {
      // Make up a width to use for reflowing into.  XXX what value to
      // use? for tables, we want to limit it; for other elements
      // (e.g. text) it can be unlimited...
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
  const nsHTMLReflowState* rs = psd->mReflowState;
  nsReflowReason reason = eReflowReason_Resize;
  if (NS_FRAME_FIRST_REFLOW & aFrame->GetStateBits()) {
    reason = eReflowReason_Initial;
  }  
  else if (rs->reason == eReflowReason_Incremental) { // XXX
    // XXXwaterson (above) previously used mBlockReflowState rather
    // than psd->mReflowState.

    // If the frame we're about to reflow is on the reflow path, then
    // propagate the reflow as `incremental' so it unwinds correctly
    // to the target frames below us.
    PRBool frameIsOnReflowPath = rs->path->HasChild(aFrame);
    if (frameIsOnReflowPath)
      reason = eReflowReason_Incremental;

    // But...if the incremental reflow command is a StyleChanged
    // reflow and its target is the current span, change the reason
    // to `style change', so that it propagates through the entire
    // subtree.
    nsHTMLReflowCommand* rc = rs->path->mReflowCommand;
    if (rc) {
      nsReflowType type;
      rc->GetType(type);
      if (type == eReflowType_StyleChanged) {
        nsIFrame* parentFrame = psd->mFrame
          ? psd->mFrame->mFrame
          : mBlockReflowState->frame;
        nsIFrame* target;
        rc->GetTarget(target);
        if (target == parentFrame) {
          reason = eReflowReason_StyleChange;
        }
      }
      else if (type == eReflowType_ReflowDirty &&
               (aFrame->GetStateBits() & NS_FRAME_IS_DIRTY) &&
               !frameIsOnReflowPath) {
        reason = eReflowReason_Dirty;
      }
    }
  }
  else if (rs->reason == eReflowReason_StyleChange) {
    reason = eReflowReason_StyleChange;
  }
  else if (rs->reason == eReflowReason_Dirty) {
    if (aFrame->GetStateBits() & NS_FRAME_IS_DIRTY)
      reason = eReflowReason_Dirty;
  }

  // Setup reflow state for reflowing the frame
  nsHTMLReflowState reflowState(mPresContext, *psd->mReflowState,
                                aFrame, availSize, reason);
  reflowState.mLineLayout = this;
  reflowState.mFlags.mIsTopOfPage = GetFlag(LL_ISTOPOFPAGE);
  SetFlag(LL_UNDERSTANDSNWHITESPACE, PR_FALSE);
  mTextJustificationNumSpaces = 0;
  mTextJustificationNumLetters = 0;

  // Stash copies of some of the computed state away for later
  // (vertical alignment, for example)
  pfd->mFrame = aFrame;
  pfd->mMargin = reflowState.mComputedMargin;
  pfd->mBorderPadding = reflowState.mComputedBorderPadding;
  pfd->mFrameType = reflowState.mFrameType;
  pfd->SetFlag(PFD_RELATIVEPOS,
               (reflowState.mStyleDisplay->mPosition == NS_STYLE_POSITION_RELATIVE));
  if (pfd->GetFlag(PFD_RELATIVEPOS)) {
    pfd->mOffsets = reflowState.mComputedOffsets;
  }

  // NOTE: While the x coordinate remains relative to the parent span,
  // the y coordinate is fixed at the top edge for the line. During
  // VerticalAlignFrames we will repair this so that the y coordinate
  // is properly set and relative to the appropriate span.
  pfd->mBounds.x = psd->mX;
  pfd->mBounds.y = mTopEdge;

  // We want to guarantee that we always make progress when
  // formatting. Therefore, if the object being placed on the line is
  // too big for the line, but it is the only thing on the line
  // (including counting floats) then we go ahead and place it
  // anyway. Its also true that if the object is a part of a larger
  // object (a multiple frame word) then we will place it on the line
  // too.
  //
  // Capture this state *before* we reflow the frame in case it clears
  // the state out. We need to know how to treat the current frame
  // when breaking.
  PRBool notSafeToBreak = CanPlaceFloatNow() || InWord();

  // Apply start margins (as appropriate) to the frame computing the
  // new starting x,y coordinates for the frame.
  ApplyStartMargin(pfd, reflowState);

  // Let frame know that are reflowing it. Note that we don't bother
  // positioning the frame yet, because we're probably going to end up
  // moving it when we do the vertical alignment
  nscoord x = pfd->mBounds.x;
  nscoord y = pfd->mBounds.y;

  aFrame->WillReflow(mPresContext);

  // Adjust spacemanager coordinate system for the frame. The
  // spacemanager coordinates are <b>inside</b> the current spans
  // border+padding, but the x/y coordinates are not (recall that
  // frame coordinates are relative to the parents origin and that the
  // parents border/padding is <b>inside</b> the parent
  // frame. Therefore we have to subtract out the parents
  // border+padding before translating.
  nsHTMLReflowMetrics metrics(mComputeMaxElementWidth);
#ifdef DEBUG
  metrics.width = nscoord(0xdeadbeef);
  metrics.height = nscoord(0xdeadbeef);
  metrics.ascent = nscoord(0xdeadbeef);
  metrics.descent = nscoord(0xdeadbeef);
  if (mComputeMaxElementWidth) {
    metrics.mMaxElementWidth = nscoord(0xdeadbeef);
  }
#endif
  nscoord tx = x - psd->mReflowState->mComputedBorderPadding.left;
  nscoord ty = y - psd->mReflowState->mComputedBorderPadding.top;
  mSpaceManager->Translate(tx, ty);

#ifdef IBMBIDI
  PRInt32 start, end;

  if (mPresContext->BidiEnabled()) {
    if (aFrame->GetStateBits() & NS_FRAME_IS_BIDI) {
      aFrame->GetOffsets(start, end);
    }
  }
#endif // IBMBIDI

  nsIAtom* frameType = aFrame->GetType();

  rv = aFrame->Reflow(mPresContext, metrics, reflowState, aReflowStatus);
  if (NS_FAILED(rv)) {
    NS_WARNING( "Reflow of frame failed in nsLineLayout" );
    return rv;
  }

  pfd->mJustificationNumSpaces = mTextJustificationNumSpaces;
  pfd->mJustificationNumLetters = mTextJustificationNumLetters;

  // XXX See if the frame is a placeholderFrame and if it is process
  // the float.
  if (frameType) {
    if (nsLayoutAtoms::placeholderFrame == frameType) {
      pfd->SetFlag(PFD_ISPLACEHOLDERFRAME, PR_TRUE);
      nsIFrame* outOfFlowFrame = ((nsPlaceholderFrame*)aFrame)->GetOutOfFlowFrame();
      if (outOfFlowFrame) {
        // Make sure it's floated and not absolutely positioned
        const nsStyleDisplay* display = outOfFlowFrame->GetStyleDisplay();
        if (!display->IsAbsolutelyPositioned()) {
          if (eReflowReason_Incremental == reason) {
            InitFloat((nsPlaceholderFrame*)aFrame, aReflowStatus);
          }
          else {
            AddFloat((nsPlaceholderFrame*)aFrame, aReflowStatus);
          }
          if (outOfFlowFrame->GetType() == nsLayoutAtoms::letterFrame) {
            SetFlag(LL_FIRSTLETTERSTYLEOK, PR_FALSE);
            // An incomplete reflow status means we should split the
            // float if the height is constrained (bug 145305). We
            // never split floating first letters.
            if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus)) 
              aReflowStatus = NS_FRAME_COMPLETE;
          }
        }
      }
    }
    else if (nsLayoutAtoms::textFrame == frameType) {
      // Note non-empty text-frames for inline frame compatability hackery
      pfd->SetFlag(PFD_ISTEXTFRAME, PR_TRUE);
      // XXX An empty text frame at the end of the line seems not
      // to have zero width.
      if (metrics.width) {
        pfd->SetFlag(PFD_ISNONEMPTYTEXTFRAME, PR_TRUE);
        nsIContent* content = pfd->mFrame->GetContent();

        nsCOMPtr<nsITextContent> textContent
          = do_QueryInterface(content);
        if (textContent) {
          pfd->SetFlag(PFD_ISNONWHITESPACETEXTFRAME,
                       !textContent->IsOnlyWhitespace());
// fix for bug 40882
#ifdef IBMBIDI
          if (mPresContext->BidiEnabled()) {
            const nsTextFragment* frag = textContent->Text();
            if (frag->Is2b()) {
              //PRBool isVisual;
              //mPresContext->IsVisualMode(isVisual);
              PRUnichar ch = /*(isVisual) ?
                              *(frag->Get2b() + frag->GetLength() - 1) :*/ *frag->Get2b();
              if (IS_BIDI_DIACRITIC(ch)) {
                mPresContext->PropertyTable()->SetProperty(aFrame,
                           nsLayoutAtoms::endsInDiacritic, NS_INT32_TO_PTR(ch),
                                                           nsnull, nsnull);
              }
            }
          }
#endif // IBMBIDI
        }
      }
    }
    else if (nsLayoutAtoms::letterFrame==frameType) {
      pfd->SetFlag(PFD_ISLETTERFRAME, PR_TRUE);
    }
  }

  mSpaceManager->Translate(-tx, -ty);

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
    if (mComputeMaxElementWidth &&
        (nscoord(0xdeadbeef) == metrics.mMaxElementWidth)) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" didn't set max-element-width!\n");
    }
#ifdef REALLY_NOISY_MAX_ELEMENT_SIZE
    // Note: there are common reflow situations where this *correctly*
    // occurs; so only enable this debug noise when you really need to
    // analyze in detail.
    if (mComputeMaxElementWidth &&
        (metrics.mMaxElementWidth > metrics.width)) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(": WARNING: maxElementWidth=%d > metrics=%d\n",
             metrics.mMaxElementWidth, metrics.width);
    }
#endif
    if ((metrics.width == nscoord(0xdeadbeef)) ||
        (metrics.height == nscoord(0xdeadbeef)) ||
        (metrics.ascent == nscoord(0xdeadbeef)) ||
        (metrics.descent == nscoord(0xdeadbeef))) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" didn't set whad %d,%d,%d,%d!\n", metrics.width, metrics.height,
             metrics.ascent, metrics.descent);
    }
  }
#endif
#ifdef DEBUG
  if (nsBlockFrame::gNoisyMaxElementWidth) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
      if (mComputeMaxElementWidth) {
        printf("  ");
        nsFrame::ListTag(stdout, aFrame);
        printf(": maxElementWidth=%d wh=%d,%d,\n",
               metrics.mMaxElementWidth,
               metrics.width, metrics.height);
      }
    }
  }
#endif

  // Unlike with non-inline reflow, the overflow area here does *not*
  // include the accumulation of the frame's bounds and its inline
  // descendants' bounds. Nor does it include the outline area; it's
  // just the union of the bounds of any absolute children. That is
  // added in later by nsLineLayout::ReflowInlineFrames.
  pfd->mCombinedArea = metrics.mOverflowArea;

  pfd->mBounds.width = metrics.width;
  pfd->mBounds.height = metrics.height;
  if (mComputeMaxElementWidth) {
    pfd->mMaxElementWidth = metrics.mMaxElementWidth;
  }

  // Size the frame, but |RelativePositionFrames| will size the view.
  aFrame->SetSize(nsSize(metrics.width, metrics.height));

  // Tell the frame that we're done reflowing it
  aFrame->DidReflow(mPresContext, &reflowState, NS_FRAME_REFLOW_FINISHED);

  if (aMetrics) {
    *aMetrics = metrics;
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
        nsHTMLContainerFrame* parent = NS_STATIC_CAST(nsHTMLContainerFrame*,
                                                      kidNextInFlow->GetParent());
        parent->DeleteNextInFlowChild(mPresContext, kidNextInFlow);
      }
    }

    // See if we can place the frame. If we can't fit it, then we
    // return now.
    if (CanPlaceFrame(pfd, reflowState, notSafeToBreak, metrics, aReflowStatus)) {
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
      aPushedFrame = PR_TRUE;
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

  if (aFrame->GetStateBits() & NS_FRAME_IS_BIDI) {
    // Since aReflowStatus may change, check it at the end
    if (NS_INLINE_IS_BREAK_BEFORE(aReflowStatus) ) {
      aFrame->AdjustOffsetsForBidi(start, end);
    }
    else if (!NS_FRAME_IS_COMPLETE(aReflowStatus) ) {
      PRInt32 newEnd;
      aFrame->GetOffsets(start, newEnd);
      if (newEnd != end) {
        nsIFrame* nextInFlow;
        aFrame->GetNextInFlow(&nextInFlow);
        if (nextInFlow) {
          nextInFlow->GetOffsets(start, end);
          nextInFlow->AdjustOffsetsForBidi(newEnd, end);
        } // nextInFlow
      } // newEnd != end
    } // !NS_FRAME_IS_COMPLETE(aReflowStatus)
  } // isBidiFrame

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

  // Only apply start-margin on the first-in flow for inline frames
  if (HasPrevInFlow(pfd->mFrame)) {
    // Zero this out so that when we compute the max-element-width of
    // the frame we will properly avoid adding in the starting margin.
    if (ltr)
      pfd->mMargin.left = 0;
    else
      pfd->mMargin.right = 0;
  }

  if (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth){
    // Adjust available width to account for the left margin. The
    // right margin will be accounted for when we finish flowing the
    // frame.
    aReflowState.availableWidth -= ltr ? pfd->mMargin.left : pfd->mMargin.right;
  }

  if (ltr)
    pfd->mBounds.x += pfd->mMargin.left;
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
                            PRBool aNotSafeToBreak,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus)
{
  NS_PRECONDITION(pfd && pfd->mFrame, "bad args, null pointers for frame data");
  // Compute right margin to use
  if (0 != pfd->mBounds.width) {
    NS_ASSERTION(aReflowState.mStyleDisplay->mFloats == NS_STYLE_FLOAT_NONE,
                 "How'd we get a floated inline frame? "
                 "The frame ctor should've dealt with this.");

    // XXXwaterson this is probably not exactly right; e.g., embeddings, etc.
    PRBool ltr = (NS_STYLE_DIRECTION_LTR == aReflowState.mStyleVisibility->mDirection);

    if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
      // Only apply end margin for the last-in-flow. Zero this out so
      // that when we compute the max-element-width of the frame we
      // will properly avoid adding in the end margin.
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

#ifdef NOISY_CAN_PLACE_FRAME
  if (nsnull != psd->mFrame) {
    nsFrame::ListTag(stdout, psd->mFrame->mFrame);
  }
  else {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
  } 
  printf(": aNotSafeToBreak=%s frame=", aNotSafeToBreak ? "true" : "false");
  nsFrame::ListTag(stdout, pfd->mFrame);
  printf(" frameWidth=%d\n", pfd->mBounds.XMost() + rightMargin - psd->mX);
#endif

  // Set outside to PR_TRUE if the result of the reflow leads to the
  // frame sticking outside of our available area.
  PRBool outside = pfd->mBounds.XMost() + pfd->mMargin.right > psd->mRightEdge;
  if (!outside) {
    // If it fits, it fits
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> inside\n");
#endif
    return PR_TRUE;
  }

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
  if (nsLayoutAtoms::brFrame == pfd->mFrame->GetType()) {
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> BR frame fits\n");
#endif
    return PR_TRUE;
  }
#endif

  if (aNotSafeToBreak) {
    // There are no frames on the line or we are in the first word on
    // the line. If the line isn't impacted by a float then the
    // current frame fits.
    if (!GetFlag(LL_IMPACTEDBYFLOATS)) {
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
    else if (GetFlag(LL_LASTFLOATWASLETTERFRAME)) {
      // Another special case: see if the float is a letter
      // frame. If it is, then allow the frame next to it to fit.
      if (pfd->GetFlag(PFD_ISNONEMPTYTEXTFRAME)) {
        // This must be the first piece of non-empty text (because
        // aNotSafeToBreak is true) or its a piece of text that is
        // part of a larger word.
        pfd->SetFlag(PFD_ISSTICKY, PR_TRUE);
      }
      else if (pfd->mSpan) {
        PerFrameData* pf = pfd->mSpan->mFirstFrame;
        while (pf) {
          if (pf->GetFlag(PFD_ISSTICKY)) {
            // If one of the spans children was sticky then the span
            // itself is sticky.
            pfd->SetFlag(PFD_ISSTICKY, PR_TRUE);
          }
          pf = pf->mNext;
        }
      }

      if (pfd->GetFlag(PFD_ISSTICKY)) {
#ifdef NOISY_CAN_PLACE_FRAME
        printf("   ==> last float was letter frame && frame is sticky\n");
#endif
        return PR_TRUE;
      }
    }
  }

  // If this is a piece of text inside a letter frame...
  if (pfd->GetFlag(PFD_ISNONEMPTYTEXTFRAME)) {
    if (psd->mFrame && psd->mFrame->GetFlag(PFD_ISLETTERFRAME)) {
      nsIFrame* prevInFlow;
      psd->mFrame->mFrame->GetPrevInFlow(&prevInFlow);
      if (prevInFlow) {
        nsIFrame* prevPrevInFlow;
        prevInFlow->GetPrevInFlow(&prevPrevInFlow);
        if (!prevPrevInFlow) {
          // And it's the first continuation of the letter frame...
          // Then make sure that the text fits
          return PR_TRUE;
        }
      }
    }
  }
  else if (pfd->GetFlag(PFD_ISLETTERFRAME)) {
    // If this is the first continuation of the letter frame...
    nsIFrame* prevInFlow;
    pfd->mFrame->GetPrevInFlow(&prevInFlow);
    if (prevInFlow) {
      nsIFrame* prevPrevInFlow;
      prevInFlow->GetPrevInFlow(&prevPrevInFlow);
      if (!prevPrevInFlow) {
        return PR_TRUE;
      }
    }
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
    // (otherwise its a "below current line" flaoter and will be placed
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
  pfd->mAscent = aMetrics.ascent;
  pfd->mDescent = aMetrics.descent;

  // If the band was updated during the reflow of that frame then we
  // need to adjust any prior frames that were reflowed.
  if (GetFlag(LL_UPDATEDBAND) && InBlockContext()) {
    UpdateFrames();
    SetFlag(LL_UPDATEDBAND, PR_FALSE);
  }

  // Advance to next X coordinate
  psd->mX = pfd->mBounds.XMost() + pfd->mMargin.right;
  psd->mRightEdge = PR_MAX(psd->mRightEdge, psd->mX);

  // If the frame is a not aware of white-space and it takes up some
  // width, disable leading white-space compression for the next frame
  // to be reflowed.
  if ((!GetFlag(LL_UNDERSTANDSNWHITESPACE)) && pfd->mBounds.width) {
    SetFlag(LL_ENDSINWHITESPACE, PR_FALSE);
  }

  // Count the number of frames on the line...
  mTotalPlacedFrames++;
  if (psd->mX != psd->mLeftEdge || pfd->mBounds.x != psd->mLeftEdge) {
    // As soon as a frame placed on the line advances an X coordinate
    // of any span we can no longer place a float on the line.
    SetFlag(LL_CANPLACEFLOAT, PR_FALSE);
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
    pfd->mFlags = 0;  // all flags default to false
    pfd->SetFlag(PFD_ISBULLET, PR_TRUE);
    pfd->mAscent = aMetrics.ascent;
    pfd->mDescent = aMetrics.descent;

    // Note: y value will be updated during vertical alignment
    pfd->mBounds = aFrame->GetRect();
    pfd->mCombinedArea = aMetrics.mOverflowArea;
    if (mComputeMaxElementWidth) {
      pfd->mMaxElementWidth = aMetrics.width;
    }
  }
  return rv;
}

#ifdef DEBUG
void
nsLineLayout::DumpPerSpanData(PerSpanData* psd, PRInt32 aIndent)
{
  nsFrame::IndentBy(stdout, aIndent);
  printf("%p: left=%d x=%d right=%d\n", NS_STATIC_CAST(void*, psd),
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

PRBool
nsLineLayout::IsPercentageUnitSides(const nsStyleSides* aSides)
{
  return eStyleUnit_Percent == aSides->GetLeftUnit()
      || eStyleUnit_Percent == aSides->GetRightUnit()
      || eStyleUnit_Percent == aSides->GetTopUnit()
      || eStyleUnit_Percent == aSides->GetBottomUnit();
}

PRBool
nsLineLayout::IsPercentageAwareReplacedElement(nsPresContext *aPresContext, 
                                               nsIFrame* aFrame)
{
  if (aFrame->GetStateBits() & NS_FRAME_REPLACED_ELEMENT)
  {
    nsIAtom* frameType = aFrame->GetType();
    if (nsLayoutAtoms::brFrame != frameType && 
        nsLayoutAtoms::textFrame != frameType)
    {
      const nsStyleMargin* margin = aFrame->GetStyleMargin();
      if (IsPercentageUnitSides(&margin->mMargin)) {
        return PR_TRUE;
      }

      const nsStylePadding* padding = aFrame->GetStylePadding();
      if (IsPercentageUnitSides(&padding->mPadding)) {
        return PR_TRUE;
      }

      const nsStyleBorder* border = aFrame->GetStyleBorder();
      if (IsPercentageUnitSides(&border->mBorder)) {
        return PR_TRUE;
      }

      const nsStylePosition* pos = aFrame->GetStylePosition();
      if (eStyleUnit_Percent == pos->mWidth.GetUnit()
        || eStyleUnit_Percent == pos->mMaxWidth.GetUnit()
        || eStyleUnit_Percent == pos->mMinWidth.GetUnit()
        || eStyleUnit_Percent == pos->mHeight.GetUnit()
        || eStyleUnit_Percent == pos->mMinHeight.GetUnit()
        || eStyleUnit_Percent == pos->mMaxHeight.GetUnit()
        || IsPercentageUnitSides(&pos->mOffset)) { // XXX need more here!!!
        return PR_TRUE;
      }
    }
  }

  return PR_FALSE;
}

PRBool IsPercentageAwareFrame(nsPresContext *aPresContext, nsIFrame *aFrame)
{
  if (aFrame->GetStateBits() & NS_FRAME_REPLACED_ELEMENT) {
    if (nsLineLayout::IsPercentageAwareReplacedElement(aPresContext, aFrame)) {
      return PR_TRUE;
    }
  }
  else
  {
    nsIFrame *child = aFrame->GetFirstChild(nsnull);
    if (child)
    { // aFrame is an inline container frame, check my frame state
      if (aFrame->GetStateBits() & NS_INLINE_FRAME_CONTAINS_PERCENT_AWARE_CHILD) {
        return PR_TRUE;
      }
    }
    // else it's a frame we just don't care about
  }  
  return PR_FALSE;
}


void
nsLineLayout::VerticalAlignLine(nsLineBox* aLineBox,
                                nscoord* aMaxElementWidthResult)
{
  // Synthesize a PerFrameData for the block frame
  PerFrameData rootPFD;
  rootPFD.mFrame = mBlockReflowState->frame;
  rootPFD.mFrameType = mBlockReflowState->mFrameType;
  rootPFD.mAscent = 0;
  rootPFD.mDescent = 0;
  mRootSpan->mFrame = &rootPFD;
  mLineBox = aLineBox;

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
  PerFrameData* pfd = psd->mFirstFrame;
  nscoord maxElementWidth = 0;
  PRBool prevFrameAccumulates = PR_FALSE;
  nscoord accumulatedWidth = 0;
#ifdef HACK_MEW
  PRBool strictMode = InStrictMode();
  PRBool inUnconstrainedTable = InUnconstrainedTableCell(*mBlockReflowState);
#endif
#ifdef DEBUG
  int frameCount = 0;
#endif

  nscoord indent = mTextIndent; // Used for the first frame.

  while (nsnull != pfd) {

    // Compute max-element-width if necessary
    if (mComputeMaxElementWidth) {

      nscoord mw = pfd->mMaxElementWidth +
        pfd->mMargin.left + pfd->mMargin.right + indent;
      // Zero |indent| after including the 'text-indent' only for the
      // frame that is indented.
      indent = 0;

      if (psd->mNoWrap) {
        maxElementWidth += mw;
      }
      else {

#ifdef HACK_MEW

#ifdef DEBUG
      if (nsBlockFrame::gNoisyMaxElementWidth) 
        frameCount++;
#endif
        // if in Quirks mode and in a table cell with an unconstrained width, then emulate an IE
        // quirk to keep consecutive images from breaking the line
        // - see bugs 54565, 32191, and their many dups
        // XXX - reconsider how textFrame text measurement happens and have it take into account
        //       image frames as well, thus eliminating the need for this code
        if (!strictMode && inUnconstrainedTable ) {

          nscoord imgSizes = AccumulateImageSizes(*mPresContext, *pfd->mFrame);
          PRBool curFrameAccumulates = (imgSizes > 0) || 
                                       (pfd->mMaxElementWidth == pfd->mBounds.width &&
                                        pfd->GetFlag(PFD_ISNONWHITESPACETEXTFRAME));
            // NOTE: we check for the maxElementWidth == the boundsWidth to detect when
            //       a textframe has whitespace in it and thus should not be used as the basis
            //       for accumulating the image width
            // - this is to handle images in a text run

          if(prevFrameAccumulates && curFrameAccumulates) {
            accumulatedWidth += mw;
          } else {
            accumulatedWidth = mw;
          } 
          // now update the prevFrame
          prevFrameAccumulates = curFrameAccumulates;
        
#ifdef DEBUG
          if (nsBlockFrame::gNoisyMaxElementWidth) {
            nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
            printf("(%d) last frame's MEW=%d | Accumulated MEW=%d\n", frameCount, mw, accumulatedWidth);
          }
#endif 

          mw = accumulatedWidth;
        }

#endif // HACK_MEW

        // and finally reset the max element width
        if (maxElementWidth < mw) {
          maxElementWidth = mw;
        }
      }
    }
    PerSpanData* span = pfd->mSpan;
#ifdef DEBUG
    NS_ASSERTION(0xFF != pfd->mVerticalAlign, "umr");
#endif
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
    printf("  [child of line]");
    nsFrame::ListTag(stdout, pfd->mFrame);
    printf(": y=%d\n", pfd->mBounds.y);
#endif
    if (span) {
      nscoord distanceFromTop = pfd->mBounds.y - mTopEdge;
      PlaceTopBottomFrames(span, distanceFromTop, lineHeight);
    }
    // check to see if the frame is an inline replace element
    // and if it is percent-aware.  If so, mark the line.
    if ((PR_FALSE==aLineBox->ResizeReflowOptimizationDisabled()) &&
         pfd->mFrameType & NS_CSS_FRAME_TYPE_INLINE)
    {
      if (IsPercentageAwareFrame(mPresContext, pfd->mFrame))
        aLineBox->DisableResizeReflowOptimization();
    }
    pfd = pfd->mNext;
  }

  // Fill in returned line-box and max-element-width data
  aLineBox->mBounds.x = psd->mLeftEdge;
  aLineBox->mBounds.y = mTopEdge;
  aLineBox->mBounds.width = psd->mX - psd->mLeftEdge;
  aLineBox->mBounds.height = lineHeight;
  mFinalLineHeight = lineHeight;
  *aMaxElementWidthResult = maxElementWidth;
  aLineBox->SetAscent(baselineY - mTopEdge);
#ifdef NOISY_VERTICAL_ALIGN
  printf(
    "  [line]==> bounds{x,y,w,h}={%d,%d,%d,%d} lh=%d a=%d mew=%d\n",
    aLineBox->mBounds.x, aLineBox->mBounds.y,
    aLineBox->mBounds.width, aLineBox->mBounds.height,
    mFinalLineHeight, aLineBox->GetAscent(),
    *aMaxElementWidthResult);
#endif

  // Undo root-span mFrame pointer to prevent brane damage later on...
  mRootSpan->mFrame = nsnull;
  mLineBox = nsnull;
}

void
nsLineLayout::PlaceTopBottomFrames(PerSpanData* psd,
                                   nscoord aDistanceFromTop,
                                   nscoord aLineHeight)
{
  PerFrameData* pfd = psd->mFirstFrame;
  while (nsnull != pfd) {
    PerSpanData* span = pfd->mSpan;
#ifdef DEBUG
    NS_ASSERTION(0xFF != pfd->mVerticalAlign, "umr");
#endif
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

#define VERTICAL_ALIGN_FRAMES_NO_MINIMUM 32767
#define VERTICAL_ALIGN_FRAMES_NO_MAXIMUM -32768

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
  nsIRenderingContext* rc = mBlockReflowState->rendContext;
  SetFontFromStyle(mBlockReflowState->rendContext, styleContext);
  nsCOMPtr<nsIFontMetrics> fm;
  rc->GetFontMetrics(*getter_AddRefs(fm));

  PRBool preMode = (mStyleText->mWhiteSpace == NS_STYLE_WHITESPACE_PRE) ||
    (mStyleText->mWhiteSpace == NS_STYLE_WHITESPACE_MOZ_PRE_WRAP);

  // See if the span is an empty continuation. It's an empty continuation iff:
  // - it has a prev-in-flow
  // - it has no next in flow
  // - it's zero sized
  nsIFrame* spanNextInFlow;
  spanFrame->GetNextInFlow(&spanNextInFlow);
  nsIFrame* spanPrevInFlow;
  spanFrame->GetPrevInFlow(&spanPrevInFlow);
  PRBool emptyContinuation = spanPrevInFlow && !spanNextInFlow &&
    (0 == spanFramePFD->mBounds.width) && (0 == spanFramePFD->mBounds.height);

#ifdef NOISY_VERTICAL_ALIGN
  printf("[%sSpan]", (psd == mRootSpan)?"Root":"");
  nsFrame::ListTag(stdout, spanFrame);
  printf(": preMode=%s strictMode=%s w/h=%d,%d emptyContinuation=%s",
         preMode ? "yes" : "no",
         InStrictMode() ? "yes" : "no",
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
  if ((emptyContinuation || mCompatMode != eCompatibility_FullStandards) &&
      ((psd == mRootSpan) ||
       ((0 == spanFramePFD->mBorderPadding.top) &&
        (0 == spanFramePFD->mBorderPadding.right) &&
        (0 == spanFramePFD->mBorderPadding.bottom) &&
        (0 == spanFramePFD->mBorderPadding.left) &&
        (0 == spanFramePFD->mMargin.top) &&
        (0 == spanFramePFD->mMargin.right) &&
        (0 == spanFramePFD->mMargin.bottom) &&
        (0 == spanFramePFD->mMargin.left)))) {
    // This code handles an issue with compatability with non-css
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
    nscoord logicalHeight =
      nsHTMLReflowState::CalcLineHeight(mPresContext, rc, spanFrame);
    nscoord contentHeight = spanFramePFD->mBounds.height -
      spanFramePFD->mBorderPadding.top - spanFramePFD->mBorderPadding.bottom;
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
    baselineY = spanFramePFD->mAscent;


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

    // sanity check (see bug 105168, non-reproducable crashes from null frame)
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
    const nsStyleTextReset* textStyle = frame->GetStyleTextReset();
    nsStyleUnit verticalAlignUnit = textStyle->mVerticalAlign.GetUnit();
#ifdef NOISY_VERTICAL_ALIGN
    printf("  [frame]");
    nsFrame::ListTag(stdout, frame);
    printf(": verticalAlignUnit=%d (enum == %d)\n",
           verticalAlignUnit,
           ((eStyleUnit_Enumerated == verticalAlignUnit)
            ? textStyle->mVerticalAlign.GetIntValue()
            : -1));
#endif

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
          nsHTMLReflowState::CalcLineHeight(mPresContext, rc, frame);
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
        nsStyleUnit lhUnit = frame->GetStyleText()->mLineHeight.GetUnit();
        canUpdate = lhUnit == eStyleUnit_Normal || lhUnit == eStyleUnit_Null;
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
            GetCompatMode() != eCompatibility_FullStandards &&
            !logicalHeight) {
          // Check if it's a BR frame that is not alone on its line (it
          // is given a height of zero to indicate this), and if so reset
          // yTop and yBottom so that BR frames don't influence the line.
          if (nsLayoutAtoms::brFrame == frame->GetType()) {
            yTop = VERTICAL_ALIGN_FRAMES_NO_MINIMUM;
            yBottom = VERTICAL_ALIGN_FRAMES_NO_MAXIMUM;
          }
        }
        if (yTop < minY) minY = yTop;
        if (yBottom > maxY) maxY = yBottom;
#ifdef NOISY_VERTICAL_ALIGN
        printf("     [frame]raw: a=%d d=%d h=%d bp=%d,%d logical: h=%d leading=%d y=%d minY=%d maxY=%d\n",
               pfd->mAscent, pfd->mDescent, pfd->mBounds.height,
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
    //  (2) if this is the first line of an LI element (whether or not
    //      there is a bullet (NN4/IE5 quirk)
    //  (3) if this is the last line of an LI, DT, or DD element
    //      (The last line before a block also counts, but not before a
    //      BR) (NN4/IE5 quirk)
    PRBool applyMinLH = !(psd->mZeroEffectiveSpanBox); // (1) above
    PRBool isFirstLine = !mLineNumber; // if the line number is 0
    PRBool isLastLine = (!mLineBox->IsLineWrapped() && !GetFlag(LL_LINEENDSINBR));
    PRBool foundLI = PR_FALSE;  // hack to fix bug 50480.
    //XXX: rather than remembering if we've found an LI, we really should be checking
    //     for the existence of a bullet frame.  Likewise, the code below should not
    //     be checking for any particular content tag type, but rather should
    //     be checking for the existence of a bullet frame to determine if it's a list element or not.
    if (!applyMinLH && (isFirstLine || isLastLine)) {
      nsIContent* blockContent = mRootSpan->mFrame->mFrame->GetContent();
      if (blockContent) {
        nsIAtom *blockTagAtom = blockContent->Tag();
        // (2) above, if the first line of LI
        if (isFirstLine && blockTagAtom == nsHTMLAtoms::li) {
          // if the line is empty, then don't force the min height
          // (see bug 75963)
          if (!IsZeroHeight()) {
            applyMinLH = PR_TRUE;
            foundLI = PR_TRUE;
          }
        }
        // (3) above, if the last line of LI, DT, or DD
        else if (!applyMinLH && isLastLine &&
                 ((blockTagAtom == nsHTMLAtoms::li) ||
                  (blockTagAtom == nsHTMLAtoms::dt) ||
                  (blockTagAtom == nsHTMLAtoms::dd))) {
          applyMinLH = PR_TRUE;
        }
      }
    }
    if (applyMinLH) {
      if ((psd->mX != psd->mLeftEdge) || preMode || foundLI) {
#ifdef NOISY_VERTICAL_ALIGN
        printf("  [span]==> adjusting min/maxY: currentValues: %d,%d", minY, maxY);
#endif
        nscoord minimumLineHeight = mMinLineHeight;
        nscoord fontAscent, fontHeight;
        fm->GetMaxAscent(fontAscent);
        if (nsHTMLReflowState::UseComputedHeight()) {
          fontHeight = spanFrame->GetStyleFont()->mFont.size;
        }
        else 
        {
          fm->GetHeight(fontHeight);
        }

        nscoord leading = minimumLineHeight - fontHeight;
        nscoord yTop = -fontAscent - leading/2;
        nscoord yBottom = yTop + minimumLineHeight;
        if (yTop < minY) minY = yTop;
        if (yBottom > maxY) maxY = yBottom;

#ifdef NOISY_VERTICAL_ALIGN
        printf(" new values: %d,%d\n", minY, maxY);
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
      (maxY == VERTICAL_ALIGN_FRAMES_NO_MINIMUM)) {
    minY = maxY = baselineY;
  }

  if ((psd != mRootSpan) && (psd->mZeroEffectiveSpanBox)) {
#ifdef NOISY_VERTICAL_ALIGN
    printf("   [span]adjusting for zeroEffectiveSpanBox\n");
    printf("     Original: minY=%d, maxY=%d, height=%d, ascent=%d, descent=%d, logicalHeight=%d, topLeading=%d, bottomLeading=%d\n",
           minY, maxY, spanFramePFD->mBounds.height,
           spanFramePFD->mAscent, spanFramePFD->mDescent,
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
      spanFramePFD->mDescent -= adjust;
      psd->mBottomLeading += adjust;
    }
#ifdef NOISY_VERTICAL_ALIGN
    printf("     New: minY=%d, maxY=%d, height=%d, ascent=%d, descent=%d, logicalHeight=%d, topLeading=%d, bottomLeading=%d\n",
           minY, maxY, spanFramePFD->mBounds.height,
           spanFramePFD->mAscent, spanFramePFD->mDescent,
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
          }
        }
        return PR_TRUE;
      }
    }
    else if (!pfd->GetFlag(PFD_ISTEXTFRAME) &&
             !pfd->GetFlag(PFD_ISPLACEHOLDERFRAME)) {
      // If we hit a frame on the end that's not text and not a placeholder,
      // then there is no trailing whitespace to trim. Stop the search.
      *aDeltaWidth = 0;
      return PR_TRUE;
    }
    else if (pfd->GetFlag(PFD_ISNONEMPTYTEXTFRAME)) {
      nscoord deltaWidth = 0;
      pfd->mFrame->TrimTrailingWhiteSpace(mPresContext,
                                          *mBlockReflowState->rendContext,
                                          deltaWidth);
#ifdef NOISY_TRIM
      nsFrame::ListTag(stdout, (psd == mRootSpan
                                ? mBlockReflowState->frame
                                : psd->mFrame->mFrame));
      printf(": trim of ");
      nsFrame::ListTag(stdout, pfd->mFrame);
      printf(" returned %d\n", deltaWidth);
#endif
      if (deltaWidth) {
        if (pfd->mJustificationNumSpaces > 0) {
          pfd->mJustificationNumSpaces--;
        }

        pfd->mBounds.width -= deltaWidth;
        if (0 == pfd->mBounds.width) {
          pfd->mMaxElementWidth = 0;
        }

        // See if the text frame has already been placed in its parent
        if (psd != mRootSpan) {
          // The frame was already placed during psd's
          // reflow. Update the frames rectangle now.
          pfd->mFrame->SetRect(pfd->mBounds);
        }

        // Adjust containing span's right edge
        psd->mX -= deltaWidth;

        // Slide any frames that follow the text frame over by the
        // right amount. The only thing that can follow the text
        // frame is empty stuff, so we are just making things
        // sensible (keeping the combined area honest).
        while (pfd->mNext) {
          pfd = pfd->mNext;
          pfd->mBounds.x -= deltaWidth;
        }
      }

      // Pass up to caller so they can shrink their span
      *aDeltaWidth = deltaWidth;
      return PR_TRUE;
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
            aState->mTotalNumSpaces > 0 &&            // we divide by this value, so must be non-zero
            aState->mTotalNumLetters >0               // we divide by this value, so must be non-zero
           ) {
          aState->mNumSpacesProcessed += pfd->mJustificationNumSpaces;

          nscoord newAllocatedWidthForSpaces =
            (aState->mTotalWidthForSpaces*aState->mNumSpacesProcessed)
              /aState->mTotalNumSpaces;
          
          dw += newAllocatedWidthForSpaces - aState->mWidthForSpacesProcessed;

          aState->mWidthForSpacesProcessed = newAllocatedWidthForSpaces;
        }

        if (aState->mTotalWidthForLetters > 0) {
          aState->mNumLettersProcessed += pfd->mJustificationNumLetters;

          nscoord newAllocatedWidthForLetters =
            (aState->mTotalWidthForLetters*aState->mNumLettersProcessed)
              /aState->mTotalNumLetters;
          
          dw += newAllocatedWidthForLetters - aState->mWidthForLettersProcessed;

          aState->mWidthForLettersProcessed = newAllocatedWidthForLetters;
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

PRBool
nsLineLayout::HorizontalAlignFrames(nsRect& aLineBounds,
                                    PRBool aAllowJustify,
                                    PRBool aShrinkWrapWidth)
{
  PerSpanData* psd = mRootSpan;
  nscoord availWidth = psd->mRightEdge;
  if (NS_UNCONSTRAINEDSIZE == availWidth) {
    // Don't bother horizontal aligning on pass1 table reflow
#ifdef NOISY_HORIZONTAL_ALIGN
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": skipping horizontal alignment in pass1 table reflow\n");
#endif
    return PR_TRUE;
  }
  availWidth -= psd->mLeftEdge;
  nscoord remainingWidth = availWidth - aLineBounds.width;
#ifdef NOISY_HORIZONTAL_ALIGN
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": availWidth=%d lineWidth=%d delta=%d\n",
           availWidth, aLineBounds.width, remainingWidth);
#endif
#ifdef IBMBIDI
  nscoord dx = 0;
#endif

  // XXXldb What if it's less than 0??
  if (remainingWidth > 0)
  {
#ifndef IBMBIDI
    nscoord dx = 0;
#endif
    switch (mTextAlign) {
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

      case NS_STYLE_TEXT_ALIGN_LEFT:
        break;

      case NS_STYLE_TEXT_ALIGN_JUSTIFY:
        // If this is not the last line then go ahead and justify the
        // frames in the line. If it is the last line then if the
        // direction is right-to-left then we right-align the frames.
        if (aAllowJustify) {
          if (!aShrinkWrapWidth) {
            PRInt32 numSpaces;
            PRInt32 numLetters;
            
            ComputeJustificationWeights(psd, &numSpaces, &numLetters);

            if (numSpaces > 0) {
              FrameJustificationState state = { numSpaces, numLetters, remainingWidth, 0, 0, 0, 0, 0 };

              ApplyFrameJustification(psd, &state);
            }
          }
        }
        else if (NS_STYLE_DIRECTION_RTL == psd->mDirection) {
          // right align the frames
          dx = remainingWidth;
        }
        break;

      case NS_STYLE_TEXT_ALIGN_CENTER:
      case NS_STYLE_TEXT_ALIGN_MOZ_CENTER:
        dx = remainingWidth / 2;
        break;
    }
#ifdef IBMBIDI
  }
  // If we need to move the frames but we're shrink wrapping, then
  // we need to wait until the final width is known
  if (aShrinkWrapWidth) {
    return PR_FALSE;
  }
  PRBool isRTL = ( (NS_STYLE_DIRECTION_RTL == psd->mDirection)
                && (!psd->mChangedFrameDirection) );
  if (dx || isRTL) {
    PerFrameData* bulletPfd = nsnull;
    nscoord maxX = aLineBounds.XMost() + dx;
    PRBool isVisualRTL = PR_FALSE;

    if (isRTL) {
      if (psd->mLastFrame->GetFlag(PFD_ISBULLET) )
        bulletPfd = psd->mLastFrame;
  
      psd->mChangedFrameDirection = PR_TRUE;

      isVisualRTL = mPresContext->IsVisualMode();
    }
    if (dx || isVisualRTL) {
#else
    if (0 != dx) {
      // If we need to move the frames but we're shrink wrapping, then
      // we need to wait until the final width is known
      if (aShrinkWrapWidth) {
        return PR_FALSE;
      }
#endif
      for (PerFrameData* pfd = psd->mFirstFrame; pfd
#ifdef IBMBIDI
           && bulletPfd != pfd
#endif
           ; pfd = pfd->mNext) {
#ifdef IBMBIDI
        if (isVisualRTL) {
          // XXXldb Ugh.  Could we handle this earlier so we don't get here?
          maxX = pfd->mBounds.x = maxX - (pfd->mMargin.left + pfd->mBounds.width + pfd->mMargin.right);
        }
        else
#endif // IBMBIDI
          pfd->mBounds.x += dx;
        pfd->mFrame->SetRect(pfd->mBounds);
      }
      aLineBounds.x += dx;
    }
#ifndef IBMBIDI
    if ((NS_STYLE_DIRECTION_RTL == psd->mDirection) &&
        !psd->mChangedFrameDirection) {
      psd->mChangedFrameDirection = PR_TRUE;
  
      /* Assume that all frames have been right aligned.*/
      if (aShrinkWrapWidth) {
        return PR_FALSE;
      }
      PerFrameData* pfd = psd->mFirstFrame;
      PRUint32 maxX = psd->mRightEdge;
      while (nsnull != pfd) {
        pfd->mBounds.x = maxX - (pfd->mMargin.left + pfd->mBounds.width + pfd->mMargin.right);
        pfd->mFrame->SetRect(pfd->mBounds);
        maxX = pfd->mBounds.x;
        pfd = pfd->mNext;
      }
    }
#endif // ndef IBMBIDI
  }

  return PR_TRUE;
}

void
nsLineLayout::RelativePositionFrames(nsRect& aCombinedArea)
{
  RelativePositionFrames(mRootSpan, aCombinedArea);
}

void
nsLineLayout::RelativePositionFrames(PerSpanData* psd, nsRect& aCombinedArea)
{
  nsRect combinedAreaResult;
  if (nsnull != psd->mFrame) {
    // The span's overflow area comes in three parts:
    // -- this frame's width and height
    // -- the pfd->mCombinedArea, which is the area of a bullet or the union
    // of a relatively positioned frame's absolute children
    // -- the bounds of all inline descendants
    // The former two parts are computed right here, we gather the descendants
    // below.
    nsRect adjustedBounds(0, 0, psd->mFrame->mBounds.width,
                          psd->mFrame->mBounds.height);
    combinedAreaResult.UnionRect(psd->mFrame->mCombinedArea, adjustedBounds);
  }
  else {
    // The minimum combined area for the frames that are direct
    // children of the block starts at the upper left corner of the
    // line and is sized to match the size of the line's bounding box
    // (the same size as the values returned from VerticalAlignFrames)
    combinedAreaResult.x = psd->mLeftEdge;
    // If this turns out to be negative, the rect will be treated as empty.
    // Which is just fine.
    combinedAreaResult.width = psd->mX - combinedAreaResult.x;
    combinedAreaResult.y = mTopEdge;
    combinedAreaResult.height = mFinalLineHeight;
  }

  for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
    nsPoint origin = nsPoint(pfd->mBounds.x, pfd->mBounds.y);
    nsIFrame* frame = pfd->mFrame;

    // Adjust the origin of the frame
    if (pfd->GetFlag(PFD_RELATIVEPOS)) {
      // right and bottom are handled by
      // nsHTMLReflowState::ComputeRelativeOffsets
      nsPoint change(pfd->mOffsets.left, pfd->mOffsets.top);
      frame->SetPosition(frame->GetPosition() + change);
      origin += change;
    }

    // We must position the view correctly before positioning its
    // descendants so that widgets are positioned properly (since only
    // some views have widgets).
    if (frame->HasView())
      nsContainerFrame::SyncFrameViewAfterReflow(mPresContext, frame,
                                                 frame->GetView(),
                                                 &pfd->mCombinedArea, //ignored
                                                 NS_FRAME_NO_SIZE_VIEW);

    // Note: the combined area of a child is in its coordinate
    // system. We adjust the childs combined area into our coordinate
    // system before computing the aggregated value by adding in
    // <b>x</b> and <b>y</b> which were computed above.
    nsRect r;
    if (pfd->mSpan) {
      // Compute a new combined area for the child span before
      // aggregating it into our combined area.
      RelativePositionFrames(pfd->mSpan, r);
    } else {
      // For simple text frames we take the union of the combined area
      // and the width/height. I think the combined area should always
      // equal the bounds in this case, but this is safe.
      nsRect adjustedBounds(0, 0, pfd->mBounds.width, pfd->mBounds.height);
      r.UnionRect(pfd->mCombinedArea, adjustedBounds);

      // If we have something that's not an inline but with a complex frame
      // hierarchy inside that contains views, they need to be
      // positioned.
      // All descendant views must be repositioned even if this frame
      // does have a view in case this frame's view does not have a
      // widget and some of the descendant views do have widgets --
      // otherwise the widgets won't be repositioned.
      nsContainerFrame::PositionChildViews(mPresContext, frame);
    }

    // Do this here (rather than along with NS_FRAME_OUTSIDE_CHILDREN
    // handling below) so we get leaf frames as well.  No need to worry
    // about the root span, since it doesn't have a frame.
    if (frame->HasView())
      nsContainerFrame::SyncFrameViewAfterReflow(mPresContext, frame,
                                                 frame->GetView(), &r,
                                                 NS_FRAME_NO_MOVE_VIEW);

    combinedAreaResult.UnionRect(combinedAreaResult, r + origin);
  }

  // If we just computed a spans combined area, we need to update its
  // NS_FRAME_OUTSIDE_CHILDREN bit..
  if (psd->mFrame) {
    PerFrameData* spanPFD = psd->mFrame;
    nsIFrame* frame = spanPFD->mFrame;
    frame->FinishAndStoreOverflow(&combinedAreaResult, frame->GetSize());
  }
  aCombinedArea = combinedAreaResult;
}

void
nsLineLayout::ForgetWordFrame(nsIFrame* aFrame)
{
  if (mWordFrames && 0 != mWordFrames->GetSize()) {
    NS_ASSERTION((void*)aFrame == mWordFrames->PeekFront(), "forget-word-frame");
    mWordFrames->PopFront();
  }
}

nsIFrame*
nsLineLayout::FindNextText(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  // Grovel through the frame hierarchy to find a text frame that is
  // "adjacent" to aFrame.

  // So this is kind of funky. During reflow, overflow frames will
  // have their parent pointers set up lazily. We assume that, on
  // entry, aFrame has it's parent pointer set correctly (as do all of
  // its ancestors). Starting from that, we need to make sure that as
  // we traverse through frames trying to find the next text frame, we
  // leave the frames with their parent pointers set correctly, so the
  // *next* time we come through here, we're good to go.

  // Build a path from the enclosing block frame down to aFrame. We'll
  // use this to walk the frame tree. (XXXwaterson if I was clever, I
  // wouldn't need to build this up before hand, and could incorporate
  // this logic into the walking code directly.)
  nsAutoVoidArray stack;
  for (;;) {
    stack.InsertElementAt(aFrame, 0);

    aFrame = aFrame->GetParent();

    NS_ASSERTION(aFrame, "wow, no block frame found");
    if (! aFrame)
      break;

    if (NS_STYLE_DISPLAY_INLINE != aFrame->GetStyleDisplay()->mDisplay)
      break;
  }

  // Using the path we've built up, walk the frame tree looking for
  // the text frame that follows aFrame.
  PRInt32 count;
  while ((count = stack.Count()) != 0) {
    PRInt32 lastIndex = count - 1;
    nsIFrame* top = NS_STATIC_CAST(nsIFrame*, stack.ElementAt(lastIndex));

    // If this is a frame that'll break a word, then bail.
    PRBool canContinue;
    top->CanContinueTextRun(canContinue);
    if (! canContinue)
      return nsnull;

    // Advance to top's next sibling
    nsIFrame* next = top->GetNextSibling();

    if (! next) {
      // No more siblings. Pop the top element to walk back up the
      // frame tree.
      stack.RemoveElementAt(lastIndex);
      continue;
    }

    // We know top's parent is good, but next's might not be. So let's
    // set it to be sure.
    next->SetParent(top->GetParent());

    // Save next at the top of the stack...
    stack.ReplaceElementAt(next, lastIndex);

    // ...and prowl down to next's deepest child. We'll need to check
    // for potential run-busters "on the way down", too.
    for (;;) {
      next->CanContinueTextRun(canContinue);
      if (! canContinue)
        return nsnull;

      nsIFrame* child = next->GetFirstChild(nsnull);

      if (! child)
        break;

      stack.AppendElement(child);
      next = child;
    }

    // Ignore continuing frames
    if (HasPrevInFlow(next))
      continue;

    // If this is a text frame, return it.
    if (nsLayoutAtoms::textFrame == next->GetType())
      return next;
  }

  // If we get here, then there are no more text frames in this block.
  return nsnull;
}


PRBool
nsLineLayout::TreatFrameAsBlock(nsIFrame* aFrame)
{
  const nsStyleDisplay* display = aFrame->GetStyleDisplay();
  if (NS_STYLE_POSITION_ABSOLUTE == display->mPosition) {
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
