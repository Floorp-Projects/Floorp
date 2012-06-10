/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFrameSelection_h___
#define nsFrameSelection_h___

#include "mozilla/Attributes.h"

#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsISelectionController.h"
#include "nsITableLayout.h"
#include "nsITableCellLayout.h"
#include "nsIDOMElement.h"
#include "nsGUIEvent.h"
#include "nsRange.h"

// IID for the nsFrameSelection interface
// 3c6ae2d0-4cf1-44a1-9e9d-2411867f19c6
#define NS_FRAME_SELECTION_IID      \
{ 0x3c6ae2d0, 0x4cf1, 0x44a1, \
  { 0x9e, 0x9d, 0x24, 0x11, 0x86, 0x7f, 0x19, 0xc6 } }

#ifdef IBMBIDI // Constant for Set/Get CaretBidiLevel
#define BIDI_LEVEL_UNDEFINED 0x80
#endif

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
  PRInt32 mStart;
  PRInt32 mEnd;
  SelectionType mType;
  nsTextRangeStyle mTextRangeStyle;
  SelectionDetails *mNext;
};

class nsIPresShell;
class nsIScrollableFrame;

enum EWordMovementType { eStartWord, eEndWord, eDefaultBehavior };

/** PeekOffsetStruct is used to group various arguments (both input and output)
 *  that are passed to nsFrame::PeekOffset(). See below for the description of
 *  individual arguments.
 */
struct NS_STACK_CLASS nsPeekOffsetStruct
{
  nsPeekOffsetStruct(nsSelectionAmount aAmount,
                     nsDirection aDirection,
                     PRInt32 aStartOffset,
                     nscoord aDesiredX,
                     bool aJumpLines,
                     bool aScrollViewStop,
                     bool aIsKeyboardSelect,
                     bool aVisual,
                     EWordMovementType aWordMovementType = eDefaultBehavior)
    : mAmount(aAmount)
    , mDirection(aDirection)
    , mStartOffset(aStartOffset)
    , mDesiredX(aDesiredX)
    , mWordMovementType(aWordMovementType)
    , mJumpLines(aJumpLines)
    , mScrollViewStop(aScrollViewStop)
    , mIsKeyboardSelect(aIsKeyboardSelect)
    , mVisual(aVisual)
    , mResultContent()
    , mResultFrame(nsnull)
    , mContentOffset(0)
    , mAttachForward(false)
  {
  }

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
  PRInt32 mStartOffset;
  
  // mDesiredX: The desired x coordinate for the caret.
  //            Used with: eSelectLine.
  nscoord mDesiredX;

  // mWordMovementType: An enum that determines whether to prefer the start or end of a word
  //                    or to use the default beahvior, which is a combination of 
  //                    direction and the platform-based pref
  //                    "layout.word_select.eat_space_to_next_word"
  EWordMovementType mWordMovementType;

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

  /*** Output arguments ***/

  // mResultContent: Content reached as a result of the peek.
  nsCOMPtr<nsIContent> mResultContent;

  // mResultFrame: Frame reached as a result of the peek.
  //               Used with: eSelectCharacter, eSelectWord.
  nsIFrame *mResultFrame;

  // mContentOffset: Offset into content reached as a result of the peek.
  PRInt32 mContentOffset;

  // mAttachForward: When the result position is between two frames,
  //                 indicates which of the two frames the caret should be painted in.
  //                 false means "the end of the frame logically before the caret", 
  //                 true means "the beginning of the frame logically after the caret".
  //                 Used with: eSelectLine, eSelectBeginLine, eSelectEndLine.
  bool mAttachForward;
};

struct nsPrevNextBidiLevels
{
  void SetData(nsIFrame* aFrameBefore,
               nsIFrame* aFrameAfter,
               PRUint8 aLevelBefore,
               PRUint8 aLevelAfter)
  {
    mFrameBefore = aFrameBefore;
    mFrameAfter = aFrameAfter;
    mLevelBefore = aLevelBefore;
    mLevelAfter = aLevelAfter;
  }
  nsIFrame* mFrameBefore;
  nsIFrame* mFrameAfter;
  PRUint8 mLevelBefore;
  PRUint8 mLevelAfter;
};

