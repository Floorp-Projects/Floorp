/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The XUL Popup Manager keeps track of all open popups.
 */

#ifndef nsXULPopupManager_h__
#define nsXULPopupManager_h__

#include "mozilla/Logging.h"
#include "nsIContent.h"
#include "nsIRollupListener.h"
#include "nsIDOMEventListener.h"
#include "nsPoint.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsIReflowCallback.h"
#include "nsThreadUtils.h"
#include "nsPresContext.h"
#include "nsStyleConsts.h"
#include "nsWidgetInitData.h"
#include "mozilla/Attributes.h"
#include "Units.h"

// X.h defines KeyPress
#ifdef KeyPress
#  undef KeyPress
#endif

/**
 * There are two types that are used:
 *   - dismissable popups such as menus, which should close up when there is a
 *     click outside the popup. In this situation, the entire chain of menus
 *     above should also be closed.
 *   - panels, which stay open until a request is made to close them. This
 *     type is used by tooltips.
 *
 * When a new popup is opened, it is appended to the popup chain, stored in a
 * linked list in mPopups.
 * Popups are stored in this list linked from newest to oldest. When a click
 * occurs outside one of the open dismissable popups, the chain is closed by
 * calling Rollup.
 */

class nsContainerFrame;
class nsMenuFrame;
class nsMenuPopupFrame;
class nsMenuBarFrame;
class nsMenuParent;
class nsIDocShellTreeItem;
class nsPIDOMWindowOuter;
class nsRefreshDriver;

namespace mozilla {
class PresShell;
namespace dom {
class Event;
class KeyboardEvent;
}  // namespace dom
}  // namespace mozilla

// when a menu command is executed, the closemenu attribute may be used
// to define how the menu should be closed up
enum CloseMenuMode {
  CloseMenuMode_Auto,   // close up the chain of menus, default value
  CloseMenuMode_None,   // don't close up any menus
  CloseMenuMode_Single  // close up only the menu the command is inside
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

enum nsIgnoreKeys {
  eIgnoreKeys_False,
  eIgnoreKeys_True,
  eIgnoreKeys_Shortcuts,
};

#define NS_DIRECTION_IS_INLINE(dir) \
  (dir == eNavigationDirection_Start || dir == eNavigationDirection_End)
#define NS_DIRECTION_IS_BLOCK(dir) \
  (dir == eNavigationDirection_Before || dir == eNavigationDirection_After)
#define NS_DIRECTION_IS_BLOCK_TO_EDGE(dir) \
  (dir == eNavigationDirection_First || dir == eNavigationDirection_Last)

static_assert(NS_STYLE_DIRECTION_LTR == 0 && NS_STYLE_DIRECTION_RTL == 1,
              "Left to Right should be 0 and Right to Left should be 1");

/**
 * DirectionFromKeyCodeTable: two arrays, the first for left-to-right and the
 * other for right-to-left, that map keycodes to values of
 * nsNavigationDirection.
 */
extern const nsNavigationDirection DirectionFromKeyCodeTable[2][6];

#define NS_DIRECTION_FROM_KEY_CODE(frame, keycode)                 \
  (DirectionFromKeyCodeTable[frame->StyleVisibility()->mDirection] \
                            [keycode -                             \
                             mozilla::dom::KeyboardEvent_Binding::DOM_VK_END])

// nsMenuChainItem holds info about an open popup. Items are stored in a
// doubly linked list. Note that the linked list is stored beginning from
// the lowest child in a chain of menus, as this is the active submenu.
class nsMenuChainItem {
 private:
  nsMenuPopupFrame* mFrame;  // the popup frame
  nsPopupType mPopupType;    // the popup type of the frame
  bool mNoAutoHide;          // true for noautohide panels
  bool mIsContext;           // true for context menus
  bool mOnMenuBar;           // true if the menu is on a menu bar
  nsIgnoreKeys mIgnoreKeys;  // indicates how keyboard listeners should be used

  // True if the popup should maintain its position relative to the anchor when
  // the anchor moves.
  bool mFollowAnchor;

