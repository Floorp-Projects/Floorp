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

  mRequest = nsnull;
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

  mMessage = nsnull;
  static_cast<SmsRequest*>(mRequest.get())->Reset();

  PRInt32 requestId = SmsRequestManager::GetInstance()->AddRequest(mRequest);

  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, NS_ERROR_FAILURE);

  smsDBService->GetNextMessageInList(mListId, requestId, 0);

  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla

