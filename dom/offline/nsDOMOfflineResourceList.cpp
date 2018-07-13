/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMOfflineResourceList.h"
#include "nsIScriptSecurityManager.h"
#include "nsError.h"
#include "mozilla/dom/DOMStringList.h"
#include "nsIPrefetchService.h"
#include "nsCPrefetchService.h"
#include "nsMemory.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsContentUtils.h"
#include "nsILoadContext.h"
#include "nsIObserverService.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebNavigation.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/OfflineResourceListBinding.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/BasePrincipal.h"

#include "nsXULAppAPI.h"
#define IS_CHILD_PROCESS() \
    (GeckoProcessType_Default != XRE_GetProcessType())

using namespace mozilla;
using namespace mozilla::dom;

// Event names

#define CHECKING_STR    "checking"
#define ERROR_STR       "error"
#define NOUPDATE_STR    "noupdate"
#define DOWNLOADING_STR "downloading"
#define PROGRESS_STR    "progress"
#define CACHED_STR      "cached"
#define UPDATEREADY_STR "updateready"
#define OBSOLETE_STR    "obsolete"

// To prevent abuse of the resource list for data storage, the number
// of offline urls and their length are limited.

static const char kMaxEntriesPref[] =  "offline.max_site_resources";
#define DEFAULT_MAX_ENTRIES 100
#define MAX_URI_LENGTH 2048

//
// nsDOMOfflineResourceList
//

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsDOMOfflineResourceList,
                                   DOMEventTargetHelper,
                                   mCacheUpdate,
                                   mPendingEvents)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMOfflineResourceList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMOfflineResourceList)
  NS_INTERFACE_MAP_ENTRY(nsIOfflineCacheUpdateObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsDOMOfflineResourceList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsDOMOfflineResourceList, DOMEventTargetHelper)

nsDOMOfflineResourceList::nsDOMOfflineResourceList(nsIURI *aManifestURI,
                                                   nsIURI *aDocumentURI,
                                                   nsIPrincipal *aLoadingPrincipal,
                                                   nsPIDOMWindowInner *aWindow)
  : DOMEventTargetHelper(aWindow)
  , mInitialized(false)
  , mManifestURI(aManifestURI)
  , mDocumentURI(aDocumentURI)
  , mLoadingPrincipal(aLoadingPrincipal)
  , mExposeCacheUpdateStatus(true)
  , mStatus(OfflineResourceList_Binding::IDLE)
  , mCachedKeys(nullptr)
  , mCachedKeysCount(0)
{
}

nsDOMOfflineResourceList::~nsDOMOfflineResourceList()
{
  ClearCachedKeys();
}

JSObject*
nsDOMOfflineResourceList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return OfflineResourceList_Binding::Wrap(aCx, this, aGivenProto);
}

