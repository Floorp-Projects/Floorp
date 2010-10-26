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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Neil Deakin <enndeakin@sympatico.ca>
 *   Johnny Stenback <jst@mozilla.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

#include "prnetdb.h"
#include "nsCOMPtr.h"
#include "nsDOMError.h"
#include "nsDOMClassInfo.h"
#include "nsUnicharUtils.h"
#include "nsIDocument.h"
#include "nsDOMStorage.h"
#include "nsEscape.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsReadableUtils.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsIPrefBranch.h"
#include "nsICookiePermission.h"
#include "nsIPermission.h"
#include "nsIPermissionManager.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsIJSContextStack.h"
#include "nsIPrivateBrowsingService.h"
#include "nsDOMString.h"
#include "nsNetCID.h"
#include "nsIProxyObjectManager.h"

static const PRUint32 ASK_BEFORE_ACCEPT = 1;
static const PRUint32 ACCEPT_SESSION = 2;
static const PRUint32 BEHAVIOR_REJECT = 2;

static const PRUint32 DEFAULT_QUOTA = 5 * 1024;
// Be generous with offline apps by default...
static const PRUint32 DEFAULT_OFFLINE_APP_QUOTA = 200 * 1024;
// ... but warn if it goes over this amount
static const PRUint32 DEFAULT_OFFLINE_WARN_QUOTA = 50 * 1024;

// Intervals to flush the temporary table after in seconds
#define NS_DOMSTORAGE_MAXIMUM_TEMPTABLE_INACTIVITY_TIME (5)
#define NS_DOMSTORAGE_MAXIMUM_TEMPTABLE_AGE (30)

static const char kPermissionType[] = "cookie";
static const char kStorageEnabled[] = "dom.storage.enabled";
static const char kDefaultQuota[] = "dom.storage.default_quota";
static const char kCookiesBehavior[] = "network.cookie.cookieBehavior";
static const char kCookiesLifetimePolicy[] = "network.cookie.lifetimePolicy";
static const char kOfflineAppWarnQuota[] = "offline-apps.quota.warn";
static const char kOfflineAppQuota[] = "offline-apps.quota.max";

// The URI returned is the innermost URI that should be used for
// security-check-like stuff.  aHost is its hostname, correctly canonicalized.
static nsresult
GetPrincipalURIAndHost(nsIPrincipal* aPrincipal, nsIURI** aURI, nsCString& aHost)
{
  nsresult rv = aPrincipal->GetDomain(aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!*aURI) {
    rv = aPrincipal->GetURI(aURI);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!*aURI) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(*aURI);
  if (!innerURI) {
    return NS_ERROR_UNEXPECTED;
  }

  rv = innerURI->GetAsciiHost(aHost);
  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  
  innerURI.swap(*aURI);

  return NS_OK;
}

//
// Helper that tells us whether the caller is secure or not.
//

static PRBool
IsCallerSecure()
{
  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  nsContentUtils::GetSecurityManager()->
    GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));

  if (!subjectPrincipal) {
    // No subject principal means no code is running. Default to not
    // being secure in that case.

    return PR_FALSE;
  }

  nsCOMPtr<nsIURI> codebase;
  subjectPrincipal->GetURI(getter_AddRefs(codebase));

  if (!codebase) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIURI> innerUri = NS_GetInnermostURI(codebase);

  if (!innerUri) {
    return PR_FALSE;
  }

  PRBool isHttps = PR_FALSE;
  nsresult rv = innerUri->SchemeIs("https", &isHttps);

  return NS_SUCCEEDED(rv) && isHttps;
}

PRUint32
GetOfflinePermission(const nsACString &aDomain)
{
  // Fake a URI for the permission manager
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("http://") + aDomain);

  PRUint32 perm;
  if (uri) {
    nsCOMPtr<nsIPermissionManager> permissionManager =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

    if (permissionManager &&
        NS_SUCCEEDED(permissionManager->TestPermission(uri, "offline-app", &perm)))
        return perm;
  }

  return nsIPermissionManager::UNKNOWN_ACTION;
}

PRBool
IsOfflineAllowed(const nsACString &aDomain)
{
  PRInt32 perm = GetOfflinePermission(aDomain);
  return IS_PERMISSION_ALLOWED(perm);
}

// Returns two quotas - A hard limit for which adding data will be an error,
// and a limit after which a warning event will be sent to the observer
// service.  The warn limit may be -1, in which case there will be no warning.
// If aOverrideQuota is set, the larger offline apps quota is used and no
// warning is sent.
static PRUint32
GetQuota(const nsACString &aDomain, PRInt32 *aQuota, PRInt32 *aWarnQuota,
         bool aOverrideQuota)
{
  PRUint32 perm = GetOfflinePermission(aDomain);
  if (IS_PERMISSION_ALLOWED(perm) || aOverrideQuota) {
    // This is an offline app, give more space by default.
    *aQuota = ((PRInt32)nsContentUtils::GetIntPref(kOfflineAppQuota,
                                                   DEFAULT_OFFLINE_APP_QUOTA) * 1024);

    if (perm == nsIOfflineCacheUpdateService::ALLOW_NO_WARN ||
        aOverrideQuota) {
      *aWarnQuota = -1;
    } else {
      *aWarnQuota = ((PRInt32)nsContentUtils::GetIntPref(kOfflineAppWarnQuota,
                                                         DEFAULT_OFFLINE_WARN_QUOTA) * 1024);
    }
    return perm;
  }

  // FIXME: per-domain quotas?
  *aQuota = ((PRInt32)nsContentUtils::GetIntPref(kDefaultQuota,
                                                 DEFAULT_QUOTA) * 1024);
  *aWarnQuota = -1;

  return perm;
}

nsSessionStorageEntry::nsSessionStorageEntry(KeyTypePointer aStr)
  : nsStringHashKey(aStr), mItem(nsnull)
{
}

nsSessionStorageEntry::nsSessionStorageEntry(const nsSessionStorageEntry& aToCopy)
  : nsStringHashKey(aToCopy), mItem(nsnull)
{
  NS_ERROR("We're horked.");
}

nsSessionStorageEntry::~nsSessionStorageEntry()
{
}

//
// nsDOMStorageManager
//

nsDOMStorageManager* nsDOMStorageManager::gStorageManager;

nsDOMStorageManager::nsDOMStorageManager()
  : mInPrivateBrowsing(PR_FALSE)
{
}

NS_IMPL_ISUPPORTS2(nsDOMStorageManager,
                   nsIDOMStorageManager,
                   nsIObserver)

//static
nsresult
nsDOMStorageManager::Initialize()
{
  gStorageManager = new nsDOMStorageManager();
  if (!gStorageManager)
    return NS_ERROR_OUT_OF_MEMORY;

  if (!gStorageManager->mStorages.Init()) {
    delete gStorageManager;
    gStorageManager = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(gStorageManager);

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os)
    return NS_OK;

  os->AddObserver(gStorageManager, "cookie-changed", PR_FALSE);
  os->AddObserver(gStorageManager, "offline-app-removed", PR_FALSE);
  os->AddObserver(gStorageManager, NS_PRIVATE_BROWSING_SWITCH_TOPIC, PR_FALSE);
  os->AddObserver(gStorageManager, "profile-after-change", PR_FALSE);
  os->AddObserver(gStorageManager, "perm-changed", PR_FALSE);

  return NS_OK;
}

//static
nsDOMStorageManager*
nsDOMStorageManager::GetInstance()
{
  NS_ASSERTION(gStorageManager,
               "nsDOMStorageManager::GetInstance() called before Initialize()");
  NS_IF_ADDREF(gStorageManager);
  return gStorageManager;
}

//static
void
nsDOMStorageManager::Shutdown()
{
  NS_IF_RELEASE(gStorageManager);
  gStorageManager = nsnull;

#ifdef MOZ_STORAGE
  delete nsDOMStorage::gStorageDB;
  nsDOMStorage::gStorageDB = nsnull;
#endif
}

