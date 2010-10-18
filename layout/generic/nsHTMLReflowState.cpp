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

/* struct containing the input to nsIFrame::Reflow */

#include "nsCOMPtr.h"
#include "nsStyleConsts.h"
#include "nsCSSAnonBoxes.h"
#include "nsFrame.h"
#include "nsIContent.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDeviceContext.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsBlockFrame.h"
#include "nsLineBox.h"
#include "nsImageFrame.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsIServiceManager.h"
#include "nsIPercentHeightObserver.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#ifdef IBMBIDI
#include "nsBidiUtils.h"
#endif

#ifdef NS_DEBUG
#undef NOISY_VERTICAL_ALIGN
#else
#undef NOISY_VERTICAL_ALIGN
#endif

using namespace mozilla;

// Prefs-driven control for |text-decoration: blink|
static PRPackedBool sPrefIsLoaded = PR_FALSE;
static PRPackedBool sBlinkIsAllowed = PR_TRUE;

enum eNormalLineHeightControl {
  eUninitialized = -1,
  eNoExternalLeading = 0,   // does not include external leading 
  eIncludeExternalLeading,  // use whatever value font vendor provides
  eCompensateLeading        // compensate leading if leading provided by font vendor is not enough
};

static eNormalLineHeightControl sNormalLineHeightControl = eUninitialized;

// Initialize a <b>root</b> reflow state with a rendering context to
// use for measuring things.
nsHTMLReflowState::nsHTMLReflowState(nsPresContext*       aPresContext,
                                     nsIFrame*            aFrame,
                                     nsIRenderingContext* aRenderingContext,
                                     const nsSize&        aAvailableSpace)
  : nsCSSOffsetState(aFrame, aRenderingContext)
  , mBlockDelta(0)
  , mReflowDepth(0)
{
  NS_PRECONDITION(aPresContext, "no pres context");
  NS_PRECONDITION(aRenderingContext, "no rendering context");
  NS_PRECONDITION(aFrame, "no frame");
  parentReflowState = nsnull;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;
  mFloatManager = nsnull;
  mLineLayout = nsnull;
  mFlags.mSpecialHeightReflow = PR_FALSE;
  mFlags.mIsTopOfPage = PR_FALSE;
  mFlags.mTableIsSplittable = PR_FALSE;
  mFlags.mNextInFlowUntouched = PR_FALSE;
  mFlags.mAssumingHScrollbar = mFlags.mAssumingVScrollbar = PR_FALSE;
  mFlags.mHasClearance = PR_FALSE;
  mFlags.mHeightDependsOnAncestorCell = PR_FALSE;
  mDiscoveredClearance = nsnull;
  mPercentHeightObserver = nsnull;
  Init(aPresContext);
}

static PRBool CheckNextInFlowParenthood(nsIFrame* aFrame, nsIFrame* aParent)
{
  nsIFrame* frameNext = aFrame->GetNextInFlow();
  nsIFrame* parentNext = aParent->GetNextInFlow();
  return frameNext && parentNext && frameNext->GetParent() == parentNext;
}

// Initialize a reflow state for a child frames reflow. Some state
// is copied from the parent reflow state; the remaining state is
// computed.
nsHTMLReflowState::nsHTMLReflowState(nsPresContext*           aPresContext,
                                     const nsHTMLReflowState& aParentReflowState,
                                     nsIFrame*                aFrame,
                                     const nsSize&            aAvailableSpace,
                                     nscoord                  aContainingBlockWidth,
                                     nscoord                  aContainingBlockHeight,
                                     PRBool                   aInit)
  : nsCSSOffsetState(aFrame, aParentReflowState.rendContext)
  , mBlockDelta(0)
  , mReflowDepth(aParentReflowState.mReflowDepth + 1)
  , mFlags(aParentReflowState.mFlags)
{
  NS_PRECONDITION(aPresContext, "no pres context");
  NS_PRECONDITION(aFrame, "no frame");
  NS_PRECONDITION((aContainingBlockWidth == -1) ==
                    (aContainingBlockHeight == -1),
                  "cb width and height should only be non-default together");
  NS_PRECONDITION(aInit == PR_TRUE || aInit == PR_FALSE,
                  "aInit out of range for PRBool");
  NS_PRECONDITION(!mFlags.mSpecialHeightReflow ||
                  !NS_SUBTREE_DIRTY(aFrame),
                  "frame should be clean when getting special height reflow");

  parentReflowState = &aParentReflowState;

  // If the parent is dirty, then the child is as well.
  // XXX Are the other cases where the parent reflows a child a second
  // time, as a resize?
  if (!mFlags.mSpecialHeightReflow)
    frame->AddStateBits(parentReflowState->frame->GetStateBits() &
                        NS_FRAME_IS_DIRTY);

  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;

  mFloatManager = aParentReflowState.mFloatManager;
  if (frame->IsFrameOfType(nsIFrame::eLineParticipant))
    mLineLayout = aParentReflowState.mLineLayout;
  else
    mLineLayout = nsnull;
  mFlags.mIsTopOfPage = aParentReflowState.mFlags.mIsTopOfPage;
  mFlags.mNextInFlowUntouched = aParentReflowState.mFlags.mNextInFlowUntouched &&
    CheckNextInFlowParenthood(aFrame, aParentReflowState.frame);
  mFlags.mAssumingHScrollbar = mFlags.mAssumingVScrollbar = PR_FALSE;
  mFlags.mHasClearance = PR_FALSE;
  mDiscoveredClearance = nsnull;
  mPercentHeightObserver = (aParentReflowState.mPercentHeightObserver && 
                            aParentReflowState.mPercentHeightObserver->NeedsToObserve(*this)) 
                           ? aParentReflowState.mPercentHeightObserver : nsnull;

  if (aInit) {
    Init(aPresContext, aContainingBlockWidth, aContainingBlockHeight);
  }
}

inline nscoord
nsCSSOffsetState::ComputeWidthValue(nscoord aContainingBlockWidth,
                                    nscoord aContentEdgeToBoxSizing,
                                    nscoord aBoxSizingToMarginEdge,
                                    const nsStyleCoord& aCoord)
{
  return nsLayoutUtils::ComputeWidthValue(rendContext, frame,
                                          aContainingBlockWidth,
                                          aContentEdgeToBoxSizing,
                                          aBoxSizingToMarginEdge,
                                          aCoord);
}

nscoord
nsCSSOffsetState::ComputeWidthValue(nscoord aContainingBlockWidth,
                                    PRUint8 aBoxSizing,
                                    const nsStyleCoord& aCoord)
{
  nscoord inside = 0, outside = mComputedBorderPadding.LeftRight() +
                                mComputedMargin.LeftRight();
  switch (aBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      inside = mComputedBorderPadding.LeftRight();
      break;
    case NS_STYLE_BOX_SIZING_PADDING:
      inside = mComputedPadding.LeftRight();
      break;
  }
  outside -= inside;

  return ComputeWidthValue(aContainingBlockWidth, inside,
                           outside, aCoord);
}

void
nsHTMLReflowState::SetComputedWidth(nscoord aComputedWidth)
{
  NS_ASSERTION(frame, "Must have a frame!");
  // It'd be nice to assert that |frame| is not in reflow, but this fails for
  // two reasons:
  //
  // 1) Viewport frames reset the computed width on a copy of their reflow
  //    state when reflowing fixed-pos kids.  In that case we actually don't
  //    want to mess with the resize flags, because comparing the frame's rect
  //    to the munged computed width is pointless.
  // 2) nsFrame::BoxReflow creates a reflow state for its parent.  This reflow
  //    state is not used to reflow the parent, but just as a parent for the
  //    frame's own reflow state.  So given a nsBoxFrame inside some non-XUL
  //    (like a text control, for example), we'll end up creating a reflow
  //    state for the parent while the parent is reflowing.

  NS_PRECONDITION(aComputedWidth >= 0, "Invalid computed width");
  if (mComputedWidth != aComputedWidth) {
    mComputedWidth = aComputedWidth;
    if (frame->GetType() != nsGkAtoms::viewportFrame) { // Or check GetParent()?
      InitResizeFlags(frame->PresContext());
    }
  }
}

void
nsHTMLReflowState::SetComputedHeight(nscoord aComputedHeight)
{
  NS_ASSERTION(frame, "Must have a frame!");
  // It'd be nice to assert that |frame| is not in reflow, but this fails
  // because:
  //
  //    nsFrame::BoxReflow creates a reflow state for its parent.  This reflow
  //    state is not used to reflow the parent, but just as a parent for the
  //    frame's own reflow state.  So given a nsBoxFrame inside some non-XUL
  //    (like a text control, for example), we'll end up creating a reflow
  //    state for the parent while the parent is reflowing.

  NS_PRECONDITION(aComputedHeight >= 0, "Invalid computed height");
  if (mComputedHeight != aComputedHeight) {
    mComputedHeight = aComputedHeight;
    InitResizeFlags(frame->PresContext());
  }
}

