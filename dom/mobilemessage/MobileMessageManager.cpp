/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageManager.h"

#include "DeletedMessageInfo.h"
#include "DOMCursor.h"
#include "DOMRequest.h"
#include "MobileMessageCallback.h"
#include "MobileMessageCursorCallback.h"
#include "mozilla/dom/mobilemessage/Constants.h" // For kSms*ObserverTopic
#include "mozilla/dom/MozMessageDeletedEvent.h"
#include "mozilla/dom/MozMmsEvent.h"
#include "mozilla/dom/MozMobileMessageManagerBinding.h"
#include "mozilla/dom/MozSmsEvent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsIDOMMozMmsMessage.h"
#include "nsIDOMMozSmsMessage.h"
#include "nsIMmsService.h"
#include "nsIMobileMessageCallback.h"
#include "nsIMobileMessageDatabaseService.h"
#include "nsIMobileMessageService.h"
#include "nsIObserverService.h"
#include "nsISmsService.h"
#include "nsServiceManagerUtils.h" // For do_GetService()

// Service instantiation
#include "ipc/SmsIPCService.h"
#include "MobileMessageService.h"
#ifdef MOZ_WIDGET_ANDROID
#include "android/MobileMessageDatabaseService.h"
#include "android/SmsService.h"
#elif defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
#include "nsIGonkMobileMessageDatabaseService.h"
#include "nsIGonkSmsService.h"
#endif
#include "nsXULAppAPI.h" // For XRE_GetProcessType()

#define RECEIVED_EVENT_NAME         NS_LITERAL_STRING("received")
#define RETRIEVING_EVENT_NAME       NS_LITERAL_STRING("retrieving")
#define SENDING_EVENT_NAME          NS_LITERAL_STRING("sending")
#define SENT_EVENT_NAME             NS_LITERAL_STRING("sent")
#define FAILED_EVENT_NAME           NS_LITERAL_STRING("failed")
#define DELIVERY_SUCCESS_EVENT_NAME NS_LITERAL_STRING("deliverysuccess")
#define DELIVERY_ERROR_EVENT_NAME   NS_LITERAL_STRING("deliveryerror")
#define READ_SUCCESS_EVENT_NAME     NS_LITERAL_STRING("readsuccess")
#define READ_ERROR_EVENT_NAME       NS_LITERAL_STRING("readerror")
#define DELETED_EVENT_NAME          NS_LITERAL_STRING("deleted")

using namespace mozilla::dom::mobilemessage;

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(MobileMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MobileMessageManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MobileMessageManager, DOMEventTargetHelper)

MobileMessageManager::MobileMessageManager(nsPIDOMWindow *aWindow)
  : DOMEventTargetHelper(aWindow)
{
}

void
MobileMessageManager::Init()
{
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
  obs->AddObserver(this, kSmsReadSuccessObserverTopic, false);
  obs->AddObserver(this, kSmsReadErrorObserverTopic, false);
  obs->AddObserver(this, kSmsDeletedObserverTopic, false);
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
  obs->RemoveObserver(this, kSmsReadSuccessObserverTopic);
  obs->RemoveObserver(this, kSmsReadErrorObserverTopic);
  obs->RemoveObserver(this, kSmsDeletedObserverTopic);
}

