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

#include "nsCSSLayout.h"
#include "nsFrameReflowState.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsStyleConsts.h"

// XXX We could support "arbitrary" negative margins if we detected
// frames falling outside the parent frame and wrap them in a view
// when it happens.

// XXX move the nsCSSLayout alignment code here? Will body frame be
// using it?

nsInlineReflow::nsInlineReflow(nsLineLayout& aLineLayout,
                               nsFrameReflowState& aOuterReflowState,
                               nsHTMLContainerFrame* aOuterFrame)
  : mLineLayout(aLineLayout),
    mPresContext(aOuterReflowState.mPresContext),
    mOuterReflowState(aOuterReflowState),
    mComputeMaxElementSize(aOuterReflowState.mComputeMaxElementSize)
{
  mSpaceManager = aLineLayout.mSpaceManager;
  NS_ASSERTION(nsnull != mSpaceManager, "caller must have space manager");
  mOuterFrame = aOuterFrame;
  mFrameData = mFrameDataBuf;
  mNumFrameData = sizeof(mFrameDataBuf) / sizeof(mFrameDataBuf[0]);
}

nsInlineReflow::~nsInlineReflow()
{
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
  mY = aY;
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
  mMaxAscent = 0;
  mMaxDescent = 0;
  mFirstFrame = nsnull;
  mFrameNum = 0;
  mMaxElementSize.width = 0;
  mMaxElementSize.height = 0;
}

void
nsInlineReflow::UpdateBand(nscoord aX, nscoord aY,
                           nscoord aWidth, nscoord aHeight)
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
  mY = aY;
  if (NS_UNCONSTRAINEDSIZE == aHeight) {
    mBottomEdge = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mBottomEdge = aY + aHeight;
  }
}

const nsStyleDisplay*
nsInlineReflow::GetDisplay()
{
  if (nsnull != mDisplay) {
    return mDisplay;
  }
  mFrame->GetStyleData(eStyleStruct_Display,
                       (const nsStyleStruct*&)mDisplay);
  return mDisplay;
}

const nsStylePosition*
nsInlineReflow::GetPosition()
{
  if (nsnull != mPosition) {
    return mPosition;
  }
  mFrame->GetStyleData(eStyleStruct_Position,
                       (const nsStyleStruct*&)mPosition);
  return mPosition;
}

const nsStyleSpacing*
nsInlineReflow::GetSpacing()
{
  if (nsnull != mSpacing) {
    return mSpacing;
  }
  mFrame->GetStyleData(eStyleStruct_Spacing,
                       (const nsStyleStruct*&)mSpacing);
  return mSpacing;
}

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

void
nsInlineReflow::SetFrame(nsIFrame* aFrame)
{
  if (nsnull == mFirstFrame) {
    mFirstFrame = aFrame;
  }

  // We can break before the frame if we placed at least one frame on
  // the line.
  mCanBreakBeforeFrame = mLineLayout.GetPlacedFrames() > 0;

  mFrame = aFrame;
  mDisplay = nsnull;
  mSpacing = nsnull;
  mPosition = nsnull;
  mTreatFrameAsBlock = TreatFrameAsBlockFrame();
  mIsInlineAware = PR_FALSE;
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
  SetFrame(aFrame);

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

void
nsInlineReflow::CalculateMargins()
{
  const nsStyleSpacing* spacing = GetSpacing();
  if (mTreatFrameAsBlock) {
    CalculateBlockMarginsFor(mPresContext, mFrame, spacing, mMargin);
  }
  else {
    // Get the margins from the style system
    spacing->CalcMarginFor(mFrame, mMargin);
  }
}

void
nsInlineReflow::CalculateBlockMarginsFor(nsIPresContext& aPresContext,
                                         nsIFrame* aFrame,
                                         const nsStyleSpacing* aSpacing,
                                         nsMargin& aMargin)
{
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
  }
  if (eStyleUnit_Auto == bottomUnit) {
    aMargin.bottom = fontHeight;
  }

  // XXX Add in code to provide a zero top margin when the frame is
  // the "first" block frame in a margin-root situation.?

  // XXX Add in code to provide a zero bottom margin when the frame is
  // the "last" block frame in a margin-root situation.?
}

