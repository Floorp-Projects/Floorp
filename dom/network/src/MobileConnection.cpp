/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnection.h"
#include "nsIDOMDOMRequest.h"
#include "nsIDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsIObserverService.h"
#include "USSDReceivedEvent.h"
#include "mozilla/Services.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

#define VOICECHANGE_EVENTNAME      NS_LITERAL_STRING("voicechange")
#define DATACHANGE_EVENTNAME       NS_LITERAL_STRING("datachange")
#define CARDSTATECHANGE_EVENTNAME  NS_LITERAL_STRING("cardstatechange")
#define USSDRECEIVED_EVENTNAME     NS_LITERAL_STRING("ussdreceived")

DOMCI_DATA(MozMobileConnection, mozilla::dom::network::MobileConnection)

namespace mozilla {
namespace dom {
namespace network {

const char* kVoiceChangedTopic     = "mobile-connection-voice-changed";
const char* kDataChangedTopic      = "mobile-connection-data-changed";
const char* kCardStateChangedTopic = "mobile-connection-cardstate-changed";
const char* kUssdReceivedTopic     = "mobile-connection-ussd-received";

NS_IMPL_CYCLE_COLLECTION_CLASS(MobileConnection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MobileConnection,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(cardstatechange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(voicechange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(datachange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(ussdreceived)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MobileConnection,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(cardstatechange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(voicechange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(datachange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(ussdreceived)
  tmp->mProvider = nullptr;
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
  mProvider = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);

  // Not being able to acquire the provider isn't fatal since we check
  // for it explicitly below.
  if (!mProvider) {
    NS_WARNING("Could not acquire nsIMobileConnectionProvider!");
  }
}

void
MobileConnection::Init(nsPIDOMWindow* aWindow)
{
  BindToOwner(aWindow);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    NS_WARNING("Could not acquire nsIObserverService!");
    return;
  }

  obs->AddObserver(this, kVoiceChangedTopic, false);
  obs->AddObserver(this, kDataChangedTopic, false);
  obs->AddObserver(this, kCardStateChangedTopic, false);
  obs->AddObserver(this, kUssdReceivedTopic, false);
}

void
MobileConnection::Shutdown()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    NS_WARNING("Could not acquire nsIObserverService!");
    return;
  }

  obs->RemoveObserver(this, kVoiceChangedTopic);
  obs->RemoveObserver(this, kDataChangedTopic);
  obs->RemoveObserver(this, kCardStateChangedTopic);
  obs->RemoveObserver(this, kUssdReceivedTopic);
}

// nsIObserver

NS_IMETHODIMP
MobileConnection::Observe(nsISupports* aSubject,
                          const char* aTopic,
                          const PRUnichar* aData)
{
  if (!strcmp(aTopic, kVoiceChangedTopic)) {
    InternalDispatchEvent(VOICECHANGE_EVENTNAME);
    return NS_OK;
  }

  if (!strcmp(aTopic, kDataChangedTopic)) {
    InternalDispatchEvent(DATACHANGE_EVENTNAME);
    return NS_OK;
  }

  if (!strcmp(aTopic, kCardStateChangedTopic)) {
    InternalDispatchEvent(CARDSTATECHANGE_EVENTNAME);
    return NS_OK;
  }

  if (!strcmp(aTopic, kUssdReceivedTopic)) {
    nsString ussd;
    ussd.Assign(aData);
    nsRefPtr<USSDReceivedEvent> event = USSDReceivedEvent::Create(ussd);
    NS_ASSERTION(event, "This should never fail!");

    nsresult rv =
      event->Dispatch(ToIDOMEventTarget(), USSDRECEIVED_EVENTNAME);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  MOZ_NOT_REACHED("Unknown observer topic!");
  return NS_OK;
}

// nsIDOMMozMobileConnection

NS_IMETHODIMP
MobileConnection::GetCardState(nsAString& cardState)
{
  if (!mProvider) {
    cardState.SetIsVoid(true);
    return NS_OK;
  }
  return mProvider->GetCardState(cardState);
}

NS_IMETHODIMP
MobileConnection::GetVoice(nsIDOMMozMobileConnectionInfo** voice)
{
  if (!mProvider) {
    *voice = nullptr;
    return NS_OK;
  }
  return mProvider->GetVoiceConnectionInfo(voice);
}

NS_IMETHODIMP
MobileConnection::GetData(nsIDOMMozMobileConnectionInfo** data)
{
  if (!mProvider) {
    *data = nullptr;
    return NS_OK;
  }
  return mProvider->GetDataConnectionInfo(data);
}

NS_IMETHODIMP
MobileConnection::GetNetworkSelectionMode(nsAString& networkSelectionMode)
{
  if (!mProvider) {
    networkSelectionMode.SetIsVoid(true);
    return NS_OK;
  }
  return mProvider->GetNetworkSelectionMode(networkSelectionMode);
}

NS_IMETHODIMP
MobileConnection::GetNetworks(nsIDOMDOMRequest** request)
{
  *request = nullptr;

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetNetworks(GetOwner(), request);
}

NS_IMETHODIMP
MobileConnection::SelectNetwork(nsIDOMMozMobileNetworkInfo* network, nsIDOMDOMRequest** request)
{
  *request = nullptr;

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SelectNetwork(GetOwner(), network, request);
}

NS_IMETHODIMP
MobileConnection::SelectNetworkAutomatically(nsIDOMDOMRequest** request)
{
  *request = nullptr;

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SelectNetworkAutomatically(GetOwner(), request);
}

NS_IMETHODIMP
MobileConnection::GetCardLock(const nsAString& aLockType, nsIDOMDOMRequest** aDomRequest)
{
  *aDomRequest = nullptr;

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetCardLock(GetOwner(), aLockType, aDomRequest);
}

NS_IMETHODIMP
MobileConnection::UnlockCardLock(const jsval& aInfo, nsIDOMDOMRequest** aDomRequest)
{
  *aDomRequest = nullptr;

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->UnlockCardLock(GetOwner(), aInfo, aDomRequest);
}

NS_IMETHODIMP
MobileConnection::SetCardLock(const jsval& aInfo, nsIDOMDOMRequest** aDomRequest)
{
  *aDomRequest = nullptr;

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetCardLock(GetOwner(), aInfo, aDomRequest);
}

NS_IMETHODIMP
MobileConnection::SendUSSD(const nsAString& aUSSDString,
                           nsIDOMDOMRequest** request)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SendUSSD(GetOwner(), aUSSDString, request);
}

NS_IMETHODIMP
MobileConnection::CancelUSSD(nsIDOMDOMRequest** request)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->CancelUSSD(GetOwner(), request);
}

nsresult
MobileConnection::InternalDispatchEvent(const nsAString& aType)
{
  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nullptr, nullptr);
  nsresult rv = event->InitEvent(aType, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(MobileConnection, cardstatechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, voicechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, datachange)
NS_IMPL_EVENT_HANDLER(MobileConnection, ussdreceived)

} // namespace network
} // namespace dom
} // namespace mozilla
