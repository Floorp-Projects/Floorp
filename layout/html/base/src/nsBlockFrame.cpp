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
#include "nsBlockFrame.h"
#include "nsBlockReflowContext.h"
#include "nsBlockBandData.h"
#include "nsBulletFrame.h"
#include "nsLineBox.h"
#include "nsInlineFrame.h"
#include "nsLineLayout.h"
#include "nsPlaceholderFrame.h"
#include "nsStyleConsts.h"
#include "nsHTMLIIDs.h"
#include "nsCSSRendering.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIReflowCommand.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIFontMetrics.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLValue.h"
#include "nsDOMEvent.h"
#include "nsIHTMLContent.h"
#include "prprf.h"
#include "nsLayoutAtoms.h"

// XXX temporary for :first-letter support
#include "nsITextContent.h"
static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID);/* XXX */

// XXX for IsEmptyLine
#include "nsTextFragment.h"

// XXX HTML:P's that are empty yet have style indicating they should
// clear floaters - we need to ignore the clear behavior.

#ifdef NS_DEBUG
#undef NOISY_FIRST_LINE
#undef  REALLY_NOISY_FIRST_LINE
#undef NOISY_FIRST_LETTER
#undef NOISY_MAX_ELEMENT_SIZE
#undef NOISY_FLOATER_CLEARING
#undef NOISY_INCREMENTAL_REFLOW
#undef NOISY_FINAL_SIZE
#undef NOISY_REMOVE_FRAME
#undef NOISY_DAMAGE_REPAIR
#undef NOISY_COMBINED_AREA
#undef NOISY_VERTICAL_MARGINS
#undef NOISY_REFLOW_REASON
#undef REFLOW_STATUS_COVERAGE
#else
#undef NOISY_FIRST_LINE
#undef REALLY_NOISY_FIRST_LINE
#undef NOISY_FIRST_LETTER
#undef NOISY_MAX_ELEMENT_SIZE
#undef NOISY_FLOATER_CLEARING
#undef NOISY_INCREMENTAL_REFLOW
#undef NOISY_FINAL_SIZE
#undef NOISY_REMOVE_FRAME
#undef NOISY_DAMAGE_REPAIR
#undef NOISY_COMBINED_AREA
#undef NOISY_VERTICAL_MARGINS
#undef NOISY_REFLOW_REASON
#undef REFLOW_STATUS_COVERAGE
#endif

//----------------------------------------------------------------------

// Debugging support code

#ifdef NOISY_INCREMENTAL_REFLOW
static PRInt32 gNoiseIndent;
static const char* kReflowCommandType[] = {
  "FrameAppended",
  "FrameInserted",
  "FrameRemoved",
  "ContentChanged",
  "StyleChanged",
  "PullupReflow",
  "PushReflow",
  "CheckPullupReflow",
  "ReflowDirty",
  "UserDefined",
};
#endif

#ifdef REALLY_NOISY_FIRST_LINE
static void
DumpStyleGeneaology(nsIFrame* aFrame, const char* gap)
{
  fputs(gap, stdout);
  nsFrame::ListTag(stdout, aFrame);
  printf(": ");
  nsIStyleContext* sc;
  aFrame->GetStyleContext(&sc);
  while (nsnull != sc) {
    nsIStyleContext* psc;
    printf("%p ", sc);
    psc = sc->GetParent();
    NS_RELEASE(sc);
    sc = psc;
  }
  printf("\n");
}
#endif

#ifdef REFLOW_STATUS_COVERAGE
static void
RecordReflowStatus(PRBool aChildIsBlock, nsReflowStatus aFrameReflowStatus)
{
  static PRUint32 record[2];

  // 0: child-is-block
  // 1: child-is-inline
  PRIntn index = 0;
  if (!aChildIsBlock) index |= 1;

  // Compute new status
  PRUint32 newS = record[index];
  if (NS_INLINE_IS_BREAK(aFrameReflowStatus)) {
    if (NS_INLINE_IS_BREAK_BEFORE(aFrameReflowStatus)) {
      newS |= 1;
    }
    else if (NS_FRAME_IS_NOT_COMPLETE(aFrameReflowStatus)) {
      newS |= 2;
    }
    else {
      newS |= 4;
    }
  }
  else if (NS_FRAME_IS_NOT_COMPLETE(aFrameReflowStatus)) {
    newS |= 8;
  }
  else {
    newS |= 16;
  }

  // Log updates to the status that yield different values
  if (record[index] != newS) {
    record[index] = newS;
    printf("record(%d): %02x %02x\n", index, record[0], record[1]);
  }
}
#endif

//----------------------------------------------------------------------

inline void CombineRects(const nsRect& r1, nsRect& r2)
{
  nscoord x0 = r2.x;
  nscoord y0 = r2.y;
  nscoord x1 = x0 + r2.width;
  nscoord y1 = y0 + r2.height;
  nscoord x = r1.x;
  nscoord y = r1.y;
  nscoord xmost = x + r1.width;
  nscoord ymost = y + r1.height;
  if (x < x0) {
    x0 = x;
  }
  if (xmost > x1) {
    x1 = xmost;
  }
  if (y < y0) {
    y0 = y;
  }
  if (ymost > y1) {
    y1 = ymost;
  }
  r2.x = x0;
  r2.y = y0;
  r2.width = x1 - x0;
  r2.height = y1 - y0;
}

//----------------------------------------------------------------------

class nsBlockReflowState {
public:
  nsBlockReflowState(const nsHTMLReflowState& aReflowState,
                     nsIPresContext* aPresContext,
                     nsBlockFrame* aFrame,
                     const nsHTMLReflowMetrics& aMetrics,
                     nsLineLayout* aLineLayout);

  ~nsBlockReflowState();

  /**
   * Update our state when aLine is skipped over during incremental
   * reflow.
   */
  void RecoverStateFrom(nsLineBox* aLine, PRBool aPrevLineWasClean);

  /**
   * Get the available reflow space for the current y coordinate. The
   * available space is relative to our coordinate system (0,0) is our
   * upper left corner.
   */
  void GetAvailableSpace() {
#ifdef DEBUG
    // Verify that the caller setup the coordinate system properly
    nscoord wx, wy;
    mSpaceManager->GetTranslation(wx, wy);
    NS_ASSERTION((wx == mSpaceManagerX) && (wy == mSpaceManagerY),
                 "bad coord system");
#endif
    mBand.GetAvailableSpace(mY - BorderPadding().top, mAvailSpaceRect);

#ifdef NOISY_INCREMENTAL_REFLOW
    if (mReflowState.reason == eReflowReason_Incremental) {
      nsFrame::IndentBy(stdout, gNoiseIndent);
      printf("GetAvailableSpace: band=%d,%d,%d,%d count=%d\n",
             mAvailSpaceRect.x, mAvailSpaceRect.y,
             mAvailSpaceRect.width, mAvailSpaceRect.height,
             mBand.GetTrapezoidCount());
    }
#endif
  }

  void InitFloater(nsPlaceholderFrame* aPlaceholderFrame);

  void AddFloater(nsPlaceholderFrame* aPlaceholderFrame,
                  PRBool aInitialReflow);

  PRBool CanPlaceFloater(const nsRect& aFloaterRect, PRUint8 aFloats);

  void PlaceFloater(nsPlaceholderFrame* aFloater,
                    const nsMargin& aFloaterMargins,
                    PRBool* aIsLeftFloater,
                    nsPoint* aNewOrigin);

  void PlaceBelowCurrentLineFloaters(nsVoidArray* aFloaters,
                                     PRBool aReflowFloaters);

  void PlaceCurrentLineFloaters(nsVoidArray* aFloaters);

  void ClearFloaters(nscoord aY, PRUint8 aBreakType);

  PRBool ClearPastFloaters(PRUint8 aBreakType);

  PRBool IsLeftMostChild(nsIFrame* aFrame);

  PRBool IsAdjacentWithTop() const {
    return mY == mReflowState.mComputedBorderPadding.top;
  }

  const nsMargin& BorderPadding() const {
    return mReflowState.mComputedBorderPadding;
  }

  void UpdateMaxElementSize(const nsSize& aMaxElementSize) {
#ifdef NOISY_MAX_ELEMENT_SIZE
    nsSize oldSize = mMaxElementSize;
#endif
    if (aMaxElementSize.width > mMaxElementSize.width) {
      mMaxElementSize.width = aMaxElementSize.width;
    }
    if (aMaxElementSize.height > mMaxElementSize.height) {
      mMaxElementSize.height = aMaxElementSize.height;
    }
#ifdef NOISY_MAX_ELEMENT_SIZE
    if ((mMaxElementSize.width != oldSize.width) ||
        (mMaxElementSize.height != oldSize.height)) {
      nsFrame::IndentBy(stdout, GetDepth());
      if (NS_UNCONSTRAINEDSIZE == mReflowState.availableWidth) {
        printf("PASS1 ");
      }
      nsFrame::ListTag(stdout, mBlock);
      printf(": old max-element-size=%d,%d new=%d,%d\n",
             oldSize.width, oldSize.height,
             mMaxElementSize.width, mMaxElementSize.height);
    }
#endif
  }

  void RecoverVerticalMargins(nsLineBox* aLine,
                              PRBool aApplyTopMargin,
                              nscoord* aTopMarginResult,
                              nscoord* aBottomMarginResult);

  void ComputeBlockAvailSpace(nsSplittableType aSplitType, nsRect& aResult);

  void RecoverStateFrom(nsLineBox* aLine,
                        PRBool aApplyTopMargin,
                        nscoord aDeltaY);

  //----------------------------------------

  // This state is the "global" state computed once for the reflow of
  // the block.

  // The block frame that is using this object
  nsBlockFrame* mBlock;

  nsIPresContext* mPresContext;

  const nsHTMLReflowState& mReflowState;

  nsLineLayout* mLineLayout;

  nsISpaceManager* mSpaceManager;

  // The coordinates within the spacemanager where the block is being
  // placed <b>after</b> taking into account the blocks border and
  // padding. This, therefore, represents the inner "content area" (in
  // spacemanager coordinates) where child frames will be placed,
  // including child blocks and floaters.
  nscoord mSpaceManagerX, mSpaceManagerY;

  // XXX get rid of this
  nsReflowStatus mReflowStatus;

  nscoord mBottomEdge;

  PRBool mUnconstrainedWidth;

  PRBool mUnconstrainedHeight;

  // The content area to reflow child frames within. The x/y
  // coordinates are known to be mBorderPadding.left and
  // mBorderPadding.top. The width/height may be NS_UNCONSTRAINEDSIZE
  // if the container reflowing this frame has given the frame an
  // unconstrained area.
  nsSize mContentArea;

  // Our wrapping behavior
  PRBool mNoWrap;

  // Is this frame a root for top/bottom margin collapsing?
  PRBool mIsTopMarginRoot, mIsBottomMarginRoot;

  // See ShouldApplyTopMargin
  PRBool mApplyTopMargin;

  //----------------------------------------

  // This state is "running" state updated by the reflow of each line
  // in the block. This same state is "recovered" when a line is not
  // dirty and is passed over during incremental reflow.

  // The current line being reflowed
  nsLineBox* mCurrentLine;

  // The previous line just reflowed
  nsLineBox* mPrevLine;

  // The current Y coordinate in the block
  nscoord mY;

  // The available space within the current band.
  nsRect mAvailSpaceRect;

  // The maximum x-most of each line
  nscoord mKidXMost;

  // The combined area of all floaters placed so far
  nsRect mFloaterCombinedArea;

  // Previous child. This is used when pulling up a frame to update
  // the sibling list.
  nsIFrame* mPrevChild;

  // The next immediate child frame that is the target of an
  // incremental reflow command. Once that child has been reflowed we
  // null this slot out.
  nsIFrame* mNextRCFrame;

  // The previous child frames collapsed bottom margin value.
  nscoord mPrevBottomMargin;

  // The current next-in-flow for the block. When lines are pulled
  // from a next-in-flow, this is used to know which next-in-flow to
  // pull from. When a next-in-flow is emptied of lines, we advance
  // this to the next next-in-flow.
  nsBlockFrame* mNextInFlow;

  // The current band data for the current Y coordinate
  nsBlockBandData mBand;

  //----------------------------------------

  // Temporary line-reflow state. This state is used during the reflow
  // of a given line, but doesn't have meaning before or after.
  nsVoidArray mPendingFloaters;

  PRBool mComputeMaxElementSize;

  nsSize mMaxElementSize;
};

// XXX This is vile. Make it go away
void
nsLineLayout::InitFloater(nsPlaceholderFrame* aFrame)
{
  mBlockRS->InitFloater(aFrame);
}
void
nsLineLayout::AddFloater(nsPlaceholderFrame* aFrame)
{
  mBlockRS->AddFloater(aFrame, PR_FALSE);
}

//----------------------------------------------------------------------

nsBlockReflowState::nsBlockReflowState(const nsHTMLReflowState& aReflowState,
                                       nsIPresContext* aPresContext,
                                       nsBlockFrame* aFrame,
                                       const nsHTMLReflowMetrics& aMetrics,
                                       nsLineLayout* aLineLayout)
  : mBlock(aFrame),
    mPresContext(aPresContext),
    mReflowState(aReflowState),
    mIsTopMarginRoot(PR_FALSE),
    mIsBottomMarginRoot(PR_FALSE),
    mApplyTopMargin(PR_FALSE),
    mNextRCFrame(nsnull),
    mPrevBottomMargin(0)
{
  mLineLayout = aLineLayout;

  mSpaceManager = aReflowState.spaceManager;

  // Translate into our content area and then save the 
  // coordinate system origin for later.
  const nsMargin& borderPadding = BorderPadding();
  mSpaceManager->Translate(borderPadding.left, borderPadding.top);
  mSpaceManager->GetTranslation(mSpaceManagerX, mSpaceManagerY);

  mReflowStatus = NS_FRAME_COMPLETE;

  mPresContext = aPresContext;
  mBlock->GetNextInFlow((nsIFrame**)&mNextInFlow);
  mKidXMost = 0;

  // Compute content area width (the content area is inside the border
  // and padding)
  mUnconstrainedWidth = PR_FALSE;
  if (NS_UNCONSTRAINEDSIZE != aReflowState.computedWidth) {
    mContentArea.width = aReflowState.computedWidth;
  }
  else {
    if (NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) {
      mContentArea.width = NS_UNCONSTRAINEDSIZE;
      mUnconstrainedWidth = PR_TRUE;
    }
    else {
      nscoord lr = borderPadding.left + borderPadding.right;
      mContentArea.width = aReflowState.availableWidth - lr;
    }
  }

  // Compute content area height. Unlike the width, if we have a
  // specified style height we ignore it since extra content is
  // managed by the "overflow" property. When we don't have a
  // specified style height then we may end up limiting our height if
  // the availableHeight is constrained (this situation occurs when we
  // are paginated).
  mUnconstrainedHeight = PR_FALSE;
  if (NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight) {
    // We are in a paginated situation. The bottom edge is just inside
    // the bottom border and padding. The content area height doesn't
    // include either border or padding edge.
    mBottomEdge = aReflowState.availableHeight - borderPadding.bottom;
    mContentArea.height = mBottomEdge - borderPadding.top;
  }
  else {
    // When we are not in a paginated situation then we always use
    // an constrained height.
    mUnconstrainedHeight = PR_TRUE;
    mContentArea.height = mBottomEdge = NS_UNCONSTRAINEDSIZE;
  }

  mY = borderPadding.top;
  mBand.Init(mSpaceManager, mContentArea);

  mPrevChild = nsnull;
  mCurrentLine = nsnull;
  mPrevLine = nsnull;

  const nsStyleText* styleText;
  mBlock->GetStyleData(eStyleStruct_Text,
                       (const nsStyleStruct*&) styleText);
  switch (styleText->mWhiteSpace) {
  case NS_STYLE_WHITESPACE_PRE:
  case NS_STYLE_WHITESPACE_NOWRAP:
    mNoWrap = PR_TRUE;
    break;
  default:
    mNoWrap = PR_FALSE;
    break;
  }

  mComputeMaxElementSize = nsnull != aMetrics.maxElementSize;;
  mMaxElementSize.SizeTo(0, 0);

  if (0 != borderPadding.top) {
    mIsTopMarginRoot = PR_TRUE;
  }
  if (0 != borderPadding.bottom) {
    mIsBottomMarginRoot = PR_TRUE;
  }
}

nsBlockReflowState::~nsBlockReflowState()
{
  // Restore the coordinate system
  const nsMargin& borderPadding = BorderPadding();
  mSpaceManager->Translate(-borderPadding.left, -borderPadding.top);
}

// Compute the amount of available space for reflowing a block frame
// at the current Y coordinate. This method assumes that
// GetAvailableSpace has already been called.
void
nsBlockReflowState::ComputeBlockAvailSpace(nsSplittableType aSplitType,
                                           nsRect& aResult)
{
  nscoord availHeight = mUnconstrainedHeight
    ? NS_UNCONSTRAINEDSIZE
    : mBottomEdge - mY;

  const nsMargin& borderPadding = BorderPadding();
  nscoord availX, availWidth;
  if (NS_FRAME_SPLITTABLE_NON_RECTANGULAR == aSplitType) {
    // Frames that know how to do non-rectangular splitting are given
    // the entire available space, including space consumed by
    // floaters.
    availX = borderPadding.left;
    availWidth = mUnconstrainedWidth
      ? NS_UNCONSTRAINEDSIZE
      : mContentArea.width;
  }
  else {
    // Assume the frame is clueless about the space manager and only
    // give it free space.
    availX = mAvailSpaceRect.x + borderPadding.left;
    availWidth = mAvailSpaceRect.width;
  }
  aResult.SetRect(availX, mY, availWidth, availHeight);
}

PRBool
nsBlockReflowState::ClearPastFloaters(PRUint8 aBreakType)
{
  nscoord saveY, deltaY;

  PRBool applyTopMargin = PR_FALSE;
  switch (aBreakType) {
  case NS_STYLE_CLEAR_LEFT:
  case NS_STYLE_CLEAR_RIGHT:
  case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
    // Apply the previous margin before clearing
    saveY = mY + mPrevBottomMargin;
    ClearFloaters(saveY, aBreakType);

    // Determine how far we just moved. If we didn't move then there
    // was nothing to clear to don't mess with the normal margin
    // collapsing behavior. In either case we need to restore the Y
    // coordinate to what it was before the clear.
    deltaY = mY - saveY;
    if (0 != deltaY) {
      // Pretend that the distance we just moved is a previous
      // blocks bottom margin. Note that GetAvailableSpace has been
      // done so that the available space calculations will be done
      // after clearing the appropriate floaters.
      //
      // What we are doing here is applying CSS2 section 9.5.2's
      // rules for clearing - "The top margin of the generated box
      // is increased enough that the top border edge is below the
      // bottom outer edge of the floating boxes..."
      //
      // What this will do is cause the top-margin of the block
      // frame we are about to reflow to be collapsed with that
      // distance.
      mPrevBottomMargin = deltaY;
      mY = saveY;

      // Force margin to be applied in this circumstance
      applyTopMargin = PR_TRUE;
    }
    else {
      // Put aState.mY back to its original value since no clearing
      // happened. That way the previous blocks bottom margin will
      // be applied properly.
      mY = saveY - mPrevBottomMargin;
    }
    break;
  }
  return applyTopMargin;
}

// Recover the collapsed vertical margin values for aLine. Note that
// the values are not collapsed with aState.mPrevBottomMargin, nor are
// they collapsed with each other when the line height is zero.
void
nsBlockReflowState::RecoverVerticalMargins(nsLineBox* aLine,
                                           PRBool aApplyTopMargin,
                                           nscoord* aTopMarginResult,
                                           nscoord* aBottomMarginResult)
{
  if (aLine->IsBlock()) {
    // Update band data
    GetAvailableSpace();

    // Setup reflow state to compute the block childs top and bottom
    // margins
    nsIFrame* frame = aLine->mFirstChild;
    nsSplittableType splitType = NS_FRAME_NOT_SPLITTABLE;
    frame->IsSplittable(splitType);
    nsRect availSpaceRect;
    ComputeBlockAvailSpace(splitType, availSpaceRect);
    nsSize availSpace(availSpaceRect.width, availSpaceRect.height);
    nsHTMLReflowState reflowState(*mPresContext, mReflowState,
                                  frame, availSpace);

    // Compute collapsed top margin
    nscoord topMargin = 0;
    if (aApplyTopMargin) {
      topMargin =
        nsBlockReflowContext::ComputeCollapsedTopMargin(mPresContext,
                                                        reflowState);
    }

    // Compute collapsed bottom margin
    nscoord bottomMargin = reflowState.computedMargin.bottom;
    bottomMargin =
      nsBlockReflowContext::MaxMargin(bottomMargin,
                                      aLine->mCarriedOutBottomMargin);
    *aTopMarginResult = topMargin;
    *aBottomMarginResult = bottomMargin;
  }
  else {
    // XXX_ib
    *aTopMarginResult = 0;
    *aBottomMarginResult = 0;
  }
}

