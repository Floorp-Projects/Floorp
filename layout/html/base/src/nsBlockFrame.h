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
#ifndef nsBlockFrame_h___
#define nsBlockFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsLineLayout.h"
#include "nsInlineLayout.h"
#include "nsVoidArray.h"
#include "nsISpaceManager.h"

class nsBlockFrame;
struct nsInlineLayout;
class nsPlaceholderFrame;
struct LineData;

/* 52b33130-0b99-11d2-932e-00805f8add32 */
#define NS_BLOCK_FRAME_CID \
{ 0x52b33130, 0x0b99, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// XXX hide this as soon as list bullet code is cleaned up

struct nsBlockReflowState : public nsReflowState {
  nsBlockReflowState(nsIPresContext*      aPresContext,
                     nsISpaceManager*     aSpaceManager,
                     nsBlockFrame*        aBlock,
                     nsIStyleContext*     aBlockSC,
                     const nsReflowState& aReflowState,
                     nsReflowMetrics&     aMetrics,
                     PRBool               aComputeMaxElementSize);

  ~nsBlockReflowState();

  /**
   * Update the mCurrentBand data based on the current mY position.
   */
  void GetAvailableSpace();

  void AddFloater(nsPlaceholderFrame* aPlaceholderFrame);

  void PlaceFloater(nsPlaceholderFrame* aFloater);

  void PlaceFloaters(nsVoidArray* aFloaters);

  void ClearFloaters(PRUint8 aBreakType);

  PRBool IsLeftMostChild(nsIFrame* aFrame);

  nsIPresContext* mPresContext;
  nsISpaceManager* mSpaceManager;
  nsBlockFrame* mBlock;
  PRBool mBlockIsPseudo;
  nsBlockFrame* mNextInFlow;
  PRUint8 mTextAlign;
  PRUint8 mDirection;

  nsMargin mBorderPadding;
  nsSize mInnerSize;            // inner area after removing border+padding
  nsSize mStyleSize;
  PRIntn mStyleSizeFlags;
  nscoord mDeltaWidth;
  nscoord mBottomEdge;          // maximum Y
  nscoord mBulletPadding;
  nscoord mLeftPadding;

  PRPackedBool mUnconstrainedWidth;
  PRPackedBool mUnconstrainedHeight;
  PRPackedBool mNoWrap;
  PRPackedBool mComputeMaxElementSize;
  nscoord mX, mY;
  nscoord mPrevBottomMargin;
  nscoord mOuterTopMargin;
  nscoord mKidXMost;

  // When a block that contains a block is unconstrained we need to
  // give the inner block a limited width otherwise they don't reflow
  // properly.
  PRPackedBool mHaveBlockMaxWidth;
  nscoord mBlockMaxWidth;

  nsSize mMaxElementSize;

  nsLineLayout mLineLayout;

  nsInlineLayout mInlineLayout;
  PRBool mInlineLayoutPrepared;

  nsIFrame* mPrevChild;

  LineData* mFreeList;

  nsVoidArray mPendingFloaters;
  nscoord mSpaceManagerX, mSpaceManagerY;

  LineData* mCurrentLine;
  LineData* mPrevLine;

  // XXX The next list ordinal for counting list bullets
  PRInt32 mNextListOrdinal;

  // XXX what happens if we need more than 12 trapezoids?
  struct BlockBandData : public nsBandData {
    // Trapezoids used during band processing
    nsBandTrapezoid data[12];

    // Bounding rect of available space between any left and right floaters
    nsRect          availSpace;

    BlockBandData() {
      size = 12;
      trapezoids = data;
    }

    /**
     * Computes the bounding rect of the available space, i.e. space
     * between any left and right floaters Uses the current trapezoid
     * data, see nsISpaceManager::GetBandData(). Also updates member
     * data "availSpace".
     */
    void ComputeAvailSpaceRect();
  };

  BlockBandData mCurrentBand;
};

inline void
nsLineLayout::AddFloater(nsPlaceholderFrame* aFrame)
{
  mBlockReflowState->AddFloater(aFrame);
}

extern nsresult NS_NewBlockFrame(nsIFrame**  aInstancePtrResult,
                                 nsIContent* aContent,
                                 nsIFrame*   aParent);

#endif /* nsCSSBlockFrame_h___ */
