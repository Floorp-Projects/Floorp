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
#include "nsHashtablesFwd.h"
#include "nsIContent.h"
#include "nsIRollupListener.h"
#include "nsIDOMEventListener.h"
#include "Units.h"
#include "nsPoint.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIObserver.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/FunctionRef.h"
#include "mozilla/widget/InitData.h"
#include "mozilla/widget/NativeMenu.h"

// XXX Avoid including this here by moving function bodies to the cpp file.
#include "mozilla/dom/Element.h"

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
class nsITimer;
class nsIDocShellTreeItem;
class nsMenuPopupFrame;
class nsPIDOMWindowOuter;
class nsRefreshDriver;

namespace mozilla {
class PresShell;
namespace dom {
class Event;
class KeyboardEvent;
class UIEvent;
class XULButtonElement;
class XULMenuBarElement;
class XULPopupElement;
}  // namespace dom
}  // namespace mozilla

// XUL popups can be in several different states. When opening a popup, the
// state changes as follows:
//   ePopupClosed - initial state
//   ePopupShowing - during the period when the popupshowing event fires
//   ePopupOpening - between the popupshowing event and being visible. Creation
//                   of the child frames, layout and reflow occurs in this
//                   state. The popup is stored in the popup manager's list of
//                   open popups during this state.
//   ePopupVisible - layout is done and the popup's view and widget are made
//                   visible. The popup is visible on screen but may be
//                   transitioning. The popupshown event has not yet fired.
//   ePopupShown - the popup has been shown and is fully ready. This state is
//                 assigned just before the popupshown event fires.
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
  // state while a popup is waiting to be laid out and positioned
  ePopupPositioning,
  // state while a popup is open but the widget is not yet visible
  ePopupOpening,
  // state while a popup is visible and waiting for the popupshown event
  ePopupVisible,
  // state while a popup is open and visible on screen
  ePopupShown,
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

enum class HidePopupOption : uint8_t {
  // If the entire chain of menus should be closed.
  HideChain,
  // If the parent <menu> of the popup should not be deselected. This will not
  // be set when the menu is closed by pressing the Escape key.
  DeselectMenu,
  // If the first popuphiding event should be sent asynchrously. This should
  // be set if HidePopup is called from a frame.
  Async,
  // If this popup is hiding due to being cancelled.
  IsRollup,
  // Whether animations should be disabled for rolled-up popups.
  DisableAnimations,
};

using HidePopupOptions = mozilla::EnumSet<HidePopupOption>;

/**
 * DirectionFromKeyCodeTable: two arrays, the first for left-to-right and the
 * other for right-to-left, that map keycodes to values of
 * nsNavigationDirection.
 */
extern const nsNavigationDirection DirectionFromKeyCodeTable[2][6];

#define NS_DIRECTION_FROM_KEY_CODE(frame, keycode)                    \
  (DirectionFromKeyCodeTable                                          \
       [static_cast<uint8_t>((frame)->StyleVisibility()->mDirection)] \
       [(keycode) - mozilla::dom::KeyboardEvent_Binding::DOM_VK_END])

// Used to hold information about a popup that is about to be opened.
struct PendingPopup {
  using Element = mozilla::dom::Element;
  using Event = mozilla::dom::Event;

  PendingPopup(Element* aPopup, Event* aEvent);

  const RefPtr<Element> mPopup;
  const RefPtr<Event> mEvent;

  // Device pixels relative to the showing popup's presshell's
  // root prescontext's root frame.
  mozilla::LayoutDeviceIntPoint mMousePoint;

  // Cached modifiers used to trigger the popup.
  mozilla::Modifiers mModifiers;

  already_AddRefed<nsIContent> GetTriggerContent() const;

  void InitMousePoint();

  void SetMousePoint(mozilla::LayoutDeviceIntPoint aMousePoint) {
    mMousePoint = aMousePoint;
  }

  uint16_t MouseInputSource() const;
};

// nsMenuChainItem holds info about an open popup. Items are stored in a
// doubly linked list. Note that the linked list is stored beginning from
// the lowest child in a chain of menus, as this is the active submenu.
class nsMenuChainItem {
  using PopupType = mozilla::widget::PopupType;