void
nsBlockReflowState::RecoverStateFrom(nsLineBox* aLine,
                                     PRBool aApplyTopMargin,
                                     nscoord aDeltaY)
{
  // Make the line being recovered the current line
  mCurrentLine = aLine;

  // Update aState.mPrevChild as if we had reflowed all of the frames
  // in this line.
  mPrevChild = aLine->LastChild();

  // Recover mKidXMost
  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > mKidXMost) {
#ifdef DEBUG
    if (CRAZY_WIDTH(xmost)) {
      nsFrame::ListTag(stdout, mBlock);
      printf(": WARNING: xmost:%d\n", xmost);
    }
#endif
    mKidXMost = xmost;
  }

  // The line may have clear before semantics.
  if (NS_STYLE_CLEAR_NONE != aLine->mBreakType) {
    // Clear past floaters before the block if the clear style is not none
    aApplyTopMargin = ClearPastFloaters(aLine->mBreakType);
  }

  // Recover mPrevBottomMargin and calculate the line's new Y
  // coordinate (newLineY)
  nscoord newLineY = mY;
  if (0 == aLine->mBounds.height) {
    if (aLine->IsBlock() &&
        nsBlockReflowContext::IsHTMLParagraph(aLine->mFirstChild)) {
      // Empty HTML paragraphs disappear entirely - their margins go
      // to zero.
    }
    else {
      // The line's top and bottom margin values need to be collapsed
      // with the mPrevBottomMargin to determine a new
      // mPrevBottomMargin value.
      nscoord topMargin, bottomMargin;
      RecoverVerticalMargins(aLine, aApplyTopMargin,
                             &topMargin, &bottomMargin);
      nscoord m = nsBlockReflowContext::MaxMargin(topMargin, bottomMargin);
      m = nsBlockReflowContext::MaxMargin(m, mPrevBottomMargin);
      mPrevBottomMargin = m;
    }
  }
  else {
    // Recover the top and bottom marings for this line
    nscoord topMargin, bottomMargin;
    RecoverVerticalMargins(aLine, aApplyTopMargin,
                           &topMargin, &bottomMargin);

    // Compute the collapsed top margin value
    nscoord collapsedTopMargin =
      nsBlockReflowContext::MaxMargin(topMargin, mPrevBottomMargin);

    // The lineY is just below the collapsed top margin value. The
    // mPrevBottomMargin gets set to the bottom margin value for the
    // line.
    newLineY += collapsedTopMargin;
    mPrevBottomMargin = bottomMargin;
  }
  nscoord finalDeltaY = newLineY - aLine->mBounds.y;
  if (0 != finalDeltaY) {
    // Slide the frames in the line by the computed delta. This also
    // updates the lines Y coordinate and the combined area's Y
    // coordinate.
    mBlock->SlideLine(mPresContext, mSpaceManager, aLine, finalDeltaY);
  }

  // Now that the line's Y coordinate is updated, place floaters back
  // into the space manager
  if (nsnull != aLine->mFloaters) {
    mY = aLine->mBounds.y;
    if (0 == aLine->mBounds.height) {
      mY += mPrevBottomMargin;
    }
    PlaceCurrentLineFloaters(aLine->mFloaters);
    mY = aLine->mBounds.YMost();
    PlaceBelowCurrentLineFloaters(aLine->mFloaters, PR_FALSE);
  }

  // XXX Recover floater combined area
  //   XXX not necessary at the moment because
  //   PlaceCurrentLineFloaters and PlaceBelowCurrentLineFloaters
  //   always reflow the floaters...

  // Recover mY
  mY = aLine->mBounds.YMost();

  // It's possible that the line has clear after semantics
  if (!aLine->IsBlock() && (NS_STYLE_CLEAR_NONE != aLine->mBreakType)) {
    switch (aLine->mBreakType) {
    case NS_STYLE_CLEAR_LEFT:
    case NS_STYLE_CLEAR_RIGHT:
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      ClearFloaters(mY, aLine->mBreakType);
      break;
    }
  }
}

//----------------------------------------------------------------------

const nsIID kBlockFrameCID = NS_BLOCK_FRAME_CID;

nsresult
NS_NewBlockFrame(nsIFrame*& aNewFrame, PRUint32 aFlags)
{
  nsBlockFrame* it = new nsBlockFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetFlags(aFlags);
  aNewFrame = it;
  return NS_OK;
}

nsBlockFrame::nsBlockFrame()
{
}

nsBlockFrame::~nsBlockFrame()
{
  nsTextRun::DeleteTextRuns(mTextRuns);
}

NS_IMETHODIMP
nsBlockFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  // Outside bullets are not in our child-list so check for them here
  // and delete them when present.
  if (HaveOutsideBullet()) {
    mBullet->DeleteFrame(aPresContext);
    mBullet = nsnull;
  }

  nsLineBox::DeleteLineList(aPresContext, mLines);
  nsLineBox::DeleteLineList(aPresContext, mOverflowLines);

  mFloaters.DeleteFrames(aPresContext);

  return nsBlockFrameSuper::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
nsBlockFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kBlockFrameCID)) {
    nsBlockFrame* tmp = this;
    *aInstancePtr = (void*) tmp;
    return NS_OK;
  }
  return nsBlockFrameSuper::QueryInterface(aIID, aInstancePtr);
}

static nsresult
ReResolveLineList(nsIPresContext* aPresContext,
                  nsLineBox* aLine,
                  nsIStyleContext* aStyleContext,
                  PRInt32 aParentChange,
                  nsStyleChangeList* aChangeList)
{
  nsresult rv = NS_OK;
  PRInt32 childChange;
  while (nsnull != aLine) {
    nsIFrame* child = aLine->mFirstChild;
    PRInt32 n = aLine->mChildCount;
    while ((--n >= 0) && NS_SUCCEEDED(rv)) {
      rv = child->ReResolveStyleContext(aPresContext, aStyleContext, 
                                        aParentChange, aChangeList, &childChange);
      child->GetNextSibling(&child);
    }
    aLine = aLine->mNext;
  }
  return rv;
}

NS_IMETHODIMP
nsBlockFrame::ReResolveStyleContext(nsIPresContext* aPresContext,
                                    nsIStyleContext* aParentContext,
                                    PRInt32 aParentChange,
                                    nsStyleChangeList* aChangeList,
                                    PRInt32* aLocalChange)
{
  // NOTE: using nsFrame's ReResolveStyleContext method to avoid
  // useless version in base classes.
  PRInt32 ourChange = aParentChange;
  nsresult rv = nsFrame::ReResolveStyleContext(aPresContext, aParentContext, 
                                               ourChange, aChangeList,
                                               &ourChange);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (NS_COMFALSE != rv) {
    if (HaveOutsideBullet() && mBullet) {
      nsIStyleContext* newBulletSC;
      nsIStyleContext* oldBulletSC = nsnull;
      mBullet->GetStyleContext(&oldBulletSC);
      aPresContext->ResolvePseudoStyleContextFor(mContent,
                                              nsHTMLAtoms::mozListBulletPseudo,
                                              mStyleContext,
                                              PR_FALSE, &newBulletSC);
      rv = mBullet->SetStyleContext(aPresContext, newBulletSC);
      CaptureStyleChangeFor(this, oldBulletSC, newBulletSC, 
                            ourChange, aChangeList, &ourChange);
      NS_RELEASE(oldBulletSC);
      NS_RELEASE(newBulletSC);
    }

    if (aLocalChange) {
      *aLocalChange = ourChange;
    }

    // Update the child frames on each line
    rv = ReResolveLineList(aPresContext, mLines, mStyleContext, 
                           ourChange, aChangeList);

    // Update any overflow lines too
    if (NS_SUCCEEDED(rv) && (nsnull != mOverflowLines)) {
      rv = ReResolveLineList(aPresContext, mOverflowLines, mStyleContext, 
                             ourChange, aChangeList);
    }

    // Just in case, we update the prev-in-flow's overflow lines too
    if (NS_SUCCEEDED(rv) && (nsnull != mPrevInFlow)) {
      nsLineBox* lines = ((nsBlockFrame*)mPrevInFlow)->mOverflowLines;
      if (nsnull != lines) {
        rv = ReResolveLineList(aPresContext, lines, mStyleContext, 
                               ourChange, aChangeList);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsBlockFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_SPLITTABLE_NON_RECTANGULAR;
  return NS_OK;
}

static void
ListTextRuns(FILE* out, PRInt32 aIndent, nsTextRun* aRuns)
{
  while (nsnull != aRuns) {
    aRuns->List(out, aIndent);
    aRuns = aRuns->GetNext();
  }
}

NS_METHOD
nsBlockFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
  nsIView* view;
  GetView(&view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", view);
  }

  // Output the flow linkage
  if (nsnull != mPrevInFlow) {
    fprintf(out, " prev-in-flow=%p", mPrevInFlow);
  }
  if (nsnull != mNextInFlow) {
    fprintf(out, " next-in-flow=%p", mNextInFlow);
  }

  // Output the rect and state
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  if (0 != mFlags) {
    fprintf(out, " [flags=%x]", mFlags);
  }
  fputs("<\n", out);
  aIndent++;

  // Output the lines
  if (nsnull != mLines) {
    nsLineBox* line = mLines;
    while (nsnull != line) {
      line->List(out, aIndent);
      line = line->mNext;
    }
  }

  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  for (;;) {
    nsIFrame* kid;
    GetAdditionalChildListName(listIndex++, &listName);
    if (nsnull == listName) {
      break;
    }
    FirstChild(listName, &kid);
    if (nsnull != kid) {
      IndentBy(out, aIndent);
      nsAutoString tmp;
      if (nsnull != listName) {
        listName->ToString(tmp);
        fputs(tmp, out);
      }
      fputs("<\n", out);
      while (nsnull != kid) {
        kid->List(out, aIndent + 1);
        kid->GetNextSibling(&kid);
      }
      IndentBy(out, aIndent);
      fputs(">\n", out);
    }
    NS_IF_RELEASE(listName);
  }

  // Output the text-runs
  if (nsnull != mTextRuns) {
    IndentBy(out, aIndent);
    fputs("text-runs <\n", out);

    ListTextRuns(out, aIndent + 1, mTextRuns);

    IndentBy(out, aIndent);
    fputs(">\n", out);
  }

  aIndent--;
  IndentBy(out, aIndent);
  fputs(">\n", out);

  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Block", aResult);
}

NS_IMETHODIMP
nsBlockFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsHTMLAtoms::blockFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

NS_IMETHODIMP
nsBlockFrame::FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  if (nsnull == aListName) {
    *aFirstChild = (nsnull != mLines) ? mLines->mFirstChild : nsnull;
    return NS_OK;
  }
  else if (aListName == nsLayoutAtoms::floaterList) {
    *aFirstChild = mFloaters.FirstChild();
    return NS_OK;
  }
  else if (aListName == nsLayoutAtoms::bulletList) {
    if (HaveOutsideBullet()) {
      *aFirstChild = mBullet;
      return NS_OK;
    }
  }
  *aFirstChild = nsnull;
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsBlockFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                         nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  *aListName = nsnull;
  switch (aIndex) {
  case NS_BLOCK_FRAME_FLOATER_LIST_INDEX:
    *aListName = nsLayoutAtoms::floaterList;
    NS_ADDREF(*aListName);
    break;
  case NS_BLOCK_FRAME_BULLET_LIST_INDEX:
    if (HaveOutsideBullet()) {
      *aListName = nsLayoutAtoms::bulletList;
      NS_ADDREF(*aListName);
    }
    break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::IsPercentageBase(PRBool& aBase) const
{
  aBase = PR_TRUE;
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Frame structure methods

//////////////////////////////////////////////////////////////////////
// Reflow methods

NS_IMETHODIMP
nsBlockFrame::Reflow(nsIPresContext&          aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  nsLineLayout lineLayout(aPresContext, aReflowState.spaceManager,
                          &aReflowState, nsnull != aMetrics.maxElementSize);
  nsBlockReflowState state(aReflowState, &aPresContext, this, aMetrics,
                           &lineLayout);
  lineLayout.Init(&state);
  if (NS_BLOCK_MARGIN_ROOT & mFlags) {
    state.mIsTopMarginRoot = PR_TRUE;
    state.mIsBottomMarginRoot = PR_TRUE;
  }
  if (state.mIsTopMarginRoot) {
    state.mApplyTopMargin = PR_TRUE;
  }

  if (eReflowReason_Resize != aReflowState.reason) {
    RenumberLists();
    ComputeTextRuns(&aPresContext);
  }

  nsresult rv = NS_OK;
  nsIFrame* target;
  switch (aReflowState.reason) {
  case eReflowReason_Initial:
#ifdef NOISY_REFLOW_REASON
    ListTag(stdout);
    printf(": reflow=initial\n");
#endif
    DrainOverflowLines();
    rv = PrepareInitialReflow(state);
    mState &= ~NS_FRAME_FIRST_REFLOW;
    break;

  case eReflowReason_Incremental:
    aReflowState.reflowCommand->GetTarget(target);
    if (this == target) {
      nsIReflowCommand::ReflowType type;
      aReflowState.reflowCommand->GetType(type);
#ifdef NOISY_REFLOW_REASON
      ListTag(stdout);
      printf(": reflow=incremental type=%d\n", type);
#endif
      switch (type) {
      case nsIReflowCommand::FrameAppended:
      case nsIReflowCommand::FrameInserted:
      case nsIReflowCommand::FrameRemoved:
        break;
      case nsIReflowCommand::StyleChanged:
        rv = PrepareStyleChangedReflow(state);
        break;
      case nsIReflowCommand::ReflowDirty:
        break;
      default:
        // Map any other incremental operations into full reflows
        rv = PrepareResizeReflow(state);
        break;
      }
    }
    else {
      // Get next frame in reflow command chain
      aReflowState.reflowCommand->GetNext(state.mNextRCFrame);
#ifdef NOISY_REFLOW_REASON
      ListTag(stdout);
      printf(": reflow=incremental next=%p", state.mNextRCFrame);
      if (state.mNextRCFrame) {
        nsFrame::ListTag(stdout, state.mNextRCFrame);
      }
      printf("\n");
#endif

      // Now do the reflow
      rv = PrepareChildIncrementalReflow(state);
    }
    break;

  case eReflowReason_Resize:
  default:
#ifdef NOISY_REFLOW_REASON
    ListTag(stdout);
    printf(": reflow=resize (%d)\n", aReflowState.reason);
#endif
    DrainOverflowLines();
    rv = PrepareResizeReflow(state);
    break;
  }

  // Now reflow...
  rv = ReflowDirtyLines(state);
  aStatus = state.mReflowStatus;
  if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
    if (NS_STYLE_OVERFLOW_HIDDEN == aReflowState.mStyleDisplay->mOverflow) {
      aStatus = NS_FRAME_COMPLETE;
    }
    else {
#ifdef DEBUG_kipp
      ListTag(stdout); printf(": block is not complete\n");
#endif
    }
  }

// XXX_pref get rid of this!
  BuildFloaterList();

  // Compute our final size
  ComputeFinalSize(aReflowState, state, aMetrics);

#ifdef NOISY_FINAL_SIZE
  ListTag(stdout);
  printf(": availSize=%d,%d computed=%d,%d metrics=%d,%d carriedMargin=%d\n",
         aReflowState.availableWidth, aReflowState.availableHeight,
         aReflowState.computedWidth, aReflowState.computedHeight,
         aMetrics.width, aMetrics.height,
         aMetrics.mCarriedOutBottomMargin);
#endif
  return rv;
}

void
nsBlockFrame::ComputeFinalSize(const nsHTMLReflowState& aReflowState,
                               nsBlockReflowState& aState,
                               nsHTMLReflowMetrics& aMetrics)
{
  const nsMargin& borderPadding = aState.BorderPadding();

  // Special check for zero sized content: If our content is zero
  // sized then we collapse into nothingness.
  //
  // Consensus after discussion with a few CSS folks is that html's
  // notion of collapsing <P>'s should take precedence over non
  // auto-sided block elements. Therefore we don't honor the width,
  // height, border or padding attributes (the parent has to not apply
  // a margin for us also).
  //
  // Note that this is <b>only</b> done for html paragraphs. Its not
  // appropriate to apply it to other containers, especially XML
  // content!
  PRBool isHTMLParagraph = 0 != (mState & NS_BLOCK_IS_HTML_PARAGRAPH);
  if (isHTMLParagraph &&
      (aReflowState.mStyleDisplay->mDisplay == NS_STYLE_DISPLAY_BLOCK) &&
      (((0 == aState.mKidXMost) ||
        (0 == aState.mKidXMost - borderPadding.left)) &&
       (0 == aState.mY - borderPadding.top))) {
    // Zero out the works
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
    aMetrics.mCarriedOutBottomMargin = 0;
    if (nsnull != aMetrics.maxElementSize) {
      aMetrics.maxElementSize->width = 0;
      aMetrics.maxElementSize->height = 0;
    }
  }
  else {
    // Compute final width
    nscoord maxWidth = 0, maxHeight = 0;
    nscoord minWidth = aState.mKidXMost + borderPadding.right;
    if (!aState.mUnconstrainedWidth && aReflowState.HaveFixedContentWidth()) {
      // Use style defined width
      aMetrics.width = borderPadding.left + aReflowState.computedWidth +
        borderPadding.right;
      // XXX quote css1 section here
      if ((0 == aReflowState.computedWidth) && (aMetrics.width < minWidth)) {
        aMetrics.width = minWidth;
      }

      // When style defines the width use it for the max-element-size
      // because we can't shrink any smaller.
      maxWidth = aMetrics.width;
    }
    else {
      nscoord computedWidth = minWidth;
      PRBool compact = PR_FALSE;
#if 0
      if (NS_STYLE_DISPLAY_COMPACT == aReflowState.mStyleDisplay->mDisplay) {
        // If we are display: compact AND we have no lines or we have
        // exactly one line and that line is not a block line AND that
        // line doesn't end in a BR of any sort THEN we remain a compact
        // frame.
        if ((nsnull == mLines) ||
            ((nsnull == mLines->mNext) && !mLines->IsBlock() &&
             (NS_STYLE_CLEAR_NONE == mLines->mBreakType)
             /*XXX && (computedWidth <= aState.mCompactMarginWidth) */
              )) {
          compact = PR_TRUE;
        }
      }
#endif

      // There are two options here. We either shrink wrap around our
      // contents or we fluff out to the maximum block width. Note:
      // We always shrink wrap when given an unconstrained width.
      if ((0 == (NS_BLOCK_SHRINK_WRAP & mFlags)) &&
          !aState.mUnconstrainedWidth &&
          !compact) {
        // Set our width to the max width if we aren't already that
        // wide. Note that the max-width has nothing to do with our
        // contents (CSS2 section XXX)
        nscoord maxWidth = borderPadding.left + aState.mContentArea.width +
          borderPadding.right;
        computedWidth = maxWidth;
      }
      else if (aState.mComputeMaxElementSize) {
        if (aState.mNoWrap) {
          // When no-wrap is true the max-element-size.width is the
          // width of the widest line plus the right border. Note that
          // aState.mKidXMost already has the left border factored in
          maxWidth = aState.mKidXMost + borderPadding.right;
        }
        else {
          // Add in border and padding dimensions to already computed
          // max-element-size values.
          maxWidth = aState.mMaxElementSize.width +
            borderPadding.left + borderPadding.right;
        }

        // See if our max-element-size width is larger than our
        // computed-width. This happens when we are impacted by a
        // floater. When this does happen, our desired size needs to
        // include room for the floater.
        if (computedWidth < maxWidth) {
#ifdef DEBUG_kipp
          ListTag(stdout);
          printf(": adjusting width from %d to %d\n", computedWidth,
                 maxWidth);
#endif
          computedWidth = maxWidth;
        }
      }

      // Apply min/max values
      if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMaxWidth) {
        nscoord computedMaxWidth = aReflowState.mComputedMaxWidth +
          borderPadding.left + borderPadding.right;
        if (computedWidth > computedMaxWidth) {
          computedWidth = aReflowState.mComputedMaxWidth;
        }
      }
      if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMinWidth) {
        nscoord computedMinWidth = aReflowState.mComputedMinWidth +
          borderPadding.left + borderPadding.right;
        if (computedWidth < computedMinWidth) {
          computedWidth = computedMinWidth;
        }
      }
      aMetrics.width = computedWidth;
    }

    // Compute final height
    if (NS_UNCONSTRAINEDSIZE != aReflowState.computedHeight) {
      // Use style defined height
      aMetrics.height = borderPadding.top + aReflowState.computedHeight +
        borderPadding.bottom;

      // When style defines the height use it for the max-element-size
      // because we can't shrink any smaller.
      maxHeight = aMetrics.height;
    }
    else {
      nscoord autoHeight = aState.mY;

      // Shrink wrap our height around our contents.
      if (aState.mIsBottomMarginRoot) {
        // When we are a bottom-margin root make sure that our last
        // childs bottom margin is fully applied.
        // XXX check for a fit
        autoHeight += aState.mPrevBottomMargin;
      }
      autoHeight += borderPadding.bottom;

      // Apply min/max values
      if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMaxHeight) {
        nscoord computedMaxHeight = aReflowState.mComputedMaxHeight +
          borderPadding.top + borderPadding.bottom;
        if (autoHeight > computedMaxHeight) {
          autoHeight = computedMaxHeight;
        }
      }
      if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMinHeight) {
        nscoord computedMinHeight = aReflowState.mComputedMinHeight +
          borderPadding.top + borderPadding.bottom;
        if (autoHeight < computedMinHeight) {
          autoHeight = computedMinHeight;
        }
      }
      aMetrics.height = autoHeight;

      if (aState.mComputeMaxElementSize) {
        maxHeight = aState.mMaxElementSize.height +
          borderPadding.top + borderPadding.bottom;
      }
    }

    aMetrics.ascent = aMetrics.height;
    aMetrics.descent = 0;
    if (aState.mComputeMaxElementSize) {
      // Store away the final value
      aMetrics.maxElementSize->width = maxWidth;
      aMetrics.maxElementSize->height = maxHeight;
    }

    // Return bottom margin information
    aMetrics.mCarriedOutBottomMargin =
      aState.mIsBottomMarginRoot ? 0 : aState.mPrevBottomMargin;

#ifdef DEBUG
    if (CRAZY_WIDTH(aMetrics.width) || CRAZY_HEIGHT(aMetrics.height)) {
      ListTag(stdout);
      printf(": WARNING: desired:%d,%d\n", aMetrics.width, aMetrics.height);
    }
    if (aState.mComputeMaxElementSize &&
        ((maxWidth > aMetrics.width) || (maxHeight > aMetrics.height))) {
      ListTag(stdout);
      printf(": WARNING: max-element-size:%d,%d desired:%d,%d maxSize:%d,%d\n",
             maxWidth, maxHeight, aMetrics.width, aMetrics.height,
             aState.mReflowState.availableWidth,
             aState.mReflowState.availableHeight);
    }
#endif
#ifdef NOISY_MAX_ELEMENT_SIZE
    if (aState.mComputeMaxElementSize) {
      IndentBy(stdout, GetDepth());
      if (NS_UNCONSTRAINEDSIZE == aState.mReflowState.availableWidth) {
        printf("PASS1 ");
      }
      ListTag(stdout);
      printf(": max-element-size:%d,%d desired:%d,%d maxSize:%d,%d\n",
             maxWidth, maxHeight, aMetrics.width, aMetrics.height,
             aState.mReflowState.availableWidth,
             aState.mReflowState.availableHeight);
    }
#endif
  }

  // Compute the combined area of our children
  // XXX take into account the overflow->clip property!
