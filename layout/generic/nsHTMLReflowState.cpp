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

// Prefs-driven control for |text-decoration: blink|
static PRPackedBool sPrefIsLoaded = PR_FALSE;
static PRPackedBool sBlinkIsAllowed = PR_TRUE;

enum eNormalLineHeightControl {
  eUninitialized = -1,
  eNoExternalLeading = 0,   // does not include external leading 
  eIncludeExternalLeading,  // use whatever value font vendor provides
  eCompensateLeading        // compensate leading if leading provided by font vendor is not enough
};

#ifdef FONT_LEADING_APIS_V2
static eNormalLineHeightControl sNormalLineHeightControl = eUninitialized;
#endif

// Initialize a <b>root</b> reflow state with a rendering context to
// use for measuring things.
nsHTMLReflowState::nsHTMLReflowState(nsPresContext*       aPresContext,
                                     nsIFrame*            aFrame,
                                     nsIRenderingContext* aRenderingContext,
                                     const nsSize&        aAvailableSpace)
  : nsCSSOffsetState(aFrame, aRenderingContext)
  , mReflowDepth(0)
{
  NS_PRECONDITION(aPresContext, "no pres context");
  NS_PRECONDITION(aRenderingContext, "no rendering context");
  NS_PRECONDITION(aFrame, "no frame");
  parentReflowState = nsnull;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;
  mSpaceManager = nsnull;
  mLineLayout = nsnull;
  mFlags.mSpecialHeightReflow = PR_FALSE;
  mFlags.mIsTopOfPage = PR_FALSE;
  mFlags.mTableIsSplittable = PR_FALSE;
  mFlags.mNextInFlowUntouched = PR_FALSE;
  mFlags.mAssumingHScrollbar = mFlags.mAssumingVScrollbar = PR_FALSE;
  mFlags.mHasClearance = PR_FALSE;
  mDiscoveredClearance = nsnull;
  mPercentHeightObserver = nsnull;
  mPercentHeightReflowInitiator = nsnull;
  Init(aPresContext);
#ifdef IBMBIDI
  mRightEdge = NS_UNCONSTRAINEDSIZE;
#endif
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
                  !(aFrame->GetStateBits() & (NS_FRAME_IS_DIRTY |
                                              NS_FRAME_HAS_DIRTY_CHILDREN)),
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

  mSpaceManager = aParentReflowState.mSpaceManager;
  mLineLayout = aParentReflowState.mLineLayout;
  mFlags.mIsTopOfPage = aParentReflowState.mFlags.mIsTopOfPage;
  mFlags.mNextInFlowUntouched = aParentReflowState.mFlags.mNextInFlowUntouched &&
    CheckNextInFlowParenthood(aFrame, aParentReflowState.frame);
  mFlags.mAssumingHScrollbar = mFlags.mAssumingVScrollbar = PR_FALSE;
  mFlags.mHasClearance = PR_FALSE;
  mDiscoveredClearance = nsnull;
  mPercentHeightObserver = (aParentReflowState.mPercentHeightObserver && 
                            aParentReflowState.mPercentHeightObserver->NeedsToObserve(*this)) 
                           ? aParentReflowState.mPercentHeightObserver : nsnull;
  mPercentHeightReflowInitiator = aParentReflowState.mPercentHeightReflowInitiator;

  if (aInit) {
    Init(aPresContext, aContainingBlockWidth, aContainingBlockHeight);
  }

#ifdef IBMBIDI
  mRightEdge = aParentReflowState.mRightEdge;
#endif // IBMBIDI
}

inline void
nsCSSOffsetState::ComputeWidthDependentValue(nscoord aContainingBlockWidth,
                                             const nsStyleCoord& aCoord,
                                             nscoord& aResult)
{
  aResult = nsLayoutUtils::ComputeWidthDependentValue(rendContext, frame,
                                                      aContainingBlockWidth,
                                                      aCoord);
}

