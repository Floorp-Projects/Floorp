/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMWheelEvent_h__
#define mozilla_dom_DOMWheelEvent_h__

#include "nsIDOMWheelEvent.h"
#include "nsDOMMouseEvent.h"
#include "mozilla/dom/WheelEventBinding.h"

namespace mozilla {
namespace dom {

class DOMWheelEvent : public nsDOMMouseEvent,
                      public nsIDOMWheelEvent
{
public:
  DOMWheelEvent(mozilla::dom::EventTarget* aOwner,
                nsPresContext* aPresContext,
                widget::WheelEvent* aWheelEvent);
  virtual ~DOMWheelEvent();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMWheelEvent Interface
  NS_DECL_NSIDOMWHEELEVENT
  
  // Forward to base class
  NS_FORWARD_TO_NSDOMMOUSEEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, JS::Value* aVal);

  static
  already_AddRefed<DOMWheelEvent> Constructor(const GlobalObject& aGlobal,
                                              const nsAString& aType,
                                              const WheelEventInit& aParam,
                                              mozilla::ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope)
  {
    return mozilla::dom::WheelEventBinding::Wrap(aCx, aScope, this);
  }

  double DeltaX()
  {
    return static_cast<widget::WheelEvent*>(mEvent)->deltaX;
  }

  double DeltaY()
  {
    return static_cast<widget::WheelEvent*>(mEvent)->deltaY;
  }

  double DeltaZ()
  {
    return static_cast<widget::WheelEvent*>(mEvent)->deltaZ;
  }

  uint32_t DeltaMode()
  {
    return static_cast<widget::WheelEvent*>(mEvent)->deltaMode;
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DOMWheelEvent_h__