  nsMenuPopupFrame* mFrame;  // the popup frame
  PopupType mPopupType;      // the popup type of the frame
  bool mNoAutoHide;          // true for noautohide panels
  bool mIsContext;           // true for context menus
  bool mOnMenuBar;           // true if the menu is on a menu bar
  nsIgnoreKeys mIgnoreKeys;  // indicates how keyboard listeners should be used

  // True if the popup should maintain its position relative to the anchor when
  // the anchor moves.
  bool mFollowAnchor;

  // The last seen position of the anchor, relative to the screen.
  nsRect mCurrentRect;

  mozilla::UniquePtr<nsMenuChainItem> mParent;
  // Back pointer, safe because mChild keeps us alive.
  nsMenuChainItem* mChild = nullptr;

 public:
  nsMenuChainItem(nsMenuPopupFrame* aFrame, bool aNoAutoHide, bool aIsContext,
                  PopupType aPopupType)
      : mFrame(aFrame),
        mPopupType(aPopupType),
        mNoAutoHide(aNoAutoHide),
        mIsContext(aIsContext),
        mOnMenuBar(false),
        mIgnoreKeys(eIgnoreKeys_False),
        mFollowAnchor(false) {
    NS_ASSERTION(aFrame, "null frame passed to nsMenuChainItem constructor");
    MOZ_COUNT_CTOR(nsMenuChainItem);
  }

  MOZ_COUNTED_DTOR(nsMenuChainItem)

  mozilla::dom::XULPopupElement* Element();
  nsMenuPopupFrame* Frame() { return mFrame; }
  PopupType GetPopupType() { return mPopupType; }
  bool IsNoAutoHide() { return mNoAutoHide; }
  void SetNoAutoHide(bool aNoAutoHide) { mNoAutoHide = aNoAutoHide; }
  bool IsMenu() { return mPopupType == PopupType::Menu; }
  bool IsContextMenu() { return mIsContext; }
  nsIgnoreKeys IgnoreKeys() { return mIgnoreKeys; }
  void SetIgnoreKeys(nsIgnoreKeys aIgnoreKeys) { mIgnoreKeys = aIgnoreKeys; }
  bool IsOnMenuBar() { return mOnMenuBar; }
  void SetOnMenuBar(bool aOnMenuBar) { mOnMenuBar = aOnMenuBar; }
  nsMenuChainItem* GetParent() { return mParent.get(); }
  nsMenuChainItem* GetChild() { return mChild; }
  bool FollowsAnchor() { return mFollowAnchor; }
  void UpdateFollowAnchor();
  void CheckForAnchorChange();

  // set the parent of this item to aParent, also changing the parent
  // to have this as a child.
  void SetParent(mozilla::UniquePtr<nsMenuChainItem> aParent);
  // Removes the parent pointer and returns it.
  mozilla::UniquePtr<nsMenuChainItem> Detach();
};

// this class is used for dispatching popuphiding events asynchronously.
class nsXULPopupHidingEvent : public mozilla::Runnable {
  using PopupType = mozilla::widget::PopupType;
  using Element = mozilla::dom::Element;

 public:
  nsXULPopupHidingEvent(Element* aPopup, Element* aNextPopup,
                        Element* aLastPopup, PopupType aPopupType,
                        HidePopupOptions aOptions)
      : mozilla::Runnable("nsXULPopupHidingEvent"),
        mPopup(aPopup),
        mNextPopup(aNextPopup),
        mLastPopup(aLastPopup),
        mPopupType(aPopupType),
        mOptions(aOptions) {
    NS_ASSERTION(aPopup,
                 "null popup supplied to nsXULPopupHidingEvent constructor");
    // aNextPopup and aLastPopup may be null
  }

  NS_IMETHOD Run() override;

 private:
  nsCOMPtr<Element> mPopup;
  nsCOMPtr<Element> mNextPopup;
  nsCOMPtr<Element> mLastPopup;
  PopupType mPopupType;
  HidePopupOptions mOptions;
};

// this class is used for dispatching popuppositioned events asynchronously.
class nsXULPopupPositionedEvent : public mozilla::Runnable {
  using Element = mozilla::dom::Element;

 public:
  explicit nsXULPopupPositionedEvent(Element* aPopup)
      : mozilla::Runnable("nsXULPopupPositionedEvent"), mPopup(aPopup) {
    MOZ_ASSERT(aPopup);
  }

  NS_IMETHOD Run() override;

  // Asynchronously dispatch a popuppositioned event at aPopup if this is a
  // panel that should receieve such events. Return true if the event was sent.
  static bool DispatchIfNeeded(Element* aPopup);

