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
#include "nsInlineReflow.h"
#include "nsLineLayout.h"
#include "nsIFontMetrics.h"
#include "nsIPresContext.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"

#include "nsFrameReflowState.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsStyleConsts.h"

#ifdef NS_DEBUG
#undef NOISY_VERTICAL_ALIGN
#else
#undef NOISY_VERTICAL_ALIGN
#endif

#define PLACED_LEFT  0x1
#define PLACED_RIGHT 0x2

// XXX handle DIR=right-to-left

// XXX remove support for block reflow from this and move it into its
// own class (nsBlockReflow?)

nsInlineReflow::nsInlineReflow(nsLineLayout& aLineLayout,
                               nsFrameReflowState& aOuterReflowState,
                               nsHTMLContainerFrame* aOuterFrame,
                               PRBool aOuterIsBlock)
  : mLineLayout(aLineLayout),
    mPresContext(aOuterReflowState.mPresContext),
    mOuterReflowState(aOuterReflowState),
    mComputeMaxElementSize(aOuterReflowState.mComputeMaxElementSize),
    mOuterIsBlock(aOuterIsBlock)
{
  mSpaceManager = aLineLayout.mSpaceManager;
  NS_ASSERTION(nsnull != mSpaceManager, "caller must have space manager");
  mOuterFrame = aOuterFrame;
  mFrameDataBase = mFrameDataBuf;
  mNumFrameData = sizeof(mFrameDataBuf) / sizeof(mFrameDataBuf[0]);
}

nsInlineReflow::~nsInlineReflow()
{
  if (mFrameDataBase != mFrameDataBuf) {
    delete [] mFrameDataBase;
  }
}

