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

static const PRUint32 ASK_BEFORE_ACCEPT = 1;
static const PRUint32 ACCEPT_SESSION = 2;
static const PRUint32 BEHAVIOR_REJECT = 2;

static const PRUint32 DEFAULT_QUOTA = 5 * 1024;
// Be generous with offline apps by default...
static const PRUint32 DEFAULT_OFFLINE_APP_QUOTA = 200 * 1024;
// ... but warn if it goes over this amount
static const PRUint32 DEFAULT_OFFLINE_WARN_QUOTA = 50 * 1024;

static const char kPermissionType[] = "cookie";
static const char kStorageEnabled[] = "dom.storage.enabled";
static const char kDefaultQuota[] = "dom.storage.default_quota";
static const char kCookiesBehavior[] = "network.cookie.cookieBehavior";
static const char kCookiesLifetimePolicy[] = "network.cookie.lifetimePolicy";
static const char kOfflineAppWarnQuota[] = "offline-apps.quota.warn";
static const char kOfflineAppQuota[] = "offline-apps.quota.max";

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


// Returns two quotas - A hard limit for which adding data will be an error,
// and a limit after which a warning event will be sent to the observer
// service.  The warn limit may be -1, in which case there will be no warning.
static void
GetQuota(const nsAString &aDomain, PRInt32 *aQuota, PRInt32 *aWarnQuota)
{
  // Fake a URI for the permission manager
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), NS_LITERAL_STRING("http://") + aDomain);

  if (uri) {
    nsCOMPtr<nsIPermissionManager> permissionManager =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

    PRUint32 perm;
    if (permissionManager &&
        NS_SUCCEEDED(permissionManager->TestExactPermission(uri, "offline-app", &perm)) &&
        perm != nsIPermissionManager::UNKNOWN_ACTION &&
        perm != nsIPermissionManager::DENY_ACTION) {
      // This is an offline app, give more space by default.
      *aQuota = ((PRInt32)nsContentUtils::GetIntPref(kOfflineAppQuota,
                                                     DEFAULT_OFFLINE_APP_QUOTA) * 1024);

      if (perm == nsIOfflineCacheUpdateService::ALLOW_NO_WARN) {
        *aWarnQuota = -1;
      } else {
        *aWarnQuota = ((PRInt32)nsContentUtils::GetIntPref(kOfflineAppWarnQuota,
                                                           DEFAULT_OFFLINE_WARN_QUOTA) * 1024);
      }
      return;
    }
  }

  // FIXME: per-domain quotas?
  *aQuota = ((PRInt32)nsContentUtils::GetIntPref(kDefaultQuota,
                                                 DEFAULT_QUOTA) * 1024);
  *aWarnQuota = -1;
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

  nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");
  if (os) {
    os->AddObserver(gStorageManager, "cookie-changed", PR_FALSE);
    os->AddObserver(gStorageManager, "offline-app-removed", PR_FALSE);
  }

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
GetOfflineDomains(nsStringArray& aDomains)
{
  nsCOMPtr<nsIPermissionManager> permissionManager =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  if (permissionManager) {
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    nsresult rv = permissionManager->GetEnumerator(getter_AddRefs(enumerator));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsIPermission> perm;
      rv = enumerator->GetNext(getter_AddRefs(perm));
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

          aDomains.AppendString(NS_ConvertUTF8toUTF16(host));
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
  if (!strcmp(aTopic, "offline-app-removed")) {
#ifdef MOZ_STORAGE
    nsresult rv = nsDOMStorage::InitDB();
    NS_ENSURE_SUCCESS(rv, rv);
    return nsDOMStorage::gStorageDB->RemoveOwner(nsDependentString(aData));
#endif
  } else if (!strcmp(aTopic, "cookie-changed") &&
             !nsCRT::strcmp(aData, NS_LITERAL_STRING("cleared").get())) {
    mStorages.EnumerateEntries(ClearStorage, nsnull);

#ifdef MOZ_STORAGE
    nsresult rv = nsDOMStorage::InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    // Remove global storage for domains that aren't marked for offline use.
    nsStringArray domains;
    rv = GetOfflineDomains(domains);
    NS_ENSURE_SUCCESS(rv, rv);
    return nsDOMStorage::gStorageDB->RemoveOwners(domains, PR_FALSE);
#endif
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorageManager::GetUsage(const nsAString& aDomain,
                              PRInt32 *aUsage)
{
  nsresult rv = nsDOMStorage::InitDB();
  NS_ENSURE_SUCCESS(rv, rv);

  return nsDOMStorage::gStorageDB->GetUsage(aDomain, aUsage);
}

NS_IMETHODIMP
nsDOMStorageManager::ClearOfflineApps()
{
    nsresult rv = nsDOMStorage::InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    nsStringArray domains;
    rv = GetOfflineDomains(domains);
    NS_ENSURE_SUCCESS(rv, rv);
    return nsDOMStorage::gStorageDB->RemoveOwners(domains, PR_TRUE);
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
nsDOMStorageDB* nsDOMStorage::gStorageDB = nsnull;
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
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mURI)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mURI)
  {
    if (tmp->mItems.IsInitialized()) {
      tmp->mItems.EnumerateEntries(SessionStorageTraverser, &cb);
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsDOMStorage, nsIDOMStorage)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsDOMStorage, nsIDOMStorage)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMStorage)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMStorage)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Storage)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
NS_NewDOMStorage(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  nsDOMStorage* storage = new nsDOMStorage();
  if (!storage)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(storage);
  *aResult = storage;

  return NS_OK;
}

