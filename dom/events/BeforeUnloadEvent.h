/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BeforeUnloadEvent_h_
#define mozilla_dom_BeforeUnloadEvent_h_

#include "mozilla/dom/BeforeUnloadEventBinding.h"
#include "mozilla/dom/Event.h"

namespace mozilla {
namespace dom {

class BeforeUnloadEvent : public Event
{
public:
  BeforeUnloadEvent(EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    WidgetEvent* aEvent)
    : Event(aOwner, aPresContext, aEvent)
  {
  }

  virtual BeforeUnloadEvent* AsBeforeUnloadEvent() override
  {
    return this;
  }

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return BeforeUnloadEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(BeforeUnloadEvent, Event)

  void GetReturnValue(nsAString& aReturnValue);
  void SetReturnValue(const nsAString& aReturnValue);

protected:
  ~BeforeUnloadEvent() {}

  nsString mText;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::BeforeUnloadEvent>
NS_NewDOMBeforeUnloadEvent(mozilla::dom::EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           mozilla::WidgetEvent* aEvent);

#endif // mozilla_dom_BeforeUnloadEvent_h_
