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
  if (mFlushTimer) {
    mFlushTimer->Cancel();
  }
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

  mFlushTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mFlushTimer->Init(nsDOMStorageManager::gStorageManager, 5000,
                         nsITimer::TYPE_REPEATING_SLACK);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMStorageDBWrapper::EnsureLoadTemporaryTableForStorage(nsDOMStorage* aStorage)
{
  if (aStorage->CanUseChromePersist())
    return mChromePersistentDB.EnsureLoadTemporaryTableForStorage(aStorage);
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return NS_OK;
  if (aStorage->SessionOnly())
    return NS_OK;

  return mPersistentDB.EnsureLoadTemporaryTableForStorage(aStorage);
}

nsresult
nsDOMStorageDBWrapper::FlushAndDeleteTemporaryTableForStorage(nsDOMStorage* aStorage)
{
  if (aStorage->CanUseChromePersist())
    return mChromePersistentDB.FlushAndDeleteTemporaryTableForStorage(aStorage);
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return NS_OK;
  if (aStorage->SessionOnly())
    return NS_OK;

  return mPersistentDB.FlushAndDeleteTemporaryTableForStorage(aStorage);
}

nsresult
nsDOMStorageDBWrapper::GetAllKeys(nsDOMStorage* aStorage,
                                  nsTHashtable<nsSessionStorageEntry>* aKeys)
{
  if (aStorage->CanUseChromePersist())
    return mChromePersistentDB.GetAllKeys(aStorage, aKeys);
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return mPrivateBrowsingDB.GetAllKeys(aStorage, aKeys);
  if (aStorage->SessionOnly())
    return mSessionOnlyDB.GetAllKeys(aStorage, aKeys);

  return mPersistentDB.GetAllKeys(aStorage, aKeys);
}

nsresult
nsDOMStorageDBWrapper::GetKeyValue(nsDOMStorage* aStorage,
                                   const nsAString& aKey,
                                   nsAString& aValue,
                                   PRBool* aSecure)
{
  if (aStorage->CanUseChromePersist())
    return mChromePersistentDB.GetKeyValue(aStorage, aKey, aValue, aSecure);
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return mPrivateBrowsingDB.GetKeyValue(aStorage, aKey, aValue, aSecure);
  if (aStorage->SessionOnly())
    return mSessionOnlyDB.GetKeyValue(aStorage, aKey, aValue, aSecure);

  return mPersistentDB.GetKeyValue(aStorage, aKey, aValue, aSecure);
}

nsresult
nsDOMStorageDBWrapper::SetKey(nsDOMStorage* aStorage,
                              const nsAString& aKey,
                              const nsAString& aValue,
                              PRBool aSecure,
                              PRInt32 aQuota,
                              PRBool aExcludeOfflineFromUsage,
                              PRInt32 *aNewUsage)
{
  if (aStorage->CanUseChromePersist())
    return mChromePersistentDB.SetKey(aStorage, aKey, aValue, aSecure,
                                      aQuota, aExcludeOfflineFromUsage, aNewUsage);
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return mPrivateBrowsingDB.SetKey(aStorage, aKey, aValue, aSecure,
                                          aQuota, aExcludeOfflineFromUsage, aNewUsage);
  if (aStorage->SessionOnly())
    return mSessionOnlyDB.SetKey(aStorage, aKey, aValue, aSecure,
                                      aQuota, aExcludeOfflineFromUsage, aNewUsage);

  return mPersistentDB.SetKey(aStorage, aKey, aValue, aSecure,
                                   aQuota, aExcludeOfflineFromUsage, aNewUsage);
}

nsresult
nsDOMStorageDBWrapper::SetSecure(nsDOMStorage* aStorage,
                                 const nsAString& aKey,
                                 const PRBool aSecure)
{
  if (aStorage->CanUseChromePersist())
    return mChromePersistentDB.SetSecure(aStorage, aKey, aSecure);
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return mPrivateBrowsingDB.SetSecure(aStorage, aKey, aSecure);
  if (aStorage->SessionOnly())
    return mSessionOnlyDB.SetSecure(aStorage, aKey, aSecure);

  return mPersistentDB.SetSecure(aStorage, aKey, aSecure);
}

