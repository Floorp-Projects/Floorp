/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsBlockFrame_h___
#define nsBlockFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsIFloaterContainer.h"
#include "nsIRunaround.h"
#include "nsISpaceManager.h"
#include "nsLineLayout.h"
#include "nsVoidArray.h"

struct nsMargin;
struct nsStyleDisplay;
struct nsStyleFont;
struct nsStyleText;
class nsBlockFrame;
struct nsBandData;

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

struct nsBlockReflowState {
  nsBlockReflowState();
  ~nsBlockReflowState();

  nsresult Initialize(nsIPresContext* aPresContext,
                      nsISpaceManager* aSpaceManager,
                      const nsSize& aMaxSize,
                      nsSize* aMaxElementSize,
                      nsBlockFrame* aBlock);

  nsresult RecoverState(nsLineData* aLine);

  nsIPresContext* mPresContext;

  nsBlockFrame* mBlock;
  PRBool mBlockIsPseudo;

  // Current line being reflowed
  nsLineLayout* mCurrentLine;

  // Previous line's last child frame
  nsIFrame* mPrevKidFrame;

  // Layout position information
  nscoord mX;
  nscoord mY;
  nsSize mAvailSize;
  nsSize mStyleSize;
  PRIntn mStyleSizeFlags;
  PRPackedBool mUnconstrainedWidth;
  PRPackedBool mUnconstrainedHeight;
  nsSize* mMaxElementSizePointer;
  nscoord mKidXMost;

  // Change in width since last reflow
  nscoord mDeltaWidth;

  // Bottom margin information from the previous line (only when
  // the previous line contains a block element)
  nscoord mPrevNegBottomMargin;
  nscoord mPrevPosBottomMargin;

  // Block frame border+padding information
  nsMargin mBorderPadding;

  // Space manager and current band information
  nsISpaceManager* mSpaceManager;
  nsBlockBandData mCurrentBand;

  // Array of floaters to place below current line
  nsVoidArray mPendingFloaters;

  PRInt32 mNextListOrdinal;
  PRPackedBool mFirstChildIsInsideBullet;
};

//----------------------------------------------------------------------

/* 94e8e410-de21-11d1-89bf-006008911b81 */
#define NS_BLOCKFRAME_CID \
 {0x94e8e410, 0xde21, 0x11d1, {0x89, 0xbf, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81}}

/**
 * <h2>Block Reflow</h2>
 *
 * The block frame reflow machinery performs "2D" layout. Inline
 * elements are flowed into logical lines (left to right or right to
 * left) and the lines are stacked vertically. nsLineLayout is used
 * for this part of the process. Block elements are flowed directly by
 * the block reflow logic after flushing out any preceeding line.<p>
 *
 * During reflow, the block frame will make available to child frames
 * it's reflow state using the presentation shell's cached data
 * mechanism. <p>
 *
 * <h2>Reflowing Mapped Content</h2>
 * <h2>Pullup</h2>
 * <h2>Reflowing Unmapped Content</h2>
 * <h2>Content Insertion Handling</h2>
 * <h2>Content Deletion Handling</h2>
 * <h2>Style Change Handling</h2>
 *
 * <h3>Assertions</h3>
 * <b>mLastContentIsComplete</b> always reflects the state of the last
 * child frame on our chlid list. 
 */

// XXX we don't use nsContainerFrame mOverFlowList!!! wasted memory

