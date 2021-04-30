/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMOfflineResourceList.h"
#include "nsError.h"
#include "mozilla/Components.h"
#include "mozilla/dom/DOMStringList.h"
#include "nsMemory.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsICookieJarSettings.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsContentUtils.h"
#include "nsILoadContext.h"
#include "nsIObserverService.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebNavigation.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/OfflineResourceListBinding.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/StaticPrefs_browser.h"

#include "nsXULAppAPI.h"
#define IS_CHILD_PROCESS() (GeckoProcessType_Default != XRE_GetProcessType())

using namespace mozilla;
using namespace mozilla::dom;

// Event names

#define CHECKING_STR u"checking"
#define ERROR_STR u"error"
#define NOUPDATE_STR u"noupdate"
#define DOWNLOADING_STR u"downloading"
#define PROGRESS_STR u"progress"
#define CACHED_STR u"cached"
#define UPDATEREADY_STR u"updateready"
#define OBSOLETE_STR u"obsolete"

// To prevent abuse of the resource list for data storage, the number
// of offline urls and their length are limited.

static const char kMaxEntriesPref[] = "offline.max_site_resources";
#define DEFAULT_MAX_ENTRIES 100
#define MAX_URI_LENGTH 2048

//
// nsDOMOfflineResourceList
//

NS_IMPL_CYCLE_COLLECTION_WEAK_INHERITED(nsDOMOfflineResourceList,
                                        DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMOfflineResourceList)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsDOMOfflineResourceList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsDOMOfflineResourceList, DOMEventTargetHelper)

nsDOMOfflineResourceList::nsDOMOfflineResourceList(
    nsIURI* aManifestURI, nsIURI* aDocumentURI, nsIPrincipal* aLoadingPrincipal,
    nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow) {}

nsDOMOfflineResourceList::~nsDOMOfflineResourceList() { ClearCachedKeys(); }

JSObject* nsDOMOfflineResourceList::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return OfflineResourceList_Binding::Wrap(aCx, this, aGivenProto);
}

void nsDOMOfflineResourceList::Disconnect() {}

already_AddRefed<DOMStringList> nsDOMOfflineResourceList::GetMozItems(
    ErrorResult& aRv) {
  if (IS_CHILD_PROCESS()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  RefPtr<DOMStringList> items = new DOMStringList();
  return items.forget();
}

bool nsDOMOfflineResourceList::MozHasItem(const nsAString& aURI,
                                          ErrorResult& aRv) {
  return false;
}

uint32_t nsDOMOfflineResourceList::GetMozLength(ErrorResult& aRv) {
  if (IS_CHILD_PROCESS()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return 0;
  }

  return 0;
}

void nsDOMOfflineResourceList::MozItem(uint32_t aIndex, nsAString& aURI,
                                       ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_AVAILABLE);
}

void nsDOMOfflineResourceList::IndexedGetter(uint32_t aIndex, bool& aFound,
                                             nsAString& aURI,
                                             ErrorResult& aRv) {
  if (IS_CHILD_PROCESS()) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  aFound = false;
}

void nsDOMOfflineResourceList::MozAdd(const nsAString& aURI, ErrorResult& aRv) {
}

void nsDOMOfflineResourceList::MozRemove(const nsAString& aURI,
                                         ErrorResult& aRv) {}

uint16_t nsDOMOfflineResourceList::GetStatus(ErrorResult& aRv) {
  return OfflineResourceList_Binding::UNCACHED;
}

void nsDOMOfflineResourceList::Update(ErrorResult& aRv) {}

void nsDOMOfflineResourceList::SwapCache(ErrorResult& aRv) {}

void nsDOMOfflineResourceList::FirePendingEvents() {}

void nsDOMOfflineResourceList::SendEvent(const nsAString& aEventName) {}

//
// nsDOMOfflineResourceList::nsIObserver
//
NS_IMETHODIMP
nsDOMOfflineResourceList::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  return NS_OK;
}

nsresult nsDOMOfflineResourceList::GetCacheKey(const nsAString& aURI,
                                               nsCString& aKey) {
  nsCOMPtr<nsIURI> requestedURI;
  nsresult rv = NS_NewURI(getter_AddRefs(requestedURI), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetCacheKey(requestedURI, aKey);
}

nsresult nsDOMOfflineResourceList::GetCacheKey(nsIURI* aURI, nsCString& aKey) {
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

nsresult nsDOMOfflineResourceList::CacheKeys() { return NS_OK; }

void nsDOMOfflineResourceList::ClearCachedKeys() {}
