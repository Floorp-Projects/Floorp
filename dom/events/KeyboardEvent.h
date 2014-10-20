/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_KeyboardEvent_h_
#define mozilla_dom_KeyboardEvent_h_

#include "mozilla/dom/UIEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/EventForwards.h"
#include "nsIDOMKeyEvent.h"

namespace mozilla {
namespace dom {

class KeyboardEvent : public UIEvent,
                      public nsIDOMKeyEvent
{
public:
  KeyboardEvent(EventTarget* aOwner,
                nsPresContext* aPresContext,
                WidgetKeyboardEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMKeyEvent Interface
  NS_DECL_NSIDOMKEYEVENT

  // Forward to base class
  NS_FORWARD_TO_UIEVENT

  static already_AddRefed<KeyboardEvent> Constructor(
                                           const GlobalObject& aGlobal,
                                           const nsAString& aType,
                                           const KeyboardEventInit& aParam,
                                           ErrorResult& aRv);

  virtual JSObject* WrapObjectInternal(JSContext* aCx) MOZ_OVERRIDE
  {
    return KeyboardEventBinding::Wrap(aCx, this);
  }

  bool AltKey();
  bool CtrlKey();
  bool ShiftKey();
  bool MetaKey();

  bool GetModifierState(const nsAString& aKey)
  {
    return GetModifierStateInternal(aKey);
  }

  bool Repeat();
  bool IsComposing();
  uint32_t CharCode();
  uint32_t KeyCode();
  virtual uint32_t Which() MOZ_OVERRIDE;
  uint32_t Location();

  void GetCode(nsAString& aCode);

  void InitKeyEvent(const nsAString& aType, bool aCanBubble, bool aCancelable,
                    nsIDOMWindow* aView, bool aCtrlKey, bool aAltKey,
                    bool aShiftKey, bool aMetaKey,
                    uint32_t aKeyCode, uint32_t aCharCode,
                    ErrorResult& aRv)
  {
    aRv = InitKeyEvent(aType, aCanBubble, aCancelable, aView,
                       aCtrlKey, aAltKey, aShiftKey,aMetaKey,
                       aKeyCode, aCharCode);
  }

protected:
  ~KeyboardEvent() {}

private:
  // True, if the instance is created with Constructor().
  bool mInitializedByCtor;
  // If the instance is created with Constructor(), which may have independent
  // value.  mInitializedWhichValue stores it.  I.e., this is invalid when
  // mInitializedByCtor is false.
  uint32_t mInitialzedWhichValue;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_KeyboardEvent_h_