static PLDHashOperator
ClearStorage(nsDOMStorageEntry* aEntry, void* userArg)
{
  aEntry->mStorage->ClearAll();
  return PL_DHASH_REMOVE;
}

static nsresult
GetOfflineDomains(nsTArray<nsString>& aDomains)
{
  nsCOMPtr<nsIPermissionManager> permissionManager =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  if (permissionManager) {
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    nsresult rv = permissionManager->GetEnumerator(getter_AddRefs(enumerator));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> supp;
      rv = enumerator->GetNext(getter_AddRefs(supp));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIPermission> perm(do_QueryInterface(supp, &rv));
      NS_ENSURE_SUCCESS(rv, rv);

      PRUint32 capability;
      rv = perm->GetCapability(&capability);
      NS_ENSURE_SUCCESS(rv, rv);
      if (capability != nsIPermissionManager::DENY_ACTION) {
        nsCAutoString type;
        rv = perm->GetType(type);
        NS_ENSURE_SUCCESS(rv, rv);

        if (type.EqualsLiteral("offline-app")) {
          nsCAutoString host;
          rv = perm->GetHost(host);
          NS_ENSURE_SUCCESS(rv, rv);

          aDomains.AppendElement(NS_ConvertUTF8toUTF16(host));
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsDOMStorageManager::Observe(nsISupports *aSubject,
                             const char *aTopic,
                             const PRUnichar *aData)
{
  if (!strcmp(aTopic, "profile-after-change")) {
    nsCOMPtr<nsIPrivateBrowsingService> pbs =
      do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
    if (pbs)
      pbs->GetPrivateBrowsingEnabled(&gStorageManager->mInPrivateBrowsing);
  }
  else if (!strcmp(aTopic, "offline-app-removed")) {
#ifdef MOZ_STORAGE
    nsresult rv = nsDOMStorage::InitDB();
    NS_ENSURE_SUCCESS(rv, rv);
    return nsDOMStorage::gStorageDB->RemoveOwner(NS_ConvertUTF16toUTF8(aData),
                                                 PR_TRUE);
#endif
  } else if (!strcmp(aTopic, "cookie-changed") &&
             !nsCRT::strcmp(aData, NS_LITERAL_STRING("cleared").get())) {
    mStorages.EnumerateEntries(ClearStorage, nsnull);

#ifdef MOZ_STORAGE
    nsresult rv = nsDOMStorage::InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    // Remove global storage for domains that aren't marked for offline use.
    nsTArray<nsString> domains;
    rv = GetOfflineDomains(domains);
    NS_ENSURE_SUCCESS(rv, rv);
    return nsDOMStorage::gStorageDB->RemoveOwners(domains, PR_TRUE, PR_FALSE);
#endif
  } else if (!strcmp(aTopic, NS_PRIVATE_BROWSING_SWITCH_TOPIC)) {
    mStorages.EnumerateEntries(ClearStorage, nsnull);
    if (!nsCRT::strcmp(aData, NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).get()))
      mInPrivateBrowsing = PR_TRUE;
    else if (!nsCRT::strcmp(aData, NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).get()))
      mInPrivateBrowsing = PR_FALSE;
#ifdef MOZ_STORAGE
    nsresult rv = nsDOMStorage::InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    return nsDOMStorage::gStorageDB->DropPrivateBrowsingStorages();
#endif
  } else if (!strcmp(aTopic, "perm-changed")) {
    // Check for cookie permission change
    nsCOMPtr<nsIPermission> perm(do_QueryInterface(aSubject));
    if (perm) {
      nsCAutoString type;
      perm->GetType(type);
      if (type != NS_LITERAL_CSTRING("cookie"))
        return NS_OK;

      PRUint32 cap = 0;
      perm->GetCapability(&cap);
      if (!(cap & nsICookiePermission::ACCESS_SESSION) ||
          nsDependentString(aData) != NS_LITERAL_STRING("deleted"))
        return NS_OK;

      nsCAutoString host;
      perm->GetHost(host);
      if (host.IsEmpty())
        return NS_OK;

#ifdef MOZ_STORAGE
      nsresult rv = nsDOMStorage::InitDB();
      NS_ENSURE_SUCCESS(rv, rv);

      return nsDOMStorage::gStorageDB->DropSessionOnlyStoragesForHost(host);
#endif
    }
  } else if (!strcmp(aTopic, "timer-callback")) {
    nsCOMPtr<nsIObserverService> obsserv = mozilla::services::GetObserverService();
    if (obsserv)
      obsserv->NotifyObservers(nsnull, NS_DOMSTORAGE_FLUSH_TIMER_OBSERVER, nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorageManager::GetUsage(const nsAString& aDomain,
                              PRInt32 *aUsage)
{
  nsresult rv = nsDOMStorage::InitDB();
  NS_ENSURE_SUCCESS(rv, rv);

  return nsDOMStorage::gStorageDB->GetUsage(NS_ConvertUTF16toUTF8(aDomain),
                                            PR_FALSE, aUsage);
}

NS_IMETHODIMP
nsDOMStorageManager::ClearOfflineApps()
{
    nsresult rv = nsDOMStorage::InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    nsTArray<nsString> domains;
    rv = GetOfflineDomains(domains);
    NS_ENSURE_SUCCESS(rv, rv);
    return nsDOMStorage::gStorageDB->RemoveOwners(domains, PR_TRUE, PR_TRUE);
}

NS_IMETHODIMP
nsDOMStorageManager::GetLocalStorageForPrincipal(nsIPrincipal *aPrincipal,
                                                 const nsSubstring &aDocumentURI,
                                                 nsIDOMStorage **aResult)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);
  *aResult = nsnull;

  nsresult rv;

  nsRefPtr<nsDOMStorage2> storage = new nsDOMStorage2();
  if (!storage)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = storage->InitAsLocalStorage(aPrincipal, aDocumentURI);
  if (NS_FAILED(rv))
    return rv;

  *aResult = storage.get();
  storage.forget();

  return NS_OK;
}

void
nsDOMStorageManager::AddToStoragesHash(nsDOMStorage* aStorage)
{
  nsDOMStorageEntry* entry = mStorages.PutEntry(aStorage);
  if (entry)
    entry->mStorage = aStorage;
}

void
nsDOMStorageManager::RemoveFromStoragesHash(nsDOMStorage* aStorage)
{
  nsDOMStorageEntry* entry = mStorages.GetEntry(aStorage);
  if (entry)
    mStorages.RemoveEntry(aStorage);
}

//
// nsDOMStorage
//

#ifdef MOZ_STORAGE
nsDOMStorageDBWrapper* nsDOMStorage::gStorageDB = nsnull;
#endif

nsDOMStorageEntry::nsDOMStorageEntry(KeyTypePointer aStr)
  : nsVoidPtrHashKey(aStr), mStorage(nsnull)
{
}

nsDOMStorageEntry::nsDOMStorageEntry(const nsDOMStorageEntry& aToCopy)
  : nsVoidPtrHashKey(aToCopy), mStorage(nsnull)
{
  NS_ERROR("DOMStorage horked.");
}

nsDOMStorageEntry::~nsDOMStorageEntry()
{
}

PLDHashOperator
SessionStorageTraverser(nsSessionStorageEntry* aEntry, void* userArg) {
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(userArg);

  cb->NoteXPCOMChild((nsIDOMStorageItem *) aEntry->mItem);

  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMStorage)
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsDOMStorage)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMStorage)
  {
    if (tmp->mItems.IsInitialized()) {
      tmp->mItems.EnumerateEntries(SessionStorageTraverser, &cb);
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(StorageObsolete, nsDOMStorage)

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsDOMStorage, nsIDOMStorageObsolete)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsDOMStorage, nsIDOMStorageObsolete)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMStorage)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorageObsolete)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageObsolete)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(StorageObsolete)
NS_INTERFACE_MAP_END

nsresult
NS_NewDOMStorage(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  nsDOMStorage* storage = new nsDOMStorage();
  if (!storage)
    return NS_ERROR_OUT_OF_MEMORY;

  return storage->QueryInterface(aIID, aResult);
}

