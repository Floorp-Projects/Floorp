#if 0
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
#include "nsInlineReflow.h"
#include "nsLineLayout.h"
#include "nsIFontMetrics.h"
#include "nsIPresContext.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"

#include "nsHTMLContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsCRT.h"

#ifdef NS_DEBUG
#undef NOISY_VERTICAL_ALIGN
#else
#undef NOISY_VERTICAL_ALIGN
#endif

#define PLACED_LEFT  0x1
#define PLACED_RIGHT 0x2

// XXX handle DIR=right-to-left

nsInlineReflow::nsInlineReflow(nsLineLayout& aLineLayout,
                               const nsHTMLReflowState& aOuterReflowState,
                               nsHTMLContainerFrame* aOuterFrame,
                               PRBool aOuterIsBlock,
                               PRBool aComputeMaxElementSize)
  : mOuterReflowState(aOuterReflowState),
    mOuterFrame(aOuterFrame),
    mSpaceManager(aLineLayout.mSpaceManager),
    mLineLayout(aLineLayout),
    mPresContext(aLineLayout.mPresContext),
    mOuterIsBlock(aOuterIsBlock),
    mComputeMaxElementSize(aComputeMaxElementSize)
{
  NS_ASSERTION(nsnull != mSpaceManager, "caller must have space manager");
  mFrameDataBase = mFrameDataBuf;
  mNumFrameData = sizeof(mFrameDataBuf) / sizeof(mFrameDataBuf[0]);
  mMinLineHeight = -1;

  // Stash away some style data that we need
  const nsStyleText* styleText;
  mOuterFrame->GetStyleData(eStyleStruct_Text,
                            (const nsStyleStruct*&) styleText);
  mTextAlign = styleText->mTextAlign;
  switch (styleText->mWhiteSpace) {
  case NS_STYLE_WHITESPACE_PRE:
  case NS_STYLE_WHITESPACE_NOWRAP:
    mNoWrap = PR_TRUE;
    break;
  default:
    mNoWrap = PR_FALSE;
    break;
  }
  mDirection = aOuterReflowState.mStyleDisplay->mDirection;

  mNextRCFrame = nsnull;
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
  if ((aWidth > 200000) && (aWidth != NS_UNCONSTRAINEDSIZE)) {
    mOuterFrame->ListTag(stdout);
    printf(": Init: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
    aWidth = NS_UNCONSTRAINEDSIZE;
  }
  if ((aHeight > 200000) && (aHeight != NS_UNCONSTRAINEDSIZE)) {
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
  if ((aWidth > 200000) && (aWidth != NS_UNCONSTRAINEDSIZE)) {
    mOuterFrame->ListTag(stdout);
    printf(": UpdateBand: bad caller: width WAS %d(0x%x)\n",
           aWidth, aWidth);
    aWidth = NS_UNCONSTRAINEDSIZE;
  }
  if ((aHeight > 200000) && (aHeight != NS_UNCONSTRAINEDSIZE)) {
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
  if (NS_STYLE_DIRECTION_LTR == mDirection) {
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

  return NS_OK;
}

nsresult
nsInlineReflow::ReflowFrame(nsIFrame* aFrame,
                            PRBool aIsAdjacentWithTop,
                            nsReflowStatus& aReflowStatus)
{
  // Prepare for reflowing the frame
  nsresult rv = SetFrame(aFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Compute the available size for the frame.
  nsSize availSize;
  if (NS_UNCONSTRAINEDSIZE == mRightEdge) {
    availSize.width = NS_UNCONSTRAINEDSIZE;
  }
  else {
    availSize.width = mRightEdge - mX;
    if (mNoWrap) {
      availSize.width = mOuterReflowState.availableWidth;
    }
  }
  if (NS_UNCONSTRAINEDSIZE == mBottomEdge) {
    availSize.height = NS_UNCONSTRAINEDSIZE;
  }
  else {
    availSize.height = mBottomEdge - mTopEdge;
  }

  // Get reflow reason set correctly. It's possible that a child was
  // created and then it was decided that it could not be reflowed
  // (for example, a block frame that isn't at the start of a
  // line). In this case the reason will be wrong so we need to check
  // the frame state.
  nsReflowReason reason = eReflowReason_Resize;
  nsFrameState state;
  aFrame->GetFrameState(&state);
  if (NS_FRAME_FIRST_REFLOW & state) {
    reason = eReflowReason_Initial;
  }
  else if (mNextRCFrame == aFrame) {
    reason = eReflowReason_Incremental;
    // Make sure we only incrementally reflow once
    mNextRCFrame = nsnull;
  }

  // Setup reflow state for reflowing the frame
  nsHTMLReflowState reflowState(mPresContext, mOuterReflowState, aFrame,
                                availSize, reason);
  reflowState.lineLayout = &mLineLayout;
  if (!aIsAdjacentWithTop) {
    reflowState.isTopOfPage = PR_FALSE;  // make sure this is cleared
  }
  mLineLayout.SetUnderstandsWhiteSpace(PR_FALSE);

  // Stash copies of some of the computed state away for later
  // (vertical alignment, for example)
  PerFrameData* pfd = mFrameData;
  pfd->mMargin = reflowState.computedMargin;
  pfd->mBorderPadding = reflowState.mComputedBorderPadding;
  pfd->mFrameType = reflowState.frameType;

  // Capture this state *before* we reflow the frame in case it clears
  // the state out. We need to know how to treat the current frame
  // when breaking.
  mInWord = mLineLayout.InWord();

  // Apply left margins (as appropriate) to the frame computing the
  // new starting x,y coordinates for the frame.
  ApplyLeftMargin(reflowState);

  // Let frame know that are reflowing it
  nscoord x = pfd->mBounds.x;
  nscoord y = pfd->mBounds.y;
  nsIHTMLReflow* htmlReflow;

  aFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  htmlReflow->WillReflow(mPresContext);

  // Adjust spacemanager coordinate system for the frame. The
  // spacemanager coordinates are <b>inside</b> the mOuterFrame's
  // border+padding, but the x/y coordinates are not (recall that
  // frame coordinates are relative to the parents origin and that the
  // parents border/padding is <b>inside</b> the parent
  // frame. Therefore we have to subtract out the parents
  // border+padding before translating.
  nsSize innerMaxElementSize;
  nsHTMLReflowMetrics metrics(mComputeMaxElementSize
                              ? &innerMaxElementSize
                              : nsnull);
#ifdef DEBUG
  if (mComputeMaxElementSize) {
    metrics.maxElementSize->width = nscoord(0xdeadbeef);
    metrics.maxElementSize->height = nscoord(0xdeadbeef);
  }
#endif
  nscoord tx = x - mOuterReflowState.mComputedBorderPadding.left;
  nscoord ty = y - mOuterReflowState.mComputedBorderPadding.top;
  mSpaceManager->Translate(tx, ty);
  htmlReflow->Reflow(mPresContext, metrics, reflowState, aReflowStatus);
  mSpaceManager->Translate(-tx, -ty);

#ifdef DEBUG_kipp
  NS_ASSERTION((metrics.width > -200000) && (metrics.width < 200000), "oy");
  NS_ASSERTION((metrics.height > -200000) && (metrics.height < 200000), "oy");
#endif
#ifdef DEBUG
  if (mComputeMaxElementSize &&
      ((nscoord(0xdeadbeef) == metrics.maxElementSize->width) ||
       (nscoord(0xdeadbeef) == metrics.maxElementSize->height))) {
    printf("nsInlineReflow: ");
    nsFrame::ListTag(stdout, aFrame);
    printf(" didn't set max-element-size!\n");
    metrics.maxElementSize->width = 0;
    metrics.maxElementSize->height = 0;
  }
#endif

  aFrame->GetFrameState(&state);
  if (NS_FRAME_OUTSIDE_CHILDREN & state) {
    pfd->mCombinedArea = metrics.mCombinedArea;
  }
  else {
    pfd->mCombinedArea.x = 0;
    pfd->mCombinedArea.y = 0;
    pfd->mCombinedArea.width = metrics.width;
    pfd->mCombinedArea.height = metrics.height;
  }
  pfd->mBounds.width = metrics.width;
  pfd->mBounds.height = metrics.height;
  if (mComputeMaxElementSize) {
    pfd->mMaxElementSize = *metrics.maxElementSize;
  }

  // Now that frame has been reflowed at least one time make sure that
  // the NS_FRAME_FIRST_REFLOW bit is cleared so that never give it an
  // initial reflow reason again.
  if (eReflowReason_Initial == reason) {
    aFrame->GetFrameState(&state);
    aFrame->SetFrameState(state & ~NS_FRAME_FIRST_REFLOW);
  }

  if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    // If frame is complete and has a next-in-flow, we need to delete
    // them now. Do not do this when a break-before is signaled because
    // the frame is going to get reflowed again (and may end up wanting
    // a next-in-flow where it ends up).
    if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
      nsIFrame* kidNextInFlow;
      aFrame->GetNextInFlow(&kidNextInFlow);
      if (nsnull != kidNextInFlow) {
        // Remove all of the childs next-in-flows. Make sure that we ask
        // the right parent to do the removal (it's possible that the
        // parent is not this because we are executing pullup code)
        nsHTMLContainerFrame* parent;
        aFrame->GetParent((nsIFrame**) &parent);
        parent->DeleteChildsNextInFlow(mPresContext, aFrame);
      }
    }
  }

  // Reflow the frame. If the frame must be placed somewhere else
  // then we return immediately.
  if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    // See if we can place the frame. If we can't fit it, then we
    // return now.
    if (CanPlaceFrame(reflowState, metrics, aReflowStatus)) {
      // Place the frame, updating aBounds with the final size and
      // location.  Then apply the bottom+right margins (as
      // appropriate) to the frame.
      PlaceFrame(metrics);
    }
  }

  return rv;
}

void
nsInlineReflow::ApplyLeftMargin(const nsHTMLReflowState& aReflowState)
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
  switch (aReflowState.mStyleDisplay->mFloats) {
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
    pfd->mFrame->GetPrevInFlow(&prevInFlow);
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
nsInlineReflow::CanPlaceFrame(const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aMetrics,
                              nsReflowStatus& aStatus)
{
  PerFrameData* pfd = mFrameData;

  // Compute right margin to use
  mRightMargin = 0;
  if (0 != pfd->mBounds.width) {
    switch (aReflowState.mStyleDisplay->mFloats) {
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

  // Set outside to PR_TRUE if the result of the reflow leads to the
  // frame sticking outside of our available area.
  PRBool outside = pfd->mBounds.XMost() + mRightMargin > mRightEdge;

  // There are several special conditions that exist which allow us to
  // ignore outside. If they are true then we can place frame and
  // return PR_TRUE.
  if (!mCanBreakBeforeFrame || mInWord || mNoWrap) {
    return PR_TRUE;
  }

  if (0 == pfd->mMargin.left + pfd->mBounds.width + pfd->mMargin.right) {
    // Empty frames always fit right where they are
    return PR_TRUE;
  }

  if (0 == mFrameNum) {
    return PR_TRUE;
  }

  if (outside) {
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
    return PR_FALSE;
  }
  return PR_TRUE;
#if XXX
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
      mInWord) {
    return PR_TRUE;
  }

  // See if the frame fits. If it doesn't then we fabricate up a
  // break-before status.
  if (pfd->mBounds.XMost() + mRightMargin > mRightEdge) {
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
    return PR_FALSE;
  }

  return PR_TRUE;
#endif
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
  }
}

void
nsInlineReflow::AddFrame(nsIFrame* aFrame, const nsHTMLReflowMetrics& aMetrics)
{
  SetFrame(aFrame);
  PerFrameData* pfd = mFrameDataBase + mFrameNum;
  mFrameNum++;
  pfd->mFrameType = NS_CSS_FRAME_TYPE_INLINE|NS_FRAME_REPLACED_ELEMENT;
  pfd->mAscent = aMetrics.ascent;
  pfd->mDescent = aMetrics.descent;
  pfd->mMargin.SizeTo(0, 0, 0, 0);
  pfd->mBorderPadding.SizeTo(0, 0, 0, 0);
  aFrame->GetRect(pfd->mBounds);        // y value is irrelevant
  pfd->mCombinedArea = aMetrics.mCombinedArea;
  if (mComputeMaxElementSize) {
    pfd->mMaxElementSize.SizeTo(aMetrics.width, aMetrics.height);
  }
}

void
nsInlineReflow::RemoveFrame(nsIFrame* aFrame)
{
  PerFrameData* pfd = mFrameDataBase;
  PerFrameData* last = pfd + mFrameNum - 1;
  while (pfd <= last) {
    if (pfd->mFrame == aFrame) {
      mFrameNum--;
      if (pfd != last) {
        // Slide down the other structs over the vacancy
        nsCRT::memmove(pfd, pfd + 1, (last - pfd) * sizeof(PerFrameData));
      }
      break;
    }
    pfd++;
  }
}

PRBool
nsInlineReflow::IsZeroHeight() const
{
  PerFrameData* pfd = mFrameDataBase;
  PerFrameData* last = pfd + mFrameNum - 1;
  while (pfd <= last) {
    if (0 != pfd->mBounds.height) {
      return PR_FALSE;
    }
    pfd++;
  }
  return PR_TRUE;
}

// XXX what about ebina's center vs. ncsa-center?

// XXX Make sure that when a block or inline is reflowing an inline
// frame that is wrapping an anonymous block that none of the
// alignment routines are used.

// XXX avoid during pass1 table reflow
void
nsInlineReflow::VerticalAlignFrames(nsRect& aLineBox,
                                    nscoord& aMaxAscent,
                                    nscoord& aMaxDescent)
{
#if 1
  aLineBox.x = mLeftEdge;
  aLineBox.y = mTopEdge;
  aLineBox.width = mX - mLeftEdge;
  aLineBox.height = mMinLineHeight;
  aMaxAscent = 0;
  aMaxDescent = 0;
#else
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
  nsCOMPtr<nsIFontMetrics> fm;
  mPresContext.GetMetricsFor(font->mFont, getter_AddRefs(fm));

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

    // Compute vertical height of frame; add in margins (note: if the
    // margins are for an inline-non-replaced frame then other code
    // has forced them to zero).
    nscoord height = pfd->mBounds.height;
    height += pfd->mMargin.top + pfd->mMargin.bottom;
    pfd->mAscent += pfd->mMargin.top;

    if (NS_CSS_FRAME_TYPE_INLINE == pfd->mFrameType) {
      // According to the CSS2 spec (section 10.8 and 10.8.1) border,
      // padding and margins around inline non-replaced elements do
      // not enter into inline box height calculations (and therefore
      // the line box calculation). To accomplish that here we have to
      // subtract out the border and padding during vertical alignment
      // from the inline non-replaced frame height.
      height -= pfd->mBorderPadding.top + pfd->mBorderPadding.bottom;

      // When a line-height is specified for an inline-non-replaced
      // element then its value determines the exact height of the box
      // for the purposes of vertical alignment and line-height
      // sizing.
      nscoord lh = nsHTMLReflowState::CalcLineHeight(mPresContext, frame);
#ifdef NOISY_VERTICAL_ALIGN
      printf("  ");
      nsFrame::ListTag(stdout, pfd->mFrame);
      printf(": lineHeight=%d height=%d\n", lh, pfd->mBounds.height);
#endif
      if (lh >= 0) {
        nscoord leading = lh - height;
        nscoord topLeading = leading / 2;
        pfd->mAscent += topLeading;
        pfd->mMargin.top = topLeading;
        pfd->mMargin.bottom = leading - topLeading;
        height = lh;
      }
    }

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
        if (mOuterIsBlock) {
          if (height > maxTopHeight) {
            maxTopHeight = height;
          }
          continue;
        }
        else {
          // When parent is not the block, baseline align top/bottom
          // frames initially. The block will do a post-process after
          // the line-height is determined to place these frames.
          yTop = -pfd->mAscent;
          mLineLayout.RecordPass2VAlignFrame();
        }
        break;
      case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
        if (mOuterIsBlock) {
          if (height > maxBottomHeight) {
            maxBottomHeight = height;
          }
          continue;
        }
        else {
          // When parent is not the block, baseline align top/bottom
          // frames initially. The block will do a post-process after
          // the line-height is determined to place these frames.
          yTop = -pfd->mAscent;
          mLineLayout.RecordPass2VAlignFrame();
        }
        break;
      case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
        // Align the midpoint of the frame with 1/2 the parents x-height
        fm->GetXHeight(fontParam);
        yTop = -(fontParam + height)/2;
        break;
      case NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM:
        fm->GetMaxDescent(fontParam);
        yTop = fontParam - height;
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
      // "lowers" the box by the given distance (with zero being the
      // baseline). Since Y coordinates increase towards the bottom of
      // the screen we reverse the sign.
      yTop = -pfd->mAscent - textStyle->mVerticalAlign.GetCoordValue();
      break;
    case eStyleUnit_Percent:
      // Similar to a length value (eStyleUnit_Coord) except that the
      // percentage is a function of the elements line-height value.
      yTop = -pfd->mAscent -
        nscoord(textStyle->mVerticalAlign.GetPercentValue() * pfd->mBounds.height);
      break;
    default:
      yTop = -pfd->mAscent;
      break;
    }
    pfd->mBounds.y = yTop;

#ifdef DEBUG_kipp
    NS_ASSERTION((pfd->mBounds.y >= -200000) &&
                 (pfd->mBounds.y < 200000), "yikes");
#endif
#ifdef NOISY_VERTICAL_ALIGN
    printf("  ");
    nsAutoString tmp;
    pfd->mFrame->GetFrameName(tmp);
    fputs(tmp, stdout);
    printf(": yTop=%d minYTop=%d yBottom=%d maxYBottom=%d [bounds=%d]\n",
           yTop, minYTop, yTop + height, maxYBottom,
           pfd->mBounds.height);
    NS_ASSERTION(yTop >= -200000, "bad yTop");
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
    // CSS2 10.8.1 says that line-height, as applied to blocks, will
    // provide the *minimum* height of the line (line-height as
    // applied to a non-replaced inline element defines the exact
    // height of the inline element, not including borders and
    // padding). Therefore, we only adjust the line-height here if the
    // outer container is a block (because if the outer container is
    // an inline then its line-height will be applied by itself).
    if (mOuterIsBlock) {
      nscoord minLineHeight = mMinLineHeight;
      if (minLineHeight > lineHeight) {
        // Apply half of the changed line-height to the top and bottom
        // positioning of each frame.
        topEdge += (minLineHeight - lineHeight) / 2;
        lineHeight = minLineHeight;
      }
    }
  }
  aLineBox.height = lineHeight;
#ifdef NOISY_VERTICAL_ALIGN
          printf("  *lineHeight=newLineHeight=%d topEdgeDelta=%d\n",
                 lineHeight, topEdge - mTopEdge);
#endif

  // Pass2 - position each of the frames
  mMaxElementSize.SizeTo(0, 0);
  for (pfd = pfd0; pfd < end; pfd++) {
    nsIFrame* frame = pfd->mFrame;

    // Factor in this frame to the max-element-size
    if (mComputeMaxElementSize) {
      // The max-element width is the sum of the interior max-element
      // width plus the left and right margins that are applied to the
      // frame.
      nscoord mw = pfd->mMaxElementSize.width +
        pfd->mMargin.left + pfd->mMargin.right;
      if (mw > mMaxElementSize.width) {
        mMaxElementSize.width = mw;
      }

      // The max-element height is the sum of the interior max-element
      // height plus the top and bottom margins.
      nscoord mh = pfd->mMaxElementSize.height +
        pfd->mMargin.top + pfd->mMargin.bottom;
      if (mh > mMaxElementSize.height) {
        mMaxElementSize.height = mh;
      }
    }

    const nsStyleText* textStyle;
    frame->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textStyle);
    nsStyleUnit verticalAlignUnit = textStyle->mVerticalAlign.GetUnit();
    if (eStyleUnit_Enumerated == verticalAlignUnit) {
      PRUint8 verticalAlignEnum = textStyle->mVerticalAlign.GetIntValue();
      switch (verticalAlignEnum) {
      case NS_STYLE_VERTICAL_ALIGN_TOP:
#ifdef NOISY_VERTICAL_ALIGN
        printf("  ");
        nsFrame::ListTag(stdout, pfd->mFrame);
        printf(": [top] mTopEdge=%d margin.top=%d\n",
               mTopEdge, pfd->mMargin.top);
#endif
        pfd->mBounds.y = mTopEdge + pfd->mMargin.top;
        if (NS_CSS_FRAME_TYPE_INLINE == pfd->mFrameType) {
          pfd->mBounds.y -= pfd->mBorderPadding.top;
        }
        break;

      case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
#ifdef NOISY_VERTICAL_ALIGN
        printf("  ");
        nsFrame::ListTag(stdout, pfd->mFrame);
        printf(": [bottom] mTopEdge=%d lineHeight=%d height=%d margin.bottom=%d\n",
               mTopEdge, lineHeight, pfd->mBounds.height,
               pfd->mMargin.bottom);
#endif
        pfd->mBounds.y = mTopEdge + lineHeight -
          pfd->mBounds.height - pfd->mMargin.bottom;
        if (NS_CSS_FRAME_TYPE_INLINE == pfd->mFrameType) {
          pfd->mBounds.y += pfd->mBorderPadding.bottom;
        }
        break;

      default:
#ifdef NOISY_VERTICAL_ALIGN
        printf("  ");
        nsFrame::ListTag(stdout, pfd->mFrame);
        printf(": y=%d topEdge=%d maxAscent=%d margin.top=%d\n",
               pfd->mBounds.y, topEdge, maxAscent, pfd->mMargin.top);
#endif
        pfd->mBounds.y = topEdge + maxAscent + pfd->mBounds.y +
          pfd->mMargin.top;
        if (NS_CSS_FRAME_TYPE_INLINE == pfd->mFrameType) {
          pfd->mBounds.y -= pfd->mBorderPadding.top;
        }
        break;
      }
    }
    else {
      pfd->mBounds.y = topEdge + maxAscent + pfd->mBounds.y +
        pfd->mMargin.top;
      if (NS_CSS_FRAME_TYPE_INLINE == pfd->mFrameType) {
        pfd->mBounds.y -= pfd->mBorderPadding.top;
      }
    }
#ifdef DEBUG_kipp
    NS_ASSERTION((pfd->mBounds.y >= -200000) &&
                 (pfd->mBounds.y < 200000), "yikes");
#endif
    frame->SetRect(pfd->mBounds);

    if (mOuterIsBlock && mLineLayout.NeedPass2VAlign()) {
      // Perform pass2 vertical alignment for top/bottom aligned
      // frames that are not our direct descendants.
      nsIHTMLReflow* ihr;
      nsresult rv = frame->QueryInterface(kIHTMLReflowIID, (void**)&ihr);
      if (NS_SUCCEEDED(rv)) {
        nscoord distanceFromTopEdge = pfd->mBounds.y - mTopEdge;
        ihr->VerticalAlignFrames(mPresContext, mOuterReflowState,
                                 lineHeight, distanceFromTopEdge,
                                 pfd->mCombinedArea);
      }
    }
  }
  aMaxAscent = maxAscent;
  aMaxDescent = lineHeight - maxAscent;
#ifdef NOISY_VERTICAL_ALIGN
  printf("  ==> ");
  mOuterFrame->ListTag(stdout);
  printf(": lineBox=%d,%d,%d,%d maxAscent=%d maxDescent=%d\n",
         aLineBox.x, aLineBox.y, aLineBox.width, aLineBox.height,
         aMaxAscent, aMaxDescent);
#endif
#endif
}

