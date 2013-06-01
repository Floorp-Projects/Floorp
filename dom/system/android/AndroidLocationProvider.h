/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidLocationProvider_h
#define AndroidLocationProvider_h

#include "nsIGeolocationProvider.h"

class AndroidLocationProvider MOZ_FINAL : public nsIGeolocationProvider
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIGEOLOCATIONPROVIDER

    AndroidLocationProvider();
private:
    ~AndroidLocationProvider();
};

#endif /* AndroidLocationProvider_h */
