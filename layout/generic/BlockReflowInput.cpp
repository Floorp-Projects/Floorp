/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* state used in reflow of block frames */

#include "BlockReflowInput.h"

#include <algorithm>
#include "LayoutLogging.h"
#include "nsBlockFrame.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsIFrameInlines.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_layout.h"
#include "TextOverflow.h"

#ifdef DEBUG
#  include "nsBlockDebugFlags.h"
#endif

using namespace mozilla;
using namespace mozilla::layout;

BlockReflowInput::BlockReflowInput(const ReflowInput& aReflowInput,
                                   nsPresContext* aPresContext,
                                   nsBlockFrame* aFrame, bool aBStartMarginRoot,
                                   bool aBEndMarginRoot,
                                   bool aBlockNeedsFloatManager,
                                   nscoord aConsumedBSize)
    : mBlock(aFrame),
      mPresContext(aPresContext),
      mReflowInput(aReflowInput),
      mContentArea(aReflowInput.GetWritingMode()),
      mPushedFloats(nullptr),
      mOverflowTracker(nullptr),
      mBorderPadding(
          mReflowInput
              .ComputedLogicalBorderPadding(mReflowInput.GetWritingMode())
              .ApplySkipSides(aFrame->PreReflowBlockLevelLogicalSkipSides())),
      mPrevBEndMargin(),
      mMinLineHeight(aReflowInput.GetLineHeight()),
      mLineNumber(0),
      mFloatBreakType(StyleClear::None),
      mConsumedBSize(aConsumedBSize) {
  NS_ASSERTION(mConsumedBSize != NS_UNCONSTRAINEDSIZE,
               "The consumed block-size should be constrained!");

  WritingMode wm = aReflowInput.GetWritingMode();

  // Note that mContainerSize is the physical size, needed to
  // convert logical block-coordinates in vertical-rl writing mode
  // (measured from a RHS origin) to physical coordinates within the
  // containing block.
  // If aReflowInput doesn't have a constrained ComputedWidth(), we set
  // mContainerSize.width to zero, which means lines will be positioned
  // (physically) incorrectly; we will fix them up at the end of
  // nsBlockFrame::Reflow, after we know the total block-size of the
  // frame.
  mContainerSize.width = aReflowInput.ComputedWidth();
  if (mContainerSize.width == NS_UNCONSTRAINEDSIZE) {
    mContainerSize.width = 0;
  }

  mContainerSize.width += mBorderPadding.LeftRight(wm);

  // For now at least, we don't do that fix-up for mContainerHeight.
  // It's only used in nsBidiUtils::ReorderFrames for vertical rtl
  // writing modes, which aren't fully supported for the time being.
  mContainerSize.height =
      aReflowInput.ComputedHeight() + mBorderPadding.TopBottom(wm);

  if (aBStartMarginRoot || 0 != mBorderPadding.BStart(wm)) {
    mFlags.mIsBStartMarginRoot = true;
    mFlags.mShouldApplyBStartMargin = true;
  }
  if (aBEndMarginRoot || 0 != mBorderPadding.BEnd(wm)) {
    mFlags.mIsBEndMarginRoot = true;
  }
  if (aBlockNeedsFloatManager) {
    mFlags.mBlockNeedsFloatManager = true;
  }

  // We need to check mInsideLineClamp here since we are here before the block
  // has been reflowed, and CanHaveOverflowMarkers() relies on the block's
  // NS_BLOCK_HAS_LINE_CLAMP_ELLIPSIS state bit to know if a -webkit-line-clamp
  // ellipsis is set on one of the block's lines.  And that state bit is only
  // set after we do the bsize measuring reflow of the flex item.
  mFlags.mCanHaveOverflowMarkers =
      aReflowInput.mFlags.mInsideLineClamp ||
      css::TextOverflow::CanHaveOverflowMarkers(mBlock);

  MOZ_ASSERT(FloatManager(),
             "Float manager should be valid when creating BlockReflowInput!");

  // Save the coordinate system origin for later.
  FloatManager()->GetTranslation(mFloatManagerI, mFloatManagerB);
  FloatManager()->PushState(&mFloatManagerStateBefore);  // never popped

  mNextInFlow = static_cast<nsBlockFrame*>(mBlock->GetNextInFlow());

  LAYOUT_WARN_IF_FALSE(NS_UNCONSTRAINEDSIZE != aReflowInput.ComputedISize(),
                       "have unconstrained width; this should only result "
                       "from very large sizes, not attempts at intrinsic "
                       "width calculation");
  mContentArea.ISize(wm) = aReflowInput.ComputedISize();

  // Compute content area block-size. Unlike the inline-size, if we have a
  // specified style block-size, we ignore it since extra content is managed by
  // the "overflow" property. When we don't have a specified style block-size,
  // then we may end up limiting our block-size if the available block-size is
  // constrained (this situation occurs when we are paginated).
  if (NS_UNCONSTRAINEDSIZE != aReflowInput.AvailableBSize()) {
    // We are in a paginated situation. The block-end edge is just inside the
    // block-end border and padding. The content area block-size doesn't include
    // either border or padding edge.
    auto bp = aFrame->StyleBorder()->mBoxDecorationBreak ==
                      StyleBoxDecorationBreak::Clone
                  ? mBorderPadding.BStartEnd(wm)
                  : mBorderPadding.BStart(wm);
    mContentArea.BSize(wm) = std::max(0, aReflowInput.AvailableBSize() - bp);
  } else {
    // When we are not in a paginated situation, then we always use a
    // unconstrained block-size.
    mContentArea.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  }
  mContentArea.IStart(wm) = mBorderPadding.IStart(wm);
  mBCoord = mContentArea.BStart(wm) = mBorderPadding.BStart(wm);

  mPrevChild = nullptr;
  mCurrentLine = aFrame->LinesEnd();
}

