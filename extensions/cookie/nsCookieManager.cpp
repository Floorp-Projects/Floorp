/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCookieManager.h"
#include "nsCookies.h"
#include "nsIGenericFactory.h"
#include "nsIScriptGlobalObject.h"

////////////////////////////////////////////////////////////////////////////////

// used in logging via ::Add(), to indicate origin of cookie
#ifdef PR_LOGGING
#define kCookieHeader "(added via cookiemanager interface)"
#else
#define kCookieHeader ""
#endif

////////////////////////////////////////////////////////////////////////////////

class nsCookieEnumerator : public nsISimpleEnumerator
{
  public:

    NS_DECL_ISUPPORTS

    // note: mCookieCount is initialized just once in the ctor. While it might
    // appear that the cookie list can change while the cookiemanager is running,
    // the cookieservice is actually on the same thread, so it can't. Note that
    // a new nsCookieEnumerator is created each time the cookiemanager is loaded.
    // So we only need to get the count once. If we ever change the cookieservice to
    // run on a different thread, then something to the effect of a lock will be
    // required. see bug 191682 for details.
    nsCookieEnumerator() :
      mCookieIndex(0)
    {
      PRInt32 temp;
      COOKIE_RemoveExpiredCookies(NOW_IN_SECONDS, temp);
      mCookieCount = sCookieList->Count();
    }

    NS_IMETHOD HasMoreElements(PRBool *result) 
    {
      *result = mCookieIndex < mCookieCount;
      return NS_OK;
    }

    NS_IMETHOD GetNext(nsISupports **result) 
    {
      if (mCookieIndex >= mCookieCount) {
        *result = nsnull;
        NS_ERROR("bad cookie index");
        return NS_ERROR_UNEXPECTED;
      }

      cookie_CookieStruct *cookieInList = NS_STATIC_CAST(cookie_CookieStruct*, sCookieList->ElementAt(mCookieIndex++));
      NS_ASSERTION(cookieInList, "corrupt cookie list");

      // create a new nsICookie and copy the cookie data
      if (!(*result = COOKIE_ChangeFormat(cookieInList).get())) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      return NS_OK;
    }

    virtual ~nsCookieEnumerator() 
    {
    }

  protected:

    PRInt32 mCookieIndex;
    PRInt32 mCookieCount;

};

NS_IMPL_ISUPPORTS1(nsCookieEnumerator, nsISimpleEnumerator);


////////////////////////////////////////////////////////////////////////////////
// nsCookieManager Implementation

NS_IMPL_ISUPPORTS3(nsCookieManager, nsICookieManager, nsICookieManager2, nsISupportsWeakReference);

nsCookieManager::nsCookieManager()
{
}

nsCookieManager::~nsCookieManager()
{
}

nsresult nsCookieManager::Init()
{
  COOKIE_Read();
  return NS_OK;
}

NS_IMETHODIMP nsCookieManager::RemoveAll()
{
  ::COOKIE_RemoveAll();
  ::COOKIE_Write();
  return NS_OK;
}

NS_IMETHODIMP nsCookieManager::GetEnumerator(nsISimpleEnumerator **entries)
{
  *entries = nsnull;

  nsCookieEnumerator* cookieEnum = new nsCookieEnumerator();
  if (!cookieEnum) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(cookieEnum);
  *entries = cookieEnum;
  return NS_OK;
}

NS_IMETHODIMP nsCookieManager::Add(const nsACString &aDomain,
                                   const nsACString &aPath,
                                   const nsACString &aName,
                                   const nsACString &aValue,
                                   PRBool           aIsSecure,
                                   PRInt32          aExpires)
{
  cookie_CookieStruct *cookie = new cookie_CookieStruct;
  if (!cookie) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsInt64 currentTime = NOW_IN_SECONDS;
  cookie->host = aDomain;
  cookie->path = aPath;
  cookie->name = aName;
  cookie->cookie = aValue;
  cookie->expires = nsInt64(aExpires);
  cookie->lastAccessed = currentTime;
  cookie->isSession = PR_FALSE;
  cookie->isSecure = aIsSecure;
  cookie->isDomain = PR_TRUE;
  cookie->status = nsICookie::STATUS_UNKNOWN;
  cookie->policy = nsICookie::POLICY_UNKNOWN;

  nsresult rv = COOKIE_Add(cookie, currentTime, nsnull, kCookieHeader);
  if (NS_FAILED(rv)) {
    delete cookie;
  }
  return NS_OK;
}

NS_IMETHODIMP nsCookieManager::Remove(const nsACString& host,
                                      const nsACString& name,
                                      const nsACString& path,
                                      PRBool            blocked)
{
  ::COOKIE_Remove(host, name, path, blocked);
  return NS_OK;
}