void
nsInlineReflow::Init(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
#ifdef NS_DEBUG
  if ((aWidth > 100000) && (aWidth != NS_UNCONSTRAINEDSIZE)) {
    mOuterFrame->ListTag(stdout);
    printf(": Init: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
    aWidth = NS_UNCONSTRAINEDSIZE;
  }
  if ((aHeight > 100000) && (aHeight != NS_UNCONSTRAINEDSIZE)) {
    mOuterFrame->ListTag(stdout);
    printf(": Init: bad caller: height WAS %d(0x%x)\n",
           aHeight, aHeight);
    aHeight = NS_UNCONSTRAINEDSIZE;
  }
#endif

  mLeftEdge = aX;
  mX = aX;
  if (NS_UNCONSTRAINEDSIZE == aWidth) {
    mRightEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mRightEdge = aX + aWidth;
  }
  mTopEdge = aY;
  if (NS_UNCONSTRAINEDSIZE == aHeight) {
    mBottomEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mBottomEdge = aY + aHeight;
  }
  mCarriedOutTopMargin = 0;
  mCarriedOutBottomMargin = 0;

  mFrameData = nsnull;
  mFrameNum = 0;
  mMaxElementSize.width = 0;
  mMaxElementSize.height = 0;
  mUpdatedBand = PR_FALSE;
  mPlacedFloaters = 0;
}

void
nsInlineReflow::UpdateBand(nscoord aX, nscoord aY,
                           nscoord aWidth, nscoord aHeight,
                           PRBool aPlacedLeftFloater)
{
  NS_PRECONDITION(mX == mLeftEdge, "update-band called after place-frame");

#ifdef NS_DEBUG
  if ((aWidth > 100000) && (aWidth != NS_UNCONSTRAINEDSIZE)) {
    mOuterFrame->ListTag(stdout);
    printf(": UpdateBand: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
    aWidth = NS_UNCONSTRAINEDSIZE;
  }
  if ((aHeight > 100000) && (aHeight != NS_UNCONSTRAINEDSIZE)) {
    mOuterFrame->ListTag(stdout);
    printf(": UpdateBand: bad caller: height WAS %d(0x%x)\n",
           aHeight, aHeight);
    aHeight = NS_UNCONSTRAINEDSIZE;
  }
#endif

  mLeftEdge = aX;
  mX = aX;
  if (NS_UNCONSTRAINEDSIZE == aWidth) {
    mRightEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mRightEdge = aX + aWidth;
  }
  mTopEdge = aY;
  if (NS_UNCONSTRAINEDSIZE == aHeight) {
    mBottomEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mBottomEdge = aY + aHeight;
  }
  mUpdatedBand = PR_TRUE;
  mPlacedFloaters |= (aPlacedLeftFloater ? PLACED_LEFT : PLACED_RIGHT);
}

void
nsInlineReflow::UpdateFrames()
{
  if (NS_STYLE_DIRECTION_LTR == mOuterReflowState.mDirection) {
    if (PLACED_LEFT & mPlacedFloaters) {
      PerFrameData* pfd = mFrameDataBase;
      PerFrameData* end = pfd + mFrameNum;
      for (; pfd < end; pfd++) {
        pfd->mBounds.x = mX;
      }
    }
  }
  else if (PLACED_RIGHT & mPlacedFloaters) {
    // XXX handle DIR=right-to-left
  }
}

const nsStyleDisplay*
nsInlineReflow::GetDisplay()
{
  if (nsnull != mDisplay) {
    return mDisplay;
  }
  mFrameData->mFrame->GetStyleData(eStyleStruct_Display,
                                   (const nsStyleStruct*&)mDisplay);
  return mDisplay;
}

const nsStylePosition*
nsInlineReflow::GetPosition()
{
  if (nsnull != mPosition) {
    return mPosition;
  }
  mFrameData->mFrame->GetStyleData(eStyleStruct_Position,
                                   (const nsStyleStruct*&)mPosition);
  return mPosition;
}

const nsStyleSpacing*
nsInlineReflow::GetSpacing()
{
  if (nsnull != mSpacing) {
    return mSpacing;
  }
  mFrameData->mFrame->GetStyleData(eStyleStruct_Spacing,
                                   (const nsStyleStruct*&)mSpacing);
  return mSpacing;
}

// XXX use frameType instead after constructing a reflow state
PRBool
nsInlineReflow::TreatFrameAsBlockFrame()
{
  const nsStyleDisplay* display = GetDisplay();
  const nsStylePosition* position = GetPosition();

  if (NS_STYLE_POSITION_ABSOLUTE == position->mPosition) {
    return PR_FALSE;
  }
  if (NS_STYLE_FLOAT_NONE != display->mFloats) {
    return PR_FALSE;
  }
  switch (display->mDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
  case NS_STYLE_DISPLAY_COMPACT:
  case NS_STYLE_DISPLAY_RUN_IN:
  case NS_STYLE_DISPLAY_TABLE:
    return PR_TRUE;
  default:
    break;
  }
  return PR_FALSE;
}

nsresult
nsInlineReflow::SetFrame(nsIFrame* aFrame)
{
  // Make sure we have a PerFrameData for this frame
  PRInt32 frameNum = mFrameNum;
  if (frameNum == mNumFrameData) {
    mNumFrameData *= 2;
    PerFrameData* newData = new PerFrameData[mNumFrameData];
    if (nsnull == newData) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsCRT::memcpy(newData, mFrameDataBase, sizeof(PerFrameData) * frameNum);
    if (mFrameDataBase != mFrameDataBuf) {
      delete [] mFrameDataBase;
    }
    mFrameDataBase = newData;
  }
  mFrameData = mFrameDataBase + mFrameNum;

  // We can break before the frame if we placed at least one frame on
  // the line.
  mCanBreakBeforeFrame = mLineLayout.GetPlacedFrames() > 0;

  mFrameData->mFrame = aFrame;
  mDisplay = nsnull;
  mSpacing = nsnull;
  mPosition = nsnull;

  return NS_OK;
}

nsresult
nsInlineReflow::ReflowFrame(nsIFrame* aFrame, nsReflowStatus& aReflowStatus)
{
  nsSize innerMaxElementSize;
  nsHTMLReflowMetrics metrics(mComputeMaxElementSize
                              ? &innerMaxElementSize
                              : nsnull);

  // Prepare for reflowing the frame
  nsresult rv = SetFrame(aFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Next figure out how much available space there is for the frame.
  // Calculate raw margin values.
  CalculateMargins();

  // Apply top+left margins (as appropriate) to the frame computing
  // the new starting x,y coordinates for the frame.
  ApplyTopLeftMargins();
  
  // Compute the available area to reflow the frame into.
  if (!ComputeAvailableSize()) {
    aReflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
    return NS_OK;
  }

  // Reflow the frame. If the frame must be placed somewhere else
  // then we return immediately.
  if (ReflowFrame(metrics, aReflowStatus)) {
    // See if we can place the frame. If we can't fit it, then we
    // return now.
    if (CanPlaceFrame(metrics, aReflowStatus)) {
      // Place the frame, updating aBounds with the final size and
      // location.  Then apply the bottom+right margins (as
      // appropriate) to the frame.
      PlaceFrame(metrics);
    }
  }

  return rv;
}

void
nsInlineReflow::CalculateMargins()
{
  // Get the margins from the style system
  PerFrameData* pfd = mFrameData;
  nsHTMLReflowState::ComputeMarginFor(pfd->mFrame, &mOuterReflowState,
                                      pfd->mMargin);
}

PRUintn
nsInlineReflow::CalculateBlockMarginsFor(nsIPresContext& aPresContext,
                                         nsIFrame* aFrame,
                                         const nsHTMLReflowState* aParentRS,
                                         const nsStyleSpacing* aSpacing,
                                         nsMargin& aMargin)
{
  PRUint32 rv = 0;

  nsHTMLReflowState::ComputeMarginFor(aFrame, aParentRS, aMargin);

  // Get font height if we will be doing an auto margin. We use the
  // default font height for the auto margin value.
  nsStyleUnit topUnit = aSpacing->mMargin.GetTopUnit();
  nsStyleUnit bottomUnit = aSpacing->mMargin.GetBottomUnit();
  nscoord fontHeight = 0;
  if ((eStyleUnit_Auto == topUnit) || (eStyleUnit_Auto == bottomUnit)) {
    // XXX Use the font for the frame, not the default font???
    const nsFont& defaultFont = aPresContext.GetDefaultFont();
    nsIFontMetrics* fm = aPresContext.GetMetricsFor(defaultFont);
    fm->GetHeight(fontHeight);
    NS_RELEASE(fm);
  }

  // For auto margins use the font height computed above
  if (eStyleUnit_Auto == topUnit) {
    aMargin.top = fontHeight;
    rv |= NS_CARRIED_TOP_MARGIN_IS_AUTO;
  }
  if (eStyleUnit_Auto == bottomUnit) {
    aMargin.bottom = fontHeight;
    rv |= NS_CARRIED_BOTTOM_MARGIN_IS_AUTO;
  }
  return rv;
}

// XXX doesn't apply top margin

void
nsInlineReflow::ApplyTopLeftMargins()
{
  PerFrameData* pfd = mFrameData;

  // If this is the first frame of a block, and its the first line of
  // a block then see if the text-indent property amounts to anything.
  nscoord indent = 0;
  if (mOuterIsBlock &&
      (0 == mLineLayout.GetLineNumber()) &&
      (0 == mFrameNum)) {
    const nsStyleText* text;
    mOuterFrame->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&) text);
    nsStyleUnit unit = text->mTextIndent.GetUnit();
    if (eStyleUnit_Coord == unit) {
      indent = text->mTextIndent.GetCoordValue();
    }
    else if (eStyleUnit_Percent == unit) {
      nscoord width =
        nsHTMLReflowState::GetContainingBlockContentWidth(mOuterReflowState.parentReflowState);
      if (0 != width) {
        indent = nscoord(text->mTextIndent.GetPercentValue() * width);
      }
    }
  }

  pfd->mBounds.x = mX + indent;
  pfd->mBounds.y = mTopEdge;

  // Compute left margin
  nsIFrame* prevInFlow;
  const nsStyleDisplay* display = GetDisplay();
  switch (display->mFloats) {
  default:
    NS_NOTYETIMPLEMENTED("Unsupported floater type");
    // FALL THROUGH

  case NS_STYLE_FLOAT_LEFT:
  case NS_STYLE_FLOAT_RIGHT:
    // When something is floated, its margins are applied there
    // not here.
    break;

  case NS_STYLE_FLOAT_NONE:
    // Only apply left-margin on the first-in flow for inline frames
    pfd->mFrame->GetPrevInFlow(prevInFlow);
    if (nsnull != prevInFlow) {
      // Zero this out so that when we compute the max-element-size
      // of the frame we will properly avoid adding in the left
      // margin.
      pfd->mMargin.left = 0;
    }
    pfd->mBounds.x += pfd->mMargin.left;
    break;
  }
}

PRBool
nsInlineReflow::ComputeAvailableSize()
{
  PerFrameData* pfd = mFrameData;

  // Compute the available size from the outer's perspective
  if (NS_UNCONSTRAINEDSIZE == mRightEdge) {
    mFrameAvailSize.width = NS_UNCONSTRAINEDSIZE;
  }
  else {
    // XXX What does a negative right margin mean here?
    mFrameAvailSize.width = mRightEdge - pfd->mBounds.x - pfd->mMargin.right;
  }
  if (NS_UNCONSTRAINEDSIZE == mBottomEdge) {
    mFrameAvailSize.height = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mFrameAvailSize.height = mBottomEdge - pfd->mBounds.y -
      pfd->mMargin.bottom;
  }
  if (mOuterReflowState.mNoWrap) {
    mFrameAvailSize.width = mOuterReflowState.maxSize.width;
    return PR_TRUE;
  }
#if 0
  // Give up now if there is no chance. Note that we allow a reflow if
  // the available space is zero because that way things that end up
  // zero sized won't trigger a new line to be created. We also allow
  // a reflow if we can't break before this frame.
  mInWord = mLineLayout.InWord();
  if (!mInWord && mCanBreakBeforeFrame &&
      ((mFrameAvailSize.width < 0) || (mFrameAvailSize.height < 0))) {
    return PR_FALSE;
  }
#endif
  return PR_TRUE;
}

/**
 * Reflow the frame, choosing the appropriate reflow method.
 */
PRBool
nsInlineReflow::ReflowFrame(nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus)
{
  PerFrameData* pfd = mFrameData;

  // Get reflow reason set correctly. It's possible that a child was
  // created and then it was decided that it could not be reflowed
  // (for example, a block frame that isn't at the start of a
  // line). In this case the reason will be wrong so we need to check
  // the frame state.
  nsReflowReason reason = eReflowReason_Resize;
  nsIFrame* frame = pfd->mFrame;
  nsFrameState state;
  frame->GetFrameState(state);
  if (NS_FRAME_FIRST_REFLOW & state) {
    reason = eReflowReason_Initial;
  }
  else if (mOuterReflowState.mNextRCFrame == frame) {
    reason = eReflowReason_Incremental;
    // Make sure we only incrementally reflow once
    mOuterReflowState.mNextRCFrame = nsnull;
  }

  // Setup reflow state for reflowing the frame
  nsHTMLReflowState reflowState(mPresContext, frame, mOuterReflowState,
                                mFrameAvailSize);
  reflowState.lineLayout = &mLineLayout;
  reflowState.reason = reason;
  mLineLayout.SetUnderstandsWhiteSpace(PR_FALSE);

  // Let frame know that are reflowing it
  nscoord x = pfd->mBounds.x;
  nscoord y = pfd->mBounds.y;
  nsIHTMLReflow* htmlReflow;

  frame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  htmlReflow->WillReflow(mPresContext);

  // Adjust spacemanager coordinate system for the frame. The
  // spacemanager coordinates are <b>inside</b> the mOuterFrame's
  // border+padding, but the x/y coordinates are not (recall that
  // frame coordinates are relative to the parents origin and that the
  // parents border/padding is <b>inside</b> the parent
  // frame. Therefore we have to subtract out the parents
  // border+padding before translating.
  nscoord tx = x - mOuterReflowState.mBorderPadding.left;
  nscoord ty = y - mOuterReflowState.mBorderPadding.top;
  mSpaceManager->Translate(tx, ty);

  frame->MoveTo(x, y);
  htmlReflow->Reflow(mPresContext, aMetrics, reflowState, aStatus);
  // XXX could return this in aStatus to save a few cycles
  // XXX could mandate that child sets up mCombinedArea too...
  frame->GetFrameState(state);
  if (NS_FRAME_OUTSIDE_CHILDREN & state) {
    pfd->mCombinedArea = aMetrics.mCombinedArea;
  }
  else {
    pfd->mCombinedArea.x = 0;
    pfd->mCombinedArea.y = 0;
    pfd->mCombinedArea.width = aMetrics.width;
    pfd->mCombinedArea.height = aMetrics.height;
  }
  pfd->mBounds.width = aMetrics.width;
  pfd->mBounds.height = aMetrics.height;

  mSpaceManager->Translate(-tx, -ty);

  // Now that frame has been reflowed at least one time make sure that
  // the NS_FRAME_FIRST_REFLOW bit is cleared so that never give it an
  // initial reflow reason again.
  if (eReflowReason_Initial == reason) {
    frame->GetFrameState(state);
    frame->SetFrameState(state & ~NS_FRAME_FIRST_REFLOW);
  }

  if (!NS_INLINE_IS_BREAK_BEFORE(aStatus)) {
    // If frame is complete and has a next-in-flow, we need to delete
    // them now. Do not do this when a break-before is signaled because
    // the frame is going to get reflowed again (and may end up wanting
    // a next-in-flow where it ends up).
    if (NS_FRAME_IS_COMPLETE(aStatus)) {
      nsIFrame* kidNextInFlow;
      frame->GetNextInFlow(kidNextInFlow);
      if (nsnull != kidNextInFlow) {
        // Remove all of the childs next-in-flows. Make sure that we ask
        // the right parent to do the removal (it's possible that the
        // parent is not this because we are executing pullup code)
        nsHTMLContainerFrame* parent;
        frame->GetGeometricParent((nsIFrame*&) parent);
        parent->DeleteChildsNextInFlow(mPresContext, frame);
      }
    }
  }

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsInlineReflow::ReflowFrame: frame=%p reflowStatus=%x %s",
      frame, aStatus,
      mLineLayout.GetUnderstandsWhiteSpace() ? "UWS" : "!UWS"));

  return !NS_INLINE_IS_BREAK_BEFORE(aStatus);
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
PRBool
nsInlineReflow::CanPlaceFrame(nsHTMLReflowMetrics& aMetrics,
                              nsReflowStatus& aStatus)
{
  PerFrameData* pfd = mFrameData;

  // Compute right margin to use
  mRightMargin = 0;
  if (0 != pfd->mBounds.width) {
    const nsStyleDisplay* display = GetDisplay();
    switch (display->mFloats) {
    default:
      NS_NOTYETIMPLEMENTED("Unsupported floater type");
      // FALL THROUGH

    case NS_STYLE_FLOAT_LEFT:
    case NS_STYLE_FLOAT_RIGHT:
      // When something is floated, its margins are applied there
      // not here.
      break;

    case NS_STYLE_FLOAT_NONE:
      // Only apply right margin for the last-in-flow
      if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
        // Zero this out so that when we compute the
        // max-element-size of the frame we will properly avoid
        // adding in the right margin.
        pfd->mMargin.right = 0;
      }
      mRightMargin = pfd->mMargin.right;
      break;
    }
  }

  // If this is the first frame going into this inline reflow or it's
  // the first placed frame in the line or wrapping is disabled then
  // the frame fits regardless of who much room there is. This
  // guarantees that we always make progress regardless of the
  // limitations of the reflow area. If the container reflowing this
  // frame ends up too big then the container may be pushable to a new
  // location.
  if ((0 == mFrameNum) ||
      (0 == mLineLayout.GetPlacedFrames()) ||
      mOuterReflowState.mNoWrap ||
      mInWord ||
      (0 == pfd->mMargin.left + pfd->mBounds.width + pfd->mMargin.right)) {
    return PR_TRUE;
  }

  // See if the frame fits. If it doesn't then we fabricate up a
  // break-before status.
  if (pfd->mBounds.XMost() + mRightMargin > mRightEdge) {
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
    return PR_FALSE;
  }

  return PR_TRUE;
}