nsresult
nsDOMStorageDBWrapper::RemoveKey(nsDOMStorage* aStorage,
                                 const nsAString& aKey,
                                 PRBool aExcludeOfflineFromUsage,
                                 PRInt32 aKeyUsage)
{
  if (aStorage->CanUseChromePersist())
    return mChromePersistentDB.RemoveKey(aStorage, aKey, aExcludeOfflineFromUsage, aKeyUsage);
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return mPrivateBrowsingDB.RemoveKey(aStorage, aKey, aExcludeOfflineFromUsage, aKeyUsage);
  if (aStorage->SessionOnly())
    return mSessionOnlyDB.RemoveKey(aStorage, aKey, aExcludeOfflineFromUsage, aKeyUsage);

  return mPersistentDB.RemoveKey(aStorage, aKey, aExcludeOfflineFromUsage, aKeyUsage);
}

nsresult
nsDOMStorageDBWrapper::ClearStorage(nsDOMStorage* aStorage)
{
  if (aStorage->CanUseChromePersist())
    return mChromePersistentDB.ClearStorage(aStorage);
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return mPrivateBrowsingDB.ClearStorage(aStorage);
  if (aStorage->SessionOnly())
    return mSessionOnlyDB.ClearStorage(aStorage);

  return mPersistentDB.ClearStorage(aStorage);
}

nsresult
nsDOMStorageDBWrapper::DropSessionOnlyStoragesForHost(const nsACString& aHostName)
{
  return mSessionOnlyDB.RemoveOwner(aHostName, PR_TRUE);
}

nsresult
nsDOMStorageDBWrapper::DropPrivateBrowsingStorages()
{
  return mPrivateBrowsingDB.RemoveAll();
}

nsresult
nsDOMStorageDBWrapper::RemoveOwner(const nsACString& aOwner,
                                   PRBool aIncludeSubDomains)
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
nsDOMStorageDBWrapper::RemoveTimeRange(PRInt64 aSince)
{
  nsresult rv;

  rv = mPrivateBrowsingDB.RemoveTimeRange(aSince);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return NS_OK;

  rv = mSessionOnlyDB.RemoveTimeRange(aSince);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPersistentDB.RemoveTimeRange(aSince);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsDOMStorageDBWrapper::RemoveOwners(const nsTArray<nsString> &aOwners,
                                    PRBool aIncludeSubDomains, PRBool aMatch)
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
nsDOMStorageDBWrapper::GetUsage(nsDOMStorage* aStorage,
                                PRBool aExcludeOfflineFromUsage, PRInt32 *aUsage)
{
  if (aStorage->CanUseChromePersist())
    return mChromePersistentDB.GetUsage(aStorage, aExcludeOfflineFromUsage, aUsage);    
  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode())
    return mPrivateBrowsingDB.GetUsage(aStorage, aExcludeOfflineFromUsage, aUsage);
  if (aStorage->SessionOnly())
    return mSessionOnlyDB.GetUsage(aStorage, aExcludeOfflineFromUsage, aUsage);

  return mPersistentDB.GetUsage(aStorage, aExcludeOfflineFromUsage, aUsage);
}

nsresult
nsDOMStorageDBWrapper::GetUsage(const nsACString& aDomain,
                                PRBool aIncludeSubDomains, PRInt32 *aUsage)
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
    PRBool isAboutUrl = PR_FALSE;
    if ((NS_SUCCEEDED(aUri->SchemeIs("about", &isAboutUrl)) && isAboutUrl) ||
        (NS_SUCCEEDED(aUri->SchemeIs("moz-safe-about", &isAboutUrl)) && isAboutUrl)) {
      rv = aUri->GetPath(domainScope);
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
                                              PRBool aIncludeSubDomains,
                                              PRBool aEffectiveTLDplus1Only,
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
