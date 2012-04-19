/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnection.h"
#include "nsIDOMDOMRequest.h"
#include "nsIDOMClassInfo.h"

DOMCI_DATA(MozMobileConnection, mozilla::dom::network::MobileConnection)

namespace mozilla {
namespace dom {
namespace network {

NS_IMPL_CYCLE_COLLECTION_CLASS(MobileConnection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MobileConnection,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(cardstatechange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(voicechange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(datachange)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MobileConnection,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(cardstatechange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(voicechange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(datachange)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MobileConnection)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozMobileConnection)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozMobileConnection)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozMobileConnection)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MobileConnection, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MobileConnection, nsDOMEventTargetHelper)

MobileConnection::MobileConnection()
{
}

void
MobileConnection::Init(nsPIDOMWindow* aWindow)
{
  BindToOwner(aWindow);
}

void
MobileConnection::Shutdown()
{
}

// nsIObserver

NS_IMETHODIMP
MobileConnection::Observe(nsISupports* aSubject,
                          const char* aTopic,
                          const PRUnichar* aData)
{
  return NS_OK;
}

// nsIDOMMozMobileConnection

NS_IMETHODIMP
MobileConnection::GetCardState(nsAString& cardState)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileConnection::GetVoice(nsIDOMMozMobileConnectionInfo** voice)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileConnection::GetData(nsIDOMMozMobileConnectionInfo** data)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileConnection::GetNetworks(nsIDOMDOMRequest** request)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_EVENT_HANDLER(MobileConnection, cardstatechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, voicechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, datachange)

} // namespace network
} // namespace dom
} // namespace mozilla
