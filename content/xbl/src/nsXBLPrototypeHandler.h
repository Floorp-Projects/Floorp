/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLPrototypeHandler_h__
#define nsXBLPrototypeHandler_h__

#include "nsIAtom.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIController.h"
#include "nsAutoPtr.h"
#include "nsXBLEventHandler.h"
#include "nsIWeakReference.h"
#include "nsIScriptGlobalObject.h"
#include "nsDOMScriptObjectHolder.h"
#include "nsCycleCollectionParticipant.h"

class nsIDOMEvent;
class nsIContent;
class nsIDOMUIEvent;
class nsIDOMKeyEvent;
class nsIDOMMouseEvent;
class nsIDOMEventTarget;
class nsXBLPrototypeBinding;

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
public:
  // This constructor is used by XBL handlers (both the JS and command shorthand variety)
  nsXBLPrototypeHandler(const PRUnichar* aEvent, const PRUnichar* aPhase,
                        const PRUnichar* aAction, const PRUnichar* aCommand,
                        const PRUnichar* aKeyCode, const PRUnichar* aCharCode,
                        const PRUnichar* aModifiers, const PRUnichar* aButton,
                        const PRUnichar* aClickCount, const PRUnichar* aGroup,
                        const PRUnichar* aPreventDefault,
                        const PRUnichar* aAllowUntrusted,
                        nsXBLPrototypeBinding* aBinding,
                        PRUint32 aLineNumber);

  // This constructor is used only by XUL key handlers (e.g., <key>)
  nsXBLPrototypeHandler(nsIContent* aKeyElement);

  // This constructor is used for handlers loaded from the cache
  nsXBLPrototypeHandler(nsXBLPrototypeBinding* aBinding);

  ~nsXBLPrototypeHandler();

  // if aCharCode is not zero, it is used instead of the charCode of aKeyEvent.
  bool KeyEventMatched(nsIDOMKeyEvent* aKeyEvent,
                         PRUint32 aCharCode = 0,
                         bool aIgnoreShiftKey = false);
  inline bool KeyEventMatched(nsIAtom* aEventType,
                                nsIDOMKeyEvent* aEvent,
                                PRUint32 aCharCode = 0,
                                bool aIgnoreShiftKey = false)
  {
    if (aEventType != mEventName)
      return false;

    return KeyEventMatched(aEvent, aCharCode, aIgnoreShiftKey);
  }

  bool MouseEventMatched(nsIDOMMouseEvent* aMouseEvent);
  inline bool MouseEventMatched(nsIAtom* aEventType,
                                  nsIDOMMouseEvent* aEvent)
  {
    if (aEventType != mEventName)
      return false;

    return MouseEventMatched(aEvent);
  }

  already_AddRefed<nsIContent> GetHandlerElement();

  void AppendHandlerText(const nsAString& aText);

  PRUint8 GetPhase() { return mPhase; }
  PRUint8 GetType() { return mType; }

  nsXBLPrototypeHandler* GetNextHandler() { return mNextHandler; }
  void SetNextHandler(nsXBLPrototypeHandler* aHandler) { mNextHandler = aHandler; }

  nsresult ExecuteHandler(nsIDOMEventTarget* aTarget, nsIDOMEvent* aEvent);

  already_AddRefed<nsIAtom> GetEventName();
  void SetEventName(nsIAtom* aName) { mEventName = aName; }

  nsXBLEventHandler* GetEventHandler()
  {
    if (!mHandler) {
      NS_NewXBLEventHandler(this, mEventName, getter_AddRefs(mHandler));
      // XXX Need to signal out of memory?
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

  nsresult Read(nsIScriptContext* aContext, nsIObjectInputStream* aStream);
  nsresult Write(nsIScriptContext* aContext, nsIObjectOutputStream* aStream);

public:
  static PRUint32 gRefCnt;
  
protected:
  void Init() {
    ++gRefCnt;
    if (gRefCnt == 1)
      // Get the primary accelerator key.
      InitAccessKeys();
  }

  already_AddRefed<nsIController> GetController(nsIDOMEventTarget* aTarget);
  
  inline PRInt32 GetMatchingKeyCode(const nsAString& aKeyName);
  void ConstructPrototype(nsIContent* aKeyElement, 
                          const PRUnichar* aEvent=nullptr, const PRUnichar* aPhase=nullptr,
                          const PRUnichar* aAction=nullptr, const PRUnichar* aCommand=nullptr,
                          const PRUnichar* aKeyCode=nullptr, const PRUnichar* aCharCode=nullptr,
                          const PRUnichar* aModifiers=nullptr, const PRUnichar* aButton=nullptr,
                          const PRUnichar* aClickCount=nullptr, const PRUnichar* aGroup=nullptr,
                          const PRUnichar* aPreventDefault=nullptr,
                          const PRUnichar* aAllowUntrusted=nullptr);

  void ReportKeyConflict(const PRUnichar* aKey, const PRUnichar* aModifiers, nsIContent* aElement, const char *aMessageName);
  void GetEventType(nsAString& type);
  bool ModifiersMatchMask(nsIDOMUIEvent* aEvent,
                            bool aIgnoreShiftKey = false);
  nsresult DispatchXBLCommand(nsIDOMEventTarget* aTarget, nsIDOMEvent* aEvent);
  nsresult DispatchXULKeyCommand(nsIDOMEvent* aEvent);
  nsresult EnsureEventHandler(nsIScriptGlobalObject* aGlobal,
                              nsIScriptContext *aBoundContext, nsIAtom *aName,
                              nsScriptObjectHolder<JSObject>& aHandler);
  static PRInt32 KeyToMask(PRInt32 key);
  
  static PRInt32 kAccelKey;
  static PRInt32 kMenuAccessKey;
  static void InitAccessKeys();

  static const PRInt32 cShift;
  static const PRInt32 cAlt;
  static const PRInt32 cControl;
  static const PRInt32 cMeta;
  static const PRInt32 cOS;

  static const PRInt32 cShiftMask;
  static const PRInt32 cAltMask;
  static const PRInt32 cControlMask;
  static const PRInt32 cMetaMask;
  static const PRInt32 cOSMask;

  static const PRInt32 cAllModifiers;

protected:
  union {
    nsIWeakReference* mHandlerElement;  // For XUL <key> element handlers. [STRONG]
    PRUnichar*        mHandlerText;     // For XBL handlers (we don't build an
                                        // element for the <handler>, and instead
                                        // we cache the JS text or command name
                                        // that we should use.
  };

  PRUint32 mLineNumber;  // The line number we started at in the XBL file
  
  // The following four values make up 32 bits.
  PRUint8 mPhase;            // The phase (capturing, bubbling)
  PRUint8 mType;             // The type of the handler.  The handler is either a XUL key
                             // handler, an XBL "command" event, or a normal XBL event with
                             // accompanying JavaScript.  The high bit is used to indicate
                             // whether this handler should prevent the default action.
  PRUint8 mMisc;             // Miscellaneous extra information.  For key events,
                             // stores whether or not we're a key code or char code.
                             // For mouse events, stores the clickCount.

  PRInt32 mKeyMask;          // Which modifier keys this event handler expects to have down
                             // in order to be matched.
 
  // The primary filter information for mouse/key events.
  PRInt32 mDetail;           // For key events, contains a charcode or keycode. For
                             // mouse events, stores the button info.

  // Prototype handlers are chained. We own the next handler in the chain.
  nsXBLPrototypeHandler* mNextHandler;
  nsCOMPtr<nsIAtom> mEventName; // The type of the event, e.g., "keypress"
  nsRefPtr<nsXBLEventHandler> mHandler;
  nsXBLPrototypeBinding* mPrototypeBinding; // the binding owns us
};

#endif