nsDOMStorage::nsDOMStorage()
  : mUseDB(PR_FALSE), mSessionOnly(PR_TRUE), mItemsCached(PR_FALSE)
{
  mItems.Init(8);
  if (nsDOMStorageManager::gStorageManager)
    nsDOMStorageManager::gStorageManager->AddToStoragesHash(this);
}

nsDOMStorage::nsDOMStorage(nsIURI* aURI, const nsAString& aDomain, PRBool aUseDB)
  : mUseDB(aUseDB),
    mSessionOnly(PR_TRUE),
    mItemsCached(PR_FALSE),
    mURI(aURI),
    mDomain(aDomain)
{
#ifndef MOZ_STORAGE
  mUseDB = PR_FALSE;
#endif

  mItems.Init(8);
  if (nsDOMStorageManager::gStorageManager)
    nsDOMStorageManager::gStorageManager->AddToStoragesHash(this);
}

nsDOMStorage::~nsDOMStorage()
{
  if (nsDOMStorageManager::gStorageManager)
    nsDOMStorageManager::gStorageManager->RemoveFromStoragesHash(this);
}

void
nsDOMStorage::Init(nsIURI* aURI, const nsAString& aDomain, PRBool aUseDB)
{
  mURI = aURI;
  mDomain.Assign(aDomain);
#ifdef MOZ_STORAGE
  mUseDB = aUseDB;
#else
  mUseDB = PR_FALSE;
#endif
}

