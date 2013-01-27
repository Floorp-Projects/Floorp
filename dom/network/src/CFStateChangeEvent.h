/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_network_cfstatechangeevent_h
#define mozilla_dom_network_cfstatechangeevent_h

#include "nsIDOMCFStateChangeEvent.h"
#include "nsDOMEvent.h"

namespace mozilla {
namespace dom {
namespace network {

class CFStateChangeEvent : public nsDOMEvent,
                           public nsIDOMCFStateChangeEvent
{
  bool mSuccess;
  uint16_t mAction;
  uint16_t mReason;
  nsString mNumber;
  uint16_t mTimeSeconds;
  uint16_t mServiceClass;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_NSIDOMCFSTATECHANGEEVENT

  static already_AddRefed<CFStateChangeEvent>
  Create(bool aSuccess,
         uint16_t aAction,
         uint16_t aReason,
         nsAString& aNumber,
         uint16_t aTimeSeconds,
         uint16_t aServiceClass);

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
  CFStateChangeEvent()
  : nsDOMEvent(nullptr, nullptr)
  { }

  ~CFStateChangeEvent()
  { }
};

}
}
}

#endif // mozilla_dom_network_cfstatechangeevent_h