inline void
nsCSSOffsetState::ComputeHeightDependentValue(nscoord aContainingBlockHeight,
                                              const nsStyleCoord& aCoord,
                                              nscoord& aResult)
{
  aResult = nsLayoutUtils::ComputeHeightDependentValue(rendContext, frame,
                                                       aContainingBlockHeight,
                                                       aCoord);
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

  nscoord oldComputedWidth = mComputedWidth;
  mComputedWidth = aComputedWidth;
  if (mComputedWidth != oldComputedWidth &&
      frame->GetType() != nsGkAtoms::viewportFrame) {  // Or check GetParent()?
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
  NS_ASSERTION(availableWidth != NS_UNCONSTRAINEDSIZE,
               "shouldn't use unconstrained widths anymore");

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

  NS_ASSERTION((mFrameType == NS_CSS_FRAME_TYPE_INLINE &&
                !frame->IsFrameOfType(nsIFrame::eReplaced)) ||
               frame->GetType() == nsGkAtoms::textFrame ||
               mComputedWidth != NS_UNCONSTRAINEDSIZE,
               "shouldn't use unconstrained widths anymore");
}

void nsHTMLReflowState::InitCBReflowState()
{
  if (!parentReflowState) {
    mCBReflowState = nsnull;
    return;
  }

  if (parentReflowState->frame->IsContainingBlock() ||
      // Absolutely positioned frames should always be kids of the frames that
      // determine their containing block
      (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE)) {
    // a block inside a table cell needs to use the table cell
    if (parentReflowState->parentReflowState &&
        IS_TABLE_CELL(parentReflowState->parentReflowState->frame->GetType())) {
      mCBReflowState = parentReflowState->parentReflowState;
    } else {
      mCBReflowState = parentReflowState;
    }
      
    return;
  }
  
  mCBReflowState = parentReflowState->mCBReflowState;
}

void
nsHTMLReflowState::InitResizeFlags(nsPresContext* aPresContext)
{
  mFlags.mHResize = !(frame->GetStateBits() & NS_FRAME_IS_DIRTY) &&
                    frame->GetSize().width !=
                      mComputedWidth + mComputedBorderPadding.LeftRight();

  // XXX Should we really need to null check mCBReflowState?  (We do for
  // at least nsBoxFrame).
  if (mFlags.mSpecialHeightReflow && IS_TABLE_CELL(frame->GetType()) &&
      (frame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT)) {
    mFlags.mVResize = PR_TRUE;
  } else if (mCBReflowState && !frame->IsContainingBlock()) {
    // XXX Is this problematic for relatively positioned inlines acting
    // as containing block for absolutely positioned elements?
    mFlags.mVResize = mCBReflowState->mFlags.mVResize;
  } else if (mComputedHeight == NS_AUTOHEIGHT) {
    if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
        mCBReflowState) {
      // XXX This condition doesn't quite match CalcQuirkContainingBlockHeight.
      mFlags.mVResize = mCBReflowState->mFlags.mVResize;
    } else {
      mFlags.mVResize = mFlags.mHResize || 
                        (frame->GetStateBits() &
                         (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN));
    }
  } else {
    // not 'auto' height
    mFlags.mVResize = frame->GetSize().height !=
                        mComputedHeight + mComputedBorderPadding.TopBottom();
  }

  // It would be nice to check that |mComputedHeight != NS_AUTOHEIGHT|
  // &&ed with the percentage height check.  However, this doesn't get
  // along with table special height reflows, since a special height
  // reflow (a quirk that makes such percentage heights work on children
  // of table cells) can cause not just a single percentage height to
  // become fixed, but an entire descendant chain of percentage heights
  // to become fixed.
  if ((mStylePosition->mHeight.GetUnit() == eStyleUnit_Percent ||
       mStylePosition->mMinHeight.GetUnit() == eStyleUnit_Percent ||
       mStylePosition->mMaxHeight.GetUnit() == eStyleUnit_Percent ||
       mStylePosition->mOffset.GetTopUnit() == eStyleUnit_Percent ||
       mStylePosition->mOffset.GetBottomUnit() == eStyleUnit_Percent ||
       frame->IsBoxFrame()) &&
      mCBReflowState) {
    const nsHTMLReflowState *rs = this;
    do {
      rs = rs->parentReflowState;
      if (rs->frame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT)
        break; // no need to go further
      rs->frame->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
    } while (rs != mCBReflowState);
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
  // Make sure the frame was actually moved out of the flow, and don't
  // just assume what the style says
  // XXXldb nsRuleNode::ComputeDisplayData should take care of this, right?
  if (frame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    if (disp->IsAbsolutelyPositioned()) {
      frameType = NS_CSS_FRAME_TYPE_ABSOLUTE;
    }
    else if (NS_STYLE_FLOAT_NONE != disp->mFloats) {
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
                                          nscoord aContainingBlockHeight)
{
  nsStyleCoord  coord;

  // Compute the 'left' and 'right' values. 'Left' moves the boxes to the right,
  // and 'right' moves the boxes to the left. The computed values are always:
  // left=-right
  PRBool  leftIsAuto = eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit();
  PRBool  rightIsAuto = eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit();

  // Check for percentage based values and an unconstrained containing
  // block width. Treat them like 'auto'
  if (NS_UNCONSTRAINEDSIZE == aContainingBlockWidth) {
    if (eStyleUnit_Percent == mStylePosition->mOffset.GetLeftUnit()) {
      leftIsAuto = PR_TRUE;
    }
    if (eStyleUnit_Percent == mStylePosition->mOffset.GetRightUnit()) {
      rightIsAuto = PR_TRUE;
    }
  }

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
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 mStylePosition->mOffset.GetRight(coord),
                                 mComputedOffsets.right);
      
      // Computed value for 'left' is minus the value of 'right'
      mComputedOffsets.left = -mComputedOffsets.right;
    }

  } else {
    NS_ASSERTION(rightIsAuto, "unexpected specified constraint");
    
    // 'Left' isn't 'auto' so compute its value
    ComputeWidthDependentValue(aContainingBlockWidth,
                               mStylePosition->mOffset.GetLeft(coord),
                               mComputedOffsets.left);

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
    if (eStyleUnit_Percent == mStylePosition->mOffset.GetTopUnit()) {
      topIsAuto = PR_TRUE;
    }
    if (eStyleUnit_Percent == mStylePosition->mOffset.GetBottomUnit()) {
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
      ComputeHeightDependentValue(aContainingBlockHeight,
                                  mStylePosition->mOffset.GetBottom(coord),
                                  mComputedOffsets.bottom);
      
      // Computed value for 'top' is minus the value of 'bottom'
      mComputedOffsets.top = -mComputedOffsets.bottom;
    }

  } else {
    NS_ASSERTION(bottomIsAuto, "unexpected specified constraint");
    
    // 'Top' isn't 'auto' so compute its value
    ComputeHeightDependentValue(aContainingBlockHeight,
                                mStylePosition->mOffset.GetTop(coord),
                                mComputedOffsets.top);

    // Computed value for 'bottom' is minus the value of 'top'
    mComputedOffsets.bottom = -mComputedOffsets.top;
  }
}

