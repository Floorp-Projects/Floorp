/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsBlockFrame_h___
#define nsBlockFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsHTMLParts.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsLineBox.h"
#include "nsReflowPath.h"
#include "nsCSSPseudoElements.h"
#include "nsStyleSet.h"

class nsBlockReflowState;
class nsBulletFrame;
class nsLineBox;
class nsFirstLineFrame;
class nsILineIterator;
class nsIntervalSet;
/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_BLOCK_FRAME_FLOAT_LIST_INDEX         0
#define NS_BLOCK_FRAME_BULLET_LIST_INDEX        1
#define NS_BLOCK_FRAME_OVERFLOW_LIST_INDEX      2
#define NS_BLOCK_FRAME_OVERFLOW_OOF_LIST_INDEX  3
#define NS_BLOCK_FRAME_ABSOLUTE_LIST_INDEX      4
#define NS_BLOCK_FRAME_LAST_LIST_INDEX      NS_BLOCK_FRAME_ABSOLUTE_LIST_INDEX

#define nsBlockFrameSuper nsHTMLContainerFrame

#define NS_BLOCK_FRAME_CID \
 { 0xa6cf90df, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

extern const nsIID kBlockFrameCID;

/*
 * Base class for block and inline frames.
 * The block frame has an additional named child list:
 * - "Absolute-list" which contains the absolutely positioned frames
 *
 * @see nsLayoutAtoms::absoluteList
 */ 
class nsBlockFrame : public nsBlockFrameSuper
{
public:
  typedef nsLineList::iterator                  line_iterator;
  typedef nsLineList::const_iterator            const_line_iterator;
  typedef nsLineList::reverse_iterator          reverse_line_iterator;
  typedef nsLineList::const_reverse_iterator    const_reverse_line_iterator;

  line_iterator begin_lines() { return mLines.begin(); }
  line_iterator end_lines() { return mLines.end(); }
  const_line_iterator begin_lines() const { return mLines.begin(); }
  const_line_iterator end_lines() const { return mLines.end(); }
  reverse_line_iterator rbegin_lines() { return mLines.rbegin(); }
  reverse_line_iterator rend_lines() { return mLines.rend(); }
  const_reverse_line_iterator rbegin_lines() const { return mLines.rbegin(); }
  const_reverse_line_iterator rend_lines() const { return mLines.rend(); }

  friend nsresult NS_NewBlockFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags);

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD SetInitialChildList(nsPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD  AppendFrames(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  virtual nsIFrame* GetFirstChild(nsIAtom* aListName) const;
  virtual nsIAtom* GetAdditionalChildListName(PRInt32 aIndex) const;
  NS_IMETHOD Destroy(nsPresContext* aPresContext);
  NS_IMETHOD IsSplittable(nsSplittableType& aIsSplittable) const;
  virtual PRBool IsContainingBlock() const;
  NS_IMETHOD Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);
  virtual nsIAtom* GetType() const;
#ifdef DEBUG
  NS_IMETHOD List(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD_(nsFrameState) GetDebugStateBits() const;
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD VerifyTree() const;
#endif

#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif

  // line cursor methods to speed up searching for the line(s)
  // containing a point. The basic idea is that we set the cursor
  // property if the lines' combinedArea.ys and combinedArea.yMosts
  // are non-decreasing (considering only non-empty combinedAreas;
  // empty combinedAreas never participate in event handling or
  // painting), and the block has sufficient number of lines. The
  // cursor property points to a "recently used" line. If we get a
  // series of GetFrameForPoint or Paint requests that work on lines
  // "near" the cursor, then we can find those nearby lines quickly by
  // starting our search at the cursor.

  // Clear out line cursor because we're disturbing the lines (i.e., Reflow)
  void ClearLineCursor();
  // Get the first line that might contain y-coord 'y', or nsnull if you must search
  // all lines. If nonnull is returned then we guarantee that the lines'
  // combinedArea.ys and combinedArea.yMosts are non-decreasing.
  // The actual line returned might not contain 'y', but if not, it is guaranteed
  // to be before any line which does contain 'y'.
  nsLineBox* GetFirstLineContaining(nscoord y);
  // Set the line cursor to our first line. Only call this if you
  // guarantee that the lines' combinedArea.ys and combinedArea.yMosts
  // are non-decreasing.
  void SetupLineCursor();

  nsresult GetFrameForPointUsing(nsPresContext* aPresContext,
                                 const nsPoint& aPoint,
                                 nsIAtom*       aList,
                                 nsFramePaintLayer aWhichLayer,
                                 PRBool         aConsiderSelf,
                                 nsIFrame**     aFrame);
  NS_IMETHOD GetFrameForPoint(nsPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);
  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);
  NS_IMETHOD ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild);

  NS_IMETHOD IsVisibleForPainting(nsPresContext *     aPresContext, 
                                  nsIRenderingContext& aRenderingContext,
                                  PRBool               aCheckVis,
                                  PRBool*              aIsVisible);

  virtual PRBool IsEmpty();

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD AttributeChanged(nsPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DO_SELECTION
  NS_IMETHOD  HandleEvent(nsPresContext* aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus* aEventStatus);

  NS_IMETHOD  HandleDrag(nsPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

  nsIFrame * FindHitFrame(nsBlockFrame * aBlockFrame, 
                          const nscoord aX, const nscoord aY,
                          const nsPoint & aPoint);

#endif

  virtual void DeleteNextInFlowChild(nsPresContext* aPresContext,
                                     nsIFrame*       aNextInFlow);

  /** return the topmost block child based on y-index.
    * almost always the first or second line, if there is one.
    * accounts for lines that hold only compressed white space, etc.
    */
  nsIFrame* GetTopBlockChild(nsPresContext *aPresContext);

  // Returns the line containing aFrame, or end_lines() if the frame
  // isn't in the block.
  line_iterator FindLineFor(nsIFrame* aFrame);

  static nsresult GetCurrentLine(nsBlockReflowState *aState, nsLineBox **aOutCurrentLine);

  inline nscoord GetAscent() { return mAscent; }

  // Create a contination for aPlaceholder and its out of flow frame and
  // add it to the list of overflow floats
  nsresult SplitPlaceholder(nsPresContext& aPresContext, nsIFrame& aPlaceholder);

  void UndoSplitPlaceholders(nsBlockReflowState& aState,
                             nsIFrame*           aLastPlaceholder);
  
  virtual void PaintChild(nsPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsIFrame*            aFrame,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags = 0) {
    nsContainerFrame::PaintChild(aPresContext, aRenderingContext,
                                 aDirtyRect, aFrame, aWhichLayer, aFlags);
  }

protected:
  nsBlockFrame();
  virtual ~nsBlockFrame();

  already_AddRefed<nsStyleContext> GetFirstLetterStyle(nsPresContext* aPresContext)
  {
    return aPresContext->StyleSet()->
      ProbePseudoStyleFor(mContent,
                          nsCSSPseudoElements::firstLetter, mStyleContext);
  }

  /*
   * Overides member function of nsHTMLContainerFrame. Needed to handle the 
   * lines in a nsBlockFrame properly.
   */
  virtual void PaintTextDecorationLines(nsIRenderingContext& aRenderingContext,
                                        nscolor aColor,
                                        nscoord aOffset,
                                        nscoord aAscent,
                                        nscoord aSize);

  /**
   * GetClosestLine will return the line that VERTICALLY owns the point closest to aPoint.y
   * aOrigin is the offset for this block frame to its frame.
   * aPoint is the point to search for.
   * aClosestLine is the result.
   */
  nsresult GetClosestLine(nsILineIterator *aLI, 
                             const nsPoint &aOrigin, 
                             const nsPoint &aPoint, 
                             PRInt32 &aClosestLine);

  void SetFlags(PRUint32 aFlags) {
    mState &= ~NS_BLOCK_FLAGS_MASK;
    mState |= aFlags;
  }

  PRBool HaveOutsideBullet() const {
#if defined(DEBUG) && !defined(DEBUG_rods)
    if(mState & NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET) {
      NS_ASSERTION(mBullet,"NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET flag set and no mBullet");
    }
#endif
    return 0 != (mState & NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET);
  }

  /** move the frames contained by aLine by aDY
    * if aLine is a block, it's child floats are added to the state manager
    */
  void SlideLine(nsBlockReflowState& aState,
                 nsLineBox* aLine, nscoord aDY);

  /** grab overflow lines from this block's prevInFlow, and make them
    * part of this block's mLines list.
    * @return PR_TRUE if any lines were drained.
    */
  PRBool DrainOverflowLines(nsPresContext* aPresContext);

  virtual PRIntn GetSkipSides() const;

  virtual void ComputeFinalSize(const nsHTMLReflowState& aReflowState,
                                nsBlockReflowState&      aState,
                                nsHTMLReflowMetrics&     aMetrics);

  void ComputeCombinedArea(const nsHTMLReflowState& aReflowState,
                           nsHTMLReflowMetrics& aMetrics);

  /** add the frames in aFrameList to this block after aPrevSibling
    * this block thinks in terms of lines, but the frame construction code
    * knows nothing about lines at all. So we need to find the line that
    * contains aPrevSibling and add aFrameList after aPrevSibling on that line.
    * new lines are created as necessary to handle block data in aFrameList.
    */
  nsresult AddFrames(nsPresContext* aPresContext,
                     nsIFrame* aFrameList,
                     nsIFrame* aPrevSibling);

  /** does all the real work for removing aDeletedFrame from this
    * finds the line containing aFrame.
    * handled continued frames
    * marks lines dirty as needed
    */
  nsresult DoRemoveFrame(nsPresContext* aPresContext,
                         nsIFrame* aDeletedFrame);

  // Remove a float, abs, rel positioned frame from the appropriate block's list
  static void DoRemoveOutOfFlowFrame(nsPresContext* aPresContext,
                                     nsIFrame*       aFrame);

  /** set up the conditions necessary for an initial reflow */
  nsresult PrepareInitialReflow(nsBlockReflowState& aState);

  /** set up the conditions necessary for an styleChanged reflow */
  nsresult PrepareStyleChangedReflow(nsBlockReflowState& aState);

  /** set up the conditions necessary for an incremental reflow.
    * the primary task is to mark the minimumly sufficient lines dirty. 
    */
  nsresult PrepareChildIncrementalReflow(nsBlockReflowState& aState);

  /**
   * Retarget an inline incremental reflow from continuing frames that
   * will be destroyed.
   *
   * @param aState |aState.mNextRCFrame| contains the next frame in
   * the reflow path; this will be ``rewound'' to either the target
   * frame's primary frame, or to the first continuation frame after a
   * ``hard break''. In other words, it will be set to the closest
   * continuation which will not be destroyed by the unconstrained
   * reflow. The remaining frames in the reflow path for
   * |aState.mReflowState.reflowCommand| will be altered similarly.
   *
   * @param aLine is initially the line box that contains the target
   * frame. It will be ``rewound'' in lockstep with
   * |aState.mNextRCFrame|.
   *
   * @param aPrevInFlow points to the target frame's prev-in-flow.
   */
  void RetargetInlineIncrementalReflow(nsReflowPath::iterator &aFrame,
                                       line_iterator          &aLine,
                                       nsIFrame               *aPrevInFlow);

  /** set up the conditions necessary for an resize reflow
    * the primary task is to mark the minimumly sufficient lines dirty. 
    */
  nsresult PrepareResizeReflow(nsBlockReflowState& aState);

  /** reflow all lines that have been marked dirty */
  nsresult ReflowDirtyLines(nsBlockReflowState& aState);

  //----------------------------------------
  // Methods for line reflow
  /**
   * Reflow a line.  
   * @param aState           the current reflow state
   * @param aLine            the line to reflow.  can contain a single block frame
   *                         or contain 1 or more inline frames.
   * @param aKeepReflowGoing [OUT] indicates whether the caller should continue to reflow more lines
   * @param aDamageDirtyArea if PR_TRUE, do extra work to mark the changed areas as damaged for painting
   *                         this indicates that frames may have changed size, for example
   */
  nsresult ReflowLine(nsBlockReflowState& aState,
                      line_iterator aLine,
                      PRBool* aKeepReflowGoing,
                      PRBool aDamageDirtyArea = PR_FALSE);

  // Return PR_TRUE if aLine gets pushed.
  PRBool PlaceLine(nsBlockReflowState& aState,
                   nsLineLayout&       aLineLayout,
                   line_iterator       aLine,
                   PRBool*             aKeepReflowGoing,
                   PRBool              aUpdateMaximumWidth);

  /**
   * Mark |aLine| dirty, and, if necessary because of possible
   * pull-up, mark the previous line dirty as well.
   */
  nsresult MarkLineDirty(line_iterator aLine);

  // XXX blech
  void PostPlaceLine(nsBlockReflowState& aState,
                     nsLineBox* aLine,
                     nscoord aMaxElementWidth);

  // XXX where to go
  PRBool ShouldJustifyLine(nsBlockReflowState& aState,
                           line_iterator aLine);

  void DeleteLine(nsBlockReflowState& aState,
                  nsLineList::iterator aLine,
                  nsLineList::iterator aLineEnd);

  //----------------------------------------
  // Methods for individual frame reflow

  PRBool ShouldApplyTopMargin(nsBlockReflowState& aState,
                              nsLineBox* aLine);

  nsresult ReflowBlockFrame(nsBlockReflowState& aState,
                            line_iterator aLine,
                            PRBool* aKeepGoing);

  nsresult ReflowInlineFrames(nsBlockReflowState& aState,
                              line_iterator aLine,
                              PRBool* aKeepLineGoing,
                              PRBool aDamageDirtyArea,
                              PRBool aUpdateMaximumWidth = PR_FALSE);

  nsresult DoReflowInlineFrames(nsBlockReflowState& aState,
                                nsLineLayout& aLineLayout,
                                line_iterator aLine,
                                PRBool* aKeepReflowGoing,
                                PRUint8* aLineReflowStatus,
                                PRBool aUpdateMaximumWidth,
                                PRBool aDamageDirtyArea);

  nsresult ReflowInlineFrame(nsBlockReflowState& aState,
                             nsLineLayout& aLineLayout,
                             line_iterator aLine,
                             nsIFrame* aFrame,
                             PRUint8* aLineReflowStatus);

  // An incomplete aReflowStatus indicates the float should be split
  // but only if the available height is constrained.
  nsresult ReflowFloat(nsBlockReflowState& aState,
                       nsPlaceholderFrame* aPlaceholder,
                       nsFloatCache*       aFloatCache,
                       nsReflowStatus&     aReflowStatus);

  //----------------------------------------
  // Methods for pushing/pulling lines/frames

  virtual nsresult CreateContinuationFor(nsBlockReflowState& aState,
                                         nsLineBox*          aLine,
                                         nsIFrame*           aFrame,
                                         PRBool&             aMadeNewFrame);

  // Push aLine which contains a positioned element that was truncated. Clean up any 
  // placeholders on the same line that were continued. Set aKeepReflowGoing to false. 
  void PushTruncatedPlaceholderLine(nsBlockReflowState& aState,
                                    line_iterator       aLine,
                                    nsIFrame*           aLastPlaceholder,
                                    PRBool&             aKeepReflowGoing);

  nsresult SplitLine(nsBlockReflowState& aState,
                     nsLineLayout& aLineLayout,
                     line_iterator aLine,
                     nsIFrame* aFrame);

  nsresult PullFrame(nsBlockReflowState& aState,
                     line_iterator aLine,
                     PRBool     aDamageDeletedLine,
                     nsIFrame*& aFrameResult);

  nsresult PullFrameFrom(nsBlockReflowState& aState,
                         nsLineBox* aToLine,
                         nsLineList& aFromContainer,
                         nsLineList::iterator aFromLine,
                         PRBool aUpdateGeometricParent,
                         PRBool aDamageDeletedLines,
                         nsIFrame*& aFrameResult);

  void PushLines(nsBlockReflowState& aState,
                 nsLineList::iterator aLineBefore);

  //----------------------------------------
  //XXX
  virtual void PaintChildren(nsPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer,
                             PRUint32             aFlags = 0);

  void PaintFloats(nsPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  void PropagateFloatDamage(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            nscoord aDeltaY);

  void BuildFloatList();

  //----------------------------------------
  // List handling kludge

  void RenumberLists(nsPresContext* aPresContext);

  PRBool RenumberListsInBlock(nsPresContext* aPresContext,
                              nsBlockFrame* aContainerFrame,
                              PRInt32* aOrdinal,
                              PRInt32 aDepth);

  PRBool RenumberListsFor(nsPresContext* aPresContext, nsIFrame* aKid, PRInt32* aOrdinal, PRInt32 aDepth);

  PRBool FrameStartsCounterScope(nsIFrame* aFrame);

  nsresult UpdateBulletPosition(nsBlockReflowState& aState);

  void ReflowBullet(nsBlockReflowState& aState,
                    nsHTMLReflowMetrics& aMetrics);

  //----------------------------------------

  nsLineList* GetOverflowLines() const;
  nsLineList* RemoveOverflowLines() const;
  nsresult SetOverflowLines(nsLineList* aOverflowLines);

  nsFrameList* GetOverflowPlaceholders() const;
  nsFrameList* RemoveOverflowPlaceholders() const;
  nsresult SetOverflowPlaceholders(nsFrameList* aOverflowPlaceholders);

  nsFrameList* GetOverflowOutOfFlows() const;
  nsFrameList* RemoveOverflowOutOfFlows() const;
  nsresult SetOverflowOutOfFlows(nsFrameList* aFloaters);

  nsIFrame* LastChild();

#ifdef NS_DEBUG
  PRBool IsChild(nsIFrame* aFrame);
  void VerifyLines(PRBool aFinalCheckOK);
  void VerifyOverflowSituation();
  PRInt32 GetDepth() const;
#endif

  // Ascent of our first line to support 'vertical-align: baseline' in table-cells
  nscoord mAscent;

  nsLineList mLines;

  // List of all floats in this block
  nsFrameList mFloats;

  // XXX_fix_me: subclass one more time!
  // For list-item frames, this is the bullet frame.
  nsBulletFrame* mBullet;

  friend class nsBlockReflowState;

private:
  nsAbsoluteContainingBlock mAbsoluteContainer;


#ifdef DEBUG
public:
  static PRBool gLamePaintMetrics;
  static PRBool gLameReflowMetrics;
  static PRBool gNoisy;
  static PRBool gNoisyDamageRepair;
  static PRBool gNoisyMaxElementWidth;
  static PRBool gNoisyReflow;
  static PRBool gReallyNoisyReflow;
  static PRBool gNoisySpaceManager;
  static PRBool gVerifyLines;
  static PRBool gDisableResizeOpt;

  static PRInt32 gNoiseIndent;

  static const char* kReflowCommandType[];

protected:
  static void InitDebugFlags();
#endif
};

#endif /* nsBlockFrame_h___ */

