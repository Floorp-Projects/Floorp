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

/*
 * rendering object for CSS display:block and display:list-item objects,
 * also used inside table cells
 */

#ifndef nsBlockFrame_h___
#define nsBlockFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsHTMLParts.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsLineBox.h"
#include "nsCSSPseudoElements.h"
#include "nsStyleSet.h"

enum LineReflowStatus {
  // The line was completely reflowed and fit in available width, and we should
  // try to pull up content from the next line if possible.
  LINE_REFLOW_OK,
  // The line was completely reflowed and fit in available width, but we should
  // not try to pull up content from the next line.
  LINE_REFLOW_STOP,
  // We need to reflow the line again at its current vertical position. The
  // new reflow should not try to pull up any frames from the next line.
  LINE_REFLOW_REDO_NO_PULL,
  // We need to reflow the line again at a lower vertical postion where there
  // may be more horizontal space due to different float configuration.
  LINE_REFLOW_REDO_NEXT_BAND,
  // The line did not fit in the available vertical space. Try pushing it to
  // the next page or column if it's not the first line on the current page/column.
  LINE_REFLOW_TRUNCATED
};

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

/**
 * Some invariants:
 * -- The overflow out-of-flows list contains the out-of-
 * flow frames whose placeholders are in the overflow list.
 * -- A given piece of content has at most one placeholder
 * frame in a block's normal child list.
 * -- A given piece of content can have an unlimited number
 * of placeholder frames in the overflow-lines list.
 * -- A line containing a continuation placeholder contains
 * only continuation placeholders.
 * -- While a block is being reflowed, its overflowPlaceholdersList
 * frame property points to an nsFrameList in its
 * nsBlockReflowState. This list contains placeholders for
 * floats whose prev-in-flow is in the block's regular line
 * list. The list is always empty/non-existent after the
 * block has been reflowed.
 * -- In all these frame lists, if there are two frames for
 * the same content appearing in the list, then the frames
 * appear with the prev-in-flow before the next-in-flow.
 * -- While reflowing a block, its overflow line list
 * will usually be empty but in some cases will have lines
 * (while we reflow the block at its shrink-wrap width).
 * In this case any new overflowing content must be
 * prepended to the overflow lines.
 */

// see nsHTMLParts.h for the public block state bits
#define NS_BLOCK_HAS_LINE_CURSOR            0x01000000
#define NS_BLOCK_HAS_OVERFLOW_LINES         0x02000000
#define NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS  0x04000000
#define NS_BLOCK_HAS_OVERFLOW_PLACEHOLDERS  0x08000000

// Set on any block that has descendant frames in the normal
// flow with 'clear' set to something other than 'none'
// (including <BR CLEAR="..."> frames)
#define NS_BLOCK_HAS_CLEAR_CHILDREN         0x10000000

#define nsBlockFrameSuper nsHTMLContainerFrame