nsresult
NS_NewDOMStorage2(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  nsDOMStorage2* storage = new nsDOMStorage2();
  if (!storage)
    return NS_ERROR_OUT_OF_MEMORY;

  return storage->QueryInterface(aIID, aResult);
}

nsDOMStorage::nsDOMStorage()
  : mUseDB(PR_FALSE)
  , mSessionOnly(PR_TRUE)
  , mStorageType(nsPIDOMStorage::Unknown)
  , mItemsCached(PR_FALSE)
  , mEventBroadcaster(nsnull)
  , mCanUseChromePersist(false)
  , mLoadedTemporaryTable(false)
{
  mSecurityChecker = this;
  mItems.Init(8);
  if (nsDOMStorageManager::gStorageManager)
    nsDOMStorageManager::gStorageManager->AddToStoragesHash(this);
}

nsDOMStorage::nsDOMStorage(nsDOMStorage& aThat)
  : mUseDB(PR_FALSE) // Any clone is not using the database
  , mDomain(aThat.mDomain)
  , mSessionOnly(PR_TRUE)
  , mStorageType(aThat.mStorageType)
  , mItemsCached(PR_FALSE)
#ifdef MOZ_STORAGE
  , mScopeDBKey(aThat.mScopeDBKey)
#endif
  , mEventBroadcaster(nsnull)
  , mCanUseChromePersist(aThat.mCanUseChromePersist)
  , mLoadedTemporaryTable(aThat.mLoadedTemporaryTable)
  , mLastTemporaryTableAccessTime(aThat.mLastTemporaryTableAccessTime)
  , mTemporaryTableAge(aThat.mTemporaryTableAge)
{
  mSecurityChecker = this;
  mItems.Init(8);

  if (nsDOMStorageManager::gStorageManager)
    nsDOMStorageManager::gStorageManager->AddToStoragesHash(this);
}

nsDOMStorage::~nsDOMStorage()
{
  if (nsDOMStorageManager::gStorageManager)
    nsDOMStorageManager::gStorageManager->RemoveFromStoragesHash(this);
}

static
nsresult
GetDomainURI(nsIPrincipal *aPrincipal, PRBool aIncludeDomain, nsIURI **_domain)
{
  nsCOMPtr<nsIURI> uri;

  if (aIncludeDomain) {
    nsresult rv = aPrincipal->GetDomain(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!uri) {
    nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Check if we really got any URI. System principal doesn't return a URI
  // instance and we would crash in NS_GetInnermostURI below.
  if (!uri)
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
  if (!innerURI)
    return NS_ERROR_UNEXPECTED;
  innerURI.forget(_domain);

  return NS_OK;
}

nsresult
nsDOMStorage::InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI)
{
  nsCOMPtr<nsIURI> domainURI;
  nsresult rv = GetDomainURI(aPrincipal, PR_TRUE, getter_AddRefs(domainURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // No need to check for a return value. If this would fail we would not get
  // here as we call GetPrincipalURIAndHost (nsDOMStorage.cpp:88) from
  // nsDOMStorage::CanUseStorage before we query the storage manager for a new
  // sessionStorage. It calls GetAsciiHost on innermost URI. If it fails, we
  // won't get to InitAsSessionStorage.
  domainURI->GetAsciiHost(mDomain);

  mDocumentURI = aDocumentURI;

#ifdef MOZ_STORAGE
  mUseDB = PR_FALSE;
  mScopeDBKey.Truncate();
  mQuotaDomainDBKey.Truncate();
#endif

  mStorageType = SessionStorage;
  return NS_OK;
}

nsresult
nsDOMStorage::InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI)
{
  nsCOMPtr<nsIURI> domainURI;
  nsresult rv = GetDomainURI(aPrincipal, PR_FALSE, getter_AddRefs(domainURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // No need to check for a return value. If this would fail we would not get
  // here as we call GetPrincipalURIAndHost (nsDOMStorage.cpp:88) from
  // nsDOMStorage::CanUseStorage before we query the storage manager for a new
  // localStorage. It calls GetAsciiHost on innermost URI. If it fails, we won't
  // get to InitAsLocalStorage. Actually, mDomain will get replaced with
  // mPrincipal in bug 455070. It is not even used for localStorage.
  domainURI->GetAsciiHost(mDomain);

  mDocumentURI = aDocumentURI;

#ifdef MOZ_STORAGE
  nsDOMStorageDBWrapper::CreateOriginScopeDBKey(domainURI, mScopeDBKey);

  // XXX Bug 357323, we have to solve the issue how to define
  // origin for file URLs. In that case CreateOriginScopeDBKey
  // fails (the result is empty) and we must avoid database use
  // in that case because it produces broken entries w/o owner.
  mUseDB = !mScopeDBKey.IsEmpty();

  nsDOMStorageDBWrapper::CreateQuotaDomainDBKey(mDomain,
      PR_TRUE, PR_FALSE, mQuotaDomainDBKey);
  nsDOMStorageDBWrapper::CreateQuotaDomainDBKey(mDomain,
      PR_TRUE, PR_TRUE, mQuotaETLDplus1DomainDBKey);
#endif

  mStorageType = LocalStorage;

  nsCOMPtr<nsIURI> URI;
  if (NS_SUCCEEDED(aPrincipal->GetURI(getter_AddRefs(URI))) && URI) {
    mCanUseChromePersist = URICanUseChromePersist(URI);
  }

  RegisterObservers();

  return NS_OK;
}

nsresult
nsDOMStorage::InitAsGlobalStorage(const nsACString &aDomainDemanded)
{
  mDomain = aDomainDemanded;
#ifdef MOZ_STORAGE
  nsDOMStorageDBWrapper::CreateDomainScopeDBKey(aDomainDemanded, mScopeDBKey);

  // XXX Bug 357323, we have to solve the issue how to define
  // origin for file URLs. In that case CreateOriginScopeDBKey
  // fails (the result is empty) and we must avoid database use
  // in that case because it produces broken entries w/o owner.
  if (!(mUseDB = !mScopeDBKey.IsEmpty()))
    mScopeDBKey.AppendLiteral(":");

  nsDOMStorageDBWrapper::CreateQuotaDomainDBKey(aDomainDemanded,
      PR_TRUE, PR_FALSE, mQuotaDomainDBKey);
  nsDOMStorageDBWrapper::CreateQuotaDomainDBKey(aDomainDemanded,
      PR_TRUE, PR_TRUE, mQuotaETLDplus1DomainDBKey);
#endif

  mStorageType = GlobalStorage;
  mEventBroadcaster = this;

  RegisterObservers();

  return NS_OK;
}

static PLDHashOperator
CopyStorageItems(nsSessionStorageEntry* aEntry, void* userArg)
{
  nsDOMStorage* newstorage = static_cast<nsDOMStorage*>(userArg);

  newstorage->SetItem(aEntry->GetKey(), aEntry->mItem->GetValueInternal());

  if (aEntry->mItem->IsSecure()) {
    newstorage->SetSecure(aEntry->GetKey(), PR_TRUE);
  }

  return PL_DHASH_NEXT;
}

nsresult
nsDOMStorage::CloneFrom(nsDOMStorage* aThat)
{
  aThat->mItems.EnumerateEntries(CopyStorageItems, this);
  return NS_OK;
}

//static
PRBool
nsDOMStorage::CanUseStorage(PRPackedBool* aSessionOnly)
{
  // check if the calling domain can use storage. Downgrade to session
  // only if only session storage may be used.
  NS_ASSERTION(aSessionOnly, "null session flag");
  *aSessionOnly = PR_FALSE;

  if (!nsContentUtils::GetBoolPref(kStorageEnabled))
    return PR_FALSE;

  // chrome can always use storage regardless of permission preferences
  if (nsContentUtils::IsCallerChrome())
    return PR_TRUE;

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  nsContentUtils::GetSecurityManager()->
    GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));

  // if subjectPrincipal were null we'd have returned after
  // IsCallerChrome().

  nsCOMPtr<nsIURI> subjectURI;
  nsCAutoString unused;
  if (NS_FAILED(GetPrincipalURIAndHost(subjectPrincipal,
                                       getter_AddRefs(subjectURI),
                                       unused))) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIPermissionManager> permissionManager =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  if (!permissionManager)
    return PR_FALSE;

  PRUint32 perm;
  permissionManager->TestPermission(subjectURI, kPermissionType, &perm);

  if (perm == nsIPermissionManager::DENY_ACTION)
    return PR_FALSE;

  // In private browsing mode we ougth to behave as in session-only cookies
  // mode to prevent detection of being in private browsing mode and ensuring
  // that there will be no traces left.
  if (perm == nsICookiePermission::ACCESS_SESSION ||
      nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode()) {
    *aSessionOnly = PR_TRUE;
  }
  else if (perm != nsIPermissionManager::ALLOW_ACTION) {
    PRUint32 cookieBehavior = nsContentUtils::GetIntPref(kCookiesBehavior);
    PRUint32 lifetimePolicy = nsContentUtils::GetIntPref(kCookiesLifetimePolicy);

    // Treat "ask every time" as "reject always".
    // Chrome persistent pages can bypass this check.
    if ((cookieBehavior == BEHAVIOR_REJECT || lifetimePolicy == ASK_BEFORE_ACCEPT) &&
        !URICanUseChromePersist(subjectURI))
      return PR_FALSE;

    if (lifetimePolicy == ACCEPT_SESSION)
      *aSessionOnly = PR_TRUE;
  }

  return PR_TRUE;
}

PRBool
nsDOMStorage::CacheStoragePermissions()
{
  // Bug 488446, disallowing storage use when in session only mode.
  // This is temporary fix before we find complete solution for storage
  // behavior in private browsing mode or session-only cookies mode.
  if (!CanUseStorage(&mSessionOnly))
    return PR_FALSE;

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (!ssm)
    return PR_FALSE;

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  ssm->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));

  NS_ASSERTION(mSecurityChecker, "Has non-null mSecurityChecker");
  return mSecurityChecker->CanAccess(subjectPrincipal);
}

