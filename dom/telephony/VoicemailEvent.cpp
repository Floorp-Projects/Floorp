/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VoicemailEvent.h"
#include "nsDOMClassInfo.h"
#include "nsIDOMVoicemailStatus.h"

DOMCI_DATA(MozVoicemailEvent, mozilla::dom::telephony::VoicemailEvent)

USING_TELEPHONY_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(VoicemailEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(VoicemailEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mStatus)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(VoicemailEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mStatus)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(VoicemailEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozVoicemailEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozVoicemailEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(VoicemailEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(VoicemailEvent, nsDOMEvent)

nsresult
VoicemailEvent::InitVoicemailEvent(const nsAString& aEventTypeArg,
                                   bool aCanBubbleArg, bool aCancelableArg,
                                   nsIDOMMozVoicemailStatus* aStatus)
{
  nsresult rv = nsDOMEvent::InitEvent(aEventTypeArg, aCanBubbleArg,
                                      aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mStatus = aStatus;
  return NS_OK;
}

NS_IMETHODIMP
VoicemailEvent::GetStatus(nsIDOMMozVoicemailStatus** aStatus)
{
  NS_IF_ADDREF(*aStatus = mStatus);
  return NS_OK;
}

namespace {

nsresult
NS_NewDOMVoicemailEvent(nsIDOMEvent** aInstancePtrResult,
                  nsPresContext* aPresContext,
                  nsEvent* aEvent)
{
  return CallQueryInterface(
    new mozilla::dom::telephony::VoicemailEvent(aPresContext, aEvent),
    aInstancePtrResult);
}

}
