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
#include "nsIURL.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsCRT.h"

///////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS2(nsCookieHTTPNotify, nsIHTTPNotify, nsINetNotify);

///////////////////////////////////
// nsCookieHTTPNotify Implementation

NS_COOKIE nsresult NS_NewCookieHTTPNotify(nsIHTTPNotify** aHTTPNotify) {
  if (aHTTPNotify == NULL) {
    return NS_ERROR_NULL_POINTER;
  } 
  nsCookieHTTPNotify* it = new nsCookieHTTPNotify();
  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(nsIHTTPNotify::GetIID(), (void **) aHTTPNotify);
}

nsCookieHTTPNotify::nsCookieHTTPNotify() {
  NS_INIT_REFCNT();
}

nsCookieHTTPNotify::~nsCookieHTTPNotify() {
}

///////////////////////////////////
// nsIHTTPNotify

NS_IMETHODIMP
nsCookieHTTPNotify::ModifyRequest(nsISupports *aContext) {
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
  char *url;
  if (pURL == nsnull) {
    NS_RELEASE(pHTTPConnection);
    return rv;
  }
  rv = pURL->GetSpec(&url);
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
  const char* cookie = ::COOKIE_GetCookie(url);
  if (cookie == nsnull) {
    NS_RELEASE(pURL);
    NS_RELEASE(pHTTPConnection);
    nsCRT::free(url);
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
    nsCRT::free(url);
    return rv;
  }
  NS_RELEASE(pURL);
  NS_RELEASE(pHTTPConnection);
  nsCRT::free(url);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieHTTPNotify::AsyncExamineResponse(nsISupports *aContext) {
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
    printf("\nRecieving ... %s\n", cookie);
    nsIURI* pURL;
    rv = pHTTPConnection->GetURI(&pURL);
    if (NS_FAILED(rv)) {
      NS_RELEASE(pHTTPConnection);
      nsCRT::free(cookie);
      return rv;
    }
    char *url;
    if (pURL == nsnull) {
      NS_RELEASE(pHTTPConnection);
      nsCRT::free(cookie);
      return rv;
    }
    rv = pURL->GetSpec(&url);
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
      if(pDate) {
        COOKIE_SetCookieStringFromHttp(url, cookie, pDate);
        nsCRT::free(pDate);
      }
    }
    NS_RELEASE(pURL);
    nsCRT::free(url);
    nsCRT::free(cookie);
  }
  NS_RELEASE(pHTTPConnection);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsCookieHTTPFactory Implementation

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsCookieHTTPNotifyFactory, kIFactoryIID);

nsCookieHTTPNotifyFactory::nsCookieHTTPNotifyFactory(void) {
  NS_INIT_REFCNT();
}

nsCookieHTTPNotifyFactory::~nsCookieHTTPNotifyFactory(void) {
}

nsresult
nsCookieHTTPNotifyFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
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
  return rv;
}

nsresult
nsCookieHTTPNotifyFactory::LockFactory(PRBool aLock) {
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
