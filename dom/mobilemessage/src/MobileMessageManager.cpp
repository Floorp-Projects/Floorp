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
#include "mozilla/dom/mobilemessage/Constants.h" // For MessageType
#include "nsIDOMMozSmsEvent.h"
#include "nsIDOMMozMmsEvent.h"
#include "nsIDOMMozSmsMessage.h"
#include "nsIDOMMozMmsMessage.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsIMobileMessageDatabaseService.h"
#include "nsIXPConnect.h"
#include "nsIPermissionManager.h"
#include "GeneratedEvents.h"
#include "DOMRequest.h"
#include "nsIMobileMessageCallback.h"
#include "MobileMessageCallback.h"
#include "MobileMessageCursorCallback.h"
#include "DOMCursor.h"

#define RECEIVED_EVENT_NAME         NS_LITERAL_STRING("received")
#define RETRIEVING_EVENT_NAME       NS_LITERAL_STRING("retrieving")
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
NS_IMPL_EVENT_HANDLER(MobileMessageManager, retrieving)
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
  obs->AddObserver(this, kSmsRetrievingObserverTopic, false);
  obs->AddObserver(this, kSmsSendingObserverTopic, false);
  obs->AddObserver(this, kSmsSentObserverTopic, false);
  obs->AddObserver(this, kSmsFailedObserverTopic, false);
  obs->AddObserver(this, kSmsDeliverySuccessObserverTopic, false);
  obs->AddObserver(this, kSmsDeliveryErrorObserverTopic, false);
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
  obs->RemoveObserver(this, kSmsRetrievingObserverTopic);
  obs->RemoveObserver(this, kSmsSendingObserverTopic);
  obs->RemoveObserver(this, kSmsSentObserverTopic);
  obs->RemoveObserver(this, kSmsFailedObserverTopic);
  obs->RemoveObserver(this, kSmsDeliverySuccessObserverTopic);
  obs->RemoveObserver(this, kSmsDeliveryErrorObserverTopic);
}

NS_IMETHODIMP
MobileMessageManager::GetSegmentInfoForText(const nsAString& aText,
                                            nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, NS_ERROR_FAILURE);

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsCOMPtr<nsIMobileMessageCallback> msgCallback =
    new MobileMessageCallback(request);
  nsresult rv = smsService->GetSegmentInfoForText(aText, msgCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(aRequest);
  return NS_OK;
}