  // The last seen position of the anchor, relative to the screen.
  nsRect mCurrentRect;

  nsMenuChainItem* mParent;
  nsMenuChainItem* mChild;

 public:
  nsMenuChainItem(nsMenuPopupFrame* aFrame, bool aNoAutoHide, bool aIsContext,
                  nsPopupType aPopupType)
      : mFrame(aFrame),
        mPopupType(aPopupType),
        mNoAutoHide(aNoAutoHide),
        mIsContext(aIsContext),
        mOnMenuBar(false),
        mIgnoreKeys(eIgnoreKeys_False),
        mFollowAnchor(false),
        mParent(nullptr),
        mChild(nullptr) {
    NS_ASSERTION(aFrame, "null frame passed to nsMenuChainItem constructor");
    MOZ_COUNT_CTOR(nsMenuChainItem);
  }

  ~nsMenuChainItem() { MOZ_COUNT_DTOR(nsMenuChainItem); }

  nsIContent* Content();
  nsMenuPopupFrame* Frame() { return mFrame; }
  nsPopupType PopupType() { return mPopupType; }
  bool IsNoAutoHide() { return mNoAutoHide; }
  void SetNoAutoHide(bool aNoAutoHide) { mNoAutoHide = aNoAutoHide; }
  bool IsMenu() { return mPopupType == ePopupTypeMenu; }
  bool IsContextMenu() { return mIsContext; }
  nsIgnoreKeys IgnoreKeys() { return mIgnoreKeys; }
  void SetIgnoreKeys(nsIgnoreKeys aIgnoreKeys) { mIgnoreKeys = aIgnoreKeys; }
  bool IsOnMenuBar() { return mOnMenuBar; }
  void SetOnMenuBar(bool aOnMenuBar) { mOnMenuBar = aOnMenuBar; }
  nsMenuChainItem* GetParent() { return mParent; }
  nsMenuChainItem* GetChild() { return mChild; }
  bool FollowsAnchor() { return mFollowAnchor; }
  void UpdateFollowAnchor();
  void CheckForAnchorChange();

  // set the parent of this item to aParent, also changing the parent
  // to have this as a child.
  void SetParent(nsMenuChainItem* aParent);

  // Removes an item from the chain. The root pointer must be supplied in case
  // the item is the first item in the chain in which case the pointer will be
  // set to the next item, or null if there isn't another item. After detaching,
  // this item will not have a parent or a child.
  void Detach(nsMenuChainItem** aRoot);
};

// this class is used for dispatching popupshowing events asynchronously.
class nsXULPopupShowingEvent : public mozilla::Runnable {
 public:
  nsXULPopupShowingEvent(nsIContent* aPopup, bool aIsContextMenu,
                         bool aSelectFirstItem)
      : mozilla::Runnable("nsXULPopupShowingEvent"),
        mPopup(aPopup),
        mIsContextMenu(aIsContextMenu),
        mSelectFirstItem(aSelectFirstItem) {
    NS_ASSERTION(aPopup,
                 "null popup supplied to nsXULPopupShowingEvent constructor");
  }

  NS_IMETHOD Run() override;

 private:
  nsCOMPtr<nsIContent> mPopup;
  bool mIsContextMenu;
  bool mSelectFirstItem;
};

// this class is used for dispatching popuphiding events asynchronously.
class nsXULPopupHidingEvent : public mozilla::Runnable {
 public:
  nsXULPopupHidingEvent(nsIContent* aPopup, nsIContent* aNextPopup,
                        nsIContent* aLastPopup, nsPopupType aPopupType,
                        bool aDeselectMenu, bool aIsCancel)
      : mozilla::Runnable("nsXULPopupHidingEvent"),
        mPopup(aPopup),
        mNextPopup(aNextPopup),
        mLastPopup(aLastPopup),
        mPopupType(aPopupType),
        mDeselectMenu(aDeselectMenu),
        mIsRollup(aIsCancel) {
    NS_ASSERTION(aPopup,
                 "null popup supplied to nsXULPopupHidingEvent constructor");
    // aNextPopup and aLastPopup may be null
  }

