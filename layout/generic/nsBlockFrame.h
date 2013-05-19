/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object for CSS display:block, inline-block, and list-item
 * boxes, also used for various anonymous boxes
 */

#ifndef nsBlockFrame_h___
#define nsBlockFrame_h___

#include "nsContainerFrame.h"
#include "nsHTMLParts.h"
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

// This block has had a child marked dirty, so before we reflow we need
// to look through the lines to find any such children and mark
// appropriate lines dirty.
#define NS_BLOCK_LOOK_FOR_DIRTY_FRAMES      NS_FRAME_STATE_BIT(61)

// Are our cached intrinsic widths intrinsic widths for font size
// inflation?  i.e., what was the current state of
// GetPresContext()->mInflationDisabledForShrinkWrap at the time they
// were computed?
// nsBlockFrame is the only thing that caches intrinsic widths that
// needs to track this because it's the only thing that caches intrinsic
// widths that lives inside of things (form controls) that do intrinsic
// sizing with font inflation enabled.
#define NS_BLOCK_FRAME_INTRINSICS_INFLATED  NS_FRAME_STATE_BIT(62)

#define nsBlockFrameSuper nsContainerFrame

/*
 * Base class for block and inline frames.
 * The block frame has an additional child list, kAbsoluteList, which
 * contains the absolutely positioned frames.
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

  friend nsIFrame* NS_NewBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, uint32_t aFlags);

  // nsQueryFrame
  NS_DECL_QUERYFRAME

  // nsIFrame
  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow) MOZ_OVERRIDE;
  NS_IMETHOD SetInitialChildList(ChildListID     aListID,
                                 nsFrameList&    aChildList);
  NS_IMETHOD  AppendFrames(ChildListID     aListID,
                           nsFrameList&    aFrameList);
  NS_IMETHOD  InsertFrames(ChildListID     aListID,
                           nsIFrame*       aPrevFrame,
                           nsFrameList&    aFrameList);
  NS_IMETHOD  RemoveFrame(ChildListID     aListID,
                          nsIFrame*       aOldFrame);
  virtual const nsFrameList& GetChildList(ChildListID aListID) const;
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const;
  virtual nscoord GetBaseline() const MOZ_OVERRIDE;
  virtual nscoord GetCaretBaseline() const MOZ_OVERRIDE;
  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  virtual nsSplittableType GetSplittableType() const;
  virtual bool IsFloatContainingBlock() const;
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;
  virtual nsIAtom* GetType() const;
  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
             ~(nsIFrame::eCanContainOverflowContainers |
               nsIFrame::eBlockFrame));
  }

  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0);
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0);

#ifdef DEBUG
  void List(FILE* out, int32_t aIndent, uint32_t aFlags = 0) const;
  NS_IMETHOD_(nsFrameState) GetDebugStateBits() const;
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
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
  // Get the first line that might contain y-coord 'y', or nullptr if you must search
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
  virtual bool IsVisibleInSelection(nsISelection* aSelection);

  virtual bool IsEmpty();
  virtual bool CachedIsEmpty();
  virtual bool IsSelfEmpty();

  // Given that we have a bullet, does it actually draw something, i.e.,
  // do we have either a 'list-style-type' or 'list-style-image' that is
  // not 'none'?
  bool BulletIsEmpty() const;

  /**
   * Return the bullet text equivalent.
   */
  void GetBulletText(nsAString& aText) const;

  /**
   * Return true if there's a bullet.
   */
  bool HasBullet() const {
    return HasOutsideBullet() || HasInsideBullet();
  }

  /**
   * @return true if this frame has an inside bullet frame.
   */
  bool HasInsideBullet() const {
    return 0 != (mState & NS_BLOCK_FRAME_HAS_INSIDE_BULLET);
  }

  /**
   * @return true if this frame has an outside bullet frame.
   */
  bool HasOutsideBullet() const {
    return 0 != (mState & NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET);
  }

  /**
   * @return the bullet frame or nullptr if we don't have one.
   */
  nsBulletFrame* GetBullet() const {
    nsBulletFrame* outside = GetOutsideBullet();
    return outside ? outside : GetInsideBullet();
  }

  virtual void MarkIntrinsicWidthsDirty();
