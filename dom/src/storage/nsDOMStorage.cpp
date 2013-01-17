/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "StorageChild.h"
#include "StorageParent.h"
#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"
using mozilla::dom::StorageChild;
using mozilla::dom::ContentChild;

#include "prnetdb.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMJSUtils.h"
#include "nsUnicharUtils.h"
#include "nsDOMStorage.h"
#include "nsEscape.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsReadableUtils.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsICookiePermission.h"
#include "nsIPermission.h"
#include "nsIPermissionManager.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIJSContextStack.h"
#include "nsDOMString.h"
#include "nsNetCID.h"
#include "mozilla/Preferences.h"
#include "nsThreadUtils.h"
#include "mozilla/Telemetry.h"
#include "DictionaryHelpers.h"
#include "GeneratedEvents.h"
#include "mozIApplicationClearPrivateDataParams.h"

// calls FlushAndEvictFromCache(false)
#define NS_DOMSTORAGE_FLUSH_TIMER_TOPIC "domstorage-flush-timer"

// calls FlushAndEvictFromCache(false)
#define NS_DOMSTORAGE_FLUSH_FORCE_TOPIC "domstorage-flush-force"

using namespace mozilla;

static const uint32_t ASK_BEFORE_ACCEPT = 1;
static const uint32_t ACCEPT_SESSION = 2;
static const uint32_t BEHAVIOR_REJECT = 2;

static const char kPermissionType[] = "cookie";
static const char kStorageEnabled[] = "dom.storage.enabled";
static const char kCookiesBehavior[] = "network.cookie.cookieBehavior";
static const char kCookiesLifetimePolicy[] = "network.cookie.lifetimePolicy";

//
// Helper that tells us whether the caller is secure or not.
//

static bool
IsCallerSecure()
{
  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  nsresult rv = nsContentUtils::GetSecurityManager()->
                  GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
  NS_ENSURE_SUCCESS(rv, false);

  if (!subjectPrincipal) {
    // No subject principal means no code is running. Default to not
    // being secure in that case.

    return false;
  }

  nsCOMPtr<nsIURI> codebase;
  subjectPrincipal->GetURI(getter_AddRefs(codebase));

  if (!codebase) {
    return false;
  }

  nsCOMPtr<nsIURI> innerUri = NS_GetInnermostURI(codebase);

  if (!innerUri) {
    return false;
  }

  bool isHttps = false;
  rv = innerUri->SchemeIs("https", &isHttps);

  return NS_SUCCEEDED(rv) && isHttps;
}

nsSessionStorageEntry::nsSessionStorageEntry(KeyTypePointer aStr)
  : nsStringHashKey(aStr), mItem(nullptr)
{
}

nsSessionStorageEntry::nsSessionStorageEntry(const nsSessionStorageEntry& aToCopy)
  : nsStringHashKey(aToCopy), mItem(nullptr)
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
{
}

NS_IMPL_ISUPPORTS3(nsDOMStorageManager,
                   nsIDOMStorageManager,
                   nsIObserver,
                   nsISupportsWeakReference)

