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
#ifndef nsInlineReflow_h___
#define nsInlineReflow_h___

#include "nsIFrame.h"
#include "nsIHTMLReflow.h"

class nsHTMLContainerFrame;
class nsLineLayout;
struct nsStyleDisplay;
struct nsStylePosition;
struct nsStyleSpacing;

// XXX Make this class handle x,y offsets from the outer state's
// borderpadding value.

/** A class that implements a css compatible horizontal reflow
 * algorithm. Frames are stacked left to right (or right to left)
 * until the horizontal space runs out. Then the 1 or more frames are
 * vertically aligned, horizontally positioned and relatively
 * positioned.  */
class nsInlineReflow {
public:
  nsInlineReflow(nsLineLayout& aLineLayout,
                 const nsHTMLReflowState& aOuterReflowState,
                 nsHTMLContainerFrame* aOuterFrame,
                 PRBool aOuterIsBlock,
                 PRBool aComputeMaxElementSize);
  ~nsInlineReflow();

  void Init(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  void SetMinLineHeight(nscoord aMinLineHeight) {
    mMinLineHeight = aMinLineHeight;
  }

  void SetNextRCFrame(nsIFrame* aNextRCFrame) {
    mNextRCFrame = aNextRCFrame;
  }

  nsIFrame* GetNextRCFrame() const {
    return mNextRCFrame;
  }

  void UpdateBand(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                  PRBool aPlacedLeftFloater);

  nsresult ReflowFrame(nsIFrame* aFrame,
                       PRBool aIsAdjacentWithTop,
                       nsReflowStatus& aReflowStatus);

  // Add frame into the reflow state as if this code reflowed it so
  // that it can be manipulated by the vertical/horizontal,
  // etc. alignment routines.
  void AddFrame(nsIFrame* aFrame, const nsHTMLReflowMetrics& aMetrics);

  // Take aFrame out of the list of frames to align
  void RemoveFrame(nsIFrame* aFrame);

  void VerticalAlignFrames(nsRect& aLineBox,
                           nscoord& aMaxAscent,
                           nscoord& aMaxDescent);

  void HorizontalAlignFrames(nsRect& aLineBox, PRBool aAllowJustify);

  void TrimTrailingWhiteSpace(nsRect& aLineBox);

  void RelativePositionFrames(nsRect& aCombinedArea);

  PRInt32 GetCurrentFrameNum() const { return mFrameNum; }

  void ChangeFrameCount(PRInt32 aCount) {
    NS_ASSERTION(aCount <= mFrameNum, "bad change frame count");
    mFrameNum = aCount;
  }

  const nsSize& GetMaxElementSize() const { return mMaxElementSize; }

  nscoord GetCarriedOutTopMargin() const { return mCarriedOutTopMargin; }

  nscoord GetCarriedOutBottomMargin() const { return mCarriedOutBottomMargin; }

  nscoord GetTopMargin() const {
    return mFrameData->mMargin.top;
  }

  nscoord GetBottomMargin() const {
    return mFrameData->mMargin.bottom;
  }

  nscoord GetAvailWidth() const {
    return mRightEdge - mX;
  }

  nscoord GetWidth() const {
    return mX - mLeftEdge;
  }

  PRBool IsZeroHeight() const;

protected:
  nsresult SetFrame(nsIFrame* aFrame);

  void ApplyLeftMargin(const nsHTMLReflowState& aReflowState);

  PRBool CanPlaceFrame(const nsHTMLReflowState& aReflowState,
                       nsHTMLReflowMetrics& aMetrics,
                       nsReflowStatus& aStatus);

  void PlaceFrame(nsHTMLReflowMetrics& aMetrics);

  void UpdateFrames();

  void JustifyFrames(nscoord aMaxWidth, nsRect& aLineBox);

  // The outer frame that contains the frames that we reflow.
  const nsHTMLReflowState& mOuterReflowState;
  nsHTMLContainerFrame* mOuterFrame;
  nsISpaceManager* mSpaceManager;
  nsLineLayout& mLineLayout;
  nsIPresContext& mPresContext;
  PRBool mOuterIsBlock;
  nscoord mMinLineHeight;

  PRIntn mFrameNum;

  nsIFrame* mNextRCFrame;

  /*
   * For each frame reflowed, we keep this state around
   */
  struct PerFrameData {
    nsIFrame* mFrame;
    nsCSSFrameType mFrameType;
    nscoord mAscent;            // computed ascent value
    nscoord mDescent;           // computed descent value
    nsMargin mMargin;           // computed margin value
    nsMargin mBorderPadding;    // computed border+padding value
    nsSize mMaxElementSize;     // from frames reflow

    // Location and size of frame after its reflowed but before it is
    // positioned finally by VerticalAlignFrames
    nsRect mBounds;

    // Combined area value from nsHTMLReflowMetrics
    nsRect mCombinedArea;

    PRBool mSplittable;
  };

  PerFrameData* mFrameData;
  PerFrameData* mFrameDataBase;
  PerFrameData mFrameDataBuf[20];
  PRIntn mNumFrameData;

  PRBool mCanBreakBeforeFrame;          // we can break before the frame

  PRBool mComputeMaxElementSize;
  nsSize mMaxElementSize;

  // The frame's computed margin values (includes auto value
  // computation)
  nscoord mRightMargin;/* XXX */
  nscoord mCarriedOutTopMargin;
  nscoord mCarriedOutBottomMargin;

  nscoord mLeftEdge;
  nscoord mX;
  nscoord mRightEdge;

  nscoord mTopEdge;
  nscoord mBottomEdge;

  PRBool mUpdatedBand;
  PRUint8 mPlacedFloaters;
  PRBool mInWord;
  PRUint8 mDirection;
  PRUint8 mTextAlign;
  PRBool mNoWrap;
};

#endif /* nsInlineReflow_h___ */
#endif
