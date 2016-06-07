/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFrameSelection_h___
#define nsFrameSelection_h___

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/TextRange.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsISelectionController.h"
#include "nsISelectionListener.h"
#include "nsITableCellLayout.h"
#include "nsIDOMElement.h"
#include "WordMovementType.h"
#include "CaretAssociationHint.h"
#include "nsBidiPresUtils.h"

class nsRange;

// IID for the nsFrameSelection interface
// 3c6ae2d0-4cf1-44a1-9e9d-2411867f19c6
#define NS_FRAME_SELECTION_IID      \
{ 0x3c6ae2d0, 0x4cf1, 0x44a1, \
  { 0x9e, 0x9d, 0x24, 0x11, 0x86, 0x7f, 0x19, 0xc6 } }

#define BIDI_LEVEL_UNDEFINED 0x80

//----------------------------------------------------------------------

// Selection interface

struct SelectionDetails
{
#ifdef NS_BUILD_REFCNT_LOGGING
  SelectionDetails() {
    MOZ_COUNT_CTOR(SelectionDetails);
  }
  ~SelectionDetails() {
    MOZ_COUNT_DTOR(SelectionDetails);
  }
#endif
  int32_t mStart;
  int32_t mEnd;
  mozilla::RawSelectionType mRawSelectionType;
  mozilla::TextRangeStyle mTextRangeStyle;
  SelectionDetails *mNext;
};

class nsIPresShell;
class nsIScrollableFrame;

/** PeekOffsetStruct is used to group various arguments (both input and output)
 *  that are passed to nsFrame::PeekOffset(). See below for the description of
 *  individual arguments.
 */
struct MOZ_STACK_CLASS nsPeekOffsetStruct
{
  nsPeekOffsetStruct(nsSelectionAmount aAmount,
                     nsDirection aDirection,
                     int32_t aStartOffset,
                     nsPoint aDesiredPos,
                     bool aJumpLines,
                     bool aScrollViewStop,
                     bool aIsKeyboardSelect,
                     bool aVisual,
                     bool aExtend,
                     mozilla::EWordMovementType aWordMovementType = mozilla::eDefaultBehavior);

  // Note: Most arguments (input and output) are only used with certain values
  // of mAmount. These values are indicated for each argument below.
  // Arguments with no such indication are used with all values of mAmount.

  /*** Input arguments ***/
  // Note: The value of some of the input arguments may be changed upon exit.

  // mAmount: The type of movement requested (by character, word, line, etc.)
  nsSelectionAmount mAmount;

  // mDirection: eDirPrevious or eDirNext.
  //             * Note for visual bidi movement:
  //             eDirPrevious means 'left-then-up' if the containing block is LTR, 
  //             'right-then-up' if it is RTL.
  //             eDirNext means 'right-then-down' if the containing block is LTR, 
  //             'left-then-down' if it is RTL.
  //             Between paragraphs, eDirPrevious means "go to the visual end of the 
  //             previous paragraph", and eDirNext means "go to the visual beginning
  //             of the next paragraph".
  //             Used with: eSelectCharacter, eSelectWord, eSelectLine, eSelectParagraph.
  nsDirection mDirection;

  // mStartOffset: Offset into the content of the current frame where the peek starts.
  //               Used with: eSelectCharacter, eSelectWord
  int32_t mStartOffset;
  
  // mDesiredPos: The desired inline coordinate for the caret
  //              (one of .x or .y will be used, depending on line's writing mode)
  //              Used with: eSelectLine.
  nsPoint mDesiredPos;

  // mWordMovementType: An enum that determines whether to prefer the start or end of a word
  //                    or to use the default beahvior, which is a combination of 
  //                    direction and the platform-based pref
  //                    "layout.word_select.eat_space_to_next_word"
  mozilla::EWordMovementType mWordMovementType;

  // mJumpLines: Whether to allow jumping across line boundaries.
  //             Used with: eSelectCharacter, eSelectWord.
  bool mJumpLines;

  // mScrollViewStop: Whether to stop when reaching a scroll view boundary.
  //                  Used with: eSelectCharacter, eSelectWord, eSelectLine.
  bool mScrollViewStop;