void BlockReflowInput::ComputeReplacedBlockOffsetsForFloats(
    nsIFrame* aFrame, const LogicalRect& aFloatAvailableSpace,
    nscoord& aIStartResult, nscoord& aIEndResult) const {
  WritingMode wm = mReflowInput.GetWritingMode();
  // The frame is clueless about the float manager and therefore we
  // only give it free space. An example is a table frame - the
  // tables do not flow around floats.
  // However, we can let its margins intersect floats.
  NS_ASSERTION(aFloatAvailableSpace.IStart(wm) >= mContentArea.IStart(wm),
               "bad avail space rect inline-coord");
  NS_ASSERTION(aFloatAvailableSpace.ISize(wm) == 0 ||
                   aFloatAvailableSpace.IEnd(wm) <= mContentArea.IEnd(wm),
               "bad avail space rect inline-size");

  nscoord iStartOffset, iEndOffset;
  if (aFloatAvailableSpace.ISize(wm) == mContentArea.ISize(wm)) {
    // We don't need to compute margins when there are no floats around.
    iStartOffset = 0;
    iEndOffset = 0;
  } else {
    LogicalMargin frameMargin(wm);
    SizeComputationInput os(aFrame, mReflowInput.mRenderingContext, wm,
                            mContentArea.ISize(wm));
    frameMargin = os.ComputedLogicalMargin(wm);

    nscoord iStartFloatIOffset =
        aFloatAvailableSpace.IStart(wm) - mContentArea.IStart(wm);
    iStartOffset = std::max(iStartFloatIOffset, frameMargin.IStart(wm)) -
                   frameMargin.IStart(wm);
    iStartOffset = std::max(iStartOffset, 0);  // in case of negative margin
    nscoord iEndFloatIOffset =
        mContentArea.IEnd(wm) - aFloatAvailableSpace.IEnd(wm);
    iEndOffset =
        std::max(iEndFloatIOffset, frameMargin.IEnd(wm)) - frameMargin.IEnd(wm);
    iEndOffset = std::max(iEndOffset, 0);  // in case of negative margin
  }
  aIStartResult = iStartOffset;
  aIEndResult = iEndOffset;
}

// Compute the amount of available space for reflowing a block frame
// at the current block-direction coordinate. This method assumes that
// GetFloatAvailableSpace has already been called.
void BlockReflowInput::ComputeBlockAvailSpace(
    nsIFrame* aFrame, const nsFlowAreaRect& aFloatAvailableSpace,
    bool aBlockAvoidsFloats, LogicalRect& aResult) {
#ifdef REALLY_NOISY_REFLOW
  printf("CBAS frame=%p has floats %d\n", aFrame,
         aFloatAvailableSpace.HasFloats());
#endif
  WritingMode wm = mReflowInput.GetWritingMode();
  const nscoord availBSize = mReflowInput.AvailableBSize();
  aResult.BStart(wm) = mBCoord;
  aResult.BSize(wm) = availBSize == NS_UNCONSTRAINEDSIZE ? NS_UNCONSTRAINEDSIZE
                                                         : availBSize - mBCoord;
  // mBCoord might be greater than ContentBEnd() if the block's top margin
  // pushes it off the page/column. Negative available block-size can confuse
  // other code and is nonsense in principle.

  // XXX Do we really want this condition to be this restrictive (i.e.,
  // more restrictive than it used to be)?  The |else| here is allowed
  // by the CSS spec, but only out of desperation given implementations,
  // and the behavior it leads to is quite undesirable (it can cause
  // things to become extremely narrow when they'd fit quite well a
  // little bit lower).  Should the else be a quirk or something that
  // applies to a specific set of frame classes and no new ones?
  // If we did that, then for those frames where the condition below is
  // true but nsBlockFrame::BlockCanIntersectFloats is false,
  // nsBlockFrame::ISizeToClearPastFloats would need to use the
  // shrink-wrap formula, max(MinISize, min(avail width, PrefISize))
  // rather than just using MinISize.
  NS_ASSERTION(
      nsBlockFrame::BlockCanIntersectFloats(aFrame) == !aBlockAvoidsFloats,
      "unexpected replaced width");
  if (!aBlockAvoidsFloats) {
    if (aFloatAvailableSpace.HasFloats()) {
      // Use the float-edge property to determine how the child block
      // will interact with the float.
      const nsStyleBorder* borderStyle = aFrame->StyleBorder();
      switch (borderStyle->mFloatEdge) {
        default:
        case StyleFloatEdge::ContentBox:  // content and only content does
                                          // runaround of floats
          // The child block will flow around the float. Therefore
          // give it all of the available space.
          aResult.IStart(wm) = mContentArea.IStart(wm);
          aResult.ISize(wm) = mContentArea.ISize(wm);
          break;
        case StyleFloatEdge::MarginBox: {
          // The child block's margins should be placed adjacent to,
          // but not overlap the float.
          aResult.IStart(wm) = aFloatAvailableSpace.mRect.IStart(wm);
          aResult.ISize(wm) = aFloatAvailableSpace.mRect.ISize(wm);
        } break;
      }
    } else {
      // Since there are no floats present the float-edge property
      // doesn't matter therefore give the block element all of the
      // available space since it will flow around the float itself.
      aResult.IStart(wm) = mContentArea.IStart(wm);
      aResult.ISize(wm) = mContentArea.ISize(wm);
    }
  } else {
    nscoord iStartOffset, iEndOffset;
    ComputeReplacedBlockOffsetsForFloats(aFrame, aFloatAvailableSpace.mRect,
                                         iStartOffset, iEndOffset);
    aResult.IStart(wm) = mContentArea.IStart(wm) + iStartOffset;
    aResult.ISize(wm) = mContentArea.ISize(wm) - iStartOffset - iEndOffset;
  }

#ifdef REALLY_NOISY_REFLOW
  printf("  CBAS: result %d %d %d %d\n", aResult.IStart(wm), aResult.BStart(wm),
         aResult.ISize(wm), aResult.BSize(wm));
#endif
}

