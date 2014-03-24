/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* state and methods used while laying out a single line of a block frame */

// This has to be defined before nsLineLayout.h is included, because
// nsLineLayout.h has a #include for plarena.h, which needs this defined:
#define PL_ARENA_CONST_ALIGN_MASK (sizeof(void*)-1)
#include "nsLineLayout.h"

#include "SVGTextFrame.h"
#include "nsBlockFrame.h"
#include "nsStyleConsts.h"
#include "nsContainerFrame.h"
#include "nsFloatManager.h"
#include "nsStyleContext.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsLayoutUtils.h"
#include "nsTextFrame.h"
#include "nsStyleStructInlines.h"
#include "nsBidiPresUtils.h"
#include <algorithm>

#ifdef DEBUG
#undef  NOISY_INLINEDIR_ALIGN
#undef  NOISY_BLOCKDIR_ALIGN
#undef  REALLY_NOISY_BLOCKDIR_ALIGN
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
    mLastOptionalBreakContent(nullptr),
    mForceBreakContent(nullptr),
    mBlockRS(nullptr),/* XXX temporary */
    mLastOptionalBreakPriority(gfxBreakPriority::eNoBreak),
    mLastOptionalBreakContentOffset(-1),
    mForceBreakContentOffset(-1),
    mMinLineBSize(0),
    mTextIndent(0),
    mFirstLetterStyleOK(false),
    mIsTopOfPage(false),
    mImpactedByFloats(false),
    mLastFloatWasLetterFrame(false),
    mLineIsEmpty(false),
    mLineEndsInBR(false),
    mNeedBackup(false),
    mInFirstLine(false),
    mGotLineBox(false),
    mInFirstLetter(false),
    mHasBullet(false),
    mDirtyNextLine(false),
    mLineAtStart(false)
{
  MOZ_ASSERT(aOuterReflowState, "aOuterReflowState must not be null");
  NS_ASSERTION(aFloatManager || aOuterReflowState->frame->GetType() ==
                                  nsGkAtoms::letterFrame,
               "float manager should be present");
  MOZ_COUNT_CTOR(nsLineLayout);

  // Stash away some style data that we need
  nsBlockFrame* blockFrame = do_QueryFrame(aOuterReflowState->frame);
  if (blockFrame)
    mStyleText = blockFrame->StyleTextForLineLayout();
  else
    mStyleText = aOuterReflowState->frame->StyleText();

  mLineNumber = 0;
  mTotalPlacedFrames = 0;
  mBStartEdge = 0;
  mTrimmableWidth = 0;

  mInflationMinFontSize =
    nsLayoutUtils::InflationMinFontSizeFor(aOuterReflowState->frame);

  // Instead of always pre-initializing the free-lists for frames and
  // spans, we do it on demand so that situations that only use a few
  // frames and spans won't waste a lot of time in unneeded
  // initialization.
  PL_INIT_ARENA_POOL(&mArena, "nsLineLayout", 1024);
  mFrameFreeList = nullptr;
  mSpanFreeList = nullptr;

  mCurrentSpan = mRootSpan = nullptr;
  mSpanDepth = 0;

  if (aLine) {
    mGotLineBox = true;
    mLineBox = *aLine;
  }
}

nsLineLayout::~nsLineLayout()
{
  MOZ_COUNT_DTOR(nsLineLayout);

  NS_ASSERTION(nullptr == mRootSpan, "bad line-layout user");

  PL_FinishArenaPool(&mArena);
}

// Find out if the frame has a non-null prev-in-flow, i.e., whether it
// is a continuation.
inline bool
HasPrevInFlow(nsIFrame *aFrame)
{
  nsIFrame *prevInFlow = aFrame->GetPrevInFlow();
  return prevInFlow != nullptr;
}

void
nsLineLayout::BeginLineReflow(nscoord aICoord, nscoord aBCoord,
                              nscoord aISize, nscoord aBSize,
                              bool aImpactedByFloats,
                              bool aIsTopOfPage,
                              WritingMode aWritingMode,
                              nscoord aContainerWidth)
{
  NS_ASSERTION(nullptr == mRootSpan, "bad linelayout user");
  NS_WARN_IF_FALSE(aISize != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
#ifdef DEBUG
  if ((aISize != NS_UNCONSTRAINEDSIZE) && CRAZY_SIZE(aISize)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": Init: bad caller: width WAS %d(0x%x)\n",
           aISize, aISize);
  }
  if ((aBSize != NS_UNCONSTRAINEDSIZE) && CRAZY_SIZE(aBSize)) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": Init: bad caller: height WAS %d(0x%x)\n",
           aBSize, aBSize);
  }
#endif
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": BeginLineReflow: %d,%d,%d,%d impacted=%s %s\n",
         aICoord, aBCoord, aISize, aBSize,
         aImpactedByFloats?"true":"false",
         aIsTopOfPage ? "top-of-page" : "");
#endif
#ifdef DEBUG
  mSpansAllocated = mSpansFreed = mFramesAllocated = mFramesFreed = 0;
#endif

  mFirstLetterStyleOK = false;
  mIsTopOfPage = aIsTopOfPage;
  mImpactedByFloats = aImpactedByFloats;
  mTotalPlacedFrames = 0;
  mLineIsEmpty = true;
  mLineAtStart = true;
  mLineEndsInBR = false;
  mSpanDepth = 0;
  mMaxStartBoxBSize = mMaxEndBoxBSize = 0;

  if (mGotLineBox) {
    mLineBox->ClearHasBullet();
  }

  PerSpanData* psd = NewPerSpanData();
  mCurrentSpan = mRootSpan = psd;
  psd->mReflowState = mBlockReflowState;
  psd->mIStart = aICoord;
  psd->mICoord = aICoord;
  psd->mIEnd = aICoord + aISize;
  mContainerWidth = aContainerWidth;

  // If we're in a constrained height frame, then we don't allow a
  // max line box width to take effect.
  if (!(LineContainerFrame()->GetStateBits() &
        NS_FRAME_IN_CONSTRAINED_HEIGHT)) {

    // If the available size is greater than the maximum line box width (if
    // specified), then we need to adjust the line box width to be at the max
    // possible width.
    nscoord maxLineBoxWidth =
      LineContainerFrame()->PresContext()->PresShell()->MaxLineBoxWidth();

    if (maxLineBoxWidth > 0 &&
        psd->mIEnd - psd->mIStart > maxLineBoxWidth) {
      psd->mIEnd = psd->mIStart + maxLineBoxWidth;
    }
  }

  mBStartEdge = aBCoord;

  psd->mNoWrap =
    !mStyleText->WhiteSpaceCanWrapStyle() || LineContainerFrame()->IsSVGText();
  psd->mWritingMode = aWritingMode;

  // If this is the first line of a block then see if the text-indent
  // property amounts to anything.

  if (0 == mLineNumber && !HasPrevInFlow(mBlockReflowState->frame)) {
    const nsStyleCoord &textIndent = mStyleText->mTextIndent;
    nscoord pctBasis = 0;
    if (textIndent.HasPercent()) {
      pctBasis =
        nsHTMLReflowState::GetContainingBlockContentWidth(mBlockReflowState);

      if (mGotLineBox) {
        mLineBox->DisableResizeReflowOptimization();
      }
    }
    nscoord indent = nsRuleNode::ComputeCoordPercentCalc(textIndent, pctBasis);

    mTextIndent = indent;

    psd->mICoord += indent;
  }
}

void
nsLineLayout::EndLineReflow()
{
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": EndLineReflow: width=%d\n", mRootSpan->mICoord - mRootSpan->mIStart);
#endif

  FreeSpan(mRootSpan);
  mCurrentSpan = mRootSpan = nullptr;

  NS_ASSERTION(mSpansAllocated == mSpansFreed, "leak");
  NS_ASSERTION(mFramesAllocated == mFramesFreed, "leak");

#if 0
  static int32_t maxSpansAllocated = NS_LINELAYOUT_NUM_SPANS;
  static int32_t maxFramesAllocated = NS_LINELAYOUT_NUM_FRAMES;
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
// on the line is placed. Each span can still have a per-span mICoord that
// tracks where a child frame is going in its span; they don't need a
// per-span mIStart?

void
nsLineLayout::UpdateBand(const nsRect& aNewAvailSpace,
                         nsIFrame* aFloatFrame)
{
  WritingMode lineWM = mRootSpan->mWritingMode;
  LogicalRect availSpace(lineWM, aNewAvailSpace, mContainerWidth);
#ifdef REALLY_NOISY_REFLOW
  printf("nsLL::UpdateBand %d, %d, %d, %d, (logical %d, %d, %d, %d); frame=%p\n  will set mImpacted to true\n",
         aNewAvailSpace.x, aNewAvailSpace.y,
         aNewAvailSpace.width, aNewAvailSpace.height,
         availSpace.IStart(lineWM), availSpace.BStart(lineWM),
         availSpace.ISize(lineWM), availSpace.BSize(lineWM),
         aFloatFrame);
#endif
#ifdef DEBUG
  if ((availSpace.ISize(lineWM) != NS_UNCONSTRAINEDSIZE) &&
      CRAZY_SIZE(availSpace.ISize(lineWM))) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": UpdateBand: bad caller: ISize WAS %d(0x%x)\n",
           availSpace.ISize(lineWM), availSpace.ISize(lineWM));
  }
  if ((availSpace.BSize(lineWM) != NS_UNCONSTRAINEDSIZE) &&
      CRAZY_SIZE(availSpace.BSize(lineWM))) {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
    printf(": UpdateBand: bad caller: BSize WAS %d(0x%x)\n",
           availSpace.BSize(lineWM), availSpace.BSize(lineWM));
  }
#endif

  // Compute the difference between last times width and the new width
  NS_WARN_IF_FALSE(mRootSpan->mIEnd != NS_UNCONSTRAINEDSIZE &&
                   aNewAvailSpace.width != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
  // The root span's mIStart moves to aICoord
  nscoord deltaICoord = availSpace.IStart(lineWM) - mRootSpan->mIStart;
  // The width of all spans changes by this much (the root span's
  // mIEnd moves to aICoord + aISize, its new width is aISize)
  nscoord deltaISize = availSpace.ISize(lineWM) -
                       (mRootSpan->mIEnd - mRootSpan->mIStart);
#ifdef NOISY_REFLOW
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": UpdateBand: %d,%d,%d,%d deltaISize=%d deltaICoord=%d\n",
         aNewAvailSpace.x, aNewAvailSpace.y,
         aNewAvailSpace.width, aNewAvailSpace.height, deltaISize, deltaICoord);
#endif

  // Update the root span position
  mRootSpan->mIStart += deltaICoord;
  mRootSpan->mIEnd += deltaICoord;
  mRootSpan->mICoord += deltaICoord;

  // Now update the right edges of the open spans to account for any
  // change in available space width
  for (PerSpanData* psd = mCurrentSpan; psd; psd = psd->mParent) {
    psd->mIEnd += deltaISize;
    psd->mContainsFloat = true;
#ifdef NOISY_REFLOW
    printf("  span %p: oldIEnd=%d newIEnd=%d\n",
           psd, psd->mIEnd - deltaISize, psd->mIEnd);
#endif
  }
  NS_ASSERTION(mRootSpan->mContainsFloat &&
               mRootSpan->mIStart == availSpace.IStart(lineWM) &&
               mRootSpan->mIEnd == availSpace.IEnd(lineWM),
               "root span was updated incorrectly?");

  // Update frame bounds
  // Note: Only adjust the outermost frames (the ones that are direct
  // children of the block), not the ones in the child spans. The reason
  // is simple: the frames in the spans have coordinates local to their
  // parent therefore they are moved when their parent span is moved.
  if (deltaICoord != 0) {
    for (PerFrameData* pfd = mRootSpan->mFirstFrame; pfd; pfd = pfd->mNext) {
      pfd->mBounds.IStart(lineWM) += deltaICoord;
    }
  }

  mBStartEdge = aNewAvailSpace.y;
  mImpactedByFloats = true;

  mLastFloatWasLetterFrame = nsGkAtoms::letterFrame == aFloatFrame->GetType();
}

