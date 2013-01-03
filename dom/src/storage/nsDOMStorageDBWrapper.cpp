/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsDOMStorage.h"
#include "nsDOMStorageDBWrapper.h"
#include "nsIFile.h"
#include "nsIURL.h"
#include "nsIVariant.h"
#include "nsIEffectiveTLDService.h"
#include "nsIScriptSecurityManager.h"
#include "nsAppDirectoryServiceDefs.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozIStorageService.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageFunction.h"
#include "nsPrintfCString.h"
#include "nsNetUtil.h"
#include "nsIPrincipal.h"

void ReverseString(const nsCSubstring& source, nsCSubstring& result)
{
  nsACString::const_iterator sourceBegin, sourceEnd;
  source.BeginReading(sourceBegin);
  source.EndReading(sourceEnd);

  result.SetLength(source.Length());
  nsACString::iterator destEnd;
  result.EndWriting(destEnd);

  while (sourceBegin != sourceEnd) {
    *(--destEnd) = *sourceBegin;
    ++sourceBegin;
  }
}

nsDOMStorageDBWrapper::nsDOMStorageDBWrapper()
{
}

nsDOMStorageDBWrapper::~nsDOMStorageDBWrapper()
{
}

void
nsDOMStorageDBWrapper::Close()
{
  mPersistentDB.Close();
}

