/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CommandEvent_h_
#define mozilla_dom_CommandEvent_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/CommandEventBinding.h"
#include "mozilla/dom/Event.h"

namespace mozilla {
namespace dom {

class CommandEvent : public Event
{
public:
  CommandEvent(EventTarget* aOwner,
               nsPresContext* aPresContext,
               WidgetCommandEvent* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(CommandEvent, Event)

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return CommandEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  void GetCommand(nsAString& aCommand);

protected:
  ~CommandEvent() {}
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::CommandEvent>
NS_NewDOMCommandEvent(mozilla::dom::EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      mozilla::WidgetCommandEvent* aEvent);

#endif // mozilla_dom_CommandEvent_h_