// XXX_perf: This can be done incrementally
  nscoord x0 = 0, y0 = 0, x1 = aMetrics.width, y1 = aMetrics.height;
  if ((0 != aMetrics.height) || mFloaters.NotEmpty()) {
    nsLineBox* line = mLines;
    while (nsnull != line) {
      // Compute min and max x/y values for the reflowed frame's
      // combined areas
      nscoord x = line->mCombinedArea.x;
      nscoord y = line->mCombinedArea.y;
      nscoord xmost = x + line->mCombinedArea.width;
      nscoord ymost = y + line->mCombinedArea.height;
      if (x < x0) {
        x0 = x;
      }
      if (xmost > x1) {
        x1 = xmost;
      }
      if (y < y0) {
        y0 = y;
      }
      if (ymost > y1) {
        y1 = ymost;
      }
      line = line->mNext;
    }
  }
#ifdef NOISY_COMBINED_AREA
  IndentBy(stdout, GetDepth());
  ListTag(stdout);
  printf(": ca=%d,%d,%d,%d\n", x0, y0, x1-x0, y1-y0);
#endif

  // If the combined area of our children exceeds our bounding box
  // then set the NS_FRAME_OUTSIDE_CHILDREN flag, otherwise clear it.
  aMetrics.mCombinedArea.x = x0;
  aMetrics.mCombinedArea.y = y0;
  aMetrics.mCombinedArea.width = x1 - x0;
  aMetrics.mCombinedArea.height = y1 - y0;
  if ((aMetrics.mCombinedArea.x < 0) ||
      (aMetrics.mCombinedArea.y < 0) ||
      (aMetrics.mCombinedArea.XMost() > aMetrics.width) ||
      (aMetrics.mCombinedArea.YMost() > aMetrics.height)) {
    mState |= NS_FRAME_OUTSIDE_CHILDREN;
  }
  else {
    mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
  }
}

nsresult
nsBlockFrame::PrepareInitialReflow(nsBlockReflowState& aState)
{
  PrepareResizeReflow(aState);
  return NS_OK;
}

nsresult
nsBlockFrame::PrepareChildIncrementalReflow(nsBlockReflowState& aState)
{
  // If by chance we are inside a table, then give up and reflow
  // everything because we don't cache max-element-size information in
  // the lines.
  if (aState.mComputeMaxElementSize) {
    return PrepareResizeReflow(aState);
  }

  // Determine the line being impacted
  PRBool isFloater;
  nsLineBox* line = FindLineFor(aState.mNextRCFrame, isFloater);
  if (nsnull == line) {
    // This can't happen, but just in case it does...
    return PrepareResizeReflow(aState);
  }

  // XXX: temporary: If the child frame is a floater then punt
  if (isFloater) {
    return PrepareResizeReflow(aState);
  }

  // XXX need code for run-in/compact

  // Mark (at least) the affected line dirty.
  line->MarkDirty();
  if (aState.mNoWrap || line->IsBlock()) {
    // If we aren't wrapping then we know for certain that any changes
    // to a childs reflow can't affect the line that follows. This is
    // also true if the line is a block line.
  }
  else {
    // XXX: temporary: For now we are conservative and mark this line
    // and any inline lines that follow it dirty.
    line = line->mNext;
    while (nsnull != line) {
      if (line->IsBlock()) {
        break;
      }
      line->MarkDirty();
      line = line->mNext;
    }
  }
  return NS_OK;
}

void
nsBlockFrame::UpdateBulletPosition()
{
  if (nsnull == mBullet) {
    // Don't bother if there is no bullet
    return;
  }
  const nsStyleList* styleList;
  GetStyleData(eStyleStruct_List, (const nsStyleStruct*&) styleList);
  if (NS_STYLE_LIST_STYLE_POSITION_INSIDE == styleList->mListStylePosition) {
    if (HaveOutsideBullet()) {
      // We now have an inside bullet, but used to have an outside
      // bullet.  Adjust the frame lists and mark the first line
      // dirty.
      if (nsnull != mLines) {
        mBullet->SetNextSibling(mLines->mFirstChild);
        mLines->mFirstChild = mBullet;
        mLines->mChildCount++;
        mLines->MarkDirty();
      }
    }
    mState &= ~NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET;
  }
  else {
    if (!HaveOutsideBullet()) {
      // We now have an outside bullet, but used to have an inside
      // bullet. Take the bullet frame out of the first lines frame
      // list.
      if ((nsnull != mLines) && (mBullet == mLines->mFirstChild)) {
        nsIFrame* next;
        mBullet->GetNextSibling(&next);
        mBullet->SetNextSibling(nsnull);
        NS_ASSERTION(mLines->mChildCount > 0, "empty line w/o bullet");
        if (--mLines->mChildCount == 0) {
          nsLineBox* nextLine = mLines->mNext;
          delete mLines;
          mLines = nextLine;
          if (nsnull != nextLine) {
            nextLine->MarkDirty();
          }
        }
        else {
          mLines->mFirstChild = next;
          mLines->MarkDirty();
        }
      }
    }
    mState |= NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET;
  }
#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
}

nsresult
nsBlockFrame::PrepareStyleChangedReflow(nsBlockReflowState& aState)
{
  UpdateBulletPosition();
  // XXX temporary
  return PrepareResizeReflow(aState);
}

nsresult
nsBlockFrame::PrepareResizeReflow(nsBlockReflowState& aState)
{
  // Mark everything dirty
  nsLineBox* line = mLines;
  while (nsnull != line) {
    line->MarkDirty();
    line = line->mNext;
  }
  return NS_OK;
}

//----------------------------------------

nsLineBox*
nsBlockFrame::FindLineFor(nsIFrame* aFrame, PRBool& aIsFloaterResult)
{
  aIsFloaterResult = PR_FALSE;
  nsLineBox* line = mLines;
  while (nsnull != line) {
    if (line->Contains(aFrame)) {
      return line;
    }
    if (nsnull != line->mFloaters) {
      nsVoidArray& a = *line->mFloaters;
      PRInt32 i, n = a.Count();
      for (i = 0; i < n; i++) {
        nsPlaceholderFrame* ph = (nsPlaceholderFrame*) a[i];
        if (aFrame == ph->GetAnchoredItem()) {
          aIsFloaterResult = PR_TRUE;
          return line;
        }
      }
    }
    line = line->mNext;
  }
  return line;
}

void
nsBlockFrame::RecoverStateFrom(nsBlockReflowState& aState,
                               nsLineBox* aLine,
                               nscoord aDeltaY)
{
  PRBool applyTopMargin = PR_FALSE;
  if (aLine->IsBlock()) {
    nsIFrame* framePrevInFlow;
    aLine->mFirstChild->GetPrevInFlow(&framePrevInFlow);
    if (nsnull == framePrevInFlow) {
      applyTopMargin = ShouldApplyTopMargin(aState, aLine);
    }
  }

  aState.RecoverStateFrom(aLine, applyTopMargin, aDeltaY);
}

/**
 * Propogate reflow "damage" from the just reflowed line (aLine) to
 * any subsequent lines that were affected. The only thing that causes
 * damage is a change to the impact that floaters make.
 */
void
nsBlockFrame::PropogateReflowDamage(nsBlockReflowState& aState,
                                    nsLineBox* aLine,
                                    nscoord aDeltaY)
{
  if (aLine->mCombinedArea.YMost() > aLine->mBounds.YMost()) {
    // The line has an object that extends outside of its bounding box.
    nscoord impactY0 = aLine->mCombinedArea.y;
    nscoord impactY1 = aLine->mCombinedArea.YMost();
#ifdef NOISY_INCREMENTAL_REFLOW
    if (aState.mReflowState.reason == eReflowReason_Incremental) {
      IndentBy(stdout, gNoiseIndent);
      printf("impactY0=%d impactY1=%d deltaY=%d\n",
             impactY0, impactY1, aDeltaY);
    }
#endif

    // XXX Because we don't know what it is (it might be a floater; it
    // might be something that is just relatively positioned) we
    // *assume* that it's a floater and that lines that follow will
    // need reflowing.

    // Note: we cannot stop after the first non-intersecting line
    // because lines might be overlapping because of negative margins.
    nsLineBox* next = aLine->mNext;
    while (nsnull != next) {
      nscoord lineY0 = next->mBounds.y + aDeltaY;
      nscoord lineY1 = lineY0 + next->mBounds.height;
      if ((lineY0 < impactY1) && (impactY0 < lineY1)) {
#ifdef NOISY_INCREMENTAL_REFLOW
        if (aState.mReflowState.reason == eReflowReason_Incremental) {
          IndentBy(stdout, gNoiseIndent);
          printf("line=%p setting dirty\n", next);
        }
#endif
        next->MarkDirty();
      }
      next = next->mNext;
    }
  }
}

/**
 * Reflow the dirty lines
 */
nsresult
nsBlockFrame::ReflowDirtyLines(nsBlockReflowState& aState)
{
  nsresult rv = NS_OK;
  PRBool keepGoing = PR_TRUE;

  // Inform line layout of where the text runs are
  aState.mLineLayout->SetReflowTextRuns(mTextRuns);

#ifdef NOISY_INCREMENTAL_REFLOW
  if (aState.mReflowState.reason == eReflowReason_Incremental) {
    nsIReflowCommand::ReflowType type;
    aState.mReflowState.reflowCommand->GetType(type);
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": incrementally reflowing dirty lines: type=%s(%d)\n",
           kReflowCommandType[type], type);
    gNoiseIndent++;
  }
#endif

  // Reflow the lines that are already ours
  aState.mPrevLine = nsnull;
  nsLineBox* line = mLines;
  nscoord deltaY = 0;
  while (nsnull != line) {
#ifdef NOISY_INCREMENTAL_REFLOW
    if (aState.mReflowState.reason == eReflowReason_Incremental) {
      IndentBy(stdout, gNoiseIndent);
      printf("line=%p mY=%d dirty=%s oldBounds=%d,%d,%d,%d deltaY=%d\n",
             line, aState.mY, line->IsDirty() ? "yes" : "no",
             line->mBounds.x, line->mBounds.y,
             line->mBounds.width, line->mBounds.height,
             deltaY);
      gNoiseIndent++;
    }
#endif
    if (line->IsDirty()) {
      // Compute the dirty lines "before" YMost, after factoring in
      // the running deltaY value - the running value is implicit in
      // aState.mY.
      nscoord oldHeight = line->mBounds.height;

      // Reflow the dirty line
      rv = ReflowLine(aState, line, &keepGoing);
      if (NS_FAILED(rv)) {
        return rv;
      }
      DidReflowLine(aState, line, keepGoing);
      if (!keepGoing) {
        if (0 == line->ChildCount()) {
          DeleteLine(aState, line);
        }
        break;
      }
      nscoord newHeight = line->mBounds.height;
      deltaY += newHeight - oldHeight;

      // If the next line is clean then check and see if reflowing the
      // current line "damaged" the next line. Damage occurs when the
      // current line contains floaters that intrude upon the
      // subsequent lines.
      nsLineBox* next = line->mNext;
      if ((nsnull != next) && !next->IsDirty()) {
        PropogateReflowDamage(aState, line, deltaY);
      }
    }
    else {
      // XXX what if the slid line doesn't fit because we are in a
      // vertically constrained situation?
      // Recover state as if we reflowed this line
      RecoverStateFrom(aState, line, deltaY);
    }
#ifdef NOISY_INCREMENTAL_REFLOW
    if (aState.mReflowState.reason == eReflowReason_Incremental) {
      gNoiseIndent--;
      IndentBy(stdout, gNoiseIndent);
      printf("line=%p mY=%d newBounds=%d,%d,%d,%d deltaY=%d\n",
             line, aState.mY,
             line->mBounds.x, line->mBounds.y,
             line->mBounds.width, line->mBounds.height,
             deltaY);
    }
#endif

    // If this is an inline frame then its time to stop
    aState.mPrevLine = line;
    line = line->mNext;
    aState.mLineLayout->AdvanceToNextLine();
  }

  // Pull data from a next-in-flow if we can
  while (keepGoing && (nsnull != aState.mNextInFlow)) {
    // Grab first line from our next-in-flow
    line = aState.mNextInFlow->mLines;
    if (nsnull == line) {
      aState.mNextInFlow = (nsBlockFrame*) aState.mNextInFlow->mNextInFlow;
      continue;
    }
    // XXX See if the line is not dirty; if it's not maybe we can
    // avoid the pullup if it can't fit?
    aState.mNextInFlow->mLines = line->mNext;
    line->mNext = nsnull;
    if (0 == line->ChildCount()) {
      // The line is empty. Try the next one.
      NS_ASSERTION(nsnull == line->mFirstChild, "bad empty line");
      delete line;
      continue;
    }

    // XXX move to a subroutine: run-in, overflow, pullframe and this do this
    // Make the children in the line ours.
    nsIFrame* frame = line->mFirstChild;
    nsIFrame* lastFrame = nsnull;
    PRInt32 n = line->ChildCount();
    while (--n >= 0) {
      frame->SetParent(this);
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented
      nsHTMLContainerFrame::ReparentFrameView(frame, mNextInFlow, this);
      lastFrame = frame;
      frame->GetNextSibling(&frame);
    }
    lastFrame->SetNextSibling(nsnull);

    // Add line to our line list
    if (nsnull == aState.mPrevLine) {
      NS_ASSERTION(nsnull == mLines, "bad aState.mPrevLine");
      mLines = line;
    }
    else {
      NS_ASSERTION(nsnull == aState.mPrevLine->mNext, "bad aState.mPrevLine");
      aState.mPrevLine->mNext = line;
      aState.mPrevChild->SetNextSibling(line->mFirstChild);
    }

    // Now reflow it and any lines that it makes during it's reflow
    // (we have to loop here because reflowing the line may case a new
    // line to be created; see SplitLine's callers for examples of
    // when this happens).
    while (nsnull != line) {
      rv = ReflowLine(aState, line, &keepGoing);
      if (NS_FAILED(rv)) {
        return rv;
      }
      DidReflowLine(aState, line, keepGoing);
      if (!keepGoing) {
        if (0 == line->ChildCount()) {
          DeleteLine(aState, line);
        }
        break;
      }

      // If this is an inline frame then its time to stop
      aState.mPrevLine = line;
      line = line->mNext;
      aState.mLineLayout->AdvanceToNextLine();
    }
  }

#ifdef NOISY_INCREMENTAL_REFLOW
  if (aState.mReflowState.reason == eReflowReason_Incremental) {
    gNoiseIndent--;
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": done reflowing dirty lines (status=%x)\n",
           aState.mReflowStatus);
  }
#endif

  return rv;
}

void
nsBlockFrame::DeleteLine(nsBlockReflowState& aState,
                         nsLineBox* aLine)
{
  NS_PRECONDITION(0 == aLine->ChildCount(), "can't delete !empty line");
  if (0 == aLine->ChildCount()) {
    if (nsnull == aState.mPrevLine) {
      NS_ASSERTION(aLine == mLines, "huh");
      mLines = nsnull;
    }
    else {
      NS_ASSERTION(aState.mPrevLine->mNext == aLine, "bad prev-line");
      aState.mPrevLine->mNext = aLine->mNext;
    }
    delete aLine;
  }
}

/**
 * Reflow a line. The line will either contain a single block frame
 * or contain 1 or more inline frames. aLineReflowStatus indicates
 * whether or not the caller should continue to reflow more lines.
 */
nsresult
nsBlockFrame::ReflowLine(nsBlockReflowState& aState,
                         nsLineBox* aLine,
                         PRBool* aKeepReflowGoing)
{
  nsresult rv = NS_OK;

  // If the line is empty then first pull a frame into it so that we
  // know what kind of line it is (block or inline).
  if (0 == aLine->ChildCount()) {
#ifdef DEBUG_kipp
    printf("XXX: ");
    ListTag(stdout);
    printf(": huh? reflow-line with empty line!\n");
#endif
    nsIFrame* frame;
    rv = PullFrame(aState, aLine, frame);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (nsnull == frame) {
      *aKeepReflowGoing = PR_FALSE;
      return rv;
    }
  }

  // Setup the line-layout for the new line
  aState.mCurrentLine = aLine;
  aLine->ClearDirty();
  aLine->SetNeedDidReflow();

  // Now that we know what kind of line we have, reflow it
  if (aLine->IsBlock()) {
    NS_ASSERTION(nsnull == aLine->mFloaters, "bad line");
    rv = ReflowBlockFrame(aState, aLine, aKeepReflowGoing);
  }
  else {
    rv = ReflowInlineFrames(aState, aLine, aKeepReflowGoing);
  }

  return rv;
}

/**
 * Pull frame from the next available location (one of our lines or
 * one of our next-in-flows lines).
 */
nsresult
nsBlockFrame::PullFrame(nsBlockReflowState& aState,
                        nsLineBox* aLine,
                        nsIFrame*& aFrameResult)
{
  nsresult rv = NS_OK;
  PRBool stopPulling;
  aFrameResult = nsnull;

  // First check our remaining lines
  while (nsnull != aLine->mNext) {
    rv = PullFrame(aState, aLine, &aLine->mNext, PR_FALSE,
                   aFrameResult, stopPulling);
    if (NS_FAILED(rv) || stopPulling) {
      return rv;
    }
  }

  // Pull frames from the next-in-flow(s) until we can't
  nsBlockFrame* nextInFlow = aState.mNextInFlow;
  while (nsnull != nextInFlow) {
    nsLineBox* line = nextInFlow->mLines;
    if (nsnull == line) {
      nextInFlow = (nsBlockFrame*) nextInFlow->mNextInFlow;
      aState.mNextInFlow = nextInFlow;
      continue;
    }
    rv = PullFrame(aState, aLine, &nextInFlow->mLines, PR_TRUE,
                   aFrameResult, stopPulling);
    if (NS_FAILED(rv) || stopPulling) {
      return rv;
    }
  }
  return rv;
}

/**
 * Try to pull a frame out of a line pointed at by aFromList. If a
 * frame is pulled then aPulled will be set to PR_TRUE. In addition,
 * if aUpdateGeometricParent is set then the pulled frames geometric
 * parent will be updated (e.g. when pulling from a next-in-flows line
 * list).
 *
 * Note: pulling a frame from a line that is a place-holder frame
 * doesn't automatically remove the corresponding floater from the
 * line's floater array. This happens indirectly: either the line gets
 * emptied (and destroyed) or the line gets reflowed (because we mark
 * it dirty) and the code at the top of ReflowLine empties the
 * array. So eventually, it will be removed, just not right away.
 */
nsresult
nsBlockFrame::PullFrame(nsBlockReflowState& aState,
                        nsLineBox* aLine,
                        nsLineBox** aFromList,
                        PRBool aUpdateGeometricParent,
                        nsIFrame*& aFrameResult,
                        PRBool& aStopPulling)
{
  nsLineBox* fromLine = *aFromList;
  NS_ASSERTION(nsnull != fromLine, "bad line to pull from");
  if (0 == fromLine->ChildCount()) {
    // Discard empty lines immediately. Empty lines can happen here
    // because of DeleteChildsNextInFlow not being able to delete
    // lines. Don't stop pulling - there may be more frames around.
    *aFromList = fromLine->mNext;
    NS_ASSERTION(nsnull == fromLine->mFirstChild, "bad empty line");
    delete fromLine;
    aStopPulling = PR_FALSE;
    aFrameResult = nsnull;
  }
  else if ((0 != aLine->ChildCount()) && fromLine->IsBlock()) {
    // If our line is not empty and the child in aFromLine is a block
    // then we cannot pull up the frame into this line. In this case
    // we stop pulling.
    aStopPulling = PR_TRUE;
    aFrameResult = nsnull;
  }
  else {
    // Take frame from fromLine
    nsIFrame* frame = fromLine->mFirstChild;
    if (0 == aLine->mChildCount++) {
      aLine->mFirstChild = frame;
      aLine->SetIsBlock(fromLine->IsBlock());
      NS_ASSERTION(aLine->CheckIsBlock(), "bad line isBlock");
      NS_ASSERTION(nsnull == aLine->mFloaters, "bad line floaters");
    }
    if (0 != --fromLine->mChildCount) {
      NS_ASSERTION(fromLine->mChildCount < 10000, "bad line count");
      // Mark line dirty now that we pulled a child
      fromLine->MarkDirty();
      frame->GetNextSibling(&fromLine->mFirstChild);
    }
    else {
      // Free up the fromLine now that it's empty
      *aFromList = fromLine->mNext;
      delete fromLine;
    }

    // Change geometric parents
    if (aUpdateGeometricParent) {
      // Before we set the new parent frame get the current parent
      nsIFrame* oldParentFrame;
      frame->GetParent(&oldParentFrame);
      frame->SetParent(this);

      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented
      NS_ASSERTION(oldParentFrame != this, "unexpected parent frame");
      nsHTMLContainerFrame::ReparentFrameView(frame, oldParentFrame, this);
      
      // The frame is being pulled from a next-in-flow; therefore we
      // need to add it to our sibling list.
      if (nsnull != aState.mPrevChild) {
        aState.mPrevChild->SetNextSibling(frame);
      }
      frame->SetNextSibling(nsnull);
    }

    // Stop pulling because we found a frame to pull
    aStopPulling = PR_TRUE;
    aFrameResult = frame;
#ifdef DEBUG
    VerifyLines(PR_TRUE);
#endif
  }
  return NS_OK;
}

void
nsBlockFrame::DidReflowLine(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            PRBool aLineReflowStatus)
{
  // If the line no longer needs a floater array, get rid of it and
  // save some memory
  nsVoidArray* array = aLine->mFloaters;
  if (nsnull != array) {
    if (0 == array->Count()) {
      delete array;
      aLine->mFloaters = nsnull;
    }
    else {
      array->Compact();
    }
  }
}

void
nsBlockFrame::SlideLine(nsIPresContext* aPresContext,
                        nsISpaceManager* aSpaceManager,
                        nsLineBox* aLine, nscoord aDY)
{
#if 0
ListTag(stdout); printf(": SlideLine: line=%p dy=%d\n", aLine, aDY);
#endif
  // Adjust the Y coordinate of the frames in the line
  nsRect r;
  nsIFrame* kid = aLine->mFirstChild;
  PRInt32 n = aLine->ChildCount();
  while (--n >= 0) {
    kid->GetRect(r);
    r.y += aDY;
    kid->SetRect(r);

    // If the child has any floaters that impact the space manager,
    // slide them now.
    nsIHTMLReflow* ihr;
    if (NS_OK == kid->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
      ihr->MoveInSpaceManager(aPresContext, aSpaceManager, 0, aDY);
    }

    kid->GetNextSibling(&kid);
  }

  // Adjust line state
  aLine->mBounds.y += aDY;
  aLine->mCombinedArea.y += aDY;
}

