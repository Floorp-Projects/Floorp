/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Mike Pinkerton (pinkerton@netscape.com)
 *   Dean Tessman <dean_tessman@hotmail.com>
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

//
// nsMenuPopupFrame
//

#ifndef nsMenuPopupFrame_h__
#define nsMenuPopupFrame_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsMenuFrame.h"
#include "nsIDOMEventTarget.h"

#include "nsBoxFrame.h"
#include "nsMenuParent.h"
#include "nsIWidget.h"

#include "nsITimer.h"

// XUL popups can be in several different states. When opening a popup, the
// state changes as follows:
//   ePopupClosed - initial state
//   ePopupShowing - during the period when the popupshowing event fires
//   ePopupOpen - between the popupshowing event and being visible. Creation
//                of the child frames, layout and reflow occurs in this state.
//   ePopupOpenAndVisible - layout is done and AdjustView is called to make
//                          the popup's widget visible. The popup is now
//                          visible and the popupshown event fires.
// When closing a popup:
//   ePopupHidden - during the period when the popuphiding event fires and
//                  the popup is removed.
//   ePopupClosed - the popup's widget is made invisible.
enum nsPopupState {
  // state when a popup is not open
  ePopupClosed,
  // state from when a popup is requested to be shown to after the
  // popupshowing event has been fired.
  ePopupShowing,
  // state while a popup is open but the widget is not yet visible
  ePopupOpen,
  // state while a popup is open and visible on screen
  ePopupOpenAndVisible,
  // state from when a popup is requested to be hidden to when it is closed.
  ePopupHiding,
  // state which indicates that the popup was hidden without firing the
  // popuphiding or popuphidden events. It is used when executing a menu
  // command because the menu needs to be hidden before the command event
  // fires, yet the popuphiding and popuphidden events are fired after. This
  // state can also occur when the popup is removed because the document is
  // unloaded.
  ePopupInvisible
};

// values are selected so that the direction can be flipped just by
// changing the sign
#define POPUPALIGNMENT_NONE 0
#define POPUPALIGNMENT_TOPLEFT 1
#define POPUPALIGNMENT_TOPRIGHT -1
#define POPUPALIGNMENT_BOTTOMLEFT 2
#define POPUPALIGNMENT_BOTTOMRIGHT -2

#define INC_TYP_INTERVAL  1000  // 1s. If the interval between two keypresses is shorter than this, 
                                //   treat as a continue typing
// XXX, kyle.yuan@sun.com, there are 4 definitions for the same purpose:
//  nsMenuPopupFrame.h, nsListControlFrame.cpp, listbox.xml, tree.xml
//  need to find a good place to put them together.
//  if someone changes one, please also change the other.

nsIFrame* NS_NewMenuPopupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsIViewManager;
class nsIView;
class nsMenuPopupFrame;

class nsMenuPopupFrame : public nsBoxFrame, public nsMenuParent
{
public:
  nsMenuPopupFrame(nsIPresShell* aShell, nsStyleContext* aContext);

  // nsMenuParent interface
  virtual nsMenuFrame* GetCurrentMenuItem();
  NS_IMETHOD SetCurrentMenuItem(nsMenuFrame* aMenuItem);
  virtual void CurrentMenuIsBeingDestroyed();
  NS_IMETHOD ChangeMenuItem(nsMenuFrame* aMenuItem, PRBool aSelectFirstItem);

  // as popups are opened asynchronously, the popup pending state is used to
  // prevent multiple requests from attempting to open the same popup twice
  nsPopupState PopupState() { return mPopupState; }
  void SetPopupState(nsPopupState aPopupState) { mPopupState = aPopupState; }

  NS_IMETHOD SetActive(PRBool aActiveFlag) { return NS_OK; } // We don't care.
  virtual PRBool IsActive() { return PR_FALSE; }
  virtual PRBool IsMenuBar() { return PR_FALSE; }

  /*
   * When this popup is open, should clicks outside of it be consumed?
   * Return PR_TRUE if the popup should rollup on an outside click, 
   * but consume that click so it can't be used for anything else.
   * Return PR_FALSE to allow clicks outside the popup to activate content 
   * even when the popup is open.
   * ---------------------------------------------------------------------
   * 
   * Should clicks outside of a popup be eaten?
   *
   *       Menus     Autocomplete     Comboboxes
   * Mac     Eat           No              Eat
   * Win     No            No              Eat     
   * Unix    Eat           No              Eat
   *
   */
  PRBool ConsumeOutsideClicks();

