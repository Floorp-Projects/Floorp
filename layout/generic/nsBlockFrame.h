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
 * rendering object for CSS display:block, inline-block, and list-item
 * boxes, also used for various anonymous boxes
 */

#ifndef nsBlockFrame_h___
#define nsBlockFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsHTMLParts.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsLineBox.h"
#include "nsCSSPseudoElements.h"
#include "nsStyleSet.h"
#include "nsFloatManager.h"

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
  // We need to reflow the line again using the floats from its height
  // this reflow, since its height made it hit floats that were not
  // adjacent to its top.
  LINE_REFLOW_REDO_MORE_FLOATS,
  // We need to reflow the line again at a lower vertical postion where there
  // may be more horizontal space due to different float configuration.
  LINE_REFLOW_REDO_NEXT_BAND,
  // The line did not fit in the available vertical space. Try pushing it to
  // the next page or column if it's not the first line on the current page/column.
  LINE_REFLOW_TRUNCATED
};

class nsBlockReflowState;
class nsBlockInFlowLineIterator;
class nsBulletFrame;
class nsLineBox;
class nsFirstLineFrame;
class nsIntervalSet;
/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_BLOCK_LIST_COUNT  (NS_CONTAINER_LIST_COUNT_INCL_OC + 5)

/**
 * Some invariants:
 * -- The overflow out-of-flows list contains the out-of-
 * flow frames whose placeholders are in the overflow list.
 * -- A given piece of content has at most one placeholder
 * frame in a block's normal child list.
 * -- While a block is being reflowed, and from then until
 * its next-in-flow is reflowed it may have a
 * PushedFloatProperty frame property that points to
 * an nsFrameList. This list contains continuations for
 * floats whose prev-in-flow is in the block's regular float
 * list and first-in-flows of floats that did not fit, but
 * whose placeholders are in the block or one of its
 * prev-in-flows.
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

/**
 * Something in the block has changed that requires Bidi resolution to be
 * performed on the block. This flag must be either set on all blocks in a 
 * continuation chain or none of them.
 */
#define NS_BLOCK_NEEDS_BIDI_RESOLUTION      NS_FRAME_STATE_BIT(20)
#define NS_BLOCK_HAS_PUSHED_FLOATS          NS_FRAME_STATE_BIT(21)
#define NS_BLOCK_HAS_LINE_CURSOR            NS_FRAME_STATE_BIT(24)
#define NS_BLOCK_HAS_OVERFLOW_LINES         NS_FRAME_STATE_BIT(25)
#define NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS  NS_FRAME_STATE_BIT(26)

// Set on any block that has descendant frames in the normal
// flow with 'clear' set to something other than 'none'
// (including <BR CLEAR="..."> frames)
#define NS_BLOCK_HAS_CLEAR_CHILDREN         NS_FRAME_STATE_BIT(27)

#define nsBlockFrameSuper nsHTMLContainerFrame

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
  NS_DECL_QUERYFRAME_TARGET(nsBlockFrame)
  NS_DECL_FRAMEARENA_HELPERS

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
  line_iterator line(nsLineBox* aList) { return mLines.begin(aList); }
  reverse_line_iterator rline(nsLineBox* aList) { return mLines.rbegin(aList); }

  friend nsIFrame* NS_NewBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags);

  // This is a child list too, but we let nsBlockReflowState get to it
  // directly too.
  NS_DECLARE_FRAME_PROPERTY(PushedFloatProperty,
                            nsContainerFrame::DestroyFrameList)

  // nsQueryFrame
  NS_DECL_QUERYFRAME

  // nsIFrame
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsFrameList&    aChildList);
  NS_IMETHOD  AppendFrames(nsIAtom*        aListName,
                           nsFrameList&    aFrameList);
  NS_IMETHOD  InsertFrames(nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsFrameList&    aFrameList);
  NS_IMETHOD  RemoveFrame(nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  virtual nsFrameList GetChildList(nsIAtom* aListName) const;
  virtual nscoord GetBaseline() const;
  virtual nscoord GetCaretBaseline() const;
  virtual nsIAtom* GetAdditionalChildListName(PRInt32 aIndex) const;
  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  virtual nsSplittableType GetSplittableType() const;
  virtual PRBool IsContainingBlock() const;
  virtual PRBool IsFloatContainingBlock() const;
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);
  virtual void InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aX, nscoord aY, nsIFrame* aForChild,
                                  PRUint32 aFlags);
  virtual nsIAtom* GetType() const;
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
             ~(nsIFrame::eCanContainOverflowContainers |
               nsIFrame::eBlockFrame));
  }

