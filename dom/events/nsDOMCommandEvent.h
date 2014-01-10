/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMCommandEvent_h__
#define nsDOMCommandEvent_h__

#include "nsIDOMCommandEvent.h"
#include "nsDOMEvent.h"
#include "mozilla/dom/CommandEventBinding.h"
#include "mozilla/EventForwards.h"

class nsDOMCommandEvent : public nsDOMEvent,
                          public nsIDOMCommandEvent
{
public:
  nsDOMCommandEvent(mozilla::dom::EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    mozilla::WidgetCommandEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMCOMMANDEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::CommandEventBinding::Wrap(aCx, aScope, this);
  }

  void InitCommandEvent(const nsAString& aType,
                        bool aCanBubble,
                        bool aCancelable,
                        const nsAString& aCommand,
                        mozilla::ErrorResult& aRv)
  {
    aRv = InitCommandEvent(aType, aCanBubble, aCancelable, aCommand);
  }
};

#endif // nsDOMCommandEvent_h__
