/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMCloseEvent_h__
#define nsDOMCloseEvent_h__

#include "nsIDOMCloseEvent.h"
#include "nsDOMEvent.h"

/**
 * Implements the CloseEvent event, used for notifying that a WebSocket
 * connection has been closed.
 *
 * See http://dev.w3.org/html5/websockets/#closeevent for further details.
 */
class nsDOMCloseEvent : public nsDOMEvent,
                        public nsIDOMCloseEvent
{
public:
  nsDOMCloseEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent),
    mWasClean(false),
    mReasonCode(1005) {}
                     
  NS_DECL_ISUPPORTS_INHERITED

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

  NS_DECL_NSIDOMCLOSEEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);
private:
  bool mWasClean;
  PRUint16 mReasonCode;
  nsString mReason;
};

#endif // nsDOMCloseEvent_h__