namespace mozilla {
class Selection;
}
class nsIScrollableFrame;

/**
 * Methods which are marked with *unsafe* should be handled with special care.
 * They may cause nsFrameSelection to be deleted, if strong pointer isn't used,
 * or they may cause other objects to be deleted.
 */

class nsFrameSelection MOZ_FINAL : public nsISupports {
public:
  enum HINT { HINTLEFT = 0, HINTRIGHT = 1};  //end of this line or beginning of next
  /*interfaces for addref and release and queryinterface*/
  
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsFrameSelection)

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
                       PRUint32 aContentOffset,
                       PRUint32 aContentEndOffset,
                       bool aContinueSelection,
                       bool aMultipleSelection,
                       bool aHint);

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
  nsresult HandleTableSelection(nsINode *aParentContent,
                                PRInt32 aContentOffset,
                                PRInt32 aTarget,
                                nsMouseEvent *aMouseEvent);

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
                                       PRInt32 aStartRowIndex,
                                       PRInt32 aStartColumnIndex,
                                       PRInt32 aEndRowIndex,
                                       PRInt32 aEndColumnIndex);

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
                                            PRInt32 aStartRowIndex,
                                            PRInt32 aStartColumnIndex,
                                            PRInt32 aEndRowIndex,
                                            PRInt32 aEndColumnIndex);

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
                                            PRInt32 aStartRowIndex,
                                            PRInt32 aStartColumnIndex,
                                            PRInt32 aEndRowIndex,
                                            PRInt32 aEndColumnIndex);

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
                                PRUint32 aDelay);

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
                                    PRInt32 aContentOffset,
                                    PRInt32 aContentLength,
                                    bool aSlowCheck) const;

  /** SetMouseDownState(bool);
   *  sets the mouse state to aState for resons of drag state.
   * @param aState is the new state of mousedown
   */
  /*unsafe*/
  void SetMouseDownState(bool aState);

  /** GetMouseDownState(bool *);
   *  gets the mouse state to aState for resons of drag state.
   * @param aState will hold the state of mousedown
   */
  bool GetMouseDownState() const { return mMouseDownState; }

  /**
    if we are in table cell selection mode. aka ctrl click in table cell
   */
  bool GetTableCellSelection() const { return mSelectingTableCellMode != 0; }
  void ClearTableCellSelection() { mSelectingTableCellMode = 0; }

  /** GetSelection
   * no query interface for selection. must use this method now.
   * @param aSelectionType enum value defined in nsISelection for the seleciton you want.
   */
  mozilla::Selection* GetSelection(SelectionType aType) const;

  /**
   * ScrollSelectionIntoView scrolls a region of the selection,
   * so that it is visible in the scrolled view.
   *
   * @param aType the selection to scroll into view.
   * @param aRegion the region inside the selection to scroll into view.
   * @param aFlags the scroll flags.  Valid bits include:
   * SCROLL_SYNCHRONOUS: when set, scrolls the selection into view
   * before returning. If not set, posts a request which is processed
   * at some point after the method returns.
   * SCROLL_FIRST_ANCESTOR_ONLY: if set, only the first ancestor will be scrolled
   * into view.
   */
  /*unsafe*/
  nsresult ScrollSelectionIntoView(SelectionType aType,
                                   SelectionRegion aRegion,
                                   PRInt16 aFlags) const;

  /** RepaintSelection repaints the selected frames that are inside the selection
   *  specified by aSelectionType.
   * @param aSelectionType enum value defined in nsISelection for the seleciton you want.
   */
  nsresult RepaintSelection(SelectionType aType) const;

  /** GetFrameForNodeOffset given a node and its child offset, return the nsIFrame and
   *  the offset into that frame. 
   * @param aNode input parameter for the node to look at
   * @param aOffset offset into above node.
   * @param aReturnOffset will contain offset into frame.
   */
  virtual nsIFrame* GetFrameForNodeOffset(nsIContent *aNode,
                                          PRInt32     aOffset,
                                          HINT        aHint,
                                          PRInt32    *aReturnOffset) const;

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

  void SetHint(HINT aHintRight) { mHint = aHintRight; }
  HINT GetHint() const { return mHint; }
  
