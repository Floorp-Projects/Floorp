/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMozTouchEvent_h__
#define nsDOMMozTouchEvent_h__

#include "nsIDOMMozTouchEvent.h"
#include "nsDOMMouseEvent.h"

class nsPresContext;

class nsDOMMozTouchEvent : public nsDOMMouseEvent,
                           public nsIDOMMozTouchEvent
{
public:
  nsDOMMozTouchEvent(nsPresContext* aPresCOntext, nsMozTouchEvent* aEvent);
  virtual ~nsDOMMozTouchEvent();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMMOZTOUCHEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMMOUSEEVENT
};

#endif
