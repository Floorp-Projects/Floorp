/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * NOTE!!  This is not a general class, but specific to layout and frames.
 * Consumers looking for the general selection interface should look at
 * nsISelection.
 */

#ifndef nsIFrameSelection_h___
#define nsIFrameSelection_h___

#include "nsISupports.h"
#include "nsIFrame.h"
#include "nsIFocusTracker.h"   
#include "nsISelection.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsIStyleContext.h"
#include "nsISelectionController.h"


// IID for the nsIFrameSelection interface
#define NS_IFRAMESELECTION_IID      \
{ 0xf46e4171, 0xdeaa, 0x11d1, \
  { 0x97, 0xfc, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }


//----------------------------------------------------------------------

// Selection interface

struct SelectionDetails
{
  PRInt32 mStart;
  PRInt32 mEnd;
  SelectionType mType;
  SelectionDetails *mNext;
};

/*PeekOffsetStruct
   *  @param mTracker is used to get the PresContext usefull for measuring text ect.
   *  @param mDesiredX is the "desired" location of the new caret
   *  @param mAmount eWord, eCharacter, eLine
   *  @param mDirection enum defined in this file to be eForward or eBackward
   *  @param mStartOffset start offset to start the peek. 0 == beginning -1 = end
   *  @param mResultContent content that actually is the next/previous
   *  @param mResultOffset offset for result content
   *  @param mResultFrame resulting frame for peeking
   *  @param mEatingWS boolean to tell us the state of our search for Next/Prev
   *  @param mPreferLeft true = prev line end, false = next line begin
   *  @param mJumpLines if this is true then its ok to cross lines while peeking
*/
struct nsPeekOffsetStruct
{
  void SetData(nsIFocusTracker *aTracker, 
               nscoord aDesiredX, 
               nsSelectionAmount aAmount,
               nsDirection aDirection,
               PRInt32 aStartOffset, 
               PRBool aEatingWS,
               PRBool aPreferLeft,
               PRBool aJumpLines)
      {
       mTracker=aTracker;mDesiredX=aDesiredX;mAmount=aAmount;
       mDirection=aDirection;mStartOffset=aStartOffset;mEatingWS=aEatingWS;
       mPreferLeft=aPreferLeft;mJumpLines = aJumpLines;
      }
  nsIFocusTracker *mTracker;
  nscoord mDesiredX;
  nsSelectionAmount mAmount;
  nsDirection mDirection;
  PRInt32 mStartOffset;
  nsCOMPtr<nsIContent> mResultContent;
  PRInt32 mContentOffset;
  PRInt32 mContentOffsetEnd;
  nsIFrame *mResultFrame;
  PRBool mEatingWS;
  PRBool mPreferLeft;
  PRBool mJumpLines;
};

class nsIScrollableView;


class nsIFrameSelection : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IFRAMESELECTION_IID; return iid; }
  enum HINT {HINTLEFT=0,HINTRIGHT=1}mHint;//end of this line or beginning of next

  /** Init will initialize the frame selector with the necessary focus tracker to 
   *  be used by most of the methods
   *  @param aTracker is the parameter to be used for most of the other calls for callbacks ect
   *  @param aLimiter limits the selection to nodes with aLimiter parents
   */
  NS_IMETHOD Init(nsIFocusTracker *aTracker, nsIContent *aLimiter) = 0; //default since this isnt used for embedding

  /* SetScrollableView sets the scroll view
   *  @param aScrollView is the scroll view for this selection.
   */
  NS_IMETHOD SetScrollableView(nsIScrollableView *aScrollView) =0;

  /* GetScrollableView gets the current scroll view
   *  @param aScrollView is the scroll view for this selection.
   */
  NS_IMETHOD GetScrollableView(nsIScrollableView **aScrollView) =0;

  /** ShutDown will be called when the owner of the frame selection is shutting down
   *  this should be the time to release all member variable interfaces. all methods
   *  called after ShutDown should return NS_ERROR_FAILURE
   */
  NS_IMETHOD ShutDown() = 0;

  /** HandleKeyEvent will accept an event and frame and 
   *  will return NS_OK if it handles the event or NS_COMFALSE if not.
   *  <P>DOES NOT ADDREF<P>
   *  @param aGuiEvent is the event that should be dealt with by aFocusFrame
   *  @param aFrame is the frame that MAY handle the event
   */
  NS_IMETHOD HandleTextEvent(nsGUIEvent *aGuiEvent) = 0;

  /** HandleKeyEvent will accept an event and frame and 
   *  will return NS_OK if it handles the event or NS_COMFALSE if not.
   *  <P>DOES NOT ADDREF<P>
   *  @param aGuiEvent is the event that should be dealt with by aFocusFrame
   *  @param aFrame is the frame that MAY handle the event
   */
  NS_IMETHOD HandleKeyEvent(nsIPresContext* aPresContext, nsGUIEvent *aGuiEvent) = 0;

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
   *         1 = end of this line 0 = beggining of this line
   */
  NS_IMETHOD HandleClick(nsIContent *aNewFocus, PRUint32 aContentOffset, PRUint32 aContentEndOffset , 
                       PRBool aContinueSelection, PRBool aMultipleSelection, PRBool aHint) = 0; 

  /** HandleDrag extends the selection to contain the frame closest to aPoint.
   *  @param aPresContext is the context to use when figuring out what frame contains the point.
   *  @param aFrame is the parent of all frames to use when searching for the closest frame to the point.
   *  @param aPoint is relative to aFrame's parent view.
   */
  NS_IMETHOD HandleDrag(nsIPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint) = 0;

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
   *  @param aMouseEvent         passed in so we we can get where event occured and what keys are pressed
   */
  NS_IMETHOD HandleTableSelection(nsIContent *aParentContent, PRInt32 aContentOffset, PRInt32 aTarget, nsMouseEvent *aMouseEvent) = 0;

  /** StartAutoScrollTimer is responsible for scrolling the view so that aPoint is always
   *  visible, and for selecting any frame that contains aPoint. The timer will also reset
   *  itself to fire again if the view has not scrolled to the end of the document.
   *  @param aPresContext is the context to use when figuring out what frame contains the point.
   *  @param aFrame is the parent of all frames to use when searching for the closest frame to the point.
   *  @param aPoint is relative to aFrame's parent view.
   *  @param aDelay is the timer's interval.
   */
  NS_IMETHOD StartAutoScrollTimer(nsIPresContext *aPresContext, nsIFrame *aFrame, nsPoint& aPoint, PRUint32 aDelay) = 0;

  /** StopAutoScrollTimer stops any active auto scroll timer.
   */
  NS_IMETHOD StopAutoScrollTimer() = 0;

  /** EnableFrameNotification
   *  mutch like start batching, except all dirty calls are ignored. no notifications will go 
   *  out until enableNotifications with a PR_TRUE is called
   */
  NS_IMETHOD EnableFrameNotification(PRBool aEnable) = 0;

  /** Lookup Selection
   *  returns in frame coordinates the selection beginning and ending with the type of selection given
   * @param aContent is the content asking
   * @param aContentOffset is the starting content boundary
   * @param aContentLength is the length of the content piece asking
   * @param aReturnDetails linkedlist of return values for the selection. 
   * @param aSlowCheck will check using slow method with no shortcuts
   */
  NS_IMETHOD LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                             SelectionDetails **aReturnDetails, PRBool aSlowCheck) = 0;

  /** SetMouseDownState(PRBool);
   *  sets the mouse state to aState for resons of drag state.
   * @param aState is the new state of mousedown
   */
  NS_IMETHOD SetMouseDownState(PRBool aState)=0;

  /** GetMouseDownState(PRBool *);
   *  gets the mouse state to aState for resons of drag state.
   * @param aState will hold the state of mousedown
   */
  NS_IMETHOD GetMouseDownState(PRBool *aState)=0;

  /**
    if we are in table cell selection mode. aka ctrl click in table cell
   */
  NS_IMETHOD GetTableCellSelection(PRBool *aState)=0;

  /** GetSelection
   * no query interface for selection. must use this method now.
   * @param aSelectionType enum value defined in nsISelection for the seleciton you want.
   */
  NS_IMETHOD GetSelection(SelectionType aSelectionType, nsISelection **aSelection)=0;

  /**
   * ScrollSelectionIntoView scrolls a region of the selection,
   * so that it is visible in the scrolled view.
   *
   * @param aType the selection to scroll into view.
   * @param aRegion the region inside the selection to scroll into view.
   */
  NS_IMETHOD ScrollSelectionIntoView(SelectionType aSelectionType, SelectionRegion aRegion)=0;

  /** RepaintSelection repaints the selected frames that are inside the selection
   *  specified by aSelectionType.
   * @param aSelectionType enum value defined in nsISelection for the seleciton you want.
   */
  NS_IMETHOD RepaintSelection(nsIPresContext* aPresContext, SelectionType aSelectionType)=0;

  /** GetFrameForNodeOffset given a node and its child offset, return the nsIFrame and
   *  the offset into that frame. 
   * @param aNode input parameter for the node to look at
   * @param aOffset offset into above node.
   * @param aReturnFrame will contain the return frame. MUST NOT BE NULL or will return error
   * @param aReturnOffset will contain offset into frame.
   */
  NS_IMETHOD GetFrameForNodeOffset(nsIContent *aNode, PRInt32 aOffset, HINT aHint, nsIFrame **aReturnFrame, PRInt32 *aReturnOffset)=0;

  /** AdjustOffsetsFromStyle. Called after detecting that a click or drag will
   *  select the frame, this function looks for user-select style on that frame or a parent
   *  frame, and adjust the content and offsets accordingly.
   * @param aFrame the frame that was clicked
   * @param outContent content node to be selected
   * @param outStartOffset selection start offset
   * @param outEndOffset selection end offset
   */
  NS_IMETHOD AdjustOffsetsFromStyle(nsIFrame *aFrame, PRBool *changeSelection,
        nsIContent** outContent, PRInt32* outStartOffset, PRInt32* outEndOffset)=0;
  
  
  NS_IMETHOD GetHint(HINT *aHint)=0;
  NS_IMETHOD SetHint(HINT aHint)=0;

  /** CharacterMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one character left or right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  NS_IMETHOD CharacterMove(PRBool aForward, PRBool aExtend)=0;

  /** WordMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one word left or right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  NS_IMETHOD WordMove(PRBool aForward, PRBool aExtend)=0;

  /** LineMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move one line up or down.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  NS_IMETHOD LineMove(PRBool aForward, PRBool aExtend)=0;

  /** IntraLineMove will generally be called from the nsiselectioncontroller implementations.
   *  the effect being the selection will move to beginning or end of line
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  NS_IMETHOD IntraLineMove(PRBool aForward, PRBool aExtend)=0;

  /** Select All will generally be called from the nsiselectioncontroller implementations.
   *  it will select the whole doc
   */
  NS_IMETHOD SelectAll()=0;

  /** Sets/Gets The display selection enum.
   */
  NS_IMETHOD SetDisplaySelection(PRInt16 aState)=0;
  NS_IMETHOD GetDisplaySelection(PRInt16 *aState)=0;

  /** Allow applications to specify how we should place the caret
   *  when the user clicks over an existing selection. A aDelay
   *  value of PR_TRUE means delay clearing the selection and
   *  placing the caret until MouseUp, when the user clicks over
   *  an existing selection. This is especially usefull when applications
   *  want to support Drag & Drop of the current selection. A value
   *  of PR_FALSE means place the caret immediately. If the application
   *  never calls this method, the nsIFrameSelection implementation
   *  assumes the default value is PR_TRUE.
   * @param aDelay PR_TRUE if we should delay caret placement.
   */
  NS_IMETHOD SetDelayCaretOverExistingSelection(PRBool aDelay)=0;

  /** Get the current delay caret setting. If aDelay contains
   *  a return value of PR_TRUE, the caret is placed on MouseUp
   *  when clicking over an existing selection. If PR_FALSE,
   *  the selection is cleared and caret is placed immediately
   *  in all cases.
   * @param aDelay will contain the return value.
   */
  NS_IMETHOD GetDelayCaretOverExistingSelection(PRBool *aDelay)=0;

  /** If we are delaying caret placement til MouseUp (see
   *  Set/GetDelayCaretOverExistingSelection()), this method
   *  can be used to store the data received during the MouseDown
   *  so that we can place the caret during the MouseUp event.
   * @aMouseEvent the event received by the selection MouseDown
   *  handling method. A NULL value can be use to tell this method
   *  that any data is storing is no longer valid.
   */
  NS_IMETHOD SetDelayedCaretData(nsMouseEvent *aMouseEvent)=0;

  /** Get the delayed MouseDown event data neccessary to place the
   *  caret during MouseUp processing.
   * @aMouseEvent will contain a pointer to the event received
   *  by the selection during MouseDown processing. It can be NULL
   *  if the data is no longer valid.
   */
  NS_IMETHOD GetDelayedCaretData(nsMouseEvent **aMouseEvent)=0;


  /** Get the content node that limits the selection
   *  When searching up a nodes for parents, as in a text edit field
   *    in an browser page, we must stop at this node else we reach into the 
   *    parent page, which is very bad!
   */
  NS_IMETHOD GetLimiter(nsIContent **aLimiterContent)=0;

