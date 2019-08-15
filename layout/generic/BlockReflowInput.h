/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* state used in reflow of block frames */

#ifndef BlockReflowInput_h
#define BlockReflowInput_h

#include "nsFloatManager.h"
#include "nsLineBox.h"
#include "mozilla/ReflowInput.h"

class nsBlockFrame;
class nsFrameList;
class nsOverflowContinuationTracker;

namespace mozilla {

// BlockReflowInput contains additional reflow input information that the
// block frame uses along with ReflowInput. Like ReflowInput, this
// is read-only data that is passed down from a parent frame to its children.
class BlockReflowInput {
  using BandInfoType = nsFloatManager::BandInfoType;
  using ShapeType = nsFloatManager::ShapeType;

  // Block reflow input flags.
  struct Flags {
    Flags()
        : mHasUnconstrainedBSize(false),
          mIsBStartMarginRoot(false),
          mIsBEndMarginRoot(false),
          mShouldApplyBStartMargin(false),
          mIsFirstInflow(false),
          mHasLineAdjacentToTop(false),
          mBlockNeedsFloatManager(false),
          mIsLineLayoutEmpty(false),
          mIsOverflowContainer(false),
          mIsFloatListInBlockPropertyTable(false),
          mCanHaveOverflowMarkers(false) {}

    // Set in the BlockReflowInput constructor when the frame being reflowed has
    // been given NS_UNCONSTRAINEDSIZE as its available BSize in the
    // ReflowInput. If set, NS_UNCONSTRAINEDSIZE is passed to nsLineLayout as
    // the available BSize.
    bool mHasUnconstrainedBSize : 1;

    // Set in the BlockReflowInput constructor when reflowing a "block margin
    // root" frame (i.e. a frame with the NS_BLOCK_MARGIN_ROOT flag set, for
    // which margins apply by default).
    //
    // The flag is also set when reflowing a frame whose computed BStart border
    // padding is non-zero.
    bool mIsBStartMarginRoot : 1;

    // Set in the BlockReflowInput constructor when reflowing a "block margin
    // root" frame (i.e. a frame with the NS_BLOCK_MARGIN_ROOT flag set, for
    // which margins apply by default).
    //
    // The flag is also set when reflowing a frame whose computed BEnd border
    // padding is non-zero.
    bool mIsBEndMarginRoot : 1;

    // Set if the BStart margin should be considered when placing a linebox that
    // contains a block frame. It may be set as a side-effect of calling
    // nsBlockFrame::ShouldApplyBStartMargin(); once set,
    // ShouldApplyBStartMargin() uses it as a fast-path way to return whether
    // the BStart margin should apply.
    //
    // If the flag hasn't been set in the block reflow input, then
    // ShouldApplyBStartMargin() will crawl the line list to see if a block
    // frame precedes the specified frame. If so, the BStart margin should be
    // applied, and the flag is set to cache the result. (If not, the BStart
    // margin will be applied as a result of the generational margin collapsing
    // logic in nsBlockReflowContext::ComputeCollapsedBStartMargin(). In this
    // case, the flag won't be set, so subsequent calls to
    // ShouldApplyBStartMargin() will continue crawl the line list.)
    //
    // This flag is also set in the BlockReflowInput constructor if
    // mIsBStartMarginRoot is set; that is, the frame being reflowed is a margin
    // root by default.
    bool mShouldApplyBStartMargin : 1;

    bool mIsFirstInflow : 1;

    // Set when mLineAdjacentToTop is valid.
    bool mHasLineAdjacentToTop : 1;

    // Set when the block has the equivalent of NS_BLOCK_FLOAT_MGR.
    bool mBlockNeedsFloatManager : 1;

    // Set when nsLineLayout::LineIsEmpty was true at the end of reflowing
    // the current line.
    bool mIsLineLayoutEmpty : 1;

    bool mIsOverflowContainer : 1;

    // Set when our mPushedFloats list is stored on the block's property table.
    bool mIsFloatListInBlockPropertyTable : 1;

