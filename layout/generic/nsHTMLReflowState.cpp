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
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsFrame.h"
#include "nsIHTMLReflow.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsLayoutAtoms.h"
#include "nsIRenderingContext.h"

#ifdef NS_DEBUG
#undef NOISY_VERTICAL_ALIGN
#else
#undef NOISY_VERTICAL_ALIGN
#endif

// Initialize a <b>root</b> reflow state with a rendering context to
// use for measuring things.
nsHTMLReflowState::nsHTMLReflowState(nsIPresContext&      aPresContext,
                                     nsIFrame*            aFrame,
                                     nsReflowReason       aReason,
                                     nsIRenderingContext* aRenderingContext,
                                     const nsSize&        aAvailableSpace)
{
  NS_PRECONDITION(nsnull != aRenderingContext, "no rendering context");

  parentReflowState = nsnull;
  frame = aFrame;
  reason = aReason;
  reflowCommand = nsnull;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;
  rendContext = aRenderingContext;
  mSpaceManager = nsnull;
  mLineLayout = nsnull;
  isTopOfPage = PR_FALSE;
  Init(aPresContext);
}

// Initialize a <b>root</b> reflow state for an <b>incremental</b>
// reflow.
nsHTMLReflowState::nsHTMLReflowState(nsIPresContext&      aPresContext,
                                     nsIFrame*            aFrame,
                                     nsIReflowCommand&    aReflowCommand,
                                     nsIRenderingContext* aRenderingContext,
                                     const nsSize&        aAvailableSpace)
{
  NS_PRECONDITION(nsnull != aRenderingContext, "no rendering context");

  parentReflowState = nsnull;
  frame = aFrame;
  reason = eReflowReason_Incremental;
  reflowCommand = &aReflowCommand;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;
  rendContext = aRenderingContext;
  mSpaceManager = nsnull;
  mLineLayout = nsnull;
  isTopOfPage = PR_FALSE;
  Init(aPresContext);
}

// Initialize a reflow state for a child frames reflow. Some state
// is copied from the parent reflow state; the remaining state is
// computed.
nsHTMLReflowState::nsHTMLReflowState(nsIPresContext&          aPresContext,
                                     const nsHTMLReflowState& aParentReflowState,
                                     nsIFrame*                aFrame,
                                     const nsSize&            aAvailableSpace,
                                     nsReflowReason           aReason)
{
  parentReflowState = &aParentReflowState;
  frame = aFrame;
  reason = aReason;
  reflowCommand = (reason == eReflowReason_Incremental)
    ? aParentReflowState.reflowCommand
    : nsnull;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;

  rendContext = aParentReflowState.rendContext;
  mSpaceManager = aParentReflowState.mSpaceManager;
  mLineLayout = aParentReflowState.mLineLayout;
  isTopOfPage = aParentReflowState.isTopOfPage;

  Init(aPresContext);
}

// Same as the previous except that the reason is taken from the
// parent's reflow state.
nsHTMLReflowState::nsHTMLReflowState(nsIPresContext&          aPresContext,
                                     const nsHTMLReflowState& aParentReflowState,
                                     nsIFrame*                aFrame,
                                     const nsSize&            aAvailableSpace)
{
  parentReflowState = &aParentReflowState;
  frame = aFrame;
  reason = aParentReflowState.reason;
  reflowCommand = aParentReflowState.reflowCommand;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;

  rendContext = aParentReflowState.rendContext;
  mSpaceManager = aParentReflowState.mSpaceManager;
  mLineLayout = aParentReflowState.mLineLayout;
  isTopOfPage = aParentReflowState.isTopOfPage;

  Init(aPresContext);
}

void
nsHTMLReflowState::Init(nsIPresContext& aPresContext)
{
  mCompactMarginWidth = 0;
  mAlignCharOffset = 0;
  mUseAlignCharOffset = 0;

  frame->GetStyleData(eStyleStruct_Position,
                      (const nsStyleStruct*&)mStylePosition);
  frame->GetStyleData(eStyleStruct_Display,
                      (const nsStyleStruct*&)mStyleDisplay);
  frame->GetStyleData(eStyleStruct_Spacing,
                      (const nsStyleStruct*&)mStyleSpacing);
  mFrameType = DetermineFrameType(frame, mStylePosition, mStyleDisplay);
  InitConstraints(aPresContext);
}

const nsHTMLReflowState*
nsHTMLReflowState::GetContainingBlockReflowState(const nsReflowState* aParentRS)
{
  while (nsnull != aParentRS) {
    if (nsnull != aParentRS->frame) {
      PRBool isContainingBlock;
      // XXX This needs to go and we need to start using the info in the
      // reflow state...
      nsresult rv = aParentRS->frame->IsPercentageBase(isContainingBlock);
      if (NS_SUCCEEDED(rv) && isContainingBlock) {
        return (const nsHTMLReflowState*) aParentRS;
      }
    }
    aParentRS = aParentRS->parentReflowState;
  }
  return nsnull;
}

const nsHTMLReflowState*
nsHTMLReflowState::GetPageBoxReflowState(const nsReflowState* aParentRS)
{
  // XXX write me as soon as we can ask a frame if it's a page frame...
  return nsnull;
}

nscoord
nsHTMLReflowState::GetContainingBlockContentWidth(const nsReflowState* aParentRS)
{
  nscoord width = 0;
  const nsHTMLReflowState* rs =
    GetContainingBlockReflowState(aParentRS);
  if (nsnull != rs) {
    return ((nsHTMLReflowState*)aParentRS)->mComputedWidth;/* XXX cast */
  }
  return width;
}

nsCSSFrameType
nsHTMLReflowState::DetermineFrameType(nsIFrame* aFrame)
{
  const nsStylePosition* stylePosition;
  aFrame->GetStyleData(eStyleStruct_Position,
                       (const nsStyleStruct*&)stylePosition);
  const nsStyleDisplay* styleDisplay;
  aFrame->GetStyleData(eStyleStruct_Display,
                       (const nsStyleStruct*&)styleDisplay);
  return DetermineFrameType(aFrame, stylePosition, styleDisplay);
}