void
nsHTMLReflowState::Init(nsPresContext* aPresContext,
                        nscoord         aContainingBlockWidth,
                        nscoord         aContainingBlockHeight,
                        const nsMargin* aBorder,
                        const nsMargin* aPadding)
{
  NS_WARN_IF_FALSE(availableWidth != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");

  mStylePosition = frame->GetStylePosition();
  mStyleDisplay = frame->GetStyleDisplay();
  mStyleVisibility = frame->GetStyleVisibility();
  mStyleBorder = frame->GetStyleBorder();
  mStyleMargin = frame->GetStyleMargin();
  mStylePadding = frame->GetStylePadding();
  mStyleText = frame->GetStyleText();

  InitFrameType();
  InitCBReflowState();

  InitConstraints(aPresContext, aContainingBlockWidth, aContainingBlockHeight, aBorder, aPadding);

  InitResizeFlags(aPresContext);

  NS_WARN_IF_FALSE((mFrameType == NS_CSS_FRAME_TYPE_INLINE &&
                    !frame->IsFrameOfType(nsIFrame::eReplaced)) ||
                   frame->GetType() == nsGkAtoms::textFrame ||
                   mComputedWidth != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
}

void nsHTMLReflowState::InitCBReflowState()
{
  if (!parentReflowState) {
    mCBReflowState = nsnull;
    return;
  }

  // If outer tables ever become containing blocks, we need to make sure to use
  // their mCBReflowState in the non-absolutely-positioned case for inner
  // tables.
  NS_ASSERTION(frame->GetType() != nsGkAtoms::tableFrame ||
               !frame->GetParent()->IsContainingBlock(),
               "Outer table should not be containing block");

  if (parentReflowState->frame->IsContainingBlock() ||
      // Absolutely positioned frames should always be kids of the frames that
      // determine their containing block....
      (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE)) {
    // an absolutely positioned inner table needs to use the parent of
    // the outer table.  So the above comment about absolutely
    // positioned frames is sort of a lie.
    if (parentReflowState->parentReflowState &&
        frame->GetType() == nsGkAtoms::tableFrame) {
      mCBReflowState = parentReflowState->parentReflowState;
    } else {
      mCBReflowState = parentReflowState;
    }
      
    return;
  }
  
  mCBReflowState = parentReflowState->mCBReflowState;
}

/* Check whether CalcQuirkContainingBlockHeight would stop on the
 * given reflow state, using its block as a height.  (essentially 
 * returns false for any case in which CalcQuirkContainingBlockHeight 
 * has a "continue" in its main loop.)
 *
 * XXX Maybe refactor CalcQuirkContainingBlockHeight so it uses 
 * this function as well
 */
static PRBool
IsQuirkContainingBlockHeight(const nsHTMLReflowState* rs) 
{
  nsIAtom* frameType = rs->frame->GetType();
  if (nsGkAtoms::blockFrame == frameType ||
#ifdef MOZ_XUL
      nsGkAtoms::XULLabelFrame == frameType ||
#endif
      nsGkAtoms::scrollFrame == frameType) {
    // Note: This next condition could change due to a style change,
    // but that would cause a style reflow anyway, which means we're ok.
    if (NS_AUTOHEIGHT == rs->ComputedHeight()) {
      if (!rs->frame->GetStyleDisplay()->IsAbsolutelyPositioned()) {
        return PR_FALSE;
      }
    }
  }
  return PR_TRUE;
}


void
nsHTMLReflowState::InitResizeFlags(nsPresContext* aPresContext)
{
  mFlags.mHResize = !(frame->GetStateBits() & NS_FRAME_IS_DIRTY) &&
                    frame->GetSize().width !=
                      mComputedWidth + mComputedBorderPadding.LeftRight();

  // XXX Should we really need to null check mCBReflowState?  (We do for
  // at least nsBoxFrame).
  if (IS_TABLE_CELL(frame->GetType()) &&
      (mFlags.mSpecialHeightReflow ||
       (frame->GetFirstInFlow()->GetStateBits() &
         NS_TABLE_CELL_HAD_SPECIAL_REFLOW)) &&
      (frame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT)) {
    // Need to set the bit on the cell so that
    // mCBReflowState->mFlags.mVResize is set correctly below when
    // reflowing descendant.
    mFlags.mVResize = PR_TRUE;
  } else if (mCBReflowState && !frame->IsContainingBlock()) {
    // XXX Is this problematic for relatively positioned inlines acting
    // as containing block for absolutely positioned elements?
    // Possibly; in that case we should at least be checking
    // NS_SUBTREE_DIRTY, I'd think.
    mFlags.mVResize = mCBReflowState->mFlags.mVResize;
  } else if (mComputedHeight == NS_AUTOHEIGHT) {
    if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
        mCBReflowState) {
      mFlags.mVResize = mCBReflowState->mFlags.mVResize;
    } else {
      mFlags.mVResize = mFlags.mHResize;
    }
    mFlags.mVResize = mFlags.mVResize || NS_SUBTREE_DIRTY(frame);
  } else {
    // not 'auto' height
    mFlags.mVResize = frame->GetSize().height !=
                        mComputedHeight + mComputedBorderPadding.TopBottom();
  }

  PRBool dependsOnCBHeight =
    (mStylePosition->HeightDependsOnContainer() &&
     // FIXME: condition this on not-abspos?
     mStylePosition->mHeight.GetUnit() != eStyleUnit_Auto) ||
    (mStylePosition->MinHeightDependsOnContainer() &&
     // FIXME: condition this on not-abspos?
     mStylePosition->mMinHeight.GetUnit() != eStyleUnit_Auto) ||
    (mStylePosition->MaxHeightDependsOnContainer() &&
     // FIXME: condition this on not-abspos?
     mStylePosition->mMaxHeight.GetUnit() != eStyleUnit_Auto) ||
    mStylePosition->OffsetHasPercent(NS_SIDE_TOP) ||
    mStylePosition->mOffset.GetBottomUnit() != eStyleUnit_Auto ||
    frame->IsBoxFrame() ||
    (mStylePosition->mHeight.GetUnit() == eStyleUnit_Auto &&
     frame->GetIntrinsicSize().height.GetUnit() == eStyleUnit_Percent);

  if (mStyleText->mLineHeight.GetUnit() == eStyleUnit_Enumerated) {
    NS_ASSERTION(mStyleText->mLineHeight.GetIntValue() ==
                 NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT,
                 "bad line-height value");

    // line-height depends on block height
    frame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
    // but only on containing blocks if this frame is not a suitable block
    dependsOnCBHeight |= !frame->IsContainingBlock();
  }

  // If we're the descendant of a table cell that performs special height
  // reflows and we could be the child that requires them, always set
  // the vertical resize in case this is the first pass before the
  // special height reflow.  However, don't do this if it actually is
  // the special height reflow, since in that case it will already be
  // set correctly above if we need it set.
  if (!mFlags.mVResize && mCBReflowState &&
      (IS_TABLE_CELL(mCBReflowState->frame->GetType()) || 
       mCBReflowState->mFlags.mHeightDependsOnAncestorCell) &&
      !mCBReflowState->mFlags.mSpecialHeightReflow && 
      dependsOnCBHeight) {
    mFlags.mVResize = PR_TRUE;
    mFlags.mHeightDependsOnAncestorCell = PR_TRUE;
  }

  // Set NS_FRAME_CONTAINS_RELATIVE_HEIGHT if it's needed.

  // It would be nice to check that |mComputedHeight != NS_AUTOHEIGHT|
  // &&ed with the percentage height check.  However, this doesn't get
  // along with table special height reflows, since a special height
  // reflow (a quirk that makes such percentage heights work on children
  // of table cells) can cause not just a single percentage height to
  // become fixed, but an entire descendant chain of percentage heights
  // to become fixed.
  if (dependsOnCBHeight && mCBReflowState) {
    const nsHTMLReflowState *rs = this;
    PRBool hitCBReflowState = PR_FALSE;
    do {
      rs = rs->parentReflowState;
      if (!rs) {
        break;
      }
        
      if (rs->frame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT)
        break; // no need to go further
      rs->frame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
      
      // Keep track of whether we've hit the containing block, because
      // we need to go at least that far.
      if (rs == mCBReflowState) {
        hitCBReflowState = PR_TRUE;
      }

    } while (!hitCBReflowState ||
             (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
              !IsQuirkContainingBlockHeight(rs)));
    // Note: We actually don't need to set the
    // NS_FRAME_CONTAINS_RELATIVE_HEIGHT bit for the cases
    // where we hit the early break statements in
    // CalcQuirkContainingBlockHeight. But it doesn't hurt
    // us to set the bit in these cases.
    
  }
  if (frame->GetStateBits() & NS_FRAME_IS_DIRTY) {
    // If we're reflowing everything, then we'll find out if we need
    // to re-set this.
    frame->RemoveStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
  }
}

/* static */
nscoord
nsHTMLReflowState::GetContainingBlockContentWidth(const nsHTMLReflowState* aReflowState)
{
  const nsHTMLReflowState* rs = aReflowState->mCBReflowState;
  if (!rs)
    return 0;
  return rs->mComputedWidth;
}

/* static */
nsIFrame*
nsHTMLReflowState::GetContainingBlockFor(const nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "Must have frame to work with");
  nsIFrame* container = aFrame->GetParent();
  if (aFrame->GetStyleDisplay()->IsAbsolutelyPositioned()) {
    // Absolutely positioned frames are just kids of their containing
    // blocks (which may happen to be inlines).
    return container;
  }
  while (container && !container->IsContainingBlock()) {
    container = container->GetParent();
  }
  return container;
}

void
nsHTMLReflowState::InitFrameType()
{
  const nsStyleDisplay *disp = mStyleDisplay;
  nsCSSFrameType frameType;

  // Section 9.7 of the CSS2 spec indicates that absolute position
  // takes precedence over float which takes precedence over display.
  // XXXldb nsRuleNode::ComputeDisplayData should take care of this, right?
  // Make sure the frame was actually moved out of the flow, and don't

  // just assume what the style says, because we might not have had a
  // useful float/absolute containing block
  nsIFrame* frameToTest =
    frame->GetType() == nsGkAtoms::tableFrame ? frame->GetParent() : frame;

  DISPLAY_INIT_TYPE(frameToTest, this);

  NS_ASSERTION(frameToTest->GetStyleDisplay()->IsAbsolutelyPositioned() ==
                 disp->IsAbsolutelyPositioned(),
               "Unexpected position style");
  NS_ASSERTION(frameToTest->GetStyleDisplay()->IsFloating() ==
                 disp->IsFloating(), "Unexpected float style");
  if (frameToTest->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    if (disp->IsAbsolutelyPositioned()) {
      frameType = NS_CSS_FRAME_TYPE_ABSOLUTE;
      //XXXfr hack for making frames behave properly when in overflow container lists
      //      see bug 154892; need to revisit later
      if (frameToTest->GetPrevInFlow())
        frameType = NS_CSS_FRAME_TYPE_BLOCK;
    }
    else if (disp->IsFloating()) {
      frameType = NS_CSS_FRAME_TYPE_FLOATING;
    } else {
      NS_ASSERTION(disp->mDisplay == NS_STYLE_DISPLAY_POPUP,
                   "unknown out of flow frame type");
      frameType = NS_CSS_FRAME_TYPE_UNKNOWN;
    }
  }
  else {
    switch (disp->mDisplay) {
    case NS_STYLE_DISPLAY_BLOCK:
    case NS_STYLE_DISPLAY_LIST_ITEM:
    case NS_STYLE_DISPLAY_TABLE:
    case NS_STYLE_DISPLAY_TABLE_CAPTION:
      frameType = NS_CSS_FRAME_TYPE_BLOCK;
      break;

    case NS_STYLE_DISPLAY_INLINE:
    case NS_STYLE_DISPLAY_INLINE_BLOCK:
    case NS_STYLE_DISPLAY_MARKER:
    case NS_STYLE_DISPLAY_INLINE_TABLE:
    case NS_STYLE_DISPLAY_INLINE_BOX:
    case NS_STYLE_DISPLAY_INLINE_GRID:
    case NS_STYLE_DISPLAY_INLINE_STACK:
      frameType = NS_CSS_FRAME_TYPE_INLINE;
      break;

    case NS_STYLE_DISPLAY_RUN_IN:
    case NS_STYLE_DISPLAY_COMPACT:
      // XXX need to look ahead at the frame's sibling
      frameType = NS_CSS_FRAME_TYPE_BLOCK;
      break;

    case NS_STYLE_DISPLAY_TABLE_CELL:
    case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
    case NS_STYLE_DISPLAY_TABLE_COLUMN:
    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
    case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
    case NS_STYLE_DISPLAY_TABLE_ROW:
      frameType = NS_CSS_FRAME_TYPE_INTERNAL_TABLE;
      break;

    case NS_STYLE_DISPLAY_NONE:
    default:
      frameType = NS_CSS_FRAME_TYPE_UNKNOWN;
      break;
    }
  }

  // See if the frame is replaced
  if (frame->IsFrameOfType(nsIFrame::eReplacedContainsBlock)) {
    frameType = NS_FRAME_REPLACED_CONTAINS_BLOCK(frameType);
  } else if (frame->IsFrameOfType(nsIFrame::eReplaced)) {
    frameType = NS_FRAME_REPLACED(frameType);
  }

  mFrameType = frameType;
}