    // Set when we need text-overflow or -webkit-line-clamp processing.
    bool mCanHaveOverflowMarkers : 1;
  };

 public:
  BlockReflowInput(const ReflowInput& aReflowInput, nsPresContext* aPresContext,
                   nsBlockFrame* aFrame, bool aBStartMarginRoot,
                   bool aBEndMarginRoot, bool aBlockNeedsFloatManager,
                   nscoord aConsumedBSize = NS_UNCONSTRAINEDSIZE);

  /**
   * Get the available reflow space (the area not occupied by floats)
   * for the current y coordinate. The available space is relative to
   * our coordinate system, which is the content box, with (0, 0) in the
   * upper left.
   *
   * Returns whether there are floats present at the given block-direction
   * coordinate and within the inline size of the content rect.
   */
  nsFlowAreaRect GetFloatAvailableSpace() const {
    return GetFloatAvailableSpace(mBCoord);
  }
  nsFlowAreaRect GetFloatAvailableSpaceForPlacingFloat(nscoord aBCoord) const {
    return GetFloatAvailableSpaceWithState(aBCoord, ShapeType::Margin, nullptr);
  }
  nsFlowAreaRect GetFloatAvailableSpace(nscoord aBCoord) const {
    return GetFloatAvailableSpaceWithState(aBCoord, ShapeType::ShapeOutside,
                                           nullptr);
  }
  nsFlowAreaRect GetFloatAvailableSpaceWithState(
      nscoord aBCoord, ShapeType aShapeType,
      nsFloatManager::SavedState* aState) const;
  nsFlowAreaRect GetFloatAvailableSpaceForBSize(
      nscoord aBCoord, nscoord aBSize,
      nsFloatManager::SavedState* aState) const;

  /*
   * The following functions all return true if they were able to
   * place the float, false if the float did not fit in available
   * space.
   * aLineLayout is null when we are reflowing pushed floats (because
   * they are not associated with a line box).
   */
  bool AddFloat(nsLineLayout* aLineLayout, nsIFrame* aFloat,
                nscoord aAvailableISize);

  bool FlowAndPlaceFloat(nsIFrame* aFloat);

  void PlaceBelowCurrentLineFloats(nsLineBox* aLine);

  // Returns the first coordinate >= aBCoord that clears the
  // floats indicated by aBreakType and has enough inline size between floats
  // (or no floats remaining) to accomodate aReplacedBlock.
  nscoord ClearFloats(nscoord aBCoord, mozilla::StyleClear aBreakType,
                      nsIFrame* aReplacedBlock = nullptr, uint32_t aFlags = 0);

  nsFloatManager* FloatManager() const {
    MOZ_ASSERT(mReflowInput.mFloatManager,
               "Float manager should be valid during the lifetime of "
               "BlockReflowInput!");
    return mReflowInput.mFloatManager;
  }

  // Advances to the next band, i.e., the next horizontal stripe in
  // which there is a different set of floats.
  // Return false if it did not advance, which only happens for
  // constrained heights (and means that we should get pushed to the
  // next column/page).
  bool AdvanceToNextBand(const mozilla::LogicalRect& aFloatAvailableSpace,
                         nscoord* aBCoord) const {
    mozilla::WritingMode wm = mReflowInput.GetWritingMode();
    if (aFloatAvailableSpace.BSize(wm) > 0) {
      // See if there's room in the next band.
      *aBCoord += aFloatAvailableSpace.BSize(wm);
    } else {
      if (mReflowInput.AvailableHeight() != NS_UNCONSTRAINEDSIZE) {
        // Stop trying to clear here; we'll just get pushed to the
        // next column or page and try again there.
        return false;
      }
      MOZ_ASSERT_UNREACHABLE("avail space rect with zero height!");
      *aBCoord += 1;
    }
    return true;
  }

  bool ReplacedBlockFitsInAvailSpace(
      nsIFrame* aReplacedBlock,
      const nsFlowAreaRect& aFloatAvailableSpace) const;