nsIFrame*
nsHTMLReflowState::GetNearestContainingBlock(nsIFrame* aFrame, nscoord& aCBLeftEdge,
                                             nscoord& aCBWidth)
{
  for (aFrame = aFrame->GetParent(); aFrame && !aFrame->IsContainingBlock();
       aFrame = aFrame->GetParent())
    /* do nothing */;

  NS_ASSERTION(aFrame, "Must find containing block somewhere");
  NS_ASSERTION(aFrame != frame, "How did that happen?");

  /* Now aFrame is the containing block we want */

  /* Check whether the containing block is currently being reflown.
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
  nscoord       mLeft, mRight;
  nscoord       mTop;
  PRPackedBool  mLeftIsExact, mRightIsExact;

  nsHypotheticalBox() {
    mLeftIsExact = mRightIsExact = PR_FALSE;
  }
};
      
static PRBool
GetIntrinsicSizeFor(nsIFrame* aFrame, nsSize& aIntrinsicSize)
{
  // See if it is an image frame
  PRBool    result = PR_FALSE;

  // Currently the only type of replaced frame that we can get the intrinsic
  // size for is an image frame
  // XXX We should add back the GetReflowMetrics() function and one of the
  // things should be the intrinsic size...
  if (aFrame->GetType() == nsGkAtoms::imageFrame) {
    nsImageFrame* imageFrame = (nsImageFrame*)aFrame;

    imageFrame->GetIntrinsicImageSize(aIntrinsicSize);
    result = (aIntrinsicSize != nsSize(0, 0));
  }
  return result;
}

nscoord
nsHTMLReflowState::CalculateHorizBorderPaddingMargin(nscoord aContainingBlockWidth)
{
  const nsMargin& border = mStyleBorder->GetBorder();
  nsMargin padding, margin;

  // See if the style system can provide us the padding directly
  if (!mStylePadding->GetPadding(padding)) {
    nsStyleCoord left, right;

    // We have to compute the left and right values
    ComputeWidthDependentValue(aContainingBlockWidth,
                               mStylePadding->mPadding.GetLeft(left),
                               padding.left);
    ComputeWidthDependentValue(aContainingBlockWidth,
                               mStylePadding->mPadding.GetRight(right),
                               padding.right);
  }

  // See if the style system can provide us the margin directly
  if (!mStyleMargin->GetMargin(margin)) {
    nsStyleCoord left, right;

    // We have to compute the left and right values
    if (eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit()) {
      // XXX FIXME (or does CalculateBlockSideMargins do this?)
      margin.left = 0;  // just ignore
    } else {
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 mStyleMargin->mMargin.GetLeft(left),
                                 margin.left);
    }
    if (eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit()) {
      // XXX FIXME (or does CalculateBlockSideMargins do this?)
      margin.right = 0;  // just ignore
    } else {
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 mStyleMargin->mMargin.GetRight(right),
                                 margin.right);
    }
  }

  return padding.left + padding.right + border.left + border.right +
         margin.left + margin.right;
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
  nsStyleUnit widthUnit = mStylePosition->mWidth.GetUnit();
  nsSize      intrinsicSize;
  PRBool      knowIntrinsicSize = PR_FALSE;
  if (NS_FRAME_IS_REPLACED(mFrameType) && (eStyleUnit_Auto == widthUnit)) {
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
    nscoord horizBorderPaddingMargin;

    // Determine the total amount of horizontal border/padding/margin that
    // the element would have had if it had been in the flow. Note that we
    // ignore any 'auto' and 'inherit' values
    horizBorderPaddingMargin = CalculateHorizBorderPaddingMargin(aBlockContentWidth);

    if (NS_FRAME_IS_REPLACED(mFrameType) && (eStyleUnit_Auto == widthUnit)) {
      // It's a replaced element with an 'auto' width so the box width is
      // its intrinsic size plus any border/padding/margin
      if (knowIntrinsicSize) {
        boxWidth = intrinsicSize.width + horizBorderPaddingMargin;
        knowBoxWidth = PR_TRUE;
      }

    } else if (eStyleUnit_Auto == widthUnit) {
      // The box width is the containing block width
      boxWidth = aBlockContentWidth;
      knowBoxWidth = PR_TRUE;
    
    } else {
      // We need to compute it. It's important we do this, because if it's
      // percentage based this computed value may be different from the comnputed
      // value calculated using the absolute containing block width
      ComputeWidthDependentValue(aBlockContentWidth, mStylePosition->mWidth,
                                 boxWidth);
      boxWidth += horizBorderPaddingMargin;
      knowBoxWidth = PR_TRUE;
    }
  }
  
  // Get the 'direction' of the block
  const nsStyleVisibility* blockVis = aContainingBlock->GetStyleVisibility();

  // Get the placeholder x-offset and y-offset in the coordinate
  // space of the block frame that contains it
  // XXXbz the placeholder is not fully reflown yet if our containing block is
  // relatively positioned...
  nsPoint placeholderOffset = aPlaceholderFrame->GetOffsetTo(aContainingBlock);

  // First, determine the hypothetical box's mTop
  nsBlockFrame* blockFrame;
  if (NS_SUCCEEDED(aContainingBlock->QueryInterface(kBlockFrameCID,
                                  NS_REINTERPRET_CAST(void**, &blockFrame)))) {
    // We need the immediate child of the block frame, and that may not be
    // the placeholder frame
    nsIFrame *blockChild =
      nsLayoutUtils::FindChildContainingDescendant(blockFrame, aPlaceholderFrame);
    nsBlockFrame::line_iterator lineBox = blockFrame->FindLineFor(blockChild);

    // How we determine the hypothetical box depends on whether the element
    // would have been inline-level or block-level
    if (NS_STYLE_DISPLAY_INLINE == mStyleDisplay->mOriginalDisplay) {
      // Use the top of the inline box which the placeholder lives in as the
      // hypothetical box's top.
      aHypotheticalBox.mTop = lineBox->mBounds.y;
    } else {
      // The element would have been block-level which means it would be below
      // the line containing the placeholder frame, unless all the frames
      // before it are empty.  In that case, it would have been just before
      // this line.      
      // XXXbz the line box is not fully reflown yet if our containing block is
      // relatively positioned...
      if (lineBox != blockFrame->end_lines()) {
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
          // The top of the hypothetical box is the top of the line containing
          // the placeholder, since there is nothing in the line before our
          // placeholder except empty frames.
          aHypotheticalBox.mTop = lineBox->mBounds.y;
        } else {
          // The top of the hypothetical box is just below the line containing
          // the placeholder.
          aHypotheticalBox.mTop = lineBox->mBounds.YMost();
        }
      } else {
        // Just use the placeholder's y-offset
        aHypotheticalBox.mTop = placeholderOffset.y;
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
    aHypotheticalBox.mLeftIsExact = PR_TRUE;

    if (knowBoxWidth) {
      aHypotheticalBox.mRight = aHypotheticalBox.mLeft + boxWidth;
      aHypotheticalBox.mRightIsExact = PR_TRUE;
    } else {
      // We can't compute the right edge because we don't know the desired
      // width. So instead use the right content edge of the block parent,
      // but remember it's not exact
      aHypotheticalBox.mRight = aBlockLeftContentEdge + aBlockContentWidth;
      aHypotheticalBox.mRightIsExact = PR_FALSE;
    }

  } else {
    // The placeholder represents the right edge of the hypothetical box
    if (NS_STYLE_DISPLAY_INLINE == mStyleDisplay->mOriginalDisplay) {
      aHypotheticalBox.mRight = placeholderOffset.x;
    } else {
      aHypotheticalBox.mRight = aBlockLeftContentEdge + aBlockContentWidth;
    }
    aHypotheticalBox.mRightIsExact = PR_TRUE;
    
    if (knowBoxWidth) {
      aHypotheticalBox.mLeft = aHypotheticalBox.mRight - boxWidth;
      aHypotheticalBox.mLeftIsExact = PR_TRUE;
    } else {
      // We can't compute the left edge because we don't know the desired
      // width. So instead use the left content edge of the block parent,
      // but remember it's not exact
      aHypotheticalBox.mLeft = aBlockLeftContentEdge;
      aHypotheticalBox.mLeftIsExact = PR_FALSE;
    }

  }

  // The current coordinate space is that of the nearest block to the placeholder.
  // Convert to the coordinate space of the absolute containing block
  // One weird thing here is that for fixed-positioned elements we want to do
  // the conversion incorrectly; specifically we want to ignore any scrolling
  // that may have happened;
  nsPoint cbOffset;
  if (mStyleDisplay->mPosition == NS_STYLE_POSITION_FIXED) {
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
  aHypotheticalBox.mRight -= border.right;
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

  // Get the placeholder frame
  nsIFrame*     placeholderFrame;

  aPresContext->PresShell()->GetPlaceholderFrameFor(frame, &placeholderFrame);
  NS_ASSERTION(nsnull != placeholderFrame, "no placeholder frame");

  // Find the nearest containing block frame to the placeholder frame,
  // and return its left edge and width.
  nscoord cbLeftEdge, cbWidth;
  nsIFrame* cbFrame = GetNearestContainingBlock(placeholderFrame, cbLeftEdge,
                                                cbWidth);
  
  // If both 'left' and 'right' are 'auto' or both 'top' and 'bottom' are
  // 'auto', then compute the hypothetical box of where the element would
  // have been if it had been in the flow
  nsHypotheticalBox hypotheticalBox;
  if (((eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit()) &&
       (eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit())) ||
      ((eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit()) &&
       (eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit()))) {

    CalculateHypotheticalBox(aPresContext, placeholderFrame, cbFrame,
                             cbLeftEdge, cbWidth, cbrs, hypotheticalBox);
  }

  // Initialize the 'left' and 'right' computed offsets
  // XXX Handle new 'static-position' value...
  PRBool        leftIsAuto = PR_FALSE, rightIsAuto = PR_FALSE;
  nsStyleCoord  coord;
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit()) {
    mComputedOffsets.left = 0;
    leftIsAuto = PR_TRUE;
  } else {
    ComputeWidthDependentValue(containingBlockWidth,
                               mStylePosition->mOffset.GetLeft(coord),
                               mComputedOffsets.left);
  }
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit()) {
    mComputedOffsets.right = 0;
    rightIsAuto = PR_TRUE;
  } else {
    ComputeWidthDependentValue(containingBlockWidth,
                               mStylePosition->mOffset.GetRight(coord),
                               mComputedOffsets.right);
  }

  PRUint8 direction = cbrs ? cbrs->mStyleVisibility->mDirection : NS_STYLE_DIRECTION_LTR;

  // Use the horizontal component of the hypothetical box in the cases
  // where it's needed.
  if (leftIsAuto && rightIsAuto) {
    // Use the 'direction' to dictate whether 'left' or 'right' is
    // treated like 'static-position'
    if (NS_STYLE_DIRECTION_LTR == direction) {
      if (hypotheticalBox.mLeftIsExact) {
        mComputedOffsets.left = hypotheticalBox.mLeft;
        leftIsAuto = PR_FALSE;
      } else {
        // Well, we don't know 'left' so we have to use 'right' and
        // then solve for 'left'
        mComputedOffsets.right = hypotheticalBox.mRight;
        rightIsAuto = PR_FALSE;
      }
    } else {
      if (hypotheticalBox.mRightIsExact) {
        mComputedOffsets.right = containingBlockWidth - hypotheticalBox.mRight;
        rightIsAuto = PR_FALSE;
      } else {
        // Well, we don't know 'right' so we have to use 'left' and
        // then solve for 'right'
        mComputedOffsets.left = hypotheticalBox.mLeft;
        leftIsAuto = PR_FALSE;
      }
    }
  }

  // Initialize the 'top' and 'bottom' computed offsets
  PRBool      topIsAuto = PR_FALSE, bottomIsAuto = PR_FALSE;
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit()) {
    mComputedOffsets.top = 0;
    topIsAuto = PR_TRUE;
  } else {
    nsStyleCoord c;
    ComputeHeightDependentValue(containingBlockHeight,
                                mStylePosition->mOffset.GetTop(c),
                                mComputedOffsets.top);
  }
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit()) {
    mComputedOffsets.bottom = 0;        
    bottomIsAuto = PR_TRUE;
  } else {
    nsStyleCoord c;
    ComputeHeightDependentValue(containingBlockHeight,
                                mStylePosition->mOffset.GetBottom(c),
                                mComputedOffsets.bottom);
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
      // We're over-constrained so use 'direction' to dictate which
      // value to ignore.  (And note that the spec says to ignore 'left'
      // or 'right' rather than 'margin-left' or 'margin-right'.)
      if (NS_STYLE_DIRECTION_LTR == direction) {
        // Ignore the specified value for 'right'.
        mComputedOffsets.right += availMarginSpace;
      } else {
        // Ignore the specified value for 'left'.
        mComputedOffsets.left += availMarginSpace;
      }
    } else if (marginLeftIsAuto) {
      if (marginRightIsAuto) {
        // Both 'margin-left' and 'margin-right' are 'auto', so they get
        // equal values
        mComputedMargin.left = availMarginSpace / 2;
        mComputedMargin.right = availMarginSpace - mComputedMargin.left;
      } else {
        // Just 'margin-left' is 'auto'
        mComputedMargin.left = availMarginSpace - mComputedMargin.right;
      }
    } else {
      // Just 'margin-right' is 'auto'
      mComputedMargin.right = availMarginSpace - mComputedMargin.left;
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
        mComputedMargin.top = availMarginSpace - mComputedMargin.bottom;
      }
    } else {
      // Just 'margin-bottom' is 'auto'
      mComputedMargin.bottom = availMarginSpace - mComputedMargin.top;
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
 *  When we encounter scrolledContent area frames, we skip over them, since they are guaranteed to not be useful for computing the containing block.
 */
nscoord
CalcQuirkContainingBlockHeight(const nsHTMLReflowState* aCBReflowState)
{
  nsHTMLReflowState* firstAncestorRS = nsnull; // a candidate for html frame
  nsHTMLReflowState* secondAncestorRS = nsnull; // a candidate for body frame
  
  // initialize the default to NS_AUTOHEIGHT as this is the containings block
  // computed height when this function is called. It is possible that we 
  // don't alter this height especially if we are restricted to one level
  nscoord result = NS_AUTOHEIGHT; 
                             
  const nsHTMLReflowState* rs = aCBReflowState;
  for (; rs && rs->frame; rs = (nsHTMLReflowState *)(rs->parentReflowState)) { 
    nsIAtom* frameType = rs->frame->GetType();
    // if the ancestor is auto height then skip it and continue up if it 
    // is the first block/area frame and possibly the body/html
    if (nsGkAtoms::blockFrame == frameType ||
        nsGkAtoms::areaFrame == frameType ||
        nsGkAtoms::scrollFrame == frameType) {
      
      if (nsGkAtoms::areaFrame == frameType) {
        // Skip over scrolled-content area frames
        if (rs->frame->GetStyleContext()->GetPseudoType() ==
            nsCSSAnonBoxes::scrolledContent) {
          continue;
        }
      }

      secondAncestorRS = firstAncestorRS;
      firstAncestorRS = (nsHTMLReflowState*)rs;

      // If the current frame we're looking at is positioned, we don't want to
      // go any further (see bug 221784).  The behavior we want here is: 1) If
      // not auto-height, use this as the percentage base.  2) If auto-height,
      // keep looking, unless the frame is positioned.
      if (NS_AUTOHEIGHT == rs->mComputedHeight) {
        if (rs->frame->GetStyleDisplay()->IsAbsolutelyPositioned()) {
          break;
        } else {
          continue;
        }
      }
    }
    else if (nsGkAtoms::canvasFrame == frameType) {
      // Use scroll frames' computed height if we have one, this will
      // allow us to get viewport height for native scrollbars.
      nsHTMLReflowState* scrollState = (nsHTMLReflowState *)rs->parentReflowState;
      if (nsGkAtoms::scrollFrame == scrollState->frame->GetType()) {
        rs = scrollState;
      }
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
             ? rs->availableHeight : rs->mComputedHeight;
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
    // if we got to the html frame, then subtract out 
    // margin/border/padding for the BODY element
    else if (nsGkAtoms::areaFrame == frameType) {
      // make sure it is the body
      if (nsGkAtoms::canvasFrame == rs->parentReflowState->frame->GetType()) {
        result -= GetVerticalMarginBorderPadding(secondAncestorRS);
      }
    }
    break;
  }

  // Make sure not to return a negative height here!
  return PR_MAX(result, 0);
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

      // If the containing block is the initial containing block and it has a
      // height that depends on its content, then use the viewport height instead.
      // This gives us a reasonable value against which to compute percentage
      // based heights and to do bottom relative positioning
      if ((NS_AUTOHEIGHT == aContainingBlockHeight) &&
          nsLayoutUtils::IsInitialContainingBlock(aContainingBlockRS->frame)) {

        // Use the viewport height as the containing block height
        const nsHTMLReflowState* rs = aContainingBlockRS->parentReflowState;
        while (rs) {
          aContainingBlockHeight = rs->mComputedHeight;
          rs = rs->parentReflowState;
        }

      } else {
        aContainingBlockHeight +=
          aContainingBlockRS->mComputedPadding.TopBottom();
      }
    }
  } else {
    // an element in quirks mode gets a containing block based on looking for a
    // parent with a non-auto height if the element has a percent height
    if (NS_AUTOHEIGHT == aContainingBlockHeight) {
      if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
          mStylePosition->mHeight.GetUnit() == eStyleUnit_Percent) {
        aContainingBlockHeight = CalcQuirkContainingBlockHeight(aContainingBlockRS);
      }
    }
  }
}