//static
PRBool
nsDOMStorage::CanUseStorage(nsIURI* aURI, PRPackedBool* aSessionOnly)
{
  // check if the domain can use storage. Downgrade to session only if only
  // session storage may be used.
  NS_ASSERTION(aSessionOnly, "null session flag");
  *aSessionOnly = PR_FALSE;

  if (!nsContentUtils::GetBoolPref(kStorageEnabled))
    return PR_FALSE;

  // chrome can always use storage regardless of permission preferences
  if (nsContentUtils::IsCallerChrome())
    return PR_TRUE;

  nsCOMPtr<nsIPermissionManager> permissionManager =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  if (!permissionManager)
    return PR_FALSE;

  PRUint32 perm;
  permissionManager->TestPermission(aURI, kPermissionType, &perm);

  if (perm == nsIPermissionManager::DENY_ACTION)
    return PR_FALSE;

  if (perm == nsICookiePermission::ACCESS_SESSION) {
    *aSessionOnly = PR_TRUE;
  }
  else if (perm != nsIPermissionManager::ALLOW_ACTION) {
    PRUint32 cookieBehavior = nsContentUtils::GetIntPref(kCookiesBehavior);
    PRUint32 lifetimePolicy = nsContentUtils::GetIntPref(kCookiesLifetimePolicy);

    // treat ask as reject always
    if (cookieBehavior == BEHAVIOR_REJECT || lifetimePolicy == ASK_BEFORE_ACCEPT)
      return PR_FALSE;

    if (lifetimePolicy == ACCEPT_SESSION)
      *aSessionOnly = PR_TRUE;
  }

  return PR_TRUE;
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
    nsAutoString unused;
    nsresult rv = GetDBValue(aKey, value, &secure, unused);
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
  nsRefPtr<nsDOMStorageItem> newitem = nsnull;
  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);
  if (entry) {
    if (entry->mItem->IsSecure() && !IsCallerSecure()) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
    if (!UseDB()) {
      entry->mItem->SetValueInternal(aData);
    }
  }
  else {
    if (UseDB())
      newitem = new nsDOMStorageItem(this, aKey, aData, PR_FALSE);
    else 
      newitem = new nsDOMStorageItem(this, aKey, aData, PR_FALSE);
    if (!newitem)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (UseDB()) {
    rv = SetDBValue(aKey, aData, IsCallerSecure());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (newitem) {
    entry = mItems.PutEntry(aKey);
    NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);
    entry->mItem = newitem;
  }

  // SetDBValue already calls BroadcastChangeNotification so don't do it again
  if (!UseDB())
    BroadcastChangeNotification();

  return NS_OK;
}

NS_IMETHODIMP nsDOMStorage::RemoveItem(const nsAString& aKey)
{
  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;

  if (aKey.IsEmpty())
    return NS_OK;

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
    nsAutoString owner;
    rv = GetDBValue(aKey, value, &secureItem, owner);
    if (rv == NS_ERROR_DOM_NOT_FOUND_ERR)
      return NS_OK;
    NS_ENSURE_SUCCESS(rv, rv);

    rv = gStorageDB->RemoveKey(mDomain, aKey, owner,
                               aKey.Length() + value.Length());
    NS_ENSURE_SUCCESS(rv, rv);

    mItemsCached = PR_FALSE;

    BroadcastChangeNotification();
#endif
  }
  else if (entry) {
    // clear string as StorageItems may be referencing this item
    entry->mItem->ClearValue();

    BroadcastChangeNotification();
  }

  if (entry) {
    mItems.RawRemoveEntry(entry);
  }

  return NS_OK;
}

nsresult
nsDOMStorage::InitDB()
{
#ifdef MOZ_STORAGE
  if (!gStorageDB) {
    gStorageDB = new nsDOMStorageDB();
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

    rv = gStorageDB->GetAllKeys(mDomain, this, &mItems);
    NS_ENSURE_SUCCESS(rv, rv);

    mItemsCached = PR_TRUE;
  }
#endif

  return NS_OK;
}

nsresult
nsDOMStorage::GetDBValue(const nsAString& aKey, nsAString& aValue,
                         PRBool* aSecure, nsAString& aOwner)
{
  aValue.Truncate();

#ifdef MOZ_STORAGE
  if (!UseDB())
    return NS_OK;

  nsresult rv = InitDB();
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  rv = gStorageDB->GetKeyValue(mDomain, aKey, value, aSecure, aOwner);
  if (NS_FAILED(rv))
    return rv;

  if (!IsCallerSecure() && *aSecure) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  aValue.Assign(value);
#endif

  return NS_OK;
}

