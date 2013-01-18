/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsEvent.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMSmsMessage.h"

DOMCI_DATA(MozSmsEvent, mozilla::dom::sms::SmsEvent)

namespace mozilla {
namespace dom {
namespace sms {

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SmsEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessage)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SmsEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessage)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(SmsEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(SmsEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SmsEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozSmsEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozSmsEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozSmsEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

nsresult
SmsEvent::Init(const nsAString& aEventTypeArg, bool aCanBubbleArg,
               bool aCancelableArg, nsIDOMMozSmsMessage* aMessage)
{
  nsresult rv = nsDOMEvent::InitEvent(aEventTypeArg, aCanBubbleArg,
                                      aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mMessage = aMessage;
  return NS_OK;
}

NS_IMETHODIMP
SmsEvent::GetMessageMoz(nsIDOMMozSmsMessage** aMessage)
{
  NS_IF_ADDREF(*aMessage = mMessage);
  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla

nsresult
NS_NewDOMSmsEvent(nsIDOMEvent** aInstancePtrResult,
                  nsPresContext* aPresContext,
                  nsEvent* aEvent)
{
  return CallQueryInterface(new mozilla::dom::sms::SmsEvent(aPresContext, aEvent),
                            aInstancePtrResult);
}
