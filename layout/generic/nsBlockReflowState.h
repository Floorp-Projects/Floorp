/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* state used in reflow of block frames */

#ifndef nsBlockReflowState_h__
#define nsBlockReflowState_h__

#include "nsFloatManager.h"
#include "nsLineBox.h"
#include "nsHTMLReflowState.h"

class nsBlockFrame;
class nsFrameList;
class nsOverflowContinuationTracker;

  // block reflow state flags
#define BRS_UNCONSTRAINEDHEIGHT   0x00000001
#define BRS_ISTOPMARGINROOT       0x00000002  // Is this frame a root for top/bottom margin collapsing?
#define BRS_ISBOTTOMMARGINROOT    0x00000004
#define BRS_APPLYTOPMARGIN        0x00000008  // See ShouldApplyTopMargin
#define BRS_ISFIRSTINFLOW         0x00000010
// Set when mLineAdjacentToTop is valid
#define BRS_HAVELINEADJACENTTOTOP 0x00000020
// Set when the block has the equivalent of NS_BLOCK_FLOAT_MGR
#define BRS_FLOAT_MGR             0x00000040
// Set when nsLineLayout::LineIsEmpty was true at the end of reflowing
// the current line
#define BRS_LINE_LAYOUT_EMPTY     0x00000080
#define BRS_ISOVERFLOWCONTAINER   0x00000100
// Our mPushedFloats list is stored on the blocks' proptable
#define BRS_PROPTABLE_FLOATCLIST  0x00000200
#define BRS_LASTFLAG              BRS_PROPTABLE_FLOATCLIST

class nsBlockReflowState {
public:
  nsBlockReflowState(const nsHTMLReflowState& aReflowState,
                     nsPresContext* aPresContext,
                     nsBlockFrame* aFrame,
                     bool aTopMarginRoot, bool aBottomMarginRoot,
                     bool aBlockNeedsFloatManager,
                     nscoord aConsumedHeight = NS_INTRINSICSIZE);

  /**
   * Get the available reflow space (the area not occupied by floats)
   * for the current y coordinate. The available space is relative to
   * our coordinate system, which is the content box, with (0, 0) in the
   * upper left.
   *
   * Returns whether there are floats present at the given vertical
   * coordinate and within the width of the content rect.
   */
  nsFlowAreaRect GetFloatAvailableSpace() const
    { return GetFloatAvailableSpace(mY); }
  nsFlowAreaRect GetFloatAvailableSpace(nscoord aY) const
    { return GetFloatAvailableSpaceWithState(aY, nullptr); }
  nsFlowAreaRect
    GetFloatAvailableSpaceWithState(nscoord aY,
                                    nsFloatManager::SavedState *aState) const;
  nsFlowAreaRect
    GetFloatAvailableSpaceForHeight(nscoord aY, nscoord aHeight,
                                    nsFloatManager::SavedState *aState) const;

  /*
   * The following functions all return true if they were able to
   * place the float, false if the float did not fit in available
   * space.
   * aLineLayout is null when we are reflowing pushed floats (because
   * they are not associated with a line box).
   */
  bool AddFloat(nsLineLayout*       aLineLayout,
                  nsIFrame*           aFloat,
                  nscoord             aAvailableWidth);
private:
  bool CanPlaceFloat(nscoord aFloatWidth,
                       const nsFlowAreaRect& aFloatAvailableSpace);
public:
  bool FlowAndPlaceFloat(nsIFrame* aFloat);
private:
  void PushFloatPastBreak(nsIFrame* aFloat);
public:
  void PlaceBelowCurrentLineFloats(nsFloatCacheFreeList& aFloats,
                                   nsLineBox* aLine);

  // Returns the first coordinate >= aY that clears the
  // floats indicated by aBreakType and has enough width between floats
  // (or no floats remaining) to accomodate aReplacedBlock.
  nscoord ClearFloats(nscoord aY, uint8_t aBreakType,
                      nsIFrame *aReplacedBlock = nullptr,
                      uint32_t aFlags = 0);

  bool IsAdjacentWithTop() const {
    return mY == mBorderPadding.top;
  }

  /**
   * Return mBlock's computed physical border+padding with GetSkipSides applied.
   */
  const nsMargin& BorderPadding() const {
    return mBorderPadding;
  }

  /**
   * Retrieve the height "consumed" by any previous-in-flows.
   */
  nscoord GetConsumedHeight();

  // Reconstruct the previous bottom margin that goes above |aLine|.
  void ReconstructMarginAbove(nsLineList::iterator aLine);

  // Caller must have called GetAvailableSpace for the correct position
  // (which need not be the current mY).
  void ComputeReplacedBlockOffsetsForFloats(nsIFrame* aFrame,
                                            const nsRect& aFloatAvailableSpace,
                                            nscoord& aLeftResult,
                                            nscoord& aRightResult);