nsCSSFrameType
nsHTMLReflowState::DetermineFrameType(nsIFrame* aFrame,
                                      const nsStylePosition* aPosition,
                                      const nsStyleDisplay* aDisplay)
{
  nsCSSFrameType frameType;

  // Section 9.7 of the CSS2 spec indicates that absolute position
  // takes precedence over float which takes precedence over display.
  if (aPosition->IsAbsolutelyPositioned()) {
    frameType = NS_CSS_FRAME_TYPE_ABSOLUTE;
  }
  else if (NS_STYLE_FLOAT_NONE != aDisplay->mFloats) {
    frameType = NS_CSS_FRAME_TYPE_FLOATING;
  }
  else {
    switch (aDisplay->mDisplay) {
    case NS_STYLE_DISPLAY_BLOCK:
    case NS_STYLE_DISPLAY_LIST_ITEM:
    case NS_STYLE_DISPLAY_TABLE:
      frameType = NS_CSS_FRAME_TYPE_BLOCK;
      break;

    case NS_STYLE_DISPLAY_INLINE:
    case NS_STYLE_DISPLAY_MARKER:
    case NS_STYLE_DISPLAY_INLINE_TABLE:
      frameType = NS_CSS_FRAME_TYPE_INLINE;
      break;

    case NS_STYLE_DISPLAY_RUN_IN:
    case NS_STYLE_DISPLAY_COMPACT:
      // XXX need to look ahead at the frame's sibling
      frameType = NS_CSS_FRAME_TYPE_BLOCK;
      break;

    case NS_STYLE_DISPLAY_TABLE_CELL:
    case NS_STYLE_DISPLAY_TABLE_CAPTION:
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
  nsFrameState  frameState;
  aFrame->GetFrameState(&frameState);
  if (frameState & NS_FRAME_REPLACED_ELEMENT) {
    frameType = NS_FRAME_REPLACED(frameType);
  }

  return frameType;
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
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
    
    if (NS_STYLE_DIRECTION_LTR == display->mDirection) {
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
      if (eStyleUnit_Inherit == mStylePosition->mOffset.GetRightUnit()) {
        mComputedOffsets.right = cbrs->mComputedOffsets.right;
      } else {
        ComputeHorizontalValue(aContainingBlockWidth, mStylePosition->mOffset.GetRightUnit(),
                               mStylePosition->mOffset.GetRight(coord),
                               mComputedOffsets.right);
      }
      
      // Computed value for 'left' is minus the value of 'right'
      mComputedOffsets.left = -mComputedOffsets.right;
    }

  } else {
    NS_ASSERTION(rightIsAuto, "unexpected specified constraint");
    
    // 'Left' isn't 'auto' so compute its value
    if (eStyleUnit_Inherit == mStylePosition->mOffset.GetLeftUnit()) {
      mComputedOffsets.left = cbrs->mComputedOffsets.left;
    } else {
      ComputeHorizontalValue(aContainingBlockWidth, mStylePosition->mOffset.GetLeftUnit(),
                             mStylePosition->mOffset.GetLeft(coord),
                             mComputedOffsets.left);
    }

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
      if (eStyleUnit_Inherit == mStylePosition->mOffset.GetBottomUnit()) {
        mComputedOffsets.bottom = cbrs->mComputedOffsets.bottom;
      } else {
        ComputeVerticalValue(aContainingBlockHeight, mStylePosition->mOffset.GetBottomUnit(),
                               mStylePosition->mOffset.GetBottom(coord),
                               mComputedOffsets.bottom);
      }
      
      // Computed value for 'top' is minus the value of 'bottom'
      mComputedOffsets.top = -mComputedOffsets.bottom;
    }

  } else {
    NS_ASSERTION(bottomIsAuto, "unexpected specified constraint");
    
    // 'Top' isn't 'auto' so compute its value
    if (eStyleUnit_Inherit == mStylePosition->mOffset.GetTopUnit()) {
      mComputedOffsets.top = cbrs->mComputedOffsets.top;
    } else {
      ComputeVerticalValue(aContainingBlockHeight, mStylePosition->mOffset.GetTopUnit(),
                             mStylePosition->mOffset.GetTop(coord),
                             mComputedOffsets.top);
    }

    // Computed value for 'bottom' is minus the value of 'top'
    mComputedOffsets.bottom = -mComputedOffsets.top;
  }
}