void
nsHTMLReflowState::ComputeRelativeOffsets(const nsHTMLReflowState* cbrs,
                                          nscoord aContainingBlockWidth,
                                          nscoord aContainingBlockHeight,
                                          nsPresContext* aPresContext)
{
  // Compute the 'left' and 'right' values. 'Left' moves the boxes to the right,
  // and 'right' moves the boxes to the left. The computed values are always:
  // left=-right
  PRBool  leftIsAuto = eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit();
  PRBool  rightIsAuto = eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit();

  // If neither 'left' not 'right' are auto, then we're over-constrained and
  // we ignore one of them
  if (!leftIsAuto && !rightIsAuto) {
    if (mCBReflowState &&
        NS_STYLE_DIRECTION_RTL == mCBReflowState->mStyleVisibility->mDirection) {
      leftIsAuto = PR_TRUE;
    } else {
      rightIsAuto = PR_TRUE;
    }
  }

  if (leftIsAuto) {
    if (rightIsAuto) {
      // If both are 'auto' (their initial values), the computed values are 0
      mComputedOffsets.left = mComputedOffsets.right = 0;
    } else {
      // 'Right' isn't 'auto' so compute its value
      mComputedOffsets.right = nsLayoutUtils::
        ComputeWidthDependentValue(aContainingBlockWidth,
                                   mStylePosition->mOffset.GetRight());

      // Computed value for 'left' is minus the value of 'right'
      mComputedOffsets.left = -mComputedOffsets.right;
    }

  } else {
    NS_ASSERTION(rightIsAuto, "unexpected specified constraint");
    
    // 'Left' isn't 'auto' so compute its value
    mComputedOffsets.left = nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 mStylePosition->mOffset.GetLeft());

    // Computed value for 'right' is minus the value of 'left'
    mComputedOffsets.right = -mComputedOffsets.left;
  }

  // Compute the 'top' and 'bottom' values. The 'top' and 'bottom' properties
  // move relatively positioned elements up and down. They also must be each 
  // other's negative
  PRBool  topIsAuto = eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit();
  PRBool  bottomIsAuto = eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit();

  // Check for percentage based values and a containing block height that
  // depends on the content height. Treat them like 'auto'
  if (NS_AUTOHEIGHT == aContainingBlockHeight) {
    if (mStylePosition->OffsetHasPercent(NS_SIDE_TOP)) {
      topIsAuto = PR_TRUE;
    }
    if (mStylePosition->OffsetHasPercent(NS_SIDE_BOTTOM)) {
      bottomIsAuto = PR_TRUE;
    }
  }

  // If neither is 'auto', 'bottom' is ignored
  if (!topIsAuto && !bottomIsAuto) {
    bottomIsAuto = PR_TRUE;
  }

  if (topIsAuto) {
    if (bottomIsAuto) {
      // If both are 'auto' (their initial values), the computed values are 0
      mComputedOffsets.top = mComputedOffsets.bottom = 0;
    } else {
      // 'Bottom' isn't 'auto' so compute its value
      mComputedOffsets.bottom = nsLayoutUtils::
        ComputeHeightDependentValue(aContainingBlockHeight,
                                    mStylePosition->mOffset.GetBottom());
      
      // Computed value for 'top' is minus the value of 'bottom'
      mComputedOffsets.top = -mComputedOffsets.bottom;
    }

  } else {
    NS_ASSERTION(bottomIsAuto, "unexpected specified constraint");
    
    // 'Top' isn't 'auto' so compute its value
    mComputedOffsets.top = nsLayoutUtils::
      ComputeHeightDependentValue(aContainingBlockHeight,
                                  mStylePosition->mOffset.GetTop());

    // Computed value for 'bottom' is minus the value of 'top'
    mComputedOffsets.bottom = -mComputedOffsets.top;
  }

  // Store the offset
  FrameProperties props(aPresContext->PropertyTable(), frame);
  nsPoint* offsets = static_cast<nsPoint*>
    (props.Get(nsIFrame::ComputedOffsetProperty()));
  if (offsets) {
    offsets->MoveTo(mComputedOffsets.left, mComputedOffsets.top);
  } else {
    props.Set(nsIFrame::ComputedOffsetProperty(),
              new nsPoint(mComputedOffsets.left, mComputedOffsets.top));
  }
}

static nsIFrame*
GetNearestContainingBlock(nsIFrame *aFrame)
{
  nsIFrame *cb = aFrame;
  do {
    cb = cb->GetParent();
  } while (!cb->IsContainingBlock());
  return cb;
}

nsIFrame*
nsHTMLReflowState::GetHypotheticalBoxContainer(nsIFrame* aFrame,
                                               nscoord& aCBLeftEdge,
                                               nscoord& aCBWidth)
{
  aFrame = GetNearestContainingBlock(aFrame);
  NS_ASSERTION(aFrame != frame, "How did that happen?");

  /* Now aFrame is the containing block we want */

  /* Check whether the containing block is currently being reflowed.
     If so, use the info from the reflow state. */
  const nsHTMLReflowState* state;
  if (aFrame->GetStateBits() & NS_FRAME_IN_REFLOW) {
    for (state = parentReflowState; state && state->frame != aFrame;
         state = state->parentReflowState) {
      /* do nothing */
    }
  } else {
    state = nsnull;
  }
  
  if (state) {
    aCBLeftEdge = state->mComputedBorderPadding.left;
    aCBWidth = state->mComputedWidth;
  } else {
    /* Didn't find a reflow state for aFrame.  Just compute the information we
       want, on the assumption that aFrame already knows its size.  This really
       ought to be true by now. */
    NS_ASSERTION(!(aFrame->GetStateBits() & NS_FRAME_IN_REFLOW),
                 "aFrame shouldn't be in reflow; we'll lie if it is");
    nsMargin borderPadding = aFrame->GetUsedBorderAndPadding();
    aCBLeftEdge = borderPadding.left;
    aCBWidth = aFrame->GetSize().width - borderPadding.LeftRight();
  }

  return aFrame;
}

// When determining the hypothetical box that would have been if the element
// had been in the flow we may not be able to exactly determine both the left
// and right edges. For example, if the element is a non-replaced inline-level
// element we would have to reflow it in order to determine it desired width.
// In that case depending on the progression direction either the left or
// right edge would be marked as not being exact
struct nsHypotheticalBox {
  // offsets from left edge of containing block (which is a padding edge)
  nscoord       mLeft, mRight;
  // offset from top edge of containing block (which is a padding edge)
  nscoord       mTop;
#ifdef DEBUG
  PRPackedBool  mLeftIsExact, mRightIsExact;
#endif

  nsHypotheticalBox() {
#ifdef DEBUG
    mLeftIsExact = mRightIsExact = PR_FALSE;
#endif
  }
};
      
static PRBool
GetIntrinsicSizeFor(nsIFrame* aFrame, nsSize& aIntrinsicSize)
{
  // See if it is an image frame
  PRBool success = PR_FALSE;

  // Currently the only type of replaced frame that we can get the intrinsic
  // size for is an image frame
  // XXX We should add back the GetReflowMetrics() function and one of the
  // things should be the intrinsic size...
  if (aFrame->GetType() == nsGkAtoms::imageFrame) {
    nsImageFrame* imageFrame = (nsImageFrame*)aFrame;

    if (NS_SUCCEEDED(imageFrame->GetIntrinsicImageSize(aIntrinsicSize))) {
      success = (aIntrinsicSize != nsSize(0, 0));
    }
  }
  return success;
}

/**
 * aInsideBoxSizing returns the part of the horizontal padding, border,
 * and margin that goes inside the edge given by -moz-box-sizing;
 * aOutsideBoxSizing returns the rest.
 */
void
nsHTMLReflowState::CalculateHorizBorderPaddingMargin(
                       nscoord aContainingBlockWidth,
                       nscoord* aInsideBoxSizing,
                       nscoord* aOutsideBoxSizing)
{
  const nsMargin& border = mStyleBorder->GetActualBorder();
  nsMargin padding, margin;

  // See if the style system can provide us the padding directly
  if (!mStylePadding->GetPadding(padding)) {
    // We have to compute the left and right values
    padding.left = nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 mStylePadding->mPadding.GetLeft());
    padding.right = nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 mStylePadding->mPadding.GetRight());
  }

  // See if the style system can provide us the margin directly
  if (!mStyleMargin->GetMargin(margin)) {
    // We have to compute the left and right values
    if (eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit()) {
      // XXX FIXME (or does CalculateBlockSideMargins do this?)
      margin.left = 0;  // just ignore
    } else {
      margin.left = nsLayoutUtils::
        ComputeWidthDependentValue(aContainingBlockWidth,
                                   mStyleMargin->mMargin.GetLeft());
    }
    if (eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit()) {
      // XXX FIXME (or does CalculateBlockSideMargins do this?)
      margin.right = 0;  // just ignore
    } else {
      margin.right = nsLayoutUtils::
        ComputeWidthDependentValue(aContainingBlockWidth,
                                   mStyleMargin->mMargin.GetRight());
    }
  }

  nscoord outside =
    padding.LeftRight() + border.LeftRight() + margin.LeftRight();
  nscoord inside = 0;
  switch (mStylePosition->mBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      inside += border.LeftRight();
      // fall through
    case NS_STYLE_BOX_SIZING_PADDING:
      inside += padding.LeftRight();
  }
  outside -= inside;
  *aInsideBoxSizing = inside;
  *aOutsideBoxSizing = outside;
  return;
}

/**
 * Returns PR_TRUE iff a pre-order traversal of the normal child
 * frames rooted at aFrame finds no non-empty frame before aDescendant.
 */
static PRBool AreAllEarlierInFlowFramesEmpty(nsIFrame* aFrame,
  nsIFrame* aDescendant, PRBool* aFound) {
  if (aFrame == aDescendant) {
    *aFound = PR_TRUE;
    return PR_TRUE;
  }
  if (!aFrame->IsSelfEmpty()) {
    *aFound = PR_FALSE;
    return PR_FALSE;
  }
  for (nsIFrame* f = aFrame->GetFirstChild(nsnull); f; f = f->GetNextSibling()) {
    PRBool allEmpty = AreAllEarlierInFlowFramesEmpty(f, aDescendant, aFound);
    if (*aFound || !allEmpty) {
      return allEmpty;
    }
  }
  *aFound = PR_FALSE;
  return PR_TRUE;
}

