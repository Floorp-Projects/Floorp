/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_network_dataerrorevent_h
#define mozilla_dom_network_dataerrorevent_h

#include "nsIDOMDataErrorEvent.h"
#include "nsDOMEvent.h"

namespace mozilla {
namespace dom {
namespace network {

class DataErrorEvent : public nsDOMEvent,
                       public nsIDOMDataErrorEvent
{
  nsString mMessage;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_NSIDOMDATAERROREVENT

  static already_AddRefed<DataErrorEvent>
  Create(nsAString& aMessage);

  nsresult
  Dispatch(nsIDOMEventTarget* aTarget, const nsAString& aEventType)
  {
    NS_ASSERTION(aTarget, "Null pointer!");
    NS_ASSERTION(!aEventType.IsEmpty(), "Empty event type!");

    nsresult rv = InitEvent(aEventType, false, false);
    NS_ENSURE_SUCCESS(rv, rv);

    SetTrusted(true);

    nsDOMEvent* thisEvent = this;

    bool dummy;
    rv = aTarget->DispatchEvent(thisEvent, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  DataErrorEvent()
  : nsDOMEvent(nullptr, nullptr)
  { }

  ~DataErrorEvent()
  { }
};

}
}
}

#endif // mozilla_dom_network_dataerrorevent_h
