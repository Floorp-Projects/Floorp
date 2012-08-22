
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsRequestManager_h
#define mozilla_dom_sms_SmsRequestManager_h

#include "nsCOMArray.h"
#include "SmsRequest.h"
#include "nsISmsRequestManager.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace sms {

class SmsRequestManager MOZ_FINAL : nsISmsRequestManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISMSREQUESTMANAGER

private:
  nsresult DispatchTrustedEventToRequest(const nsAString& aEventName,
                                         nsIDOMMozSmsRequest* aRequest);
  SmsRequest* GetRequest(int32_t aRequestId);

  template <class T>
  nsresult NotifySuccess(int32_t aRequestId, T aParam);
  nsresult NotifyError(int32_t aRequestId, int32_t aError);

  nsCOMArray<nsIDOMMozSmsRequest> mRequests;
};

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsRequestManager_h
