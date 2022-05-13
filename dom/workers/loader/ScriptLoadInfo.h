/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_ScriptLoadInfo_h__
#define mozilla_dom_workers_ScriptLoadInfo_h__

#include "nsIRequest.h"
#include "mozilla/dom/Promise.h"
#include "nsIInputStream.h"
#include "nsIChannel.h"

class nsIChannel;
class nsIReferrerInfo;
class nsIURI;

namespace mozilla::dom {

class ClientInfo;
class WorkerPrivate;

struct ScriptLoadInfo {
  ScriptLoadInfo() {
    MOZ_ASSERT(mScriptIsUTF8 == false, "set by member initializer");
    MOZ_ASSERT(mScriptLength == 0, "set by member initializer");
    mScript.mUTF16 = nullptr;
  }

  ~ScriptLoadInfo() {
    if (void* data = mScriptIsUTF8 ? static_cast<void*>(mScript.mUTF8)
                                   : static_cast<void*>(mScript.mUTF16)) {
      js_free(data);
    }
  }

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

  nsCOMPtr<nsIChannel> mChannel;
  nsresult mLoadResult = NS_ERROR_NOT_INITIALIZED;

  // If |mScriptIsUTF8|, then |mUTF8| is active, otherwise |mUTF16| is active.
  union {
    char16_t* mUTF16;
    Utf8Unit* mUTF8;
  } mScript;
  size_t mScriptLength = 0;  // in code units
  bool mScriptIsUTF8 = false;

  bool ScriptTextIsNull() const {
    return mScriptIsUTF8 ? mScript.mUTF8 == nullptr : mScript.mUTF16 == nullptr;
  }

  void InitUTF8Script() {
    MOZ_ASSERT(ScriptTextIsNull());
    MOZ_ASSERT(mScriptLength == 0);

    mScriptIsUTF8 = true;
    mScript.mUTF8 = nullptr;
    mScriptLength = 0;
  }

  void InitUTF16Script() {
    MOZ_ASSERT(ScriptTextIsNull());
    MOZ_ASSERT(mScriptLength == 0);

    mScriptIsUTF8 = false;
    mScript.mUTF16 = nullptr;
    mScriptLength = 0;
  }

  bool mLoadingFinished = false;
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

  bool Finished() const {
    return mLoadingFinished && !mCachePromise && !mChannel;
  }
};

}  // namespace mozilla::dom
#endif /* mozilla_dom_workers_ScriptLoadInfo_h__ */