void
nsHTMLReflowState::InitAbsoluteConstraints(nsIPresContext& aPresContext,
                                           const nsHTMLReflowState* cbrs,
                                           nscoord containingBlockWidth,
                                           nscoord containingBlockHeight)
{
  // If any of the offsets are 'auto', then get the placeholder frame
  // and compute its origin relative to the containing block
  nsPoint placeholderOffset(0, 0);
  if ((eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit()) ||
      (eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit()) ||
      (eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit()) ||
      (eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit())) {
    
    // Get the placeholder frame
    nsIFrame*     placeholderFrame;
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext.GetShell(getter_AddRefs(presShell));

    presShell->GetPlaceholderFrameFor(frame, &placeholderFrame);
    NS_ASSERTION(nsnull != placeholderFrame, "no placeholder frame");
    if (nsnull != placeholderFrame) {
      placeholderFrame->GetOrigin(placeholderOffset);

      nsIFrame* parent;
      placeholderFrame->GetParent(&parent);
      while ((nsnull != parent) && (parent != cbrs->frame)) {
        nsPoint origin;

        parent->GetOrigin(origin);
        placeholderOffset += origin;
        parent->GetParent(&parent);
      }

      // Offsets are relative to the containing block's padding edge, so translate
      // from the frame's edge to the padding edge
      nsMargin              blockBorder;
      const nsStyleSpacing* blockSpacing;

      cbrs->frame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)blockSpacing);
      blockSpacing->GetBorder(blockBorder);
      placeholderOffset.x -= blockBorder.top;
      placeholderOffset.y -= blockBorder.bottom;
    }
  }

  nsStyleUnit widthUnit = mStylePosition->mWidth.GetUnit();
  nsStyleUnit heightUnit = mStylePosition->mHeight.GetUnit();

  // Initialize the 'left' and 'right' computed offsets
  PRBool        leftIsAuto = PR_FALSE, rightIsAuto = PR_FALSE;
  nsStyleCoord  coord;
  if (eStyleUnit_Inherit == mStylePosition->mOffset.GetLeftUnit()) {
    mComputedOffsets.left = cbrs->mComputedOffsets.left;
  } else if (eStyleUnit_Auto == mStylePosition->mOffset.GetLeftUnit()) {
    if (NS_STYLE_DIRECTION_LTR == mStyleDisplay->mDirection) {
      mComputedOffsets.left = placeholderOffset.x;
    } else {
      mComputedOffsets.left = 0;
      leftIsAuto = PR_TRUE;
    }
  } else {
    ComputeHorizontalValue(containingBlockWidth, mStylePosition->mOffset.GetLeftUnit(),
                           mStylePosition->mOffset.GetLeft(coord),
                           mComputedOffsets.left);
  }
  if (eStyleUnit_Inherit == mStylePosition->mOffset.GetRightUnit()) {
    mComputedOffsets.right = cbrs->mComputedOffsets.right;
  } else if (eStyleUnit_Auto == mStylePosition->mOffset.GetRightUnit()) {
    if (NS_STYLE_DIRECTION_RTL == mStyleDisplay->mDirection) {
      mComputedOffsets.right = placeholderOffset.x;
    } else {
      mComputedOffsets.right = 0;
      rightIsAuto = PR_TRUE;
    }
  } else {
    ComputeHorizontalValue(containingBlockWidth, mStylePosition->mOffset.GetRightUnit(),
                           mStylePosition->mOffset.GetRight(coord),
                           mComputedOffsets.right);
  }

  // Calculate the computed width
  PRBool  marginLeftIsAuto = (eStyleUnit_Auto == mStyleSpacing->mMargin.GetLeftUnit());
  PRBool  marginRightIsAuto = (eStyleUnit_Auto == mStyleSpacing->mMargin.GetRightUnit());

  if (eStyleUnit_Auto == widthUnit) {
    // The element has a 'width' value of 'auto'
    if (NS_FRAME_IS_REPLACED(mFrameType)) {
      // Substitute the element's intrinsic width
      mComputedWidth = NS_INTRINSICSIZE;

    } else {
      // Any remaining 'auto' values for 'left', 'right', 'margin-left', or
      // 'margin-right' are replaced with 0 (their default value)
      mComputedWidth = containingBlockWidth - mComputedOffsets.left -
        mComputedMargin.left - mComputedBorderPadding.left -
        mComputedBorderPadding.right -
        mComputedMargin.right - mComputedOffsets.right;

      // Factor in any minimum and maximum size information
      if (mComputedWidth > mComputedMaxWidth) {
        mComputedWidth = mComputedMaxWidth;
      } else if (mComputedWidth < mComputedMinWidth) {
        mComputedWidth = mComputedMinWidth;
      } else {
        // Note that we wait until after checking minimum and maximum size
        // information, because if we use the minimum or maximum value instead
        // then the rules are applied again and that means margin recalculation
        leftIsAuto = PR_FALSE;
        rightIsAuto = PR_FALSE;
        marginLeftIsAuto = PR_FALSE;
        marginRightIsAuto = PR_FALSE;
      }
    }

  } else {
    if (eStyleUnit_Inherit == widthUnit) {
      mComputedWidth = containingBlockWidth;
    } else {
      // Use the specified value for the computed width
      ComputeHorizontalValue(containingBlockWidth, widthUnit,
                             mStylePosition->mWidth, mComputedWidth);
    }

    // Factor in any minimum and maximum size information
    if (mComputedWidth > mComputedMaxWidth) {
      mComputedWidth = mComputedMaxWidth;
    } else if (mComputedWidth < mComputedMinWidth) {
      mComputedWidth = mComputedMinWidth;
    }

    // See what edge the width applies to (the default is the content
    // edge)
    if (mComputedWidth != NS_UNCONSTRAINEDSIZE) {
      if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_PADDING) {
        mComputedWidth -= mComputedPadding.left + mComputedPadding.right;
      } else if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_BORDER) {
        mComputedWidth -= mComputedBorderPadding.left + mComputedBorderPadding.right;
      }
    }
  }

  // Calculate any remaining 'auto' values for the offsets and margins
  if (leftIsAuto) {
    // Any 'auto' on 'margin-left' or 'margin-right' are replaced with 0
    // (their default value)
    mComputedOffsets.left = containingBlockWidth - mComputedMargin.left -
      mComputedBorderPadding.left - mComputedWidth -
      mComputedBorderPadding.right -
      mComputedMargin.right - mComputedOffsets.right;

  } else if (rightIsAuto) {
    // Any 'auto' on 'margin-left' or 'margin-right' are replaced with 0
    // (their default value)
    mComputedOffsets.right = containingBlockWidth - mComputedOffsets.left -
      mComputedMargin.left - mComputedBorderPadding.left - mComputedWidth -
      mComputedBorderPadding.right - mComputedMargin.right;

  } else if (marginLeftIsAuto || marginRightIsAuto) {
    // All that's left to solve for are 'auto' values for 'margin-left' and
    // 'margin-right'
    if (NS_FRAME_IS_REPLACED(mFrameType)) {
      // We can't solve for 'auto' values for 'margin-left' and 'margin-right'
      // until after we reflow the frame and it tells us its intrinsic width
      if (marginLeftIsAuto) {
        mComputedMargin.left = NS_AUTOMARGIN;
      }
      if (marginRightIsAuto) {
        mComputedMargin.right = NS_AUTOMARGIN;
      }
    } else {
      // Calculate the amount of space for margins
      nscoord availMarginSpace = containingBlockWidth -
        mComputedOffsets.left - mComputedBorderPadding.left -
        mComputedWidth - mComputedBorderPadding.right -
        mComputedOffsets.right;
  
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
  }

  // Initialize the 'top' and 'bottom' computed offsets
  PRBool  bottomIsAuto = PR_FALSE;
  if (eStyleUnit_Inherit == mStylePosition->mOffset.GetTopUnit()) {
    mComputedOffsets.top = cbrs->mComputedOffsets.top;
  } else if ((eStyleUnit_Auto == mStylePosition->mOffset.GetTopUnit()) ||
      ((NS_AUTOHEIGHT == containingBlockHeight) &&
       (eStyleUnit_Percent == mStylePosition->mOffset.GetTopUnit()))) {
    // Use the placeholder position
    mComputedOffsets.top = placeholderOffset.y;
  } else {
    nsStyleCoord c;
    ComputeVerticalValue(containingBlockHeight,
                         mStylePosition->mOffset.GetTopUnit(),
                         mStylePosition->mOffset.GetTop(c),
                         mComputedOffsets.top);
  }
  if (eStyleUnit_Inherit == mStylePosition->mOffset.GetBottomUnit()) {
    mComputedOffsets.bottom = cbrs->mComputedOffsets.bottom;
  } else if ((eStyleUnit_Auto == mStylePosition->mOffset.GetBottomUnit()) ||
      ((NS_AUTOHEIGHT == containingBlockHeight) &&
       (eStyleUnit_Percent == mStylePosition->mOffset.GetBottomUnit()))) {
    if (eStyleUnit_Auto == heightUnit) {
      mComputedOffsets.bottom = 0;        
    } else {
      bottomIsAuto = PR_TRUE;
    }
  } else {
    nsStyleCoord c;
    ComputeVerticalValue(containingBlockHeight,
                         mStylePosition->mOffset.GetBottomUnit(),
                         mStylePosition->mOffset.GetBottom(c),
                         mComputedOffsets.bottom);
  }

  // Check for a percentage based height and a containing block height
  // that depends on its content height, i.e., not explicitly specified
  if (eStyleUnit_Percent == heightUnit) {
    if (NS_AUTOHEIGHT == containingBlockHeight) {
      // Interpret the height like 'auto'
      heightUnit = eStyleUnit_Auto;
    }
  }

  // Calculate the computed height
  PRBool  marginTopIsAuto = (eStyleUnit_Auto == mStyleSpacing->mMargin.GetTopUnit());
  PRBool  marginBottomIsAuto = (eStyleUnit_Auto == mStyleSpacing->mMargin.GetBottomUnit());
  
  if (eStyleUnit_Auto == heightUnit) {
    if (NS_FRAME_IS_REPLACED(mFrameType)) {
      mComputedHeight = NS_INTRINSICSIZE;
    } else {
      // Solve for the value of 'height'
      if (NS_AUTOHEIGHT == containingBlockHeight) {
        // If the containing block's height was not explicitly specified (i.e.,
        // it depends on its content height), then so does our height
        mComputedHeight = NS_AUTOHEIGHT;

      } else {
        // Replace any 'auto' on 'margin-top' or 'margin-bottom' with 0 (their
        // default values). If 'bottom' is 'auto', then replace it with '0' (its
        // default value), too
        mComputedHeight = containingBlockHeight - mComputedOffsets.top - 
          mComputedMargin.top - mComputedBorderPadding.top -
          mComputedBorderPadding.bottom -
          mComputedMargin.bottom - mComputedOffsets.bottom;
        
        // Factor in any minimum and maximum size information
        if (mComputedHeight > mComputedMaxHeight) {
          mComputedHeight = mComputedMaxHeight;
        } else if (mComputedHeight < mComputedMinHeight) {
          mComputedHeight = mComputedMinHeight;
        } else {
          // Note that we wait until after checkin minimum and maximum size
          // information, because if we use the minimum or maximum value instead
          // then the rules are applied again and that means margin recalculation
          marginTopIsAuto = PR_FALSE;
          marginBottomIsAuto = PR_FALSE;
          bottomIsAuto = PR_FALSE;
        }
      }
    }
  } else {
    if (eStyleUnit_Inherit == heightUnit) {
      mComputedHeight = containingBlockHeight;
    } else {
      // Use the specified value for the computed height
      ComputeVerticalValue(containingBlockHeight, heightUnit,
                           mStylePosition->mHeight, mComputedHeight);
    }

    // Factor in any minimum and maximum size information
    if (mComputedHeight > mComputedMaxHeight) {
      mComputedHeight = mComputedMaxHeight;
    }
    if (mComputedHeight < mComputedMinHeight) {
      mComputedHeight = mComputedMinHeight;
    }

    // See what edge the height applies to (the default is the content
    // edge)
    if (mComputedHeight != NS_AUTOHEIGHT) {
      if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_PADDING) {
        mComputedHeight -= mComputedPadding.top + mComputedPadding.bottom;
      } else if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_BORDER) {
        mComputedHeight -= mComputedBorderPadding.top + mComputedBorderPadding.bottom;
      }
    }
  }

  // Calculate any remaining 'auto' values for the offsets and margins
  if (NS_AUTOHEIGHT != containingBlockHeight) {
    if (bottomIsAuto) {
      // Any 'auto' on 'margin-top' or 'margin-bottom' are replaced with 0
      mComputedOffsets.bottom = containingBlockHeight - mComputedOffsets.top -
        mComputedMargin.top - mComputedBorderPadding.top - mComputedHeight -
        mComputedBorderPadding.bottom - mComputedMargin.bottom;

    } else if (marginTopIsAuto || marginBottomIsAuto) {
      // All that's left to solve for are 'auto' values for 'margin-top' and
      // 'margin-bottom'
      if (NS_FRAME_IS_REPLACED(mFrameType)) {
        // We can't solve for 'auto' values for 'margin-top' and 'margin-bottom'
        // until after we reflow the frame and it tells us its intrinsic height
        if (marginTopIsAuto) {
          mComputedMargin.top = NS_AUTOMARGIN;
        }
        if (marginBottomIsAuto) {
          mComputedMargin.bottom = NS_AUTOMARGIN;
        }
      } else {
        // Calculate the amount of space for margins
        nscoord availMarginSpace = containingBlockHeight - mComputedOffsets.top -
          mComputedBorderPadding.top - mComputedHeight - mComputedBorderPadding.bottom -
          mComputedOffsets.bottom;

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
    }
  }
}

