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

#include "nsCSSContainerFrame.h"
#include "nsIFloaterContainer.h"
#include "nsIRunaround.h"
#include "nsISpaceManager.h"
#include "nsVoidArray.h"

struct nsCSSInlineLayout;
class nsPlaceholderFrame;
struct FrameData;

struct nsBlockBandData : public nsBandData {
  // Trapezoids used during band processing
  nsBandTrapezoid data[12];

  // Bounding rect of available space between any left and right floaters
  nsRect          availSpace;

  nsBlockBandData() {
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

//----------------------------------------------------------------------

struct nsCSSBlockReflowState : public nsReflowState {
  nsCSSBlockReflowState(nsIPresContext*      aPresContext,
                   nsISpaceManager*     aSpaceManager,
                   nsCSSContainerFrame* aBlock,
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
  nsBlockBandData mCurrentBand;
  nsCSSContainerFrame* mBlock;
  PRBool mBlockIsPseudo;
  nsIStyleContext* mBlockSC;

  nsMargin mBorderPadding;
  nsSize mAvailSize;
  nsSize mStyleSize;
  PRIntn mStyleSizeFlags;
  nscoord mDeltaWidth;

  PRPackedBool mUnconstrainedWidth, mUnconstrainedHeight;
  PRPackedBool mNoWrap;
  nscoord mX, mY;
  nscoord mPrevPosBottomMargin;
  nscoord mPrevNegBottomMargin;
  nscoord mKidXMost;

  nsSize* mMaxElementSize;

  nsVoidArray mPendingFloaters;

  nsCSSInlineLayout* mCurrentLine;

  // XXX The next list ordinal for counting list bullets
  PRInt32 mNextListOrdinal;
};

//----------------------------------------------------------------------

class nsCSSBlockFrame : public nsCSSContainerFrame,
                        public nsIRunaround,
                        public nsIFloaterContainer
{
public:
  static nsresult NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);
  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD ListTag(FILE* out) const;
  // XXX implement regular reflow method too!

  // nsIRunaround
  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsISpaceManager*     aSpaceManager,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsRect&              aDesiredRect,
                    nsReflowStatus&      aStatus);

  // nsIFloaterContainer
  virtual PRBool AddFloater(nsIPresContext*      aPresContext,
                            const nsReflowState& aPlaceholderReflowState,
                            nsIFrame*            aFloater,
                            nsPlaceholderFrame*  aPlaceholder);

protected:
  nsCSSBlockFrame(nsIContent* aContent, nsIFrame* aParent);
  ~nsCSSBlockFrame();

  virtual void WillDeleteNextInFlowFrame(nsIFrame* aNextInFlow);

  virtual PRIntn GetSkipSides() const;

  void RecoverState(nsCSSBlockReflowState& aStatus, FrameData* fp);

  nsresult FrameAppendedReflow(nsCSSBlockReflowState& aState);

  nsresult ChildIncrementalReflow(nsCSSBlockReflowState& aState);

  nsresult ResizeReflow(nsCSSBlockReflowState& aState);

  void ComputeFinalSize(nsCSSBlockReflowState& aState,
                        nsReflowMetrics&       aMetrics,
                        nsRect&                aDesiredRect);

  void ReflowFloater(nsIPresContext*        aPresContext,
                     nsCSSBlockReflowState& aState,
                     nsIFrame*              aFloaterFrame);

  PRBool IsLeftMostChild(nsIFrame* aFrame);

  FrameData* mFrames;

  // placeholder frames for floaters to display at the top line
  nsVoidArray* mRunInFloaters;
};

#endif /* nsCSSBlockFrame_h___ */