//static
nsresult
nsDOMStorageManager::Initialize()
{
  gStorageManager = new nsDOMStorageManager();
  if (!gStorageManager)
    return NS_ERROR_OUT_OF_MEMORY;

  gStorageManager->mStorages.Init();
  NS_ADDREF(gStorageManager);

  // No observers needed in non-chrome
  if (XRE_GetProcessType() != GeckoProcessType_Default)
    return NS_OK;

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os)
    return NS_OK;

  nsresult rv;
  rv = os->AddObserver(gStorageManager, "cookie-changed", true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = os->AddObserver(gStorageManager, "profile-after-change", true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = os->AddObserver(gStorageManager, "perm-changed", true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = os->AddObserver(gStorageManager, "browser:purge-domain-data", true);
  NS_ENSURE_SUCCESS(rv, rv);
  // Used for temporary table flushing
  rv = os->AddObserver(gStorageManager, "profile-before-change", true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = os->AddObserver(gStorageManager, NS_DOMSTORAGE_FLUSH_TIMER_TOPIC, true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = os->AddObserver(gStorageManager, "last-pb-context-exited", true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = os->AddObserver(gStorageManager, "webapps-clear-data", true);
  NS_ENSURE_SUCCESS(rv, rv);

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
  gStorageManager = nullptr;

  ShutdownDB();
}

//static
void
nsDOMStorageManager::ShutdownDB()
{
  delete DOMStorageImpl::gStorageDB;
  DOMStorageImpl::gStorageDB = nullptr;
}

static PLDHashOperator
ClearStorage(nsDOMStorageEntry* aEntry, void* userArg)
{
  aEntry->mStorage->ClearAll();
  return PL_DHASH_REMOVE;
}

static PLDHashOperator
ClearStorageIfDomainMatches(nsDOMStorageEntry* aEntry, void* userArg)
{
  nsAutoCString* aKey = static_cast<nsAutoCString*> (userArg);
  if (StringBeginsWith(aEntry->mStorage->GetScopeDBKey(), *aKey)) {
    aEntry->mStorage->ClearAll();
  }
  return PL_DHASH_REMOVE;
}

nsresult
nsDOMStorageManager::Observe(nsISupports *aSubject,
                             const char *aTopic,
                             const PRUnichar *aData)
{
  if (!strcmp(aTopic, "profile-after-change")) {
  } else if (!strcmp(aTopic, "cookie-changed") &&
             !nsCRT::strcmp(aData, NS_LITERAL_STRING("cleared").get())) {
    mStorages.EnumerateEntries(ClearStorage, nullptr);

    nsresult rv = DOMStorageImpl::InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    return DOMStorageImpl::gStorageDB->RemoveAll();
  } else if (!strcmp(aTopic, "perm-changed")) {
    // Check for cookie permission change
    nsCOMPtr<nsIPermission> perm(do_QueryInterface(aSubject));
    if (perm) {
      nsAutoCString type;
      perm->GetType(type);
      if (type != NS_LITERAL_CSTRING("cookie"))
        return NS_OK;

      uint32_t cap = 0;
      perm->GetCapability(&cap);
      if (!(cap & nsICookiePermission::ACCESS_SESSION) ||
          nsDependentString(aData) != NS_LITERAL_STRING("deleted"))
        return NS_OK;

      nsAutoCString host;
      perm->GetHost(host);
      if (host.IsEmpty())
        return NS_OK;

      nsresult rv = DOMStorageImpl::InitDB();
      NS_ENSURE_SUCCESS(rv, rv);

      return DOMStorageImpl::gStorageDB->DropSessionOnlyStoragesForHost(host);
    }
  } else if (!strcmp(aTopic, "timer-callback")) {
    nsCOMPtr<nsIObserverService> obsserv = mozilla::services::GetObserverService();
    if (obsserv)
      obsserv->NotifyObservers(nullptr, NS_DOMSTORAGE_FLUSH_TIMER_TOPIC, nullptr);
  } else if (!strcmp(aTopic, "browser:purge-domain-data")) {
    // Convert the domain name to the ACE format
    nsAutoCString aceDomain;
    nsresult rv;
    nsCOMPtr<nsIIDNService> converter = do_GetService(NS_IDNSERVICE_CONTRACTID);
    if (converter) {
      rv = converter->ConvertUTF8toACE(NS_ConvertUTF16toUTF8(aData), aceDomain);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // In case the IDN service is not available, this is the best we can come up with!
      NS_EscapeURL(NS_ConvertUTF16toUTF8(aData),
                   esc_OnlyNonASCII | esc_AlwaysCopy,
                   aceDomain);
    }

    nsAutoCString key;
    rv = nsDOMStorageDBWrapper::CreateReversedDomain(aceDomain, key);
    NS_ENSURE_SUCCESS(rv, rv);

    // Clear the storage entries for matching domains
    mStorages.EnumerateEntries(ClearStorageIfDomainMatches, &key);

    rv = DOMStorageImpl::InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    DOMStorageImpl::gStorageDB->RemoveOwner(aceDomain);
  } else if (!strcmp(aTopic, "profile-before-change")) {
    if (DOMStorageImpl::gStorageDB) {
      DebugOnly<nsresult> rv =
        DOMStorageImpl::gStorageDB->FlushAndEvictFromCache(true);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                       "DOMStorage: cache commit failed");
      DOMStorageImpl::gStorageDB->Close();
      nsDOMStorageManager::ShutdownDB();
    }
  } else if (!strcmp(aTopic, NS_DOMSTORAGE_FLUSH_TIMER_TOPIC)) {
    if (DOMStorageImpl::gStorageDB) {
      DebugOnly<nsresult> rv =
        DOMStorageImpl::gStorageDB->FlushAndEvictFromCache(false);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                       "DOMStorage: cache commit failed");
    }
  } else if (!strcmp(aTopic, NS_DOMSTORAGE_FLUSH_FORCE_TOPIC)) {
    if (DOMStorageImpl::gStorageDB) {
      DebugOnly<nsresult> rv =
        DOMStorageImpl::gStorageDB->FlushAndEvictFromCache(false);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                       "DOMStorage: cache  commit failed");
    }
  } else if (!strcmp(aTopic, "last-pb-context-exited")) {
    if (DOMStorageImpl::gStorageDB) {
      return DOMStorageImpl::gStorageDB->DropPrivateBrowsingStorages();
    }
  } else if (!strcmp(aTopic, "webapps-clear-data")) {
    if (!DOMStorageImpl::gStorageDB) {
      return NS_OK;
    }

    nsCOMPtr<mozIApplicationClearPrivateDataParams> params =
      do_QueryInterface(aSubject);
    if (!params) {
      NS_ERROR("'webapps-clear-data' notification's subject should be a mozIApplicationClearPrivateDataParams");
      return NS_ERROR_UNEXPECTED;
    }

    uint32_t appId;
    bool browserOnly;

    nsresult rv = params->GetAppId(&appId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = params->GetBrowserOnly(&browserOnly);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_ASSERT(appId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

    return DOMStorageImpl::gStorageDB->RemoveAllForApp(appId, browserOnly);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMStorageManager::GetUsage(const nsAString& aDomain,
                              int32_t *aUsage)
{
  nsresult rv = DOMStorageImpl::InitDB();
  NS_ENSURE_SUCCESS(rv, rv);

  return DOMStorageImpl::gStorageDB->GetUsage(NS_ConvertUTF16toUTF8(aDomain),
                                              aUsage, false);
}

NS_IMETHODIMP
nsDOMStorageManager::GetLocalStorageForPrincipal(nsIPrincipal *aPrincipal,
                                                 const nsSubstring &aDocumentURI,
                                                 bool aPrivate,
                                                 nsIDOMStorage **aResult)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);
  *aResult = nullptr;

  nsresult rv;

  nsRefPtr<nsDOMStorage2> storage = new nsDOMStorage2();
  if (!storage)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = storage->InitAsLocalStorage(aPrincipal, aDocumentURI, aPrivate);
  if (NS_FAILED(rv))
    return rv;

  *aResult = storage.get();
  storage.forget();

  return NS_OK;
}

void
nsDOMStorageManager::AddToStoragesHash(DOMStorageImpl* aStorage)
{
  nsDOMStorageEntry* entry = mStorages.PutEntry(aStorage);
  if (entry)
    entry->mStorage = aStorage;
}

void
nsDOMStorageManager::RemoveFromStoragesHash(DOMStorageImpl* aStorage)
{
  nsDOMStorageEntry* entry = mStorages.GetEntry(aStorage);
  if (entry)
    mStorages.RemoveEntry(aStorage);
}

//
// nsDOMStorage
//

nsDOMStorageDBWrapper* DOMStorageImpl::gStorageDB = nullptr;

nsDOMStorageEntry::nsDOMStorageEntry(KeyTypePointer aStr)
  : nsPtrHashKey<const void>(aStr), mStorage(nullptr)
{
}

nsDOMStorageEntry::nsDOMStorageEntry(const nsDOMStorageEntry& aToCopy)
  : nsPtrHashKey<const void>(aToCopy), mStorage(nullptr)
{
  NS_ERROR("DOMStorage horked.");
}

nsDOMStorageEntry::~nsDOMStorageEntry()
{
}

NS_IMPL_CYCLE_COLLECTION_1(nsDOMStorage, mStorageImpl)

DOMCI_DATA(StorageObsolete, nsDOMStorage)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMStorage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMStorage)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMStorage)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorageObsolete)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageObsolete)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(StorageObsolete)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMStorage::GetInterface(const nsIID & aIID, void **result)
{
  nsresult rv = mStorageImpl->QueryInterface(aIID, result);
  if (NS_SUCCEEDED(rv))
    return rv;
  return QueryInterface(aIID, result);;
}