/**
 * Place the frame. Update running counters.
 */
void
nsInlineReflow::PlaceFrame(nsHTMLReflowMetrics& aMetrics)
{
  PerFrameData* pfd = mFrameData;

//XXX disabled: css2 spec claims otherwise; the problem is we need to
//handle continuations differently so that empty continuations disappear

  // If frame is zero width then do not apply its left and right margins.
  PRBool emptyFrame = PR_FALSE;
  if ((0 == pfd->mBounds.width) && (0 == pfd->mBounds.height)) {
    pfd->mBounds.x = mX;
    pfd->mBounds.y = mTopEdge;
    emptyFrame = PR_TRUE;
  }

  // Record ascent and update max-ascent and max-descent values
  pfd->mAscent = aMetrics.ascent;
  pfd->mDescent = aMetrics.descent;
  mFrameNum++;

  // If the band was updated during the reflow of that frame then we
  // need to adjust any prior frames that were reflowed.
  if (mUpdatedBand && mOuterIsBlock) {
    UpdateFrames();
  }
  mUpdatedBand = PR_FALSE;

  // Save carried out information for caller
  mCarriedOutTopMargin = aMetrics.mCarriedOutTopMargin;
  mCarriedOutBottomMargin = aMetrics.mCarriedOutBottomMargin;

  // Advance to next X coordinate
  mX = pfd->mBounds.XMost() + mRightMargin;

  // If the frame is a not aware of white-space and it takes up some
  // area, disable leading white-space compression for the next frame
  // to be reflowed.
  if (!mLineLayout.GetUnderstandsWhiteSpace() && !emptyFrame) {
    mLineLayout.SetEndsInWhiteSpace(PR_FALSE);
  }

  // Compute the bottom margin to apply. Note that the margin only
  // applies if the frame ends up with a non-zero height.
  if (!emptyFrame) {
    // Inform line layout that we have placed a non-empty frame
    mLineLayout.AddPlacedFrame(mFrameData->mFrame);

    // Update max-element-size
    if (mComputeMaxElementSize) {
      // The max-element width is the sum of the interior max-element
      // width plus the left and right margins that are applied to the
      // frame.
      nscoord mw = aMetrics.maxElementSize->width +
        pfd->mMargin.left + pfd->mMargin.right;
      if (mw > mMaxElementSize.width) {
        mMaxElementSize.width = mw;
      }

      // XXX take into account top/bottom margins
      nscoord mh = aMetrics.maxElementSize->height;
      if (mh > mMaxElementSize.height) {
        mMaxElementSize.height = mh;
      }
    }
  }
}

