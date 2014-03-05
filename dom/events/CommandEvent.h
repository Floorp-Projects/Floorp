/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CommandEvent_h_
#define mozilla_dom_CommandEvent_h_

#include "mozilla/EventForwards.h"
#include "mozilla/dom/CommandEventBinding.h"
#include "mozilla/dom/Event.h"
#include "nsIDOMCommandEvent.h"

namespace mozilla {
namespace dom {

class CommandEvent : public Event,
                     public nsIDOMCommandEvent
{
public:
  CommandEvent(EventTarget* aOwner,
               nsPresContext* aPresContext,
               WidgetCommandEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMCOMMANDEVENT

  // Forward to base class
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return CommandEventBinding::Wrap(aCx, aScope, this);
  }

  void InitCommandEvent(const nsAString& aType,
                        bool aCanBubble,
                        bool aCancelable,
                        const nsAString& aCommand,
                        ErrorResult& aRv)
  {
    aRv = InitCommandEvent(aType, aCanBubble, aCancelable, aCommand);
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CommandEvent_h_