bool BlockReflowInput::ReplacedBlockFitsInAvailSpace(
    nsIFrame* aReplacedBlock,
    const nsFlowAreaRect& aFloatAvailableSpace) const {
  if (!aFloatAvailableSpace.HasFloats()) {
    // If there aren't any floats here, then we always fit.
    // We check this before calling ISizeToClearPastFloats, which is
    // somewhat expensive.
    return true;
  }
  WritingMode wm = mReflowInput.GetWritingMode();
  nsBlockFrame::ReplacedElementISizeToClear replacedISize =
      nsBlockFrame::ISizeToClearPastFloats(*this, aFloatAvailableSpace.mRect,
                                           aReplacedBlock);
  // The inline-start side of the replaced element should be offset by
  // the larger of the float intrusion or the replaced element's own
  // start margin.  The inline-end side is similar, except for Web
  // compatibility we ignore the margin.
  return std::max(
             aFloatAvailableSpace.mRect.IStart(wm) - mContentArea.IStart(wm),
             replacedISize.marginIStart) +
             replacedISize.borderBoxISize +
             (mContentArea.IEnd(wm) - aFloatAvailableSpace.mRect.IEnd(wm)) <=
         mContentArea.ISize(wm);
}

nsFlowAreaRect BlockReflowInput::GetFloatAvailableSpaceWithState(
    nscoord aBCoord, ShapeType aShapeType,
    nsFloatManager::SavedState* aState) const {
  WritingMode wm = mReflowInput.GetWritingMode();
#ifdef DEBUG
  // Verify that the caller setup the coordinate system properly
  nscoord wI, wB;
  FloatManager()->GetTranslation(wI, wB);

  NS_ASSERTION((wI == mFloatManagerI) && (wB == mFloatManagerB),
               "bad coord system");
#endif

  nscoord blockSize = (mContentArea.BSize(wm) == nscoord_MAX)
                          ? nscoord_MAX
                          : std::max(mContentArea.BEnd(wm) - aBCoord, 0);
  nsFlowAreaRect result = FloatManager()->GetFlowArea(
      wm, aBCoord, blockSize, BandInfoType::BandFromPoint, aShapeType,
      mContentArea, aState, ContainerSize());
  // Keep the inline size >= 0 for compatibility with nsSpaceManager.
  if (result.mRect.ISize(wm) < 0) {
    result.mRect.ISize(wm) = 0;
  }

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsIFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("%s: band=%d,%d,%d,%d hasfloats=%d\n", __func__,
           result.mRect.IStart(wm), result.mRect.BStart(wm),
           result.mRect.ISize(wm), result.mRect.BSize(wm), result.HasFloats());
  }
#endif
  return result;
}

nsFlowAreaRect BlockReflowInput::GetFloatAvailableSpaceForBSize(
    nscoord aBCoord, nscoord aBSize, nsFloatManager::SavedState* aState) const {
  WritingMode wm = mReflowInput.GetWritingMode();
#ifdef DEBUG
  // Verify that the caller setup the coordinate system properly
  nscoord wI, wB;
  FloatManager()->GetTranslation(wI, wB);

  NS_ASSERTION((wI == mFloatManagerI) && (wB == mFloatManagerB),
               "bad coord system");
#endif
  nsFlowAreaRect result = FloatManager()->GetFlowArea(
      wm, aBCoord, aBSize, BandInfoType::WidthWithinHeight,
      ShapeType::ShapeOutside, mContentArea, aState, ContainerSize());
  // Keep the width >= 0 for compatibility with nsSpaceManager.
  if (result.mRect.ISize(wm) < 0) {
    result.mRect.ISize(wm) = 0;
  }

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsIFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("%s: space=%d,%d,%d,%d hasfloats=%d\n", __func__,
           result.mRect.IStart(wm), result.mRect.BStart(wm),
           result.mRect.ISize(wm), result.mRect.BSize(wm), result.HasFloats());
  }
#endif
  return result;
}

/*
 * Reconstruct the vertical margin before the line |aLine| in order to
 * do an incremental reflow that begins with |aLine| without reflowing
 * the line before it.  |aLine| may point to the fencepost at the end of
 * the line list, and it is used this way since we (for now, anyway)
 * always need to recover margins at the end of a block.
 *
 * The reconstruction involves walking backward through the line list to
 * find any collapsed margins preceding the line that would have been in
 * the reflow input's |mPrevBEndMargin| when we reflowed that line in
 * a full reflow (under the rule in CSS2 that all adjacent vertical
 * margins of blocks collapse).
 */
void BlockReflowInput::ReconstructMarginBefore(nsLineList::iterator aLine) {
  mPrevBEndMargin.Zero();
  nsBlockFrame* block = mBlock;

  nsLineList::iterator firstLine = block->LinesBegin();
  for (;;) {
    --aLine;
    if (aLine->IsBlock()) {
      mPrevBEndMargin = aLine->GetCarriedOutBEndMargin();
      break;
    }
    if (!aLine->IsEmpty()) {
      break;
    }
    if (aLine == firstLine) {
      // If the top margin was carried out (and thus already applied),
      // set it to zero.  Either way, we're done.
      if (!mFlags.mIsBStartMarginRoot) {
        mPrevBEndMargin.Zero();
      }
      break;
    }
  }
}