// The URI returned is the innermost URI that should be used for
// security-check-like stuff.  aHost is its hostname, correctly canonicalized.
static nsresult
GetPrincipalURIAndHost(nsIPrincipal* aPrincipal, nsIURI** aURI, nsString& aHost)
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

  nsCAutoString asciiHost;
  rv = innerURI->GetAsciiHost(asciiHost);
  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  
  CopyUTF8toUTF16(asciiHost, aHost);
  innerURI.swap(*aURI);

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

  // Get the current domain for quota enforcement
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (!ssm)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  ssm->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));

  nsAutoString currentDomain;

  if (subjectPrincipal) {
    nsCOMPtr<nsIURI> unused;
    rv = GetPrincipalURIAndHost(subjectPrincipal, getter_AddRefs(unused),
                                currentDomain);
    // Don't bail out on NS_ERROR_DOM_SECURITY_ERR, since we want to allow
    // trusted file:// URIs below.
    if (NS_FAILED(rv) && rv != NS_ERROR_DOM_SECURITY_ERR) {
      return rv;
    }

    if (currentDomain.IsEmpty()) {
      // allow chrome urls and trusted file urls to write using
      // the storage's domain
      if (nsContentUtils::IsCallerTrustedForWrite())
        currentDomain = mDomain;
      else
        return NS_ERROR_DOM_SECURITY_ERR;
    }
  } else {
    currentDomain = mDomain;
  }

  PRInt32 quota;
  PRInt32 warnQuota;
  GetQuota(currentDomain, &quota, &warnQuota);

  PRInt32 usage;
  rv = gStorageDB->SetKey(mDomain, aKey, aValue, aSecure,
                          currentDomain, quota, &usage);
  NS_ENSURE_SUCCESS(rv, rv);

  mItemsCached = PR_FALSE;

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

    nsCOMPtr<nsIObserverService> os =
      do_GetService("@mozilla.org/observer-service;1");
    os->NotifyObservers(window, "dom-storage-warn-quota-exceeded",
                        currentDomain.get());
  }

  BroadcastChangeNotification();
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

    return gStorageDB->SetSecure(mDomain, aKey, aSecure);
  }
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif

  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);
  NS_ASSERTION(entry, "Don't use SetSecure() with non-existing keys!");

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

already_AddRefed<nsIDOMStorage>
nsDOMStorage::Clone(nsIURI* aURI)
{
  if (UseDB()) {
    NS_ERROR("Uh, don't clone a global storage object.");

    return nsnull;
  }

  nsDOMStorage* storage = new nsDOMStorage(aURI, mDomain, PR_FALSE);
  if (!storage)
    return nsnull;

  mItems.EnumerateEntries(CopyStorageItems, storage);

  NS_ADDREF(storage);

  return storage;
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

void
nsDOMStorage::BroadcastChangeNotification()
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_FAILED(rv)) {
    return;
  }

  // Fire off a notification that a storage object changed. If the
  // storage object is a session storage object, we don't pass a
  // domain, but if it's a global storage object we do.
  observerService->NotifyObservers((nsIDOMStorage *)this,
                                   "dom-storage-changed",
                                   UseDB() ? mDomain.get() : nsnull);
}

//
// nsDOMStorageList
//

NS_INTERFACE_MAP_BEGIN(nsDOMStorageList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(StorageList)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMStorageList)
NS_IMPL_RELEASE(nsDOMStorageList)

nsIDOMStorage*
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

  nsCOMPtr<nsIURI> uri;
  nsAutoString currentDomain;
  if (subjectPrincipal) {
    *aResult = GetPrincipalURIAndHost(subjectPrincipal, getter_AddRefs(uri),
                                      currentDomain);
    NS_ENSURE_SUCCESS(*aResult, nsnull);

    if (uri) {
      PRPackedBool sessionOnly;
      if (!nsDOMStorage::CanUseStorage(uri, &sessionOnly)) {
        *aResult = NS_ERROR_DOM_SECURITY_ERR;
        return nsnull;
      }
    }
  }

  PRBool isSystem = nsContentUtils::IsCallerTrustedForRead();

  if (isSystem || !currentDomain.IsEmpty()) {
    return GetStorageForDomain(uri, NS_ConvertUTF8toUTF16(requestedDomain),
                               currentDomain, isSystem, aResult);
  }

  *aResult = NS_ERROR_DOM_SECURITY_ERR;
  return nsnull;
}

NS_IMETHODIMP
nsDOMStorageList::NamedItem(const nsAString& aDomain,
                            nsIDOMStorage** aStorage)
{
  nsresult rv;
  NS_IF_ADDREF(*aStorage = GetNamedItem(aDomain, &rv));
  return rv;
}

// static
PRBool
nsDOMStorageList::CanAccessDomain(const nsAString& aRequestedDomain,
                                  const nsAString& aCurrentDomain)
{
  return aRequestedDomain.Equals(aCurrentDomain);
}

