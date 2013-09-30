/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGeolocation.h"
#include "nsGeoPosition.h"
#include "AndroidBridge.h"
#include "AndroidLocationProvider.h"

using namespace mozilla;

extern nsIGeolocationUpdate *gLocationCallback;

NS_IMPL_ISUPPORTS1(AndroidLocationProvider, nsIGeolocationProvider)

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
    if (!AndroidBridge::Bridge())
        return NS_ERROR_NOT_IMPLEMENTED;
    GeckoAppShell::EnableLocation(true);
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
    if (!AndroidBridge::Bridge())
        return NS_ERROR_NOT_IMPLEMENTED;
    GeckoAppShell::EnableLocation(false);
    return NS_OK;
}

NS_IMETHODIMP
AndroidLocationProvider::SetHighAccuracy(bool enable)
{
    if (!AndroidBridge::Bridge())
        return NS_ERROR_NOT_IMPLEMENTED;
    GeckoAppShell::EnableLocationHighAccuracy(enable);
    return NS_OK;
}
