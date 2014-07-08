/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TimeEvent_h_
#define mozilla_dom_TimeEvent_h_

#include "mozilla/dom/Event.h"
#include "mozilla/dom/TimeEventBinding.h"
#include "nsIDOMTimeEvent.h"

namespace mozilla {
namespace dom {

class TimeEvent MOZ_FINAL : public Event,
                            public nsIDOMTimeEvent
{
public:
  TimeEvent(EventTarget* aOwner,
            nsPresContext* aPresContext,
            WidgetEvent* aEvent);

  // nsISupports interface:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TimeEvent, Event)

  // nsIDOMTimeEvent interface:
  NS_DECL_NSIDOMTIMEEVENT

  // Forward to base class
  NS_FORWARD_TO_EVENT

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return TimeEventBinding::Wrap(aCx, this);
  }

  int32_t Detail() const
  {
    return mDetail;
  }

  nsIDOMWindow* GetView() const
  {
    return mView;
  }

  void InitTimeEvent(const nsAString& aType, nsIDOMWindow* aView,
                     int32_t aDetail, ErrorResult& aRv)
  {
    aRv = InitTimeEvent(aType, aView, aDetail);
  }

private:
  ~TimeEvent() {}

  nsCOMPtr<nsIDOMWindow> mView;
  int32_t mDetail;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TimeEvent_h_
