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
#include "nsCSSInlineLayout.h"
#include "nsCSSLineLayout.h"
#include "nsCSSLayout.h"
#include "nsHTMLIIDs.h"
#include "nsCSSContainerFrame.h"

#include "nsIFontMetrics.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIReflowCommand.h"
#include "nsIRunaround.h"

nsCSSInlineLayout::nsCSSInlineLayout(nsCSSLineLayout&     aLineLayout,
                                     nsCSSContainerFrame* aContainerFrame,
                                     nsIStyleContext*     aContainerStyle)
  : mLineLayout(aLineLayout)
{
  mContainerFrame = aContainerFrame;
  mAscents = mAscentBuf;
  mMaxAscents = sizeof(mAscentBuf) / sizeof(mAscentBuf[0]);

  mContainerFont = (const nsStyleFont*)
    aContainerStyle->GetStyleData(eStyleStruct_Font);
  mContainerText = (const nsStyleText*)
    aContainerStyle->GetStyleData(eStyleStruct_Text);
  mContainerDisplay = (const nsStyleDisplay*)
    aContainerStyle->GetStyleData(eStyleStruct_Display);
  mDirection = mContainerDisplay->mDirection;
  mIsBullet = PR_FALSE;
  mNextRCFrame = nsnull;
}

nsCSSInlineLayout::~nsCSSInlineLayout()
{
  if (mAscents != mAscentBuf) {
    delete [] mAscents;
  }
}

void
nsCSSInlineLayout::Init(const nsReflowState* aContainerReflowState)
{
  mContainerReflowState = aContainerReflowState;
}

nsresult
nsCSSInlineLayout::SetAscent(nscoord aAscent)
{
  PRInt32 frameNum = mFrameNum;
  if (frameNum == mMaxAscents) {
    mMaxAscents *= 2;
    nscoord* newAscents = new nscoord[mMaxAscents];
    if (nsnull == newAscents) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsCRT::memcpy(newAscents, mAscents, sizeof(nscoord) * frameNum);
    if (mAscents != mAscentBuf) {
      delete [] mAscents;
    }
    mAscents = newAscents;
  }
  mAscents[frameNum] = aAscent;
  return NS_OK;
}

void
nsCSSInlineLayout::Prepare(PRBool aUnconstrainedWidth,
                           PRBool aNoWrap,
                           PRBool aComputeMaxElementSize)
{
  mIsBullet = PR_FALSE;
  mFrameNum = 0;
  mUnconstrainedWidth = aUnconstrainedWidth;
  mNoWrap = aNoWrap;
  mComputeMaxElementSize = aComputeMaxElementSize;
  if (aComputeMaxElementSize) {
    mMaxElementSize.width = 0;
    mMaxElementSize.height = 0;
  }
  mMaxAscent = 0;
  mMaxDescent = 0;
}

void
nsCSSInlineLayout::SetReflowSpace(nscoord aX, nscoord aY,
                                  nscoord aAvailWidth, nscoord aAvailHeight)
{
  mAvailWidth = aAvailWidth;
  mAvailHeight = aAvailHeight;
  mX = aX;
  mY = aY;
  mLeftEdge = aX;
  mRightEdge = aX + aAvailWidth;
}

//XXX block children of inline frames needs handling *here*

