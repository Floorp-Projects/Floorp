/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include <stdio.h>
#include "nsCookieHTTPNotify.h"
#include "nsIHTTPChannel.h"
#include "nsCookie.h"
#include "nsCRT.h"


///////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS(nsCookieHTTPNotify, nsIHTTPNotify::GetIID());

///////////////////////////////////
// nsCookieHTTPNotify Implementation

NS_COOKIE nsresult NS_NewCookieHTTPNotify(nsIHTTPNotify** aHTTPNotify)
{
    if (aHTTPNotify == NULL)
    {
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


///////////////////////////////////
// nsIHTTPNotify


NS_IMETHODIMP
nsCookieHTTPNotify::ModifyRequest(nsISupports *aContext)
{
    nsresult rv;
    nsIHTTPChannel* pHTTPConnection= nsnull;

    if (aContext) {
        rv = aContext->QueryInterface(nsIHTTPChannel::GetIID(), 
                                        (void**)pHTTPConnection);
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    if (NS_FAILED(rv))
        return rv; 

    const char* cookie = "testCookieVal";
    rv = pHTTPConnection->SetRequestHeader("Cookie", cookie);

    if (NS_FAILED(rv)) {
        NS_RELEASE(pHTTPConnection);
        return rv;
    }

    NS_RELEASE(pHTTPConnection);

    return NS_OK;
}

NS_IMETHODIMP
nsCookieHTTPNotify::AsyncExamineResponse(nsISupports *aContext)
{
    nsresult rv;
    nsIHTTPChannel* pHTTPConnection= nsnull;

    if (aContext) {
        rv = aContext->QueryInterface(nsIHTTPChannel::GetIID(), 
                                        (void**)pHTTPConnection);
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    if (NS_FAILED(rv))
        return rv; 

    
    char* cookie = "testCookieVal";
    rv = pHTTPConnection->GetResponseHeader("Set-Cookie", &cookie);

    if (NS_FAILED(rv)) {
        NS_RELEASE(pHTTPConnection);
        return rv;
    }

    if (cookie) {
        printf("\nRecieving ... %s\n", cookie);
        nsCRT::free(cookie);
    }
    
    NS_RELEASE(pHTTPConnection);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsCookieHTTPFactory Implementation

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsCookieHTTPNotifyFactory, kIFactoryIID);

nsCookieHTTPNotifyFactory::nsCookieHTTPNotifyFactory(void)
{
    NS_INIT_REFCNT();
}

nsCookieHTTPNotifyFactory::~nsCookieHTTPNotifyFactory(void)
{

}

nsresult
nsCookieHTTPNotifyFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;
    
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;
    nsIHTTPNotify* inst = nsnull;

    if (NS_FAILED(rv = NS_NewCookieHTTPNotify(&inst)))
        return rv;

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = inst->QueryInterface(aIID, aResult);

    if (NS_FAILED(rv)) {
        *aResult = NULL;
    }
    return rv;
}

nsresult
nsCookieHTTPNotifyFactory::LockFactory(PRBool aLock)
{
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////



