/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMSimpleGestureEvent_h__
#define nsDOMSimpleGestureEvent_h__

#include "nsIDOMSimpleGestureEvent.h"
#include "nsDOMMouseEvent.h"

class nsPresContext;

class nsDOMSimpleGestureEvent : public nsDOMMouseEvent,
                                public nsIDOMSimpleGestureEvent
{
public:
  nsDOMSimpleGestureEvent(nsPresContext*, nsSimpleGestureEvent*);
  virtual ~nsDOMSimpleGestureEvent();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMSIMPLEGESTUREEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMMOUSEEVENT
};

#endif
