/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "USSDReceivedEvent.h"
#include "nsIDOMClassInfo.h"
#include "nsDOMClassInfoID.h"
#include "nsContentUtils.h"

DOMCI_DATA(USSDReceivedEvent, mozilla::dom::network::USSDReceivedEvent)

namespace mozilla {
namespace dom {
namespace network {

already_AddRefed<USSDReceivedEvent>
USSDReceivedEvent::Create(nsAString& aMessage)
{
  NS_ASSERTION(!aMessage.IsEmpty(), "Empty message!");

  nsRefPtr<USSDReceivedEvent> event = new USSDReceivedEvent();

  event->mMessage = aMessage;

  return event.forget();
}

NS_IMPL_ADDREF_INHERITED(USSDReceivedEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(USSDReceivedEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(USSDReceivedEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMUSSDReceivedEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(USSDReceivedEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
USSDReceivedEvent::GetMessage(nsAString& aMessage)
{
  aMessage.Assign(mMessage);
  return NS_OK;
}

}
}
}