  NS_IMETHOD Run() override;

 private:
  nsCOMPtr<nsIContent> mPopup;
  nsCOMPtr<nsIContent> mNextPopup;
  nsCOMPtr<nsIContent> mLastPopup;
  nsPopupType mPopupType;
  bool mDeselectMenu;
  bool mIsRollup;
};

// this class is used for dispatching popuppositioned events asynchronously.
class nsXULPopupPositionedEvent : public mozilla::Runnable {
 public:
  explicit nsXULPopupPositionedEvent(nsIContent* aPopup, bool aIsContextMenu,
                                     bool aSelectFirstItem)
      : mozilla::Runnable("nsXULPopupPositionedEvent"),
        mPopup(aPopup),
        mIsContextMenu(aIsContextMenu),
        mSelectFirstItem(aSelectFirstItem) {
    NS_ASSERTION(aPopup,
                 "null popup supplied to nsXULPopupShowingEvent constructor");
  }

  NS_IMETHOD Run() override;

  // Asynchronously dispatch a popuppositioned event at aPopup if this is a
  // panel that should receieve such events. Return true if the event was sent.
  static bool DispatchIfNeeded(nsIContent* aPopup, bool aIsContextMenu,
                               bool aSelectFirstItem);

 private:
  nsCOMPtr<nsIContent> mPopup;
  bool mIsContextMenu;
  bool mSelectFirstItem;
};

// this class is used for dispatching menu command events asynchronously.
class nsXULMenuCommandEvent : public mozilla::Runnable {
 public:
  nsXULMenuCommandEvent(mozilla::dom::Element* aMenu, bool aIsTrusted,
                        bool aShift, bool aControl, bool aAlt, bool aMeta,
                        bool aUserInput, bool aFlipChecked)
      : mozilla::Runnable("nsXULMenuCommandEvent"),
        mMenu(aMenu),
        mIsTrusted(aIsTrusted),
        mShift(aShift),
        mControl(aControl),
        mAlt(aAlt),
        mMeta(aMeta),
        mUserInput(aUserInput),
        mFlipChecked(aFlipChecked),
        mCloseMenuMode(CloseMenuMode_Auto) {
    NS_ASSERTION(aMenu,
                 "null menu supplied to nsXULMenuCommandEvent constructor");
  }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override;

  void SetCloseMenuMode(CloseMenuMode aCloseMenuMode) {
    mCloseMenuMode = aCloseMenuMode;
  }

