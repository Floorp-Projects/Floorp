/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDeviceProximityEvent_h__
#define nsDOMDeviceProximityEvent_h__

#include "nsIDOMDeviceProximityEvent.h"
#include "nsDOMEvent.h"

class nsDOMDeviceProximityEvent
 : public nsDOMEvent
 , public nsIDOMDeviceProximityEvent
{
public:

 nsDOMDeviceProximityEvent(nsPresContext* aPresContext, nsEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent),
    mValue(-1),
    mMin(0),
    mMax(0) {}

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  // nsIDOMDeviceProximityEvent Interface
  NS_DECL_NSIDOMDEVICEPROXIMITYEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx,
                                jsval* aVal);
protected:
  double mValue, mMin, mMax;
};

#endif