private:
  void CheckIntrinsicCacheAgainstShrinkWrapState();
public:
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);

  virtual nsRect ComputeTightBounds(gfxContext* aContext) const;
  
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD AttributeChanged(int32_t         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              int32_t         aModType);

  /**
   * Move any frames on our overflow list to the end of our principal list.
   * @return true if there were any overflow frames
   */
  virtual bool DrainSelfOverflowList();

  virtual nsresult StealFrame(nsPresContext* aPresContext,
                              nsIFrame*      aChild,
                              bool           aForceNormal = false);

  virtual void DeleteNextInFlowChild(nsPresContext* aPresContext,
                                     nsIFrame*      aNextInFlow,
                                     bool           aDeletingEmptyFrames);

  /**
   * Determines whether the collapsed margin carried out of the last
   * line includes the margin-top of a line with clearance (in which
   * case we must avoid collapsing that margin with our bottom margin)
   */
  bool CheckForCollapsedBottomMarginFromClearanceLine();

  static nsresult GetCurrentLine(nsBlockReflowState *aState, nsLineBox **aOutCurrentLine);

  /**
   * Determine if this block is a margin root at the top/bottom edges.
   */
  void IsMarginRoot(bool* aTopMarginRoot, bool* aBottomMarginRoot);

  static bool BlockNeedsFloatManager(nsIFrame* aBlock);

  /**
   * Returns whether aFrame is a block frame that will wrap its contents
   * around floats intruding on it from the outside.  (aFrame need not
   * be a block frame, but if it's not, the result will be false.)
   */
  static bool BlockCanIntersectFloats(nsIFrame* aFrame);

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
  
  struct FrameLines {
    nsLineList mLines;
    nsFrameList mFrames;
  };

