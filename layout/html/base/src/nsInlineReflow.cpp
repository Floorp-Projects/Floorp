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

// XXX We could support "arbitrary" negative margins if we detected
// frames falling outside the parent frame and wrap them in a view
// when it happens.

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

  mIsBlock = PR_FALSE;
  mIsFirstChild = PR_FALSE;
  mFrameData = nsnull;
  mFrameNum = 0;
  mMaxElementSize.width = 0;
  mMaxElementSize.height = 0;
  mUpdatedBand = PR_FALSE;
  mPlacedLeftFloater = PR_FALSE;
  mTreatFrameAsBlock = PR_FALSE;
}

void
nsInlineReflow::UpdateBand(nscoord aX, nscoord aY,
                           nscoord aWidth, nscoord aHeight,
                           PRBool aPlacedLeftFloater)
{
  NS_PRECONDITION(mX == mLeftEdge, "update-band called after place-frame");

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
  mPlacedLeftFloater = aPlacedLeftFloater;
}

void
nsInlineReflow::UpdateFrames()
{
  if (NS_STYLE_DIRECTION_LTR == mOuterReflowState.mDirection) {
    if (mPlacedLeftFloater) {
      PerFrameData* pfd = mFrameDataBase;
      PerFrameData* end = pfd + mFrameNum;
      for (; pfd < end; pfd++) {
        pfd->mBounds.x = mX;
      }
    }
  }
  else if (!mPlacedLeftFloater) {
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
  mTreatFrameAsBlock = TreatFrameAsBlockFrame();
  mIsInlineAware = PR_FALSE;

  return NS_OK;
}

nsReflowStatus
nsInlineReflow::ReflowFrame(nsIFrame* aFrame)
{
  nsReflowStatus result;
  nsSize innerMaxElementSize;
  nsHTMLReflowMetrics metrics(mComputeMaxElementSize
                              ? &innerMaxElementSize
                              : nsnull);

  // Prepare for reflowing the frame
  nsresult rv = SetFrame(aFrame);
  if (NS_OK != rv) {
    return rv;
  }

  // Do a quick check and see if we are trying to place a block on a
  // line that already has a placed frame on it.
  PRInt32 placedFrames = mLineLayout.GetPlacedFrames();
  if (mTreatFrameAsBlock && (placedFrames > 0)) {
    return NS_INLINE_LINE_BREAK_BEFORE();
  }

  // Next figure out how much available space there is for the frame.
  // Calculate raw margin values.
  CalculateMargins();

  // Apply top+left margins (as appropriate) to the frame computing
  // the new starting x,y coordinates for the frame.
  ApplyTopLeftMargins();
  
  // Compute the available area to reflow the frame into.
  if (!ComputeAvailableSize()) {
    return NS_INLINE_LINE_BREAK_BEFORE();
  }

  // Reflow the frame. If the frame must be placed somewhere else
  // then we return immediately.
  nsRect bounds;
  if (ReflowFrame(metrics, bounds, result)) {
    // See if we can place the frame. If we can't fit it, then we
    // return now.
    if (CanPlaceFrame(metrics, bounds, result)) {
      // Place the frame, updating aBounds with the final size and
      // location.  Then apply the bottom+right margins (as
      // appropriate) to the frame.
      PlaceFrame(metrics, bounds);
    }
  }

  return result;
}

// XXX looks more like a method on PerFrameData?
void
nsInlineReflow::CalculateMargins()
{
  PerFrameData* pfd = mFrameData;
  const nsStyleSpacing* spacing = GetSpacing();
  if (mTreatFrameAsBlock) {
    pfd->mMarginFlags = CalculateBlockMarginsFor(mPresContext, pfd->mFrame,
                                                 spacing, pfd->mMargin);
  }
  else {
    // Get the margins from the style system
    spacing->CalcMarginFor(pfd->mFrame, pfd->mMargin);
    pfd->mMarginFlags = 0;
  }
}

PRUintn
nsInlineReflow::CalculateBlockMarginsFor(nsIPresContext& aPresContext,
                                         nsIFrame* aFrame,
                                         const nsStyleSpacing* aSpacing,
                                         nsMargin& aMargin)
{
  PRUint32 rv = 0;

  aSpacing->CalcMarginFor(aFrame, aMargin);

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

// XXX doesn't apply top margin; it should for inlines, yes? should
// top margins for inlines end up in the ascent and bottom margins in
// the descent?

void
nsInlineReflow::ApplyTopLeftMargins()
{
  mFrameX = mX;
  mFrameY = mTopEdge;

  // Compute left margin
  nscoord leftMargin = 0;
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
    leftMargin = mFrameData->mMargin.left;
    break;
  }
  mFrameX += leftMargin;
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
    mFrameAvailSize.width = mRightEdge - mFrameX - pfd->mMargin.right;
  }
  if (NS_UNCONSTRAINEDSIZE == mBottomEdge) {
    mFrameAvailSize.height = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mFrameAvailSize.height = mBottomEdge - mFrameY - pfd->mMargin.bottom;
  }

  // Give up now if there is no chance. Note that we allow a reflow if
  // the available space is zero because that way things that end up
  // zero sized won't trigger a new line to be created. We also allow
  // a reflow if we can't break before this frame.
  mInNBU = mLineLayout.InNonBreakingUnit();
  if (!mInNBU && mCanBreakBeforeFrame &&
      ((mFrameAvailSize.width < 0) || (mFrameAvailSize.height < 0))) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

/**
 * Reflow the frame, choosing the appropriate reflow method.
 */
PRBool
nsInlineReflow::ReflowFrame(nsHTMLReflowMetrics& aMetrics,
                            nsRect& aBounds,
                            nsReflowStatus& aStatus)
{
  // Get reflow reason set correctly. It's possible that a child was
  // created and then it was decided that it could not be reflowed
  // (for example, a block frame that isn't at the start of a
  // line). In this case the reason will be wrong so we need to check
  // the frame state.
  nsReflowReason reason = eReflowReason_Resize;
  nsIFrame* frame = mFrameData->mFrame;
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
  if (!mTreatFrameAsBlock) {
    mIsInlineAware = PR_TRUE;
//XX    reflowState.frameType = eReflowType_Inline;
    reflowState.lineLayout = &mLineLayout;
  }
  reflowState.reason = reason;

  // Let frame know that are reflowing it
  nscoord x = mFrameX;
  nscoord y = mFrameY;
  nsIHTMLReflow* htmlReflow;

  frame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  htmlReflow->WillReflow(mPresContext);

  aBounds.x = x;
  aBounds.y = y;

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

  htmlReflow->Reflow(mPresContext, aMetrics, reflowState, aStatus);
  aBounds.width = aMetrics.width;
  aBounds.height = aMetrics.height;

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
     ("nsInlineReflow::ReflowFrame: frame=%p reflowStatus=%x %saware",
      frame, aStatus, mIsInlineAware ? "" :"not "));

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
                              nsRect& aBounds,
                              nsReflowStatus& aStatus)
{
  PerFrameData* pfd = mFrameData;

  // Compute right margin to use
  mRightMargin = 0;
  if (0 != aBounds.width) {
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
      mRightMargin = pfd->mMargin.right;
      break;
    }
  }

  // If the frame is a block frame but the parent frame is not a block
  // frame and the frame count is not zero then break before the block
  // frame. This forces blocks that are children of inlines to be the
  // one and only child of the inline frame.
  if (mTreatFrameAsBlock && !mOuterIsBlock && (0 != mFrameNum)) {
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
    return PR_FALSE;
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
      mOuterReflowState.mNoWrap) {
    return PR_TRUE;
  }

  // If this frame is part of a non-breaking-unit then it fits
  if (mInNBU) {
    return PR_TRUE;
  }

  // See if the frame fits. If it doesn't then we fabricate up a
  // break-before status.
  if (aBounds.XMost() + mRightMargin > mRightEdge) {
    // Retain the completion status of the frame that was reflowed in
    // case the caller cares.
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
    return PR_FALSE;
  }

  return PR_TRUE;
}