// Prefs callback to pick up changes
PR_STATIC_CALLBACK(int)
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

#ifdef FONT_LEADING_APIS_V2
static eNormalLineHeightControl GetNormalLineHeightCalcControl(void)
{
  if (sNormalLineHeightControl == eUninitialized) {
    // browser.display.normal_lineheight_calc_control is not user
    // changeable, so no need to register callback for it.
    sNormalLineHeightControl =
      NS_STATIC_CAST(eNormalLineHeightControl,
                     nsContentUtils::GetIntPref("browser.display.normal_lineheight_calc_control", eNoExternalLeading));
  }
  return sNormalLineHeightControl;
}
#endif

// XXX refactor this code to have methods for each set of properties
// we are computing: width,height,line-height; margin; offsets

void
nsHTMLReflowState::InitConstraints(nsPresContext* aPresContext,
                                   nscoord         aContainingBlockWidth,
                                   nscoord         aContainingBlockHeight,
                                   const nsMargin* aBorder,
                                   const nsMargin* aPadding)
{
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

#if 0
    nsFrame::ListTag(stdout, frame); printf(": cb=");
    nsFrame::ListTag(stdout, cbrs->frame); printf(" size=%d,%d\n", aContainingBlockWidth, aContainingBlockHeight);
#endif

    // See if the containing block height is based on the size of its
    // content
    nsIAtom* fType;
    if (NS_AUTOHEIGHT == aContainingBlockHeight) {
      // See if the containing block is (1) a scrolled frame, i.e. its
      // parent is a scroll frame. The presence of the intervening
      // frame (that the scroll frame scrolls) needs to be hidden from
      // the containingBlockHeight calcuation, or (2) a cell frame which needs
      // to use the mComputedHeight of the cell instead of what the cell block passed in.
      if (cbrs->parentReflowState) {
        nsIFrame* f = cbrs->parentReflowState->frame;
        fType = f->GetType();
        if (nsGkAtoms::scrollFrame == fType) {
          // Use the scroll frame's computed height instead
          aContainingBlockHeight = cbrs->parentReflowState->mComputedHeight;
        }
        else {
          fType = cbrs->frame->GetType();
          if (IS_TABLE_CELL(fType)) {
            // use the cell's computed height 
            aContainingBlockHeight = cbrs->mComputedHeight;
          }
        }
      }
    }

    InitOffsets(aContainingBlockWidth, aBorder, aPadding);

    nsStyleUnit heightUnit = mStylePosition->mHeight.GetUnit();

    // Check for a percentage based height and a containing block height
    // that depends on the content height
    // XXX twiddling heightUnit doesn't help anymore
    if (eStyleUnit_Percent == heightUnit) {
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
      ComputeRelativeOffsets(cbrs, aContainingBlockWidth, aContainingBlockHeight);
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
      nsStyleUnit widthUnit = mStylePosition->mWidth.GetUnit();
      if ((NS_STYLE_DISPLAY_TABLE_ROW == mStyleDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == mStyleDisplay->mDisplay)) {
        // 'width' property doesn't apply to table rows and row groups
        widthUnit = eStyleUnit_Auto;
        rowOrRowGroup = PR_TRUE;
      }

      if (eStyleUnit_Auto == widthUnit) {
        mComputedWidth = availableWidth;

        if ((mComputedWidth != NS_UNCONSTRAINEDSIZE) && !rowOrRowGroup){
          // Internal table elements don't have margins. Only tables and
          // cells have border and padding
          mComputedWidth -= mComputedBorderPadding.left +
            mComputedBorderPadding.right;
        }
      
      } else {
        NS_ASSERTION(widthUnit == mStylePosition->mWidth.GetUnit(),
                     "unexpected width unit change");
        ComputeWidthDependentValue(aContainingBlockWidth,
                                   mStylePosition->mWidth, mComputedWidth);
      }

      // Calculate the computed height
      if ((NS_STYLE_DISPLAY_TABLE_COLUMN == mStyleDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == mStyleDisplay->mDisplay)) {
        // 'height' property doesn't apply to table columns and column groups
        heightUnit = eStyleUnit_Auto;
      }
      if (eStyleUnit_Auto == heightUnit) {
        mComputedHeight = NS_AUTOHEIGHT;
      } else {
        NS_ASSERTION(heightUnit == mStylePosition->mHeight.GetUnit(),
                     "unexpected height unit change");
        ComputeHeightDependentValue(aContainingBlockHeight,
                                    mStylePosition->mHeight,
                                    mComputedHeight);
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
                           !isBlock);

      mComputedWidth = size.width;
      mComputedHeight = size.height;

      if (isBlock)
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
  // Compute margins from the specified margin style information. These
  // become the default computed values, and may be adjusted below
  // XXX fix to provide 0,0 for the top&bottom margins for
  // inline-non-replaced elements
  ComputeMargin(aContainingBlockWidth);

  const nsStyleDisplay *disp = frame->GetStyleDisplay();
  PRBool isThemed = frame->IsThemed(disp);
  nsPresContext *presContext = frame->PresContext();

  if (isThemed &&
      presContext->GetTheme()->GetWidgetPadding(presContext->DeviceContext(),
                                                frame, disp->mAppearance,
                                                &mComputedPadding)) {
    mComputedPadding.top = presContext->DevPixelsToAppUnits(mComputedPadding.top);
    mComputedPadding.right = presContext->DevPixelsToAppUnits(mComputedPadding.right);
    mComputedPadding.bottom = presContext->DevPixelsToAppUnits(mComputedPadding.bottom);
    mComputedPadding.left = presContext->DevPixelsToAppUnits(mComputedPadding.left);
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
    presContext->GetTheme()->GetWidgetBorder(presContext->DeviceContext(),
                                             frame, disp->mAppearance,
                                             &mComputedBorderPadding);
    mComputedBorderPadding.top =
      presContext->DevPixelsToAppUnits(mComputedBorderPadding.top);
    mComputedBorderPadding.right =
      presContext->DevPixelsToAppUnits(mComputedBorderPadding.right);
    mComputedBorderPadding.bottom =
      presContext->DevPixelsToAppUnits(mComputedBorderPadding.bottom);
    mComputedBorderPadding.left =
      presContext->DevPixelsToAppUnits(mComputedBorderPadding.left);
  }
  else if (aBorder) {  // border is an input arg
    mComputedBorderPadding = *aBorder;
  }
  else {
    mComputedBorderPadding = frame->GetStyleBorder()->GetBorder();
  }
  mComputedBorderPadding += mComputedPadding;

  if (frame->GetType() == nsGkAtoms::tableFrame) {
    nsTableFrame *tableFrame = NS_STATIC_CAST(nsTableFrame*, frame);

    if (tableFrame->IsBorderCollapse()) {
      // border-collapsed tables don't use any of their padding, and
      // only part of their border.  We need to do this here before we
      // try to do anything like handling 'auto' widths,
      // '-moz-box-sizing', or 'auto' margins.
      mComputedPadding.SizeTo(0,0,0,0);
      mComputedBorderPadding = tableFrame->GetIncludedOuterBCBorder();
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
  NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aComputedWidth &&
               NS_UNCONSTRAINEDSIZE != aAvailWidth,
               "this shouldn't happen anymore");

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
  if (isAutoLeftMargin) {
    if (isAutoRightMargin) {
      // Both margins are 'auto' so their computed values are equal
      mComputedMargin.left = availMarginSpace / 2;
      mComputedMargin.right = availMarginSpace - mComputedMargin.left;
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

#ifdef FONT_LEADING_APIS_V2
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
#else
  aFontMetrics->GetNormalLineHeight(normalLineHeight);
#endif // FONT_LEADING_APIS_V2
  return normalLineHeight;
}

static nscoord
ComputeLineHeight(nsPresContext* aPresContext,
                  nsIRenderingContext* aRenderingContext,
                  nsStyleContext* aStyleContext)
{
  NS_PRECONDITION(nsnull != aRenderingContext, "no rendering context");

  nscoord lineHeight;

  const nsStyleFont* font = aStyleContext->GetStyleFont();
  const nsStyleCoord& lhCoord = aStyleContext->GetStyleText()->mLineHeight;
  
  nsStyleUnit unit = lhCoord.GetUnit();

  if (unit == eStyleUnit_Coord) {
    // For length values just use the pre-computed value
    lineHeight = lhCoord.GetCoordValue();
  } else if (unit == eStyleUnit_Factor) {
    // For factor units the computed value of the line-height property 
    // is found by multiplying the factor by the font's computed size
    // (adjusted for min-size prefs and text zoom).
    float factor = lhCoord.GetFactorValue();
    lineHeight = NSToCoordRound(factor * font->mFont.size);
  } else {
    NS_ASSERTION(eStyleUnit_Normal == unit, "bad unit");
    nsCOMPtr<nsIDeviceContext> deviceContext;
    aRenderingContext->GetDeviceContext(*getter_AddRefs(deviceContext));
    const nsStyleVisibility* vis = aStyleContext->GetStyleVisibility();
    nsCOMPtr<nsIFontMetrics> fm;
    deviceContext->GetMetricsFor(font->mFont, vis->mLangGroup,
                                 *getter_AddRefs(fm));
    lineHeight = GetNormalLineHeight(fm);
  }
  return lineHeight;
}

nscoord
nsHTMLReflowState::CalcLineHeight(nsPresContext* aPresContext,
                                  nsIRenderingContext* aRenderingContext,
                                  nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame && aFrame->GetStyleContext(),
               "Bogus data passed in to CalcLineHeight");

  nscoord lineHeight = ComputeLineHeight(aPresContext, aRenderingContext,
                                         aFrame->GetStyleContext());

  NS_ASSERTION(lineHeight >= 0, "ComputeLineHeight screwed up");

  return lineHeight;
}

/* static */
void
nsCSSOffsetState::DestroyMarginFunc(void*    aFrame,
                                    nsIAtom* aPropertyName,
                                    void*    aPropertyValue,
                                    void*    aDtorData)
{
  delete NS_STATIC_CAST(nsMargin*, aPropertyValue);
}

void
nsCSSOffsetState::ComputeMargin(nscoord aContainingBlockWidth)
{
  // If style style can provide us the margin directly, then use it.
  const nsStyleMargin *styleMargin = frame->GetStyleMargin();
  if (!styleMargin->GetMargin(mComputedMargin)) {
    // We have to compute the value
    if (NS_UNCONSTRAINEDSIZE == aContainingBlockWidth) {
      mComputedMargin.left = 0;
      mComputedMargin.right = 0;

      if (eStyleUnit_Coord == styleMargin->mMargin.GetLeftUnit()) {
        nsStyleCoord left;
        
        styleMargin->mMargin.GetLeft(left),
        mComputedMargin.left = left.GetCoordValue();
      }
      if (eStyleUnit_Coord == styleMargin->mMargin.GetRightUnit()) {
        nsStyleCoord right;
        
        styleMargin->mMargin.GetRight(right),
        mComputedMargin.right = right.GetCoordValue();
      }

    } else {
      nsStyleCoord left, right;

      ComputeWidthDependentValue(aContainingBlockWidth,
                                 styleMargin->mMargin.GetLeft(left),
                                 mComputedMargin.left);
      ComputeWidthDependentValue(aContainingBlockWidth,
                                 styleMargin->mMargin.GetRight(right),
                                 mComputedMargin.right);
    }

    nsStyleCoord top, bottom;
    // According to the CSS2 spec, margin percentages are
    // calculated with respect to the *width* of the containing
    // block, even for margin-top and margin-bottom.
    // XXX This isn't true for page boxes, if we implement them.
    ComputeWidthDependentValue(aContainingBlockWidth,
                               styleMargin->mMargin.GetTop(top),
                               mComputedMargin.top);
    ComputeWidthDependentValue(aContainingBlockWidth,
                               styleMargin->mMargin.GetBottom(bottom),
                               mComputedMargin.bottom);

    // XXX We need to include 'auto' horizontal margins in this too!
    frame->SetProperty(nsGkAtoms::usedMarginProperty,
                       new nsMargin(mComputedMargin),
                       DestroyMarginFunc);
  }
}

void
nsCSSOffsetState::ComputePadding(nscoord aContainingBlockWidth)
{
  // If style can provide us the padding directly, then use it.
  const nsStylePadding *stylePadding = frame->GetStylePadding();
  if (!stylePadding->GetPadding(mComputedPadding)) {
    // We have to compute the value
    nsStyleCoord left, right, top, bottom;

    ComputeWidthDependentValue(aContainingBlockWidth,
                               stylePadding->mPadding.GetLeft(left),
                               mComputedPadding.left);
    ComputeWidthDependentValue(aContainingBlockWidth,
                               stylePadding->mPadding.GetRight(right),
                               mComputedPadding.right);

    // According to the CSS2 spec, percentages are calculated with respect to
    // containing block width for padding-top and padding-bottom
    ComputeWidthDependentValue(aContainingBlockWidth,
                               stylePadding->mPadding.GetTop(top),
                               mComputedPadding.top);
    ComputeWidthDependentValue(aContainingBlockWidth,
                               stylePadding->mPadding.GetBottom(bottom),
                               mComputedPadding.bottom);

    frame->SetProperty(nsGkAtoms::usedPaddingProperty,
                       new nsMargin(mComputedPadding),
                       DestroyMarginFunc);
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
      *aFrameWidth = PR_MIN(*aFrameWidth, mComputedMaxWidth);
    }
    *aFrameWidth = PR_MAX(*aFrameWidth, mComputedMinWidth);
  }

  if (aFrameHeight) {
    if (NS_UNCONSTRAINEDSIZE != mComputedMaxHeight) {
      *aFrameHeight = PR_MIN(*aFrameHeight, mComputedMaxHeight);
    }
    *aFrameHeight = PR_MAX(*aFrameHeight, mComputedMinHeight);
  }
}

void
nsHTMLReflowState::ComputeMinMaxValues(nscoord aContainingBlockWidth,
                                       nscoord aContainingBlockHeight,
                                       const nsHTMLReflowState* aContainingBlockRS)
{
  ComputeWidthDependentValue(aContainingBlockWidth, mStylePosition->mMinWidth,
                             mComputedMinWidth);

  if (eStyleUnit_Null == mStylePosition->mMaxWidth.GetUnit()) {
    // Specified value of 'none'
    mComputedMaxWidth = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    ComputeWidthDependentValue(aContainingBlockWidth, mStylePosition->mMaxWidth,
                               mComputedMaxWidth);
  }

  // If the computed value of 'min-width' is greater than the value of
  // 'max-width', 'max-width' is set to the value of 'min-width'
  if (mComputedMinWidth > mComputedMaxWidth) {
    mComputedMaxWidth = mComputedMinWidth;
  }

  // Check for percentage based values and a containing block height that
  // depends on the content height. Treat them like 'auto'
  if ((NS_AUTOHEIGHT == aContainingBlockHeight) &&
      (eStyleUnit_Percent == mStylePosition->mMinHeight.GetUnit())) {
    mComputedMinHeight = 0;
  } else {
    ComputeHeightDependentValue(aContainingBlockHeight,
                                mStylePosition->mMinHeight, mComputedMinHeight);
  }
  nsStyleUnit maxHeightUnit = mStylePosition->mMaxHeight.GetUnit();
  if (eStyleUnit_Null == maxHeightUnit) {
    // Specified value of 'none'
    mComputedMaxHeight = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    // Check for percentage based values and a containing block height that
    // depends on the content height. Treat them like 'auto'
    if ((NS_AUTOHEIGHT == aContainingBlockHeight) && 
        (eStyleUnit_Percent == maxHeightUnit)) {
      mComputedMaxHeight = NS_UNCONSTRAINEDSIZE;
    } else {
      ComputeHeightDependentValue(aContainingBlockHeight,
                                  mStylePosition->mMaxHeight, mComputedMaxHeight);
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
