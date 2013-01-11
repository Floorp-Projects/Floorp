/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* state used in reflow of block frames */

#include "mozilla/DebugOnly.h"

#include "nsBlockReflowContext.h"
#include "nsBlockReflowState.h"
#include "nsBlockFrame.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include "nsFrameManager.h"
#include "mozilla/AutoRestore.h"
#include "FrameLayerBuilder.h"

#include "nsINameSpaceManager.h"

#ifdef DEBUG
#include "nsBlockDebugFlags.h"
#endif

using namespace mozilla;
using namespace mozilla::layout;

nsBlockReflowState::nsBlockReflowState(const nsHTMLReflowState& aReflowState,
                                       nsPresContext* aPresContext,
                                       nsBlockFrame* aFrame,
                                       const nsHTMLReflowMetrics& aMetrics,
                                       bool aTopMarginRoot,
                                       bool aBottomMarginRoot,
                                       bool aBlockNeedsFloatManager)
  : mBlock(aFrame),
    mPresContext(aPresContext),
    mReflowState(aReflowState),
    mPushedFloats(nullptr),
    mOverflowTracker(nullptr),
    mPrevBottomMargin(),
    mLineNumber(0),
    mFlags(0),
    mFloatBreakType(NS_STYLE_CLEAR_NONE)
{
  SetFlag(BRS_ISFIRSTINFLOW, aFrame->GetPrevInFlow() == nullptr);
  SetFlag(BRS_ISOVERFLOWCONTAINER,
          IS_TRUE_OVERFLOW_CONTAINER(aFrame));

  const nsMargin& borderPadding = BorderPadding();

  if (aTopMarginRoot || 0 != aReflowState.mComputedBorderPadding.top) {
    SetFlag(BRS_ISTOPMARGINROOT, true);
  }
  if (aBottomMarginRoot || 0 != aReflowState.mComputedBorderPadding.bottom) {
    SetFlag(BRS_ISBOTTOMMARGINROOT, true);
  }
  if (GetFlag(BRS_ISTOPMARGINROOT)) {
    SetFlag(BRS_APPLYTOPMARGIN, true);
  }
  if (aBlockNeedsFloatManager) {
    SetFlag(BRS_FLOAT_MGR, true);
  }
  
  mFloatManager = aReflowState.mFloatManager;

  NS_ASSERTION(mFloatManager,
               "FloatManager should be set in nsBlockReflowState" );
  if (mFloatManager) {
    // Save the coordinate system origin for later.
    mFloatManager->GetTranslation(mFloatManagerX, mFloatManagerY);
    mFloatManager->PushState(&mFloatManagerStateBefore); // never popped
  }

  mReflowStatus = NS_FRAME_COMPLETE;

  mPresContext = aPresContext;
  mNextInFlow = static_cast<nsBlockFrame*>(mBlock->GetNextInFlow());

  NS_WARN_IF_FALSE(NS_UNCONSTRAINEDSIZE != aReflowState.ComputedWidth(),
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
  mContentArea.width = aReflowState.ComputedWidth();

  // Compute content area height. Unlike the width, if we have a
  // specified style height we ignore it since extra content is
  // managed by the "overflow" property. When we don't have a
  // specified style height then we may end up limiting our height if
  // the availableHeight is constrained (this situation occurs when we
  // are paginated).
  if (NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight) {
    // We are in a paginated situation. The bottom edge is just inside
    // the bottom border and padding. The content area height doesn't
    // include either border or padding edge.
    mBottomEdge = aReflowState.availableHeight - borderPadding.bottom;
    mContentArea.height = NS_MAX(0, mBottomEdge - borderPadding.top);
  }
  else {
    // When we are not in a paginated situation then we always use
    // an constrained height.
    SetFlag(BRS_UNCONSTRAINEDHEIGHT, true);
    mContentArea.height = mBottomEdge = NS_UNCONSTRAINEDSIZE;
  }
  mContentArea.x = borderPadding.left;
  mY = mContentArea.y = borderPadding.top;

  mPrevChild = nullptr;
  mCurrentLine = aFrame->end_lines();

  mMinLineHeight = aReflowState.CalcLineHeight();
}

void
nsBlockReflowState::ComputeReplacedBlockOffsetsForFloats(nsIFrame* aFrame,
                                                         const nsRect& aFloatAvailableSpace,
                                                         nscoord& aLeftResult,
                                                         nscoord& aRightResult)
{
  // The frame is clueless about the float manager and therefore we
  // only give it free space. An example is a table frame - the
  // tables do not flow around floats.
  // However, we can let its margins intersect floats.
  NS_ASSERTION(aFloatAvailableSpace.x >= mContentArea.x, "bad avail space rect x");
  NS_ASSERTION(aFloatAvailableSpace.width == 0 ||
               aFloatAvailableSpace.XMost() <= mContentArea.XMost(),
               "bad avail space rect width");

  nscoord leftOffset, rightOffset;
  if (aFloatAvailableSpace.width == mContentArea.width) {
    // We don't need to compute margins when there are no floats around.
    leftOffset = 0;
    rightOffset = 0;
  } else {
    nsMargin frameMargin;
    nsCSSOffsetState os(aFrame, mReflowState.rendContext, mContentArea.width);
    frameMargin = os.mComputedMargin;

    nscoord leftFloatXOffset = aFloatAvailableSpace.x - mContentArea.x;
    leftOffset = NS_MAX(leftFloatXOffset, frameMargin.left) -
                 frameMargin.left;
    leftOffset = NS_MAX(leftOffset, 0); // in case of negative margin
    nscoord rightFloatXOffset =
      mContentArea.XMost() - aFloatAvailableSpace.XMost();
    rightOffset = NS_MAX(rightFloatXOffset, frameMargin.right) -
                  frameMargin.right;
    rightOffset = NS_MAX(rightOffset, 0); // in case of negative margin
  }
  aLeftResult = leftOffset;
  aRightResult = rightOffset;
}

// Compute the amount of available space for reflowing a block frame
// at the current Y coordinate. This method assumes that
// GetAvailableSpace has already been called.
void
nsBlockReflowState::ComputeBlockAvailSpace(nsIFrame* aFrame,
                                           const nsStyleDisplay* aDisplay,
                                           const nsFlowAreaRect& aFloatAvailableSpace,
                                           bool aBlockAvoidsFloats,
                                           nsRect& aResult)
{
#ifdef REALLY_NOISY_REFLOW
  printf("CBAS frame=%p has floats %d\n",
         aFrame, aFloatAvailableSpace.mHasFloats);
#endif
  aResult.y = mY;
  aResult.height = GetFlag(BRS_UNCONSTRAINEDHEIGHT)
    ? NS_UNCONSTRAINEDSIZE
    : mReflowState.availableHeight - mY;
  // mY might be greater than mBottomEdge if the block's top margin pushes
  // it off the page/column. Negative available height can confuse other code
  // and is nonsense in principle.

  // XXX Do we really want this condition to be this restrictive (i.e.,
  // more restrictive than it used to be)?  The |else| here is allowed
  // by the CSS spec, but only out of desperation given implementations,
  // and the behavior it leads to is quite undesirable (it can cause
  // things to become extremely narrow when they'd fit quite well a
  // little bit lower).  Should the else be a quirk or something that
  // applies to a specific set of frame classes and no new ones?
  // If we did that, then for those frames where the condition below is
  // true but nsBlockFrame::BlockCanIntersectFloats is false,
  // nsBlockFrame::WidthToClearPastFloats would need to use the
  // shrink-wrap formula, max(MIN_WIDTH, min(avail width, PREF_WIDTH))
  // rather than just using MIN_WIDTH.
  NS_ASSERTION(nsBlockFrame::BlockCanIntersectFloats(aFrame) == 
                 !aBlockAvoidsFloats,
               "unexpected replaced width");
  if (!aBlockAvoidsFloats) {
    if (aFloatAvailableSpace.mHasFloats) {
      // Use the float-edge property to determine how the child block
      // will interact with the float.
      const nsStyleBorder* borderStyle = aFrame->GetStyleBorder();
      switch (borderStyle->mFloatEdge) {
        default:
        case NS_STYLE_FLOAT_EDGE_CONTENT:  // content and only content does runaround of floats
          // The child block will flow around the float. Therefore
          // give it all of the available space.
          aResult.x = mContentArea.x;
          aResult.width = mContentArea.width;
          break;
        case NS_STYLE_FLOAT_EDGE_MARGIN:
          {
            // The child block's margins should be placed adjacent to,
            // but not overlap the float.
            aResult.x = aFloatAvailableSpace.mRect.x;
            aResult.width = aFloatAvailableSpace.mRect.width;
          }
          break;
      }
    }
    else {
      // Since there are no floats present the float-edge property
      // doesn't matter therefore give the block element all of the
      // available space since it will flow around the float itself.
      aResult.x = mContentArea.x;
      aResult.width = mContentArea.width;
    }
  }
  else {
    nscoord leftOffset, rightOffset;
    ComputeReplacedBlockOffsetsForFloats(aFrame, aFloatAvailableSpace.mRect,
                                         leftOffset, rightOffset);
    aResult.x = mContentArea.x + leftOffset;
    aResult.width = mContentArea.width - leftOffset - rightOffset;
  }

#ifdef REALLY_NOISY_REFLOW
  printf("  CBAS: result %d %d %d %d\n", aResult.x, aResult.y, aResult.width, aResult.height);
#endif
}

nsFlowAreaRect
nsBlockReflowState::GetFloatAvailableSpaceWithState(
                      nscoord aY,
                      nsFloatManager::SavedState *aState) const
{
#ifdef DEBUG
  // Verify that the caller setup the coordinate system properly
  nscoord wx, wy;
  mFloatManager->GetTranslation(wx, wy);
  NS_ASSERTION((wx == mFloatManagerX) && (wy == mFloatManagerY),
               "bad coord system");
#endif

  nscoord height = (mContentArea.height == nscoord_MAX)
                     ? nscoord_MAX : NS_MAX(mContentArea.YMost() - aY, 0);
  nsFlowAreaRect result =
    mFloatManager->GetFlowArea(aY, nsFloatManager::BAND_FROM_POINT,
                               height, mContentArea, aState);
  // Keep the width >= 0 for compatibility with nsSpaceManager.
  if (result.mRect.width < 0)
    result.mRect.width = 0;

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("GetAvailableSpace: band=%d,%d,%d,%d hasfloats=%d\n",
           result.mRect.x, result.mRect.y, result.mRect.width,
           result.mRect.height, result.mHasFloats);
  }
#endif
  return result;
}