void
nsBlockFrame::SlideFloaters(nsIPresContext* aPresContext,
                            nsISpaceManager* aSpaceManager,
                            nsLineBox* aLine, nscoord aDY)
{
  nsVoidArray* floaters = aLine->mFloaters;
  if (nsnull != floaters) {
    nsRect r;
    PRInt32 i, n = floaters->Count();
    for (i = 0; i < n; i++) {
      nsPlaceholderFrame* ph = (nsPlaceholderFrame*) floaters->ElementAt(i);
      nsIFrame* floater = ph->GetAnchoredItem();
      floater->GetRect(r);
      r.y += aDY;
      floater->SetRect(r);
    }
  }
}

NS_IMETHODIMP
nsBlockFrame::MoveInSpaceManager(nsIPresContext* aPresContext,
                                 nsISpaceManager* aSpaceManager,
                                 nscoord aDeltaX, nscoord aDeltaY)
{
#if 0
ListTag(stdout); printf(": MoveInSpaceManager: d=%d,%d\n", aDeltaX, aDeltaY);
#endif
  nsLineBox* line = mLines;
  while (nsnull != line) {
    // Move the floaters in the spacemanager
    nsVoidArray* floaters = line->mFloaters;
    if (nsnull != floaters) {
      PRInt32 i, n = floaters->Count();
      for (i = 0; i < n; i++) {
        nsPlaceholderFrame* ph = (nsPlaceholderFrame*) floaters->ElementAt(i);
        nsIFrame* floater = ph->GetAnchoredItem();
        aSpaceManager->OffsetRegion(floater, aDeltaX, aDeltaY);
#if 0
((nsFrame*)kid)->ListTag(stdout); printf(": offset=%d,%d\n", aDeltaX, aDeltaY);
#endif
      }
    }

    // Tell kids about the move too
    PRInt32 n = line->ChildCount();
    nsIFrame* kid = line->mFirstChild;
    while (--n >= 0) {
      nsIHTMLReflow* ihr;
      if (NS_OK == kid->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
        ihr->MoveInSpaceManager(aPresContext, aSpaceManager, aDeltaX, aDeltaY);
      }
      kid->GetNextSibling(&kid);
    }
    
    line = line->mNext;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsBlockFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent*     aChild,
                               nsIAtom*        aAttribute,
                               PRInt32         aHint)
{
  nsresult rv = nsBlockFrameSuper::AttributeChanged(aPresContext, aChild,
                                                    aAttribute, aHint);

  if (NS_OK != rv) {
    return rv;
  }
  if (nsHTMLAtoms::start == aAttribute) {
    // XXX Not sure if this is necessary anymore
    RenumberLists();

    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                 nsIReflowCommand::ContentChanged,
                                 nsnull,
                                 aAttribute);
    if (NS_SUCCEEDED(rv)) {
      shell->AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }
  else if (nsHTMLAtoms::value == aAttribute) {
    const nsStyleDisplay* styleDisplay;
    GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
    if (NS_STYLE_DISPLAY_LIST_ITEM == styleDisplay->mDisplay) {
      nsIFrame* nextAncestor = mParent;
      nsBlockFrame* blockParent = nsnull;
      
      // Search for the closest ancestor that's a block frame. We
      // make the assumption that all related list items share a
      // common block parent.
      while (nextAncestor != nsnull) {
        if (NS_OK == nextAncestor->QueryInterface(kBlockFrameCID, 
                                                  (void**)&blockParent)) {
          break;
        }
        nextAncestor->GetParent(&nextAncestor);
      }

      // Tell the enclosing block frame to renumber list items within
      // itself
      if (nsnull != blockParent) {
        // XXX Not sure if this is necessary anymore
        blockParent->RenumberLists();

        nsCOMPtr<nsIPresShell> shell;
        aPresContext->GetShell(getter_AddRefs(shell));
        
        nsIReflowCommand* reflowCmd;
        rv = NS_NewHTMLReflowCommand(&reflowCmd, blockParent,
                                     nsIReflowCommand::ContentChanged,
                                     nsnull,
                                     aAttribute);
        if (NS_SUCCEEDED(rv)) {
          shell->AppendReflowCommand(reflowCmd);
          NS_RELEASE(reflowCmd);
        }
      }
    }
  }

  return rv;
}

nsBlockFrame*
nsBlockFrame::FindFollowingBlockFrame(nsIFrame* aFrame)
{
  nsBlockFrame* followingBlockFrame = nsnull;
  nsIFrame* frame = aFrame;
  for (;;) {
    nsIFrame* nextFrame;
    frame->GetNextSibling(&nextFrame);
    if (nsnull != nextFrame) {
      const nsStyleDisplay* display;
      nextFrame->GetStyleData(eStyleStruct_Display,
                              (const nsStyleStruct*&) display);
      if (NS_STYLE_DISPLAY_BLOCK == display->mDisplay) {
        followingBlockFrame = (nsBlockFrame*) nextFrame;
        break;
      }
      else if (NS_STYLE_DISPLAY_INLINE == display->mDisplay) {
        // If it's a text-frame and it's just whitespace and we are
        // in a normal whitespace situation THEN skip it and keep
        // going...
        // XXX WRITE ME!
      }
      frame = nextFrame;
    }
    else
      break;
  }
  return followingBlockFrame;
}

PRBool
nsBlockFrame::ShouldApplyTopMargin(nsBlockReflowState& aState,
                                   nsLineBox* aLine)
{
  if (aState.mApplyTopMargin) {
    // Apply short-circuit check to avoid searching the line list
    return PR_TRUE;
  }

  if (!aState.IsAdjacentWithTop()) {
    // If we aren't at the top Y coordinate then something of non-zero
    // height must have been placed. Therefore the childs top-margin
    // applies.
    aState.mApplyTopMargin = PR_TRUE;
    return PR_TRUE;
  }

  // Determine if this line is "essentially" the first line
  nsLineBox* line = mLines;
  while (line != aLine) {
    if ((nsnull != line->mFloaters) && (0 != line->mFloaters->Count())) {
      // A line which preceeds aLine is not empty therefore the top
      // margin applies.
      aState.mApplyTopMargin = PR_TRUE;
      return PR_TRUE;
    }
    if (line->IsBlock()) {
      // A line which preceeds aLine contains a block; therefore the
      // top margin applies.
      aState.mApplyTopMargin = PR_TRUE;
      return PR_TRUE;
    }
    line = line->mNext;
  }

  // The line being reflowed is "essentially" the first line in the
  // block. Therefore its top-margin will be collapsed by the
  // generational collapsing logic with its parent (us).
  return PR_FALSE;
}

nsIFrame*
nsBlockFrame::GetTopBlockChild()
{
  nsIFrame* firstChild = mLines ? mLines->mFirstChild : nsnull;
  if (firstChild) {
    if (mLines->IsBlock()) {
      // Winner
      return firstChild;
    }

    // If the first line is not a block line then the second line must
    // be a block line otherwise the top child can't be a block.
    nsLineBox* next = mLines->mNext;
    if ((nsnull == next) || !next->IsBlock()) {
      // There is no line after the first line or its not a block so
      // don't bother trying to skip over the first line.
      return nsnull;
    }

    // The only time we can skip over the first line and pretend its
    // not there is if the line contains only compressed
    // whitespace. If white-space is significant to this frame then we
    // can't skip over the line.
    const nsStyleText* styleText;
    GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&) styleText);
    if ((NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace) ||
        (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace)) {
      // Whitespace is significant
      return nsnull;
    }

    // See if each frame is a text frame that contains nothing but
    // whitespace.
    PRInt32 n = mLines->mChildCount;
    while (--n >= 0) {
      nsIContent* content;
      nsresult rv = firstChild->GetContent(&content);
      if (NS_FAILED(rv) || (nsnull == content)) {
        return nsnull;
      }
      nsITextContent* tc;
      rv = content->QueryInterface(kITextContentIID, (void**) &tc);
      NS_RELEASE(content);
      if (NS_FAILED(rv) || (nsnull == tc)) {
        return nsnull;
      }
      PRBool isws = PR_FALSE;
      tc->IsOnlyWhitespace(&isws);
      NS_RELEASE(tc);
      if (!isws) {
        return nsnull;
      }
      firstChild->GetNextSibling(&firstChild);
    }

    // If we make it to this point then every frame on the first line
    // was compressible white-space. Since we already know that the
    // second line contains a block, that block is the
    // top-block-child.
    return next->mFirstChild;
  }

  return nsnull;
}

nsresult
nsBlockFrame::ReflowBlockFrame(nsBlockReflowState& aState,
                               nsLineBox* aLine,
                               PRBool* aKeepReflowGoing)
{
  NS_PRECONDITION(*aKeepReflowGoing, "bad caller");

  nsresult rv = NS_OK;

  nsIFrame* frame = aLine->mFirstChild;

  // When reflowing a block frame we always get the available space
  aState.GetAvailableSpace();

  // Prepare the block reflow engine
  const nsStyleDisplay* display;
  frame->GetStyleData(eStyleStruct_Display,
                      (const nsStyleStruct*&) display);
  nsBlockReflowContext brc(aState.mPresContext, aState.mReflowState,
                           aState.mComputeMaxElementSize);
  brc.SetNextRCFrame(aState.mNextRCFrame);

  // See if we should apply the top margin. If the block frame being
  // reflowed is a continuation (non-null prev-in-flow) then we don't
  // apply its top margin because its not significant. Otherwise, dig
  // deeper.
  PRBool applyTopMargin = PR_FALSE;
  nsIFrame* framePrevInFlow;
  frame->GetPrevInFlow(&framePrevInFlow);
  if (nsnull == framePrevInFlow) {
    applyTopMargin = ShouldApplyTopMargin(aState, aLine);
  }

  // Clear past floaters before the block if the clear style is not none
  aLine->mBreakType = display->mBreakType;
  if (NS_STYLE_CLEAR_NONE != aLine->mBreakType) {
    applyTopMargin = aState.ClearPastFloaters(aLine->mBreakType);
  }

  // Compute the available space for the block
  nsSplittableType splitType = NS_FRAME_NOT_SPLITTABLE;
  frame->IsSplittable(splitType);
  nsRect availSpace;
  aState.ComputeBlockAvailSpace(splitType, availSpace);

  // Reflow the block into the available space
  nsReflowStatus frameReflowStatus;
  nsMargin computedOffsets;
  rv = brc.ReflowBlock(frame, availSpace, applyTopMargin,
                       aState.mPrevBottomMargin, aState.IsAdjacentWithTop(),
                       computedOffsets, frameReflowStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aState.mPrevChild = frame;

#if defined(REFLOW_STATUS_COVERAGE)
  RecordReflowStatus(PR_TRUE, frameReflowStatus);
#endif

  if (NS_INLINE_IS_BREAK_BEFORE(frameReflowStatus)) {
    // None of the child block fits.
    PushLines(aState);
    *aKeepReflowGoing = PR_FALSE;
    aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
  }
  else {
    // Note: line-break-after a block is a nop

    // Try to place the child block
    PRBool isAdjacentWithTop = aState.IsAdjacentWithTop();
    nscoord collapsedBottomMargin;
    *aKeepReflowGoing = brc.PlaceBlock(isAdjacentWithTop, computedOffsets,
                                       &collapsedBottomMargin,
                                       aLine->mBounds, aLine->mCombinedArea);
    if (*aKeepReflowGoing) {
      // Some of the child block fit

      // Advance to new Y position
      nscoord newY = aLine->mBounds.YMost();
      aState.mY = newY;
      aLine->mCarriedOutBottomMargin = brc.GetCarriedOutBottomMargin();

      // Continue the block frame now if it didn't completely fit in
      // the available space.
      if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus)) {
        PRBool madeContinuation;
        rv = CreateContinuationFor(aState, aLine, frame, madeContinuation);
        if (NS_FAILED(rv)) {
          return rv;
        }

        // Push continuation to a new line, but only if we actually
        // made one.
        if (madeContinuation) {
          frame->GetNextSibling(&frame);
          nsLineBox* line = new nsLineBox(frame, 1, LINE_IS_BLOCK);
          if (nsnull == line) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          line->mNext = aLine->mNext;
          aLine->mNext = line;

          // Do not count the continuation child on the line it used
          // to be on
          aLine->mChildCount--;
          NS_ASSERTION(aLine->mChildCount < 10000, "bad line count"); 
       }

        // Advance to next line since some of the block fit. That way
        // only the following lines will be pushed.
        aState.mPrevLine = aLine;
        PushLines(aState);
        aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
        *aKeepReflowGoing = PR_FALSE;

        // The bottom margin for a block is only applied on the last
        // flow block. Since we just continued the child block frame,
        // we know that line->mFirstChild is not the last flow block
        // therefore zero out the running margin value.
        aState.mPrevBottomMargin = 0;
      }
      else {
        aState.mPrevBottomMargin = collapsedBottomMargin;
      }
#ifdef NOISY_VERTICAL_MARGINS
      ListTag(stdout);
      printf(": frame=");
      nsFrame::ListTag(stdout, frame);
      printf(" carriedOutBottomMargin=%d collapsedBottomMargin=%d => %d\n",
             aLine->mCarriedOutBottomMargin, collapsedBottomMargin,
             aState.mPrevBottomMargin);
#endif

      // Post-process the "line"
      nsSize maxElementSize(0, 0);
      if (aState.mComputeMaxElementSize) {
        maxElementSize = brc.GetMaxElementSize();
        if ((0 != aState.mBand.GetFloaterCount()) &&
            (NS_FRAME_SPLITTABLE_NON_RECTANGULAR != splitType)) {
          // Add in floater impacts to the lines max-element-size, but
          // only if the block element isn't one of us (otherwise the
          // floater impacts will be counted twice).
          ComputeLineMaxElementSize(aState, aLine, &maxElementSize);
        }
      }
      PostPlaceLine(aState, aLine, maxElementSize);

      // Place the "marker" (bullet) frame.
      //
      // According to the CSS2 spec, section 12.6.1, the "marker" box
      // participates in the height calculation of the list-item box's
      // first line box.
      //
      // There are exactly two places a bullet can be placed: near the
      // first or second line. Its only placed on the second line in a
      // rare case: an empty first line followed by a second line that
      // contains a block (example: <LI>\n<P>... ). This is where
      // the second case can happen.
      if (HaveOutsideBullet() &&
          ((aLine == mLines) ||
           ((0 == mLines->mBounds.height) && (aLine == mLines->mNext)))) {
        // Reflow the bullet
        nsHTMLReflowMetrics metrics(nsnull);
        ReflowBullet(aState, metrics);

        // For bullets that are placed next to a child block, there will
        // be no correct ascent value. Therefore, make one up...
        nscoord ascent = 0;
        const nsStyleFont* font;
        nsresult rv;
        rv = frame->GetStyleData(eStyleStruct_Font,
                                 (const nsStyleStruct*&) font);
        if (NS_SUCCEEDED(rv) && (nsnull != font)) {
          nsIRenderingContext& rc = *aState.mReflowState.rendContext;
          rc.SetFont(font->mFont);
          nsIFontMetrics* fm;
          rv = rc.GetFontMetrics(fm);
          if (NS_SUCCEEDED(rv) && (nsnull != fm)) {
            fm->GetMaxAscent(ascent);
            NS_RELEASE(fm);
          }
        }

        // Tall bullets won't look particularly nice here...
        nsRect bbox;
        mBullet->GetRect(bbox);
        nscoord topMargin = applyTopMargin ? collapsedBottomMargin : 0;
        bbox.y = aState.BorderPadding().top + ascent -
          metrics.ascent + topMargin;
        mBullet->SetRect(bbox);
      }
    }
    else {
      // None of the block fits. Determine the correct reflow status.
      if (aLine == mLines) {
        // If it's our very first line then we need to be pushed to
        // our parents next-in-flow. Therefore, return break-before
        // status for our reflow status.
        aState.mReflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
      }
      else {
        // Push the line that didn't fit and any lines that follow it
        // to our next-in-flow.
        PushLines(aState);
        aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
      }
    }
  }
#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
  return rv;
}

#define LINE_REFLOW_OK   0
#define LINE_REFLOW_STOP 1
#define LINE_REFLOW_REDO 2

nsresult
nsBlockFrame::ReflowInlineFrames(nsBlockReflowState& aState,
                                 nsLineBox* aLine,
                                 PRBool* aKeepReflowGoing)
{
  nsresult rv = NS_OK;
  *aKeepReflowGoing = PR_TRUE;

#ifdef DEBUG
  PRInt32 spins = 0;
#endif
  for (;;) {
    if (nsnull != aLine->mFloaters) {
      // Forget all of the floaters on the line
      aLine->mFloaters->Clear();
    }
    aState.mFloaterCombinedArea.SetRect(0, 0, 0, 0);
    aState.mPendingFloaters.Clear();

    // Setup initial coordinate system for reflowing the inline frames
    // into. Apply a previous block frame's bottom margin first.
    aState.mY += aState.mPrevBottomMargin;
    aState.GetAvailableSpace();

    const nsMargin& borderPadding = aState.BorderPadding();
    nscoord x = aState.mAvailSpaceRect.x + borderPadding.left;
    nscoord availWidth = aState.mAvailSpaceRect.width;
    nscoord availHeight;
    if (aState.mUnconstrainedHeight) {
      availHeight = NS_UNCONSTRAINEDSIZE;
    }
    else {
      /* XXX get the height right! */
      availHeight = aState.mAvailSpaceRect.height;
    }
    PRBool impactedByFloaters = 0 != aState.mBand.GetFloaterCount();
    nsLineLayout* lineLayout = aState.mLineLayout;
    lineLayout->BeginLineReflow(x, aState.mY,
                                availWidth, availHeight,
                                impactedByFloaters,
                                PR_FALSE /*XXX isTopOfPage*/);

    // Reflow the frames that are already on the line first
    PRUint8 lineReflowStatus = LINE_REFLOW_OK;
    PRInt32 i;
    nsIFrame* frame = aLine->mFirstChild;
    for (i = 0; i < aLine->ChildCount(); i++) {
      rv = ReflowInlineFrame(aState, aLine, frame, &lineReflowStatus);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (LINE_REFLOW_OK != lineReflowStatus) {
        // It is possible that one or more of next lines are empty
        // (because of DeleteChildsNextInFlow). If so, delete them now
        // in case we are finished.
        nsLineBox* nextLine = aLine->mNext;
        while ((nsnull != nextLine) && (0 == nextLine->ChildCount())) {
          // XXX Is this still necessary now that DeleteChildsNextInFlow
          // uses DoRemoveFrame?
          aLine->mNext = nextLine->mNext;
          NS_ASSERTION(nsnull == nextLine->mFirstChild, "bad empty line");
          delete nextLine;
          nextLine = aLine->mNext;
        }
        break;
      }
      frame->GetNextSibling(&frame);
    }

    // Pull frames and reflow them until we can't
    while (LINE_REFLOW_OK == lineReflowStatus) {
      nsIFrame* frame;
      rv = PullFrame(aState, aLine, frame);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (nsnull == frame) {
        break;
      }
      while (LINE_REFLOW_OK == lineReflowStatus) {
        PRInt32 oldCount = aLine->ChildCount();
        rv = ReflowInlineFrame(aState, aLine, frame, &lineReflowStatus);
        if (NS_FAILED(rv)) {
          return rv;
        }
        if (aLine->ChildCount() != oldCount) {
          // We just created a continuation for aFrame AND its going
          // to end up on this line (e.g. :first-letter
          // situation). Therefore we have to loop here before trying
          // to pull another frame.
          frame->GetNextSibling(&frame);
        }
        else {
          break;
        }
      }
    }
    if (LINE_REFLOW_REDO == lineReflowStatus) {
      // This happens only when we have a line that is impacted by
      // floaters and the first element in the line doesn't fit with
      // the floaters.
      //
      // What we do is to advance past the first floater we find and
      // then reflow the line all over again.
      NS_ASSERTION(aState.mBand.GetFloaterCount(),
                   "redo line on totally empty line");
      NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aState.mAvailSpaceRect.height,
                   "unconstrained height on totally empty line");
      aState.mY += aState.mAvailSpaceRect.height;
      // XXX: a small optimization can be done here when paginating:
      // if the new Y coordinate is past the end of the block then
      // push the line and return now instead of later on after we are
      // past the floater.
      lineLayout->EndLineReflow();
#ifdef DEBUG
      spins++;
      if (1000 == spins) {
        ListTag(stdout);
        printf(": yikes! spinning on a line over 1000 times!\n");
        NS_ABORT();
      }
#endif
      continue;
    }

    // If we are propogating out a break-before status then there is
    // no point in placing the line.
    NS_ASSERTION(aLine->mChildCount < 10000, "bad line child count");
    if (NS_INLINE_IS_BREAK_BEFORE(aState.mReflowStatus)) {
      lineLayout->EndLineReflow();
    }
    else {
      rv = PlaceLine(aState, aLine, aKeepReflowGoing);
    }
    break;
  }

  return rv;
}

/**
 * Reflow an inline frame. The reflow status is mapped from the frames
 * reflow status to the lines reflow status (not to our reflow status).
 * The line reflow status is simple: PR_TRUE means keep placing frames
 * on the line; PR_FALSE means don't (the line is done). If the line
 * has some sort of breaking affect then aLine->mBreakType will be set
 * to something other than NS_STYLE_CLEAR_NONE.
 */
