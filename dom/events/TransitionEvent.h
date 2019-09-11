/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_TransitionEvent_h_
#define mozilla_dom_TransitionEvent_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/TransitionEventBinding.h"
#include "nsStringFwd.h"

namespace mozilla {
namespace dom {

class TransitionEvent : public Event {
 public:
  TransitionEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                  InternalTransitionEvent* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(TransitionEvent, Event)

  static already_AddRefed<TransitionEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const TransitionEventInit& aParam);

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override {
    return TransitionEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  void GetPropertyName(nsAString& aPropertyName) const;
  void GetPseudoElement(nsAString& aPreudoElement) const;

  float ElapsedTime();

 protected:
  ~TransitionEvent() {}
};

}  // namespace dom
}  // namespace mozilla

already_AddRefed<mozilla::dom::TransitionEvent> NS_NewDOMTransitionEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::InternalTransitionEvent* aEvent);

#endif  // mozilla_dom_TransitionEvent_h_
