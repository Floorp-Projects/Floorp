/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDOMOfflineResourceList.h"
#include "nsDOMClassInfo.h"
#include "nsDOMError.h"
#include "nsIPrefetchService.h"
#include "nsCPrefetchService.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsICacheSession.h"
#include "nsICacheService.h"
#include "nsIOfflineCacheSession.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsIDOMLoadStatus.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"

// To prevent abuse of the resource list for data storage, the number
// of offline urls and their length are limited.

static const char kMaxEntriesPref[] =  "offline.max_site_resources";
#define DEFAULT_MAX_ENTRIES 100
#define MAX_URI_LENGTH 2048

static nsCAutoString gCachedHostPort;
static char **gCachedKeys = nsnull;
static PRUint32 gCachedKeysCount = 0;

//
// nsDOMOfflineResourceList
//

NS_INTERFACE_MAP_BEGIN(nsDOMOfflineResourceList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMOfflineResourceList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMOfflineResourceList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(OfflineResourceList)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMOfflineResourceList)
NS_IMPL_RELEASE(nsDOMOfflineResourceList)

nsDOMOfflineResourceList::nsDOMOfflineResourceList(nsIURI *aURI)
  : mInitialized(PR_FALSE)
  , mURI(aURI)
{
}

nsDOMOfflineResourceList::~nsDOMOfflineResourceList()
{
}

nsresult
nsDOMOfflineResourceList::Init()
{
  if (mInitialized) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(mURI);
  if (!innerURI)
    return NS_ERROR_FAILURE;

  nsresult rv = innerURI->GetHostPort(mHostPort);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheService> serv = do_GetService(NS_CACHESERVICE_CONTRACTID,
                                                 &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheSession> session;
  rv = serv->CreateSession("HTTP-offline",
                           nsICache::STORE_OFFLINE,
                           nsICache::STREAM_BASED,
                           getter_AddRefs(session));
  NS_ENSURE_SUCCESS(rv, rv);

  mCacheSession = do_QueryInterface(session, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mInitialized = PR_TRUE;

  return NS_OK;
}

//
// nsDOMOfflineResourceList::nsIDOMOfflineResourceList
//

NS_IMETHODIMP
nsDOMOfflineResourceList::GetLength(PRUint32 *aLength)
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CacheKeys();
  NS_ENSURE_SUCCESS(rv, rv);

  *aLength = gCachedKeysCount;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::Item(PRUint32 aIndex, nsAString& aURI)
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  SetDOMStringToNull(aURI);

  rv = CacheKeys();
  NS_ENSURE_SUCCESS(rv, rv);

  if (aIndex >= gCachedKeysCount)
    return NS_ERROR_NOT_AVAILABLE;

  CopyUTF8toUTF16(gCachedKeys[aIndex], aURI);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::Add(const nsAString& aURI)
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  if (aURI.Length() > MAX_URI_LENGTH) return NS_ERROR_DOM_BAD_URI;

  // this will fail if the URI is not absolute
  nsCOMPtr<nsIURI> requestedURI;
  rv = NS_NewURI(getter_AddRefs(requestedURI), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 length;
  rv = GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  PRUint32 maxEntries = nsContentUtils::GetIntPref(kMaxEntriesPref,
                                                   DEFAULT_MAX_ENTRIES);

  if (length > maxEntries) return NS_ERROR_NOT_AVAILABLE;

  ClearCachedKeys();

  nsCOMPtr<nsIOfflineCacheUpdate> update =
    do_CreateInstance(NS_OFFLINECACHEUPDATE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = update->Init(PR_TRUE, mHostPort, NS_LITERAL_CSTRING(""), mURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = update->AddURI(requestedURI, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = update->Schedule();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::Remove(const nsAString& aURI)
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString key;
  rv = GetCacheKey(aURI, key);
  NS_ENSURE_SUCCESS(rv, rv);

  ClearCachedKeys();

  return mCacheSession->RemoveOwnedKey(mHostPort,
                                       NS_LITERAL_CSTRING(""),
                                       key);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::Has(const nsAString& aURI, PRBool *aExists)
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString key;
  rv = GetCacheKey(aURI, key);
  NS_ENSURE_SUCCESS(rv, rv);

  return mCacheSession->KeyIsOwned(mHostPort, NS_LITERAL_CSTRING(""),
                                   key, aExists);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::Clear()
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  ClearCachedKeys();

  return mCacheSession->SetOwnedKeys(mHostPort,
                                     NS_LITERAL_CSTRING(""),
                                     0, nsnull);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::Refresh()
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOfflineCacheUpdate> update =
    do_CreateInstance(NS_OFFLINECACHEUPDATE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = update->Init(PR_FALSE, mHostPort, NS_LITERAL_CSTRING(""), mURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = update->Schedule();
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsDOMOfflineResourceList::GetCacheKey(const nsAString &aURI, nsCString &aKey)
{
  nsCOMPtr<nsIURI> requestedURI;
  nsresult rv = NS_NewURI(getter_AddRefs(requestedURI), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetCacheKey(requestedURI, aKey);
}

nsresult
nsDOMOfflineResourceList::GetCacheKey(nsIURI *aURI, nsCString &aKey)
{
  nsresult rv = aURI->GetSpec(aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  // url fragments aren't used in cache keys
  nsCAutoString::const_iterator specStart, specEnd;
  aKey.BeginReading(specStart);
  aKey.EndReading(specEnd);
  if (FindCharInReadable('#', specStart, specEnd)) {
    aKey.BeginReading(specEnd);
    aKey = Substring(specEnd, specStart);
  }

  return NS_OK;
}

nsresult
nsDOMOfflineResourceList::CacheKeys()
{
  if (gCachedKeys && mHostPort == gCachedHostPort)
    return NS_OK;

  ClearCachedKeys();

  nsresult rv = mCacheSession->GetOwnedKeys(mHostPort, NS_LITERAL_CSTRING(""),
                                            &gCachedKeysCount, &gCachedKeys);

  if (NS_SUCCEEDED(rv))
    gCachedHostPort = mHostPort;

  return rv;
}

void
nsDOMOfflineResourceList::ClearCachedKeys()
{
  if (gCachedKeys) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(gCachedKeysCount, gCachedKeys);
    gCachedKeys = nsnull;
    gCachedKeysCount = 0;
  }

  gCachedHostPort = "";
}