JSObject*
MobileMessageManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozMobileMessageManagerBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DOMRequest>
MobileMessageManager::GetSegmentInfoForText(const nsAString& aText,
                                            ErrorResult& aRv)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  if (!smsService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(window);
  nsCOMPtr<nsIMobileMessageCallback> msgCallback =
    new MobileMessageCallback(request);
  nsresult rv = smsService->GetSegmentInfoForText(aText, msgCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
MobileMessageManager::Send(nsISmsService* aSmsService,
                           uint32_t aServiceId,
                           const nsAString& aNumber,
                           const nsAString& aText,
                           ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(window);
  nsCOMPtr<nsIMobileMessageCallback> msgCallback =
    new MobileMessageCallback(request);

  // By default, we don't send silent messages via MobileMessageManager.
  nsresult rv = aSmsService->Send(aServiceId, aNumber, aText,
                                  false, msgCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
MobileMessageManager::Send(const nsAString& aNumber,
                           const nsAString& aText,
                           const SmsSendParameters& aSendParams,
                           ErrorResult& aRv)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  if (!smsService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Use the default one unless |aSendParams.serviceId| is available.
  uint32_t serviceId;
  if (aSendParams.mServiceId.WasPassed()) {
    serviceId = aSendParams.mServiceId.Value();
  } else {
    nsresult rv = smsService->GetSmsDefaultServiceId(&serviceId);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
  }

  return Send(smsService, serviceId, aNumber, aText, aRv);
}

void
MobileMessageManager::Send(const Sequence<nsString>& aNumbers,
                           const nsAString& aText,
                           const SmsSendParameters& aSendParams,
                           nsTArray<nsRefPtr<DOMRequest>>& aReturn,
                           ErrorResult& aRv)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  if (!smsService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Use the default one unless |aSendParams.serviceId| is available.
  uint32_t serviceId;
  if (aSendParams.mServiceId.WasPassed()) {
    serviceId = aSendParams.mServiceId.Value();
  } else {
    nsresult rv = smsService->GetSmsDefaultServiceId(&serviceId);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  }

  const uint32_t size = aNumbers.Length();
  for (uint32_t i = 0; i < size; ++i) {
    nsRefPtr<DOMRequest> request = Send(smsService, serviceId, aNumbers[i], aText, aRv);
    if (aRv.Failed()) {
      return;
    }
    aReturn.AppendElement(request);
  }
}

already_AddRefed<DOMRequest>
MobileMessageManager::SendMMS(const MmsParameters& aParams,
                              const MmsSendParameters& aSendParams,
                              ErrorResult& aRv)
{
  nsCOMPtr<nsIMmsService> mmsService = do_GetService(MMS_SERVICE_CONTRACTID);
  if (!mmsService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Use the default one unless |aSendParams.serviceId| is available.
  uint32_t serviceId;
  nsresult rv;
  if (aSendParams.mServiceId.WasPassed()) {
    serviceId = aSendParams.mServiceId.Value();
  } else {
    rv = mmsService->GetMmsDefaultServiceId(&serviceId);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(window))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JSContext *cx = jsapi.cx();
  JS::Rooted<JS::Value> val(cx);
  if (!ToJSValue(cx, aParams, &val)) {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(window);
  nsCOMPtr<nsIMobileMessageCallback> msgCallback = new MobileMessageCallback(request);
  rv = mmsService->Send(serviceId, val, msgCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
MobileMessageManager::GetMessage(int32_t aId,
                                 ErrorResult& aRv)
{
  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  if (!dbService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(window);
  nsCOMPtr<nsIMobileMessageCallback> msgCallback = new MobileMessageCallback(request);
  nsresult rv = dbService->GetMessageMoz(aId, msgCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
MobileMessageManager::Delete(int32_t* aIdArray,
                             uint32_t aSize,
                             ErrorResult& aRv)
{
  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  if (!dbService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(window);
  nsCOMPtr<nsIMobileMessageCallback> msgCallback =
    new MobileMessageCallback(request);

  nsresult rv = dbService->DeleteMessage(aIdArray, aSize, msgCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
MobileMessageManager::Delete(int32_t aId,
                             ErrorResult& aRv)
{
  return Delete(&aId, 1, aRv);
}

already_AddRefed<DOMRequest>
MobileMessageManager::Delete(nsIDOMMozSmsMessage* aMessage,
                             ErrorResult& aRv)
{
  int32_t id;

  DebugOnly<nsresult> rv = aMessage->GetId(&id);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return Delete(id, aRv);
}

already_AddRefed<DOMRequest>
MobileMessageManager::Delete(nsIDOMMozMmsMessage* aMessage,
                             ErrorResult& aRv)
{
  int32_t id;

  DebugOnly<nsresult> rv = aMessage->GetId(&id);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return Delete(id, aRv);
}

already_AddRefed<DOMRequest>
MobileMessageManager::Delete(const Sequence<OwningLongOrMozSmsMessageOrMozMmsMessage>& aParams,
                             ErrorResult& aRv)
{
  const uint32_t size = aParams.Length();
  FallibleTArray<int32_t> idArray;
  if (!idArray.SetLength(size, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  DebugOnly<nsresult> rv;
  for (uint32_t i = 0; i < size; i++) {
    const OwningLongOrMozSmsMessageOrMozMmsMessage& element = aParams[i];
    int32_t &id = idArray[i];

    if (element.IsLong()) {
      id = element.GetAsLong();
    } else if (element.IsMozMmsMessage()) {
      rv = element.GetAsMozMmsMessage()->GetId(&id);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    } else /*if (element.IsMozSmsMessage())*/ {
      rv = element.GetAsMozSmsMessage()->GetId(&id);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  return Delete(idArray.Elements(), size, aRv);
}

already_AddRefed<DOMCursor>
MobileMessageManager::GetMessages(const MobileMessageFilter& aFilter,
                                  bool aReverse,
                                  ErrorResult& aRv)
{
  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  if (!dbService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  bool hasStartDate = !aFilter.mStartDate.IsNull();
  uint64_t startDate = 0;
  if (hasStartDate) {
    startDate = aFilter.mStartDate.Value();
  }

  bool hasEndDate = !aFilter.mEndDate.IsNull();
  uint64_t endDate = 0;
  if (hasEndDate) {
    endDate = aFilter.mEndDate.Value();
  }

  nsAutoArrayPtr<const char16_t*> ptrNumbers;
  uint32_t numbersCount = 0;
  if (!aFilter.mNumbers.IsNull() &&
      aFilter.mNumbers.Value().Length()) {
    const FallibleTArray<nsString>& numbers = aFilter.mNumbers.Value();
    uint32_t index;

    numbersCount = numbers.Length();
    ptrNumbers = new const char16_t* [numbersCount];
    for (index = 0; index < numbersCount; index++) {
      ptrNumbers[index] = numbers[index].get();
    }
  }

  nsString delivery;
  delivery.SetIsVoid(true);
  if (!aFilter.mDelivery.IsNull()) {
    const uint32_t index = static_cast<uint32_t>(aFilter.mDelivery.Value());
    const EnumEntry& entry =
      MobileMessageFilterDeliveryValues::strings[index];
    delivery.AssignASCII(entry.value, entry.length);
  }

  bool hasRead = !aFilter.mRead.IsNull();
  bool read = false;
  if (hasRead) {
    read = aFilter.mRead.Value();
  }

  uint64_t threadId = 0;
  if (!aFilter.mThreadId.IsNull()) {
    threadId = aFilter.mThreadId.Value();
  }

  nsRefPtr<MobileMessageCursorCallback> cursorCallback =
    new MobileMessageCursorCallback();
  nsCOMPtr<nsICursorContinueCallback> continueCallback;
  nsresult rv = dbService->CreateMessageCursor(hasStartDate, startDate,
                                               hasEndDate, endDate,
                                               ptrNumbers, numbersCount,
                                               delivery,
                                               hasRead, read,
                                               threadId,
                                               aReverse, cursorCallback,
                                               getter_AddRefs(continueCallback));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  cursorCallback->mDOMCursor =
    new MobileMessageCursor(window, continueCallback);

  nsRefPtr<DOMCursor> cursor(cursorCallback->mDOMCursor);
  return cursor.forget();
}

already_AddRefed<DOMRequest>
MobileMessageManager::MarkMessageRead(int32_t aId,
                                      bool aValue,
                                      bool aSendReadReport,
                                      ErrorResult& aRv)
{
  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  if (!dbService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(window);
  nsCOMPtr<nsIMobileMessageCallback> msgCallback = new MobileMessageCallback(request);
  nsresult rv = dbService->MarkMessageRead(aId, aValue, aSendReadReport,
                                           msgCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMCursor>
MobileMessageManager::GetThreads(ErrorResult& aRv)
{
  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  if (!dbService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<MobileMessageCursorCallback> cursorCallback =
    new MobileMessageCursorCallback();

  nsCOMPtr<nsICursorContinueCallback> continueCallback;
  nsresult rv = dbService->CreateThreadCursor(cursorCallback,
                                              getter_AddRefs(continueCallback));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  cursorCallback->mDOMCursor =
    new MobileMessageCursor(window, continueCallback);

  nsRefPtr<DOMCursor> cursor(cursorCallback->mDOMCursor);
  return cursor.forget();
}

already_AddRefed<DOMRequest>
MobileMessageManager::RetrieveMMS(int32_t aId,
                                  ErrorResult& aRv)
{
  nsCOMPtr<nsIMmsService> mmsService = do_GetService(MMS_SERVICE_CONTRACTID);
  if (!mmsService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(window);
  nsCOMPtr<nsIMobileMessageCallback> msgCallback = new MobileMessageCallback(request);

  nsresult rv = mmsService->Retrieve(aId, msgCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<DOMRequest>
MobileMessageManager::RetrieveMMS(nsIDOMMozMmsMessage* aMessage,
                                  ErrorResult& aRv)
{
  int32_t id;

  DebugOnly<nsresult> rv = aMessage->GetId(&id);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return RetrieveMMS(id, aRv);
}

nsresult
MobileMessageManager::DispatchTrustedSmsEventToSelf(const char* aTopic,
                                                    const nsAString& aEventName,
                                                    nsISupports* aMsg)
{
  nsCOMPtr<nsIDOMMozSmsMessage> sms = do_QueryInterface(aMsg);
  if (sms) {
    MozSmsEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;
    init.mMessage = sms;

    nsRefPtr<MozSmsEvent> event =
      MozSmsEvent::Constructor(this, aEventName, init);
    return DispatchTrustedEvent(event);
  }

  nsCOMPtr<nsIDOMMozMmsMessage> mms = do_QueryInterface(aMsg);
  if (mms) {
    MozMmsEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;
    init.mMessage = mms;

    nsRefPtr<MozMmsEvent> event =
      MozMmsEvent::Constructor(this, aEventName, init);
    return DispatchTrustedEvent(event);
  }

  nsAutoCString errorMsg;
  errorMsg.AssignLiteral("Got a '");
  errorMsg.Append(aTopic);
  errorMsg.AppendLiteral("' topic without a valid message!");
  NS_ERROR(errorMsg.get());
  return NS_OK;
}

nsresult
MobileMessageManager::DispatchTrustedDeletedEventToSelf(nsISupports* aDeletedInfo)
{
  nsCOMPtr<nsIDeletedMessageInfo> deletedInfo = do_QueryInterface(aDeletedInfo);
  if (deletedInfo) {
    MozMessageDeletedEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;
    DeletedMessageInfo* info =
      static_cast<DeletedMessageInfo*>(deletedInfo.get());

    uint32_t msgIdLength = info->GetData().deletedMessageIds().Length();
    if (msgIdLength) {
      Sequence<int32_t>& deletedMsgIds = init.mDeletedMessageIds.SetValue();
      if (!deletedMsgIds.AppendElements(info->GetData().deletedMessageIds(),
                                        fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    uint32_t threadIdLength = info->GetData().deletedThreadIds().Length();
    if (threadIdLength) {
      Sequence<uint64_t>& deletedThreadIds = init.mDeletedThreadIds.SetValue();
      if (!deletedThreadIds.AppendElements(info->GetData().deletedThreadIds(),
                                           fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    nsRefPtr<MozMessageDeletedEvent> event =
      MozMessageDeletedEvent::Constructor(this, DELETED_EVENT_NAME, init);
    return DispatchTrustedEvent(event);
  }

  NS_ERROR("Got a 'deleted' topic without a valid message!");

  return NS_OK;
}

NS_IMETHODIMP
MobileMessageManager::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData)
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

  if (!strcmp(aTopic, kSmsReadSuccessObserverTopic)) {
    return DispatchTrustedSmsEventToSelf(aTopic, READ_SUCCESS_EVENT_NAME, aSubject);
  }

  if (!strcmp(aTopic, kSmsReadErrorObserverTopic)) {
    return DispatchTrustedSmsEventToSelf(aTopic, READ_ERROR_EVENT_NAME, aSubject);
  }

  if (!strcmp(aTopic, kSmsDeletedObserverTopic)) {
    return DispatchTrustedDeletedEventToSelf(aSubject);
  }

  return NS_OK;
}

already_AddRefed<DOMRequest>
MobileMessageManager::GetSmscAddress(const Optional<uint32_t>& aServiceId,
                                     ErrorResult& aRv)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  if (!smsService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Use the default one unless |aSendParams.serviceId| is available.
  uint32_t serviceId;
  nsresult rv;
  if (aServiceId.WasPassed()) {
    serviceId = aServiceId.Value();
  } else {
    rv = smsService->GetSmsDefaultServiceId(&serviceId);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<DOMRequest> request = new DOMRequest(window);
  nsCOMPtr<nsIMobileMessageCallback> msgCallback = new MobileMessageCallback(request);
  rv = smsService->GetSmscAddress(serviceId, msgCallback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget();
}

already_AddRefed<Promise>
MobileMessageManager::SetSmscAddress(const SmscAddress& aSmscAddress,
                                     const Optional<uint32_t>& aServiceId,
                                     ErrorResult& aRv)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  if (!smsService) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Use the default one unless |serviceId| is available.
  uint32_t serviceId;
  nsresult rv;
  if (aServiceId.WasPassed()) {
    serviceId = aServiceId.Value();
  } else {
    rv = smsService->GetSmsDefaultServiceId(&serviceId);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(window);
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!aSmscAddress.mAddress.WasPassed()) {
    NS_WARNING("SmscAddress.address is a mandatory field and can not be omitted.");
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return promise.forget();
  }

  nsString address = aSmscAddress.mAddress.Value();
  TypeOfNumber ton = aSmscAddress.mTypeOfAddress.mTypeOfNumber;
  NumberPlanIdentification npi =
    aSmscAddress.mTypeOfAddress.mNumberPlanIdentification;

  // If the address begins with +, set TON to international no matter what has
  // passed in.
  if (!address.IsEmpty() && address[0] == '+') {
    ton = TypeOfNumber::International;
  }

  nsCOMPtr<nsIMobileMessageCallback> msgCallback =
    new MobileMessageCallback(promise);

  rv = smsService->SetSmscAddress(serviceId, address,
    static_cast<uint32_t>(ton), static_cast<uint32_t>(npi), msgCallback);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  return promise.forget();
}


} // namespace dom
} // namespace mozilla

already_AddRefed<nsISmsService>
NS_CreateSmsService()
{
  nsCOMPtr<nsISmsService> smsService;

  if (XRE_IsContentProcess()) {
    smsService = SmsIPCService::GetSingleton();
  } else {
#ifdef MOZ_WIDGET_ANDROID
    smsService = new SmsService();
#elif defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
    smsService = do_GetService(GONK_SMSSERVICE_CONTRACTID);
#endif
  }

  return smsService.forget();
}

already_AddRefed<nsIMobileMessageDatabaseService>
NS_CreateMobileMessageDatabaseService()
{
  nsCOMPtr<nsIMobileMessageDatabaseService> mobileMessageDBService;
  if (XRE_IsContentProcess()) {
    mobileMessageDBService = SmsIPCService::GetSingleton();
  } else {
#ifdef MOZ_WIDGET_ANDROID
    mobileMessageDBService = new MobileMessageDatabaseService();
#elif defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
    mobileMessageDBService =
      do_CreateInstance(GONK_MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
#endif
  }

  return mobileMessageDBService.forget();
}

already_AddRefed<nsIMmsService>
NS_CreateMmsService()
{
  nsCOMPtr<nsIMmsService> mmsService;

  if (XRE_IsContentProcess()) {
    mmsService = SmsIPCService::GetSingleton();
  } else {
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
    mmsService = do_CreateInstance("@mozilla.org/mms/gonkmmsservice;1");
#endif
  }

  return mmsService.forget();
}

already_AddRefed<nsIMobileMessageService>
NS_CreateMobileMessageService()
{
  nsCOMPtr<nsIMobileMessageService> service = new MobileMessageService();
  return service.forget();
}