  virtual PRBool IsContextMenu() { return mIsContextMenu; }

  virtual PRBool MenuClosed() { return PR_TRUE; }

  NS_IMETHOD GetWidget(nsIWidget **aWidget);

  // The dismissal listener gets created and attached to the window.
  void AttachedDismissalListener();

  // Overridden methods
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  virtual void Destroy();

  virtual void InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aX, nscoord aY, nsIFrame* aForChild,
                                  PRUint32 aFlags);

  // returns true if the popup is a panel with the noautohide attribute set to
  // true. These panels do not roll up automatically.
  PRBool IsNoAutoHide();

  // returns true if the popup is a top-most window. Otherwise, the
  // panel appears in front of the parent window.
  PRBool IsTopMost();

  void EnsureWidget();

  virtual nsresult CreateWidgetForView(nsIView* aView);

  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  virtual PRBool IsLeaf() const;

  // AdjustView should be called by the parent frame after the popup has been
  // laid out, so that the view can be shown.
  void AdjustView();

  nsIView* GetRootViewForPopup(nsIFrame* aStartFrame);

  // set the position of the popup either relative to the anchor aAnchorFrame
  // (or the frame for mAnchorContent if aAnchorFrame is null) or at a specific
  // point if a screen position (mScreenXPos and mScreenYPos) are set. The popup
  // will be adjusted so that it is on screen.
  nsresult SetPopupPosition(nsIFrame* aAnchorFrame);

  PRBool HasGeneratedChildren() { return mGeneratedChildren; }
  void SetGeneratedChildren() { mGeneratedChildren = PR_TRUE; }

  // called when the Enter key is pressed while the popup is open. This will
  // just pass the call down to the current menu, if any. If a current menu
  // should be opened as a result, this method should return the frame for
  // that menu, or null if no menu should be opened. Also, calling Enter will
  // reset the current incremental search string, calculated in
  // FindMenuWithShortcut.
  nsMenuFrame* Enter();

  nsPopupType PopupType() const { return mPopupType; }
  PRBool IsMenu() { return mPopupType == ePopupTypeMenu; }
  PRBool IsOpen() { return mPopupState == ePopupOpen || mPopupState == ePopupOpenAndVisible; }
  PRBool HasOpenChanged() { return mIsOpenChanged; }

  // returns true if the popup is in a content shell, or false for a popup in
  // a chrome shell
  PRBool IsInContentShell() { return mInContentShell; }

  // the Initialize methods are used to set the anchor position for
  // each way of opening a popup.
  void InitializePopup(nsIContent* aAnchorContent,
                       const nsAString& aPosition,
                       PRInt32 aXPos, PRInt32 aYPos,
                       PRBool aAttributesOverride);

  /**
   * @param aIsContextMenu if true, then the popup is
   * positioned at a slight offset from aXPos/aYPos to ensure the
   * (presumed) mouse position is not over the menu.
   */
  void InitializePopupAtScreen(PRInt32 aXPos, PRInt32 aYPos,
                               PRBool aIsContextMenu);

  void InitializePopupWithAnchorAlign(nsIContent* aAnchorContent,
                                      nsAString& aAnchor,
                                      nsAString& aAlign,
                                      PRInt32 aXPos, PRInt32 aYPos);

  // indicate that the popup should be opened
  PRBool ShowPopup(PRBool aIsContextMenu, PRBool aSelectFirstItem);
  // indicate that the popup should be hidden. The new state should either be
  // ePopupClosed or ePopupInvisible.
  void HidePopup(PRBool aDeselectMenu, nsPopupState aNewState);

  // locate and return the menu frame that should be activated for the
  // supplied key event. If doAction is set to true by this method,
  // then the menu's action should be carried out, as if the user had pressed
  // the Enter key. If doAction is false, the menu should just be highlighted.
  // This method also handles incremental searching in menus so the user can
  // type the first few letters of an item/s name to select it.
  nsMenuFrame* FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent, PRBool& doAction);

  void ClearIncrementalString() { mIncrementalString.Truncate(); }

  virtual nsIAtom* GetType() const { return nsGkAtoms::menuPopupFrame; }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
      return MakeFrameName(NS_LITERAL_STRING("MenuPopup"), aResult);
  }
