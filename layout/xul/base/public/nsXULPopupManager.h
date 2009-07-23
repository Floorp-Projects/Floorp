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
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2006
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

/**
 * The XUL Popup Manager keeps track of all open popups.
 */

#ifndef nsXULPopupManager_h__
#define nsXULPopupManager_h__

#include "prlog.h"
#include "nsGUIEvent.h"
#include "nsIContent.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsIDOMKeyListener.h"
#include "nsPoint.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "nsStyleConsts.h"

/**
 * There are two types that are used:
 *   - dismissable popups such as menus, which should close up when there is a
 *     click outside the popup. In this situation, the entire chain of menus
 *     above should also be closed.
 *   - panels, which stay open until a request is made to close them. This
 *     type is used by tooltips.
 *
 * When a new popup is opened, it is appended to the popup chain, stored in a
 * linked list in mPopups for dismissable menus and panels or mNoHidePanels
 * for tooltips and panels with noautohide="true".
 * Popups are stored in this list linked from newest to oldest. When a click
 * occurs outside one of the open dismissable popups, the chain is closed by
 * calling Rollup.
 */

class nsIPresShell;
class nsMenuFrame;
class nsMenuPopupFrame;
class nsMenuBarFrame;
class nsMenuParent;
class nsIDOMKeyEvent;
class nsIDocShellTreeItem;

// when a menu command is executed, the closemenu attribute may be used
// to define how the menu should be closed up
enum CloseMenuMode {
  CloseMenuMode_Auto, // close up the chain of menus, default value
  CloseMenuMode_None, // don't close up any menus
  CloseMenuMode_Single // close up only the menu the command is inside
};

/**
 * nsNavigationDirection: an enum expressing navigation through the menus in
 * terms which are independent of the directionality of the chrome. The
 * terminology, derived from XSL-FO and CSS3 (e.g. 
 * http://www.w3.org/TR/css3-text/#TextLayout), is BASE (Before, After, Start,
 * End), with the addition of First and Last (mapped to Home and End
 * respectively).
 *
 * In languages such as English where the inline progression is left-to-right
 * and the block progression is top-to-bottom (lr-tb), these terms will map out
 * as in the following diagram
 *
 *  --- inline progression --->
 *
 *           First              |
 *           ...                |
 *           Before             |
 *         +--------+         block
 *   Start |        | End  progression
 *         +--------+           |
 *           After              |
 *           ...                |
 *           Last               V
 * 
 */

enum nsNavigationDirection {
  eNavigationDirection_Last,
  eNavigationDirection_First,
  eNavigationDirection_Start,
  eNavigationDirection_Before,
  eNavigationDirection_End,
  eNavigationDirection_After
};

#define NS_DIRECTION_IS_INLINE(dir) (dir == eNavigationDirection_Start ||     \
                                     dir == eNavigationDirection_End)
#define NS_DIRECTION_IS_BLOCK(dir) (dir == eNavigationDirection_Before || \
                                    dir == eNavigationDirection_After)
#define NS_DIRECTION_IS_BLOCK_TO_EDGE(dir) (dir == eNavigationDirection_First ||    \
                                            dir == eNavigationDirection_Last)

PR_STATIC_ASSERT(NS_STYLE_DIRECTION_LTR == 0 && NS_STYLE_DIRECTION_RTL == 1);
PR_STATIC_ASSERT((NS_VK_HOME == NS_VK_END + 1) &&
                 (NS_VK_LEFT == NS_VK_END + 2) &&
                 (NS_VK_UP == NS_VK_END + 3) &&
                 (NS_VK_RIGHT == NS_VK_END + 4) &&
                 (NS_VK_DOWN == NS_VK_END + 5));

/**
 * DirectionFromKeyCodeTable: two arrays, the first for left-to-right and the
 * other for right-to-left, that map keycodes to values of
 * nsNavigationDirection.
 */
extern const nsNavigationDirection DirectionFromKeyCodeTable[2][6];

