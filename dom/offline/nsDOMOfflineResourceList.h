/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMOfflineResourceList_h___
#define nsDOMOfflineResourceList_h___

#include "nscore.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIApplicationCacheService.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsCOMArray.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsIScriptContext.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {
class DOMStringList;
class Event;
} // namespace dom
} // namespace mozilla

class nsDOMOfflineResourceList final : public mozilla::DOMEventTargetHelper,
                                       public nsIDOMOfflineResourceList,
                                       public nsIObserver,
                                       public nsIOfflineCacheUpdateObserver,
                                       public nsSupportsWeakReference
{
  typedef mozilla::ErrorResult ErrorResult;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMOFFLINERESOURCELIST
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIOFFLINECACHEUPDATEOBSERVER

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMOfflineResourceList,
                                           mozilla::DOMEventTargetHelper)

  nsDOMOfflineResourceList(nsIURI* aManifestURI,
                           nsIURI* aDocumentURI,
                           nsIPrincipal* aLoadingPrincipal,
                           nsPIDOMWindowInner* aWindow);

  void FirePendingEvents();
  void Disconnect();

  nsresult Init();

  nsPIDOMWindowInner* GetParentObject() const
  {
    return GetOwner();
  }
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint16_t GetStatus(ErrorResult& aRv)
  {
    uint16_t status = 0;
    aRv = GetStatus(&status);
    return status;
  }
  void Update(ErrorResult& aRv)
  {
    aRv = Update();
  }
  void SwapCache(ErrorResult& aRv)
  {
    aRv = SwapCache();
  }

  IMPL_EVENT_HANDLER(checking)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(noupdate)
  IMPL_EVENT_HANDLER(downloading)
  IMPL_EVENT_HANDLER(progress)
  IMPL_EVENT_HANDLER(cached)
  IMPL_EVENT_HANDLER(updateready)
  IMPL_EVENT_HANDLER(obsolete)

  already_AddRefed<mozilla::dom::DOMStringList> GetMozItems(ErrorResult& aRv);
  bool MozHasItem(const nsAString& aURI, ErrorResult& aRv);
  uint32_t GetMozLength(ErrorResult& aRv);
  void MozItem(uint32_t aIndex, nsAString& aURI, ErrorResult& aRv);
  void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aURI,
                     ErrorResult& aRv);
  uint32_t Length()
  {
    mozilla::IgnoredErrorResult rv;
    uint32_t length = GetMozLength(rv);
    return rv.Failed() ? 0 : length;
  }
  void MozAdd(const nsAString& aURI, ErrorResult& aRv);
  void MozRemove(const nsAString& aURI, ErrorResult& aRv)
  {
    aRv = MozRemove(aURI);
  }

protected:
  virtual ~nsDOMOfflineResourceList();

private:
  nsresult SendEvent(const nsAString &aEventName);

  nsresult UpdateAdded(nsIOfflineCacheUpdate *aUpdate);
  nsresult UpdateCompleted(nsIOfflineCacheUpdate *aUpdate);

  already_AddRefed<nsIApplicationCacheContainer> GetDocumentAppCacheContainer();
  already_AddRefed<nsIApplicationCache> GetDocumentAppCache();

  nsresult GetCacheKey(const nsAString &aURI, nsCString &aKey);
  nsresult GetCacheKey(nsIURI *aURI, nsCString &aKey);

  nsresult CacheKeys();
  void ClearCachedKeys();

  bool mInitialized;

  nsCOMPtr<nsIURI> mManifestURI;
  // AsciiSpec of mManifestURI
  nsCString mManifestSpec;

  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
  nsCOMPtr<nsIApplicationCacheService> mApplicationCacheService;
  nsCOMPtr<nsIApplicationCache> mAvailableApplicationCache;
  nsCOMPtr<nsIOfflineCacheUpdate> mCacheUpdate;
  bool mExposeCacheUpdateStatus;
  uint16_t mStatus;

  // The set of dynamic keys for this application cache object.
  char **mCachedKeys;
  uint32_t mCachedKeysCount;

  nsCOMArray<mozilla::dom::Event> mPendingEvents;
};

#endif
