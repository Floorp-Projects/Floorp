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
#include "nsIInlineReflow.h"

class nsFrameReflowState;
class nsHTMLContainerFrame;
class nsIRunaround;
class nsLineLayout;
struct nsStyleDisplay;
struct nsStylePosition;
struct nsStyleSpacing;

/** A class that implements a css compatible horizontal reflow
 * algorithm. Frames are stacked left to right (or right to left)
 * until the horizontal space runs out. Then the 1 or more frames are
 * vertically aligned, horizontally positioned and relatively
 * positioned.  */
class nsInlineReflow {
public:
  nsInlineReflow(nsLineLayout& aLineLayout,
                 nsFrameReflowState& aOuterReflowState,
                 nsHTMLContainerFrame* aOuter);
  ~nsInlineReflow();

  void Init(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  void SetIsFirstChild(PRBool aValue) {
    mIsFirstChild = aValue;
  }

  void UpdateBand(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  nsInlineReflowStatus ReflowFrame(nsIFrame* aFrame);

  void VerticalAlignFrames(nsRect& aLineBox);

  void HorizontalAlignFrames(const nsRect& aLineBox);

  void RelativePositionFrames();

  nscoord GetMaxAscent() const { return mMaxAscent; }

  nscoord GetMaxDescent() const { return mMaxDescent; }

  PRInt32 GetCurrentFrameNum() const { return mFrameNum; }

  nscoord GetInnerBottomMargin() const { return mInnerBottomMargin; }

  nscoord GetBottomMargin() const { return mBottomMargin; }

  PRBool GetIsBlock() const { return mIsBlock; }

  const nsSize& GetMaxElementSize() const { return mMaxElementSize; }

protected:
  void SetFrame(nsIFrame* aFrame);

  PRBool TreatFrameAsBlockFrame();

  const nsStyleDisplay* GetDisplay();

  const nsStylePosition* GetPosition();

  const nsStyleSpacing* GetSpacing();

  void CalculateMargins();

  void ApplyTopLeftMargins();

  PRBool ComputeAvailableSize();

  PRBool ReflowFrame(nsReflowMetrics& aMetrics,
                     nsRect& aBounds,
                     nsInlineReflowStatus& aStatus);

  PRBool CanPlaceFrame(nsReflowMetrics& aMetrics,
                       nsRect& aBounds,
                       nsInlineReflowStatus& aStatus);

  void PlaceFrame(nsReflowMetrics& aMetrics, nsRect& aBounds);

  nsresult SetFrameData(const nsReflowMetrics& aMetrics);

  // The outer frame that contains the frames that we reflow.
  nsHTMLContainerFrame* mOuterFrame;
  nsISpaceManager* mSpaceManager;
  nsLineLayout& mLineLayout;
  nsFrameReflowState& mOuterReflowState;
  nsIPresContext& mPresContext;

  nsIFrame* mFirstFrame;
  PRIntn mFrameNum;

  struct PerFrameData {
    nscoord mAscent;            // computed ascent value
    nscoord mDescent;           // computed descent value
    nsMargin mMargin;           // computed margin value
  };

  PerFrameData* mFrameData;
  PerFrameData mFrameDataBuf[20];
  PRIntn mNumFrameData;

  nscoord mMaxAscent;
  nscoord mMaxDescent;

  // Current frame state
  nsIFrame* mFrame;
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
  nsMargin mMargin;

  // The computed delta top margin that has been applied for the
  // frame.  This is the collapsed margin value.
  nscoord mDeltaTopMargin;

  // The sum ofthe previous top margin plus additional margin
  // contributed by the frame being reflowed.
  nscoord mTotalTopMargin;

  nscoord mRightMargin;/* XXX why? */
  nscoord mInnerBottomMargin;
  nscoord mBottomMargin;

  // The computed available size and location for the frame
  nscoord mFrameX, mFrameY;
  nsSize mFrameAvailSize;

  nscoord mLeftEdge;
  nscoord mX;
  nscoord mRightEdge;

  nscoord mTopEdge;
  nscoord mY;
  nscoord mBottomEdge;
};

#endif /* nsInlineReflow_h___ */