nsresult
nsBlockFrame::ReflowInlineFrame(nsBlockReflowState& aState,
                                nsLineBox* aLine,
                                nsIFrame* aFrame,
                                PRUint8* aLineReflowStatus)
{
  *aLineReflowStatus = LINE_REFLOW_OK;

  // If it's currently ok to be reflowing in first-letter style then
  // we must be about to reflow a frame that has first-letter style.
  PRBool reflowingFirstLetter = aState.mLineLayout->GetFirstLetterStyleOK();

  // Reflow the inline frame
  nsLineLayout* lineLayout = aState.mLineLayout;
  nsReflowStatus frameReflowStatus;
  nsresult rv = lineLayout->ReflowFrame(aFrame, &aState.mNextRCFrame,
                                        frameReflowStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }
#ifdef REALLY_NOISY_REFLOW_CHILD
  nsFrame::ListTag(stdout, aFrame);
  printf(": status=%x\n", frameReflowStatus);
#endif

#if defined(REFLOW_STATUS_COVERAGE)
  RecordReflowStatus(PR_FALSE, frameReflowStatus);
#endif

  // Send post-reflow notification
  aState.mPrevChild = aFrame;

  // Process the child frames reflow status. There are 5 cases:
  // complete, not-complete, break-before, break-after-complete,
  // break-after-not-complete. There are two situations: we are a
  // block or we are an inline. This makes a total of 10 cases
  // (fortunately, there is some overlap).
  aLine->mBreakType = NS_STYLE_CLEAR_NONE;
  if (NS_INLINE_IS_BREAK(frameReflowStatus)) {
    // Always abort the line reflow (because a line break is the
    // minimal amount of break we do).
    *aLineReflowStatus = LINE_REFLOW_STOP;

    // XXX what should aLine->mBreakType be set to in all these cases?
    PRUint8 breakType = NS_INLINE_GET_BREAK_TYPE(frameReflowStatus);
    NS_ASSERTION(breakType != NS_STYLE_CLEAR_NONE, "bad break type");
    NS_ASSERTION(NS_STYLE_CLEAR_PAGE != breakType, "no page breaks yet");

    if (NS_INLINE_IS_BREAK_BEFORE(frameReflowStatus)) {
      // Break-before cases.
      if (aFrame == aLine->mFirstChild) {
        // If we break before the first frame on the line then we must
        // be trying to place content where theres no room (e.g. on a
        // line with wide floaters). Inform the caller to reflow the
        // line after skipping past a floater.
        *aLineReflowStatus = LINE_REFLOW_REDO;
      }
      else {
        // It's not the first child on this line so go ahead and split
        // the line. We will see the frame again on the next-line.
        rv = SplitLine(aState, aLine, aFrame);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
    }
    else {
      // Break-after cases
      aLine->mBreakType = breakType;
      if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus)) {
        // Create a continuation for the incomplete frame. Note that the
        // frame may already have a continuation.
        PRBool madeContinuation;
        rv = CreateContinuationFor(aState, aLine, aFrame, madeContinuation);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }

      // Split line, but after the frame just reflowed
      nsIFrame* nextFrame;
      aFrame->GetNextSibling(&nextFrame);
      rv = SplitLine(aState, aLine, nextFrame);
      if (NS_FAILED(rv)) {
        return rv;
      }

      // Mark next line dirty in case SplitLine didn't end up
      // pushing any frames.
      nsLineBox* next = aLine->mNext;
      if ((nsnull != next) && !next->IsBlock()) {
        next->MarkDirty();
      }
    }
  }
  else if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus)) {
    // Frame is not-complete, no special breaking status

    // Create a continuation for the incomplete frame. Note that the
    // frame may already have a continuation.
    PRBool madeContinuation;
    rv = CreateContinuationFor(aState, aLine, aFrame, madeContinuation);
    if (NS_FAILED(rv)) {
      return rv;
    }

    PRBool needSplit = PR_FALSE;
    if (!reflowingFirstLetter) {
      needSplit = PR_TRUE;
    }
    else {
    }

    if (needSplit) {
      // Split line after the current frame
      *aLineReflowStatus = LINE_REFLOW_STOP;
      aFrame->GetNextSibling(&aFrame);
      rv = SplitLine(aState, aLine, aFrame);
      if (NS_FAILED(rv)) {
        return rv;
      }
      // Mark next line dirty in case SplitLine didn't end up
      // pushing any frames.
      nsLineBox* next = aLine->mNext;
      if ((nsnull != next) && !next->IsBlock()) {
        next->MarkDirty();
      }
    }
  }

  return NS_OK;
}

/**
 * Create a continuation, if necessary, for aFrame. Place it on the
 * same line that aFrame is on. Set aMadeNewFrame to PR_TRUE if a
 * new frame is created.
 */
nsresult
nsBlockFrame::CreateContinuationFor(nsBlockReflowState& aState,
                                    nsLineBox* aLine,
                                    nsIFrame* aFrame,
                                    PRBool& aMadeNewFrame)
{
  aMadeNewFrame = PR_FALSE;
  nsresult rv;
  nsIFrame* nextInFlow;
  rv = CreateNextInFlow(*aState.mPresContext, this, aFrame, nextInFlow);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsnull != nextInFlow) {
    aMadeNewFrame = PR_TRUE;
    aLine->mChildCount++;
  }
#ifdef DEBUG
  VerifyLines(PR_FALSE);
#endif
  return rv;
}

nsresult
nsBlockFrame::SplitLine(nsBlockReflowState& aState,
                        nsLineBox* aLine,
                        nsIFrame* aFrame)
{
  nsLineLayout* lineLayout = aState.mLineLayout;
  PRInt32 pushCount = aLine->ChildCount() - lineLayout->GetCurrentSpanCount();
  NS_ASSERTION(pushCount >= 0, "bad push count"); 
//printf("BEFORE (pushCount=%d):\n", pushCount);
//aLine->List(stdout, 0);
  if (0 != pushCount) {
    NS_ASSERTION(aLine->ChildCount() > pushCount, "bad push");
    NS_ASSERTION(nsnull != aFrame, "whoops");
    nsLineBox* to = aLine->mNext;
    if (nsnull != to) {
      // Only push into the next line if it's empty; otherwise we can
      // end up pushing a frame which is continued into the same frame
      // as it's continuation. This causes all sorts of bad side
      // effects so we don't allow it.
      if (0 != to->ChildCount()) {
        nsLineBox* insertedLine = new nsLineBox(aFrame, pushCount, 0);
        if (nsnull == insertedLine) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        aLine->mNext = insertedLine;
        insertedLine->mNext = to;
        to = insertedLine;
      } else {
        to->mFirstChild = aFrame;
        to->mChildCount = pushCount;
        to->MarkDirty();
      }
    } else {
      to = new nsLineBox(aFrame, pushCount, 0);
      if (nsnull == to) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      aLine->mNext = to;
    }
    to->SetIsBlock(aLine->IsBlock());
    to->SetIsFirstLine(aLine->IsFirstLine());
    aLine->mChildCount -= pushCount;

    // Let line layout know that some frames are no longer part of its
    // state.
    if (!aLine->IsBlock()) {
      lineLayout->SplitLineTo(aLine->ChildCount());
    }
#ifdef DEBUG
    VerifyLines(PR_TRUE);
#endif
  }
  return NS_OK;
}

PRBool
nsBlockFrame::ShouldJustifyLine(nsBlockReflowState& aState, nsLineBox* aLine)
{
  nsLineBox* next = aLine->mNext;
  while (nsnull != next) {
    // There is another line
    if (0 != next->ChildCount()) {
      // If the next line is a block line then we must not justify
      // this line because it means that this line is the last in a
      // group of inline lines.
      return !next->IsBlock();
    }

    // The next line is empty, try the next one
    next = next->mNext;
  }

  // XXX Not sure about this part
  // Try our next-in-flows lines to answer the question
  nsBlockFrame* nextInFlow = (nsBlockFrame*) mNextInFlow;
  while (nsnull != nextInFlow) {
    nsLineBox* line = nextInFlow->mLines;
    while (nsnull != line) {
      if (0 != line->ChildCount()) {
        return !line->IsBlock();
      }
      line = line->mNext;
    }
    nextInFlow = (nsBlockFrame*) nextInFlow->mNextInFlow;
  }

  // This is the last line - so don't allow justification
  return PR_FALSE;
}

nsresult
nsBlockFrame::PlaceLine(nsBlockReflowState& aState,
                        nsLineBox* aLine,
                        PRBool* aKeepReflowGoing)
{
  nsresult rv = NS_OK;

  // Vertically align the frames on this line.
  //
  // According to the CSS2 spec, section 12.6.1, the "marker" box
  // participates in the height calculation of the list-item box's
  // first line box.
  //
  // There are exactly two places a bullet can be placed: near the
  // first or second line. Its only placed on the second line in a
  // rare case: an empty first line followed by a second line that
  // contains a block (example: <LI>\n<P>... ).
  //
  // For this code, only the first case is possible because this
  // method is used for placing a line of inline frames. If the rare
  // case is happening then the worst that will happen is that the
  // bullet frame will be reflowed twice.
  nsLineLayout* lineLayout = aState.mLineLayout;
  PRBool addedBullet = PR_FALSE;
  if (HaveOutsideBullet() && (aLine == mLines) &&
      !lineLayout->IsZeroHeight()) {
    nsHTMLReflowMetrics metrics(nsnull);
    ReflowBullet(aState, metrics);
    lineLayout->AddBulletFrame(mBullet, metrics);
    addedBullet = PR_TRUE;
  }
  nsSize maxElementSize;
  lineLayout->VerticalAlignFrames(aLine->mBounds, maxElementSize);
#ifdef DEBUG
  if (CRAZY_HEIGHT(aLine->mBounds.y)) {
    nsFrame::ListTag(stdout);
    printf(": line=%p y=%d line.bounds.height=%d\n",
           aLine, aLine->mBounds.y, aLine->mBounds.height);
  }
#endif

  // Only block frames horizontally align their children because
  // inline frames "shrink-wrap" around their children (therefore
  // there is no extra horizontal space).
#if XXX_fix_me
  PRBool allowJustify = PR_TRUE;
  if (NS_STYLE_TEXT_ALIGN_JUSTIFY == aState.mStyleText->mTextAlign) {
    allowJustify = ShouldJustifyLine(aState, aLine);
  }
#else
  PRBool allowJustify = PR_FALSE;
#endif
  lineLayout->TrimTrailingWhiteSpace(aLine->mBounds);
  lineLayout->HorizontalAlignFrames(aLine->mBounds, allowJustify);
  lineLayout->RelativePositionFrames(aLine->mCombinedArea);
  if (addedBullet) {
    lineLayout->RemoveBulletFrame(mBullet);
  }

  // Inline lines do not have margins themselves; however they are
  // impacted by prior block margins. If this line ends up having some
  // height then we zero out the previous bottom margin value that was
  // already applied to the line's starting Y coordinate. Otherwise we
  // leave it be so that the previous blocks bottom margin can be
  // collapsed with a block that follows.
  nscoord newY;
  if (aLine->mBounds.height > 0) {
    // This line has some height. Therefore the application of the
    // previous-bottom-margin should stick.
    aState.mPrevBottomMargin = 0;
    newY = aLine->mBounds.YMost();
  }
  else {
    // Don't let the previous-bottom-margin value affect the newY
    // coordinate (it was applied in ReflowInlineFrames speculatively)
    // since the line is empty.
    nscoord dy = -aState.mPrevBottomMargin;
    newY = aState.mY + dy;
    aLine->mCombinedArea.y += dy;
    aLine->mBounds.y += dy;
  }
  aLine->mCarriedOutBottomMargin = 0;/* XXX_ib */

  // See if the line fit. If it doesn't we need to push it. Our first
  // line will always fit.
  if ((mLines != aLine) && (newY > aState.mBottomEdge)) {
    // Push this line and all of it's children and anything else that
    // follows to our next-in-flow
    PushLines(aState);

    // Stop reflow and whack the reflow status if reflow hasn't
    // already been stopped.
    if (*aKeepReflowGoing) {
      NS_ASSERTION(NS_FRAME_COMPLETE == aState.mReflowStatus,
                   "lost reflow status");
      aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
      *aKeepReflowGoing = PR_FALSE;
    }
    lineLayout->EndLineReflow();
    return rv;
  }

  aState.mY = newY;
  if (aState.mComputeMaxElementSize) {
    if (0 != aState.mBand.GetFloaterCount()) {
      // Add in floater impacts to the lines max-element-size
      ComputeLineMaxElementSize(aState, aLine, &maxElementSize);
    }
  }
  PostPlaceLine(aState, aLine, maxElementSize);

  // Any below current line floaters to place?
  if (0 != aState.mPendingFloaters.Count()) {
    // Now is the time that we reflow them and update the computed
    // line combined area.
    aState.PlaceBelowCurrentLineFloaters(&aState.mPendingFloaters, PR_TRUE);
    aState.mPendingFloaters.Clear();
  }

  // When a line has floaters, factor them into the combined-area
  // computations.
  if (nsnull != aLine->mFloaters) {
    // Combine the floater combined area (stored in aState) and the
    // value computed by the line layout code.
    CombineRects(aState.mFloaterCombinedArea, aLine->mCombinedArea);
  }

  // Apply break-after clearing if necessary
  switch (aLine->mBreakType) {
  case NS_STYLE_CLEAR_LEFT:
  case NS_STYLE_CLEAR_RIGHT:
  case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
    aState.ClearFloaters(aState.mY, aLine->mBreakType);
    break;
  }

  lineLayout->EndLineReflow();

  return rv;
}

// Compute the line's max-element-size by adding into the raw value
// computed by reflowing the contents of the line (aMaxElementSize)
// the impact of floaters on this line or the preceeding lines.
void
nsBlockFrame::ComputeLineMaxElementSize(nsBlockReflowState& aState,
                                        nsLineBox* aLine,
                                        nsSize* aMaxElementSize)
{
  nscoord maxWidth, maxHeight;
  aState.mBand.GetMaxElementSize(&maxWidth, &maxHeight);
#ifdef NOISY_MAX_ELEMENT_SIZE
  IndentBy(stdout, GetDepth());
  if (NS_UNCONSTRAINEDSIZE == aState.mReflowState.availableWidth) {
    printf("PASS1 ");
  }
  ListTag(stdout);
  printf(": maxFloaterSize=%d,%d\n", maxWidth, maxHeight);
#endif

  // If the floaters are wider than the content, then use the maximum
  // floater width as the maximum width.
  //
  // It used to be the case that we would always place some content
  // next to a floater, regardless of the amount of available space
  // after subtracing off the floaters sizes. This can lead to content
  // overlapping floaters, so we no longer do this (and pass CSS2's
  // conformance tests). This is not how navigator 4-1 used to do
  // things.
  if (maxWidth > aMaxElementSize->width) {
    aMaxElementSize->width = maxWidth;
  }

  // Only update the max-element-size's height value if the floater is
  // part of the current line.
  if ((nsnull != aLine->mFloaters) &&
      (0 != aLine->mFloaters->Count())) {
    // If the maximum-height of the tallest floater is larger than the
    // maximum-height of the content then update the max-element-size
    // height
    if (maxHeight > aMaxElementSize->height) {
      aMaxElementSize->height = maxHeight;
    }
  }
}

void
nsBlockFrame::PostPlaceLine(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            const nsSize& aMaxElementSize)
{
  // Update max-element-size
  if (aState.mComputeMaxElementSize) {
    aState.UpdateMaxElementSize(aMaxElementSize);
  }

#if XXX_need_line_outside_children
  // Compute LINE_OUTSIDE_CHILDREN state for this line. The bit is set
  // if any child frame has outside children.
  if ((aLine->mCombinedArea.x < aLine->mBounds.x) ||
      (aLine->mCombinedArea.XMost() > aLine->mBounds.XMost()) ||
      (aLine->mCombinedArea.y < aLine->mBounds.y) ||
      (aLine->mCombinedArea.YMost() > aLine->mBounds.YMost())) {
    aLine->SetOutsideChildren();
  }
  else {
    aLine->ClearOutsideChildren();
  }
#endif

  // Update xmost
  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > aState.mKidXMost) {
#ifdef DEBUG
    if (CRAZY_WIDTH(xmost)) {
      ListTag(stdout);
      printf(": line=%p xmost=%d\n", aLine, xmost);
    }
#endif
    aState.mKidXMost = xmost;
  }
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
    aFrame->FirstChild(nsnull, &kid);
    while (nsnull != kid) {
      nsresult rv = FindFloatersIn(kid, aArray);
      if (NS_OK != rv) {
        return rv;
      }
      kid->GetNextSibling(&kid);
    }
  }
  return NS_OK;
}