// Calculate the hypothetical box that the element would have if it were in
// the flow. The values returned are relative to the padding edge of the
// absolute containing block
// aContainingBlock is the placeholder's containing block (XXX rename it?)
// cbrs->frame is the actual containing block
void
nsHTMLReflowState::CalculateHypotheticalBox(nsPresContext*    aPresContext,
                                            nsIFrame*         aPlaceholderFrame,
                                            nsIFrame*         aContainingBlock,
                                            nscoord           aBlockLeftContentEdge,
                                            nscoord           aBlockContentWidth,
                                            const nsHTMLReflowState* cbrs,
                                            nsHypotheticalBox& aHypotheticalBox)
{
  NS_ASSERTION(mStyleDisplay->mOriginalDisplay != NS_STYLE_DISPLAY_NONE,
               "mOriginalDisplay has not been properly initialized");
  
  // If it's a replaced element and it has a 'auto' value for 'width', see if we
  // can get the intrinsic size. This will allow us to exactly determine both the
  // left and right edges
  PRBool isAutoWidth = mStylePosition->mWidth.GetUnit() == eStyleUnit_Auto;
  nsSize      intrinsicSize;
  PRBool      knowIntrinsicSize = PR_FALSE;
  if (NS_FRAME_IS_REPLACED(mFrameType) && isAutoWidth) {
    // See if we can get the intrinsic size of the element
    knowIntrinsicSize = GetIntrinsicSizeFor(frame, intrinsicSize);
  }

  // See if we can calculate what the box width would have been if the
  // element had been in the flow
  nscoord boxWidth;
  PRBool  knowBoxWidth = PR_FALSE;
  if ((NS_STYLE_DISPLAY_INLINE == mStyleDisplay->mOriginalDisplay) &&
      !NS_FRAME_IS_REPLACED(mFrameType)) {
    // For non-replaced inline-level elements the 'width' property doesn't apply,
    // so we don't know what the width would have been without reflowing it

  } else {
    // It's either a replaced inline-level element or a block-level element

    // Determine the total amount of horizontal border/padding/margin that
    // the element would have had if it had been in the flow. Note that we
    // ignore any 'auto' and 'inherit' values
    nscoord insideBoxSizing, outsideBoxSizing;
    CalculateHorizBorderPaddingMargin(aBlockContentWidth,
                                      &insideBoxSizing, &outsideBoxSizing);

    if (NS_FRAME_IS_REPLACED(mFrameType) && isAutoWidth) {
      // It's a replaced element with an 'auto' width so the box width is
      // its intrinsic size plus any border/padding/margin
      if (knowIntrinsicSize) {
        boxWidth = intrinsicSize.width + outsideBoxSizing + insideBoxSizing;
        knowBoxWidth = PR_TRUE;
      }

    } else if (isAutoWidth) {
      // The box width is the containing block width
      boxWidth = aBlockContentWidth;
      knowBoxWidth = PR_TRUE;
    
    } else {
      // We need to compute it. It's important we do this, because if it's
      // percentage based this computed value may be different from the computed
      // value calculated using the absolute containing block width
      boxWidth = ComputeWidthValue(aBlockContentWidth,
                                   insideBoxSizing, outsideBoxSizing,
                                   mStylePosition->mWidth) + 
                 insideBoxSizing + outsideBoxSizing;
      knowBoxWidth = PR_TRUE;
    }
  }
  
  // Get the 'direction' of the block
  const nsStyleVisibility* blockVis = aContainingBlock->GetStyleVisibility();

  // Get the placeholder x-offset and y-offset in the coordinate
  // space of its containing block
  // XXXbz the placeholder is not fully reflowed yet if our containing block is
  // relatively positioned...
  nsPoint placeholderOffset = aPlaceholderFrame->GetOffsetTo(aContainingBlock);

  // First, determine the hypothetical box's mTop.  We want to check the
  // content insertion frame of aContainingBlock for block-ness, but make
  // sure to compute all coordinates in the coordinate system of
  // aContainingBlock.
  nsBlockFrame* blockFrame =
    nsLayoutUtils::GetAsBlock(aContainingBlock->GetContentInsertionFrame());
  if (blockFrame) {
    nscoord blockYOffset = blockFrame->GetOffsetTo(aContainingBlock).y;
    PRBool isValid;
    nsBlockInFlowLineIterator iter(blockFrame, aPlaceholderFrame, &isValid);
    if (!isValid) {
      // Give up.  We're probably dealing with somebody using
      // position:absolute inside native-anonymous content anyway.
      aHypotheticalBox.mTop = placeholderOffset.y;
    } else {
      NS_ASSERTION(iter.GetContainer() == blockFrame,
                   "Found placeholder in wrong block!");
      nsBlockFrame::line_iterator lineBox = iter.GetLine();

      // How we determine the hypothetical box depends on whether the element
      // would have been inline-level or block-level
      if (NS_STYLE_DISPLAY_INLINE == mStyleDisplay->mOriginalDisplay) {
        // Use the top of the inline box which the placeholder lives in
        // as the hypothetical box's top.
        aHypotheticalBox.mTop = lineBox->mBounds.y + blockYOffset;
      } else {
        // The element would have been block-level which means it would
        // be below the line containing the placeholder frame, unless
        // all the frames before it are empty.  In that case, it would
        // have been just before this line.
        // XXXbz the line box is not fully reflowed yet if our
        // containing block is relatively positioned...
        if (lineBox != iter.End()) {
          nsIFrame * firstFrame = lineBox->mFirstChild;
          PRBool found = PR_FALSE;
          PRBool allEmpty = PR_TRUE;
          while (firstFrame) { // See bug 223064
            allEmpty = AreAllEarlierInFlowFramesEmpty(firstFrame,
              aPlaceholderFrame, &found);
            if (found || !allEmpty)
              break;
            firstFrame = firstFrame->GetNextSibling();
          }
          NS_ASSERTION(firstFrame, "Couldn't find placeholder!");

          if (allEmpty) {
            // The top of the hypothetical box is the top of the line
            // containing the placeholder, since there is nothing in the
            // line before our placeholder except empty frames.
            aHypotheticalBox.mTop = lineBox->mBounds.y + blockYOffset;
          } else {
            // The top of the hypothetical box is just below the line
            // containing the placeholder.
            aHypotheticalBox.mTop = lineBox->mBounds.YMost() + blockYOffset;
          }
        } else {
          // Just use the placeholder's y-offset wrt the containing block
          aHypotheticalBox.mTop = placeholderOffset.y;
        }
      }
    }
  } else {
    // The containing block is not a block, so it's probably something
    // like a XUL box, etc.
    // Just use the placeholder's y-offset
    aHypotheticalBox.mTop = placeholderOffset.y;
  }

  // Second, determine the hypothetical box's mLeft & mRight
  // To determine the left and right offsets we need to look at the block's 'direction'
  if (NS_STYLE_DIRECTION_LTR == blockVis->mDirection) {
    // How we determine the hypothetical box depends on whether the element
    // would have been inline-level or block-level
    if (NS_STYLE_DISPLAY_INLINE == mStyleDisplay->mOriginalDisplay) {
      // The placeholder represents the left edge of the hypothetical box
      aHypotheticalBox.mLeft = placeholderOffset.x;
    } else {
      aHypotheticalBox.mLeft = aBlockLeftContentEdge;
    }
#ifdef DEBUG
    aHypotheticalBox.mLeftIsExact = PR_TRUE;
#endif

    if (knowBoxWidth) {
      aHypotheticalBox.mRight = aHypotheticalBox.mLeft + boxWidth;
#ifdef DEBUG
      aHypotheticalBox.mRightIsExact = PR_TRUE;
#endif
    } else {
      // We can't compute the right edge because we don't know the desired
      // width. So instead use the right content edge of the block parent,
      // but remember it's not exact
      aHypotheticalBox.mRight = aBlockLeftContentEdge + aBlockContentWidth;
#ifdef DEBUG
      aHypotheticalBox.mRightIsExact = PR_FALSE;
#endif
    }

  } else {
    // The placeholder represents the right edge of the hypothetical box
    if (NS_STYLE_DISPLAY_INLINE == mStyleDisplay->mOriginalDisplay) {
      aHypotheticalBox.mRight = placeholderOffset.x;
    } else {
      aHypotheticalBox.mRight = aBlockLeftContentEdge + aBlockContentWidth;
    }
#ifdef DEBUG
    aHypotheticalBox.mRightIsExact = PR_TRUE;
#endif
    
    if (knowBoxWidth) {
      aHypotheticalBox.mLeft = aHypotheticalBox.mRight - boxWidth;
#ifdef DEBUG
      aHypotheticalBox.mLeftIsExact = PR_TRUE;
#endif
    } else {
      // We can't compute the left edge because we don't know the desired
      // width. So instead use the left content edge of the block parent,
      // but remember it's not exact
      aHypotheticalBox.mLeft = aBlockLeftContentEdge;
#ifdef DEBUG
      aHypotheticalBox.mLeftIsExact = PR_FALSE;
#endif
    }

  }

  // The current coordinate space is that of the nearest block to the placeholder.
  // Convert to the coordinate space of the absolute containing block
  // One weird thing here is that for fixed-positioned elements we want to do
  // the conversion incorrectly; specifically we want to ignore any scrolling
  // that may have happened;
  nsPoint cbOffset;
  if (mStyleDisplay->mPosition == NS_STYLE_POSITION_FIXED &&
      // Exclude cases inside -moz-transform where fixed is like absolute.
      nsLayoutUtils::IsReallyFixedPos(frame)) {
    // In this case, cbrs->frame will always be an ancestor of
    // aContainingBlock, so can just walk our way up the frame tree.
    // Make sure to not add positions of frames whose parent is a
    // scrollFrame, since we're doing fixed positioning, which assumes
    // everything is scrolled to (0,0).
    cbOffset.MoveTo(0, 0);
    do {
      NS_ASSERTION(aContainingBlock,
                   "Should hit cbrs->frame before we run off the frame tree!");
      cbOffset += aContainingBlock->GetPositionIgnoringScrolling();
      aContainingBlock = aContainingBlock->GetParent();
    } while (aContainingBlock != cbrs->frame);
  } else {
    // XXXldb We need to either ignore scrolling for the absolute
    // positioning case too (and take the incompatibility) or figure out
    // how to make these positioned elements actually *move* when we
    // scroll, and thus avoid the resulting incremental reflow bugs.
    cbOffset = aContainingBlock->GetOffsetTo(cbrs->frame);
  }
  aHypotheticalBox.mLeft += cbOffset.x;
  aHypotheticalBox.mTop += cbOffset.y;
  aHypotheticalBox.mRight += cbOffset.x;
  
  // The specified offsets are relative to the absolute containing block's
  // padding edge and our current values are relative to the border edge, so
  // translate.
  nsMargin border = cbrs->mComputedBorderPadding - cbrs->mComputedPadding;
  aHypotheticalBox.mLeft -= border.left;
  aHypotheticalBox.mRight -= border.left;
  aHypotheticalBox.mTop -= border.top;
}

