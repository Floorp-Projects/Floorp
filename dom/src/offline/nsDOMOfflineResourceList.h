/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsDOMEvent.h"
#include "nsIObserver.h"
#include "nsIScriptContext.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsDOMEventTargetHelper.h"

class nsIDOMWindow;

class nsDOMOfflineResourceList : public nsDOMEventTargetHelper,
                                 public nsIDOMOfflineResourceList,
                                 public nsIObserver,
                                 public nsIOfflineCacheUpdateObserver,
                                 public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMOFFLINERESOURCELIST
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIOFFLINECACHEUPDATEOBSERVER

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMOfflineResourceList,
                                           nsDOMEventTargetHelper)

  nsDOMOfflineResourceList(nsIURI* aManifestURI,
                           nsIURI* aDocumentURI,
                           nsPIDOMWindow* aWindow);
  virtual ~nsDOMOfflineResourceList();

  void FirePendingEvents();
  void Disconnect();

  nsresult Init();

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
  nsCOMPtr<nsIApplicationCacheService> mApplicationCacheService;
  nsCOMPtr<nsIApplicationCache> mAvailableApplicationCache;
  nsCOMPtr<nsIOfflineCacheUpdate> mCacheUpdate;
  bool mExposeCacheUpdateStatus;
  PRUint16 mStatus;

  // The set of dynamic keys for this application cache object.
  char **mCachedKeys;
  PRUint32 mCachedKeysCount;

  nsRefPtr<nsDOMEventListenerWrapper> mOnCheckingListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnErrorListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnNoUpdateListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnDownloadingListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnProgressListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnCachedListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnUpdateReadyListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnObsoleteListener;

  nsCOMArray<nsIDOMEvent> mPendingEvents;
};

#endif
