/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDeviceOrientationEvent_h__
#define nsDOMDeviceOrientationEvent_h__

#include "nsIDOMDeviceOrientationEvent.h"
#include "nsDOMEvent.h"

class nsDOMDeviceOrientationEvent : public nsDOMEvent,
                                    public nsIDOMDeviceOrientationEvent
{
public:

  nsDOMDeviceOrientationEvent(nsPresContext* aPresContext, nsEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent),
    mAlpha(0),
    mBeta(0),
    mGamma(0),
    mAbsolute(true) {}

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  // nsIDOMDeviceOrientationEvent Interface
  NS_DECL_NSIDOMDEVICEORIENTATIONEVENT

protected:
  double mAlpha, mBeta, mGamma;
  bool mAbsolute;
};

#endif
