/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMOfflineResourceList.h"
#include "nsDOMClassInfoID.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsDOMLists.h"
#include "nsIPrefetchService.h"
#include "nsCPrefetchService.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsICacheSession.h"
#include "nsICacheService.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsIDOMLoadStatus.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsIJSContextStack.h"
#include "nsEventDispatcher.h"
#include "nsIObserverService.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebNavigation.h"
#include "mozilla/Preferences.h"

#include "nsXULAppAPI.h"
#define IS_CHILD_PROCESS() \
    (GeckoProcessType_Default != XRE_GetProcessType())

using namespace mozilla;

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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMOfflineResourceList)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMOfflineResourceList,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCacheUpdate)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnCheckingListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnNoUpdateListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnDownloadingListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnProgressListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnCachedListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnUpdateReadyListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnObsoleteListener)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mPendingEvents);

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMOfflineResourceList,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCacheUpdate)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnCheckingListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnNoUpdateListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnDownloadingListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnProgressListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnCachedListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnUpdateReadyListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnObsoleteListener)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mPendingEvents)

NS_IMPL_CYCLE_COLLECTION_UNLINK_END

DOMCI_DATA(OfflineResourceList, nsDOMOfflineResourceList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMOfflineResourceList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMOfflineResourceList)
  NS_INTERFACE_MAP_ENTRY(nsIOfflineCacheUpdateObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(OfflineResourceList)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsDOMOfflineResourceList, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsDOMOfflineResourceList, nsDOMEventTargetHelper)

nsDOMOfflineResourceList::nsDOMOfflineResourceList(nsIURI *aManifestURI,
                                                   nsIURI *aDocumentURI,
                                                   nsPIDOMWindow *aWindow)
  : mInitialized(false)
  , mManifestURI(aManifestURI)
  , mDocumentURI(aDocumentURI)
  , mExposeCacheUpdateStatus(true)
  , mStatus(nsIDOMOfflineResourceList::IDLE)
  , mCachedKeys(nullptr)
  , mCachedKeysCount(0)
{
  BindToOwner(aWindow);
}

nsDOMOfflineResourceList::~nsDOMOfflineResourceList()
{
  ClearCachedKeys();
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

    PRUint32 numUpdates;
    rv = cacheUpdateService->GetNumUpdates(&numUpdates);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRUint32 i = 0; i < numUpdates; i++) {
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
  mOnCheckingListener = nullptr;
  mOnErrorListener = nullptr;
  mOnNoUpdateListener = nullptr;
  mOnDownloadingListener = nullptr;
  mOnProgressListener = nullptr;
  mOnCachedListener = nullptr;
  mOnUpdateReadyListener = nullptr;
  mOnObsoleteListener = nullptr;

  mPendingEvents.Clear();

  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nullptr;
  }
}

//
// nsDOMOfflineResourceList::nsIDOMOfflineResourceList
//