#define NS_BLOCK_FRAME_CID \
 { 0xa6cf90df, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

extern const nsIID kBlockFrameCID;

/*
 * Base class for block and inline frames.
 * The block frame has an additional named child list:
 * - "Absolute-list" which contains the absolutely positioned frames
 *
 * @see nsGkAtoms::absoluteList
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

  friend nsIFrame* NS_NewBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags);

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD  AppendFrames(nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  virtual nsIFrame* GetFirstChild(nsIAtom* aListName) const;
  virtual nscoord GetBaseline() const;
  virtual nsIAtom* GetAdditionalChildListName(PRInt32 aIndex) const;
  virtual void Destroy();
  virtual nsSplittableType GetSplittableType() const;
  virtual PRBool IsContainingBlock() const;
  virtual PRBool IsFloatContainingBlock() const;
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);
  virtual void InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aX, nscoord aY, nsIFrame* aForChild,
                                  PRBool aImmediate);
  virtual nsIAtom* GetType() const;
#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
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
  // series of requests that work on lines
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

  virtual void ChildIsDirty(nsIFrame* aChild);
  virtual PRBool IsVisibleInSelection(nsISelection* aSelection);

  virtual PRBool IsEmpty();
  virtual PRBool CachedIsEmpty();
  virtual PRBool IsSelfEmpty();

  virtual void MarkIntrinsicWidthsDirty();
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  virtual void DeleteNextInFlowChild(nsPresContext* aPresContext,
                                     nsIFrame*       aNextInFlow);

  /**
   * Determines whether the collapsed margin carried out of the last
   * line includes the margin-top of a line with clearance (in which
   * case we must avoid collapsing that margin with our bottom margin)
   */
  PRBool CheckForCollapsedBottomMarginFromClearanceLine();

  /** return the topmost block child based on y-index.
    * almost always the first or second line, if there is one.
    * accounts for lines that hold only compressed white space, etc.
    */
  nsIFrame* GetTopBlockChild(nsPresContext *aPresContext);

  // Returns the line containing aFrame, or end_lines() if the frame
  // isn't in the block.
  line_iterator FindLineFor(nsIFrame* aFrame);

  static nsresult GetCurrentLine(nsBlockReflowState *aState, nsLineBox **aOutCurrentLine);

  // Create a contination for aPlaceholder and its out of flow frame and
  // add it to the list of overflow floats
  nsresult SplitPlaceholder(nsBlockReflowState& aState, nsIFrame* aPlaceholder);

  void UndoSplitPlaceholders(nsBlockReflowState& aState,
                             nsIFrame*           aLastPlaceholder);
  
  PRBool HandleOverflowPlaceholdersForPulledFrame(
    nsBlockReflowState& aState, nsIFrame* aFrame);

  PRBool HandleOverflowPlaceholdersOnPulledLine(
    nsBlockReflowState& aState, nsLineBox* aLine);

  static PRBool BlockIsMarginRoot(nsIFrame* aBlock);
  static PRBool BlockNeedsSpaceManager(nsIFrame* aBlock);
  
protected:
  nsBlockFrame(nsStyleContext* aContext)
    : nsHTMLContainerFrame(aContext)
    , mMinWidth(NS_INTRINSIC_WIDTH_UNKNOWN)
    , mPrefWidth(NS_INTRINSIC_WIDTH_UNKNOWN)
  {
#ifdef DEBUG
  InitDebugFlags();
#endif
  }
  virtual ~nsBlockFrame();

#ifdef DEBUG
  already_AddRefed<nsStyleContext> GetFirstLetterStyle(nsPresContext* aPresContext)
  {
    return aPresContext->StyleSet()->
      ProbePseudoStyleFor(mContent,
                          nsCSSPseudoElements::firstLetter, mStyleContext);
  }
#endif

  /*
   * Overides member function of nsHTMLContainerFrame. Needed to handle the 
   * lines in a nsBlockFrame properly.
   */
  virtual void PaintTextDecorationLine(nsIRenderingContext& aRenderingContext,
                                       nsPoint aPt,
                                       nsLineBox* aLine,
                                       nscolor aColor,
                                       nscoord aOffset,
                                       nscoord aAscent,
                                       nscoord aSize);

  void TryAllLines(nsLineList::iterator* aIterator,
                   nsLineList::iterator* aEndIterator,
                   PRBool* aInOverflowLines);

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
    * if aLine is a block, its child floats are added to the state manager
    */
  void SlideLine(nsBlockReflowState& aState,
                 nsLineBox* aLine, nscoord aDY);

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
  nsresult AddFrames(nsIFrame* aFrameList,
                     nsIFrame* aPrevSibling);

#ifdef IBMBIDI
  /**
   * Test whether the frame is a form control in a visual Bidi page.
   * This is necessary for backwards-compatibility, because most visual
   * pages use logical order for form controls so that they will
   * display correctly on native widgets in OSs with Bidi support
   * @param aPresContext the pres context
   * @return whether the frame is a BIDI form control
   */
  PRBool IsVisualFormControl(nsPresContext* aPresContext);
#endif

public:
  /** does all the real work for removing aDeletedFrame from this
    * finds the line containing aFrame.
    * handled continued frames
    * marks lines dirty as needed
    * @param aDestroyFrames if false then we don't actually destroy the
    * frame or its next in flows, we just remove them. This does NOT work
    * on out of flow frames so always use PR_TRUE for out of flows.
    * @param aRemoveOnlyFluidContinuations if true, only in-flows are removed;
    * if false, all continuations are removed.
    */
  nsresult DoRemoveFrame(nsIFrame* aDeletedFrame, PRBool aDestroyFrames = PR_TRUE, 
                         PRBool aRemoveOnlyFluidContinuations = PR_TRUE);
protected:

  /** grab overflow lines from this block's prevInFlow, and make them
    * part of this block's mLines list.
    * @return PR_TRUE if any lines were drained.
    */
  PRBool DrainOverflowLines(nsBlockReflowState& aState);

  /**
    * Remove a float from our float list and also the float cache
    * for the line its placeholder is on.
    */
  line_iterator RemoveFloat(nsIFrame* aFloat);

  void CollectFloats(nsIFrame* aFrame, nsFrameList& aList, nsIFrame** aTail,
                     PRBool aFromOverflow);
  // Remove a float, abs, rel positioned frame from the appropriate block's list
  static void DoRemoveOutOfFlowFrame(nsIFrame* aFrame);

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
   */
  nsresult ReflowLine(nsBlockReflowState& aState,
                      line_iterator aLine,
                      PRBool* aKeepReflowGoing);

  // Return PR_TRUE if aLine gets pushed.
  PRBool PlaceLine(nsBlockReflowState& aState,
                   nsLineLayout&       aLineLayout,
                   line_iterator       aLine,
                   PRBool*             aKeepReflowGoing);

  /**
   * Mark |aLine| dirty, and, if necessary because of possible
   * pull-up, mark the previous line dirty as well.
   */
  nsresult MarkLineDirty(line_iterator aLine);

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
                              PRBool* aKeepLineGoing);

  nsresult DoReflowInlineFrames(nsBlockReflowState& aState,
                                nsLineLayout& aLineLayout,
                                line_iterator aLine,
                                PRBool* aKeepReflowGoing,
                                LineReflowStatus* aLineReflowStatus,
                                PRBool aAllowPullUp);

  nsresult ReflowInlineFrame(nsBlockReflowState& aState,
                             nsLineLayout& aLineLayout,
                             line_iterator aLine,
                             nsIFrame* aFrame,
                             LineReflowStatus* aLineReflowStatus);

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
                     nsIFrame* aFrame,
                     LineReflowStatus* aLineReflowStatus);

  nsresult PullFrame(nsBlockReflowState& aState,
                     line_iterator aLine,
                     nsIFrame*& aFrameResult);

  PRBool PullFrameFrom(nsBlockReflowState& aState,
                       nsLineBox* aLine,
                       nsBlockFrame* aFromContainer,
                       PRBool aFromOverflowLine,
                       nsLineList::iterator aFromLine,
                       nsIFrame*& aFrameResult);

  void PushLines(nsBlockReflowState& aState,
                 nsLineList::iterator aLineBefore);


  void ReparentFloats(nsIFrame* aFirstFrame,
                      nsBlockFrame* aOldParent, PRBool aFromOverflow);

  void PropagateFloatDamage(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            nscoord aDeltaY);

  void CheckFloats(nsBlockReflowState& aState);

  //----------------------------------------
  // List handling kludge

  void RenumberLists(nsPresContext* aPresContext);

  PRBool RenumberListsInBlock(nsPresContext* aPresContext,
                              nsBlockFrame* aContainerFrame,
                              PRInt32* aOrdinal,
                              PRInt32 aDepth);

  PRBool RenumberListsFor(nsPresContext* aPresContext, nsIFrame* aKid, PRInt32* aOrdinal, PRInt32 aDepth);

  static PRBool FrameStartsCounterScope(nsIFrame* aFrame);

  void ReflowBullet(nsBlockReflowState& aState,
                    nsHTMLReflowMetrics& aMetrics);

  //----------------------------------------