class nsBlockFrame : public nsHTMLContainerFrame,
                     public nsIRunaround,
                     public nsIFloaterContainer
{
public:
  /**
   * Create a new block frame that maps the given piece of content.
   */
  static nsresult NewFrame(nsIFrame**  aInstancePtrResult,
                           nsIContent* aContent,
                           nsIFrame*   aParent);

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD ContentAppended(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer);
  NS_IMETHOD ContentInserted(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInParent);
  NS_IMETHOD ContentReplaced(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aOldChild,
                             nsIContent*     aNewChild,
                             PRInt32         aIndexInParent);
  NS_IMETHOD ContentDeleted(nsIPresShell*   aShell,
                            nsIPresContext* aPresContext,
                            nsIContent*     aContainer,
                            nsIContent*     aChild,
                            PRInt32         aIndexInParent);
  NS_IMETHOD GetReflowMetrics(nsIPresContext*  aPresContext,
                              nsReflowMetrics& aMetrics);
  NS_IMETHOD IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD CreateContinuingFrame(nsIPresContext*  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  NS_IMETHOD ListTag(FILE* out) const;
  NS_IMETHOD VerifyTree() const;

  // nsIRunaround
  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsISpaceManager*     aSpaceManager,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsRect&              aDesiredRect,
                    nsReflowStatus&      aStatus);

  // nsIFloaterContainer
  virtual PRBool AddFloater(nsIPresContext*   aPresContext,
                            nsIFrame*         aFloater,
                            PlaceholderFrame* aPlaceholder);
  virtual void PlaceFloater(nsIPresContext*   aPresContext,
                            nsIFrame*         aFloater,
                            PlaceholderFrame* aPlaceholder);

  // nsBlockFrame
  nsresult ReflowInlineChild(nsIFrame*            aKidFrame,
                             nsIPresContext*      aPresContext,
                             nsReflowMetrics&     aDesiredSize,
                             const nsReflowState& aReflowState,
                             nsReflowStatus&      aStatus);

  nsresult ReflowBlockChild(nsIFrame*            aKidFrame,
                            nsIPresContext*      aPresContext,
                            nsISpaceManager*     aSpaceManager,
                            nsReflowMetrics&     aDesiredSize,
                            const nsReflowState& aReflowState,
                            nsRect&              aDesiredRect,
                            nsReflowStatus&      aStatus);

  nsLineData* GetFirstLine();

  static nsBlockReflowState* FindBlockReflowState(nsIPresContext* aPresContext,
                                                  nsIFrame* aFrame);

protected:
  nsBlockFrame(nsIContent* aContent, nsIFrame* aParent);

  virtual ~nsBlockFrame();

  virtual PRIntn GetSkipSides() const;

  virtual void WillDeleteNextInFlowFrame(nsIFrame* aNextInFlow);

  nsresult InitializeState(nsIPresContext*     aPresContext,
                           nsISpaceManager*    aSpaceManager,
                           const nsSize&       aMaxSize,
                           nsSize*             aMaxElementSize,
                           nsBlockReflowState& aState);

  nsresult DoResizeReflow(nsBlockReflowState& aState,
                          const nsSize&       aMaxSize,
                          nsRect&             aDesiredRect,
                          nsReflowStatus&     aStatus);

  void ComputeDesiredRect(nsBlockReflowState& aState,
                          const nsSize&       aMaxSize,
                          nsRect&             aDesiredRect);

  nsLineData* LastLine();
  nsLineData* FindLine(nsIFrame* aFrame);

  nsresult IncrementalReflowAfter(nsBlockReflowState& aState,
                                  nsLineData*         aLine,
                                  nsresult            aReflowStatus,
                                  const nsRect&       aOldBounds);

  void DestroyLines();

  void DrainOverflowList();

  nsLineData* CreateLineForOverflowList(nsIFrame* aOverflowList);

#ifdef NS_DEBUG
  nsresult VerifyLines(PRBool aFinalCheck) const;
#endif

  nsresult PlaceLine(nsBlockReflowState& aState,
                     nsLineLayout&       aLineLayout,
                     nsLineData*         aLine);

  PRBool IsLeftMostChild(nsIFrame* aFrame);

  void PlaceFloater(nsIPresContext*     aPresContext,
                    nsIFrame*           aFloater,
                    PlaceholderFrame*   aPlaceholder,
                    nsBlockReflowState& aState);
  void PlaceBelowCurrentLineFloaters(nsBlockReflowState& aState,
                                     nsVoidArray*        aFloaterList,
                                     nscoord             aY);

  nsresult GetAvailableSpace(nsBlockReflowState& aState, nscoord aY);

  PRBool MoreToReflow(nsBlockReflowState& aState);

  nsresult PushLines(nsBlockReflowState& aState,
                     nsLineData*         aLine);

  nsresult ReflowMapped(nsBlockReflowState& aState);
  nsresult ReflowMappedFrom(nsBlockReflowState& aState, nsLineData* aLine);

  nsresult ReflowUnmapped(nsBlockReflowState& aState);

  nsLineData* mLines;
  nsVoidArray* mRunInFloaters;  // placeholder frames for floaters to display
                                // at the top line
};

#endif /* nsBlockFrame_h___ */
