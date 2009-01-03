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
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsIDOMEventTarget.h"
#include "nsDOMEvent.h"
#include "nsIObserver.h"
#include "nsIScriptContext.h"
#include "nsCycleCollectionParticipant.h"

class nsIDOMWindow;

class nsDOMOfflineResourceList : public nsIDOMOfflineResourceList,
                                 public nsIObserver,
                                 public nsIOfflineCacheUpdateObserver,
                                 public nsIDOMEventTarget,
                                 public nsSupportsWeakReference
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMOFFLINERESOURCELIST
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIOFFLINECACHEUPDATEOBSERVER
  NS_DECL_NSIDOMEVENTTARGET

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMOfflineResourceList,
                                           nsIDOMOfflineResourceList)

  nsDOMOfflineResourceList(nsIURI* aManifestURI,
                           nsIURI* aDocumentURI,
                           nsIDOMWindow* aWindow);
  virtual ~nsDOMOfflineResourceList();

  void FirePendingEvents();
  void Disconnect();

private:
  nsresult Init();

  void NotifyEventListeners(nsIDOMEventListener *aListener,
                            const nsCOMArray<nsIDOMEventListener>& aListeners,
                            nsIDOMEvent* aEvent);

  nsresult SendEvent(const nsAString &aEventName,
                     nsIDOMEventListener *aListener,
                     const nsCOMArray<nsIDOMEventListener> &aListeners);

  nsresult UpdateAdded(nsIOfflineCacheUpdate *aUpdate);
  nsresult UpdateCompleted(nsIOfflineCacheUpdate *aUpdate);

  already_AddRefed<nsIApplicationCacheContainer> GetDocumentAppCacheContainer();
  already_AddRefed<nsIApplicationCache> GetDocumentAppCache();

  nsresult GetCacheKey(const nsAString &aURI, nsCString &aKey);
  nsresult GetCacheKey(nsIURI *aURI, nsCString &aKey);

  nsresult CacheKeys();
  void ClearCachedKeys();

  PRBool mInitialized;

  nsCOMPtr<nsIURI> mManifestURI;
  // AsciiSpec of mManifestURI
  nsCString mManifestSpec;

  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIWeakReference> mWindow;
  nsCOMPtr<nsIApplicationCacheService> mApplicationCacheService;
  nsCOMPtr<nsIOfflineCacheUpdate> mCacheUpdate;

  // The set of dynamic keys for this application cache object.
  char **mCachedKeys;
  PRUint32 mCachedKeysCount;

  nsCOMArray<nsIDOMEventListener> mCheckingListeners;
  nsCOMArray<nsIDOMEventListener> mErrorListeners;
  nsCOMArray<nsIDOMEventListener> mNoUpdateListeners;
  nsCOMArray<nsIDOMEventListener> mDownloadingListeners;
  nsCOMArray<nsIDOMEventListener> mProgressListeners;
  nsCOMArray<nsIDOMEventListener> mCachedListeners;
  nsCOMArray<nsIDOMEventListener> mUpdateReadyListeners;
  nsCOMArray<nsIDOMEventListener> mObsoleteListeners;

  nsCOMPtr<nsIDOMEventListener> mOnCheckingListener;
  nsCOMPtr<nsIDOMEventListener> mOnErrorListener;
  nsCOMPtr<nsIDOMEventListener> mOnNoUpdateListener;
  nsCOMPtr<nsIDOMEventListener> mOnDownloadingListener;
  nsCOMPtr<nsIDOMEventListener> mOnProgressListener;
  nsCOMPtr<nsIDOMEventListener> mOnCachedListener;
  nsCOMPtr<nsIDOMEventListener> mOnUpdateReadyListener;
  nsCOMPtr<nsIDOMEventListener> mOnObsoleteListener;

  struct PendingEvent {
    nsCOMPtr<nsIDOMEvent> event;
    nsCOMPtr<nsIDOMEventListener> listener;
    nsCOMArray<nsIDOMEventListener> listeners;
  };

  nsTArray<PendingEvent> mPendingEvents;
};

#endif