#define NS_DIRECTION_FROM_KEY_CODE(frame, keycode)                     \
  (DirectionFromKeyCodeTable[frame->GetStyleVisibility()->mDirection]  \
                            [keycode - NS_VK_END])

// nsMenuChainItem holds info about an open popup. Items are stored in a
// doubly linked list. Note that the linked list is stored beginning from
// the lowest child in a chain of menus, as this is the active submenu.
class nsMenuChainItem
{
private:
  nsMenuPopupFrame* mFrame; // the popup frame
  nsPopupType mPopupType; // the popup type of the frame
  PRPackedBool mIsContext; // true for context menus
  PRPackedBool mOnMenuBar; // true if the menu is on a menu bar
  PRPackedBool mIgnoreKeys; // true if keyboard listeners should not be used

  nsMenuChainItem* mParent;
  nsMenuChainItem* mChild;

public:
  nsMenuChainItem(nsMenuPopupFrame* aFrame, PRBool aIsContext, nsPopupType aPopupType)
    : mFrame(aFrame),
      mPopupType(aPopupType),
      mIsContext(aIsContext),
      mOnMenuBar(PR_FALSE),
      mIgnoreKeys(PR_FALSE),
      mParent(nsnull),
      mChild(nsnull)
  {
    NS_ASSERTION(aFrame, "null frame passed to nsMenuChainItem constructor");
    MOZ_COUNT_CTOR(nsMenuChainItem);
  }

  ~nsMenuChainItem()
  {
    MOZ_COUNT_DTOR(nsMenuChainItem);
  }

  nsIContent* Content();
  nsMenuPopupFrame* Frame() { return mFrame; }
  nsPopupType PopupType() { return mPopupType; }
  PRBool IsMenu() { return mPopupType == ePopupTypeMenu; }
  PRBool IsContextMenu() { return mIsContext; }
  PRBool IgnoreKeys() { return mIgnoreKeys; }
  PRBool IsOnMenuBar() { return mOnMenuBar; }
  void SetIgnoreKeys(PRBool aIgnoreKeys) { mIgnoreKeys = aIgnoreKeys; }
  void SetOnMenuBar(PRBool aOnMenuBar) { mOnMenuBar = aOnMenuBar; }
  nsMenuChainItem* GetParent() { return mParent; }
  nsMenuChainItem* GetChild() { return mChild; }

  // set the parent of this item to aParent, also changing the parent
  // to have this as a child.
  void SetParent(nsMenuChainItem* aParent);

  // removes an item from the chain. The root pointer must be supplied in case
  // the item is the first item in the chain in which case the pointer will be
  // set to the next item, or null if there isn't another item. After detaching,
  // this item will not have a parent or a child.
  void Detach(nsMenuChainItem** aRoot);
};

// this class is used for dispatching popupshowing events asynchronously.
class nsXULPopupShowingEvent : public nsRunnable
{
public:
  nsXULPopupShowingEvent(nsIContent *aPopup,
                         nsIContent *aMenu,
                         nsPopupType aPopupType,
                         PRBool aIsContextMenu,
                         PRBool aSelectFirstItem)
    : mPopup(aPopup),
      mMenu(aMenu),
      mPopupType(aPopupType),
      mIsContextMenu(aIsContextMenu),
      mSelectFirstItem(aSelectFirstItem)
  {
    NS_ASSERTION(aPopup, "null popup supplied to nsXULPopupShowingEvent constructor");
    NS_ASSERTION(aMenu, "null menu supplied to nsXULPopupShowingEvent constructor");
  }

  NS_IMETHOD Run();

private:
  nsCOMPtr<nsIContent> mPopup;
  nsCOMPtr<nsIContent> mMenu;
  nsPopupType mPopupType;
  PRBool mIsContextMenu;
  PRBool mSelectFirstItem;
};