  bool IsAdjacentWithTop() const {
    return mBCoord == mBorderPadding.BStart(mReflowInput.GetWritingMode());
  }

  /**
   * Return mBlock's computed physical border+padding with GetSkipSides applied.
   */
  const mozilla::LogicalMargin& BorderPadding() const { return mBorderPadding; }

  // Reconstruct the previous block-end margin that goes before |aLine|.
  void ReconstructMarginBefore(nsLineList::iterator aLine);

  // Caller must have called GetFloatAvailableSpace for the correct position
  // (which need not be the current mBCoord).
  void ComputeReplacedBlockOffsetsForFloats(
      nsIFrame* aFrame, const mozilla::LogicalRect& aFloatAvailableSpace,
      nscoord& aIStartResult, nscoord& aIEndResult) const;

  // Caller must have called GetFloatAvailableSpace for the current mBCoord
  void ComputeBlockAvailSpace(nsIFrame* aFrame,
                              const nsFlowAreaRect& aFloatAvailableSpace,
                              bool aBlockAvoidsFloats,
                              mozilla::LogicalRect& aResult);

  void RecoverStateFrom(nsLineList::iterator aLine, nscoord aDeltaBCoord);

  void AdvanceToNextLine() {
    if (mFlags.mIsLineLayoutEmpty) {
      mFlags.mIsLineLayoutEmpty = false;
    } else {
      mLineNumber++;
    }
  }

  //----------------------------------------

  // This state is the "global" state computed once for the reflow of
  // the block.

  // The block frame that is using this object
  nsBlockFrame* mBlock;

  nsPresContext* mPresContext;

  const ReflowInput& mReflowInput;

  // The coordinates within the float manager where the block is being
  // placed <b>after</b> taking into account the blocks border and
  // padding. This, therefore, represents the inner "content area" (in
  // float manager coordinates) where child frames will be placed,
  // including child blocks and floats.
  nscoord mFloatManagerI, mFloatManagerB;

  // XXX get rid of this
  nsReflowStatus mReflowStatus;

  // The float manager state as it was before the contents of this
  // block.  This is needed for positioning bullets, since we only want
  // to move the bullet to flow around floats that were before this
  // block, not floats inside of it.
  nsFloatManager::SavedState mFloatManagerStateBefore;

  nscoord mBEndEdge;

  // The content area to reflow child frames within.  This is within
  // this frame's coordinate system and writing mode, which means
  // mContentArea.IStart == BorderPadding().IStart and
  // mContentArea.BStart == BorderPadding().BStart.
  // The block size may be NS_UNCONSTRAINEDSIZE, which indicates that there
  // is no page/column boundary below (the common case).
  // mContentArea.BEnd() should only be called after checking that
  // mContentArea.BSize is not NS_UNCONSTRAINEDSIZE; otherwise
  // coordinate overflow may occur.
  mozilla::LogicalRect mContentArea;
  nscoord ContentIStart() const {
    return mContentArea.IStart(mReflowInput.GetWritingMode());
  }
  nscoord ContentISize() const {
    return mContentArea.ISize(mReflowInput.GetWritingMode());
  }
  nscoord ContentIEnd() const {
    return mContentArea.IEnd(mReflowInput.GetWritingMode());
  }
  nscoord ContentBStart() const {
    return mContentArea.BStart(mReflowInput.GetWritingMode());
  }
  nscoord ContentBSize() const {
    return mContentArea.BSize(mReflowInput.GetWritingMode());
  }
  nscoord ContentBEnd() const {
    return mContentArea.BEnd(mReflowInput.GetWritingMode());
  }
  mozilla::LogicalSize ContentSize(mozilla::WritingMode aWM) const {
    mozilla::WritingMode wm = mReflowInput.GetWritingMode();
    return mContentArea.Size(wm).ConvertTo(aWM, wm);
  }

  // Physical size. Use only for physical <-> logical coordinate conversion.
  nsSize mContainerSize;
  const nsSize& ContainerSize() const { return mContainerSize; }