nsresult
NS_NewDOMStorage2(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  nsDOMStorage2* storage = new nsDOMStorage2();
  return storage->QueryInterface(aIID, aResult);
}

DOMStorageBase::DOMStorageBase()
  : mStorageType(nsPIDOMStorage::Unknown)
  , mUseDB(false)
  , mSessionOnly(true)
  , mInPrivateBrowsing(false)
{
}

DOMStorageBase::DOMStorageBase(DOMStorageBase& aThat)
  : mStorageType(aThat.mStorageType)
  , mUseDB(false) // Clones don't use the DB
  , mSessionOnly(true)
  , mScopeDBKey(aThat.mScopeDBKey)
  , mQuotaDBKey(aThat.mQuotaDBKey)
  , mInPrivateBrowsing(aThat.mInPrivateBrowsing)
{
}

void
DOMStorageBase::InitAsSessionStorage(nsIPrincipal* aPrincipal, bool aPrivate)
{
  MOZ_ASSERT(mQuotaDBKey.IsEmpty());
  mUseDB = false;
  mScopeDBKey.Truncate();
  mStorageType = nsPIDOMStorage::SessionStorage;
  mInPrivateBrowsing = aPrivate;
}

void
DOMStorageBase::InitAsLocalStorage(nsIPrincipal* aPrincipal, bool aPrivate)
{
  nsDOMStorageDBWrapper::CreateScopeDBKey(aPrincipal, mScopeDBKey);

  // XXX Bug 357323, we have to solve the issue how to define
  // origin for file URLs. In that case CreateOriginScopeDBKey
  // fails (the result is empty) and we must avoid database use
  // in that case because it produces broken entries w/o owner.
  mUseDB = !mScopeDBKey.IsEmpty();

  nsDOMStorageDBWrapper::CreateQuotaDBKey(aPrincipal, mQuotaDBKey);
  mStorageType = nsPIDOMStorage::LocalStorage;
  mInPrivateBrowsing = aPrivate;
}

PLDHashOperator
SessionStorageTraverser(nsSessionStorageEntry* aEntry, void* userArg) {
  nsCycleCollectionTraversalCallback *cb = 
      static_cast<nsCycleCollectionTraversalCallback*>(userArg);

  cb->NoteXPCOMChild((nsIDOMStorageItem *) aEntry->mItem);

  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(DOMStorageImpl)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMStorageImpl)
{
  if (tmp->mItems.IsInitialized()) {
    tmp->mItems.EnumerateEntries(SessionStorageTraverser, &cb);
  }
}
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMStorageImpl)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMStorageImpl)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMStorageImpl)
  NS_INTERFACE_MAP_ENTRY(nsIPrivacyTransitionObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrivacyTransitionObserver)
NS_INTERFACE_MAP_END

DOMStorageImpl::DOMStorageImpl(nsDOMStorage* aStorage)
{
  Init(aStorage);
}

DOMStorageImpl::DOMStorageImpl(nsDOMStorage* aStorage, DOMStorageImpl& aThat)
  : DOMStorageBase(aThat)
{
  Init(aStorage);
}

void
DOMStorageImpl::Init(nsDOMStorage* aStorage)
{
  mItemsCachedVersion = 0;
  mItems.Init(8);
  mOwner = aStorage;
  if (nsDOMStorageManager::gStorageManager)
    nsDOMStorageManager::gStorageManager->AddToStoragesHash(this);
}

DOMStorageImpl::~DOMStorageImpl()
{
  if (nsDOMStorageManager::gStorageManager)
    nsDOMStorageManager::gStorageManager->RemoveFromStoragesHash(this);
}

nsresult
DOMStorageImpl::InitDB()
{
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
      gStorageDB = nullptr;

      return rv;
    }
  }

  return NS_OK;
}

