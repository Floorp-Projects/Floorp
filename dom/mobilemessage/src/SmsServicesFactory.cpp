/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsServicesFactory.h"
#include "nsXULAppAPI.h"
#include "ipc/SmsIPCService.h"
#ifdef MOZ_WIDGET_ANDROID
#include "android/MobileMessageDatabaseService.h"
#include "android/SmsService.h"
#include "android/MmsService.h"
#elif defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
#include "gonk/SmsService.h"
#else
#include "fallback/MmsService.h"
#include "fallback/MobileMessageDatabaseService.h"
#include "fallback/SmsService.h"
#endif
#include "nsServiceManagerUtils.h"

#define RIL_MMSSERVICE_CONTRACTID "@mozilla.org/mms/rilmmsservice;1"
#define RIL_MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID "@mozilla.org/mobilemessage/rilmobilemessagedatabaseservice;1"

namespace mozilla {
namespace dom {
namespace mobilemessage {

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

/* static */ already_AddRefed<nsIMobileMessageDatabaseService>
SmsServicesFactory::CreateMobileMessageDatabaseService()
{
  nsCOMPtr<nsIMobileMessageDatabaseService> mobileMessageDBService;
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    mobileMessageDBService = new SmsIPCService();
  } else {
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
    mobileMessageDBService = do_GetService(RIL_MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
#else
    mobileMessageDBService = new MobileMessageDatabaseService();
#endif
  }

  return mobileMessageDBService.forget();
}

/* static */ already_AddRefed<nsIMmsService>
SmsServicesFactory::CreateMmsService()
{
  nsCOMPtr<nsIMmsService> mmsService;

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    mmsService = new SmsIPCService();
  } else {
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
    mmsService = do_CreateInstance(RIL_MMSSERVICE_CONTRACTID);
#else
    mmsService = new MmsService();
#endif
  }

  return mmsService.forget();
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
