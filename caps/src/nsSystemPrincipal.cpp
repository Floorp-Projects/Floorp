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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* The privileged system principal. */

#include "nsSystemPrincipal.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"


static NS_DEFINE_IID(kIPrincipalIID, NS_IPRINCIPAL_IID);
NS_IMPL_ISUPPORTS(nsSystemPrincipal, kIPrincipalIID);

////////////////////////////////////
// Methods implementing nsIPrincipal
////////////////////////////////////

NS_IMETHODIMP
nsSystemPrincipal::ToString(char **result)
{
    // NB TODO
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::Equals(nsIPrincipal *other, PRBool *result)
{
    *result = (other == this);
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::GetJSPrincipals(JSPrincipals **jsprin)
{
    if (mJSPrincipals.nsIPrincipalPtr == nsnull) {
        mJSPrincipals.nsIPrincipalPtr = this;
        NS_ADDREF(mJSPrincipals.nsIPrincipalPtr);
        // matching release in nsDestroyJSPrincipals
    }
    *jsprin = &mJSPrincipals;
    JSPRINCIPALS_HOLD(cx, *jsprin);
    return NS_OK;
}

NS_IMETHODIMP
nsSystemPrincipal::CanAccess(const char *capability, PRBool *result)
{
    // The system principal has all privileges.
    *result = PR_TRUE;
    return NS_OK;
}


//////////////////////////////////////////
// Constructor, Destructor, initialization
//////////////////////////////////////////

nsSystemPrincipal::nsSystemPrincipal()
{
    NS_INIT_ISUPPORTS();
}

NS_IMETHODIMP
nsSystemPrincipal::Init()
{
    char *codebase = nsCRT::strdup("[System Principal]");
    if (!codebase)
        return NS_ERROR_OUT_OF_MEMORY;
    if (NS_FAILED(mJSPrincipals.Init(codebase))) 
        return NS_ERROR_FAILURE;
    return NS_OK;
}

nsSystemPrincipal::~nsSystemPrincipal(void)
{
}
