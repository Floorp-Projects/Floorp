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
#include "nsCodebasePrincipal.h"
#include "nsJSPrincipals.h"
#include "xp.h"
#include "plstr.h"

PR_STATIC_CALLBACK(void *) 
nsGetPrincipalArray(JSContext * cx, struct JSPrincipals * prin) {
    return nsnull;
}

PR_STATIC_CALLBACK(JSBool) 
nsGlobalPrivilegesEnabled(JSContext * cx , struct JSPrincipals *jsprin) {
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
nsDestroyJSPrincipals(JSContext * cx, struct JSPrincipals * jsprin) {
    nsJSPrincipals * nsjsprin = (nsJSPrincipals *)jsprin;
    nsIPrincipal *p = nsjsprin->nsIPrincipalPtr;
    NS_IF_RELEASE(p);
    // The nsIPrincipal that we release owns the JSPrincipal struct,
    // so we don't need to worry about "codebase"
}

nsJSPrincipals::nsJSPrincipals() {
    codebase = nsnull;
    getPrincipalArray = nsGetPrincipalArray;
    globalPrivilegesEnabled = nsGlobalPrivilegesEnabled;
    refcount = 0;
    destroy = nsDestroyJSPrincipals;
    nsIPrincipalPtr = nsnull;
}

nsresult
nsJSPrincipals::Init(nsIPrincipal * prin) {
    char * cb;
    nsICodebasePrincipal * cbprin;
    prin->QueryInterface(NS_GET_IID(nsICodebasePrincipal),(void * *)& cbprin);
    cbprin->GetURLString(& cb);
    nsIPrincipalPtr = prin;
    codebase = PL_strdup(cb);
    if (!codebase) 
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(nsIPrincipalPtr);
    return NS_OK;
}

nsJSPrincipals::~nsJSPrincipals() {
    if (codebase)
        PL_strfree(codebase); 
    if (nsIPrincipalPtr)
        NS_RELEASE(nsIPrincipalPtr);
}


