/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsBlockFrame_h___
#define nsBlockFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsHTMLParts.h"

class nsBlockReflowState;
class nsBulletFrame;
class nsLineBox;
class nsTextRun;
class nsFirstLineFrame;

/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_BLOCK_FRAME_FLOATER_LIST_INDEX 0
#define NS_BLOCK_FRAME_BULLET_LIST_INDEX  1
#define NS_BLOCK_FRAME_LAST_LIST_INDEX    NS_BLOCK_FRAME_BULLET_LIST_INDEX

/**
 * Additional frame-state bits. There are more of these bits
 * defined in nsHTMLParts.h (XXX: note: this should be cleaned up)
 */
#define NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET 0x80000000
#define NS_BLOCK_IS_HTML_PARAGRAPH        0x40000000
#define NS_BLOCK_HAS_FIRST_LETTER_STYLE   0x20000000

#define nsBlockFrameSuper nsHTMLContainerFrame

#define NS_BLOCK_FRAME_CID \
 { 0xa6cf90df, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

extern const nsIID kBlockFrameCID;

// Base class for block and inline frames
class nsBlockFrame : public nsBlockFrameSuper
{
public:
  friend nsresult NS_NewBlockFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags);

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;
  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);
  NS_IMETHOD IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD IsPercentageBase(PRBool& aBase) const;
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer);
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
#ifdef DEBUG
  NS_IMETHOD List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
  NS_IMETHOD VerifyTree() const;
#endif
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);
  NS_IMETHOD ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild);

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);