  // mIsKeyboardSelect: Whether the peeking is done in response to a keyboard action.
  //                    Used with: eSelectWord.
  bool mIsKeyboardSelect;

  // mVisual: Whether bidi caret behavior is visual (true) or logical (false).
  //          Used with: eSelectCharacter, eSelectWord, eSelectBeginLine, eSelectEndLine.
  bool mVisual;

  // mExtend: Whether the selection is being extended or moved.
  bool mExtend;

  /*** Output arguments ***/

  // mResultContent: Content reached as a result of the peek.
  nsCOMPtr<nsIContent> mResultContent;

  // mResultFrame: Frame reached as a result of the peek.
  //               Used with: eSelectCharacter, eSelectWord.
  nsIFrame *mResultFrame;

  // mContentOffset: Offset into content reached as a result of the peek.
  int32_t mContentOffset;

  // mAttachForward: When the result position is between two frames,
  //                 indicates which of the two frames the caret should be painted in.
  //                 false means "the end of the frame logically before the caret", 
  //                 true means "the beginning of the frame logically after the caret".
  //                 Used with: eSelectLine, eSelectBeginLine, eSelectEndLine.
  mozilla::CaretAssociationHint mAttach;
};

struct nsPrevNextBidiLevels
{
  void SetData(nsIFrame* aFrameBefore,
               nsIFrame* aFrameAfter,
               nsBidiLevel aLevelBefore,
               nsBidiLevel aLevelAfter)
  {
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
namespace dom {
class Selection;
} // namespace dom
} // namespace mozilla
class nsIScrollableFrame;

/**
 * Methods which are marked with *unsafe* should be handled with special care.
 * They may cause nsFrameSelection to be deleted, if strong pointer isn't used,
 * or they may cause other objects to be deleted.
 */

class nsFrameSelection final {
public:
  typedef mozilla::CaretAssociationHint CaretAssociateHint;