nsInlineReflowStatus
nsCSSInlineLayout::ReflowAndPlaceFrame(nsIFrame* aFrame)
{
  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsCSSInlineLayout::ReflowAndPlaceFrame: frame=%p x=%d",
                aFrame, mX));

  // Compute the maximum size of the frame. If there is no room at all
  // for it, then trigger a line-break before the frame.
  nsSize maxSize;
  nsMargin margin;
  if (!ComputeMaxSize(aFrame, margin, maxSize)) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsCSSInlineLayout::ReflowAndPlaceFrame: break-before"));
    return NS_INLINE_LINE_BREAK_BEFORE(0);/* XXX indicate never-reflowed? */
  }

  // Get reflow reason set correctly. It's possible that we created a
  // child and then decided that we cannot reflow it (for example, a
  // block frame that isn't at the start of a line). In this case the
  // reason will be wrong so we need to check the frame state.
  nsReflowReason reason = eReflowReason_Resize;
  nsFrameState state;
  aFrame->GetFrameState(state);
  if (NS_FRAME_FIRST_REFLOW & state) {
    reason = eReflowReason_Initial;
  }
  else if (mNextRCFrame == aFrame) {
    reason = eReflowReason_Incremental;
    // Make sure we only incrementally reflow once
    mNextRCFrame = nsnull;
  }

  // Setup reflow state for reflowing the frame
  nsReflowState reflowState(aFrame, *mContainerReflowState, maxSize);
  reflowState.reason = reason;
  nsInlineReflowStatus rs;
  nsSize frameMaxElementSize;
  nsReflowMetrics metrics(mComputeMaxElementSize
                          ? &frameMaxElementSize
                          : nsnull);
  PRBool isAware;
  aFrame->WillReflow(*mLineLayout.mPresContext);
  rs = ReflowFrame(aFrame, metrics, reflowState, isAware);
  if (NS_IS_REFLOW_ERROR(rs)) {
    return rs;
  }
  if (NS_INLINE_IS_BREAK_BEFORE(rs)) {
    return rs;
  }

  // XXX the 0,0 part of this is hack: get rid of it
  if (!isAware && ((0 != metrics.width) || (0 != metrics.height))) {
    mLineLayout.SetSkipLeadingWhiteSpace(PR_FALSE);
  }

  // It's possible the frame didn't fit
  if (metrics.width > maxSize.width) {
    if (!IsFirstChild()) {
      // We are out of room.
      // XXX mKidPrevInFlow
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                   ("nsCSSInlineLayout::ReflowAndPlaceFrame: !fit size=%d,%d",
                    metrics.width, metrics.height));

      // XXX if the child requested a break and we need to break too...

      return NS_INLINE_LINE_BREAK_BEFORE(0);
    }
  }

  nsRect frameRect(mX, mY, metrics.width, metrics.height);
  return PlaceFrame(aFrame, frameRect, metrics, margin, rs);
}

// XXX RTL
PRBool
nsCSSInlineLayout::IsFirstChild()
{
  return 0 == mFrameNum;
}

PRBool
nsCSSInlineLayout::ComputeMaxSize(nsIFrame* aFrame,
                                  nsMargin& aKidMargin,
                                  nsSize&   aResult)
{
  const nsStyleSpacing* kidSpacing;
  aFrame->GetStyleData(eStyleStruct_Spacing,
                       (const nsStyleStruct*&)kidSpacing);
  kidSpacing->CalcMarginFor(aFrame, aKidMargin);
  if (mUnconstrainedWidth || mNoWrap) {
    aResult.width = NS_UNCONSTRAINEDSIZE;
  }
  else {
    aResult.width = mRightEdge - mX;
    aResult.width -= aKidMargin.left + aKidMargin.right;
    if (!IsFirstChild() && (aResult.width <= 0)) {
      // XXX Make sure child is dirty for next time
      aFrame->WillReflow(*mLineLayout.mPresContext);
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
         ("nsCSSInlineLayout::ComputeMaxSize: !fit"));
      return PR_FALSE;
    }
  }

  // Give the child a limited height in case it's a block child and
  // our height was limited.
  aResult.height = mAvailHeight;
  return PR_TRUE;
}

