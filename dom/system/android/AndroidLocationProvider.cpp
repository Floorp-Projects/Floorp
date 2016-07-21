/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGeolocation.h"
#include "nsGeoPosition.h"
#include "AndroidLocationProvider.h"
#include "GeneratedJNIWrappers.h"

using namespace mozilla;

extern nsIGeolocationUpdate *gLocationCallback;

NS_IMPL_ISUPPORTS(AndroidLocationProvider, nsIGeolocationProvider)

AndroidLocationProvider::AndroidLocationProvider()
{
}

AndroidLocationProvider::~AndroidLocationProvider()
{
    NS_IF_RELEASE(gLocationCallback);
}

NS_IMETHODIMP
AndroidLocationProvider::Startup()
{
    java::GeckoAppShell::EnableLocation(true);
    return NS_OK;
}

NS_IMETHODIMP
AndroidLocationProvider::Watch(nsIGeolocationUpdate* aCallback)
{
    NS_IF_RELEASE(gLocationCallback);
    gLocationCallback = aCallback;
    NS_IF_ADDREF(gLocationCallback);
    return NS_OK;
}

NS_IMETHODIMP
AndroidLocationProvider::Shutdown()
{
    java::GeckoAppShell::EnableLocation(false);
    return NS_OK;
}

NS_IMETHODIMP
AndroidLocationProvider::SetHighAccuracy(bool enable)
{
    java::GeckoAppShell::EnableLocationHighAccuracy(enable);
    return NS_OK;
}
