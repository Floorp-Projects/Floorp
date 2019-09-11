/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WheelEvent_h_
#define mozilla_dom_WheelEvent_h_

#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/WheelEventBinding.h"
#include "mozilla/EventForwards.h"

namespace mozilla {
namespace dom {

class WheelEvent : public MouseEvent {
 public:
  WheelEvent(EventTarget* aOwner, nsPresContext* aPresContext,
             WidgetWheelEvent* aWheelEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(WheelEvent, MouseEvent)

  static already_AddRefed<WheelEvent> Constructor(const GlobalObject& aGlobal,
                                                  const nsAString& aType,
                                                  const WheelEventInit& aParam);

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override {
    return WheelEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  // NOTE: DeltaX(), DeltaY() and DeltaZ() return CSS pixels when deltaMode is
  //       DOM_DELTA_PIXEL. (The internal event's delta values are device pixels
  //       if it's dispatched by widget)
  double DeltaX();
  double DeltaY();
  double DeltaZ();
  uint32_t DeltaMode();

  void InitWheelEvent(const nsAString& aType, bool aCanBubble, bool aCancelable,
                      nsGlobalWindowInner* aView, int32_t aDetail,
                      int32_t aScreenX, int32_t aScreenY, int32_t aClientX,
                      int32_t aClientY, uint16_t aButton,
                      EventTarget* aRelatedTarget,
                      const nsAString& aModifiersList, double aDeltaX,
                      double aDeltaY, double aDeltaZ, uint32_t aDeltaMode);

 protected:
  ~WheelEvent() {}

 private:
  int32_t mAppUnitsPerDevPixel;
};

}  // namespace dom
}  // namespace mozilla

already_AddRefed<mozilla::dom::WheelEvent> NS_NewDOMWheelEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::WidgetWheelEvent* aEvent);

#endif  // mozilla_dom_WheelEvent_h_