protected:
  nsBlockFrame(nsStyleContext* aContext)
    : nsContainerFrame(aContext)
    , mMinWidth(NS_INTRINSIC_WIDTH_UNKNOWN)
    , mPrefWidth(NS_INTRINSIC_WIDTH_UNKNOWN)
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

  NS_DECLARE_FRAME_PROPERTY(LineCursorProperty, nullptr)
  nsLineBox* GetLineCursor() {
    return (GetStateBits() & NS_BLOCK_HAS_LINE_CURSOR) ?
      static_cast<nsLineBox*>(Properties().Get(LineCursorProperty())) : nullptr;
  }

  nsLineBox* NewLineBox(nsIFrame* aFrame, bool aIsBlock) {
    return NS_NewLineBox(PresContext()->PresShell(), aFrame, aIsBlock);
  }
  nsLineBox* NewLineBox(nsLineBox* aFromLine, nsIFrame* aFrame, int32_t aCount) {
    return NS_NewLineBox(PresContext()->PresShell(), aFromLine, aFrame, aCount);
  }
  void FreeLineBox(nsLineBox* aLine) {
    if (aLine == GetLineCursor()) {
      ClearLineCursor();
    }
    aLine->Destroy(PresContext()->PresShell());
  }
  /**
   * Helper method for StealFrame.
   */
  void RemoveFrameFromLine(nsIFrame* aChild, nsLineList::iterator aLine,
                           nsFrameList& aFrameList, nsLineList& aLineList);

  void TryAllLines(nsLineList::iterator* aIterator,
                   nsLineList::iterator* aStartIterator,
                   nsLineList::iterator* aEndIterator,
                   bool*        aInOverflowLines,
                   FrameLines** aOverflowLines);

  void SetFlags(nsFrameState aFlags) {
    mState &= ~NS_BLOCK_FLAGS_MASK;
    mState |= aFlags;
  }

  /** move the frames contained by aLine by aDY
    * if aLine is a block, its child floats are added to the state manager
    */
  void SlideLine(nsBlockReflowState& aState,
                 nsLineBox* aLine, nscoord aDY);

  virtual int GetSkipSides() const MOZ_OVERRIDE;

  void ComputeFinalSize(const nsHTMLReflowState& aReflowState,
                        nsBlockReflowState&      aState,
                        nsHTMLReflowMetrics&     aMetrics,
                        nscoord*                 aBottomEdgeOfChildren);

  void ComputeOverflowAreas(const nsRect&         aBounds,
                            const nsStyleDisplay* aDisplay,
                            nscoord               aBottomEdgeOfChildren,
                            nsOverflowAreas&      aOverflowAreas);

  /**
   * Add the frames in aFrameList to this block after aPrevSibling.
   * This block thinks in terms of lines, but the frame construction code
   * knows nothing about lines at all so we need to find the line that
   * contains aPrevSibling and add aFrameList after aPrevSibling on that line.
   * New lines are created as necessary to handle block data in aFrameList.
   * This function will clear aFrameList.
   */
  void AddFrames(nsFrameList& aFrameList, nsIFrame* aPrevSibling);

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
  bool IsVisualFormControl(nsPresContext* aPresContext);
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
  nsresult DoRemoveFrame(nsIFrame* aDeletedFrame, uint32_t aFlags);

  void ReparentFloats(nsIFrame* aFirstFrame, nsBlockFrame* aOldParent,
                      bool aReparentSiblings);

  virtual bool UpdateOverflow();

  /** Load all of aFrame's floats into the float manager iff aFrame is not a
   *  block formatting context. Handles all necessary float manager translations;
   *  assumes float manager is in aFrame's parent's coord system.
   *  Safe to call on non-blocks (does nothing).
   */
  static void RecoverFloatsFor(nsIFrame*       aFrame,
                               nsFloatManager& aFloatManager);

  /**
   * Determine if we have any pushed floats from a previous continuation.
   *
   * @returns true, if any of the floats at the beginning of our mFloats list
   *          have the NS_FRAME_IS_PUSHED_FLOAT bit set; false otherwise.
   */
  bool HasPushedFloatsFromPrevContinuation() const {
    if (!mFloats.IsEmpty()) {
      // If we have pushed floats, then they should be at the beginning of our
      // float list.
      if (mFloats.FirstChild()->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT) {
        return true;
      }
    }

#ifdef DEBUG
    // Double-check the above assertion that pushed floats should be at the
    // beginning of our floats list.
    for (nsFrameList::Enumerator e(mFloats); !e.AtEnd(); e.Next()) {
      nsIFrame* f = e.get();
      NS_ASSERTION(!(f->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT),
        "pushed floats must be at the beginning of the float list");
    }
#endif
    return false;
  }