void
DOMStorageImpl::InitFromChild(bool aUseDB,
                              bool aSessionOnly, bool aPrivate,
                              const nsACString& aScopeDBKey,
                              const nsACString& aQuotaDBKey,
                              uint32_t aStorageType)
{
  mUseDB = aUseDB;
  mSessionOnly = aSessionOnly;
  mInPrivateBrowsing = aPrivate;
  mScopeDBKey = aScopeDBKey;
  mQuotaDBKey = aQuotaDBKey;
  mStorageType = static_cast<nsPIDOMStorage::nsDOMStorageType>(aStorageType);
}

void
DOMStorageImpl::SetSessionOnly(bool aSessionOnly)
{
  mSessionOnly = aSessionOnly;
}

bool
DOMStorageImpl::CacheStoragePermissions()
{
  // If this is a cross-process situation, we don't have a real storage owner.
  // All the correct checks have been done on the child, so we just need to
  // make sure that our session-only status is correctly updated.
  if (!mOwner)
    return CanUseStorage();
  
  return mOwner->CacheStoragePermissions();
}

nsresult
DOMStorageImpl::GetCachedValue(const nsAString& aKey, nsAString& aValue,
                               bool* aSecure)
{
  aValue.Truncate();
  *aSecure = false;

  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);
  if (!entry)
    return NS_ERROR_NOT_AVAILABLE;

  aValue = entry->mItem->GetValueInternal();
  *aSecure = entry->mItem->IsSecure();

  return NS_OK;
}

nsresult
DOMStorageImpl::GetDBValue(const nsAString& aKey, nsAString& aValue,
                           bool* aSecure)
{
  aValue.Truncate();

  if (!UseDB())
    return NS_OK;

  nsresult rv = InitDB();
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  rv = gStorageDB->GetKeyValue(this, aKey, value, aSecure);

  if (rv == NS_ERROR_DOM_NOT_FOUND_ERR) {
    SetDOMStringToNull(aValue);
  }

  if (NS_FAILED(rv))
    return rv;

  aValue.Assign(value);

  return NS_OK;
}