// this class is used for dispatching popuphiding events asynchronously.
class nsXULPopupHidingEvent : public nsRunnable
{
public:
  nsXULPopupHidingEvent(nsIContent *aPopup,
                        nsIContent* aNextPopup,
                        nsIContent* aLastPopup,
                        nsPopupType aPopupType,
                        PRBool aDeselectMenu)
    : mPopup(aPopup),
      mNextPopup(aNextPopup),
      mLastPopup(aLastPopup),
      mPopupType(aPopupType),
      mDeselectMenu(aDeselectMenu)
  {
    NS_ASSERTION(aPopup, "null popup supplied to nsXULPopupHidingEvent constructor");
    // aNextPopup and aLastPopup may be null
  }

  NS_IMETHOD Run();

private:
  nsCOMPtr<nsIContent> mPopup;
  nsCOMPtr<nsIContent> mNextPopup;
  nsCOMPtr<nsIContent> mLastPopup;
  nsPopupType mPopupType;
  PRBool mDeselectMenu;
};

// this class is used for dispatching menu command events asynchronously.
class nsXULMenuCommandEvent : public nsRunnable
{
public:
  nsXULMenuCommandEvent(nsIContent *aMenu,
                        PRBool aIsTrusted,
                        PRBool aShift,
                        PRBool aControl,
                        PRBool aAlt,
                        PRBool aMeta,
                        PRBool aUserInput,
                        CloseMenuMode aCloseMenuMode)
    : mMenu(aMenu),
      mIsTrusted(aIsTrusted),
      mShift(aShift),
      mControl(aControl),
      mAlt(aAlt),
      mMeta(aMeta),
      mUserInput(aUserInput),
      mCloseMenuMode(aCloseMenuMode)
  {
    NS_ASSERTION(aMenu, "null menu supplied to nsXULMenuCommandEvent constructor");
  }

  NS_IMETHOD Run();

private:
  nsCOMPtr<nsIContent> mMenu;
  PRBool mIsTrusted;
  PRBool mShift;
  PRBool mControl;
  PRBool mAlt;
  PRBool mMeta;
  PRBool mUserInput;
  CloseMenuMode mCloseMenuMode;
};