nsIDOMStorage*
nsDOMStorageList::GetStorageForDomain(nsIURI* aURI,
                                      const nsAString& aRequestedDomain,
                                      const nsAString& aCurrentDomain,
                                      PRBool aNoCurrentDomainCheck,
                                      nsresult* aResult)
{
  nsStringArray requestedDomainArray;
  if ((!aNoCurrentDomainCheck &&
       !CanAccessDomain(aRequestedDomain, aCurrentDomain)) ||
      !ConvertDomainToArray(aRequestedDomain, &requestedDomainArray)) {
    *aResult = NS_ERROR_DOM_SECURITY_ERR;

    return nsnull;
  }

  // now rebuild a string for the domain.
  nsAutoString usedDomain;
  PRInt32 requestedPos = 0;
  for (requestedPos = 0; requestedPos < requestedDomainArray.Count();
       requestedPos++) {
    if (!usedDomain.IsEmpty())
      usedDomain.AppendLiteral(".");
    usedDomain.Append(*requestedDomainArray[requestedPos]);
  }

  *aResult = NS_OK;

  // now have a valid domain, so look it up in the storage table
  nsIDOMStorage* storage = mStorages.GetWeak(usedDomain);
  if (!storage) {
    nsCOMPtr<nsIDOMStorage> newstorage = new nsDOMStorage(aURI, usedDomain, PR_TRUE);
    if (newstorage && mStorages.Put(usedDomain, newstorage))
      storage = newstorage;
    else
      *aResult = NS_ERROR_OUT_OF_MEMORY;
  }

  return storage;
}

// static
PRBool
nsDOMStorageList::ConvertDomainToArray(const nsAString& aDomain,
                                       nsStringArray* aArray)
{
  PRInt32 length = aDomain.Length();
  PRInt32 n = 0;
  while (n < length) {
    PRInt32 dotpos = aDomain.FindChar('.', n);
    nsAutoString domain;

    if (dotpos == -1) // no more dots
      domain.Assign(Substring(aDomain, n));
    else if (dotpos - n == 0) // no point continuing in this case
      return false;
    else if (dotpos >= 0)
      domain.Assign(Substring(aDomain, n, dotpos - n));

    ToLowerCase(domain);
    aArray->AppendString(domain);

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
    cb.NoteXPCOMChild((nsIDOMStorage*) tmp->mStorage);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsDOMStorageItem, nsIDOMStorageItem)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsDOMStorageItem, nsIDOMStorageItem)
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
    nsAutoString owner;
    return mStorage->GetDBValue(mKey, value, aSecure, owner);
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
    nsAutoString unused;
    nsresult rv = mStorage->GetDBValue(mKey, aValue, &secure, unused);
    return (rv == NS_ERROR_DOM_NOT_FOUND_ERR) ? NS_OK : rv;
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

// QueryInterface implementation for nsDOMStorageEvent
NS_INTERFACE_MAP_BEGIN(nsDOMStorageEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(StorageEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMStorageEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMStorageEvent, nsDOMEvent)


NS_IMETHODIMP
nsDOMStorageEvent::GetDomain(nsAString& aDomain)
{
  // mDomain will be #session for session storage for events that fire
  // due to a change in a session storage object.
  aDomain = mDomain;

  return NS_OK;
}

nsresult
nsDOMStorageEvent::Init()
{
  nsresult rv = InitEvent(NS_LITERAL_STRING("storage"), PR_TRUE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // This init method is only called by native code, so set the
  // trusted flag here.
  SetTrusted(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorageEvent::InitStorageEvent(const nsAString& aTypeArg,
                                    PRBool aCanBubbleArg,
                                    PRBool aCancelableArg,
                                    const nsAString& aDomainArg)
{
  nsresult rv = InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mDomain = aDomainArg;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorageEvent::InitStorageEventNS(const nsAString& aNamespaceURIArg,
                                      const nsAString& aTypeArg,
                                      PRBool aCanBubbleArg,
                                      PRBool aCancelableArg,
                                      const nsAString& aDomainArg)
{
  // XXXjst: Figure out what to do with aNamespaceURIArg here!
  nsresult rv = InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mDomain = aDomainArg;

  return NS_OK;
}