nsLineLayout::PerSpanData*
nsLineLayout::NewPerSpanData()
{
  PerSpanData* psd = mSpanFreeList;
  if (!psd) {
    void *mem;
    PL_ARENA_ALLOCATE(mem, &mArena, sizeof(PerSpanData));
    if (!mem) {
      NS_RUNTIMEABORT("OOM");
    }
    psd = reinterpret_cast<PerSpanData*>(mem);
  }
  else {
    mSpanFreeList = psd->mNextFreeSpan;
  }
  psd->mParent = nullptr;
  psd->mFrame = nullptr;
  psd->mFirstFrame = nullptr;
  psd->mLastFrame = nullptr;
  psd->mContainsFloat = false;
  psd->mZeroEffectiveSpanBox = false;
  psd->mHasNonemptyContent = false;

#ifdef DEBUG
  mSpansAllocated++;
#endif
  return psd;
}

void
nsLineLayout::BeginSpan(nsIFrame* aFrame,
                        const nsHTMLReflowState* aSpanReflowState,
                        nscoord aIStart, nscoord aIEnd,
                        nscoord* aBaseline)
{
  NS_ASSERTION(aIEnd != NS_UNCONSTRAINEDSIZE,
               "should no longer be using unconstrained sizes");
#ifdef NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth+1);
  nsFrame::ListTag(stdout, aFrame);
  printf(": BeginSpan leftEdge=%d rightEdge=%d\n", aIStart, aIEnd);
#endif

  PerSpanData* psd = NewPerSpanData();
  // Link up span frame's pfd to point to its child span data
  PerFrameData* pfd = mCurrentSpan->mLastFrame;
  NS_ASSERTION(pfd->mFrame == aFrame, "huh?");
  pfd->mSpan = psd;

  // Init new span
  psd->mFrame = pfd;
  psd->mParent = mCurrentSpan;
  psd->mReflowState = aSpanReflowState;
  psd->mIStart = aIStart;
  psd->mICoord = aIStart;
  psd->mIEnd = aIEnd;
  psd->mBaseline = aBaseline;

  nsIFrame* frame = aSpanReflowState->frame;
  psd->mNoWrap = !frame->StyleText()->WhiteSpaceCanWrap(frame);
  psd->mWritingMode = aSpanReflowState->GetWritingMode();

  // Switch to new span
  mCurrentSpan = psd;
  mSpanDepth++;
}

nscoord
nsLineLayout::EndSpan(nsIFrame* aFrame)
{
  NS_ASSERTION(mSpanDepth > 0, "end-span without begin-span");
#ifdef NOISY_REFLOW
  nsFrame::IndentBy(stdout, mSpanDepth);
  nsFrame::ListTag(stdout, aFrame);
  printf(": EndSpan width=%d\n", mCurrentSpan->mICoord - mCurrentSpan->mIStart);
#endif
  PerSpanData* psd = mCurrentSpan;
  nscoord iSizeResult = psd->mLastFrame ? (psd->mICoord - psd->mIStart) : 0;

  mSpanDepth--;
  mCurrentSpan->mReflowState = nullptr;  // no longer valid so null it out!
  mCurrentSpan = mCurrentSpan->mParent;
  return iSizeResult;
}

int32_t
nsLineLayout::GetCurrentSpanCount() const
{
  NS_ASSERTION(mCurrentSpan == mRootSpan, "bad linelayout user");
  int32_t count = 0;
  PerFrameData* pfd = mRootSpan->mFirstFrame;
  while (nullptr != pfd) {
    count++;
    pfd = pfd->mNext;
  }
  return count;
}