public:
  nsLineList* GetOverflowLines() const;
protected:
  nsLineList* RemoveOverflowLines();
  nsresult SetOverflowLines(nsLineList* aOverflowLines);

  nsFrameList* GetOverflowPlaceholders() const;

  /**
   * This class is useful for efficiently modifying the out of flow
   * overflow list. It gives the client direct writable access to
   * the frame list temporarily but ensures that property is only
   * written back if absolutely necessary.
   */
  struct nsAutoOOFFrameList {
    nsFrameList mList;

    nsAutoOOFFrameList(nsBlockFrame* aBlock) :
      mList(aBlock->GetOverflowOutOfFlows().FirstChild()),
      aOldHead(mList.FirstChild()), mBlock(aBlock) {}
    ~nsAutoOOFFrameList() {
      if (mList.FirstChild() != aOldHead) {
        mBlock->SetOverflowOutOfFlows(mList);
      }
    }
  protected:
    nsIFrame* aOldHead;
    nsBlockFrame* mBlock;
  };
  friend struct nsAutoOOFFrameList;

  nsFrameList GetOverflowOutOfFlows() const;
  void SetOverflowOutOfFlows(const nsFrameList& aList);

  nsIFrame* LastChild();

#ifdef NS_DEBUG
  PRBool IsChild(nsIFrame* aFrame);
  void VerifyLines(PRBool aFinalCheckOK);
  void VerifyOverflowSituation();
  PRInt32 GetDepth() const;
#endif

  nscoord mMinWidth, mPrefWidth;

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
  static PRBool gNoisyIntrinsic;
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

#ifdef DEBUG
class AutoNoisyIndenter {
public:
  AutoNoisyIndenter(PRBool aDoIndent) : mIndented(aDoIndent) {
    if (mIndented) {
      nsBlockFrame::gNoiseIndent++;
    }
  }
  ~AutoNoisyIndenter() {
    if (mIndented) {
      nsBlockFrame::gNoiseIndent--;
    }
  }
private:
  PRBool mIndented;
};
#endif

#endif /* nsBlockFrame_h___ */

