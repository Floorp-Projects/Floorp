/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsCursor.h"
#include "nsIDOMClassInfo.h"
#include "nsError.h"
#include "nsIDOMSmsMessage.h"
#include "SmsRequest.h"
#include "nsISmsDatabaseService.h"

DOMCI_DATA(MozSmsCursor, mozilla::dom::sms::SmsCursor)

namespace mozilla {
namespace dom {
namespace sms {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SmsCursor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozSmsCursor)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozSmsCursor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_2(SmsCursor, mRequest, mMessage)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SmsCursor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SmsCursor)

SmsCursor::SmsCursor()
  : mListId(-1)
{
}

SmsCursor::SmsCursor(int32_t aListId, nsISmsRequest* aRequest)
  : mListId(aListId)
  , mRequest(aRequest)
{
}

SmsCursor::~SmsCursor()
{
  NS_ASSERTION(!mMessage, "mMessage shouldn't be set!");

  if (mListId != -1) {
    nsCOMPtr<nsISmsDatabaseService> smsDBService =
      do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);

    if (!smsDBService) {
      NS_ERROR("Can't find SmsDBService!");
    }

    smsDBService->ClearMessageList(mListId);
  }
}

void
SmsCursor::Disconnect()
{
  NS_ASSERTION(!mMessage, "mMessage shouldn't be set!");

  mRequest = nullptr;
  mListId = -1;
}

NS_IMETHODIMP
SmsCursor::GetMessage(nsIDOMMozSmsMessage** aMessage)
{
  NS_IF_ADDREF(*aMessage = mMessage);
  return NS_OK;
}

NS_IMETHODIMP
SmsCursor::Continue()
{
  // No message means we are waiting for a message or we got the last one.
  if (!mMessage) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsRefPtr<SmsRequest> request = static_cast<SmsRequest*>(mRequest.get());

  mMessage = nullptr;
  request->Reset();

  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);

  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(request);
  smsDBService->GetNextMessageInList(mListId, forwarder);

  // We intenionally increase the refcount. The release will be called
  // in the corresponding callback.
  request.forget();
  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla

