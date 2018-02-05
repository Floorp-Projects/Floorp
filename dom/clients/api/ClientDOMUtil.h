/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientDOMUtil_h
#define _mozilla_dom_ClientDOMUtil_h

#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ClientOpPromise.h"
#include "mozilla/dom/WorkerHolderToken.h"
#include "mozilla/dom/WorkerPrivate.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

// Utility method to properly execute a ClientManager operation.  It
// will properly hold a worker thread alive and avoid executing callbacks
// if the thread is shutting down.
template<typename Func, typename Arg, typename Resolve, typename Reject>
void
StartClientManagerOp(Func aFunc, const Arg& aArg, nsISerialEventTarget* aTarget,
                     Resolve aResolve, Reject aReject)
{
  RefPtr<WorkerHolderToken> token;
  if (!NS_IsMainThread()) {
    token = WorkerHolderToken::Create(GetCurrentThreadWorkerPrivate(),
                                      WorkerStatus::Closing);
  }

  RefPtr<ClientOpPromise> promise = aFunc(aArg, aTarget);
  promise->Then(aTarget, __func__,
    [aResolve, token](const ClientOpResult& aResult) {
      if (token && token->IsShuttingDown()) {
        return;
      }
      aResolve(aResult);
    }, [aReject, token](nsresult aResult) {
      if (token && token->IsShuttingDown()) {
        return;
      }
      aReject(aResult);
    });
}

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientDOMUtil_h