void
nsLineLayout::SplitLineTo(int32_t aNewCount)
{
  NS_ASSERTION(mCurrentSpan == mRootSpan, "bad linelayout user");

#ifdef REALLY_NOISY_PUSHING
  printf("SplitLineTo %d (current count=%d); before:\n", aNewCount,
         GetCurrentSpanCount());
  DumpPerSpanData(mRootSpan, 1);
#endif
  PerSpanData* psd = mRootSpan;
  PerFrameData* pfd = psd->mFirstFrame;
  while (nullptr != pfd) {
    if (--aNewCount == 0) {
      // Truncate list at pfd (we keep pfd, but anything following is freed)
      PerFrameData* next = pfd->mNext;
      pfd->mNext = nullptr;
      psd->mLastFrame = pfd;

      // Now release all of the frames following pfd
      pfd = next;
      while (nullptr != pfd) {
        next = pfd->mNext;
        pfd->mNext = mFrameFreeList;
        mFrameFreeList = pfd;
#ifdef DEBUG
        mFramesFreed++;
#endif
        if (nullptr != pfd->mSpan) {
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
    psd->mFirstFrame = nullptr;
    psd->mLastFrame = nullptr;
  }
  else {
    PerFrameData* prevFrame = pfd->mPrev;
    prevFrame->mNext = nullptr;
    psd->mLastFrame = prevFrame;
  }

  // Now free it, and if it has a span, free that too
  pfd->mNext = mFrameFreeList;
  mFrameFreeList = pfd;
#ifdef DEBUG
  mFramesFreed++;
#endif
  if (nullptr != pfd->mSpan) {
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
  while (nullptr != pfd) {
    if (nullptr != pfd->mSpan) {
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

bool
nsLineLayout::IsZeroBSize()
{
  PerSpanData* psd = mCurrentSpan;
  PerFrameData* pfd = psd->mFirstFrame;
  while (nullptr != pfd) {
    if (0 != pfd->mBounds.BSize(psd->mWritingMode)) {
      return false;
    }
    pfd = pfd->mNext;
  }
  return true;
}

nsLineLayout::PerFrameData*
nsLineLayout::NewPerFrameData(nsIFrame* aFrame)
{
  PerFrameData* pfd = mFrameFreeList;
  if (!pfd) {
    void *mem;
    PL_ARENA_ALLOCATE(mem, &mArena, sizeof(PerFrameData));
    if (!mem) {
      NS_RUNTIMEABORT("OOM");
    }
    pfd = reinterpret_cast<PerFrameData*>(mem);
  }
  else {
    mFrameFreeList = pfd->mNext;
  }
  pfd->mSpan = nullptr;
  pfd->mNext = nullptr;
  pfd->mPrev = nullptr;
  pfd->mFlags = 0;  // all flags default to false
  pfd->mFrame = aFrame;

  WritingMode frameWM = aFrame->GetWritingMode();
  WritingMode lineWM = mRootSpan->mWritingMode;
  pfd->mBounds = LogicalRect(lineWM);
  pfd->mMargin = LogicalMargin(frameWM);
  pfd->mBorderPadding = LogicalMargin(frameWM);
  pfd->mOffsets = LogicalMargin(frameWM);

#ifdef DEBUG
  pfd->mBlockDirAlign = 0xFF;
  mFramesAllocated++;
#endif
  return pfd;
}

bool
nsLineLayout::LineIsBreakable() const
{
  // XXX mTotalPlacedFrames should go away and we should just use
  // mLineIsEmpty here instead
  if ((0 != mTotalPlacedFrames) || mImpactedByFloats) {
    return true;
  }
  return false;
}

// Checks all four sides for percentage units.  This means it should
// only be used for things (margin, padding) where percentages on top
// and bottom depend on the *width* just like percentages on left and
// right.
static bool
HasPercentageUnitSide(const nsStyleSides& aSides)
{
  NS_FOR_CSS_SIDES(side) {
    if (aSides.Get(side).HasPercent())
      return true;
  }
  return false;
}

static bool
IsPercentageAware(const nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "null frame is not allowed");

  nsIAtom *fType = aFrame->GetType();
  if (fType == nsGkAtoms::textFrame) {
    // None of these things can ever be true for text frames.
    return false;
  }

  // Some of these things don't apply to non-replaced inline frames
  // (that is, fType == nsGkAtoms::inlineFrame), but we won't bother making
  // things unnecessarily complicated, since they'll probably be set
  // quite rarely.

  const nsStyleMargin* margin = aFrame->StyleMargin();
  if (HasPercentageUnitSide(margin->mMargin)) {
    return true;
  }

  const nsStylePadding* padding = aFrame->StylePadding();
  if (HasPercentageUnitSide(padding->mPadding)) {
    return true;
  }

  // Note that borders can't be aware of percentages

  const nsStylePosition* pos = aFrame->StylePosition();

  if ((pos->WidthDependsOnContainer() &&
       pos->mWidth.GetUnit() != eStyleUnit_Auto) ||
      pos->MaxWidthDependsOnContainer() ||
      pos->MinWidthDependsOnContainer() ||
      pos->OffsetHasPercent(NS_SIDE_RIGHT) ||
      pos->OffsetHasPercent(NS_SIDE_LEFT)) {
    return true;
  }

  if (eStyleUnit_Auto == pos->mWidth.GetUnit()) {
    // We need to check for frames that shrink-wrap when they're auto
    // width.
    const nsStyleDisplay* disp = aFrame->StyleDisplay();
    if (disp->mDisplay == NS_STYLE_DISPLAY_INLINE_BLOCK ||
        disp->mDisplay == NS_STYLE_DISPLAY_INLINE_TABLE ||
        fType == nsGkAtoms::HTMLButtonControlFrame ||
        fType == nsGkAtoms::gfxButtonControlFrame ||
        fType == nsGkAtoms::fieldSetFrame ||
        fType == nsGkAtoms::comboboxDisplayFrame) {
      return true;
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
      const IntrinsicSize &intrinsicSize = f->GetIntrinsicSize();
      if (intrinsicSize.width.GetUnit() == eStyleUnit_None &&
          intrinsicSize.height.GetUnit() == eStyleUnit_None) {
        return true;
      }
    }
  }

  return false;
}

nsresult
nsLineLayout::ReflowFrame(nsIFrame* aFrame,
                          nsReflowStatus& aReflowStatus,
                          nsHTMLReflowMetrics* aMetrics,
                          bool& aPushedFrame)
{
  // Initialize OUT parameter
  aPushedFrame = false;

  PerFrameData* pfd = NewPerFrameData(aFrame);
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
  // (block-direction alignment, for example)
  WritingMode frameWM = aFrame->GetWritingMode();
  WritingMode lineWM = mRootSpan->mWritingMode;

  // NOTE: While the x coordinate remains relative to the parent span,
  // the y coordinate is fixed at the top edge for the line. During
  // BlockDirAlignFrames we will repair this so that the y coordinate
  // is properly set and relative to the appropriate span.
  pfd->mBounds.IStart(lineWM) = psd->mICoord;
  pfd->mBounds.BStart(lineWM) = mBStartEdge;

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
  bool notSafeToBreak = LineIsEmpty() && !mImpactedByFloats;

  // Figure out whether we're talking about a textframe here
  nsIAtom* frameType = aFrame->GetType();
  bool isText = frameType == nsGkAtoms::textFrame;
  
  // Compute the available size for the frame. This available width
  // includes room for the side margins.
  // For now, set the available height to unconstrained always.
  nsSize availSize(mBlockReflowState->ComputedWidth(), NS_UNCONSTRAINEDSIZE);

  // Inline-ish and text-ish things don't compute their width;
  // everything else does.  We need to give them an available width that
  // reflects the space left on the line.
  NS_WARN_IF_FALSE(psd->mIEnd != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
  nscoord availableSpaceOnLine = psd->mIEnd - psd->mICoord;

  // Setup reflow state for reflowing the frame
  Maybe<nsHTMLReflowState> reflowStateHolder;
  if (!isText) {
    reflowStateHolder.construct(mPresContext, *psd->mReflowState,
                                aFrame, availSize);
    nsHTMLReflowState& reflowState = reflowStateHolder.ref();
    reflowState.mLineLayout = this;
    reflowState.mFlags.mIsTopOfPage = mIsTopOfPage;
    if (reflowState.ComputedWidth() == NS_UNCONSTRAINEDSIZE)
      reflowState.AvailableWidth() = availableSpaceOnLine;
    WritingMode stateWM = reflowState.GetWritingMode();
    pfd->mMargin =
      reflowState.ComputedLogicalMargin().ConvertTo(frameWM, stateWM);
    pfd->mBorderPadding =
      reflowState.ComputedLogicalBorderPadding().ConvertTo(frameWM, stateWM);
    pfd->SetFlag(PFD_RELATIVEPOS,
                 reflowState.mStyleDisplay->IsRelativelyPositionedStyle());
    if (pfd->GetFlag(PFD_RELATIVEPOS)) {
      pfd->mOffsets =
        reflowState.ComputedLogicalOffsets().ConvertTo(frameWM, stateWM);
    }

    // Apply start margins (as appropriate) to the frame computing the
    // new starting x,y coordinates for the frame.
    ApplyStartMargin(pfd, reflowState);
  }
  // if isText(), no need to propagate NS_FRAME_IS_DIRTY from the parent,
  // because reflow doesn't look at the dirty bits on the frame being reflowed.

  // See if this frame depends on the width of its containing block.  If
  // so, disable resize reflow optimizations for the line.  (Note that,
  // to be conservative, we do this if we *try* to fit a frame on a
  // line, even if we don't succeed.)  (Note also that we can only make
  // this IsPercentageAware check *after* we've constructed our
  // nsHTMLReflowState, because that construction may be what forces aFrame
  // to lazily initialize its (possibly-percent-valued) intrinsic size.)
  if (mGotLineBox && IsPercentageAware(aFrame)) {
    mLineBox->DisableResizeReflowOptimization();
  }

  // Let frame know that are reflowing it. Note that we don't bother
  // positioning the frame yet, because we're probably going to end up
  // moving it when we do the block-direction alignment
  aFrame->WillReflow(mPresContext);

  // Adjust spacemanager coordinate system for the frame.
  nsHTMLReflowMetrics metrics(lineWM);
#ifdef DEBUG
  metrics.Width() = nscoord(0xdeadbeef);
  metrics.Height() = nscoord(0xdeadbeef);
#endif
  nsRect physicalBounds = pfd->mBounds.GetPhysicalRect(lineWM, mContainerWidth);
  nscoord tx = physicalBounds.x;
  nscoord ty = physicalBounds.y;
  mFloatManager->Translate(tx, ty);

  int32_t savedOptionalBreakOffset;
  gfxBreakPriority savedOptionalBreakPriority;
  nsIContent* savedOptionalBreakContent =
    GetLastOptionalBreakPosition(&savedOptionalBreakOffset,
                                 &savedOptionalBreakPriority);

  if (!isText) {
    nsresult rv = aFrame->Reflow(mPresContext, metrics, reflowStateHolder.ref(),
                                 aReflowStatus);
    if (NS_FAILED(rv)) {
      NS_WARNING( "Reflow of frame failed in nsLineLayout" );
      return rv;
    }
  } else {
    static_cast<nsTextFrame*>(aFrame)->
      ReflowText(*this, availableSpaceOnLine, psd->mReflowState->rendContext,
                 metrics, aReflowStatus);
  }
  
  pfd->mJustificationNumSpaces = mTextJustificationNumSpaces;
  pfd->mJustificationNumLetters = mTextJustificationNumLetters;

  // See if the frame is a placeholderFrame and if it is process
  // the float. At the same time, check if the frame has any non-collapsed-away
  // content.
  bool placedFloat = false;
  bool isEmpty;
  if (!frameType) {
    isEmpty = pfd->mFrame->IsEmpty();
  } else {
    if (nsGkAtoms::placeholderFrame == frameType) {
      isEmpty = true;
      pfd->SetFlag(PFD_SKIPWHENTRIMMINGWHITESPACE, true);
      nsIFrame* outOfFlowFrame = nsLayoutUtils::GetFloatFromPlaceholder(aFrame);
      if (outOfFlowFrame) {
        // Add mTrimmableWidth to the available width since if the line ends
        // here, the width of the inline content will be reduced by
        // mTrimmableWidth.
        nscoord availableWidth = psd->mIEnd - (psd->mICoord - mTrimmableWidth);
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
      pfd->SetFlag(PFD_ISTEXTFRAME, true);
      nsTextFrame* textFrame = static_cast<nsTextFrame*>(pfd->mFrame);
      isEmpty = !textFrame->HasNoncollapsedCharacters();
      if (!isEmpty) {
        pfd->SetFlag(PFD_ISNONEMPTYTEXTFRAME, true);
        nsIContent* content = textFrame->GetContent();

        const nsTextFragment* frag = content->GetText();
        if (frag) {
          pfd->SetFlag(PFD_ISNONWHITESPACETEXTFRAME,
                       !content->TextIsOnlyWhitespace());
        }
      }
    }
    else if (nsGkAtoms::brFrame == frameType) {
      pfd->SetFlag(PFD_SKIPWHENTRIMMINGWHITESPACE, true);
      isEmpty = false;
    } else {
      if (nsGkAtoms::letterFrame==frameType) {
        pfd->SetFlag(PFD_ISLETTERFRAME, true);
      }
      if (pfd->mSpan) {
        isEmpty = !pfd->mSpan->mHasNonemptyContent && pfd->mFrame->IsSelfEmpty();
      } else {
        isEmpty = pfd->mFrame->IsEmpty();
      }
    }
  }

  mFloatManager->Translate(-tx, -ty);

  NS_ASSERTION(metrics.Width() >= 0, "bad width");
  NS_ASSERTION(metrics.Height() >= 0,"bad height");
  if (metrics.Width() < 0) metrics.Width() = 0;
  if (metrics.Height() < 0) metrics.Height() = 0;

#ifdef DEBUG
  // Note: break-before means ignore the reflow metrics since the
  // frame will be reflowed another time.
  if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    if (CRAZY_SIZE(metrics.Width()) || CRAZY_SIZE(metrics.Height())) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" metrics=%d,%d!\n", metrics.Width(), metrics.Height());
    }
    if ((metrics.Width() == nscoord(0xdeadbeef)) ||
        (metrics.Height() == nscoord(0xdeadbeef))) {
      printf("nsLineLayout: ");
      nsFrame::ListTag(stdout, aFrame);
      printf(" didn't set w/h %d,%d!\n", metrics.Width(), metrics.Height());
    }
  }
#endif

  // Unlike with non-inline reflow, the overflow area here does *not*
  // include the accumulation of the frame's bounds and its inline
  // descendants' bounds. Nor does it include the outline area; it's
  // just the union of the bounds of any absolute children. That is
  // added in later by nsLineLayout::ReflowInlineFrames.
  pfd->mOverflowAreas = metrics.mOverflowAreas;

  pfd->mBounds.ISize(lineWM) = metrics.ISize();
  pfd->mBounds.BSize(lineWM) = metrics.BSize();

  // Size the frame, but |RelativePositionFrames| will size the view.
  aFrame->SetSize(nsSize(metrics.Width(), metrics.Height()));

  // Tell the frame that we're done reflowing it
  aFrame->DidReflow(mPresContext,
                    isText ? nullptr : reflowStateHolder.addr(),
                    nsDidReflowStatus::FINISHED);

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
      if (nullptr != kidNextInFlow) {
        // Remove all of the childs next-in-flows. Make sure that we ask
        // the right parent to do the removal (it's possible that the
        // parent is not this because we are executing pullup code)
        nsContainerFrame* parent = static_cast<nsContainerFrame*>
                                                  (kidNextInFlow->GetParent());
        parent->DeleteNextInFlowChild(kidNextInFlow, true);
      }
    }

    // Check whether this frame breaks up text runs. All frames break up text
    // runs (hence return false here) except for text frames and inline containers.
    bool continuingTextRun = aFrame->CanContinueTextRun();
    
    // Clear any residual mTrimmableWidth if this isn't a text frame
    if (!continuingTextRun && !pfd->GetFlag(PFD_SKIPWHENTRIMMINGWHITESPACE)) {
      mTrimmableWidth = 0;
    }

    // See if we can place the frame. If we can't fit it, then we
    // return now.
    bool optionalBreakAfterFits;
    NS_ASSERTION(isText ||
                 !reflowStateHolder.ref().IsFloating(),
                 "How'd we get a floated inline frame? "
                 "The frame ctor should've dealt with this.");
    if (CanPlaceFrame(pfd, notSafeToBreak, continuingTextRun,
                      savedOptionalBreakContent != nullptr, metrics,
                      aReflowStatus, &optionalBreakAfterFits)) {
      if (!isEmpty) {
        psd->mHasNonemptyContent = true;
        mLineIsEmpty = false;
        if (!pfd->mSpan) {
          // nonempty leaf content has been placed
          mLineAtStart = false;
        }
      }

      // Place the frame, updating aBounds with the final size and
      // location.  Then apply the bottom+right margins (as
      // appropriate) to the frame.
      PlaceFrame(pfd, metrics);
      PerSpanData* span = pfd->mSpan;
      if (span) {
        // The frame we just finished reflowing is an inline
        // container.  It needs its child frames aligned in the block direction,
        // so do most of it now.
        BlockDirAlignFrames(span);
      }
      
      if (!continuingTextRun) {
        if (!psd->mNoWrap && (!LineIsEmpty() || placedFloat)) {
          // record soft break opportunity after this content that can't be
          // part of a text run. This is not a text frame so we know
          // that offset INT32_MAX means "after the content".
          if (NotifyOptionalBreakPosition(aFrame->GetContent(), INT32_MAX, optionalBreakAfterFits, gfxBreakPriority::eNormalBreak)) {
            // If this returns true then we are being told to actually break here.
            aReflowStatus = NS_INLINE_LINE_BREAK_AFTER(aReflowStatus);
          }
        }
      }
    }
    else {
      PushFrame(aFrame);
      aPushedFrame = true;
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

  return NS_OK;
}

void
nsLineLayout::ApplyStartMargin(PerFrameData* pfd,
                               nsHTMLReflowState& aReflowState)
{
  NS_ASSERTION(!aReflowState.IsFloating(),
               "How'd we get a floated inline frame? "
               "The frame ctor should've dealt with this.");

  WritingMode frameWM = pfd->mFrame->GetWritingMode();
  WritingMode lineWM = mRootSpan->mWritingMode;

  // Only apply start-margin on the first-in flow for inline frames,
  // and make sure to not apply it to any inline other than the first
  // in an ib split.  Note that the ib sibling (block-in-inline
  // sibling) annotations only live on the first continuation, but we
  // don't want to apply the start margin for later continuations
  // anyway.
  if (pfd->mFrame->GetPrevContinuation() ||
      pfd->mFrame->FrameIsNonFirstInIBSplit()) {
    // Zero this out so that when we compute the max-element-width of
    // the frame we will properly avoid adding in the starting margin.
    pfd->mMargin.IStart(frameWM) = 0;
  }
  if ((pfd->mFrame->LastInFlow()->GetNextContinuation() ||
      pfd->mFrame->FrameIsNonLastInIBSplit())
    && !pfd->GetFlag(PFD_ISLETTERFRAME)) {
    pfd->mMargin.IEnd(frameWM) = 0;
  }
  nscoord startMargin = pfd->mMargin.ConvertTo(lineWM, frameWM).IStart(lineWM);
  if (startMargin) {
    // In RTL mode, we will only apply the start margin to the frame bounds
    // after we finish flowing the frame and know more accurately whether we
    // want to skip the margins.
    if (lineWM.IsBidiLTR() && frameWM.IsBidiLTR()) {
      pfd->mBounds.IStart(lineWM) += startMargin;
    }

    NS_WARN_IF_FALSE(NS_UNCONSTRAINEDSIZE != aReflowState.AvailableWidth(),
                     "have unconstrained width; this should only result from "
                     "very large sizes, not attempts at intrinsic width "
                     "calculation");
    if (NS_UNCONSTRAINEDSIZE == aReflowState.ComputedWidth()) {
      // For inline-ish and text-ish things (which don't compute widths
      // in the reflow state), adjust available width to account for the
      // start margin. The end margin will be accounted for when we
      // finish flowing the frame.
      aReflowState.AvailableWidth() -= startMargin;
    }
  }
}

nscoord
nsLineLayout::GetCurrentFrameInlineDistanceFromBlock()
{
  PerSpanData* psd;
  nscoord x = 0;
  for (psd = mCurrentSpan; psd; psd = psd->mParent) {
    x += psd->mICoord;
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
bool
nsLineLayout::CanPlaceFrame(PerFrameData* pfd,
                            bool aNotSafeToBreak,
                            bool aFrameCanContinueTextRun,
                            bool aCanRollBackBeforeFrame,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus,
                            bool* aOptionalBreakAfterFits)
{
  NS_PRECONDITION(pfd && pfd->mFrame, "bad args, null pointers for frame data");

  *aOptionalBreakAfterFits = true;

  WritingMode frameWM = pfd->mFrame->GetWritingMode();
  WritingMode lineWM = mRootSpan->mWritingMode;
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
  if (pfd->mFrame->GetPrevContinuation() ||
      pfd->mFrame->FrameIsNonFirstInIBSplit()) {
    pfd->mMargin.IStart(frameWM) = 0;
  }
  if ((NS_FRAME_IS_NOT_COMPLETE(aStatus) ||
       pfd->mFrame->LastInFlow()->GetNextContinuation() ||
       pfd->mFrame->FrameIsNonLastInIBSplit())
      && !pfd->GetFlag(PFD_ISLETTERFRAME)) {
    pfd->mMargin.IEnd(frameWM) = 0;
  }
  LogicalMargin usedMargins = pfd->mMargin.ConvertTo(lineWM, frameWM);
  nscoord startMargin = usedMargins.IStart(lineWM);
  nscoord endMargin = usedMargins.IEnd(lineWM);

  if (!(lineWM.IsBidiLTR() && frameWM.IsBidiLTR())) {
    pfd->mBounds.IStart(lineWM) += startMargin;
  }

  PerSpanData* psd = mCurrentSpan;
  if (psd->mNoWrap) {
    // When wrapping is off, everything fits.
    return true;
  }

#ifdef NOISY_CAN_PLACE_FRAME
  if (nullptr != psd->mFrame) {
    nsFrame::ListTag(stdout, psd->mFrame->mFrame);
  }
  else {
    nsFrame::ListTag(stdout, mBlockReflowState->frame);
  } 
  printf(": aNotSafeToBreak=%s frame=", aNotSafeToBreak ? "true" : "false");
  nsFrame::ListTag(stdout, pfd->mFrame);
  printf(" frameWidth=%d, margins=%d,%d\n",
         pfd->mBounds.IEnd(lineWM) + endMargin - psd->mICoord,
         startMargin, endMargin);
#endif

  // Set outside to true if the result of the reflow leads to the
  // frame sticking outside of our available area.
  bool outside = pfd->mBounds.IEnd(lineWM) - mTrimmableWidth + endMargin >
                 psd->mIEnd;
  if (!outside) {
    // If it fits, it fits
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> inside\n");
#endif
    return true;
  }
  *aOptionalBreakAfterFits = false;

  // When it doesn't fit, check for a few special conditions where we
  // allow it to fit anyway.
  if (0 == startMargin + pfd->mBounds.ISize(lineWM) + endMargin) {
    // Empty frames always fit right where they are
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> empty frame fits\n");
#endif
    return true;
  }

#ifdef FIX_BUG_50257
  // another special case:  always place a BR
  if (nsGkAtoms::brFrame == pfd->mFrame->GetType()) {
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> BR frame fits\n");
#endif
    return true;
  }
#endif

  if (aNotSafeToBreak) {
    // There are no frames on the line that take up width and the line is
    // not impacted by floats, so we must allow the current frame to be
    // placed on the line
#ifdef NOISY_CAN_PLACE_FRAME
    printf("   ==> not-safe and not-impacted fits: ");
    while (nullptr != psd) {
      printf("<psd=%p x=%d left=%d> ", psd, psd->mICoord, psd->mIStart);
      psd = psd->mParent;
    }
    printf("\n");
#endif
    return true;
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
    return true;
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
    mNeedBackup = true;
    return true;
  }

#ifdef NOISY_CAN_PLACE_FRAME
  printf("   ==> didn't fit\n");
#endif
  aStatus = NS_INLINE_LINE_BREAK_BEFORE();
  return false;
}

/**
 * Place the frame. Update running counters.
 */
void
nsLineLayout::PlaceFrame(PerFrameData* pfd, nsHTMLReflowMetrics& aMetrics)
{
  // Record ascent and update max-ascent and max-descent values
  if (aMetrics.TopAscent() == nsHTMLReflowMetrics::ASK_FOR_BASELINE)
    pfd->mAscent = pfd->mFrame->GetBaseline();
  else
    pfd->mAscent = aMetrics.TopAscent();

  // Advance to next inline coordinate
  WritingMode frameWM = pfd->mFrame->GetWritingMode();
  WritingMode lineWM = mRootSpan->mWritingMode;
  mCurrentSpan->mICoord = pfd->mBounds.IEnd(lineWM) +
                          pfd->mMargin.ConvertTo(lineWM, frameWM).IEnd(lineWM);

  // Count the number of non-placeholder frames on the line...
  if (pfd->mFrame->GetType() == nsGkAtoms::placeholderFrame) {
    NS_ASSERTION(pfd->mBounds.ISize(lineWM) == 0 &&
                 pfd->mBounds.BSize(lineWM) == 0,
                 "placeholders should have 0 width/height (checking "
                 "placeholders were never counted by the old code in "
                 "this function)");
  } else {
    mTotalPlacedFrames++;
  }
}

void
nsLineLayout::AddBulletFrame(nsIFrame* aFrame,
                             const nsHTMLReflowMetrics& aMetrics)
{
  NS_ASSERTION(mCurrentSpan == mRootSpan, "bad linelayout user");
  NS_ASSERTION(mGotLineBox, "must have line box");

  nsIFrame *blockFrame = mBlockReflowState->frame;
  NS_ASSERTION(blockFrame->IsFrameOfType(nsIFrame::eBlockFrame),
               "must be for block");
  if (!static_cast<nsBlockFrame*>(blockFrame)->BulletIsEmpty()) {
    mHasBullet = true;
    mLineBox->SetHasBullet();
  }

  PerFrameData* pfd = NewPerFrameData(aFrame);
  mRootSpan->AppendFrame(pfd);
  pfd->SetFlag(PFD_ISBULLET, true);
  if (aMetrics.TopAscent() == nsHTMLReflowMetrics::ASK_FOR_BASELINE)
    pfd->mAscent = aFrame->GetBaseline();
  else
    pfd->mAscent = aMetrics.TopAscent();

  // Note: block-coord value will be updated during block-direction alignment
  pfd->mBounds = LogicalRect(mRootSpan->mWritingMode,
                             aFrame->GetRect(), mContainerWidth);
  pfd->mOverflowAreas = aMetrics.mOverflowAreas;
}

#ifdef DEBUG
void
nsLineLayout::DumpPerSpanData(PerSpanData* psd, int32_t aIndent)
{
  nsFrame::IndentBy(stdout, aIndent);
  printf("%p: left=%d x=%d right=%d\n", static_cast<void*>(psd),
         psd->mIStart, psd->mICoord, psd->mIEnd);
  PerFrameData* pfd = psd->mFirstFrame;
  while (nullptr != pfd) {
    nsFrame::IndentBy(stdout, aIndent+1);
    nsFrame::ListTag(stdout, pfd->mFrame);
    nsRect rect = pfd->mBounds.GetPhysicalRect(psd->mWritingMode,
                                               mContainerWidth);
    printf(" %d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height);
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
nsLineLayout::BlockDirAlignLine()
{
  // Synthesize a PerFrameData for the block frame
  PerFrameData rootPFD(mBlockReflowState->frame->GetWritingMode());
  rootPFD.mFrame = mBlockReflowState->frame;
  rootPFD.mAscent = 0;
  mRootSpan->mFrame = &rootPFD;

  // Partially place the children of the block frame. The baseline for
  // this operation is set to zero so that the y coordinates for all
  // of the placed children will be relative to there.
  PerSpanData* psd = mRootSpan;
  BlockDirAlignFrames(psd);

  // *** Note that comments here still use the anachronistic term "line-height"
  // when we really mean "size of the line in the block direction". This is
  // partly for brevity and partly to retain the association with the CSS
  // line-height property
  //
  // Compute the line-height. The line-height will be the larger of:
  //
  // [1] maxBCoord - minBCoord (the distance between the first child's
  // block-start edge and the last child's block-end edge)
  //
  // [2] the maximum logical box block size (since not every frame may have
  // participated in #1; for example: block-start/end aligned frames)
  //
  // [3] the minimum line height ("line-height" property set on the
  // block frame)
  nscoord lineBSize = psd->mMaxBCoord - psd->mMinBCoord;

  // Now that the line-height is computed, we need to know where the
  // baseline is in the line. Position baseline so that mMinBCoord is just
  // inside the start of the line box.
  nscoord baselineBCoord;
  if (psd->mMinBCoord < 0) {
    baselineBCoord = mBStartEdge - psd->mMinBCoord;
  }
  else {
    baselineBCoord = mBStartEdge;
  }

  // It's also possible that the line block-size isn't tall enough because
  // of block start/end aligned elements that were not accounted for in
  // min/max BCoord.
  //
  // The CSS2 spec doesn't really say what happens when to the
  // baseline in this situations. What we do is if the largest start
  // aligned box block size is greater than the line block-size then we leave
  // the baseline alone. If the largest end aligned box is greater
  // than the line block-size then we slide the baseline forward by the extra
  // amount.
  //
  // Navigator 4 gives precedence to the first top/bottom aligned
  // object.  We just let block end aligned objects win.
  if (lineBSize < mMaxEndBoxBSize) {
    // When the line is shorter than the maximum block start aligned box
    nscoord extra = mMaxEndBoxBSize - lineBSize;
    baselineBCoord += extra;
    lineBSize = mMaxEndBoxBSize;
  }
  if (lineBSize < mMaxStartBoxBSize) {
    lineBSize = mMaxStartBoxBSize;
  }
#ifdef NOISY_BLOCKDIR_ALIGN
  printf("  [line]==> lineBSize=%d baselineBCoord=%d\n", lineBSize, baselineBCoord);
#endif

  // Now position all of the frames in the root span. We will also
  // recurse over the child spans and place any block start/end aligned
  // frames we find.
  // XXX PERFORMANCE: set a bit per-span to avoid the extra work
  // (propagate it upward too)
  WritingMode lineWM = psd->mWritingMode;
  for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
    if (pfd->mBlockDirAlign == VALIGN_OTHER) {
      pfd->mBounds.BStart(lineWM) += baselineBCoord;
      pfd->mFrame->SetRect(lineWM, pfd->mBounds, mContainerWidth);
    }
  }
  PlaceStartEndFrames(psd, -mBStartEdge, lineBSize);

  // If the frame being reflowed has text decorations, we simulate the
  // propagation of those decorations to a line-level element by storing the
  // offset in a frame property on any child frames that are aligned in the
  // block direction somewhere other than the baseline. This property is then
  // used by nsTextFrame::GetTextDecorations when the same conditions are met.
  if (rootPFD.mFrame->StyleContext()->HasTextDecorationLines()) {
    for (const PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
      const nsIFrame *const f = pfd->mFrame;
      if (f->VerticalAlignEnum() != NS_STYLE_VERTICAL_ALIGN_BASELINE) {
        const nscoord offset = baselineBCoord - pfd->mBounds.BStart(lineWM);
        f->Properties().Set(nsIFrame::LineBaselineOffset(),
                            NS_INT32_TO_PTR(offset));
      }
    }
  }

  // Fill in returned line-box and max-element-width data
  mLineBox->mBounds.x = psd->mIStart;
  mLineBox->mBounds.y = mBStartEdge;
  mLineBox->mBounds.width = psd->mICoord - psd->mIStart;
  mLineBox->mBounds.height = lineBSize;

  mFinalLineBSize = lineBSize;
  mLineBox->SetAscent(baselineBCoord - mBStartEdge);
#ifdef NOISY_BLOCKDIR_ALIGN
  printf(
    "  [line]==> bounds{x,y,w,h}={%d,%d,%d,%d} lh=%d a=%d\n",
    mLineBox->mBounds.x, mLineBox->mBounds.y,
    mLineBox->mBounds.width, mLineBox->mBounds.height,
    mFinalLineBSize, mLineBox->GetAscent());
#endif

  // Undo root-span mFrame pointer to prevent brane damage later on...
  mRootSpan->mFrame = nullptr;
}

void
nsLineLayout::PlaceStartEndFrames(PerSpanData* psd,
                                  nscoord aDistanceFromStart,
                                  nscoord aLineBSize)
{
  for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
    PerSpanData* span = pfd->mSpan;
#ifdef DEBUG
    NS_ASSERTION(0xFF != pfd->mBlockDirAlign, "umr");
#endif
    WritingMode frameWM = pfd->mFrame->GetWritingMode();
    WritingMode lineWM = mRootSpan->mWritingMode;
    switch (pfd->mBlockDirAlign) {
      case VALIGN_TOP:
        if (span) {
          pfd->mBounds.BStart(lineWM) = -aDistanceFromStart - span->mMinBCoord;
        }
        else {
          pfd->mBounds.BStart(lineWM) =
            -aDistanceFromStart + pfd->mMargin.BStart(frameWM);
        }
        pfd->mFrame->SetRect(lineWM, pfd->mBounds, mContainerWidth);
#ifdef NOISY_BLOCKDIR_ALIGN
        printf("    ");
        nsFrame::ListTag(stdout, pfd->mFrame);
        printf(": y=%d dTop=%d [bp.top=%d topLeading=%d]\n",
               pfd->mBounds.BStart(lineWM), aDistanceFromStart,
               span ? pfd->mBorderPadding.BStart(frameWM) : 0,
               span ? span->mBStartLeading : 0);
#endif
        break;
      case VALIGN_BOTTOM:
        if (span) {
          // Compute bottom leading
          pfd->mBounds.BStart(lineWM) =
            -aDistanceFromStart + aLineBSize - span->mMaxBCoord;
        }
        else {
          pfd->mBounds.BStart(lineWM) = -aDistanceFromStart + aLineBSize -
            pfd->mMargin.BEnd(frameWM) - pfd->mBounds.BSize(lineWM);
        }
        pfd->mFrame->SetRect(lineWM, pfd->mBounds, mContainerWidth);
#ifdef NOISY_BLOCKDIR_ALIGN
        printf("    ");
        nsFrame::ListTag(stdout, pfd->mFrame);
        printf(": y=%d\n", pfd->mBounds.BStart(lineWM));
#endif
        break;
    }
    if (span) {
      nscoord fromStart = aDistanceFromStart + pfd->mBounds.BStart(lineWM);
      PlaceStartEndFrames(span, fromStart, aLineBSize);
    }
  }
}

static float
GetInflationForBlockDirAlignment(nsIFrame* aFrame,
                                 nscoord aInflationMinFontSize)
{
  if (aFrame->IsSVGText()) {
    const nsIFrame* container =
      nsLayoutUtils::GetClosestFrameOfType(aFrame, nsGkAtoms::svgTextFrame);
    NS_ASSERTION(container, "expected to find an ancestor SVGTextFrame");
    return
      static_cast<const SVGTextFrame*>(container)->GetFontSizeScaleFactor();
  }
  return nsLayoutUtils::FontSizeInflationInner(aFrame, aInflationMinFontSize);
}

#define BLOCKDIR_ALIGN_FRAMES_NO_MINIMUM nscoord_MAX
#define BLOCKDIR_ALIGN_FRAMES_NO_MAXIMUM nscoord_MIN

// Place frames in the block direction within a given span. Note: this doesn't
// place block start/end aligned frames as those have to wait until the
// entire line box block size is known. This is called after the span
// frame has finished being reflowed so that we know its block size.
void
nsLineLayout::BlockDirAlignFrames(PerSpanData* psd)
{
  // Get parent frame info
  PerFrameData* spanFramePFD = psd->mFrame;
  nsIFrame* spanFrame = spanFramePFD->mFrame;

  // Get the parent frame's font for all of the frames in this span
  nsRefPtr<nsFontMetrics> fm;
  float inflation =
    GetInflationForBlockDirAlignment(spanFrame, mInflationMinFontSize);
  nsLayoutUtils::GetFontMetricsForFrame(spanFrame, getter_AddRefs(fm),
                                        inflation);
  mBlockReflowState->rendContext->SetFont(fm);

  bool preMode = mStyleText->WhiteSpaceIsSignificant();

  // See if the span is an empty continuation. It's an empty continuation iff:
  // - it has a prev-in-flow
  // - it has no next in flow
  // - it's zero sized
  WritingMode frameWM = spanFramePFD->mFrame->GetWritingMode();
  WritingMode lineWM = mRootSpan->mWritingMode;
  bool emptyContinuation = psd != mRootSpan &&
    spanFrame->GetPrevInFlow() && !spanFrame->GetNextInFlow() &&
    spanFramePFD->mBounds.IsZeroSize();

#ifdef NOISY_BLOCKDIR_ALIGN
  printf("[%sSpan]", (psd == mRootSpan)?"Root":"");
  nsFrame::ListTag(stdout, spanFrame);
  printf(": preMode=%s strictMode=%s w/h=%d,%d emptyContinuation=%s",
         preMode ? "yes" : "no",
         mPresContext->CompatibilityMode() != eCompatibility_NavQuirks ? "yes" : "no",
         spanFramePFD->mBounds.ISize(lineWM),
         spanFramePFD->mBounds.BSize(lineWM),
         emptyContinuation ? "yes" : "no");
  if (psd != mRootSpan) {
    WritingMode frameWM = spanFramePFD->mFrame->GetWritingMode();
    printf(" bp=%d,%d,%d,%d margin=%d,%d,%d,%d",
           spanFramePFD->mBorderPadding.Top(frameWM),
           spanFramePFD->mBorderPadding.Right(frameWM),
           spanFramePFD->mBorderPadding.Bottom(frameWM),
           spanFramePFD->mBorderPadding.Left(frameWM),
           spanFramePFD->mMargin.Top(frameWM),
           spanFramePFD->mMargin.Right(frameWM),
           spanFramePFD->mMargin.Bottom(frameWM),
           spanFramePFD->mMargin.Left(frameWM));
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
  // line block-size.
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
  bool zeroEffectiveSpanBox = false;
  // XXXldb If we really have empty continuations, then all these other
  // checks don't make sense for them.
  // XXXldb This should probably just use nsIFrame::IsSelfEmpty, assuming that
  // it agrees with this code.  (If it doesn't agree, it probably should.)
  if ((emptyContinuation ||
       mPresContext->CompatibilityMode() != eCompatibility_FullStandards) &&
      ((psd == mRootSpan) ||
       (spanFramePFD->mBorderPadding.IsEmpty() &&
        spanFramePFD->mMargin.IsEmpty()))) {
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
    zeroEffectiveSpanBox = true;
    for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
      if (pfd->GetFlag(PFD_ISTEXTFRAME) &&
          (pfd->GetFlag(PFD_ISNONWHITESPACETEXTFRAME) || preMode ||
           pfd->mBounds.ISize(mRootSpan->mWritingMode) != 0)) {
        zeroEffectiveSpanBox = false;
        break;
      }
    }
  }
  psd->mZeroEffectiveSpanBox = zeroEffectiveSpanBox;

  // Setup baselineBCoord, minBCoord, and maxBCoord
  nscoord baselineBCoord, minBCoord, maxBCoord;
  if (psd == mRootSpan) {
    // Use a zero baselineBCoord since we don't yet know where the baseline
    // will be (until we know how tall the line is; then we will
    // know). In addition, use extreme values for the minBCoord and maxBCoord
    // values so that only the child frames will impact their values
    // (since these are children of the block, there is no span box to
    // provide initial values).
    baselineBCoord = 0;
    minBCoord = BLOCKDIR_ALIGN_FRAMES_NO_MINIMUM;
    maxBCoord = BLOCKDIR_ALIGN_FRAMES_NO_MAXIMUM;
#ifdef NOISY_BLOCKDIR_ALIGN
    printf("[RootSpan]");
    nsFrame::ListTag(stdout, spanFrame);
    printf(": pass1 valign frames: topEdge=%d minLineBSize=%d zeroEffectiveSpanBox=%s\n",
           mBStartEdge, mMinLineBSize,
           zeroEffectiveSpanBox ? "yes" : "no");
#endif
  }
  else {
    // Compute the logical block size for this span. The logical block size
    // is based on the "line-height" value, not the font-size. Also
    // compute the top leading.
    float inflation =
      GetInflationForBlockDirAlignment(spanFrame, mInflationMinFontSize);
    nscoord logicalBSize = nsHTMLReflowState::
      CalcLineHeight(spanFrame->GetContent(), spanFrame->StyleContext(),
                     mBlockReflowState->ComputedHeight(),
                     inflation);
    nscoord contentBSize = spanFramePFD->mBounds.BSize(lineWM) -
      spanFramePFD->mBorderPadding.BStart(frameWM) -
      spanFramePFD->mBorderPadding.BEnd(frameWM);

    // Special-case for a ::first-letter frame, set the line height to
    // the frame block size if the user has left line-height == normal
    if (spanFramePFD->GetFlag(PFD_ISLETTERFRAME) &&
        !spanFrame->GetPrevInFlow() &&
        spanFrame->StyleText()->mLineHeight.GetUnit() == eStyleUnit_Normal) {
      logicalBSize = spanFramePFD->mBounds.BSize(lineWM);
    }

    nscoord leading = logicalBSize - contentBSize;
    psd->mBStartLeading = leading / 2;
    psd->mBEndLeading = leading - psd->mBStartLeading;
    psd->mLogicalBSize = logicalBSize;

    if (zeroEffectiveSpanBox) {
      // When the span-box is to be ignored, zero out the initial
      // values so that the span doesn't impact the final line
      // height. The contents of the span can impact the final line
      // height.

      // Note that things are readjusted for this span after its children
      // are reflowed
      minBCoord = BLOCKDIR_ALIGN_FRAMES_NO_MINIMUM;
      maxBCoord = BLOCKDIR_ALIGN_FRAMES_NO_MAXIMUM;
    }
    else {

      // The initial values for the min and max block coord values are in the
      // span's coordinate space, and cover the logical block size of the span.
      // If there are child frames in this span that stick out of this area
      // then the minBCoord and maxBCoord are updated by the amount of logical
      // blockSize that is outside this range.
      minBCoord = spanFramePFD->mBorderPadding.BStart(frameWM) -
                  psd->mBStartLeading;
      maxBCoord = minBCoord + psd->mLogicalBSize;
    }

    // This is the distance from the top edge of the parents visual
    // box to the baseline. The span already computed this for us,
    // so just use it.
    *psd->mBaseline = baselineBCoord = spanFramePFD->mAscent;


#ifdef NOISY_BLOCKDIR_ALIGN
    printf("[%sSpan]", (psd == mRootSpan)?"Root":"");
    nsFrame::ListTag(stdout, spanFrame);
    printf(": baseLine=%d logicalBSize=%d topLeading=%d h=%d bp=%d,%d zeroEffectiveSpanBox=%s\n",
           baselineBCoord, psd->mLogicalBSize, psd->mBStartLeading,
           spanFramePFD->mBounds.BSize(lineWM),
           spanFramePFD->mBorderPadding.Top(frameWM),
           spanFramePFD->mBorderPadding.Bottom(frameWM),
           zeroEffectiveSpanBox ? "yes" : "no");
#endif
  }

  nscoord maxStartBoxBSize = 0;
  nscoord maxEndBoxBSize = 0;
  PerFrameData* pfd = psd->mFirstFrame;
  while (nullptr != pfd) {
    nsIFrame* frame = pfd->mFrame;
    WritingMode frameWM = frame->GetWritingMode();

    // sanity check (see bug 105168, non-reproducible crashes from null frame)
    NS_ASSERTION(frame, "null frame in PerFrameData - something is very very bad");
    if (!frame) {
      return;
    }

    // Compute the logical block size of the frame
    nscoord logicalBSize;
    PerSpanData* frameSpan = pfd->mSpan;
    if (frameSpan) {
      // For span frames the logical-block-size and start-leading were
      // pre-computed when the span was reflowed.
      logicalBSize = frameSpan->mLogicalBSize;
    }
    else {
      // For other elements the logical block size is the same as the
      // frame's block size plus its margins.
      logicalBSize = pfd->mBounds.BSize(lineWM) +
                     pfd->mMargin.BStartEnd(frameWM);
      if (logicalBSize < 0 &&
          mPresContext->CompatibilityMode() == eCompatibility_NavQuirks) {
        pfd->mAscent -= logicalBSize;
        logicalBSize = 0;
      }
    }

    // Get vertical-align property ("vertical-align" is the CSS name for
    // block-direction align)
    const nsStyleCoord& verticalAlign =
      frame->StyleTextReset()->mVerticalAlign;
    uint8_t verticalAlignEnum = frame->VerticalAlignEnum();
#ifdef NOISY_BLOCKDIR_ALIGN
    printf("  [frame]");
    nsFrame::ListTag(stdout, frame);
    printf(": verticalAlignUnit=%d (enum == %d",
           verticalAlign.GetUnit(),
           ((eStyleUnit_Enumerated == verticalAlign.GetUnit())
            ? verticalAlign.GetIntValue()
            : -1));
    if (verticalAlignEnum != nsIFrame::eInvalidVerticalAlign) {
      printf(", after SVG dominant-baseline conversion == %d",
             verticalAlignEnum);
    }
    printf(")\n");
#endif

    if (verticalAlignEnum != nsIFrame::eInvalidVerticalAlign) {
      switch (verticalAlignEnum) {
        default:
        case NS_STYLE_VERTICAL_ALIGN_BASELINE:
        {
          // The element's baseline is aligned with the baseline of
          // the parent.
          pfd->mBounds.BStart(lineWM) = baselineBCoord - pfd->mAscent;
          pfd->mBlockDirAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_SUB:
        {
          // Lower the baseline of the box to the subscript offset
          // of the parent's box. This is identical to the baseline
          // alignment except for the addition of the subscript
          // offset to the baseline BCoord.
          nscoord parentSubscript = fm->SubscriptOffset();
          nscoord revisedBaselineBCoord = baselineBCoord + parentSubscript;
          pfd->mBounds.BStart(lineWM) = revisedBaselineBCoord - pfd->mAscent;
          pfd->mBlockDirAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_SUPER:
        {
          // Raise the baseline of the box to the superscript offset
          // of the parent's box. This is identical to the baseline
          // alignment except for the subtraction of the superscript
          // offset to the baseline BCoord.
          nscoord parentSuperscript = fm->SuperscriptOffset();
          nscoord revisedBaselineBCoord = baselineBCoord - parentSuperscript;
          pfd->mBounds.BStart(lineWM) = revisedBaselineBCoord - pfd->mAscent;
          pfd->mBlockDirAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_TOP:
        {
          pfd->mBlockDirAlign = VALIGN_TOP;
          nscoord subtreeBSize = logicalBSize;
          if (frameSpan) {
            subtreeBSize = frameSpan->mMaxBCoord - frameSpan->mMinBCoord;
            NS_ASSERTION(subtreeBSize >= logicalBSize,
                         "unexpected subtree block size");
          }
          if (subtreeBSize > maxStartBoxBSize) {
            maxStartBoxBSize = subtreeBSize;
          }
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
        {
          pfd->mBlockDirAlign = VALIGN_BOTTOM;
          nscoord subtreeBSize = logicalBSize;
          if (frameSpan) {
            subtreeBSize = frameSpan->mMaxBCoord - frameSpan->mMinBCoord;
            NS_ASSERTION(subtreeBSize >= logicalBSize,
                         "unexpected subtree block size");
          }
          if (subtreeBSize > maxEndBoxBSize) {
            maxEndBoxBSize = subtreeBSize;
          }
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
        {
          // Align the midpoint of the frame with 1/2 the parents
          // x-height above the baseline.
          nscoord parentXHeight = fm->XHeight();
          if (frameSpan) {
            pfd->mBounds.BStart(lineWM) = baselineBCoord -
              (parentXHeight + pfd->mBounds.BSize(lineWM))/2;
          }
          else {
            pfd->mBounds.BStart(lineWM) = baselineBCoord -
              (parentXHeight + logicalBSize)/2 +
              pfd->mMargin.BStart(frameWM);
          }
          pfd->mBlockDirAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_TEXT_TOP:
        {
          // The top of the logical box is aligned with the top of
          // the parent element's text.
          nscoord parentAscent = fm->MaxAscent();
          if (frameSpan) {
            pfd->mBounds.BStart(lineWM) = baselineBCoord - parentAscent -
              pfd->mBorderPadding.BStart(frameWM) + frameSpan->mBStartLeading;
          }
          else {
            pfd->mBounds.BStart(lineWM) = baselineBCoord - parentAscent +
                                          pfd->mMargin.BStart(frameWM);
          }
          pfd->mBlockDirAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM:
        {
          // The bottom of the logical box is aligned with the
          // bottom of the parent elements text.
          nscoord parentDescent = fm->MaxDescent();
          if (frameSpan) {
            pfd->mBounds.BStart(lineWM) = baselineBCoord + parentDescent -
                                          pfd->mBounds.BSize(lineWM) +
                                          pfd->mBorderPadding.BEnd(frameWM) -
                                          frameSpan->mBEndLeading;
          }
          else {
            pfd->mBounds.BStart(lineWM) = baselineBCoord + parentDescent -
                                          pfd->mBounds.BSize(lineWM) -
                                          pfd->mMargin.BEnd(frameWM);
          }
          pfd->mBlockDirAlign = VALIGN_OTHER;
          break;
        }

        case NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE:
        {
          // Align the midpoint of the frame with the baseline of the parent.
          if (frameSpan) {
            pfd->mBounds.BStart(lineWM) = baselineBCoord -
                                          pfd->mBounds.BSize(lineWM)/2;
          }
          else {
            pfd->mBounds.BStart(lineWM) = baselineBCoord - logicalBSize/2 +
                                          pfd->mMargin.BStart(frameWM);
          }
          pfd->mBlockDirAlign = VALIGN_OTHER;
          break;
        }
      }
    } else {
      // We have either a coord, a percent, or a calc().
      nscoord pctBasis = 0;
      if (verticalAlign.HasPercent()) {
        // Percentages are like lengths, except treated as a percentage
        // of the elements line block size value.
        float inflation =
          GetInflationForBlockDirAlignment(frame, mInflationMinFontSize);
        pctBasis = nsHTMLReflowState::CalcLineHeight(frame->GetContent(),
          frame->StyleContext(), mBlockReflowState->ComputedBSize(),
          inflation);
      }
      nscoord offset =
        nsRuleNode::ComputeCoordPercentCalc(verticalAlign, pctBasis);
      // According to the CSS2 spec (10.8.1), a positive value
      // "raises" the box by the given distance while a negative value
      // "lowers" the box by the given distance (with zero being the
      // baseline). Since Y coordinates increase towards the bottom of
      // the screen we reverse the sign.
      nscoord revisedBaselineBCoord = baselineBCoord - offset;
      pfd->mBounds.BStart(lineWM) = revisedBaselineBCoord - pfd->mAscent;
      pfd->mBlockDirAlign = VALIGN_OTHER;
    }

    // Update minBCoord/maxBCoord for frames that we just placed. Do not factor
    // text into the equation.
    if (pfd->mBlockDirAlign == VALIGN_OTHER) {
      // Text frames and bullets do not contribute to the min/max Y values for
      // the line (instead their parent frame's font-size contributes).
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
      bool canUpdate = !pfd->GetFlag(PFD_ISTEXTFRAME);
      if ((!canUpdate && pfd->GetFlag(PFD_ISNONWHITESPACETEXTFRAME)) ||
          (canUpdate && (pfd->GetFlag(PFD_ISBULLET) ||
                         nsGkAtoms::bulletFrame == frame->GetType()))) {
        // Only consider bullet / non-empty text frames when line-height:normal.
        canUpdate =
          frame->StyleText()->mLineHeight.GetUnit() == eStyleUnit_Normal;
      }
      if (canUpdate) {
        nscoord blockStart, blockEnd;
        if (frameSpan) {
          // For spans that were are now placing, use their position
          // plus their already computed min-Y and max-Y values for
          // computing blockStart and blockEnd.
          blockStart = pfd->mBounds.BStart(lineWM) + frameSpan->mMinBCoord;
          blockEnd = pfd->mBounds.BStart(lineWM) + frameSpan->mMaxBCoord;
        }
        else {
          blockStart = pfd->mBounds.BStart(lineWM) -
                       pfd->mMargin.BStart(frameWM);
          blockEnd = blockStart + logicalBSize;
        }
        if (!preMode &&
            mPresContext->CompatibilityMode() != eCompatibility_FullStandards &&
            !logicalBSize) {
          // Check if it's a BR frame that is not alone on its line (it
          // is given a block size of zero to indicate this), and if so reset
          // blockStart and blockEnd so that BR frames don't influence the line.
          if (nsGkAtoms::brFrame == frame->GetType()) {
            blockStart = BLOCKDIR_ALIGN_FRAMES_NO_MINIMUM;
            blockEnd = BLOCKDIR_ALIGN_FRAMES_NO_MAXIMUM;
          }
        }
        if (blockStart < minBCoord) minBCoord = blockStart;
        if (blockEnd > maxBCoord) maxBCoord = blockEnd;
#ifdef NOISY_BLOCKDIR_ALIGN
        printf("     [frame]raw: a=%d h=%d bp=%d,%d logical: h=%d leading=%d y=%d minBCoord=%d maxBCoord=%d\n",
               pfd->mAscent, pfd->mBounds.BSize(lineWM),
               pfd->mBorderPadding.Top(frameWM),
               pfd->mBorderPadding.Bottom(frameWM),
               logicalBSize,
               frameSpan ? frameSpan->mBStartLeading : 0,
               pfd->mBounds.BStart(lineWM), minBCoord, maxBCoord);
#endif
      }
      if (psd != mRootSpan) {
        frame->SetRect(lineWM, pfd->mBounds, mContainerWidth);
      }
    }
    pfd = pfd->mNext;
  }

  // Factor in the minimum line block-size when handling the root-span for
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
    bool applyMinLH = !psd->mZeroEffectiveSpanBox || mHasBullet;
    bool isLastLine = (!mLineBox->IsLineWrapped() && !mLineEndsInBR);
    if (!applyMinLH && isLastLine) {
      nsIContent* blockContent = mRootSpan->mFrame->mFrame->GetContent();
      if (blockContent) {
        nsIAtom *blockTagAtom = blockContent->Tag();
        // (3) above, if the last line of LI, DT, or DD
        if (blockTagAtom == nsGkAtoms::li ||
            blockTagAtom == nsGkAtoms::dt ||
            blockTagAtom == nsGkAtoms::dd) {
          applyMinLH = true;
        }
      }
    }
    if (applyMinLH) {
      if (psd->mHasNonemptyContent || preMode || mHasBullet) {
#ifdef NOISY_BLOCKDIR_ALIGN
        printf("  [span]==> adjusting min/maxBCoord: currentValues: %d,%d", minBCoord, maxBCoord);
#endif
        nscoord minimumLineBSize = mMinLineBSize;
        nscoord blockStart =
          -nsLayoutUtils::GetCenteredFontBaseline(fm, minimumLineBSize);
        nscoord blockEnd = blockStart + minimumLineBSize;

        if (blockStart < minBCoord) minBCoord = blockStart;
        if (blockEnd > maxBCoord) maxBCoord = blockEnd;

#ifdef NOISY_BLOCKDIR_ALIGN
        printf(" new values: %d,%d\n", minBCoord, maxBCoord);
#endif
#ifdef NOISY_BLOCKDIR_ALIGN
        printf("            Used mMinLineBSize: %d, blockStart: %d, blockEnd: %d\n", mMinLineBSize, blockStart, blockEnd);
#endif
      }
      else {
        // XXX issues:
        // [1] BR's on empty lines stop working
        // [2] May not honor css2's notion of handling empty elements
        // [3] blank lines in a pre-section ("\n") (handled with preMode)

        // XXX Are there other problems with this?
#ifdef NOISY_BLOCKDIR_ALIGN
        printf("  [span]==> zapping min/maxBCoord: currentValues: %d,%d newValues: 0,0\n",
               minBCoord, maxBCoord);
#endif
        minBCoord = maxBCoord = 0;
      }
    }
  }

  if ((minBCoord == BLOCKDIR_ALIGN_FRAMES_NO_MINIMUM) ||
      (maxBCoord == BLOCKDIR_ALIGN_FRAMES_NO_MAXIMUM)) {
    minBCoord = maxBCoord = baselineBCoord;
  }

  if ((psd != mRootSpan) && (psd->mZeroEffectiveSpanBox)) {
#ifdef NOISY_BLOCKDIR_ALIGN
    printf("   [span]adjusting for zeroEffectiveSpanBox\n");
    printf("     Original: minBCoord=%d, maxBCoord=%d, bSize=%d, ascent=%d, logicalBSize=%d, topLeading=%d, bottomLeading=%d\n",
           minBCoord, maxBCoord, spanFramePFD->mBounds.BSize(frameWM),
           spanFramePFD->mAscent,
           psd->mLogicalBSize, psd->mBStartLeading, psd->mBEndLeading);
#endif
    nscoord goodMinBCoord = spanFramePFD->mBorderPadding.BStart(frameWM) -
                            psd->mBStartLeading;
    nscoord goodMaxBCoord = goodMinBCoord + psd->mLogicalBSize;

    // For cases like the one in bug 714519 (text-decoration placement
    // or making nsLineLayout::IsZeroBSize() handle
    // vertical-align:top/bottom on a descendant of the line that's not
    // a child of it), we want to treat elements that are
    // vertical-align: top or bottom somewhat like children for the
    // purposes of this quirk.  To some extent, this is guessing, since
    // they might end up being aligned anywhere.  However, we'll guess
    // that they'll be placed aligned with the top or bottom of this
    // frame (as though this frame is the only thing in the line).
    // (Guessing isn't crazy, since all we're doing is reducing the
    // scope of a quirk and making the behavior more standards-like.)
    if (maxStartBoxBSize > maxBCoord - minBCoord) {
      // Distribute maxStartBoxBSize to ascent (baselineBCoord - minBCoord), and
      // then to descent (maxBCoord - baselineBCoord) by adjusting minBCoord or
      // maxBCoord, but not to exceed goodMinBCoord and goodMaxBCoord.
      nscoord distribute = maxStartBoxBSize - (maxBCoord - minBCoord);
      nscoord ascentSpace = std::max(minBCoord - goodMinBCoord, 0);
      if (distribute > ascentSpace) {
        distribute -= ascentSpace;
        minBCoord -= ascentSpace;
        nscoord descentSpace = std::max(goodMaxBCoord - maxBCoord, 0);
        if (distribute > descentSpace) {
          maxBCoord += descentSpace;
        } else {
          maxBCoord += distribute;
        }
      } else {
        minBCoord -= distribute;
      }
    }
    if (maxEndBoxBSize > maxBCoord - minBCoord) {
      // Likewise, but preferring descent to ascent.
      nscoord distribute = maxEndBoxBSize - (maxBCoord - minBCoord);
      nscoord descentSpace = std::max(goodMaxBCoord - maxBCoord, 0);
      if (distribute > descentSpace) {
        distribute -= descentSpace;
        maxBCoord += descentSpace;
        nscoord ascentSpace = std::max(minBCoord - goodMinBCoord, 0);
        if (distribute > ascentSpace) {
          minBCoord -= ascentSpace;
        } else {
          minBCoord -= distribute;
        }
      } else {
        maxBCoord += distribute;
      }
    }

    if (minBCoord > goodMinBCoord) {
      nscoord adjust = minBCoord - goodMinBCoord; // positive

      // shrink the logical extents
      psd->mLogicalBSize -= adjust;
      psd->mBStartLeading -= adjust;
    }
    if (maxBCoord < goodMaxBCoord) {
      nscoord adjust = goodMaxBCoord - maxBCoord;
      psd->mLogicalBSize -= adjust;
      psd->mBEndLeading -= adjust;
    }
    if (minBCoord > 0) {

      // shrink the content by moving its block start down.  This is tricky,
      // since the block start is the 0 for many coordinates, so what we do is
      // move everything else up.
      spanFramePFD->mAscent -= minBCoord; // move the baseline up
      spanFramePFD->mBounds.BSize(lineWM) -= minBCoord; // move the block end up
      psd->mBStartLeading += minBCoord;
      *psd->mBaseline -= minBCoord;

      pfd = psd->mFirstFrame;
      while (nullptr != pfd) {
        pfd->mBounds.BStart(lineWM) -= minBCoord; // move all the children
                                                  // back up
        pfd->mFrame->SetRect(lineWM, pfd->mBounds, mContainerWidth);
        pfd = pfd->mNext;
      }
      maxBCoord -= minBCoord; // since minBCoord is in the frame's own
                              // coordinate system
      minBCoord = 0;
    }
    if (maxBCoord < spanFramePFD->mBounds.BSize(lineWM)) {
      nscoord adjust = spanFramePFD->mBounds.BSize(lineWM) - maxBCoord;
      spanFramePFD->mBounds.BSize(lineWM) -= adjust; // move the bottom up
      psd->mBEndLeading += adjust;
    }
#ifdef NOISY_BLOCKDIR_ALIGN
    printf("     New: minBCoord=%d, maxBCoord=%d, bSize=%d, ascent=%d, logicalBSize=%d, topLeading=%d, bottomLeading=%d\n",
           minBCoord, maxBCoord, spanFramePFD->mBounds.BSize(lineWM),
           spanFramePFD->mAscent,
           psd->mLogicalBSize, psd->mBStartLeading, psd->mBEndLeading);
#endif
  }

  psd->mMinBCoord = minBCoord;
  psd->mMaxBCoord = maxBCoord;
#ifdef NOISY_BLOCKDIR_ALIGN
  printf("  [span]==> minBCoord=%d maxBCoord=%d delta=%d maxStartBoxBSize=%d maxEndBoxBSize=%d\n",
         minBCoord, maxBCoord, maxBCoord - minBCoord, maxStartBoxBSize, maxEndBoxBSize);
#endif
  if (maxStartBoxBSize > mMaxStartBoxBSize) {
    mMaxStartBoxBSize = maxStartBoxBSize;
  }
  if (maxEndBoxBSize > mMaxEndBoxBSize) {
    mMaxEndBoxBSize = maxEndBoxBSize;
  }
}

static void SlideSpanFrameRect(nsIFrame* aFrame, nscoord aDeltaWidth)
{
  // This should not use nsIFrame::MovePositionBy because it happens
  // prior to relative positioning.  In particular, because
  // nsBlockFrame::PlaceLine calls aLineLayout.TrimTrailingWhiteSpace()
  // prior to calling aLineLayout.RelativePositionFrames().
  nsPoint p = aFrame->GetPosition();
  p.x -= aDeltaWidth;
  aFrame->SetPosition(p);
}

bool
nsLineLayout::TrimTrailingWhiteSpaceIn(PerSpanData* psd,
                                       nscoord* aDeltaISize)
{
  PerFrameData* pfd = psd->mFirstFrame;
  if (!pfd) {
    *aDeltaISize = 0;
    return false;
  }
  pfd = pfd->Last();
  while (nullptr != pfd) {
#ifdef REALLY_NOISY_TRIM
    nsFrame::ListTag(stdout, (psd == mRootSpan
                              ? mBlockReflowState->frame
                              : psd->mFrame->mFrame));
    printf(": attempting trim of ");
    nsFrame::ListTag(stdout, pfd->mFrame);
    printf("\n");
#endif
    PerSpanData* childSpan = pfd->mSpan;
    WritingMode lineWM = mRootSpan->mWritingMode;
    if (childSpan) {
      // Maybe the child span has the trailing white-space in it?
      if (TrimTrailingWhiteSpaceIn(childSpan, aDeltaISize)) {
        nscoord deltaISize = *aDeltaISize;
        if (deltaISize) {
          // Adjust the child spans frame size
          pfd->mBounds.ISize(lineWM) -= deltaISize;
          if (psd != mRootSpan) {
            // When the child span is not a direct child of the block
            // we need to update the child spans frame rectangle
            // because it most likely will not be done again. Spans
            // that are direct children of the block will be updated
            // later, however, because the BlockDirAlignFrames method
            // will be run after this method.
            nsIFrame* f = pfd->mFrame;
            LogicalRect r(lineWM, f->GetRect(), mContainerWidth);
            r.ISize(lineWM) -= deltaISize;
            f->SetRect(lineWM, r, mContainerWidth);
          }

          // Adjust the inline end edge of the span that contains the child span
          psd->mICoord -= deltaISize;

          // Slide any frames that follow the child span over by the
          // correct amount. The only thing that can follow the child
          // span is empty stuff, so we are just making things
          // sensible (keeping the combined area honest).
          while (pfd->mNext) {
            pfd = pfd->mNext;
            pfd->mBounds.IStart(lineWM) -= deltaISize;
            if (psd != mRootSpan) {
              // When the child span is not a direct child of the block
              // we need to update the child span's frame rectangle
              // because it most likely will not be done again. Spans
              // that are direct children of the block will be updated
              // later, however, because the BlockDirAlignFrames method
              // will be run after this method.
              SlideSpanFrameRect(pfd->mFrame, deltaISize);
            }
          }
        }
        return true;
      }
    }
    else if (!pfd->GetFlag(PFD_ISTEXTFRAME) &&
             !pfd->GetFlag(PFD_SKIPWHENTRIMMINGWHITESPACE)) {
      // If we hit a frame on the end that's not text and not a placeholder,
      // then there is no trailing whitespace to trim. Stop the search.
      *aDeltaISize = 0;
      return true;
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
        pfd->SetFlag(PFD_RECOMPUTEOVERFLOW, true);
      }

      if (trimOutput.mDeltaWidth) {
        pfd->mBounds.ISize(lineWM) -= trimOutput.mDeltaWidth;

        // See if the text frame has already been placed in its parent
        if (psd != mRootSpan) {
          // The frame was already placed during psd's
          // reflow. Update the frames rectangle now.
          pfd->mFrame->SetRect(lineWM, pfd->mBounds, mContainerWidth);
        }

        // Adjust containing span's right edge
        psd->mICoord -= trimOutput.mDeltaWidth;

        // Slide any frames that follow the text frame over by the
        // right amount. The only thing that can follow the text
        // frame is empty stuff, so we are just making things
        // sensible (keeping the combined area honest).
        while (pfd->mNext) {
          pfd = pfd->mNext;
          pfd->mBounds.IStart(lineWM) -= trimOutput.mDeltaWidth;
          if (psd != mRootSpan) {
            // When the child span is not a direct child of the block
            // we need to update the child spans frame rectangle
            // because it most likely will not be done again. Spans
            // that are direct children of the block will be updated
            // later, however, because the BlockDirAlignFrames method
            // will be run after this method.
            SlideSpanFrameRect(pfd->mFrame, trimOutput.mDeltaWidth);
          }
        }
      }

      if (pfd->GetFlag(PFD_ISNONEMPTYTEXTFRAME) || trimOutput.mChanged) {
        // Pass up to caller so they can shrink their span
        *aDeltaISize = trimOutput.mDeltaWidth;
        return true;
      }
    }
    pfd = pfd->mPrev;
  }

  *aDeltaISize = 0;
  return false;
}

bool
nsLineLayout::TrimTrailingWhiteSpace()
{
  PerSpanData* psd = mRootSpan;
  nscoord deltaISize;
  TrimTrailingWhiteSpaceIn(psd, &deltaISize);
  return 0 != deltaISize;
}

void
nsLineLayout::ComputeJustificationWeights(PerSpanData* aPSD,
                                          int32_t* aNumSpaces,
                                          int32_t* aNumLetters)
{
  NS_ASSERTION(aPSD, "null arg");
  NS_ASSERTION(aNumSpaces, "null arg");
  NS_ASSERTION(aNumLetters, "null arg");
  int32_t numSpaces = 0;
  int32_t numLetters = 0;

  for (PerFrameData* pfd = aPSD->mFirstFrame; pfd != nullptr; pfd = pfd->mNext) {

    if (true == pfd->GetFlag(PFD_ISTEXTFRAME)) {
      numSpaces += pfd->mJustificationNumSpaces;
      numLetters += pfd->mJustificationNumLetters;
    }
    else if (pfd->mSpan != nullptr) {
      int32_t spanSpaces;
      int32_t spanLetters;

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

  nscoord deltaICoord = 0;
  for (PerFrameData* pfd = aPSD->mFirstFrame; pfd != nullptr; pfd = pfd->mNext) {
    // Don't reposition bullets (and other frames that occur out of X-order?)
    if (!pfd->GetFlag(PFD_ISBULLET)) {
      nscoord dw = 0;
      WritingMode lineWM = aPSD->mWritingMode;

      pfd->mBounds.IStart(lineWM) += deltaICoord;

      if (true == pfd->GetFlag(PFD_ISTEXTFRAME)) {
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
          pfd->SetFlag(PFD_RECOMPUTEOVERFLOW, true);
        }
      }
      else {
        if (nullptr != pfd->mSpan) {
          dw += ApplyFrameJustification(pfd->mSpan, aState);
        }
      }

      pfd->mBounds.ISize(lineWM) += dw;

      deltaICoord += dw;
      pfd->mFrame->SetRect(lineWM, pfd->mBounds, mContainerWidth);
    }
  }
  return deltaICoord;
}

void
nsLineLayout::InlineDirAlignFrames(nsRect& aLineBounds,
                                    bool aIsLastLine,
                                    int32_t aFrameCount)
{
  /**
   * NOTE: aIsLastLine ain't necessarily so: it is correctly set by caller
   * only in cases where the last line needs special handling.
   */
  PerSpanData* psd = mRootSpan;
  NS_WARN_IF_FALSE(psd->mIEnd != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
  nscoord availWidth = psd->mIEnd - psd->mIStart;
  nscoord remainingWidth = availWidth - aLineBounds.width;
#ifdef NOISY_INLINEDIR_ALIGN
  nsFrame::ListTag(stdout, mBlockReflowState->frame);
  printf(": availWidth=%d lineBounds.x=%d lineWidth=%d delta=%d\n",
         availWidth, aLineBounds.x, aLineBounds.width, remainingWidth);
#endif

  WritingMode lineWM = psd->mWritingMode;

  // 'text-align-last: auto' is equivalent to the value of the 'text-align'
  // property except when 'text-align' is set to 'justify', in which case it
  // is 'justify' when 'text-justify' is 'distribute' and 'start' otherwise.
  //
  // XXX: the code below will have to change when we implement text-justify
  //
  nscoord dx = 0;
  uint8_t textAlign = mStyleText->mTextAlign;
  bool textAlignTrue = mStyleText->mTextAlignTrue;
  if (aIsLastLine) {
    textAlignTrue = mStyleText->mTextAlignLastTrue;
    if (mStyleText->mTextAlignLast == NS_STYLE_TEXT_ALIGN_AUTO) {
      if (textAlign == NS_STYLE_TEXT_ALIGN_JUSTIFY) {
        textAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
      }
    } else {
      textAlign = mStyleText->mTextAlignLast;
    }
  }

  if ((remainingWidth > 0 || textAlignTrue) &&
      !(mBlockReflowState->frame->IsSVGText())) {

    switch (textAlign) {
      case NS_STYLE_TEXT_ALIGN_JUSTIFY:
        int32_t numSpaces;
        int32_t numLetters;
            
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
        // Fall through to the default case if we could not justify to fill
        // the space.

      case NS_STYLE_TEXT_ALIGN_DEFAULT:
        // default alignment is to start edge so do nothing
        break;

      case NS_STYLE_TEXT_ALIGN_LEFT:
      case NS_STYLE_TEXT_ALIGN_MOZ_LEFT:
        if (!lineWM.IsBidiLTR()) {
          dx = remainingWidth;
        }
        break;

      case NS_STYLE_TEXT_ALIGN_RIGHT:
      case NS_STYLE_TEXT_ALIGN_MOZ_RIGHT:
        if (lineWM.IsBidiLTR()) {
          dx = remainingWidth;
        }
        break;

      case NS_STYLE_TEXT_ALIGN_END:
        dx = remainingWidth;
        break;


      case NS_STYLE_TEXT_ALIGN_CENTER:
      case NS_STYLE_TEXT_ALIGN_MOZ_CENTER:
        dx = remainingWidth / 2;
        break;
    }
  }

  if (dx) {
    for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
      pfd->mBounds.IStart(lineWM) += dx;
      pfd->mFrame->SetRect(lineWM, pfd->mBounds, mContainerWidth);
    }
    aLineBounds.x += dx;
  }

  if (mPresContext->BidiEnabled() &&
      (!mPresContext->IsVisualMode() || !lineWM.IsBidiLTR())) {
    nsBidiPresUtils::ReorderFrames(psd->mFirstFrame->mFrame, aFrameCount,
                                   lineWM, mContainerWidth);
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
  WritingMode wm = psd->mWritingMode;
  if (nullptr != psd->mFrame) {
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
    overflowAreas.VisualOverflow().UnionRect(
      psd->mFrame->mOverflowAreas.VisualOverflow(), adjustedBounds);
  }
  else {
    LogicalRect rect(wm, psd->mIStart, mBStartEdge,
                     psd->mICoord - psd->mIStart, mFinalLineBSize);
    // The minimum combined area for the frames that are direct
    // children of the block starts at the upper left corner of the
    // line and is sized to match the size of the line's bounding box
    // (the same size as the values returned from BlockDirAlignFrames)
    overflowAreas.VisualOverflow() = rect.GetPhysicalRect(wm, mContainerWidth);
    overflowAreas.ScrollableOverflow() = overflowAreas.VisualOverflow();
  }

  for (PerFrameData* pfd = psd->mFirstFrame; pfd; pfd = pfd->mNext) {
    nsIFrame* frame = pfd->mFrame;
    nsPoint origin = frame->GetPosition();

    // Adjust the origin of the frame
    if (pfd->GetFlag(PFD_RELATIVEPOS)) {
      //XXX temporary until ApplyRelativePositioning can handle logical offsets
      nsMargin physicalOffsets =
        pfd->mOffsets.GetPhysicalMargin(pfd->mFrame->GetWritingMode());
      // right and bottom are handled by
      // nsHTMLReflowState::ComputeRelativeOffsets
      nsHTMLReflowState::ApplyRelativePositioning(pfd->mFrame,
                                                  physicalOffsets,
                                                  &origin);
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
        //     overflow area until Reflow and BlockDirAlignLine have finished
        if (pfd->GetFlag(PFD_RECOMPUTEOVERFLOW) ||
            frame->StyleContext()->HasTextDecorationLines()) {
          nsTextFrame* f = static_cast<nsTextFrame*>(frame);
          r = f->RecomputeOverflow(*mBlockReflowState);
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
