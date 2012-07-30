/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_network_ussdreceivedevent_h
#define mozilla_dom_network_ussdreceivedevent_h

#include "nsIDOMUSSDReceivedEvent.h"
#include "nsDOMEvent.h"

namespace mozilla {
namespace dom {
namespace network {

class USSDReceivedEvent : public nsDOMEvent,
                          public nsIDOMUSSDReceivedEvent
{
  nsString mMessage;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_NSIDOMUSSDRECEIVEDEVENT

  static already_AddRefed<USSDReceivedEvent>
  Create(nsAString& aMessage);

  nsresult
  Dispatch(nsIDOMEventTarget* aTarget, const nsAString& aEventType)
  {
    NS_ASSERTION(aTarget, "Null pointer!");
    NS_ASSERTION(!aEventType.IsEmpty(), "Empty event type!");

    nsresult rv = InitEvent(aEventType, false, false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetTrusted(true);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIDOMEvent* thisEvent =
      static_cast<nsDOMEvent*>(const_cast<USSDReceivedEvent*>(this));

    bool dummy;
    rv = aTarget->DispatchEvent(thisEvent, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  USSDReceivedEvent()
  : nsDOMEvent(nullptr, nullptr)
  { }

  ~USSDReceivedEvent()
  { }
};

}
}
}

#endif // mozilla_dom_network_ussdreceivedevent_h
