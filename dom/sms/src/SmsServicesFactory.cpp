/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsServicesFactory.h"
#include "nsXULAppAPI.h"
#include "SmsService.h"
#include "SmsIPCService.h"
#ifndef MOZ_B2G_RIL
#include "SmsDatabaseService.h"
#endif
#include "nsServiceManagerUtils.h"

#define RIL_SMS_DATABASE_SERVICE_CONTRACTID "@mozilla.org/sms/rilsmsdatabaseservice;1"

namespace mozilla {
namespace dom {
namespace sms {

/* static */ already_AddRefed<nsISmsService>
SmsServicesFactory::CreateSmsService()
{
  nsCOMPtr<nsISmsService> smsService;

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    smsService = new SmsIPCService();
  } else {
    smsService = new SmsService();
  }

  return smsService.forget();
}

/* static */ already_AddRefed<nsISmsDatabaseService>
SmsServicesFactory::CreateSmsDatabaseService()
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService;

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    smsDBService = new SmsIPCService();
  } else {
#ifdef MOZ_B2G_RIL
    smsDBService = do_GetService(RIL_SMS_DATABASE_SERVICE_CONTRACTID);
#else
    smsDBService = new SmsDatabaseService();
#endif
  }

  return smsDBService.forget();
}

} // namespace sms
} // namespace dom
} // namespace mozilla
