/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerLoadContext_h__
#define mozilla_dom_workers_WorkerLoadContext_h__

#include "nsIInputStream.h"
#include "nsIRequest.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/Promise.h"
#include "js/loader/ScriptKind.h"
#include "js/loader/ScriptLoadRequest.h"
#include "js/loader/LoadContextBase.h"

class nsIReferrerInfo;
class nsIURI;

namespace mozilla::dom {

class ClientInfo;
class WorkerPrivate;

namespace workerinternals::loader {
class CacheCreator;
}

class WorkerLoadContext : public JS::loader::LoadContextBase {
 public:
  explicit WorkerLoadContext(const nsString& aURL);

  ~WorkerLoadContext() = default;

  nsString mURL;

  // This full URL string is populated only if this object is used in a
  // ServiceWorker.
  nsString mFullURL;

  // This promise is set only when the script is for a ServiceWorker but
  // it's not in the cache yet. The promise is resolved when the full body is
  // stored into the cache.  mCachePromise will be set to nullptr after
  // resolution.
  RefPtr<Promise> mCachePromise;

  // The reader stream the cache entry should be filled from, for those cases
  // when we're going to have an mCachePromise.
  nsCOMPtr<nsIInputStream> mCacheReadStream;

  nsresult mLoadResult = NS_ERROR_NOT_INITIALIZED;

  RefPtr<workerinternals::loader::CacheCreator> mCacheCreator;

  void ClearCacheCreator();

  void SetCacheCreator(
      RefPtr<workerinternals::loader::CacheCreator> aCacheCreator);

  RefPtr<workerinternals::loader::CacheCreator> GetCacheCreator();

  bool mExecutionScheduled = false;
  bool mExecutionResult = false;

  Maybe<nsString> mSourceMapURL;

  enum CacheStatus {
    // By default a normal script is just loaded from the network. But for
    // ServiceWorkers, we have to check if the cache contains the script and
    // load it from the cache.
    Uncached,

    WritingToCache,

    ReadingFromCache,

    // This script has been loaded from the ServiceWorker cache.
    Cached,

    // This script must be stored in the ServiceWorker cache.
    ToBeCached,

    // Something went wrong or the worker went away.
    Cancel
  };

  CacheStatus mCacheStatus = Uncached;

  Maybe<bool> mMutedErrorFlag;

  bool Finished() const { return mRequest->IsReadyToRun() && !mCachePromise; }
};

}  // namespace mozilla::dom
#endif /* mozilla_dom_workers_WorkerLoadContext_h__ */
