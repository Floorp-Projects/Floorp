/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_NetworkLoadHandler_h__
#define mozilla_dom_workers_NetworkLoadHandler_h__

#include "nsIStreamLoader.h"
#include "mozilla/dom/WorkerLoadContext.h"
#include "mozilla/dom/ScriptLoadHandler.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom::workerinternals::loader {

class WorkerScriptLoader;

/*
 * [DOMDOC] NetworkLoadHandler for Workers
 *
 * A LoadHandler is a ScriptLoader helper class that reacts to an
 * nsIStreamLoader's events for
 * loading JS scripts. It is primarily responsible for decoding the stream into
 * UTF8 or UTF16. Additionally, it takes care of any work that needs to follow
 * the completion of a stream. Every LoadHandler also manages additional tasks
 * for the type of load that it is doing.
 *
 * As part of worker loading we have an number of tasks that we need to take
 * care of after a successfully completed stream, including setting a final URI
 * on the WorkerPrivate if we have loaded a main script, or handling CSP issues.
 * These are handled in DataReceivedFromNetwork, and implement roughly the same
 * set of tasks as you will find in the CacheLoadhandler, which has a companion
 * method DataReceivedFromcache.
 *
 * In the worker context, the LoadHandler is run on the main thread, and all
 * work in this file ultimately is done by the main thread, including decoding.
 *
 */

class NetworkLoadHandler final : public nsIStreamLoaderObserver,
                                 public nsIRequestObserver {
 public:
  NS_DECL_ISUPPORTS

  NetworkLoadHandler(WorkerScriptLoader* aLoader,
                     JS::loader::ScriptLoadRequest* aRequest);

  NS_IMETHOD
  OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                   nsresult aStatus, uint32_t aStringLen,
                   const uint8_t* aString) override;

  nsresult DataReceivedFromNetwork(nsIStreamLoader* aLoader, nsresult aStatus,
                                   uint32_t aStringLen, const uint8_t* aString);

  NS_IMETHOD
  OnStartRequest(nsIRequest* aRequest) override;

  nsresult PrepareForRequest(nsIRequest* aRequest);

  NS_IMETHOD
  OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) override {
    // Nothing to do here!
    return NS_OK;
  }

 private:
  ~NetworkLoadHandler() = default;

  RefPtr<WorkerScriptLoader> mLoader;
  UniquePtr<ScriptDecoder> mDecoder;
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
  WorkerLoadContext* mLoadContext;
};

}  // namespace mozilla::dom::workerinternals::loader

#endif /* mozilla_dom_workers_NetworkLoadHandler_h__ */
