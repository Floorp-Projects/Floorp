/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsMenuBarListener_h
#define nsMenuBarListener_h

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsIDOMEventListener.h"

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

class nsMenuBarFrame;
class nsIDOMKeyEvent;

namespace mozilla {
namespace dom {
class EventTarget;
} // namespace dom
} // namespace mozilla

/**
 * EventListener implementation for menubar.
 */
class nsMenuBarListener final : public nsIDOMEventListener
{
public:
  explicit nsMenuBarListener(nsMenuBarFrame* aMenuBarFrame,
                             nsIContent* aMenuBarContent);

  NS_DECL_ISUPPORTS

  /**
   * nsIDOMEventListener interface method.
   */
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) override;

  /**
   * When mMenuBarFrame is being destroyed, this should be called.
   */
  void OnDestroyMenuBarFrame();

  static void InitializeStatics();

  /**
   * GetMenuAccessKey() returns keyCode value of a modifier key which is
   * used for accesskey.  Returns 0 if the platform doesn't support access key.
   */
  static nsresult GetMenuAccessKey(int32_t* aAccessKey);

  /**
   * IsAccessKeyPressed() returns true if the modifier state of aEvent matches
   * the modifier state of access key.
   */
  static bool IsAccessKeyPressed(nsIDOMKeyEvent* aEvent);

protected:
  virtual ~nsMenuBarListener();

  nsresult KeyUp(nsIDOMEvent* aMouseEvent);
  nsresult KeyDown(nsIDOMEvent* aMouseEvent);
  nsresult KeyPress(nsIDOMEvent* aMouseEvent);
  nsresult Blur(nsIDOMEvent* aEvent);
  nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  nsresult Fullscreen(nsIDOMEvent* aEvent);

  static void InitAccessKey();

  static mozilla::Modifiers GetModifiersForAccessKey(nsIDOMKeyEvent* event);

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
  // Whether or not the ALT key is currently down.
  bool mAccessKeyDown;
  // Whether or not the ALT key down is canceled by other action.
  bool mAccessKeyDownCanceled;
  // Does the access key by itself focus the menubar?
  static bool mAccessKeyFocuses;
  // See nsIDOMKeyEvent.h for sample values.
  static int32_t mAccessKey;
  // Modifier mask for the access key.
  static mozilla::Modifiers mAccessKeyMask;
};

#endif // #ifndef nsMenuBarListener_h