nsInlineReflowStatus
nsCSSInlineLayout::ReflowFrame(nsIFrame*            aKidFrame,
                               nsReflowMetrics&     aMetrics,
                               const nsReflowState& aReflowState,
                               PRBool&              aInlineAware)
{
  // There are 3 ways to reflow the child frame: using the nsIRunaround
  // interface, using the nsIInlineReflow interface or using the default
  // Reflow method in nsIFrame. The order of precedence is nsIRunaround,
  // nsIInlineReflow, nsIFrame. For all three API's we map the reflow status
  // into an nsInlineReflowStatus.

  nsresult rv;
  nsIRunaround* runAround;
  nsIInlineReflow* inlineReflow;
  if ((nsnull != mLineLayout.mSpaceManager) &&
      (NS_OK == aKidFrame->QueryInterface(kIRunaroundIID,
                                          (void**)&runAround))) {
    nsRect r;
    runAround->Reflow(mLineLayout.mPresContext, mLineLayout.mSpaceManager,
                      aMetrics, aReflowState, r, rv);
    aMetrics.width = r.width;
    aMetrics.height = r.height;
    aMetrics.ascent = r.height;
    aMetrics.descent = 0;
    aInlineAware = PR_FALSE;
  }
  else if (NS_OK == aKidFrame->QueryInterface(kIInlineReflowIID,
                                              (void**)&inlineReflow)) {
    rv = inlineReflow->InlineReflow(mLineLayout, aMetrics, aReflowState);
    aInlineAware = PR_TRUE;
  }
  else {
    aKidFrame->Reflow(mLineLayout.mPresContext, aMetrics, aReflowState, rv);
    aInlineAware = PR_FALSE;
  }

  if (NS_FRAME_IS_COMPLETE(rv)) {
    nsIFrame* kidNextInFlow;
    aKidFrame->GetNextInFlow(kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsIFrame* parent;
      aKidFrame->GetGeometricParent(parent);
      ((nsCSSContainerFrame*)parent)->DeleteNextInFlowsFor(aKidFrame);
    }
  }

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsCSSInlineLayout::ReflowFrame: frame=%p reflowStatus=%x %saware",
      aKidFrame, rv, aInlineAware ? "" :"not "));
  return rv;
}

nsInlineReflowStatus
nsCSSInlineLayout::PlaceFrame(nsIFrame* aFrame,
                              nsRect& aFrameRect,
                              const nsReflowMetrics& aFrameMetrics,
                              const nsMargin& aFrameMargin,
                              nsInlineReflowStatus aFrameReflowStatus)
{
  nscoord horizontalMargins = 0;

  // Special case to position outside list bullets.
  // XXX RTL bullets
  if (mIsBullet) {
    // We are placing the first child of the container and we have
    // list-style-position of "outside" therefore this is the
    // bullet that is being reflowed. The bullet is placed in the
    // padding area of this block. Don't worry about getting the Y
    // coordinate of the bullet right (vertical alignment will
    // take care of that).

    // Compute gap between bullet and inner rect left edge
    nsIFontMetrics* fm =
      mLineLayout.mPresContext->GetMetricsFor(mContainerFont->mFont);
    nscoord kidAscent = fm->GetMaxAscent();
    nscoord dx = fm->GetHeight() / 2;  // from old layout engine
    NS_RELEASE(fm);

    // XXX RTL bullets
    aFrameRect.x = mX - aFrameRect.width - dx;
    aFrame->SetRect(aFrameRect);
  }
  else {
    // Place normal in-flow child
    aFrame->SetRect(aFrameRect);

    // XXX RTL
    // Advance
    const nsStyleDisplay* frameDisplay;
    aFrame->GetStyleData(eStyleStruct_Display,
                         (const nsStyleStruct*&) frameDisplay);
    switch (frameDisplay->mFloats) {
    default:
      NS_NOTYETIMPLEMENTED("Unsupported floater type");
      // FALL THROUGH

    case NS_STYLE_FLOAT_LEFT:
    case NS_STYLE_FLOAT_RIGHT:
      // When something is floated, its margins are applied there
      // not here.
      break;

    case NS_STYLE_FLOAT_NONE:
      horizontalMargins = aFrameMargin.left + aFrameMargin.right;
      break;
    }
    mX += aFrameMetrics.width + horizontalMargins;
  }

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsCSSInlineLayout::PlaceFrame: frame=%p {%d, %d, %d, %d}",
                aFrame,
                aFrameRect.x, aFrameRect.y,
                aFrameRect.width, aFrameRect.height));

  // XXX this is not right; the max-element-size of a child depends on
  // it's margins which it doesn't know how to add in

  // Fold in child's max-element-size information into our own
  if (mComputeMaxElementSize) {
    if (aFrameMetrics.maxElementSize->width > mMaxElementSize.width) {
      mMaxElementSize.width = aFrameMetrics.maxElementSize->width;
    }
    if (aFrameMetrics.maxElementSize->height > mMaxElementSize.height) {
      mMaxElementSize.height = aFrameMetrics.maxElementSize->height;
    }
  }

  if (aFrameMetrics.ascent > mMaxAscent) {
    mMaxAscent = aFrameMetrics.ascent;
  }
  if (aFrameMetrics.descent > mMaxDescent) {
    mMaxDescent = aFrameMetrics.descent;
  }
  nsresult rv = SetAscent(aFrameMetrics.ascent);
  if (NS_OK != rv) {
    return nsInlineReflowStatus(rv);
  }
  mFrameNum++;

  return aFrameReflowStatus;
}

