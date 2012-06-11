/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMNotifyPaintEvent_h_
#define nsDOMNotifyPaintEvent_h_

#include "nsIDOMNotifyPaintEvent.h"
#include "nsDOMEvent.h"
#include "nsPresContext.h"

class nsPaintRequestList;

class nsDOMNotifyPaintEvent : public nsDOMEvent,
                              public nsIDOMNotifyPaintEvent
{
public:
  nsDOMNotifyPaintEvent(nsPresContext*           aPresContext,
                        nsEvent*                 aEvent,
                        PRUint32                 aEventType,
                        nsInvalidateRequestList* aInvalidateRequests);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMNOTIFYPAINTEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT_NO_SERIALIZATION_NO_DUPLICATION
  NS_IMETHOD DuplicatePrivateData()
  {
    return nsDOMEvent::DuplicatePrivateData();
  }
  NS_IMETHOD_(void) Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType);
  NS_IMETHOD_(bool) Deserialize(const IPC::Message* aMsg, void** aIter);
private:
  nsRegion GetRegion();

  nsTArray<nsInvalidateRequestList::Request> mInvalidateRequests;
};

#endif // nsDOMNotifyPaintEvent_h_
