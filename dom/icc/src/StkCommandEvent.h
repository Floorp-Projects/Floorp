/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_stkcommandevent_h
#define mozilla_dom_icc_stkcommandevent_h

#include "nsDOMEvent.h"
#include "SimToolKit.h"

namespace mozilla {
namespace dom {
namespace icc {

class StkCommandEvent : public nsDOMEvent,
                        public nsIDOMMozStkCommandEvent
{
  nsString mCommand;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_NSIDOMMOZSTKCOMMANDEVENT
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(StkCommandEvent, nsDOMEvent)

  static already_AddRefed<StkCommandEvent>
  Create(EventTarget* aOwner, const nsAString& aMessage);

  nsresult
  Dispatch(EventTarget* aTarget, const nsAString& aEventType)
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
  StkCommandEvent(mozilla::dom::EventTarget* aOwner)
  : nsDOMEvent(aOwner, nullptr, nullptr)
  { }

  ~StkCommandEvent()
  { }
};

}
}
}

#endif // mozilla_dom_icc_stkcommandevent_h