// Called by InitConstraints() to compute the containing block rectangle for
// the element. Handles the special logic for absolutely positioned elements
void
nsHTMLReflowState::ComputeContainingBlockRectangle(const nsHTMLReflowState* aContainingBlockRS,
                                                   nscoord& aContainingBlockWidth,
                                                   nscoord& aContainingBlockHeight)
{
  // Unless the element is absolutely positioned, the containing block is
  // formed by the content edge of the nearest block-level ancestor
  aContainingBlockWidth = aContainingBlockRS->mComputedWidth;
  aContainingBlockHeight = aContainingBlockRS->mComputedHeight;
  
  if (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE) {
    // See if the ancestor is block-level or inline-level
    if (NS_FRAME_GET_TYPE(aContainingBlockRS->mFrameType) == NS_CSS_FRAME_TYPE_INLINE) {
      // The CSS2 spec says that if the ancestor is inline-level, the containing
      // block depends on the 'direction' property of the ancestor. For direction
      // 'ltr', it's the top and left of the content edges of the first box and
      // the bottom and right content edges of the last box
      //
      // XXX This is a pain because it isn't top-down and it requires that we've
      // completely reflowed the ancestor. It also isn't clear what happens when
      // a relatively positioned ancestor is split across pages. So instead use
      // the computed width and height of the nearest block-level ancestor
      const nsHTMLReflowState*  cbrs = aContainingBlockRS;
      while (cbrs) {
        nsCSSFrameType  type = NS_FRAME_GET_TYPE(cbrs->mFrameType);
        if ((NS_CSS_FRAME_TYPE_BLOCK == type) ||
            (NS_CSS_FRAME_TYPE_FLOATING == type) ||
            (NS_CSS_FRAME_TYPE_ABSOLUTE == type)) {

          aContainingBlockWidth = cbrs->mComputedWidth;
          aContainingBlockHeight = cbrs->mComputedHeight;

          if (NS_CSS_FRAME_TYPE_ABSOLUTE == type) {
            aContainingBlockWidth += cbrs->mComputedPadding.left +
                                     cbrs->mComputedPadding.right;
            aContainingBlockHeight += cbrs->mComputedPadding.top +
                                      cbrs->mComputedPadding.bottom;
          }
          break;
        }

        cbrs = (const nsHTMLReflowState*)cbrs->parentReflowState;   // XXX cast
      }

    } else {
      // If the ancestor is block-level, the containing block is formed by the
      // padding edge of the ancestor
      aContainingBlockWidth += aContainingBlockRS->mComputedPadding.left +
                               aContainingBlockRS->mComputedPadding.right;
      aContainingBlockHeight += aContainingBlockRS->mComputedPadding.top +
                                aContainingBlockRS->mComputedPadding.bottom;
    }
  }
}

// XXX refactor this code to have methods for each set of properties
// we are computing: width,height,line-height; margin; offsets

void
nsHTMLReflowState::InitConstraints(nsIPresContext& aPresContext)
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
    const nsHTMLReflowState* cbrs =
      GetContainingBlockReflowState(parentReflowState);
    NS_ASSERTION(nsnull != cbrs, "no containing block");

    // Compute the containing block rectangle for the element
    nscoord containingBlockWidth;
    nscoord containingBlockHeight;
    ComputeContainingBlockRectangle(cbrs, containingBlockWidth, containingBlockHeight);

    // See if the element is relatively positioned
    if (NS_STYLE_POSITION_RELATIVE == mStylePosition->mPosition) {
      ComputeRelativeOffsets(cbrs, containingBlockWidth, containingBlockHeight);
    } else {
      // Initialize offsets to 0
      mComputedOffsets.SizeTo(0, 0, 0, 0);
    }

#if 0
    nsFrame::ListTag(stdout, frame); printf(": cb=");
    nsFrame::ListTag(stdout, cbrs->frame); printf(" size=%d,%d\n", containingBlockWidth, containingBlockHeight);