void BlockReflowInput::SetupPushedFloatList() {
  MOZ_ASSERT(!mFlags.mIsFloatListInBlockPropertyTable == !mPushedFloats,
             "flag mismatch");
  if (!mFlags.mIsFloatListInBlockPropertyTable) {
    // If we're being re-Reflow'd without our next-in-flow having been
    // reflowed, some pushed floats from our previous reflow might
    // still be on our pushed floats list.  However, that's
    // actually fine, since they'll all end up being stolen and
    // reordered into the correct order again.
    // (nsBlockFrame::ReflowDirtyLines ensures that any lines with
    // pushed floats are reflowed.)
    mPushedFloats = mBlock->EnsurePushedFloats();
    mFlags.mIsFloatListInBlockPropertyTable = true;
  }
}

void BlockReflowInput::AppendPushedFloatChain(nsIFrame* aFloatCont) {
  SetupPushedFloatList();
  while (true) {
    aFloatCont->AddStateBits(NS_FRAME_IS_PUSHED_FLOAT);
    mPushedFloats->AppendFrame(mBlock, aFloatCont);
    aFloatCont = aFloatCont->GetNextInFlow();
    if (!aFloatCont || aFloatCont->GetParent() != mBlock) {
      break;
    }
    mBlock->StealFrame(aFloatCont);
  }
}

/**
 * Restore information about floats into the float manager for an
 * incremental reflow, and simultaneously push the floats by
 * |aDeltaBCoord|, which is the amount |aLine| was pushed relative to its
 * parent.  The recovery of state is one of the things that makes
 * incremental reflow O(N^2) and this state should really be kept
 * around, attached to the frame tree.
 */
void BlockReflowInput::RecoverFloats(nsLineList::iterator aLine,
                                     nscoord aDeltaBCoord) {
  WritingMode wm = mReflowInput.GetWritingMode();
  if (aLine->HasFloats()) {
    // Place the floats into the float manager again. Also slide
    // them, just like the regular frames on the line.
    nsFloatCache* fc = aLine->GetFirstFloat();
    while (fc) {
      nsIFrame* floatFrame = fc->mFloat;
      if (aDeltaBCoord != 0) {
        floatFrame->MovePositionBy(nsPoint(0, aDeltaBCoord));
        nsContainerFrame::PositionFrameView(floatFrame);
        nsContainerFrame::PositionChildViews(floatFrame);
      }
#ifdef DEBUG
      if (nsBlockFrame::gNoisyReflow || nsBlockFrame::gNoisyFloatManager) {
        nscoord tI, tB;
        FloatManager()->GetTranslation(tI, tB);
        nsIFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
        printf("RecoverFloats: tIB=%d,%d (%d,%d) ", tI, tB, mFloatManagerI,
               mFloatManagerB);
        floatFrame->ListTag(stdout);
        LogicalRect region =
            nsFloatManager::GetRegionFor(wm, floatFrame, ContainerSize());
        printf(" aDeltaBCoord=%d region={%d,%d,%d,%d}\n", aDeltaBCoord,
               region.IStart(wm), region.BStart(wm), region.ISize(wm),
               region.BSize(wm));
      }
#endif
      FloatManager()->AddFloat(
          floatFrame,
          nsFloatManager::GetRegionFor(wm, floatFrame, ContainerSize()), wm,
          ContainerSize());
      fc = fc->Next();
    }
  } else if (aLine->IsBlock()) {
    nsBlockFrame::RecoverFloatsFor(aLine->mFirstChild, *FloatManager(), wm,
                                   ContainerSize());
  }
}

/**
 * Everything done in this function is done O(N) times for each pass of
 * reflow so it is O(N*M) where M is the number of incremental reflow
 * passes.  That's bad.  Don't do stuff here.
 *
 * When this function is called, |aLine| has just been slid by |aDeltaBCoord|
 * and the purpose of RecoverStateFrom is to ensure that the
 * BlockReflowInput is in the same state that it would have been in
 * had the line just been reflowed.
 *
 * Most of the state recovery that we have to do involves floats.
 */
void BlockReflowInput::RecoverStateFrom(nsLineList::iterator aLine,
                                        nscoord aDeltaBCoord) {
  // Make the line being recovered the current line
  mCurrentLine = aLine;

  // Place floats for this line into the float manager
  if (aLine->HasFloats() || aLine->IsBlock()) {
    RecoverFloats(aLine, aDeltaBCoord);

#ifdef DEBUG
    if (nsBlockFrame::gNoisyReflow || nsBlockFrame::gNoisyFloatManager) {
      FloatManager()->List(stdout);
    }
#endif
  }
}

// This is called by the line layout's AddFloat method when a
// place-holder frame is reflowed in a line. If the float is a
// left-most child (it's x coordinate is at the line's left margin)
// then the float is place immediately, otherwise the float
// placement is deferred until the line has been reflowed.

