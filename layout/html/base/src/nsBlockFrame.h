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

class  nsTextRun;
class  BulletFrame;
struct LineData;
struct nsBlockReflowState;

/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_BLOCK_FRAME_FLOATER_LIST_INDEX 0
#define NS_BLOCK_FRAME_BULLET_LIST_INDEX  1
#define NS_BLOCK_FRAME_LAST_LIST_INDEX    NS_BLOCK_FRAME_BULLET_LIST_INDEX

#define nsBlockFrameSuper nsHTMLContainerFrame

class nsBlockFrame : public nsBlockFrameSuper
{
public:
  nsBlockFrame();
  ~nsBlockFrame();

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD ReResolveStyleContext(nsIPresContext* aPresContext,
                                   nsIStyleContext* aParentContext);
  NS_IMETHOD FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const;
  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom*& aListName) const;
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);
  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);
  NS_IMETHOD IsPercentageBase(PRBool& aBase) const;
  NS_IMETHOD List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const;
  NS_IMETHOD GetFrameName(nsString& aResult) const;
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

  // nsBlockFrame
  void TakeRunInFrames(nsBlockFrame* aRunInFrame);
  PRBool ShouldPlaceBullet(LineData* aLine);
  void PlaceBullet(nsBlockReflowState& aState,
                   nscoord aMaxAscent,
                   nscoord aTopMargin);

#ifdef DO_SELECTION
  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus& aEventStatus);

  NS_IMETHOD  HandleDrag(nsIPresContext& aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);

  nsIFrame * FindHitFrame(nsBlockFrame * aBlockFrame, 
                          const nscoord aX, const nscoord aY,
                          const nsPoint & aPoint);

#endif

  virtual PRBool DeleteChildsNextInFlow(nsIPresContext& aPresContext,
                                        nsIFrame* aNextInFlow);

  void RestoreStyleFor(nsIPresContext& aPresContext, nsIFrame* aFrame);

  void SetFlags(PRUint32 aFlags) {
    mFlags = aFlags;
  }

  void RecoverLineMargins(nsBlockReflowState& aState,
                          LineData* aPrevLine,
                          nscoord& aTopMarginResult,
                          nscoord& aBottomMarginResult);

  PRUintn CalculateMargins(nsBlockReflowState& aState,
                           LineData* aLine,
                           PRBool aInlineContext,
                           nscoord& aTopMarginResult,
                           nscoord& aBottomMarginResult);

  void SlideFrames(nsIPresContext& aPresContext,
                   nsISpaceManager* aSpaceManager,
                   LineData* aLine, nscoord aDY);

  PRBool DrainOverflowLines();

  PRBool RemoveChild(LineData* aLines, nsIFrame* aChild);

  PRIntn GetSkipSides() const;

  nsresult InitialReflow(nsBlockReflowState& aState);

  nsresult FrameAppendedReflow(nsBlockReflowState& aState);

  nsresult InsertNewFrame(nsIPresContext&  aPresContext,
                          nsBlockFrame*    aParentFrame,
                          nsIFrame*        aNewFrame,
                          nsIFrame*        aPrevSibling);
  nsresult FrameInsertedReflow(nsBlockReflowState& aState);

  nsresult FrameRemovedReflow(nsBlockReflowState& aState);

  nsresult StyleChangedReflow(nsBlockReflowState& aState);

  nsresult FindTextRuns(nsBlockReflowState& aState);

  nsresult ChildIncrementalReflow(nsBlockReflowState& aState);

  nsresult ResizeReflow(nsBlockReflowState& aState);

  void ComputeFinalSize(nsBlockReflowState&  aState,
                        nsHTMLReflowMetrics& aMetrics);

  void BuildFloaterList();

  nsresult ReflowLinesAt(nsBlockReflowState& aState, LineData* aLine);

  PRBool ReflowLine(nsBlockReflowState& aState,
                    LineData* aLine,
                    nsReflowStatus& aReflowResult);

  PRBool PlaceLine(nsBlockReflowState& aState,
                   LineData* aLine,
                   nsReflowStatus aReflowStatus);

  PRBool IsLastLine(nsBlockReflowState& aState,
                    LineData* aLine,
                    nsReflowStatus aReflowStatus);

  void FindFloaters(LineData* aLine);

  void PrepareInlineReflow(nsBlockReflowState& aState, nsIFrame* aFrame, PRBool aIsBlock);

  PRBool ReflowInlineFrame(nsBlockReflowState& aState,
                           LineData* aLine,
                           nsIFrame* aFrame,
                           nsReflowStatus& aResult,
                           PRBool& aAddedToLine);

  nsresult SplitLine(nsBlockReflowState& aState,
                     LineData* aLine,
                     nsIFrame* aFrame);

  nsBlockFrame* FindFollowingBlockFrame(nsIFrame* aFrame);

  PRBool ReflowBlockFrame(nsBlockReflowState& aState,
                          LineData* aLine,
                          nsIFrame* aFrame,
                          nsReflowStatus& aResult);

  PRBool PullFrame(nsBlockReflowState& aState,
                   LineData* aToLine,
                   LineData** aFromList,
                   PRBool aUpdateGeometricParent,
                   nsReflowStatus& aResult,
                   PRBool& aAddedToLine);

  void PushLines(nsBlockReflowState& aState);

  void ReflowFloater(nsIPresContext& aPresContext,
                     nsBlockReflowState& aState,
                     nsIFrame* aFloaterFrame,
                     nsHTMLReflowState& aFloaterReflowState);

  void PaintChildren(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);
  void PaintFloaters(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);

  nsresult AppendNewFrames(nsIPresContext& aPresContext, nsIFrame*);

  void RenumberLists(nsBlockReflowState& aState);

#ifdef NS_DEBUG
  PRBool IsChild(nsIFrame* aFrame);
#endif

  nsIStyleContext* mFirstLineStyle;
  nsIStyleContext* mFirstLetterStyle;

  LineData* mLines;

  LineData* mOverflowLines;

  // Text run information
  nsTextRun* mTextRuns;

  // For list-item frames, this is the bullet frame.
  BulletFrame* mBullet;

  // List of all floaters in this block
  nsIFrame* mFloaters;

  // Body configuration flags passed into this block when this block
  // is used by the body.
  PRUint32 mFlags;

  static nsIAtom* gFloaterAtom;
  static nsIAtom* gBulletAtom;
};

#endif /* nsBlockFrame_h___ */

