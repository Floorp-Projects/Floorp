/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCodebasePrincipal.h"
#include "nsJSPrincipals.h"
#include "plstr.h"
#include "nsXPIDLString.h"

PR_STATIC_CALLBACK(void *) 
nsGetPrincipalArray(JSContext *cx, struct JSPrincipals *prin) 
{
    return nsnull;
}

PR_STATIC_CALLBACK(JSBool) 
nsGlobalPrivilegesEnabled(JSContext *cx , struct JSPrincipals *jsprin) 
{
    return JS_TRUE;
}

PR_STATIC_CALLBACK(void)
nsDestroyJSPrincipals(JSContext *cx, struct JSPrincipals *jsprin) {
    nsJSPrincipals *nsjsprin = (nsJSPrincipals *)jsprin;
    // We need to destroy the nsIPrincipal. We'll do this by adding
    // to the refcount and calling release

    // Note that we don't want to use NS_IF_RELEASE because it will try
    // to set nsjsprin->nsIPrincipalPtr to nsnull *after* nsjsprin has
    // already been destroyed.
    nsjsprin->refcount++;
    if (nsjsprin->nsIPrincipalPtr)
        nsjsprin->nsIPrincipalPtr->Release();
    // The nsIPrincipal that we release owns the JSPrincipal struct,
    // so we don't need to worry about "codebase"
}

nsJSPrincipals::nsJSPrincipals()
{
    codebase = nsnull;
    getPrincipalArray = nsGetPrincipalArray;
    globalPrivilegesEnabled = nsGlobalPrivilegesEnabled;
    refcount = 0;
    destroy = nsDestroyJSPrincipals;
    nsIPrincipalPtr = nsnull;
}

nsresult
nsJSPrincipals::Init(char *aCodebase) 
{
    codebase = aCodebase;
    return NS_OK;
}

nsJSPrincipals::~nsJSPrincipals() 
{
    if (codebase)
        PL_strfree(codebase); 
}