#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD_(nsFrameState) GetDebugStateBits() const;
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

  // line cursor methods to speed up searching for the line(s)
  // containing a point. The basic idea is that we set the cursor
  // property if the lines' overflowArea.VisualOverflow().ys and
  // overflowArea.VisualOverflow().yMosts are non-decreasing
  // (considering only non-empty overflowArea.VisualOverflow()s; empty
  // overflowArea.VisualOverflow()s never participate in event handling
  // or painting), and the block has sufficient number of lines. The
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

  // Given that we have a bullet, does it actually draw something, i.e.,
  // do we have either a 'list-style-type' or 'list-style-image' that is
  // not 'none'?
  PRBool BulletIsEmpty() const;
  virtual PRBool BulletIsEmptyExternal() const
  {
    return BulletIsEmpty();
  }
  virtual void GetBulletText(nsAString& aText) const;

  virtual void MarkIntrinsicWidthsDirty();
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);

  virtual nsRect ComputeTightBounds(gfxContext* aContext) const;
  
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  virtual nsresult StealFrame(nsPresContext* aPresContext,
                              nsIFrame*      aChild,
                              PRBool         aForceNormal = PR_FALSE);

  virtual void DeleteNextInFlowChild(nsPresContext* aPresContext,
                                     nsIFrame*      aNextInFlow,
                                     PRBool         aDeletingEmptyFrames);

  /**
   * Determines whether the collapsed margin carried out of the last
   * line includes the margin-top of a line with clearance (in which
   * case we must avoid collapsing that margin with our bottom margin)
   */
  PRBool CheckForCollapsedBottomMarginFromClearanceLine();

  static nsresult GetCurrentLine(nsBlockReflowState *aState, nsLineBox **aOutCurrentLine);

  static PRBool BlockIsMarginRoot(nsIFrame* aBlock);
  static PRBool BlockNeedsFloatManager(nsIFrame* aBlock);

  /**
   * Returns whether aFrame is a block frame that will wrap its contents
   * around floats intruding on it from the outside.  (aFrame need not
   * be a block frame, but if it's not, the result will be false.)
   */
  static PRBool BlockCanIntersectFloats(nsIFrame* aFrame);

  /**
   * Returns the width that needs to be cleared past floats for blocks
   * that cannot intersect floats.  aState must already have
   * GetAvailableSpace called on it for the vertical position that we
   * care about (which need not be its current mY)
   */
  struct ReplacedElementWidthToClear {
    nscoord marginLeft, borderBoxWidth, marginRight;
    nscoord MarginBoxWidth() const
      { return marginLeft + borderBoxWidth + marginRight; }
  };
  static ReplacedElementWidthToClear
    WidthToClearPastFloats(nsBlockReflowState& aState,
                           const nsRect& aFloatAvailableSpace,
                           nsIFrame* aFrame);

  /**
   * Creates a contination for aFloat and adds it to the list of overflow floats.
   * Also updates aState.mReflowStatus to include the float's incompleteness.
   * Must only be called while this block frame is in reflow.
   * aFloatStatus must be the float's true, unmodified reflow status.
   * 
   */
  nsresult SplitFloat(nsBlockReflowState& aState,
                      nsIFrame*           aFloat,
                      nsReflowStatus      aFloatStatus);

  /**
   * Walks up the frame tree, starting with aCandidate, and returns the first
   * block frame that it encounters.
   */
  static nsBlockFrame* GetNearestAncestorBlock(nsIFrame* aCandidate);
  
