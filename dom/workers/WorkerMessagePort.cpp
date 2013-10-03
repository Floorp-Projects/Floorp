/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerMessagePort.h"

#include "DOMBindingInlines.h"
#include "Events.h"
#include "WorkerPrivate.h"

using mozilla::dom::Optional;
using mozilla::dom::Sequence;

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::workers::events;

namespace {

bool
DispatchMessageEvent(JSContext* aCx, JS::HandleObject aMessagePort,
                     JSAutoStructuredCloneBuffer& aBuffer,
                     nsTArray<nsCOMPtr<nsISupports>>& aClonedObjects)
{
  MOZ_ASSERT(aMessagePort);

  JSAutoStructuredCloneBuffer buffer;
  aBuffer.swap(buffer);

  nsTArray<nsCOMPtr<nsISupports>> clonedObjects;
  aClonedObjects.SwapElements(clonedObjects);

  JS::Rooted<JSObject*> event(aCx,
    CreateMessageEvent(aCx, buffer, clonedObjects, false));
  if (!event) {
    return false;
  }

  bool dummy;
  return DispatchEventToTarget(aCx, aMessagePort, event, &dummy);
}


class QueuedMessageEventRunnable : public WorkerRunnable
{
  JSAutoStructuredCloneBuffer mBuffer;
  nsTArray<nsCOMPtr<nsISupports>> mClonedObjects;
  nsRefPtr<WorkerMessagePort> mMessagePort;
  JSObject* mMessagePortObject;

public:
  QueuedMessageEventRunnable(WorkerPrivate* aWorkerPrivate,
                             JSAutoStructuredCloneBuffer& aBuffer,
                             nsTArray<nsCOMPtr<nsISupports>>& aClonedObjects)
  : WorkerRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount,
                   RunWhenClearing),
    mMessagePortObject(nullptr)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    mBuffer.swap(aBuffer);
    mClonedObjects.SwapElements(aClonedObjects);
  }

  bool
  Hold(JSContext* aCx, WorkerMessagePort* aMessagePort)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aMessagePort);
    MOZ_ASSERT(aMessagePort->GetJSObject());
    MOZ_ASSERT(!mMessagePortObject);

    if (!JS_AddNamedObjectRoot(aCx, &mMessagePortObject,
                               "WorkerMessagePort::MessageEventRunnable::"
                               "mMessagePortObject")) {
      return false;
    }

    mMessagePortObject = aMessagePort->GetJSObject();
    mMessagePort = aMessagePort;
    return true;
  }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    JS::Rooted<JSObject*> messagePortObject(aCx, mMessagePortObject);
    mMessagePortObject = nullptr;

    JS_RemoveObjectRoot(aCx, &mMessagePortObject);

    nsRefPtr<WorkerMessagePort> messagePort;
    mMessagePort.swap(messagePort);

    return DispatchMessageEvent(aCx, messagePortObject, mBuffer,
                                mClonedObjects);
  }
};

} // anonymous namespace

void
WorkerMessagePort::_trace(JSTracer* aTrc)
{
  EventTarget::_trace(aTrc);
}

void
WorkerMessagePort::_finalize(JSFreeOp* aFop)
{
  EventTarget::_finalize(aFop);
}

void
WorkerMessagePort::PostMessage(
                             JSContext* /* aCx */, JS::HandleValue aMessage,
                             const Optional<Sequence<JS::Value>>& aTransferable,
                             ErrorResult& aRv)
{
  if (mClosed) {
    aRv = NS_ERROR_DOM_INVALID_STATE_ERR;
    return;
  }

  JSContext* cx = GetJSContext();

  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);
  MOZ_ASSERT(workerPrivate);

  workerPrivate->PostMessageToParentMessagePort(cx, Serial(), aMessage,
                                                aTransferable, aRv);
}

void
WorkerMessagePort::Start()
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(GetJSContext());
  MOZ_ASSERT(workerPrivate);

  if (mClosed) {
    NS_WARNING("Called start() after calling close()!");
    return;
  }

  if (mStarted) {
    return;
  }

  mStarted = true;

  if (!mQueuedMessages.IsEmpty()) {
    for (uint32_t index = 0; index < mQueuedMessages.Length(); index++) {
      MessageInfo& info = mQueuedMessages[index];

      nsRefPtr<QueuedMessageEventRunnable> runnable =
        new QueuedMessageEventRunnable(workerPrivate, info.mBuffer,
                                       info.mClonedObjects);

      JSContext* cx = GetJSContext();

      if (!runnable->Hold(cx, this) ||
          !runnable->Dispatch(cx)) {
        NS_WARNING("Failed to dispatch queued event!");
        break;
      }
    }
    mQueuedMessages.Clear();
  }
}

bool
WorkerMessagePort::MaybeDispatchEvent(
                                JSContext* aCx,
                                JSAutoStructuredCloneBuffer& aBuffer,
                                nsTArray<nsCOMPtr<nsISupports>>& aClonedObjects)
{
  if (mClosed) {
    NS_WARNING("Not going to ever run this event!");
    aBuffer.clear();
    aClonedObjects.Clear();
    return true;
  }

  if (!mStarted) {
    // Queue the message for later.
    MessageInfo* info = mQueuedMessages.AppendElement();
    info->mBuffer.swap(aBuffer);
    info->mClonedObjects.SwapElements(aClonedObjects);
    return true;
  }

  // Go ahead and dispatch the event.
  JS::Rooted<JSObject*> target(aCx, GetJSObject());
  return DispatchMessageEvent(aCx, target, aBuffer, aClonedObjects);
}

void
WorkerMessagePort::CloseInternal()
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(GetJSContext());
  MOZ_ASSERT(workerPrivate);
  MOZ_ASSERT(!IsClosed());
  MOZ_ASSERT_IF(mStarted, mQueuedMessages.IsEmpty());

  mClosed = true;

  workerPrivate->DisconnectMessagePort(Serial());

  if (!mStarted) {
    mQueuedMessages.Clear();
  }
}
