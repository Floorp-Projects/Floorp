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
#include "nsCOMPtr.h"
#include "nsStyleConsts.h"
#include "nsCSSAnonBoxes.h"
#include "nsFrame.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsLayoutAtoms.h"
#include "nsIDeviceContext.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsBlockFrame.h"
#include "nsLineBox.h"
#include "nsImageFrame.h"
#include "nsIServiceManager.h"
#include "nsIPercentHeightObserver.h"
#include "nsContentUtils.h"
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

#ifdef DEBUG
const char*
nsHTMLReflowState::ReasonToString(nsReflowReason aReason)
{
  static const char* reasons[] = {
    "initial", "incremental", "resize", "style-change", "dirty"
  };

  return reasons[aReason];
}
#endif

// Initialize a <b>root</b> reflow state with a rendering context to
// use for measuring things.
nsHTMLReflowState::nsHTMLReflowState(nsPresContext*      aPresContext,
                                     nsIFrame*            aFrame,
                                     nsReflowReason       aReason,
                                     nsIRenderingContext* aRenderingContext,
                                     const nsSize&        aAvailableSpace)
  : mReflowDepth(0)
{
  NS_PRECONDITION(nsnull != aRenderingContext, "no rendering context");
  parentReflowState = nsnull;
  frame = aFrame;
  reason = aReason;
  path = nsnull;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;
  rendContext = aRenderingContext;
  mSpaceManager = nsnull;
  mLineLayout = nsnull;
  mFlags.mSpecialHeightReflow = PR_FALSE;
  mFlags.mIsTopOfPage = PR_FALSE;
  mFlags.mNextInFlowUntouched = PR_FALSE;
  mPercentHeightObserver = nsnull;
  mPercentHeightReflowInitiator = nsnull;
  Init(aPresContext);
#ifdef IBMBIDI
  mFlags.mVisualBidiFormControl = IsBidiFormControl(aPresContext);
  mRightEdge = NS_UNCONSTRAINEDSIZE;
#endif
}

