/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_DOMTIMEEVENT_H_
#define NS_DOMTIMEEVENT_H_

#include "nsIDOMTimeEvent.h"
#include "nsDOMEvent.h"
#include "mozilla/dom/TimeEventBinding.h"

class nsDOMTimeEvent MOZ_FINAL : public nsDOMEvent,
                                 public nsIDOMTimeEvent
{
public:
  nsDOMTimeEvent(mozilla::dom::EventTarget* aOwner,
                 nsPresContext* aPresContext,
                 mozilla::WidgetEvent* aEvent);

  // nsISupports interface:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMTimeEvent, nsDOMEvent)

  // nsIDOMTimeEvent interface:
  NS_DECL_NSIDOMTIMEEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE
  {
    return mozilla::dom::TimeEventBinding::Wrap(aCx, aScope, this);
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
                     int32_t aDetail, mozilla::ErrorResult& aRv)
  {
    aRv = InitTimeEvent(aType, aView, aDetail);
  }

private:
  nsCOMPtr<nsIDOMWindow> mView;
  int32_t mDetail;
};

#endif // NS_DOMTIMEEVENT_H_