// XXX what about ebina's center vs. ncsa-center?

void
nsInlineReflow::VerticalAlignFrames(nsRect& aLineBox,
                                    nscoord& aMaxAscent,
                                    nscoord& aMaxDescent)
{
  PerFrameData* pfd0 = mFrameDataBase;
  PerFrameData* end = pfd0 + mFrameNum;

  // XXX I think that the line box should wrap around the children,
  // even if they have negative margins, right?
  aLineBox.x = mLeftEdge;
  aLineBox.y = mTopEdge;
  aLineBox.width = mX - mLeftEdge;

  // Get the parent elements font in case we need it
  const nsStyleFont* font;
  mOuterFrame->GetStyleData(eStyleStruct_Font,
                            (const nsStyleStruct*&)font);
  nsIFontMetrics* fm = mPresContext.GetMetricsFor(font->mFont);

#ifdef NOISY_VERTICAL_ALIGN
  mOuterFrame->ListTag(stdout);
  printf(": valign frames (count=%d, line#%d)\n",
         mFrameNum, mLineLayout.GetLineNumber());
#endif

  // Examine each and determine the minYTop, the maxYBottom and the
  // maximum height. We will use these values to determine the final
  // height of the line box and then position each frame.
  nscoord minYTop = 0;
  nscoord maxYBottom = 0;
  nscoord maxTopHeight = 0;
  nscoord maxBottomHeight = 0;
  PerFrameData* pfd;
  for (pfd = pfd0; pfd < end; pfd++) {
    PRUint8 verticalAlignEnum;
    nscoord fontParam;

    nsIFrame* frame = pfd->mFrame;

    // yTop = Y coordinate for the top of frame box <B>relative</B> to
    // the baseline of the linebox which is assumed to be at Y=0
    nscoord yTop;
    nscoord height = pfd->mBounds.height + pfd->mMargin.top +
      pfd->mMargin.bottom;
    pfd->mAscent += pfd->mMargin.top;

    const nsStyleText* textStyle;
    frame->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textStyle);
    nsStyleUnit verticalAlignUnit = textStyle->mVerticalAlign.GetUnit();

    switch (verticalAlignUnit) {
    case eStyleUnit_Enumerated:
      verticalAlignEnum = textStyle->mVerticalAlign.GetIntValue();
      switch (verticalAlignEnum) {
      default:
      case NS_STYLE_VERTICAL_ALIGN_BASELINE:
        yTop = -pfd->mAscent;
        break;
      case NS_STYLE_VERTICAL_ALIGN_SUB:
        // Align the frames baseline on the subscript baseline
        fm->GetSubscriptOffset(fontParam);
        yTop = fontParam - pfd->mAscent;
        break;
      case NS_STYLE_VERTICAL_ALIGN_SUPER:
        // Align the frames baseline on the superscript baseline
        fm->GetSuperscriptOffset(fontParam);
        yTop = -fontParam - pfd->mAscent;
        break;
      case NS_STYLE_VERTICAL_ALIGN_TOP:
        if (height > maxTopHeight) {
          maxTopHeight = height;
        }
        continue;
      case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
        if (height > maxBottomHeight) {
          maxBottomHeight = height;
        }
        continue;
      case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
        // Align the midpoint of the frame with 1/2 the parents x-height
        fm->GetXHeight(fontParam);
        yTop = -(fontParam / 2) - (pfd->mBounds.height/2);
        break;
      case NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM:
        fm->GetMaxDescent(fontParam);
        yTop = fontParam - pfd->mBounds.height;
        break;
      case NS_STYLE_VERTICAL_ALIGN_TEXT_TOP:
        fm->GetMaxAscent(fontParam);
        yTop = -fontParam;
        break;
      }
      break;
    case eStyleUnit_Coord:
      // According to the CSS2 spec (10.8.1), a positive value
      // "raises" the box by the given distance while a negative value
      // "lowers" the box by the given distance. Since Y coordinates
      // increase towards the bottom of the screen we reverse the
      // sign. All of the raising and lowering is done relative to the
      // baseline, so we start our adjustments there.
      yTop = -pfd->mAscent - textStyle->mVerticalAlign.GetCoordValue();
      break;
    case eStyleUnit_Percent:
      // The percentage is relative to the line-height of the element
      // itself. The line-height will be the final height of the
      // inline element (CSS2 10.8.1 says that the line-height defines
      // the precise height of inline non-replaced elements).
      yTop = -pfd->mAscent -
        nscoord(textStyle->mVerticalAlign.GetPercentValue() * pfd->mBounds.height);
      break;
    default:
      yTop = -pfd->mAscent;
      break;
    }
    pfd->mBounds.y = yTop;

#ifdef NOISY_VERTICAL_ALIGN
    printf("  ");
    nsAutoString tmp;
    pfd->mFrame->GetFrameName(tmp);
    fputs(tmp, stdout);
    printf(": yTop=%d minYTop=%d yBottom=%d maxYBottom=%d\n",
           yTop, minYTop, yTop + height, maxYBottom);
    NS_ASSERTION(yTop >= -1000000, "bad yTop");
#endif

    if (yTop < minYTop) {
      minYTop = yTop;
    }
    // yBottom = Y coordinate for the bottom of the frame box, again
    // relative to the baseline where Y=0
    nscoord yBottom = yTop + height;
    if (yBottom > maxYBottom) {
      maxYBottom = yBottom;
    }
  }

  // Once we have finished the above abs(minYTop) represents the
  // maximum ascent of the line box. "CSS2 spec section 10.8: the line
  // box height is the distance between the uppermost box top
  // (minYTop) and the lowermost box bottom (maxYBottom)."
  nscoord lineHeight = maxYBottom - minYTop;
  nscoord maxAscent = -minYTop;
