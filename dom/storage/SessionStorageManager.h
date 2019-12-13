/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStorageManager_h
#define mozilla_dom_SessionStorageManager_h

#include "nsIDOMStorageManager.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "StorageObserver.h"

namespace mozilla {
namespace dom {

class ContentParent;
class KeyValuePair;
class SessionStorageCache;
class SessionStorageObserver;

// sessionStorage is a data store that's unique to each tab (i.e. top-level
// browsing context) and origin. Before the advent of Fission all the data
// for a given tab could be stored in a single content process; now each
// site-specific process stores only its portion of the data. As content
// processes terminate, their sessionStorage data needs to be saved in the
// parent process, in case the same origin appears again in the tab (e.g.
// by navigating an iframe element). Therefore SessionStorageManager
// objects exist in both the parent and content processes.
//
// Whenever a content process terminates it sends its sessionStorage data
// to the parent process (see SessionStorageService); whenever a content
// process navigates to an origin for the first time in a given tab, the
// parent process sends it the saved data. To avoid sending the data
// multiple times, the parent process maintains a table of content
// processes that already received it; this table is empty in content
// processes.
//
// Note: the current implementation is expected to be replaced by a new
// implementation using LSNG.
class SessionStorageManager final : public nsIDOMSessionStorageManager,
                                    public StorageObserverSink {
 public:
  explicit SessionStorageManager(RefPtr<BrowsingContext> aBrowsingContext);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMSTORAGEMANAGER
  NS_DECL_NSIDOMSESSIONSTORAGEMANAGER

  NS_DECL_CYCLE_COLLECTION_CLASS(SessionStorageManager)

  RefPtr<BrowsingContext> GetBrowsingContext() const {
    return mBrowsingContext;
  }

  void SendSessionStorageDataToParentProcess();
  void SendSessionStorageDataToContentProcess(ContentParent* aActor,
                                              nsIPrincipal* aPrincipal);

  void LoadSessionStorageData(ContentParent* aSource,
                              const nsACString& aOriginAttrs,
                              const nsACString& aOriginKey,
                              const nsTArray<KeyValuePair>& aDefaultData,
                              const nsTArray<KeyValuePair>& aSessionData);

 private:
  ~SessionStorageManager();

  // StorageObserverSink, handler to various chrome clearing notification
  nsresult Observe(const char* aTopic,
                   const nsAString& aOriginAttributesPattern,
                   const nsACString& aOriginScope) override;

  enum ClearStorageType {
    eAll,
    eSessionOnly,
  };

  void ClearStorages(ClearStorageType aType,
                     const OriginAttributesPattern& aPattern,
                     const nsACString& aOriginScope);

  nsresult GetSessionStorageCacheHelper(nsIPrincipal* aPrincipal,
                                        bool aMakeIfNeeded,
                                        SessionStorageCache* aCloneFrom,
                                        RefPtr<SessionStorageCache>* aRetVal);

  nsresult GetSessionStorageCacheHelper(const nsACString& aOriginAttrs,
                                        const nsACString& aOriginKey,
                                        bool aMakeIfNeeded,
                                        SessionStorageCache* aCloneFrom,
                                        RefPtr<SessionStorageCache>* aRetVal);

  struct OriginRecord {
    RefPtr<SessionStorageCache> mCache;
    nsTHashtable<nsUint64HashKey> mKnownTo;
  };

  OriginRecord* GetOriginRecord(const nsACString& aOriginAttrs,
                                const nsACString& aOriginKey,
                                bool aMakeIfNeeded,
                                SessionStorageCache* aCloneFrom);

  template <typename Actor>
  void SendSessionStorageCache(Actor* aActor, const nsACString& aOriginAttrs,
                               const nsACString& aOriginKey,
                               SessionStorageCache* aCache);

  using OriginKeyHashTable = nsClassHashtable<nsCStringHashKey, OriginRecord>;
  nsClassHashtable<nsCStringHashKey, OriginKeyHashTable> mOATable;

  RefPtr<SessionStorageObserver> mObserver;

  RefPtr<BrowsingContext> mBrowsingContext;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SessionStorageManager_h