void
nsInlineReflow::TrimTrailingWhiteSpace(nsRect& aLineBox)
{
#if XXX_still_do_this
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
#endif
}

void
nsInlineReflow::HorizontalAlignFrames(nsRect& aLineBox, PRBool aAllowJustify)
{
  if (NS_UNCONSTRAINEDSIZE == mRightEdge) {
    // Don't bother
    return;
  }

  nscoord maxWidth = mRightEdge - mLeftEdge;
  if (aLineBox.width < maxWidth) {
    nscoord dx = 0;
    switch (mTextAlign) {
    case NS_STYLE_TEXT_ALIGN_DEFAULT:
      if (NS_STYLE_DIRECTION_LTR == mDirection) {
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
      if (aAllowJustify) {
        JustifyFrames(maxWidth, aLineBox);
        return;
      }
      else if (NS_STYLE_DIRECTION_RTL == mDirection) {
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
      PerFrameData* pfd = mFrameDataBase;
      PerFrameData* end = pfd + mFrameNum;
      nsPoint origin;
      for (; pfd < end; pfd++) {
        nsIFrame* kid = pfd->mFrame;;
        kid->GetOrigin(origin);
        kid->MoveTo(origin.x + dx, origin.y);
        kid->GetNextSibling(&kid);
      }
    }
  }
}

// XXX avoid during pass1 table reflow
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
      nsStyleCoord coord;
      nscoord dx = 0;
      switch (kidPosition->mOffset.GetLeftUnit()) {
      case eStyleUnit_Percent:
        printf("XXX: not yet implemented: %% relative position\n");
      case eStyleUnit_Auto:
        break;
      case eStyleUnit_Coord:
        dx = kidPosition->mOffset.GetLeft(coord).GetCoordValue();
        break;
      default:
        break;
      }
      nscoord dy = 0;
      switch (kidPosition->mOffset.GetTopUnit()) {
      case eStyleUnit_Percent:
        printf("XXX: not yet implemented: %% relative position\n");
      case eStyleUnit_Auto:
        break;
      case eStyleUnit_Coord:
        dy = kidPosition->mOffset.GetTop(coord).GetCoordValue();
        break;
      default:
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
#endif