nsresult
DOMStorageImpl::SetDBValue(const nsAString& aKey,
                           const nsAString& aValue,
                           bool aSecure)
{
  if (!UseDB())
    return NS_OK;

  nsresult rv = InitDB();
  NS_ENSURE_SUCCESS(rv, rv);

  CacheKeysFromDB();

  rv = gStorageDB->SetKey(this, aKey, aValue, aSecure);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
DOMStorageImpl::SetSecure(const nsAString& aKey, bool aSecure)
{
  if (UseDB()) {
    nsresult rv = InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    return gStorageDB->SetSecure(this, aKey, aSecure);
  }

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
DOMStorageImpl::ClearAll()
{
  mItems.EnumerateEntries(ClearStorageItem, nullptr);
  mItemsCachedVersion = 0;
}

struct CopyArgs {
  DOMStorageImpl* storage;
  bool callerSecure;
};

static PLDHashOperator
CopyStorageItems(nsSessionStorageEntry* aEntry, void* userArg)
{
  // When copying items from one impl to another, we may not
  // have an mOwner that we can call SetItem on. Therefore we need
  // to replicate its behaviour.
  
  CopyArgs* args = static_cast<CopyArgs*>(userArg);

  nsAutoString unused;
  nsresult rv = args->storage->SetValue(args->callerSecure, aEntry->GetKey(),
                                        aEntry->mItem->GetValueInternal(), unused);
  if (NS_FAILED(rv))
    return PL_DHASH_NEXT;

  if (aEntry->mItem->IsSecure()) {
    args->storage->SetSecure(aEntry->GetKey(), true);
  }

  return PL_DHASH_NEXT;
}

nsresult
DOMStorageImpl::CloneFrom(bool aCallerSecure, DOMStorageBase* aThat)
{
  // For various reasons, we no longer call SetItem in CopyStorageItems,
  // so we need to ensure that the storage permissions are correct.
  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;
  
  DOMStorageImpl* that = static_cast<DOMStorageImpl*>(aThat);
  CopyArgs args = { this, aCallerSecure };
  that->mItems.EnumerateEntries(CopyStorageItems, &args);
  return NS_OK;
}

nsresult
DOMStorageImpl::CacheKeysFromDB()
{
  // cache all the keys in the hash. This is used by the Length and Key methods
  // use this cache for better performance. The disadvantage is that the
  // order may break if someone changes the keys in the database directly.
  if (gStorageDB->IsScopeDirty(this)) {
    nsresult rv = InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    mItems.Clear();

    rv = gStorageDB->GetAllKeys(this, &mItems);
    NS_ENSURE_SUCCESS(rv, rv);

    gStorageDB->MarkScopeCached(this);
  }

  return NS_OK;
}

struct KeysArrayBuilderStruct
{
  bool callerIsSecure;
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

nsTArray<nsString>*
DOMStorageImpl::GetKeys(bool aCallerSecure)
{
  if (UseDB())
    CacheKeysFromDB();

  KeysArrayBuilderStruct keystruct;
  keystruct.callerIsSecure = aCallerSecure;
  keystruct.keys = new nsTArray<nsString>();
  if (keystruct.keys)
    mItems.EnumerateEntries(KeysArrayBuilder, &keystruct);
 
  return keystruct.keys;
}

class ItemCounterState
{
 public:
  ItemCounterState(bool aIsCallerSecure)
  : mIsCallerSecure(aIsCallerSecure), mCount(0)
  {
  }

  bool mIsCallerSecure;
  uint32_t mCount;
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

nsresult
DOMStorageImpl::GetLength(bool aCallerSecure, uint32_t* aLength)
{
  // Force reload of items from database.  This ensures sync localStorages for
  // same origins among different windows.
  if (UseDB())
    CacheKeysFromDB();

  ItemCounterState state(aCallerSecure);

  mItems.EnumerateEntries(ItemCounter, &state);

  *aLength = state.mCount;
  return NS_OK;
}

class IndexFinderData
{
 public:
  IndexFinderData(bool aIsCallerSecure, uint32_t aWantedIndex)
  : mIsCallerSecure(aIsCallerSecure), mIndex(0), mWantedIndex(aWantedIndex),
    mItem(nullptr)
  {
  }

  bool mIsCallerSecure;
  uint32_t mIndex;
  uint32_t mWantedIndex;
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

nsresult
DOMStorageImpl::GetKey(bool aCallerSecure, uint32_t aIndex, nsAString& aKey)
{
  // XXX: This does a linear search for the key at index, which would
  // suck if there's a large numer of indexes. Do we care? If so,
  // maybe we need to have a lazily populated key array here or
  // something?

  if (UseDB()) {
    CacheKeysFromDB();
  }

  IndexFinderData data(aCallerSecure, aIndex);
  mItems.EnumerateEntries(IndexFinder, &data);

  if (!data.mItem) {
    // aIndex was larger than the number of accessible keys. Return null.
    aKey.SetIsVoid(true);
    return NS_OK;
  }

  aKey = data.mItem->GetKey();
  return NS_OK;
}

// The behaviour of this function must be kept in sync with StorageChild::GetValue.
// See the explanatory comment there for more details.
nsIDOMStorageItem*
DOMStorageImpl::GetValue(bool aCallerSecure, const nsAString& aKey,
                         nsresult* aResult)
{
  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);
  nsIDOMStorageItem* item = nullptr;
  if (entry) {
    if (aCallerSecure || !entry->mItem->IsSecure()) {
      item = entry->mItem;
    }
  }
  else if (UseDB()) {
    bool secure;
    nsAutoString value;
    nsresult rv = GetDBValue(aKey, value, &secure);
    // return null if access isn't allowed or the key wasn't found
    if (rv == NS_ERROR_DOM_SECURITY_ERR || rv == NS_ERROR_DOM_NOT_FOUND_ERR ||
        (!aCallerSecure && secure))
      return nullptr;

    *aResult = rv;
    NS_ENSURE_SUCCESS(rv, nullptr);

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
DOMStorageImpl::SetValue(bool aIsCallerSecure, const nsAString& aKey,
                         const nsAString& aData, nsAString& aOldValue)
{
  nsresult rv;
  nsString oldValue;
  SetDOMStringToNull(oldValue);

  // First store the value to the database, we need to do this before we update
  // the mItems cache.  SetDBValue is using the old cached value to decide
  // on quota checking.
  if (UseDB()) {
    rv = SetDBValue(aKey, aData, aIsCallerSecure);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);
  if (entry) {
    if (entry->mItem->IsSecure() && !aIsCallerSecure) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
    oldValue = entry->mItem->GetValueInternal();
    entry->mItem->SetValueInternal(aData);
  }
  else {
    nsRefPtr<nsDOMStorageItem> newitem =
        new nsDOMStorageItem(this, aKey, aData, aIsCallerSecure);
    if (!newitem)
      return NS_ERROR_OUT_OF_MEMORY;
    entry = mItems.PutEntry(aKey);
    NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);
    entry->mItem = newitem;
  }
  aOldValue = oldValue;
  return NS_OK;
}

nsresult
DOMStorageImpl::RemoveValue(bool aCallerSecure, const nsAString& aKey,
                            nsAString& aOldValue)
{
  nsString oldValue;
  nsSessionStorageEntry *entry = mItems.GetEntry(aKey);

  if (entry && entry->mItem->IsSecure() && !aCallerSecure) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (UseDB()) {
    nsresult rv = InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    CacheKeysFromDB();
    entry = mItems.GetEntry(aKey);

    nsAutoString value;
    bool secureItem;
    rv = GetDBValue(aKey, value, &secureItem);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!aCallerSecure && secureItem)
      return NS_ERROR_DOM_SECURITY_ERR;

    oldValue = value;

    rv = gStorageDB->RemoveKey(this, aKey);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (entry) {
    // clear string as StorageItems may be referencing this item
    oldValue = entry->mItem->GetValueInternal();
    entry->mItem->ClearValue();
  }

  if (entry) {
    mItems.RawRemoveEntry(entry);
  }
  aOldValue = oldValue;
  return NS_OK;
}

static PLDHashOperator
CheckSecure(nsSessionStorageEntry* aEntry, void* userArg)
{
  bool* secure = (bool*)userArg;
  if (aEntry->mItem->IsSecure()) {
    *secure = true;
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

nsresult
DOMStorageImpl::Clear(bool aCallerSecure, int32_t* aOldCount)
{
  if (UseDB())
    CacheKeysFromDB();

  int32_t oldCount = mItems.Count();

  bool foundSecureItem = false;
  mItems.EnumerateEntries(CheckSecure, &foundSecureItem);

  if (foundSecureItem && !aCallerSecure) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (UseDB()) {
    nsresult rv = InitDB();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = gStorageDB->ClearStorage(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aOldCount = oldCount;
  mItems.Clear();
  return NS_OK;
}

NS_IMETHODIMP
DOMStorageImpl::PrivateModeChanged(bool enabled)
{
  mInPrivateBrowsing = enabled;
  CanUseStorage(); // cause mSessionOnly to update as well
  mItems.Clear();
  mItemsCachedVersion = 0;
  return NS_OK;
}

nsDOMStorage::nsDOMStorage()
  : mStorageType(nsPIDOMStorage::Unknown)
  , mEventBroadcaster(nullptr)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default)
    mStorageImpl = new StorageChild(this);
  else
    mStorageImpl = new DOMStorageImpl(this);
}

nsDOMStorage::nsDOMStorage(nsDOMStorage& aThat)
  : mStorageType(aThat.mStorageType)
  , mPrincipal(aThat.mPrincipal)
  , mEventBroadcaster(nullptr)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    StorageChild* other = static_cast<StorageChild*>(aThat.mStorageImpl.get());
    mStorageImpl = new StorageChild(this, *other);
  } else {
    DOMStorageImpl* other = static_cast<DOMStorageImpl*>(aThat.mStorageImpl.get());
    mStorageImpl = new DOMStorageImpl(this, *other);
  }
}

nsDOMStorage::~nsDOMStorage()
{
}

nsresult
nsDOMStorage::InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                   bool aPrivate)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!uri) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mDocumentURI = aDocumentURI;
  mPrincipal = aPrincipal;

  mStorageType = SessionStorage;

  mStorageImpl->InitAsSessionStorage(mPrincipal, aPrivate);
  return NS_OK;
}

nsresult
nsDOMStorage::InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                 bool aPrivate)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!uri) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mDocumentURI = aDocumentURI;
  mPrincipal = aPrincipal;

  mStorageType = LocalStorage;

  mStorageImpl->InitAsLocalStorage(aPrincipal, aPrivate);
  return NS_OK;
}

