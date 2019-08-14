/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsFloatManager.h"

enum class LineReflowStatus {
  // The line was completely reflowed and fit in available width, and we should
  // try to pull up content from the next line if possible.
  OK,
  // The line was completely reflowed and fit in available width, but we should
  // not try to pull up content from the next line.
  Stop,
  // We need to reflow the line again at its current vertical position. The
  // new reflow should not try to pull up any frames from the next line.
  RedoNoPull,
  // We need to reflow the line again using the floats from its height
  // this reflow, since its height made it hit floats that were not
  // adjacent to its top.
  RedoMoreFloats,
  // We need to reflow the line again at a lower vertical postion where there
  // may be more horizontal space due to different float configuration.
  RedoNextBand,
  // The line did not fit in the available vertical space. Try pushing it to
  // the next page or column if it's not the first line on the current
  // page/column.
  Truncated
};

class nsBlockInFlowLineIterator;
namespace mozilla {
class BlockReflowInput;
class PresShell;
class ServoRestyleState;
class ServoStyleSet;
}  // namespace mozilla

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

/*
 * Base class for block and inline frames.
 * The block frame has an additional child list, kAbsoluteList, which
 * contains the absolutely positioned frames.
 */
class nsBlockFrame : public nsContainerFrame {
  using BlockReflowInput = mozilla::BlockReflowInput;

 public:
  NS_DECL_FRAMEARENA_HELPERS(nsBlockFrame)

  typedef nsLineList::iterator LineIterator;
  typedef nsLineList::const_iterator ConstLineIterator;
  typedef nsLineList::reverse_iterator ReverseLineIterator;
  typedef nsLineList::const_reverse_iterator ConstReverseLineIterator;

  LineIterator LinesBegin() { return mLines.begin(); }
  LineIterator LinesEnd() { return mLines.end(); }
  ConstLineIterator LinesBegin() const { return mLines.begin(); }
  ConstLineIterator LinesEnd() const { return mLines.end(); }
  ReverseLineIterator LinesRBegin() { return mLines.rbegin(); }
  ReverseLineIterator LinesREnd() { return mLines.rend(); }
  ConstReverseLineIterator LinesRBegin() const { return mLines.rbegin(); }
  ConstReverseLineIterator LinesREnd() const { return mLines.rend(); }
  LineIterator LinesBeginFrom(nsLineBox* aList) { return mLines.begin(aList); }
  ReverseLineIterator LinesRBeginFrom(nsLineBox* aList) {
    return mLines.rbegin(aList);
  }

  friend nsBlockFrame* NS_NewBlockFrame(mozilla::PresShell* aPresShell,
                                        ComputedStyle* aStyle);

  // nsQueryFrame
  NS_DECL_QUERYFRAME

  // nsIFrame
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
  void SetInitialChildList(ChildListID aListID,
                           nsFrameList& aChildList) override;
  void AppendFrames(ChildListID aListID, nsFrameList& aFrameList) override;
  void InsertFrames(ChildListID aListID, nsIFrame* aPrevFrame,
                    const nsLineList::iterator* aPrevFrameLine,
                    nsFrameList& aFrameList) override;
  void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame) override;
  nsContainerFrame* GetContentInsertionFrame() override;
  void AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) override;
  const nsFrameList& GetChildList(ChildListID aListID) const override;
  void GetChildLists(nsTArray<ChildList>* aLists) const override;
  nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;
  bool GetVerticalAlignBaseline(mozilla::WritingMode aWM,
                                nscoord* aBaseline) const override {
    NS_ASSERTION(!aWM.IsOrthogonalTo(GetWritingMode()),
                 "You should only call this on frames with a WM that's "
                 "parallel to aWM");
    nscoord lastBaseline;
    if (GetNaturalBaselineBOffset(aWM, BaselineSharingGroup::Last,
                                  &lastBaseline)) {
      *aBaseline = BSize() - lastBaseline;
      return true;
    }
    return false;
  }
  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord* aBaseline) const override;
  nscoord GetCaretBaseline() const override;
  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) override;
  bool IsFloatContainingBlock() const override;
  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;
  bool IsFrameOfType(uint32_t aFlags) const override {
    return nsContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eCanContainOverflowContainers));
  }

  void InvalidateFrame(uint32_t aDisplayItemKey = 0,
                       bool aRebuildDisplayItems = true) override;
  void InvalidateFrameWithRect(const nsRect& aRect,
                               uint32_t aDisplayItemKey = 0,
                               bool aRebuildDisplayItems = true) override;