// static
PRBool
nsDOMStorage::URICanUseChromePersist(nsIURI* aURI) {
  PRBool isAbout;
  return
    (NS_SUCCEEDED(aURI->SchemeIs("moz-safe-about", &isAbout)) && isAbout) ||
    (NS_SUCCEEDED(aURI->SchemeIs("about", &isAbout)) && isAbout);
}

bool
nsDOMStorage::CanUseChromePersist()
{
  return mCanUseChromePersist;
}

class ItemCounterState
{
public:
  ItemCounterState(PRBool aIsCallerSecure)
    : mIsCallerSecure(aIsCallerSecure), mCount(0)
  {
  }

  PRBool mIsCallerSecure;
  PRBool mCount;
private:
  ItemCounterState(); // Not to be implemented
};

static PLDHashOperator
ItemCounter(nsSessionStorageEntry* aEntry, void* userArg)
{
  ItemCounterState *state = (ItemCounterState *)userArg;

  if (state->mIsCallerSecure || !aEntry->mItem->IsSecure()) {
    ++state->mCount;
  }

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsDOMStorage::GetLength(PRUint32 *aLength)
{
  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;

  // Force reload of items from database.  This ensures sync localStorages for
  // same origins among different windows.
  mItemsCached = PR_FALSE;
  if (UseDB())
    CacheKeysFromDB();

  ItemCounterState state(IsCallerSecure());

  mItems.EnumerateEntries(ItemCounter, &state);

  *aLength = state.mCount;

  return NS_OK;
}

class IndexFinderData
{
public:
  IndexFinderData(PRBool aIsCallerSecure, PRUint32 aWantedIndex)
    : mIsCallerSecure(aIsCallerSecure), mIndex(0), mWantedIndex(aWantedIndex),
      mItem(nsnull)
  {
  }

  PRBool mIsCallerSecure;
  PRUint32 mIndex;
  PRUint32 mWantedIndex;
  nsSessionStorageEntry *mItem;

private:
  IndexFinderData(); // Not to be implemented
};

static PLDHashOperator
IndexFinder(nsSessionStorageEntry* aEntry, void* userArg)
{
  IndexFinderData *data = (IndexFinderData *)userArg;

  if (data->mIndex == data->mWantedIndex &&
      (data->mIsCallerSecure || !aEntry->mItem->IsSecure())) {
    data->mItem = aEntry;

    return PL_DHASH_STOP;
  }

  ++data->mIndex;

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsDOMStorage::Key(PRUint32 aIndex, nsAString& aKey)
{
  // XXXjst: This is as retarded as the DOM spec is, takes an unsigned
  // int, but the spec talks about what to do if a negative value is
  // passed in.

  // XXX: This does a linear search for the key at index, which would
  // suck if there's a large numer of indexes. Do we care? If so,
  // maybe we need to have a lazily populated key array here or
  // something?

  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;

  if (UseDB())
    CacheKeysFromDB();

  IndexFinderData data(IsCallerSecure(), aIndex);
  mItems.EnumerateEntries(IndexFinder, &data);

  if (!data.mItem) {
    // aIndex was larger than the number of accessible keys. Throw.
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  aKey = data.mItem->GetKey();

  return NS_OK;
}

nsIDOMStorageItem*
nsDOMStorage::GetNamedItem(const nsAString& aKey, nsresult* aResult)
{
  if (!CacheStoragePermissions()) {
    *aResult = NS_ERROR_DOM_SECURITY_ERR;
    return nsnull;
  }

  *aResult = NS_OK;
  if (aKey.IsEmpty())
    return nsnull;

  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);
  nsIDOMStorageItem* item = nsnull;
  if (entry) {
    if (IsCallerSecure() || !entry->mItem->IsSecure()) {
      item = entry->mItem;
    }
  }
  else if (UseDB()) {
    PRBool secure;
    nsAutoString value;
    nsresult rv = GetDBValue(aKey, value, &secure);
    // return null if access isn't allowed or the key wasn't found
    if (rv == NS_ERROR_DOM_SECURITY_ERR || rv == NS_ERROR_DOM_NOT_FOUND_ERR)
      return nsnull;

    *aResult = rv;
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsRefPtr<nsDOMStorageItem> newitem =
      new nsDOMStorageItem(this, aKey, value, secure);
    if (newitem && (entry = mItems.PutEntry(aKey))) {
      item = entry->mItem = newitem;
    }
    else {
      *aResult = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return item;
}

nsresult
nsDOMStorage::GetItem(const nsAString& aKey, nsAString &aData)
{
  nsresult rv;

  // IMPORTANT:
  // CacheStoragePermissions() is called inside of
  // GetItem(nsAString, nsIDOMStorageItem)
  // To call it particularly in this method would just duplicate
  // the call. If the code changes, make sure that call to
  // CacheStoragePermissions() is put here!

  nsCOMPtr<nsIDOMStorageItem> item;
  rv = GetItem(aKey, getter_AddRefs(item));
  if (NS_FAILED(rv))
    return rv;

  if (item) {
    rv = item->GetValue(aData);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
    SetDOMStringToNull(aData);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorage::GetItem(const nsAString& aKey, nsIDOMStorageItem **aItem)
{
  nsresult rv;

  NS_IF_ADDREF(*aItem = GetNamedItem(aKey, &rv));

  return rv;
}

NS_IMETHODIMP
nsDOMStorage::SetItem(const nsAString& aKey, const nsAString& aData)
{
  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;

  if (aKey.IsEmpty())
    return NS_OK;

  nsresult rv;
  nsString oldValue;
  SetDOMStringToNull(oldValue);

  // First store the value to the database, we need to do this before we update
  // the mItems cache.  SetDBValue is using the old cached value to decide
  // on quota checking.
  bool isCallerSecure = IsCallerSecure();
  if (UseDB()) {
    rv = SetDBValue(aKey, aData, isCallerSecure);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);
  if (entry) {
    if (entry->mItem->IsSecure() && !isCallerSecure) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
    oldValue = entry->mItem->GetValueInternal();
    entry->mItem->SetValueInternal(aData);
  }
  else {
    nsRefPtr<nsDOMStorageItem> newitem =
      new nsDOMStorageItem(this, aKey, aData, isCallerSecure);
    if (!newitem)
      return NS_ERROR_OUT_OF_MEMORY;
    entry = mItems.PutEntry(aKey);
    NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);
    entry->mItem = newitem;
  }

  if ((oldValue != aData || mStorageType == GlobalStorage) && mEventBroadcaster)
    mEventBroadcaster->BroadcastChangeNotification(aKey, oldValue, aData);

  return NS_OK;
}

NS_IMETHODIMP nsDOMStorage::RemoveItem(const nsAString& aKey)
{
  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;

  if (aKey.IsEmpty())
    return NS_OK;

  nsString oldValue;
  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);

  if (entry && entry->mItem->IsSecure() && !IsCallerSecure()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (UseDB()) {
#ifdef MOZ_STORAGE
    nsresult rv = InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString value;
    PRBool secureItem;
    rv = GetDBValue(aKey, value, &secureItem);
    if (rv == NS_ERROR_DOM_NOT_FOUND_ERR)
      return NS_OK;
    NS_ENSURE_SUCCESS(rv, rv);

    oldValue = value;

    rv = gStorageDB->RemoveKey(this, aKey, !IsOfflineAllowed(mDomain),
                               aKey.Length() + value.Length());
    NS_ENSURE_SUCCESS(rv, rv);

    // Before bug 536544 got fixed we were dropping mItemsCached flag here
#endif
  }
  else if (entry) {
    // clear string as StorageItems may be referencing this item
    oldValue = entry->mItem->GetValueInternal();
    entry->mItem->ClearValue();
  }

  if (entry) {
    mItems.RawRemoveEntry(entry);
  }

  if ((!oldValue.IsEmpty() && mStorageType != GlobalStorage) && mEventBroadcaster) {
    nsAutoString nullString;
    SetDOMStringToNull(nullString);
    mEventBroadcaster->BroadcastChangeNotification(aKey, oldValue, nullString);
  }

  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
CheckSecure(nsSessionStorageEntry* aEntry, void* userArg)
{
  PRBool* secure = (PRBool*)userArg;
  if (aEntry->mItem->IsSecure()) {
    *secure = PR_TRUE;
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

nsresult
nsDOMStorage::Clear()
{
  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;

  if (UseDB())
    CacheKeysFromDB();

  PRInt32 oldCount = mItems.Count();

  PRBool foundSecureItem = PR_FALSE;
  mItems.EnumerateEntries(CheckSecure, &foundSecureItem);

  if (foundSecureItem && !IsCallerSecure()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

#ifdef MOZ_STORAGE
  if (UseDB()) {
    nsresult rv = InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = gStorageDB->ClearStorage(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

  mItems.Clear();

  if (oldCount && mEventBroadcaster) {
    nsAutoString nullString;
    SetDOMStringToNull(nullString);
    mEventBroadcaster->BroadcastChangeNotification(nullString, nullString, nullString);
  }

  return NS_OK;
}

nsresult
nsDOMStorage::InitDB()
{
#ifdef MOZ_STORAGE
  if (!gStorageDB) {
    gStorageDB = new nsDOMStorageDBWrapper();
    if (!gStorageDB)
      return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = gStorageDB->Init();
    if (NS_FAILED(rv)) {
      // Failed to initialize the DB, delete it and null out the
      // pointer so we don't end up attempting to use an
      // un-initialized DB later on.

      delete gStorageDB;
      gStorageDB = nsnull;

      return rv;
    }
  }
#endif

  return NS_OK;
}

nsresult
nsDOMStorage::CacheKeysFromDB()
{
#ifdef MOZ_STORAGE
  // cache all the keys in the hash. This is used by the Length and Key methods
  // use this cache for better performance. The disadvantage is that the
  // order may break if someone changes the keys in the database directly.
  if (!mItemsCached) {
    nsresult rv = InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    mItems.Clear();

    rv = gStorageDB->GetAllKeys(this, &mItems);
    NS_ENSURE_SUCCESS(rv, rv);

    mItemsCached = PR_TRUE;
  }
#endif

  return NS_OK;
}

nsresult
nsDOMStorage::GetCachedValue(const nsAString& aKey, nsAString& aValue,
                             PRBool* aSecure)
{
  aValue.Truncate();
  *aSecure = PR_FALSE;

  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);
  if (!entry)
    return NS_ERROR_NOT_AVAILABLE;

  aValue = entry->mItem->GetValueInternal();
  *aSecure = entry->mItem->IsSecure();

  return NS_OK;
}

nsresult
nsDOMStorage::GetDBValue(const nsAString& aKey, nsAString& aValue,
                         PRBool* aSecure)
{
  aValue.Truncate();

#ifdef MOZ_STORAGE
  if (!UseDB())
    return NS_OK;

  nsresult rv = InitDB();
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  rv = gStorageDB->GetKeyValue(this, aKey, value, aSecure);

  if (rv == NS_ERROR_DOM_NOT_FOUND_ERR && mStorageType != GlobalStorage) {
    SetDOMStringToNull(aValue);
  }

  if (NS_FAILED(rv))
    return rv;

  if (!IsCallerSecure() && *aSecure) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  aValue.Assign(value);
#endif

  return NS_OK;
}

nsresult
nsDOMStorage::SetDBValue(const nsAString& aKey,
                         const nsAString& aValue,
                         PRBool aSecure)
{
#ifdef MOZ_STORAGE
  if (!UseDB())
    return NS_OK;

  nsresult rv = InitDB();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 offlineAppPermission;
  PRInt32 quota;
  PRInt32 warnQuota;
  offlineAppPermission = GetQuota(mDomain, &quota, &warnQuota,
                                  CanUseChromePersist());

  PRInt32 usage;
  rv = gStorageDB->SetKey(this, aKey, aValue, aSecure, quota,
                          !IS_PERMISSION_ALLOWED(offlineAppPermission),
                          &usage);
  NS_ENSURE_SUCCESS(rv, rv);

  // Before bug 536544 got fixed we were dropping mItemsCached flag here

  if (warnQuota >= 0 && usage > warnQuota) {
    // try to include the window that exceeded the warn quota
    nsCOMPtr<nsIDOMWindow> window;
    JSContext *cx;
    nsCOMPtr<nsIJSContextStack> stack =
      do_GetService("@mozilla.org/js/xpc/ContextStack;1");
    if (stack && NS_SUCCEEDED(stack->Peek(&cx)) && cx) {
      nsCOMPtr<nsIScriptContext> scriptContext;
      scriptContext = GetScriptContextFromJSContext(cx);
      if (scriptContext) {
        window = do_QueryInterface(scriptContext->GetGlobalObject());
      }
    }

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    os->NotifyObservers(window, "dom-storage-warn-quota-exceeded",
                        NS_ConvertUTF8toUTF16(mDomain).get());
  }

#endif

  return NS_OK;
}

nsresult
nsDOMStorage::SetSecure(const nsAString& aKey, PRBool aSecure)
{
#ifdef MOZ_STORAGE
  if (UseDB()) {
    nsresult rv = InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    return gStorageDB->SetSecure(this, aKey, aSecure);
  }
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif

  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);
  NS_ASSERTION(entry, "Don't use SetSecure() with nonexistent keys!");

  if (entry) {
    entry->mItem->SetSecureInternal(aSecure);
  }  

  return NS_OK;
}

static PLDHashOperator
ClearStorageItem(nsSessionStorageEntry* aEntry, void* userArg)
{
  aEntry->mItem->SetValueInternal(EmptyString());
  return PL_DHASH_NEXT;
}

void
nsDOMStorage::ClearAll()
{
  mItems.EnumerateEntries(ClearStorageItem, nsnull);
  mItemsCached = PR_FALSE;
}

already_AddRefed<nsIDOMStorage>
nsDOMStorage::Clone()
{
  NS_ASSERTION(PR_FALSE, "Old DOMStorage doesn't implement cloning");
  return nsnull;
}

already_AddRefed<nsIDOMStorage>
nsDOMStorage::Fork(const nsSubstring &aDocumentURI)
{
  NS_ASSERTION(PR_FALSE, "Old DOMStorage doesn't implement forking");
  return nsnull;
}

PRBool nsDOMStorage::IsForkOf(nsIDOMStorage* aThat)
{
  NS_ASSERTION(PR_FALSE, "Old DOMStorage doesn't implement forking");
  return PR_FALSE;
}

struct KeysArrayBuilderStruct
{
  PRBool callerIsSecure;
  nsTArray<nsString> *keys;
};

static PLDHashOperator
KeysArrayBuilder(nsSessionStorageEntry* aEntry, void* userArg)
{
  KeysArrayBuilderStruct *keystruct = (KeysArrayBuilderStruct *)userArg;
  
  if (keystruct->callerIsSecure || !aEntry->mItem->IsSecure())
    keystruct->keys->AppendElement(aEntry->GetKey());

  return PL_DHASH_NEXT;
}

nsTArray<nsString> *
nsDOMStorage::GetKeys()
{
  if (UseDB())
    CacheKeysFromDB();

  KeysArrayBuilderStruct keystruct;
  keystruct.callerIsSecure = IsCallerSecure();
  keystruct.keys = new nsTArray<nsString>();
  if (keystruct.keys)
    mItems.EnumerateEntries(KeysArrayBuilder, &keystruct);
 
  return keystruct.keys;
}

nsIPrincipal*
nsDOMStorage::Principal()
{
  return nsnull;
}

PRBool
nsDOMStorage::CanAccessSystem(nsIPrincipal *aPrincipal)
{
  if (!aPrincipal)
    return PR_TRUE;

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (!ssm)
    return PR_FALSE;

  PRBool isSystem;
  nsresult rv = ssm->IsSystemPrincipal(aPrincipal, &isSystem);

  return NS_SUCCEEDED(rv) && isSystem;
}

PRBool
nsDOMStorage::CanAccess(nsIPrincipal *aPrincipal)
{
  // Allow C++/system callers to access the storage
  if (CanAccessSystem(aPrincipal))
    return PR_TRUE;

  nsCAutoString domain;
  nsCOMPtr<nsIURI> unused;
  nsresult rv = GetPrincipalURIAndHost(aPrincipal,
                                       getter_AddRefs(unused), domain);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  return domain.Equals(mDomain);
}

nsPIDOMStorage::nsDOMStorageType
nsDOMStorage::StorageType()
{
  return mStorageType;
}

void
nsDOMStorage::BroadcastChangeNotification(const nsSubstring &aKey,
                                          const nsSubstring &aOldValue,
                                          const nsSubstring &aNewValue)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService) {
    return;
  }

  // Fire off a notification that a storage object changed. If the
  // storage object is a session storage object, we don't pass a
  // domain, but if it's a global storage object we do.
  observerService->NotifyObservers((nsIDOMStorageObsolete *)this,
                                   "dom-storage-changed",
                                   NS_ConvertUTF8toUTF16(mDomain).get());
}

nsresult
nsDOMStorage::MaybeCommitTemporaryTable(bool force)
{
#ifdef MOZ_STORAGE
  if (!UseDB())
    return NS_OK;

  if (!mLoadedTemporaryTable)
    return NS_OK;

  // If we are not forced to flush (e.g. on shutdown) then don't flush if the
  // last table access is less then 5 seconds ago or the table itself is not
  // older then 30 secs
  if (!force &&
     ((TimeStamp::Now() - mLastTemporaryTableAccessTime).ToSeconds() < 
       NS_DOMSTORAGE_MAXIMUM_TEMPTABLE_INACTIVITY_TIME) &&
     ((TimeStamp::Now() - mTemporaryTableAge).ToSeconds() < 
       NS_DOMSTORAGE_MAXIMUM_TEMPTABLE_AGE))
    return NS_OK;

  return gStorageDB->FlushAndDeleteTemporaryTableForStorage(this);
#endif

  return NS_OK;
}

nsresult
nsDOMStorage::RegisterObservers()
{
  nsCOMPtr<nsIObserverService> obsserv = mozilla::services::GetObserverService();
  if (obsserv) {
    obsserv->AddObserver(this, "profile-before-change", PR_TRUE);
    obsserv->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
    obsserv->AddObserver(this, NS_DOMSTORAGE_FLUSH_TIMER_OBSERVER, PR_TRUE);
  }
  return NS_OK;
}

bool
nsDOMStorage::WasTemporaryTableLoaded()
{
  return mLoadedTemporaryTable;
}

void
nsDOMStorage::SetTemporaryTableLoaded(bool loaded)
{
  if (loaded) {
    mLastTemporaryTableAccessTime = TimeStamp::Now();
    if (!mLoadedTemporaryTable)
      mTemporaryTableAge = mLastTemporaryTableAccessTime;
  }

  mLoadedTemporaryTable = loaded;
}

NS_IMETHODIMP
nsDOMStorage::Observe(nsISupports *subject,
                      const char *topic,
                      const PRUnichar *data)
{
  bool isProfileBeforeChange = !strcmp(topic, "profile-before-change");
  bool isXPCOMShutdown = !strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  bool isFlushTimer = !strcmp(topic, NS_DOMSTORAGE_FLUSH_TIMER_OBSERVER);

  if (isXPCOMShutdown || isProfileBeforeChange || isFlushTimer) {
    nsresult rv = MaybeCommitTemporaryTable(isXPCOMShutdown || isProfileBeforeChange);
    if (NS_FAILED(rv)) {
      NS_WARNING("DOMStorage: temporary table commit failed");
    }

    return NS_OK;
  }

  NS_WARNING("Unrecognized topic in nsDOMStorage::Observe");
  return NS_OK;
}

//
// nsDOMStorage2
//

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMStorage2)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMStorage2)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mStorage)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMStorage2)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mStorage, nsIDOMStorageObsolete)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(Storage, nsDOMStorage2)

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsDOMStorage2, nsIDOMStorage)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsDOMStorage2, nsIDOMStorage)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMStorage2)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMStorage)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Storage)
NS_INTERFACE_MAP_END

