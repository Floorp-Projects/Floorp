/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BeforeUnloadEvent_h_
#define mozilla_dom_BeforeUnloadEvent_h_

#include "mozilla/dom/BeforeUnloadEventBinding.h"
#include "mozilla/dom/Event.h"
#include "nsIDOMBeforeUnloadEvent.h"

namespace mozilla {
namespace dom {

class BeforeUnloadEvent : public Event,
                          public nsIDOMBeforeUnloadEvent
{
public:
  BeforeUnloadEvent(EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    WidgetEvent* aEvent)
    : Event(aOwner, aPresContext, aEvent)
  {
  }

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return BeforeUnloadEventBinding::Wrap(aCx, this, aGivenProto);
  }

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to Event
  NS_FORWARD_TO_EVENT

  // nsIDOMBeforeUnloadEvent Interface
  NS_DECL_NSIDOMBEFOREUNLOADEVENT

protected:
  ~BeforeUnloadEvent() {}

  nsString mText;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BeforeUnloadEvent_h_
