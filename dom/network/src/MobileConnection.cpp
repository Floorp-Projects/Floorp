/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnection.h"
#include "nsIDOMDOMRequest.h"
#include "nsIDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsIObserverService.h"
#include "USSDReceivedEvent.h"
#include "DataErrorEvent.h"
#include "mozilla/Services.h"
#include "IccManager.h"
#include "GeneratedEvents.h"
#include "nsIDOMICCCardLockErrorEvent.h"

#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsJSON.h"
#include "jsapi.h"

#include "mozilla/dom/USSDReceivedEventBinding.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

#define VOICECHANGE_EVENTNAME      NS_LITERAL_STRING("voicechange")
#define DATACHANGE_EVENTNAME       NS_LITERAL_STRING("datachange")
#define CARDSTATECHANGE_EVENTNAME  NS_LITERAL_STRING("cardstatechange")
#define ICCINFOCHANGE_EVENTNAME    NS_LITERAL_STRING("iccinfochange")
#define USSDRECEIVED_EVENTNAME     NS_LITERAL_STRING("ussdreceived")
#define DATAERROR_EVENTNAME        NS_LITERAL_STRING("dataerror")
#define ICCCARDLOCKERROR_EVENTNAME NS_LITERAL_STRING("icccardlockerror")

DOMCI_DATA(MozMobileConnection, mozilla::dom::network::MobileConnection)

namespace mozilla {
namespace dom {
namespace network {

const char* kVoiceChangedTopic     = "mobile-connection-voice-changed";
const char* kDataChangedTopic      = "mobile-connection-data-changed";
const char* kCardStateChangedTopic = "mobile-connection-cardstate-changed";
const char* kIccInfoChangedTopic   = "mobile-connection-iccinfo-changed";
const char* kUssdReceivedTopic     = "mobile-connection-ussd-received";
const char* kDataErrorTopic        = "mobile-connection-data-error";
const char* kIccCardLockErrorTopic = "mobile-connection-icccardlock-error";

NS_IMPL_CYCLE_COLLECTION_CLASS(MobileConnection)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MobileConnection,
                                                  nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MobileConnection,
                                                nsDOMEventTargetHelper)
  tmp->mProvider = nullptr;
  tmp->mIccManager = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MobileConnection)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozMobileConnection)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozMobileConnection)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozMobileConnection)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MobileConnection, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MobileConnection, nsDOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(MobileConnection, cardstatechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, iccinfochange)
NS_IMPL_EVENT_HANDLER(MobileConnection, voicechange)
NS_IMPL_EVENT_HANDLER(MobileConnection, datachange)
NS_IMPL_EVENT_HANDLER(MobileConnection, ussdreceived)
NS_IMPL_EVENT_HANDLER(MobileConnection, dataerror)
NS_IMPL_EVENT_HANDLER(MobileConnection, icccardlockerror)