#ifdef IBMBIDI
  /** SetCaretBidiLevel sets the caret bidi level
   *  @param aLevel the caret bidi level
   *  This method is virtual since it gets called from outside of layout.
   */
  virtual void SetCaretBidiLevel (PRUint8 aLevel);
  /** GetCaretBidiLevel gets the caret bidi level
   *  This method is virtual since it gets called from outside of layout.
   */
  virtual PRUint8 GetCaretBidiLevel() const;
  /** UndefineCaretBidiLevel sets the caret bidi level to "undefined"
   *  This method is virtual since it gets called from outside of layout.
   */
  virtual void UndefineCaretBidiLevel();
#endif

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
  void SetDisplaySelection(PRInt16 aState) { mDisplaySelection = aState; }
  PRInt16 GetDisplaySelection() const { return mDisplaySelection; }

  /** This method can be used to store the data received during a MouseDown
   *  event so that we can place the caret during the MouseUp event.
   * @aMouseEvent the event received by the selection MouseDown
   *  handling method. A NULL value can be use to tell this method
   *  that any data is storing is no longer valid.
   */
  void SetDelayedCaretData(nsMouseEvent *aMouseEvent);

  /** Get the delayed MouseDown event data necessary to place the
   *  caret during MouseUp processing.
   * @return a pointer to the event received
   *  by the selection during MouseDown processing. It can be NULL
   *  if the data is no longer valid.
   */
  bool HasDelayedCaretData() { return mDelayedMouseEventValid; }
  bool IsShiftDownInDelayedCaretData()
  {
    NS_ASSERTION(mDelayedMouseEventValid, "No valid delayed caret data");
    return mDelayedMouseEventIsShift;
  }
  PRUint32 GetClickCountInDelayedCaretData()
  {
    NS_ASSERTION(mDelayedMouseEventValid, "No valid delayed caret data");
    return mDelayedMouseEventClickCount;
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
   *   nsnull.
   *
   *  This method is virtual since it gets called from outside of layout. 
   */
  virtual nsPrevNextBidiLevels GetPrevNextBidiLevels(nsIContent *aNode,
                                                     PRUint32 aContentOffset,
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
                             PRUint8 aBidiLevel,
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

  nsFrameSelection();

  void StartBatchChanges();
  void EndBatchChanges();
  /*unsafe*/
  nsresult DeleteFromDocument();

  nsIPresShell *GetShell()const  { return mShell; }

  void DisconnectFromPresShell();
private:
  nsresult TakeFocus(nsIContent *aNewFocus,
                     PRUint32 aContentOffset,
                     PRUint32 aContentEndOffset,
                     HINT aHint,
                     bool aContinueSelection,
                     bool aMultipleSelection);

  void BidiLevelFromMove(nsIPresShell* aPresShell,
                         nsIContent *aNode,
                         PRUint32 aContentOffset,
                         PRUint32 aKeycode,
                         HINT aHint);
  void BidiLevelFromClick(nsIContent *aNewFocus, PRUint32 aContentOffset);
  nsPrevNextBidiLevels GetPrevNextBidiLevels(nsIContent *aNode,
                                             PRUint32 aContentOffset,
                                             HINT aHint,
                                             bool aJumpLines) const;

  bool AdjustForMaintainedSelection(nsIContent *aContent, PRInt32 aOffset);

// post and pop reasons for notifications. we may stack these later
  void    PostReason(PRInt16 aReason) { mSelectionChangeReason = aReason; }
  PRInt16 PopReason()
  {
    PRInt16 retval = mSelectionChangeReason;
    mSelectionChangeReason = 0;
    return retval;
  }

  friend class mozilla::Selection;
#ifdef DEBUG
  void printSelection();       // for debugging
#endif /* DEBUG */

  void ResizeBuffer(PRUint32 aNewBufSize);
/*HELPER METHODS*/
  nsresult     MoveCaret(PRUint32 aKeycode, bool aContinueSelection,
                         nsSelectionAmount aAmount);
  nsresult     MoveCaret(PRUint32 aKeycode, bool aContinueSelection,
                         nsSelectionAmount aAmount,
                         bool aVisualMovement);

  nsresult     FetchDesiredX(nscoord &aDesiredX); //the x position requested by the Key Handling for up down
  void         InvalidateDesiredX(); //do not listen to mDesiredX you must get another.
  void         SetDesiredX(nscoord aX); //set the mDesiredX

  nsresult     ConstrainFrameAndPointToAnchorSubtree(nsIFrame *aFrame, nsPoint& aPoint, nsIFrame **aRetFrame, nsPoint& aRetPoint);

  PRUint32     GetBatching() const {return mBatching; }
  bool         GetNotifyFrames() const { return mNotifyFrames; }
  void         SetDirty(bool aDirty=true){if (mBatching) mChangesDuringBatching = aDirty;}

  // nsFrameSelection may get deleted when calling this,
  // so remember to use nsCOMPtr when needed.
  nsresult     NotifySelectionListeners(SelectionType aType);     // add parameters to say collapsed etc?

  nsRefPtr<mozilla::Selection> mDomSelections[nsISelectionController::NUM_SELECTIONTYPES];

  // Table selection support.
  // Interfaces that let us get info based on cellmap locations
  nsITableLayout* GetTableLayout(nsIContent *aTableContent) const;
  nsITableCellLayout* GetCellLayout(nsIContent *aCellContent) const;

  nsresult SelectBlockOfCells(nsIContent *aStartNode, nsIContent *aEndNode);
  nsresult SelectRowOrColumn(nsIContent *aCellContent, PRUint32 aTarget);
  nsresult UnselectCells(nsIContent *aTable,
                         PRInt32 aStartRowIndex, PRInt32 aStartColumnIndex,
                         PRInt32 aEndRowIndex, PRInt32 aEndColumnIndex,
                         bool aRemoveOutsideOfCellRange);

  nsresult GetCellIndexes(nsIContent *aCell, PRInt32 &aRowIndex, PRInt32 &aColIndex);

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
  nsresult CreateAndAddRange(nsINode *aParentNode, PRInt32 aOffset);
  nsresult ClearNormalSelection();

  nsCOMPtr<nsINode> mCellParent; //used to snap to table selection
  nsCOMPtr<nsIContent> mStartSelectedCell;
  nsCOMPtr<nsIContent> mEndSelectedCell;
  nsCOMPtr<nsIContent> mAppendStartSelectedCell;
  nsCOMPtr<nsIContent> mUnselectCellOnMouseUp;
  PRInt32  mSelectingTableCellMode;
  PRInt32  mSelectedCellIndex;

  // maintain selection
  nsRefPtr<nsRange> mMaintainRange;
  nsSelectionAmount mMaintainedAmount;

  //batching
  PRInt32 mBatching;
    
  nsIContent *mLimiter;     //limit selection navigation to a child of this node.
  nsIContent *mAncestorLimiter; // Limit selection navigation to a descendant of
                                // this node.
  nsIPresShell *mShell;

  PRInt16 mSelectionChangeReason; // reason for notifications of selection changing
  PRInt16 mDisplaySelection; //for visual display purposes.

  HINT  mHint;   //hint to tell if the selection is at the end of this line or beginning of next
#ifdef IBMBIDI
  PRUint8 mCaretBidiLevel;
#endif

  PRInt32 mDesiredX;
  PRUint32 mDelayedMouseEventClickCount;
  bool mDelayedMouseEventIsShift;
  bool mDelayedMouseEventValid;

  bool mChangesDuringBatching;
  bool mNotifyFrames;
  bool mDragSelectingCells;
  bool mMouseDownState;   //for drag purposes
  bool mMouseDoubleDownState; //has the doubleclick down happened
  bool mDesiredXSet;

  PRInt8 mCaretMovementStyle;
};

#endif /* nsFrameSelection_h___ */