// XXXldb This behavior doesn't quite fit with CSS1 and CSS2 --
// technically we're supposed let the current line flow around the
// float as well unless it won't fit next to what we already have.
// But nobody else implements it that way...
bool BlockReflowInput::AddFloat(nsLineLayout* aLineLayout, nsIFrame* aFloat,
                                nscoord aAvailableISize) {
  MOZ_ASSERT(aLineLayout, "must have line layout");
  MOZ_ASSERT(mBlock->LinesEnd() != mCurrentLine, "null ptr");
  MOZ_ASSERT(aFloat->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW),
             "aFloat must be an out-of-flow frame");

  MOZ_ASSERT(aFloat->GetParent(), "float must have parent");
  MOZ_ASSERT(aFloat->GetParent()->IsBlockFrameOrSubclass(),
             "float's parent must be block");
  if (aFloat->HasAnyStateBits(NS_FRAME_IS_PUSHED_FLOAT) ||
      aFloat->GetParent() != mBlock) {
    MOZ_ASSERT(aFloat->HasAnyStateBits(NS_FRAME_IS_PUSHED_FLOAT |
                                       NS_FRAME_FIRST_REFLOW),
               "float should be in this block unless it was marked as "
               "pushed float, or just inserted");
    MOZ_ASSERT(aFloat->GetParent()->FirstContinuation() ==
               mBlock->FirstContinuation());
    // If, in a previous reflow, the float was pushed entirely to
    // another column/page, we need to steal it back.  (We might just
    // push it again, though.)  Likewise, if that previous reflow
    // reflowed this block but not its next continuation, we might need
    // to steal it from our own float-continuations list.
    //
    // For more about pushed floats, see the comment above
    // nsBlockFrame::DrainPushedFloats.
    auto* floatParent = static_cast<nsBlockFrame*>(aFloat->GetParent());
    floatParent->StealFrame(aFloat);

    aFloat->RemoveStateBits(NS_FRAME_IS_PUSHED_FLOAT);

    // Appending is fine, since if a float was pushed to the next
    // page/column, all later floats were also pushed.
    mBlock->mFloats.AppendFrame(mBlock, aFloat);
  }

  // Because we are in the middle of reflowing a placeholder frame
  // within a line (and possibly nested in an inline frame or two
  // that's a child of our block) we need to restore the space
  // manager's translation to the space that the block resides in
  // before placing the float.
  nscoord oI, oB;
  FloatManager()->GetTranslation(oI, oB);
  nscoord dI = oI - mFloatManagerI;
  nscoord dB = oB - mFloatManagerB;
  FloatManager()->Translate(-dI, -dB);

  bool placed;

  // Now place the float immediately if possible. Otherwise stash it
  // away in mBelowCurrentLineFloats and place it later.
  // If one or more floats has already been pushed to the next line,
  // don't let this one go on the current line, since that would violate
  // float ordering.
  LogicalRect floatAvailableSpace =
      GetFloatAvailableSpaceForPlacingFloat(mBCoord).mRect;
  if (mBelowCurrentLineFloats.IsEmpty() &&
      (aLineLayout->LineIsEmpty() ||
       mBlock->ComputeFloatISize(*this, floatAvailableSpace, aFloat) <=
           aAvailableISize)) {
    // And then place it
    placed = FlowAndPlaceFloat(aFloat);
    if (placed) {
      // Pass on updated available space to the current inline reflow engine
      WritingMode wm = mReflowInput.GetWritingMode();
      // If we have mLineBSize, we are reflowing the line again due to
      // LineReflowStatus::RedoMoreFloats. We should use mLineBSize to query the
      // correct available space.
      nsFlowAreaRect floatAvailSpace =
          mLineBSize.isNothing() ? GetFloatAvailableSpace(mBCoord)
                                 : GetFloatAvailableSpaceForBSize(
                                       mBCoord, mLineBSize.value(), nullptr);
      LogicalRect availSpace(wm, floatAvailSpace.mRect.IStart(wm), mBCoord,
                             floatAvailSpace.mRect.ISize(wm),
                             floatAvailSpace.mRect.BSize(wm));
      aLineLayout->UpdateBand(wm, availSpace, aFloat);
      // Record this float in the current-line list
      mCurrentLineFloats.Append(mFloatCacheFreeList.Alloc(aFloat));
    } else {
      (*aLineLayout->GetLine())->SetHadFloatPushed();
    }
  } else {
    // Always claim to be placed; we don't know whether we fit yet, so we
    // deal with this in PlaceBelowCurrentLineFloats
    placed = true;
    // This float will be placed after the line is done (it is a
    // below-current-line float).
    mBelowCurrentLineFloats.Append(mFloatCacheFreeList.Alloc(aFloat));
  }

  // Restore coordinate system
  FloatManager()->Translate(dI, dB);

  return placed;
}

bool BlockReflowInput::CanPlaceFloat(
    nscoord aFloatISize, const nsFlowAreaRect& aFloatAvailableSpace) {
  // A float fits at a given block-dir position if there are no floats
  // at its inline-dir position (no matter what its inline size) or if
  // its inline size fits in the space remaining after prior floats have
  // been placed.
  // FIXME: We should allow overflow by up to half a pixel here (bug 21193).
  return !aFloatAvailableSpace.HasFloats() ||
         aFloatAvailableSpace.mRect.ISize(mReflowInput.GetWritingMode()) >=
             aFloatISize;
}

// Return the inline-size that the float (including margins) will take up
// in the writing mode of the containing block. If this returns
// NS_UNCONSTRAINEDSIZE, we're dealing with an orthogonal block that
// has block-size:auto, and we'll need to actually reflow it to find out
// how much inline-size it will occupy in the containing block's mode.
static nscoord FloatMarginISize(const ReflowInput& aCBReflowInput,
                                nscoord aFloatAvailableISize, nsIFrame* aFloat,
                                const SizeComputationInput& aFloatOffsetState) {
  AutoMaybeDisableFontInflation an(aFloat);
  WritingMode wm = aFloatOffsetState.GetWritingMode();

  auto floatSize = aFloat->ComputeSize(
      aCBReflowInput.mRenderingContext, wm, aCBReflowInput.ComputedSize(wm),
      aFloatAvailableISize,
      aFloatOffsetState.ComputedLogicalMargin(wm).Size(wm),
      aFloatOffsetState.ComputedLogicalBorderPadding(wm).Size(wm), {},
      ComputeSizeFlag::ShrinkWrap);

  WritingMode cbwm = aCBReflowInput.GetWritingMode();
  nscoord floatISize = floatSize.mLogicalSize.ConvertTo(cbwm, wm).ISize(cbwm);
  if (floatISize == NS_UNCONSTRAINEDSIZE) {
    return NS_UNCONSTRAINEDSIZE;  // reflow is needed to get the true size
  }

  return floatISize +
         aFloatOffsetState.ComputedLogicalMargin(cbwm).IStartEnd(cbwm) +
         aFloatOffsetState.ComputedLogicalBorderPadding(cbwm).IStartEnd(cbwm);
}