 private:
  RefPtr<mozilla::dom::Element> mMenu;
  bool mIsTrusted;
  bool mShift;
  bool mControl;
  bool mAlt;
  bool mMeta;
  bool mUserInput;
  bool mFlipChecked;
  CloseMenuMode mCloseMenuMode;
};

class nsXULPopupManager final : public nsIDOMEventListener,
                                public nsIRollupListener,
                                public nsIObserver {
 public:
  friend class nsXULPopupShowingEvent;
  friend class nsXULPopupHidingEvent;
  friend class nsXULPopupPositionedEvent;
  friend class nsXULMenuCommandEvent;
  friend class TransitionEnder;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER

  // nsIRollupListener
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual bool Rollup(uint32_t aCount, bool aFlush, const nsIntPoint* pos,
                      nsIContent** aLastRolledUp) override;
  virtual bool ShouldRollupOnMouseWheelEvent() override;
  virtual bool ShouldConsumeOnMouseWheelEvent() override;
  virtual bool ShouldRollupOnMouseActivate() override;
  virtual uint32_t GetSubmenuWidgetChain(
      nsTArray<nsIWidget*>* aWidgetChain) override;
  virtual void NotifyGeometryChange() override {}
  virtual nsIWidget* GetRollupWidget() override;

  static nsXULPopupManager* sInstance;

  // initialize and shutdown methods called by nsLayoutStatics
  static nsresult Init();
  static void Shutdown();

  // returns a weak reference to the popup manager instance, could return null
  // if a popup manager could not be allocated
  static nsXULPopupManager* GetInstance();

  // Returns the immediate parent frame of inserted children of aFrame's
  // content.
  //
  // FIXME(emilio): Or something like that, because this is kind of broken in a
  // variety of situations like multiple insertion points.
  static nsContainerFrame* ImmediateParentFrame(nsContainerFrame* aFrame);

  // This should be called when a window is moved or resized to adjust the
  // popups accordingly.
  void AdjustPopupsOnWindowChange(nsPIDOMWindowOuter* aWindow);
  void AdjustPopupsOnWindowChange(mozilla::PresShell* aPresShell);

  // given a menu frame, find the prevous or next menu frame. If aPopup is
  // true then navigate a menupopup, from one item on the menu to the previous
  // or next one. This is used for cursor navigation between items in a popup
  // menu. If aIsPopup is false, the navigation is on a menubar, so navigate
  // between menus on the menubar. This is used for left/right cursor
  // navigation.
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
  // aWrap - true to wrap around to the beginning and continue searching if not
  //         found. False to end at the beginning or end of the menu.
  static nsMenuFrame* GetPreviousMenuItem(nsContainerFrame* aParent,
                                          nsMenuFrame* aStart, bool aIsPopup,
                                          bool aWrap);
  static nsMenuFrame* GetNextMenuItem(nsContainerFrame* aParent,
                                      nsMenuFrame* aStart, bool aIsPopup,
                                      bool aWrap);

  // returns true if the menu item aContent is a valid menuitem which may
  // be navigated to. aIsPopup should be true for items on a popup, or false
  // for items on a menubar.
  static bool IsValidMenuItem(nsIContent* aContent, bool aOnPopup);

  // inform the popup manager that a menu bar has been activated or deactivated,
  // either because one of its menus has opened or closed, or that the menubar
  // has been focused such that its menus may be navigated with the keyboard.
  // aActivate should be true when the menubar should be focused, and false
  // when the active menu bar should be defocused. In the latter case, if
  // aMenuBar isn't currently active, yet another menu bar is, that menu bar
  // will remain active.
  void SetActiveMenuBar(nsMenuBarFrame* aMenuBar, bool aActivate);

  // retrieve the node and offset of the last mouse event used to open a
  // context menu. This information is determined from the rangeParent and
  // the rangeOffset of the event supplied to ShowPopup or ShowPopupAtScreen.
  // This is used by the implementation of Document::GetPopupRangeParent
  // and Document::GetPopupRangeOffset.
  nsINode* GetMouseLocationParent();
  int32_t MouseLocationOffset();

  /**
   * Open a <menu> given its content node. If aSelectFirstItem is
   * set to true, the first item on the menu will automatically be
   * selected. If aAsynchronous is true, the event will be dispatched
   * asynchronously. This should be true when called from frame code.
   */
  void ShowMenu(nsIContent* aMenu, bool aSelectFirstItem, bool aAsynchronous);

  /**
   * Open a popup, either anchored or unanchored. If aSelectFirstItem is
   * true, then the first item in the menu is selected. The arguments are
   * similar to those for XULPopupElement::OpenPopup.
   *
   * aTriggerEvent should be the event that triggered the event. This is used
   * to determine the coordinates and trigger node for the popup. This may be
   * null if the popup was not triggered by an event.
   *
   * This fires the popupshowing event synchronously.
   */
  void ShowPopup(nsIContent* aPopup, nsIContent* aAnchorContent,
                 const nsAString& aPosition, int32_t aXPos, int32_t aYPos,
                 bool aIsContextMenu, bool aAttributesOverride,
                 bool aSelectFirstItem, mozilla::dom::Event* aTriggerEvent);

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
  void ShowPopupAtScreen(nsIContent* aPopup, int32_t aXPos, int32_t aYPos,
                         bool aIsContextMenu,
                         mozilla::dom::Event* aTriggerEvent);

  /* Open a popup anchored at a screen rectangle specified by aRect.
   * The remaining arguments are similar to ShowPopup.
   */
  void ShowPopupAtScreenRect(nsIContent* aPopup, const nsAString& aPosition,
                             const nsIntRect& aRect, bool aIsContextMenu,
                             bool aAttributesOverride,
                             mozilla::dom::Event* aTriggerEvent);

  /**
   * Open a tooltip at a specific screen position specified by aXPos and aYPos,
   * measured in CSS pixels.
   *
   * This fires the popupshowing event synchronously.
   */
  void ShowTooltipAtScreen(nsIContent* aPopup, nsIContent* aTriggerContent,
                           int32_t aXPos, int32_t aYPos);

  /*
   * Hide a popup aPopup. If the popup is in a <menu>, then also inform the
   * menu that the popup is being hidden.
   *
   * aHideChain - true if the entire chain of menus should be closed. If false,
   *              only this popup is closed.
   * aDeselectMenu - true if the parent <menu> of the popup should be
   *                 deselected. This will be false when the menu is closed by
   *                 pressing the Escape key.
   * aAsynchronous - true if the first popuphiding event should be sent
   *                 asynchrously. This should be true if HidePopup is called
   *                 from a frame.
   * aIsCancel - true if this popup is hiding due to being cancelled.
   * aLastPopup - optional popup to close last when hiding a chain of menus.
   *              If null, then all popups will be closed.
   */
  void HidePopup(nsIContent* aPopup, bool aHideChain, bool aDeselectMenu,
                 bool aAsynchronous, bool aIsCancel,
                 nsIContent* aLastPopup = nullptr);

  /**
   * Hide a popup after a short delay. This is used when rolling over menu
   * items. This timer is stored in mCloseTimer. The timer may be cancelled and
   * the popup closed by calling KillMenuTimer.
   */
  void HidePopupAfterDelay(nsMenuPopupFrame* aPopup);

  /**
   * Hide all of the popups from a given docshell. This should be called when
   * the document is hidden.
   */
  void HidePopupsInDocShell(nsIDocShellTreeItem* aDocShellToHide);

  /**
   * Enable or disable the dynamic noautohide state of a panel.
   *
   * aPanel - the panel whose state is to change
   * aShouldRollup - whether the panel is no longer noautohide
   */
  void EnableRollup(nsIContent* aPopup, bool aShouldRollup);

  /**
   * Check if any popups need to be repositioned or hidden after a style or
   * layout change. This will update, for example, any arrow type panels when
   * the anchor that is is pointing to has moved, resized or gone away.
   * Only those popups that pertain to the supplied aRefreshDriver are updated.
   */
  void UpdatePopupPositions(nsRefreshDriver* aRefreshDriver);

  /**
   * Enable or disable anchor following on the popup if needed.
   */
  void UpdateFollowAnchor(nsMenuPopupFrame* aPopup);

  /**
   * Execute a menu command from the triggering event aEvent.
   *
   * aMenu - a menuitem to execute
   * aEvent - an nsXULMenuCommandEvent that contains all the info from the mouse
   *          event which triggered the menu to be executed, may not be null
   */
  void ExecuteMenu(nsIContent* aMenu, nsXULMenuCommandEvent* aEvent);

  /**
   * Return true if the popup for the supplied content node is open.
   */
  bool IsPopupOpen(nsIContent* aPopup);

  /**
   * Return true if the popup for the supplied menu parent is open.
   */
  bool IsPopupOpenForMenuParent(nsMenuParent* aMenuParent);

  /**
   * Return the frame for the topmost open popup of a given type, or null if
   * no popup of that type is open. If aType is ePopupTypeAny, a menu of any
   * type is returned.
   */
  nsIFrame* GetTopPopup(nsPopupType aType);

  /**
   * Return an array of all the open and visible popup frames for
   * menus, in order from top to bottom.
   */
  void GetVisiblePopups(nsTArray<nsIFrame*>& aPopups);

  /**
   * Get the node that last triggered a popup or tooltip in the document
   * aDocument. aDocument must be non-null and be a document contained within
   * the same window hierarchy as the popup to retrieve.
   */
  already_AddRefed<nsINode> GetLastTriggerPopupNode(
      mozilla::dom::Document* aDocument) {
    return GetLastTriggerNode(aDocument, false);
  }

  already_AddRefed<nsINode> GetLastTriggerTooltipNode(
      mozilla::dom::Document* aDocument) {
    return GetLastTriggerNode(aDocument, true);
  }

  /**
   * Return false if a popup may not be opened. This will return false if the
   * popup is already open, if the popup is in a content shell that is not
   * focused, or if it is a submenu of another menu that isn't open.
   */
  bool MayShowPopup(nsMenuPopupFrame* aFrame);

  /**
   * Indicate that the popup associated with aView has been moved to the
   * specified screen coordiates.
   */
  void PopupMoved(nsIFrame* aFrame, nsIntPoint aPoint);

  /**
   * Indicate that the popup associated with aView has been resized to the
   * given device pixel size aSize.
   */
  void PopupResized(nsIFrame* aFrame, mozilla::LayoutDeviceIntSize aSize);

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
  bool HasContextMenu(nsMenuPopupFrame* aPopup);

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
   * Handles navigation for menu accelkeys. If aFrame is specified, then the
   * key is handled by that popup, otherwise if aFrame is null, the key is
   * handled by the active popup or menubar.
   */
  bool HandleShortcutNavigation(mozilla::dom::KeyboardEvent* aKeyEvent,
                                nsMenuPopupFrame* aFrame);

  /**
   * Handles cursor navigation within a menu. Returns true if the key has
   * been handled.
   */
  bool HandleKeyboardNavigation(uint32_t aKeyCode);

  /**
   * Handle keyboard navigation within a menu popup specified by aFrame.
   * Returns true if the key was handled and other default handling
   * should not occur.
   */
  bool HandleKeyboardNavigationInPopup(nsMenuPopupFrame* aFrame,
                                       nsNavigationDirection aDir) {
    return HandleKeyboardNavigationInPopup(nullptr, aFrame, aDir);
  }

  /**
   * Handles the keyboard event with keyCode value. Returns true if the event
   * has been handled.
   */
  bool HandleKeyboardEventWithKeyCode(mozilla::dom::KeyboardEvent* aKeyEvent,
                                      nsMenuChainItem* aTopVisibleMenuItem);

  // Sets mIgnoreKeys of the Top Visible Menu Item
  nsresult UpdateIgnoreKeys(bool aIgnoreKeys);

  nsresult KeyUp(mozilla::dom::KeyboardEvent* aKeyEvent);
  nsresult KeyDown(mozilla::dom::KeyboardEvent* aKeyEvent);
  nsresult KeyPress(mozilla::dom::KeyboardEvent* aKeyEvent);

 protected:
  nsXULPopupManager();
  ~nsXULPopupManager();

  // get the nsMenuPopupFrame, if any, for the given content node
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsMenuPopupFrame* GetPopupFrameForContent(nsIContent* aContent,
                                            bool aShouldFlush);

  // return the topmost menu, skipping over invisible popups
  nsMenuChainItem* GetTopVisibleMenu();

  // Hide all of the visible popups from the given list. This function can
  // cause style changes and frame destruction.
  void HidePopupsInList(const nsTArray<nsMenuPopupFrame*>& aFrames);

  // set the event that was used to trigger the popup, or null to clear the
  // event details. aTriggerContent will be set to the target of the event.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void InitTriggerEvent(mozilla::dom::Event* aEvent, nsIContent* aPopup,
                        nsIContent** aTriggerContent);