 private:
  const nsCOMPtr<Element> mPopup;
};

// this class is used for dispatching menu command events asynchronously.
class nsXULMenuCommandEvent : public mozilla::Runnable {
  using Element = mozilla::dom::Element;

 public:
  nsXULMenuCommandEvent(Element* aMenu, bool aIsTrusted,
                        mozilla::Modifiers aModifiers, bool aUserInput,
                        bool aFlipChecked, int16_t aButton)
      : mozilla::Runnable("nsXULMenuCommandEvent"),
        mMenu(aMenu),
        mModifiers(aModifiers),
        mButton(aButton),
        mIsTrusted(aIsTrusted),
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
  RefPtr<Element> mMenu;

  mozilla::Modifiers mModifiers;
  int16_t mButton;
  bool mIsTrusted;
  bool mUserInput;
  bool mFlipChecked;
  CloseMenuMode mCloseMenuMode;
};

class nsXULPopupManager final : public nsIDOMEventListener,
                                public nsIRollupListener,
                                public nsIObserver,
                                public mozilla::widget::NativeMenu::Observer {
 public:
  friend class nsXULPopupHidingEvent;
  friend class nsXULPopupPositionedEvent;
  friend class nsXULMenuCommandEvent;
  friend class TransitionEnder;

  using PopupType = mozilla::widget::PopupType;
  using Element = mozilla::dom::Element;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER

  // nsIRollupListener
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  bool Rollup(const RollupOptions&,
              nsIContent** aLastRolledUp = nullptr) override;
  bool ShouldRollupOnMouseWheelEvent() override;
  bool ShouldConsumeOnMouseWheelEvent() override;
  bool ShouldRollupOnMouseActivate() override;
  uint32_t GetSubmenuWidgetChain(nsTArray<nsIWidget*>* aWidgetChain) override;
  nsIWidget* GetRollupWidget() override;
  bool RollupNativeMenu() override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY bool RollupTooltips();

  enum class RollupKind { Tooltip, Menu };
  MOZ_CAN_RUN_SCRIPT
  bool RollupInternal(RollupKind, const RollupOptions&,
                      nsIContent** aLastRolledUp);

  // NativeMenu::Observer
  void OnNativeMenuOpened() override;
  void OnNativeMenuClosed() override;
  void OnNativeSubMenuWillOpen(mozilla::dom::Element* aPopupElement) override;
  void OnNativeSubMenuDidOpen(mozilla::dom::Element* aPopupElement) override;
  void OnNativeSubMenuClosed(mozilla::dom::Element* aPopupElement) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void OnNativeMenuWillActivateItem(
      mozilla::dom::Element* aMenuItemElement) override;

  static nsXULPopupManager* sInstance;

  // initialize and shutdown methods called by nsLayoutStatics
  static nsresult Init();
  static void Shutdown();

  // returns a weak reference to the popup manager instance, could return null
  // if a popup manager could not be allocated
  static nsXULPopupManager* GetInstance();

  // This should be called when a window is moved or resized to adjust the
  // popups accordingly.
  void AdjustPopupsOnWindowChange(nsPIDOMWindowOuter* aWindow);
  void AdjustPopupsOnWindowChange(mozilla::PresShell* aPresShell);

  // inform the popup manager that a menu bar has been activated or deactivated,
  // either because one of its menus has opened or closed, or that the menubar
  // has been focused such that its menus may be navigated with the keyboard.
  // aActivate should be true when the menubar should be focused, and false
  // when the active menu bar should be defocused. In the latter case, if
  // aMenuBar isn't currently active, yet another menu bar is, that menu bar
  // will remain active.
  void SetActiveMenuBar(mozilla::dom::XULMenuBarElement* aMenuBar,
                        bool aActivate);

  struct MayShowMenuResult {
    const bool mIsNative = false;
    mozilla::dom::XULButtonElement* const mMenuButton = nullptr;
    nsMenuPopupFrame* const mMenuPopupFrame = nullptr;

    explicit operator bool() const {
      MOZ_ASSERT(!!mMenuButton == !!mMenuPopupFrame);
      return mIsNative || mMenuButton;
    }
  };

  MayShowMenuResult MayShowMenu(nsIContent* aMenu);

  /**
   * Open a <menu> given its content node. If aSelectFirstItem is
   * set to true, the first item on the menu will automatically be
   * selected.
   */
  void ShowMenu(nsIContent* aMenu, bool aSelectFirstItem);

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
  void ShowPopup(Element* aPopup, nsIContent* aAnchorContent,
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
  void ShowPopupAtScreen(Element* aPopup, int32_t aXPos, int32_t aYPos,
                         bool aIsContextMenu,
                         mozilla::dom::Event* aTriggerEvent);

  /* Open a popup anchored at a screen rectangle specified by aRect.
   * The remaining arguments are similar to ShowPopup.
   */
  void ShowPopupAtScreenRect(Element* aPopup, const nsAString& aPosition,
                             const nsIntRect& aRect, bool aIsContextMenu,
                             bool aAttributesOverride,
                             mozilla::dom::Event* aTriggerEvent);

  /**
   * Open a popup as a native menu, at a specific screen position specified by
   * aXPos and aYPos, measured in CSS pixels.
   *
   * This fires the popupshowing event synchronously.
   *
   * Returns whether native menus are supported for aPopup on this platform.
   * TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY bool ShowPopupAsNativeMenu(
      Element* aPopup, int32_t aXPos, int32_t aYPos, bool aIsContextMenu,
      mozilla::dom::Event* aTriggerEvent);

  /**
   * Open a tooltip at a specific screen position specified by aXPos and aYPos,
   * measured in device pixels. This fires the popupshowing event synchronously.
   */
  void ShowTooltipAtScreen(Element* aPopup, nsIContent* aTriggerContent,
                           const mozilla::LayoutDeviceIntPoint&);

  /*
   * Hide a popup aPopup. If the popup is in a <menu>, then also inform the
   * menu that the popup is being hidden.
   * aLastPopup - optional popup to close last when hiding a chain of menus.
   *              If null, then all popups will be closed.
   */
  void HidePopup(Element* aPopup, HidePopupOptions,
                 Element* aLastPopup = nullptr);

  /*
   * Hide the popup of a <menu>.
   */
  void HideMenu(nsIContent* aMenu);

  /**
   * Hide a popup after a short delay. This is used when rolling over menu
   * items. This timer is stored in mCloseTimer. The timer may be cancelled and
   * the popup closed by calling KillMenuTimer.
   */
  void HidePopupAfterDelay(nsMenuPopupFrame* aPopup, int32_t aDelay);

  /**
   * Hide all of the popups from a given docshell. This should be called when
   * the document is hidden.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void HidePopupsInDocShell(nsIDocShellTreeItem* aDocShellToHide);

  /**
   * Check if any popups need to be repositioned or hidden after a style or
   * layout change. This will update, for example, any arrow type panels when
   * the anchor that is is pointing to has moved, resized or gone away.
   * Only those popups that pertain to the supplied aRefreshDriver are updated.
   */
  void UpdatePopupPositions(nsRefreshDriver* aRefreshDriver);

  /**
   * Get the first nsMenuChainItem that is matched by the matching callback
   * function provided.
   */
  nsMenuChainItem* FirstMatchingPopup(
      mozilla::FunctionRef<bool(nsMenuChainItem*)> aMatcher) const;

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
  MOZ_CAN_RUN_SCRIPT void ExecuteMenu(nsIContent* aMenu,
                                      nsXULMenuCommandEvent* aEvent);

  /**
   * If a native menu is open, and aItem is an item in the menu's subtree,
   * execute the item with the help of the native menu and close the menu.
   * Returns true if a native menu was open.
   */
  bool ActivateNativeMenuItem(nsIContent* aItem, mozilla::Modifiers aModifiers,
                              int16_t aButton, mozilla::ErrorResult& aRv);

  /**
   * Return true if the popup for the supplied content node is open.
   */
  bool IsPopupOpen(Element* aPopup);

  /**
   * Return the frame for the topmost open popup of a given type, or null if
   * no popup of that type is open. If aType is PopupType::Any, a menu of any
   * type is returned.
   */
  nsIFrame* GetTopPopup(PopupType aType);

  /**
   * Returns the topmost active menuitem that's currently visible, if any.
   */
  nsIContent* GetTopActiveMenuItemContent();

  /**
   * Return an array of all the open and visible popup frames for
   * menus, in order from top to bottom.
   * XXX should we always include native menu?
   */
  void GetVisiblePopups(nsTArray<nsMenuPopupFrame*>& aPopups,
                        bool aIncludeNativeMenu = false);

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
   * specified device pixel coordinates.
   */
  void PopupMoved(nsIFrame* aFrame, const mozilla::LayoutDeviceIntPoint& aPoint,
                  bool aByMoveToRect);

  /**
   * Indicate that the popup associated with aView has been resized to the
   * given device pixel size aSize.
   */
  void PopupResized(nsIFrame* aFrame,
                    const mozilla::LayoutDeviceIntSize& aSize);

  /**
   * Called when a popup frame is destroyed. In this case, just remove the
   * item and later popups from the list. No point going through HidePopup as
   * the frames have gone away.
   */
  MOZ_CAN_RUN_SCRIPT void PopupDestroyed(nsMenuPopupFrame* aFrame);

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
  void UpdateMenuItems(Element* aPopup);

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
  void CancelMenuTimer(nsMenuPopupFrame*);

  /**
   * Handles navigation for menu accelkeys. If aFrame is specified, then the
   * key is handled by that popup, otherwise if aFrame is null, the key is
   * handled by the active popup or menubar.
   */
  MOZ_CAN_RUN_SCRIPT bool HandleShortcutNavigation(
      mozilla::dom::KeyboardEvent& aKeyEvent, nsMenuPopupFrame* aFrame);

  /**
   * Handles cursor navigation within a menu. Returns true if the key has
   * been handled.
   */
  MOZ_CAN_RUN_SCRIPT bool HandleKeyboardNavigation(uint32_t aKeyCode);

  /**
   * Handle keyboard navigation within a menu popup specified by aFrame.
   * Returns true if the key was handled and other default handling
   * should not occur.
   */
  MOZ_CAN_RUN_SCRIPT bool HandleKeyboardNavigationInPopup(
      nsMenuPopupFrame* aFrame, nsNavigationDirection aDir) {
    return HandleKeyboardNavigationInPopup(nullptr, aFrame, aDir);
  }

  /**
   * Handles the keyboard event with keyCode value. Returns true if the event
   * has been handled.
   */
  MOZ_CAN_RUN_SCRIPT bool HandleKeyboardEventWithKeyCode(
      mozilla::dom::KeyboardEvent* aKeyEvent,
      nsMenuChainItem* aTopVisibleMenuItem);

  // Sets mIgnoreKeys of the Top Visible Menu Item
  nsresult UpdateIgnoreKeys(bool aIgnoreKeys);

  nsPopupState GetPopupState(mozilla::dom::Element* aPopupElement);

  mozilla::dom::Event* GetOpeningPopupEvent() const {
    return mPendingPopup->mEvent.get();
  }

  MOZ_CAN_RUN_SCRIPT nsresult KeyUp(mozilla::dom::KeyboardEvent* aKeyEvent);
  MOZ_CAN_RUN_SCRIPT nsresult KeyDown(mozilla::dom::KeyboardEvent* aKeyEvent);
  MOZ_CAN_RUN_SCRIPT nsresult KeyPress(mozilla::dom::KeyboardEvent* aKeyEvent);

 protected:
  nsXULPopupManager();
  ~nsXULPopupManager();

  // get the nsMenuPopupFrame, if any, for the given content node
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsMenuPopupFrame* GetPopupFrameForContent(nsIContent* aContent,
                                            bool aShouldFlush);

  // Get the menu to start rolling up.
  nsMenuChainItem* GetRollupItem(RollupKind);

  // Return the topmost menu, skipping over invisible popups
  nsMenuChainItem* GetTopVisibleMenu() {
    return GetRollupItem(RollupKind::Menu);
  }

  // Add the chain item to the chain and update mPopups to point to it.
  void AddMenuChainItem(mozilla::UniquePtr<nsMenuChainItem>);

  // Removes the chain item from the chain and deletes it.
  void RemoveMenuChainItem(nsMenuChainItem*);

  // Hide all of the visible popups from the given list. This function can
  // cause style changes and frame destruction.
  MOZ_CAN_RUN_SCRIPT void HidePopupsInList(
      const nsTArray<nsMenuPopupFrame*>& aFrames);

  // Hide, but don't close, visible menus. Called before executing a menu item.
  // The caller promises to close the menus properly (with a call to HidePopup)
  // once the item has been executed.
  MOZ_CAN_RUN_SCRIPT void HideOpenMenusBeforeExecutingMenu(CloseMenuMode aMode);

  // callbacks for ShowPopup and HidePopup as events may be done asynchronously
  MOZ_CAN_RUN_SCRIPT void ShowPopupCallback(Element* aPopup,
                                            nsMenuPopupFrame* aPopupFrame,
                                            bool aIsContextMenu,
                                            bool aSelectFirstItem);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void HidePopupCallback(
      Element* aPopup, nsMenuPopupFrame* aPopupFrame, Element* aNextPopup,
      Element* aLastPopup, PopupType aPopupType, HidePopupOptions);

  /**
   * Trigger frame construction and reflow in the popup, fire a popupshowing
   * event on the popup and then open the popup.
   *
   * aPendingPopup - information about the popup to open
   * aIsContextMenu - true for context menus
   * aSelectFirstItem - true to select the first item in the menu
   * TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void BeginShowingPopup(
      const PendingPopup& aPendingPopup, bool aIsContextMenu,
      bool aSelectFirstItem);

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
   * aOptions - the relevant options to hide the popup. Only a subset is looked
   * at.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void FirePopupHidingEvent(Element* aPopup, Element* aNextPopup,
                            Element* aLastPopup, nsPresContext* aPresContext,
                            PopupType aPopupType, HidePopupOptions aOptions);

  /**
   * Handle keyboard navigation within a menu popup specified by aItem.
   */
  MOZ_CAN_RUN_SCRIPT
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
  MOZ_CAN_RUN_SCRIPT bool HandleKeyboardNavigationInPopup(
      nsMenuChainItem* aItem, nsMenuPopupFrame* aFrame,
      nsNavigationDirection aDir);

 protected:
  already_AddRefed<nsINode> GetLastTriggerNode(
      mozilla::dom::Document* aDocument, bool aIsTooltip);

  /**
   * Fire a popupshowing event for aPopup.
   */
  MOZ_CAN_RUN_SCRIPT nsEventStatus FirePopupShowingEvent(
      const PendingPopup& aPendingPopup, nsPresContext* aPresContext);

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
  // TODO: Convert UpdateKeyboardListeners() to MOZ_CAN_RUN_SCRIPT and get rid
  //       of the kungFuDeathGrip in it.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void UpdateKeyboardListeners();

  /*
   * Returns true if the docshell for aDoc is aExpected or a child of aExpected.
   */
  bool IsChildOfDocShell(mozilla::dom::Document* aDoc,
                         nsIDocShellTreeItem* aExpected);

  // Finds a chain item in mPopups.
  nsMenuChainItem* FindPopup(Element* aPopup) const;

  // the document the key event listener is attached to
  nsCOMPtr<mozilla::dom::EventTarget> mKeyListener;

  // widget that is currently listening to rollup events
  nsCOMPtr<nsIWidget> mWidget;

  // set to the currently active menu bar, if any
  mozilla::dom::XULMenuBarElement* mActiveMenuBar;

  // linked list of normal menus and panels. mPopups points to the innermost
  // popup, which keeps alive all their parents.
  mozilla::UniquePtr<nsMenuChainItem> mPopups;

  // timer used for HidePopupAfterDelay
  nsCOMPtr<nsITimer> mCloseTimer;
  nsMenuPopupFrame* mTimerMenu = nullptr;

  // Information about the popup that is currently firing a popupshowing event.
  const PendingPopup* mPendingPopup;

  // If a popup is displayed as a native menu, this is non-null while the
  // native menu is open.
  // mNativeMenu has a strong reference to the menupopup nsIContent.
  RefPtr<mozilla::widget::NativeMenu> mNativeMenu;

  // If the currently open native menu activated an item, this is the item's
  // close menu mode. Nothing() if mNativeMenu is null or if no item was
  // activated.
  mozilla::Maybe<CloseMenuMode> mNativeMenuActivatedItemCloseMenuMode;

  // If a popup is displayed as a native menu, this map contains the popup state
  // for any of its non-closed submenus. This state cannot be stored on the
  // submenus' nsMenuPopupFrames, because we usually don't generate frames for
  // the contents of native menus.
  // If a submenu is not present in this map, it means it's closed.
  // This map is empty if mNativeMenu is null.
  nsTHashMap<RefPtr<mozilla::dom::Element>, nsPopupState>
      mNativeMenuSubmenuStates;
};

#endif
