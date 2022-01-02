/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GlobalKeyListener_h_
#define mozilla_GlobalKeyListener_h_

#include "mozilla/EventForwards.h"
#include "mozilla/layers/KeyboardMap.h"
#include "nsIDOMEventListener.h"
#include "nsIWeakReferenceUtils.h"

class nsAtom;

namespace mozilla {
class EventListenerManager;
class WidgetKeyboardEvent;
struct IgnoreModifierState;

namespace layers {
class KeyboardMap;
}

namespace dom {
class Element;
class EventTarget;
class KeyboardEvent;
}  // namespace dom

class KeyEventHandler;

/**
 * A generic listener for key events.
 *
 * Maintains a list of shortcut handlers and is registered as a listener for DOM
 * key events from a target. Responsible for executing the appropriate handler
 * when a keyboard event is received.
 */
class GlobalKeyListener : public nsIDOMEventListener {
 public:
  explicit GlobalKeyListener(dom::EventTarget* aTarget);

  void InstallKeyboardEventListenersTo(
      EventListenerManager* aEventListenerManager);
  void RemoveKeyboardEventListenersFrom(
      EventListenerManager* aEventListenerManager);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

 protected:
  virtual ~GlobalKeyListener() = default;

  MOZ_CAN_RUN_SCRIPT
  void WalkHandlers(dom::KeyboardEvent* aKeyEvent);

  // walk the handlers, looking for one to handle the event
  MOZ_CAN_RUN_SCRIPT
  bool WalkHandlersInternal(dom::KeyboardEvent* aKeyEvent, bool aExecute,
                            bool* aOutReservedForChrome = nullptr);

  // walk the handlers for aEvent, aCharCode and aIgnoreModifierState. Execute
  // it if aExecute = true.
  MOZ_CAN_RUN_SCRIPT
  bool WalkHandlersAndExecute(dom::KeyboardEvent* aKeyEvent, uint32_t aCharCode,
                              const IgnoreModifierState& aIgnoreModifierState,
                              bool aExecute,
                              bool* aOutReservedForChrome = nullptr);

  // HandleEvent function for the capturing phase in the default event group.
  MOZ_CAN_RUN_SCRIPT
  void HandleEventOnCaptureInDefaultEventGroup(dom::KeyboardEvent* aEvent);
  // HandleEvent function for the capturing phase in the system event group.
  MOZ_CAN_RUN_SCRIPT
  void HandleEventOnCaptureInSystemEventGroup(dom::KeyboardEvent* aEvent);

  // Check if any handler would handle the given event. Optionally returns
  // whether the command handler for the event is marked with the "reserved"
  // attribute.
  MOZ_CAN_RUN_SCRIPT
  bool HasHandlerForEvent(dom::KeyboardEvent* aEvent,
                          bool* aOutReservedForChrome = nullptr);

  // Returns true if the key would be reserved for the given handler. A reserved
  // key is not sent to a content process or single-process equivalent.
  bool IsReservedKey(WidgetKeyboardEvent* aKeyEvent, KeyEventHandler* aHandler);

  // lazily load the handlers. Overridden to handle being attached
  // to a particular element rather than the document
  virtual void EnsureHandlers() = 0;

  virtual bool CanHandle(KeyEventHandler* aHandler, bool aWillExecute) const {
    return true;
  }

  virtual bool IsDisabled() const { return false; }

  virtual already_AddRefed<dom::EventTarget> GetHandlerTarget(
      KeyEventHandler* aHandler) {
    return do_AddRef(mTarget);
  }

  dom::EventTarget* mTarget;  // weak ref;

  KeyEventHandler* mHandler;  // Linked list of event handlers.
};

/**
 * A listener for shortcut keys defined in XUL keyset elements.
 *
 * Listens for keyboard events from the document object and triggers the
 * appropriate XUL key elements.
 */
class XULKeySetGlobalKeyListener final : public GlobalKeyListener {
 public:
  explicit XULKeySetGlobalKeyListener(dom::Element* aElement,
                                      dom::EventTarget* aTarget);

  static void AttachKeyHandler(dom::Element* aElementTarget);
  static void DetachKeyHandler(dom::Element* aElementTarget);

 protected:
  virtual ~XULKeySetGlobalKeyListener();

  // Returns the element which was passed as a parameter to the constructor,
  // unless the element has been removed from the document. Optionally returns
  // whether the disabled attribute is set on the element (assuming the element
  // is non-null).
  dom::Element* GetElement(bool* aIsDisabled = nullptr) const;

  virtual void EnsureHandlers() override;

  virtual bool CanHandle(KeyEventHandler* aHandler,
                         bool aWillExecute) const override;
  virtual bool IsDisabled() const override;
  virtual already_AddRefed<dom::EventTarget> GetHandlerTarget(
      KeyEventHandler* aHandler) override;

  /**
   * GetElementForHandler() retrieves an element for the handler.  The element
   * may be a command element or a key element.
   *
   * @param aHandler           The handler.
   * @param aElementForHandler Must not be nullptr.  The element is returned to
   *                           this.
   * @return                   true if the handler is valid.  Otherwise, false.
   */
  bool GetElementForHandler(KeyEventHandler* aHandler,
                            dom::Element** aElementForHandler) const;

  /**
   * IsExecutableElement() returns true if aElement is executable.
   * Otherwise, false. aElement should be a command element or a key element.
   */
  bool IsExecutableElement(dom::Element* aElement) const;

  // Using weak pointer to the DOM Element.
  nsWeakPtr mWeakPtrForElement;
};

/**
 * Listens for built-in shortcut keys.
 *
 * Listens to DOM keyboard events from the window or text input and runs the
 * built-in shortcuts (see dom/events/keyevents) as necessary.
 */
class RootWindowGlobalKeyListener final : public GlobalKeyListener {
 public:
  explicit RootWindowGlobalKeyListener(dom::EventTarget* aTarget);

  static void AttachKeyHandler(dom::EventTarget* aTarget);

  static layers::KeyboardMap CollectKeyboardShortcuts();

 protected:
  // Is an HTML editable element focused
  static bool IsHTMLEditorFocused();

  virtual void EnsureHandlers() override;
};

}  // namespace mozilla

#endif