void
nsHTMLReflowState::InitAbsoluteConstraints(nsPresContext* aPresContext,
                                           const nsHTMLReflowState* cbrs,
                                           nscoord containingBlockWidth,
                                           nscoord containingBlockHeight)
{
  NS_PRECONDITION(containingBlockHeight != NS_AUTOHEIGHT,
                  "containing block height must be constrained");

  nsIFrame* outOfFlow = 
    frame->GetType() == nsGkAtoms::tableFrame ? frame->GetParent() : frame;
  NS_ASSERTION(outOfFlow->GetStateBits() & NS_FRAME_OUT_OF_FLOW,
               "Why are we here?");

  // Get the placeholder frame
  nsIFrame*     placeholderFrame;

  placeholderFrame = aPresContext->PresShell()->GetPlaceholderFrameFor(outOfFlow);
  NS_ASSERTION(nsnull != placeholderFrame, "no placeholder frame");

  // If both 'left' and 'right' are 'auto' or both 'top' and 'bottom' are
  // 'auto', then compute the hypothetical box of where the element would
  // have been if it had been in the flow
  nsHypotheticalBox hypotheticalBox;
  if (((eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit()) &&
       (eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit())) ||
      ((eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit()) &&
       (eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit()))) {
    // Find the nearest containing block frame to the placeholder frame,
    // and return its left edge and width.
    nscoord cbLeftEdge, cbWidth;
    nsIFrame* cbFrame = GetHypotheticalBoxContainer(placeholderFrame,
                                                    cbLeftEdge,
                                                    cbWidth);

    CalculateHypotheticalBox(aPresContext, placeholderFrame, cbFrame,
                             cbLeftEdge, cbWidth, cbrs, hypotheticalBox);
  }

  // Initialize the 'left' and 'right' computed offsets
  // XXX Handle new 'static-position' value...
  PRBool        leftIsAuto = PR_FALSE, rightIsAuto = PR_FALSE;
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit()) {
    mComputedOffsets.left = 0;
    leftIsAuto = PR_TRUE;
  } else {
    mComputedOffsets.left = nsLayoutUtils::
      ComputeWidthDependentValue(containingBlockWidth,
                                 mStylePosition->mOffset.GetLeft());
  }
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit()) {
    mComputedOffsets.right = 0;
    rightIsAuto = PR_TRUE;
  } else {
    mComputedOffsets.right = nsLayoutUtils::
      ComputeWidthDependentValue(containingBlockWidth,
                                 mStylePosition->mOffset.GetRight());
  }

  // Use the horizontal component of the hypothetical box in the cases
  // where it's needed.
  if (leftIsAuto && rightIsAuto) {
    // Use the direction of the original ("static-position") containing block
    // to dictate whether 'left' or 'right' is treated like 'static-position'.
    if (NS_STYLE_DIRECTION_LTR == GetNearestContainingBlock(placeholderFrame)
                                    ->GetStyleVisibility()->mDirection) {
      NS_ASSERTION(hypotheticalBox.mLeftIsExact, "should always have "
                   "exact value on containing block's start side");
      mComputedOffsets.left = hypotheticalBox.mLeft;
      leftIsAuto = PR_FALSE;
    } else {
      NS_ASSERTION(hypotheticalBox.mRightIsExact, "should always have "
                   "exact value on containing block's start side");
      mComputedOffsets.right = containingBlockWidth - hypotheticalBox.mRight;
      rightIsAuto = PR_FALSE;
    }
  }

  // Initialize the 'top' and 'bottom' computed offsets
  PRBool      topIsAuto = PR_FALSE, bottomIsAuto = PR_FALSE;
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit()) {
    mComputedOffsets.top = 0;
    topIsAuto = PR_TRUE;
  } else {
    mComputedOffsets.top = nsLayoutUtils::
      ComputeHeightDependentValue(containingBlockHeight,
                                  mStylePosition->mOffset.GetTop());
  }
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit()) {
    mComputedOffsets.bottom = 0;        
    bottomIsAuto = PR_TRUE;
  } else {
    mComputedOffsets.bottom = nsLayoutUtils::
      ComputeHeightDependentValue(containingBlockHeight,
                                  mStylePosition->mOffset.GetBottom());
  }

  if (topIsAuto && bottomIsAuto) {
    // Treat 'top' like 'static-position'
    mComputedOffsets.top = hypotheticalBox.mTop;
    topIsAuto = PR_FALSE;
  }

  PRBool widthIsAuto = eStyleUnit_Auto == mStylePosition->mWidth.GetUnit();
  PRBool heightIsAuto = eStyleUnit_Auto == mStylePosition->mHeight.GetUnit();

  PRBool shrinkWrap = leftIsAuto || rightIsAuto;
  nsSize size =
    frame->ComputeSize(rendContext,
                       nsSize(containingBlockWidth,
                              containingBlockHeight),
                       containingBlockWidth, // XXX or availableWidth?
                       nsSize(mComputedMargin.LeftRight() +
                                mComputedOffsets.LeftRight(),
                              mComputedMargin.TopBottom() +
                                mComputedOffsets.TopBottom()),
                       nsSize(mComputedBorderPadding.LeftRight() -
                                mComputedPadding.LeftRight(),
                              mComputedBorderPadding.TopBottom() -
                                mComputedPadding.TopBottom()),
                       nsSize(mComputedPadding.LeftRight(),
                              mComputedPadding.TopBottom()),
                       shrinkWrap);
  mComputedWidth = size.width;
  mComputedHeight = size.height;
  NS_ASSERTION(mComputedWidth >= 0, "Bogus width");
  NS_ASSERTION(mComputedHeight == NS_UNCONSTRAINEDSIZE ||
               mComputedHeight >= 0, "Bogus height");

  // XXX Now that we have ComputeSize, can we condense many of the
  // branches off of widthIsAuto?

  if (leftIsAuto) {
    // We know 'right' is not 'auto' anymore thanks to the hypothetical
    // box code above.
    // Solve for 'left'.
    if (widthIsAuto) {
      // XXXldb This, and the corresponding code in
      // nsAbsoluteContainingBlock.cpp, could probably go away now that
      // we always compute widths.
      mComputedOffsets.left = NS_AUTOOFFSET;
    } else {
      mComputedOffsets.left = containingBlockWidth - mComputedMargin.left -
        mComputedBorderPadding.left - mComputedWidth - mComputedBorderPadding.right - 
        mComputedMargin.right - mComputedOffsets.right;

    }
  } else if (rightIsAuto) {
    // We know 'left' is not 'auto' anymore thanks to the hypothetical
    // box code above.
    // Solve for 'right'.
    if (widthIsAuto) {
      // XXXldb This, and the corresponding code in
      // nsAbsoluteContainingBlock.cpp, could probably go away now that
      // we always compute widths.
      mComputedOffsets.right = NS_AUTOOFFSET;
    } else {
      mComputedOffsets.right = containingBlockWidth - mComputedOffsets.left -
        mComputedMargin.left - mComputedBorderPadding.left - mComputedWidth -
        mComputedBorderPadding.right - mComputedMargin.right;
    }
  } else {
    // Neither 'left' nor 'right' is 'auto'.  However, the width might
    // still not fill all the available space (even though we didn't
    // shrink-wrap) in case:
    //  * width was specified
    //  * we're dealing with a replaced element
    //  * width was constrained by min-width or max-width.

    nscoord availMarginSpace = containingBlockWidth -
                               mComputedOffsets.LeftRight() -
                               mComputedMargin.LeftRight() -
                               mComputedBorderPadding.LeftRight() -
                               mComputedWidth;
    PRBool marginLeftIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit();
    PRBool marginRightIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit();

    if (availMarginSpace < 0 ||
        (!marginLeftIsAuto && !marginRightIsAuto)) {
      // We're over-constrained so use the direction of the containing block
      // to dictate which value to ignore.  (And note that the spec says to ignore
      // 'left' or 'right' rather than 'margin-left' or 'margin-right'.)
      if (cbrs &&
          NS_STYLE_DIRECTION_RTL == cbrs->mStyleVisibility->mDirection) {
        // Ignore the specified value for 'left'.
        mComputedOffsets.left += availMarginSpace;
      } else {
        // Ignore the specified value for 'right'.
        mComputedOffsets.right += availMarginSpace;
      }
    } else if (marginLeftIsAuto) {
      if (marginRightIsAuto) {
        // Both 'margin-left' and 'margin-right' are 'auto', so they get
        // equal values
        mComputedMargin.left = availMarginSpace / 2;
        mComputedMargin.right = availMarginSpace - mComputedMargin.left;
      } else {
        // Just 'margin-left' is 'auto'
        mComputedMargin.left = availMarginSpace;
      }
    } else {
      // Just 'margin-right' is 'auto'
      mComputedMargin.right = availMarginSpace;
    }
  }

  if (topIsAuto) {
    // solve for 'top'
    if (heightIsAuto) {
      mComputedOffsets.top = NS_AUTOOFFSET;
    } else {
      mComputedOffsets.top = containingBlockHeight - mComputedMargin.top -
        mComputedBorderPadding.top - mComputedHeight - mComputedBorderPadding.bottom - 
        mComputedMargin.bottom - mComputedOffsets.bottom;
    }
  } else if (bottomIsAuto) {
    // solve for 'bottom'
    if (heightIsAuto) {
      mComputedOffsets.bottom = NS_AUTOOFFSET;
    } else {
      mComputedOffsets.bottom = containingBlockHeight - mComputedOffsets.top -
        mComputedMargin.top - mComputedBorderPadding.top - mComputedHeight -
        mComputedBorderPadding.bottom - mComputedMargin.bottom;
    }
  } else {
    // Neither 'top' nor 'bottom' is 'auto'.
    nscoord autoHeight = containingBlockHeight -
                         mComputedOffsets.TopBottom() -
                         mComputedMargin.TopBottom() -
                         mComputedBorderPadding.TopBottom();
    if (autoHeight < 0) {
      autoHeight = 0;
    }

    if (mComputedHeight == NS_UNCONSTRAINEDSIZE) {
      // For non-replaced elements with 'height' auto, the 'height'
      // fills the remaining space.
      mComputedHeight = autoHeight;

      // XXX Do these need box-sizing adjustments?
      if (mComputedHeight > mComputedMaxHeight)
        mComputedHeight = mComputedMaxHeight;
      if (mComputedHeight < mComputedMinHeight)
        mComputedHeight = mComputedMinHeight;
    }

    // The height might still not fill all the available space in case:
    //  * height was specified
    //  * we're dealing with a replaced element
    //  * height was constrained by min-height or max-height.
    nscoord availMarginSpace = autoHeight - mComputedHeight;
    PRBool marginTopIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetTopUnit();
    PRBool marginBottomIsAuto =
      eStyleUnit_Auto == mStyleMargin->mMargin.GetBottomUnit();

    if (availMarginSpace < 0 || (!marginTopIsAuto && !marginBottomIsAuto)) {
      // We're over-constrained so ignore the specified value for
      // 'bottom'.  (And note that the spec says to ignore 'bottom'
      // rather than 'margin-bottom'.)
      mComputedOffsets.bottom += availMarginSpace;
    } else if (marginTopIsAuto) {
      if (marginBottomIsAuto) {
        // Both 'margin-top' and 'margin-bottom' are 'auto', so they get
        // equal values
        mComputedMargin.top = availMarginSpace / 2;
        mComputedMargin.bottom = availMarginSpace - mComputedMargin.top;
      } else {
        // Just 'margin-top' is 'auto'
        mComputedMargin.top = availMarginSpace;
      }
    } else {
      // Just 'margin-bottom' is 'auto'
      mComputedMargin.bottom = availMarginSpace;
    }
  }
}

nscoord 
GetVerticalMarginBorderPadding(const nsHTMLReflowState* aReflowState)
{
  nscoord result = 0;
  if (!aReflowState) return result;

  // zero auto margins
  nsMargin margin = aReflowState->mComputedMargin;
  if (NS_AUTOMARGIN == margin.top) 
    margin.top = 0;
  if (NS_AUTOMARGIN == margin.bottom) 
    margin.bottom = 0;

  result += margin.top + margin.bottom;
  result += aReflowState->mComputedBorderPadding.top + 
            aReflowState->mComputedBorderPadding.bottom;

  return result;
}

/* Get the height based on the viewport of the containing block specified 
 * in aReflowState when the containing block has mComputedHeight == NS_AUTOHEIGHT
 * This will walk up the chain of containing blocks looking for a computed height
 * until it finds the canvas frame, or it encounters a frame that is not a block,
 * area, or scroll frame. This handles compatibility with IE (see bug 85016 and bug 219693)
 *
 *  When we encounter scrolledContent block frames, we skip over them, since they are guaranteed to not be useful for computing the containing block.
 *
 * See also IsQuirkContainingBlockHeight.
 */