nsresult
nsDOMOfflineResourceList::Init()
{
  if (mInitialized) {
    return NS_OK;
  }

  if (!mManifestURI) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  mManifestURI->GetAsciiSpec(mManifestSpec);

  nsresult rv = nsContentUtils::GetSecurityManager()->
                   CheckSameOriginURI(mManifestURI, mDocumentURI, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Dynamically-managed resources are stored as a separate ownership list
  // from the manifest.
  nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(mDocumentURI);
  if (!innerURI)
    return NS_ERROR_FAILURE;

  if (!IS_CHILD_PROCESS())
  {
    mApplicationCacheService =
      do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Check for in-progress cache updates
    nsCOMPtr<nsIOfflineCacheUpdateService> cacheUpdateService =
      do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t numUpdates;
    rv = cacheUpdateService->GetNumUpdates(&numUpdates);
    NS_ENSURE_SUCCESS(rv, rv);

    for (uint32_t i = 0; i < numUpdates; i++) {
      nsCOMPtr<nsIOfflineCacheUpdate> cacheUpdate;
      rv = cacheUpdateService->GetUpdate(i, getter_AddRefs(cacheUpdate));
      NS_ENSURE_SUCCESS(rv, rv);

      UpdateAdded(cacheUpdate);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // watch for new offline cache updates
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (!observerService)
    return NS_ERROR_FAILURE;

  rv = observerService->AddObserver(this, "offline-cache-update-added", true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = observerService->AddObserver(this, "offline-cache-update-completed", true);
  NS_ENSURE_SUCCESS(rv, rv);

  mInitialized = true;

  return NS_OK;
}

void
nsDOMOfflineResourceList::Disconnect()
{
  mPendingEvents.Clear();

  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nullptr;
  }
}

//
// nsDOMOfflineResourceList::nsIDOMOfflineResourceList
//

already_AddRefed<DOMStringList>
nsDOMOfflineResourceList::GetMozItems(ErrorResult& aRv)
{
  if (IS_CHILD_PROCESS()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  RefPtr<DOMStringList> items = new DOMStringList();

  // If we are not associated with an application cache, return an
  // empty list.
  nsCOMPtr<nsIApplicationCache> appCache = GetDocumentAppCache();
  if (!appCache) {
    return items.forget();
  }

  aRv = Init();
  if (aRv.Failed()) {
    return nullptr;
  }

  uint32_t length;
  char **keys;
  aRv = appCache->GatherEntries(nsIApplicationCache::ITEM_DYNAMIC,
                                &length, &keys);
  if (aRv.Failed()) {
    return nullptr;
  }

  for (uint32_t i = 0; i < length; i++) {
    items->Add(NS_ConvertUTF8toUTF16(keys[i]));
  }

  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(length, keys);

  return items.forget();
}

NS_IMETHODIMP
nsDOMOfflineResourceList::GetMozItems(nsISupports** aItems)
{
  ErrorResult rv;
  RefPtr<DOMStringList> items = GetMozItems(rv);
  items.forget(aItems);
  return rv.StealNSResult();
}

bool
nsDOMOfflineResourceList::MozHasItem(const nsAString& aURI, ErrorResult& aRv)
{
  if (IS_CHILD_PROCESS()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return false;
  }

  nsresult rv = Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return false;
  }

  nsCOMPtr<nsIApplicationCache> appCache = GetDocumentAppCache();
  if (!appCache) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  nsAutoCString key;
  rv = GetCacheKey(aURI, key);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return false;
  }

  uint32_t types;
  rv = appCache->GetTypes(key, &types);
  if (rv == NS_ERROR_CACHE_KEY_NOT_FOUND) {
    return false;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return false;
  }

  return types & nsIApplicationCache::ITEM_DYNAMIC;
}

uint32_t
nsDOMOfflineResourceList::GetMozLength(ErrorResult& aRv)
{
  if (IS_CHILD_PROCESS()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return 0;
  }

  if (!mManifestURI) {
    return 0;
  }

  nsresult rv = Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return 0;
  }

  rv = CacheKeys();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return 0;
  }

  return mCachedKeysCount;
}

void
nsDOMOfflineResourceList::MozItem(uint32_t aIndex, nsAString& aURI,
                                  ErrorResult& aRv)
{
  bool found;
  IndexedGetter(aIndex, found, aURI, aRv);
  if (!aRv.Failed() && !found) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
  }
}