// A frame property that stores the last shape source / margin / etc. if there's
// any shape, in order to invalidate the float area properly when it changes.
//
// TODO(emilio): This could really belong to GetRegionFor / StoreRegionFor, but
// when I tried it was a bit awkward because of the logical -> physical
// conversion that happens there.
//
// Maybe all this code could be refactored to make this cleaner, but keeping the
// two properties separated was slightly nicer.
struct ShapeInvalidationData {
  StyleShapeOutside mShapeOutside{StyleShapeOutside::None()};
  float mShapeImageThreshold = 0.0;
  LengthPercentage mShapeMargin;

  ShapeInvalidationData() = default;

  explicit ShapeInvalidationData(const nsStyleDisplay& aDisplay) {
    Update(aDisplay);
  }

  static bool IsNeeded(const nsStyleDisplay& aDisplay) {
    return !aDisplay.mShapeOutside.IsNone();
  }

  void Update(const nsStyleDisplay& aDisplay) {
    MOZ_ASSERT(IsNeeded(aDisplay));
    mShapeOutside = aDisplay.mShapeOutside;
    mShapeImageThreshold = aDisplay.mShapeImageThreshold;
    mShapeMargin = aDisplay.mShapeMargin;
  }

  bool Matches(const nsStyleDisplay& aDisplay) const {
    return mShapeOutside == aDisplay.mShapeOutside &&
           mShapeImageThreshold == aDisplay.mShapeImageThreshold &&
           mShapeMargin == aDisplay.mShapeMargin;
  }
};

NS_DECLARE_FRAME_PROPERTY_DELETABLE(ShapeInvalidationDataProperty,
                                    ShapeInvalidationData)

