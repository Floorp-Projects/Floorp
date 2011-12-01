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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Neil Deakin <enndeakin@sympatico.ca>
 *   Honza Bambas <honzab@firemni.cz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsDOMError.h"
#include "nsDOMStorage.h"
#include "nsDOMStorageDBWrapper.h"
#include "nsIFile.h"
#include "nsIURL.h"
#include "nsIVariant.h"
#include "nsIEffectiveTLDService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozIStorageService.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageFunction.h"
#include "nsPrintfCString.h"
#include "nsNetUtil.h"

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
  mChromePersistentDB.Close();
}

nsresult
nsDOMStorageDBWrapper::Init()
{
  nsresult rv;

  rv = mPersistentDB.Init(NS_LITERAL_STRING("webappsstore.sqlite"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mChromePersistentDB.Init(NS_LITERAL_STRING("chromeappsstore.sqlite"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mSessionOnlyDB.Init(&mPersistentDB);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPrivateBrowsingDB.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMStorageDBWrapper::FlushAndDeleteTemporaryTables(bool force)
{
  nsresult rv1, rv2;
  rv1 = mChromePersistentDB.FlushTemporaryTables(force);
  rv2 = mPersistentDB.FlushTemporaryTables(force);

  // Everything flushed?  Then no need for a timer.
  if (!mChromePersistentDB.mTempTableLoads.Count() && 
      !mPersistentDB.mTempTableLoads.Count())
    StopTempTableFlushTimer();

  return NS_FAILED(rv1) ? rv1 : rv2;
}

#define IMPL_FORWARDER_GUTS(_return, _code)                                \
  PR_BEGIN_MACRO                                                      \
  if (aStorage->CanUseChromePersist())                                \
    _return mChromePersistentDB._code;                                \
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())  \
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
                              bool aSecure,
                              PRInt32 aQuota,
                              bool aExcludeOfflineFromUsage,
                              PRInt32 *aNewUsage)
{
  IMPL_FORWARDER(SetKey(aStorage, aKey, aValue, aSecure,
                        aQuota, aExcludeOfflineFromUsage, aNewUsage));
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
                                 const nsAString& aKey,
                                 bool aExcludeOfflineFromUsage,
                                 PRInt32 aKeyUsage)
{
  IMPL_FORWARDER(RemoveKey(aStorage, aKey, aExcludeOfflineFromUsage, aKeyUsage));
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
  return mSessionOnlyDB.RemoveOwner(aHostName, true);
}

nsresult
nsDOMStorageDBWrapper::DropPrivateBrowsingStorages()
{
  return mPrivateBrowsingDB.RemoveAll();
}

nsresult
nsDOMStorageDBWrapper::RemoveOwner(const nsACString& aOwner,
                                   bool aIncludeSubDomains)
{
  nsresult rv;

  rv = mPrivateBrowsingDB.RemoveOwner(aOwner, aIncludeSubDomains);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return NS_OK;

  rv = mSessionOnlyDB.RemoveOwner(aOwner, aIncludeSubDomains);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPersistentDB.RemoveOwner(aOwner, aIncludeSubDomains);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}


nsresult
nsDOMStorageDBWrapper::RemoveOwners(const nsTArray<nsString> &aOwners,
                                    bool aIncludeSubDomains, bool aMatch)
{
  nsresult rv;

  rv = mPrivateBrowsingDB.RemoveOwners(aOwners, aIncludeSubDomains, aMatch);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return NS_OK;

  rv = mSessionOnlyDB.RemoveOwners(aOwners, aIncludeSubDomains, aMatch);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPersistentDB.RemoveOwners(aOwners, aIncludeSubDomains, aMatch);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsDOMStorageDBWrapper::RemoveAll()
{
  nsresult rv;

  rv = mPrivateBrowsingDB.RemoveAll();
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return NS_OK;

  rv = mSessionOnlyDB.RemoveAll();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPersistentDB.RemoveAll();
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsDOMStorageDBWrapper::GetUsage(DOMStorageImpl* aStorage,
                                bool aExcludeOfflineFromUsage, PRInt32 *aUsage)
{
  IMPL_FORWARDER(GetUsage(aStorage, aExcludeOfflineFromUsage, aUsage));
}

nsresult
nsDOMStorageDBWrapper::GetUsage(const nsACString& aDomain,
                                bool aIncludeSubDomains, PRInt32 *aUsage)
{
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return mPrivateBrowsingDB.GetUsage(aDomain, aIncludeSubDomains, aUsage);

#if 0
  // XXX Check where from all this method gets called, not sure this should
  // include any potential session-only data
  nsresult rv;
  rv = mSessionOnlyDB.GetUsage(aDomain, aIncludeSubDomains, aUsage);
  if (NS_SUECEEDED(rv))
    return rv;
#endif

  return mPersistentDB.GetUsage(aDomain, aIncludeSubDomains, aUsage);
}

nsresult
nsDOMStorageDBWrapper::CreateOriginScopeDBKey(nsIURI* aUri, nsACString& aKey)
{
  nsresult rv;

  rv = CreateDomainScopeDBKey(aUri, aKey);
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString scheme;
  rv = aUri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  aKey.AppendLiteral(":");
  aKey.Append(scheme);

  PRInt32 port = NS_GetRealPort(aUri);
  if (port != -1) {
    aKey.AppendLiteral(":");
    aKey.Append(nsPrintfCString(32, "%d", port));
  }

  return NS_OK;
}

nsresult
nsDOMStorageDBWrapper::CreateDomainScopeDBKey(nsIURI* aUri, nsACString& aKey)
{
  nsresult rv;

  nsCAutoString domainScope;
  rv = aUri->GetAsciiHost(domainScope);
  NS_ENSURE_SUCCESS(rv, rv);

  if (domainScope.IsEmpty()) {
    // About pages have an empty host but a valid path.  Since they are handled
    // internally by our own redirector, we can trust them and use path as key.
    // if file:/// protocol, let's make the exact directory the domain
    bool isScheme = false;
    if ((NS_SUCCEEDED(aUri->SchemeIs("about", &isScheme)) && isScheme) ||
        (NS_SUCCEEDED(aUri->SchemeIs("moz-safe-about", &isScheme)) && isScheme)) {
      rv = aUri->GetPath(domainScope);
      NS_ENSURE_SUCCESS(rv, rv);
      // While the host is always canonicalized to lowercase, the path is not,
      // thus need to force the casing.
      ToLowerCase(domainScope);
    }
    else if (NS_SUCCEEDED(aUri->SchemeIs("file", &isScheme)) && isScheme) {
      nsCOMPtr<nsIURL> url = do_QueryInterface(aUri, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = url->GetDirectory(domainScope);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  rv = CreateDomainScopeDBKey(domainScope, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMStorageDBWrapper::CreateDomainScopeDBKey(const nsACString& aAsciiDomain,
                                              nsACString& aKey)
{
  if (aAsciiDomain.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  ReverseString(aAsciiDomain, aKey);

  aKey.AppendLiteral(".");
  return NS_OK;
}

nsresult
nsDOMStorageDBWrapper::CreateQuotaDomainDBKey(const nsACString& aAsciiDomain,
                                              bool aIncludeSubDomains,
                                              bool aEffectiveTLDplus1Only,
                                              nsACString& aKey)
{
  nsresult rv;

  nsCAutoString subdomainsDBKey;
  if (aEffectiveTLDplus1Only) {
    nsCOMPtr<nsIEffectiveTLDService> eTLDService(do_GetService(
      NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("http://") + aAsciiDomain);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString eTLDplusOne;
    rv = eTLDService->GetBaseDomain(uri, 0, eTLDplusOne);
    if (NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS == rv) {
      // XXX bug 357323 - what to do for localhost/file exactly?
      eTLDplusOne = aAsciiDomain;
      rv = NS_OK;
    }
    NS_ENSURE_SUCCESS(rv, rv);

    CreateDomainScopeDBKey(eTLDplusOne, subdomainsDBKey);
  }
  else
    CreateDomainScopeDBKey(aAsciiDomain, subdomainsDBKey);

  if (!aIncludeSubDomains)
    subdomainsDBKey.AppendLiteral(":");

  aKey.Assign(subdomainsDBKey);
  return NS_OK;
}

nsresult
nsDOMStorageDBWrapper::GetDomainFromScopeKey(const nsACString& aScope,
                                         nsACString& aDomain)
{
  nsCAutoString reverseDomain, scope;
  scope = aScope;
  scope.Left(reverseDomain, scope.FindChar(':')-1);

  ReverseString(reverseDomain, aDomain);
  return NS_OK;
}

void
nsDOMStorageDBWrapper::EnsureTempTableFlushTimer()
{
  if (!mTempTableFlushTimer) {
    nsresult rv;
    mTempTableFlushTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);

    if (!NS_SUCCEEDED(rv)) {
      mTempTableFlushTimer = nsnull;
      return;
    }

    mTempTableFlushTimer->Init(nsDOMStorageManager::gStorageManager, 5000,
                               nsITimer::TYPE_REPEATING_SLACK);
  }
}

void
nsDOMStorageDBWrapper::StopTempTableFlushTimer()
{
  if (mTempTableFlushTimer) {
    mTempTableFlushTimer->Cancel();
    mTempTableFlushTimer = nsnull;
  }
}