  // callbacks for ShowPopup and HidePopup as events may be done asynchronously
  void ShowPopupCallback(nsIContent* aPopup, nsMenuPopupFrame* aPopupFrame,
                         bool aIsContextMenu, bool aSelectFirstItem);
  void HidePopupCallback(nsIContent* aPopup, nsMenuPopupFrame* aPopupFrame,
                         nsIContent* aNextPopup, nsIContent* aLastPopup,
                         nsPopupType aPopupType, bool aDeselectMenu);

  /**
   * Fire a popupshowing event on the popup and then open the popup.
   *
   * aPopup - the popup to open
   * aIsContextMenu - true for context menus
   * aSelectFirstItem - true to select the first item in the menu
   * aTriggerEvent - the event that triggered the showing event.
   *                 This is currently used to propagate the
   *                 inputSource attribute. May be null.
   */
  void FirePopupShowingEvent(nsIContent* aPopup, bool aIsContextMenu,
                             bool aSelectFirstItem,
                             mozilla::dom::Event* aTriggerEvent);

  /**
   * Fire a popuphiding event and then hide the popup. This will be called
   * recursively if aNextPopup and aLastPopup are set in order to hide a chain
   * of open menus. If these are not set, only one popup is closed. However,
   * if the popup type indicates a menu, yet the next popup is not a menu,
   * then this ends the closing of popups. This allows a menulist inside a
   * non-menu to close up the menu but not close up the panel it is contained
   * within.
   *
   * The caller must keep a strong reference to aPopup, aNextPopup and
   * aLastPopup.
   *
   * aPopup - the popup to hide
   * aNextPopup - the next popup to hide
   * aLastPopup - the last popup in the chain to hide
   * aPresContext - nsPresContext for the popup's frame
   * aPopupType - the PopupType of the frame.
   * aDeselectMenu - true to unhighlight the menu when hiding it
   * aIsCancel - true if this popup is hiding due to being cancelled.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void FirePopupHidingEvent(nsIContent* aPopup, nsIContent* aNextPopup,
                            nsIContent* aLastPopup, nsPresContext* aPresContext,
                            nsPopupType aPopupType, bool aDeselectMenu,
                            bool aIsCancel);

  /**
   * Handle keyboard navigation within a menu popup specified by aItem.
   */
  bool HandleKeyboardNavigationInPopup(nsMenuChainItem* aItem,
                                       nsNavigationDirection aDir) {
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
  bool HandleKeyboardNavigationInPopup(nsMenuChainItem* aItem,
                                       nsMenuPopupFrame* aFrame,
                                       nsNavigationDirection aDir);

 protected:
  already_AddRefed<nsINode> GetLastTriggerNode(
      mozilla::dom::Document* aDocument, bool aIsTooltip);

  /**
   * Set mouse capturing for the current popup. This traps mouse clicks that
   * occur outside the popup so that it can be closed up. aOldPopup should be
   * set to the popup that was previously the current popup.
   */
  void SetCaptureState(nsIContent* aOldPopup);

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
  bool IsChildOfDocShell(mozilla::dom::Document* aDoc,
                         nsIDocShellTreeItem* aExpected);

  // the document the key event listener is attached to
  nsCOMPtr<mozilla::dom::EventTarget> mKeyListener;

  // widget that is currently listening to rollup events
  nsCOMPtr<nsIWidget> mWidget;

  // range parent and offset set in SetTriggerEvent
  nsCOMPtr<nsINode> mRangeParent;
  int32_t mRangeOffset;
  // Device pixels relative to the showing popup's presshell's
  // root prescontext's root frame.
  mozilla::LayoutDeviceIntPoint mCachedMousePoint;

  // cached modifiers
  mozilla::Modifiers mCachedModifiers;

  // set to the currently active menu bar, if any
  nsMenuBarFrame* mActiveMenuBar;

  // linked list of normal menus and panels.
  nsMenuChainItem* mPopups;

  // timer used for HidePopupAfterDelay
  nsCOMPtr<nsITimer> mCloseTimer;

  // a popup that is waiting on the timer
  nsMenuPopupFrame* mTimerMenu;

  // the popup that is currently being opened, stored only during the
  // popupshowing event
  nsCOMPtr<nsIContent> mOpeningPopup;

  // If true, all popups won't hide automatically on blur
  static bool sDevtoolsDisableAutoHide;
};

#endif