void
nsCSSInlineLayout::AlignFrames(nsIFrame* aFrame, PRInt32 aFrameCount,
                               nsRect& aBounds)
{
  NS_PRECONDITION(aFrameCount == mFrameNum, "bogus reflow");

  // Vertically align the children on the line; this will compute
  // the actual line height for us.

  // XXX Fold in vertical alignment code here and make it update the
  // max ascent and max descent values properly. When line-height is
  // involved, give half of it to ascent and half to descent

  nscoord lineHeight =
    nsCSSLayout::VerticallyAlignChildren(mLineLayout.mPresContext,
                                         mContainerFrame, mContainerFont,
                                         mY, aFrame, aFrameCount,
                                         mAscents, mMaxAscent/* XXX maxDescent */);
  nscoord lineWidth = mX - mLeftEdge;

  // Save away line bounds before other adjustments
  aBounds.x = mLeftEdge;
  aBounds.y = mY;
  aBounds.width = lineWidth;
  aBounds.height = lineHeight;

  // Now horizontally place the children
  if (!mUnconstrainedWidth && (mAvailWidth > lineWidth)) {
    nsCSSLayout::HorizontallyPlaceChildren(mLineLayout.mPresContext,
                                           mContainerFrame,
                                           mContainerText->mTextAlign,
                                           mDirection,
                                           aFrame, aFrameCount,
                                           lineWidth,
                                           mAvailWidth);
  }

  // Last, apply relative positioning
  nsCSSLayout::RelativePositionChildren(mLineLayout.mPresContext,
                                        mContainerFrame,
                                        aFrame, aFrameCount);
}

nsresult
nsCSSInlineLayout::MaybeCreateNextInFlow(nsIFrame*  aFrame,
                                         nsIFrame*& aNextInFlowResult)
{
  aNextInFlowResult = nsnull;

  nsIFrame* nextInFlow;
  aFrame->GetNextInFlow(nextInFlow);
  if (nsnull == nextInFlow) {
    // Create a continuation frame for the child frame and insert it
    // into our lines child list.
    nsIFrame* nextFrame;
    aFrame->GetNextSibling(nextFrame);
    nsIStyleContext* kidSC;
    aFrame->GetStyleContext(mLineLayout.mPresContext, kidSC);
    aFrame->CreateContinuingFrame(mLineLayout.mPresContext, mContainerFrame,
                                  kidSC, nextInFlow);
    NS_RELEASE(kidSC);
    if (nsnull == nextInFlow) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aFrame->SetNextSibling(nextInFlow);
    nextInFlow->SetNextSibling(nextFrame);

    NS_FRAME_LOG(NS_FRAME_TRACE_NEW_FRAMES,
       ("nsCSSInlineLayout::MaybeCreateNextInFlow: frame=%p nextInFlow=%p",
        aFrame, nextInFlow));

    aNextInFlowResult = nextInFlow;
  }
  return NS_OK;
}