static nscoord
CalcQuirkContainingBlockHeight(const nsHTMLReflowState* aCBReflowState)
{
  nsHTMLReflowState* firstAncestorRS = nsnull; // a candidate for html frame
  nsHTMLReflowState* secondAncestorRS = nsnull; // a candidate for body frame
  
  // initialize the default to NS_AUTOHEIGHT as this is the containings block
  // computed height when this function is called. It is possible that we 
  // don't alter this height especially if we are restricted to one level
  nscoord result = NS_AUTOHEIGHT; 
                             
  const nsHTMLReflowState* rs = aCBReflowState;
  for (; rs; rs = (nsHTMLReflowState *)(rs->parentReflowState)) { 
    nsIAtom* frameType = rs->frame->GetType();
    // if the ancestor is auto height then skip it and continue up if it 
    // is the first block frame and possibly the body/html
    if (nsGkAtoms::blockFrame == frameType ||
#ifdef MOZ_XUL
        nsGkAtoms::XULLabelFrame == frameType ||
#endif
        nsGkAtoms::scrollFrame == frameType) {

      secondAncestorRS = firstAncestorRS;
      firstAncestorRS = (nsHTMLReflowState*)rs;

      // If the current frame we're looking at is positioned, we don't want to
      // go any further (see bug 221784).  The behavior we want here is: 1) If
      // not auto-height, use this as the percentage base.  2) If auto-height,
      // keep looking, unless the frame is positioned.
      if (NS_AUTOHEIGHT == rs->ComputedHeight()) {
        if (rs->frame->GetStyleDisplay()->IsAbsolutelyPositioned()) {
          break;
        } else {
          continue;
        }
      }
    }
    else if (nsGkAtoms::canvasFrame == frameType) {
      // Always continue on to the height calculation
    }
    else if (nsGkAtoms::pageContentFrame == frameType) {
      nsIFrame* prevInFlow = rs->frame->GetPrevInFlow();
      // only use the page content frame for a height basis if it is the first in flow
      if (prevInFlow) 
        break;
    }
    else {
      break;
    }

    // if the ancestor is the page content frame then the percent base is 
    // the avail height, otherwise it is the computed height
    result = (nsGkAtoms::pageContentFrame == frameType)
             ? rs->availableHeight : rs->ComputedHeight();
    // if unconstrained - don't sutract borders - would result in huge height
    if (NS_AUTOHEIGHT == result) return result;

    // if we got to the canvas or page content frame, then subtract out 
    // margin/border/padding for the BODY and HTML elements
    if ((nsGkAtoms::canvasFrame == frameType) || 
        (nsGkAtoms::pageContentFrame == frameType)) {

      result -= GetVerticalMarginBorderPadding(firstAncestorRS); 
      result -= GetVerticalMarginBorderPadding(secondAncestorRS); 

#ifdef DEBUG
      // make sure the first ancestor is the HTML and the second is the BODY
      if (firstAncestorRS) {
        nsIContent* frameContent = firstAncestorRS->frame->GetContent();
        if (frameContent) {
          nsIAtom *contentTag = frameContent->Tag();
          NS_ASSERTION(contentTag == nsGkAtoms::html, "First ancestor is not HTML");
        }
      }
      if (secondAncestorRS) {
        nsIContent* frameContent = secondAncestorRS->frame->GetContent();
        if (frameContent) {
          nsIAtom *contentTag = frameContent->Tag();
          NS_ASSERTION(contentTag == nsGkAtoms::body, "Second ancestor is not BODY");
        }
      }
#endif
      
    }
    // if we got to the html frame (a block child of the canvas) ...
    else if (nsGkAtoms::blockFrame == frameType &&
             nsGkAtoms::canvasFrame ==
               rs->parentReflowState->frame->GetType()) {
      // ... then subtract out margin/border/padding for the BODY element
      result -= GetVerticalMarginBorderPadding(secondAncestorRS);
    }
    break;
  }

  // Make sure not to return a negative height here!
  return NS_MAX(result, 0);
}
// Called by InitConstraints() to compute the containing block rectangle for
// the element. Handles the special logic for absolutely positioned elements
void
nsHTMLReflowState::ComputeContainingBlockRectangle(nsPresContext*          aPresContext,
                                                   const nsHTMLReflowState* aContainingBlockRS,
                                                   nscoord&                 aContainingBlockWidth,
                                                   nscoord&                 aContainingBlockHeight)
{
  // Unless the element is absolutely positioned, the containing block is
  // formed by the content edge of the nearest block-level ancestor
  aContainingBlockWidth = aContainingBlockRS->mComputedWidth;
  aContainingBlockHeight = aContainingBlockRS->mComputedHeight;
  
  if (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE) {
    // See if the ancestor is block-level or inline-level
    if (NS_FRAME_GET_TYPE(aContainingBlockRS->mFrameType) == NS_CSS_FRAME_TYPE_INLINE) {
      // Base our size on the actual size of the frame.  In cases when this is
      // completely bogus (eg initial reflow), this code shouldn't even be
      // called, since the code in nsPositionedInlineFrame::Reflow will pass in
      // the containing block dimensions to our constructor.
      // XXXbz we should be taking the in-flows into account too, but
      // that's very hard.
      nsMargin computedBorder = aContainingBlockRS->mComputedBorderPadding -
        aContainingBlockRS->mComputedPadding;
      aContainingBlockWidth = aContainingBlockRS->frame->GetRect().width -
        computedBorder.LeftRight();;
      NS_ASSERTION(aContainingBlockWidth >= 0,
                   "Negative containing block width!");
      aContainingBlockHeight = aContainingBlockRS->frame->GetRect().height -
        computedBorder.TopBottom();
      NS_ASSERTION(aContainingBlockHeight >= 0,
                   "Negative containing block height!");
    } else {
      // If the ancestor is block-level, the containing block is formed by the
      // padding edge of the ancestor
      aContainingBlockWidth += aContainingBlockRS->mComputedPadding.LeftRight();
      aContainingBlockHeight += aContainingBlockRS->mComputedPadding.TopBottom();
    }
  } else {
    // an element in quirks mode gets a containing block based on looking for a
    // parent with a non-auto height if the element has a percent height
    // Note: We don't emulate this quirk for percents in calc().
    if (NS_AUTOHEIGHT == aContainingBlockHeight) {
      if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
          mStylePosition->mHeight.GetUnit() == eStyleUnit_Percent) {
        aContainingBlockHeight = CalcQuirkContainingBlockHeight(aContainingBlockRS);
      }
    }
  }
}

// Prefs callback to pick up changes
static int
PrefsChanged(const char *aPrefName, void *instance)
{
  sBlinkIsAllowed =
    nsContentUtils::GetBoolPref("browser.blink_allowed", sBlinkIsAllowed);

  return 0; /* PREF_OK */
}

// Check to see if |text-decoration: blink| is allowed.  The first time
// called, register the callback and then force-load the pref.  After that,
// just use the cached value.
static PRBool BlinkIsAllowed(void)
{
  if (!sPrefIsLoaded) {
    // Set up a listener and check the initial value
    nsContentUtils::RegisterPrefCallback("browser.blink_allowed", PrefsChanged,
                                         nsnull);
    PrefsChanged(nsnull, nsnull);
    sPrefIsLoaded = PR_TRUE;
  }
  return sBlinkIsAllowed;
}

static eNormalLineHeightControl GetNormalLineHeightCalcControl(void)
{
  if (sNormalLineHeightControl == eUninitialized) {
    // browser.display.normal_lineheight_calc_control is not user
    // changeable, so no need to register callback for it.
    sNormalLineHeightControl =
      static_cast<eNormalLineHeightControl>
                 (nsContentUtils::GetIntPref("browser.display.normal_lineheight_calc_control", eNoExternalLeading));
  }
  return sNormalLineHeightControl;
}

static inline PRBool
IsSideCaption(nsIFrame* aFrame, const nsStyleDisplay* aStyleDisplay)
{
  if (aStyleDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE_CAPTION)
    return PR_FALSE;
  PRUint8 captionSide = aFrame->GetStyleTableBorder()->mCaptionSide;
  return captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
         captionSide == NS_STYLE_CAPTION_SIDE_RIGHT;
}

// XXX refactor this code to have methods for each set of properties
// we are computing: width,height,line-height; margin; offsets

