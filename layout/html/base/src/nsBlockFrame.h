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

class nsBlockReflowState;
class nsBulletFrame;
class nsLineBox;
class nsTextRun;

/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_BLOCK_FRAME_FLOATER_LIST_INDEX 0
#define NS_BLOCK_FRAME_BULLET_LIST_INDEX  1
#define NS_BLOCK_FRAME_LAST_LIST_INDEX    NS_BLOCK_FRAME_BULLET_LIST_INDEX

#define nsBaseIBFrameSuper nsHTMLContainerFrame

// Additional nsBaseIBFrame.mFlags bit:
// This is really an inline frame, not a block frame
#define BLOCK_IS_INLINE 0x100

// Base class for block and inline frames
class nsBaseIBFrame : public nsBaseIBFrameSuper
{
public:
  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const;
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD ReResolveStyleContext(nsIPresContext* aPresContext,
                                   nsIStyleContext* aParentContext);
  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame) = 0;
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);
  NS_IMETHOD List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const;
  NS_IMETHOD GetFrameName(nsString& aResult) const = 0;
  NS_IMETHOD VerifyTree() const;
  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD MoveInSpaceManager(nsIPresContext& aPresContext,
                                nsISpaceManager* aSpaceManager,
                                nscoord aDeltaX, nscoord aDeltaY);

#ifdef DO_SELECTION
  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus& aEventStatus);

  NS_IMETHOD  HandleDrag(nsIPresContext& aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);

  nsIFrame * FindHitFrame(nsBaseIBFrame * aBlockFrame, 
                          const nscoord aX, const nscoord aY,
                          const nsPoint & aPoint);

#endif

  virtual PRBool DeleteChildsNextInFlow(nsIPresContext& aPresContext,
                                        nsIFrame* aNextInFlow);

protected:
  nsBaseIBFrame();
  virtual ~nsBaseIBFrame();

  void SetFlags(PRUint32 aFlags) {
    mFlags = aFlags;
  }

  void SlideFrames(nsIPresContext& aPresContext,
                   nsISpaceManager* aSpaceManager,
                   nsLineBox* aLine, nscoord aDY);

  PRBool DrainOverflowLines();

  PRBool RemoveChild(nsLineBox* aLines, nsIFrame* aChild);

  virtual PRIntn GetSkipSides() const;

  virtual void ComputeFinalSize(nsBlockReflowState&  aState,
                                nsHTMLReflowMetrics& aMetrics);

  nsresult InsertNewFrame(nsIPresContext&  aPresContext,
                          nsBaseIBFrame* aParentFrame,
                          nsIFrame* aNewFrame,
                          nsIFrame* aPrevSibling);

  nsresult RemoveFrame(nsBlockReflowState& aState,
                       nsBaseIBFrame* aParentFrame,
                       nsIFrame* aDeletedFrame,
                       nsIFrame* aPrevSibling);

  virtual nsresult PrepareInitialReflow(nsBlockReflowState& aState);

  virtual nsresult PrepareFrameAppendedReflow(nsBlockReflowState& aState);

  virtual nsresult PrepareFrameInsertedReflow(nsBlockReflowState& aState);

  virtual nsresult PrepareFrameRemovedReflow(nsBlockReflowState& aState);

  virtual nsresult PrepareStyleChangedReflow(nsBlockReflowState& aState);

  virtual nsresult PrepareChildIncrementalReflow(nsBlockReflowState& aState);

  virtual nsresult PrepareResizeReflow(nsBlockReflowState& aState);

  virtual nsresult ReflowDirtyLines(nsBlockReflowState& aState);

  nsresult RecoverStateFrom(nsBlockReflowState& aState,
                            nsLineBox* aLine);

  //----------------------------------------
  // Methods for line reflow

  virtual void WillReflowLine(nsBlockReflowState& aState,
                              nsLineBox* aLine);

  virtual nsresult ReflowLine(nsBlockReflowState& aState,
                              nsLineBox* aLine,
                              PRBool& aKeepGoing);

  virtual void DidReflowLine(nsBlockReflowState& aState,
                             nsLineBox* aLine,
                             PRBool aKeepGoing);

  nsresult PlaceLine(nsBlockReflowState& aState,
                     nsLineBox* aLine,
                     PRBool& aKeepGoing);

  // XXX blech
  void PostPlaceLine(nsBlockReflowState& aState,
                     nsLineBox* aLine,
                     const nsSize& aMaxElementSize);

  virtual void DidPlaceLine(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            nscoord aTopMargin, nscoord aBottomMargin,
                            PRBool aKeepGoing);

  // XXX where to go
  PRBool ShouldJustifyLine(nsBlockReflowState& aState, nsLineBox* aLine);

  void FindFloaters(nsLineBox* aLine);

  void DeleteLine(nsBlockReflowState& aState, nsLineBox* aLine);

  //----------------------------------------
  // Methods for individual frame reflow

  virtual void WillReflowFrame(nsBlockReflowState& aState,
                               nsLineBox* aLine,
                               nsIFrame* aFrame);

  nsresult ReflowBlockFrame(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            PRBool& aKeepGoing);

  nsresult ReflowInlineFrame(nsBlockReflowState& aState,
                             nsLineBox* aLine,
                             nsIFrame* aFrame,
                             PRBool& aKeepLineGoing,
                             PRBool& aKeepReflowGoing);

  virtual void DidReflowFrame(nsBlockReflowState& aState,
                              nsLineBox* aLine,
                              nsIFrame* aFrame,
                              nsReflowStatus aStatus);

  void ReflowFloater(nsIPresContext& aPresContext,
                     nsBlockReflowState& aState,
                     nsIFrame* aFloaterFrame,
                     nsHTMLReflowState& aFloaterReflowState);

  //----------------------------------------
  // Methods for pushing/pulling lines/frames

  virtual nsresult CreateContinuationFor(nsBlockReflowState& aState,
                                         nsLineBox* aLine,
                                         nsIFrame* aFrame,
                                         PRBool& aMadeNewFrame);

  nsresult SplitLine(nsBlockReflowState& aState,
                     nsLineBox* aLine,
                     nsIFrame* aFrame);

  nsBaseIBFrame* FindFollowingBlockFrame(nsIFrame* aFrame);

  nsresult PullFrame(nsBlockReflowState& aState,
                     nsLineBox* aLine,
                     nsIFrame*& aFrameResult);

  nsresult PullFrame(nsBlockReflowState& aState,
                     nsLineBox* aToLine,
                     nsLineBox** aFromList,
                     PRBool aUpdateGeometricParent,
                     nsIFrame*& aFrameResult,
                     PRBool& aStopPulling);

  void PushLines(nsBlockReflowState& aState);

  //----------------------------------------
  //XXX
  void PaintChildren(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);
  void PaintFloaters(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);

  nsresult AppendNewFrames(nsIPresContext& aPresContext, nsIFrame*);

  nsLineBox* FindLineFor(nsIFrame* aFrame, PRBool& aIsFloaterResult);

  void PropogateReflowDamage(nsBlockReflowState& aState,
                             nsLineBox* aLine,
                             nscoord aDeltaY);

  nsresult IsComplete();

