/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLPrototypeHandler_h__
#define nsXBLPrototypeHandler_h__

#include "mozilla/EventForwards.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIController.h"
#include "nsAutoPtr.h"
#include "nsXBLEventHandler.h"
#include "nsIWeakReference.h"
#include "nsCycleCollectionParticipant.h"
#include "js/TypeDecls.h"

class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIDOMKeyEvent;
class nsIDOMMouseEvent;
class nsIObjectInputStream;
class nsIObjectOutputStream;
class nsXBLPrototypeBinding;

namespace mozilla {

struct IgnoreModifierState;

namespace dom {
class AutoJSAPI;
class EventTarget;
} // namespace dom

namespace layers {
class KeyboardShortcut;
} // namespace layers

} // namespace mozilla

#define NS_HANDLER_TYPE_XBL_JS              (1 << 0)
#define NS_HANDLER_TYPE_XBL_COMMAND         (1 << 1)
#define NS_HANDLER_TYPE_XUL                 (1 << 2)
#define NS_HANDLER_HAS_ALLOW_UNTRUSTED_ATTR (1 << 4)
#define NS_HANDLER_ALLOW_UNTRUSTED          (1 << 5)
#define NS_HANDLER_TYPE_SYSTEM              (1 << 6)
#define NS_HANDLER_TYPE_PREVENTDEFAULT      (1 << 7)

// XXX Use nsIDOMEvent:: codes?
#define NS_PHASE_CAPTURING          1
#define NS_PHASE_TARGET             2
#define NS_PHASE_BUBBLING           3

class nsXBLPrototypeHandler
{
  typedef mozilla::IgnoreModifierState IgnoreModifierState;
  typedef mozilla::layers::KeyboardShortcut KeyboardShortcut;
  typedef mozilla::Modifiers Modifiers;

public:
  // This constructor is used by XBL handlers (both the JS and command shorthand variety)
  nsXBLPrototypeHandler(const char16_t* aEvent, const char16_t* aPhase,
                        const char16_t* aAction, const char16_t* aCommand,
                        const char16_t* aKeyCode, const char16_t* aCharCode,
                        const char16_t* aModifiers, const char16_t* aButton,
                        const char16_t* aClickCount, const char16_t* aGroup,
                        const char16_t* aPreventDefault,
                        const char16_t* aAllowUntrusted,
                        nsXBLPrototypeBinding* aBinding,
                        uint32_t aLineNumber);

  // This constructor is used only by XUL key handlers (e.g., <key>)
  explicit nsXBLPrototypeHandler(nsIContent* aKeyElement, bool aReserved);

  // This constructor is used for handlers loaded from the cache
  explicit nsXBLPrototypeHandler(nsXBLPrototypeBinding* aBinding);

  ~nsXBLPrototypeHandler();

  /**
   * Try and convert this XBL handler into an APZ KeyboardShortcut for handling
   * key events on the compositor thread. This only works for XBL handlers that
   * represent scroll commands.
   *
   * @param aOut the converted KeyboardShortcut, must be non null
   * @return whether the handler was converted into a KeyboardShortcut
   */
  bool TryConvertToKeyboardShortcut(
          KeyboardShortcut* aOut) const;

  bool EventTypeEquals(nsIAtom* aEventType) const
  {
    return mEventName == aEventType;
  }

  // if aCharCode is not zero, it is used instead of the charCode of aKeyEvent.
  bool KeyEventMatched(nsIDOMKeyEvent* aKeyEvent,
                       uint32_t aCharCode,
                       const IgnoreModifierState& aIgnoreModifierState);

  bool MouseEventMatched(nsIDOMMouseEvent* aMouseEvent);
  inline bool MouseEventMatched(nsIAtom* aEventType,
                                  nsIDOMMouseEvent* aEvent)
  {
    if (!EventTypeEquals(aEventType)) {
      return false;
    }
    return MouseEventMatched(aEvent);
  }

  already_AddRefed<nsIContent> GetHandlerElement();

  void AppendHandlerText(const nsAString& aText);

  uint8_t GetPhase() { return mPhase; }
  uint8_t GetType() { return mType; }
  bool GetIsReserved() { return mReserved; }

  nsXBLPrototypeHandler* GetNextHandler() { return mNextHandler; }
  void SetNextHandler(nsXBLPrototypeHandler* aHandler) { mNextHandler = aHandler; }

  nsresult ExecuteHandler(mozilla::dom::EventTarget* aTarget, nsIDOMEvent* aEvent);

  already_AddRefed<nsIAtom> GetEventName();
  void SetEventName(nsIAtom* aName) { mEventName = aName; }