#ifdef DEBUG_FRAME_DUMP
  void List(FILE* out = stderr, const char* aPrefix = "",
            uint32_t aFlags = 0) const override;
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

#ifdef DEBUG
  const char* LineReflowStatusToString(
      LineReflowStatus aLineReflowStatus) const;
#endif

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

  // Line cursor methods to speed up line searching in which one query
  // result is expected to be close to the next in general. This is
  // mainly for searching line(s) containing a point. It is also used
  // as a cache for local computation. Use AutoLineCursorSetup for the
  // latter case so that it wouldn't interact unexpectedly with the
  // former. The basic idea for the former is that we set the cursor
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
  // Get the first line that might contain y-coord 'y', or nullptr if you must
  // search all lines. If nonnull is returned then we guarantee that the lines'
  // combinedArea.ys and combinedArea.yMosts are non-decreasing.
  // The actual line returned might not contain 'y', but if not, it is
  // guaranteed to be before any line which does contain 'y'.
  nsLineBox* GetFirstLineContaining(nscoord y);
  // Set the line cursor to our first line. Only call this if you
  // guarantee that either the lines' combinedArea.ys and combinedArea.
  // yMosts are non-decreasing, or the line cursor is cleared before
  // building the display list of this frame.
  void SetupLineCursor();

  /**
   * Helper RAII class for automatically set and clear line cursor for
   * temporary use. If the frame already has line cursor, this would be
   * a no-op.
   */
  class MOZ_STACK_CLASS AutoLineCursorSetup {
   public:
    explicit AutoLineCursorSetup(nsBlockFrame* aFrame)
        : mFrame(aFrame), mOrigCursor(aFrame->GetLineCursor()) {
      if (!mOrigCursor) {
        mFrame->SetupLineCursor();
      }
    }
    ~AutoLineCursorSetup() {
      if (mOrigCursor) {
        mFrame->SetProperty(LineCursorProperty(), mOrigCursor);
      } else {
        mFrame->ClearLineCursor();
      }
    }

   private:
    nsBlockFrame* mFrame;
    nsLineBox* mOrigCursor;
  };

  void ChildIsDirty(nsIFrame* aChild) override;

  bool IsEmpty() override;
  bool CachedIsEmpty() override;
  bool IsSelfEmpty() override;

  // Given that we have a ::marker frame, does it actually draw something, i.e.,
  // do we have either a 'list-style-type' or 'list-style-image' that is
  // not 'none', and no 'content'?
  bool MarkerIsEmpty() const;

  /**
   * Return true if this frame has a ::marker frame.
   */
  bool HasMarker() const { return HasOutsideMarker() || HasInsideMarker(); }

  /**
   * @return true if this frame has an inside ::marker frame.
   */
  bool HasInsideMarker() const {
    return 0 != (mState & NS_BLOCK_FRAME_HAS_INSIDE_MARKER);
  }

  /**
   * @return true if this frame has an outside ::marker frame.
   */
  bool HasOutsideMarker() const {
    return 0 != (mState & NS_BLOCK_FRAME_HAS_OUTSIDE_MARKER);
  }

  /**
   * @return the ::marker frame or nullptr if we don't have one.
   */
  nsIFrame* GetMarker() const {
    nsIFrame* outside = GetOutsideMarker();
    return outside ? outside : GetInsideMarker();
  }

  /**
   * @return the first-letter frame or nullptr if we don't have one.
   */
  nsIFrame* GetFirstLetter() const;

  /**
   * @return the ::first-line frame or nullptr if we don't have one.
   */
  nsIFrame* GetFirstLineFrame() const;

  void MarkIntrinsicISizesDirty() override;

 private:
  void CheckIntrinsicCacheAgainstShrinkWrapState();

 public:
  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  nsRect ComputeTightBounds(DrawTarget* aDrawTarget) const override;

  nsresult GetPrefWidthTightBounds(gfxContext* aContext, nscoord* aX,
                                   nscoord* aXMost) override;

  /**
   * Compute the final block size of this frame.
   *
   * @param aReflowInput Data structure passed from parent during reflow.
   * @param aStatus [in/out] The reflow status for this reflow operation. When
   *        this function is called, aStatus should represent what our status
   *        would be as if we were shrinkwrapping our children's block-size.
   *        This function will then adjust aStatus before returning, if our
   *        status is different in light of our actual final block-size and
   *        current page/column's available block-size.
   * @param aBEndEdgeOfChildren The distance between this frame's block-start
   *        border-edge and the block-end edge of our last child's border-box.
   *        This is effectively our block-start border-padding plus the
   *        block-size of our children, precomputed outside of this function.
   * @param aBorderPadding The margins representing the border padding for block
   *        frames. Can be 0.
   * @param aConsumed The block-size already consumed by our previous-in-flows.
   * @return our final block-size with respect to aReflowInput's writing-mode.
   */
  nscoord ComputeFinalBSize(const ReflowInput& aReflowInput,
                            nsReflowStatus& aStatus,
                            nscoord aBEndEdgeOfChildren,
                            const mozilla::LogicalMargin& aBorderPadding,
                            nscoord aConsumed);

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  /**
   * Move any frames on our overflow list to the end of our principal list.
   * @return true if there were any overflow frames
   */
  bool DrainSelfOverflowList() override;

  nsresult StealFrame(nsIFrame* aChild) override;

  void DeleteNextInFlowChild(nsIFrame* aNextInFlow,
                             bool aDeletingEmptyFrames) override;

  /**
   * This is a special method that allows a child class of nsBlockFrame to
   * return a special, customized nsStyleText object to the nsLineLayout
   * constructor. It is used when the nsBlockFrame child needs to specify its
   * custom rendering style.
   */
  virtual const nsStyleText* StyleTextForLineLayout();

  /**
   * Determines whether the collapsed margin carried out of the last
   * line includes the margin-top of a line with clearance (in which
   * case we must avoid collapsing that margin with our bottom margin)
   */
  bool CheckForCollapsedBEndMarginFromClearanceLine();

  static nsresult GetCurrentLine(BlockReflowInput* aState,
                                 nsLineBox** aOutCurrentLine);

  /**
   * Determine if this block is a margin root at the top/bottom edges.
   */
  void IsMarginRoot(bool* aBStartMarginRoot, bool* aBEndMarginRoot);

  static bool BlockNeedsFloatManager(nsIFrame* aBlock);

  /**
   * Returns whether aFrame is a block frame that will wrap its contents
   * around floats intruding on it from the outside.  (aFrame need not
   * be a block frame, but if it's not, the result will be false.)
   */
  static bool BlockCanIntersectFloats(nsIFrame* aFrame);

  /**
   * Returns the inline size that needs to be cleared past floats for
   * blocks that cannot intersect floats.  aState must already have
   * GetFloatAvailableSpace called on it for the block-dir position that we
   * care about (which need not be its current mBCoord)
   */
  struct ReplacedElementISizeToClear {
    // Note that we care about the inline-start margin but can ignore
    // the inline-end margin.
    nscoord marginIStart, borderBoxISize;
  };
  static ReplacedElementISizeToClear ISizeToClearPastFloats(
      const BlockReflowInput& aState,
      const mozilla::LogicalRect& aFloatAvailableSpace, nsIFrame* aFrame);

  /**
   * Creates a contination for aFloat and adds it to the list of overflow
   * floats. Also updates aState.mReflowStatus to include the float's
   * incompleteness. Must only be called while this block frame is in reflow.
   * aFloatStatus must be the float's true, unmodified reflow status.
   */
  void SplitFloat(BlockReflowInput& aState, nsIFrame* aFloat,
                  const nsReflowStatus& aFloatStatus);

  /**
   * Walks up the frame tree, starting with aCandidate, and returns the first
   * block frame that it encounters.
   */
  static nsBlockFrame* GetNearestAncestorBlock(nsIFrame* aCandidate);

  struct FrameLines {
    nsLineList mLines;
    nsFrameList mFrames;
  };

  /**
   * Update the styles of our various pseudo-elements (marker, first-line,
   * etc, but _not_ first-letter).
   */
  void UpdatePseudoElementStyles(mozilla::ServoRestyleState& aRestyleState);

  // Update our first-letter styles during stylo post-traversal.  This needs to
  // be done at a slightly different time than our other pseudo-elements.
  void UpdateFirstLetterStyle(mozilla::ServoRestyleState& aRestyleState);

 protected:
  explicit nsBlockFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                        ClassID aID = kClassID)
      : nsContainerFrame(aStyle, aPresContext, aID) {
#ifdef DEBUG
    InitDebugFlags();
#endif
  }

  virtual ~nsBlockFrame();

