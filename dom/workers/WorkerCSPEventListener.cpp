/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerCSPEventListener.h"
#include "WorkerPrivate.h"
#include "WorkerRef.h"
#include "mozilla/dom/SecurityPolicyViolationEvent.h"
#include "mozilla/dom/SecurityPolicyViolationEventBinding.h"

using namespace mozilla::dom;

namespace {

class WorkerCSPEventRunnable final : public MainThreadWorkerRunnable
{
public:
  WorkerCSPEventRunnable(WorkerPrivate* aWorkerPrivate,
                         const nsAString& aJSON)
    : MainThreadWorkerRunnable(aWorkerPrivate)
    , mJSON(aJSON)
  {}

private:
  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    SecurityPolicyViolationEventInit violationEventInit;
    if (NS_WARN_IF(!violationEventInit.Init(mJSON))) {
      return true;
    }

    RefPtr<mozilla::dom::Event> event =
      mozilla::dom::SecurityPolicyViolationEvent::Constructor(
        aWorkerPrivate->GlobalScope(),
        NS_LITERAL_STRING("securitypolicyviolation"),
        violationEventInit);
    event->SetTrusted(true);

    aWorkerPrivate->GlobalScope()->DispatchEvent(*event);
    return true;
  }

  const nsString mJSON;
};

} // anonymous

NS_IMPL_ISUPPORTS(WorkerCSPEventListener, nsICSPEventListener)

/* static */ already_AddRefed<WorkerCSPEventListener>
WorkerCSPEventListener::Create(WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<WorkerCSPEventListener> listener = new WorkerCSPEventListener();

  listener->mWorkerRef = WeakWorkerRef::Create(aWorkerPrivate, [listener]() {
      MutexAutoLock lock(listener->mMutex);
      listener->mWorkerRef = nullptr;
    });

  if (NS_WARN_IF(!listener->mWorkerRef)) {
    return nullptr;
  }

  return listener.forget();
}

WorkerCSPEventListener::WorkerCSPEventListener()
  : mMutex("WorkerCSPEventListener::mMutex")
{}

NS_IMETHODIMP
WorkerCSPEventListener::OnCSPViolationEvent(const nsAString& aJSON)
{
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);
  if (!mWorkerRef) {
    return NS_OK;
  }

  WorkerPrivate* workerPrivate = mWorkerRef->GetUnsafePrivate();
  MOZ_ASSERT(workerPrivate);

  RefPtr<WorkerCSPEventRunnable> runnable =
    new WorkerCSPEventRunnable(workerPrivate, aJSON);
  runnable->Dispatch();

  return NS_OK;
}