#ifdef NS_DEBUG
  PRBool IsChild(nsIFrame* aFrame);
#endif

  nsLineBox* mLines;

  nsLineBox* mOverflowLines;

  // XXX subclass!
  PRUint32 mFlags;

  static nsIAtom* gFloaterAtom;
  static nsIAtom* gBulletAtom;

  friend class nsBlockReflowState;
};

//----------------------------------------------------------------------

#define nsBlockFrameSuper nsBaseIBFrame

/**
 * The block frame has two additional named child lists:
 * - "Floater-list" which contains the floated frames
 * - "Bullet-list" which contains the bullet frame
 *
 * @see nsLayoutAtoms::bulletList
 * @see nsLayoutAtoms::floaterList
 */
class nsBlockFrame : public nsBlockFrameSuper
{
public:
  friend nsresult NS_NewBlockFrame(nsIFrame*& aNewFrame, PRUint32 aFlags);

  // nsISupports overrides
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame overrides
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const;
  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom*& aListName) const;
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD ReResolveStyleContext(nsIPresContext* aPresContext,
                                   nsIStyleContext* aParentContext);
  NS_IMETHOD CreateContinuingFrame(nsIPresContext& aCX,
                                   nsIFrame* aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*& aContinuingFrame);
  NS_IMETHOD IsPercentageBase(PRBool& aBase) const;
  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const;

  // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

protected:
  nsBlockFrame();
  virtual ~nsBlockFrame();

  // nsContainerFrame overrides
  virtual void PaintChildren(nsIPresContext&      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect);

  // nsBaseIBFrame overrides
  virtual void ComputeFinalSize(nsBlockReflowState&  aState,
                                nsHTMLReflowMetrics& aMetrics);
  virtual nsresult PrepareInitialReflow(nsBlockReflowState& aState);
  virtual nsresult PrepareFrameAppendedReflow(nsBlockReflowState& aState);
  virtual nsresult PrepareFrameInsertedReflow(nsBlockReflowState& aState);
  virtual nsresult PrepareFrameRemovedReflow(nsBlockReflowState& aState);
  virtual nsresult ReflowDirtyLines(nsBlockReflowState& aState);
  virtual void WillReflowLine(nsBlockReflowState& aState,
                              nsLineBox* aLine);
  virtual void DidPlaceLine(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            nscoord aTopMargin, nscoord aBottomMargin,
                            PRBool aLineReflowStatus);
  virtual void WillReflowFrame(nsBlockReflowState& aState,
                               nsLineBox* aLine,
                               nsIFrame* aFrame);

  void TakeRunInFrames(nsBlockFrame* aRunInFrame);

  nsresult FindTextRuns(nsBlockReflowState& aState);

  void BuildFloaterList();

  void RenumberLists(nsBlockReflowState& aState);

  PRBool ShouldPlaceBullet(nsLineBox* aLine);

  void PlaceBullet(nsBlockReflowState& aState,
                   nscoord aMaxAscent,
                   nscoord aTopMargin);

  void RestoreStyleFor(nsIPresContext& aPresContext, nsIFrame* aFrame);

  nsIStyleContext* mFirstLineStyle;

  nsIStyleContext* mFirstLetterStyle;

  // Text run information
  nsTextRun* mTextRuns;

  // List of all floaters in this block
  nsIFrame* mFloaters;

  // XXX_fix_me: subclass one more time!
  // For list-item frames, this is the bullet frame.
  nsBulletFrame* mBullet;

  //friend class nsBaseIBFrame;
};

#endif /* nsBlockFrame_h___ */