bool
DOMStorageBase::CanUseStorage()
{
  return nsDOMStorage::CanUseStorage(this);
}

//static
bool
nsDOMStorage::CanUseStorage(DOMStorageBase* aStorage /* = NULL */)
{
  if (aStorage) {
    // check if the calling domain can use storage. Downgrade to session
    // only if only session storage may be used.
    aStorage->mSessionOnly = false;
  }

  if (!Preferences::GetBool(kStorageEnabled)) {
    return false;
  }

  // chrome can always use storage regardless of permission preferences
  if (nsContentUtils::IsCallerChrome())
    return true;

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  nsresult rv = nsContentUtils::GetSecurityManager()->
                  GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
  NS_ENSURE_SUCCESS(rv, false);

  // if subjectPrincipal were null we'd have returned after
  // IsCallerChrome().

  nsCOMPtr<nsIPermissionManager> permissionManager =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  if (!permissionManager)
    return false;

  uint32_t perm;
  permissionManager->TestPermissionFromPrincipal(subjectPrincipal,
                                                 kPermissionType, &perm);

  if (perm == nsIPermissionManager::DENY_ACTION)
    return false;

  // In private browsing mode we ougth to behave as in session-only cookies
  // mode to prevent detection of being in private browsing mode and ensuring
  // that there will be no traces left.
  if (perm == nsICookiePermission::ACCESS_SESSION ||
      (aStorage && aStorage->IsPrivate())) {
    if (aStorage)
      aStorage->mSessionOnly = true;
  }
  else if (perm != nsIPermissionManager::ALLOW_ACTION) {
    uint32_t cookieBehavior = Preferences::GetUint(kCookiesBehavior);
    uint32_t lifetimePolicy = Preferences::GetUint(kCookiesLifetimePolicy);

    // Treat "ask every time" as "reject always".
    if ((cookieBehavior == BEHAVIOR_REJECT || lifetimePolicy == ASK_BEFORE_ACCEPT))
      return false;

    if (lifetimePolicy == ACCEPT_SESSION && aStorage)
      aStorage->mSessionOnly = true;
  }

  return true;
}

bool
nsDOMStorage::CacheStoragePermissions()
{
  // Bug 488446, disallowing storage use when in session only mode.
  // This is temporary fix before we find complete solution for storage
  // behavior in private browsing mode or session-only cookies mode.
  if (!mStorageImpl->CanUseStorage())
    return false;

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (!ssm)
    return false;

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  nsresult rv = ssm->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
  NS_ENSURE_SUCCESS(rv, false);

  return CanAccess(subjectPrincipal);
}

NS_IMETHODIMP
nsDOMStorage::GetLength(uint32_t *aLength)
{
  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;
  
  return mStorageImpl->GetLength(IsCallerSecure(), aLength);
}

NS_IMETHODIMP
nsDOMStorage::Key(uint32_t aIndex, nsAString& aKey)
{
  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;

  return mStorageImpl->GetKey(IsCallerSecure(), aIndex, aKey);
}

