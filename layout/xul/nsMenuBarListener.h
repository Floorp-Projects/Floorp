/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsMenuBarListener_h
#define nsMenuBarListener_h

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsIContent.h"
#include "nsIDOMEventListener.h"

// X.h defines KeyPress
#ifdef KeyPress
#  undef KeyPress
#endif

class nsMenuFrame;
class nsMenuBarFrame;

namespace mozilla {
namespace dom {
class EventTarget;
class KeyboardEvent;
}  // namespace dom
}  // namespace mozilla

/**
 * EventListener implementation for menubar.
 */
class nsMenuBarListener final : public nsIDOMEventListener {
 public:
  explicit nsMenuBarListener(nsMenuBarFrame* aMenuBarFrame,
                             nsIContent* aMenuBarContent);

  NS_DECL_ISUPPORTS

  /**
   * nsIDOMEventListener interface method.
   */
  NS_DECL_NSIDOMEVENTLISTENER

  /**
   * When mMenuBarFrame is being destroyed, this should be called.
   */
  void OnDestroyMenuBarFrame();

  /**
   * GetMenuAccessKey() returns keyCode value of a modifier key which is
   * used for accesskey.  Returns 0 if the platform doesn't support access key.
   */
  static nsresult GetMenuAccessKey(int32_t* aAccessKey);

  /**
   * IsAccessKeyPressed() returns true if the modifier state of aEvent matches
   * the modifier state of access key.
   */
  static bool IsAccessKeyPressed(mozilla::dom::KeyboardEvent* aEvent);

 protected:
  virtual ~nsMenuBarListener();

  nsresult KeyUp(mozilla::dom::Event* aMouseEvent);
  nsresult KeyDown(mozilla::dom::Event* aMouseEvent);
  nsresult KeyPress(mozilla::dom::Event* aMouseEvent);
  nsresult Blur(mozilla::dom::Event* aEvent);
  nsresult OnWindowDeactivated(mozilla::dom::Event* aEvent);
  nsresult MouseDown(mozilla::dom::Event* aMouseEvent);
  nsresult Fullscreen(mozilla::dom::Event* aEvent);

  static void InitAccessKey();

  static mozilla::Modifiers GetModifiersForAccessKey(
      mozilla::dom::KeyboardEvent* event);

  /**
   * Given a key event for an Alt+shortcut combination,
   * return the menu, if any, that would be opened. If aPeek
   * is false, then play a beep and deactivate the menubar on Windows.
   */
  nsMenuFrame* GetMenuForKeyEvent(mozilla::dom::KeyboardEvent* aKeyEvent,
                                  bool aPeek);

  /**
   * Call MarkAsReservedByChrome if the user's preferences indicate that
   * the key should be chrome-only.
   */
  void ReserveKeyIfNeeded(mozilla::dom::Event* aKeyEvent);

  // This should only be called by the nsMenuBarListener during event dispatch,
  // thus ensuring that this doesn't get destroyed during the process.
  void ToggleMenuActiveState();

  bool Destroyed() const { return !mMenuBarFrame; }

  // The menu bar object.
  nsMenuBarFrame* mMenuBarFrame;
  // The event target to listen to the events.
  // XXX Should this store this as strong reference?  However,
  //     OnDestroyMenuBarFrame() should be called at destroying mMenuBarFrame.
  //     So, weak reference must be safe.
  mozilla::dom::EventTarget* mEventTarget;
  // The top window as EventTarget.
  mozilla::dom::EventTarget* mTopWindowEventTarget;
  // Whether or not the ALT key is currently down.
  bool mAccessKeyDown;
  // Whether or not the ALT key down is canceled by other action.
  bool mAccessKeyDownCanceled;
  // See KeyboardEvent for sample values (DOM_VK_* constants).
  static int32_t mAccessKey;
  // Modifier mask for the access key.
  static mozilla::Modifiers mAccessKeyMask;
};

#endif  // #ifndef nsMenuBarListener_h
