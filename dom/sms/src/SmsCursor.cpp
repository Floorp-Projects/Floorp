/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsCursor.h"
#include "nsIDOMClassInfo.h"
#include "nsDOMError.h"
#include "nsIDOMSmsMessage.h"
#include "nsIDOMSmsRequest.h"
#include "SmsRequest.h"
#include "SmsRequestManager.h"
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

SmsCursor::SmsCursor(PRInt32 aListId, nsIDOMMozSmsRequest* aRequest)
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

  mMessage = nullptr;
  static_cast<SmsRequest*>(mRequest.get())->Reset();

  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);

  PRInt32 requestId;
  nsresult rv = requestManager->AddRequest(mRequest, &requestId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);

  smsDBService->GetNextMessageInList(mListId, requestId, 0);

  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla

