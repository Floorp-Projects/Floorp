/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Geolocation.h"
#include "GeolocationPosition.h"
#include "AndroidLocationProvider.h"
#include "mozilla/java/GeckoAppShellWrappers.h"

using namespace mozilla;

extern nsIGeolocationUpdate* gLocationCallback;

NS_IMPL_ISUPPORTS(AndroidLocationProvider, nsIGeolocationProvider)

AndroidLocationProvider::AndroidLocationProvider() {}

AndroidLocationProvider::~AndroidLocationProvider() {
  NS_IF_RELEASE(gLocationCallback);
}

NS_IMETHODIMP
AndroidLocationProvider::Startup() {
  if (java::GeckoAppShell::EnableLocationUpdates(true)) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
AndroidLocationProvider::Watch(nsIGeolocationUpdate* aCallback) {
  NS_IF_RELEASE(gLocationCallback);
  gLocationCallback = aCallback;
  NS_IF_ADDREF(gLocationCallback);
  return NS_OK;
}

NS_IMETHODIMP
AndroidLocationProvider::Shutdown() {
  if (java::GeckoAppShell::EnableLocationUpdates(false)) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
AndroidLocationProvider::SetHighAccuracy(bool enable) {
  java::GeckoAppShell::EnableLocationHighAccuracy(enable);
  return NS_OK;
}