  nsXBLEventHandler* GetEventHandler()
  {
    if (!mHandler) {
      mHandler = NS_NewXBLEventHandler(this, mEventName);
    }

    return mHandler;
  }

  nsXBLEventHandler* GetCachedEventHandler()
  {
    return mHandler;
  }

  bool HasAllowUntrustedAttr()
  {
    return (mType & NS_HANDLER_HAS_ALLOW_UNTRUSTED_ATTR) != 0;
  }

  // This returns a valid value only if HasAllowUntrustedEventsAttr returns
  // true.
  bool AllowUntrustedEvents()
  {
    return (mType & NS_HANDLER_ALLOW_UNTRUSTED) != 0;
  }

  nsresult Read(nsIObjectInputStream* aStream);
  nsresult Write(nsIObjectOutputStream* aStream);

public:
  static uint32_t gRefCnt;

protected:
  void Init() {
    ++gRefCnt;
    if (gRefCnt == 1)
      // Get the primary accelerator key.
      InitAccessKeys();
  }

  already_AddRefed<nsIController> GetController(mozilla::dom::EventTarget* aTarget);

  inline int32_t GetMatchingKeyCode(const nsAString& aKeyName);
  void ConstructPrototype(nsIContent* aKeyElement,
                          const char16_t* aEvent=nullptr, const char16_t* aPhase=nullptr,
                          const char16_t* aAction=nullptr, const char16_t* aCommand=nullptr,
                          const char16_t* aKeyCode=nullptr, const char16_t* aCharCode=nullptr,
                          const char16_t* aModifiers=nullptr, const char16_t* aButton=nullptr,
                          const char16_t* aClickCount=nullptr, const char16_t* aGroup=nullptr,
                          const char16_t* aPreventDefault=nullptr,
                          const char16_t* aAllowUntrusted=nullptr);

  void ReportKeyConflict(const char16_t* aKey, const char16_t* aModifiers, nsIContent* aElement, const char *aMessageName);
  void GetEventType(nsAString& type);
  bool ModifiersMatchMask(nsIDOMUIEvent* aEvent,
                          const IgnoreModifierState& aIgnoreModifierState);
  nsresult DispatchXBLCommand(mozilla::dom::EventTarget* aTarget, nsIDOMEvent* aEvent);
  nsresult DispatchXULKeyCommand(nsIDOMEvent* aEvent);
  nsresult EnsureEventHandler(mozilla::dom::AutoJSAPI& jsapi, nsIAtom* aName,
                              JS::MutableHandle<JSObject*> aHandler);

  Modifiers GetModifiers() const;
  Modifiers GetModifiersMask() const;

  static int32_t KeyToMask(int32_t key);
  static int32_t AccelKeyMask();

  static int32_t kMenuAccessKey;
  static void InitAccessKeys();

  static const int32_t cShift;
  static const int32_t cAlt;
  static const int32_t cControl;
  static const int32_t cMeta;
  static const int32_t cOS;

  static const int32_t cShiftMask;
  static const int32_t cAltMask;
  static const int32_t cControlMask;
  static const int32_t cMetaMask;
  static const int32_t cOSMask;

  static const int32_t cAllModifiers;

protected:
  union {
    nsIWeakReference* mHandlerElement;  // For XUL <key> element handlers. [STRONG]
    char16_t*        mHandlerText;     // For XBL handlers (we don't build an
                                        // element for the <handler>, and instead
                                        // we cache the JS text or command name
                                        // that we should use.
  };

  uint32_t mLineNumber;  // The line number we started at in the XBL file

  // The following four values make up 32 bits.
  uint8_t mPhase;            // The phase (capturing, bubbling)
  uint8_t mType;             // The type of the handler.  The handler is either a XUL key
                             // handler, an XBL "command" event, or a normal XBL event with
                             // accompanying JavaScript.  The high bit is used to indicate
                             // whether this handler should prevent the default action.
  uint8_t mMisc;             // Miscellaneous extra information.  For key events,
                             // stores whether or not we're a key code or char code.
                             // For mouse events, stores the clickCount.

  bool mReserved;            // <key> is reserved for chrome. Not used by handlers.

  int32_t mKeyMask;          // Which modifier keys this event handler expects to have down
                             // in order to be matched.

  // The primary filter information for mouse/key events.
  int32_t mDetail;           // For key events, contains a charcode or keycode. For
                             // mouse events, stores the button info.

  // Prototype handlers are chained. We own the next handler in the chain.
  nsXBLPrototypeHandler* mNextHandler;
  nsCOMPtr<nsIAtom> mEventName; // The type of the event, e.g., "keypress"
  RefPtr<nsXBLEventHandler> mHandler;
  nsXBLPrototypeBinding* mPrototypeBinding; // the binding owns us
};

#endif
