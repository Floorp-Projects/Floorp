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
#ifndef nsCSSBlockFrame_h___
#define nsCSSBlockFrame_h___

#include "nsContainerFrame.h"
#include "nsCSSLineLayout.h"
#include "nsCSSInlineLayout.h"
#include "nsVoidArray.h"
#include "nsISpaceManager.h"

class nsCSSBlockFrame;
struct nsCSSInlineLayout;
struct LineData;

// XXX hide this as soon as list bullet code is cleaned up

struct nsCSSBlockReflowState : public nsReflowState {
  nsCSSBlockReflowState(nsIPresContext*      aPresContext,
                        nsISpaceManager*     aSpaceManager,
                        nsCSSBlockFrame*     aBlock,
                        nsIStyleContext*     aBlockSC,
                        const nsReflowState& aReflowState,
                        nsSize*              aMaxElementSize);

  ~nsCSSBlockReflowState();

  /**
   * Update the mCurrentBand data based on the current mY position.
   */
  void GetAvailableSpace();

  void PlaceCurrentLineFloater(nsIFrame* aFloater);

  void PlaceBelowCurrentLineFloaters(nsVoidArray* aFloaterList);

  void ClearFloaters(PRUint8 aBreakType);

  nsIPresContext* mPresContext;
  nsISpaceManager* mSpaceManager;
  nsCSSBlockFrame* mBlock;
  PRBool mBlockIsPseudo;
  nsCSSBlockFrame* mNextInFlow;
  PRUint8 mTextAlign;
  PRUint8 mDirection;

  nsMargin mBorderPadding;
  nsSize mInnerSize;            // inner area after removing border+padding
  nsSize mStyleSize;
  PRIntn mStyleSizeFlags;
  nscoord mDeltaWidth;
  nscoord mBottomEdge;          // maximum Y

  PRPackedBool mUnconstrainedWidth, mUnconstrainedHeight;
  PRPackedBool mNoWrap;
  nscoord mX, mY;
  nscoord mPrevPosBottomMargin;
  nscoord mPrevNegBottomMargin;
  nscoord mKidXMost;

  nsSize* mMaxElementSize;

  nsCSSLineLayout mLineLayout;

  nsCSSInlineLayout mInlineLayout;
  PRBool mInlineLayoutPrepared;

  nsIFrame* mPrevChild;

  LineData* mFreeList;

  nsVoidArray mPendingFloaters;

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

extern nsresult NS_NewCSSBlockFrame(nsIFrame**  aInstancePtrResult,
                                    nsIContent* aContent,
                                    nsIFrame*   aParent);

#endif /* nsCSSBlockFrame_h___ */
