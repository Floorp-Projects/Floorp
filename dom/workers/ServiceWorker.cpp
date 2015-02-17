/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorker.h"

#include "nsPIDOMWindow.h"
#include "SharedWorker.h"
#include "WorkerPrivate.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"

#ifdef XP_WIN
#undef PostMessage
#endif

using mozilla::ErrorResult;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {
namespace workers {

bool
ServiceWorkerVisible(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.serviceWorkers.enabled", false);
  }

  ServiceWorkerGlobalScope* scope = nullptr;
  nsresult rv = UnwrapObject<prototypes::id::ServiceWorkerGlobalScope_workers,
                             mozilla::dom::ServiceWorkerGlobalScopeBinding_workers::NativeType>(aObj, scope);
  return NS_SUCCEEDED(rv);
}

ServiceWorker::ServiceWorker(nsPIDOMWindow* aWindow,
                             SharedWorker* aSharedWorker)
  : DOMEventTargetHelper(aWindow),
    mState(ServiceWorkerState::Installing),
    mSharedWorker(aSharedWorker)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mSharedWorker);
}

ServiceWorker::~ServiceWorker()
{
  AssertIsOnMainThread();
}

NS_IMPL_ADDREF_INHERITED(ServiceWorker, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorker, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServiceWorker)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorker, DOMEventTargetHelper,
                                   mSharedWorker)

JSObject*
ServiceWorker::WrapObject(JSContext* aCx)
{
  AssertIsOnMainThread();

  return ServiceWorkerBinding::Wrap(aCx, this);
}

void
ServiceWorker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                           const Optional<Sequence<JS::Value>>& aTransferable,
                           ErrorResult& aRv)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if (State() == ServiceWorkerState::Redundant) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  workerPrivate->PostMessage(aCx, aMessage, aTransferable, aRv);
}

WorkerPrivate*
ServiceWorker::GetWorkerPrivate() const
{
  // At some point in the future, this may be optimized to terminate a worker
  // that hasn't been used in a certain amount of time or when there is memory
  // pressure or similar.
  MOZ_ASSERT(mSharedWorker);
  return mSharedWorker->GetWorkerPrivate();
}

} // namespace workers
} // namespace dom
} // namespace mozilla