void
nsBlockFrame::FindFloaters(nsLineBox* aLine)
{
  nsVoidArray* floaters = aLine->mFloaters;
  if (nsnull != floaters) {
    // Empty floater array before proceeding
    floaters->Clear();
  }

  nsIFrame* frame = aLine->mFirstChild;
  PRInt32 n = aLine->ChildCount();
  while (--n >= 0) {
    FindFloatersIn(frame, floaters);
    frame->GetNextSibling(&frame);
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

  nsLineBox* lastLine = aState.mPrevLine;
  nsLineBox* nextLine = lastLine->mNext;

  lastLine->mNext = nsnull;
  mOverflowLines = nextLine;

  // Mark all the overflow lines dirty so that they get reflowed when
  // they are pulled up by our next-in-flow.
  while (nsnull != nextLine) {
    nextLine->MarkDirty();
    nextLine = nextLine->mNext;
  }

  // Break frame sibling list
  nsIFrame* lastFrame = lastLine->LastChild();
  lastFrame->SetNextSibling(nsnull);

#ifdef DEBUG
  VerifyOverflowSituation();
#endif
}

PRBool
nsBlockFrame::DrainOverflowLines()
{
#ifdef DEBUG
  VerifyOverflowSituation();
#endif
  PRBool drained = PR_FALSE;

  // First grab the prev-in-flows overflow lines
  nsBlockFrame* prevBlock = (nsBlockFrame*) mPrevInFlow;
  if (nsnull != prevBlock) {
    nsLineBox* line = prevBlock->mOverflowLines;
    if (nsnull != line) {
      drained = PR_TRUE;
      prevBlock->mOverflowLines = nsnull;

      // Make all the frames on the mOverflowLines list mine
      nsIFrame* lastFrame = nsnull;
      nsIFrame* frame = line->mFirstChild;
      while (nsnull != frame) {
        frame->SetParent(this);

        // When pushing and pulling frames we need to check for whether any
        // views need to be reparented
        nsHTMLContainerFrame::ReparentFrameView(frame, prevBlock, this);

        // Get the next frame
        lastFrame = frame;
        frame->GetNextSibling(&frame);
      }

      // Join the line lists
      if (nsnull == mLines) {
        mLines = line;
      }
      else {
        // Join the sibling lists together
        lastFrame->SetNextSibling(mLines->mFirstChild);

        // Place overflow lines at the front of our line list
        nsLineBox* lastLine = nsLineBox::LastLine(line);
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
    nsLineBox* lastLine = nsLineBox::LastLine(mLines);
    if (nsnull == lastLine) {
      mLines = mOverflowLines;
    }
    else {
      lastLine->mNext = mOverflowLines;
      nsIFrame* lastFrame = lastLine->LastChild();
      lastFrame->SetNextSibling(mOverflowLines->mFirstChild);
    }
    mOverflowLines = nsnull;
    drained = PR_TRUE;
  }
  return drained;
}

//////////////////////////////////////////////////////////////////////
// Frame list manipulation routines

nsIFrame*
nsBlockFrame::LastChild()
{
  if (mLines) {
    nsLineBox* line = nsLineBox::LastLine(mLines);
    return line->LastChild();
  }
  return nsnull;
}

nsresult
nsBlockFrame::WrapFramesInFirstLineFrame(nsIPresContext* aPresContext)
{
  nsLineBox* line = mLines;
  if (!line) {
    return NS_OK;
  }

  // Find the first and last inline line that has frames that aren't
  // yet in the first-line-frame.
  nsLineBox* prevLine = nsnull;
  nsIFrame* firstInlineFrame = nsnull;
  nsFirstLineFrame* lineFrame = nsnull;
  nsIFrame* nextSib = nsnull;
  nsresult rv = NS_OK;
  while (line) {
    if (line->IsBlock()) {
      nextSib = line->mFirstChild;
      break;
    }
    if (line->IsFirstLine()) {
      NS_ASSERTION(1 == line->mChildCount, "bad first line");
      lineFrame = (nsFirstLineFrame*) line->mFirstChild;
      prevLine = line;
      line = line->mNext;
    }
    else {
      if (!firstInlineFrame) {
        firstInlineFrame = line->mFirstChild;
      }
      if (prevLine) {
        prevLine->mNext = line->mNext;
        delete line;
        line = prevLine->mNext;
      }
      else {
        prevLine = line;
        line = line->mNext;
      }
    }
  }
  if (!firstInlineFrame) {
    // All of the inline frames are already where they should be
    return NS_OK;
  }

  // Make the last inline frame thats going into the first-line-frame
  // have no next sibling.
  if (line) {
    nsFrameList frames(firstInlineFrame);
    nsIFrame* lastInlineFrame = frames.GetPrevSiblingFor(line->mFirstChild);
    lastInlineFrame->SetNextSibling(nsnull);
  }

  // If there is no first-line-frame currently, we create it
  if (!lineFrame) {
    nsIStyleContext* firstLineStyle = GetFirstLineStyle(aPresContext);

    // Create line frame
    nsresult rv = NS_NewFirstLineFrame((nsIFrame**) &lineFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = lineFrame->Init(*aPresContext, mContent, this,
                         firstLineStyle, nsnull);
    if (NS_FAILED(rv)) {
      return rv;
    }
    line = mLines;
    line->mFirstChild = lineFrame;
    line->mChildCount = 1;
    line->SetIsFirstLine(PR_TRUE);

    NS_RELEASE(firstLineStyle);
  }

  // Connect last first-line-frame to the remaining frames
  lineFrame->SetNextSibling(nextSib);

  // Put the new children into the line-frame
  lineFrame->AppendFrames2(aPresContext, firstInlineFrame);

  // Mark the first-line frames dirty
  line = mLines;
  while (line && line->IsFirstLine()) {
    line->MarkDirty();
    line = line->mNext;
  }

  return rv;
}

NS_IMETHODIMP
nsBlockFrame::AppendFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
  if (nsLayoutAtoms::floaterList == aListName) {
    // XXX we don't *really* care about this right now because we are
    // BuildFloaterList ing still
    mFloaters.AppendFrames(nsnull, aFrameList);
    return NS_OK;
  }
  else if (nsLayoutAtoms::absoluteList == aListName) {
    // XXX temporary until area frame is updated
    return nsFrame::AppendFrames(aPresContext, aPresShell, aListName,
                                 aFrameList);
  }
  else if (nsnull != aListName) {
    return NS_ERROR_INVALID_ARG;
  }

  // Find the proper last-child for where the append should go
  nsIFrame* lastKid = nsnull;
  nsLineBox* lastLine = nsLineBox::LastLine(mLines);
  if (lastLine) {
    lastKid = lastLine->LastChild();
    if (lastLine->IsFirstLine()) {
      // Get last frame in the nsFirstLineFrame
      lastKid->FirstChild(nsnull, &lastKid);
      nsFrameList frames(lastKid);
      lastKid = frames.LastChild();
    }
  }

  // Add frames after the last child
  nsresult rv = AddFrames(&aPresContext, aFrameList, lastKid);
  if (NS_SUCCEEDED(rv)) {
    // Generate reflow command to reflow the dirty lines
    nsIReflowCommand* reflowCmd = nsnull;
    nsresult rv;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                 nsIReflowCommand::ReflowDirty,
                                 nsnull);
    if (NS_SUCCEEDED(rv)) {
      if (nsnull != aListName) {
        reflowCmd->SetChildListName(aListName);
      }
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsBlockFrame::InsertFrames(nsIPresContext& aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList)
{
  if (nsLayoutAtoms::floaterList == aListName) {
    // XXX we don't *really* care about this right now because we are
    // BuildFloaterList ing still
    mFloaters.AppendFrames(nsnull, aFrameList);
    return NS_OK;
  }
  else if (nsLayoutAtoms::absoluteList == aListName) {
    // XXX temporary until area frame and floater code is updated
    return nsFrame::InsertFrames(aPresContext, aPresShell, aListName,
                                 aPrevFrame, aFrameList);
  }
  else if (nsnull != aListName) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = AddFrames(&aPresContext, aFrameList, aPrevFrame);
  if (NS_SUCCEEDED(rv)) {
    // Generate reflow command to reflow the dirty lines
    nsIReflowCommand* reflowCmd = nsnull;
    nsresult rv;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                 nsIReflowCommand::ReflowDirty,
                                 nsnull);
    if (NS_SUCCEEDED(rv)) {
      if (nsnull != aListName) {
        reflowCmd->SetChildListName(aListName);
      }
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }
  return rv;
}

nsresult
nsBlockFrame::AddFrames(nsIPresContext* aPresContext,
                        nsIFrame* aFrameList,
                        nsIFrame* aPrevSibling)
{
  if (nsnull == aFrameList) {
    return NS_OK;
  }

  // Attempt to find the line that contains the previous sibling
  nsLineBox* prevSibLine = nsnull;
  if (aPrevSibling) {
    // Its possible we have an nsFirstLineFrame managing some of our
    // child frames. If we do and the AddFrames is targetted at it,
    // use AddFirstLineFrames to get the frames properly placed.
    nsIFrame* prevSiblingParent;
    aPrevSibling->GetParent(&prevSiblingParent);
    if (prevSiblingParent != this) {
      // We are attempting an insert into a nsFirstLineFrame. Mark the
      // first-line's dirty. Not exactly optimial, but it will
      // guarantee a correct reflow.
      nsLineBox* line = mLines;
      while (line) {
        if (!line->IsFirstLine()) {
          break;
        }
        line->MarkDirty();
        line = line->mNext;
      }
      return AddFirstLineFrames(aPresContext,
                                (nsFirstLineFrame*)prevSiblingParent,
                                aFrameList, aPrevSibling);
    }
    else {
      // Find the line that contains the previous sibling
      prevSibLine = nsLineBox::FindLineContaining(mLines, aPrevSibling);
      if (nsnull == prevSibLine) {
        aPrevSibling = nsnull;
      }
    }
  }
  else if (mLines && mLines->IsFirstLine()) {
    mLines->MarkDirty();
    return AddFirstLineFrames(aPresContext,
                              (nsFirstLineFrame*)mLines->mFirstChild,
                              aFrameList, nsnull);
  }

  // Update the sibling list first
  nsFrameList newFrames(aFrameList);
  nsIFrame* lastNewFrame = newFrames.LastChild();
  nsIFrame* prevSiblingNextFrame = nsnull;
  if (aPrevSibling) {
    aPrevSibling->GetNextSibling(&prevSiblingNextFrame);
    aPrevSibling->SetNextSibling(aFrameList);
  }
  else if (mLines) {
    prevSiblingNextFrame = mLines->mFirstChild;
  }

  // Walk through the new frames being added and update the line data
  // structures to fit.
  nsIFrame* frame = aFrameList;
  nsIFrame* firstInlineFrame = nsnull;
  nsIFrame* prevSibling = aPrevSibling;
  PRInt32 pendingInlines = 0;
  while (frame) {
    if (nsLineLayout::TreatFrameAsBlock(frame)) {
      // Flush out any pending inline frames
      if (pendingInlines) {
        AddInlineFrames(aPresContext, &prevSibLine, firstInlineFrame,
                        pendingInlines);
        pendingInlines = 0;
      }

#if 0
      // If the block is going into the middle of a line, we need to
      // split the line in two.
      if (prevSibLine && !prevSibLine->IsBlock() &&
          (prevSibLine->mChildCount > 1)) {
        // Find the index of the block frame in the line. Note that if
        // the block frame is inserted past the prevSibLine's current
        // child count then we don't need to split the line in two
        // (because prevSibLine's list of children is already correct)
        PRInt32 ix = -1;
        nsIFrame* f = prevSibLine->mFirstChild;
        PRInt32 i, n = prevSibLine->mChildCount;
        for (i = 0; i < n; i++) {
          if (f == frame) {
            ix = i;
            break;
          }
          f->GetNextSibling(&f);
        }
        if (ix < 0) {
          // The block frame is just past the end of the prevSibLine's
          // list of children. No splitting is needed.
        }
        else {
          // We found the block frame in the prevSibLine. Therefore,
          // we need to split the line in two such that the frames
          // following the block go into a new line.
          nsIFrame* nextSib;
          frame->GetNextSibling(&nextSib);
          NS_ASSERTION(nsnull != nextSib, "weirdness");

          // Make a new line with the split off frames
          nsLineBox* line = new nsLineBox(, count, 0);
          if (!line) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          line->mNext = prevSibLine->mNext;
          prevSibLine->mNext = line;
        }

        if (prevSibling != lastKid) {
          // Find the index of the block frame in the line
          frame->GetNextSibling(&nextSib);
          nsIFrame* saveNextSib = nextSib;
          PRInt32 count = 1;
          while (nextSib != lastKid) {
            count++;
            nextSib->GetNextSibling(&nextSib);
          }


          // Take away the split off frames from the prevSibLine count
          NS_ASSERTION(prevSibLine->mChildCount >= count, "bad line count"); 
          prevSibLine->mChildCount -= count;
          prevSibLine->MarkDirty();
        }
      }
#endif

      // Create a new line for the block frame and add its line to the
      // line list.
      nsLineBox* line = new nsLineBox(frame, 1, LINE_IS_BLOCK);
      if (!line) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      if (prevSibLine) {
        // Append new line after prevSibLine
        line->mNext = prevSibLine->mNext;
        prevSibLine->mNext = line;
      }
      else {
        // New line is going before the other lines
        line->mNext = mLines;
        mLines = line;
      }
      prevSibLine = line;
    }
    else {
      if (0 == pendingInlines) {
        firstInlineFrame = frame;
      }
      pendingInlines++;
    }
    prevSibling = frame;
    frame->GetNextSibling(&frame);
  }
  if (prevSiblingNextFrame) {
    lastNewFrame->SetNextSibling(prevSiblingNextFrame);
  }

  // Flush out any lingering inline frames
  if (pendingInlines) {
    AddInlineFrames(aPresContext, &prevSibLine, firstInlineFrame,
                    pendingInlines);
  }

  // Fixup any frames that should be in a first-line frame but aren't
  if ((NS_BLOCK_HAS_FIRST_LINE_STYLE & mState) &&
      (nsnull != mLines) && !mLines->IsBlock()) {
    // We just added one or more frame(s) to the first line.
    WrapFramesInFirstLineFrame(aPresContext);
  }

#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
  MarkEmptyLines(aPresContext);
  return NS_OK;
}

nsresult
nsBlockFrame::AddInlineFrames(nsIPresContext* aPresContext,
                              nsLineBox** aPrevLinep,
                              nsIFrame* aFirstInlineFrame,
                              PRInt32 aPendingInlines)
{
  NS_ASSERTION(aPendingInlines >= 0, "bad pending inline count"); 
  nsLineBox* prevLine = *aPrevLinep;
  if (prevLine) {
    if (prevLine->IsBlock()) {
      // The frames cannot go on the previous line
      nsLineBox* line = new nsLineBox(aFirstInlineFrame, aPendingInlines, 0);
      if (!line) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      line->mNext = prevLine->mNext;
      prevLine->mNext = line;
      *aPrevLinep = line;
    }
    else {
      // The previous line contains inline frames. These inline frames
      // can go there.
      prevLine->MarkDirty();
      prevLine->mChildCount += aPendingInlines;
    }
  }
  else {
    // Make a new line to hold the inline frames
    nsLineBox* line = new nsLineBox(aFirstInlineFrame, aPendingInlines, 0);
    if (!line) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    line->mNext = mLines;
    mLines = line;
    *aPrevLinep = line;
  }

  return NS_OK;
}

nsresult
nsBlockFrame::AddFirstLineFrames(nsIPresContext* aPresContext,
                                 nsFirstLineFrame* aLineFrame,
                                 nsIFrame* aFrameList,
                                 nsIFrame* aPrevSibling)
{
  // The first run of inline frames being added always go into the
  // line frame. If we hit a block frame then we need to chop the line
  // frame into two pieces.
  nsIFrame* frame = aFrameList;
  nsIFrame* lastAddedFrame = frame;
  nsIFrame* firstInlineFrame = nsnull;
  PRInt32 pendingInlines = 0;
  while (frame) {
    if (nsLineLayout::TreatFrameAsBlock(frame)) {
      // There are 3 cases. The block is going to the front of the
      // line-frames children, in the middle or at the end.
      if (aPrevSibling) {
        nsIFrame* next;
        aPrevSibling->GetNextSibling(&next);

        // Take kids from the line frame starting at next.
        nsIFrame* kids;
        if (nsnull == next) {
          // The block goes in front of aLineFrame's continuation, if
          // it has one.
          nsFirstLineFrame* nextLineFrame;
          aLineFrame->GetNextInFlow((nsIFrame**) &nextLineFrame);
          if (!nextLineFrame) {
            // No continuation, therefore the block goes after aLineFrame
            FixParentAndView(aPresContext, frame);
            return AddFrames(aPresContext, frame, aLineFrame);
          }
          // Use nextLineFrame and take away all its kids
          aLineFrame = nextLineFrame;
        }
        kids = TakeKidsFromLineFrame(aLineFrame, next);

        // We will leave the line-frame and its continuations in
        // place but mark the lines dirty so that they are reflowed
        // and the empty line-frames (if any) are cleaned up.
        nsLineBox* line = mLines;
        nsIFrame* lastLineFrame = aLineFrame;
        while (line && line->IsFirstLine()) {
          line->MarkDirty();
          lastLineFrame = line->mFirstChild;
          line = line->mNext;
        }

        // Join the taken kids onto the *end* of the frames being
        // added.
        nsFrameList newFrames(frame);
        newFrames.AppendFrames(this, kids);
        FixParentAndView(aPresContext, frame);
        return AddFrames(aPresContext, frame, lastLineFrame);
      }
      else {
        // Block is trying to go to the front of the line frame (and
        // therefore in front of all the line-frames children and its
        // continuations children). Therefore, we don't need a line
        // frame anymore.
        nsIFrame* kids = TakeKidsFromLineFrame(aLineFrame, nsnull);

        // Join the kids onto the end of the frames being added
        nsFrameList newFrames(frame);
        newFrames.AppendFrames(this, kids);
        FixParentAndView(aPresContext, newFrames.FirstChild());

        // Remove the line frame (and its continuations). This also
        // removes the nsLineBox's that pointed to the line-frame.  Do
        // this after FixParentAndView because FixParentAndView needs
        // a valid old-parent to work.
        DoRemoveFrame(aPresContext, aLineFrame);

        // Re-enter AddFrames, this time there won't be any first-line
        // frames so we will use the normal path.
        return AddFrames(aPresContext, newFrames.FirstChild(), nsnull);
      }
    }
    else {
      if (0 == pendingInlines) {
        firstInlineFrame = frame;
      }
      pendingInlines++;
    }
    lastAddedFrame = frame;
    frame->GetNextSibling(&frame);
  }

  // All of the frames being added are inline frames
  if (pendingInlines) {
    return aLineFrame->InsertFrames2(aPresContext, aPrevSibling, aFrameList);
  }
  return NS_OK;
}

nsIFrame*
nsBlockFrame::TakeKidsFromLineFrame(nsFirstLineFrame* aLineFrame,
                                    nsIFrame* aFromKid)
{
  nsFrameList kids;
  nsIFrame* lastKid;
  if (aFromKid) {
    kids.SetFrames(aFromKid);
    aLineFrame->RemoveFramesFrom(aFromKid);
  }
  else {
    aLineFrame->FirstChild(nsnull, &lastKid);
    kids.SetFrames(lastKid);
    aLineFrame->RemoveAllFrames();
  }
  lastKid = kids.LastChild();

  // Capture the next-in-flows kids as well.
  for (;;) {
    aLineFrame->GetNextInFlow((nsIFrame**) &aLineFrame);
    if (!aLineFrame) {
      break;
    }
    aLineFrame->FirstChild(nsnull, &aFromKid);
    aLineFrame->RemoveAllFrames();
    lastKid->SetNextSibling(aFromKid);
    nsFrameList tmp(aFromKid);
    lastKid = tmp.LastChild();
  }

  return kids.FirstChild();
}

void
nsBlockFrame::FixParentAndView(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  while (aFrame) {
    nsIFrame* oldParent;
    aFrame->GetParent(&oldParent);
    aFrame->SetParent(this);
    if (this != oldParent) {
      nsHTMLContainerFrame::ReparentFrameView(aFrame, oldParent, this);
    }
    aFrame->ReResolveStyleContext(aPresContext, mStyleContext, 
                                  NS_STYLE_HINT_REFLOW,
                                  nsnull, nsnull);
    aFrame->GetNextSibling(&aFrame);
  }
}

NS_IMETHODIMP
nsBlockFrame::RemoveFrame(nsIPresContext& aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame)
{
  nsresult rv = NS_OK;

  if (nsLayoutAtoms::floaterList == aListName) {
    // Remove floater from the floater list first
    mFloaters.RemoveFrame(aOldFrame);

    // Find which line contains the floater
    nsLineBox* line = mLines;
    while (nsnull != line) {
      nsVoidArray* floaters = line->mFloaters;
      if (nsnull != floaters) {
        PRInt32 i, count = floaters->Count();
        for (i = 0; i < count; i++) {
          nsPlaceholderFrame* ph = (nsPlaceholderFrame*)floaters->ElementAt(i);
          if (ph->GetAnchoredItem() == aOldFrame) {
            // Note: the placeholder is part of the line's child list
            // and will be removed later.
            // XXX stop storing pointers to the placeholder in the line list???
            ph->SetAnchoredItem(nsnull);
            floaters->RemoveElementAt(i);
            aOldFrame->DeleteFrame(aPresContext);
            goto found_it;
          }
        }
      }
      line = line->mNext;
    }
   found_it:

    // Mark every line at and below the line where the floater was dirty
    while (nsnull != line) {
      line->MarkDirty();
      line = line->mNext;
    }

    // We will reflow *after* removing the placeholder (which is done 2nd)
    return NS_OK;
  }
  else if (nsLayoutAtoms::absoluteList == aListName) {
    // XXX temporary until area frame code is updated
    return nsFrame::RemoveFrame(aPresContext, aPresShell, aListName,
                                aOldFrame);
  }
  else if (nsnull != aListName) {
    return NS_ERROR_INVALID_ARG;
  }
  else {
    rv = DoRemoveFrame(&aPresContext, aOldFrame);
  }

  if (NS_SUCCEEDED(rv)) {
    // Generate reflow command to reflow the dirty lines
    nsIReflowCommand* reflowCmd = nsnull;
    nsresult rv;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                 nsIReflowCommand::ReflowDirty,
                                 nsnull);
    if (NS_SUCCEEDED(rv)) {
      if (nsnull != aListName) {
        reflowCmd->SetChildListName(aListName);
      }
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }
  return rv;
}

nsresult
nsBlockFrame::DoRemoveFrame(nsIPresContext* aPresContext,
                            nsIFrame* aDeletedFrame)
{
  nsIFrame* parent;
  aDeletedFrame->GetParent(&parent);
  if (parent != this) {
    return RemoveFirstLineFrame(aPresContext, (nsFirstLineFrame*)parent,
                                aDeletedFrame);
  }

  // Find the line and the previous sibling that contains
  // deletedFrame; we also find the pointer to the line.
  nsBlockFrame* flow = this;
  nsLineBox** linep = &flow->mLines;
  nsLineBox* line = flow->mLines;
  nsLineBox* prevLine = nsnull;
  nsIFrame* prevSibling = nsnull;
  while (nsnull != line) {
    nsIFrame* frame = line->mFirstChild;
    PRInt32 n = line->ChildCount();
    while (--n >= 0) {
      if (frame == aDeletedFrame) {
        goto found_frame;
      }
      prevSibling = frame;
      frame->GetNextSibling(&frame);
    }
    linep = &line->mNext;
    prevLine = line;
    line = line->mNext;
  }
 found_frame:;
#ifdef NS_DEBUG
  NS_ASSERTION(nsnull != line, "can't find deleted frame in lines");
  if (nsnull != prevSibling) {
    nsIFrame* tmp;
    prevSibling->GetNextSibling(&tmp);
    NS_ASSERTION(tmp == aDeletedFrame, "bad prevSibling");
  }
#endif

  // Remove frame and all of its continuations
  while (nsnull != aDeletedFrame) {
    while ((nsnull != line) && (nsnull != aDeletedFrame)) {
#ifdef NS_DEBUG
      aDeletedFrame->GetParent(&parent);
      NS_ASSERTION(flow == parent, "messed up delete code");
      NS_ASSERTION(line->Contains(aDeletedFrame), "frame not in line");
#endif

      // See if the frame being deleted is the last one on the line
      PRBool isLastFrameOnLine = PR_FALSE;
      if (1 == line->mChildCount) {
        isLastFrameOnLine = PR_TRUE;
      }
      else if (line->LastChild() == aDeletedFrame) {
        isLastFrameOnLine = PR_TRUE;
      }

      // Remove aDeletedFrame from the line
      nsIFrame* nextFrame;
      aDeletedFrame->GetNextSibling(&nextFrame);
      if (line->mFirstChild == aDeletedFrame) {
        line->mFirstChild = nextFrame;
      }
      if (prevLine && !prevLine->IsBlock()) {
        // Since we just removed a frame that follows some inline
        // frames, we need to reflow the previous line.
        prevLine->MarkDirty();
      }

      // Take aDeletedFrame out of the sibling list. Note that
      // prevSibling will only be nsnull when we are deleting the very
      // first frame.
      if (nsnull != prevSibling) {
        prevSibling->SetNextSibling(nextFrame);
      }

      // Destroy frame; capture its next-in-flow first in case we need
      // to destroy that too.
      nsIFrame* nextInFlow;
      aDeletedFrame->GetNextInFlow(&nextInFlow);
      nsSplittableType st;
      aDeletedFrame->IsSplittable(st);
      if (NS_FRAME_NOT_SPLITTABLE != st) {
        aDeletedFrame->RemoveFromFlow();
      }
#ifdef NOISY_REMOVE_FRAME
      printf("DoRemoveFrame: prevLine=%p line=%p frame=",
             prevLine, line);
      nsFrame::ListTag(stdout, aDeletedFrame);
      printf(" prevSibling=%p nextInFlow=%p\n", prevSibling, nextInFlow);
#endif
      aDeletedFrame->DeleteFrame(*aPresContext);
      aDeletedFrame = nextInFlow;

      // If line is empty, remove it now
      nsLineBox* next = line->mNext;
      if (0 == --line->mChildCount) {
        *linep = next;
        line->mNext = nsnull;
        delete line;
        line = next;
      }
      else {
        NS_ASSERTION(line->mChildCount < 10000, "bad line count"); 
        // Make the line that just lost a frame dirty
        line->MarkDirty();

        // If we just removed the last frame on the line then we need
        // to advance to the next line.
        if (isLastFrameOnLine) {
          prevLine = line;
          linep = &line->mNext;
          line = next;
        }
      }

      // See if we should keep looking in the current flow's line list.
      if (nsnull != aDeletedFrame) {
        if (aDeletedFrame != nextFrame) {
          // The deceased frames continuation is not the next frame in
          // the current flow's frame list. Therefore we know that the
          // continuation is in a different parent. So break out of
          // the loop so that we advance to the next parent.
#ifdef NS_DEBUG
          nsIFrame* parent;
          aDeletedFrame->GetParent(&parent);
          NS_ASSERTION(parent != flow, "strange continuation");
#endif
          break;
        }
      }
    }

    // Advance to next flow block if the frame has more continuations
    if (nsnull != aDeletedFrame) {
      flow = (nsBlockFrame*) flow->mNextInFlow;
      NS_ASSERTION(nsnull != flow, "whoops, continuation without a parent");
      prevLine = nsnull;
      line = flow->mLines;
      linep = &flow->mLines;
      prevSibling = nsnull;
    }
  }

  // Fixup any frames that should be in a first-line frame but aren't
  if ((NS_BLOCK_HAS_FIRST_LINE_STYLE & mState) &&
      (nsnull != mLines) && !mLines->IsBlock()) {
    // We just added one or more frame(s) to the first line, or we
    // removed a block that preceeded the first line.
    WrapFramesInFirstLineFrame(aPresContext);
  }

#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
  MarkEmptyLines(aPresContext);
  return NS_OK;
}

nsresult
nsBlockFrame::RemoveFirstLineFrame(nsIPresContext* aPresContext,
                                   nsFirstLineFrame* aLineFrame,
                                   nsIFrame* aDeletedFrame)
{
  // Strip deleted frame out of the nsFirstLineFrame
  aLineFrame->RemoveFrame2(aPresContext, aDeletedFrame);
  aDeletedFrame->DeleteFrame(*aPresContext);

  // See if the line-frame and its continuations are now empty
  nsFirstLineFrame* lf = (nsFirstLineFrame*) aLineFrame->GetFirstInFlow();
  nsFirstLineFrame* lf0 = lf;
  PRBool empty = PR_TRUE;
  while (lf) {
    nsIFrame* kids;
    lf->FirstChild(nsnull, &kids);
    if (kids) {
      empty = PR_FALSE;
      break;
    }
    lf->GetNextInFlow((nsIFrame**) &lf);
  }
  if (empty) {
    return DoRemoveFrame(aPresContext, lf0);
  }

  // Mark first-line lines dirty
  nsLineBox* line = mLines;
  while (line && line->IsFirstLine()) {
    line->MarkDirty();
    line = line->mNext;
  }
  return NS_OK;
}

static PRBool
IsEmptyLine(nsIPresContext* aPresContext, nsLineBox* aLine)
{
  PRInt32 i, n = aLine->ChildCount();
  nsIFrame* frame = aLine->mFirstChild;
  for (i = 0; i < n; i++) {
    nsIContent* content;
    nsresult rv = frame->GetContent(&content);
    if (NS_FAILED(rv) || (nsnull == content)) {
      // If it doesn't have any content then this can't be an empty line
      return PR_FALSE;
    }
    nsITextContent* tc;
    rv = content->QueryInterface(kITextContentIID, (void**) &tc);
    if (NS_FAILED(rv) || (nsnull == tc)) {
      // If it's not text content then this can't be an empty line
      NS_RELEASE(content);
      return PR_FALSE;
    }

    const nsTextFragment* frag;
    PRInt32 numFrags;
    rv = tc->GetText(frag, numFrags);
    if (NS_FAILED(rv)) {
      NS_RELEASE(content);
      NS_RELEASE(tc);
      return PR_FALSE;
    }

    // If the text has any non-whitespace characters in it then the
    // line is not an empty line.
    while (--numFrags >= 0) {
      PRInt32 len = frag->GetLength();
      if (frag->Is2b()) {
        const PRUnichar* cp = frag->Get2b();
        const PRUnichar* end = cp + len;
        while (cp < end) {
          PRUnichar ch = *cp++;
          if (!XP_IS_SPACE(ch)) {
            NS_RELEASE(tc);
            NS_RELEASE(content);
            return PR_FALSE;
          }
        }
      }
      else {
        const char* cp = frag->Get1b();
        const char* end = cp + len;
        while (cp < end) {
          char ch = *cp++;
          if (!XP_IS_SPACE(ch)) {
            NS_RELEASE(tc);
            NS_RELEASE(content);
            return PR_FALSE;
          }
        }
      }
      frag++;
    }

    NS_RELEASE(tc);
    NS_RELEASE(content);
    frame->GetNextSibling(&frame);
  }
  return PR_TRUE;
}

void
nsBlockFrame::MarkEmptyLines(nsIPresContext* aPresContext)
{
  // PRE-formatted content considers whitespace significant
  const nsStyleText* text;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&) text);
  if ((NS_STYLE_WHITESPACE_PRE == text->mWhiteSpace) ||
      (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == text->mWhiteSpace)) {
    return;
  }

  PRBool afterBlock = PR_TRUE;
  nsLineBox* line = mLines;
  while (nsnull != line) {
    if (line->IsBlock()) {
      afterBlock = PR_TRUE;
    }
    else if (afterBlock) {
      afterBlock = PR_FALSE;

      // This is an inline line and it is immediately after a block
      // (or its our first line). See if it contains nothing but
      // collapsible text.
      PRBool isEmpty = IsEmptyLine(aPresContext, line);
      line->SetIsEmptyLine(isEmpty);
    }
    else {
      line->SetIsEmptyLine(PR_FALSE);
    }
    line = line->mNext;
  }
}

void
nsBlockFrame::DeleteChildsNextInFlow(nsIPresContext& aPresContext,
                                     nsIFrame* aChild)
{
  NS_PRECONDITION(IsChild(aChild), "bad geometric parent");
  nsIFrame* nextInFlow;
  aChild->GetNextInFlow(&nextInFlow);
  NS_PRECONDITION(nsnull != nextInFlow, "null next-in-flow");
  nsBlockFrame* parent;
  nextInFlow->GetParent((nsIFrame**)&parent);
  NS_PRECONDITION(nsnull != parent, "next-in-flow with no parent");
  NS_PRECONDITION(nsnull != parent->mLines, "next-in-flow with weird parent");
  NS_PRECONDITION(nsnull == parent->mOverflowLines, "parent with overflow");
  parent->DoRemoveFrame(&aPresContext, nextInFlow);
}

////////////////////////////////////////////////////////////////////////
// Floater support

nsresult
nsBlockFrame::ReflowFloater(nsBlockReflowState& aState,
                            nsPlaceholderFrame* aPlaceholder,
                            nsRect& aCombinedRect,
                            nsMargin& aMarginResult)
{
  // Reflow the floater. Since floaters are continued we given them an
  // unbounded height. Floaters with an auto width are sized to zero
  // according to the css2 spec.
  nsRect availSpace(0, 0, aState.mAvailSpaceRect.width, NS_UNCONSTRAINEDSIZE);
  nsIFrame* floater = aPlaceholder->GetAnchoredItem();
  PRBool isAdjacentWithTop = aState.IsAdjacentWithTop();

  // Setup block reflow state to reflow the floater
  nsBlockReflowContext brc(aState.mPresContext, aState.mReflowState,
                           aState.mComputeMaxElementSize);
  brc.SetNextRCFrame(aState.mNextRCFrame);

  // Reflow the floater
  nsReflowStatus frameReflowStatus;
  nsMargin computedOffsets;
  nsresult rv = brc.ReflowBlock(floater, availSpace, PR_TRUE,
                                0, isAdjacentWithTop,
                                computedOffsets, frameReflowStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Capture the margin information for the caller
  const nsMargin& m = brc.GetMargin();
  aMarginResult.top = brc.GetTopMargin();
  aMarginResult.right = m.right;
  aMarginResult.bottom = 
    nsBlockReflowContext::MaxMargin(brc.GetCarriedOutBottomMargin(),
                                    m.bottom);
  aMarginResult.left = m.left;

  const nsHTMLReflowMetrics& metrics = brc.GetMetrics();
  aCombinedRect = metrics.mCombinedArea;
  floater->SizeTo(metrics.width, metrics.height);
  return NS_OK;
}

void
nsBlockReflowState::InitFloater(nsPlaceholderFrame* aPlaceholder)
{
  // Set the geometric parent of the floater
  nsIFrame* floater = aPlaceholder->GetAnchoredItem();
  floater->SetParent(mBlock);

  // Then add the floater to the current line and place it when
  // appropriate
  AddFloater(aPlaceholder, PR_TRUE);
}

// This is called by the line layout's AddFloater method when a
// place-holder frame is reflowed in a line. If the floater is a
// left-most child (it's x coordinate is at the line's left margin)
// then the floater is place immediately, otherwise the floater
// placement is deferred until the line has been reflowed.
void
nsBlockReflowState::AddFloater(nsPlaceholderFrame* aPlaceholder,
                               PRBool aInitialReflow)
{
  NS_PRECONDITION(nsnull != mCurrentLine, "null ptr");

  // Update the current line's floater array
  if (nsnull == mCurrentLine->mFloaters) {
    mCurrentLine->mFloaters = new nsVoidArray();
  }
  mCurrentLine->mFloaters->AppendElement(aPlaceholder);

  // Now place the floater immediately if possible. Otherwise stash it
  // away in mPendingFloaters and place it later.
  if (mLineLayout->CanPlaceFloaterNow()) {
    nsRect combinedArea;
    nsMargin floaterMargins;
    mBlock->ReflowFloater(*this, aPlaceholder, combinedArea, floaterMargins);

    // Because we are in the middle of reflowing a placeholder frame
    // within a line (and possibly nested in an inline frame or two
    // that's a child of our block) we need to restore the space
    // manager's translation to the space that the block resides in
    // before placing the floater.
    PRBool isLeftFloater;
    nscoord ox, oy;
    mSpaceManager->GetTranslation(ox, oy);
    nscoord dx = ox - mSpaceManagerX;
    nscoord dy = oy - mSpaceManagerY;
    mSpaceManager->Translate(-dx, -dy);
    nsPoint origin;
    PlaceFloater(aPlaceholder, floaterMargins, &isLeftFloater, &origin);

    // Update the floater combined-area
    combinedArea.x += origin.x;
    combinedArea.y += origin.y;
    CombineRects(combinedArea, mFloaterCombinedArea);

    // Pass on updated available space to the current inline reflow engine
    GetAvailableSpace();
    mLineLayout->UpdateBand(mAvailSpaceRect.x + BorderPadding().left, mY,
                            mAvailSpaceRect.width,
                            mAvailSpaceRect.height,
                            isLeftFloater);

    // Restore coordinate system
    mSpaceManager->Translate(dx, dy);
  }
  else {
    // This floater will be placed after the line is done (it is a
    // below current line floater).
    mPendingFloaters.AppendElement(aPlaceholder);
  }
}

PRBool
nsBlockReflowState::IsLeftMostChild(nsIFrame* aFrame)
{
  for (;;) {
    nsIFrame* parent;
    aFrame->GetParent(&parent);
    if (parent == mBlock) {
      nsIFrame* child = mCurrentLine->mFirstChild;
      PRInt32 n = mCurrentLine->ChildCount();
      while ((nsnull != child) && (aFrame != child) && (--n >= 0)) {
        nsSize  size;

        // Is the child zero-sized?
        child->GetSize(size);
        if (size.width > 0) {
          // We found a non-zero sized child frame that precedes aFrame
          return PR_FALSE;
        }
        child->GetNextSibling(&child);
      }
      break;
    }
    else {
      // See if there are any non-zero sized child frames that precede
      // aFrame in the child list
      nsIFrame* child;
      parent->FirstChild(nsnull, &child);
      while ((nsnull != child) && (aFrame != child)) {
        nsSize  size;

        // Is the child zero-sized?
        child->GetSize(size);
        if (size.width > 0) {
          // We found a non-zero sized child frame that precedes aFrame
          return PR_FALSE;
        }
        child->GetNextSibling(&child);
      }
    }
  
    // aFrame is the left-most non-zero sized frame in its geometric parent.
    // Walk up one level and check that its parent is left-most as well
    aFrame = parent;
  }
  return PR_TRUE;
}

PRBool
nsBlockReflowState::CanPlaceFloater(const nsRect& aFloaterRect,
                                    PRUint8 aFloats)
{
  // If the current Y coordinate is not impacted by any floaters
  // then by definition the floater fits.
  PRBool result = PR_TRUE;
  if (0 != mBand.GetFloaterCount()) {
    if (mAvailSpaceRect.width < aFloaterRect.width) {
      // The available width is too narrow (and its been impacted by a
      // prior floater)
      result = PR_FALSE;
    }
    else {
      // At this point we know that there is enough horizontal space for
      // the floater (somewhere). Lets see if there is enough vertical
      // space.
      if (mAvailSpaceRect.height < aFloaterRect.height) {
        // The available height is too short. However, its possible that
        // there is enough open space below which is not impacted by a
        // floater.
        //
        // Compute the X coordinate for the floater based on its float
        // type, assuming its placed on the current line. This is
        // where the floater will be placed horizontally if it can go
        // here.
        nscoord x0;
        if (NS_STYLE_FLOAT_LEFT == aFloats) {
          x0 = mAvailSpaceRect.x;
        }
        else {
          x0 = mAvailSpaceRect.XMost() - aFloaterRect.width;

          // In case the floater is too big, don't go past the left edge
          if (x0 < mAvailSpaceRect.x) {
            x0 = mAvailSpaceRect.x;
          }
        }
        nscoord x1 = x0 + aFloaterRect.width;

        // Calculate the top and bottom y coordinates, again assuming
        // that the floater is placed on the current line.
        const nsMargin& borderPadding = BorderPadding();
        nscoord y0 = mY - borderPadding.top;
        if (y0 < 0) {
          // CSS2 spec, 9.5.1 rule [4]: A floating box's outer top may not
          // be higher than the top of its containing block.

          // XXX It's not clear if it means the higher than the outer edge
          // or the border edge or the inner edge?
          y0 = 0;
        }
        nscoord y1 = y0 + aFloaterRect.height;

        nscoord saveY = mY;
        for (;;) {
          // Get the available space at the new Y coordinate
          mY += mAvailSpaceRect.height;
          GetAvailableSpace();

          if (0 == mBand.GetFloaterCount()) {
            // Winner. This band has no floaters on it, therefore
            // there can be no overlap.
            break;
          }

          // Check and make sure the floater won't intersect any
          // floaters on this band. The floaters starting and ending
          // coordinates must be entirely in the available space.
          if ((x0 < mAvailSpaceRect.x) || (x1 > mAvailSpaceRect.XMost())) {
            // The floater can't go here.
            result = PR_FALSE;
            break;
          }

          // See if there is now enough height for the floater.
          if (y1 < mY + mAvailSpaceRect.height) {
            // Winner. The bottom Y coordinate of the floater is in
            // this band.
            break;
          }
        }

        // Restore Y coordinate and available space information
        // regardless of the outcome.
        mY = saveY;
        GetAvailableSpace();
      }
    }
  }
  return result;
}

void
nsBlockReflowState::PlaceFloater(nsPlaceholderFrame* aPlaceholder,
                                 const nsMargin& aFloaterMargins,
                                 PRBool* aIsLeftFloater,
                                 nsPoint* aNewOrigin)
{
  // Save away the Y coordinate before placing the floater. We will
  // restore mY at the end after placing the floater. This is
  // necessary because any adjustments to mY during the floater
  // placement are for the floater only, not for any non-floating
  // content.
  nscoord saveY = mY;
  nsIFrame* floater = aPlaceholder->GetAnchoredItem();

  // Get the type of floater
  const nsStyleDisplay* floaterDisplay;
  const nsStyleSpacing* floaterSpacing;
  floater->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&)floaterDisplay);
  floater->GetStyleData(eStyleStruct_Spacing,
                        (const nsStyleStruct*&)floaterSpacing);

  // See if the floater should clear any preceeding floaters...
  if (NS_STYLE_CLEAR_NONE != floaterDisplay->mBreakType) {
    ClearFloaters(mY, floaterDisplay->mBreakType);
  }
  else {
    // Get the band of available space
    GetAvailableSpace();
  }

  // Get the floaters bounding box and margin information
  nsRect region;
  floater->GetRect(region);

  // Adjust the floater size by its margin. That's the area that will
  // impact the space manager.
  region.width += aFloaterMargins.left + aFloaterMargins.right;
  region.height += aFloaterMargins.top + aFloaterMargins.bottom;

  // Find a place to place the floater. The CSS2 spec doesn't want
  // floaters overlapping each other or sticking out of the containing
  // block if possible (CSS2 spec section 9.5.1, see the rule list).
  NS_ASSERTION((NS_STYLE_FLOAT_LEFT == floaterDisplay->mFloats) ||
               (NS_STYLE_FLOAT_RIGHT == floaterDisplay->mFloats),
               "invalid float type");

  // While there is not enough room for the floater, clear past other
  // floaters until there is room (or the band is not impacted by a
  // floater).
  // 
  // Note: The CSS2 spec says that floaters should be placed as high
  // as possible.
  while (!CanPlaceFloater(region, floaterDisplay->mFloats)) {
    mY += mAvailSpaceRect.height;
    GetAvailableSpace();
  }

  // Assign an x and y coordinate to the floater. Note that the x,y
  // coordinates are computed <b>relative to the translation in the
  // spacemanager</b> which means that the impacted region will be
  // <b>inside</b> the border/padding area.
  if (NS_STYLE_FLOAT_LEFT == floaterDisplay->mFloats) {
    *aIsLeftFloater = PR_TRUE;
    region.x = mAvailSpaceRect.x;
  }
  else {
    *aIsLeftFloater = PR_FALSE;
    region.x = mAvailSpaceRect.XMost() - region.width;
  }
  const nsMargin& borderPadding = BorderPadding();
  region.y = mY - borderPadding.top;
  if (region.y < 0) {
    // CSS2 spec, 9.5.1 rule [4]: A floating box's outer top may not
    // be higher than the top of its containing block.

    // XXX It's not clear if it means the higher than the outer edge
    // or the border edge or the inner edge?
    region.y = 0;
  }

  // Place the floater in the space manager
  mSpaceManager->AddRectRegion(floater, region);

  // Set the origin of the floater frame, in frame coordinates. These
  // coordinates are <b>not</b> relative to the spacemanager
  // translation, therefore we have to factor in our border/padding.
  nscoord x = borderPadding.left + aFloaterMargins.left + region.x;
  nscoord y = borderPadding.top + aFloaterMargins.top + region.y;
  floater->MoveTo(x, y);
  if (aNewOrigin) {
    aNewOrigin->x = x;
    aNewOrigin->y = y;
  }

  // Now restore mY
  mY = saveY;

#ifdef NOISY_INCREMENTAL_REFLOW
  if (mReflowState.reason == eReflowReason_Incremental) {
    nsRect r;
    floater->GetRect(r);
    nsFrame::IndentBy(stdout, gNoiseIndent);
    printf("placed floater: ");
    ((nsFrame*)floater)->ListTag(stdout);
    printf(" %d,%d,%d,%d\n", r.x, r.y, r.width, r.height);
  }
#endif
}

/**
 * Place below-current-line floaters.
 */
void
nsBlockReflowState::PlaceBelowCurrentLineFloaters(nsVoidArray* aFloaters,
                                                  PRBool aReflowFloaters)
{
  NS_PRECONDITION(aFloaters->Count() > 0, "no floaters");

  PRInt32 numFloaters = aFloaters->Count();
  for (PRInt32 i = 0; i < numFloaters; i++) {
    nsPlaceholderFrame* placeholderFrame = (nsPlaceholderFrame*)
      aFloaters->ElementAt(i);
    if (!IsLeftMostChild(placeholderFrame)) {
// XXX_perf
      // Before we can place it we have to reflow it
      nsRect combinedArea;
      nsMargin floaterMargins;
      mBlock->ReflowFloater(*this, placeholderFrame, combinedArea,
                            floaterMargins);

      PRBool isLeftFloater;
      nsPoint origin;
      PlaceFloater(placeholderFrame, floaterMargins, &isLeftFloater,
                   &origin);

      // Update the floater combined-area
      combinedArea.x += origin.x;
      combinedArea.y += origin.y;
      CombineRects(combinedArea, mFloaterCombinedArea);
    }
  }
}

/**
 * Place current-line floaters.
 */
void
nsBlockReflowState::PlaceCurrentLineFloaters(nsVoidArray* aFloaters)
{
  NS_PRECONDITION(aFloaters->Count() > 0, "no floaters");

  PRInt32 numFloaters = aFloaters->Count();
  for (PRInt32 i = 0; i < numFloaters; i++) {
    nsPlaceholderFrame* placeholderFrame = (nsPlaceholderFrame*)
      aFloaters->ElementAt(i);
    if (IsLeftMostChild(placeholderFrame)) {
// XXX_perf
      // Before we can place it we have to reflow it
      nsRect combinedArea;
      nsMargin floaterMargins;
      mBlock->ReflowFloater(*this, placeholderFrame, combinedArea,
                            floaterMargins);

      PRBool isLeftFloater;
      nsPoint origin;
      PlaceFloater(placeholderFrame, floaterMargins, &isLeftFloater, &origin);

      // Update the floater combined-area
      combinedArea.x += origin.x;
      combinedArea.y += origin.y;
      CombineRects(combinedArea, mFloaterCombinedArea);
    }
  }
}

void
nsBlockReflowState::ClearFloaters(nscoord aY, PRUint8 aBreakType)
{
#ifdef NOISY_INCREMENTAL_REFLOW
  if (mReflowState.reason == eReflowReason_Incremental) {
    nsFrame::IndentBy(stdout, gNoiseIndent);
    printf("clear floaters: in: mY=%d aY=%d(%d)\n",
           mY, aY, aY - BorderPadding().top);
  }
#endif

  const nsMargin& bp = BorderPadding();
  nscoord newY = mBand.ClearFloaters(aY - bp.top, aBreakType);
  mY = newY + bp.top;
  GetAvailableSpace();

#ifdef NOISY_INCREMENTAL_REFLOW
  if (mReflowState.reason == eReflowReason_Incremental) {
    nsFrame::IndentBy(stdout, gNoiseIndent);
    printf("clear floaters: out: mY=%d(%d)\n", mY, mY - bp.top);
  }
#endif
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

#ifdef NOISY_DAMAGE_REPAIR
static void ComputeCombinedArea(nsLineBox* aLine,
                                nscoord aWidth, nscoord aHeight,
                                nsRect& aResult)
{
  nscoord x0 = 0, y0 = 0, x1 = aWidth, y1 = aHeight;
  while (nsnull != aLine) {
    // Compute min and max x/y values for the reflowed frame's
    // combined areas
    nscoord x = aLine->mCombinedArea.x;
    nscoord y = aLine->mCombinedArea.y;
    nscoord xmost = x + aLine->mCombinedArea.width;
    nscoord ymost = y + aLine->mCombinedArea.height;
    if (x < x0) {
      x0 = x;
    }
    if (xmost > x1) {
      x1 = xmost;
    }
    if (y < y0) {
      y0 = y;
    }
    if (ymost > y1) {
      y1 = ymost;
    }
    aLine = aLine->mNext;
  }

  aResult.x = x0;
  aResult.y = y0;
  aResult.width = x1 - x0;
  aResult.height = y1 - y0;
}
#endif

NS_IMETHODIMP
nsBlockFrame::Paint(nsIPresContext&      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer)
{
#ifdef NOISY_DAMAGE_REPAIR
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    PRInt32 depth = GetDepth();
    nsRect ca;
    ComputeCombinedArea(mLines, mRect.width, mRect.height, ca);
    nsFrame::IndentBy(stdout, depth);
    ListTag(stdout);
    printf(": bounds=%d,%d,%d,%d dirty=%d,%d,%d,%d ca=%d,%d,%d,%d\n",
           mRect.x, mRect.y, mRect.width, mRect.height,
           aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height,
           ca.x, ca.y, ca.width, ca.height);
  }
#endif  

  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);

  // If overflow is hidden then set the clip rect so that children
  // don't leak out of us
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    aRenderingContext.PushState();
    SetClipRect(aRenderingContext);
  }

  // Only paint the border and background if we're visible
  if (disp->mVisible && (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) &&
      (0 != mRect.width) && (0 != mRect.height)) {
    PRIntn skipSides = GetSkipSides();
    const nsStyleColor* color = (const nsStyleColor*)
      mStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleSpacing* spacing = (const nsStyleSpacing*)
      mStyleContext->GetStyleData(eStyleStruct_Spacing);

    // Paint background and border
    nsRect rect(0, 0, mRect.width, mRect.height);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *color, *spacing, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, *spacing, mStyleContext,
                                skipSides);
  }

  // Child elements have the opportunity to override the visibility
  // property and display even if the parent is hidden
  if (NS_FRAME_PAINT_LAYER_FLOATERS == aWhichLayer) {
    PaintFloaters(aPresContext, aRenderingContext, aDirtyRect);
  }
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    PRBool clipState;
    aRenderingContext.PopState(clipState);
  }

  return NS_OK;
}

