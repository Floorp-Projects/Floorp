/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
    virtual ~nsAppStartupNotifier();
};

#endif /* nsAppStartupNotifier_h___ */

