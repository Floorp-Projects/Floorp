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
#ifndef nsCSSInlineLayout_h___
#define nsCSSInlineLayout_h___

#include "nsCSSContainerFrame.h"
#include "nsIInlineReflow.h"
class nsCSSLineLayout;
struct nsStyleDisplay;
struct nsStyleFont;
struct nsStyleText;

/**
 * This structure contains the horizontal layout state for line
 * layout frame placement. This is factored out of nsCSSLineLayout so
 * that the block layout code and the inline layout code can use
 * nsCSSLineLayout to reflow and place frames.
 */
struct nsCSSInlineLayout {
  nsCSSInlineLayout(nsCSSLineLayout&     aLineLayout,
                    nsCSSContainerFrame* aContainerFrame,
                    nsIStyleContext*     aContainerStyle);
  ~nsCSSInlineLayout();

  void Init(const nsReflowState* aContainerReflowState);

  void Prepare(PRBool aUnconstrainedWidth,
               PRBool aNoWrap,
               PRBool aComputeMaxElementSize);

  void SetReflowSpace(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  nsInlineReflowStatus ReflowAndPlaceFrame(nsIFrame* aFrame);

  /**
   * Align the frames that have been reflowed by this object.
   * aBounds is filled in with the size of the "line" before any
   * horziontal alignment or relative positioning. aBounds.width will
   * be the line width, aBounds.height will be the line height.
   */
  void AlignFrames(nsIFrame* aFrame, PRInt32 aFrameCount, nsRect& aBounds);

  PRBool IsFirstChild();

  nsresult SetAscent(nscoord aAscent);

  PRBool ComputeMaxSize(nsIFrame* aFrame,
                        nsMargin& aKidMargin,
                        nsSize&   aResult);

  nsInlineReflowStatus ReflowFrame(nsIFrame*            aFrame,
                                   nsReflowMetrics&     aDesiredSize,
                                   const nsReflowState& aReflowState,
                                   PRBool&              aInlineAware);

  nsInlineReflowStatus PlaceFrame(nsIFrame* aFrame,
                                  nsRect& kidRect,
                                  const nsReflowMetrics& kidMetrics,
                                  const nsMargin& kidMargin,
                                  nsInlineReflowStatus kidReflowStatus);

  nsresult MaybeCreateNextInFlow(nsIFrame*  aFrame,
                                 nsIFrame*& aNextInFlowResult);

  nsCSSLineLayout& mLineLayout;
  nsCSSContainerFrame* mContainerFrame;
  const nsStyleFont* mContainerFont;
  const nsStyleText* mContainerText;
  const nsStyleDisplay* mContainerDisplay;
  const nsReflowState* mContainerReflowState;
  PRUint8 mDirection;

  PRPackedBool mUnconstrainedWidth;
  PRPackedBool mNoWrap;
  PRPackedBool mComputeMaxElementSize;
  PRPackedBool mIsBullet;
  nscoord mAvailWidth;
  nscoord mAvailHeight;
  nscoord mX, mY;
  nscoord mLeftEdge, mRightEdge;

  PRInt32 mFrameNum;
  nscoord* mAscents;
  nscoord mAscentBuf[20];
  nscoord mMaxAscents;

  nscoord mMaxAscent;
  nscoord mMaxDescent;
  nsSize mMaxElementSize;
};

#endif /* nsCSSInlineLayout_h___ */
