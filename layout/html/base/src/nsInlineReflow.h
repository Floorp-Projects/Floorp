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

class nsFrameReflowState;
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
                 nsFrameReflowState& aOuterReflowState,
                 nsHTMLContainerFrame* aOuter,
                 PRBool aOuterIsBlock);
  ~nsInlineReflow();

  void Init(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  void SetIsFirstChild(PRBool aValue) {
    mIsFirstChild = aValue;
  }

  void UpdateBand(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                  PRBool aPlacedLeftFloater);

  nsReflowStatus ReflowFrame(nsIFrame* aFrame);

  void VerticalAlignFrames(nsRect& aLineBox,
                           nscoord& aMaxAscent,
                           nscoord& aMaxDescent);

  void HorizontalAlignFrames(const nsRect& aLineBox);

  void RelativePositionFrames();

  PRInt32 GetCurrentFrameNum() const { return mFrameNum; }

  void ChangeFrameCount(PRInt32 aCount) {
    NS_ASSERTION(aCount <= mFrameNum, "bad change frame count");
    mFrameNum = aCount;
  }

  PRBool GetIsBlock() const { return mIsBlock; }

  const nsSize& GetMaxElementSize() const { return mMaxElementSize; }

  nscoord GetCarriedOutTopMargin() const { return mCarriedOutTopMargin; }

  nscoord GetCarriedOutBottomMargin() const { return mCarriedOutBottomMargin; }

  nscoord GetCarriedOutMarginFlags() const { return mCarriedOutMarginFlags; }

  nscoord GetTopMargin() const {
    return mFrameData->mMargin.top;
  }

  nscoord GetBottomMargin() const {
    return mFrameData->mMargin.bottom;
  }

  PRUintn GetMarginFlags() const {
    return mFrameData->mMarginFlags;
  }

  static PRUintn CalculateBlockMarginsFor(nsIPresContext& aPresContext,
                                          nsIFrame* aFrame,
                                          const nsStyleSpacing* aSpacing,
                                          nsMargin& aMargin);

// Return value from CalculateBlockMarginsFor
#define NS_TOP_MARGIN_IS_AUTO    0x1
#define NS_BOTTOM_MARGIN_IS_AUTO 0x2

  static nscoord MaxMargin(nscoord a, nscoord b);

  /**
   * Calculate the line-height value for a given frame.
   */
  static nscoord CalcLineHeightFor(nsIPresContext& aPresContext,
                                   nsIFrame* aFrame,
                                   nscoord aBaseLineHeight);

protected:
  nsresult SetFrame(nsIFrame* aFrame);

  PRBool TreatFrameAsBlockFrame();

  const nsStyleDisplay* GetDisplay();

  const nsStylePosition* GetPosition();

  const nsStyleSpacing* GetSpacing();

  void CalculateMargins();

  void ApplyTopLeftMargins();

  PRBool ComputeAvailableSize();

  PRBool ReflowFrame(nsHTMLReflowMetrics& aMetrics,
                     nsReflowStatus& aStatus);

  PRBool CanPlaceFrame(nsHTMLReflowMetrics& aMetrics,
                       nsReflowStatus& aStatus);

  void PlaceFrame(nsHTMLReflowMetrics& aMetrics);

  void UpdateFrames();

  // The outer frame that contains the frames that we reflow.
  nsHTMLContainerFrame* mOuterFrame;
  nsISpaceManager* mSpaceManager;
  nsLineLayout& mLineLayout;
  nsFrameReflowState& mOuterReflowState;
  nsIPresContext& mPresContext;
  PRBool mOuterIsBlock;

  PRIntn mFrameNum;

  /*
   * For each frame reflowed, we keep this state around
   */
  struct PerFrameData {
    nsIFrame* mFrame;

    nscoord mAscent;            // computed ascent value
    nscoord mDescent;           // computed descent value

    nsMargin mMargin;           // computed margin value
    PRUintn mMarginFlags;

    // Location and size of frame after its reflowed but before it is
    // positioned finally by VerticalAlignFrames
    nsRect mBounds;

    nsSize mMaxElementSize;
  };

  PerFrameData* mFrameData;
  PerFrameData* mFrameDataBase;
  PerFrameData mFrameDataBuf[20];
  PRIntn mNumFrameData;

  // Current frame state
  const nsStyleSpacing* mSpacing;
  const nsStylePosition* mPosition;
  const nsStyleDisplay* mDisplay;
  PRBool mTreatFrameAsBlock;            // treat the frame as a block frame

  // Entire reflow should act like a block; this means that somewhere
  // during reflow we encountered a block frame. There can be more one
  // than one frame in the reflow group (because of empty frames), so
  // this flag keeps us honest regarding the state of this reflow.
  PRBool mIsBlock;

  PRBool mCanBreakBeforeFrame;          // we can break before the frame
  PRBool mIsInlineAware;
  PRBool mIsFirstChild;

  PRBool mComputeMaxElementSize;
  nsSize mMaxElementSize;

  // The frame's computed margin values (includes auto value
  // computation)
  nscoord mRightMargin;/* XXX */
  nscoord mCarriedOutTopMargin;
  nscoord mCarriedOutBottomMargin;
  PRUintn mCarriedOutMarginFlags;

  // The computed available size and location for the frame
  nsSize mFrameAvailSize;

  nscoord mLeftEdge;
  nscoord mX;
  nscoord mRightEdge;

  nscoord mTopEdge;
  nscoord mBottomEdge;

  PRBool mUpdatedBand;
  PRBool mPlacedLeftFloater;
  PRBool mInNBU;
};

inline nscoord
nsInlineReflow::MaxMargin(nscoord a, nscoord b)
{
  if (a < 0) {
    if (b < 0) {
      if (a < b) return a;
      return b;
    }
    return b + a;
  }
  else if (b < 0) {
    return a + b;
  }
  if (a > b) return a;
  return b;
}

#endif /* nsInlineReflow_h___ */