#endif

    // See if the containing block height is based on the size of its
    // content
    if (NS_AUTOHEIGHT == containingBlockHeight) {
      // See if the containing block is a scrolled frame, i.e. its
      // parent is a scroll frame. The presence of the intervening
      // frame (that the scroll frame scrolls) needs to be hidden from
      // the containingBlockHeight calcuation.
      if (cbrs->parentReflowState) {
        nsIFrame* f = cbrs->parentReflowState->frame;
        nsIAtom*  cbFrameType;

        f->GetFrameType(&cbFrameType);
        if (nsLayoutAtoms::scrollFrame == cbFrameType) {
          // Use the scroll frame's computed height instead
          containingBlockHeight =
            ((nsHTMLReflowState*)cbrs->parentReflowState)->mComputedHeight;
        }
        NS_IF_RELEASE(cbFrameType);
      }
    }

    // Compute margins from the specified margin style information. These
    // become the default computed values, and may be adjusted below
    // XXX fix to provide 0,0 for the top&bottom margins for
    // inline-non-replaced elements
    ComputeMargin(containingBlockWidth, cbrs);
    ComputePadding(containingBlockWidth, cbrs);
    if (!mStyleSpacing->GetBorder(mComputedBorderPadding)) {
      // CSS2 has no percentage borders
      mComputedBorderPadding.SizeTo(0, 0, 0, 0);
    }
    mComputedBorderPadding += mComputedPadding;

    nsStyleUnit widthUnit = mStylePosition->mWidth.GetUnit();
    nsStyleUnit heightUnit = mStylePosition->mHeight.GetUnit();

    // Check for a percentage based width and an unconstrained containing
    // block width
    if (eStyleUnit_Percent == widthUnit) {
      if (NS_UNCONSTRAINEDSIZE == containingBlockWidth) {
        // Interpret the width like 'auto'
        widthUnit = eStyleUnit_Auto;
      }
    }
    // Check for a percentage based height and a containing block height
    // that depends on the content height
    if (eStyleUnit_Percent == heightUnit) {
      if (NS_AUTOHEIGHT == containingBlockHeight) {
        // Interpret the height like 'auto'
        heightUnit = eStyleUnit_Auto;
      }
    }

    // Calculate the computed values for min and max properties
    ComputeMinMaxValues(containingBlockWidth, containingBlockHeight, cbrs);

    // Calculate the computed width and height. This varies by frame type
    if ((NS_FRAME_REPLACED(NS_CSS_FRAME_TYPE_INLINE) == mFrameType) ||
        (NS_FRAME_REPLACED(NS_CSS_FRAME_TYPE_FLOATING) == mFrameType)) {
      // Inline replaced element and floating replaced element are basically
      // treated the same. First calculate the computed width
      if (eStyleUnit_Inherit == widthUnit) {
        mComputedWidth = containingBlockWidth;
      } else if (eStyleUnit_Auto == widthUnit) {
        // A specified value of 'auto' uses the element's intrinsic width
        mComputedWidth = NS_INTRINSICSIZE;
      } else {
        ComputeHorizontalValue(containingBlockWidth, widthUnit,
                               mStylePosition->mWidth,
                               mComputedWidth);
      }

      if (mComputedWidth != NS_INTRINSICSIZE) {
        // Take into account minimum and maximum sizes
        if (mComputedWidth > mComputedMaxWidth) {
          mComputedWidth = mComputedMaxWidth;
        } else if (mComputedWidth < mComputedMinWidth) {
          mComputedWidth = mComputedMinWidth;
        }
  
        // See what edge the width applies to (the default is the content
        // edge)
        if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_PADDING) {
          mComputedWidth -= mComputedPadding.left + mComputedPadding.right;
        } else if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_BORDER) {
          mComputedWidth -= mComputedBorderPadding.left + mComputedBorderPadding.right;
        }
      }

      // Now calculate the computed height
      if (eStyleUnit_Inherit == heightUnit) {
        mComputedHeight = containingBlockHeight;
      } else if (eStyleUnit_Auto == heightUnit) {
        // A specified value of 'auto' uses the element's intrinsic height
        mComputedHeight = NS_INTRINSICSIZE;
      } else {
        ComputeVerticalValue(containingBlockHeight, heightUnit,
                             mStylePosition->mHeight,
                             mComputedHeight);
      }

      if (mComputedHeight != NS_INTRINSICSIZE) {
        // Take into account minimum and maximum sizes
        if (mComputedHeight > mComputedMaxHeight) {
          mComputedHeight = mComputedMaxHeight;
        } else if (mComputedHeight < mComputedMinHeight) {
          mComputedHeight = mComputedMinHeight;
        }
  
        // See what edge the height applies to (the default is the content
        // edge)
        if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_PADDING) {
          mComputedHeight -= mComputedPadding.top + mComputedPadding.bottom;
        } else if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_BORDER) {
          mComputedHeight -= mComputedBorderPadding.top + mComputedBorderPadding.bottom;
        }
      }
    
    } else if (NS_CSS_FRAME_TYPE_FLOATING == mFrameType) {
      // Floating non-replaced element. First calculate the computed width
      if (eStyleUnit_Inherit == widthUnit) {
        mComputedWidth = containingBlockWidth;
      } else if (eStyleUnit_Auto == widthUnit) {
        // A specified value of 'auto' becomes a computed width of 0
        mComputedWidth = 0;
      } else {
        ComputeHorizontalValue(containingBlockWidth, widthUnit,
                               mStylePosition->mWidth,
                               mComputedWidth);
      }

      // Take into account minimum and maximum sizes
      if (mComputedWidth > mComputedMaxWidth) {
        mComputedWidth = mComputedMaxWidth;
      } else if (mComputedWidth < mComputedMinWidth) {
        mComputedWidth = mComputedMinWidth;
      }

      // See what edge the width applies to (the default is the content
      // edge)
      if ((mComputedWidth > 0) && (mComputedWidth != NS_UNCONSTRAINEDSIZE)) {
        if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_PADDING) {
          mComputedWidth -= mComputedPadding.left + mComputedPadding.right;
        } else if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_BORDER) {
          mComputedWidth -= mComputedBorderPadding.left + mComputedBorderPadding.right;
        }
      }

      // Now calculate the computed height
      if (eStyleUnit_Inherit == heightUnit) {
        mComputedHeight = containingBlockHeight;
      } else if (eStyleUnit_Auto == heightUnit) {
        mComputedHeight = NS_AUTOHEIGHT;  // let it choose its height
      } else {
        ComputeVerticalValue(containingBlockHeight, heightUnit,
                             mStylePosition->mHeight,
                             mComputedHeight);
      }

      // Take into account minimum and maximum sizes
      if (mComputedHeight > mComputedMaxHeight) {
        mComputedHeight = mComputedMaxHeight;
      } else if (mComputedHeight < mComputedMinHeight) {
        mComputedHeight = mComputedMinHeight;
      }

      // See what edge the height applies to (the default is the content
      // edge)
      if (mComputedHeight != NS_AUTOHEIGHT) {
        if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_PADDING) {
          mComputedHeight -= mComputedPadding.top + mComputedPadding.bottom;
        } else if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_BORDER) {
          mComputedHeight -= mComputedBorderPadding.top + mComputedBorderPadding.bottom;
        }
      }
    
    } else if (NS_CSS_FRAME_TYPE_INTERNAL_TABLE == mFrameType) {
      // Internal table elements. The rules vary depending on the type.
      // Calculate the computed width
      if ((NS_STYLE_DISPLAY_TABLE_ROW == mStyleDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_ROW_GROUP == mStyleDisplay->mDisplay)) {
        // 'width' property doesn't apply to table rows and row groups
        widthUnit = eStyleUnit_Auto;
      }

      if (eStyleUnit_Inherit == widthUnit) {
        mComputedWidth = containingBlockWidth;
      } else if (eStyleUnit_Auto == widthUnit) {
        mComputedWidth = availableWidth;

        if (mComputedWidth != NS_UNCONSTRAINEDSIZE) {
          // Internal table elements don't have margins, but they have border
          // and padding
          mComputedWidth -= mComputedBorderPadding.left +
            mComputedBorderPadding.right;
        }
      
      } else {
        ComputeHorizontalValue(containingBlockWidth, widthUnit,
                               mStylePosition->mWidth,
                               mComputedWidth);
      }

      // Calculate the computed height
      if ((NS_STYLE_DISPLAY_TABLE_COLUMN == mStyleDisplay->mDisplay) ||
          (NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == mStyleDisplay->mDisplay)) {
        // 'height' property doesn't apply to table columns and column groups
        heightUnit = eStyleUnit_Auto;
      }
      if (eStyleUnit_Inherit == heightUnit) {
        mComputedHeight = containingBlockHeight;
      } else if (eStyleUnit_Auto == heightUnit) {
        mComputedHeight = NS_AUTOHEIGHT;
      } else {
        ComputeVerticalValue(containingBlockHeight, heightUnit,
                             mStylePosition->mHeight,
                             mComputedHeight);
      }

      // Doesn't apply to table elements
      mComputedMinWidth = mComputedMinHeight = 0;
      mComputedMaxWidth = mComputedMaxHeight = NS_UNCONSTRAINEDSIZE;

    } else if (NS_FRAME_GET_TYPE(mFrameType) == NS_CSS_FRAME_TYPE_ABSOLUTE) {
      // XXX not sure if this belongs here or somewhere else - cwk
      // an nsHTMLFrameInnerFrame doesn't get a placeholder frame, the nsHTMLFrameOuterFrame does
      nsIAtom* targetFrameType;
      frame->GetFrameType(&targetFrameType);
      if (nsLayoutAtoms::htmlFrameInnerFrame != targetFrameType) {
        InitAbsoluteConstraints(aPresContext, cbrs, containingBlockWidth,
                                containingBlockHeight);
      }
      NS_IF_RELEASE(targetFrameType);
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
                          containingBlockWidth,
                          containingBlockHeight);
    }
  }
}

