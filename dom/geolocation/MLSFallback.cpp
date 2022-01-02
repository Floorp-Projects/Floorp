/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MLSFallback.h"
#include "GeolocationPosition.h"
#include "nsComponentManagerUtils.h"
#include "nsIGeolocationProvider.h"
#include "nsServiceManagerUtils.h"

NS_IMPL_ISUPPORTS(MLSFallback, nsITimerCallback, nsINamed)

MLSFallback::MLSFallback(uint32_t delay) : mDelayMs(delay) {}

MLSFallback::~MLSFallback() = default;

nsresult MLSFallback::Startup(nsIGeolocationUpdate* aWatcher) {
  if (mHandoffTimer || mMLSFallbackProvider) {
    return NS_OK;
  }

  mUpdateWatcher = aWatcher;

  // No need to schedule a timer callback if there is no startup delay.
  if (mDelayMs == 0) {
    return CreateMLSFallbackProvider();
  }

  return NS_NewTimerWithCallback(getter_AddRefs(mHandoffTimer), this, mDelayMs,
                                 nsITimer::TYPE_ONE_SHOT);
}

nsresult MLSFallback::Shutdown() {
  mUpdateWatcher = nullptr;

  if (mHandoffTimer) {
    mHandoffTimer->Cancel();
    mHandoffTimer = nullptr;
  }

  nsresult rv = NS_OK;
  if (mMLSFallbackProvider) {
    rv = mMLSFallbackProvider->Shutdown();
    mMLSFallbackProvider = nullptr;
  }
  return rv;
}

NS_IMETHODIMP
MLSFallback::Notify(nsITimer* aTimer) { return CreateMLSFallbackProvider(); }

NS_IMETHODIMP
MLSFallback::GetName(nsACString& aName) {
  aName.AssignLiteral("MLSFallback");
  return NS_OK;
}

nsresult MLSFallback::CreateMLSFallbackProvider() {
  if (mMLSFallbackProvider || !mUpdateWatcher) {
    return NS_OK;
  }

  nsresult rv;
  mMLSFallbackProvider =
      do_CreateInstance("@mozilla.org/geolocation/mls-provider;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mMLSFallbackProvider) {
    rv = mMLSFallbackProvider->Startup();
    if (NS_SUCCEEDED(rv)) {
      mMLSFallbackProvider->Watch(mUpdateWatcher);
    }
  }
  mUpdateWatcher = nullptr;
  return rv;
}
