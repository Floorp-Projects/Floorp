/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Worker.h"

#include "MessageEventRunnable.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "WorkerPrivate.h"
#include "EventWithOptionsRunnable.h"
#include "js/RootingAPI.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsISupports.h"
#include "nsDebug.h"
#include "mozilla/dom/WorkerStatus.h"
#include "mozilla/RefPtr.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

namespace mozilla::dom {

/* static */
already_AddRefed<Worker> Worker::Constructor(const GlobalObject& aGlobal,
                                             const nsAString& aScriptURL,
                                             const WorkerOptions& aOptions,
                                             ErrorResult& aRv) {
  JSContext* cx = aGlobal.Context();

  nsCOMPtr<nsIGlobalObject> globalObject =
      do_QueryInterface(aGlobal.GetAsSupports());

  if (globalObject->GetAsInnerWindow() &&
      !globalObject->GetAsInnerWindow()->IsCurrentInnerWindow()) {
    aRv.ThrowInvalidStateError(
        "Cannot create worker for a going to be discarded document");
    return nullptr;
  }

  RefPtr<WorkerPrivate> workerPrivate = WorkerPrivate::Constructor(
      cx, aScriptURL, false /* aIsChromeWorker */, WorkerKindDedicated,
      aOptions.mCredentials, aOptions.mType, aOptions.mName, VoidCString(),
      nullptr /*aLoadInfo */, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<Worker> worker = new Worker(globalObject, workerPrivate.forget());
  return worker.forget();
}

Worker::Worker(nsIGlobalObject* aGlobalObject,
               already_AddRefed<WorkerPrivate> aWorkerPrivate)
    : DOMEventTargetHelper(aGlobalObject),
      mWorkerPrivate(std::move(aWorkerPrivate)) {
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->SetParentEventTargetRef(this);
}

Worker::~Worker() { Terminate(); }

JSObject* Worker::WrapObject(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) {
  JS::Rooted<JSObject*> wrapper(aCx,
                                Worker_Binding::Wrap(aCx, this, aGivenProto));
  if (wrapper) {
    // Most DOM objects don't assume they have a reflector. If they don't have
    // one and need one, they create it. But in workers code, we assume that the
    // reflector is always present.  In order to guarantee that it's always
    // present, we have to preserve it. Otherwise the GC will happily collect it
    // as needed.
    MOZ_ALWAYS_TRUE(TryPreserveWrapper(wrapper));
  }

  return wrapper;
}

void Worker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                         const Sequence<JSObject*>& aTransferable,
                         ErrorResult& aRv) {
  NS_ASSERT_OWNINGTHREAD(Worker);

  if (!mWorkerPrivate || mWorkerPrivate->ParentStatusProtected() > Running) {
    return;
  }
  RefPtr<WorkerPrivate> workerPrivate = mWorkerPrivate;
  Unused << workerPrivate;

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  NS_ConvertUTF16toUTF8 nameOrScriptURL(
      mWorkerPrivate->WorkerName().IsEmpty()
          ? Substring(
                mWorkerPrivate->ScriptURL(), 0,
                std::min(size_t(1024), mWorkerPrivate->ScriptURL().Length()))
          : Substring(
                mWorkerPrivate->WorkerName(), 0,
                std::min(size_t(1024), mWorkerPrivate->WorkerName().Length())));
  AUTO_PROFILER_MARKER_TEXT("Worker.postMessage", DOM, {}, nameOrScriptURL);
  uint32_t flags = uint32_t(js::ProfilingStackFrame::Flags::RELEVANT_FOR_JS);
  if (mWorkerPrivate->IsChromeWorker()) {
    flags |= uint32_t(js::ProfilingStackFrame::Flags::NONSENSITIVE);
  }
  mozilla::AutoProfilerLabel PROFILER_RAII(
      "Worker.postMessage", nameOrScriptURL.get(),
      JS::ProfilingCategoryPair::DOM, flags);

  RefPtr<MessageEventRunnable> runnable =
      new MessageEventRunnable(mWorkerPrivate, WorkerRunnable::WorkerThread);

  JS::CloneDataPolicy clonePolicy;
  // DedicatedWorkers are always part of the same agent cluster.
  clonePolicy.allowIntraClusterClonableSharedObjects();

  if (NS_IsMainThread()) {
    nsGlobalWindowInner* win = nsContentUtils::IncumbentInnerWindow();
    if (win && win->IsSharedMemoryAllowed()) {
      clonePolicy.allowSharedMemoryObjects();
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    if (worker && worker->IsSharedMemoryAllowed()) {
      clonePolicy.allowSharedMemoryObjects();
    }
  }

  runnable->Write(aCx, aMessage, transferable, clonePolicy, aRv);

  if (!mWorkerPrivate || mWorkerPrivate->ParentStatusProtected() > Running) {
    return;
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // The worker could have closed between the time we entered this function and
  // checked ParentStatusProtected and now, which could cause the dispatch to
  // fail.
  Unused << NS_WARN_IF(!runnable->Dispatch());
}

void Worker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                         const StructuredSerializeOptions& aOptions,
                         ErrorResult& aRv) {
  PostMessage(aCx, aMessage, aOptions.mTransfer, aRv);
}

void Worker::PostEventWithOptions(JSContext* aCx,
                                  JS::Handle<JS::Value> aOptions,
                                  const Sequence<JSObject*>& aTransferable,
                                  EventWithOptionsRunnable* aRunnable,
                                  ErrorResult& aRv) {
  NS_ASSERT_OWNINGTHREAD(Worker);

  if (NS_WARN_IF(!mWorkerPrivate ||
                 mWorkerPrivate->ParentStatusProtected() > Running)) {
    return;
  }
  RefPtr<WorkerPrivate> workerPrivate = mWorkerPrivate;
  Unused << workerPrivate;

  aRunnable->InitOptions(aCx, aOptions, aTransferable, aRv);

  if (NS_WARN_IF(!mWorkerPrivate ||
                 mWorkerPrivate->ParentStatusProtected() > Running)) {
    return;
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  Unused << NS_WARN_IF(!aRunnable->Dispatch());
}

void Worker::Terminate() {
  NS_ASSERT_OWNINGTHREAD(Worker);

  if (mWorkerPrivate) {
    mWorkerPrivate->Cancel();
    mWorkerPrivate = nullptr;
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Worker)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Worker, DOMEventTargetHelper)
  if (tmp->mWorkerPrivate) {
    tmp->mWorkerPrivate->Traverse(cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Worker, DOMEventTargetHelper)
  tmp->Terminate();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(Worker, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Worker)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Worker, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Worker, DOMEventTargetHelper)

}  // namespace mozilla::dom
