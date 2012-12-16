/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsFilter.h"
#include "SmsManager.h"
#include "nsIDOMClassInfo.h"
#include "nsISmsService.h"
#include "nsIObserverService.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "Constants.h"
#include "SmsEvent.h"
#include "nsIDOMSmsMessage.h"
#include "SmsRequest.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsISmsDatabaseService.h"
#include "nsIXPConnect.h"
#include "nsIPermissionManager.h"

#define RECEIVED_EVENT_NAME         NS_LITERAL_STRING("received")
#define SENDING_EVENT_NAME          NS_LITERAL_STRING("sending")
#define SENT_EVENT_NAME             NS_LITERAL_STRING("sent")
#define FAILED_EVENT_NAME           NS_LITERAL_STRING("failed")
#define DELIVERY_SUCCESS_EVENT_NAME NS_LITERAL_STRING("deliverysuccess")
#define DELIVERY_ERROR_EVENT_NAME   NS_LITERAL_STRING("deliveryerror")

DOMCI_DATA(MozSmsManager, mozilla::dom::sms::SmsManager)

namespace mozilla {
namespace dom {
namespace sms {

NS_IMPL_CYCLE_COLLECTION_INHERITED_0(SmsManager, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SmsManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozSmsManager)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozSmsManager)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozSmsManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(SmsManager, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SmsManager, nsDOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(SmsManager, received)
NS_IMPL_EVENT_HANDLER(SmsManager, sending)
NS_IMPL_EVENT_HANDLER(SmsManager, sent)
NS_IMPL_EVENT_HANDLER(SmsManager, failed)
NS_IMPL_EVENT_HANDLER(SmsManager, deliverysuccess)
NS_IMPL_EVENT_HANDLER(SmsManager, deliveryerror)

/* static */already_AddRefed<SmsManager>
SmsManager::CreateInstanceIfAllowed(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(aWindow, "Null pointer!");

#ifndef MOZ_WEBSMS_BACKEND
  return nullptr;
#endif

  // First of all, the general pref has to be turned on.
  bool enabled = false;
  Preferences::GetBool("dom.sms.enabled", &enabled);
  NS_ENSURE_TRUE(enabled, nullptr);

  nsCOMPtr<nsIPermissionManager> permMgr =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, nullptr);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(aWindow, "sms", &permission);

  if (permission != nsIPermissionManager::ALLOW_ACTION) {
    return nullptr;
  }

  // Check the Sms Service:
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, nullptr);

  bool result = false;
  smsService->HasSupport(&result);
  if (!result) {
    return nullptr;
  }

  nsRefPtr<SmsManager> smsMgr = new SmsManager();
  smsMgr->Init(aWindow);

  return smsMgr.forget();
}

void
SmsManager::Init(nsPIDOMWindow *aWindow)
{
  BindToOwner(aWindow);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  // GetObserverService() can return null is some situations like shutdown.
  if (!obs) {
    return;
  }

  obs->AddObserver(this, kSmsReceivedObserverTopic, false);
  obs->AddObserver(this, kSmsSendingObserverTopic, false);
  obs->AddObserver(this, kSmsSentObserverTopic, false);
  obs->AddObserver(this, kSmsFailedObserverTopic, false);
  obs->AddObserver(this, kSmsDeliverySuccessObserverTopic, false);
  obs->AddObserver(this, kSmsDeliveryErrorObserverTopic, false);
}

void
SmsManager::Shutdown()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  // GetObserverService() can return null is some situations like shutdown.
  if (!obs) {
    return;
  }

  obs->RemoveObserver(this, kSmsReceivedObserverTopic);
  obs->RemoveObserver(this, kSmsSendingObserverTopic);
  obs->RemoveObserver(this, kSmsSentObserverTopic);
  obs->RemoveObserver(this, kSmsFailedObserverTopic);
  obs->RemoveObserver(this, kSmsDeliverySuccessObserverTopic);
  obs->RemoveObserver(this, kSmsDeliveryErrorObserverTopic);
}

NS_IMETHODIMP
SmsManager::GetNumberOfMessagesForText(const nsAString& aText, uint16_t* aResult)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, NS_OK);

  smsService->GetNumberOfMessagesForText(aText, aResult);

  return NS_OK;
}

