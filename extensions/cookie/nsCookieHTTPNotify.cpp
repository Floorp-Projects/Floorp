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
#include "nsCRT.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"
#include "nsINetModuleMgr.h" 
#include "nsILoadGroup.h"
#include "nsICategoryManager.h"
#include "nsIHTTPProtocolHandler.h"		// for NS_HTTP_STARTUP_CATEGORY

static NS_DEFINE_CID(kINetModuleMgrCID, NS_NETMODULEMGR_CID);

///////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS2(nsCookieHTTPNotify, nsIHTTPNotify, nsINetNotify);

///////////////////////////////////
// nsCookieHTTPNotify Implementation

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCookieHTTPNotify, Init)

nsresult nsCookieHTTPNotify::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    return nsCookieHTTPNotifyConstructor(aOuter, aIID, aResult);
}

NS_METHOD nsCookieHTTPNotify::RegisterProc(nsIComponentManager *aCompMgr,
                                           nsIFile *aPath,
                                           const char *registryLocation,
                                           const char *componentType)
{
    // Register ourselves into the NS_CATEGORY_HTTP_STARTUP
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman = do_GetService("mozilla.categorymanager.1", &rv);
    if (NS_FAILED(rv)) return rv;

    nsCID cid = NS_COOKIEHTTPNOTIFY_CID;
    char *cidString = cid.ToString();
    nsXPIDLCString prevEntry;
    rv = catman->AddCategoryEntry(NS_HTTP_STARTUP_CATEGORY, cidString, "Http Cookie Notify",
                                  PR_TRUE, PR_TRUE, getter_Copies(prevEntry));
    nsAllocator::Free(cidString);

    return NS_OK;

}

NS_METHOD nsCookieHTTPNotify::UnregisterProc(nsIComponentManager *aCompMgr,
                                             nsIFile *aPath,
                                             const char *registryLocation)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman = do_GetService("mozilla.categorymanager.1", &rv);
    if (NS_FAILED(rv)) return rv;

    nsCID cid = NS_COOKIEHTTPNOTIFY_CID;
    char *cidString = cid.ToString();
    nsXPIDLCString prevEntry;
    rv = catman->DeleteCategoryEntry("http-startup-category", cidString, PR_TRUE,
                                     getter_Copies(prevEntry));
    nsAllocator::Free(cidString);

    // Return value is not used from this function.
    return NS_OK;
}

NS_IMETHODIMP
nsCookieHTTPNotify::Init()
{
    mCookieHeader = getter_AddRefs(NS_NewAtom("cookie"));
    if (!mCookieHeader) return NS_ERROR_OUT_OF_MEMORY;
    mSetCookieHeader = getter_AddRefs(NS_NewAtom("set-cookie"));
    if (!mSetCookieHeader) return NS_ERROR_OUT_OF_MEMORY;
    mExpiresHeader = getter_AddRefs(NS_NewAtom("date"));
    if (!mExpiresHeader) return NS_ERROR_OUT_OF_MEMORY;

    // Register to handing http requests and responses
    nsresult rv = NS_OK;
    nsCOMPtr<nsINetModuleMgr> pNetModuleMgr = do_GetService(kINetModuleMgrCID, &rv); 
    if (NS_FAILED(rv)) return rv;
    rv = pNetModuleMgr->RegisterModule(NS_NETWORK_MODULE_MANAGER_HTTP_REQUEST_PROGID,
                                       (nsIHTTPNotify *)this);
    if (NS_FAILED(rv)) return rv;

    rv = pNetModuleMgr->RegisterModule(NS_NETWORK_MODULE_MANAGER_HTTP_RESPONSE_PROGID,
                                       (nsIHTTPNotify *)this);
    return rv;
}

nsCookieHTTPNotify::nsCookieHTTPNotify()
{
    NS_INIT_REFCNT();
    mCookieService = nsnull;
#ifdef DEBUG_dp
    printf("CookieHTTPNotify Created.\n");
#endif /* DEBUG_dp */
}

nsCookieHTTPNotify::~nsCookieHTTPNotify()
{
}

NS_IMETHODIMP
nsCookieHTTPNotify::SetupCookieService()
{
    nsresult rv = NS_OK;
    if (!mCookieService)
    {
      mCookieService = do_GetService(NS_COOKIESERVICE_PROGID, &rv);
    }
    return rv;
}

