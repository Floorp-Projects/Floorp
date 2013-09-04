/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/telephony/TelephonyFactory.h"
#ifdef MOZ_WIDGET_GONK
#include "nsIGonkTelephonyProvider.h"
#endif
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"
#include "TelephonyIPCProvider.h"

USING_TELEPHONY_NAMESPACE

/* static */ already_AddRefed<nsITelephonyProvider>
TelephonyFactory::CreateTelephonyProvider()
{
  nsCOMPtr<nsITelephonyProvider> provider;

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    provider = new TelephonyIPCProvider();
#ifdef MOZ_WIDGET_GONK
  } else {
    provider = do_CreateInstance(GONK_TELEPHONY_PROVIDER_CONTRACTID);
#endif
  }

  return provider.forget();
}