nsDOMStorage2::nsDOMStorage2()
{
}

nsDOMStorage2::nsDOMStorage2(nsDOMStorage2& aThat)
{
  mStorage = new nsDOMStorage(*aThat.mStorage.get());
  mStorage->mSecurityChecker = mStorage;
  mPrincipal = aThat.mPrincipal;
}

nsresult
nsDOMStorage2::InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI)
{
  mStorage = new nsDOMStorage();
  if (!mStorage)
    return NS_ERROR_OUT_OF_MEMORY;

  // Leave security checks only for domain (nsDOMStorage implementation)
  mStorage->mSecurityChecker = mStorage;
  mPrincipal = aPrincipal;
  mDocumentURI = aDocumentURI;

  return mStorage->InitAsSessionStorage(aPrincipal, aDocumentURI);
}

nsresult
nsDOMStorage2::InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI)
{
  mStorage = new nsDOMStorage();
  if (!mStorage)
    return NS_ERROR_OUT_OF_MEMORY;

  mStorage->mSecurityChecker = this;
  mPrincipal = aPrincipal;
  mDocumentURI = aDocumentURI;

  return mStorage->InitAsLocalStorage(aPrincipal, aDocumentURI);
}

nsresult
nsDOMStorage2::InitAsGlobalStorage(const nsACString &aDomainDemanded)
{
  NS_ASSERTION(PR_FALSE, "Should not initialize nsDOMStorage2 as global storage.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

already_AddRefed<nsIDOMStorage>
nsDOMStorage2::Clone()
{
  nsDOMStorage2* storage = new nsDOMStorage2(*this);
  if (!storage)
    return nsnull;

  storage->mStorage->CloneFrom(mStorage);
  NS_ADDREF(storage);

  return storage;
}

already_AddRefed<nsIDOMStorage>
nsDOMStorage2::Fork(const nsSubstring &aDocumentURI)
{
  nsRefPtr<nsDOMStorage2> storage = new nsDOMStorage2();
  if (!storage)
    return nsnull;

  nsresult rv = storage->InitAsSessionStorageFork(mPrincipal, aDocumentURI, mStorage);
  if (NS_FAILED(rv))
    return nsnull;

  nsIDOMStorage* result = static_cast<nsIDOMStorage*>(storage.get());
  storage.forget();
  return result;
}

PRBool nsDOMStorage2::IsForkOf(nsIDOMStorage* aThat)
{
  if (!aThat)
    return PR_FALSE;

  nsDOMStorage2* storage = static_cast<nsDOMStorage2*>(aThat);
  return mStorage == storage->mStorage;
}

nsresult
nsDOMStorage2::InitAsSessionStorageFork(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI, nsIDOMStorageObsolete* aStorage)
{
  mPrincipal = aPrincipal;
  mDocumentURI = aDocumentURI;
  mStorage = static_cast<nsDOMStorage*>(aStorage);

  return NS_OK;
}

nsTArray<nsString> *
nsDOMStorage2::GetKeys()
{
  return mStorage->GetKeys();
}

nsIPrincipal*
nsDOMStorage2::Principal()
{
  return mPrincipal;
}

PRBool
nsDOMStorage2::CanAccess(nsIPrincipal *aPrincipal)
{
  if (mStorage->mSecurityChecker != this)
    return mStorage->mSecurityChecker->CanAccess(aPrincipal);

  // Allow C++ callers to access the storage
  if (!aPrincipal)
    return PR_TRUE;

  // Allow more powerful principals (e.g. system) to access the storage
  PRBool subsumes;
  nsresult rv = aPrincipal->Subsumes(mPrincipal, &subsumes);
  if (NS_FAILED(rv))
    return PR_FALSE;

  return subsumes;
}

nsPIDOMStorage::nsDOMStorageType
nsDOMStorage2::StorageType()
{
  if (mStorage)
    return mStorage->StorageType();

  return nsPIDOMStorage::Unknown;
}

void
nsDOMStorage2::BroadcastChangeNotification(const nsSubstring &aKey,
                                          const nsSubstring &aOldValue,
                                          const nsSubstring &aNewValue)
{
  nsresult rv;
  nsCOMPtr<nsIDOMStorageEvent> event = new nsDOMStorageEvent();
  rv = event->InitStorageEvent(NS_LITERAL_STRING("storage"),
                               PR_FALSE,
                               PR_FALSE,
                               aKey,
                               aOldValue,
                               aNewValue,
                               mDocumentURI,
                               static_cast<nsIDOMStorage*>(this));
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService) {
    return;
  }

  nsCOMPtr<nsIObserverService> observerServiceProxy;
  rv = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                            NS_GET_IID(nsIObserverService),
                            observerService,
                            NS_PROXY_ASYNC | NS_PROXY_ALWAYS,
                            getter_AddRefs(observerServiceProxy));
  if (NS_FAILED(rv)) {
    return;
  }

  // Fire off a notification that a storage object changed.
  observerServiceProxy->NotifyObservers(event,
                                        "dom-storage2-changed",
                                        nsnull);
}

