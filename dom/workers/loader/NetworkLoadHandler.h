/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_NetworkLoadHandler_h__
#define mozilla_dom_workers_NetworkLoadHandler_h__

#include "nsIStreamLoader.h"
#include "mozilla/dom/ScriptLoadInfo.h"

namespace mozilla {
namespace dom {

namespace workerinternals::loader {

class WorkerScriptLoader;

class NetworkLoadHandler final : public nsIStreamLoaderObserver,
                                 public nsIRequestObserver {
 public:
  NS_DECL_ISUPPORTS

  NetworkLoadHandler(WorkerScriptLoader* aLoader, ScriptLoadInfo& aLoadInfo);

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
  WorkerPrivate* const mWorkerPrivate;
  ScriptLoadInfo& mLoadInfo;
};

}  // namespace workerinternals::loader

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_workers_NetworkLoadHandler_h__ */