nsIDOMStorageItem*
nsDOMStorage::GetNamedItem(const nsAString& aKey, nsresult* aResult)
{
  if (!CacheStoragePermissions()) {
    *aResult = NS_ERROR_DOM_SECURITY_ERR;
    return nullptr;
  }

  *aResult = NS_OK;
  return mStorageImpl->GetValue(IsCallerSecure(), aKey, aResult);
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

static Telemetry::ID
TelemetryIDForKey(nsPIDOMStorage::nsDOMStorageType type)
{
  switch (type) {
  default:
    MOZ_ASSERT(false);
    // We need to return something to satisfy the compiler.
    // Fallthrough.
  case nsPIDOMStorage::LocalStorage:
    return Telemetry::LOCALDOMSTORAGE_KEY_SIZE_BYTES;
  case nsPIDOMStorage::SessionStorage:
    return Telemetry::SESSIONDOMSTORAGE_KEY_SIZE_BYTES;
  }
}

static Telemetry::ID
TelemetryIDForValue(nsPIDOMStorage::nsDOMStorageType type)
{
  switch (type) {
  default:
    MOZ_ASSERT(false);
    // We need to return something to satisfy the compiler.
    // Fallthrough.
  case nsPIDOMStorage::LocalStorage:
    return Telemetry::LOCALDOMSTORAGE_VALUE_SIZE_BYTES;
  case nsPIDOMStorage::SessionStorage:
    return Telemetry::SESSIONDOMSTORAGE_VALUE_SIZE_BYTES;
  }
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

  Telemetry::Accumulate(TelemetryIDForKey(mStorageType), aKey.Length());
  Telemetry::Accumulate(TelemetryIDForValue(mStorageType), aData.Length());

  nsString oldValue;
  nsresult rv = mStorageImpl->SetValue(IsCallerSecure(), aKey, aData, oldValue);
  if (NS_FAILED(rv))
    return rv;

  if (oldValue != aData && mEventBroadcaster)
    mEventBroadcaster->BroadcastChangeNotification(aKey, oldValue, aData);

  return NS_OK;
}

NS_IMETHODIMP nsDOMStorage::RemoveItem(const nsAString& aKey)
{
  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;

  nsString oldValue;
  nsresult rv = mStorageImpl->RemoveValue(IsCallerSecure(), aKey, oldValue);
  if (rv == NS_ERROR_DOM_NOT_FOUND_ERR)
    return NS_OK;
  if (NS_FAILED(rv))
    return rv;

  if (!oldValue.IsEmpty() && mEventBroadcaster) {
    nsAutoString nullString;
    SetDOMStringToNull(nullString);
    mEventBroadcaster->BroadcastChangeNotification(aKey, oldValue, nullString);
  }

  return NS_OK;
}

nsresult
nsDOMStorage::Clear()
{
  if (!CacheStoragePermissions())
    return NS_ERROR_DOM_SECURITY_ERR;

  int32_t oldCount;
  nsresult rv = mStorageImpl->Clear(IsCallerSecure(), &oldCount);
  if (NS_FAILED(rv))
    return rv;
  
  if (oldCount && mEventBroadcaster) {
    nsAutoString nullString;
    SetDOMStringToNull(nullString);
    mEventBroadcaster->BroadcastChangeNotification(nullString, nullString, nullString);
  }

  return NS_OK;
}

already_AddRefed<nsIDOMStorage>
nsDOMStorage::Clone()
{
  NS_ASSERTION(false, "Old DOMStorage doesn't implement cloning");
  return nullptr;
}

already_AddRefed<nsIDOMStorage>
nsDOMStorage::Fork(const nsSubstring &aDocumentURI)
{
  NS_ASSERTION(false, "Old DOMStorage doesn't implement forking");
  return nullptr;
}

bool nsDOMStorage::IsForkOf(nsIDOMStorage* aThat)
{
  NS_ASSERTION(false, "Old DOMStorage doesn't implement forking");
  return false;
}

nsresult
nsDOMStorage::CloneFrom(nsDOMStorage* aThat)
{
  return mStorageImpl->CloneFrom(IsCallerSecure(), aThat->mStorageImpl);
}

nsTArray<nsString> *
nsDOMStorage::GetKeys()
{
  return mStorageImpl->GetKeys(IsCallerSecure());
}

nsIPrincipal*
nsDOMStorage::Principal()
{
  return nullptr;
}

bool
nsDOMStorage::CanAccessSystem(nsIPrincipal *aPrincipal)
{
  if (!aPrincipal)
    return true;

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (!ssm)
    return false;

  bool isSystem;
  nsresult rv = ssm->IsSystemPrincipal(aPrincipal, &isSystem);

  return NS_SUCCEEDED(rv) && isSystem;
}

bool
nsDOMStorage::CanAccess(nsIPrincipal *aPrincipal)
{
  // Allow C++ callers to access the storage
  if (!aPrincipal)
    return true;

  // Allow more powerful principals (e.g. system) to access the storage

  // For content, either the code base or domain must be the same.  When code
  // base is the same, this is enough to say it is safe for a page to access
  // this storage.

  bool subsumes;
  nsresult rv = aPrincipal->SubsumesIgnoringDomain(mPrincipal, &subsumes);
  if (NS_FAILED(rv))
    return false;

  if (!subsumes) {
    nsresult rv = aPrincipal->Subsumes(mPrincipal, &subsumes);
    if (NS_FAILED(rv))
      return false;
  }

  return subsumes;
}

nsPIDOMStorage::nsDOMStorageType
nsDOMStorage::StorageType()
{
  return mStorageType;
}

//
// nsDOMStorage2
//

NS_IMPL_CYCLE_COLLECTION_1(nsDOMStorage2, mStorage)

DOMCI_DATA(Storage, nsDOMStorage2)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMStorage2)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMStorage2)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMStorage2)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Storage)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMStorage2::GetInterface(const nsIID & aIID, void **result)
{
  nsresult rv = mStorage->GetInterface(aIID, result);
  if (NS_SUCCEEDED(rv))
    return rv;
  return QueryInterface(aIID, result);;
}

nsDOMStorage2::nsDOMStorage2()
{
}

