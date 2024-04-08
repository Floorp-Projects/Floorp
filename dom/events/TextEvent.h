/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextEvent_h
#define mozilla_dom_TextEvent_h

#include "mozilla/dom/UIEvent.h"

#include "mozilla/dom/TextEventBinding.h"
#include "mozilla/EventForwards.h"

class nsIPrincipal;

namespace mozilla::dom {

class TextEvent : public UIEvent {
 public:
  TextEvent(EventTarget* aOwner, nsPresContext* aPresContext,
            InternalLegacyTextEvent* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(TextEvent, UIEvent)

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override {
    return TextEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  void GetData(nsAString& aData, nsIPrincipal& aSubjectPrincipal) const;
  void InitTextEvent(const nsAString& typeArg, bool canBubbleArg,
                     bool cancelableArg, nsGlobalWindowInner* viewArg,
                     const nsAString& dataArg);

 protected:
  ~TextEvent() = default;
};

}  // namespace mozilla::dom

already_AddRefed<mozilla::dom::TextEvent> NS_NewDOMTextEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::InternalLegacyTextEvent* aEvent);

#endif  // mozilla_dom_InputEvent_h_