protected:

  /** grab overflow lines from this block's prevInFlow, and make them
    * part of this block's mLines list.
    * @return true if any lines were drained.
    */
  bool DrainOverflowLines();

  /**
   * @return false iff this block does not have a float on any child list.
   * This function is O(1).
   */
  bool MaybeHasFloats() const {
    if (!mFloats.IsEmpty()) {
      return true;
    }
    // XXX this could be replaced with HasPushedFloats() if we enforced
    // removing the property when the frame list becomes empty.
    nsFrameList* list = GetPushedFloats();
    if (list && !list->IsEmpty()) {
      return true;
    }
    // For the OverflowOutOfFlowsProperty I think we do enforce that, but it's
    // a mix of out-of-flow frames, so that's why the method name has "Maybe".
    return GetStateBits() & NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS;
  }

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
  uint8_t FindTrailingClear();

  /**
   * Remove a float from our float list.
   */
  void RemoveFloat(nsIFrame* aFloat);
  /**
   * Remove a float from the float cache for the line its placeholder is on.
   */
  void RemoveFloatFromFloatCache(nsIFrame* aFloat);

  void CollectFloats(nsIFrame* aFrame, nsFrameList& aList,
                     bool aCollectFromSiblings) {
    if (MaybeHasFloats()) {
      DoCollectFloats(aFrame, aList, aCollectFromSiblings);
    }
  }
  void DoCollectFloats(nsIFrame* aFrame, nsFrameList& aList,
                       bool aCollectFromSiblings);

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
                      bool* aKeepReflowGoing);

  // Return false if it needs another reflow because of reduced space
  // between floats that are next to it (but not next to its top), and
  // return true otherwise.
  bool PlaceLine(nsBlockReflowState& aState,
                   nsLineLayout&       aLineLayout,
                   line_iterator       aLine,
                   nsFloatManager::SavedState* aFloatStateBeforeLine,
                   nsRect&             aFloatAvailableSpace, /* in-out */
                   nscoord&            aAvailableSpaceHeight, /* in-out */
                   bool*             aKeepReflowGoing);

  /**
    * If NS_BLOCK_LOOK_FOR_DIRTY_FRAMES is set, call MarkLineDirty
    * on any line with a child frame that is dirty.
    */
  void LazyMarkLinesDirty();

  /**
   * Mark |aLine| dirty, and, if necessary because of possible
   * pull-up, mark the previous line dirty as well. Also invalidates textruns
   * on those lines because the text in the lines might have changed due to
   * addition/removal of frames.
   * @param aLine the line to mark dirty
   * @param aLineList the line list containing that line
   */
  void MarkLineDirty(line_iterator aLine, const nsLineList* aLineList);

  // XXX where to go
  bool IsLastLine(nsBlockReflowState& aState,
                  line_iterator aLine);

  void DeleteLine(nsBlockReflowState& aState,
                  nsLineList::iterator aLine,
                  nsLineList::iterator aLineEnd);

  //----------------------------------------
  // Methods for individual frame reflow

  bool ShouldApplyTopMargin(nsBlockReflowState& aState,
                              nsLineBox* aLine);

  nsresult ReflowBlockFrame(nsBlockReflowState& aState,
                            line_iterator aLine,
                            bool* aKeepGoing);

  nsresult ReflowInlineFrames(nsBlockReflowState& aState,
                              line_iterator aLine,
                              bool* aKeepLineGoing);

  nsresult DoReflowInlineFrames(nsBlockReflowState& aState,
                                nsLineLayout& aLineLayout,
                                line_iterator aLine,
                                nsFlowAreaRect& aFloatAvailableSpace,
                                nscoord& aAvailableSpaceHeight,
                                nsFloatManager::SavedState*
                                  aFloatStateBeforeLine,
                                bool* aKeepReflowGoing,
                                LineReflowStatus* aLineReflowStatus,
                                bool aAllowPullUp);

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
                       bool                aFloatPushedDown,
                       nsReflowStatus&     aReflowStatus);

  //----------------------------------------
  // Methods for pushing/pulling lines/frames

  /**
   * Create a next-in-flow, if necessary, for aFrame. If a new frame is
   * created, place it in aLine if aLine is not null.
   * @param aState the block reflow state
   * @param aLine where to put a new frame
   * @param aFrame the frame
   * @param aMadeNewFrame true if a new frame was created, false if not
   * @return NS_OK if a next-in-flow already exists or is successfully created
   */
  virtual nsresult CreateContinuationFor(nsBlockReflowState& aState,
                                         nsLineBox*          aLine,
                                         nsIFrame*           aFrame,
                                         bool&             aMadeNewFrame);

  /**
   * Push aLine (and any after it), since it cannot be placed on this
   * page/column.  Set aKeepReflowGoing to false and set
   * flag aState.mReflowStatus as incomplete.
   */
  void PushTruncatedLine(nsBlockReflowState& aState,
                         line_iterator       aLine,
                         bool*               aKeepReflowGoing);

  nsresult SplitLine(nsBlockReflowState& aState,
                     nsLineLayout& aLineLayout,
                     line_iterator aLine,
                     nsIFrame* aFrame,
                     LineReflowStatus* aLineReflowStatus);

  /**
   * Pull a frame from the next available location (one of our lines or
   * one of our next-in-flows lines).
   * @return the pulled frame or nullptr
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
   * @return the pulled frame or nullptr
   */
  nsIFrame* PullFrameFrom(nsLineBox*           aLine,
                          nsBlockFrame*        aFromContainer,
                          nsLineList::iterator aFromLine);

  /**
   * Push the line after aLineBefore to the overflow line list.
   * @param aLineBefore a line in 'mLines' (or begin_lines() when
   *        pushing the first line)
   */
  void PushLines(nsBlockReflowState& aState,
                 nsLineList::iterator aLineBefore);

  void PropagateFloatDamage(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            nscoord aDeltaY);

  void CheckFloats(nsBlockReflowState& aState);

  //----------------------------------------
  // List handling kludge

  // If this returns true, the block it's called on should get the
  // NS_FRAME_HAS_DIRTY_CHILDREN bit set on it by the caller; either directly
  // if it's already in reflow, or via calling FrameNeedsReflow() to schedule a
  // reflow.
  bool RenumberLists(nsPresContext* aPresContext);

  static bool RenumberListsInBlock(nsPresContext* aPresContext,
                                   nsBlockFrame* aBlockFrame,
                                   int32_t* aOrdinal,
                                   int32_t aDepth,
                                   int32_t aIncrement);

  static bool RenumberListsFor(nsPresContext* aPresContext, nsIFrame* aKid,
                               int32_t* aOrdinal, int32_t aDepth,
                               int32_t aIncrement);

  static bool FrameStartsCounterScope(nsIFrame* aFrame);

  void ReflowBullet(nsIFrame* aBulletFrame,
                    nsBlockReflowState& aState,
                    nsHTMLReflowMetrics& aMetrics,
                    nscoord aLineTop);

  //----------------------------------------

  virtual nsILineIterator* GetLineIterator();