nsresult
SmsManager::Send(JSContext* aCx, JSObject* aGlobal, JSString* aNumber,
                 const nsAString& aMessage, jsval* aRequest)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  if (!smsService) {
    NS_ERROR("No SMS Service!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMMozSmsRequest> request = SmsRequest::Create(this);
  nsDependentJSString number;
  number.init(aCx, aNumber);

  nsCOMPtr<nsISmsRequest> forwarder =
    new SmsRequestForwarder(static_cast<SmsRequest*>(request.get()));

  smsService->Send(number, aMessage, forwarder);

  nsresult rv = nsContentUtils::WrapNative(aCx, aGlobal, request, aRequest);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create the js value!");
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
SmsManager::Send(const jsval& aNumber, const nsAString& aMessage, jsval* aReturn)
{
  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_STATE(sc);
  JSContext* cx = sc->GetNativeContext();
  NS_ASSERTION(cx, "Failed to get a context!");

  if (!aNumber.isString() &&
      !(aNumber.isObject() && JS_IsArrayObject(cx, &aNumber.toObject()))) {
    return NS_ERROR_INVALID_ARG;
  }

  JSObject* global = sc->GetNativeGlobal();
  NS_ASSERTION(global, "Failed to get global object!");

  JSAutoRequest ar(cx);
  JSAutoCompartment ac(cx, global);

  if (aNumber.isString()) {
    return Send(cx, global, aNumber.toString(), aMessage, aReturn);
  }

  // Must be an array then.
  JSObject& numbers = aNumber.toObject();

  uint32_t size;
  JS_ALWAYS_TRUE(JS_GetArrayLength(cx, &numbers, &size));

  jsval* requests = new jsval[size];

  for (uint32_t i=0; i<size; ++i) {
    jsval number;
    if (!JS_GetElement(cx, &numbers, i, &number)) {
      return NS_ERROR_INVALID_ARG;
    }

    nsresult rv = Send(cx, global, number.toString(), aMessage, &requests[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aReturn->setObjectOrNull(JS_NewArrayObject(cx, size, requests));
  NS_ENSURE_TRUE(aReturn->isObject(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
SmsManager::GetMessageMoz(int32_t aId, nsIDOMMozSmsRequest** aRequest)
{
  nsCOMPtr<nsIDOMMozSmsRequest> req = SmsRequest::Create(this);
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);
  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(static_cast<SmsRequest*>(req.get()));
  smsDBService->GetMessageMoz(aId, forwarder);
  req.forget(aRequest);
  return NS_OK;
}

nsresult
SmsManager::Delete(int32_t aId, nsIDOMMozSmsRequest** aRequest)
{
  nsCOMPtr<nsIDOMMozSmsRequest> req = SmsRequest::Create(this);
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);

  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(static_cast<SmsRequest*>(req.get()));
  smsDBService->DeleteMessage(aId, forwarder);
  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsManager::Delete(const jsval& aParam, nsIDOMMozSmsRequest** aRequest)
{
  if (aParam.isInt32()) {
    return Delete(aParam.toInt32(), aRequest);
  }

  if (!aParam.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_STATE(sc);
  nsCOMPtr<nsIDOMMozSmsMessage> message =
    do_QueryInterface(nsContentUtils::XPConnect()->GetNativeOfWrapper(
          sc->GetNativeContext(), &aParam.toObject()));
  NS_ENSURE_TRUE(message, NS_ERROR_INVALID_ARG);

  int32_t id;
  message->GetId(&id);

  return Delete(id, aRequest);
}

NS_IMETHODIMP
SmsManager::GetMessages(nsIDOMMozSmsFilter* aFilter, bool aReverse,
                        nsIDOMMozSmsRequest** aRequest)
{
  nsCOMPtr<nsIDOMMozSmsFilter> filter = aFilter;
  
  if (!filter) {
    filter = new SmsFilter();
  }

  nsCOMPtr<nsIDOMMozSmsRequest> req = SmsRequest::Create(this);
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);
  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(static_cast<SmsRequest*>(req.get()));
  smsDBService->CreateMessageList(filter, aReverse, forwarder);
  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsManager::MarkMessageRead(int32_t aId, bool aValue,
                            nsIDOMMozSmsRequest** aRequest)
{
  nsCOMPtr<nsIDOMMozSmsRequest> req = SmsRequest::Create(this);
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);
  nsCOMPtr<nsISmsRequest> forwarder =
    new SmsRequestForwarder(static_cast<SmsRequest*>(req.get()));
  smsDBService->MarkMessageRead(aId, aValue, forwarder);
  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsManager::GetThreadList(nsIDOMMozSmsRequest** aRequest)
{
  nsCOMPtr<nsIDOMMozSmsRequest> req = SmsRequest::Create(this);
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);
  nsCOMPtr<nsISmsRequest> forwarder =
    new SmsRequestForwarder(static_cast<SmsRequest*>(req.get()));
  smsDBService->GetThreadList(forwarder);
  req.forget(aRequest);
  return NS_OK;
}

nsresult
SmsManager::DispatchTrustedSmsEventToSelf(const nsAString& aEventName, nsIDOMMozSmsMessage* aMessage)
{
  nsRefPtr<nsDOMEvent> event = new SmsEvent(nullptr, nullptr);
  nsresult rv = static_cast<SmsEvent*>(event.get())->Init(aEventName, false,
                                                          false, aMessage);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
SmsManager::Observe(nsISupports* aSubject, const char* aTopic,
                    const PRUnichar* aData)
{
  if (!strcmp(aTopic, kSmsReceivedObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-received' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedSmsEventToSelf(RECEIVED_EVENT_NAME, message);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsSendingObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-sending' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedSmsEventToSelf(SENDING_EVENT_NAME, message);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsSentObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-sent' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedSmsEventToSelf(SENT_EVENT_NAME, message);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsFailedObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-failed' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedSmsEventToSelf(FAILED_EVENT_NAME, message);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsDeliverySuccessObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-delivery-success' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedSmsEventToSelf(DELIVERY_SUCCESS_EVENT_NAME, message);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsDeliveryErrorObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-delivery-error' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedSmsEventToSelf(DELIVERY_ERROR_EVENT_NAME, message);
    return NS_OK;
  }

  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
