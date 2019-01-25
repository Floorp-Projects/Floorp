/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nstoolkitshellservice_h____
#define nstoolkitshellservice_h____

#include "nsIToolkitShellService.h"

class nsToolkitShellService : public nsIToolkitShellService {
 public:
  NS_IMETHOD IsDefaultBrowser(bool aForAllTypes,
                              bool* aIsDefaultBrowser) = 0;

  NS_IMETHODIMP IsDefaultApplication(bool* aIsDefaultBrowser) {
    // Only care about the http(s) protocol. This only matters on Windows.
    return IsDefaultBrowser(false, aIsDefaultBrowser);
  }
};

#endif  // nstoolkitshellservice_h____
