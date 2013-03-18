/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsFilter.h"
#include "MobileMessageManager.h"
#include "nsIDOMClassInfo.h"
#include "nsISmsService.h"
#include "nsIMmsService.h"
#include "nsIObserverService.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "Constants.h"
#include "nsIDOMMozSmsEvent.h"
#include "nsIDOMMozMmsEvent.h"
#include "nsIDOMMozSmsMessage.h"
#include "nsIDOMMozMmsMessage.h"
#include "SmsRequest.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsIMobileMessageDatabaseService.h"
#include "nsIXPConnect.h"
#include "nsIPermissionManager.h"
#include "GeneratedEvents.h"
#include "DOMRequest.h"
#include "nsIMobileMessageCallback.h"
#include "MobileMessageCallback.h"

#define RECEIVED_EVENT_NAME         NS_LITERAL_STRING("received")
#define SENDING_EVENT_NAME          NS_LITERAL_STRING("sending")
#define SENT_EVENT_NAME             NS_LITERAL_STRING("sent")
#define FAILED_EVENT_NAME           NS_LITERAL_STRING("failed")
#define DELIVERY_SUCCESS_EVENT_NAME NS_LITERAL_STRING("deliverysuccess")
#define DELIVERY_ERROR_EVENT_NAME   NS_LITERAL_STRING("deliveryerror")

using namespace mozilla::dom::mobilemessage;

DOMCI_DATA(MozMobileMessageManager, mozilla::dom::MobileMessageManager)

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(MobileMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozMobileMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozMobileMessageManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MobileMessageManager, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MobileMessageManager, nsDOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(MobileMessageManager, received)
NS_IMPL_EVENT_HANDLER(MobileMessageManager, sending)
NS_IMPL_EVENT_HANDLER(MobileMessageManager, sent)
NS_IMPL_EVENT_HANDLER(MobileMessageManager, failed)
NS_IMPL_EVENT_HANDLER(MobileMessageManager, deliverysuccess)
NS_IMPL_EVENT_HANDLER(MobileMessageManager, deliveryerror)

void
MobileMessageManager::Init(nsPIDOMWindow *aWindow)
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

  obs->AddObserver(this, kMmsSendingObserverTopic, false);
  obs->AddObserver(this, kMmsSentObserverTopic, false);
  obs->AddObserver(this, kMmsFailedObserverTopic, false);
  obs->AddObserver(this, kMmsReceivedObserverTopic, false);
}

void
MobileMessageManager::Shutdown()
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

  obs->RemoveObserver(this, kMmsSendingObserverTopic);
  obs->RemoveObserver(this, kMmsSentObserverTopic);
  obs->RemoveObserver(this, kMmsFailedObserverTopic);
  obs->RemoveObserver(this, kMmsReceivedObserverTopic);
}

NS_IMETHODIMP
MobileMessageManager::GetSegmentInfoForText(const nsAString& aText,
                                            nsIDOMMozSmsSegmentInfo** aResult)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, NS_ERROR_FAILURE);

  return smsService->GetSegmentInfoForText(aText, aResult);
}