NS_IMETHODIMP
nsDOMOfflineResourceList::GetMozItems(nsIDOMDOMStringList **aItems)
{
  if (IS_CHILD_PROCESS()) 
    return NS_ERROR_NOT_IMPLEMENTED;

  *aItems = nullptr;

  nsRefPtr<nsDOMStringList> items = new nsDOMStringList();
  NS_ENSURE_TRUE(items, NS_ERROR_OUT_OF_MEMORY);

  // If we are not associated with an application cache, return an
  // empty list.
  nsCOMPtr<nsIApplicationCache> appCache = GetDocumentAppCache();
  if (!appCache) {
    NS_ADDREF(*aItems = items);
    return NS_OK;
  }

  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 length;
  char **keys;
  rv = appCache->GatherEntries(nsIApplicationCache::ITEM_DYNAMIC,
                               &length, &keys);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < length; i++) {
    items->Add(NS_ConvertUTF8toUTF16(keys[i]));
  }

  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(length, keys);

  NS_ADDREF(*aItems = items);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::MozHasItem(const nsAString& aURI, bool* aExists)
{
  if (IS_CHILD_PROCESS()) 
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIApplicationCache> appCache = GetDocumentAppCache();
  if (!appCache) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCAutoString key;
  rv = GetCacheKey(aURI, key);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 types;
  rv = appCache->GetTypes(key, &types);
  if (rv == NS_ERROR_CACHE_KEY_NOT_FOUND) {
    *aExists = false;
    return NS_OK;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  *aExists = ((types & nsIApplicationCache::ITEM_DYNAMIC) != 0);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::GetMozLength(PRUint32 *aLength)
{
  if (IS_CHILD_PROCESS()) 
    return NS_ERROR_NOT_IMPLEMENTED;

  if (!mManifestURI) {
    *aLength = 0;
    return NS_OK;
  }

  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CacheKeys();
  NS_ENSURE_SUCCESS(rv, rv);

  *aLength = mCachedKeysCount;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::MozItem(PRUint32 aIndex, nsAString& aURI)
{
  if (IS_CHILD_PROCESS()) 
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  SetDOMStringToNull(aURI);

  rv = CacheKeys();
  NS_ENSURE_SUCCESS(rv, rv);

  if (aIndex >= mCachedKeysCount)
    return NS_ERROR_NOT_AVAILABLE;

  CopyUTF8toUTF16(mCachedKeys[aIndex], aURI);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::MozAdd(const nsAString& aURI)
{
  if (IS_CHILD_PROCESS()) 
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsContentUtils::OfflineAppAllowed(mDocumentURI)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIApplicationCache> appCache = GetDocumentAppCache();
  if (!appCache) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (aURI.Length() > MAX_URI_LENGTH) return NS_ERROR_DOM_BAD_URI;

  // this will fail if the URI is not absolute
  nsCOMPtr<nsIURI> requestedURI;
  rv = NS_NewURI(getter_AddRefs(requestedURI), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString scheme;
  rv = requestedURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  bool match;
  rv = mManifestURI->SchemeIs(scheme.get(), &match);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!match) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  PRUint32 length;
  rv = GetMozLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  PRUint32 maxEntries =
    Preferences::GetUint(kMaxEntriesPref, DEFAULT_MAX_ENTRIES);

  if (length > maxEntries) return NS_ERROR_NOT_AVAILABLE;

  ClearCachedKeys();

  nsCOMPtr<nsIOfflineCacheUpdate> update =
    do_CreateInstance(NS_OFFLINECACHEUPDATE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString clientID;
  rv = appCache->GetClientID(clientID);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = update->InitPartial(mManifestURI, clientID, mDocumentURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = update->AddDynamicURI(requestedURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = update->Schedule();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::MozRemove(const nsAString& aURI)
{
  if (IS_CHILD_PROCESS()) 
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsContentUtils::OfflineAppAllowed(mDocumentURI)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIApplicationCache> appCache = GetDocumentAppCache();
  if (!appCache) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsCAutoString key;
  rv = GetCacheKey(aURI, key);
  NS_ENSURE_SUCCESS(rv, rv);

  ClearCachedKeys();

  // XXX: This is a race condition.  remove() is specced to remove
  // from the currently associated application cache, but if this
  // happens during an update (or after an update, if we haven't
  // swapped yet), that remove() will be lost when the next update is
  // finished.  Need to bring this issue up.

  rv = appCache->UnmarkEntry(key, nsIApplicationCache::ITEM_DYNAMIC);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::GetStatus(PRUint16 *aStatus)
{
  nsresult rv = Init();

  // Init may fail with INVALID_STATE_ERR if there is no manifest URI.
  // The status attribute should not throw that exception, convert it
  // to an UNCACHED.
  if (rv == NS_ERROR_DOM_INVALID_STATE_ERR ||
      !nsContentUtils::OfflineAppAllowed(mDocumentURI)) {
    *aStatus = nsIDOMOfflineResourceList::UNCACHED;
    return NS_OK;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // If this object is not associated with a cache, return UNCACHED
  nsCOMPtr<nsIApplicationCache> appCache = GetDocumentAppCache();
  if (!appCache) {
    *aStatus = nsIDOMOfflineResourceList::UNCACHED;
    return NS_OK;
  }


  // If there is an update in process, use its status.
  if (mCacheUpdate && mExposeCacheUpdateStatus) {
    rv = mCacheUpdate->GetStatus(aStatus);
    if (NS_SUCCEEDED(rv) && *aStatus != nsIDOMOfflineResourceList::IDLE) {
      return NS_OK;
    }
  }

  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineResourceList::Update()
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsContentUtils::OfflineAppAllowed(mDocumentURI)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIOfflineCacheUpdateService> updateService =
    do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMWindow> window = 
    do_QueryInterface(GetOwner());

  nsCOMPtr<nsIOfflineCacheUpdate> update;
  rv = updateService->ScheduleUpdate(mManifestURI, mDocumentURI,
                                     window, getter_AddRefs(update));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
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
  mStatus = nsIDOMOfflineResourceList::IDLE;

  return NS_OK;
}

//
// nsDOMOfflineResourceList::nsIDOMEventTarget
//

NS_IMETHODIMP
nsDOMOfflineResourceList::GetOnchecking(nsIDOMEventListener **aOnchecking)
{
  return GetInnerEventListener(mOnCheckingListener, aOnchecking);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::SetOnchecking(nsIDOMEventListener *aOnchecking)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(CHECKING_STR),
                                mOnCheckingListener, aOnchecking);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::GetOnerror(nsIDOMEventListener **aOnerror)
{
  return GetInnerEventListener(mOnErrorListener, aOnerror);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::SetOnerror(nsIDOMEventListener *aOnerror)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(ERROR_STR), mOnErrorListener,
                                aOnerror);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::GetOnnoupdate(nsIDOMEventListener **aOnnoupdate)
{
  return GetInnerEventListener(mOnNoUpdateListener, aOnnoupdate);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::SetOnnoupdate(nsIDOMEventListener *aOnnoupdate)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(NOUPDATE_STR),
                                mOnNoUpdateListener, aOnnoupdate);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::GetOndownloading(nsIDOMEventListener **aOndownloading)
{
  return GetInnerEventListener(mOnDownloadingListener, aOndownloading);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::SetOndownloading(nsIDOMEventListener *aOndownloading)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(DOWNLOADING_STR),
                                mOnDownloadingListener, aOndownloading);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::GetOnprogress(nsIDOMEventListener **aOnprogress)
{
  return GetInnerEventListener(mOnProgressListener, aOnprogress);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::SetOnprogress(nsIDOMEventListener *aOnprogress)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(PROGRESS_STR),
                                mOnProgressListener, aOnprogress);
}


NS_IMETHODIMP
nsDOMOfflineResourceList::GetOnupdateready(nsIDOMEventListener **aOnupdateready)
{
  return GetInnerEventListener(mOnUpdateReadyListener, aOnupdateready);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::SetOncached(nsIDOMEventListener *aOncached)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(CACHED_STR),
                                mOnCachedListener, aOncached);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::GetOncached(nsIDOMEventListener **aOncached)
{
  return GetInnerEventListener(mOnCachedListener, aOncached);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::SetOnupdateready(nsIDOMEventListener *aOnupdateready)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(UPDATEREADY_STR),
                                mOnUpdateReadyListener, aOnupdateready);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::GetOnobsolete(nsIDOMEventListener **aOnobsolete)
{
  return GetInnerEventListener(mOnObsoleteListener, aOnobsolete);
}

NS_IMETHODIMP
nsDOMOfflineResourceList::SetOnobsolete(nsIDOMEventListener *aOnobsolete)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(OBSOLETE_STR),
                                mOnObsoleteListener, aOnobsolete);
}

void
nsDOMOfflineResourceList::FirePendingEvents()
{
  for (PRInt32 i = 0; i < mPendingEvents.Count(); ++i) {
    bool dummy;
    nsCOMPtr<nsIDOMEvent> event = mPendingEvents[i];
    DispatchEvent(event, &dummy);
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

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = nsEventDispatcher::CreateEvent(nullptr, nullptr,
                                               NS_LITERAL_STRING("Events"),
                                               getter_AddRefs(event));
  NS_ENSURE_SUCCESS(rv, rv);
  event->InitEvent(aEventName, false, true);

  // We assume anyone that managed to call SendEvent is trusted
  event->SetTrusted(true);

  // If the window is frozen or we're still catching up on events that were
  // queued while frozen, save the event for later.
  if (GetOwner()->IsFrozen() || mPendingEvents.Count() > 0) {
    mPendingEvents.AppendObject(event);
    return NS_OK;
  }

  bool dummy;
  DispatchEvent(event, &dummy);

  return NS_OK;
}


//
// nsDOMOfflineResourceList::nsIObserver
//
NS_IMETHODIMP
nsDOMOfflineResourceList::Observe(nsISupports *aSubject,
                                    const char *aTopic,
                                    const PRUnichar *aData)
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
                                     PRUint32 event)
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
      mStatus = nsIDOMOfflineResourceList::OBSOLETE;
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
  mAvailableApplicationCache = aApplicationCache;
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
    if (isUpgrade) {
      mStatus = nsIDOMOfflineResourceList::UPDATEREADY;
      SendEvent(NS_LITERAL_STRING(UPDATEREADY_STR));
    } else {
      mStatus = nsIDOMOfflineResourceList::IDLE;
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
  if (IS_CHILD_PROCESS()) 
    return NS_ERROR_NOT_IMPLEMENTED;

  if (mCachedKeys)
    return NS_OK;

  nsCOMPtr<nsIApplicationCache> appCache;
  mApplicationCacheService->GetActiveCache(mManifestSpec,
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
