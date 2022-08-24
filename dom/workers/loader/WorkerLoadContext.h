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

/*
 * WorkerScriptLoadContext (for all workers)
 *
 * LoadContexts augment the loading of a ScriptLoadRequest. They
 * describe how a ScriptLoadRequests loading and evaluation needs to be
 * augmented, based on the information provided by the loading context. The
 * WorkerLoadContext has the following generic fields applied to all worker
 * ScriptLoadRequests (and primarily used for error handling):
 *
 *    * mLoadResult
 *        Used to store the result of a load. In particular, it is used for
 *        error handling when a load fails (for example, a malformed URI).
 *    * mMutedErrorFlag
 *        Set when we finish loading a script, and used to determine whether a
 *        given error is thrown or muted.
 *
 * The rest of the fields on this class focus on enabling the ServiceWorker
 * usecase, in particular -- using the Cache API to store the worker so that
 * in the case of (for example) a page refresh, the service worker itself is
 * persisted so that it can do other work. For more details see the
 * CacheLoadHandler.h file.
 *
 */

class WorkerLoadContext : public JS::loader::LoadContextBase {
 public:
  /* Worker Load Context Kinds
   *
   * A script that is loaded and run as a worker can be one of several species.
   * Each may have slightly different behavior, but they fall into roughly two
   * categories: the Main Worker Script (the script that triggers the first
   * load) and scripts that are attached to this main worker script.
   *
   * In the specification, the Main Worker Script is referred to as the "top
   * level script" and is defined here:
   * https://html.spec.whatwg.org/multipage/webappapis.html#fetching-scripts-is-top-level
   */

  enum Kind {
    // Indicates that the is-top-level bit is true. This may be a Classic script
    // or a Module script.
    MainScript,
    // We are importing a script from the worker via ImportScript. This may only
    // be a Classic script.
    ImportScript,
    // We have an attached debugger, and these should be treated specially and
    // not like a main script (regardless of their type). This is not part of
    // the specification.
    DebuggerScript
  };

  explicit WorkerLoadContext(Kind aKind, const Maybe<ClientInfo>& aClientInfo);

  ~WorkerLoadContext() = default;

  // Used to detect if the `is top-level` bit is set on a given module.
  bool IsTopLevel() {
    return mRequest->IsTopLevel() && (mKind == Kind::MainScript);
  };

  static Kind GetKind(bool isMainScript, bool isDebuggerScript) {
    if (isDebuggerScript) {
      return Kind::DebuggerScript;
    }
    if (isMainScript) {
      return Kind::MainScript;
    }
    return Kind::ImportScript;
  };

  /* These fields are used by all workers */
  Maybe<bool> mMutedErrorFlag;
  nsresult mLoadResult = NS_ERROR_NOT_INITIALIZED;
  bool mLoadingFinished = false;
  Kind mKind;
  Maybe<ClientInfo> mClientInfo;

  /* These fields are only used by service workers */
  /* TODO: Split out a ServiceWorkerLoadContext */
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

  RefPtr<workerinternals::loader::CacheCreator> mCacheCreator;

  void ClearCacheCreator();

  void SetCacheCreator(
      RefPtr<workerinternals::loader::CacheCreator> aCacheCreator);

  RefPtr<workerinternals::loader::CacheCreator> GetCacheCreator();

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

  bool IsAwaitingPromise() const { return bool(mCachePromise); }
};

}  // namespace mozilla::dom
#endif /* mozilla_dom_workers_WorkerLoadContext_h__ */