/**
 * Place the frame. Update running counters.
 */
void
nsInlineReflow::PlaceFrame(nsHTMLReflowMetrics& aMetrics, nsRect& aBounds)
{
  PerFrameData* pfd = mFrameData;

  // Remember this for later...
  if (mTreatFrameAsBlock) {
    mIsBlock = PR_TRUE;
  }

  // If frame is zero width then do not apply its left and right margins.
  PRBool emptyFrame = PR_FALSE;
  if ((0 == aBounds.width) || (0 == aBounds.height)) {
    aBounds.x = mX;
    aBounds.y = 0;
    emptyFrame = PR_TRUE;
  }
  pfd->mBounds = aBounds;

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
  mCarriedOutMarginFlags = aMetrics.mCarriedOutMarginFlags;

  // Advance to next X coordinate
  mX = aBounds.XMost() + mRightMargin;

  // If the frame is a not inline aware and it takes up some area
  // disable leading white-space compression for the next frame to
  // be reflowed.
  if (!mIsInlineAware && !emptyFrame) {
    mLineLayout.SetSkipLeadingWhiteSpace(PR_FALSE);
  }

  // Compute the bottom margin to apply. Note that the margin only
  // applies if the frame ends up with a non-zero height.
  if (!emptyFrame) {
    // Inform line layout that we have placed a non-empty frame
#ifdef NS_DEBUG
    mLineLayout.AddPlacedFrame(mFrameData->mFrame);
#else
    mLineLayout.AddPlacedFrame();
#endif

    // Update max-element-size
    if (mComputeMaxElementSize) {
      nscoord mw = aMetrics.maxElementSize->width;
      if (mw > mMaxElementSize.width) {
        mMaxElementSize.width = mw;
      }
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

  // Short circuit 99% of this when this code has reflowed a single
  // block frame.
  aLineBox.x = mLeftEdge;
  aLineBox.y = mTopEdge;
  aLineBox.width = mX - mLeftEdge;
  if (mTreatFrameAsBlock) {
    aLineBox.height = pfd0->mBounds.height;
    aMaxAscent = pfd0->mAscent;
    aMaxDescent = pfd0->mDescent;
    pfd0->mFrame->SetRect(aLineBox);
    return;
  }

  // Get the parent elements font in case we need it
  const nsStyleFont* font;
  mOuterFrame->GetStyleData(eStyleStruct_Font,
                            (const nsStyleStruct*&)font);
  nsIFontMetrics* fm = mPresContext.GetMetricsFor(font->mFont);

  // Examine each and determine the minYTop, the maxYBottom and the
  // maximum height. We will use these values to determine the final
  // height of the line box and then position each frame.
  nscoord minYTop = 0;
  nscoord maxYBottom = 0;
  nscoord maxHeight = 0;
  PRBool haveTBFrames = PR_FALSE;
  PerFrameData* pfd;
  for (pfd = pfd0; pfd < end; pfd++) {
    PRUint8 verticalAlignEnum;
    nscoord fontParam;

    nsIFrame* frame = pfd->mFrame;

    // yTop = Y coordinate for the top of frame box <B>relative</B> to
    // the baseline of the linebox which is assumed to be at Y=0
    nscoord yTop;

    // Compute the effective height of the box applying the top and
    // bottom margins
    nscoord height = pfd->mBounds.height + pfd->mMargin.top +
      pfd->mMargin.bottom;
    if (height > maxHeight) {
      maxHeight = pfd->mBounds.height;
    }
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
      case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
        // THESE ARE DONE DURING PASS2
        haveTBFrames = PR_TRUE;
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
  // maximum ascent of the line box.

  // XXX what about positive minY?
  nscoord lineHeight = maxYBottom - minYTop;
  if (lineHeight < maxHeight) {
    // This ensures that any object aligned top/bottom will update the
    // line height properly since they don't impact the minY or
    // maxYBottom values.
    lineHeight = maxHeight;
  }
  aLineBox.height = lineHeight;
  nscoord maxAscent = -minYTop;

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
        pfd->mBounds.y = mTopEdge + maxAscent + pfd->mBounds.y -
          pfd->mMargin.top;
        break;
      }
    }
    else {
      pfd->mBounds.y = mTopEdge + maxAscent + pfd->mBounds.y -
        pfd->mMargin.top;
    }
    frame->SetRect(pfd->mBounds);
  }
  aMaxAscent = maxAscent;
  aMaxDescent = lineHeight - maxAscent;

  // XXX Now we can apply 1/2 the line-height...
  NS_RELEASE(fm);
}

void
nsInlineReflow::HorizontalAlignFrames(const nsRect& aLineBox)
{
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
    case NS_STYLE_TEXT_ALIGN_JUSTIFY:
      // Default layout has everything aligned left
      return;

    case NS_STYLE_TEXT_ALIGN_CENTER:
      dx = (maxWidth - aLineBox.width) / 2;
      break;
    }

    // Position children
    PerFrameData* pfd = mFrameDataBase;
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

void
nsInlineReflow::RelativePositionFrames()
{
  nsPoint origin;
  PerFrameData* pfd = mFrameDataBase;
  PerFrameData* end = pfd + mFrameNum;
  for (; pfd < end; pfd++) {
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
    }
  }
}
