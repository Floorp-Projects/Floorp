/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_ScriptResponseHeaderProcessor_h__
#define mozilla_dom_workers_ScriptResponseHeaderProcessor_h__

#include "mozilla/dom/WorkerCommon.h"

#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIStreamLoader.h"
#include "nsStreamUtils.h"
#include "mozilla/StaticPrefs_browser.h"

namespace mozilla::dom {

class WorkerPrivate;

namespace workerinternals::loader {

/* ScriptResponseHeaderProcessor
 *
 * This class handles Policy headers. It can be used as a RequestObserver in a
 * Tee, as it is for NetworkLoadHandler in WorkerScriptLoader, or the static
 * method can be called directly, as it is in CacheLoadHandler.
 *
 */

class ScriptResponseHeaderProcessor final : public nsIRequestObserver {
 public:
  NS_DECL_ISUPPORTS

  ScriptResponseHeaderProcessor(WorkerPrivate* aWorkerPrivate,
                                bool aIsMainScript)
      : mWorkerPrivate(aWorkerPrivate), mIsMainScript(aIsMainScript) {
    AssertIsOnMainThread();
  }

  NS_IMETHOD OnStartRequest(nsIRequest* aRequest) override {
    if (!StaticPrefs::browser_tabs_remote_useCrossOriginEmbedderPolicy()) {
      return NS_OK;
    }

    nsresult rv = ProcessCrossOriginEmbedderPolicyHeader(aRequest);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRequest->Cancel(rv);
    }

    return rv;
  }

  NS_IMETHOD OnStopRequest(nsIRequest* aRequest,
                           nsresult aStatusCode) override {
    return NS_OK;
  }

  static nsresult ProcessCrossOriginEmbedderPolicyHeader(
      WorkerPrivate* aWorkerPrivate,
      nsILoadInfo::CrossOriginEmbedderPolicy aPolicy, bool aIsMainScript);

 private:
  ~ScriptResponseHeaderProcessor() = default;

  nsresult ProcessCrossOriginEmbedderPolicyHeader(nsIRequest* aRequest);

  WorkerPrivate* const mWorkerPrivate;
  const bool mIsMainScript;
};

}  // namespace workerinternals::loader

}  // namespace mozilla::dom

#endif /* mozilla_dom_workers_ScriptResponseHeaderProcessor_h__ */
