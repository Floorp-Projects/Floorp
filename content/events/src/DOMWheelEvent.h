/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMWheelEvent_h__
#define mozilla_dom_DOMWheelEvent_h__

#include "nsIDOMWheelEvent.h"
#include "nsDOMMouseEvent.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/WheelEventBinding.h"

namespace mozilla {
namespace dom {

class DOMWheelEvent : public nsDOMMouseEvent,
                      public nsIDOMWheelEvent
{
public:
  DOMWheelEvent(mozilla::dom::EventTarget* aOwner,
                nsPresContext* aPresContext,
                WidgetWheelEvent* aWheelEvent);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMWheelEvent Interface
  NS_DECL_NSIDOMWHEELEVENT
  
  // Forward to base class
  NS_FORWARD_TO_NSDOMMOUSEEVENT

  static
  already_AddRefed<DOMWheelEvent> Constructor(const GlobalObject& aGlobal,
                                              const nsAString& aType,
                                              const WheelEventInit& aParam,
                                              mozilla::ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::WheelEventBinding::Wrap(aCx, aScope, this);
  }

  double DeltaX();
  double DeltaY();
  double DeltaZ();
  uint32_t DeltaMode();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DOMWheelEvent_h__