void
nsBlockFrame::PaintFloaters(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
  for (nsLineBox* line = mLines; nsnull != line; line = line->mNext) {
    nsVoidArray* floaters = line->mFloaters;
    if (nsnull == floaters) {
      continue;
    }
    PRInt32 i, n = floaters->Count();
    for (i = 0; i < n; i++) {
      nsPlaceholderFrame* ph = (nsPlaceholderFrame*) floaters->ElementAt(i);
      PaintChild(aPresContext, aRenderingContext, aDirtyRect,
                 ph->GetAnchoredItem(), NS_FRAME_PAINT_LAYER_BACKGROUND);
      PaintChild(aPresContext, aRenderingContext, aDirtyRect,
                 ph->GetAnchoredItem(), NS_FRAME_PAINT_LAYER_FLOATERS);
      PaintChild(aPresContext, aRenderingContext, aDirtyRect,
                 ph->GetAnchoredItem(), NS_FRAME_PAINT_LAYER_FOREGROUND);
    }
  }
}

void
nsBlockFrame::PaintChildren(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
#ifdef NOISY_DAMAGE_REPAIR
  PRInt32 depth = 0;
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    depth = GetDepth();
  }
#endif
  for (nsLineBox* line = mLines; nsnull != line; line = line->mNext) {
    // If the line has outside children or if the line intersects the
    // dirty rect then paint the children in the line.
    if ((NS_FRAME_OUTSIDE_CHILDREN & mState) ||
        !((line->mCombinedArea.YMost() <= aDirtyRect.y) ||
          (line->mCombinedArea.y >= aDirtyRect.YMost()))) {
#ifdef NOISY_DAMAGE_REPAIR
      if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
        nsFrame::IndentBy(stdout, depth+1);
        printf("draw line=%p bounds=%d,%d,%d,%d ca=%d,%d,%d,%d\n",
               line, line->mBounds.x, line->mBounds.y,
               line->mBounds.width, line->mBounds.height,
               line->mCombinedArea.x, line->mCombinedArea.y,
               line->mCombinedArea.width, line->mCombinedArea.height);
      }
#endif
      nsIFrame* kid = line->mFirstChild;
      PRInt32 n = line->ChildCount();
      while (--n >= 0) {
        PaintChild(aPresContext, aRenderingContext, aDirtyRect, kid,
                   aWhichLayer);
        kid->GetNextSibling(&kid);
      }
    }
