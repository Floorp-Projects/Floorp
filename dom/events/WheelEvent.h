/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WheelEvent_h_
#define mozilla_dom_WheelEvent_h_

#include "nsIDOMWheelEvent.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/WheelEventBinding.h"
#include "mozilla/EventForwards.h"

namespace mozilla {
namespace dom {

class WheelEvent : public MouseEvent,
                   public nsIDOMWheelEvent
{
public:
  WheelEvent(EventTarget* aOwner,
             nsPresContext* aPresContext,
             WidgetWheelEvent* aWheelEvent);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMWheelEvent Interface
  NS_DECL_NSIDOMWHEELEVENT
  
  // Forward to base class
  NS_FORWARD_TO_MOUSEEVENT

  static
  already_AddRefed<WheelEvent> Constructor(const GlobalObject& aGlobal,
                                           const nsAString& aType,
                                           const WheelEventInit& aParam,
                                           ErrorResult& aRv);

  virtual JSObject* WrapObjectInternal(JSContext* aCx) MOZ_OVERRIDE
  {
    return WheelEventBinding::Wrap(aCx, this);
  }

  // NOTE: DeltaX(), DeltaY() and DeltaZ() return CSS pixels when deltaMode is
  //       DOM_DELTA_PIXEL. (The internal event's delta values are device pixels
  //       if it's dispatched by widget)
  double DeltaX();
  double DeltaY();
  double DeltaZ();
  uint32_t DeltaMode();

protected:
  ~WheelEvent() {}

private:
  int32_t mAppUnitsPerDevPixel;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WheelEvent_h_
