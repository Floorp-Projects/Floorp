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
#include "nsIGenericFactory.h"
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

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCookieHTTPNotify, Init)

NS_COOKIE nsresult NS_NewCookieHTTPNotify(nsIHTTPNotify** aHTTPNotify)
{
    return nsCookieHTTPNotifyConstructor(nsnull, NS_GET_IID(nsIHTTPNotify), (void **) aHTTPNotify);
}

nsCookieHTTPNotify::nsCookieHTTPNotify()
{
    NS_INIT_REFCNT();
}

NS_IMETHODIMP
nsCookieHTTPNotify::Init()
{
    mCookieHeader = NS_NewAtom("cookie");
    if (!mCookieHeader) return NS_ERROR_OUT_OF_MEMORY;
    mSetCookieHeader = NS_NewAtom("set-cookie");
    if (!mSetCookieHeader) return NS_ERROR_OUT_OF_MEMORY;
    mExpiresHeader = NS_NewAtom("date");
    if (!mExpiresHeader) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

nsCookieHTTPNotify::~nsCookieHTTPNotify()
{
}

///////////////////////////////////
// nsIHTTPNotify

NS_IMETHODIMP
nsCookieHTTPNotify::ModifyRequest(nsISupports *aContext)
{
    nsresult rv;
    if (!aContext) return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIHTTPChannel> pHTTPConnection = do_QueryInterface(aContext, &rv);
    if (NS_FAILED(rv)) return rv; 

    nsCOMPtr<nsIURI> pURL;
    rv = pHTTPConnection->GetURI(getter_AddRefs(pURL));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString url;
    rv = pURL->GetSpec(getter_Copies(url));
    if (NS_FAILED(rv)) return rv;
    if (url == nsnull) return NS_ERROR_FAILURE;

    const char* cookie = ::COOKIE_GetCookie((char*)(const char*)url);
    if (cookie == nsnull) return rv;

    rv = pHTTPConnection->SetRequestHeader(mCookieHeader, cookie);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsCookieHTTPNotify::AsyncExamineResponse(nsISupports *aContext)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aContext);

    nsCOMPtr<nsIHTTPChannel> pHTTPConnection = do_QueryInterface(aContext);
    if (NS_FAILED(rv)) return rv;

    // Get the Cookie header
    nsXPIDLCString cookie;
    rv = pHTTPConnection->GetResponseHeader(mSetCookieHeader, getter_Copies(cookie));
    if (NS_FAILED(rv)) return rv;
    if (!cookie) return NS_ERROR_OUT_OF_MEMORY;

    // Get the url string
    nsCOMPtr<nsIURI> pURL;
    nsXPIDLCString url;
    rv = pHTTPConnection->GetURI(getter_AddRefs(pURL));
    if (NS_FAILED(rv)) return rv;
    rv = pURL->GetSpec(getter_Copies(url));
    if (NS_FAILED(rv)) return rv;
    if (url == nsnull) return NS_ERROR_FAILURE;
    
    // Get the expires
    nsXPIDLCString pDate;
    rv = pHTTPConnection->GetResponseHeader(mExpiresHeader, getter_Copies(pDate));
    if (NS_FAILED(rv)) return rv;

    // Save the cookie
    COOKIE_SetCookieStringFromHttp((char*)(const char*)url,
                                   (char *)(const char *)cookie,
                                   (char *)(const char *)pDate);

    return NS_OK;
}

