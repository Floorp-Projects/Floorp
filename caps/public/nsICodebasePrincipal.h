/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsICodebasePrincipal_h___
#define nsICodebasePrincipal_h___

#include "nsIPrincipal.h"

class nsICodebasePrincipal : public nsIPrincipal {
public:

    /**
     * Returns the codebase URL of the principal.
     *
     * @param result - the resulting codebase URL
     */
    NS_IMETHOD
    GetURL(const char **ppCodeBaseURL) = 0;

};

#define NS_ICODEBASEPRINCIPAL_IID                    \
{ /* c29fe440-25e1-11d2-8160-006008119d7a */         \
    0xc29fe440,                                      \
    0x25e1,                                          \
    0x11d2,                                          \
    {0x81, 0x60, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif // nsICodebasePrincipal_h___