#ifdef IBMBIDI
  /** GetPrevNextBidiLevels will return the frames and associated Bidi levels of the characters
   *   logically before and after a (collapsed) selection.
   *  @param aPresContext is the context to use
   *  @param aNode is the node containing the selection
   *  @param aContentOffset is the offset of the selection in the node
   *  @param aPrevFrame will hold the frame of the character before the selection
   *  @param aNextFrame will hold the frame of the character after the selection
   *  @param aPrevLevel will hold the Bidi level of the character before the selection
   *  @param aNextLevel will hold the Bidi level of the character after the selection
   *
   *  At the beginning and end of each line there is assumed to be a frame with Bidi level equal to the
   *   paragraph embedding level. In these cases aPrevFrame and aNextFrame respectively will return nsnull.
   */
  NS_IMETHOD GetPrevNextBidiLevels(nsIPresContext *aPresContext, nsIContent *aNode, PRUint32 aContentOffset,
                                   nsIFrame **aPrevFrame, nsIFrame **aNextFrame, PRUint8 *aPrevLevel, PRUint8 *aNextLevel)=0;

  /** GetFrameFromLevel will scan in a given direction
   *   until it finds a frame with a Bidi level less than or equal to a given level.
   *   It will return the last frame before this.
   *  @param aPresContext is the context to use
   *  @param aFrameIn is the frame to start from
   *  @param aDirection is the direction to scan
   *  @param aBidiLevel is the level to search for
   *  @param aFrameOut will hold the frame returned
   */
  NS_IMETHOD GetFrameFromLevel(nsIPresContext *aPresContext, nsIFrame *aFrameIn, nsDirection aDirection, PRUint8 aBidiLevel,
                               nsIFrame **aFrameOut)=0;

#endif // IBMBIDI
};



#endif /* nsIFrameSelection_h___ */