#ifdef NOISY_VERTICAL_ALIGN
  printf("  lineHeight=%d maxAscent=%d\n", lineHeight, maxAscent);
#endif
  if (lineHeight < maxTopHeight) {
    // If the line height ends up shorter than the tallest top aligned
    // box then the line height must grow but the line's ascent need
    // not be changed.
    lineHeight = maxTopHeight;
#ifdef NOISY_VERTICAL_ALIGN
    printf("  *lineHeight=maxTopHeight=%d\n", lineHeight);
#endif
  }
  if (lineHeight < maxBottomHeight) {
    // If the line height ends up shorter than the tallest bottom
    // aligned box then the line height must grow and the line's
    // ascent needs to be adjusted (so that the baseline aligned
    // objects move downward).
    nscoord dy = maxBottomHeight - lineHeight;
    lineHeight = maxBottomHeight;
    maxAscent += dy;
#ifdef NOISY_VERTICAL_ALIGN
    printf("  *lineHeight=maxBottomHeight=%d dy=%d\n", lineHeight, dy);
#endif
  }

  nscoord topEdge = mTopEdge;
  if (0 != lineHeight) {
    nscoord newLineHeight = CalcLineHeightFor(mPresContext, mOuterFrame,
                                              lineHeight);
    // If the newLineHeight is larger then just use; otherwise if the
    // outer frame is an inline frame then use it as well (line-height
    // on block frames specify the minimal height while on inline
    // frames it specifies the precise height).
    if (mOuterIsBlock) {/* XXX temporary until line-height inheritance issue is resolved */
    if ((newLineHeight > lineHeight) || !mOuterIsBlock) {
      topEdge += (newLineHeight - lineHeight) / 2;
      lineHeight = newLineHeight;
#ifdef NOISY_VERTICAL_ALIGN
      printf("  *lineHeight=newLineHeight=%d topEdgeDelta=%d\n",
             lineHeight, topEdge - mTopEdge);
#endif
    }
    }
  }
  aLineBox.height = lineHeight;

  // Pass2 - position each of the frames
  for (pfd = pfd0; pfd < end; pfd++) {
    nsIFrame* frame = pfd->mFrame;
    const nsStyleText* textStyle;
    frame->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textStyle);
    nsStyleUnit verticalAlignUnit = textStyle->mVerticalAlign.GetUnit();
    if (eStyleUnit_Enumerated == verticalAlignUnit) {
      PRUint8 verticalAlignEnum = textStyle->mVerticalAlign.GetIntValue();
      switch (verticalAlignEnum) {
      case NS_STYLE_VERTICAL_ALIGN_TOP:
        // XXX negative top margins on these will do weird things, maybe?
        pfd->mBounds.y = mTopEdge + pfd->mMargin.top;
        break;
      case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
        pfd->mBounds.y = mTopEdge + lineHeight - pfd->mBounds.height;
        break;
      default:
        pfd->mBounds.y = topEdge + maxAscent + pfd->mBounds.y +
          pfd->mMargin.top;
        break;
      }
    }
    else {
      pfd->mBounds.y = topEdge + maxAscent + pfd->mBounds.y +
        pfd->mMargin.top;
    }
    frame->SetRect(pfd->mBounds);
  }
  aMaxAscent = maxAscent;
  aMaxDescent = lineHeight - maxAscent;

  NS_RELEASE(fm);
}