public:
  bool HasOverflowLines() const {
    return 0 != (GetStateBits() & NS_BLOCK_HAS_OVERFLOW_LINES);
  }
  FrameLines* GetOverflowLines() const;
protected:
  FrameLines* RemoveOverflowLines();
  void SetOverflowLines(FrameLines* aOverflowLines);
  void DestroyOverflowLines();

  // Determine the computed height that's in effect for this block
  // frame (that is, our computed height minus the heights of our
  // previous in-flows).
  // XXXbz this clearly makes laying out a block with N in-flows
  // O(N^2)!  Good thing the constant is tiny.
  nscoord GetEffectiveComputedHeight(const nsHTMLReflowState& aReflowState) const;

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

  /**
   * @return the inside bullet frame or nullptr if we don't have one.
   */
  nsBulletFrame* GetInsideBullet() const;

  /**
   * @return the outside bullet frame or nullptr if we don't have one.
   */
  nsBulletFrame* GetOutsideBullet() const;

  /**
   * @return the outside bullet frame list frame property.
   */
  nsFrameList* GetOutsideBulletList() const;

  /**
   * @return true if this frame has pushed floats.
   */
  bool HasPushedFloats() const {
    return 0 != (GetStateBits() & NS_BLOCK_HAS_PUSHED_FLOATS);
  }

  // Get the pushed floats list, which is used for *temporary* storage
  // of floats during reflow, between when we decide they don't fit in
  // this block until our next continuation takes them.
  nsFrameList* GetPushedFloats() const;
  // Get the pushed floats list, or if there is not currently one,
  // make a new empty one.
  nsFrameList* EnsurePushedFloats();
  // Remove and return the pushed floats list.
  nsFrameList* RemovePushedFloats();