nsresult
MobileMessageManager::Send(JSContext* aCx, JSObject* aGlobal, JSString* aNumber,
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

  nsCOMPtr<nsIMobileMessageCallback> forwarder =
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
MobileMessageManager::Send(const jsval& aNumber, const nsAString& aMessage, jsval* aReturn)
{
  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_STATE(sc);
  AutoPushJSContext cx(sc->GetNativeContext());
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
MobileMessageManager::SendMMS(const JS::Value& aParams, nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIMmsService> mmsService = do_GetService(RIL_MMSSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mmsService, NS_ERROR_FAILURE);

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsCOMPtr<nsIMobileMessageCallback> msgCallback = new MobileMessageCallback(request);
  nsresult rv = mmsService->Send(aParams, msgCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageManager::GetMessageMoz(int32_t aId, nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIMobileMessageDatabaseService> mobileMessageDBService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mobileMessageDBService, NS_ERROR_FAILURE);

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsCOMPtr<nsIMobileMessageCallback> msgCallback = new MobileMessageCallback(request);
  nsresult rv = mobileMessageDBService->GetMessageMoz(aId, msgCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(aRequest);
  return NS_OK;
}

nsresult
MobileMessageManager::Delete(int32_t aId, nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIMobileMessageDatabaseService> mobileMessageDBService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mobileMessageDBService, NS_ERROR_FAILURE);

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsCOMPtr<nsIMobileMessageCallback> msgCallback = new MobileMessageCallback(request);
  nsresult rv = mobileMessageDBService->DeleteMessage(aId, msgCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageManager::Delete(const jsval& aParam, nsIDOMDOMRequest** aRequest)
{
  if (aParam.isInt32()) {
    return Delete(aParam.toInt32(), aRequest);
  }

  if (!aParam.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  AutoPushJSContext cx(sc->GetNativeContext());
  NS_ENSURE_STATE(sc);

  int32_t id;
  nsCOMPtr<nsIDOMMozSmsMessage> smsMessage =
    do_QueryInterface(nsContentUtils::XPConnect()->GetNativeOfWrapper(cx, &aParam.toObject()));
  if (smsMessage) {
    smsMessage->GetId(&id);
  } else {
    nsCOMPtr<nsIDOMMozMmsMessage> mmsMessage =
      do_QueryInterface(nsContentUtils::XPConnect()->GetNativeOfWrapper(cx, &aParam.toObject()));
    if (mmsMessage) {
      mmsMessage->GetId(&id);
    } else {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return Delete(id, aRequest);
}

NS_IMETHODIMP
MobileMessageManager::GetMessages(nsIDOMMozSmsFilter* aFilter, bool aReverse,
                                  nsIDOMMozSmsRequest** aRequest)
{
  nsCOMPtr<nsIDOMMozSmsFilter> filter = aFilter;
  
  if (!filter) {
    filter = new SmsFilter();
  }

  nsCOMPtr<nsIDOMMozSmsRequest> req = SmsRequest::Create(this);
  nsCOMPtr<nsIMobileMessageDatabaseService> mobileMessageDBService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mobileMessageDBService, NS_ERROR_FAILURE);
  nsCOMPtr<nsIMobileMessageCallback> forwarder =
    new SmsRequestForwarder(static_cast<SmsRequest*>(req.get()));
  mobileMessageDBService->CreateMessageList(filter, aReverse, forwarder);
  req.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageManager::MarkMessageRead(int32_t aId, bool aValue,
                                      nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIMobileMessageDatabaseService> mobileMessageDBService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mobileMessageDBService, NS_ERROR_FAILURE);

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsCOMPtr<nsIMobileMessageCallback> msgCallback = new MobileMessageCallback(request);
  nsresult rv = mobileMessageDBService->MarkMessageRead(aId, aValue, msgCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageManager::GetThreadList(nsIDOMMozSmsRequest** aRequest)
{
  nsCOMPtr<nsIDOMMozSmsRequest> req = SmsRequest::Create(this);
  nsCOMPtr<nsIMobileMessageDatabaseService> mobileMessageDBService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mobileMessageDBService, NS_ERROR_FAILURE);
  nsCOMPtr<nsIMobileMessageCallback> forwarder =
    new SmsRequestForwarder(static_cast<SmsRequest*>(req.get()));
  mobileMessageDBService->GetThreadList(forwarder);
  req.forget(aRequest);
  return NS_OK;
}

nsresult
MobileMessageManager::DispatchTrustedSmsEventToSelf(const nsAString& aEventName,
                                                    nsIDOMMozSmsMessage* aMessage)
{
  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMMozSmsEvent(getter_AddRefs(event), this, nullptr, nullptr);
  NS_ASSERTION(event, "This should never fail!");

  nsCOMPtr<nsIDOMMozSmsEvent> se = do_QueryInterface(event);
  nsresult rv = se->InitMozSmsEvent(aEventName, false, false, aMessage);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchTrustedEvent(event);
}

nsresult
MobileMessageManager::DispatchTrustedMmsEventToSelf(const nsAString& aEventName,
                                                    nsIDOMMozMmsMessage* aMessage)
{
  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMMozMmsEvent(getter_AddRefs(event), this, nullptr, nullptr);
  NS_ASSERTION(event, "This should never fail!");

  nsCOMPtr<nsIDOMMozMmsEvent> se = do_QueryInterface(event);
  nsresult rv = se->InitMozMmsEvent(aEventName, false, false, aMessage);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchTrustedEvent(event);
}

NS_IMETHODIMP
MobileMessageManager::Observe(nsISupports* aSubject, const char* aTopic,
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

  if (!strcmp(aTopic, kMmsSendingObserverTopic)) {
    nsCOMPtr<nsIDOMMozMmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'mms-sending' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedMmsEventToSelf(SENDING_EVENT_NAME, message);
    return NS_OK;
  }

  if (!strcmp(aTopic, kMmsSentObserverTopic)) {
    nsCOMPtr<nsIDOMMozMmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'mms-sent' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedMmsEventToSelf(SENT_EVENT_NAME, message);
    return NS_OK;
  }

  if (!strcmp(aTopic, kMmsFailedObserverTopic)) {
    nsCOMPtr<nsIDOMMozMmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'mms-failed' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedMmsEventToSelf(FAILED_EVENT_NAME, message);
    return NS_OK;
  }

  if (!strcmp(aTopic, kMmsReceivedObserverTopic)) {
    nsCOMPtr<nsIDOMMozMmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'mms-received' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedMmsEventToSelf(RECEIVED_EVENT_NAME, message);
    return NS_OK;
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