nsFlowAreaRect
nsBlockReflowState::GetFloatAvailableSpaceForHeight(
                      nscoord aY, nscoord aHeight,
                      nsFloatManager::SavedState *aState) const
{
#ifdef DEBUG
  // Verify that the caller setup the coordinate system properly
  nscoord wx, wy;
  mFloatManager->GetTranslation(wx, wy);
  NS_ASSERTION((wx == mFloatManagerX) && (wy == mFloatManagerY),
               "bad coord system");
#endif

  nsFlowAreaRect result =
    mFloatManager->GetFlowArea(aY, nsFloatManager::WIDTH_WITHIN_HEIGHT,
                               aHeight, mContentArea, aState);
  // Keep the width >= 0 for compatibility with nsSpaceManager.
  if (result.mRect.width < 0)
    result.mRect.width = 0;

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("GetAvailableSpaceForHeight: space=%d,%d,%d,%d hasfloats=%d\n",
           result.mRect.x, result.mRect.y, result.mRect.width,
           result.mRect.height, result.mHasFloats);
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
 * the reflow state's |mPrevBottomMargin| when we reflowed that line in
 * a full reflow (under the rule in CSS2 that all adjacent vertical
 * margins of blocks collapse).
 */
void
nsBlockReflowState::ReconstructMarginAbove(nsLineList::iterator aLine)
{
  mPrevBottomMargin.Zero();
  nsBlockFrame *block = mBlock;

  nsLineList::iterator firstLine = block->begin_lines();
  for (;;) {
    --aLine;
    if (aLine->IsBlock()) {
      mPrevBottomMargin = aLine->GetCarriedOutBottomMargin();
      break;
    }
    if (!aLine->IsEmpty()) {
      break;
    }
    if (aLine == firstLine) {
      // If the top margin was carried out (and thus already applied),
      // set it to zero.  Either way, we're done.
      if (!GetFlag(BRS_ISTOPMARGINROOT)) {
        mPrevBottomMargin.Zero();
      }
      break;
    }
  }
}

void
nsBlockReflowState::SetupPushedFloatList()
{
  NS_ABORT_IF_FALSE(!GetFlag(BRS_PROPTABLE_FLOATCLIST) == !mPushedFloats,
                    "flag mismatch");
  if (!GetFlag(BRS_PROPTABLE_FLOATCLIST)) {
    // If we're being re-Reflow'd without our next-in-flow having been
    // reflowed, some pushed floats from our previous reflow might
    // still be on our pushed floats list.  However, that's
    // actually fine, since they'll all end up being stolen and
    // reordered into the correct order again.
    // (nsBlockFrame::ReflowDirtyLines ensures that any lines with
    // pushed floats are reflowed.)
    mPushedFloats = mBlock->EnsurePushedFloats();
    SetFlag(BRS_PROPTABLE_FLOATCLIST, true);
  }
}

/**
 * Restore information about floats into the float manager for an
 * incremental reflow, and simultaneously push the floats by
 * |aDeltaY|, which is the amount |aLine| was pushed relative to its
 * parent.  The recovery of state is one of the things that makes
 * incremental reflow O(N^2) and this state should really be kept
 * around, attached to the frame tree.
 */
void
nsBlockReflowState::RecoverFloats(nsLineList::iterator aLine,
                                  nscoord aDeltaY)
{
  if (aLine->HasFloats()) {
    // Place the floats into the space-manager again. Also slide
    // them, just like the regular frames on the line.
    nsFloatCache* fc = aLine->GetFirstFloat();
    while (fc) {
      nsIFrame* floatFrame = fc->mFloat;
      if (aDeltaY != 0) {
        nsPoint p = floatFrame->GetPosition();
        floatFrame->SetPosition(nsPoint(p.x, p.y + aDeltaY));
        nsContainerFrame::PositionFrameView(floatFrame);
        nsContainerFrame::PositionChildViews(floatFrame);
      }
#ifdef DEBUG
      if (nsBlockFrame::gNoisyReflow || nsBlockFrame::gNoisyFloatManager) {
        nscoord tx, ty;
        mFloatManager->GetTranslation(tx, ty);
        nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
        printf("RecoverFloats: txy=%d,%d (%d,%d) ",
               tx, ty, mFloatManagerX, mFloatManagerY);
        nsFrame::ListTag(stdout, floatFrame);
        nsRect region = nsFloatManager::GetRegionFor(floatFrame);
        printf(" aDeltaY=%d region={%d,%d,%d,%d}\n",
               aDeltaY, region.x, region.y, region.width, region.height);
      }
#endif
      mFloatManager->AddFloat(floatFrame,
                              nsFloatManager::GetRegionFor(floatFrame));
      fc = fc->Next();
    }
  } else if (aLine->IsBlock()) {
    nsBlockFrame::RecoverFloatsFor(aLine->mFirstChild, *mFloatManager);
  }
}

/**
 * Everything done in this function is done O(N) times for each pass of
 * reflow so it is O(N*M) where M is the number of incremental reflow
 * passes.  That's bad.  Don't do stuff here.
 *
 * When this function is called, |aLine| has just been slid by |aDeltaY|
 * and the purpose of RecoverStateFrom is to ensure that the
 * nsBlockReflowState is in the same state that it would have been in
 * had the line just been reflowed.
 *
 * Most of the state recovery that we have to do involves floats.
 */
void
nsBlockReflowState::RecoverStateFrom(nsLineList::iterator aLine,
                                     nscoord aDeltaY)
{
  // Make the line being recovered the current line
  mCurrentLine = aLine;

  // Place floats for this line into the float manager
  if (aLine->HasFloats() || aLine->IsBlock()) {
    RecoverFloats(aLine, aDeltaY);

#ifdef DEBUG
    if (nsBlockFrame::gNoisyReflow || nsBlockFrame::gNoisyFloatManager) {
      mFloatManager->List(stdout);
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
bool
nsBlockReflowState::AddFloat(nsLineLayout*       aLineLayout,
                             nsIFrame*           aFloat,
                             nscoord             aAvailableWidth)
{
  NS_PRECONDITION(aLineLayout, "must have line layout");
  NS_PRECONDITION(mBlock->end_lines() != mCurrentLine, "null ptr");
  NS_PRECONDITION(aFloat->GetStateBits() & NS_FRAME_OUT_OF_FLOW,
                  "aFloat must be an out-of-flow frame");

  NS_ABORT_IF_FALSE(aFloat->GetParent(), "float must have parent");
  NS_ABORT_IF_FALSE(aFloat->GetParent()->IsFrameOfType(nsIFrame::eBlockFrame),
                    "float's parent must be block");
  NS_ABORT_IF_FALSE(aFloat->GetParent() == mBlock ||
                    (aFloat->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT),
                    "float should be in this block unless it was marked as "
                    "pushed float");
  if (aFloat->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT) {
    // If, in a previous reflow, the float was pushed entirely to
    // another column/page, we need to steal it back.  (We might just
    // push it again, though.)  Likewise, if that previous reflow
    // reflowed this block but not its next continuation, we might need
    // to steal it from our own float-continuations list.
    nsBlockFrame *floatParent =
      static_cast<nsBlockFrame*>(aFloat->GetParent());
    floatParent->StealFrame(mPresContext, aFloat);

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
  nscoord ox, oy;
  mFloatManager->GetTranslation(ox, oy);
  nscoord dx = ox - mFloatManagerX;
  nscoord dy = oy - mFloatManagerY;
  mFloatManager->Translate(-dx, -dy);

  bool placed;

  // Now place the float immediately if possible. Otherwise stash it
  // away in mPendingFloats and place it later.
  // If one or more floats has already been pushed to the next line,
  // don't let this one go on the current line, since that would violate
  // float ordering.
  nsRect floatAvailableSpace = GetFloatAvailableSpace().mRect;
  if (mBelowCurrentLineFloats.IsEmpty() &&
      (aLineLayout->LineIsEmpty() ||
       mBlock->ComputeFloatWidth(*this, floatAvailableSpace, aFloat)
       <= aAvailableWidth)) {
    // And then place it
    placed = FlowAndPlaceFloat(aFloat);
    if (placed) {
      // Pass on updated available space to the current inline reflow engine
      nsFlowAreaRect floatAvailSpace = GetFloatAvailableSpace(mY);
      nsRect availSpace(nsPoint(floatAvailSpace.mRect.x, mY),
                        floatAvailSpace.mRect.Size());
      aLineLayout->UpdateBand(availSpace, aFloat);
      // Record this float in the current-line list
      mCurrentLineFloats.Append(mFloatCacheFreeList.Alloc(aFloat));
    } else {
      (*aLineLayout->GetLine())->SetHadFloatPushed();
    }
  }
  else {
    // Always claim to be placed; we don't know whether we fit yet, so we
    // deal with this in PlaceBelowCurrentLineFloats
    placed = true;
    // This float will be placed after the line is done (it is a
    // below-current-line float).
    mBelowCurrentLineFloats.Append(mFloatCacheFreeList.Alloc(aFloat));
  }

  // Restore coordinate system
  mFloatManager->Translate(dx, dy);

  return placed;
}

bool
nsBlockReflowState::CanPlaceFloat(nscoord aFloatWidth,
                                  const nsFlowAreaRect& aFloatAvailableSpace)
{
  // A float fits at a given vertical position if there are no floats at
  // its horizontal position (no matter what its width) or if its width
  // fits in the space remaining after prior floats have been placed.
  // FIXME: We should allow overflow by up to half a pixel here (bug 21193).
  return !aFloatAvailableSpace.mHasFloats ||
         aFloatAvailableSpace.mRect.width >= aFloatWidth;
}

static nscoord
FloatMarginWidth(const nsHTMLReflowState& aCBReflowState,
                 nscoord aFloatAvailableWidth,
                 nsIFrame *aFloat,
                 const nsCSSOffsetState& aFloatOffsetState)
{
  AutoMaybeDisableFontInflation an(aFloat);
  return aFloat->ComputeSize(
    aCBReflowState.rendContext,
    nsSize(aCBReflowState.ComputedWidth(),
           aCBReflowState.ComputedHeight()),
    aFloatAvailableWidth,
    nsSize(aFloatOffsetState.mComputedMargin.LeftRight(),
           aFloatOffsetState.mComputedMargin.TopBottom()),
    nsSize(aFloatOffsetState.mComputedBorderPadding.LeftRight() -
             aFloatOffsetState.mComputedPadding.LeftRight(),
           aFloatOffsetState.mComputedBorderPadding.TopBottom() -
             aFloatOffsetState.mComputedPadding.TopBottom()),
    nsSize(aFloatOffsetState.mComputedPadding.LeftRight(),
           aFloatOffsetState.mComputedPadding.TopBottom()),
    true).width +
  aFloatOffsetState.mComputedMargin.LeftRight() +
  aFloatOffsetState.mComputedBorderPadding.LeftRight();
}

bool
nsBlockReflowState::FlowAndPlaceFloat(nsIFrame* aFloat)
{
  // Save away the Y coordinate before placing the float. We will
  // restore mY at the end after placing the float. This is
  // necessary because any adjustments to mY during the float
  // placement are for the float only, not for any non-floating
  // content.
  AutoRestore<nscoord> restoreY(mY);
  // FIXME: Should give AutoRestore a getter for the value to avoid this.
  const nscoord saveY = mY;

  // Grab the float's display information
  const nsStyleDisplay* floatDisplay = aFloat->GetStyleDisplay();

  // The float's old region, so we can propagate damage.
  nsRect oldRegion = nsFloatManager::GetRegionFor(aFloat);

  // Enforce CSS2 9.5.1 rule [2], i.e., make sure that a float isn't
  // ``above'' another float that preceded it in the flow.
  mY = NS_MAX(mFloatManager->GetLowestFloatTop(), mY);

  // See if the float should clear any preceding floats...
  // XXX We need to mark this float somehow so that it gets reflowed
  // when floats are inserted before it.
  if (NS_STYLE_CLEAR_NONE != floatDisplay->mBreakType) {
    // XXXldb Does this handle vertical margins correctly?
    mY = ClearFloats(mY, floatDisplay->mBreakType);
  }
    // Get the band of available space
  nsFlowAreaRect floatAvailableSpace = GetFloatAvailableSpace(mY);
  nsRect adjustedAvailableSpace = mBlock->AdjustFloatAvailableSpace(*this,
                                    floatAvailableSpace.mRect, aFloat);

  NS_ASSERTION(aFloat->GetParent() == mBlock,
               "Float frame has wrong parent");

  nsCSSOffsetState offsets(aFloat, mReflowState.rendContext,
                           mReflowState.ComputedWidth());

  nscoord floatMarginWidth = FloatMarginWidth(mReflowState,
                                              adjustedAvailableSpace.width,
                                              aFloat, offsets);

  nsMargin floatMargin; // computed margin
  nsReflowStatus reflowStatus;

  // If it's a floating first-letter, we need to reflow it before we
  // know how wide it is (since we don't compute which letters are part
  // of the first letter until reflow!).
  bool isLetter = aFloat->GetType() == nsGkAtoms::letterFrame;
  if (isLetter) {
    mBlock->ReflowFloat(*this, adjustedAvailableSpace, aFloat,
                        floatMargin, false, reflowStatus);
    floatMarginWidth = aFloat->GetSize().width + floatMargin.LeftRight();
    NS_ASSERTION(NS_FRAME_IS_COMPLETE(reflowStatus),
                 "letter frames shouldn't break, and if they do now, "
                 "then they're breaking at the wrong point");
  }

  // Find a place to place the float. The CSS2 spec doesn't want
  // floats overlapping each other or sticking out of the containing
  // block if possible (CSS2 spec section 9.5.1, see the rule list).
  NS_ASSERTION((NS_STYLE_FLOAT_LEFT == floatDisplay->mFloats) ||
	       (NS_STYLE_FLOAT_RIGHT == floatDisplay->mFloats),
	       "invalid float type");

  // Can the float fit here?
  bool keepFloatOnSameLine = false;

  // Are we required to place at least part of the float because we're
  // at the top of the page (to avoid an infinite loop of pushing and
  // breaking).
  bool mustPlaceFloat =
    mReflowState.mFlags.mIsTopOfPage && IsAdjacentWithTop();

  for (;;) {
    if (mReflowState.availableHeight != NS_UNCONSTRAINEDSIZE &&
        floatAvailableSpace.mRect.height <= 0 &&
        !mustPlaceFloat) {
      // No space, nowhere to put anything.
      PushFloatPastBreak(aFloat);
      return false;
    }

    if (CanPlaceFloat(floatMarginWidth, floatAvailableSpace)) {
      // We found an appropriate place.
      break;
    }

    // Nope. try to advance to the next band.
    if (NS_STYLE_DISPLAY_TABLE != floatDisplay->mDisplay ||
          eCompatibility_NavQuirks != mPresContext->CompatibilityMode() ) {

      mY += floatAvailableSpace.mRect.height;
      if (adjustedAvailableSpace.height != NS_UNCONSTRAINEDSIZE) {
        adjustedAvailableSpace.height -= floatAvailableSpace.mRect.height;
      }
      floatAvailableSpace = GetFloatAvailableSpace(mY);
    } else {
      // This quirk matches the one in nsBlockFrame::AdjustFloatAvailableSpace
      // IE handles float tables in a very special way

      // see if the previous float is also a table and has "align"
      nsFloatCache* fc = mCurrentLineFloats.Head();
      nsIFrame* prevFrame = nullptr;
      while (fc) {
        if (fc->mFloat == aFloat) {
          break;
        }
        prevFrame = fc->mFloat;
        fc = fc->Next();
      }
      
      if(prevFrame) {
        //get the frame type
        if (nsGkAtoms::tableOuterFrame == prevFrame->GetType()) {
          //see if it has "align="
          // IE makes a difference between align and he float property
          nsIContent* content = prevFrame->GetContent();
          if (content) {
            // we're interested only if previous frame is align=left
            // IE messes things up when "right" (overlapping frames) 
            if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::align,
                                     NS_LITERAL_STRING("left"), eIgnoreCase)) {
              keepFloatOnSameLine = true;
              // don't advance to next line (IE quirkie behaviour)
              // it breaks rule CSS2/9.5.1/1, but what the hell
              // since we cannot evangelize the world
              break;
            }
          }
        }
      }

      // the table does not fit anymore in this line so advance to next band 
      mY += floatAvailableSpace.mRect.height;
      // To match nsBlockFrame::AdjustFloatAvailableSpace, we have to
      // get a new width for the new band.
      floatAvailableSpace = GetFloatAvailableSpace(mY);
      adjustedAvailableSpace = mBlock->AdjustFloatAvailableSpace(*this,
                                 floatAvailableSpace.mRect, aFloat);
      floatMarginWidth = FloatMarginWidth(mReflowState,
                                          adjustedAvailableSpace.width,
                                          aFloat, offsets);
    }

    mustPlaceFloat = false;
  }

  // If the float is continued, it will get the same absolute x value as its prev-in-flow

  // We don't worry about the geometry of the prev in flow, let the continuation
  // place and size itself as required.

  // Assign an x and y coordinate to the float.
  nscoord floatX, floatY;
  if (NS_STYLE_FLOAT_LEFT == floatDisplay->mFloats) {
    floatX = floatAvailableSpace.mRect.x;
  }
  else {
    if (!keepFloatOnSameLine) {
      floatX = floatAvailableSpace.mRect.XMost() - floatMarginWidth;
    } 
    else {
      // this is the IE quirk (see few lines above)
      // the table is kept in the same line: don't let it overlap the
      // previous float 
      floatX = floatAvailableSpace.mRect.x;
    }
  }
  // CSS2 spec, 9.5.1 rule [4]: "A floating box's outer top may not
  // be higher than the top of its containing block."  (Since the
  // containing block is the content edge of the block box, this
  // means the margin edge of the float can't be higher than the
  // content edge of the block that contains it.)
  floatY = NS_MAX(mY, mContentArea.y);

  // Reflow the float after computing its vertical position so it knows
  // where to break.
  if (!isLetter) {
    bool pushedDown = mY != saveY;
    mBlock->ReflowFloat(*this, adjustedAvailableSpace, aFloat,
                        floatMargin, pushedDown, reflowStatus);
  }
  if (aFloat->GetPrevInFlow())
    floatMargin.top = 0;
  if (NS_FRAME_IS_NOT_COMPLETE(reflowStatus))
    floatMargin.bottom = 0;

  // In the case that we're in columns and not splitting floats, we need
  // to check here that the float's height fit, and if it didn't, bail.
  // (This code is only for DISABLE_FLOAT_BREAKING_IN_COLUMNS .)
  //
  // Likewise, if none of the float fit, and it needs to be pushed in
  // its entirety to the next page (NS_FRAME_IS_TRUNCATED or
  // NS_INLINE_IS_BREAK_BEFORE), we need to do the same.
  if ((mContentArea.height != NS_UNCONSTRAINEDSIZE &&
       adjustedAvailableSpace.height == NS_UNCONSTRAINEDSIZE &&
       !mustPlaceFloat &&
       aFloat->GetSize().height + floatMargin.TopBottom() >
         mContentArea.YMost() - floatY) ||
      NS_FRAME_IS_TRUNCATED(reflowStatus) ||
      NS_INLINE_IS_BREAK_BEFORE(reflowStatus)) {
    PushFloatPastBreak(aFloat);
    return false;
  }

  // We can't use aFloat->ShouldAvoidBreakInside(mReflowState) here since
  // its mIsTopOfPage may be true even though the float isn't at the
  // top when floatY > 0.
  if (mContentArea.height != NS_UNCONSTRAINEDSIZE &&
      !mustPlaceFloat && (!mReflowState.mFlags.mIsTopOfPage || floatY > 0) &&
      NS_STYLE_PAGE_BREAK_AVOID == aFloat->GetStyleDisplay()->mBreakInside &&
      (!NS_FRAME_IS_FULLY_COMPLETE(reflowStatus) ||
       aFloat->GetSize().height + floatMargin.TopBottom() >
         mContentArea.YMost() - floatY) &&
      !aFloat->GetPrevInFlow()) {
    PushFloatPastBreak(aFloat);
    return false;
  }

  // Calculate the actual origin of the float frame's border rect
  // relative to the parent block; the margin must be added in
  // to get the border rect
  nsPoint origin(floatMargin.left + floatX,
                 floatMargin.top + floatY);

  // If float is relatively positioned, factor that in as well
  origin += aFloat->GetRelativeOffset(floatDisplay);

  // Position the float and make sure and views are properly
  // positioned. We need to explicitly position its child views as
  // well, since we're moving the float after flowing it.
  bool moved = aFloat->GetPosition() != origin;
  if (moved) {
    aFloat->SetPosition(origin);
    nsContainerFrame::PositionFrameView(aFloat);
    nsContainerFrame::PositionChildViews(aFloat);
  }

  // Update the float combined area state
  // XXX Floats should really just get invalidated here if necessary
  mFloatOverflowAreas.UnionWith(aFloat->GetOverflowAreas() + origin);

  // Place the float in the float manager
  // calculate region
  nsRect region = nsFloatManager::CalculateRegionFor(aFloat, floatMargin);
  // if the float split, then take up all of the vertical height
  if (NS_FRAME_IS_NOT_COMPLETE(reflowStatus) &&
      (NS_UNCONSTRAINEDSIZE != mContentArea.height)) {
    region.height = NS_MAX(region.height, mContentArea.height - floatY);
  }
  DebugOnly<nsresult> rv =
    mFloatManager->AddFloat(aFloat, region);
  NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "bad float placement");
  // store region
  nsFloatManager::StoreRegionFor(aFloat, region);

  // If the float's dimensions have changed, note the damage in the
  // float manager.
  if (!region.IsEqualEdges(oldRegion)) {
    // XXXwaterson conservative: we could probably get away with noting
    // less damage; e.g., if only height has changed, then only note the
    // area into which the float has grown or from which the float has
    // shrunk.
    nscoord top = NS_MIN(region.y, oldRegion.y);
    nscoord bottom = NS_MAX(region.YMost(), oldRegion.YMost());
    mFloatManager->IncludeInDamage(top, bottom);
  }

  if (!NS_FRAME_IS_FULLY_COMPLETE(reflowStatus)) {
    mBlock->SplitFloat(*this, aFloat, reflowStatus);
  }

#ifdef NOISY_FLOATMANAGER
  nscoord tx, ty;
  mFloatManager->GetTranslation(tx, ty);
  nsFrame::ListTag(stdout, mBlock);
  printf(": FlowAndPlaceFloat: AddFloat: txy=%d,%d (%d,%d) {%d,%d,%d,%d}\n",
         tx, ty, mFloatManagerX, mFloatManagerY,
         region.x, region.y, region.width, region.height);
#endif

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsRect r = aFloat->GetRect();
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("placed float: ");
    nsFrame::ListTag(stdout, aFloat);
    printf(" %d,%d,%d,%d\n", r.x, r.y, r.width, r.height);
  }
#endif

  return true;
}

void
nsBlockReflowState::PushFloatPastBreak(nsIFrame *aFloat)
{
  // This ensures that we:
  //  * don't try to place later but smaller floats (which CSS says
  //    must have their tops below the top of this float)
  //  * don't waste much time trying to reflow this float again until
  //    after the break
  if (aFloat->GetStyleDisplay()->mFloats == NS_STYLE_FLOAT_LEFT) {
    mFloatManager->SetPushedLeftFloatPastBreak();
  } else {
    NS_ABORT_IF_FALSE(aFloat->GetStyleDisplay()->mFloats ==
                        NS_STYLE_FLOAT_RIGHT,
                      "unexpected float value");
    mFloatManager->SetPushedRightFloatPastBreak();
  }

  // Put the float on the pushed floats list, even though it
  // isn't actually a continuation.
  DebugOnly<nsresult> rv = mBlock->StealFrame(mPresContext, aFloat);
  NS_ASSERTION(NS_SUCCEEDED(rv), "StealFrame should succeed");
  AppendPushedFloat(aFloat);

  NS_FRAME_SET_OVERFLOW_INCOMPLETE(mReflowStatus);
}

/**
 * Place below-current-line floats.
 */
void
nsBlockReflowState::PlaceBelowCurrentLineFloats(nsFloatCacheFreeList& aList,
                                                nsLineBox* aLine)
{
  nsFloatCache* fc = aList.Head();
  while (fc) {
#ifdef DEBUG
    if (nsBlockFrame::gNoisyReflow) {
      nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
      printf("placing bcl float: ");
      nsFrame::ListTag(stdout, fc->mFloat);
      printf("\n");
    }
#endif
    // Place the float
    bool placed = FlowAndPlaceFloat(fc->mFloat);
    nsFloatCache *next = fc->Next();
    if (!placed) {
      aList.Remove(fc);
      delete fc;
      aLine->SetHadFloatPushed();
    }
    fc = next;
  }
}

nscoord
nsBlockReflowState::ClearFloats(nscoord aY, uint8_t aBreakType,
                                nsIFrame *aReplacedBlock,
                                uint32_t aFlags)
{
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("clear floats: in: aY=%d\n", aY);
  }
#endif

#ifdef NOISY_FLOAT_CLEARING
  printf("nsBlockReflowState::ClearFloats: aY=%d breakType=%d\n",
         aY, aBreakType);
  mFloatManager->List(stdout);
#endif
  
  nscoord newY = aY;

  if (aBreakType != NS_STYLE_CLEAR_NONE) {
    newY = mFloatManager->ClearFloats(newY, aBreakType, aFlags);
  }

  if (aReplacedBlock) {
    for (;;) {
      nsFlowAreaRect floatAvailableSpace = GetFloatAvailableSpace(newY);
      nsBlockFrame::ReplacedElementWidthToClear replacedWidth =
        nsBlockFrame::WidthToClearPastFloats(*this, floatAvailableSpace.mRect,
                                             aReplacedBlock);
      if (!floatAvailableSpace.mHasFloats ||
          NS_MAX(floatAvailableSpace.mRect.x - mContentArea.x,
                 replacedWidth.marginLeft) +
            replacedWidth.borderBoxWidth +
            NS_MAX(mContentArea.XMost() - floatAvailableSpace.mRect.XMost(),
                   replacedWidth.marginRight) <=
          mContentArea.width) {
        break;
      }
      // See the analogous code for inlines in nsBlockFrame::DoReflowInlineFrames
      if (floatAvailableSpace.mRect.height > 0) {
        // See if there's room in the next band.
        newY += floatAvailableSpace.mRect.height;
      } else {
        if (mReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
          // Stop trying to clear here; we'll just get pushed to the
          // next column or page and try again there.
          break;
        }
        NS_NOTREACHED("avail space rect with zero height!");
        newY += 1;
      }
    }
  }

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
    printf("clear floats: out: y=%d\n", newY);
  }
#endif

  return newY;
}