MobileConnection::MobileConnection()
{
  mProvider = do_GetService(NS_RILCONTENTHELPER_CONTRACTID);

  // Not being able to acquire the provider isn't fatal since we check
  // for it explicitly below.
  if (!mProvider) {
    NS_WARNING("Could not acquire nsIMobileConnectionProvider!");
  } else {
    mProvider->RegisterMobileConnectionMsg();
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
  obs->AddObserver(this, kIccInfoChangedTopic, false);
  obs->AddObserver(this, kUssdReceivedTopic, false);
  obs->AddObserver(this, kDataErrorTopic, false);
  obs->AddObserver(this, kIccCardLockErrorTopic, false);

  mIccManager = new icc::IccManager();
  mIccManager->Init(aWindow);
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
  obs->RemoveObserver(this, kIccInfoChangedTopic);
  obs->RemoveObserver(this, kUssdReceivedTopic);
  obs->RemoveObserver(this, kDataErrorTopic);
  obs->RemoveObserver(this, kIccCardLockErrorTopic);

  if (mIccManager) {
    mIccManager->Shutdown();
    mIccManager = nullptr;
  }
}

// nsIObserver

NS_IMETHODIMP
MobileConnection::Observe(nsISupports* aSubject,
                          const char* aTopic,
                          const PRUnichar* aData)
{
  if (!strcmp(aTopic, kVoiceChangedTopic)) {
    DispatchTrustedEvent(VOICECHANGE_EVENTNAME);
    return NS_OK;
  }

  if (!strcmp(aTopic, kDataChangedTopic)) {
    DispatchTrustedEvent(DATACHANGE_EVENTNAME);
    return NS_OK;
  }

  if (!strcmp(aTopic, kCardStateChangedTopic)) {
    DispatchTrustedEvent(CARDSTATECHANGE_EVENTNAME);
    return NS_OK;
  }

  if (!strcmp(aTopic, kIccInfoChangedTopic)) {
    DispatchTrustedEvent(ICCINFOCHANGE_EVENTNAME);
    return NS_OK;
  }

  if (!strcmp(aTopic, kUssdReceivedTopic)) {
    mozilla::dom::USSDReceivedEventDict dict;
    bool ok = dict.Init(nsDependentString(aData));
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

    nsRefPtr<USSDReceivedEvent> event =
      USSDReceivedEvent::Create(dict.mMessage, dict.mSessionEnded);
    NS_ASSERTION(event, "This should never fail!");

    nsresult rv = event->Dispatch(ToIDOMEventTarget(), USSDRECEIVED_EVENTNAME);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  if (!strcmp(aTopic, kDataErrorTopic)) {
    nsString dataerror;
    dataerror.Assign(aData);
    nsRefPtr<DataErrorEvent> event = DataErrorEvent::Create(dataerror);
    NS_ASSERTION(event, "This should never fail!");

    nsresult rv =
      event->Dispatch(ToIDOMEventTarget(), DATAERROR_EVENTNAME);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  if (!strcmp(aTopic, kIccCardLockErrorTopic)) {
    nsString errorMsg;
    errorMsg.Assign(aData);

    if (errorMsg.IsEmpty()) {
      NS_ERROR("Got a 'icc-cardlock-error' topic without a valid message!");
      return NS_OK;
    }

    nsString lockType;
    int32_t retryCount = -1;

    // Decode the json string "errorMsg" and retrieve its properties:
    // "lockType" and "retryCount".
    nsresult rv;
    nsIScriptContext* sc = GetContextForEventHandlers(&rv);
    NS_ENSURE_STATE(sc);
    JSContext* cx = sc->GetNativeContext();
    NS_ASSERTION(cx, "Failed to get a context!");

    nsCOMPtr<nsIJSON> json(new nsJSON());
    jsval error;
    rv = json->DecodeToJSVal(errorMsg, cx, &error);
    NS_ENSURE_SUCCESS(rv, rv);

    jsval type;
    if (JS_GetProperty(cx, JSVAL_TO_OBJECT(error), "lockType", &type)) {
      if (JSVAL_IS_STRING(type)) {
        nsDependentJSString str;
        str.init(cx, type.toString());
        lockType.Assign(str);
      }
    }

    jsval count;
    if (JS_GetProperty(cx, JSVAL_TO_OBJECT(error), "retryCount", &count)) {
      if (JSVAL_IS_NUMBER(count)) {
        retryCount = count.toNumber();
      }
    }

    nsCOMPtr<nsIDOMEvent> event;
    NS_NewDOMICCCardLockErrorEvent(getter_AddRefs(event), nullptr, nullptr);

    nsCOMPtr<nsIDOMICCCardLockErrorEvent> e = do_QueryInterface(event);
    e->InitICCCardLockErrorEvent(NS_LITERAL_STRING("icccardlockerror"),
                                 false, false, lockType, retryCount);
    e->SetTrusted(true);
    bool dummy;
    DispatchEvent(event, &dummy);

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
MobileConnection::GetIccInfo(nsIDOMMozMobileICCInfo** aIccInfo)
{
  if (!mProvider) {
    *aIccInfo = nullptr;
    return NS_OK;
  }
  return mProvider->GetIccInfo(aIccInfo);
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
MobileConnection::GetIcc(nsIDOMMozIccManager** aIcc)
{
  NS_IF_ADDREF(*aIcc = mIccManager);
  return NS_OK;
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
MobileConnection::SendMMI(const nsAString& aMMIString,
                          nsIDOMDOMRequest** request)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SendMMI(GetOwner(), aMMIString, request);
}

NS_IMETHODIMP
MobileConnection::CancelMMI(nsIDOMDOMRequest** request)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->CancelMMI(GetOwner(), request);
}

NS_IMETHODIMP
MobileConnection::GetCallForwardingOption(uint16_t aReason,
                                          nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->GetCallForwardingOption(GetOwner(), aReason, aRequest);
}

NS_IMETHODIMP
MobileConnection::SetCallForwardingOption(nsIDOMMozMobileCFInfo* aCFInfo,
                                          nsIDOMDOMRequest** aRequest)
{
  *aRequest = nullptr;

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->SetCallForwardingOption(GetOwner(), aCFInfo, aRequest);
}

} // namespace network
} // namespace dom
} // namespace mozilla