void
nsHTMLReflowState::InitConstraints(nsPresContext* aPresContext,
                                   nscoord         aContainingBlockWidth,
                                   nscoord         aContainingBlockHeight,
                                   const nsMargin* aBorder,
                                   const nsMargin* aPadding)
{
  DISPLAY_INIT_CONSTRAINTS(frame, this,
                           aContainingBlockWidth, aContainingBlockHeight,
                           aBorder, aPadding);

  // Since we are in reflow, we don't need to store these properties anymore
  FrameProperties props(aPresContext->PropertyTable(), frame);
  props.Delete(nsIFrame::UsedBorderProperty());
  props.Delete(nsIFrame::UsedPaddingProperty());
  props.Delete(nsIFrame::UsedMarginProperty());

  // If this is the root frame, then set the computed width and
  // height equal to the available space
  if (nsnull == parentReflowState) {
    // XXXldb This doesn't mean what it used to!
    InitOffsets(aContainingBlockWidth, aBorder, aPadding);
    // Override mComputedMargin since reflow roots start from the
    // frame's boundary, which is inside the margin.
    mComputedMargin.SizeTo(0, 0, 0, 0);
    mComputedOffsets.SizeTo(0, 0, 0, 0);

    mComputedWidth = availableWidth - mComputedBorderPadding.LeftRight();
    if (mComputedWidth < 0)
      mComputedWidth = 0;
    if (availableHeight != NS_UNCONSTRAINEDSIZE) {
      mComputedHeight = availableHeight - mComputedBorderPadding.TopBottom();
      if (mComputedHeight < 0)
        mComputedHeight = 0;
    } else {
      mComputedHeight = NS_UNCONSTRAINEDSIZE;
    }

    mComputedMinWidth = mComputedMinHeight = 0;
    mComputedMaxWidth = mComputedMaxHeight = NS_UNCONSTRAINEDSIZE;
  } else {
    // Get the containing block reflow state
    const nsHTMLReflowState* cbrs = mCBReflowState;
    NS_ASSERTION(nsnull != cbrs, "no containing block");

    // If we weren't given a containing block width and height, then
    // compute one
    if (aContainingBlockWidth == -1) {
      ComputeContainingBlockRectangle(aPresContext, cbrs, aContainingBlockWidth, 
                                      aContainingBlockHeight);
    }

    // See if the containing block height is based on the size of its
    // content
    nsIAtom* fType;
    if (NS_AUTOHEIGHT == aContainingBlockHeight) {
      // See if the containing block is a cell frame which needs
      // to use the mComputedHeight of the cell instead of what the cell block passed in.
      // XXX It seems like this could lead to bugs with min-height and friends
      if (cbrs->parentReflowState) {
        fType = cbrs->frame->GetType();
        if (IS_TABLE_CELL(fType)) {
          // use the cell's computed height 
          aContainingBlockHeight = cbrs->mComputedHeight;
        }
      }
    }

    InitOffsets(aContainingBlockWidth, aBorder, aPadding);

    const nsStyleCoord &height = mStylePosition->mHeight;
    nsStyleUnit heightUnit = height.GetUnit();

    // Check for a percentage based height and a containing block height
    // that depends on the content height
    // XXX twiddling heightUnit doesn't help anymore
    // FIXME Shouldn't we fix that?
    if (height.HasPercent()) {
      if (NS_AUTOHEIGHT == aContainingBlockHeight) {
        // this if clause enables %-height on replaced inline frames,
        // such as images.  See bug 54119.  The else clause "heightUnit = eStyleUnit_Auto;"
        // used to be called exclusively.
        if (NS_FRAME_REPLACED(NS_CSS_FRAME_TYPE_INLINE) == mFrameType ||
            NS_FRAME_REPLACED_CONTAINS_BLOCK(
                NS_CSS_FRAME_TYPE_INLINE) == mFrameType) {
          // Get the containing block reflow state
          NS_ASSERTION(nsnull != cbrs, "no containing block");
          // in quirks mode, get the cb height using the special quirk method
          if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode()) {
            if (!IS_TABLE_CELL(fType)) {
              aContainingBlockHeight = CalcQuirkContainingBlockHeight(cbrs);
              if (aContainingBlockHeight == NS_AUTOHEIGHT) {
                heightUnit = eStyleUnit_Auto;
              }
            }
            else {
              heightUnit = eStyleUnit_Auto;
            }
          }
          // in standard mode, use the cb height.  if it's "auto", as will be the case
          // by default in BODY, use auto height as per CSS2 spec.
          else 
          {
            if (NS_AUTOHEIGHT != cbrs->mComputedHeight)
              aContainingBlockHeight = cbrs->mComputedHeight;
            else
              heightUnit = eStyleUnit_Auto;
          }
        }
        else {
          // default to interpreting the height like 'auto'
          heightUnit = eStyleUnit_Auto;
        }
      }
    }

    // Compute our offsets if the element is relatively positioned.  We need
    // the correct containing block width and height here, which is why we need
    // to do it after all the quirks-n-such above.
    if (NS_STYLE_POSITION_RELATIVE == mStyleDisplay->mPosition) {
      ComputeRelativeOffsets(cbrs, aContainingBlockWidth, aContainingBlockHeight, aPresContext);
    } else {
      // Initialize offsets to 0
      mComputedOffsets.SizeTo(0, 0, 0, 0);
    }

    // Calculate the computed values for min and max properties.  Note that
    // this MUST come after we've computed our border and padding.
    ComputeMinMaxValues(aContainingBlockWidth, aContainingBlockHeight, cbrs);

    // Calculate the computed width and height. This varies by frame type

    if (NS_CSS_FRAME_TYPE_INTERNAL_TABLE == mFrameType) {
      // Internal table elements. The rules vary depending on the type.
      // Calculate the computed width
      PRBool rowOrRowGroup = PR_FALSE;
      const nsStyleCoord &width = mStylePosition->mWidth;
      nsStyleUnit widthUnit = width.GetUnit();
      if ((NS_STYLE_DISPLAY_TABLE_ROW == mStyleDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == mStyleDisplay->mDisplay)) {
        // 'width' property doesn't apply to table rows and row groups
        widthUnit = eStyleUnit_Auto;
        rowOrRowGroup = PR_TRUE;
      }

      // calc() acts like auto on internal table elements
      if (eStyleUnit_Auto == widthUnit || width.IsCalcUnit()) {
        mComputedWidth = availableWidth;

        if ((mComputedWidth != NS_UNCONSTRAINEDSIZE) && !rowOrRowGroup){
          // Internal table elements don't have margins. Only tables and
          // cells have border and padding
          mComputedWidth -= mComputedBorderPadding.left +
            mComputedBorderPadding.right;
          if (mComputedWidth < 0)
            mComputedWidth = 0;
        }
        NS_ASSERTION(mComputedWidth >= 0, "Bogus computed width");
      
      } else {
        NS_ASSERTION(widthUnit == mStylePosition->mWidth.GetUnit(),
                     "unexpected width unit change");
        mComputedWidth = ComputeWidthValue(aContainingBlockWidth,
                                           mStylePosition->mBoxSizing,
                                           mStylePosition->mWidth);
      }

      // Calculate the computed height
      if ((NS_STYLE_DISPLAY_TABLE_COLUMN == mStyleDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == mStyleDisplay->mDisplay)) {
        // 'height' property doesn't apply to table columns and column groups
        heightUnit = eStyleUnit_Auto;
      }
      // calc() acts like 'auto' on internal table elements
      if (eStyleUnit_Auto == heightUnit || height.IsCalcUnit()) {
        mComputedHeight = NS_AUTOHEIGHT;
      } else {
        NS_ASSERTION(heightUnit == mStylePosition->mHeight.GetUnit(),
                     "unexpected height unit change");
        mComputedHeight = nsLayoutUtils::
          ComputeHeightValue(aContainingBlockHeight, mStylePosition->mHeight);
      }

      // Doesn't apply to table elements
      mComputedMinWidth = mComputedMinHeight = 0;
      mComputedMaxWidth = mComputedMaxHeight = NS_UNCONSTRAINEDSIZE;

    } else if (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE) {
      // XXX not sure if this belongs here or somewhere else - cwk
      InitAbsoluteConstraints(aPresContext, cbrs, aContainingBlockWidth,
                              aContainingBlockHeight);
    } else {
      PRBool isBlock =
        NS_CSS_FRAME_TYPE_BLOCK == NS_FRAME_GET_TYPE(mFrameType);
      // make sure legend frames with display:block and width:auto still
      // shrink-wrap
      PRBool shrinkWrap = !isBlock || frame->GetType() == nsGkAtoms::legendFrame;
      nsSize size =
        frame->ComputeSize(rendContext,
                           nsSize(aContainingBlockWidth,
                                  aContainingBlockHeight),
                           availableWidth,
                           nsSize(mComputedMargin.LeftRight(),
                                  mComputedMargin.TopBottom()),
                           nsSize(mComputedBorderPadding.LeftRight() -
                                    mComputedPadding.LeftRight(),
                                  mComputedBorderPadding.TopBottom() -
                                    mComputedPadding.TopBottom()),
                           nsSize(mComputedPadding.LeftRight(),
                                  mComputedPadding.TopBottom()),
                           shrinkWrap);

      mComputedWidth = size.width;
      mComputedHeight = size.height;
      NS_ASSERTION(mComputedWidth >= 0, "Bogus width");
      NS_ASSERTION(mComputedHeight == NS_UNCONSTRAINEDSIZE ||
                   mComputedHeight >= 0, "Bogus height");

      if (isBlock && !IsSideCaption(frame, mStyleDisplay))
        CalculateBlockSideMargins(availableWidth, mComputedWidth);
    }
  }
  // Check for blinking text and permission to display it
  mFlags.mBlinks = (parentReflowState && parentReflowState->mFlags.mBlinks);
  if (!mFlags.mBlinks && BlinkIsAllowed()) {
    const nsStyleTextReset* st = frame->GetStyleTextReset();
    mFlags.mBlinks = 
      ((st->mTextDecoration & NS_STYLE_TEXT_DECORATION_BLINK) != 0);
  }
}

void
nsCSSOffsetState::InitOffsets(nscoord aContainingBlockWidth,
                              const nsMargin *aBorder,
                              const nsMargin *aPadding)
{
  DISPLAY_INIT_OFFSETS(frame, this, aContainingBlockWidth, aBorder, aPadding);

  // Compute margins from the specified margin style information. These
  // become the default computed values, and may be adjusted below
  // XXX fix to provide 0,0 for the top&bottom margins for
  // inline-non-replaced elements
  ComputeMargin(aContainingBlockWidth);

  const nsStyleDisplay *disp = frame->GetStyleDisplay();
  PRBool isThemed = frame->IsThemed(disp);
  nsPresContext *presContext = frame->PresContext();

  nsIntMargin widget;
  if (isThemed &&
      presContext->GetTheme()->GetWidgetPadding(presContext->DeviceContext(),
                                                frame, disp->mAppearance,
                                                &widget)) {
    mComputedPadding.top = presContext->DevPixelsToAppUnits(widget.top);
    mComputedPadding.right = presContext->DevPixelsToAppUnits(widget.right);
    mComputedPadding.bottom = presContext->DevPixelsToAppUnits(widget.bottom);
    mComputedPadding.left = presContext->DevPixelsToAppUnits(widget.left);
  }
  else if (aPadding) { // padding is an input arg
    mComputedPadding.top    = aPadding->top;
    mComputedPadding.right  = aPadding->right;
    mComputedPadding.bottom = aPadding->bottom;
    mComputedPadding.left   = aPadding->left;
  }
  else {
    ComputePadding(aContainingBlockWidth);
  }

  if (isThemed) {
    nsIntMargin widget;
    presContext->GetTheme()->GetWidgetBorder(presContext->DeviceContext(),
                                             frame, disp->mAppearance,
                                             &widget);
    mComputedBorderPadding.top =
      presContext->DevPixelsToAppUnits(widget.top);
    mComputedBorderPadding.right =
      presContext->DevPixelsToAppUnits(widget.right);
    mComputedBorderPadding.bottom =
      presContext->DevPixelsToAppUnits(widget.bottom);
    mComputedBorderPadding.left =
      presContext->DevPixelsToAppUnits(widget.left);
  }
  else if (aBorder) {  // border is an input arg
    mComputedBorderPadding = *aBorder;
  }
  else {
    mComputedBorderPadding = frame->GetStyleBorder()->GetActualBorder();
  }
  mComputedBorderPadding += mComputedPadding;

  nsIAtom* frameType = frame->GetType();
  if (frameType == nsGkAtoms::tableFrame) {
    nsTableFrame *tableFrame = static_cast<nsTableFrame*>(frame);

    if (tableFrame->IsBorderCollapse()) {
      // border-collapsed tables don't use any of their padding, and
      // only part of their border.  We need to do this here before we
      // try to do anything like handling 'auto' widths,
      // '-moz-box-sizing', or 'auto' margins.
      mComputedPadding.SizeTo(0,0,0,0);
      mComputedBorderPadding = tableFrame->GetIncludedOuterBCBorder();
    }
  } else if (frameType == nsGkAtoms::scrollbarFrame) {
    // scrollbars may have had their width or height smashed to zero
    // by the associated scrollframe, in which case we must not report
    // any padding or border.
    nsSize size(frame->GetSize());
    if (size.width == 0 || size.height == 0) {
      mComputedPadding.left = 0;
      mComputedPadding.right = 0;
      mComputedBorderPadding.left = 0;
      mComputedBorderPadding.right = 0;
      mComputedPadding.top = 0;
      mComputedPadding.bottom = 0;
      mComputedBorderPadding.top = 0;
      mComputedBorderPadding.bottom = 0;
    }
  }
}

