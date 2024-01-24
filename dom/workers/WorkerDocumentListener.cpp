/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerDocumentListener.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "nsGlobalWindowInner.h"

namespace mozilla::dom {

WorkerDocumentListener::WorkerDocumentListener()
    : mMutex("mozilla::dom::WorkerDocumentListener::mMutex") {}

WorkerDocumentListener::~WorkerDocumentListener() = default;

RefPtr<WorkerDocumentListener> WorkerDocumentListener::Create(
    WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  auto listener = MakeRefPtr<WorkerDocumentListener>();

  RefPtr<StrongWorkerRef> strongWorkerRef =
      StrongWorkerRef::Create(aWorkerPrivate, "WorkerDocumentListener",
                              [listener]() { listener->Destroy(); });
  if (NS_WARN_IF(!strongWorkerRef)) {
    return nullptr;
  }

  listener->mWorkerRef = new ThreadSafeWorkerRef(strongWorkerRef);
  uint64_t windowID = aWorkerPrivate->WindowID();

  aWorkerPrivate->DispatchToMainThread(NS_NewRunnableFunction(
      "WorkerDocumentListener::Create",
      [listener, windowID] { listener->SetListening(windowID, true); }));

  return listener;
}

void WorkerDocumentListener::OnVisible(bool aVisible) {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mMutex);
  if (!mWorkerRef) {
    // We haven't handled the runnable to release this yet.
    return;
  }

  class VisibleRunnable final : public WorkerRunnable {
   public:
    VisibleRunnable(WorkerPrivate* aWorkerPrivate, bool aVisible)
        : WorkerRunnable(aWorkerPrivate, "VisibleRunnable"),
          mVisible(aVisible) {}

    bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) {
      WorkerGlobalScope* scope = aWorkerPrivate->GlobalScope();
      MOZ_ASSERT(scope);
      scope->OnDocumentVisible(mVisible);
      return true;
    }

   private:
    const bool mVisible;
  };

  auto runnable = MakeRefPtr<VisibleRunnable>(mWorkerRef->Private(), aVisible);
  runnable->Dispatch();
}

void WorkerDocumentListener::SetListening(uint64_t aWindowID, bool aListen) {
  MOZ_ASSERT(NS_IsMainThread());

  auto* window = nsGlobalWindowInner::GetInnerWindowWithId(aWindowID);
  Document* doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    // This would typically happen during shutdown if there is an active worker
    // listening for document events. The Document may already be freed when we
    // try to deregister for notifications.
    return;
  }

  if (aListen) {
    doc->AddWorkerDocumentListener(this);
  } else {
    doc->RemoveWorkerDocumentListener(this);
  }
}

void WorkerDocumentListener::Destroy() {
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(mWorkerRef);
  WorkerPrivate* workerPrivate = mWorkerRef->Private();
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  uint64_t windowID = workerPrivate->WindowID();
  workerPrivate->DispatchToMainThread(NS_NewRunnableFunction(
      "WorkerDocumentListener::Destroy", [self = RefPtr{this}, windowID] {
        self->SetListening(windowID, false);
      }));
  mWorkerRef = nullptr;
}

}  // namespace mozilla::dom