// Compute the box data for block and block-replaced elements in the
// normal flow.
void
nsHTMLReflowState::ComputeBlockBoxData(nsIPresContext& aPresContext,
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
      } else {
        mComputedWidth = availableWidth - mComputedMargin.left -
          mComputedMargin.right - mComputedBorderPadding.left -
          mComputedBorderPadding.right;

        // Take into account any min and max values
        if (mComputedWidth > mComputedMaxWidth) {
          // Apply the rules again, but this time using 'max-width' as
          // the value for 'width'
          mComputedWidth = mComputedMaxWidth;
        } else if (mComputedWidth < mComputedMinWidth) {
          // Apply the rules again, but this time using 'min-width' as
          // the value for 'width'
          mComputedWidth = mComputedMinWidth;
        }
      }
    }
  } else {
    if (eStyleUnit_Inherit == aWidthUnit) {
      // Use parent element's width. Note that if its width was
      // 'inherit', then it already did this so we don't need to
      // recurse upwards.
      //
      // We use the containing block's width here for the "parent"
      // elements width, because we want to skip over any intervening
      // inline elements (since width doesn't apply to them).
      if (NS_UNCONSTRAINEDSIZE != aContainingBlockWidth) {
        mComputedWidth = aContainingBlockWidth;
      }
      else {
        mComputedWidth = NS_UNCONSTRAINEDSIZE;
      }
    }
    else {
      ComputeHorizontalValue(aContainingBlockWidth, aWidthUnit,
                             mStylePosition->mWidth, mComputedWidth);
    }

    // Take into account any min and max values
    if (mComputedWidth > mComputedMaxWidth) {
      mComputedWidth = mComputedMaxWidth;
    } else if (mComputedWidth < mComputedMinWidth) {
      mComputedWidth = mComputedMinWidth;
    }

    // See what edge the width applies to (the default is the content
    // edge)
    if (mComputedWidth != NS_UNCONSTRAINEDSIZE) {
      if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_PADDING) {
        mComputedWidth -= mComputedPadding.left + mComputedPadding.right;
      } else if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_BORDER) {
        mComputedWidth -= mComputedBorderPadding.left + mComputedBorderPadding.right;
      }
    }

    // Now that we have the computed-width, compute the side margins
    CalculateBlockSideMargins(cbrs, mComputedWidth);
  }

  // Compute the content height
  if (eStyleUnit_Inherit == aHeightUnit) {
    // Use parent elements height (note that if its height was inherit
    // then it already did this so we don't need to recurse upwards).
    //
    // We use the containing blocks height here for the "parent"
    // elements height because we want to skip over any interveening
    // inline elements (since height doesn't apply to them).
    if (NS_UNCONSTRAINEDSIZE != aContainingBlockHeight) {
      mComputedHeight = aContainingBlockHeight;
    }
    else {
      mComputedHeight = NS_UNCONSTRAINEDSIZE;
    }
  } else if (eStyleUnit_Auto == aHeightUnit) {
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
  // Take into account any min and max values
  if (mComputedHeight > mComputedMaxHeight) {
    mComputedHeight = mComputedMaxHeight;
  } else if (mComputedHeight < mComputedMinHeight) {
    mComputedHeight = mComputedMinHeight;
  }
  // See what edge the height applies to (the default is the content
  // edge)
  if (mComputedHeight != NS_UNCONSTRAINEDSIZE) {
    if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_PADDING) {
      mComputedHeight -= mComputedPadding.top + mComputedPadding.bottom;
    } else if (mStylePosition->mBoxSizing == NS_STYLE_BOX_SIZING_BORDER) {
      mComputedHeight -= mComputedBorderPadding.top + mComputedBorderPadding.bottom;
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
nsHTMLReflowState::CalculateBlockSideMargins(const nsHTMLReflowState* cbrs,
                                             nscoord amComputedWidth)
{
  // We can only provide values for auto side margins in a constrained
  // reflow. For unconstrained reflow there is no effective width to
  // compute against...
  if ((NS_UNCONSTRAINEDSIZE == amComputedWidth) ||
      (NS_UNCONSTRAINEDSIZE == cbrs->mComputedWidth)) {
    return;
  }

  nscoord sum = mComputedMargin.left + mComputedBorderPadding.left +
    amComputedWidth + mComputedBorderPadding.right + mComputedMargin.right;
  if (sum == cbrs->mComputedWidth) {
    // The sum is already correct
    return;
  }

  // Determine the left and right margin values. The width value
  // remains constant while we do this.
  PRBool isAutoLeftMargin =
    eStyleUnit_Auto == mStyleSpacing->mMargin.GetLeftUnit();
  PRBool isAutoRightMargin =
    eStyleUnit_Auto == mStyleSpacing->mMargin.GetRightUnit();

  // Calculate how much space is available for margins
  nscoord availMarginSpace = cbrs->mComputedWidth - amComputedWidth -
    mComputedBorderPadding.left - mComputedBorderPadding.right;
  if (availMarginSpace < 0) {
    // Whoops - the element is too large for the available space. In
    // this case use the "direction" property to pin the element to
    // the left or right side. Note that we look at the parent's
    // direction since the parent will be placing this element.
    mComputedMargin.left = 0;
    mComputedMargin.right = 0;
    const nsHTMLReflowState* prs = (const nsHTMLReflowState*)parentReflowState;
    if (prs && (NS_STYLE_DIRECTION_RTL == prs->mStyleDisplay->mDirection)) {
      mComputedMargin.left = availMarginSpace;
    }
  }
  else {
    // See whether we're over constrained

    // XXX NOTE: do not mess with tables so that they can be centered;
    // we need a better way to factor this in!!!
    if (mStyleDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE) {
      if (!isAutoLeftMargin && !isAutoRightMargin) {
        // Neither margin is 'auto' so we're over constrained. Use the
        // 'direction' property of the parent to tell which margin to
        // ignore
        const nsHTMLReflowState* prs = (const nsHTMLReflowState*)
          parentReflowState;
        isAutoRightMargin = PR_TRUE;
        if (prs &&
            (NS_STYLE_DIRECTION_RTL == prs->mStyleDisplay->mDirection)) {
          isAutoLeftMargin = PR_TRUE;
        }
      }
    }

    if (isAutoLeftMargin) {
      if (isAutoRightMargin) {
        // Both margins are 'auto' so their computed values are equal
        mComputedMargin.left = availMarginSpace / 2;
        mComputedMargin.right = availMarginSpace - mComputedMargin.left;
      } else {
        mComputedMargin.left = availMarginSpace - mComputedMargin.right;
      }
    } else if (isAutoRightMargin) {
      mComputedMargin.right = availMarginSpace - mComputedMargin.left;
    }
  }
}

#if 0
// XXX I have no idea if this is right or not -- kipp
void
nsHTMLReflowState::CalculateTableSideMargins(const nsHTMLReflowState* cbrs,
                                             nscoord amComputedWidth)
{
  // We can only provide values for auto side margins in a constrained
  // reflow. For unconstrained reflow there is no effective width to
  // compute against...
  if ((NS_UNCONSTRAINEDSIZE != amComputedWidth) && 
      (NS_UNCONSTRAINEDSIZE != cbrs->mComputedWidth)) {
    PRBool isAutoLeftMargin =
      eStyleUnit_Auto == mStyleSpacing->mMargin.GetLeftUnit();
    PRBool isAutoRightMargin =
      eStyleUnit_Auto == mStyleSpacing->mMargin.GetRightUnit();

    // Calculate how much space is available for margins
    nscoord availMarginSpace = cbrs->mComputedWidth - amComputedWidth -
      mComputedBorderPadding.left - mComputedBorderPadding.right;

    if (availMarginSpace != 0) {
      // See whether we're over constrained
      if (isAutoLeftMargin) {
        if (isAutoRightMargin) {
          // Both margins are 'auto' so their computed values are equal
          mComputedMargin.left = availMarginSpace / 2;
          mComputedMargin.right = availMarginSpace - mComputedMargin.left;
        } else {
          mComputedMargin.left = availMarginSpace - mComputedMargin.right;
        }
      } else if (isAutoRightMargin) {
        mComputedMargin.right = availMarginSpace - mComputedMargin.left;
      }
    }
  }
}
#endif

nscoord
nsHTMLReflowState::CalcLineHeight(nsIPresContext& aPresContext,
                                  nsIFrame* aFrame)
{
  nscoord lineHeight = -1;
  nsIStyleContext* sc;
  aFrame->GetStyleContext(&sc);
  const nsStyleFont* elementFont = nsnull;
  if (nsnull != sc) {
    elementFont = (const nsStyleFont*)sc->GetStyleData(eStyleStruct_Font);
    for (;;) {
      const nsStyleText* text = (const nsStyleText*)
        sc->GetStyleData(eStyleStruct_Text);
      if (nsnull != text) {
        nsStyleUnit unit = text->mLineHeight.GetUnit();
        if (eStyleUnit_Normal == unit) {
          // Normal value
#ifdef NOISY_VERTICAL_ALIGN
          printf("  line-height: normal\n");
#endif
          break;
        } else if (eStyleUnit_Factor == unit) {
          // CSS2 spec says that the number is inherited, not the
          // computed value. Therefore use the font size of the
          // element times the inherited number.
          nscoord size = elementFont->mFont.size;
          lineHeight = nscoord(size * text->mLineHeight.GetFactorValue());
          if (lineHeight < 0) {
            lineHeight = -1;
          }
#ifdef NOISY_VERTICAL_ALIGN
          printf("  line-height: factor=%g result=%d\n",
                 text->mLineHeight.GetFactorValue(), lineHeight);
#endif
          break;
        }
        else if (eStyleUnit_Coord == unit) {
          lineHeight = text->mLineHeight.GetCoordValue();
          if (lineHeight < 0) {
            lineHeight = -1;
          }
          break;
        }
        else if (eStyleUnit_Percent == unit) {
          // XXX This could arguably be the font-metrics actual height
          // instead since the spec says use the computed height.
          const nsStyleFont* font = (const nsStyleFont*)
            sc->GetStyleData(eStyleStruct_Font);
          nscoord size = font->mFont.size;
          lineHeight = nscoord(size * text->mLineHeight.GetPercentValue());
          if (lineHeight < 0) {
            lineHeight = -1;
          }
          break;
        }
        else if (eStyleUnit_Inherit == unit) {
          nsIStyleContext* parentSC;
          parentSC = sc->GetParent();
          if (nsnull == parentSC) {
            // Note: Break before releasing to avoid double-releasing sc
            break;
          }
          NS_RELEASE(sc);
          sc = parentSC;
        }
        else {
          // other units are not part of the spec so don't bother
          // looping
          break;
        }
      }
    }
    NS_RELEASE(sc);
  }

  return lineHeight;
}

void
nsHTMLReflowState::ComputeHorizontalValue(nscoord aContainingBlockWidth,
                                          nsStyleUnit aUnit,
                                          const nsStyleCoord& aCoord,
                                          nscoord& aResult)
{
  NS_PRECONDITION(eStyleUnit_Inherit != aUnit, "unexpected unit");
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
      const nsStyleFont* font;
      frame->GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&) font);
      rendContext->SetFont(font->mFont);
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
  NS_PRECONDITION(eStyleUnit_Inherit != aUnit, "unexpected unit");
  aResult = 0;
  if (eStyleUnit_Percent == aUnit) {
    // Verify no one is trying to calculate a percentage based height against
    // a height that's shrink wrapping to its content. In that case they should
    // treat the specified value like 'auto'
    NS_ASSERTION(NS_AUTOHEIGHT != aContainingBlockHeight, "unexpected containing block height");
    float pct = aCoord.GetPercentValue();
    aResult = NSToCoordFloor(aContainingBlockHeight * pct);

  } else if (eStyleUnit_Coord == aUnit) {
    aResult = aCoord.GetCoordValue();
  }
}

