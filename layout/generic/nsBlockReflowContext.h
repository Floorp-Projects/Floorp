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
#ifndef nsBlockReflowContext_h___
#define nsBlockReflowContext_h___

#include "nsIHTMLReflow.h"

class nsBlockFrame;
class nsIFrame;
class nsIPresContext;
class nsLineLayout;
struct nsStylePosition;
struct nsStyleSpacing;

#undef SPECULATIVE_TOP_MARGIN

/**
 * An encapsulation of the state and algorithm for reflowing block frames.
 */
class nsBlockReflowContext {
public:
  nsBlockReflowContext(nsIPresContext& aPresContext,
                       const nsHTMLReflowState& aParentRS,
                       PRBool aComputeMaxElementSize);
  ~nsBlockReflowContext() { }

  void SetCompactMarginWidth(nscoord aCompactMarginWidth) {
    mCompactMarginWidth = aCompactMarginWidth;
  }

  void SetNextRCFrame(nsIFrame* aNextRCFrame) {
    mNextRCFrame = aNextRCFrame;
  }

  nsIFrame* GetNextRCFrame() const {
    return mNextRCFrame;
  }

  nsresult ReflowBlock(nsIFrame* aFrame,
                       const nsRect& aSpace,
#ifdef SPECULATIVE_TOP_MARGIN
                       PRBool aApplyTopMargin,
                       nscoord aPrevBottomMargin,
#endif
                       PRBool aIsAdjacentWithTop,
                       nsMargin& aComputedOffsets,
                       nsReflowStatus& aReflowStatus);

  PRBool PlaceBlock(PRBool aForceFit,
#ifndef SPECULATIVE_TOP_MARGIN
                    PRBool aApplyTopMargin,
                    nscoord aPrevBottomMargin,
#endif
                    const nsMargin& aComputedOffsets,
                    nsRect& aInFlowBounds,
                    nsRect& aCombinedRect);

  nscoord GetCollapsedTopMargin() const {
    return mTopMargin;
  }

  nscoord GetCollapsedBottomMargin() const {
    return mBottomMargin;
  }

  nscoord GetCarriedOutTopMargin() const {
    return mMetrics.mCarriedOutTopMargin;
  }

  nscoord GetCarriedOutBottomMargin() const {
    return mMetrics.mCarriedOutBottomMargin;
  }

  const nsSize& GetMaxElementSize() const {
    return mMaxElementSize;
  }

  static void ComputeMarginsFor(nsIPresContext& aPresContext,
                                nsIFrame* aFrame,
                                const nsStyleSpacing* aSpacing,
                                const nsHTMLReflowState& aParentRS,
                                nsMargin& aResult);

  static void CollapseMargins(const nsMargin& aMargin,
                              nscoord aCarriedOutTopMargin,
                              nscoord aCarriedOutBottomMargin,
                              nscoord aFrameHeight,
                              nscoord aPrevBottomMargin,
                              nscoord& aTopMarginResult,
                              nscoord& aBottomMarginResult);

  static nscoord MaxMargin(nscoord a, nscoord b) {
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

protected:
  nsStyleUnit GetRealMarginLeftUnit();
  nsStyleUnit GetRealMarginRightUnit();

  nsIPresContext& mPresContext;
  const nsHTMLReflowState& mOuterReflowState;

  nsIFrame* mFrame;
  nscoord mCompactMarginWidth;
  nsRect mSpace;
  nsIFrame* mNextRCFrame;

  // Spacing style for the frame we are reflowing; only valid after reflow
  const nsStyleSpacing* mStyleSpacing;

  nsMargin mMargin;
  nscoord mX, mY;
  nsHTMLReflowMetrics mMetrics;
  nscoord mSpeculativeTopMargin;
  nscoord mTopMargin;
  nscoord mBottomMargin;
  nsSize mMaxElementSize;
  PRBool mIsTable;
};

#endif /* nsBlockReflowContext_h___ */