#ifdef DO_SELECTION
  NS_IMETHOD  HandleEvent(nsIPresContext* aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus* aEventStatus);

  NS_IMETHOD  HandleDrag(nsIPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

  nsIFrame * FindHitFrame(nsBlockFrame * aBlockFrame, 
                          const nscoord aX, const nscoord aY,
                          const nsPoint & aPoint);

#endif

  virtual void DeleteChildsNextInFlow(nsIPresContext* aPresContext,
                                      nsIFrame* aNextInFlow);

  nsIFrame* GetTopBlockChild();

  nsresult UpdateSpaceManager(nsIPresContext* aPresContext,
                              nsISpaceManager* aSpaceManager);

protected:
  nsBlockFrame();
  virtual ~nsBlockFrame();

  nsIStyleContext* GetFirstLetterStyle(nsIPresContext* aPresContext);

  void SetFlags(PRUint32 aFlags) {
    mState &= ~NS_BLOCK_FLAGS_MASK;
    mState |= aFlags;
  }

  PRBool HaveOutsideBullet() const {
    return 0 != (mState & NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET);
  }

  void SlideLine(nsBlockReflowState& aState,
                 nsLineBox* aLine, nscoord aDY);

  PRBool DrainOverflowLines(nsIPresContext* aPresContext);

  virtual PRIntn GetSkipSides() const;

  virtual void ComputeFinalSize(const nsHTMLReflowState& aReflowState,
                                nsBlockReflowState&  aState,
                                nsHTMLReflowMetrics& aMetrics);

  nsresult AddFrames(nsIPresContext* aPresContext,
                     nsIFrame* aFrameList,
                     nsIFrame* aPrevSibling);

  void FixParentAndView(nsIPresContext* aPresContext, nsIFrame* aFrame);

  nsresult DoRemoveFrame(nsIPresContext* aPresContext,
                         nsIFrame* aDeletedFrame);


  nsresult PrepareInitialReflow(nsBlockReflowState& aState);

  nsresult PrepareStyleChangedReflow(nsBlockReflowState& aState);

  nsresult PrepareChildIncrementalReflow(nsBlockReflowState& aState);

  nsresult PrepareResizeReflow(nsBlockReflowState& aState);

  nsresult ReflowDirtyLines(nsBlockReflowState& aState);

  void RecoverStateFrom(nsBlockReflowState& aState,
                        nsLineBox* aLine,
                        nscoord aDeltaY,
                        nsRect* aDamageRect);

  //----------------------------------------
  // Methods for line reflow
  // XXX nuke em

  nsresult ReflowLine(nsBlockReflowState& aState,
                      nsLineBox* aLine,
                      PRBool* aKeepReflowGoing,
                      PRBool aDamageDirtyArea = PR_FALSE);

  nsresult PlaceLine(nsBlockReflowState& aState,
                     nsLineLayout& aLineLayout,
                     nsLineBox* aLine,
                     PRBool* aKeepReflowGoing,
                     PRBool aUpdateMaximumWidth);

  nsresult MarkLineDirty (nsLineBox* aLine, 
                          nsLineBox* aPrevLine);

  // XXX blech
  void PostPlaceLine(nsBlockReflowState& aState,
                     nsLineBox* aLine,
                     const nsSize& aMaxElementSize);

  void ComputeLineMaxElementSize(nsBlockReflowState& aState,
                                 nsLineBox* aLine,
                                 nsSize* aMaxElementSize);

  // XXX where to go
  PRBool ShouldJustifyLine(nsBlockReflowState& aState, nsLineBox* aLine);

  void DeleteLine(nsBlockReflowState& aState, nsLineBox* aLine);

  //----------------------------------------
  // Methods for individual frame reflow

  PRBool ShouldApplyTopMargin(nsBlockReflowState& aState,
                              nsLineBox* aLine);

  nsresult ReflowBlockFrame(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            PRBool* aKeepGoing);

  nsresult ReflowInlineFrames(nsBlockReflowState& aState,
                              nsLineBox* aLine,
                              PRBool* aKeepLineGoing,
                              PRBool aUpdateMaximumWidth = PR_FALSE);

  nsresult DoReflowInlineFrames(nsBlockReflowState& aState,
                                nsLineLayout& aLineLayout,
                                nsLineBox* aLine,
                                PRBool* aKeepReflowGoing,
                                PRUint8* aLineReflowStatus,
                                PRBool aUpdateMaximumWidth);

  nsresult DoReflowInlineFramesAuto(nsBlockReflowState& aState,
                                    nsLineBox* aLine,
                                    PRBool* aKeepReflowGoing,
                                    PRUint8* aLineReflowStatus,
                                    PRBool aUpdateMaximumWidth);

  nsresult DoReflowInlineFramesMalloc(nsBlockReflowState& aState,
                                      nsLineBox* aLine,
                                      PRBool* aKeepReflowGoing,
                                      PRUint8* aLineReflowStatus,
                                      PRBool aUpdateMaximumWidth);

  nsresult ReflowInlineFrame(nsBlockReflowState& aState,
                             nsLineLayout& aLineLayout,
                             nsLineBox* aLine,
                             nsIFrame* aFrame,
                             PRUint8* aLineReflowStatus);

  nsresult ReflowFloater(nsBlockReflowState& aState,
                         nsPlaceholderFrame* aPlaceholder,
                         nsRect& aCombinedRect,
                         nsMargin& aMarginResult,
                         nsMargin& aComputedOffsetsResult);

  //----------------------------------------
  // Methods for pushing/pulling lines/frames

  virtual nsresult CreateContinuationFor(nsBlockReflowState& aState,
                                         nsLineBox* aLine,
                                         nsIFrame* aFrame,
                                         PRBool& aMadeNewFrame);

  nsresult SplitLine(nsBlockReflowState& aState,
                     nsLineLayout& aLineLayout,
                     nsLineBox* aLine,
                     nsIFrame* aFrame);

  nsBlockFrame* FindFollowingBlockFrame(nsIFrame* aFrame);

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
  virtual void PaintChildren(nsIPresContext* aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect,
                             nsFramePaintLayer aWhichLayer);

  void PaintFloaters(nsIPresContext* aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);

  nsLineBox* FindLineFor(nsIFrame* aFrame, nsLineBox** aPrevLineResult,
                         PRBool* aIsFloaterResult);

  void PropogateReflowDamage(nsBlockReflowState& aState,
                             nsLineBox* aLine,
                             const nsRect& aOldCombinedArea,
                             nscoord aDeltaY);

  nsresult ComputeTextRuns(nsIPresContext* aPresContext);

  void BuildFloaterList();

  //----------------------------------------
  // List handling kludge

  void RenumberLists(nsIPresContext* aPresContext);

  PRBool RenumberListsIn(nsIPresContext* aPresContext,
                         nsIFrame* aContainerFrame,
                         PRInt32* aOrdinal);

  PRBool RenumberListsInBlock(nsIPresContext* aPresContext,
                              nsBlockFrame* aContainerFrame,
                              PRInt32* aOrdinal);

  PRBool RenumberListsFor(nsIPresContext* aPresContext, nsIFrame* aKid, PRInt32* aOrdinal);

  PRBool FrameStartsCounterScope(nsIFrame* aFrame);

  nsresult UpdateBulletPosition(nsBlockReflowState& aState);

  void ReflowBullet(nsBlockReflowState& aState,
                    nsHTMLReflowMetrics& aMetrics);

  //----------------------------------------

  nsLineBox* GetOverflowLines(nsIPresContext* aPresContext,
                              PRBool          aRemoveProperty) const;

  nsresult SetOverflowLines(nsIPresContext* aPresContext,
                            nsLineBox*      aOverflowFrames);

  nsIFrame* LastChild();

#ifdef NS_DEBUG
  PRBool IsChild(nsIPresContext* aPresContext, nsIFrame* aFrame);
  void VerifyLines(PRBool aFinalCheckOK);
  void VerifyOverflowSituation(nsIPresContext* aPresContext);
  PRInt32 GetDepth() const;
#endif

  nsLineBox* mLines;

  // Text run information
  nsTextRun* mTextRuns;

  // List of all floaters in this block
  nsFrameList mFloaters;

  // XXX_fix_me: subclass one more time!
  // For list-item frames, this is the bullet frame.
  nsBulletFrame* mBullet;

  friend class nsBlockReflowState;
};

#endif /* nsBlockFrame_h___ */

