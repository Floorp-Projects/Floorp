/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_KeyEventHandler_h_
#define mozilla_KeyEventHandler_h_

#include "mozilla/EventForwards.h"
#include "mozilla/MemoryReporting.h"
#include "nsAtom.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIController.h"
#include "nsIWeakReference.h"
#include "nsCycleCollectionParticipant.h"
#include "js/TypeDecls.h"
#include "mozilla/ShortcutKeys.h"

namespace mozilla {

namespace layers {
class KeyboardShortcut;
}  // namespace layers

struct IgnoreModifierState;

namespace dom {
class Event;
class UIEvent;
class Element;
class EventTarget;
class KeyboardEvent;
class Element;
}  // namespace dom

// Values of the reserved attribute. When unset, the default value depends on
// the permissions.default.shortcuts preference.
enum ReservedKey : uint8_t {
  ReservedKey_False = 0,
  ReservedKey_True = 1,
  ReservedKey_Unset = 2,
};

class KeyEventHandler final {
 public:
  // This constructor is used only by XUL key handlers (e.g., <key>)
  explicit KeyEventHandler(dom::Element* aHandlerElement,
                           ReservedKey aReserved);

  // This constructor is used for keyboard handlers for browser, editor, input
  // and textarea elements.
  explicit KeyEventHandler(ShortcutKeyData* aKeyData);

  ~KeyEventHandler();

  /**
   * Try and convert this XBL handler into an APZ KeyboardShortcut for handling
   * key events on the compositor thread. This only works for XBL handlers that
   * represent scroll commands.
   *
   * @param aOut the converted KeyboardShortcut, must be non null
   * @return whether the handler was converted into a KeyboardShortcut
   */
  bool TryConvertToKeyboardShortcut(layers::KeyboardShortcut* aOut) const;

  bool EventTypeEquals(nsAtom* aEventType) const {
    return mEventName == aEventType;
  }

  // if aCharCode is not zero, it is used instead of the charCode of
  // aKeyEventHandler.
  bool KeyEventMatched(dom::KeyboardEvent* aDomKeyboardEvent,
                       uint32_t aCharCode,
                       const IgnoreModifierState& aIgnoreModifierState);

  /**
   * Check whether the handler element is disabled.  Note that this requires
   * a QI to getting GetHandlerELement().  Therefore, this should not be used
   * first in multiple checks.
   */
  bool KeyElementIsDisabled() const;

  already_AddRefed<dom::Element> GetHandlerElement() const;

  ReservedKey GetIsReserved() { return mReserved; }

  KeyEventHandler* GetNextHandler() { return mNextHandler; }
  void SetNextHandler(KeyEventHandler* aHandler) { mNextHandler = aHandler; }

  MOZ_CAN_RUN_SCRIPT
  nsresult ExecuteHandler(dom::EventTarget* aTarget, dom::Event* aEvent);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  void GetCommand(nsAString& aCommand) const;

 public:
  static uint32_t gRefCnt;

 protected:
  void Init() { ++gRefCnt; }

  already_AddRefed<nsIController> GetController(dom::EventTarget* aTarget);

  inline int32_t GetMatchingKeyCode(const nsAString& aKeyName);
  void ConstructPrototype(dom::Element* aKeyElement,
                          const char16_t* aEvent = nullptr,
                          const char16_t* aCommand = nullptr,
                          const char16_t* aKeyCode = nullptr,
                          const char16_t* aCharCode = nullptr,
                          const char16_t* aModifiers = nullptr);
  void BuildModifiers(nsAString& aModifiers);

  void ReportKeyConflict(const char16_t* aKey, const char16_t* aModifiers,
                         dom::Element* aKeyElement, const char* aMessageName);
  void GetEventType(nsAString& aEvent);
  bool ModifiersMatchMask(dom::UIEvent* aEvent,
                          const IgnoreModifierState& aIgnoreModifierState);
  MOZ_CAN_RUN_SCRIPT
  nsresult DispatchXBLCommand(dom::EventTarget* aTarget, dom::Event* aEvent);
  MOZ_CAN_RUN_SCRIPT
  nsresult DispatchXULKeyCommand(dom::Event* aEvent);

  Modifiers GetModifiers() const;
  Modifiers GetModifiersMask() const;

  static int32_t KeyToMask(uint32_t aKey);
  static int32_t AccelKeyMask();

  static const int32_t cShift;
  static const int32_t cAlt;
  static const int32_t cControl;
  static const int32_t cMeta;

  static const int32_t cShiftMask;
  static const int32_t cAltMask;
  static const int32_t cControlMask;
  static const int32_t cMetaMask;

  static const int32_t cAllModifiers;

 protected:
  union {
    nsIWeakReference*
        mHandlerElement;  // For XUL <key> element handlers. [STRONG]
    char16_t* mCommand;   // For built-in shortcuts the command to execute.
  };

  // The following four values make up 32 bits.
  bool mIsXULKey;  // This handler is either for a XUL <key> element or it is
                   // a command dispatcher.
  uint8_t mMisc;   // Miscellaneous extra information.  For key events,
                   // stores whether or not we're a key code or char code.
                   // For mouse events, stores the clickCount.

  ReservedKey mReserved;  // <key> is reserved for chrome. Not used by handlers.

  int32_t mKeyMask;  // Which modifier keys this event handler expects to have
                     // down in order to be matched.

  // The primary filter information for mouse/key events.
  int32_t mDetail;  // For key events, contains a charcode or keycode. For
                    // mouse events, stores the button info.

  // Prototype handlers are chained. We own the next handler in the chain.
  KeyEventHandler* mNextHandler;
  RefPtr<nsAtom> mEventName;  // The type of the event, e.g., "keypress"
};

}  // namespace mozilla

#endif