class nsXULPopupManager : public nsIDOMKeyListener,
                          public nsIMenuRollup,
                          public nsIRollupListener,
                          public nsITimerCallback
{

public:
  friend class nsXULPopupShowingEvent;
  friend class nsXULPopupHidingEvent;
  friend class nsXULMenuCommandEvent;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIROLLUPLISTENER
  NS_DECL_NSITIMERCALLBACK

  virtual PRUint32 GetSubmenuWidgetChain(nsTArray<nsIWidget*> *aWidgetChain);
  virtual void AdjustPopupsOnWindowChange(void);

  static nsXULPopupManager* sInstance;

  // initialize and shutdown methods called by nsLayoutStatics
  static nsresult Init();
  static void Shutdown();

  // returns a weak reference to the popup manager instance, could return null
  // if a popup manager could not be allocated
  static nsXULPopupManager* GetInstance();

  // get the frame for a content node aContent if the frame's type
  // matches aFrameType. Otherwise, return null. If aShouldFlush is true,
  // then the frames are flushed before retrieving the frame.
  nsIFrame* GetFrameOfTypeForContent(nsIContent* aContent,
                                     nsIAtom* aFrameType,
                                     PRBool aShouldFlush);

  // given a menu frame, find the prevous or next menu frame. If aPopup is
  // true then navigate a menupopup, from one item on the menu to the previous
  // or next one. This is used for cursor navigation between items in a popup
  // menu. If aIsPopup is false, the navigation is on a menubar, so navigate
  // between menus on the menubar. This is used for left/right cursor navigation.
  //
  // Items that are not valid, such as non-menu or non-menuitem elements are
  // skipped, and the next or previous item after that is checked.
  //
  // If aStart is null, the first valid item is retrieved by GetNextMenuItem
  // and the last valid item is retrieved by GetPreviousMenuItem.
  //
  // Both methods will loop around the beginning or end if needed.
  //
  // aParent - the parent menubar or menupopup
  // aStart - the menu/menuitem to start navigation from. GetPreviousMenuItem
  //          returns the item before it, while GetNextMenuItem returns the
  //          item after it.
  // aIsPopup - true for menupopups, false for menubars
  static nsMenuFrame* GetPreviousMenuItem(nsIFrame* aParent,
                                          nsMenuFrame* aStart,
                                          PRBool aIsPopup);
  static nsMenuFrame* GetNextMenuItem(nsIFrame* aParent,
                                      nsMenuFrame* aStart,
                                      PRBool aIsPopup);

  // returns true if the menu item aContent is a valid menuitem which may
  // be navigated to. aIsPopup should be true for items on a popup, or false
  // for items on a menubar.
  static PRBool IsValidMenuItem(nsPresContext* aPresContext,
                                nsIContent* aContent,
                                PRBool aOnPopup);

  // inform the popup manager that a menu bar has been activated or deactivated,
  // either because one of its menus has opened or closed, or that the menubar
  // has been focused such that its menus may be navigated with the keyboard.
  // aActivate should be true when the menubar should be focused, and false
  // when the active menu bar should be defocused. In the latter case, if
  // aMenuBar isn't currently active, yet another menu bar is, that menu bar
  // will remain active.
  void SetActiveMenuBar(nsMenuBarFrame* aMenuBar, PRBool aActivate);

  // retrieve the node and offset of the last mouse event used to open a
  // context menu. This information is determined from the rangeParent and
  // the rangeOffset of the event supplied to ShowPopup or ShowPopupAtScreen.
  // This is used by the implementation of nsIDOMXULDocument::GetPopupRangeParent
  // and nsIDOMXULDocument::GetPopupRangeOffset.
  void GetMouseLocation(nsIDOMNode** aNode, PRInt32* aOffset);

  /**
   * Open a <menu> given its content node. If aSelectFirstItem is
   * set to true, the first item on the menu will automatically be
   * selected. If aAsynchronous is true, the event will be dispatched
   * asynchronously. This should be true when called from frame code.
   */
  void ShowMenu(nsIContent *aMenu, PRBool aSelectFirstItem, PRBool aAsynchronous);

  /**
   * Open a popup, either anchored or unanchored. If aSelectFirstItem is
   * true, then the first item in the menu is selected. The arguments are
   * similar to those for nsIPopupBoxObject::OpenPopup.
   *
   * aTriggerEvent should be the event that triggered the event. This is used
   * to determine the coordinates for the popupshowing event. This may be null
   * if the popup was not triggered by an event, or the coordinates are not
   * important. Note that this may be reworked in bug 383930.
   *
   * This fires the popupshowing event synchronously.
   */
  void ShowPopup(nsIContent* aPopup,
                 nsIContent* aAnchorContent,
                 const nsAString& aPosition,
                 PRInt32 aXPos, PRInt32 aYPos,
                 PRBool aIsContextMenu,
                 PRBool aAttributesOverride,
                 PRBool aSelectFirstItem,
                 nsIDOMEvent* aTriggerEvent);

  /**
   * Open a popup at a specific screen position specified by aXPos and aYPos,
   * measured in CSS pixels.
   *
   * This fires the popupshowing event synchronously.
   * 
   * If aIsContextMenu is true, the popup is positioned at a slight
   * offset from aXPos/aYPos to ensure that it is not under the mouse
   * cursor.
   */
  void ShowPopupAtScreen(nsIContent* aPopup,
                         PRInt32 aXPos, PRInt32 aYPos,
                         PRBool aIsContextMenu,
                         nsIDOMEvent* aTriggerEvent);

  /**
   * This method is provided only for compatibility with an older popup API.
   * New code should not call this function and should call ShowPopup instead.
   *
   * This fires the popupshowing event synchronously.
   */
  void ShowPopupWithAnchorAlign(nsIContent* aPopup,
                                nsIContent* aAnchorContent,
                                nsAString& aAnchor,
                                nsAString& aAlign,
                                PRInt32 aXPos, PRInt32 aYPos,
                                PRBool aIsContextMenu);

  /*
   * Hide a popup aPopup. If the popup is in a <menu>, then also inform the
   * menu that the popup is being hidden.
   *
   * aHideChain - true if the entire chain of menus should be closed. If false,
   *              only this popup is closed.
   * aDeselectMenu - true if the parent <menu> of the popup should be deselected.
   *                 This will be false when the menu is closed by pressing the
   *                 Escape key.
   * aAsynchronous - true if the first popuphiding event should be sent
   *                 asynchrously. This should be true if HidePopup is called
   *                 from a frame.
   * aLastPopup - optional popup to close last when hiding a chain of menus.
   *              If null, then all popups will be closed.
   */
  void HidePopup(nsIContent* aPopup,
                 PRBool aHideChain,
                 PRBool aDeselectMenu,
                 PRBool aAsynchronous,
                 nsIContent* aLastPopup = nsnull);

  /**
   * Hide a popup after a short delay. This is used when rolling over menu items.
   * This timer is stored in mCloseTimer. The timer may be cancelled and the popup
   * closed by calling KillMenuTimer.
   */
  void HidePopupAfterDelay(nsMenuPopupFrame* aPopup);

  /**
   * Hide all of the popups from a given docshell. This should be called when the
   * document is hidden.
   */
  void HidePopupsInDocShell(nsIDocShellTreeItem* aDocShellToHide);

  /**
   * Execute a menu command from the triggering event aEvent.
   *
   * aMenu - a menuitem to execute
   * aEvent - the mouse event which triggered the menu to be executed,
   *          may be null
   */
  void ExecuteMenu(nsIContent* aMenu, nsEvent* aEvent);

  /**
   * Return true if the popup for the supplied content node is open.
   */
  PRBool IsPopupOpen(nsIContent* aPopup);

  /**
   * Return true if the popup for the supplied menu parent is open.
   */
  PRBool IsPopupOpenForMenuParent(nsMenuParent* aMenuParent);

  /**
   * Return the frame for the topmost open popup of a given type, or null if
   * no popup of that type is open. If aType is ePopupTypeAny, a menu of any
   * type is returned, except for popups in the mNoHidePanels list.
   */
  nsIFrame* GetTopPopup(nsPopupType aType);

  /**
   * Return an array of all the open and visible popup frames for
   * menus, in order from top to bottom.
   */
  nsTArray<nsIFrame *> GetVisiblePopups();

  /**
   * Return false if a popup may not be opened. This will return false if the
   * popup is already open, if the popup is in a content shell that is not
   * focused, or if it is a submenu of another menu that isn't open.
   */
  PRBool MayShowPopup(nsMenuPopupFrame* aFrame);

  /**
   * Called when a popup frame is destroyed. In this case, just remove the
   * item and later popups from the list. No point going through HidePopup as
   * the frames have gone away.
   */
  void PopupDestroyed(nsMenuPopupFrame* aFrame);

  /**
   * Returns true if there is a context menu open. If aPopup is specified,
   * then the context menu must be later in the chain than aPopup. If aPopup
   * is null, returns true if any context menu at all is open.
   */
  PRBool HasContextMenu(nsMenuPopupFrame* aPopup);

  /**
   * Update the commands for the menus within the menu popup for a given
   * content node. aPopup should be a XUL menupopup element. This method
   * changes attributes on the children of aPopup, and deals only with the
   * content of the popup, not the frames.
   */
  void UpdateMenuItems(nsIContent* aPopup);

  /**
   * Stop the timer which hides a popup after a delay, started by a previous
   * call to HidePopupAfterDelay. In addition, the popup awaiting to be hidden
   * is closed asynchronously.
   */
  void KillMenuTimer();

  /**
   * Cancel the timer which closes menus after delay, but only if the menu to
   * close is aMenuParent. When a submenu is opened, the user might move the
   * mouse over a sibling menuitem which would normally close the menu. This
   * menu is closed via a timer. However, if the user moves the mouse over the
   * submenu before the timer fires, we should instead cancel the timer. This
   * ensures that the user can move the mouse diagonally over a menu.
   */
  void CancelMenuTimer(nsMenuParent* aMenuParent);

  /**
   * Handles navigation for menu accelkeys. Returns true if the key has
   * been handled. If aFrame is specified, then the key is handled by that
   * popup, otherwise if aFrame is null, the key is handled by the active
   * popup or menubar.
   */
  PRBool HandleShortcutNavigation(nsIDOMKeyEvent* aKeyEvent,
                                  nsMenuPopupFrame* aFrame);

  /**
   * Handles cursor navigation within a menu. Returns true if the key has
   * been handled.
   */
  PRBool HandleKeyboardNavigation(PRUint32 aKeyCode);

  /**
   * Handle keyboard navigation within a menu popup specified by aFrame.
   * Returns true if the key was handled and other default handling
   * should not occur.
   */
  PRBool HandleKeyboardNavigationInPopup(nsMenuPopupFrame* aFrame,
                                         nsNavigationDirection aDir)
  {
    return HandleKeyboardNavigationInPopup(nsnull, aFrame, aDir);
  }

  NS_IMETHODIMP HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);

