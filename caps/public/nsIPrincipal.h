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

#ifndef nsIPrincipal_h___
#define nsIPrincipal_h___

#include "nsISupports.h"

class nsIPrincipal : public nsISupports {
public:

    /**
     * Determines whether or not the Principal is trusted or not.
     *
     * If it is a Certificate Principal, then it will validate
     * by checking whether the given certificate chain can be 
     * successfully validated using trusted certificate information 
     * in Netscape's certificate database.
     *
     * If it is a codebase Principal, this function will return FALSE,
     * unless the following preference is set.  The following
     * preference would allow developers to test their applets without 
     * signing.
     * 
     * user_pref("signed.applets.codebase_principal_support", true);
     *
     *
     * @param scope -         the individual parameter of the certificate that 
     *                        needs to be verified. If NULL is passed, it will
     *                        verify the whole certificate chain.
     * @param result -        is PR_TRUE if the given certificate chain is trusted, 
     *                        otherwise PR_FALSE.
     */
    NS_IMETHOD
    IsTrusted(char* scope, PRBool *pbIsTrusted) = 0;
    
};

#define NS_IPRINCIPAL_IID                            \
{ /* ff9313d0-25e1-11d2-8160-006008119d7a */         \
    0xff9313d0,                                      \
    0x25e1,                                          \
    0x11d2,                                          \
    {0x81, 0x60, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif // nsIPrincipal_h___
