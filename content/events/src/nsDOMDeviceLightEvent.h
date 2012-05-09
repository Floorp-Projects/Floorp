/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDeviceLightEvent_h__
#define nsDOMDeviceLightEvent_h__

#include "nsIDOMDeviceLightEvent.h"
#include "nsDOMEvent.h"

class nsDOMDeviceLightEvent
 : public nsDOMEvent
 , public nsIDOMDeviceLightEvent
{
public:

  nsDOMDeviceLightEvent(nsPresContext* aPresContext, nsEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent),
    mValue(0) {}

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  // nsIDOMDeviceLightEvent Interface
  NS_DECL_NSIDOMDEVICELIGHTEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx,
                                jsval* aVal);
protected:
  double mValue;
};

#endif
