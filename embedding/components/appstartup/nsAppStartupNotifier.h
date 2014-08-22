/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppStartupNotifier_h___
#define nsAppStartupNotifier_h___

#include "nsIAppStartupNotifier.h"

// {1F59B001-02C9-11d5-AE76-CC92F7DB9E03}
#define NS_APPSTARTUPNOTIFIER_CID \
   { 0x1f59b001, 0x2c9, 0x11d5, { 0xae, 0x76, 0xcc, 0x92, 0xf7, 0xdb, 0x9e, 0x3 } }

class nsAppStartupNotifier : public nsIObserver
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_APPSTARTUPNOTIFIER_CID )

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    nsAppStartupNotifier();

protected:
    virtual ~nsAppStartupNotifier();
};

#endif /* nsAppStartupNotifier_h___ */

