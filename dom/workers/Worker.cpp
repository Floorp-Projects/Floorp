/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Worker.h"

#include "MessageEventRunnable.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/TimelineConsumers.h"
#include "mozilla/WorkerTimelineMarker.h"
#include "nsContentUtils.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<Worker>
Worker::Constructor(const GlobalObject& aGlobal, const nsAString& aScriptURL,
                    const WorkerOptions& aOptions, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();

  RefPtr<WorkerPrivate> workerPrivate =
    WorkerPrivate::Constructor(cx, aScriptURL, false /* aIsChromeWorker */,
                               WorkerTypeDedicated, aOptions.mName,
                               VoidCString(), nullptr /*aLoadInfo */, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> globalObject =
    do_QueryInterface(aGlobal.GetAsSupports());

  RefPtr<Worker> worker = new Worker(globalObject, workerPrivate.forget());
  return worker.forget();
}

Worker::Worker(nsIGlobalObject* aGlobalObject,
               already_AddRefed<WorkerPrivate> aWorkerPrivate)
  : DOMEventTargetHelper(aGlobalObject)
  , mWorkerPrivate(std::move(aWorkerPrivate))
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->SetParentEventTargetRef(this);
}

Worker::~Worker()
{
  Terminate();
}

JSObject*
Worker::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
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

void
Worker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                    const Sequence<JSObject*>& aTransferable,
                    ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(Worker);

  if (!mWorkerPrivate ||
      mWorkerPrivate->ParentStatusProtected() > Running) {
    return;
  }

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  RefPtr<MessageEventRunnable> runnable =
    new MessageEventRunnable(mWorkerPrivate,
                             WorkerRunnable::WorkerThreadModifyBusyCount);

  UniquePtr<AbstractTimelineMarker> start;
  UniquePtr<AbstractTimelineMarker> end;
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  bool isTimelineRecording = timelines && !timelines->IsEmpty();

  if (isTimelineRecording) {
    start = MakeUnique<WorkerTimelineMarker>(NS_IsMainThread()
      ? ProfileTimelineWorkerOperationType::SerializeDataOnMainThread
      : ProfileTimelineWorkerOperationType::SerializeDataOffMainThread,
      MarkerTracingType::START);
  }

  runnable->Write(aCx, aMessage, transferable, JS::CloneDataPolicy(), aRv);

  if (isTimelineRecording) {
    end = MakeUnique<WorkerTimelineMarker>(NS_IsMainThread()
      ? ProfileTimelineWorkerOperationType::SerializeDataOnMainThread
      : ProfileTimelineWorkerOperationType::SerializeDataOffMainThread,
      MarkerTracingType::END);
    timelines->AddMarkerForAllObservedDocShells(start);
    timelines->AddMarkerForAllObservedDocShells(end);
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (!runnable->Dispatch()) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

void
Worker::Terminate()
{
  NS_ASSERT_OWNINGTHREAD(Worker);

  if (mWorkerPrivate) {
    mWorkerPrivate->Terminate();
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
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(Worker, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Worker)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Worker, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Worker, DOMEventTargetHelper)

} // dom namespace
} // mozilla namespace
