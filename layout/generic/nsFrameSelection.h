/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFrameSelection_h___
#define nsFrameSelection_h___

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/TextRange.h"
#include "mozilla/UniquePtr.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsISelectionController.h"
#include "nsISelectionListener.h"
#include "nsITableCellLayout.h"
#include "WordMovementType.h"
#include "CaretAssociationHint.h"
#include "nsBidiPresUtils.h"

class nsRange;

#define BIDI_LEVEL_UNDEFINED 0x80

//----------------------------------------------------------------------

// Selection interface

struct SelectionDetails {
  SelectionDetails()
      : mStart(), mEnd(), mSelectionType(mozilla::SelectionType::eInvalid) {
    MOZ_COUNT_CTOR(SelectionDetails);
  }
  MOZ_COUNTED_DTOR(SelectionDetails)

  int32_t mStart;
  int32_t mEnd;
  mozilla::SelectionType mSelectionType;
  mozilla::TextRangeStyle mTextRangeStyle;
  mozilla::UniquePtr<SelectionDetails> mNext;
};

struct SelectionCustomColors {
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNTED_DEFAULT_CTOR(SelectionCustomColors)
  MOZ_COUNTED_DTOR(SelectionCustomColors)
#endif
  mozilla::Maybe<nscolor> mForegroundColor;
  mozilla::Maybe<nscolor> mBackgroundColor;
  mozilla::Maybe<nscolor> mAltForegroundColor;
  mozilla::Maybe<nscolor> mAltBackgroundColor;
};

namespace mozilla {
class PresShell;
}  // namespace mozilla

/** PeekOffsetStruct is used to group various arguments (both input and output)
 *  that are passed to nsFrame::PeekOffset(). See below for the description of
 *  individual arguments.
 */
struct MOZ_STACK_CLASS nsPeekOffsetStruct {
  enum class ForceEditableRegion {
    No,
    Yes,
  };

  nsPeekOffsetStruct(
      nsSelectionAmount aAmount, nsDirection aDirection, int32_t aStartOffset,
      nsPoint aDesiredPos, bool aJumpLines, bool aScrollViewStop,
      bool aIsKeyboardSelect, bool aVisual, bool aExtend,
      ForceEditableRegion = ForceEditableRegion::No,
      mozilla::EWordMovementType aWordMovementType = mozilla::eDefaultBehavior,
      bool aTrimSpaces = true);

  // Note: Most arguments (input and output) are only used with certain values
  // of mAmount. These values are indicated for each argument below.
  // Arguments with no such indication are used with all values of mAmount.

  /*** Input arguments ***/
  // Note: The value of some of the input arguments may be changed upon exit.

  // The type of movement requested (by character, word, line, etc.)
  nsSelectionAmount mAmount;

  // eDirPrevious or eDirNext.
  //
  // Note for visual bidi movement:
  //   * eDirPrevious means 'left-then-up' if the containing block is LTR,
  //     'right-then-up' if it is RTL.
  //   * eDirNext means 'right-then-down' if the containing block is LTR,
  //     'left-then-down' if it is RTL.
  //   * Between paragraphs, eDirPrevious means "go to the visual end of
  //     the previous paragraph", and eDirNext means "go to the visual
  //     beginning of the next paragraph".
  //
  // Used with: eSelectCharacter, eSelectWord, eSelectLine, eSelectParagraph.
  const nsDirection mDirection;

  // Offset into the content of the current frame where the peek starts.
  //
  // Used with: eSelectCharacter, eSelectWord
  int32_t mStartOffset;

  // The desired inline coordinate for the caret (one of .x or .y will be used,
  // depending on line's writing mode)
  //
  // Used with: eSelectLine.
  const nsPoint mDesiredPos;

  // An enum that determines whether to prefer the start or end of a word or to
  // use the default beahvior, which is a combination of direction and the
  // platform-based pref "layout.word_select.eat_space_to_next_word"
  mozilla::EWordMovementType mWordMovementType;

  // Whether to allow jumping across line boundaries.
  //
  // Used with: eSelectCharacter, eSelectWord.
  const bool mJumpLines;