NS_IMETHODIMP
nsDOMStorage2::GetLength(PRUint32 *aLength)
{
  return mStorage->GetLength(aLength);
}

NS_IMETHODIMP
nsDOMStorage2::Key(PRUint32 aIndex, nsAString& aKey)
{
  return mStorage->Key(aIndex, aKey);
}

NS_IMETHODIMP
nsDOMStorage2::GetItem(const nsAString& aKey, nsAString &aData)
{
  return mStorage->GetItem(aKey, aData);
}

NS_IMETHODIMP
nsDOMStorage2::SetItem(const nsAString& aKey, const nsAString& aData)
{
  mStorage->mEventBroadcaster = this;
  return mStorage->SetItem(aKey, aData);
}

NS_IMETHODIMP
nsDOMStorage2::RemoveItem(const nsAString& aKey)
{
  mStorage->mEventBroadcaster = this;
  return mStorage->RemoveItem(aKey);
}

NS_IMETHODIMP
nsDOMStorage2::Clear()
{
  mStorage->mEventBroadcaster = this;
  return mStorage->Clear();
}

//
// nsDOMStorageList
//

DOMCI_DATA(StorageList, nsDOMStorageList)

NS_INTERFACE_MAP_BEGIN(nsDOMStorageList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(StorageList)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMStorageList)
NS_IMPL_RELEASE(nsDOMStorageList)