protected:
  nsBlockFrame(nsStyleContext* aContext)
    : nsHTMLContainerFrame(aContext)
    , mMinWidth(NS_INTRINSIC_WIDTH_UNKNOWN)
    , mPrefWidth(NS_INTRINSIC_WIDTH_UNKNOWN)
    , mAbsoluteContainer(nsGkAtoms::absoluteList)
  {
#ifdef DEBUG
  InitDebugFlags();
#endif
  }
  virtual ~nsBlockFrame();

#ifdef DEBUG
#ifdef _IMPL_NS_LAYOUT
  already_AddRefed<nsStyleContext> GetFirstLetterStyle(nsPresContext* aPresContext)
  {
    return aPresContext->StyleSet()->
      ProbePseudoElementStyle(mContent->AsElement(),
                              nsCSSPseudoElements::ePseudo_firstLetter,
                              mStyleContext);
  }
#endif
#endif

  /*
   * Overides member function of nsHTMLContainerFrame. Needed to handle the 
   * lines in a nsBlockFrame properly.
   */
  virtual void PaintTextDecorationLine(gfxContext* aCtx,
                                       const nsPoint& aPt,
                                       nsLineBox* aLine,
                                       nscolor aColor,
                                       gfxFloat aOffset,
                                       gfxFloat aAscent,
                                       gfxFloat aSize,
                                       const PRUint8 aDecoration);

  virtual void AdjustForTextIndent(const nsLineBox* aLine,
                                   nscoord& start,
                                   nscoord& width);

  void TryAllLines(nsLineList::iterator* aIterator,
                   nsLineList::iterator* aStartIterator,
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
                                nsHTMLReflowMetrics&     aMetrics,
                                nscoord*                 aBottomEdgeOfChildren);

  void ComputeOverflowAreas(const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics&     aMetrics,
                            nscoord                  aBottomEdgeOfChildren);

  /** add the frames in aFrameList to this block after aPrevSibling
    * this block thinks in terms of lines, but the frame construction code
    * knows nothing about lines at all. So we need to find the line that
    * contains aPrevSibling and add aFrameList after aPrevSibling on that line.
    * new lines are created as necessary to handle block data in aFrameList.
    * This function will clear aFrameList.
    */
  virtual nsresult AddFrames(nsFrameList& aFrameList, nsIFrame* aPrevSibling);

#ifdef IBMBIDI
  /**
   * Perform Bidi resolution on this frame
   */
  nsresult ResolveBidi();

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
  /**
   * Does all the real work for removing aDeletedFrame
   * -- finds the line containing aDeletedFrame
   * -- removes all aDeletedFrame next-in-flows (or all continuations,
   * if REMOVE_FIXED_CONTINUATIONS is given)
   * -- marks lines dirty as needed
   * -- marks textruns dirty (unless FRAMES_ARE_EMPTY is given, in which
   * case textruns do not need to be dirtied)
   * -- destroys all removed frames
   */
  enum {
    REMOVE_FIXED_CONTINUATIONS = 0x02,
    FRAMES_ARE_EMPTY           = 0x04
  };
  nsresult DoRemoveFrame(nsIFrame* aDeletedFrame, PRUint32 aFlags);

  void ReparentFloats(nsIFrame* aFirstFrame,
                      nsBlockFrame* aOldParent, PRBool aFromOverflow,
                      PRBool aReparentSiblings);

  /** Load all of aFrame's floats into the float manager iff aFrame is not a
   *  block formatting context. Handles all necessary float manager translations;
   *  assumes float manager is in aFrame's parent's coord system.
   *  Safe to call on non-blocks (does nothing).
   */
  static void RecoverFloatsFor(nsIFrame*       aFrame,
                               nsFloatManager& aFloatManager);