  // mTrimSpaces: Whether we should trim spaces at begin/end of content
  const bool mTrimSpaces;

  // Whether to stop when reaching a scroll view boundary.
  //
  // Used with: eSelectCharacter, eSelectWord, eSelectLine.
  const bool mScrollViewStop;

  // Whether the peeking is done in response to a keyboard action.
  //
  // Used with: eSelectWord.
  const bool mIsKeyboardSelect;

  // Whether bidi caret behavior is visual (true) or logical (false).
  //
  // Used with: eSelectCharacter, eSelectWord, eSelectBeginLine, eSelectEndLine.
  const bool mVisual;

  // Whether the selection is being extended or moved.
  const bool mExtend;

  // If true, the offset has to end up in an editable node, otherwise we'll keep
  // searching.
  const bool mForceEditableRegion;

  /*** Output arguments ***/

  // Content reached as a result of the peek.
  nsCOMPtr<nsIContent> mResultContent;

  // Frame reached as a result of the peek.
  //
  // Used with: eSelectCharacter, eSelectWord.
  nsIFrame* mResultFrame;

  // Offset into content reached as a result of the peek.
  int32_t mContentOffset;

  // When the result position is between two frames, indicates which of the two
  // frames the caret should be painted in. false means "the end of the frame
  // logically before the caret", true means "the beginning of the frame
  // logically after the caret".
  //
  // Used with: eSelectLine, eSelectBeginLine, eSelectEndLine.
  mozilla::CaretAssociationHint mAttach;
};

struct nsPrevNextBidiLevels {
  void SetData(nsIFrame* aFrameBefore, nsIFrame* aFrameAfter,
               nsBidiLevel aLevelBefore, nsBidiLevel aLevelAfter) {
    mFrameBefore = aFrameBefore;
    mFrameAfter = aFrameAfter;
    mLevelBefore = aLevelBefore;
    mLevelAfter = aLevelAfter;
  }
  nsIFrame* mFrameBefore;
  nsIFrame* mFrameAfter;
  nsBidiLevel mLevelBefore;
  nsBidiLevel mLevelAfter;
};

namespace mozilla {
class SelectionChangeEventDispatcher;
namespace dom {
class Selection;
}  // namespace dom

/**
 * Constants for places that want to handle table selections.  These
 * indicate what part of a table is being selected.
 */
enum class TableSelectionMode : uint32_t {
  None,     /* Nothing being selected; not valid in all cases. */
  Cell,     /* A cell is being selected. */
  Row,      /* A row is being selected. */
  Column,   /* A column is being selected. */
  Table,    /* A table (including cells and captions) is being selected. */
  AllCells, /* All the cells in a table are being selected. */
};

}  // namespace mozilla
class nsIScrollableFrame;

class nsFrameSelection final {
 public:
  typedef mozilla::CaretAssociationHint CaretAssociateHint;