#endif

  void EnsureMenuItemIsVisible(nsMenuFrame* aMenuFrame);

  // This sets 'left' and 'top' attributes.
  // May kill the frame.
  void MoveTo(PRInt32 aLeft, PRInt32 aTop);

  PRBool GetAutoPosition();
  void SetAutoPosition(PRBool aShouldAutoPosition);
  void SetConsumeRollupEvent(PRUint32 aConsumeMode);

  nsIScrollableView* GetScrollableView(nsIFrame* aStart);

  // same as SetBounds except the preferred size mPrefSize is also set.
  void SetPreferredBounds(nsBoxLayoutState& aState, const nsRect& aRect);

  // retrieve the last preferred size
  nsSize PreferredSize() { return mPrefSize; }
  // set the last preferred size
  void SetPreferredSize(nsSize aSize) { mPrefSize = aSize; }

protected:
  // Move without updating attributes.                                          
  void MoveToInternal(PRInt32 aLeft, PRInt32 aTop);                             

  // redefine to tell the box system not to move the views.
  virtual void GetLayoutFlags(PRUint32& aFlags);

  void InitPositionFromAnchorAlign(const nsAString& aAnchor,
                                   const nsAString& aAlign);

  // return the position where the popup should be, when it should be
  // anchored at anchorRect. aHFlip and aVFlip will be set if the popup may be
  // flipped in that direction if there is not enough space available.
  nsPoint AdjustPositionForAnchorAlign(const nsRect& anchorRect, PRBool& aHFlip, PRBool& aVFlip);


  // check if the popup will fit into the available space and resize it. This
  // method handles only one axis at a time so is called twice, once for
  // horizontal and once for vertical. All arguments are specified for this
  // one axis. All coordinates are in app units relative to the screen.
  //   aScreenPoint - the point where the popup should appear
  //   aSize - the size of the popup
  //   aScreenBegin - the left or top edge of the screen
  //   aScreenEnd - the right or bottom edge of the screen
  //   aAnchorBegin - the left or top edge of the anchor rectangle
  //   aAnchorEnd - the right or bottom edge of the anchor rectangle
  //   aMarginBegin - the left or top margin of the popup
  //   aMarginEnd - the right or bottom margin of the popup
  //   aOffsetForContextMenu - the additional offset to add for context menus
  //   aFlip - whether to flip or resize the popup when there isn't space
  nscoord FlipOrResize(nscoord& aScreenPoint, nscoord aSize, 
                       nscoord aScreenBegin, nscoord aScreenEnd,
                       nscoord aAnchorBegin, nscoord aAnchorEnd,
                       nscoord aMarginBegin, nscoord aMarginEnd,
                       nscoord aOffsetForContextMenu, PRBool aFlip);

  // Move the popup to the position specified in its |left| and |top| attributes.
  void MoveToAttributePosition();

  // the content that the popup is anchored to, if any, which may be in a
  // different document than the popup.
  nsCOMPtr<nsIContent> mAnchorContent;

  nsMenuFrame* mCurrentMenu; // The current menu that is active.

  // popup alignment relative to the anchor node
  PRInt8 mPopupAlignment;
  PRInt8 mPopupAnchor;

  // the position of the popup. The screen coordinates, if set to values other
  // than -1, override mXPos and mYPos.
  PRInt32 mXPos;
  PRInt32 mYPos;
  PRInt32 mScreenXPos;
  PRInt32 mScreenYPos;

  nsPopupType mPopupType; // type of popup
  nsPopupState mPopupState; // open state of the popup

  PRPackedBool mIsOpenChanged; // true if the open state changed since the last layout
  PRPackedBool mIsContextMenu; // true for context menus
  // true if we need to offset the popup to ensure it's not under the mouse
  PRPackedBool mAdjustOffsetForContextMenu;
  PRPackedBool mGeneratedChildren; // true if the contents have been created

  PRPackedBool mMenuCanOverlapOSBar;    // can we appear over the taskbar/menubar?
  PRPackedBool mShouldAutoPosition; // Should SetPopupPosition be allowed to auto position popup?
  PRPackedBool mConsumeRollupEvent; // Should the rollup event be consumed?
  PRPackedBool mInContentShell; // True if the popup is in a content shell

  nsString     mIncrementalString;  // for incremental typing navigation

  // A popup's preferred size may be different than its actual size stored in
  // mRect in the case where the popup was resized because it was too large
  // for the screen. The preferred size mPrefSize holds the full size the popup
  // would be before resizing. Computations are performed using this size.
  // The parent frame is responsible for setting the preferred size using
  // SetPreferredBounds or SetPreferredSize before positioning the popup with
  // SetPopupPosition.
  nsSize mPrefSize;

  static PRInt8 sDefaultLevelParent;
}; // class nsMenuPopupFrame

#endif