protected:

  /** grab overflow lines from this block's prevInFlow, and make them
    * part of this block's mLines list.
    * @return PR_TRUE if any lines were drained.
    */
  PRBool DrainOverflowLines(nsBlockReflowState& aState);

  /** grab pushed floats from this block's prevInFlow, and splice
    * them into this block's mFloats list.
    */
  void DrainPushedFloats(nsBlockReflowState& aState);

  /** Load all our floats into the float manager (without reflowing them).
   *  Assumes float manager is in our own coordinate system.
   */
  void RecoverFloats(nsFloatManager& aFloatManager);

  /** Reflow pushed floats
   */
  nsresult ReflowPushedFloats(nsBlockReflowState& aState,
                              nsOverflowAreas&    aOverflowAreas,
                              nsReflowStatus&     aStatus);

  /** Find any trailing BR clear from the last line of the block (or its PIFs)
   */
  PRUint8 FindTrailingClear();

  /**
    * Remove a float from our float list and also the float cache
    * for the line its placeholder is on.
    */
  line_iterator RemoveFloat(nsIFrame* aFloat);

  void CollectFloats(nsIFrame* aFrame, nsFrameList& aList,
                     PRBool aFromOverflow, PRBool aCollectFromSiblings);
  // Remove a float, abs, rel positioned frame from the appropriate block's list
  static void DoRemoveOutOfFlowFrame(nsIFrame* aFrame);

  /** set up the conditions necessary for an resize reflow
    * the primary task is to mark the minimumly sufficient lines dirty. 
    */
  nsresult PrepareResizeReflow(nsBlockReflowState& aState);

  /** reflow all lines that have been marked dirty */
  nsresult ReflowDirtyLines(nsBlockReflowState& aState);

  /** Mark a given line dirty due to reflow being interrupted on or before it */
  void MarkLineDirtyForInterrupt(nsLineBox* aLine);

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

  // Return false if it needs another reflow because of reduced space
  // between floats that are next to it (but not next to its top), and
  // return true otherwise.
  PRBool PlaceLine(nsBlockReflowState& aState,
                   nsLineLayout&       aLineLayout,
                   line_iterator       aLine,
                   nsFloatManager::SavedState* aFloatStateBeforeLine,
                   nsRect&             aFloatAvailableSpace, /* in-out */
                   nscoord&            aAvailableSpaceHeight, /* in-out */
                   PRBool*             aKeepReflowGoing);

  /**
   * Mark |aLine| dirty, and, if necessary because of possible
   * pull-up, mark the previous line dirty as well. Also invalidates textruns
   * on those lines because the text in the lines might have changed due to
   * addition/removal of frames.
   * @param aLine the line to mark dirty
   * @param aLineList the line list containing that line, null means the line
   *        is in 'mLines' of this frame.
   */
  nsresult MarkLineDirty(line_iterator aLine,
                         const nsLineList* aLineList = nsnull);

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
                                nsFlowAreaRect& aFloatAvailableSpace,
                                nscoord& aAvailableSpaceHeight,
                                nsFloatManager::SavedState*
                                  aFloatStateBeforeLine,
                                PRBool* aKeepReflowGoing,
                                LineReflowStatus* aLineReflowStatus,
                                PRBool aAllowPullUp);

  nsresult ReflowInlineFrame(nsBlockReflowState& aState,
                             nsLineLayout& aLineLayout,
                             line_iterator aLine,
                             nsIFrame* aFrame,
                             LineReflowStatus* aLineReflowStatus);

  // Compute the available width for a float. 
  nsRect AdjustFloatAvailableSpace(nsBlockReflowState& aState,
                                   const nsRect&       aFloatAvailableSpace,
                                   nsIFrame*           aFloatFrame);
  // Computes the border-box width of the float
  nscoord ComputeFloatWidth(nsBlockReflowState& aState,
                            const nsRect&       aFloatAvailableSpace,
                            nsIFrame*           aFloat);
  // An incomplete aReflowStatus indicates the float should be split
  // but only if the available height is constrained.
  // aAdjustedAvailableSpace is the result of calling
  // nsBlockFrame::AdjustFloatAvailableSpace.
  nsresult ReflowFloat(nsBlockReflowState& aState,
                       const nsRect&       aAdjustedAvailableSpace,
                       nsIFrame*           aFloat,
                       nsMargin&           aFloatMargin,
                       // Whether the float's position
                       // (aAdjustedAvailableSpace) has been pushed down
                       // due to the presence of other floats.
                       PRBool              aFloatPushedDown,
                       nsReflowStatus&     aReflowStatus);

  //----------------------------------------
  // Methods for pushing/pulling lines/frames

  /**
   * Create a next-in-flow, if necessary, for aFrame. If a new frame is
   * created, place it in aLine if aLine is not null.
   * @param aState the block reflow state
   * @param aLine where to put a new frame
   * @param aFrame the frame
   * @param aMadeNewFrame PR_TRUE if a new frame was created, PR_FALSE if not
   * @return NS_OK if a next-in-flow already exists or is successfully created
   */
  virtual nsresult CreateContinuationFor(nsBlockReflowState& aState,
                                         nsLineBox*          aLine,
                                         nsIFrame*           aFrame,
                                         PRBool&             aMadeNewFrame);

  // Push aLine, which cannot be placed on this page/column but should
  // fit on a future one.  Set aKeepReflowGoing to false.
  void PushTruncatedLine(nsBlockReflowState& aState,
                         line_iterator       aLine,
                         PRBool&             aKeepReflowGoing);

  nsresult SplitLine(nsBlockReflowState& aState,
                     nsLineLayout& aLineLayout,
                     line_iterator aLine,
                     nsIFrame* aFrame,
                     LineReflowStatus* aLineReflowStatus);

  /**
   * Pull a frame from the next available location (one of our lines or
   * one of our next-in-flows lines).
   * @return the pulled frame or nsnull
   */
  nsIFrame* PullFrame(nsBlockReflowState& aState,
                      line_iterator       aLine);

  /**
   * Try to pull a frame out of a line pointed at by aFromLine.
   *
   * Note: pulling a frame from a line that is a place-holder frame
   * doesn't automatically remove the corresponding float from the
   * line's float array. This happens indirectly: either the line gets
   * emptied (and destroyed) or the line gets reflowed (because we mark
   * it dirty) and the code at the top of ReflowLine empties the
   * array. So eventually, it will be removed, just not right away.
   *
   * @return the pulled frame or nsnull
   */
  nsIFrame* PullFrameFrom(nsBlockReflowState&  aState,
                          nsLineBox*           aLine,
                          nsBlockFrame*        aFromContainer,
                          PRBool               aFromOverflowLine,
                          nsLineList::iterator aFromLine);

  void PushLines(nsBlockReflowState& aState,
                 nsLineList::iterator aLineBefore);

  void PropagateFloatDamage(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            nscoord aDeltaY);

  void CheckFloats(nsBlockReflowState& aState);

  //----------------------------------------
  // List handling kludge

  // If this returns PR_TRUE, the block it's called on should get the
  // NS_FRAME_HAS_DIRTY_CHILDREN bit set on it by the caller; either directly
  // if it's already in reflow, or via calling FrameNeedsReflow() to schedule a
  // reflow.
  PRBool RenumberLists(nsPresContext* aPresContext);

  static PRBool RenumberListsInBlock(nsPresContext* aPresContext,
                                     nsBlockFrame* aBlockFrame,
                                     PRInt32* aOrdinal,
                                     PRInt32 aDepth);

  static PRBool RenumberListsFor(nsPresContext* aPresContext, nsIFrame* aKid, PRInt32* aOrdinal, PRInt32 aDepth);

  static PRBool FrameStartsCounterScope(nsIFrame* aFrame);

  void ReflowBullet(nsBlockReflowState& aState,
                    nsHTMLReflowMetrics& aMetrics,
                    nscoord aLineTop);

  //----------------------------------------

  virtual nsILineIterator* GetLineIterator();