protected:
  nsXULPopupManager();
  ~nsXULPopupManager();

  // get the nsMenuFrame, if any, for the given content node
  nsMenuFrame* GetMenuFrameForContent(nsIContent* aContent);

  // get the nsMenuPopupFrame, if any, for the given content node
  nsMenuPopupFrame* GetPopupFrameForContent(nsIContent* aContent);

  // return the topmost menu, skipping over invisible popups
  nsMenuChainItem* GetTopVisibleMenu();

  // Hide all of the visible popups from the given list. aDeselectMenu
  // indicates whether to deselect the menu of popups when hiding; this
  // flag is passed as the first argument to HidePopup. This function
  // can cause style changes and frame destruction.
  void HidePopupsInList(const nsTArray<nsMenuPopupFrame *> &aFrames,
                        PRBool aDeselectMenu);

  // set the event that was used to trigger the popup, or null to
  // clear the event details.
  void SetTriggerEvent(nsIDOMEvent* aEvent, nsIContent* aPopup);

  // callbacks for ShowPopup and HidePopup as events may be done asynchronously
  void ShowPopupCallback(nsIContent* aPopup,
                         nsMenuPopupFrame* aPopupFrame,
                         PRBool aIsContextMenu,
                         PRBool aSelectFirstItem);
  void HidePopupCallback(nsIContent* aPopup,
                         nsMenuPopupFrame* aPopupFrame,
                         nsIContent* aNextPopup,
                         nsIContent* aLastPopup,
                         nsPopupType aPopupType,
                         PRBool aDeselectMenu);

  /**
   * Fire a popupshowing event on the popup aPopup and then open the popup.
   *
   * The caller must keep a strong reference to aPopup.
   *
   * aPopup - the popup node to open
   * aMenu - should be set to the parent menu if this is a popup associated
   *         with a menu. Otherwise, should be null.
   * aPresContext - the prescontext 
   * aPopupType - the popup frame's PopupType
   * aIsContextMenu - true for context menus
   * aSelectFirstItem - true to select the first item in the menu
   */
  void FirePopupShowingEvent(nsIContent* aPopup,
                             nsIContent* aMenu,
                             nsPresContext* aPresContext,
                             nsPopupType aPopupType,
                             PRBool aIsContextMenu,
                             PRBool aSelectFirstItem);

  /**
   * Fire a popuphiding event and then hide the popup. This will be called
   * recursively if aNextPopup and aLastPopup are set in order to hide a chain
   * of open menus. If these are not set, only one popup is closed. However,
   * if the popup type indicates a menu, yet the next popup is not a menu,
   * then this ends the closing of popups. This allows a menulist inside a
   * non-menu to close up the menu but not close up the panel it is contained
   * within.
   *
   * The caller must keep a strong reference to aPopup, aNextPopup and aLastPopup.
   *
   * aPopup - the popup to hide
   * aNextPopup - the next popup to hide
   * aLastPopup - the last popup in the chain to hide
   * aPresContext - nsPresContext for the popup's frame
   * aPopupType - the PopupType of the frame. 
   * aDeselectMenu - true to unhighlight the menu when hiding it
   */
  void FirePopupHidingEvent(nsIContent* aPopup,
                            nsIContent* aNextPopup,
                            nsIContent* aLastPopup,
                            nsPresContext *aPresContext,
                            nsPopupType aPopupType,
                            PRBool aDeselectMenu);

  /**
   * Handle keyboard navigation within a menu popup specified by aItem.
   */
  PRBool HandleKeyboardNavigationInPopup(nsMenuChainItem* aItem,
                                         nsNavigationDirection aDir)
  {
    return HandleKeyboardNavigationInPopup(aItem, aItem->Frame(), aDir);
  }

