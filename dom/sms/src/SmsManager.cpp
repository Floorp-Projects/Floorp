/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "SmsFilter.h"
#include "SmsManager.h"
#include "nsIDOMClassInfo.h"
#include "nsISmsService.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "Constants.h"
#include "SmsEvent.h"
#include "nsIDOMSmsMessage.h"
#include "nsIDOMSmsRequest.h"
#include "SmsRequestManager.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsISmsDatabaseService.h"
#include "nsIXPConnect.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define RECEIVED_EVENT_NAME  NS_LITERAL_STRING("received")
#define SENT_EVENT_NAME      NS_LITERAL_STRING("sent")
#define DELIVERED_EVENT_NAME NS_LITERAL_STRING("delivered")

DOMCI_DATA(MozSmsManager, mozilla::dom::sms::SmsManager)

namespace mozilla {
namespace dom {
namespace sms {

NS_IMPL_CYCLE_COLLECTION_CLASS(SmsManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SmsManager,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(received)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(sent)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(delivered)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SmsManager,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(received)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(sent)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(delivered)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SmsManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozSmsManager)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozSmsManager)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozSmsManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(SmsManager, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SmsManager, nsDOMEventTargetHelper)

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
  obs->AddObserver(this, kSmsSentObserverTopic, false);
  obs->AddObserver(this, kSmsDeliveredObserverTopic, false);
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
  obs->RemoveObserver(this, kSmsSentObserverTopic);
  obs->RemoveObserver(this, kSmsDeliveredObserverTopic);
}

NS_IMETHODIMP
SmsManager::GetNumberOfMessagesForText(const nsAString& aText, PRUint16* aResult)
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

  nsCOMPtr<nsIDOMMozSmsRequest> request;

  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);

  PRInt32 requestId;
  nsresult rv = requestManager->CreateRequest(this, getter_AddRefs(request),
                                              &requestId);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create the request!");
    return rv;
  }

  nsDependentJSString number;
  number.init(aCx, aNumber);

  smsService->Send(number, aMessage, requestId, 0);

  rv = nsContentUtils::WrapNative(aCx, aGlobal, request, aRequest);
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
  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, global)) {
    NS_ERROR("Failed to enter the js compartment!");
    return NS_ERROR_FAILURE;
  }

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
SmsManager::GetMessageMoz(PRInt32 aId, nsIDOMMozSmsRequest** aRequest)
{
  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);

  PRInt32 requestId;
  nsresult rv = requestManager->CreateRequest(this, aRequest, &requestId);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create the request!");
    return rv;
  }

  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);

  smsDBService->GetMessageMoz(aId, requestId, 0);

  return NS_OK;
}

nsresult
SmsManager::Delete(PRInt32 aId, nsIDOMMozSmsRequest** aRequest)
{
  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);

  PRInt32 requestId;
  nsresult rv = requestManager->CreateRequest(this, aRequest, &requestId);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create the request!");
    return rv;
  }

  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);

  smsDBService->DeleteMessage(aId, requestId, 0);

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

  PRInt32 id;
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

  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);

  PRInt32 requestId;
  nsresult rv = requestManager->CreateRequest(this, aRequest,
                                              &requestId);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create the request!");
    return rv;
  }

  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);

  smsDBService->CreateMessageList(filter, aReverse, requestId, 0);

  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(SmsManager, received)
NS_IMPL_EVENT_HANDLER(SmsManager, sent)
NS_IMPL_EVENT_HANDLER(SmsManager, delivered)

nsresult
SmsManager::DispatchTrustedSmsEventToSelf(const nsAString& aEventName, nsIDOMMozSmsMessage* aMessage)
{
  nsRefPtr<nsDOMEvent> event = new SmsEvent(nsnull, nsnull);
  nsresult rv = static_cast<SmsEvent*>(event.get())->Init(aEventName, false,
                                                          false, aMessage);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
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

  if (!strcmp(aTopic, kSmsSentObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-sent' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedSmsEventToSelf(SENT_EVENT_NAME, message);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsDeliveredObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-delivered' topic without a valid message!");
      return NS_OK;
    }

    DispatchTrustedSmsEventToSelf(DELIVERED_EVENT_NAME, message);
    return NS_OK;
  }

  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