  // Caller must have called GetAvailableSpace for the current mY
  void ComputeBlockAvailSpace(nsIFrame* aFrame,
                              const nsStyleDisplay* aDisplay,
                              const nsFlowAreaRect& aFloatAvailableSpace,
                              bool aBlockAvoidsFloats,
                              nsRect& aResult);

protected:
  void RecoverFloats(nsLineList::iterator aLine, nscoord aDeltaY);

public:
  void RecoverStateFrom(nsLineList::iterator aLine, nscoord aDeltaY);

  void AdvanceToNextLine() {
    if (GetFlag(BRS_LINE_LAYOUT_EMPTY)) {
      SetFlag(BRS_LINE_LAYOUT_EMPTY, false);
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

  const nsHTMLReflowState& mReflowState;

  nsFloatManager* mFloatManager;

  // The coordinates within the float manager where the block is being
  // placed <b>after</b> taking into account the blocks border and
  // padding. This, therefore, represents the inner "content area" (in
  // spacemanager coordinates) where child frames will be placed,
  // including child blocks and floats.
  nscoord mFloatManagerX, mFloatManagerY;

  // XXX get rid of this
  nsReflowStatus mReflowStatus;

  // The float manager state as it was before the contents of this
  // block.  This is needed for positioning bullets, since we only want
  // to move the bullet to flow around floats that were before this
  // block, not floats inside of it.
  nsFloatManager::SavedState mFloatManagerStateBefore;

  nscoord mBottomEdge;

  // The content area to reflow child frames within.  This is within
  // this frame's coordinate system, which means mContentArea.x ==
  // BorderPadding().left and mContentArea.y == BorderPadding().top.
  // The height may be NS_UNCONSTRAINEDSIZE, which indicates that there
  // is no page/column boundary below (the common case).
  // mContentArea.YMost() should only be called after checking that
  // mContentArea.height is not NS_UNCONSTRAINEDSIZE; otherwise
  // coordinate overflow may occur.
  nsRect mContentArea;
  nscoord mContainerWidth;

  // Continuation out-of-flow float frames that need to move to our
  // next in flow are placed here during reflow.  It's a pointer to
  // a frame list stored in the block's property table.
  nsFrameList *mPushedFloats;
  // This method makes sure pushed floats are accessible to
  // StealFrame. Call it before adding any frames to mPushedFloats.
  void SetupPushedFloatList();
  // Use this method to append to mPushedFloats.
  void AppendPushedFloat(nsIFrame* aFloatCont);

  // Track child overflow continuations.
  nsOverflowContinuationTracker* mOverflowTracker;

  //----------------------------------------

  // This state is "running" state updated by the reflow of each line
  // in the block. This same state is "recovered" when a line is not
  // dirty and is passed over during incremental reflow.

  // The current line being reflowed
  // If it is mBlock->end_lines(), then it is invalid.
  nsLineList::iterator mCurrentLine;

  // When BRS_HAVELINEADJACENTTOTOP is set, this refers to a line
  // which we know is adjacent to the top of the block (in other words,
  // all lines before it are empty and do not have clearance. This line is
  // always before the current line.
  nsLineList::iterator mLineAdjacentToTop;

  // The current Y coordinate in the block
  nscoord mY;

  // mBlock's computed physical border+padding with GetSkipSides applied.
  nsMargin mBorderPadding;

  // The overflow areas of all floats placed so far
  nsOverflowAreas mFloatOverflowAreas;

  nsFloatCacheFreeList mFloatCacheFreeList;

  // Previous child. This is used when pulling up a frame to update
  // the sibling list.
  nsIFrame* mPrevChild;

  // The previous child frames collapsed bottom margin value.
  nsCollapsingMargin mPrevBottomMargin;

  // The current next-in-flow for the block. When lines are pulled
  // from a next-in-flow, this is used to know which next-in-flow to
  // pull from. When a next-in-flow is emptied of lines, we advance
  // this to the next next-in-flow.
  nsBlockFrame* mNextInFlow;

  //----------------------------------------

  // Temporary line-reflow state. This state is used during the reflow
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

  nscoord mMinLineHeight;

  int32_t mLineNumber;

  int16_t mFlags;
 
  uint8_t mFloatBreakType;

  // The amount of computed height "consumed" by previous-in-flows.
  nscoord mConsumedHeight;

  void SetFlag(uint32_t aFlag, bool aValue)
  {
    NS_ASSERTION(aFlag<=BRS_LASTFLAG, "bad flag");
    if (aValue) { // set flag
      mFlags |= aFlag;
    }
    else {        // unset flag
      mFlags &= ~aFlag;
    }
  }

  bool GetFlag(uint32_t aFlag) const
  {
    NS_ASSERTION(aFlag<=BRS_LASTFLAG, "bad flag");
    return !!(mFlags & aFlag);
  }
};

#endif // nsBlockReflowState_h__