void
nsDOMOfflineResourceList::IndexedGetter(uint32_t aIndex, bool& aFound,
                                        nsAString& aURI, ErrorResult& aRv)
{
  if (IS_CHILD_PROCESS()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  nsresult rv = Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  rv = CacheKeys();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  if (aIndex >= mCachedKeysCount) {
    aFound = false;
    return;
  }

  aFound = true;
  CopyUTF8toUTF16(mCachedKeys[aIndex], aURI);
}

void
nsDOMOfflineResourceList::MozAdd(const nsAString& aURI, ErrorResult& aRv)
{
  if (IS_CHILD_PROCESS()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  nsresult rv = Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  if (!nsContentUtils::OfflineAppAllowed(mDocumentURI)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIApplicationCache> appCache = GetDocumentAppCache();
  if (!appCache) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (aURI.Length() > MAX_URI_LENGTH) {
    aRv.Throw(NS_ERROR_DOM_BAD_URI);
    return;
  }

  // this will fail if the URI is not absolute
  nsCOMPtr<nsIURI> requestedURI;
  rv = NS_NewURI(getter_AddRefs(requestedURI), aURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsAutoCString scheme;
  rv = requestedURI->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  bool match;
  rv = mManifestURI->SchemeIs(scheme.get(), &match);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  if (!match) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  uint32_t length = GetMozLength(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  uint32_t maxEntries =
    Preferences::GetUint(kMaxEntriesPref, DEFAULT_MAX_ENTRIES);

  if (length > maxEntries) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  ClearCachedKeys();

  nsCOMPtr<nsIOfflineCacheUpdate> update =
    do_CreateInstance(NS_OFFLINECACHEUPDATE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsAutoCString clientID;
  rv = appCache->GetClientID(clientID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  rv = update->InitPartial(mManifestURI, clientID,
                           mDocumentURI, mLoadingPrincipal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  rv = update->AddDynamicURI(requestedURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  rv = update->Schedule();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
}

void
nsDOMOfflineResourceList::MozRemove(const nsAString& aURI, ErrorResult& aRv)
{
  if (IS_CHILD_PROCESS()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  nsresult rv = Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  if (!nsContentUtils::OfflineAppAllowed(mDocumentURI)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIApplicationCache> appCache = GetDocumentAppCache();
  if (!appCache) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsAutoCString key;
  rv = GetCacheKey(aURI, key);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  ClearCachedKeys();

  // XXX: This is a race condition.  remove() is specced to remove
  // from the currently associated application cache, but if this
  // happens during an update (or after an update, if we haven't
  // swapped yet), that remove() will be lost when the next update is
  // finished.  Need to bring this issue up.

  rv = appCache->UnmarkEntry(key, nsIApplicationCache::ITEM_DYNAMIC);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
}

uint16_t
nsDOMOfflineResourceList::GetStatus(ErrorResult& aRv)
{
  nsresult rv = Init();

  // Init may fail with INVALID_STATE_ERR if there is no manifest URI.
  // The status attribute should not throw that exception, convert it
  // to an UNCACHED.
  if (rv == NS_ERROR_DOM_INVALID_STATE_ERR ||
      !nsContentUtils::OfflineAppAllowed(mDocumentURI)) {
    return OfflineResourceList_Binding::UNCACHED;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return 0;
  }

  // If this object is not associated with a cache, return UNCACHED
  nsCOMPtr<nsIApplicationCache> appCache = GetDocumentAppCache();
  if (!appCache) {
    return OfflineResourceList_Binding::UNCACHED;
  }


  // If there is an update in process, use its status.
  if (mCacheUpdate && mExposeCacheUpdateStatus) {
    uint16_t status;
    rv = mCacheUpdate->GetStatus(&status);
    if (NS_SUCCEEDED(rv) && status != OfflineResourceList_Binding::IDLE) {
      return status;
    }
  }

  if (mAvailableApplicationCache) {
    return OfflineResourceList_Binding::UPDATEREADY;
  }

  return mStatus;
}

void
nsDOMOfflineResourceList::Update(ErrorResult& aRv)
{
  nsresult rv = Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  if (!nsContentUtils::OfflineAppAllowed(mDocumentURI)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIOfflineCacheUpdateService> updateService =
    do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();

  nsCOMPtr<nsIOfflineCacheUpdate> update;
  rv = updateService->ScheduleUpdate(mManifestURI, mDocumentURI, mLoadingPrincipal,
                                     window, getter_AddRefs(update));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
}

NS_IMETHODIMP
nsDOMOfflineResourceList::SwapCache()
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsContentUtils::OfflineAppAllowed(mDocumentURI)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIApplicationCache> currentAppCache = GetDocumentAppCache();
  if (!currentAppCache) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  // Check the current and potentially newly available cache are not identical.
  if (mAvailableApplicationCache == currentAppCache) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mAvailableApplicationCache) {
    nsCString currClientId, availClientId;
    currentAppCache->GetClientID(currClientId);
    mAvailableApplicationCache->GetClientID(availClientId);
    if (availClientId == currClientId)
      return NS_ERROR_DOM_INVALID_STATE_ERR;
  } else if (mStatus != OfflineResourceList_Binding::OBSOLETE) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  ClearCachedKeys();

  nsCOMPtr<nsIApplicationCacheContainer> appCacheContainer =
    GetDocumentAppCacheContainer();

  // In the case of an obsolete cache group, newAppCache might be null.
  // We will disassociate from the cache in that case.
  if (appCacheContainer) {
    rv = appCacheContainer->SetApplicationCache(mAvailableApplicationCache);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mAvailableApplicationCache = nullptr;
  mStatus = OfflineResourceList_Binding::IDLE;

  return NS_OK;
}

void
nsDOMOfflineResourceList::FirePendingEvents()
{
  for (int32_t i = 0; i < mPendingEvents.Count(); ++i) {
    RefPtr<Event> event = mPendingEvents[i];
    DispatchEvent(*event);
  }
  mPendingEvents.Clear();
}

nsresult
nsDOMOfflineResourceList::SendEvent(const nsAString &aEventName)
{
  // Don't send events to closed windows
  if (!GetOwner()) {
    return NS_OK;
  }

  if (!GetOwner()->GetDocShell()) {
    return NS_OK;
  }

  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event->InitEvent(aEventName, false, true);

  // We assume anyone that managed to call SendEvent is trusted
  event->SetTrusted(true);

  // If the window is frozen or we're still catching up on events that were
  // queued while frozen, save the event for later.
  if (GetOwner()->IsFrozen() || mPendingEvents.Count() > 0) {
    mPendingEvents.AppendObject(event);
    return NS_OK;
  }

  DispatchEvent(*event);

  return NS_OK;
}


//
// nsDOMOfflineResourceList::nsIObserver
//
NS_IMETHODIMP
nsDOMOfflineResourceList::Observe(nsISupports *aSubject,
                                    const char *aTopic,
                                    const char16_t *aData)
{
  if (!strcmp(aTopic, "offline-cache-update-added")) {
    nsCOMPtr<nsIOfflineCacheUpdate> update = do_QueryInterface(aSubject);
    if (update) {
      UpdateAdded(update);
    }
  } else if (!strcmp(aTopic, "offline-cache-update-completed")) {
    nsCOMPtr<nsIOfflineCacheUpdate> update = do_QueryInterface(aSubject);
    if (update) {
      UpdateCompleted(update);
    }
  }

  return NS_OK;
}

//
// nsDOMOfflineResourceList::nsIOfflineCacheUpdateObserver
//
NS_IMETHODIMP
nsDOMOfflineResourceList::UpdateStateChanged(nsIOfflineCacheUpdate *aUpdate,
                                     uint32_t event)
{
  mExposeCacheUpdateStatus =
      (event == STATE_CHECKING) ||
      (event == STATE_DOWNLOADING) ||
      (event == STATE_ITEMSTARTED) ||
      (event == STATE_ITEMCOMPLETED) ||
      // During notification of "obsolete" we must expose state of the update
      (event == STATE_OBSOLETE);

  switch (event) {
    case STATE_ERROR:
      SendEvent(NS_LITERAL_STRING(ERROR_STR));
      break;
    case STATE_CHECKING:
      SendEvent(NS_LITERAL_STRING(CHECKING_STR));
      break;
    case STATE_NOUPDATE:
      SendEvent(NS_LITERAL_STRING(NOUPDATE_STR));
      break;
    case STATE_OBSOLETE:
      mStatus = OfflineResourceList_Binding::OBSOLETE;
      mAvailableApplicationCache = nullptr;
      SendEvent(NS_LITERAL_STRING(OBSOLETE_STR));
      break;
    case STATE_DOWNLOADING:
      SendEvent(NS_LITERAL_STRING(DOWNLOADING_STR));
      break;
    case STATE_ITEMSTARTED:
      SendEvent(NS_LITERAL_STRING(PROGRESS_STR));
      break;
    case STATE_ITEMCOMPLETED:
      // Nothing to do here...
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::ApplicationCacheAvailable(nsIApplicationCache *aApplicationCache)
{
  nsCOMPtr<nsIApplicationCache> currentAppCache = GetDocumentAppCache();
  if (currentAppCache) {
    // Document already has a cache, we cannot override it.  swapCache is
    // here to do it on demand.

    // If the newly available cache is identical to the current cache, then
    // just ignore this event.
    if (aApplicationCache == currentAppCache) {
      return NS_OK;
    }

    nsCString currClientId, availClientId;
    currentAppCache->GetClientID(currClientId);
    aApplicationCache->GetClientID(availClientId);
    if (availClientId == currClientId) {
      return NS_OK;
    }

    mAvailableApplicationCache = aApplicationCache;
    return NS_OK;
  }

  nsCOMPtr<nsIApplicationCacheContainer> appCacheContainer =
    GetDocumentAppCacheContainer();

  if (appCacheContainer) {
    appCacheContainer->SetApplicationCache(aApplicationCache);
  }

  mAvailableApplicationCache = nullptr;
  return NS_OK;
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
nsDOMOfflineResourceList::UpdateAdded(nsIOfflineCacheUpdate *aUpdate)
{
  // Ignore partial updates.
  bool partial;
  nsresult rv = aUpdate->GetPartial(&partial);
  NS_ENSURE_SUCCESS(rv, rv);

  if (partial) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> updateURI;
  rv = aUpdate->GetManifestURI(getter_AddRefs(updateURI));
  NS_ENSURE_SUCCESS(rv, rv);

  bool equals;
  rv = updateURI->Equals(mManifestURI, &equals);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!equals) {
    // This update doesn't belong to us
    return NS_OK;
  }

  NS_ENSURE_TRUE(!mCacheUpdate, NS_ERROR_FAILURE);

  // We don't need to emit signals here.  Updates are either added
  // when they are scheduled (in which case they are always IDLE) or
  // they are added when the applicationCache object is initialized, so there
  // are no listeners to accept signals anyway.

  mCacheUpdate = aUpdate;
  mCacheUpdate->AddObserver(this, true);

  return NS_OK;
}

already_AddRefed<nsIApplicationCacheContainer>
nsDOMOfflineResourceList::GetDocumentAppCacheContainer()
{
  nsCOMPtr<nsIWebNavigation> webnav = do_GetInterface(GetOwner());
  if (!webnav) {
    return nullptr;
  }

  nsCOMPtr<nsIApplicationCacheContainer> appCacheContainer =
    do_GetInterface(webnav);
  return appCacheContainer.forget();
}

already_AddRefed<nsIApplicationCache>
nsDOMOfflineResourceList::GetDocumentAppCache()
{
  nsCOMPtr<nsIApplicationCacheContainer> appCacheContainer =
    GetDocumentAppCacheContainer();

  if (appCacheContainer) {
    nsCOMPtr<nsIApplicationCache> applicationCache;
    appCacheContainer->GetApplicationCache(
      getter_AddRefs(applicationCache));
    return applicationCache.forget();
  }

  return nullptr;
}

nsresult
nsDOMOfflineResourceList::UpdateCompleted(nsIOfflineCacheUpdate *aUpdate)
{
  if (aUpdate != mCacheUpdate) {
    // This isn't the update we're watching.
    return NS_OK;
  }

  bool partial;
  mCacheUpdate->GetPartial(&partial);
  bool isUpgrade;
  mCacheUpdate->GetIsUpgrade(&isUpgrade);

  bool succeeded;
  nsresult rv = mCacheUpdate->GetSucceeded(&succeeded);

  mCacheUpdate->RemoveObserver(this);
  mCacheUpdate = nullptr;

  if (NS_SUCCEEDED(rv) && succeeded && !partial) {
    mStatus = OfflineResourceList_Binding::IDLE;
    if (isUpgrade) {
      SendEvent(NS_LITERAL_STRING(UPDATEREADY_STR));
    } else {
      SendEvent(NS_LITERAL_STRING(CACHED_STR));
    }
  }

  return NS_OK;
}

nsresult
nsDOMOfflineResourceList::GetCacheKey(nsIURI *aURI, nsCString &aKey)
{
  nsresult rv = aURI->GetAsciiSpec(aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  // url fragments aren't used in cache keys
  nsAutoCString::const_iterator specStart, specEnd;
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
  if (IS_CHILD_PROCESS())
    return NS_ERROR_NOT_IMPLEMENTED;

  if (mCachedKeys)
    return NS_OK;

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(GetOwner());
  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(window);
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(webNav);

  nsAutoCString originSuffix;
  if (loadContext) {
    mozilla::OriginAttributes oa;
    loadContext->GetOriginAttributes(oa);

    oa.CreateSuffix(originSuffix);
  }

  nsAutoCString groupID;
  mApplicationCacheService->BuildGroupIDForSuffix(
      mManifestURI, originSuffix, groupID);

  nsCOMPtr<nsIApplicationCache> appCache;
  mApplicationCacheService->GetActiveCache(groupID,
                                           getter_AddRefs(appCache));

  if (!appCache) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  return appCache->GatherEntries(nsIApplicationCache::ITEM_DYNAMIC,
                                 &mCachedKeysCount, &mCachedKeys);
}

void
nsDOMOfflineResourceList::ClearCachedKeys()
{
  if (mCachedKeys) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mCachedKeysCount, mCachedKeys);
    mCachedKeys = nullptr;
    mCachedKeysCount = 0;
  }
}
