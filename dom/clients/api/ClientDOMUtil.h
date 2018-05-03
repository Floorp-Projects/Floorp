/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientDOMUtil_h
#define _mozilla_dom_ClientDOMUtil_h

#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ClientOpPromise.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
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
StartClientManagerOp(Func aFunc, const Arg& aArg, nsIGlobalObject* aGlobal,
                     Resolve aResolve, Reject aReject)
{
  MOZ_DIAGNOSTIC_ASSERT(aGlobal);

  nsCOMPtr<nsISerialEventTarget> target =
    aGlobal->EventTargetFor(TaskCategory::Other);

  auto holder = MakeRefPtr<DOMMozPromiseRequestHolder<ClientOpPromise>>(aGlobal);

  aFunc(aArg, target)->Then(
    target, __func__,
    [aResolve, holder](const ClientOpResult& aResult) {
      holder->Complete();
      aResolve(aResult);
    }, [aReject, holder](nsresult aResult) {
      holder->Complete();
      aReject(aResult);
    })->Track(*holder);
}

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientDOMUtil_h