#ifdef DEBUG
  void VerifyLines(bool aFinalCheckOK);
  void VerifyOverflowSituation();
  int32_t GetDepth() const;
#endif

  nscoord mMinWidth, mPrefWidth;

  nsLineList mLines;

  // List of all floats in this block
  // XXXmats blocks rarely have floats, make it a frame property
  nsFrameList mFloats;

  friend class nsBlockReflowState;
  friend class nsBlockInFlowLineIterator;

#ifdef DEBUG
public:
  static bool gLamePaintMetrics;
  static bool gLameReflowMetrics;
  static bool gNoisy;
  static bool gNoisyDamageRepair;
  static bool gNoisyIntrinsic;
  static bool gNoisyReflow;
  static bool gReallyNoisyReflow;
  static bool gNoisyFloatManager;
  static bool gVerifyLines;
  static bool gDisableResizeOpt;

  static int32_t gNoiseIndent;

  static const char* kReflowCommandType[];

protected:
  static void InitDebugFlags();
#endif
};

#ifdef DEBUG
class AutoNoisyIndenter {
public:
  AutoNoisyIndenter(bool aDoIndent) : mIndented(aDoIndent) {
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
  bool mIndented;
};
#endif

/**
 * Iterates over all lines in the prev-in-flows/next-in-flows of this block.
 */
class nsBlockInFlowLineIterator {
public:
  typedef nsBlockFrame::line_iterator line_iterator;
  /**
   * Set up the iterator to point to aLine which must be a normal line
   * in aFrame (not an overflow line).
   */
  nsBlockInFlowLineIterator(nsBlockFrame* aFrame, line_iterator aLine);
  /**
   * Set up the iterator to point to the first line found starting from
   * aFrame. Sets aFoundValidLine to false if there is no such line.
   * After aFoundValidLine has returned false, don't call any methods on this
   * object again.
   */
  nsBlockInFlowLineIterator(nsBlockFrame* aFrame, bool* aFoundValidLine);
  /**
   * Set up the iterator to point to the line that contains aFindFrame (either
   * directly or indirectly).  If aFrame is out of flow, or contained in an
   * out-of-flow, finds the line containing the out-of-flow's placeholder. If
   * the frame is not found, sets aFoundValidLine to false. After
   * aFoundValidLine has returned false, don't call any methods on this
   * object again.
   */
  nsBlockInFlowLineIterator(nsBlockFrame* aFrame, nsIFrame* aFindFrame,
                            bool* aFoundValidLine);

  line_iterator GetLine() { return mLine; }
  bool IsLastLineInList();
  nsBlockFrame* GetContainer() { return mFrame; }
  bool GetInOverflow() { return mLineList != &mFrame->mLines; }

  /**
   * Returns the current line list we're iterating, null means
   * we're iterating |mLines| of the container.
   */
  nsLineList* GetLineList() { return mLineList; }

  /**
   * Returns the end-iterator of whatever line list we're in.
   */
  line_iterator End();

  /**
   * Returns false if there are no more lines. After this has returned false,
   * don't call any methods on this object again.
   */
  bool Next();
  /**
   * Returns false if there are no more lines. After this has returned false,
   * don't call any methods on this object again.
   */
  bool Prev();

private:
  friend class nsBlockFrame;
  // XXX nsBlockFrame uses this internally in one place.  Try to remove it.
  nsBlockInFlowLineIterator(nsBlockFrame* aFrame, line_iterator aLine, bool aInOverflow);

  nsBlockFrame* mFrame;
  line_iterator mLine;
  nsLineList*   mLineList;  // the line list mLine is in

  /**
   * Moves iterator to next valid line reachable from the current block.
   * Returns false if there are no valid lines.
   */
  bool FindValidLine();
};

#endif /* nsBlockFrame_h___ */