nsIDOMStorageObsolete*
nsDOMStorageList::GetNamedItem(const nsAString& aDomain, nsresult* aResult)
{
  nsCAutoString requestedDomain;

  // Normalize the requested domain
  nsCOMPtr<nsIIDNService> idn = do_GetService(NS_IDNSERVICE_CONTRACTID);
  if (idn) {
    *aResult = idn->ConvertUTF8toACE(NS_ConvertUTF16toUTF8(aDomain),
                                     requestedDomain);
    NS_ENSURE_SUCCESS(*aResult, nsnull);
  } else {
    // Don't have the IDN service, best we can do is URL escape.
    NS_EscapeURL(NS_ConvertUTF16toUTF8(aDomain),
                 esc_OnlyNonASCII | esc_AlwaysCopy,
                 requestedDomain);
  }
  ToLowerCase(requestedDomain);

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (!ssm) {
    *aResult = NS_ERROR_FAILURE;
    return nsnull;
  }

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  *aResult = ssm->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
  NS_ENSURE_SUCCESS(*aResult, nsnull);

  nsCAutoString currentDomain;
  if (subjectPrincipal) {
    nsCOMPtr<nsIURI> unused;
    *aResult = GetPrincipalURIAndHost(subjectPrincipal, getter_AddRefs(unused),
                                      currentDomain);
    NS_ENSURE_SUCCESS(*aResult, nsnull);

    PRPackedBool sessionOnly;
    if (!nsDOMStorage::CanUseStorage(&sessionOnly)) {
      *aResult = NS_ERROR_DOM_SECURITY_ERR;
      return nsnull;
    }
  }

  PRBool isSystem = nsContentUtils::IsCallerTrustedForRead();
  if (currentDomain.IsEmpty() && !isSystem) {
    *aResult = NS_ERROR_DOM_SECURITY_ERR;
    return nsnull;
  }

  return GetStorageForDomain(requestedDomain,
                             currentDomain, isSystem, aResult);
}

NS_IMETHODIMP
nsDOMStorageList::NamedItem(const nsAString& aDomain,
                            nsIDOMStorageObsolete** aStorage)
{
  nsresult rv;
  NS_IF_ADDREF(*aStorage = GetNamedItem(aDomain, &rv));
  return rv;
}

// static
PRBool
nsDOMStorageList::CanAccessDomain(const nsACString& aRequestedDomain,
                                  const nsACString& aCurrentDomain)
{
  return aRequestedDomain.Equals(aCurrentDomain);
}