  /*interfaces for addref and release and queryinterface*/

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsFrameSelection)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsFrameSelection)

  enum class FocusMode {
    kExtendSelection,     /** Keep old anchor point. */
    kCollapseToNewPoint,  /** Collapses the Selection to the new point. */
    kMultiRangeSelection, /** Keeps existing non-collapsed ranges and marks them
                             as generated. */
  };

  /**
   * HandleClick will take the focus to the new frame at the new offset and
   * will either extend the selection from the old anchor, or replace the old
   * anchor. the old anchor and focus position may also be used to deselect
   * things
   *
   * @param aNewfocus is the content that wants the focus
   *
   * @param aContentOffset is the content offset of the parent aNewFocus
   *
   * @param aContentOffsetEnd is the content offset of the parent aNewFocus and
   * is specified different when you need to select to and include both start
   * and end points
   *
   * @param aHint will tell the selection which direction geometrically to
   * actually show the caret on. 1 = end of this line 0 = beginning of this line
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult HandleClick(nsIContent* aNewFocus,
                                                   uint32_t aContentOffset,
                                                   uint32_t aContentEndOffset,
                                                   FocusMode aFocusMode,
                                                   CaretAssociateHint aHint);

  /**
   * HandleDrag extends the selection to contain the frame closest to aPoint.
   *
   * @param aPresContext is the context to use when figuring out what frame
   * contains the point.
   *
   * @param aFrame is the parent of all frames to use when searching for the
   * closest frame to the point.
   *
   * @param aPoint is relative to aFrame
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void HandleDrag(nsIFrame* aFrame,
                                              const nsPoint& aPoint);

  /**
   * HandleTableSelection will set selection to a table, cell, etc
   * depending on information contained in aFlags
   *
   * @param aParentContent is the paretent of either a table or cell that user
   * clicked or dragged the mouse in
   *
   * @param aContentOffset is the offset of the table or cell
   *
   * @param aTarget indicates what to select
   *   * TableSelectionMode::Cell
   *       We should select a cell (content points to the cell)
   *   * TableSelectionMode::Row
   *       We should select a row (content points to any cell in row)
   *   * TableSelectionMode::Column
   *       We should select a row (content points to any cell in column)
   *   * TableSelectionMode::Table
   *       We should select a table (content points to the table)
   *   * TableSelectionMode::AllCells
   *       We should select all cells (content points to any cell in table)
   *
   * @param aMouseEvent passed in so we can get where event occurred
   * and what keys are pressed
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  HandleTableSelection(nsINode* aParentContent, int32_t aContentOffset,
                       mozilla::TableSelectionMode aTarget,
                       mozilla::WidgetMouseEvent* aMouseEvent);

  /**
   * Add cell to the selection with `SelectionType::eNormal`.
   *
   * @param  aCell  [in] HTML td element.
   */
  nsresult SelectCellElement(nsIContent* aCell);

 public:
  /**
   * Remove cells from selection inside of the given cell range.
   *
   * @param  aTable             [in] HTML table element
   * @param  aStartRowIndex     [in] row index where the cells range starts
   * @param  aStartColumnIndex  [in] column index where the cells range starts
   * @param  aEndRowIndex       [in] row index where the cells range ends
   * @param  aEndColumnIndex    [in] column index where the cells range ends
   */
  // TODO: annotate this with `MOZ_CAN_RUN_SCRIPT` instead.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult RemoveCellsFromSelection(nsIContent* aTable, int32_t aStartRowIndex,
                                    int32_t aStartColumnIndex,
                                    int32_t aEndRowIndex,
                                    int32_t aEndColumnIndex);

  /**
   * Remove cells from selection outside of the given cell range.
   *
   * @param  aTable             [in] HTML table element
   * @param  aStartRowIndex     [in] row index where the cells range starts
   * @param  aStartColumnIndex  [in] column index where the cells range starts
   * @param  aEndRowIndex       [in] row index where the cells range ends
   * @param  aEndColumnIndex    [in] column index where the cells range ends
   */
  // TODO: annotate this with `MOZ_CAN_RUN_SCRIPT` instead.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult RestrictCellsToSelection(nsIContent* aTable, int32_t aStartRowIndex,
                                    int32_t aStartColumnIndex,
                                    int32_t aEndRowIndex,
                                    int32_t aEndColumnIndex);

  /**
   * StartAutoScrollTimer is responsible for scrolling frames so that
   * aPoint is always visible, and for selecting any frame that contains
   * aPoint. The timer will also reset itself to fire again if we have
   * not scrolled to the end of the document.
   *
   * @param aFrame is the outermost frame to use when searching for
   * the closest frame for the point, i.e. the frame that is capturing
   * the mouse
   *
   * @param aPoint is relative to aFrame.
   *
   * @param aDelay is the timer's interval.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult StartAutoScrollTimer(nsIFrame* aFrame, const nsPoint& aPoint,
                                uint32_t aDelay);

  /**
   * Stops any active auto scroll timer.
   */
  void StopAutoScrollTimer();

  /**
   * Returns in frame coordinates the selection beginning and ending with the
   * type of selection given
   *
   * @param aContent is the content asking
   * @param aContentOffset is the starting content boundary
   * @param aContentLength is the length of the content piece asking
   * @param aSlowCheck will check using slow method with no shortcuts
   */
  mozilla::UniquePtr<SelectionDetails> LookUpSelection(nsIContent* aContent,
                                                       int32_t aContentOffset,
                                                       int32_t aContentLength,
                                                       bool aSlowCheck) const;

  /**
   * Sets the drag state to aState for resons of drag state.
   *
   * @param aState is the new state of drag
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void SetDragState(bool aState);

  /**
   * Gets the drag state to aState for resons of drag state.
   *
   * @param aState will hold the state of drag
   */
  bool GetDragState() const { return mDragState; }

  /**
   * If we are in table cell selection mode. aka ctrl click in table cell
   */
  bool GetTableCellSelection() const {
    return mTableSelection.mMode != mozilla::TableSelectionMode::None;
  }
  void ClearTableCellSelection() {
    mTableSelection.mMode = mozilla::TableSelectionMode::None;
  }

  /**
   * No query interface for selection. must use this method now.
   *
   * @param aSelectionType The selection type what you want.
   */
  mozilla::dom::Selection* GetSelection(
      mozilla::SelectionType aSelectionType) const;

  /**
   * ScrollSelectionIntoView scrolls a region of the selection,
   * so that it is visible in the scrolled view.
   *
   * @param aSelectionType the selection to scroll into view.
   *
   * @param aRegion the region inside the selection to scroll into view.
   *
   * @param aFlags the scroll flags.  Valid bits include:
   *   * SCROLL_SYNCHRONOUS: when set, scrolls the selection into view
   *     before returning. If not set, posts a request which is processed
   *     at some point after the method returns.
   *   * SCROLL_FIRST_ANCESTOR_ONLY: if set, only the first ancestor will be
   *     scrolled into view.
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  ScrollSelectionIntoView(mozilla::SelectionType aSelectionType,
                          SelectionRegion aRegion, int16_t aFlags) const;

  /**
   * RepaintSelection repaints the selected frames that are inside the
   * selection specified by aSelectionType.
   *
   * @param aSelectionType The selection type what you want to repaint.
   */
  nsresult RepaintSelection(mozilla::SelectionType aSelectionType);

  bool IsValidSelectionPoint(nsINode* aNode) const;

  /**
   * Given a node and its child offset, return the nsIFrame and the offset into
   * that frame.
   *
   * @param aNode input parameter for the node to look at
   * @param aOffset offset into above node.
   * @param aReturnOffset will contain offset into frame.
   */
  static nsIFrame* GetFrameForNodeOffset(nsIContent* aNode, int32_t aOffset,
                                         CaretAssociateHint aHint,
                                         int32_t* aReturnOffset);

  /**
   * GetFrameToPageSelect() returns a frame which is ancestor limit of
   * per-page selection.  The frame may not be scrollable.  E.g.,
   * when selection ancestor limit is set to a frame of an editing host of
   * contenteditable element and it's not scrollable.
   */
  nsIFrame* GetFrameToPageSelect() const;

  /**
   * This method moves caret (if aExtend is false) or expands selection (if
   * aExtend is true).  Then, scrolls aFrame one page.  Finally, this may
   * call ScrollSelectionIntoView() for making focus of selection visible
   * but depending on aSelectionIntoView value.
   *
   * @param aForward if true, scroll forward if not scroll backward
   * @param aExtend  if true, extend selection to the new point
   * @param aFrame   the frame to scroll or container of per-page selection.
   *                 if aExtend is true and selection may have ancestor limit,
   *                 should set result of GetFrameToPageSelect().
   * @param aSelectionIntoView
   *                 If IfChanged, this makes selection into view only when
   *                 selection is modified by the call.
   *                 If Yes, this makes selection into view always.
   */
  enum class SelectionIntoView { IfChanged, Yes };
  MOZ_CAN_RUN_SCRIPT nsresult PageMove(bool aForward, bool aExtend,
                                       nsIFrame* aFrame,
                                       SelectionIntoView aSelectionIntoView);

  void SetHint(CaretAssociateHint aHintRight) { mCaret.mHint = aHintRight; }
  CaretAssociateHint GetHint() const { return mCaret.mHint; }

  /**
   * SetCaretBidiLevel sets the caret bidi level.
   * @param aLevel the caret bidi level
   */
  void SetCaretBidiLevel(nsBidiLevel aLevel);

  /**
   * GetCaretBidiLevel gets the caret bidi level.
   */
  nsBidiLevel GetCaretBidiLevel() const;

  /**
   * UndefineCaretBidiLevel sets the caret bidi level to "undefined".
   */
  void UndefineCaretBidiLevel();

  /**
   * PhysicalMove will generally be called from the nsiselectioncontroller
   * implementations. the effect being the selection will move one unit
   * 'aAmount' in the given aDirection.
   * @param aDirection  the direction to move the selection
   * @param aAmount     amount of movement (char/line; word/page; eol/doc)
   * @param aExtend     continue selection
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult PhysicalMove(int16_t aDirection,
                                                    int16_t aAmount,
                                                    bool aExtend);

  /**
   * CharacterMove will generally be called from the nsiselectioncontroller
   * implementations. the effect being the selection will move one character
   * left or right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult CharacterMove(bool aForward,
                                                     bool aExtend);

  /**
   * CharacterExtendForDelete extends the selection forward (logically) to
   * the next character cell, so that the selected cell can be deleted.
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult CharacterExtendForDelete();

  /**
   * CharacterExtendForBackspace extends the selection backward (logically) to
   * the previous character cell, so that the selected cell can be deleted.
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult CharacterExtendForBackspace();

  /**
   * WordMove will generally be called from the nsiselectioncontroller
   * implementations. the effect being the selection will move one word left or
   * right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult WordMove(bool aForward, bool aExtend);

  /**
   * WordExtendForDelete extends the selection backward or forward (logically)
   * to the next word boundary, so that the selected word can be deleted.
   * @param aForward select forward in document.
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult WordExtendForDelete(bool aForward);

  /**
   * LineMove will generally be called from the nsiselectioncontroller
   * implementations. the effect being the selection will move one line up or
   * down.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult LineMove(bool aForward, bool aExtend);

  /**
   * IntraLineMove will generally be called from the nsiselectioncontroller
   * implementations. the effect being the selection will move to beginning or
   * end of line
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult IntraLineMove(bool aForward,
                                                     bool aExtend);

  /**
   * Select All will generally be called from the nsiselectioncontroller
   * implementations. it will select the whole doc
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult SelectAll();

  /** Sets/Gets The display selection enum.
   */
  void SetDisplaySelection(int16_t aState) { mDisplaySelection = aState; }
  int16_t GetDisplaySelection() const { return mDisplaySelection; }

  /**
   * This method can be used to store the data received during a MouseDown
   * event so that we can place the caret during the MouseUp event.
   *
   * @param aMouseEvent the event received by the selection MouseDown
   * handling method. A nullptr value can be use to tell this method
   * that any data is storing is no longer valid.
   */
  void SetDelayedCaretData(mozilla::WidgetMouseEvent* aMouseEvent);

  /**
   * Get the delayed MouseDown event data necessary to place the
   * caret during MouseUp processing.
   *
   * @return a pointer to the event received
   * by the selection during MouseDown processing. It can be nullptr
   * if the data is no longer valid.
   */
  bool HasDelayedCaretData() { return mDelayedMouseEvent.mIsValid; }
  bool IsShiftDownInDelayedCaretData() {
    NS_ASSERTION(mDelayedMouseEvent.mIsValid, "No valid delayed caret data");
    return mDelayedMouseEvent.mIsShift;
  }
  uint32_t GetClickCountInDelayedCaretData() {
    NS_ASSERTION(mDelayedMouseEvent.mIsValid, "No valid delayed caret data");
    return mDelayedMouseEvent.mClickCount;
  }

  bool MouseDownRecorded() {
    return !GetDragState() && HasDelayedCaretData() &&
           GetClickCountInDelayedCaretData() < 2;
  }

  /**
   * Get the content node that limits the selection
   *
   * When searching up a nodes for parents, as in a text edit field
   * in an browser page, we must stop at this node else we reach into the
   * parent page, which is very bad!
   */
  nsIContent* GetLimiter() const { return mLimiters.mLimiter; }

  nsIContent* GetAncestorLimiter() const { return mLimiters.mAncestorLimiter; }
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void SetAncestorLimiter(nsIContent* aLimiter);

  /**
   * GetPrevNextBidiLevels will return the frames and associated Bidi levels of
   * the characters logically before and after a (collapsed) selection.
   *
   * @param aNode is the node containing the selection
   * @param aContentOffset is the offset of the selection in the node
   * @param aJumpLines
   *   If true, look across line boundaries.
   *   If false, behave as if there were base-level frames at line edges.
   *
   * @return A struct holding the before/after frame and the before/after
   * level.
   *
   * At the beginning and end of each line there is assumed to be a frame with
   * Bidi level equal to the paragraph embedding level.
   *
   * In these cases the before frame and after frame respectively will be
   * nullptr.
   */
  nsPrevNextBidiLevels GetPrevNextBidiLevels(nsIContent* aNode,
                                             uint32_t aContentOffset,
                                             bool aJumpLines) const;

  /**
   * GetFrameFromLevel will scan in a given direction
   * until it finds a frame with a Bidi level less than or equal to a given
   * level. It will return the last frame before this.
   *
   * @param aPresContext is the context to use
   * @param aFrameIn is the frame to start from
   * @param aDirection is the direction to scan
   * @param aBidiLevel is the level to search for
   * @param aFrameOut will hold the frame returned
   */
  nsresult GetFrameFromLevel(nsIFrame* aFrameIn, nsDirection aDirection,
                             nsBidiLevel aBidiLevel,
                             nsIFrame** aFrameOut) const;

  /**
   * MaintainSelection will track the normal selection as being "sticky".
   * Dragging or extending selection will never allow for a subset
   * (or the whole) of the maintained selection to become unselected.
   * Primary use: double click selecting then dragging on second click
   *
   * @param aAmount the initial amount of text selected (word, line or
   * paragraph). For "line", use eSelectBeginLine.
   */
  nsresult MaintainSelection(nsSelectionAmount aAmount = eSelectNoAmount);

  nsresult ConstrainFrameAndPointToAnchorSubtree(nsIFrame* aFrame,
                                                 const nsPoint& aPoint,
                                                 nsIFrame** aRetFrame,
                                                 nsPoint& aRetPoint) const;

  /**
   * @param aPresShell is the parameter to be used for most of the other calls
   * for callbacks etc
   *
   * @param aLimiter limits the selection to nodes with aLimiter parents
   *
   * @param aAccessibleCaretEnabled true if we should enable the accessible
   * caret.
   */
  nsFrameSelection(mozilla::PresShell* aPresShell, nsIContent* aLimiter,
                   bool aAccessibleCaretEnabled);

  void StartBatchChanges();

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  /**
   * @param aReasons potentially multiple of the reasons defined in
   * nsISelectionListener.idl
   */
  void EndBatchChanges(int16_t aReasons = nsISelectionListener::NO_REASON);

  mozilla::PresShell* GetPresShell() const { return mPresShell; }

  void DisconnectFromPresShell();
  nsresult ClearNormalSelection();

  // Table selection support.
  static nsITableCellLayout* GetCellLayout(nsIContent* aCellContent);

 private:
  ~nsFrameSelection();

  MOZ_CAN_RUN_SCRIPT
  nsresult TakeFocus(nsIContent* aNewFocus, uint32_t aContentOffset,
                     uint32_t aContentEndOffset, CaretAssociateHint aHint,
                     FocusMode aFocusMode);

  void BidiLevelFromMove(mozilla::PresShell* aPresShell, nsIContent* aNode,
                         uint32_t aContentOffset, nsSelectionAmount aAmount,
                         CaretAssociateHint aHint);
  void BidiLevelFromClick(nsIContent* aNewFocus, uint32_t aContentOffset);
  static nsPrevNextBidiLevels GetPrevNextBidiLevels(nsIContent* aNode,
                                                    uint32_t aContentOffset,
                                                    CaretAssociateHint aHint,
                                                    bool aJumpLines);

  /**
   * @param aReasons potentially multiple of the reasons defined in
   * nsISelectionListener.idl.
   */
  void SetChangeReasons(int16_t aReasons) {
    mSelectionChangeReasons = aReasons;
  }

  /**
   * @param aReasons potentially multiple of the reasons defined in
   * nsISelectionListener.idl.
   */
  void AddChangeReasons(int16_t aReasons) {
    mSelectionChangeReasons |= aReasons;
  }

  /**
   * @return potentially multiple of the reasons defined in
   * nsISelectionListener.idl.
   */
  int16_t PopChangeReasons() {
    int16_t retval = mSelectionChangeReasons;
    mSelectionChangeReasons = nsISelectionListener::NO_REASON;
    return retval;
  }

  bool IsUserSelectionReason() const {
    return (mSelectionChangeReasons &
            (nsISelectionListener::DRAG_REASON |
             nsISelectionListener::MOUSEDOWN_REASON |
             nsISelectionListener::MOUSEUP_REASON |
             nsISelectionListener::KEYPRESS_REASON)) !=
           nsISelectionListener::NO_REASON;
  }

  friend class mozilla::dom::Selection;
  friend class mozilla::SelectionChangeEventDispatcher;
  friend struct mozilla::AutoPrepareFocusRange;

  /*HELPER METHODS*/
  // Whether MoveCaret should use logical or visual movement,
  // or follow the bidi.edit.caret_movement_style preference.
  enum CaretMovementStyle { eLogical, eVisual, eUsePrefStyle };
  MOZ_CAN_RUN_SCRIPT nsresult MoveCaret(nsDirection aDirection,
                                        bool aContinueSelection,
                                        nsSelectionAmount aAmount,
                                        CaretMovementStyle aMovementStyle);

  nsresult FetchDesiredPos(
      nsPoint& aDesiredPos);  // the position requested by the Key Handling for
                              // up down
  void InvalidateDesiredPos();  // do not listen to mDesiredPos.mValue you must
                                // get another.
  void SetDesiredPos(nsPoint aPos);  // set the mDesiredPos.mValue

  bool IsBatching() const { return mBatching.mCounter > 0; }

  void SetChangesDuringBatchingFlag() {
    MOZ_ASSERT(mBatching.mCounter > 0);

    mBatching.mChangesDuringBatching = true;
  }

  // nsFrameSelection may get deleted when calling this,
  // so remember to use nsCOMPtr when needed.
  MOZ_CAN_RUN_SCRIPT
  nsresult NotifySelectionListeners(mozilla::SelectionType aSelectionType);

  static nsresult GetCellIndexes(nsIContent* aCell, int32_t& aRowIndex,
                                 int32_t& aColIndex);

  static nsIContent* GetFirstCellNodeInRange(const nsRange* aRange);
  // Returns non-null table if in same table, null otherwise
  static nsIContent* IsInSameTable(nsIContent* aContent1,
                                   nsIContent* aContent2);
  // Might return null
  static nsIContent* GetParentTable(nsIContent* aCellNode);

  ////////////BEGIN nsFrameSelection members

  RefPtr<mozilla::dom::Selection>
      mDomSelections[sizeof(mozilla::kPresentSelectionTypes) /
                     sizeof(mozilla::SelectionType)];

  struct TableSelection {
    // Get our first range, if its first selected node is a cell.  If this does
    // not return null, then the first node in the returned range is a cell
    // (according to GetFirstCellNodeInRange).
    nsRange* GetFirstCellRange(const mozilla::dom::Selection& aNormalSelection);

    // Get our next range, if its first selected node is a cell.  If this does
    // not return null, then the first node in the returned range is a cell
    // (according to GetFirstCellNodeInRange).
    nsRange* GetNextCellRange(const mozilla::dom::Selection& aNormalSelection);

    [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
    HandleSelection(nsINode* aParentContent, int32_t aContentOffset,
                    mozilla::TableSelectionMode aTarget,
                    mozilla::WidgetMouseEvent* aMouseEvent, bool aDragState,
                    mozilla::dom::Selection& aNormalSelection);

    // TODO: annotate this with `MOZ_CAN_RUN_SCRIPT` instead.
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    nsresult SelectBlockOfCells(nsIContent* aStartCell, nsIContent* aEndCell,
                                mozilla::dom::Selection& aNormalSelection);

    nsresult SelectRowOrColumn(nsIContent* aCellContent,
                               mozilla::dom::Selection& aNormalSelection);

    MOZ_CAN_RUN_SCRIPT nsresult
    UnselectCells(nsIContent* aTable, int32_t aStartRowIndex,
                  int32_t aStartColumnIndex, int32_t aEndRowIndex,
                  int32_t aEndColumnIndex, bool aRemoveOutsideOfCellRange,
                  mozilla::dom::Selection& aNormalSelection);

    nsCOMPtr<nsINode> mCellParent;  // used to snap to table selection
    nsCOMPtr<nsIContent> mStartSelectedCell;
    nsCOMPtr<nsIContent> mEndSelectedCell;
    nsCOMPtr<nsIContent> mAppendStartSelectedCell;
    nsCOMPtr<nsIContent> mUnselectCellOnMouseUp;
    mozilla::TableSelectionMode mMode = mozilla::TableSelectionMode::None;
    int32_t mSelectedCellIndex = 0;
    bool mDragSelectingCells = false;
  };

  TableSelection mTableSelection;

  struct MaintainedRange {
    /**
     * @return true iff the point (aContent, aOffset) is inside and not at the
     * boundaries of mRange.
     */
    bool AdjustNormalSelection(const nsIContent* aContent, int32_t aOffset,
                               mozilla::dom::Selection& aNormalSelection) const;

    /**
     * @param aScrollViewStop see `nsPeekOffsetStruct::mScrollViewStop`.
     */
    void AdjustContentOffsets(nsIFrame::ContentOffsets& aOffsets,
                              bool aScrollViewStop) const;

    void MaintainAnchorFocusRange(
        const mozilla::dom::Selection& aNormalSelection,
        nsSelectionAmount aAmount);

    RefPtr<nsRange> mRange;
    nsSelectionAmount mAmount = eSelectNoAmount;
  };

  MaintainedRange mMaintainedRange;

  struct Batching {
    uint32_t mCounter = 0;
    bool mChangesDuringBatching = false;
  };

  Batching mBatching;

  struct Limiters {
    // Limit selection navigation to a child of this node.
    nsCOMPtr<nsIContent> mLimiter;
    // Limit selection navigation to a descendant of this node.
    nsCOMPtr<nsIContent> mAncestorLimiter;
  };

  Limiters mLimiters;

  mozilla::PresShell* mPresShell = nullptr;
  // Reasons for notifications of selection changing.
  // Can be multiple of the reasons defined in nsISelectionListener.idl.
  int16_t mSelectionChangeReasons = nsISelectionListener::NO_REASON;
  // For visual display purposes.
  int16_t mDisplaySelection = nsISelectionController::SELECTION_OFF;

  struct Caret {
    // Hint to tell if the selection is at the end of this line or beginning of
    // next.
    CaretAssociateHint mHint = mozilla::CARET_ASSOCIATE_BEFORE;
    nsBidiLevel mBidiLevel = BIDI_LEVEL_UNDEFINED;
    int8_t mMovementStyle = 0;
  };

  Caret mCaret;

  nsBidiLevel mKbdBidiLevel = NSBIDI_LTR;

  // TODO: could presumably be transformed to a `mozilla::Maybe`.
  struct DesiredPos {
    nsPoint mValue;
    bool mIsSet = false;
  };

  DesiredPos mDesiredPos;

  struct DelayedMouseEvent {
    bool mIsValid = false;
    // These values are not used since they are only valid when mIsValid is
    // true, and setting mIsValid  always overrides these values.
    bool mIsShift = false;
    uint32_t mClickCount = 0;
  };

  DelayedMouseEvent mDelayedMouseEvent;

  bool mDragState = false;  // for drag purposes
  bool mAccessibleCaretEnabled = false;

  static bool sSelectionEventsOnTextControlsEnabled;
};

#endif /* nsFrameSelection_h___ */