nsresult
MobileMessageManager::Send(JSContext* aCx, JS::Handle<JSObject*> aGlobal,
                           JS::Handle<JSString*> aNumber,
                           const nsAString& aMessage, JS::Value* aRequest)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, NS_ERROR_FAILURE);

  nsDependentJSString number;
  number.init(aCx, aNumber);

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsCOMPtr<nsIMobileMessageCallback> msgCallback =
    new MobileMessageCallback(request);

  // By default, we don't send silent messages via MobileMessageManager.
  nsresult rv = smsService->Send(number, aMessage, false, msgCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JSObject*> global(aCx, aGlobal);
  JS::Rooted<JS::Value> rval(aCx);
  rv = nsContentUtils::WrapNative(aCx, global,
                                  static_cast<nsIDOMDOMRequest*>(request.get()),
                                  &rval);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create the js value!");
    return rv;
  }

  *aRequest = rval;
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageManager::Send(const JS::Value& aNumber_, const nsAString& aMessage, JS::Value* aReturn)
{
  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_STATE(sc);
  AutoPushJSContext cx(sc->GetNativeContext());
  NS_ASSERTION(cx, "Failed to get a context!");

  JS::Rooted<JS::Value> aNumber(cx, aNumber_);
  if (!aNumber.isString() &&
      !(aNumber.isObject() && JS_IsArrayObject(cx, &aNumber.toObject()))) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> global(cx, sc->GetWindowProxy());
  NS_ASSERTION(global, "Failed to get global object!");

  JSAutoCompartment ac(cx, global);

  if (aNumber.isString()) {
    JS::Rooted<JSString*> str(cx, aNumber.toString());
    return Send(cx, global, str, aMessage, aReturn);
  }

  // Must be an array then.
  JS::Rooted<JSObject*> numbers(cx, &aNumber.toObject());

  uint32_t size;
  JS_ALWAYS_TRUE(JS_GetArrayLength(cx, numbers, &size));

  JS::Value* requests = new JS::Value[size];

  JS::Rooted<JS::Value> number(cx);
  for (uint32_t i=0; i<size; ++i) {
    if (!JS_GetElement(cx, numbers, i, &number)) {
      return NS_ERROR_INVALID_ARG;
    }

    JS::Rooted<JSString*> str(cx, number.toString());
    nsresult rv = Send(cx, global, str, aMessage, &requests[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aReturn->setObjectOrNull(JS_NewArrayObject(cx, size, requests));
  NS_ENSURE_TRUE(aReturn->isObject(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
MobileMessageManager::SendMMS(const JS::Value& aParams, nsIDOMDOMRequest** aRequest)
{
  nsCOMPtr<nsIMmsService> mmsService = do_GetService(MMS_SERVICE_CONTRACTID);
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
MobileMessageManager::GetMessageId(AutoPushJSContext &aCx,
                                   const JS::Value &aMessage, int32_t &aId)
{
  nsCOMPtr<nsIDOMMozSmsMessage> smsMessage =
    do_QueryInterface(nsContentUtils::XPConnect()->GetNativeOfWrapper(aCx, &aMessage.toObject()));
  if (smsMessage) {
    return smsMessage->GetId(&aId);
  }

  nsCOMPtr<nsIDOMMozMmsMessage> mmsMessage =
    do_QueryInterface(nsContentUtils::XPConnect()->GetNativeOfWrapper(aCx, &aMessage.toObject()));
  if (mmsMessage) {
    return mmsMessage->GetId(&aId);
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
MobileMessageManager::Delete(const JS::Value& aParam, nsIDOMDOMRequest** aRequest)
{
  // We expect Int32, SmsMessage, MmsMessage, Int32[], SmsMessage[], MmsMessage[]
  if (!aParam.isObject() && !aParam.isInt32()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  AutoPushJSContext cx(sc->GetNativeContext());
  NS_ENSURE_STATE(sc);

  int32_t id, *idArray;
  uint32_t size;

  if (aParam.isInt32()) {
    // Single Integer Message ID
    id = aParam.toInt32();

    size = 1;
    idArray = &id;
  } else if (!JS_IsArrayObject(cx, &aParam.toObject())) {
    // Single SmsMessage/MmsMessage object
    rv = GetMessageId(cx, aParam, id);
    NS_ENSURE_SUCCESS(rv, rv);

    size = 1;
    idArray = &id;
  } else {
    // Int32[], SmsMessage[], or MmsMessage[]
    JS::Rooted<JSObject*> ids(cx, &aParam.toObject());

    JS_ALWAYS_TRUE(JS_GetArrayLength(cx, ids, &size));
    nsAutoArrayPtr<int32_t> idAutoArray(new int32_t[size]);

    JS::Rooted<JS::Value> idJsValue(cx);
    for (uint32_t i = 0; i < size; i++) {
      if (!JS_GetElement(cx, ids, i, &idJsValue)) {
        return NS_ERROR_INVALID_ARG;
      }

      if (idJsValue.isInt32()) {
        idAutoArray[i] = idJsValue.toInt32();
      } else if (idJsValue.isObject()) {
        rv = GetMessageId(cx, idJsValue, id);
        NS_ENSURE_SUCCESS(rv, rv);

        idAutoArray[i] = id;
      }
    }

    idArray = idAutoArray.forget();
  }

  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(dbService, NS_ERROR_FAILURE);

  nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
  nsCOMPtr<nsIMobileMessageCallback> msgCallback =
    new MobileMessageCallback(request);

  rv = dbService->DeleteMessage(idArray, size, msgCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageManager::GetMessages(nsIDOMMozSmsFilter* aFilter,
                                  bool aReverse,
                                  nsIDOMDOMCursor** aCursor)
{
  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(dbService, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMMozSmsFilter> filter = aFilter;
  if (!filter) {
    filter = new SmsFilter();
  }

  nsRefPtr<MobileMessageCursorCallback> cursorCallback =
    new MobileMessageCursorCallback();

  nsCOMPtr<nsICursorContinueCallback> continueCallback;
  nsresult rv = dbService->CreateMessageCursor(filter, aReverse, cursorCallback,
                                               getter_AddRefs(continueCallback));
  NS_ENSURE_SUCCESS(rv, rv);

  cursorCallback->mDOMCursor = new DOMCursor(GetOwner(), continueCallback);
  NS_ADDREF(*aCursor = cursorCallback->mDOMCursor);

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
MobileMessageManager::GetThreads(nsIDOMDOMCursor** aCursor)
{
  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(dbService, NS_ERROR_FAILURE);

  nsRefPtr<MobileMessageCursorCallback> cursorCallback =
    new MobileMessageCursorCallback();

  nsCOMPtr<nsICursorContinueCallback> continueCallback;
  nsresult rv = dbService->CreateThreadCursor(cursorCallback,
                                              getter_AddRefs(continueCallback));
  NS_ENSURE_SUCCESS(rv, rv);

  cursorCallback->mDOMCursor = new DOMCursor(GetOwner(), continueCallback);
  NS_ADDREF(*aCursor = cursorCallback->mDOMCursor);

  return NS_OK;
}

NS_IMETHODIMP
MobileMessageManager::RetrieveMMS(int32_t id,
                                  nsIDOMDOMRequest** aRequest)
{
    nsCOMPtr<nsIMmsService> mmsService = do_GetService(MMS_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(mmsService, NS_ERROR_FAILURE);

    nsRefPtr<DOMRequest> request = new DOMRequest(GetOwner());
    nsCOMPtr<nsIMobileMessageCallback> msgCallback = new MobileMessageCallback(request);

    nsresult rv = mmsService->Retrieve(id, msgCallback);
    NS_ENSURE_SUCCESS(rv, rv);

    request.forget(aRequest);
    return NS_OK;
}

nsresult
MobileMessageManager::DispatchTrustedSmsEventToSelf(const char* aTopic,
                                                    const nsAString& aEventName,
                                                    nsISupports* aMsg)
{
  nsCOMPtr<nsIDOMEvent> event;

  nsCOMPtr<nsIDOMMozSmsMessage> sms = do_QueryInterface(aMsg);
  if (sms) {
    NS_NewDOMMozSmsEvent(getter_AddRefs(event), this, nullptr, nullptr);
    NS_ASSERTION(event, "This should never fail!");

    nsCOMPtr<nsIDOMMozSmsEvent> se = do_QueryInterface(event);
    nsresult rv = se->InitMozSmsEvent(aEventName, false, false, sms);
    NS_ENSURE_SUCCESS(rv, rv);
    return DispatchTrustedEvent(event);
  }

  nsCOMPtr<nsIDOMMozMmsMessage> mms = do_QueryInterface(aMsg);
  if (mms) {
    NS_NewDOMMozMmsEvent(getter_AddRefs(event), this, nullptr, nullptr);
    NS_ASSERTION(event, "This should never fail!");

    nsCOMPtr<nsIDOMMozMmsEvent> se = do_QueryInterface(event);
    nsresult rv = se->InitMozMmsEvent(aEventName, false, false, mms);
    NS_ENSURE_SUCCESS(rv, rv);
    return DispatchTrustedEvent(event);
  }

  nsAutoCString errorMsg;
  errorMsg.AssignLiteral("Got a '");
  errorMsg.Append(aTopic);
  errorMsg.AppendLiteral("' topic without a valid message!");
  NS_ERROR(errorMsg.get());
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageManager::Observe(nsISupports* aSubject, const char* aTopic,
                              const PRUnichar* aData)
{
  if (!strcmp(aTopic, kSmsReceivedObserverTopic)) {
    return DispatchTrustedSmsEventToSelf(aTopic, RECEIVED_EVENT_NAME, aSubject);
  }

  if (!strcmp(aTopic, kSmsRetrievingObserverTopic)) {
    return DispatchTrustedSmsEventToSelf(aTopic, RETRIEVING_EVENT_NAME, aSubject);
  }

  if (!strcmp(aTopic, kSmsSendingObserverTopic)) {
    return DispatchTrustedSmsEventToSelf(aTopic, SENDING_EVENT_NAME, aSubject);
  }

  if (!strcmp(aTopic, kSmsSentObserverTopic)) {
    return DispatchTrustedSmsEventToSelf(aTopic, SENT_EVENT_NAME, aSubject);
  }

  if (!strcmp(aTopic, kSmsFailedObserverTopic)) {
    return DispatchTrustedSmsEventToSelf(aTopic, FAILED_EVENT_NAME, aSubject);
  }

  if (!strcmp(aTopic, kSmsDeliverySuccessObserverTopic)) {
    return DispatchTrustedSmsEventToSelf(aTopic, DELIVERY_SUCCESS_EVENT_NAME, aSubject);
  }

  if (!strcmp(aTopic, kSmsDeliveryErrorObserverTopic)) {
    return DispatchTrustedSmsEventToSelf(aTopic, DELIVERY_ERROR_EVENT_NAME, aSubject);
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
