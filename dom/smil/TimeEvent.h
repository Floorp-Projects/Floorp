/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TimeEvent_h_
#define mozilla_dom_TimeEvent_h_

#include "mozilla/dom/Event.h"
#include "mozilla/dom/TimeEventBinding.h"
#include "nsIDOMTimeEvent.h"

class nsGlobalWindow;

namespace mozilla {
namespace dom {

class TimeEvent final : public Event,
                        public nsIDOMTimeEvent
{
public:
  TimeEvent(EventTarget* aOwner,
            nsPresContext* aPresContext,
            InternalSMILTimeEvent* aEvent);

  // nsISupports interface:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TimeEvent, Event)

  // nsIDOMTimeEvent interface:
  NS_DECL_NSIDOMTIMEEVENT

  // Forward to base class
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return TimeEventBinding::Wrap(aCx, this, aGivenProto);
  }

  void InitTimeEvent(const nsAString& aType, nsGlobalWindow* aView,
                     int32_t aDetail);


  int32_t Detail() const
  {
    return mDetail;
  }

  nsPIDOMWindowOuter* GetView() const
  {
    return mView;
  }

private:
  ~TimeEvent() {}

  nsCOMPtr<nsPIDOMWindowOuter> mView;
  int32_t mDetail;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::TimeEvent>
NS_NewDOMTimeEvent(mozilla::dom::EventTarget* aOwner,
                   nsPresContext* aPresContext,
                   mozilla::InternalSMILTimeEvent* aEvent);

#endif // mozilla_dom_TimeEvent_h_
