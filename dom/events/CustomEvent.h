/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CustomEvent_h__
#define CustomEvent_h__

#include "mozilla/dom/Event.h"

namespace mozilla {
namespace dom {

struct CustomEventInit;

class CustomEvent final : public Event {
 private:
  virtual ~CustomEvent();

  JS::Heap<JS::Value> mDetail;

 public:
  explicit CustomEvent(mozilla::dom::EventTarget* aOwner,
                       nsPresContext* aPresContext = nullptr,
                       mozilla::WidgetEvent* aEvent = nullptr);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(CustomEvent, Event)

  static already_AddRefed<CustomEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const CustomEventInit& aParam);

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  CustomEvent* AsCustomEvent() override { return this; }

  void GetDetail(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval);

  void InitCustomEvent(JSContext* aCx, const nsAString& aType, bool aCanBubble,
                       bool aCancelable, JS::Handle<JS::Value> aDetail);
};

}  // namespace dom
}  // namespace mozilla

already_AddRefed<mozilla::dom::CustomEvent> NS_NewDOMCustomEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::WidgetEvent* aEvent);

#endif  // CustomEvent_h__