// This code enforces section 10.3.3 of the CSS2 spec for this formula:
//
// 'margin-left' + 'border-left-width' + 'padding-left' + 'width' +
//   'padding-right' + 'border-right-width' + 'margin-right'
//   = width of containing block 
//
// Note: the width unit is not auto when this is called
void
nsHTMLReflowState::CalculateBlockSideMargins(nscoord aAvailWidth,
                                             nscoord aComputedWidth)
{
  NS_WARN_IF_FALSE(NS_UNCONSTRAINEDSIZE != aComputedWidth &&
                   NS_UNCONSTRAINEDSIZE != aAvailWidth,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");

  nscoord sum = mComputedMargin.left + mComputedBorderPadding.left +
    aComputedWidth + mComputedBorderPadding.right + mComputedMargin.right;
  if (sum == aAvailWidth)
    // The sum is already correct
    return;

  // Determine the left and right margin values. The width value
  // remains constant while we do this.

  // Calculate how much space is available for margins
  nscoord availMarginSpace = aAvailWidth - sum;

  // If the available margin space is negative, then don't follow the
  // usual overconstraint rules.
  if (availMarginSpace < 0) {
    if (mCBReflowState &&
        mCBReflowState->mStyleVisibility->mDirection == NS_STYLE_DIRECTION_RTL) {
      mComputedMargin.left += availMarginSpace;
    } else {
      mComputedMargin.right += availMarginSpace;
    }
    return;
  }

  // The css2 spec clearly defines how block elements should behave
  // in section 10.3.3.
  PRBool isAutoLeftMargin =
    eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit();
  PRBool isAutoRightMargin =
    eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit();
  if (!isAutoLeftMargin && !isAutoRightMargin) {
    // Neither margin is 'auto' so we're over constrained. Use the
    // 'direction' property of the parent to tell which margin to
    // ignore
    // First check if there is an HTML alignment that we should honor
    const nsHTMLReflowState* prs = parentReflowState;
    if (frame->GetType() == nsGkAtoms::tableFrame) {
      NS_ASSERTION(prs->frame->GetType() == nsGkAtoms::tableOuterFrame,
                   "table not inside outer table");
      // Center the table within the outer table based on the alignment
      // of the outer table's parent.
      prs = prs->parentReflowState;
    }
    if (prs &&
        (prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_LEFT ||
         prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER ||
         prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT)) {
      isAutoLeftMargin =
        prs->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_LEFT;
      isAutoRightMargin =
        prs->mStyleText->mTextAlign != NS_STYLE_TEXT_ALIGN_MOZ_RIGHT;
    }
    // Otherwise apply the CSS rules, and ignore one margin by forcing
    // it to 'auto', depending on 'direction'.
    else if (mCBReflowState &&
             NS_STYLE_DIRECTION_RTL == mCBReflowState->mStyleVisibility->mDirection) {
      isAutoLeftMargin = PR_TRUE;
    }
    else {
      isAutoRightMargin = PR_TRUE;
    }
  }

  // Logic which is common to blocks and tables
  // The computed margins need not be zero because the 'auto' could come from
  // overconstraint or from HTML alignment so values need to be accumulated

  if (isAutoLeftMargin) {
    if (isAutoRightMargin) {
      // Both margins are 'auto' so the computed addition should be equal
      nscoord forLeft = availMarginSpace / 2;
      mComputedMargin.left  += forLeft;
      mComputedMargin.right += availMarginSpace - forLeft;
    } else {
      mComputedMargin.left += availMarginSpace;
    }
  } else if (isAutoRightMargin) {
    mComputedMargin.right += availMarginSpace;
  }
}

#define NORMAL_LINE_HEIGHT_FACTOR 1.2f    // in term of emHeight 
// For "normal" we use the font's normal line height (em height + leading).
// If both internal leading and  external leading specified by font itself
// are zeros, we should compensate this by creating extra (external) leading 
// in eCompensateLeading mode. This is necessary because without this 
// compensation, normal line height might looks too tight. 

// For risk management, we use preference to control the behavior, and 
// eNoExternalLeading is the old behavior.
static nscoord
GetNormalLineHeight(nsIFontMetrics* aFontMetrics)
{
  NS_PRECONDITION(nsnull != aFontMetrics, "no font metrics");

  nscoord normalLineHeight;

  nscoord externalLeading, internalLeading, emHeight;
  aFontMetrics->GetExternalLeading(externalLeading);
  aFontMetrics->GetInternalLeading(internalLeading);
  aFontMetrics->GetEmHeight(emHeight);
  switch (GetNormalLineHeightCalcControl()) {
  case eIncludeExternalLeading:
    normalLineHeight = emHeight+ internalLeading + externalLeading;
    break;
  case eCompensateLeading:
    if (!internalLeading && !externalLeading)
      normalLineHeight = NSToCoordRound(emHeight * NORMAL_LINE_HEIGHT_FACTOR);
    else
      normalLineHeight = emHeight+ internalLeading + externalLeading;
    break;
  default:
    //case eNoExternalLeading:
    normalLineHeight = emHeight + internalLeading;
  }
  return normalLineHeight;
}

static nscoord
ComputeLineHeight(nsStyleContext* aStyleContext,
                  nscoord aBlockHeight)
{
  const nsStyleCoord& lhCoord = aStyleContext->GetStyleText()->mLineHeight;

  if (lhCoord.GetUnit() == eStyleUnit_Coord)
    return lhCoord.GetCoordValue();

  if (lhCoord.GetUnit() == eStyleUnit_Factor)
    // For factor units the computed value of the line-height property 
    // is found by multiplying the factor by the font's computed size
    // (adjusted for min-size prefs and text zoom).
    return NSToCoordRound(lhCoord.GetFactorValue() *
                          aStyleContext->GetStyleFont()->mFont.size);

  NS_ASSERTION(lhCoord.GetUnit() == eStyleUnit_Normal ||
               lhCoord.GetUnit() == eStyleUnit_Enumerated,
               "bad line-height unit");
  
  if (lhCoord.GetUnit() == eStyleUnit_Enumerated) {
    NS_ASSERTION(lhCoord.GetIntValue() == NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT,
                 "bad line-height value");
    if (aBlockHeight != NS_AUTOHEIGHT)
      return aBlockHeight;
  }

  nsCOMPtr<nsIFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForStyleContext(aStyleContext,
                                               getter_AddRefs(fm));
  return GetNormalLineHeight(fm);
}

nscoord
nsHTMLReflowState::CalcLineHeight() const
{
  nscoord blockHeight =
    frame->IsContainingBlock() ? mComputedHeight :
    (mCBReflowState ? mCBReflowState->mComputedHeight : NS_AUTOHEIGHT);

  return CalcLineHeight(frame->GetStyleContext(), blockHeight);
}

/* static */ nscoord
nsHTMLReflowState::CalcLineHeight(nsStyleContext* aStyleContext,
                                  nscoord aBlockHeight)
{
  NS_PRECONDITION(aStyleContext, "Must have a style context");
  
  nscoord lineHeight = ComputeLineHeight(aStyleContext, aBlockHeight);

  NS_ASSERTION(lineHeight >= 0, "ComputeLineHeight screwed up");

  return lineHeight;
}

void
nsCSSOffsetState::ComputeMargin(nscoord aContainingBlockWidth)
{
  // If style style can provide us the margin directly, then use it.
  const nsStyleMargin *styleMargin = frame->GetStyleMargin();
  if (!styleMargin->GetMargin(mComputedMargin)) {
    // We have to compute the value
    mComputedMargin.left = nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 styleMargin->mMargin.GetLeft());
    mComputedMargin.right = nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 styleMargin->mMargin.GetRight());

    // According to the CSS2 spec, margin percentages are
    // calculated with respect to the *width* of the containing
    // block, even for margin-top and margin-bottom.
    // XXX This isn't true for page boxes, if we implement them.
    mComputedMargin.top = nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 styleMargin->mMargin.GetTop());
    mComputedMargin.bottom = nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 styleMargin->mMargin.GetBottom());

    // XXX We need to include 'auto' horizontal margins in this too!
    // ... but if we did that, we'd need to fix nsFrame::GetUsedMargin
    // to use it even when the margins are all zero (since sometimes
    // they get treated as auto)
    frame->Properties().Set(nsIFrame::UsedMarginProperty(),
                            new nsMargin(mComputedMargin));
  }
}

void
nsCSSOffsetState::ComputePadding(nscoord aContainingBlockWidth)
{
  // If style can provide us the padding directly, then use it.
  const nsStylePadding *stylePadding = frame->GetStylePadding();
  if (!stylePadding->GetPadding(mComputedPadding)) {
    // We have to compute the value
    // clamp negative calc() results to 0
    mComputedPadding.left = NS_MAX(0, nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 stylePadding->mPadding.GetLeft()));
    mComputedPadding.right = NS_MAX(0, nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 stylePadding->mPadding.GetRight()));

    // According to the CSS2 spec, percentages are calculated with respect to
    // containing block width for padding-top and padding-bottom
    mComputedPadding.top = NS_MAX(0, nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 stylePadding->mPadding.GetTop()));
    mComputedPadding.bottom = NS_MAX(0, nsLayoutUtils::
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 stylePadding->mPadding.GetBottom()));

    frame->Properties().Set(nsIFrame::UsedPaddingProperty(),
                            new nsMargin(mComputedPadding));
  }
  // a table row/col group, row/col doesn't have padding
  // XXXldb Neither do border-collapse tables.
  nsIAtom* frameType = frame->GetType();
  if (nsGkAtoms::tableRowGroupFrame == frameType ||
      nsGkAtoms::tableColGroupFrame == frameType ||
      nsGkAtoms::tableRowFrame      == frameType ||
      nsGkAtoms::tableColFrame      == frameType) {
    mComputedPadding.top    = 0;
    mComputedPadding.right  = 0;
    mComputedPadding.bottom = 0;
    mComputedPadding.left   = 0;
  }
}

void
nsHTMLReflowState::ApplyMinMaxConstraints(nscoord* aFrameWidth,
                                          nscoord* aFrameHeight) const
{
  if (aFrameWidth) {
    if (NS_UNCONSTRAINEDSIZE != mComputedMaxWidth) {
      *aFrameWidth = NS_MIN(*aFrameWidth, mComputedMaxWidth);
    }
    *aFrameWidth = NS_MAX(*aFrameWidth, mComputedMinWidth);
  }

  if (aFrameHeight) {
    if (NS_UNCONSTRAINEDSIZE != mComputedMaxHeight) {
      *aFrameHeight = NS_MIN(*aFrameHeight, mComputedMaxHeight);
    }
    *aFrameHeight = NS_MAX(*aFrameHeight, mComputedMinHeight);
  }
}

void
nsHTMLReflowState::ComputeMinMaxValues(nscoord aContainingBlockWidth,
                                       nscoord aContainingBlockHeight,
                                       const nsHTMLReflowState* aContainingBlockRS)
{
  mComputedMinWidth = ComputeWidthValue(aContainingBlockWidth,
                                        mStylePosition->mBoxSizing,
                                        mStylePosition->mMinWidth);

  if (eStyleUnit_None == mStylePosition->mMaxWidth.GetUnit()) {
    // Specified value of 'none'
    mComputedMaxWidth = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    mComputedMaxWidth = ComputeWidthValue(aContainingBlockWidth,
                                          mStylePosition->mBoxSizing,
                                          mStylePosition->mMaxWidth);
  }

  // If the computed value of 'min-width' is greater than the value of
  // 'max-width', 'max-width' is set to the value of 'min-width'
  if (mComputedMinWidth > mComputedMaxWidth) {
    mComputedMaxWidth = mComputedMinWidth;
  }

  // Check for percentage based values and a containing block height that
  // depends on the content height. Treat them like 'auto'
  // Likewise, check for calc() on internal table elements; calc() on
  // such elements is unsupported.
  const nsStyleCoord &minHeight = mStylePosition->mMinHeight;
  if ((NS_AUTOHEIGHT == aContainingBlockHeight &&
       minHeight.HasPercent()) ||
      (mFrameType == NS_CSS_FRAME_TYPE_INTERNAL_TABLE &&
       minHeight.IsCalcUnit())) {
    mComputedMinHeight = 0;
  } else {
    mComputedMinHeight = nsLayoutUtils::
      ComputeHeightValue(aContainingBlockHeight, minHeight);
  }
  const nsStyleCoord &maxHeight = mStylePosition->mMaxHeight;
  nsStyleUnit maxHeightUnit = maxHeight.GetUnit();
  if (eStyleUnit_None == maxHeightUnit) {
    // Specified value of 'none'
    mComputedMaxHeight = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    // Check for percentage based values and a containing block height that
    // depends on the content height. Treat them like 'auto'
    // Likewise, check for calc() on internal table elements; calc() on
    // such elements is unsupported.
    if ((NS_AUTOHEIGHT == aContainingBlockHeight && 
         maxHeight.HasPercent()) ||
        (mFrameType == NS_CSS_FRAME_TYPE_INTERNAL_TABLE &&
         maxHeight.IsCalcUnit())) {
      mComputedMaxHeight = NS_UNCONSTRAINEDSIZE;
    } else {
      mComputedMaxHeight = nsLayoutUtils::
        ComputeHeightValue(aContainingBlockHeight, maxHeight);
    }
  }

  // If the computed value of 'min-height' is greater than the value of
  // 'max-height', 'max-height' is set to the value of 'min-height'
  if (mComputedMinHeight > mComputedMaxHeight) {
    mComputedMaxHeight = mComputedMinHeight;
  }
}

void
nsHTMLReflowState::SetTruncated(const nsHTMLReflowMetrics& aMetrics,
                                nsReflowStatus* aStatus) const
{
  if (availableHeight != NS_UNCONSTRAINEDSIZE &&
      availableHeight < aMetrics.height &&
      !mFlags.mIsTopOfPage) {
    *aStatus |= NS_FRAME_TRUNCATED;
  } else {
    *aStatus &= ~NS_FRAME_TRUNCATED;
  }
}