public:
  nsLineList* GetOverflowLines() const;
protected:
  nsLineList* RemoveOverflowLines();
  nsresult SetOverflowLines(nsLineList* aOverflowLines);

  /**
   * This class is useful for efficiently modifying the out of flow
   * overflow list. It gives the client direct writable access to
   * the frame list temporarily but ensures that property is only
   * written back if absolutely necessary.
   */
  struct nsAutoOOFFrameList {
    nsFrameList mList;

    nsAutoOOFFrameList(nsBlockFrame* aBlock)
      : mPropValue(aBlock->GetOverflowOutOfFlows())
      , mBlock(aBlock) {
      if (mPropValue) {
        mList = *mPropValue;
      }
    }
    ~nsAutoOOFFrameList() {
      mBlock->SetOverflowOutOfFlows(mList, mPropValue);
    }
  protected:
    nsFrameList* const mPropValue;
    nsBlockFrame* const mBlock;
  };
  friend struct nsAutoOOFFrameList;

  nsFrameList* GetOverflowOutOfFlows() const;
  void SetOverflowOutOfFlows(const nsFrameList& aList, nsFrameList* aPropValue);

  // Get the pushed floats list
  nsFrameList* GetPushedFloats() const;
  // Get the pushed floats list, or if there is not currently one,
  // make a new empty one.
  nsFrameList* EnsurePushedFloats();
  // Remove and return the pushed floats list.
  nsFrameList* RemovePushedFloats();