void
nsInlineReflow::HorizontalAlignFrames(nsRect& aLineBox, PRBool aIsLastLine)
{
  // Before we start, trim any trailing whitespace off of the last
  // frame in the line.
  nscoord deltaWidth;
  PerFrameData* pfd = mFrameDataBase + (mFrameNum - 1);
  if (pfd->mBounds.width > 0) {
    nsIFrame* frame = pfd->mFrame;
    nsIHTMLReflow* ihr;
    if (NS_OK == frame->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
      ihr->TrimTrailingWhiteSpace(mPresContext,
                                  *mOuterReflowState.rendContext,
                                  deltaWidth);
      aLineBox.width -= deltaWidth;
      pfd->mBounds.width -= deltaWidth;
    }
  }

  const nsStyleText* styleText = mOuterReflowState.mStyleText;
  nscoord maxWidth = mRightEdge - mLeftEdge;
  if (aLineBox.width < maxWidth) {
    nscoord dx = 0;
    switch (styleText->mTextAlign) {
    case NS_STYLE_TEXT_ALIGN_DEFAULT:
      if (NS_STYLE_DIRECTION_LTR == mOuterReflowState.mDirection) {
        // default alignment for left-to-right is left so do nothing
        return;
      }
      // Fall through to align right case for default alignment
      // used when the direction is right-to-left.

    case NS_STYLE_TEXT_ALIGN_RIGHT:
      dx = maxWidth - aLineBox.width;
      break;

    case NS_STYLE_TEXT_ALIGN_LEFT:
      return;

    case NS_STYLE_TEXT_ALIGN_JUSTIFY:
      // If this is not the last line then go ahead and justify the
      // frames in the line. If it is the last line then if the
      // direction is right-to-left then we right-align the frames.
      if (!aIsLastLine) {
        JustifyFrames(maxWidth, aLineBox);
        return;
      }
      else if (NS_STYLE_DIRECTION_RTL == mOuterReflowState.mDirection) {
        // right align the frames
        dx = maxWidth - aLineBox.width;
      }
      break;

    case NS_STYLE_TEXT_ALIGN_CENTER:
      dx = (maxWidth - aLineBox.width) / 2;
      break;
    }

    if (0 != dx) {
      // Position children
      pfd = mFrameDataBase;
      PerFrameData* end = pfd + mFrameNum;
      nsPoint origin;
      for (; pfd < end; pfd++) {
        nsIFrame* kid = pfd->mFrame;;
        kid->GetOrigin(origin);
        kid->MoveTo(origin.x + dx, origin.y);
        kid->GetNextSibling(kid);
      }
    }
  }
}