bool BlockReflowInput::FlowAndPlaceFloat(nsIFrame* aFloat) {
  MOZ_ASSERT(aFloat->GetParent() == mBlock);

  WritingMode wm = mReflowInput.GetWritingMode();
  // Save away the Y coordinate before placing the float. We will
  // restore mBCoord at the end after placing the float. This is
  // necessary because any adjustments to mBCoord during the float
  // placement are for the float only, not for any non-floating
  // content.
  AutoRestore<nscoord> restoreBCoord(mBCoord);

  // Grab the float's display information
  const nsStyleDisplay* floatDisplay = aFloat->StyleDisplay();

  // The float's old region, so we can propagate damage.
  LogicalRect oldRegion =
      nsFloatManager::GetRegionFor(wm, aFloat, ContainerSize());

  ShapeInvalidationData* invalidationData =
      aFloat->GetProperty(ShapeInvalidationDataProperty());

  // Enforce CSS2 9.5.1 rule [2], i.e., make sure that a float isn't
  // ``above'' another float that preceded it in the flow.
  mBCoord = std::max(FloatManager()->GetLowestFloatTop(), mBCoord);

  // See if the float should clear any preceding floats...
  // XXX We need to mark this float somehow so that it gets reflowed
  // when floats are inserted before it.
  if (StyleClear::None != floatDisplay->mBreakType) {
    // XXXldb Does this handle vertical margins correctly?
    auto [bCoord, result] = ClearFloats(mBCoord, floatDisplay->mBreakType);
    if (result == ClearFloatsResult::FloatsPushedOrSplit) {
      PushFloatPastBreak(aFloat);
      return false;
    }
    mBCoord = bCoord;
  }

  // Get the band of available space with respect to margin box.
  nsFlowAreaRect floatAvailableSpace =
      GetFloatAvailableSpaceForPlacingFloat(mBCoord);
  LogicalRect adjustedAvailableSpace =
      mBlock->AdjustFloatAvailableSpace(*this, floatAvailableSpace.mRect);

  NS_ASSERTION(aFloat->GetParent() == mBlock, "Float frame has wrong parent");

  SizeComputationInput offsets(aFloat, mReflowInput.mRenderingContext, wm,
                               mReflowInput.ComputedISize());

  nscoord floatMarginISize = FloatMarginISize(
      mReflowInput, adjustedAvailableSpace.ISize(wm), aFloat, offsets);

  LogicalMargin floatMargin(wm);  // computed margin
  LogicalMargin floatOffsets(wm);
  nsReflowStatus reflowStatus;

  // If it's a floating first-letter, we need to reflow it before we
  // know how wide it is (since we don't compute which letters are part
  // of the first letter until reflow!).
  // We also need to do this early reflow if FloatMarginISize returned
  // an unconstrained inline-size, which can occur if the float had an
  // orthogonal writing mode and 'auto' block-size (in its mode).
  bool earlyFloatReflow =
      aFloat->IsLetterFrame() || floatMarginISize == NS_UNCONSTRAINEDSIZE;
  if (earlyFloatReflow) {
    mBlock->ReflowFloat(*this, adjustedAvailableSpace, aFloat, floatMargin,
                        floatOffsets, false, reflowStatus);
    floatMarginISize = aFloat->ISize(wm) + floatMargin.IStartEnd(wm);
    NS_ASSERTION(reflowStatus.IsComplete(),
                 "letter frames and orthogonal floats with auto block-size "
                 "shouldn't break, and if they do now, then they're breaking "
                 "at the wrong point");
  }

  // Find a place to place the float. The CSS2 spec doesn't want
  // floats overlapping each other or sticking out of the containing
  // block if possible (CSS2 spec section 9.5.1, see the rule list).
  StyleFloat floatStyle = floatDisplay->mFloat;
  MOZ_ASSERT(StyleFloat::Left == floatStyle || StyleFloat::Right == floatStyle,
             "Invalid float type!");

  // Are we required to place at least part of the float because we're
  // at the top of the page (to avoid an infinite loop of pushing and
  // breaking).
  bool mustPlaceFloat = mReflowInput.mFlags.mIsTopOfPage && IsAdjacentWithTop();

  for (;;) {
    if (mReflowInput.AvailableHeight() != NS_UNCONSTRAINEDSIZE &&
        floatAvailableSpace.mRect.BSize(wm) <= 0 && !mustPlaceFloat) {
      // No space, nowhere to put anything.
      PushFloatPastBreak(aFloat);
      return false;
    }

    if (CanPlaceFloat(floatMarginISize, floatAvailableSpace)) {
      // We found an appropriate place.
      break;
    }

    // Nope. try to advance to the next band.
    mBCoord += floatAvailableSpace.mRect.BSize(wm);
    if (adjustedAvailableSpace.BSize(wm) != NS_UNCONSTRAINEDSIZE) {
      adjustedAvailableSpace.BSize(wm) -= floatAvailableSpace.mRect.BSize(wm);
    }
    floatAvailableSpace = GetFloatAvailableSpaceForPlacingFloat(mBCoord);
    mustPlaceFloat = false;
  }

  // If the float is continued, it will get the same absolute x value as its
  // prev-in-flow

  // We don't worry about the geometry of the prev in flow, let the continuation
  // place and size itself as required.

  // Assign inline and block dir coordinates to the float. We don't use
  // LineLeft() and LineRight() here, because we would only have to
  // convert the result back into this block's writing mode.
  LogicalPoint floatPos(wm);
  bool leftFloat = floatStyle == StyleFloat::Left;

  if (leftFloat == wm.IsBidiLTR()) {
    floatPos.I(wm) = floatAvailableSpace.mRect.IStart(wm);
  } else {
    floatPos.I(wm) = floatAvailableSpace.mRect.IEnd(wm) - floatMarginISize;
  }
  // CSS2 spec, 9.5.1 rule [4]: "A floating box's outer top may not
  // be higher than the top of its containing block."  (Since the
  // containing block is the content edge of the block box, this
  // means the margin edge of the float can't be higher than the
  // content edge of the block that contains it.)
  floatPos.B(wm) = std::max(mBCoord, ContentBStart());

  // Reflow the float after computing its vertical position so it knows
  // where to break.
  if (!earlyFloatReflow) {
    bool pushedDown = mBCoord != restoreBCoord.SavedValue();
    mBlock->ReflowFloat(*this, adjustedAvailableSpace, aFloat, floatMargin,
                        floatOffsets, pushedDown, reflowStatus);
  }
  if (aFloat->GetPrevInFlow()) {
    floatMargin.BStart(wm) = 0;
  }
  if (reflowStatus.IsIncomplete()) {
    floatMargin.BEnd(wm) = 0;
  }

  // If none of the float fit, and it needs to be pushed in its entirety to the
  // next page, we need to bail.
  if (reflowStatus.IsTruncated() || reflowStatus.IsInlineBreakBefore()) {
    PushFloatPastBreak(aFloat);
    return false;
  }

  // We can't use aFloat->ShouldAvoidBreakInside(mReflowInput) here since
  // its mIsTopOfPage may be true even though the float isn't at the
  // top when floatPos.B(wm) > 0.
  if (ContentBSize() != NS_UNCONSTRAINEDSIZE && !mustPlaceFloat &&
      (!mReflowInput.mFlags.mIsTopOfPage || floatPos.B(wm) > 0) &&
      StyleBreakWithin::Avoid == aFloat->StyleDisplay()->mBreakInside &&
      (!reflowStatus.IsFullyComplete() ||
       aFloat->BSize(wm) + floatMargin.BStartEnd(wm) >
           ContentBEnd() - floatPos.B(wm)) &&
      !aFloat->GetPrevInFlow()) {
    PushFloatPastBreak(aFloat);
    return false;
  }

  // Calculate the actual origin of the float frame's border rect
  // relative to the parent block; the margin must be added in
  // to get the border rect
  LogicalPoint origin(wm, floatMargin.IStart(wm) + floatPos.I(wm),
                      floatMargin.BStart(wm) + floatPos.B(wm));

  // If float is relatively positioned, factor that in as well
  ReflowInput::ApplyRelativePositioning(aFloat, wm, floatOffsets, &origin,
                                        ContainerSize());

  // Position the float and make sure and views are properly
  // positioned. We need to explicitly position its child views as
  // well, since we're moving the float after flowing it.
  bool moved = aFloat->GetLogicalPosition(wm, ContainerSize()) != origin;
  if (moved) {
    aFloat->SetPosition(wm, origin, ContainerSize());
    nsContainerFrame::PositionFrameView(aFloat);
    nsContainerFrame::PositionChildViews(aFloat);
  }

  // Update the float combined area state
  // XXX Floats should really just get invalidated here if necessary
  mFloatOverflowAreas.UnionWith(aFloat->GetOverflowAreasRelativeToParent());

  // Place the float in the float manager
  // calculate region
  LogicalRect region = nsFloatManager::CalculateRegionFor(
      wm, aFloat, floatMargin, ContainerSize());
  // if the float split, then take up all of the vertical height
  if (reflowStatus.IsIncomplete() && (NS_UNCONSTRAINEDSIZE != ContentBSize())) {
    region.BSize(wm) =
        std::max(region.BSize(wm), ContentBSize() - floatPos.B(wm));
  }
  FloatManager()->AddFloat(aFloat, region, wm, ContainerSize());

  // store region
  nsFloatManager::StoreRegionFor(wm, aFloat, region, ContainerSize());

  const bool invalidationDataNeeded =
      ShapeInvalidationData::IsNeeded(*floatDisplay);

  // If the float's dimensions or shape have changed, note the damage in the
  // float manager.
  if (!region.IsEqualEdges(oldRegion) ||
      !!invalidationData != invalidationDataNeeded ||
      (invalidationData && !invalidationData->Matches(*floatDisplay))) {
    // XXXwaterson conservative: we could probably get away with noting
    // less damage; e.g., if only height has changed, then only note the
    // area into which the float has grown or from which the float has
    // shrunk.
    nscoord blockStart = std::min(region.BStart(wm), oldRegion.BStart(wm));
    nscoord blockEnd = std::max(region.BEnd(wm), oldRegion.BEnd(wm));
    FloatManager()->IncludeInDamage(blockStart, blockEnd);
  }

  if (invalidationDataNeeded) {
    if (invalidationData) {
      invalidationData->Update(*floatDisplay);
    } else {
      aFloat->SetProperty(ShapeInvalidationDataProperty(),
                          new ShapeInvalidationData(*floatDisplay));
    }
  } else if (invalidationData) {
    invalidationData = nullptr;
    aFloat->RemoveProperty(ShapeInvalidationDataProperty());
  }

  if (!reflowStatus.IsFullyComplete()) {
    mBlock->SplitFloat(*this, aFloat, reflowStatus);
  } else {
    MOZ_ASSERT(!aFloat->GetNextInFlow());
  }

#ifdef DEBUG
  if (nsBlockFrame::gNoisyFloatManager) {
    nscoord tI, tB;
    FloatManager()->GetTranslation(tI, tB);
    mBlock->ListTag(stdout);
    printf(": FlowAndPlaceFloat: AddFloat: tIB=%d,%d (%d,%d) {%d,%d,%d,%d}\n",
           tI, tB, mFloatManagerI, mFloatManagerB, region.IStart(wm),
           region.BStart(wm), region.ISize(wm), region.BSize(wm));
  }

  if (nsBlockFrame::gNoisyReflow) {
    nsRect r = aFloat->GetRect();
    nsIFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("placed float: ");
    aFloat->ListTag(stdout);
    printf(" %d,%d,%d,%d\n", r.x, r.y, r.width, r.height);
  }
#endif

  return true;
}

