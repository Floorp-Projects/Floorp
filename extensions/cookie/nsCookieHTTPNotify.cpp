/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 */

#include <stdio.h>
#include "nsCookieHTTPNotify.h"
#include "nsIHTTPChannel.h"
#include "nsCookie.h"
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsCRT.h"
#include "nsXPIDLString.h"

///////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS2(nsCookieHTTPNotify, nsIHTTPNotify, nsINetNotify);

///////////////////////////////////
// nsCookieHTTPNotify Implementation

NS_COOKIE nsresult NS_NewCookieHTTPNotify(nsIHTTPNotify** aHTTPNotify)
{
    if (aHTTPNotify == NULL) {
        return NS_ERROR_NULL_POINTER;
    } 
    nsCookieHTTPNotify* it = new nsCookieHTTPNotify();
    if (it == 0) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return it->QueryInterface(nsIHTTPNotify::GetIID(), (void **) aHTTPNotify);
}

nsCookieHTTPNotify::nsCookieHTTPNotify()
{
    NS_INIT_REFCNT();
}

nsCookieHTTPNotify::~nsCookieHTTPNotify()
{
}

NS_METHOD
nsCookieHTTPNotify::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (! aResult) {
        return NS_ERROR_NULL_POINTER;
    }
    if (aOuter) {
        return NS_ERROR_NO_AGGREGATION;
    }
    *aResult = nsnull;
    nsresult rv;
    nsIHTTPNotify* inst = nsnull;
    if (NS_FAILED(rv = NS_NewCookieHTTPNotify(&inst))) {
        return rv;
    }
    if (!inst) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {
        *aResult = NULL;
    }
    NS_RELEASE(inst);
    return rv;
}

///////////////////////////////////
// nsIHTTPNotify

NS_IMETHODIMP
nsCookieHTTPNotify::ModifyRequest(nsISupports *aContext)
{
    nsresult rv;
    nsIHTTPChannel* pHTTPConnection= nsnull;
    if (aContext) {
        rv = aContext->QueryInterface(nsIHTTPChannel::GetIID(), (void**)&pHTTPConnection);
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    if (NS_FAILED(rv)) {
        return rv; 
    }
    nsIURI* pURL;
    rv = pHTTPConnection->GetURI(&pURL);
    if (NS_FAILED(rv)) {
        NS_RELEASE(pHTTPConnection);
        return rv;
    }
    nsXPIDLCString url;
    if (pURL == nsnull) {
        NS_RELEASE(pHTTPConnection);
        return rv;
    }
    rv = pURL->GetSpec(getter_Copies(url));
    if (NS_FAILED(rv)) {
        NS_RELEASE(pURL);
        NS_RELEASE(pHTTPConnection);
        return rv;
    }
    if (url == nsnull) {
        NS_RELEASE(pURL);
        NS_RELEASE(pHTTPConnection);
        return rv;
    }
    const char* cookie = ::COOKIE_GetCookie((char*)(const char*)url);
    if (cookie == nsnull) {
        NS_RELEASE(pURL);
        NS_RELEASE(pHTTPConnection);
        return rv;
    }
    nsCOMPtr<nsIAtom> cookieHeader;

    // XXX:  Should cache this atom?  HTTP atoms *msut* be lower case
    cookieHeader = NS_NewAtom("cookie");
    if (!cookieHeader) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        rv = pHTTPConnection->SetRequestHeader(cookieHeader, cookie);
    }
    if (NS_FAILED(rv)) {
        NS_RELEASE(pURL);
        NS_RELEASE(pHTTPConnection);
        return rv;
    }
    NS_RELEASE(pURL);
    NS_RELEASE(pHTTPConnection);
    return NS_OK;
}

NS_IMETHODIMP
nsCookieHTTPNotify::AsyncExamineResponse(nsISupports *aContext)
{
    nsresult rv;
    nsIHTTPChannel* pHTTPConnection= nsnull;
    if (aContext) {
        rv = aContext->QueryInterface(nsIHTTPChannel::GetIID(), (void**)&pHTTPConnection);
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    if (NS_FAILED(rv)) {
        return rv;
    }
    char* cookie;
    nsCOMPtr<nsIAtom> header;

    // XXX:  Should cache this atom?  HTTP atoms *msut* be lower case
    header = NS_NewAtom("set-cookie");
    if (!header) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        rv = pHTTPConnection->GetResponseHeader(header, &cookie);
    }
    if (NS_FAILED(rv)) {
        NS_RELEASE(pHTTPConnection);
        return rv;
    }
    if (cookie) {
        nsIURI* pURL;
        rv = pHTTPConnection->GetURI(&pURL);
        if (NS_FAILED(rv)) {
            NS_RELEASE(pHTTPConnection);
            nsCRT::free(cookie);
            return rv;
        }
        nsXPIDLCString url;
        if (pURL == nsnull) {
            NS_RELEASE(pHTTPConnection);
            nsCRT::free(cookie);
            return rv;
        }
        rv = pURL->GetSpec(getter_Copies(url));
        if (NS_FAILED(rv)) {
            NS_RELEASE(pURL);
            NS_RELEASE(pHTTPConnection);
            nsCRT::free(cookie);
            return rv;
        }
        if (url == nsnull) {
            NS_RELEASE(pURL);
            NS_RELEASE(pHTTPConnection);
            nsCRT::free(cookie);
            return rv;
        }
        char *pDate = nsnull;
        // XXX:  Should cache this atom?  HTTP atoms *msut* be lower case
        header = NS_NewAtom("date");
        if (!header) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
            rv = pHTTPConnection->GetResponseHeader(header, &pDate);
        }
        if (NS_SUCCEEDED(rv)) {
            COOKIE_SetCookieStringFromHttp((char*)(const char*)url, cookie, pDate);
            if(pDate) {
                nsCRT::free(pDate);
            }
        }
        NS_RELEASE(pURL);
        nsCRT::free(cookie);
    }
    NS_RELEASE(pHTTPConnection);
    return NS_OK;
}