#ifdef DEBUG
  already_AddRefed<ComputedStyle> GetFirstLetterStyle(
      nsPresContext* aPresContext);
#endif

  NS_DECLARE_FRAME_PROPERTY_WITHOUT_DTOR(LineCursorProperty, nsLineBox)
  bool HasLineCursor() { return GetStateBits() & NS_BLOCK_HAS_LINE_CURSOR; }
  nsLineBox* GetLineCursor() {
    return HasLineCursor() ? GetProperty(LineCursorProperty()) : nullptr;
  }

  nsLineBox* NewLineBox(nsIFrame* aFrame, bool aIsBlock) {
    return NS_NewLineBox(PresShell(), aFrame, aIsBlock);
  }
  nsLineBox* NewLineBox(nsLineBox* aFromLine, nsIFrame* aFrame,
                        int32_t aCount) {
    return NS_NewLineBox(PresShell(), aFromLine, aFrame, aCount);
  }
  void FreeLineBox(nsLineBox* aLine) {
    if (aLine == GetLineCursor()) {
      ClearLineCursor();
    }
    aLine->Destroy(PresShell());
  }
  /**
   * Helper method for StealFrame.
   */
  void RemoveFrameFromLine(nsIFrame* aChild, nsLineList::iterator aLine,
                           nsFrameList& aFrameList, nsLineList& aLineList);

  void TryAllLines(nsLineList::iterator* aIterator,
                   nsLineList::iterator* aStartIterator,
                   nsLineList::iterator* aEndIterator, bool* aInOverflowLines,
                   FrameLines** aOverflowLines);

  /** move the frames contained by aLine by aDeltaBCoord
   * if aLine is a block, its child floats are added to the state manager
   */
  void SlideLine(BlockReflowInput& aState, nsLineBox* aLine,
                 nscoord aDeltaBCoord);

  void UpdateLineContainerSize(nsLineBox* aLine,
                               const nsSize& aNewContainerSize);

  // helper for SlideLine and UpdateLineContainerSize
  void MoveChildFramesOfLine(nsLineBox* aLine, nscoord aDeltaBCoord);

  void ComputeFinalSize(const ReflowInput& aReflowInput,
                        BlockReflowInput& aState, ReflowOutput& aMetrics,
                        nscoord* aBEndEdgeOfChildren);

  void ComputeOverflowAreas(const nsRect& aBounds,
                            const nsStyleDisplay* aDisplay,
                            nscoord aBEndEdgeOfChildren,
                            nsOverflowAreas& aOverflowAreas);

  /**
   * Add the frames in aFrameList to this block after aPrevSibling.
   * This block thinks in terms of lines, but the frame construction code
   * knows nothing about lines at all so we need to find the line that
   * contains aPrevSibling and add aFrameList after aPrevSibling on that line.
   * New lines are created as necessary to handle block data in aFrameList.
   * This function will clear aFrameList.
   *
   * aPrevSiblingLine, if present, must be the line containing aPrevSibling.
   * Providing it will make this function faster.
   */
  void AddFrames(nsFrameList& aFrameList, nsIFrame* aPrevSibling,
                 const nsLineList::iterator* aPrevSiblingLine);

  // Return the :-moz-block-ruby-content child frame, if any.
  // (It's non-null only if this block frame is for 'display:block ruby'.)
  nsContainerFrame* GetRubyContentPseudoFrame();

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

 public:
  /**
   * Helper function for the frame ctor to register a ::marker frame.
   */
  void SetMarkerFrameForListItem(nsIFrame* aMarkerFrame);

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
  enum { REMOVE_FIXED_CONTINUATIONS = 0x02, FRAMES_ARE_EMPTY = 0x04 };
  void DoRemoveFrame(nsIFrame* aDeletedFrame, uint32_t aFlags) {
    AutoPostDestroyData data(PresContext());
    DoRemoveFrameInternal(aDeletedFrame, aFlags, data.mData);
  }

  void ReparentFloats(nsIFrame* aFirstFrame, nsBlockFrame* aOldParent,
                      bool aReparentSiblings);

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override;

  virtual void UnionChildOverflow(nsOverflowAreas& aOverflowAreas) override;

  /**
   * Load all of aFrame's floats into the float manager iff aFrame is not a
   * block formatting context. Handles all necessary float manager translations;
   * assumes float manager is in aFrame's parent's coord system.
   *
   * Safe to call on non-blocks (does nothing).
   */
  static void RecoverFloatsFor(nsIFrame* aFrame, nsFloatManager& aFloatManager,
                               mozilla::WritingMode aWM,
                               const nsSize& aContainerSize);

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

    // We may have a pending push of pushed floats too:
    if (HasPushedFloats()) {
      // XXX we can return 'true' here once we make HasPushedFloats
      // not lie.  (see nsBlockFrame::RemoveFloat)
      auto* pushedFloats = GetPushedFloats();
      return pushedFloats && !pushedFloats->IsEmpty();
    }
    return false;
  }

  // @see nsIFrame::AddSizeOfExcludingThisForTree
  void AddSizeOfExcludingThisForTree(nsWindowSizes&) const override;

  /**
   * Clears any -webkit-line-clamp ellipsis on a line in this block or one
   * of its descendants.
   */
  void ClearLineClampEllipsis();

 protected:
  /** @see DoRemoveFrame */
  void DoRemoveFrameInternal(nsIFrame* aDeletedFrame, uint32_t aFlags,
                             PostDestroyData& data);

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

  /**
   * Moves frames from our PushedFloats list back into our mFloats list.
   */
  void DrainSelfPushedFloats();

  /**
   * First calls DrainSelfPushedFloats() then grabs pushed floats from this
   * block's prev-in-flow, and splice them into this block's mFloats list too.
   */
  void DrainPushedFloats();

  /** Load all our floats into the float manager (without reflowing them).
   *  Assumes float manager is in our own coordinate system.
   */
  void RecoverFloats(nsFloatManager& aFloatManager, mozilla::WritingMode aWM,
                     const nsSize& aContainerSize);

  /** Reflow pushed floats
   */
  void ReflowPushedFloats(BlockReflowInput& aState,
                          nsOverflowAreas& aOverflowAreas,
                          nsReflowStatus& aStatus);

  /** Find any trailing BR clear from the last line of the block (or its PIFs)
   */
  mozilla::StyleClear FindTrailingClear();

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
  void PrepareResizeReflow(BlockReflowInput& aState);

  /** reflow all lines that have been marked dirty */
  void ReflowDirtyLines(BlockReflowInput& aState);

  /** Mark a given line dirty due to reflow being interrupted on or before it */
  void MarkLineDirtyForInterrupt(nsLineBox* aLine);

  //----------------------------------------
  // Methods for line reflow
  /**
   * Reflow a line.
   *
   * @param aState
   *   the current reflow input
   * @param aLine
   *   the line to reflow.  can contain a single block frame or contain 1 or
   *   more inline frames.
   * @param aKeepReflowGoing [OUT]
   *   indicates whether the caller should continue to reflow more lines
   */
  void ReflowLine(BlockReflowInput& aState, LineIterator aLine,
                  bool* aKeepReflowGoing);

  // Return false if it needs another reflow because of reduced space
  // between floats that are next to it (but not next to its top), and
  // return true otherwise.
  bool PlaceLine(BlockReflowInput& aState, nsLineLayout& aLineLayout,
                 LineIterator aLine,
                 nsFloatManager::SavedState* aFloatStateBeforeLine,
                 nsFlowAreaRect& aFlowArea,      // in-out
                 nscoord& aAvailableSpaceBSize,  // in-out
                 bool* aKeepReflowGoing);

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
  void MarkLineDirty(LineIterator aLine, const nsLineList* aLineList);

  // XXX where to go
  bool IsLastLine(BlockReflowInput& aState, LineIterator aLine);

  void DeleteLine(BlockReflowInput& aState, nsLineList::iterator aLine,
                  nsLineList::iterator aLineEnd);

  //----------------------------------------
  // Methods for individual frame reflow

  bool ShouldApplyBStartMargin(BlockReflowInput& aState, nsLineBox* aLine,
                               nsIFrame* aChildFrame);

  void ReflowBlockFrame(BlockReflowInput& aState, LineIterator aLine,
                        bool* aKeepGoing);

  void ReflowInlineFrames(BlockReflowInput& aState, LineIterator aLine,
                          bool* aKeepLineGoing);

  void DoReflowInlineFrames(
      BlockReflowInput& aState, nsLineLayout& aLineLayout, LineIterator aLine,
      nsFlowAreaRect& aFloatAvailableSpace, nscoord& aAvailableSpaceBSize,
      nsFloatManager::SavedState* aFloatStateBeforeLine, bool* aKeepReflowGoing,
      LineReflowStatus* aLineReflowStatus, bool aAllowPullUp);

  void ReflowInlineFrame(BlockReflowInput& aState, nsLineLayout& aLineLayout,
                         LineIterator aLine, nsIFrame* aFrame,
                         LineReflowStatus* aLineReflowStatus);

  // Compute the available inline size for a float.
  mozilla::LogicalRect AdjustFloatAvailableSpace(
      BlockReflowInput& aState,
      const mozilla::LogicalRect& aFloatAvailableSpace, nsIFrame* aFloatFrame);
  // Computes the border-box inline size of the float
  nscoord ComputeFloatISize(BlockReflowInput& aState,
                            const mozilla::LogicalRect& aFloatAvailableSpace,
                            nsIFrame* aFloat);
  // An incomplete aReflowStatus indicates the float should be split
  // but only if the available height is constrained.
  // aAdjustedAvailableSpace is the result of calling
  // nsBlockFrame::AdjustFloatAvailableSpace.
  void ReflowFloat(BlockReflowInput& aState,
                   const mozilla::LogicalRect& aAdjustedAvailableSpace,
                   nsIFrame* aFloat, mozilla::LogicalMargin& aFloatMargin,
                   mozilla::LogicalMargin& aFloatOffsets,
                   // Whether the float's position
                   // (aAdjustedAvailableSpace) has been pushed down
                   // due to the presence of other floats.
                   bool aFloatPushedDown, nsReflowStatus& aReflowStatus);

  //----------------------------------------
  // Methods for pushing/pulling lines/frames

  /**
   * Create a next-in-flow, if necessary, for aFrame. If a new frame is
   * created, place it in aLine if aLine is not null.
   * @param aState the block reflow input
   * @param aLine where to put a new frame
   * @param aFrame the frame
   * @return true if a new frame was created, false if not
   */
  bool CreateContinuationFor(BlockReflowInput& aState, nsLineBox* aLine,
                             nsIFrame* aFrame);

  /**
   * Push aLine (and any after it), since it cannot be placed on this
   * page/column.  Set aKeepReflowGoing to false and set
   * flag aState.mReflowStatus as incomplete.
   */
  void PushTruncatedLine(BlockReflowInput& aState, LineIterator aLine,
                         bool* aKeepReflowGoing);

  void SplitLine(BlockReflowInput& aState, nsLineLayout& aLineLayout,
                 LineIterator aLine, nsIFrame* aFrame,
                 LineReflowStatus* aLineReflowStatus);

  /**
   * Pull a frame from the next available location (one of our lines or
   * one of our next-in-flows lines).
   * @return the pulled frame or nullptr
   */
  nsIFrame* PullFrame(BlockReflowInput& aState, LineIterator aLine);

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
  nsIFrame* PullFrameFrom(nsLineBox* aLine, nsBlockFrame* aFromContainer,
                          nsLineList::iterator aFromLine);

  /**
   * Push the line after aLineBefore to the overflow line list.
   * @param aLineBefore a line in 'mLines' (or LinesBegin() when
   *        pushing the first line)
   */
  void PushLines(BlockReflowInput& aState, nsLineList::iterator aLineBefore);

  void PropagateFloatDamage(BlockReflowInput& aState, nsLineBox* aLine,
                            nscoord aDeltaBCoord);

  void CheckFloats(BlockReflowInput& aState);

  //----------------------------------------
  // List handling kludge

  void ReflowOutsideMarker(nsIFrame* aMarkerFrame, BlockReflowInput& aState,
                           ReflowOutput& aMetrics, nscoord aLineTop);

  //----------------------------------------

  virtual nsILineIterator* GetLineIterator() override;

 public:
  bool HasOverflowLines() const {
    return 0 != (GetStateBits() & NS_BLOCK_HAS_OVERFLOW_LINES);
  }
  FrameLines* GetOverflowLines() const;

 protected:
  FrameLines* RemoveOverflowLines();
  void SetOverflowLines(FrameLines* aOverflowLines);
  void DestroyOverflowLines();

  /**
   * This class is useful for efficiently modifying the out of flow
   * overflow list. It gives the client direct writable access to
   * the frame list temporarily but ensures that property is only
   * written back if absolutely necessary.
   */
  struct nsAutoOOFFrameList {
    nsFrameList mList;

    explicit nsAutoOOFFrameList(nsBlockFrame* aBlock)
        : mPropValue(aBlock->GetOverflowOutOfFlows()), mBlock(aBlock) {
      if (mPropValue) {
        mList = *mPropValue;
      }
    }
    ~nsAutoOOFFrameList() { mBlock->SetOverflowOutOfFlows(mList, mPropValue); }

   protected:
    nsFrameList* const mPropValue;
    nsBlockFrame* const mBlock;
  };
  friend struct nsAutoOOFFrameList;

  nsFrameList* GetOverflowOutOfFlows() const;
  void SetOverflowOutOfFlows(const nsFrameList& aList, nsFrameList* aPropValue);

  /**
   * @return the inside ::marker frame or nullptr if we don't have one.
   */
  nsIFrame* GetInsideMarker() const;

  /**
   * @return the outside ::marker frame or nullptr if we don't have one.
   */
  nsIFrame* GetOutsideMarker() const;

  /**
   * @return the outside ::marker frame list frame property.
   */
  nsFrameList* GetOutsideMarkerList() const;

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

  nscoord mCachedMinISize = NS_INTRINSIC_ISIZE_UNKNOWN;
  nscoord mCachedPrefISize = NS_INTRINSIC_ISIZE_UNKNOWN;

  nsLineList mLines;

  // List of all floats in this block
  // XXXmats blocks rarely have floats, make it a frame property
  nsFrameList mFloats;

  friend class mozilla::BlockReflowInput;
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
  explicit AutoNoisyIndenter(bool aDoIndent) : mIndented(aDoIndent) {
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
  typedef nsBlockFrame::LineIterator LineIterator;
  /**
   * Set up the iterator to point to aLine which must be a normal line
   * in aFrame (not an overflow line).
   */
  nsBlockInFlowLineIterator(nsBlockFrame* aFrame, LineIterator aLine);
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

  // Allow to be uninitialized (and then assigned from another object).
  nsBlockInFlowLineIterator() : mFrame(nullptr) {}

  LineIterator GetLine() { return mLine; }
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
  LineIterator End();

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

  // XXX nsBlockFrame uses this internally in one place.  Try to remove it.
  // XXX uhm, and nsBidiPresUtils::Resolve too.
  nsBlockInFlowLineIterator(nsBlockFrame* aFrame, LineIterator aLine,
                            bool aInOverflow);

 private:
  nsBlockFrame* mFrame;
  LineIterator mLine;
  nsLineList* mLineList;  // the line list mLine is in

  /**
   * Moves iterator to next valid line reachable from the current block.
   * Returns false if there are no valid lines.
   */
  bool FindValidLine();
};

#endif /* nsBlockFrame_h___ */
