/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMWheelEvent_h__
#define mozilla_dom_DOMWheelEvent_h__

#include "nsIDOMWheelEvent.h"
#include "nsDOMMouseEvent.h"

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
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DOMWheelEvent_h__