void
nsHTMLReflowState::ComputeMargin(nscoord aContainingBlockWidth,
                                 const nsHTMLReflowState* aContainingBlockRS)
{
  // If style style can provide us the margin directly, then use it.
  if (!mStyleSpacing->GetMargin(mComputedMargin)) {
    // We have to compute the value
    if (NS_UNCONSTRAINEDSIZE == aContainingBlockWidth) {
      mComputedMargin.left = 0;
      mComputedMargin.right = 0;

    } else {
      nsStyleCoord left, right;

      if (eStyleUnit_Inherit == mStyleSpacing->mMargin.GetLeftUnit()) {
        mComputedMargin.left = aContainingBlockRS->mComputedMargin.left;
      } else {
        ComputeHorizontalValue(aContainingBlockWidth,
                               mStyleSpacing->mMargin.GetLeftUnit(),
                               mStyleSpacing->mMargin.GetLeft(left),
                               mComputedMargin.left);
      }
      if (eStyleUnit_Inherit == mStyleSpacing->mMargin.GetRightUnit()) {
        mComputedMargin.right = aContainingBlockRS->mComputedMargin.right;
      } else {
        ComputeHorizontalValue(aContainingBlockWidth,
                               mStyleSpacing->mMargin.GetRightUnit(),
                               mStyleSpacing->mMargin.GetRight(right),
                               mComputedMargin.right);
      }
    }

    const nsHTMLReflowState* rs2 = GetPageBoxReflowState(parentReflowState);
    nsStyleCoord top, bottom;
    if (nsnull != rs2) {
      // According to the CSS2 spec, margin percentages are
      // calculated with respect to the *height* of the containing
      // block when in a paginated context.
      if (eStyleUnit_Inherit == mStyleSpacing->mMargin.GetTopUnit()) {
        mComputedMargin.top = aContainingBlockRS->mComputedMargin.top;
      } else {
        ComputeVerticalValue(rs2->mComputedHeight,
                             mStyleSpacing->mMargin.GetTopUnit(),
                             mStyleSpacing->mMargin.GetTop(top),
                             mComputedMargin.top);
      }
      if (eStyleUnit_Inherit == mStyleSpacing->mMargin.GetBottomUnit()) {
        mComputedMargin.bottom = aContainingBlockRS->mComputedMargin.bottom;
      } else {
        ComputeVerticalValue(rs2->mComputedHeight,
                             mStyleSpacing->mMargin.GetBottomUnit(),
                             mStyleSpacing->mMargin.GetBottom(bottom),
                             mComputedMargin.bottom);
      }
    }
    else {
      // According to the CSS2 spec, margin percentages are
      // calculated with respect to the *width* of the containing
      // block, even for margin-top and margin-bottom.
      if (NS_UNCONSTRAINEDSIZE == aContainingBlockWidth) {
        mComputedMargin.top = 0;
        mComputedMargin.bottom = 0;

      } else {
        if (eStyleUnit_Inherit == mStyleSpacing->mMargin.GetTopUnit()) {
          mComputedMargin.top = aContainingBlockRS->mComputedMargin.top;
        } else {
          ComputeHorizontalValue(aContainingBlockWidth,
                                 mStyleSpacing->mMargin.GetTopUnit(),
                                 mStyleSpacing->mMargin.GetTop(top),
                                 mComputedMargin.top);
        }
        if (eStyleUnit_Inherit == mStyleSpacing->mMargin.GetBottomUnit()) {
          mComputedMargin.bottom = aContainingBlockRS->mComputedMargin.bottom;
        } else {
          ComputeHorizontalValue(aContainingBlockWidth,
                                 mStyleSpacing->mMargin.GetBottomUnit(),
                                 mStyleSpacing->mMargin.GetBottom(bottom),
                                 mComputedMargin.bottom);
        }
      }
    }
  }
}