  // Continuation out-of-flow float frames that need to move to our
  // next in flow are placed here during reflow.  It's a pointer to
  // a frame list stored in the block's property table.
  nsFrameList* mPushedFloats;
  // This method makes sure pushed floats are accessible to
  // StealFrame. Call it before adding any frames to mPushedFloats.
  void SetupPushedFloatList();
  /**
   * Append aFloatCont and its next-in-flows within the same block to
   * mPushedFloats.  aFloatCont should not be on any child list when
   * making this call.  Its next-in-flows will be removed from
   * mBlock using StealFrame() before being added to mPushedFloats.
   * All appended frames will be marked NS_FRAME_IS_PUSHED_FLOAT.
   */
  void AppendPushedFloatChain(nsIFrame* aFloatCont);

  // Track child overflow continuations.
  nsOverflowContinuationTracker* mOverflowTracker;

  //----------------------------------------

  // This state is "running" state updated by the reflow of each line
  // in the block. This same state is "recovered" when a line is not
  // dirty and is passed over during incremental reflow.

  // The current line being reflowed
  // If it is mBlock->end_lines(), then it is invalid.
  nsLineList::iterator mCurrentLine;

  // When mHasLineAdjacentToTop is set, this refers to a line
  // which we know is adjacent to the top of the block (in other words,
  // all lines before it are empty and do not have clearance. This line is
  // always before the current line.
  nsLineList::iterator mLineAdjacentToTop;

  // The current block-direction coordinate in the block
  nscoord mBCoord;

  // mBlock's computed physical border+padding with GetSkipSides applied.
  mozilla::LogicalMargin mBorderPadding;

  // The overflow areas of all floats placed so far
  nsOverflowAreas mFloatOverflowAreas;

  nsFloatCacheFreeList mFloatCacheFreeList;

  // Previous child. This is used when pulling up a frame to update
  // the sibling list.
  nsIFrame* mPrevChild;

  // The previous child frames collapsed bottom margin value.
  nsCollapsingMargin mPrevBEndMargin;

  // The current next-in-flow for the block. When lines are pulled
  // from a next-in-flow, this is used to know which next-in-flow to
  // pull from. When a next-in-flow is emptied of lines, we advance
  // this to the next next-in-flow.
  nsBlockFrame* mNextInFlow;

  //----------------------------------------

  // Temporary state, for line-reflow. This state is used during the reflow
  // of a given line, but doesn't have meaning before or after.

  // The list of floats that are "current-line" floats. These are
  // added to the line after the line has been reflowed, to keep the
  // list fiddling from being N^2.
  nsFloatCacheFreeList mCurrentLineFloats;

  // The list of floats which are "below current-line"
  // floats. These are reflowed/placed after the line is reflowed
  // and placed. Again, this is done to keep the list fiddling from
  // being N^2.
  nsFloatCacheFreeList mBelowCurrentLineFloats;

  // The list of floats that are waiting on a break opportunity in order to be
  // placed, since we're on a nowrap context.
  nsTArray<nsIFrame*> mNoWrapFloats;

  nscoord mMinLineHeight;

  int32_t mLineNumber;

  Flags mFlags;

  StyleClear mFloatBreakType;

  // The amount of computed content block-size "consumed" by our previous
  // continuations.
  const nscoord mConsumedBSize;

  // Cache the current line's BSize if nsBlockFrame::PlaceLine() fails to
  // place the line. When redoing the line, it will be used to query the
  // accurate float available space in AddFloat() and
  // nsBlockFrame::PlaceLine().
  mozilla::Maybe<nscoord> mLineBSize;

 private:
  bool CanPlaceFloat(nscoord aFloatISize,
                     const nsFlowAreaRect& aFloatAvailableSpace);

  void PushFloatPastBreak(nsIFrame* aFloat);

  void RecoverFloats(nsLineList::iterator aLine, nscoord aDeltaBCoord);
};

};  // namespace mozilla

#endif  // BlockReflowInput_h