void BlockReflowInput::PushFloatPastBreak(nsIFrame* aFloat) {
  // This ensures that we:
  //  * don't try to place later but smaller floats (which CSS says
  //    must have their tops below the top of this float)
  //  * don't waste much time trying to reflow this float again until
  //    after the break
  StyleFloat floatStyle = aFloat->StyleDisplay()->mFloat;
  if (floatStyle == StyleFloat::Left) {
    FloatManager()->SetPushedLeftFloatPastBreak();
  } else {
    MOZ_ASSERT(floatStyle == StyleFloat::Right, "Unexpected float value!");
    FloatManager()->SetPushedRightFloatPastBreak();
  }

  // Put the float on the pushed floats list, even though it
  // isn't actually a continuation.
  mBlock->StealFrame(aFloat);
  AppendPushedFloatChain(aFloat);
  mReflowStatus.SetOverflowIncomplete();
}

/**
 * Place below-current-line floats.
 */
void BlockReflowInput::PlaceBelowCurrentLineFloats(nsLineBox* aLine) {
  MOZ_ASSERT(mBelowCurrentLineFloats.NotEmpty());
  nsFloatCache* fc = mBelowCurrentLineFloats.Head();
  while (fc) {
#ifdef DEBUG
    if (nsBlockFrame::gNoisyReflow) {
      nsIFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
      printf("placing bcl float: ");
      fc->mFloat->ListTag(stdout);
      printf("\n");
    }
#endif
    // Place the float
    bool placed = FlowAndPlaceFloat(fc->mFloat);
    nsFloatCache* next = fc->Next();
    if (!placed) {
      mBelowCurrentLineFloats.Remove(fc);
      delete fc;
      aLine->SetHadFloatPushed();
    }
    fc = next;
  }
  aLine->AppendFloats(mBelowCurrentLineFloats);
}

std::tuple<nscoord, BlockReflowInput::ClearFloatsResult>
BlockReflowInput::ClearFloats(nscoord aBCoord, StyleClear aBreakType,
                              nsIFrame* aReplacedBlock) {
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsIFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("clear floats: in: aBCoord=%d\n", aBCoord);
  }
#endif

#ifdef NOISY_FLOAT_CLEARING
  printf("BlockReflowInput::ClearFloats: aBCoord=%d breakType=%s\n", aBCoord,
         nsLineBox::BreakTypeToString(aBreakType));
  FloatManager()->List(stdout);
#endif

  if (!FloatManager()->HasAnyFloats()) {
    return {aBCoord, ClearFloatsResult::BCoordNoChange};
  }

  nscoord newBCoord = aBCoord;

  if (aBreakType != StyleClear::None) {
    newBCoord = FloatManager()->ClearFloats(newBCoord, aBreakType);

    if (FloatManager()->ClearContinues(aBreakType)) {
      return {newBCoord, ClearFloatsResult::FloatsPushedOrSplit};
    }
  }

  if (aReplacedBlock) {
    for (;;) {
      nsFlowAreaRect floatAvailableSpace = GetFloatAvailableSpace(newBCoord);
      if (ReplacedBlockFitsInAvailSpace(aReplacedBlock, floatAvailableSpace)) {
        break;
      }
      // See the analogous code for inlines in
      // nsBlockFrame::DoReflowInlineFrames
      if (!AdvanceToNextBand(floatAvailableSpace.mRect, &newBCoord)) {
        // Stop trying to clear here; we'll just get pushed to the
        // next column or page and try again there.
        break;
      }
    }
  }

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsIFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("clear floats: out: y=%d\n", newBCoord);
  }
#endif

  ClearFloatsResult result = newBCoord == aBCoord
                                 ? ClearFloatsResult::BCoordNoChange
                                 : ClearFloatsResult::BCoordAdvanced;
  return {newBCoord, result};
}