void
nsInlineReflow::RelativePositionFrames(nsRect& aCombinedArea)
{
  nscoord x0 = mLeftEdge, y0 = mTopEdge;
  nscoord x1 = x0, y1 = y0;
  nsPoint origin;
  PerFrameData* pfd = mFrameDataBase;
  PerFrameData* end = pfd + mFrameNum;
  for (; pfd < end; pfd++) {
    nscoord x = pfd->mCombinedArea.x + pfd->mBounds.x;
    nscoord y = pfd->mCombinedArea.y + pfd->mBounds.y;

    nsIFrame* kid = pfd->mFrame;
    const nsStylePosition* kidPosition;
    kid->GetStyleData(eStyleStruct_Position,
                      (const nsStyleStruct*&)kidPosition);
    if (NS_STYLE_POSITION_RELATIVE == kidPosition->mPosition) {
      kid->GetOrigin(origin);
      nscoord dx = 0;
      switch (kidPosition->mLeftOffset.GetUnit()) {
      case eStyleUnit_Percent:
        printf("XXX: not yet implemented: % relative position\n");
      case eStyleUnit_Auto:
        break;
      case eStyleUnit_Coord:
        dx = kidPosition->mLeftOffset.GetCoordValue();
        break;
      }
      nscoord dy = 0;
      switch (kidPosition->mTopOffset.GetUnit()) {
      case eStyleUnit_Percent:
        printf("XXX: not yet implemented: % relative position\n");
      case eStyleUnit_Auto:
        break;
      case eStyleUnit_Coord:
        dy = kidPosition->mTopOffset.GetCoordValue();
        break;
      }
      kid->MoveTo(origin.x + dx, origin.y + dy);
      x += dx;
      y += dy;
    }

    // Compute min and max x/y values for the reflowed frame's
    // combined areas
    nscoord xmost = x + pfd->mCombinedArea.width;
    nscoord ymost = y + pfd->mCombinedArea.height;
    if (x < x0) x0 = x;
    if (xmost > x1) x1 = xmost;
    if (y < y0) y0 = y;
    if (ymost > y1) y1 = ymost;
  }

  aCombinedArea.x = x0;
  aCombinedArea.y = y0;
  aCombinedArea.width = x1 - x0;
  aCombinedArea.height = y1 - y0;
}

