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

#ifndef nsCodebasePrincipal_h___
#define nsCodebasePrincipal_h___

#include "nsICodebasePrincipal.h"
#include "nsPrincipal.h"

class nsCodebasePrincipal : public nsICodebasePrincipal {
public:
    ////////////////////////////////////////////////////////////////////////////
    // from nsISupports:

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPrincipal:

    NS_IMETHOD
    IsTrusted(const char* scope, PRBool *pbIsTrusted);
     

    ///////////////////////////////////////////////////////////////////////////
    // from nsICodebasePrincipal:

    /**
     * Returns the codebase URL of the principal.
     *
     * @param result - the resulting codebase URL
     */
    NS_IMETHOD
    GetURL(char **ppCodeBaseURL);

    ////////////////////////////////////////////////////////////////////////////
    // from nsCCodebasePrincipal:

    nsCodebasePrincipal(const char *codebaseURL, nsresult *result);
    nsCodebasePrincipal(nsPrincipal *pNSPrincipal);

    virtual ~nsCodebasePrincipal(void);
    nsPrincipal *GetPeer(void);

protected:
    nsPrincipal *m_pNSPrincipal;
};

#endif // nsCCodebasePrincipal_h___
