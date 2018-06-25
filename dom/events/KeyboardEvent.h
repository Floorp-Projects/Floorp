/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_KeyboardEvent_h_
#define mozilla_dom_KeyboardEvent_h_

#include "mozilla/dom/UIEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/EventForwards.h"
#include "nsRFPService.h"

namespace mozilla {
namespace dom {

class KeyboardEvent : public UIEvent
{
public:
  KeyboardEvent(EventTarget* aOwner,
                nsPresContext* aPresContext,
                WidgetKeyboardEvent* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(KeyboardEvent, UIEvent)

  virtual KeyboardEvent* AsKeyboardEvent() override
  {
    return this;
  }

  static already_AddRefed<KeyboardEvent> Constructor(
                                           const GlobalObject& aGlobal,
                                           const nsAString& aType,
                                           const KeyboardEventInit& aParam,
                                           ErrorResult& aRv);

  virtual JSObject*
    WrapObjectInternal(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override
  {
    return KeyboardEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  bool AltKey(CallerType aCallerType = CallerType::System);
  bool CtrlKey(CallerType aCallerType = CallerType::System);
  bool ShiftKey(CallerType aCallerType = CallerType::System);
  bool MetaKey();

  bool GetModifierState(const nsAString& aKey,
                        CallerType aCallerType = CallerType::System)
  {
    bool modifierState = GetModifierStateInternal(aKey);

    if (!ShouldResistFingerprinting(aCallerType)) {
      return modifierState;
    }

    Modifiers modifier = WidgetInputEvent::GetModifier(aKey);
    return GetSpoofedModifierStates(modifier, modifierState);
  }

  bool Repeat();
  bool IsComposing();
  void GetKey(nsAString& aKey) const;
  uint32_t CharCode();
  uint32_t KeyCode(CallerType aCallerType = CallerType::System);
  virtual uint32_t Which(CallerType aCallerType = CallerType::System) override;
  uint32_t Location();

  void GetCode(nsAString& aCode, CallerType aCallerType = CallerType::System);
  void GetInitDict(KeyboardEventInit& aParam);

  void InitKeyEvent(const nsAString& aType, bool aCanBubble, bool aCancelable,
                    nsGlobalWindowInner* aView, bool aCtrlKey, bool aAltKey,
                    bool aShiftKey, bool aMetaKey,
                    uint32_t aKeyCode, uint32_t aCharCode);

  void InitKeyboardEvent(const nsAString& aType,
                         bool aCanBubble, bool aCancelable,
                         nsGlobalWindowInner* aView, const nsAString& aKey,
                         uint32_t aLocation, bool aCtrlKey, bool aAltKey,
                         bool aShiftKey, bool aMetaKey, ErrorResult& aRv);

protected:
  ~KeyboardEvent() {}

  void InitWithKeyboardEventInit(EventTarget* aOwner,
                                 const nsAString& aType,
                                 const KeyboardEventInit& aParam,
                                 ErrorResult& aRv);

private:
  // True, if the instance is created with Constructor().
  bool mInitializedByCtor;

  // If the instance is created with Constructor(), which may have independent
  // value.  mInitializedWhichValue stores it.  I.e., this is invalid when
  // mInitializedByCtor is false.
  uint32_t mInitializedWhichValue;

  // This method returns the boolean to indicate whether spoofing keyboard
  // event for fingerprinting resistance. It will return true when pref
  // 'privacy.resistFingerprinting' is true and the event target is content.
  // Otherwise, it will return false.
  bool ShouldResistFingerprinting(CallerType aCallerType);

  // This method returns the nsIDocument which is associated with the event
  // target.
  already_AddRefed<nsIDocument> GetDocument();

  // This method returns the spoofed modifier state of the given modifier key
  // for fingerprinting resistance.
  bool GetSpoofedModifierStates(const Modifiers aModifierKey,
                                const bool aRawModifierState);
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::KeyboardEvent>
NS_NewDOMKeyboardEvent(mozilla::dom::EventTarget* aOwner,
                       nsPresContext* aPresContext,
                       mozilla::WidgetKeyboardEvent* aEvent);

#endif // mozilla_dom_KeyboardEvent_h_
