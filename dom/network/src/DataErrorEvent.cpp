/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataErrorEvent.h"
#include "nsIDOMClassInfo.h"

DOMCI_DATA(DataErrorEvent, mozilla::dom::network::DataErrorEvent)

namespace mozilla {
namespace dom {
namespace network {

already_AddRefed<DataErrorEvent>
DataErrorEvent::Create(nsAString& aMessage)
{
  NS_ASSERTION(!aMessage.IsEmpty(), "Empty message!");

  nsRefPtr<DataErrorEvent> event = new DataErrorEvent();

  event->mMessage = aMessage;

  return event.forget();
}

NS_IMPL_ADDREF_INHERITED(DataErrorEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(DataErrorEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(DataErrorEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDataErrorEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DataErrorEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
DataErrorEvent::GetMessage(nsAString& aMessage)
{
  aMessage.Assign(mMessage);
  return NS_OK;
}

}
}
}