#ifdef NS_DEBUG
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
  friend class nsBlockInFlowLineIterator;

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
  static PRBool gNoisyFloatManager;
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

/**
 * Iterates over all lines in the prev-in-flows/next-in-flows of this block.
 */
class nsBlockInFlowLineIterator {
public:
  typedef nsBlockFrame::line_iterator line_iterator;
  nsBlockInFlowLineIterator(nsBlockFrame* aFrame, line_iterator aLine, PRBool aInOverflow);
  /**
   * Set up the iterator to point to the first line found starting from
   * aFrame. Sets aFoundValidLine to false if there is no such line.
   * After aFoundValidLine has returned false, don't call any methods on this
   * object again.
   */
  nsBlockInFlowLineIterator(nsBlockFrame* aFrame, PRBool* aFoundValidLine);
  /**
   * Set up the iterator to point to the line that contains aFindFrame (either
   * directly or indirectly).  If aFrame is out of flow, or contained in an
   * out-of-flow, finds the line containing the out-of-flow's placeholder. If
   * the frame is not found, sets aFoundValidLine to false. After
   * aFoundValidLine has returned false, don't call any methods on this
   * object again.
   */
  nsBlockInFlowLineIterator(nsBlockFrame* aFrame, nsIFrame* aFindFrame,
                            PRBool* aFoundValidLine);

  line_iterator GetLine() { return mLine; }
  PRBool IsLastLineInList();
  nsBlockFrame* GetContainer() { return mFrame; }
  PRBool GetInOverflow() { return mInOverflowLines != nsnull; }

  /**
   * Returns the current line list we're iterating, null means
   * we're iterating |mLines| of the container.
   */
  nsLineList* GetLineList() { return mInOverflowLines; }

  /**
   * Returns the end-iterator of whatever line list we're in.
   */
  line_iterator End();

  /**
   * Returns false if there are no more lines. After this has returned false,
   * don't call any methods on this object again.
   */
  PRBool Next();
  /**
   * Returns false if there are no more lines. After this has returned false,
   * don't call any methods on this object again.
   */
  PRBool Prev();

private:
  nsBlockFrame* mFrame;
  line_iterator mLine;
  nsLineList*   mInOverflowLines;

  /**
   * Moves iterator to next valid line reachable from the current block.
   * Returns false if there are no valid lines.
   */
  PRBool FindValidLine();
};

#endif /* nsBlockFrame_h___ */
