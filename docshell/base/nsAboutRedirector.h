/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *      Gagan Saksena (original author)
 */

#ifndef nsAboutRedirector_h__
#define nsAboutRedirector_h__

#include "nsIAboutModule.h"

class nsAboutRedirector : public nsIAboutModule
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIABOUTMODULE

    nsAboutRedirector() { NS_INIT_REFCNT(); }
    virtual ~nsAboutRedirector() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
};

#define NS_ABOUT_REDIRECTOR_MODULE_CID               \
{ /*  f0acde16-1dd1-11b2-9e35-f5786fff5a66*/         \
    0xf0acde16,                                      \
    0x1dd1,                                          \
    0x11b2,                                          \
    {0x9e, 0x35, 0xf5, 0x78, 0x6f, 0xff, 0x5a, 0x66} \
}

#endif // nsAboutRedirector_h__