// Initialize a <b>root</b> reflow state for an <b>incremental</b>
// reflow.
nsHTMLReflowState::nsHTMLReflowState(nsPresContext*      aPresContext,
                                     nsIFrame*            aFrame,
                                     nsReflowPath*        aReflowPath,
                                     nsIRenderingContext* aRenderingContext,
                                     const nsSize&        aAvailableSpace)
  : mReflowDepth(0)
{
  NS_PRECONDITION(nsnull != aRenderingContext, "no rendering context");  

  reason = eReflowReason_Incremental;
  path = aReflowPath;
  parentReflowState = nsnull;
  frame = aFrame;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;
  rendContext = aRenderingContext;
  mSpaceManager = nsnull;
  mLineLayout = nsnull;
  mFlags.mSpecialHeightReflow = PR_FALSE;
  mFlags.mIsTopOfPage = PR_FALSE;
  mFlags.mNextInFlowUntouched = PR_FALSE;
  mPercentHeightObserver = nsnull;
  mPercentHeightReflowInitiator = nsnull;
  Init(aPresContext);
#ifdef IBMBIDI
  mFlags.mVisualBidiFormControl = IsBidiFormControl(aPresContext);
  mRightEdge = NS_UNCONSTRAINEDSIZE;
#endif // IBMBIDI
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
nsHTMLReflowState::nsHTMLReflowState(nsPresContext*          aPresContext,
                                     const nsHTMLReflowState& aParentReflowState,
                                     nsIFrame*                aFrame,
                                     const nsSize&            aAvailableSpace,
                                     nsReflowReason           aReason,
                                     PRBool                   aInit)
  : mReflowDepth(aParentReflowState.mReflowDepth + 1),
    mFlags(aParentReflowState.mFlags)
{
  parentReflowState = &aParentReflowState;
  frame = aFrame;
  reason = aReason;
  if (reason == eReflowReason_Incremental) {
    // If the child frame isn't along the reflow path, then convert
    // the incremental reflow to a dirty reflow.
    path = aParentReflowState.path->GetSubtreeFor(aFrame);
    if (! path)
      reason = eReflowReason_Dirty;
  }
  else
    path = nsnull;

  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;

  rendContext = aParentReflowState.rendContext;
  mSpaceManager = aParentReflowState.mSpaceManager;
  mLineLayout = aParentReflowState.mLineLayout;
  mFlags.mIsTopOfPage = aParentReflowState.mFlags.mIsTopOfPage;
  mFlags.mNextInFlowUntouched = aParentReflowState.mFlags.mNextInFlowUntouched &&
    CheckNextInFlowParenthood(aFrame, aParentReflowState.frame);
  mPercentHeightObserver = (aParentReflowState.mPercentHeightObserver && 
                            aParentReflowState.mPercentHeightObserver->NeedsToObserve(*this)) 
                           ? aParentReflowState.mPercentHeightObserver : nsnull;
  mPercentHeightReflowInitiator = aParentReflowState.mPercentHeightReflowInitiator;

  if (aInit) {
    Init(aPresContext);
  }

#ifdef IBMBIDI
  mFlags.mVisualBidiFormControl = (aParentReflowState.mFlags.mVisualBidiFormControl) ?
                                  PR_TRUE : IsBidiFormControl(aPresContext);
  mRightEdge = aParentReflowState.mRightEdge;
#endif // IBMBIDI
}

// Same as the previous except that the reason is taken from the
// parent's reflow state.
nsHTMLReflowState::nsHTMLReflowState(nsPresContext*          aPresContext,
                                     const nsHTMLReflowState& aParentReflowState,
                                     nsIFrame*                aFrame,
                                     const nsSize&            aAvailableSpace)
  : mReflowDepth(aParentReflowState.mReflowDepth + 1),
    mFlags(aParentReflowState.mFlags)
{
  parentReflowState = &aParentReflowState;
  frame = aFrame;
  reason = aParentReflowState.reason;
  if (reason == eReflowReason_Incremental) {
    // If the child frame isn't along the reflow path, then convert
    // the incremental reflow to a dirty reflow.
    path = aParentReflowState.path->GetSubtreeFor(aFrame);
    if (! path)
      reason = eReflowReason_Dirty;
  }
  else
    path = nsnull;

  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;

  rendContext = aParentReflowState.rendContext;
  mSpaceManager = aParentReflowState.mSpaceManager;
  mLineLayout = aParentReflowState.mLineLayout;
  mFlags.mIsTopOfPage = aParentReflowState.mFlags.mIsTopOfPage;
  mFlags.mNextInFlowUntouched = aParentReflowState.mFlags.mNextInFlowUntouched &&
    CheckNextInFlowParenthood(aFrame, aParentReflowState.frame);
  mPercentHeightObserver = (aParentReflowState.mPercentHeightObserver && 
                            aParentReflowState.mPercentHeightObserver->NeedsToObserve(*this)) 
                           ? aParentReflowState.mPercentHeightObserver : nsnull;
  mPercentHeightReflowInitiator = aParentReflowState.mPercentHeightReflowInitiator;

  Init(aPresContext);

#ifdef IBMBIDI
  mFlags.mVisualBidiFormControl = (aParentReflowState.mFlags.mVisualBidiFormControl) ?
                                   PR_TRUE : IsBidiFormControl(aPresContext);
  mRightEdge = aParentReflowState.mRightEdge;
#endif // IBMBIDI
}

// Version that species the containing block width and height
nsHTMLReflowState::nsHTMLReflowState(nsPresContext*          aPresContext,
                                     const nsHTMLReflowState& aParentReflowState,
                                     nsIFrame*                aFrame,
                                     const nsSize&            aAvailableSpace,
                                     nscoord                  aContainingBlockWidth,
                                     nscoord                  aContainingBlockHeight,
                                     nsReflowReason           aReason)
  : mReflowDepth(aParentReflowState.mReflowDepth + 1),
    mFlags(aParentReflowState.mFlags)
{
  parentReflowState = &aParentReflowState;
  frame = aFrame;
  reason = aReason;
  if (reason == eReflowReason_Incremental) {
    // If the child frame isn't along the reflow path, then convert
    // the incremental reflow to a dirty reflow.
    path = aParentReflowState.path->GetSubtreeFor(aFrame);
    if (! path)
      reason = eReflowReason_Dirty;
  }
  else
    path = nsnull;

  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;

  rendContext = aParentReflowState.rendContext;
  mSpaceManager = aParentReflowState.mSpaceManager;
  mLineLayout = aParentReflowState.mLineLayout;
  mFlags.mIsTopOfPage = aParentReflowState.mFlags.mIsTopOfPage;
  mFlags.mNextInFlowUntouched = aParentReflowState.mFlags.mNextInFlowUntouched &&
    CheckNextInFlowParenthood(aFrame, aParentReflowState.frame);
  mPercentHeightObserver = (aParentReflowState.mPercentHeightObserver && 
                            aParentReflowState.mPercentHeightObserver->NeedsToObserve(*this)) 
                           ? aParentReflowState.mPercentHeightObserver : nsnull;
  mPercentHeightReflowInitiator = aParentReflowState.mPercentHeightReflowInitiator;

  Init(aPresContext, aContainingBlockWidth, aContainingBlockHeight);

#ifdef IBMBIDI
  mFlags.mVisualBidiFormControl = (aParentReflowState.mFlags.mVisualBidiFormControl) ?
                                   PR_TRUE : IsBidiFormControl(aPresContext);
  mRightEdge = aParentReflowState.mRightEdge;
#endif // IBMBIDI
}

void
nsHTMLReflowState::Init(nsPresContext* aPresContext,
                        nscoord         aContainingBlockWidth,
                        nscoord         aContainingBlockHeight,
                        nsMargin*       aBorder,
                        nsMargin*       aPadding)
{
  mCompactMarginWidth = 0;
#ifdef DEBUG
  mDebugHook = nsnull;
#endif

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

const nsHTMLReflowState*
nsHTMLReflowState::GetPageBoxReflowState(const nsHTMLReflowState* aParentRS)
{
  // XXX write me as soon as we can ask a frame if it's a page frame...
  return nsnull;
}

nscoord
nsHTMLReflowState::GetContainingBlockContentWidth(const nsHTMLReflowState* aReflowState)
{
  const nsHTMLReflowState* rs = aReflowState->mCBReflowState;
  if (!rs)
    return 0;
  return rs->mComputedWidth;
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
    else {
      NS_ASSERTION(NS_STYLE_FLOAT_NONE != disp->mFloats,
                   "unknown out of flow frame type");
      frameType = NS_CSS_FRAME_TYPE_FLOATING;
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
  if (frame->GetStateBits() & NS_FRAME_REPLACED_ELEMENT) {
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
    const nsStyleVisibility* vis = frame->GetStyleVisibility();
    
    if (NS_STYLE_DIRECTION_LTR == vis->mDirection) {
      rightIsAuto = PR_TRUE;
    } else {
      leftIsAuto = PR_TRUE;
    }
  }

  if (leftIsAuto) {
    if (rightIsAuto) {
      // If both are 'auto' (their initial values), the computed values are 0
      mComputedOffsets.left = mComputedOffsets.right = 0;
    } else {
      // 'Right' isn't 'auto' so compute its value
      ComputeHorizontalValue(aContainingBlockWidth,
                             mStylePosition->mOffset.GetRightUnit(),
                             mStylePosition->mOffset.GetRight(coord),
                             mComputedOffsets.right);
      
      // Computed value for 'left' is minus the value of 'right'
      mComputedOffsets.left = -mComputedOffsets.right;
    }

  } else {
    NS_ASSERTION(rightIsAuto, "unexpected specified constraint");
    
    // 'Left' isn't 'auto' so compute its value
    ComputeHorizontalValue(aContainingBlockWidth,
                           mStylePosition->mOffset.GetLeftUnit(),
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
      ComputeVerticalValue(aContainingBlockHeight,
                           mStylePosition->mOffset.GetBottomUnit(),
                           mStylePosition->mOffset.GetBottom(coord),
                           mComputedOffsets.bottom);
      
      // Computed value for 'top' is minus the value of 'bottom'
      mComputedOffsets.top = -mComputedOffsets.bottom;
    }

  } else {
    NS_ASSERTION(bottomIsAuto, "unexpected specified constraint");
    
    // 'Top' isn't 'auto' so compute its value
    ComputeVerticalValue(aContainingBlockHeight,
                         mStylePosition->mOffset.GetTopUnit(),
                         mStylePosition->mOffset.GetTop(coord),
                         mComputedOffsets.top);

    // Computed value for 'bottom' is minus the value of 'top'
    mComputedOffsets.bottom = -mComputedOffsets.top;
  }
}

// Returns the nearest containing block frame for the specified frame.
// Also returns the left, top, right, and bottom edges of the specified
// frame's content area. These are in the coordinate space of the block
// frame itself
static nsIFrame*
GetNearestContainingBlock(nsIFrame* aFrame, nsMargin& aContentArea)
{
  aFrame = aFrame->GetParent();
  while (aFrame) {
    nsIAtom*  frameType = aFrame->GetType();
    PRBool    isBlock =
      (frameType == nsLayoutAtoms::blockFrame) ||
      (frameType == nsLayoutAtoms::areaFrame);

    if (isBlock) {
      break;
    }
    aFrame = aFrame->GetParent();
  }

  if (aFrame) {
    nsSize  size = aFrame->GetSize();

    aContentArea.left = 0;
    aContentArea.top = 0;
    aContentArea.right = size.width;
    aContentArea.bottom = size.height;
  
    // Subtract off for border and padding. If it can't be computed because
    // it's percentage based (for example) then just ignore it
    nsStyleBorderPadding  bPad;
    nsMargin              borderPadding;
    nsStyleContext* styleContext = aFrame->GetStyleContext();
    styleContext->GetBorderPaddingFor(bPad);
    if (bPad.GetBorderPadding(borderPadding)) {
      aContentArea.left += borderPadding.left;
      aContentArea.top += borderPadding.top;
      aContentArea.right -= borderPadding.right;
      aContentArea.bottom -= borderPadding.bottom;
    }
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
  if (aFrame->GetType() == nsLayoutAtoms::imageFrame) {
    nsImageFrame* imageFrame = (nsImageFrame*)aFrame;

    imageFrame->GetIntrinsicImageSize(aIntrinsicSize);
    result = (aIntrinsicSize != nsSize(0, 0));
  }
  return result;
}

nscoord
nsHTMLReflowState::CalculateHorizBorderPaddingMargin(nscoord aContainingBlockWidth)
{
  nsMargin  border, padding, margin;

  // Get the border
  if (!mStyleBorder->GetBorder(border)) {
    // CSS2 has no percentage borders
    border.SizeTo(0, 0, 0, 0);
  }

  // See if the style system can provide us the padding directly
  if (!mStylePadding->GetPadding(padding)) {
    nsStyleCoord left, right;

    // We have to compute the left and right values
    ComputeHorizontalValue(aContainingBlockWidth,
                           mStylePadding->mPadding.GetLeftUnit(),
                           mStylePadding->mPadding.GetLeft(left),
                           padding.left);
    ComputeHorizontalValue(aContainingBlockWidth,
                           mStylePadding->mPadding.GetRightUnit(),
                           mStylePadding->mPadding.GetRight(right),
                           padding.right);
  }

  // See if the style system can provide us the margin directly
  if (!mStyleMargin->GetMargin(margin)) {
    nsStyleCoord left, right;

    // We have to compute the left and right values
    if (eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit()) {
      margin.left = 0;  // just ignore
    } else {
      ComputeHorizontalValue(aContainingBlockWidth,
                             mStyleMargin->mMargin.GetLeftUnit(),
                             mStyleMargin->mMargin.GetLeft(left),
                             margin.left);
    }
    if (eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit()) {
      margin.right = 0;  // just ignore
    } else {
      ComputeHorizontalValue(aContainingBlockWidth,
                             mStyleMargin->mMargin.GetRightUnit(),
                             mStyleMargin->mMargin.GetRight(right),
                             margin.right);
    }
  }

  return padding.left + padding.right + border.left + border.right +
         margin.left + margin.right;
}

static void
GetPlaceholderOffset(nsIFrame* aPlaceholderFrame,
                     nsIFrame* aBlockFrame,
                     nsPoint&  aOffset)
{
  aOffset = aPlaceholderFrame->GetPosition();

  // Convert the placeholder position to the coordinate space of the block
  // frame that contains it
  nsIFrame* parent = aPlaceholderFrame->GetParent();
  while (parent && (parent != aBlockFrame)) {
    aOffset += parent->GetPosition();
    parent = parent->GetParent();
  }
}

static nsIFrame*
FindImmediateChildOf(nsIFrame* aParent, nsIFrame* aDescendantFrame)
{
  nsIFrame* result = aDescendantFrame;

  while (result) {
    nsIFrame* parent = result->GetParent();
    if (parent == aParent) {
      break;
    }

    // The frame is not an immediate child of aParent so walk up another level
    result = parent;
  }

  return result;
}

// Calculate the hypothetical box that the element would have if it were in
// the flow. The values returned are relative to the padding edge of the
// absolute containing block
void
nsHTMLReflowState::CalculateHypotheticalBox(nsPresContext*    aPresContext,
                                            nsIFrame*          aPlaceholderFrame,
                                            nsIFrame*          aBlockFrame,
                                            nsMargin&          aBlockContentArea,
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
    horizBorderPaddingMargin = CalculateHorizBorderPaddingMargin(aBlockContentArea.right -
                                                                 aBlockContentArea.left);

    if (NS_FRAME_IS_REPLACED(mFrameType) && (eStyleUnit_Auto == widthUnit)) {
      // It's a replaced element with an 'auto' width so the box width is
      // its intrinsic size plus any border/padding/margin
      if (knowIntrinsicSize) {
        boxWidth = intrinsicSize.width + horizBorderPaddingMargin;
        knowBoxWidth = PR_TRUE;
      }

    } else if (eStyleUnit_Auto == widthUnit) {
      // The box width is the containing block width
      boxWidth = aBlockContentArea.right - aBlockContentArea.left;
      knowBoxWidth = PR_TRUE;
    
    } else {
      // We need to compute it. It's important we do this, because if it's
      // percentage based this computed value may be different from the comnputed
      // value calculated using the absolute containing block width
      ComputeHorizontalValue(aBlockContentArea.right - aBlockContentArea.left,
                             widthUnit, mStylePosition->mWidth, boxWidth);
      boxWidth += horizBorderPaddingMargin;
      knowBoxWidth = PR_TRUE;
    }
  }
  
  // Get the 'direction' of the block
  const nsStyleVisibility* blockVis = aBlockFrame->GetStyleVisibility();

  // Get the placeholder x-offset and y-offset in the coordinate
  // space of the block frame that contains it
  // XXXbz the placeholder is not fully reflown yet if our containing block is
  // relatively positioned...
  nsPoint placeholderOffset;
  GetPlaceholderOffset(aPlaceholderFrame, aBlockFrame, placeholderOffset);

  // First, determine the hypothetical box's mTop
  if (aBlockFrame) {
    // We need the immediate child of the block frame, and that may not be
    // the placeholder frame
    nsBlockFrame* blockFrame = NS_STATIC_CAST(nsBlockFrame*, aBlockFrame);
    nsIFrame *blockChild = FindImmediateChildOf(aBlockFrame, aPlaceholderFrame);
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
        while (firstFrame != aPlaceholderFrame) {
          NS_ASSERTION(firstFrame, "Must reach our placeholder before end of list!");
          if (!firstFrame)   // This can be removed when we split out-of-flow
            break;           // frames correctly,  see bug 223064
          if (!firstFrame->IsEmpty()) {
            break;
          }
            
          firstFrame = firstFrame->GetNextSibling();
        }
        if (firstFrame == aPlaceholderFrame) {
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
      aHypotheticalBox.mLeft = aBlockContentArea.left;
    }
    aHypotheticalBox.mLeftIsExact = PR_TRUE;

    if (knowBoxWidth) {
      aHypotheticalBox.mRight = aHypotheticalBox.mLeft + boxWidth;
      aHypotheticalBox.mRightIsExact = PR_TRUE;
    } else {
      // We can't compute the right edge because we don't know the desired
      // width. So instead use the right content edge of the block parent,
      // but remember it's not exact
      aHypotheticalBox.mRight = aBlockContentArea.right;
      aHypotheticalBox.mRightIsExact = PR_FALSE;
    }

  } else {
    // The placeholder represents the right edge of the hypothetical box
    if (NS_STYLE_DISPLAY_INLINE == mStyleDisplay->mOriginalDisplay) {
      aHypotheticalBox.mRight = placeholderOffset.x;
    } else {
      aHypotheticalBox.mRight = aBlockContentArea.right;
    }
    aHypotheticalBox.mRightIsExact = PR_TRUE;
    
    if (knowBoxWidth) {
      aHypotheticalBox.mLeft = aHypotheticalBox.mRight - boxWidth;
      aHypotheticalBox.mLeftIsExact = PR_TRUE;
    } else {
      // We can't compute the left edge because we don't know the desired
      // width. So instead use the left content edge of the block parent,
      // but remember it's not exact
      aHypotheticalBox.mLeft = aBlockContentArea.left;
      aHypotheticalBox.mLeftIsExact = PR_FALSE;
    }

  }

  // The current coordinate space is that of the nearest block to the placeholder.
  // Convert to the coordinate space of the absolute containing block
  nsIFrame* absoluteContainingBlock = cbrs->frame;
  if (aBlockFrame != absoluteContainingBlock) {
    nsIFrame* parent;
    nsIFrame* stop;
    if (NS_FRAME_GET_TYPE(cbrs->mFrameType) == NS_CSS_FRAME_TYPE_INLINE) {
      // The absolute containing block is an inline frame... so it will be a
      // descendant of aBlockFrame
      parent = absoluteContainingBlock;
      stop = aBlockFrame;
    } else {
      // aBlockFrame may be a descendant of absoluteContainingBlock
      parent = aBlockFrame;
      stop = absoluteContainingBlock;
    }
    do {
      nsPoint origin = parent->GetPosition();

      aHypotheticalBox.mLeft += origin.x;
      aHypotheticalBox.mRight += origin.x;
      aHypotheticalBox.mTop += origin.y;

      // Move up the tree one level
      parent = parent->GetParent();
    } while (parent && parent != stop);
  }

  // The specified offsets are relative to the absolute containing block's
  // padding edge or content edge, and our current values are relative to the
  // border edge, so translate.
  if (NS_FRAME_GET_TYPE(cbrs->mFrameType) == NS_CSS_FRAME_TYPE_INLINE) {
    // content edge
    const nsMargin& borderPadding = cbrs->mComputedBorderPadding;
    aHypotheticalBox.mLeft -= borderPadding.left;
    aHypotheticalBox.mRight -= borderPadding.right;
    aHypotheticalBox.mTop -= borderPadding.top;
  } else {
    // padding edge
    nsMargin border = cbrs->mComputedBorderPadding - cbrs->mComputedPadding;
    aHypotheticalBox.mLeft -= border.left;
    aHypotheticalBox.mRight -= border.right;
    aHypotheticalBox.mTop -= border.top;
  }
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
  // and return its content area left, top, right, and bottom edges
  nsMargin  blockContentArea;
  nsIFrame* blockFrame = GetNearestContainingBlock(placeholderFrame,
                                                   blockContentArea);
  
  // If both 'left' and 'right' are 'auto' or both 'top' and 'bottom' are
  // 'auto', then compute the hypothetical box of where the element would
  // have been if it had been in the flow
  nsHypotheticalBox hypotheticalBox;
  if (((eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit()) &&
       (eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit())) ||
      ((eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit()) &&
       (eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit()))) {

    CalculateHypotheticalBox(aPresContext, placeholderFrame, blockFrame,
                             blockContentArea, cbrs, hypotheticalBox);
  }

  // Initialize the 'left' and 'right' computed offsets
  // XXX Handle new 'static-position' value...
  PRBool        leftIsAuto = PR_FALSE, rightIsAuto = PR_FALSE;
  nsStyleCoord  coord;
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit()) {
    mComputedOffsets.left = 0;
    leftIsAuto = PR_TRUE;
  } else {
    ComputeHorizontalValue(containingBlockWidth, mStylePosition->mOffset.GetLeftUnit(),
                           mStylePosition->mOffset.GetLeft(coord),
                           mComputedOffsets.left);
  }
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit()) {
    mComputedOffsets.right = 0;
    rightIsAuto = PR_TRUE;
  } else {
    ComputeHorizontalValue(containingBlockWidth, mStylePosition->mOffset.GetRightUnit(),
                           mStylePosition->mOffset.GetRight(coord),
                           mComputedOffsets.right);
  }

  PRUint8 direction = mStyleVisibility->mDirection;

  // Initialize the 'width' computed value
  nsStyleUnit widthUnit = mStylePosition->mWidth.GetUnit();
  PRBool      widthIsAuto = (eStyleUnit_Auto == widthUnit);
  if (!widthIsAuto) {
    // Use the specified value for the computed width
    ComputeHorizontalValue(containingBlockWidth, widthUnit,
                           mStylePosition->mWidth, mComputedWidth);

    AdjustComputedWidth(PR_TRUE);
  }

  // See if none of 'left', 'width', and 'right', is 'auto'
  if (!leftIsAuto && !widthIsAuto && !rightIsAuto) {
    // See whether we're over-constrained
    PRInt32 availBoxSpace = containingBlockWidth - mComputedOffsets.left - mComputedOffsets.right;
    PRInt32 availContentSpace = availBoxSpace - mComputedBorderPadding.left -
                                mComputedBorderPadding.right;

    if (availContentSpace < mComputedWidth) {
      // We're over-constrained so use 'direction' to dictate which value to
      // ignore
      if (NS_STYLE_DIRECTION_LTR == direction) {
        // Ignore the specified value for 'right'
        mComputedOffsets.right = containingBlockWidth - mComputedOffsets.left -
          mComputedBorderPadding.left - mComputedWidth - mComputedBorderPadding.right;
      } else {
        // Ignore the specified value for 'left'
        mComputedOffsets.left = containingBlockWidth - mComputedBorderPadding.left -
          mComputedWidth - mComputedBorderPadding.right - mComputedOffsets.right;
      }

    } else {
      // Calculate any 'auto' margin values
      PRBool  marginLeftIsAuto = (eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit());
      PRBool  marginRightIsAuto = (eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit());
      PRInt32 availMarginSpace = availContentSpace - mComputedWidth;

      if (marginLeftIsAuto) {
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

  } else {
    // See if all three of 'left', 'width', and 'right', are 'auto'
    if (leftIsAuto && widthIsAuto && rightIsAuto) {
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

    // At this point we know that at least one of 'left', 'width', and 'right'
    // is 'auto', but not all three. Examine the various combinations
    if (widthIsAuto) {
      if (leftIsAuto || rightIsAuto) {
        if (NS_FRAME_IS_REPLACED(mFrameType)) {
          // For a replaced element we use the intrinsic size
          mComputedWidth = NS_INTRINSICSIZE;
        } else {
          // The width is shrink-to-fit but constrained by the containing block width
          mComputedWidth = NS_SHRINKWRAPWIDTH;
          
          PRInt32 maxWidth = containingBlockWidth;
          if (NS_UNCONSTRAINEDSIZE != maxWidth) {
            maxWidth -= mComputedOffsets.left + mComputedMargin.left + mComputedBorderPadding.left +
                        mComputedBorderPadding.right + mComputedMargin.right + mComputedOffsets.right;
          }
          if (maxWidth <= 0) {
            maxWidth = 1;
          }
          if (mComputedMaxWidth > maxWidth) {
            mComputedMaxWidth = maxWidth;
          }
        }

        if (leftIsAuto) {
          mComputedOffsets.left = NS_AUTOOFFSET;   // solve for 'left'
        } else {
          mComputedOffsets.right = NS_AUTOOFFSET;  // solve for 'right'
        }

      } else {
        // Only 'width' is 'auto' so just solve for 'width'
        mComputedWidth = containingBlockWidth - mComputedOffsets.left -
          mComputedMargin.left - mComputedBorderPadding.left -
          mComputedBorderPadding.right -
          mComputedMargin.right - mComputedOffsets.right;

        mComputedWidth = PR_MAX(mComputedWidth, 0);

        AdjustComputedWidth(PR_FALSE);

        // XXX If the direction is rtl then we need to reevaluate left...
      }

    } else {
      // Either 'left' or 'right' or both is 'auto'
      if (leftIsAuto && rightIsAuto) {
        // Use the 'direction' to dictate whether 'left' or 'right' is treated like
        // 'static-position'
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

      if (leftIsAuto) {
        // Solve for 'left'
        mComputedOffsets.left = containingBlockWidth - mComputedMargin.left -
          mComputedBorderPadding.left - mComputedWidth - mComputedBorderPadding.right - 
          mComputedMargin.right - mComputedOffsets.right;

      } else if (rightIsAuto) {
        // Solve for 'right'
        mComputedOffsets.right = containingBlockWidth - mComputedOffsets.left -
          mComputedMargin.left - mComputedBorderPadding.left - mComputedWidth -
          mComputedBorderPadding.right - mComputedMargin.right;
      }
    }
  }

  // Initialize the 'top' and 'bottom' computed offsets
  nsStyleUnit heightUnit = mStylePosition->mHeight.GetUnit();
  PRBool      topIsAuto = PR_FALSE, bottomIsAuto = PR_FALSE;
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit()) {
    mComputedOffsets.top = 0;
    topIsAuto = PR_TRUE;
  } else {
    nsStyleCoord c;
    ComputeVerticalValue(containingBlockHeight,
                         mStylePosition->mOffset.GetTopUnit(),
                         mStylePosition->mOffset.GetTop(c),
                         mComputedOffsets.top);
  }
  if (eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit()) {
    mComputedOffsets.bottom = 0;        
    bottomIsAuto = PR_TRUE;
  } else {
    nsStyleCoord c;
    ComputeVerticalValue(containingBlockHeight,
                         mStylePosition->mOffset.GetBottomUnit(),
                         mStylePosition->mOffset.GetBottom(c),
                         mComputedOffsets.bottom);
  }

  // Initialize the 'height' computed value
  PRBool  heightIsAuto = (eStyleUnit_Auto == heightUnit);
  if (!heightIsAuto) {
    // Use the specified value for the computed height
    ComputeVerticalValue(containingBlockHeight, heightUnit,
                         mStylePosition->mHeight, mComputedHeight);

    AdjustComputedHeight(PR_TRUE);
  }

  // See if none of 'top', 'height', and 'bottom', is 'auto'
  if (!topIsAuto && !heightIsAuto && !bottomIsAuto) {
    // See whether we're over-constrained
    PRInt32 availBoxSpace = containingBlockHeight - mComputedOffsets.top - mComputedOffsets.bottom;
    PRInt32 availContentSpace = availBoxSpace - mComputedBorderPadding.top -
                                mComputedBorderPadding.bottom;

    if (availContentSpace < mComputedHeight) {
      // We're over-constrained so ignore the specified value for 'bottom'
      mComputedOffsets.bottom = containingBlockHeight - mComputedOffsets.top -
        mComputedBorderPadding.top - mComputedHeight - mComputedBorderPadding.bottom;

    } else {
      // Calculate any 'auto' margin values
      PRBool  marginTopIsAuto = (eStyleUnit_Auto == mStyleMargin->mMargin.GetTopUnit());
      PRBool  marginBottomIsAuto = (eStyleUnit_Auto == mStyleMargin->mMargin.GetBottomUnit());
      PRInt32 availMarginSpace = availContentSpace - mComputedHeight;

      if (marginTopIsAuto) {
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

  } else {
    // See if all three of 'top', 'height', and 'bottom', are 'auto'
    if (topIsAuto && heightIsAuto && bottomIsAuto) {
      // Treat 'top' like 'static-position'
      mComputedOffsets.top = hypotheticalBox.mTop;
      topIsAuto = PR_FALSE;
    }

    // At this point we know that at least one of 'top', 'height', and 'bottom'
    // is 'auto', but not all three. Examine the various combinations
    if (heightIsAuto) {
      if (topIsAuto || bottomIsAuto) {
        if (NS_FRAME_IS_REPLACED(mFrameType)) {
          // For a replaced element we use the intrinsic size
          mComputedHeight = NS_INTRINSICSIZE;
        } else {
          // The height is based on the content
          mComputedHeight = NS_AUTOHEIGHT;
        }

        if (topIsAuto) {
          mComputedOffsets.top = NS_AUTOOFFSET;     // solve for 'top'
        } else {
          mComputedOffsets.bottom = NS_AUTOOFFSET;  // solve for 'bottom'
        }

      } else {
        // Only 'height' is 'auto' so just solve for 'height'
        mComputedHeight = containingBlockHeight - mComputedOffsets.top -
          mComputedMargin.top - mComputedBorderPadding.top -
          mComputedBorderPadding.bottom -
          mComputedMargin.bottom - mComputedOffsets.bottom;

        mComputedHeight = PR_MAX(mComputedHeight, 0);
        
        AdjustComputedHeight(PR_FALSE);
      }

    } else {
      // Either 'top' or 'bottom' or both is 'auto'
      if (topIsAuto && bottomIsAuto) {
        // Treat 'top' like 'static-position'
        mComputedOffsets.top = hypotheticalBox.mTop;
        topIsAuto = PR_FALSE;
      }

      if (topIsAuto) {
        // Solve for 'top'
        mComputedOffsets.top = containingBlockHeight - mComputedMargin.top -
          mComputedBorderPadding.top - mComputedHeight - mComputedBorderPadding.bottom - 
          mComputedMargin.bottom - mComputedOffsets.bottom;

      } else if (bottomIsAuto) {
        // Solve for 'bottom'
        mComputedOffsets.bottom = containingBlockHeight - mComputedOffsets.top -
          mComputedMargin.top - mComputedBorderPadding.top - mComputedHeight -
          mComputedBorderPadding.bottom - mComputedMargin.bottom;
      }
    }
  }

  // Now that we've solved for our auto offsets and so forth, we need to adjust
  // the offsets to what the rest of layout actually expects them to be.  The
  // offsets as computed now are relative to the parent's padding edge if the
  // parent is a block and the parent's content edge if the parent is an
  // inline.  We need them to be relative to the parent's padding edge for
  // nsAbsoluteContainer reflow to work right.
  if (NS_FRAME_GET_TYPE(cbrs->mFrameType) == NS_CSS_FRAME_TYPE_INLINE) {
    mComputedOffsets += cbrs->mComputedPadding;
  }
}

static PRBool
IsInitialContainingBlock(nsIFrame* aFrame)
{
  nsIContent* content = aFrame->GetContent();

  if (content && !content->GetParent()) {
    // The containing block corresponds to the document element so it's
    // the initial containing block
    return PR_TRUE;
  }
  return PR_FALSE;
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
CalcQuirkContainingBlockHeight(const nsHTMLReflowState& aReflowState)
{
  nsHTMLReflowState* firstAncestorRS = nsnull; // a candidate for html frame
  nsHTMLReflowState* secondAncestorRS = nsnull; // a candidate for body frame
  
  // initialize the default to NS_AUTOHEIGHT as this is the containings block
  // computed height when this function is called. It is possible that we 
  // don't alter this height especially if we are restricted to one level
  nscoord result = NS_AUTOHEIGHT; 
                             
  const nsHTMLReflowState* rs = &aReflowState;
  for (; rs && rs->frame; rs = (nsHTMLReflowState *)(rs->parentReflowState)) { 
    nsIAtom* frameType = rs->frame->GetType();
    // if the ancestor is auto height then skip it and continue up if it 
    // is the first block/area frame and possibly the body/html
    if (nsLayoutAtoms::blockFrame == frameType ||
        nsLayoutAtoms::areaFrame == frameType ||
        nsLayoutAtoms::scrollFrame == frameType) {
      
      if (nsLayoutAtoms::areaFrame == frameType) {
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
    else if (nsLayoutAtoms::canvasFrame == frameType) {
      // Use scroll frames' computed height if we have one, this will
      // allow us to get viewport height for native scrollbars.
      nsHTMLReflowState* scrollState = (nsHTMLReflowState *)rs->parentReflowState;
      if (nsLayoutAtoms::scrollFrame == scrollState->frame->GetType()) {
        rs = scrollState;
      }
    }
    else if (nsLayoutAtoms::pageContentFrame == frameType) {
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
    result = (nsLayoutAtoms::pageContentFrame == frameType)
             ? rs->availableHeight : rs->mComputedHeight;
    // if unconstrained - don't sutract borders - would result in huge height
    if (NS_AUTOHEIGHT == result) return result;

    // if we got to the canvas or page content frame, then subtract out 
    // margin/border/padding for the BODY and HTML elements
    if ((nsLayoutAtoms::canvasFrame == frameType) || 
        (nsLayoutAtoms::pageContentFrame == frameType)) {

      result -= GetVerticalMarginBorderPadding(firstAncestorRS); 
      result -= GetVerticalMarginBorderPadding(secondAncestorRS); 

#ifdef DEBUG
      // make sure the first ancestor is the HTML and the second is the BODY
      if (firstAncestorRS) {
        nsIContent* frameContent = firstAncestorRS->frame->GetContent();
        if (frameContent) {
          nsIAtom *contentTag = frameContent->Tag();
          NS_ASSERTION(contentTag == nsHTMLAtoms::html, "First ancestor is not HTML");
        }
      }
      if (secondAncestorRS) {
        nsIContent* frameContent = secondAncestorRS->frame->GetContent();
        if (frameContent) {
          nsIAtom *contentTag = frameContent->Tag();
          NS_ASSERTION(contentTag == nsHTMLAtoms::body, "Second ancestor is not BODY");
        }
      }
#endif
      
    }
    // if we got to the html frame, then subtract out 
    // margin/border/padding for the BODY element
    else if (nsLayoutAtoms::areaFrame == frameType) {
      // make sure it is the body
      if (nsLayoutAtoms::canvasFrame == rs->parentReflowState->frame->GetType()) {
        result -= GetVerticalMarginBorderPadding(secondAncestorRS);
      }
    }
    break;
  }

  return result;
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
      aContainingBlockWidth = aContainingBlockRS->frame->GetRect().width -
        (aContainingBlockRS->mComputedBorderPadding.left +
         aContainingBlockRS->mComputedBorderPadding.right);
      NS_ASSERTION(aContainingBlockWidth >= 0,
                   "Negative containing block width!");
      aContainingBlockHeight = aContainingBlockRS->frame->GetRect().height -
        (aContainingBlockRS->mComputedBorderPadding.top +
         aContainingBlockRS->mComputedBorderPadding.bottom);
      NS_ASSERTION(aContainingBlockHeight >= 0,
                   "Negative containing block height!");
    } else {
      // If the ancestor is block-level, the containing block is formed by the
      // padding edge of the ancestor
      aContainingBlockWidth += aContainingBlockRS->mComputedPadding.left +
                               aContainingBlockRS->mComputedPadding.right;

      // If the containing block is the initial containing block and it has a
      // height that depends on its content, then use the viewport height instead.
      // This gives us a reasonable value against which to compute percentage
      // based heights and to do bottom relative positioning
      if ((NS_AUTOHEIGHT == aContainingBlockHeight) &&
          IsInitialContainingBlock(aContainingBlockRS->frame)) {

        // Use the viewport height as the containing block height
        const nsHTMLReflowState* rs = aContainingBlockRS->parentReflowState;
        while (rs) {
          aContainingBlockHeight = rs->mComputedHeight;
          rs = rs->parentReflowState;
        }

      } else {
        aContainingBlockHeight += aContainingBlockRS->mComputedPadding.top +
                                  aContainingBlockRS->mComputedPadding.bottom;
      }
    }
  } else {
    // If this is an unconstrained reflow, then reset the containing block
    // width to NS_UNCONSTRAINEDSIZE. This way percentage based values have
    // no effect
    if (NS_UNCONSTRAINEDSIZE == availableWidth) {
      aContainingBlockWidth = NS_UNCONSTRAINEDSIZE;
    }
    // an element in quirks mode gets a containing block based on looking for a
    // parent with a non-auto height if the element has a percent height
    if (NS_AUTOHEIGHT == aContainingBlockHeight) {
      if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode() &&
          mStylePosition->mHeight.GetUnit() == eStyleUnit_Percent) {
        aContainingBlockHeight = CalcQuirkContainingBlockHeight(*aContainingBlockRS);
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
    // changable, so no need to register callback for it.
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
                                   nsMargin*       aBorder,
                                   nsMargin*       aPadding)
{
  // If this is the root frame, then set the computed width and
  // height equal to the available space
  if (nsnull == parentReflowState) {
    mComputedWidth = availableWidth;
    mComputedHeight = availableHeight;
    mComputedMargin.SizeTo(0, 0, 0, 0);
    mComputedPadding.SizeTo(0, 0, 0, 0);
    mComputedBorderPadding.SizeTo(0, 0, 0, 0);
    mComputedOffsets.SizeTo(0, 0, 0, 0);
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
        if (nsLayoutAtoms::scrollFrame == fType) {
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

    // Compute margins from the specified margin style information. These
    // become the default computed values, and may be adjusted below
    // XXX fix to provide 0,0 for the top&bottom margins for
    // inline-non-replaced elements
    ComputeMargin(aContainingBlockWidth, cbrs);
    if (aPadding) { // padding is an input arg
      mComputedPadding.top    = aPadding->top;
      mComputedPadding.right  = aPadding->right;
      mComputedPadding.bottom = aPadding->bottom;
      mComputedPadding.left   = aPadding->left;
    }
    else {
      ComputePadding(aContainingBlockWidth, cbrs);
    }
    if (aBorder) {  // border is an input arg
      mComputedBorderPadding.top    = aBorder->top;
      mComputedBorderPadding.right  = aBorder->right;
      mComputedBorderPadding.bottom = aBorder->bottom;
      mComputedBorderPadding.left   = aBorder->left;
    }
    else {
      if (!mStyleBorder->GetBorder(mComputedBorderPadding)) {
        // CSS2 has no percentage borders
        mComputedBorderPadding.SizeTo(0, 0, 0, 0);
      }
    }
    mComputedBorderPadding += mComputedPadding;

    nsStyleUnit widthUnit = mStylePosition->mWidth.GetUnit();
    nsStyleUnit heightUnit = mStylePosition->mHeight.GetUnit();

    // Check for a percentage based width and an unconstrained containing
    // block width
    if (eStyleUnit_Percent == widthUnit) {
      if (NS_UNCONSTRAINEDSIZE == aContainingBlockWidth) {
        widthUnit = eStyleUnit_Auto;
      }
    }
    // Check for a percentage based height and a containing block height
    // that depends on the content height
    if (eStyleUnit_Percent == heightUnit) {
      if (NS_AUTOHEIGHT == aContainingBlockHeight) {
        // this if clause enables %-height on replaced inline frames,
        // such as images.  See bug 54119.  The else clause "heightUnit = eStyleUnit_Auto;"
        // used to be called exclusively.
        if (NS_FRAME_REPLACED(NS_CSS_FRAME_TYPE_INLINE) == mFrameType) {
          // Get the containing block reflow state
          NS_ASSERTION(nsnull != cbrs, "no containing block");
          // in quirks mode, get the cb height using the special quirk method
          if (eCompatibility_NavQuirks == aPresContext->CompatibilityMode()) {
            if (!IS_TABLE_CELL(fType)) {
              aContainingBlockHeight = CalcQuirkContainingBlockHeight(*cbrs);
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

    // Calculate the computed values for min and max properties
    ComputeMinMaxValues(aContainingBlockWidth, aContainingBlockHeight, cbrs);

    // Calculate the computed width and height. This varies by frame type
    if ((NS_FRAME_REPLACED(NS_CSS_FRAME_TYPE_INLINE) == mFrameType) ||
        (NS_FRAME_REPLACED(NS_CSS_FRAME_TYPE_FLOATING) == mFrameType)) {
      // Inline replaced element and floating replaced element are basically
      // treated the same. First calculate the computed width
      if (eStyleUnit_Auto == widthUnit) {
        // A specified value of 'auto' uses the element's intrinsic width
        mComputedWidth = NS_INTRINSICSIZE;
      } else {
        ComputeHorizontalValue(aContainingBlockWidth, widthUnit,
                               mStylePosition->mWidth, mComputedWidth);
      }

      AdjustComputedWidth(PR_TRUE);

      // Now calculate the computed height
      if (eStyleUnit_Auto == heightUnit) {
        // A specified value of 'auto' uses the element's intrinsic height
        mComputedHeight = NS_INTRINSICSIZE;
      } else {
        ComputeVerticalValue(aContainingBlockHeight, heightUnit,
                             mStylePosition->mHeight,
                             mComputedHeight);
      }

      AdjustComputedHeight(PR_TRUE);

    } else if (NS_CSS_FRAME_TYPE_FLOATING == mFrameType) {
      // Floating non-replaced element. First calculate the computed width
      if (eStyleUnit_Auto == widthUnit) {
        if ((NS_UNCONSTRAINEDSIZE == aContainingBlockWidth) &&
            (eStyleUnit_Percent == mStylePosition->mWidth.GetUnit())) {
          // The element has a percentage width, but since the containing
          // block width is unconstrained we set 'widthUnit' to 'auto'
          // above. However, we want the element to be unconstrained, too
          mComputedWidth = NS_UNCONSTRAINEDSIZE;

        } else if (NS_STYLE_DISPLAY_TABLE == mStyleDisplay->mDisplay) {
          // It's an outer table because an inner table is not positioned
          // shrink wrap its width since the outer table is anonymous
          mComputedWidth = NS_SHRINKWRAPWIDTH;

        } else {
          // The CSS2 spec says the computed width should be 0; however, that's
          // not what Nav and IE do and even the spec doesn't really want that
          // to happen.
          //
          // Instead, have the element shrink wrap its width
          mComputedWidth = NS_SHRINKWRAPWIDTH;

          // Limnit the width to the containing block width
          nscoord widthFromCB = aContainingBlockWidth;
          if (NS_UNCONSTRAINEDSIZE != widthFromCB) {
            widthFromCB -= mComputedBorderPadding.left + mComputedBorderPadding.right +
                           mComputedMargin.left + mComputedMargin.right;
          }
          if (mComputedMaxWidth > widthFromCB) {
            mComputedMaxWidth = widthFromCB;
          }
        }

      } else {
        ComputeHorizontalValue(aContainingBlockWidth, widthUnit,
                               mStylePosition->mWidth, mComputedWidth);
      }

      // Take into account minimum and maximum sizes
      AdjustComputedWidth(PR_TRUE);

      // Now calculate the computed height
      if (eStyleUnit_Auto == heightUnit) {
        mComputedHeight = NS_AUTOHEIGHT;  // let it choose its height
      } else {
        ComputeVerticalValue(aContainingBlockHeight, heightUnit,
                             mStylePosition->mHeight,
                             mComputedHeight);
      }

      AdjustComputedHeight(PR_TRUE);
    
    } else if (NS_CSS_FRAME_TYPE_INTERNAL_TABLE == mFrameType) {
      // Internal table elements. The rules vary depending on the type.
      // Calculate the computed width
      PRBool rowOrRowGroup = PR_FALSE;
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
        ComputeHorizontalValue(aContainingBlockWidth, widthUnit,
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
        ComputeVerticalValue(aContainingBlockHeight, heightUnit,
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
    } else if (NS_CSS_FRAME_TYPE_INLINE == mFrameType) {
      // Inline non-replaced elements do not have computed widths or heights
      // XXX add this check to HaveFixedContentHeight/Width too
      mComputedWidth = NS_UNCONSTRAINEDSIZE;
      mComputedHeight = NS_UNCONSTRAINEDSIZE;
      mComputedMargin.top = 0;
      mComputedMargin.bottom = 0;
      mComputedMinWidth = mComputedMinHeight = 0;
      mComputedMaxWidth = mComputedMaxHeight = NS_UNCONSTRAINEDSIZE;
    } else {
      ComputeBlockBoxData(aPresContext, cbrs, widthUnit, heightUnit,
                          aContainingBlockWidth,
                          aContainingBlockHeight);
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

// Compute the box data for block and block-replaced elements in the
// normal flow.
void
nsHTMLReflowState::ComputeBlockBoxData(nsPresContext* aPresContext,
                                       const nsHTMLReflowState* cbrs,
                                       nsStyleUnit aWidthUnit,
                                       nsStyleUnit aHeightUnit,
                                       nscoord aContainingBlockWidth,
                                       nscoord aContainingBlockHeight)
{
  // Compute the content width
  if (eStyleUnit_Auto == aWidthUnit) {
    if (NS_FRAME_IS_REPLACED(mFrameType)) {
      // Block-level replaced element in the flow. A specified value of 
      // 'auto' uses the element's intrinsic width (CSS2 10.3.4)
      mComputedWidth = NS_INTRINSICSIZE;
    } else {
      // Block-level non-replaced element in the flow. 'auto' values
      // for margin-left and margin-right become 0, and the sum of the
      // areas must equal the width of the content-area of the parent
      // element.
      if (NS_UNCONSTRAINEDSIZE == availableWidth) {
        // During pass1 table reflow, auto side margin values are
        // uncomputable (== 0).
        mComputedWidth = NS_UNCONSTRAINEDSIZE;
      } else if (NS_SHRINKWRAPWIDTH == aContainingBlockWidth) {
        // The containing block should shrink wrap its width, so have
        // the child block do the same
        mComputedWidth = NS_UNCONSTRAINEDSIZE;

        // Let its content area be as wide as the containing block's max width
        // minus any margin and border/padding
        nscoord maxWidth = cbrs->mComputedMaxWidth;
        if (NS_UNCONSTRAINEDSIZE != maxWidth) {
          maxWidth -= mComputedMargin.left + mComputedBorderPadding.left + 
                      mComputedMargin.right + mComputedBorderPadding.right;
        }
        if (maxWidth < mComputedMaxWidth) {
          mComputedMaxWidth = maxWidth;
        }

      } else {
        // tables act like replaced elements regarding mComputedWidth 
        nsIAtom* fType = frame->GetType();
        if (nsLayoutAtoms::tableOuterFrame == fType) {
          mComputedWidth = 0; // XXX temp fix for trees
        } else if ((nsLayoutAtoms::tableFrame == fType) ||
                   (nsLayoutAtoms::tableCaptionFrame == fType)) {
          mComputedWidth = NS_SHRINKWRAPWIDTH;
          if (eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit()) {
            mComputedMargin.left = NS_AUTOMARGIN;
          }
          if (eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit()) {
            mComputedMargin.right = NS_AUTOMARGIN;
          }
        } else {
          mComputedWidth = availableWidth - mComputedMargin.left -
            mComputedMargin.right - mComputedBorderPadding.left -
            mComputedBorderPadding.right;
          mComputedWidth = PR_MAX(mComputedWidth, 0);
        }

        AdjustComputedWidth(PR_FALSE);
        CalculateBlockSideMargins(cbrs->mComputedWidth, mComputedWidth);
      }
    }
  } else {
    ComputeHorizontalValue(aContainingBlockWidth, aWidthUnit,
                           mStylePosition->mWidth, mComputedWidth);

    AdjustComputedWidth(PR_TRUE); 

    // Now that we have the computed-width, compute the side margins
    CalculateBlockSideMargins(cbrs->mComputedWidth, mComputedWidth);
  }

  // Compute the content height
  if (eStyleUnit_Auto == aHeightUnit) {
    if (NS_FRAME_IS_REPLACED(mFrameType)) {
      // For replaced elements use the intrinsic size for "auto"
      mComputedHeight = NS_INTRINSICSIZE;
    } else {
      // For non-replaced elements auto means unconstrained
      mComputedHeight = NS_UNCONSTRAINEDSIZE;
    }
  } else {
    ComputeVerticalValue(aContainingBlockHeight, aHeightUnit,
                         mStylePosition->mHeight, mComputedHeight);
  }
  AdjustComputedHeight(PR_TRUE);
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
  // Because of the ugly way we do intrinsic sizing within Reflow, this method
  // doesn't necessarily produce the right results.  The results will be
  // adjusted in nsBlockReflowContext::AlignBlockHorizontally after reflow.
  // The code for tables is particularly sensitive to regressions; the
  // numerous |isTable| checks are technically incorrect, but necessary
  // for basic testcases.

  // We can only provide values for auto side margins in a constrained
  // reflow. For unconstrained reflow there is no effective width to
  // compute against...
  if (NS_UNCONSTRAINEDSIZE == aComputedWidth ||
      NS_UNCONSTRAINEDSIZE == aAvailWidth)
    return;

  nscoord sum = mComputedMargin.left + mComputedBorderPadding.left +
    aComputedWidth + mComputedBorderPadding.right + mComputedMargin.right;
  if (sum == aAvailWidth)
    // The sum is already correct
    return;

  // Determine the left and right margin values. The width value
  // remains constant while we do this.

  PRBool isTable = mStyleDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE ||
                   mStyleDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_CAPTION;

  // Calculate how much space is available for margins
  nscoord availMarginSpace = aAvailWidth - sum;

  // XXXldb Should this be quirks-mode only?  And why captions?
  if (isTable)
    // XXXldb Why does this break things so badly if this is changed to
    // availMarginSpace += mComputedBorderPadding.left +
    //                     mComputedBorderPadding.right;
    availMarginSpace = aAvailWidth - aComputedWidth;

  // If the available margin space is negative, then don't follow the
  // usual overconstraint rules.
  if (availMarginSpace < 0) {
    if (!isTable) {
      if (mStyleVisibility->mDirection == NS_STYLE_DIRECTION_LTR) {
        mComputedMargin.right += availMarginSpace;
      } else {
        mComputedMargin.left += availMarginSpace;
      }
    } else {
      mComputedMargin.left = 0;
      mComputedMargin.right = 0;
      if (mStyleVisibility->mDirection == NS_STYLE_DIRECTION_RTL) {
        mComputedMargin.left = availMarginSpace;
      }
    }
    return;
  }

  // The css2 spec clearly defines how block elements should behave
  // in section 10.3.3.
  PRBool isAutoLeftMargin =
    eStyleUnit_Auto == mStyleMargin->mMargin.GetLeftUnit();
  PRBool isAutoRightMargin =
    eStyleUnit_Auto == mStyleMargin->mMargin.GetRightUnit();
  if (!isAutoLeftMargin && !isAutoRightMargin && !isTable) {
    // Neither margin is 'auto' so we're over constrained. Use the
    // 'direction' property of the parent to tell which margin to
    // ignore
    // First check if there is an HTML alignment that we should honor
    const nsHTMLReflowState* prs = parentReflowState;
    if (prs &&
        (prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER ||
         prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT)) {
      isAutoLeftMargin = PR_TRUE;
      isAutoRightMargin =
        prs->mStyleText->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER;
    }
    // Otherwise apply the CSS rules, and ignore one margin by forcing
    // it to 'auto', depending on 'direction'.
    else if (NS_STYLE_DIRECTION_LTR == mStyleVisibility->mDirection) {
      isAutoRightMargin = PR_TRUE;
    }
    else {
      isAutoLeftMargin = PR_TRUE;
    }
  }

  // Logic which is common to blocks and tables
  if (isAutoLeftMargin) {
    if (isAutoRightMargin) {
      // Both margins are 'auto' so their computed values are equal
      mComputedMargin.left = availMarginSpace / 2;
      mComputedMargin.right = availMarginSpace - mComputedMargin.left;
    } else {
      mComputedMargin.left = availMarginSpace;
    }
  } else if (isAutoRightMargin) {
    mComputedMargin.right = availMarginSpace;
  }
}

PRBool
nsHTMLReflowState::UseComputedHeight()
{
  static PRBool useComputedHeight = PR_FALSE;

#if defined(XP_UNIX) || defined(XP_WIN) || defined(XP_OS2) || defined(XP_BEOS)
  static PRBool firstTime = 1;
  if (firstTime) {
    if (getenv("GECKO_USE_COMPUTED_HEIGHT")) {
      useComputedHeight = PR_TRUE;
    }
    firstTime = 0;
  }
#endif
  return useComputedHeight;
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

  nscoord lineHeight = -1;

  const nsStyleText* text = aStyleContext->GetStyleText();
  const nsStyleFont* font = aStyleContext->GetStyleFont();
  const nsStyleVisibility* vis = aStyleContext->GetStyleVisibility();
  
  nsStyleUnit unit = text->mLineHeight.GetUnit();

  if (unit == eStyleUnit_Coord) {
    // For length values just use the pre-computed value
    lineHeight = text->mLineHeight.GetCoordValue();
  } else {
    nsCOMPtr<nsIDeviceContext> deviceContext;
    aRenderingContext->GetDeviceContext(*getter_AddRefs(deviceContext));
    nsCOMPtr<nsIFontMetrics> fm;
    deviceContext->GetMetricsFor(font->mFont, vis->mLangGroup,
                                 *getter_AddRefs(fm));
    if (unit == eStyleUnit_Factor) {
      // For factor units the computed value of the line-height property 
      // is found by multiplying the factor by the font's <b>actual</b> 
      // em height. 
      float factor;
      factor = text->mLineHeight.GetFactorValue();
      // Note: we normally use the actual font height for computing the
      // line-height raw value from the style context. On systems where
      // they disagree the actual font height is more appropriate. This
      // little hack lets us override that behavior to allow for more
      // precise layout in the face of imprecise fonts.
      nscoord emHeight = font->mFont.size;
      if (!nsHTMLReflowState::UseComputedHeight()) {
        fm->GetEmHeight(emHeight);
      }
      lineHeight = NSToCoordRound(factor * emHeight);
    } else {
      NS_ASSERTION(eStyleUnit_Normal == unit, "bad unit");
      lineHeight = font->mFont.size;
      if (!nsHTMLReflowState::UseComputedHeight()) {
        lineHeight = GetNormalLineHeight(fm);
      }
    }
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

void
nsHTMLReflowState::ComputeHorizontalValue(nscoord aContainingBlockWidth,
                                          nsStyleUnit aUnit,
                                          const nsStyleCoord& aCoord,
                                          nscoord& aResult)
{
  aResult = 0;
  if (eStyleUnit_Percent == aUnit) {
    if (NS_UNCONSTRAINEDSIZE == aContainingBlockWidth) {
      aResult = 0;
    } else {
      float pct = aCoord.GetPercentValue();
      aResult = NSToCoordFloor(aContainingBlockWidth * pct);
    }
  
  } else if (eStyleUnit_Coord == aUnit) {
    aResult = aCoord.GetCoordValue();
  }
  else if (eStyleUnit_Chars == aUnit) {
    if ((nsnull == rendContext) || (nsnull == frame)) {
      // We can't compute it without a rendering context or frame, so
      // pretend its zero...
    }
    else {
      nsStyleContext* styleContext = frame->GetStyleContext();
      SetFontFromStyle(rendContext, styleContext);
      nscoord fontWidth;
      rendContext->GetWidth('M', fontWidth);
      aResult = aCoord.GetIntValue() * fontWidth;
    }
  }
}

void
nsHTMLReflowState::ComputeVerticalValue(nscoord aContainingBlockHeight,
                                        nsStyleUnit aUnit,
                                        const nsStyleCoord& aCoord,
                                        nscoord& aResult)
{
  aResult = 0;
  if (eStyleUnit_Percent == aUnit) {
    // Verify no one is trying to calculate a percentage based height against
    // a height that's shrink wrapping to its content. In that case they should
    // treat the specified value like 'auto'
    NS_ASSERTION(NS_AUTOHEIGHT != aContainingBlockHeight, "unexpected containing block height");
    if (NS_AUTOHEIGHT!=aContainingBlockHeight)
    {
      float pct = aCoord.GetPercentValue();
      aResult = NSToCoordFloor(aContainingBlockHeight * pct);
    }
    else {  // safest thing to do for an undefined height is to make it 0
      aResult = 0;
    }

  } else if (eStyleUnit_Coord == aUnit) {
    aResult = aCoord.GetCoordValue();
  }
}

void
nsHTMLReflowState::ComputeMargin(nscoord aContainingBlockWidth,
                                 const nsHTMLReflowState* aContainingBlockRS)
{
  // If style style can provide us the margin directly, then use it.
  if (!mStyleMargin->GetMargin(mComputedMargin)) {
    // We have to compute the value
    if (NS_UNCONSTRAINEDSIZE == aContainingBlockWidth) {
      mComputedMargin.left = 0;
      mComputedMargin.right = 0;

      if (eStyleUnit_Coord == mStyleMargin->mMargin.GetLeftUnit()) {
        nsStyleCoord left;
        
        mStyleMargin->mMargin.GetLeft(left),
        mComputedMargin.left = left.GetCoordValue();
      }
      if (eStyleUnit_Coord == mStyleMargin->mMargin.GetRightUnit()) {
        nsStyleCoord right;
        
        mStyleMargin->mMargin.GetRight(right),
        mComputedMargin.right = right.GetCoordValue();
      }

    } else {
      nsStyleCoord left, right;

      ComputeHorizontalValue(aContainingBlockWidth,
                             mStyleMargin->mMargin.GetLeftUnit(),
                             mStyleMargin->mMargin.GetLeft(left),
                             mComputedMargin.left);
      ComputeHorizontalValue(aContainingBlockWidth,
                             mStyleMargin->mMargin.GetRightUnit(),
                             mStyleMargin->mMargin.GetRight(right),
                             mComputedMargin.right);
    }

    const nsHTMLReflowState* rs2 = GetPageBoxReflowState(parentReflowState);
    nsStyleCoord top, bottom;
    if (nsnull != rs2) {
      // According to the CSS2 spec, margin percentages are
      // calculated with respect to the *height* of the containing
      // block when in a paginated context.
      ComputeVerticalValue(rs2->mComputedHeight,
                           mStyleMargin->mMargin.GetTopUnit(),
                           mStyleMargin->mMargin.GetTop(top),
                           mComputedMargin.top);
      ComputeVerticalValue(rs2->mComputedHeight,
                           mStyleMargin->mMargin.GetBottomUnit(),
                           mStyleMargin->mMargin.GetBottom(bottom),
                           mComputedMargin.bottom);
    }
    else {
      // According to the CSS2 spec, margin percentages are
      // calculated with respect to the *width* of the containing
      // block, even for margin-top and margin-bottom.
      ComputeHorizontalValue(aContainingBlockWidth,
                             mStyleMargin->mMargin.GetTopUnit(),
                             mStyleMargin->mMargin.GetTop(top),
                             mComputedMargin.top);
      ComputeHorizontalValue(aContainingBlockWidth,
                             mStyleMargin->mMargin.GetBottomUnit(),
                             mStyleMargin->mMargin.GetBottom(bottom),
                             mComputedMargin.bottom);
    }
  }
}

void
nsHTMLReflowState::ComputePadding(nscoord aContainingBlockWidth,
                                  const nsHTMLReflowState* aContainingBlockRS)

{
  // If style can provide us the padding directly, then use it.
  if (!mStylePadding->GetPadding(mComputedPadding)) {
    // We have to compute the value
    nsStyleCoord left, right, top, bottom;

    ComputeHorizontalValue(aContainingBlockWidth,
                           mStylePadding->mPadding.GetLeftUnit(),
                           mStylePadding->mPadding.GetLeft(left),
                           mComputedPadding.left);
    ComputeHorizontalValue(aContainingBlockWidth,
                           mStylePadding->mPadding.GetRightUnit(),
                           mStylePadding->mPadding.GetRight(right),
                           mComputedPadding.right);

    // According to the CSS2 spec, percentages are calculated with respect to
    // containing block width for padding-top and padding-bottom
    ComputeHorizontalValue(aContainingBlockWidth,
                           mStylePadding->mPadding.GetTopUnit(),
                           mStylePadding->mPadding.GetTop(top),
                           mComputedPadding.top);
    ComputeHorizontalValue(aContainingBlockWidth,
                           mStylePadding->mPadding.GetBottomUnit(),
                           mStylePadding->mPadding.GetBottom(bottom),
                           mComputedPadding.bottom);
  }
  // a table row/col group, row/col doesn't have padding
  if (frame) {
    nsIAtom* frameType = frame->GetType();
    if ((nsLayoutAtoms::tableRowGroupFrame == frameType) ||
        (nsLayoutAtoms::tableColGroupFrame == frameType) ||
        (nsLayoutAtoms::tableRowFrame      == frameType) ||
        (nsLayoutAtoms::tableColFrame      == frameType)) {
      mComputedPadding.top    = 0;
      mComputedPadding.right  = 0;
      mComputedPadding.bottom = 0;
      mComputedPadding.left   = 0;
    }
  }
}

void
nsHTMLReflowState::ComputeMinMaxValues(nscoord aContainingBlockWidth,
                                       nscoord aContainingBlockHeight,
                                       const nsHTMLReflowState* aContainingBlockRS)
{
  nsStyleUnit minWidthUnit = mStylePosition->mMinWidth.GetUnit();
  ComputeHorizontalValue(aContainingBlockWidth, minWidthUnit,
                         mStylePosition->mMinWidth, mComputedMinWidth);
  nsStyleUnit maxWidthUnit = mStylePosition->mMaxWidth.GetUnit();
  if (eStyleUnit_Null == maxWidthUnit) {
    // Specified value of 'none'
    mComputedMaxWidth = NS_UNCONSTRAINEDSIZE;  // no limit
  } else {
    ComputeHorizontalValue(aContainingBlockWidth, maxWidthUnit,
                           mStylePosition->mMaxWidth, mComputedMaxWidth);
  }

  // If the computed value of 'min-width' is greater than the value of
  // 'max-width', 'max-width' is set to the value of 'min-width'
  if (mComputedMinWidth > mComputedMaxWidth) {
    mComputedMaxWidth = mComputedMinWidth;
  }

  nsStyleUnit minHeightUnit = mStylePosition->mMinHeight.GetUnit();
  // Check for percentage based values and a containing block height that
  // depends on the content height. Treat them like 'auto'
  if ((NS_AUTOHEIGHT == aContainingBlockHeight) &&
      (eStyleUnit_Percent == minHeightUnit)) {
    mComputedMinHeight = 0;
  } else {
    ComputeVerticalValue(aContainingBlockHeight, minHeightUnit,
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
      ComputeVerticalValue(aContainingBlockHeight, maxHeightUnit,
                           mStylePosition->mMaxHeight, mComputedMaxHeight);
    }
  }

  // If the computed value of 'min-height' is greater than the value of
  // 'max-height', 'max-height' is set to the value of 'min-height'
  if (mComputedMinHeight > mComputedMaxHeight) {
    mComputedMaxHeight = mComputedMinHeight;
  }
}


void nsHTMLReflowState::AdjustComputedHeight(PRBool aAdjustForBoxSizing)
{
  // only do the math if the height  is not a symbolic value
  if (mComputedHeight == NS_UNCONSTRAINEDSIZE) {
    return;
  }
  
  NS_ASSERTION(mComputedHeight >= 0, "Negative Height Input - very bad");

  // Factor in any minimum and maximum size information
  if (mComputedHeight > mComputedMaxHeight) {
    mComputedHeight = mComputedMaxHeight;
  } else if (mComputedHeight < mComputedMinHeight) {
    mComputedHeight = mComputedMinHeight;
  }

  if (aAdjustForBoxSizing) {
    // remove extra padding/border if box-sizing property is set
    switch (mStylePosition->mBoxSizing) {
    case NS_STYLE_BOX_SIZING_PADDING : {
      mComputedHeight -= mComputedPadding.top + mComputedPadding.bottom;
      break;
    }
    case NS_STYLE_BOX_SIZING_BORDER : {
      mComputedHeight -= mComputedBorderPadding.top + mComputedBorderPadding.bottom;
    }
    default : break;
    }
    
    // If it did go bozo because of too much border or padding, set to 0
    if(mComputedHeight < 0) mComputedHeight = 0;
  }  
}

void nsHTMLReflowState::AdjustComputedWidth(PRBool aAdjustForBoxSizing)
{
  // only do the math if the width is not a symbolic value
  if (mComputedWidth == NS_UNCONSTRAINEDSIZE) {
    return;
  }
  
  NS_ASSERTION(mComputedWidth >= 0, "Negative Width Input - very bad");

  // Factor in any minimum and maximum size information
  if (mComputedWidth > mComputedMaxWidth) {
    mComputedWidth = mComputedMaxWidth;
  } else if (mComputedWidth < mComputedMinWidth) {
    mComputedWidth = mComputedMinWidth;
  }
  
  if (aAdjustForBoxSizing) {
    // remove extra padding/border if box-sizing property is set
    switch (mStylePosition->mBoxSizing) {
    case NS_STYLE_BOX_SIZING_PADDING : {
      mComputedWidth -= mComputedPadding.left + mComputedPadding.right;
      break;
    }
    case NS_STYLE_BOX_SIZING_BORDER : {
      mComputedWidth -= mComputedBorderPadding.left + mComputedBorderPadding.right;
    }
    default : break;
    }

    // If it did go bozo because of too much border or padding, set to 0
    if(mComputedWidth < 0) mComputedWidth = 0;
  }
}
#ifdef IBMBIDI
PRBool
nsHTMLReflowState::IsBidiFormControl(nsPresContext* aPresContext)
{
  // This check is only necessary on visual bidi pages, because most
  // visual pages use logical order for form controls so that they will
  // display correctly on native widgets in OSs with Bidi support.
  // So bail out if the page is not Bidi, or not visual, or if the pref is
  // set to use visual order on forms in visual pages
  if (!aPresContext->BidiEnabled()) {
    return PR_FALSE;
  }

  if (!aPresContext->IsVisualMode()) {
    return PR_FALSE;
  }

  PRUint32 options = aPresContext->GetBidi();
  if (IBMBIDI_CONTROLSTEXTMODE_LOGICAL != GET_BIDI_OPTION_CONTROLSTEXTMODE(options)) {
    return PR_FALSE;
  }

  nsIContent* content = frame->GetContent();
  if (!content) {
    return PR_FALSE;
  }

  // If this is a root reflow, we have to walk up the content tree to
  // find out if the reflow root is a descendant of a form control.
  // Otherwise, just test this content node
  if (mReflowDepth == 0) {
    for ( ; content; content = content->GetParent()) {
      if (content->IsContentOfType(nsIContent::eHTML_FORM_CONTROL)) {
        return PR_TRUE;
      }
    }
  } else {
    return (content->IsContentOfType(nsIContent::eHTML_FORM_CONTROL));
  }
  
  return PR_FALSE;
}
#endif
