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
#include "mozilla/TimelineConsumers.h"
#include "mozilla/Unused.h"
#include "mozilla/WorkerTimelineMarker.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowOuter.h"
#include "WorkerPrivate.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

namespace mozilla {
namespace dom {

/* static */
already_AddRefed<Worker> Worker::Constructor(const GlobalObject& aGlobal,
                                             const nsAString& aScriptURL,
                                             const WorkerOptions& aOptions,
                                             ErrorResult& aRv) {
  JSContext* cx = aGlobal.Context();

  nsCOMPtr<nsIGlobalObject> globalObject =
      do_QueryInterface(aGlobal.GetAsSupports());

  if (globalObject->AsInnerWindow() &&
      !globalObject->AsInnerWindow()->IsCurrentInnerWindow()) {
    aRv.ThrowInvalidStateError(
        "Cannot create worker for a going to be discarded document");
    return nullptr;
  }

  RefPtr<WorkerPrivate> workerPrivate = WorkerPrivate::Constructor(
      cx, aScriptURL, false /* aIsChromeWorker */, WorkerKindDedicated,
      aOptions.mName, VoidCString(), nullptr /*aLoadInfo */, aRv);
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

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  NS_ConvertUTF16toUTF8 nameOrScriptURL(mWorkerPrivate->WorkerName().IsEmpty()
                                            ? mWorkerPrivate->ScriptURL()
                                            : mWorkerPrivate->WorkerName());
  AUTO_PROFILER_MARKER_TEXT("Worker.postMessage", DOM, {}, nameOrScriptURL);
  uint32_t flags = uint32_t(js::ProfilingStackFrame::Flags::RELEVANT_FOR_JS);
  if (mWorkerPrivate->IsChromeWorker()) {
    flags |= uint32_t(js::ProfilingStackFrame::Flags::NONSENSITIVE);
  }
  mozilla::AutoProfilerLabel PROFILER_RAII(
      "Worker.postMessage", nameOrScriptURL.get(),
      JS::ProfilingCategoryPair::DOM, flags);

  RefPtr<MessageEventRunnable> runnable = new MessageEventRunnable(
      mWorkerPrivate, WorkerRunnable::WorkerThreadModifyBusyCount);

  UniquePtr<AbstractTimelineMarker> start;
  UniquePtr<AbstractTimelineMarker> end;
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  bool isTimelineRecording = timelines && !timelines->IsEmpty();

  if (isTimelineRecording) {
    start = MakeUnique<WorkerTimelineMarker>(
        NS_IsMainThread()
            ? ProfileTimelineWorkerOperationType::SerializeDataOnMainThread
            : ProfileTimelineWorkerOperationType::SerializeDataOffMainThread,
        MarkerTracingType::START);
  }

  JS::CloneDataPolicy clonePolicy;
  // DedicatedWorkers are always part of the same agent cluster.
  clonePolicy.allowIntraClusterClonableSharedObjects();

  if (NS_IsMainThread()) {
    nsGlobalWindowInner* win = nsContentUtils::CallerInnerWindow();
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

  if (isTimelineRecording) {
    end = MakeUnique<WorkerTimelineMarker>(
        NS_IsMainThread()
            ? ProfileTimelineWorkerOperationType::SerializeDataOnMainThread
            : ProfileTimelineWorkerOperationType::SerializeDataOffMainThread,
        MarkerTracingType::END);
    timelines->AddMarkerForAllObservedDocShells(start);
    timelines->AddMarkerForAllObservedDocShells(end);
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

}  // namespace dom
}  // namespace mozilla