private:
  /**
   * Handle keyboard navigation within a menu popup aFrame. If aItem is
   * supplied, then it is expected to have a frame equal to aFrame.
   * If aItem is non-null, then the navigation may be redirected to
   * an open submenu if one exists. Returns true if the key was
   * handled and other default handling should not occur.
   */
  PRBool HandleKeyboardNavigationInPopup(nsMenuChainItem* aItem,
                                         nsMenuPopupFrame* aFrame,
                                         nsNavigationDirection aDir);

protected:

  /**
   * Set mouse capturing for the current popup. This traps mouse clicks that
   * occur outside the popup so that it can be closed up. aOldPopup should be
   * set to the popup that was previously the current popup.
   */
  void SetCaptureState(nsIContent *aOldPopup);

  /**
   * Key event listeners are attached to the document containing the current
   * menu for menu and shortcut navigation. Only one listener is needed at a
   * time, stored in mKeyListener, so switch it only if the document changes.
   * Having menus in different documents is very rare, so the listeners will
   * usually only be attached when the first menu opens and removed when all
   * menus have closed.
   *
   * This is also used when only a menubar is active without any open menus,
   * so that keyboard navigation between menus on the menubar may be done.
   */
  void UpdateKeyboardListeners();

  /*
   * Returns true if the docshell for aDoc is aExpected or a child of aExpected.
   */
  PRBool IsChildOfDocShell(nsIDocument* aDoc, nsIDocShellTreeItem* aExpected);

  // the document the key event listener is attached to
  nsCOMPtr<nsIDOMEventTarget> mKeyListener;

  // widget that is currently listening to rollup events
  nsCOMPtr<nsIWidget> mWidget;

  // range parent and offset set in SetTriggerEvent
  nsCOMPtr<nsIDOMNode> mRangeParent;
  PRInt32 mRangeOffset;
  // Device pixels relative to the showing popup's presshell's
  // GetViewManager()->GetRootWidget().
  nsIntPoint mCachedMousePoint;

  // set to the currently active menu bar, if any
  nsMenuBarFrame* mActiveMenuBar;

  // linked list of normal menus and panels.
  nsMenuChainItem* mPopups;

  // linked list of noautohide panels and tooltips.
  nsMenuChainItem* mNoHidePanels;

  // timer used for HidePopupAfterDelay
  nsCOMPtr<nsITimer> mCloseTimer;

  // a popup that is waiting on the timer
  nsMenuPopupFrame* mTimerMenu;
};

nsresult
NS_NewXULPopupManager(nsISupports** aResult);

#endif