// XXX performance todo: this computation can be cached,
// but not in the style-context
nscoord
nsInlineReflow::CalcLineHeightFor(nsIPresContext& aPresContext,
                                  nsIFrame* aFrame,
                                  nscoord aBaseLineHeight)
{
  nscoord lineHeight = aBaseLineHeight;

  nsIStyleContext* sc;
  aFrame->GetStyleContext(sc);
  const nsStyleFont* elementFont = nsnull;
  if (nsnull != sc) {
    elementFont = (const nsStyleFont*)sc->GetStyleData(eStyleStruct_Font);
    for (;;) {
      const nsStyleText* text = (const nsStyleText*)
        sc->GetStyleData(eStyleStruct_Text);
      if (nsnull != text) {
        nsStyleUnit unit = text->mLineHeight.GetUnit();
#ifdef NOISY_VERTICAL_ALIGN
        printf("  styleUnit=%d\n", unit);
#endif
        if (eStyleUnit_Enumerated == unit) {
          // Normal value; we use 1.0 for normal
          // XXX could come from somewhere else
          break;
        } else if (eStyleUnit_Factor == unit) {
          if (nsnull != elementFont) {
            // CSS2 spec says that the number is inherited, not the
            // computed value. Therefore use the font size of the
            // element times the inherited number.
            nscoord size = elementFont->mFont.size;
            lineHeight = nscoord(size * text->mLineHeight.GetFactorValue());
          }
          break;
        }
        else if (eStyleUnit_Coord == unit) {
          lineHeight = text->mLineHeight.GetCoordValue();
          // CSS2 spec 10.8.1: negative length values are illegal.
          if (lineHeight < 0) {
            lineHeight = aBaseLineHeight;
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
nsInlineReflow::JustifyFrames(nscoord aMaxWidth, nsRect& aLineBox)
{
  // Gather up raw data for justification
  PRInt32 i, n = mFrameNum;
  PerFrameData* pfd = mFrameDataBase;
  PRInt32 fixed = 0;
  nscoord fixedWidth = 0;
  for (i = 0; i < n; i++, pfd++) {
    nsIFrame* frame = pfd->mFrame;
    nsSplittableType isSplittable = NS_FRAME_NOT_SPLITTABLE;
    frame->IsSplittable(isSplittable);
    if ((0 == pfd->mBounds.width) ||
        NS_FRAME_IS_NOT_SPLITTABLE(isSplittable)) {
      pfd->mSplittable = PR_FALSE;
      fixed++;
      fixedWidth += pfd->mBounds.width;
    }
    else {
      pfd->mSplittable = PR_TRUE;
    }
  }

  nscoord variableWidth = aLineBox.width - fixedWidth;
  if (variableWidth > 0) {
    // Each variable width frame gets a portion of the available extra
    // space that is proportional to the space it takes in the
    // line. The extra space is given to the frame by updating its
    // position and size. The frame is responsible for adjusting the
    // position of its contents on its own (during rendering).
    PRInt32 splittable = n - fixed;
    nscoord extraSpace = aMaxWidth - aLineBox.width;
    nscoord remainingExtra = extraSpace;
    nscoord dx = 0;
    float lineWidth = float(aLineBox.width);
    pfd = mFrameDataBase;
    for (i = 0; i < n; i++, pfd++) {
      nsRect r;
      nsIFrame* frame = pfd->mFrame;
      nsIHTMLReflow* ihr;
      if (NS_OK == frame->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
        if (pfd->mSplittable && (pfd->mBounds.width > 0)) {
          float pctOfLine = float(pfd->mBounds.width) / lineWidth;
          nscoord extra = nscoord(pctOfLine * extraSpace);
          if (--splittable == 0) {
            extra = remainingExtra;
          }
          if (0 != extra) {
            nscoord used;
            ihr->AdjustFrameSize(extra, used);
            frame->GetRect(r);
            r.x += dx;
            frame->SetRect(r);
            dx += extra;
          }
          else if (0 != dx) {
            frame->GetRect(r);
            r.x += dx;
            frame->SetRect(r);
          }
          remainingExtra -= extra;
        }
        else if (0 != dx) {
          frame->GetRect(r);
          r.x += dx;
          frame->SetRect(r);
        }
      }
    }
  }
}
