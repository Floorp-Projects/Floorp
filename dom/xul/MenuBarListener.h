/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_MenuBarListener_h
#define mozilla_dom_MenuBarListener_h

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsIDOMEventListener.h"

namespace mozilla::dom {
class Document;
class EventTarget;
class KeyboardEvent;
class XULMenuBarElement;
class XULButtonElement;

class MenuBarListener final : public nsIDOMEventListener {
 public:
  explicit MenuBarListener(XULMenuBarElement&);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

  // Should be called when unbound from the document and so on.
  void Detach();

 protected:
  virtual ~MenuBarListener();

  bool IsMenuOpen() const;

  MOZ_CAN_RUN_SCRIPT nsresult KeyUp(Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT nsresult KeyDown(Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT nsresult KeyPress(Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT nsresult Blur(Event* aEvent);
  MOZ_CAN_RUN_SCRIPT nsresult OnWindowDeactivated(Event* aEvent);
  MOZ_CAN_RUN_SCRIPT nsresult MouseDown(Event* aMouseEvent);
  MOZ_CAN_RUN_SCRIPT nsresult Fullscreen(Event* aEvent);

  /**
   * Given a key event for an Alt+shortcut combination,
   * return the menu, if any, that would be opened. If aPeek
   * is false, then play a beep and deactivate the menubar on Windows.
   */
  XULButtonElement* GetMenuForKeyEvent(KeyboardEvent& aKeyEvent);

  /**
   * Call MarkAsReservedByChrome if the user's preferences indicate that
   * the key should be chrome-only.
   */
  void ReserveKeyIfNeeded(Event* aKeyEvent);

  // This should only be called by the MenuBarListener during event dispatch.
  enum class ByKeyboard : bool { No, Yes };
  MOZ_CAN_RUN_SCRIPT void ToggleMenuActiveState(ByKeyboard);

  bool Destroyed() const { return !mMenuBar; }

  // The menu bar object. Safe because it keeps us alive.
  XULMenuBarElement* mMenuBar;
  // The event target to listen to the events.
  //
  // Weak reference is safe because we clear the listener on unbind from the
  // document.
  Document* mEventTarget;
  // Whether or not the ALT key is currently down.
  bool mAccessKeyDown = false;
  // Whether or not the ALT key down is canceled by other action.
  bool mAccessKeyDownCanceled = false;
};

}  // namespace mozilla::dom

#endif  // #ifndef mozilla_dom_MenuBarListener_h
