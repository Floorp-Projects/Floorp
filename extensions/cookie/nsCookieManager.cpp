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

#include "nsIServiceManager.h"
#include "nsCookieManager.h"
#include "nsCRT.h"
#include "nsCookies.h"
#include "nsCookie.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"
#include "nsIScriptGlobalObject.h"

////////////////////////////////////////////////////////////////////////////////

class nsCookieEnumerator : public nsISimpleEnumerator
{
    public:

        NS_DECL_ISUPPORTS

        nsCookieEnumerator() : mCookieCount(0) 
        {
            NS_INIT_REFCNT();
        }

        NS_IMETHOD HasMoreElements(PRBool *result) 
        {
            *result = COOKIE_Count() > mCookieCount;
            return NS_OK;
        }

        NS_IMETHOD GetNext(nsISupports **result) 
        {
          char *name;
          char *value;
          PRBool isDomain;
          char * host;
          char * path;
          PRBool isSecure;
          PRUint64 expires;
          nsresult rv = COOKIE_Enumerate
            (mCookieCount++, &name, &value, &isDomain, &host, &path, &isSecure, &expires);
          if (NS_SUCCEEDED(rv)) {
            nsICookie *cookie =
              new nsCookie(name, value, isDomain, host, path, isSecure, expires);
            *result = cookie;
            NS_ADDREF(*result);
          } else {
            *result = nsnull;
          }
          return rv;
        }

        virtual ~nsCookieEnumerator() 
        {
        }

    protected:
        PRInt32 mCookieCount;
};

NS_IMPL_ISUPPORTS1(nsCookieEnumerator, nsISimpleEnumerator);


////////////////////////////////////////////////////////////////////////////////
// nsCookieManager Implementation

NS_IMPL_ISUPPORTS2(nsCookieManager, nsICookieManager, nsISupportsWeakReference);

nsCookieManager::nsCookieManager()
{
  NS_INIT_REFCNT();
}

nsCookieManager::~nsCookieManager(void)
{
}

nsresult nsCookieManager::Init()
{
  COOKIE_Read();
  return NS_OK;
}

NS_IMETHODIMP nsCookieManager::RemoveAll(void) {
  ::COOKIE_RemoveAll();
  return NS_OK;
}

NS_IMETHODIMP nsCookieManager::GetEnumerator(nsISimpleEnumerator * *entries)
{
    *entries = nsnull;

    nsCookieEnumerator* cookieEnum = new nsCookieEnumerator();
    if (cookieEnum == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(cookieEnum);
    *entries = cookieEnum;
    return NS_OK;
}

NS_IMETHODIMP nsCookieManager::Remove
  (const char* host, const char* name, const char* path, const PRBool permanent) {
  ::COOKIE_Remove(host, name, path, permanent);
  return NS_OK;
}
