/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#include "nsCookie.h"
#include "nsString.h"
#include "prmem.h"

// nsCookie Implementation

NS_IMPL_ISUPPORTS2(nsCookie, nsICookie, nsISupportsWeakReference);

nsCookie::nsCookie() {
  NS_INIT_REFCNT();
}

nsCookie::nsCookie
  (char * name,
   char * value,
   PRBool isDomain,
   char * host,
   char * path,
   PRBool isSecure,
   PRUint64 expires) {
  cookieName = name;
  cookieValue = value;
  cookieIsDomain = isDomain;
  cookieHost = host;
  cookiePath = path;
  cookieIsSecure = isSecure;
  cookieExpires = expires;
  NS_INIT_REFCNT();
}

nsCookie::~nsCookie(void) {
  nsCRT::free(cookieName);
  nsCRT::free(cookieValue);
  nsCRT::free(cookieHost);
  nsCRT::free(cookiePath);
}

NS_IMETHODIMP nsCookie::GetName(char * *aName) {
  if (cookieName) {
    *aName = (char *) nsMemory::Clone(cookieName, nsCRT::strlen(cookieName) + 1);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsCookie::GetValue(char * *aValue) {
  if (cookieValue) {
    *aValue = (char *) nsMemory::Clone(cookieValue, nsCRT::strlen(cookieValue) + 1);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsCookie::GetIsDomain(PRBool *aIsDomain) {
  *aIsDomain = cookieIsDomain;
  return NS_OK;
}

NS_IMETHODIMP nsCookie::GetHost(char * *aHost) {
  if (cookieHost) {
    *aHost = (char *) nsMemory::Clone(cookieHost, nsCRT::strlen(cookieHost) + 1);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsCookie::GetPath(char * *aPath) {
  if (cookiePath) {
    *aPath = (char *) nsMemory::Clone(cookiePath, nsCRT::strlen(cookiePath) + 1);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsCookie::GetIsSecure(PRBool *aIsSecure) {
  *aIsSecure = cookieIsSecure;
  return NS_OK;
}

NS_IMETHODIMP nsCookie::GetExpires(PRUint64 *aExpires) {
  *aExpires = cookieExpires;
  return NS_OK;
}