nsIDOMStorageObsolete*
nsDOMStorageList::GetStorageForDomain(const nsACString& aRequestedDomain,
                                      const nsACString& aCurrentDomain,
                                      PRBool aNoCurrentDomainCheck,
                                      nsresult* aResult)
{
  nsTArray<nsCString> requestedDomainArray;
  if ((!aNoCurrentDomainCheck &&
       !CanAccessDomain(aRequestedDomain, aCurrentDomain)) ||
    !ConvertDomainToArray(aRequestedDomain, &requestedDomainArray)) {
    *aResult = NS_ERROR_DOM_SECURITY_ERR;

    return nsnull;
  }

  // now rebuild a string for the domain.
  nsCAutoString usedDomain;
  PRUint32 requestedPos = 0;
  for (requestedPos = 0; requestedPos < requestedDomainArray.Length();
       requestedPos++) {
    if (!usedDomain.IsEmpty())
      usedDomain.Append('.');
    usedDomain.Append(requestedDomainArray[requestedPos]);
  }

  *aResult = NS_OK;

  // now have a valid domain, so look it up in the storage table
  nsIDOMStorageObsolete* storage = mStorages.GetWeak(usedDomain);
  if (!storage) {
    nsRefPtr<nsDOMStorage> newstorage;
    newstorage = new nsDOMStorage();
    if (newstorage && mStorages.Put(usedDomain, newstorage)) {
      *aResult = newstorage->InitAsGlobalStorage(usedDomain);
      if (NS_FAILED(*aResult)) {
        mStorages.Remove(usedDomain);
        return nsnull;
      }
      storage = newstorage;
    }
    else {
      *aResult = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return storage;
}

// static
PRBool
nsDOMStorageList::ConvertDomainToArray(const nsACString& aDomain,
                                       nsTArray<nsCString> *aArray)
{
  PRInt32 length = aDomain.Length();
  PRInt32 n = 0;
  while (n < length) {
    PRInt32 dotpos = aDomain.FindChar('.', n);
    nsCAutoString domain;

    if (dotpos == -1) // no more dots
      domain.Assign(Substring(aDomain, n));
    else if (dotpos - n == 0) // no point continuing in this case
      return false;
    else if (dotpos >= 0)
      domain.Assign(Substring(aDomain, n, dotpos - n));

    ToLowerCase(domain);
    aArray->AppendElement(domain);

    if (dotpos == -1)
      break;

    n = dotpos + 1;
  }

  // if n equals the length, there is a dot at the end, so treat it as invalid
  return (n != length);
}

nsresult
NS_NewDOMStorageList(nsIDOMStorageList** aResult)
{
  *aResult = new nsDOMStorageList();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

//
// nsDOMStorageItem
//

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMStorageItem)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMStorageItem)
  {
    tmp->mStorage = nsnull;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMStorageItem)
  {
    cb.NoteXPCOMChild((nsIDOMStorageObsolete*) tmp->mStorage);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsDOMStorageItem, nsIDOMStorageItem)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsDOMStorageItem, nsIDOMStorageItem)

DOMCI_DATA(StorageItem, nsDOMStorageItem)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMStorageItem)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorageItem)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageItem)
  NS_INTERFACE_MAP_ENTRY(nsIDOMToString)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(StorageItem)
NS_INTERFACE_MAP_END

nsDOMStorageItem::nsDOMStorageItem(nsDOMStorage* aStorage,
                                   const nsAString& aKey,
                                   const nsAString& aValue,
                                   PRBool aSecure)
  : mSecure(aSecure),
    mKey(aKey),
    mValue(aValue),
    mStorage(aStorage)
{
}

nsDOMStorageItem::~nsDOMStorageItem()
{
}

NS_IMETHODIMP
nsDOMStorageItem::GetSecure(PRBool* aSecure)
{
  if (!mStorage->CacheStoragePermissions() || !IsCallerSecure()) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  if (mStorage->UseDB()) {
    nsAutoString value;
    return mStorage->GetDBValue(mKey, value, aSecure);
  }

  *aSecure = IsSecure();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorageItem::SetSecure(PRBool aSecure)
{
  if (!mStorage->CacheStoragePermissions() || !IsCallerSecure()) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  if (mStorage->UseDB()) {
    nsresult rv = mStorage->SetSecure(mKey, aSecure);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mSecure = aSecure;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorageItem::GetValue(nsAString& aValue)
{
  if (!mStorage->CacheStoragePermissions())
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;

  if (mStorage->UseDB()) {
    // GetDBValue checks the secure state so no need to do it here
    PRBool secure;
    nsresult rv = mStorage->GetDBValue(mKey, aValue, &secure);
    if (rv == NS_ERROR_DOM_NOT_FOUND_ERR)
      return NS_OK;
    return rv;
  }

  if (IsSecure() && !IsCallerSecure()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  aValue = mValue;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorageItem::SetValue(const nsAString& aValue)
{
  if (!mStorage->CacheStoragePermissions())
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;

  PRBool secureCaller = IsCallerSecure();

  if (mStorage->UseDB()) {
    // SetDBValue() does the security checks for us.
    return mStorage->SetDBValue(mKey, aValue, secureCaller);
  }

  PRBool secureItem = IsSecure();

  if (!secureCaller && secureItem) {
    // The item is secure, but the caller isn't. Throw.
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  mValue = aValue;
  mSecure = secureCaller;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorageItem::ToString(nsAString& aStr)
{
  return GetValue(aStr);
}

// Cycle collection implementation for nsDOMStorageEvent
NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMStorageEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMStorageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mStorageArea)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMStorageEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mStorageArea)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsDOMStorageEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMStorageEvent, nsDOMEvent)

DOMCI_DATA(StorageEvent, nsDOMStorageEvent)

// QueryInterface implementation for nsDOMStorageEvent
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMStorageEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(StorageEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)


/* readonly attribute DOMString key; */
NS_IMETHODIMP nsDOMStorageEvent::GetKey(nsAString & aKey)
{
  aKey = mKey;
  return NS_OK;
}

/* readonly attribute DOMString oldValue; */
NS_IMETHODIMP nsDOMStorageEvent::GetOldValue(nsAString & aOldValue)
{
  aOldValue = mOldValue;
  return NS_OK;
}

/* readonly attribute DOMString newValue; */
NS_IMETHODIMP nsDOMStorageEvent::GetNewValue(nsAString & aNewValue)
{
  aNewValue = mNewValue;
  return NS_OK;
}

/* readonly attribute DOMString url; */
NS_IMETHODIMP nsDOMStorageEvent::GetUrl(nsAString & aUrl)
{
  aUrl = mUrl;
  return NS_OK;
}

/* readonly attribute nsIDOMStorage storageArea; */
NS_IMETHODIMP nsDOMStorageEvent::GetStorageArea(nsIDOMStorage * *aStorageArea)
{
  NS_ENSURE_ARG_POINTER(aStorageArea);

  NS_ADDREF(*aStorageArea = mStorageArea);
  return NS_OK;
}

/* void initStorageEvent (in DOMString typeArg, in boolean canBubbleArg, in boolean cancelableArg, in DOMString keyArg, in DOMString oldValueArg, in DOMString newValueArg, in DOMString urlArg, in nsIDOMStorage storageAreaArg); */
NS_IMETHODIMP nsDOMStorageEvent::InitStorageEvent(const nsAString & typeArg,
                                                  PRBool canBubbleArg,
                                                  PRBool cancelableArg,
                                                  const nsAString & keyArg,
                                                  const nsAString & oldValueArg,
                                                  const nsAString & newValueArg,
                                                  const nsAString & urlArg,
                                                  nsIDOMStorage *storageAreaArg)
{
  nsresult rv;

  rv = InitEvent(typeArg, canBubbleArg, cancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mKey = keyArg;
  mOldValue = oldValueArg;
  mNewValue = newValueArg;
  mUrl = urlArg;
  mStorageArea = storageAreaArg;

  return NS_OK;
}

// Obsolete globalStorage event

DOMCI_DATA(StorageEventObsolete, nsDOMStorageEventObsolete)

// QueryInterface implementation for nsDOMStorageEventObsolete
NS_INTERFACE_MAP_BEGIN(nsDOMStorageEventObsolete)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageEventObsolete)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(StorageEventObsolete)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMStorageEventObsolete, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMStorageEventObsolete, nsDOMEvent)


NS_IMETHODIMP
nsDOMStorageEventObsolete::GetDomain(nsAString& aDomain)
{
  // mDomain will be #session for session storage for events that fire
  // due to a change in a session storage object.
  aDomain = mDomain;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorageEventObsolete::InitStorageEvent(const nsAString& aTypeArg,
                                    PRBool aCanBubbleArg,
                                    PRBool aCancelableArg,
                                    const nsAString& aDomainArg)
{
  nsresult rv = InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mDomain = aDomainArg;

  return NS_OK;
}