nsDOMStorage2::nsDOMStorage2(nsDOMStorage2& aThat)
{
  mStorage = new nsDOMStorage(*aThat.mStorage.get());
  mPrincipal = aThat.mPrincipal;
}

nsresult
nsDOMStorage2::InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                    bool aPrivate)
{
  mStorage = new nsDOMStorage();
  if (!mStorage)
    return NS_ERROR_OUT_OF_MEMORY;

  mPrincipal = aPrincipal;
  mDocumentURI = aDocumentURI;

  return mStorage->InitAsSessionStorage(aPrincipal, aDocumentURI, aPrivate);
}

nsresult
nsDOMStorage2::InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                  bool aPrivate)
{
  mStorage = new nsDOMStorage();
  if (!mStorage)
    return NS_ERROR_OUT_OF_MEMORY;

  mPrincipal = aPrincipal;
  mDocumentURI = aDocumentURI;

  return mStorage->InitAsLocalStorage(aPrincipal, aDocumentURI, aPrivate);
}

already_AddRefed<nsIDOMStorage>
nsDOMStorage2::Clone()
{
  nsDOMStorage2* storage = new nsDOMStorage2(*this);
  if (!storage)
    return nullptr;

  storage->mStorage->CloneFrom(mStorage);
  NS_ADDREF(storage);

  return storage;
}

already_AddRefed<nsIDOMStorage>
nsDOMStorage2::Fork(const nsSubstring &aDocumentURI)
{
  nsRefPtr<nsDOMStorage2> storage = new nsDOMStorage2();
  storage->InitAsSessionStorageFork(mPrincipal, aDocumentURI, mStorage);
  return storage.forget();
}

bool nsDOMStorage2::IsForkOf(nsIDOMStorage* aThat)
{
  if (!aThat)
    return false;

  nsDOMStorage2* storage = static_cast<nsDOMStorage2*>(aThat);
  return mStorage == storage->mStorage;
}

void
nsDOMStorage2::InitAsSessionStorageFork(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI, nsDOMStorage* aStorage)
{
  mPrincipal = aPrincipal;
  mDocumentURI = aDocumentURI;
  mStorage = aStorage;
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

bool
nsDOMStorage2::CanAccess(nsIPrincipal *aPrincipal)
{
  return mStorage->CanAccess(aPrincipal);
}

nsPIDOMStorage::nsDOMStorageType
nsDOMStorage2::StorageType()
{
  if (mStorage)
    return mStorage->StorageType();

  return nsPIDOMStorage::Unknown;
}

namespace {

class StorageNotifierRunnable : public nsRunnable
{
public:
  StorageNotifierRunnable(nsISupports* aSubject)
    : mSubject(aSubject)
  { }

  NS_DECL_NSIRUNNABLE

private:
  nsCOMPtr<nsISupports> mSubject;
};

NS_IMETHODIMP
StorageNotifierRunnable::Run()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(mSubject, "dom-storage2-changed", nullptr);
  }
  return NS_OK;
}

} // anonymous namespace

void
nsDOMStorage2::BroadcastChangeNotification(const nsSubstring &aKey,
                                           const nsSubstring &aOldValue,
                                           const nsSubstring &aNewValue)
{
  nsresult rv;
  nsCOMPtr<nsIDOMEvent> domEvent;
  NS_NewDOMStorageEvent(getter_AddRefs(domEvent), nullptr, nullptr);
  nsCOMPtr<nsIDOMStorageEvent> event = do_QueryInterface(domEvent);
  rv = event->InitStorageEvent(NS_LITERAL_STRING("storage"),
                               false,
                               false,
                               aKey,
                               aOldValue,
                               aNewValue,
                               mDocumentURI,
                               static_cast<nsIDOMStorage*>(this));
  if (NS_FAILED(rv)) {
    return;
  }

  nsRefPtr<StorageNotifierRunnable> r = new StorageNotifierRunnable(event);
  NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
nsDOMStorage2::GetLength(uint32_t *aLength)
{
  return mStorage->GetLength(aLength);
}

NS_IMETHODIMP
nsDOMStorage2::Key(uint32_t aIndex, nsAString& aKey)
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
// nsDOMStorageItem
//

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMStorageItem)
  {
    tmp->mStorage = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMStorageItem)
  {
    cb.NoteXPCOMChild(tmp->mStorage);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMStorageItem)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMStorageItem)

DOMCI_DATA(StorageItem, nsDOMStorageItem)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMStorageItem)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorageItem)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageItem)
  NS_INTERFACE_MAP_ENTRY(nsIDOMToString)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(StorageItem)
NS_INTERFACE_MAP_END

nsDOMStorageItem::nsDOMStorageItem(DOMStorageBase* aStorage,
                                   const nsAString& aKey,
                                   const nsAString& aValue,
                                   bool aSecure)
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
nsDOMStorageItem::GetSecure(bool* aSecure)
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
nsDOMStorageItem::SetSecure(bool aSecure)
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
    bool secure;
    nsresult rv = mStorage->GetDBValue(mKey, aValue, &secure);
    if (rv == NS_ERROR_DOM_NOT_FOUND_ERR)
      return NS_OK;
    if (NS_SUCCEEDED(rv) && !IsCallerSecure() && secure)
      return NS_ERROR_DOM_SECURITY_ERR;
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

  bool secureCaller = IsCallerSecure();

  if (mStorage->UseDB()) {
    // SetDBValue() does the security checks for us.
    return mStorage->SetDBValue(mKey, aValue, secureCaller);
  }

  bool secureItem = IsSecure();

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