///////////////////////////////////
// nsIHTTPNotify

NS_IMETHODIMP
nsCookieHTTPNotify::ModifyRequest(nsISupports *aContext)
{
    nsresult rv;
    // Preconditions
    NS_ENSURE_ARG_POINTER(aContext);

    nsCOMPtr<nsIHTTPChannel> pHTTPConnection = do_QueryInterface(aContext, &rv);
    if (NS_FAILED(rv)) return rv; 

    // Get the url
    nsCOMPtr<nsIURI> pURL;
    rv = pHTTPConnection->GetURI(getter_AddRefs(pURL));
    if (NS_FAILED(rv)) return rv;

    // Get the original url that the user either typed in or clicked on
    nsCOMPtr<nsILoadGroup> pLoadGroup;
    rv = pHTTPConnection->GetLoadGroup(getter_AddRefs(pLoadGroup));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> pChannel;
    if (pLoadGroup) {
      rv = pLoadGroup->GetDefaultLoadChannel(getter_AddRefs(pChannel));
      if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIURI> pFirstURL;
    if (pChannel) {
      rv = pChannel->GetURI(getter_AddRefs(pFirstURL));
    } else {
      rv = pHTTPConnection->GetURI(getter_AddRefs(pFirstURL));
    }
    if (NS_FAILED(rv)) return rv;

    // Ensure that the cookie service exists
    rv = SetupCookieService();
    if (NS_FAILED(rv)) return rv;

    nsAutoString cookie;
    rv = mCookieService->GetCookieStringFromHTTP(pURL, pFirstURL, cookie);
    if (NS_FAILED(rv)) return rv;

    // Set the cookie into the request headers
    // XXX useless convertion from nsString to char * again
    const char *cookieRaw = cookie.ToNewCString();
    if (!cookieRaw) return NS_ERROR_OUT_OF_MEMORY;

    // only set a cookie header if we have a value to send
    if (*cookieRaw)
        rv = pHTTPConnection->SetRequestHeader(mCookieHeader, cookieRaw);
    nsAllocator::Free((void *)cookieRaw);

    return rv;
}

NS_IMETHODIMP
nsCookieHTTPNotify::AsyncExamineResponse(nsISupports *aContext)
{
    nsresult rv;
    // Preconditions
    NS_ENSURE_ARG_POINTER(aContext);

    nsCOMPtr<nsIHTTPChannel> pHTTPConnection = do_QueryInterface(aContext, &rv);
    if (NS_FAILED(rv)) return rv;

    // Get the Cookie header
    nsXPIDLCString cookieHeader;
    rv = pHTTPConnection->GetResponseHeader(mSetCookieHeader, getter_Copies(cookieHeader));
    if (NS_FAILED(rv)) return rv;
    if (!cookieHeader) return NS_ERROR_OUT_OF_MEMORY;

    // Get the url
    nsCOMPtr<nsIURI> pURL;
    rv = pHTTPConnection->GetURI(getter_AddRefs(pURL));
    if (NS_FAILED(rv)) return rv;

    // Get the original url that the user either typed in or clicked on
    nsCOMPtr<nsILoadGroup> pLoadGroup;
    rv = pHTTPConnection->GetLoadGroup(getter_AddRefs(pLoadGroup));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> pChannel;
    if (pLoadGroup) {
      rv = pLoadGroup->GetDefaultLoadChannel(getter_AddRefs(pChannel));
      if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIURI> pFirstURL;
    if (pChannel) {
      rv = pChannel->GetURI(getter_AddRefs(pFirstURL));
    } else {
      rv = pHTTPConnection->GetURI(getter_AddRefs(pFirstURL));
    }
    if (NS_FAILED(rv)) return rv;

    // Get the expires
    nsXPIDLCString expiresHeader;
    rv = pHTTPConnection->GetResponseHeader(mExpiresHeader, getter_Copies(expiresHeader));
    if (NS_FAILED(rv)) return rv;

    // Ensure that we have the cookie service
    rv = SetupCookieService();
    if (NS_FAILED(rv)) return rv;

    // Save the cookie
    rv = mCookieService->SetCookieStringFromHttp(pURL, pFirstURL, cookieHeader, expiresHeader);

    return rv;
}