void
nsInlineReflow::ApplyTopLeftMargins()
{
  mFrameX = mX;
  mFrameY = mY;

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
    leftMargin = mMargin.left;
    break;
  }
  mFrameX += leftMargin;
}

PRBool
nsInlineReflow::ComputeAvailableSize()
{
  // Compute the available size from the outer's perspective
  if (NS_UNCONSTRAINEDSIZE == mRightEdge) {
    mFrameAvailSize.width = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mFrameAvailSize.width = mRightEdge - mFrameX - mMargin.right;
  }
  if (NS_UNCONSTRAINEDSIZE == mBottomEdge) {
    mFrameAvailSize.height = NS_UNCONSTRAINEDSIZE;
  }
  else {
    mFrameAvailSize.height = mBottomEdge - mFrameY - mMargin.bottom;
  }

  // Give up now if there is no chance. Note that we allow a reflow if
  // the available space is zero because that way things that end up
  // zero sized won't trigger a new line to be created. We also allow
  // a reflow if we can't break before this frame.
  if (mCanBreakBeforeFrame &&
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
  nsFrameState state;
  mFrame->GetFrameState(state);
  if (NS_FRAME_FIRST_REFLOW & state) {
    reason = eReflowReason_Initial;
  }
  else if (mOuterReflowState.mNextRCFrame == mFrame) {
    reason = eReflowReason_Incremental;
    // Make sure we only incrementally reflow once
    mOuterReflowState.mNextRCFrame = nsnull;
  }

  // Setup reflow state for reflowing the frame
  nsHTMLReflowState reflowState(mFrame, mOuterReflowState, mFrameAvailSize);
  if (!mTreatFrameAsBlock) {
    mIsInlineAware = PR_TRUE;
    reflowState.frameType = eReflowType_Inline;
    reflowState.lineLayout = &mLineLayout;
  }
  reflowState.reason = reason;

  // Let frame know that are reflowing it
  nscoord x = mFrameX;
  nscoord y = mFrameY;
  nsIHTMLReflow* htmlReflow;

  mFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  htmlReflow->WillReflow(mPresContext);
  mFrame->MoveTo(x, y);

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
    mFrame->GetFrameState(state);
    mFrame->SetFrameState(state & ~NS_FRAME_FIRST_REFLOW);
  }

  // If frame is complete and has a next-in-flow, we need to delete
  // them now. Do not do this when a break-before is signaled because
  // the frame is going to get reflowed again (and may end up wanting
  // a next-in-flow where it ends up).
  if (!NS_INLINE_IS_BREAK_BEFORE(aStatus) &&
      NS_FRAME_IS_COMPLETE(aStatus)) {
    nsIFrame* kidNextInFlow;
    mFrame->GetNextInFlow(kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsHTMLContainerFrame* parent;
      mFrame->GetGeometricParent((nsIFrame*&) parent);
      parent->DeleteChildsNextInFlow(mPresContext, mFrame);
    }
  }

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsInlineReflow::ReflowFrame: frame=%p reflowStatus=%x %saware",
      mFrame, aStatus, mIsInlineAware ? "" :"not "));

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
      mRightMargin = mMargin.right;
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
      mOuterReflowState.mNoWrap) {
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
  mFrame->SetRect(aBounds);

  // Record ascent and update max-ascent and max-descent values
  SetFrameData(aMetrics);
  mFrameNum++;
  if (aMetrics.ascent > mMaxAscent) {
    mMaxAscent = aMetrics.ascent;
  }
  if (aMetrics.descent > mMaxDescent) {
    mMaxDescent = aMetrics.descent;
  }

  // Compute collapsed margin information
  mCarriedOutTopMargin = aMetrics.mCarriedOutTopMargin;
  mCarriedOutBottomMargin = aMetrics.mCarriedOutBottomMargin;

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
    mLineLayout.AddPlacedFrame(mFrame);
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

/**
 * Store away the ascent value associated with the current frame
 */
nsresult
nsInlineReflow::SetFrameData(const nsHTMLReflowMetrics& aMetrics)
{
  PRInt32 frameNum = mFrameNum;
  if (frameNum == mNumFrameData) {
    mNumFrameData *= 2;
    PerFrameData* newData = new PerFrameData[mNumFrameData];
    if (nsnull == newData) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsCRT::memcpy(newData, mFrameData, sizeof(PerFrameData) * frameNum);
    if (mFrameData != mFrameDataBuf) {
      delete [] mFrameData;
    }
    mFrameData = newData;
  }
  PerFrameData* pfd = &mFrameData[frameNum];
  pfd->mAscent = aMetrics.ascent;
  pfd->mDescent = aMetrics.descent;
  pfd->mMargin = mMargin;
  return NS_OK;
}

// XXX what about ebina's center vs. ncsa-center?
void
nsInlineReflow::VerticalAlignFrames(nsRect& aLineBox)
{
  nscoord x = mLeftEdge;
  nscoord y0 = mTopEdge;
  nscoord width = mX - mLeftEdge;
  nscoord height = mMaxAscent + mMaxDescent;

  if ((mFrameNum > 1) && !mIsBlock) {
    // Only when we have more than one frame should we do vertical
    // alignment. Sometimes we will have 2 frames with the second one
    // being a block; we don't vertically align then either. This
    // happens when the first frame is nothing but compressed
    // whitespace.
    const nsStyleFont* font;
    mOuterFrame->GetStyleData(eStyleStruct_Font,
                              (const nsStyleStruct*&)font);

    // Determine minimum and maximum y values for the line and
    // perform alignment of all children except those requesting bottom
    // alignment. The second pass will align bottom children (if any)
    nsIFontMetrics* fm = mPresContext.GetMetricsFor(font->mFont);
    nsIFrame* kid = mFirstFrame;
    nsRect kidRect;
    nscoord minY = y0;
    nscoord maxY = y0;
    PRIntn pass2Kids = 0;
    PRIntn kidCount = mFrameNum;
    PerFrameData* pfd = mFrameData;
    for (; --kidCount >= 0; pfd++) {
      nscoord kidAscent = pfd->mAscent;

      const nsStyleText* textStyle;
      kid->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textStyle);
      nsStyleUnit verticalAlignUnit = textStyle->mVerticalAlign.GetUnit();
      PRUint8 verticalAlignEnum = NS_STYLE_VERTICAL_ALIGN_BASELINE;

      kid->GetRect(kidRect);

      // Vertically align the child
      nscoord kidYTop = 0;

      PRBool isPass2Kid = PR_FALSE;
      nscoord fontParam;
      switch (verticalAlignUnit) {
      case eStyleUnit_Coord:
        // According to the spec, a positive value "raises" the box by
        // the given distance while a negative value "lowers" the box
        // by the given distance. Since Y coordinates increase towards
        // the bottom of the screen we reverse the sign. All of the
        // raising and lowering is done relative to the baseline, so
        // we start our adjustments there.
        kidYTop = mMaxAscent - kidAscent;               // get baseline first
        kidYTop -= textStyle->mVerticalAlign.GetCoordValue();
        break;

      case eStyleUnit_Percent:
        pass2Kids++;
        isPass2Kid = PR_TRUE;
        break;

      case eStyleUnit_Enumerated:
        verticalAlignEnum = textStyle->mVerticalAlign.GetIntValue();
        switch (verticalAlignEnum) {
        default:
        case NS_STYLE_VERTICAL_ALIGN_BASELINE:
          // Align the kid's baseline at the max baseline
          kidYTop = mMaxAscent - kidAscent;
          break;

        case NS_STYLE_VERTICAL_ALIGN_TOP:
          // Align the top of the kid with the top of the line box
          break;

        case NS_STYLE_VERTICAL_ALIGN_SUB:
          // Align the child's baseline on the superscript baseline
          fm->GetSubscriptOffset(fontParam);
          kidYTop = mMaxAscent + fontParam - kidAscent;
          break;

        case NS_STYLE_VERTICAL_ALIGN_SUPER:
          // Align the child's baseline on the subscript baseline
          fm->GetSuperscriptOffset(fontParam);
          kidYTop = mMaxAscent - fontParam - kidAscent;
          break;

        case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
          pass2Kids++;
          isPass2Kid = PR_TRUE;
          break;

        case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
          // Align the midpoint of the box with 1/2 the parent's x-height
          fm->GetXHeight(fontParam);
          kidYTop = mMaxAscent - (fontParam / 2) - (kidRect.height/2);
          break;

        case NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM:
          fm->GetMaxDescent(fontParam);
          kidYTop = mMaxAscent + fontParam - kidRect.height;
          break;

        case NS_STYLE_VERTICAL_ALIGN_TEXT_TOP:
          fm->GetMaxAscent(fontParam);
          kidYTop = mMaxAscent - fontParam;
          break;
        }
        break;
      
      default:
        // Align the kid's baseline at the max baseline
        kidYTop = mMaxAscent - kidAscent;
        break;
      }

      /* XXX or grow the box - which is it? */
      if (kidYTop < 0) {
        kidYTop = 0;
      }

      // Place kid and update min and max Y values
      if (!isPass2Kid) {
        nscoord y = y0 + kidYTop;
        if (y < minY) minY = y;
        kid->MoveTo(kidRect.x, y);
        y += kidRect.height;
        if (y > maxY) maxY = y;
      }
      else {
        nscoord y = y0 + kidRect.height;
        if (y > maxY) maxY = y;
      }

      kid->GetNextSibling(kid);
    }

    // Now compute the final line-height
    nscoord lineHeight = maxY - minY;

    if (0 != pass2Kids) {
      // Position all of the bottom aligned children
      kidCount = mFrameNum;
      kid = mFirstFrame;
      pfd = mFrameData;
      for (; --kidCount >= 0; pfd++) {
        nscoord kidAscent = pfd->mAscent;

        // Get kid's vertical align style data
        const nsStyleText* textStyle;
        kid->GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textStyle);
        nsStyleUnit verticalAlignUnit = textStyle->mVerticalAlign.GetUnit();

        if (eStyleUnit_Percent == verticalAlignUnit) {
          // According to the spec, a positive value "raises" the box by
          // the given distance while a negative value "lowers" the box
          // by the given distance. Since Y coordinates increase towards
          // the bottom of the screen we reverse the sign. All of the
          // raising and lowering is done relative to the baseline, so
          // we start our adjustments there.
          nscoord kidYTop = mMaxAscent - kidAscent;       // get baseline first
          kidYTop -=
            nscoord(textStyle->mVerticalAlign.GetPercentValue() * lineHeight);
          kid->GetRect(kidRect);
          kid->MoveTo(kidRect.x, y0 + kidYTop);
          if (--pass2Kids == 0) {
            // Stop on last pass2 kid
            break;
          }
        }
        else if (verticalAlignUnit == eStyleUnit_Enumerated) {
          PRUint8 verticalAlignEnum = textStyle->mVerticalAlign.GetIntValue();
          // Vertically align the child
          if (NS_STYLE_VERTICAL_ALIGN_BOTTOM == verticalAlignEnum) {
            // Place kid along the bottom
            kid->GetRect(kidRect);
            kid->MoveTo(kidRect.x, y0 + lineHeight - kidRect.height);
            if (--pass2Kids == 0) {
              // Stop on last pass2 kid
              break;
            }
          }
        }

        kid->GetNextSibling(kid);
      }
    }

    NS_RELEASE(fm);
  }

  aLineBox.x = x;
  aLineBox.y = y0;
  aLineBox.width = width;
  aLineBox.height = height;
}

void
nsInlineReflow::HorizontalAlignFrames(const nsRect& aLineBox)
{
  const nsStyleText* styleText = mOuterReflowState.mStyleText;
  nsCSSLayout::HorizontallyPlaceChildren(&mPresContext,
                                         mOuterFrame,
                                         styleText->mTextAlign,
                                         mOuterReflowState.mDirection,
                                         mFirstFrame, mFrameNum,
                                         aLineBox.width,
                                         mRightEdge - mLeftEdge);
}

void
nsInlineReflow::RelativePositionFrames()
{
  nsCSSLayout::RelativePositionChildren(&mPresContext,
                                        mOuterFrame,
                                        mFirstFrame, mFrameNum);
}