#ifdef NOISY_DAMAGE_REPAIR
    else {
      if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
        nsFrame::IndentBy(stdout, depth+1);
        printf("skip line=%p bounds=%d,%d,%d,%d ca=%d,%d,%d,%d\n",
               line, line->mBounds.x, line->mBounds.y,
               line->mBounds.width, line->mBounds.height,
               line->mCombinedArea.x, line->mCombinedArea.y,
               line->mCombinedArea.width, line->mCombinedArea.height);
      }
    }
#endif  
  }

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    if ((nsnull != mBullet) && HaveOutsideBullet()) {
      // Paint outside bullets manually
      PaintChild(aPresContext, aRenderingContext, aDirtyRect, mBullet,
                 aWhichLayer);
    }
  }
}

NS_IMETHODIMP
nsBlockFrame::HandleEvent(nsIPresContext& aPresContext, 
                          nsGUIEvent*     aEvent,
                          nsEventStatus&  aEventStatus)
{
  return NS_OK;
}


NS_IMETHODIMP
nsBlockFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  nsresult rv = GetFrameForPointUsing(aPoint, nsnull, aFrame);
  if (NS_OK == rv) {
    return NS_OK;
  }
  if (nsnull != mBullet) {
    rv = GetFrameForPointUsing(aPoint, nsLayoutAtoms::bulletList, aFrame);
    if (NS_OK == rv) {
      return NS_OK;
    }
  }
  if (mFloaters.NotEmpty()) {
    rv = GetFrameForPointUsing(aPoint, nsLayoutAtoms::floaterList, aFrame);
    if (NS_OK == rv) {
      return NS_OK;
    }
  }
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Debugging

#ifdef NS_DEBUG
static PRBool
InLineList(nsLineBox* aLines, nsIFrame* aFrame)
{
  while (nsnull != aLines) {
    nsIFrame* frame = aLines->mFirstChild;
    PRInt32 n = aLines->ChildCount();
    while (--n >= 0) {
      if (frame == aFrame) {
        return PR_TRUE;
      }
      frame->GetNextSibling(&frame);
    }
    aLines = aLines->mNext;
  }
  return PR_FALSE;
}

static PRBool
InSiblingList(nsLineBox* aLine, nsIFrame* aFrame)
{
  if (nsnull != aLine) {
    nsIFrame* frame = aLine->mFirstChild;
    while (nsnull != frame) {
      if (frame == aFrame) {
        return PR_TRUE;
      }
      frame->GetNextSibling(&frame);
    }
  }
  return PR_FALSE;
}

PRBool
nsBlockFrame::IsChild(nsIFrame* aFrame)
{
  nsIFrame* parent;
  aFrame->GetParent(&parent);
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

NS_IMETHODIMP
nsBlockFrame::VerifyTree() const
{
  // XXX rewrite this
  return NS_OK;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
nsBlockFrame::Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
  if (aPrevInFlow) {
    // Copy over the block/area frame flags
    nsBlockFrame*  blockFrame = (nsBlockFrame*)aPrevInFlow;

    SetFlags(blockFrame->mFlags);
  }

  nsresult rv = nsBlockFrameSuper::Init(aPresContext, aContent, aParent,
                                        aContext, aPrevInFlow);
  if (nsBlockReflowContext::IsHTMLParagraph(this)) {
    mState |= NS_BLOCK_IS_HTML_PARAGRAPH;
  }
  return rv;
}

nsIStyleContext*
nsBlockFrame::GetFirstLineStyle(nsIPresContext* aPresContext)
{
  nsIStyleContext* fls;
  aPresContext->ProbePseudoStyleContextFor(mContent,
                                           nsHTMLAtoms::firstLinePseudo,
                                           mStyleContext, PR_FALSE, &fls);
  return fls;
}

NS_IMETHODIMP
nsBlockFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;

  if (nsLayoutAtoms::floaterList == aListName) {
    mFloaters.SetFrames(aChildList);
  }
  else {

    // Lookup up the two pseudo style contexts
    nsIStyleContext* firstLineStyle = nsnull;
    if (nsnull == mPrevInFlow) {
      firstLineStyle = GetFirstLineStyle(&aPresContext);
      if (nsnull != firstLineStyle) {
        mState |= NS_BLOCK_HAS_FIRST_LINE_STYLE;
#ifdef NOISY_FIRST_LINE
        ListTag(stdout);
        printf(": first-line style found\n");
#endif
      }
    }

    rv = AddFrames(&aPresContext, aChildList, nsnull);
    NS_IF_RELEASE(firstLineStyle);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Create list bullet if this is a list-item. Note that this is done
    // here so that RenumberLists will work (it needs the bullets to
    // store the bullet numbers).
    const nsStyleDisplay* styleDisplay;
    GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
    if ((nsnull == mPrevInFlow) &&
        (NS_STYLE_DISPLAY_LIST_ITEM == styleDisplay->mDisplay) &&
        (nsnull == mBullet)) {
      // Resolve style for the bullet frame
      nsIStyleContext* kidSC;
      aPresContext.ResolvePseudoStyleContextFor(mContent, 
                                             nsHTMLAtoms::mozListBulletPseudo,
                                             mStyleContext, PR_FALSE, &kidSC);

      // Create bullet frame
      mBullet = new nsBulletFrame;
      if (nsnull == mBullet) {
        NS_RELEASE(kidSC);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mBullet->Init(aPresContext, mContent, this, kidSC, nsnull);
      NS_RELEASE(kidSC);

      // If the list bullet frame should be positioned inside then add
      // it to the flow now.
      const nsStyleList* styleList;
      GetStyleData(eStyleStruct_List, (const nsStyleStruct*&) styleList);
      if (NS_STYLE_LIST_STYLE_POSITION_INSIDE ==
          styleList->mListStylePosition) {
        AddFrames(&aPresContext, mBullet, nsnull);
        mState &= ~NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET;
      }
      else {
        mState |= NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET;
      }
    }
  }

  return NS_OK;
}

void
nsBlockFrame::RenumberLists()
{
  // Setup initial list ordinal value
  PRInt32 ordinal = 1;
  nsIHTMLContent* hc;
  if (mContent && (NS_OK == mContent->QueryInterface(kIHTMLContentIID, (void**) &hc))) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        hc->GetHTMLAttribute(nsHTMLAtoms::start, value)) {
      if (eHTMLUnit_Integer == value.GetUnit()) {
        ordinal = value.GetIntValue();
        if (ordinal <= 0) {
          ordinal = 1;
        }
      }
    }
    NS_RELEASE(hc);
  }

  // Get to first-in-flow
  nsBlockFrame* block = this;
  while (nsnull != block->mPrevInFlow) {
    block = (nsBlockFrame*) block->mPrevInFlow;
  }

  // For each flow-block...
  while (nsnull != block) {
    // For each frame in the flow-block...
    nsIFrame* frame = block->mLines ? block->mLines->mFirstChild : nsnull;
    while (nsnull != frame) {
      // If the frame is a list-item and the frame implements our
      // block frame API then get it's bullet and set the list item
      // ordinal.
      const nsStyleDisplay* display;
      frame->GetStyleData(eStyleStruct_Display,
                          (const nsStyleStruct*&) display);
      if (NS_STYLE_DISPLAY_LIST_ITEM == display->mDisplay) {
        // Make certain that the frame isa block-frame in case
        // something foriegn has crept in.
        nsBlockFrame* listItem;
        if (NS_OK == frame->QueryInterface(kBlockFrameCID,
                                           (void**) &listItem)) {
          if (nsnull != listItem->mBullet) {
            ordinal =
              listItem->mBullet->SetListItemOrdinal(ordinal);
          }
        }
      }
      frame->GetNextSibling(&frame);
    }
    block = (nsBlockFrame*) block->mNextInFlow;
  }
}

void
nsBlockFrame::ReflowBullet(nsBlockReflowState& aState,
                           nsHTMLReflowMetrics& aMetrics)
{
  // Reflow the bullet now
  nsSize availSize;
  availSize.width = NS_UNCONSTRAINEDSIZE;
  availSize.height = NS_UNCONSTRAINEDSIZE;
  nsHTMLReflowState reflowState(*aState.mPresContext, aState.mReflowState,
                                mBullet, availSize);
  reflowState.lineLayout = aState.mLineLayout;
  nsIHTMLReflow* htmlReflow;
  nsresult rv = mBullet->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  if (NS_SUCCEEDED(rv)) {
    nsReflowStatus  status;
    htmlReflow->WillReflow(*aState.mPresContext);
    htmlReflow->Reflow(*aState.mPresContext, aMetrics, reflowState, status);
    htmlReflow->DidReflow(*aState.mPresContext, NS_FRAME_REFLOW_FINISHED);
  }

  // Place the bullet now; use its right margin to distance it
  // from the rest of the frames in the line
  const nsMargin& bp = aState.BorderPadding();
  nscoord x = bp.left - reflowState.computedMargin.right - aMetrics.width;

  // Approximate the bullets position; vertical alignment will provide
  // the final vertical location.
  nscoord y = bp.top;
  mBullet->SetRect(nsRect(x, y, aMetrics.width, aMetrics.height));
}

//XXX get rid of this -- its slow
void
nsBlockFrame::BuildFloaterList()
{
  nsIFrame* head = nsnull;
  nsIFrame* current = nsnull;
  nsLineBox* line = mLines;
  while (nsnull != line) {
    if (nsnull != line->mFloaters) {
      nsVoidArray& array = *line->mFloaters;
      PRInt32 i, n = array.Count();
      for (i = 0; i < n; i++) {
        nsPlaceholderFrame* ph = (nsPlaceholderFrame*) array[i];
        nsIFrame* floater = ph->GetAnchoredItem();
        if (nsnull == head) {
          current = head = floater;
        }
        else {
          current->SetNextSibling(floater);
          current = floater;
        }
      }
    }
    line = line->mNext;
  }

  // Terminate end of floater list just in case a floater was removed
  if (nsnull != current) {
    current->SetNextSibling(nsnull);
  }
  mFloaters.SetFrames(head);
}

// XXX keep the text-run data in the first-in-flow of the block

// XXX Switch to an interface to pass to child frames -or- do the
// grovelling directly ourselves?
nsresult
nsBlockFrame::ComputeTextRuns(nsIPresContext* aPresContext)
{
  // Destroy old run information first
  nsTextRun::DeleteTextRuns(mTextRuns);
  mTextRuns = nsnull;

  nsLineLayout textRunThingy(*aPresContext);

  // Ask each child to find its text runs
  nsLineBox* line = mLines;
  while (nsnull != line) {
    if (!line->IsBlock()) {
      nsIFrame* frame = line->mFirstChild;
      PRInt32 n = line->ChildCount();
      while (--n >= 0) {
        nsIHTMLReflow* hr;
        if (NS_OK == frame->QueryInterface(kIHTMLReflowIID, (void**)&hr)) {
          nsresult rv = hr->FindTextRuns(textRunThingy);
          if (NS_OK != rv) {
            return rv;
          }
        }
        else {
          // A frame that doesn't implement nsIHTMLReflow isn't text
          // therefore it will end an open text run.
          textRunThingy.EndTextRun();
        }
        frame->GetNextSibling(&frame);
      }
    }
    else {
      // A block frame isn't text therefore it will end an open text
      // run.
      textRunThingy.EndTextRun();
    }
    line = line->mNext;
  }
  textRunThingy.EndTextRun();

  // Now take the text-runs away from the line layout engine.
  mTextRuns = textRunThingy.TakeTextRuns();
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
NS_NewAnonymousBlockFrame(nsIFrame** aNewFrame)
{
  nsAnonymousBlockFrame* it = new nsAnonymousBlockFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsAnonymousBlockFrame::nsAnonymousBlockFrame()
{
}

nsAnonymousBlockFrame::~nsAnonymousBlockFrame()
{
}

NS_IMETHODIMP
nsAnonymousBlockFrame::AppendFrames(nsIPresContext& aPresContext,
                                    nsIPresShell&   aPresShell,
                                    nsIAtom*        aListName,
                                    nsIFrame*       aFrameList)
{
  return mParent->AppendFrames(aPresContext, aPresShell, aListName,
                               aFrameList);
}

NS_IMETHODIMP
nsAnonymousBlockFrame::InsertFrames(nsIPresContext& aPresContext,
                                    nsIPresShell&   aPresShell,
                                    nsIAtom*        aListName,
                                    nsIFrame*       aPrevFrame,
                                    nsIFrame*       aFrameList)
{
  return mParent->InsertFrames(aPresContext, aPresShell, aListName,
                               aPrevFrame, aFrameList);
}

NS_IMETHODIMP
nsAnonymousBlockFrame::RemoveFrame(nsIPresContext& aPresContext,
                                   nsIPresShell&   aPresShell,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aOldFrame)
{
  return mParent->RemoveFrame(aPresContext, aPresShell, aListName,
                              aOldFrame);
}

nsresult
nsAnonymousBlockFrame::AppendFrames2(nsIPresContext* aPresContext,
                                     nsIPresShell*   aPresShell,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aFrameList)
{
  return nsAnonymousBlockFrameSuper::AppendFrames(*aPresContext, *aPresShell,
                                                  aListName, aFrameList);
}

nsresult
nsAnonymousBlockFrame::InsertFrames2(nsIPresContext* aPresContext,
                                     nsIPresShell*   aPresShell,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aPrevFrame,
                                     nsIFrame*       aFrameList)
{
  return nsAnonymousBlockFrameSuper::InsertFrames(*aPresContext, *aPresShell,
                                                  aListName, aPrevFrame,
                                                  aFrameList);
}

nsresult
nsAnonymousBlockFrame::RemoveFrame2(nsIPresContext* aPresContext,
                                    nsIPresShell*   aPresShell,
                                    nsIAtom*        aListName,
                                    nsIFrame*       aOldFrame)
{
  return nsAnonymousBlockFrameSuper::RemoveFrame(*aPresContext, *aPresShell,
                                                 aListName, aOldFrame);
}

void
nsAnonymousBlockFrame::RemoveFirstFrame()
{
  nsLineBox* line = mLines;
  if (nsnull != line) {
    nsIFrame* firstChild = line->mFirstChild;

    // If the line has floaters on it, see if the frame being removed
    // is a placeholder frame. If it is, then remove it from the lines
    // floater array and from the block frames floater child list.
    if (nsnull != line->mFloaters) {
      // XXX UNTESTED!
      nsPlaceholderFrame* placeholderFrame;
      nsVoidArray& floaters = *line->mFloaters;
      PRInt32 i, n = floaters.Count();
      for (i = 0; i < n; i++) {
        placeholderFrame = (nsPlaceholderFrame*) floaters[i];
        if (firstChild == placeholderFrame) {
          // Remove placeholder from the line's floater array
          floaters.RemoveElementAt(i);
          if (0 == floaters.Count()) {
            delete line->mFloaters;
            line->mFloaters = nsnull;
          }

          // Remove the floater from the block frames mFloaters list too
          mFloaters.RemoveFrame(placeholderFrame->GetAnchoredItem());
          break;
        }
      }
    }

    if (1 == line->mChildCount) {
      // Remove line when last frame goes away
      mLines = line->mNext;
      delete line;
    }
    else {
      // Remove frame from line and mark the line dirty
      --line->mChildCount;
      NS_ASSERTION(line->mChildCount < 10000, "bad inline count"); 
      line->MarkDirty();
      firstChild->GetNextSibling(&line->mFirstChild);
    }

    // Break linkage to next child after stolen frame
    firstChild->SetNextSibling(nsnull);
  }
#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
}

void
nsAnonymousBlockFrame::RemoveFramesFrom(nsIFrame* aFrame)
{
  nsLineBox* line = mLines;
  if (nsnull != line) {
    // Chop the child sibling list into two pieces
    nsFrameList tmp(line->mFirstChild);
    nsIFrame* prevSibling = tmp.GetPrevSiblingFor(aFrame);
    if (nsnull != prevSibling) {
      // Chop the sibling list into two pieces
      prevSibling->SetNextSibling(nsnull);

      nsLineBox* prevLine = nsnull;
      while (nsnull != line) {
        nsIFrame* frame = line->mFirstChild;
        PRInt32 i, n = line->mChildCount;
        PRBool done = PR_FALSE;
        for (i = 0; i < n; i++) {
          if (frame == aFrame) {
            // We just found the target frame (and the line its in and
            // the previous line)
            if (frame == line->mFirstChild) {
              // No more children on this line, so let it get removed
              prevLine->mNext = nsnull;
            }
            else {
              // The only frames that remain on this line are the
              // frames preceeding aFrame. Adjust the count to
              // indicate that fact.
              line->mChildCount = i;

              // Remove the lines that follow this line
              prevLine = line;
              line = line->mNext;
              prevLine->mNext = nsnull;
            }
            done = PR_TRUE;
            break;
          }
          frame->GetNextSibling(&frame);
        }
        if (done) {
          break;
        }
        prevLine = line;
        line = line->mNext;
      }
    }

    // Remove all of the remaining lines
    while (nsnull != line) {
      nsLineBox* next = line->mNext;
      delete line;
      line = next;
    }
  }
#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
}

#ifdef DEBUG
void
nsBlockFrame::VerifyLines(PRBool aFinalCheckOK)
{
  nsLineBox* line = mLines;
  if (!line) {
    return;
  }

  // Add up the counts on each line. Also validate that IsFirstLine is
  // set properly.
  PRInt32 count = 0;
  PRBool seenBlock = PR_FALSE;
  while (nsnull != line) {
    if (aFinalCheckOK) {
      if (line->IsBlock()) {
        seenBlock = PR_TRUE;
      }
      if (line->IsFirstLine() || line->IsBlock()) {
        NS_ASSERTION(1 == line->mChildCount, "bad first line");
      }
      if (NS_BLOCK_HAS_FIRST_LINE_STYLE & mState) {
        if (seenBlock) {
          NS_ASSERTION(!line->IsFirstLine(), "bad first line");
        }
        else {
          NS_ASSERTION(line->IsFirstLine(), "bad first line");
        }
      }
    }
    NS_ASSERTION(line->mChildCount < 10000, "bad line child count");
    count += line->mChildCount;
    line = line->mNext;
  }

  // Then count the frames
  PRInt32 frameCount = 0;
  nsIFrame* frame = mLines->mFirstChild;
  while (nsnull != frame) {
    frameCount++;
    frame->GetNextSibling(&frame);
  }
  NS_ASSERTION(count == frameCount, "bad line list");

  // Next: test that each line has right number of frames on it
  line = mLines;
  nsLineBox* prevLine = nsnull;
  while (nsnull != line) {
    count = line->mChildCount;
    frame = line->mFirstChild;
    while (--count >= 0) {
      frame->GetNextSibling(&frame);
    }
    prevLine = line;
    line = line->mNext;
    if ((nsnull != line) && (0 != line->mChildCount)) {
      NS_ASSERTION(frame == line->mFirstChild, "bad line list");
    }
  }
}

// Its possible that a frame can have some frames on an overflow
// list. But its never possible for multiple frames to have overflow
// lists. Check that this fact is actually true.
void
nsBlockFrame::VerifyOverflowSituation()
{
  PRBool haveOverflow = PR_FALSE;
  nsBlockFrame* flow = (nsBlockFrame*) GetFirstInFlow();
  while (nsnull != flow) {
    if (nsnull != flow->mOverflowLines) {
      NS_ASSERTION(nsnull != flow->mOverflowLines->mFirstChild,
                   "bad overflow list");
      NS_ASSERTION(!haveOverflow, "two frames with overflow lists");
      haveOverflow = PR_TRUE;
    }
    flow = (nsBlockFrame*) flow->mNextInFlow;
  }
}

PRInt32
nsBlockFrame::GetDepth() const
{
  PRInt32 depth = 0;
  nsIFrame* parent = mParent;
  while (nsnull != parent) {
    parent->GetParent(&parent);
    depth++;
  }
  return depth;
}
#endif