  /*interfaces for addref and release and queryinterface*/
  
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsFrameSelection)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsFrameSelection)

  /** Init will initialize the frame selector with the necessary pres shell to 
   *  be used by most of the methods
   *  @param aShell is the parameter to be used for most of the other calls for callbacks etc
   *  @param aLimiter limits the selection to nodes with aLimiter parents
   */
  void Init(nsIPresShell *aShell, nsIContent *aLimiter);

  /** HandleClick will take the focus to the new frame at the new offset and 
   *  will either extend the selection from the old anchor, or replace the old anchor.
   *  the old anchor and focus position may also be used to deselect things
   *  @param aNewfocus is the content that wants the focus
   *  @param aContentOffset is the content offset of the parent aNewFocus
   *  @param aContentOffsetEnd is the content offset of the parent aNewFocus and is specified different
   *                           when you need to select to and include both start and end points
   *  @param aContinueSelection is the flag that tells the selection to keep the old anchor point or not.
   *  @param aMultipleSelection will tell the frame selector to replace /or not the old selection. 
   *         cannot coexist with aContinueSelection
   *  @param aHint will tell the selection which direction geometrically to actually show the caret on. 
   *         1 = end of this line 0 = beginning of this line
   */
  /*unsafe*/
  nsresult HandleClick(nsIContent *aNewFocus,
                       uint32_t aContentOffset,
                       uint32_t aContentEndOffset,
                       bool aContinueSelection,
                       bool aMultipleSelection,
                       CaretAssociateHint aHint);

  /** HandleDrag extends the selection to contain the frame closest to aPoint.
   *  @param aPresContext is the context to use when figuring out what frame contains the point.
   *  @param aFrame is the parent of all frames to use when searching for the closest frame to the point.
   *  @param aPoint is relative to aFrame
   */
  /*unsafe*/
  void HandleDrag(nsIFrame *aFrame, nsPoint aPoint);

  /** HandleTableSelection will set selection to a table, cell, etc
   *   depending on information contained in aFlags
   *  @param aParentContent is the paretent of either a table or cell that user clicked or dragged the mouse in
   *  @param aContentOffset is the offset of the table or cell
   *  @param aTarget indicates what to select (defined in nsISelectionPrivate.idl/nsISelectionPrivate.h):
   *    TABLESELECTION_CELL      We should select a cell (content points to the cell)
   *    TABLESELECTION_ROW       We should select a row (content points to any cell in row)
   *    TABLESELECTION_COLUMN    We should select a row (content points to any cell in column)
   *    TABLESELECTION_TABLE     We should select a table (content points to the table)
   *    TABLESELECTION_ALLCELLS  We should select all cells (content points to any cell in table)
   *  @param aMouseEvent         passed in so we can get where event occurred and what keys are pressed
   */
  /*unsafe*/
  nsresult HandleTableSelection(nsINode* aParentContent,
                                int32_t aContentOffset,
                                int32_t aTarget,
                                mozilla::WidgetMouseEvent* aMouseEvent);

  /**
   * Add cell to the selection.
   *
   * @param  aCell  [in] HTML td element.
   */
  virtual nsresult SelectCellElement(nsIContent *aCell);

  /**
   * Add cells to the selection inside of the given cells range.
   *
   * @param  aTable             [in] HTML table element
   * @param  aStartRowIndex     [in] row index where the cells range starts
   * @param  aStartColumnIndex  [in] column index where the cells range starts
   * @param  aEndRowIndex       [in] row index where the cells range ends
   * @param  aEndColumnIndex    [in] column index where the cells range ends
   */
  virtual nsresult AddCellsToSelection(nsIContent *aTable,
                                       int32_t aStartRowIndex,
                                       int32_t aStartColumnIndex,
                                       int32_t aEndRowIndex,
                                       int32_t aEndColumnIndex);

  /**
   * Remove cells from selection inside of the given cell range.
   *
   * @param  aTable             [in] HTML table element
   * @param  aStartRowIndex     [in] row index where the cells range starts
   * @param  aStartColumnIndex  [in] column index where the cells range starts
   * @param  aEndRowIndex       [in] row index where the cells range ends
   * @param  aEndColumnIndex    [in] column index where the cells range ends
   */
  virtual nsresult RemoveCellsFromSelection(nsIContent *aTable,
                                            int32_t aStartRowIndex,
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
  virtual nsresult RestrictCellsToSelection(nsIContent *aTable,
                                            int32_t aStartRowIndex,
                                            int32_t aStartColumnIndex,
                                            int32_t aEndRowIndex,
                                            int32_t aEndColumnIndex);

  /** StartAutoScrollTimer is responsible for scrolling frames so that
   *  aPoint is always visible, and for selecting any frame that contains
   *  aPoint. The timer will also reset itself to fire again if we have
   *  not scrolled to the end of the document.
   *  @param aFrame is the outermost frame to use when searching for
   *  the closest frame for the point, i.e. the frame that is capturing
   *  the mouse
   *  @param aPoint is relative to aFrame.
   *  @param aDelay is the timer's interval.
   */
  /*unsafe*/
  nsresult StartAutoScrollTimer(nsIFrame *aFrame,
                                nsPoint aPoint,
                                uint32_t aDelay);

  /** StopAutoScrollTimer stops any active auto scroll timer.
   */
  void StopAutoScrollTimer();

  /** Lookup Selection
   *  returns in frame coordinates the selection beginning and ending with the type of selection given
   * @param aContent is the content asking
   * @param aContentOffset is the starting content boundary
   * @param aContentLength is the length of the content piece asking
   * @param aReturnDetails linkedlist of return values for the selection. 
   * @param aSlowCheck will check using slow method with no shortcuts
   */
  SelectionDetails* LookUpSelection(nsIContent *aContent,
                                    int32_t aContentOffset,
                                    int32_t aContentLength,
                                    bool aSlowCheck) const;

  /** SetDragState(bool);
   *  sets the drag state to aState for resons of drag state.
   * @param aState is the new state of drag
   */
  /*unsafe*/
  void SetDragState(bool aState);

  /** GetDragState(bool *);
   *  gets the drag state to aState for resons of drag state.
   * @param aState will hold the state of drag
   */
  bool GetDragState() const { return mDragState; }

  /**
    if we are in table cell selection mode. aka ctrl click in table cell
   */
  bool GetTableCellSelection() const { return mSelectingTableCellMode != 0; }
  void ClearTableCellSelection() { mSelectingTableCellMode = 0; }

  /** GetSelection
   * no query interface for selection. must use this method now.
   * @param aRawSelectionType The constants defined in nsISelectionController
   *                          for the selection you want.
   */
  mozilla::dom::Selection*
    GetSelection(mozilla::RawSelectionType aRawSelectionType) const;

  /**
   * ScrollSelectionIntoView scrolls a region of the selection,
   * so that it is visible in the scrolled view.
   *
   * @param aRawSelectionType the selection to scroll into view.
   * @param aRegion the region inside the selection to scroll into view.
   * @param aFlags the scroll flags.  Valid bits include:
   * SCROLL_SYNCHRONOUS: when set, scrolls the selection into view
   * before returning. If not set, posts a request which is processed
   * at some point after the method returns.
   * SCROLL_FIRST_ANCESTOR_ONLY: if set, only the first ancestor will be scrolled
   * into view.
   *
   */
  /*unsafe*/
  nsresult ScrollSelectionIntoView(mozilla::RawSelectionType aRawSelectionType,
                                   SelectionRegion aRegion,
                                   int16_t aFlags) const;

  /** RepaintSelection repaints the selected frames that are inside the selection
   *  specified by aRawSelectionType.
   * @param aRawSelectionType The constants defined in nsISelectionController
   *                          for the seleciton you want.
   */
  nsresult RepaintSelection(mozilla::RawSelectionType aRawSelectionType) const;

  /** GetFrameForNodeOffset given a node and its child offset, return the nsIFrame and
   *  the offset into that frame. 
   * @param aNode input parameter for the node to look at
   * @param aOffset offset into above node.
   * @param aReturnOffset will contain offset into frame.
   */
  virtual nsIFrame* GetFrameForNodeOffset(nsIContent*        aNode,
                                          int32_t            aOffset,
                                          CaretAssociateHint aHint,
                                          int32_t*           aReturnOffset) const;

  /**
   * Scrolling then moving caret placement code in common to text areas and 
   * content areas should be located in the implementer
   * This method will accept the following parameters and perform the scroll 
   * and caret movement.  It remains for the caller to call the final 
   * ScrollCaretIntoView if that called wants to be sure the caret is always
   * visible.
   *
   * @param aForward if true, scroll forward if not scroll backward
   * @param aExtend  if true, extend selection to the new point
   * @param aScrollableFrame the frame to scroll
   */
  /*unsafe*/
  void CommonPageMove(bool aForward,
                      bool aExtend,
                      nsIScrollableFrame* aScrollableFrame);

  void SetHint(CaretAssociateHint aHintRight) { mHint = aHintRight; }
  CaretAssociateHint GetHint() const { return mHint; }

  /** SetCaretBidiLevel sets the caret bidi level
   *  @param aLevel the caret bidi level
   *  This method is virtual since it gets called from outside of layout.
   */
  virtual void SetCaretBidiLevel(nsBidiLevel aLevel);
  /** GetCaretBidiLevel gets the caret bidi level
   *  This method is virtual since it gets called from outside of layout.
   */
  virtual nsBidiLevel GetCaretBidiLevel() const;
  /** UndefineCaretBidiLevel sets the caret bidi level to "undefined"
   *  This method is virtual since it gets called from outside of layout.
   */
  virtual void UndefineCaretBidiLevel();

  /** PhysicalMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one unit 'aAmount' in the
   *  given aDirection.
   * @param aDirection  the direction to move the selection
   * @param aAmount     amount of movement (char/line; word/page; eol/doc)
   * @param aExtend     continue selection
   */
  /*unsafe*/
  nsresult PhysicalMove(int16_t aDirection, int16_t aAmount, bool aExtend);

  /** CharacterMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one character left or right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  /*unsafe*/
  nsresult CharacterMove(bool aForward, bool aExtend);

  /** CharacterExtendForDelete extends the selection forward (logically) to
   * the next character cell, so that the selected cell can be deleted.
   */
  /*unsafe*/
  nsresult CharacterExtendForDelete();

  /** CharacterExtendForBackspace extends the selection backward (logically) to
   * the previous character cell, so that the selected cell can be deleted.
   */
  /*unsafe*/
  nsresult CharacterExtendForBackspace();

  /** WordMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one word left or right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  /*unsafe*/
  nsresult WordMove(bool aForward, bool aExtend);

  /** WordExtendForDelete extends the selection backward or forward (logically) to the
   *  next word boundary, so that the selected word can be deleted.
   * @param aForward select forward in document.
   */
  /*unsafe*/
  nsresult WordExtendForDelete(bool aForward);
  
  /** LineMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one line up or down.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  /*unsafe*/
  nsresult LineMove(bool aForward, bool aExtend);

  /** IntraLineMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move to beginning or end of line
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  /*unsafe*/
  nsresult IntraLineMove(bool aForward, bool aExtend); 

  /** Select All will generally be called from the nsiselectioncontroller implementations.
   *  it will select the whole doc
   */
  /*unsafe*/
  nsresult SelectAll();

  /** Sets/Gets The display selection enum.
   */
  void SetDisplaySelection(int16_t aState) { mDisplaySelection = aState; }
  int16_t GetDisplaySelection() const { return mDisplaySelection; }

  /** This method can be used to store the data received during a MouseDown
   *  event so that we can place the caret during the MouseUp event.
   * @aMouseEvent the event received by the selection MouseDown
   *  handling method. A nullptr value can be use to tell this method
   *  that any data is storing is no longer valid.
   */
  void SetDelayedCaretData(mozilla::WidgetMouseEvent* aMouseEvent);

  /** Get the delayed MouseDown event data necessary to place the
   *  caret during MouseUp processing.
   * @return a pointer to the event received
   *  by the selection during MouseDown processing. It can be nullptr
   *  if the data is no longer valid.
   */
  bool HasDelayedCaretData() { return mDelayedMouseEventValid; }
  bool IsShiftDownInDelayedCaretData()
  {
    NS_ASSERTION(mDelayedMouseEventValid, "No valid delayed caret data");
    return mDelayedMouseEventIsShift;
  }
  uint32_t GetClickCountInDelayedCaretData()
  {
    NS_ASSERTION(mDelayedMouseEventValid, "No valid delayed caret data");
    return mDelayedMouseEventClickCount;
  }

  bool MouseDownRecorded()
  {
    return !GetDragState() &&
           HasDelayedCaretData() &&
           GetClickCountInDelayedCaretData() < 2;
  }

  /** Get the content node that limits the selection
   *  When searching up a nodes for parents, as in a text edit field
   *    in an browser page, we must stop at this node else we reach into the 
   *    parent page, which is very bad!
   */
  nsIContent* GetLimiter() const { return mLimiter; }

  nsIContent* GetAncestorLimiter() const { return mAncestorLimiter; }
  /*unsafe*/
  void SetAncestorLimiter(nsIContent *aLimiter);

  /** This will tell the frame selection that a double click has been pressed 
   *  so it can track abort future drags if inside the same selection
   *  @aDoubleDown has the double click down happened
   */
  void SetMouseDoubleDown(bool aDoubleDown) { mMouseDoubleDownState = aDoubleDown; }
  
  /** This will return whether the double down flag was set.
   *  @return whether the double down flag was set
   */
  bool GetMouseDoubleDown() const { return mMouseDoubleDownState; }

  /** GetPrevNextBidiLevels will return the frames and associated Bidi levels of the characters
   *   logically before and after a (collapsed) selection.
   *  @param aNode is the node containing the selection
   *  @param aContentOffset is the offset of the selection in the node
   *  @param aJumpLines If true, look across line boundaries.
   *                    If false, behave as if there were base-level frames at line edges.  
   *
   *  @return A struct holding the before/after frame and the before/after level.
   *
   *  At the beginning and end of each line there is assumed to be a frame with
   *   Bidi level equal to the paragraph embedding level.
   *  In these cases the before frame and after frame respectively will be 
   *   nullptr.
   *
   *  This method is virtual since it gets called from outside of layout. 
   */
  virtual nsPrevNextBidiLevels GetPrevNextBidiLevels(nsIContent *aNode,
                                                     uint32_t aContentOffset,
                                                     bool aJumpLines) const;

  /** GetFrameFromLevel will scan in a given direction
   *   until it finds a frame with a Bidi level less than or equal to a given level.
   *   It will return the last frame before this.
   *  @param aPresContext is the context to use
   *  @param aFrameIn is the frame to start from
   *  @param aDirection is the direction to scan
   *  @param aBidiLevel is the level to search for
   *  @param aFrameOut will hold the frame returned
   */
  nsresult GetFrameFromLevel(nsIFrame *aFrameIn,
                             nsDirection aDirection,
                             nsBidiLevel aBidiLevel,
                             nsIFrame **aFrameOut) const;

  /**
   * MaintainSelection will track the current selection as being "sticky".
   * Dragging or extending selection will never allow for a subset
   * (or the whole) of the maintained selection to become unselected.
   * Primary use: double click selecting then dragging on second click
   * @param aAmount the initial amount of text selected (word, line or paragraph).
   *                For "line", use eSelectBeginLine.
   */
  nsresult MaintainSelection(nsSelectionAmount aAmount = eSelectNoAmount);

  nsresult ConstrainFrameAndPointToAnchorSubtree(nsIFrame *aFrame,
                                                 nsPoint& aPoint,
                                                 nsIFrame **aRetFrame,
                                                 nsPoint& aRetPoint);

  nsFrameSelection();

  void StartBatchChanges();
  void EndBatchChanges(int16_t aReason = nsISelectionListener::NO_REASON);

  /*unsafe*/
  nsresult DeleteFromDocument();

  nsIPresShell *GetShell()const  { return mShell; }

  void DisconnectFromPresShell();
  nsresult ClearNormalSelection();

private:
  ~nsFrameSelection();

  nsresult TakeFocus(nsIContent *aNewFocus,
                     uint32_t aContentOffset,
                     uint32_t aContentEndOffset,
                     CaretAssociateHint aHint,
                     bool aContinueSelection,
                     bool aMultipleSelection);

  void BidiLevelFromMove(nsIPresShell* aPresShell,
                         nsIContent *aNode,
                         uint32_t aContentOffset,
                         nsSelectionAmount aAmount,
                         CaretAssociateHint aHint);
  void BidiLevelFromClick(nsIContent *aNewFocus, uint32_t aContentOffset);
  nsPrevNextBidiLevels GetPrevNextBidiLevels(nsIContent *aNode,
                                             uint32_t aContentOffset,
                                             CaretAssociateHint aHint,
                                             bool aJumpLines) const;

  bool AdjustForMaintainedSelection(nsIContent *aContent, int32_t aOffset);

// post and pop reasons for notifications. we may stack these later
  void    PostReason(int16_t aReason) { mSelectionChangeReason = aReason; }
  int16_t PopReason()
  {
    int16_t retval = mSelectionChangeReason;
    mSelectionChangeReason = nsISelectionListener::NO_REASON;
    return retval;
  }
  bool IsUserSelectionReason() const
  {
    return (mSelectionChangeReason &
            (nsISelectionListener::DRAG_REASON |
             nsISelectionListener::MOUSEDOWN_REASON |
             nsISelectionListener::MOUSEUP_REASON |
             nsISelectionListener::KEYPRESS_REASON)) !=
           nsISelectionListener::NO_REASON;
  }

  friend class mozilla::dom::Selection;
  friend struct mozilla::AutoPrepareFocusRange;
#ifdef DEBUG
  void printSelection();       // for debugging
#endif /* DEBUG */

  void ResizeBuffer(uint32_t aNewBufSize);

/*HELPER METHODS*/
  // Whether MoveCaret should use logical or visual movement,
  // or follow the bidi.edit.caret_movement_style preference.
  enum CaretMovementStyle {
    eLogical,
    eVisual,
    eUsePrefStyle
  };
  nsresult     MoveCaret(nsDirection aDirection, bool aContinueSelection,
                         nsSelectionAmount aAmount,
                         CaretMovementStyle aMovementStyle);

  nsresult     FetchDesiredPos(nsPoint &aDesiredPos); //the position requested by the Key Handling for up down
  void         InvalidateDesiredPos(); //do not listen to mDesiredPos you must get another.
  void         SetDesiredPos(nsPoint aPos); //set the mDesiredPos

  uint32_t     GetBatching() const {return mBatching; }
  bool         GetNotifyFrames() const { return mNotifyFrames; }
  void         SetDirty(bool aDirty=true){if (mBatching) mChangesDuringBatching = aDirty;}

  // nsFrameSelection may get deleted when calling this,
  // so remember to use nsCOMPtr when needed.
  nsresult     NotifySelectionListeners(
                 mozilla::RawSelectionType aRawSelectionType);

  RefPtr<mozilla::dom::Selection> mDomSelections[nsISelectionController::NUM_SELECTIONTYPES];

  // Table selection support.
  nsITableCellLayout* GetCellLayout(nsIContent *aCellContent) const;

  nsresult SelectBlockOfCells(nsIContent *aStartNode, nsIContent *aEndNode);
  nsresult SelectRowOrColumn(nsIContent *aCellContent, uint32_t aTarget);
  nsresult UnselectCells(nsIContent *aTable,
                         int32_t aStartRowIndex, int32_t aStartColumnIndex,
                         int32_t aEndRowIndex, int32_t aEndColumnIndex,
                         bool aRemoveOutsideOfCellRange);

  nsresult GetCellIndexes(nsIContent *aCell, int32_t &aRowIndex, int32_t &aColIndex);

  // Get our first range, if its first selected node is a cell.  If this does
  // not return null, then the first node in the returned range is a cell
  // (according to GetFirstCellNodeInRange).
  nsRange* GetFirstCellRange();
  // Get our next range, if its first selected node is a cell.  If this does
  // not return null, then the first node in the returned range is a cell
  // (according to GetFirstCellNodeInRange).
  nsRange* GetNextCellRange();
  nsIContent* GetFirstCellNodeInRange(nsRange *aRange) const;
  // Returns non-null table if in same table, null otherwise
  nsIContent* IsInSameTable(nsIContent *aContent1, nsIContent *aContent2) const;
  // Might return null
  nsIContent* GetParentTable(nsIContent *aCellNode) const;
  nsresult CreateAndAddRange(nsINode *aParentNode, int32_t aOffset);

  nsCOMPtr<nsINode> mCellParent; //used to snap to table selection
  nsCOMPtr<nsIContent> mStartSelectedCell;
  nsCOMPtr<nsIContent> mEndSelectedCell;
  nsCOMPtr<nsIContent> mAppendStartSelectedCell;
  nsCOMPtr<nsIContent> mUnselectCellOnMouseUp;
  int32_t  mSelectingTableCellMode;
  int32_t  mSelectedCellIndex;

  // maintain selection
  RefPtr<nsRange> mMaintainRange;
  nsSelectionAmount mMaintainedAmount;

  //batching
  int32_t mBatching;
    
  // Limit selection navigation to a child of this node.
  nsCOMPtr<nsIContent> mLimiter;
  // Limit selection navigation to a descendant of this node.
  nsCOMPtr<nsIContent> mAncestorLimiter;

  nsIPresShell *mShell;

  int16_t mSelectionChangeReason; // reason for notifications of selection changing
  int16_t mDisplaySelection; //for visual display purposes.

  CaretAssociateHint mHint;   //hint to tell if the selection is at the end of this line or beginning of next
  nsBidiLevel mCaretBidiLevel;
  nsBidiLevel mKbdBidiLevel;

  nsPoint mDesiredPos;
  uint32_t mDelayedMouseEventClickCount;
  bool mDelayedMouseEventIsShift;
  bool mDelayedMouseEventValid;

  bool mChangesDuringBatching;
  bool mNotifyFrames;
  bool mDragSelectingCells;
  bool mDragState;   //for drag purposes
  bool mMouseDoubleDownState; //has the doubleclick down happened
  bool mDesiredPosSet;

  int8_t mCaretMovementStyle;

  static bool sSelectionEventsEnabled;
};

#endif /* nsFrameSelection_h___ */