nsresult
nsDOMStorageDBWrapper::Init()
{
  nsresult rv;

  rv = mPersistentDB.Init(NS_LITERAL_STRING("webappsstore.sqlite"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSessionOnlyDB.Init(&mPersistentDB);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPrivateBrowsingDB.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMStorageDBWrapper::FlushAndEvictFromCache(bool aIsShuttingDown)
{
  nsresult rv = mPersistentDB.FlushAndEvictFromCache(aIsShuttingDown);

  // Nothing in the cache?  Then no need for a timer.
  if (!mPersistentDB.IsFlushTimerNeeded()) {
    StopCacheFlushTimer();
  }

  return rv;
}

#define IMPL_FORWARDER_GUTS(_return, _code)                                \
  PR_BEGIN_MACRO                                                      \
  if (aStorage->IsPrivate())                                          \
    _return mPrivateBrowsingDB._code;                                 \
  if (aStorage->SessionOnly())                                        \
    _return mSessionOnlyDB._code;                                     \
  _return mPersistentDB._code;                                        \
  PR_END_MACRO

#define IMPL_FORWARDER(_code)                                  \
  IMPL_FORWARDER_GUTS(return, _code)

#define IMPL_VOID_FORWARDER(_code)                                    \
  IMPL_FORWARDER_GUTS((void), _code)

nsresult
nsDOMStorageDBWrapper::GetAllKeys(DOMStorageImpl* aStorage,
                                  nsTHashtable<nsSessionStorageEntry>* aKeys)
{
  IMPL_FORWARDER(GetAllKeys(aStorage, aKeys));
}

nsresult
nsDOMStorageDBWrapper::GetKeyValue(DOMStorageImpl* aStorage,
                                   const nsAString& aKey,
                                   nsAString& aValue,
                                   bool* aSecure)
{
  IMPL_FORWARDER(GetKeyValue(aStorage, aKey, aValue, aSecure));
}

nsresult
nsDOMStorageDBWrapper::SetKey(DOMStorageImpl* aStorage,
                              const nsAString& aKey,
                              const nsAString& aValue,
                              bool aSecure)
{
  IMPL_FORWARDER(SetKey(aStorage, aKey, aValue, aSecure));
}

nsresult
nsDOMStorageDBWrapper::SetSecure(DOMStorageImpl* aStorage,
                                 const nsAString& aKey,
                                 const bool aSecure)
{
  IMPL_FORWARDER(SetSecure(aStorage, aKey, aSecure));
}

nsresult
nsDOMStorageDBWrapper::RemoveKey(DOMStorageImpl* aStorage,
                                 const nsAString& aKey)
{
  IMPL_FORWARDER(RemoveKey(aStorage, aKey));
}

nsresult
nsDOMStorageDBWrapper::ClearStorage(DOMStorageImpl* aStorage)
{
  IMPL_FORWARDER(ClearStorage(aStorage));
}

void
nsDOMStorageDBWrapper::MarkScopeCached(DOMStorageImpl* aStorage)
{
  IMPL_VOID_FORWARDER(MarkScopeCached(aStorage));
}

bool
nsDOMStorageDBWrapper::IsScopeDirty(DOMStorageImpl* aStorage)
{
  IMPL_FORWARDER(IsScopeDirty(aStorage));
}

nsresult
nsDOMStorageDBWrapper::DropSessionOnlyStoragesForHost(const nsACString& aHostName)
{
  return mSessionOnlyDB.RemoveOwner(aHostName);
}

nsresult
nsDOMStorageDBWrapper::DropPrivateBrowsingStorages()
{
  return mPrivateBrowsingDB.RemoveAll();
}

nsresult
nsDOMStorageDBWrapper::RemoveOwner(const nsACString& aOwner)
{
  nsresult rv;

  rv = mPrivateBrowsingDB.RemoveOwner(aOwner);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSessionOnlyDB.RemoveOwner(aOwner);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPersistentDB.RemoveOwner(aOwner);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}


nsresult
nsDOMStorageDBWrapper::RemoveAll()
{
  nsresult rv;

  rv = mPrivateBrowsingDB.RemoveAll();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSessionOnlyDB.RemoveAll();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPersistentDB.RemoveAll();
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsDOMStorageDBWrapper::RemoveAllForApp(uint32_t aAppId, bool aOnlyBrowserElement)
{
  // We only care about removing the permament storage. Temporary storage such
  // as session storage or private browsing storage will not be re-used anyway
  // and will be automatically deleted at some point.
  return mPersistentDB.RemoveAllForApp(aAppId, aOnlyBrowserElement);
}

nsresult
nsDOMStorageDBWrapper::GetUsage(DOMStorageImpl* aStorage, int32_t *aUsage)
{
  IMPL_FORWARDER(GetUsage(aStorage, aUsage));
}

nsresult
nsDOMStorageDBWrapper::GetUsage(const nsACString& aDomain,
                                int32_t *aUsage, bool aPrivate)
{
  if (aPrivate)
    return mPrivateBrowsingDB.GetUsage(aDomain, aUsage);

#if 0
  // XXX Check where from all this method gets called, not sure this should
  // include any potential session-only data
  nsresult rv;
  rv = mSessionOnlyDB.GetUsage(aDomain, aUsage);
  if (NS_SUECEEDED(rv))
    return rv;
#endif

  return mPersistentDB.GetUsage(aDomain, aUsage);
}

nsresult
nsDOMStorageDBWrapper::CreateScopeDBKey(nsIPrincipal* aPrincipal,
                                        nsACString& aKey)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  nsAutoCString domainScope;
  rv = uri->GetAsciiHost(domainScope);
  NS_ENSURE_SUCCESS(rv, rv);

  if (domainScope.IsEmpty()) {
    // About pages have an empty host but a valid path.  Since they are handled
    // internally by our own redirector, we can trust them and use path as key.
    // if file:/// protocol, let's make the exact directory the domain
    bool isScheme = false;
    if ((NS_SUCCEEDED(uri->SchemeIs("about", &isScheme)) && isScheme) ||
        (NS_SUCCEEDED(uri->SchemeIs("moz-safe-about", &isScheme)) && isScheme)) {
      rv = uri->GetPath(domainScope);
      NS_ENSURE_SUCCESS(rv, rv);
      // While the host is always canonicalized to lowercase, the path is not,
      // thus need to force the casing.
      ToLowerCase(domainScope);
    }
    else if (NS_SUCCEEDED(uri->SchemeIs("file", &isScheme)) && isScheme) {
      nsCOMPtr<nsIURL> url = do_QueryInterface(uri, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = url->GetDirectory(domainScope);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsAutoCString key;

  rv = CreateReversedDomain(domainScope, key);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString scheme;
  rv = uri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  key.Append(NS_LITERAL_CSTRING(":") + scheme);

  int32_t port = NS_GetRealPort(uri);
  if (port != -1) {
    key.Append(nsPrintfCString(":%d", port));
  }

  uint32_t appId;
  rv = aPrincipal->GetAppId(&appId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isInBrowserElement;
  rv = aPrincipal->GetIsInBrowserElement(&isInBrowserElement);
  NS_ENSURE_SUCCESS(rv, rv);

  if (appId == nsIScriptSecurityManager::NO_APP_ID && !isInBrowserElement) {
    aKey.Assign(key);
    return NS_OK;
  }

  aKey.Truncate();
  aKey.AppendInt(appId);
  aKey.Append(NS_LITERAL_CSTRING(":") + (isInBrowserElement ?
              NS_LITERAL_CSTRING("t") : NS_LITERAL_CSTRING("f")) +
              NS_LITERAL_CSTRING(":") + key);

  return NS_OK;
}

nsresult
nsDOMStorageDBWrapper::CreateReversedDomain(const nsACString& aAsciiDomain,
                                            nsACString& aKey)
{
  if (aAsciiDomain.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  ReverseString(aAsciiDomain, aKey);

  aKey.AppendLiteral(".");
  return NS_OK;
}

nsresult
nsDOMStorageDBWrapper::CreateQuotaDBKey(nsIPrincipal* aPrincipal,
                                        nsACString& aKey)
{
  nsresult rv;

  nsAutoCString subdomainsDBKey;
  nsCOMPtr<nsIEffectiveTLDService> eTLDService(do_GetService(
    NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  nsAutoCString eTLDplusOne;
  rv = eTLDService->GetBaseDomain(uri, 0, eTLDplusOne);
  if (NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS == rv) {
    // XXX bug 357323 - what to do for localhost/file exactly?
    rv = uri->GetAsciiHost(eTLDplusOne);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  CreateReversedDomain(eTLDplusOne, subdomainsDBKey);

  uint32_t appId;
  rv = aPrincipal->GetAppId(&appId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isInBrowserElement;
  rv = aPrincipal->GetIsInBrowserElement(&isInBrowserElement);
  NS_ENSURE_SUCCESS(rv, rv);

  if (appId == nsIScriptSecurityManager::NO_APP_ID && !isInBrowserElement) {
    aKey.Assign(subdomainsDBKey);
    return NS_OK;
  }

  aKey.Truncate();
  aKey.AppendInt(appId);
  aKey.Append(NS_LITERAL_CSTRING(":") + (isInBrowserElement ?
              NS_LITERAL_CSTRING("t") : NS_LITERAL_CSTRING("f")) +
              NS_LITERAL_CSTRING(":") + subdomainsDBKey);

  return NS_OK;
}

void
nsDOMStorageDBWrapper::EnsureCacheFlushTimer()
{
  if (!mCacheFlushTimer) {
    nsresult rv;
    mCacheFlushTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);

    if (!NS_SUCCEEDED(rv)) {
      mCacheFlushTimer = nullptr;
      return;
    }

    mCacheFlushTimer->Init(nsDOMStorageManager::gStorageManager, 5000,
                           nsITimer::TYPE_REPEATING_SLACK);
  }
}

void
nsDOMStorageDBWrapper::StopCacheFlushTimer()
{
  if (mCacheFlushTimer) {
    mCacheFlushTimer->Cancel();
    mCacheFlushTimer = nullptr;
  }
}