void
nsHTMLReflowState::ComputePadding(nscoord aContainingBlockWidth,
                                  const nsHTMLReflowState* aContainingBlockRS)

{
  // If style can provide us the padding directly, then use it.
  if (!mStyleSpacing->GetPadding(mComputedPadding)) {
    // We have to compute the value
    nsStyleCoord left, right, top, bottom;

    if (eStyleUnit_Inherit == mStyleSpacing->mPadding.GetLeftUnit()) {
      mComputedPadding.left = aContainingBlockRS->mComputedPadding.left;
    } else {
      ComputeHorizontalValue(aContainingBlockWidth,
                             mStyleSpacing->mPadding.GetLeftUnit(),
                             mStyleSpacing->mPadding.GetLeft(left),
                             mComputedPadding.left);
    }
    if (eStyleUnit_Inherit == mStyleSpacing->mPadding.GetRightUnit()) {
      mComputedPadding.right = aContainingBlockRS->mComputedPadding.right;
    } else {
      ComputeHorizontalValue(aContainingBlockWidth,
                             mStyleSpacing->mPadding.GetRightUnit(),
                             mStyleSpacing->mPadding.GetRight(right),
                             mComputedPadding.right);
    }

    // According to the CSS2 spec, percentages are calculated with respect to
    // containing block width for padding-top and padding-bottom
    if (eStyleUnit_Inherit == mStyleSpacing->mPadding.GetTopUnit()) {
      mComputedPadding.top = aContainingBlockRS->mComputedPadding.top;
    } else {
      ComputeHorizontalValue(aContainingBlockWidth,
                             mStyleSpacing->mPadding.GetTopUnit(),
                             mStyleSpacing->mPadding.GetTop(top),
                             mComputedPadding.top);
    }
    if (eStyleUnit_Inherit == mStyleSpacing->mPadding.GetBottomUnit()) {
      mComputedPadding.bottom = aContainingBlockRS->mComputedPadding.bottom;
    } else {
      ComputeHorizontalValue(aContainingBlockWidth,
                             mStyleSpacing->mPadding.GetBottomUnit(),
                             mStyleSpacing->mPadding.GetBottom(bottom),
                             mComputedPadding.bottom);
    }
  }
}

void
nsHTMLReflowState::ComputeMinMaxValues(nscoord aContainingBlockWidth,
                                       nscoord aContainingBlockHeight,
                                       const nsHTMLReflowState* aContainingBlockRS)
{
  nsStyleUnit minWidthUnit = mStylePosition->mMinWidth.GetUnit();
  if (eStyleUnit_Inherit == minWidthUnit) {
    mComputedMinWidth = aContainingBlockRS->mComputedMinWidth;
  } else {
    ComputeHorizontalValue(aContainingBlockWidth, minWidthUnit,
                           mStylePosition->mMinWidth, mComputedMinWidth);
  }
  nsStyleUnit maxWidthUnit = mStylePosition->mMaxWidth.GetUnit();
  if (eStyleUnit_Inherit == maxWidthUnit) {
    mComputedMaxWidth = aContainingBlockRS->mComputedMaxWidth;
  } else if (eStyleUnit_Null == maxWidthUnit) {
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
  if (eStyleUnit_Inherit == minHeightUnit) {
    mComputedMinHeight = aContainingBlockRS->mComputedMinHeight;
  } else {
    // Check for percentage based values and a containing block height that
    // depends on the content height. Treat them like 'auto'
    if ((NS_AUTOHEIGHT == aContainingBlockHeight) &&
        (eStyleUnit_Percent == minHeightUnit)) {
      mComputedMinHeight = 0;
    } else {
      ComputeVerticalValue(aContainingBlockHeight, minHeightUnit,
                           mStylePosition->mMinHeight, mComputedMinHeight);
    }
  }
  nsStyleUnit maxHeightUnit = mStylePosition->mMaxHeight.GetUnit();
  if (eStyleUnit_Inherit == maxHeightUnit) {
    mComputedMaxHeight = aContainingBlockRS->mComputedMaxHeight;
  } else if (eStyleUnit_Null == maxHeightUnit) {
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
