/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePort.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsIDOMEvent.h"

#include "SharedWorker.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

using mozilla::dom::EventHandlerNonNull;
using mozilla::dom::MessagePortBase;
using mozilla::dom::Optional;
using mozilla::dom::Sequence;
using mozilla::dom::AutoNoJSAPI;
using namespace mozilla;

USING_WORKERS_NAMESPACE

namespace {

class DelayedEventRunnable MOZ_FINAL : public WorkerRunnable
{
  nsRefPtr<MessagePort> mMessagePort;
  nsTArray<nsCOMPtr<nsIDOMEvent>> mEvents;

public:
  DelayedEventRunnable(WorkerPrivate* aWorkerPrivate,
                       TargetAndBusyBehavior aBehavior,
                       MessagePort* aMessagePort,
                       nsTArray<nsCOMPtr<nsIDOMEvent>>& aEvents)
  : WorkerRunnable(aWorkerPrivate, aBehavior), mMessagePort(aMessagePort)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aMessagePort);
    MOZ_ASSERT(aEvents.Length());

    mEvents.SwapElements(aEvents);
  }

  bool PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    if (mBehavior == WorkerThreadModifyBusyCount) {
      return aWorkerPrivate->ModifyBusyCount(aCx, true);
    }

    return true;
  }

  void PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                    bool aDispatchResult)
  {
    if (!aDispatchResult) {
      if (mBehavior == WorkerThreadModifyBusyCount) {
        aWorkerPrivate->ModifyBusyCount(aCx, false);
      }
      if (aCx) {
        JS_ReportPendingException(aCx);
      }
    }
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate);
};

} // anonymous namespace

MessagePort::MessagePort(nsPIDOMWindow* aWindow, SharedWorker* aSharedWorker,
                         uint64_t aSerial)
: MessagePortBase(aWindow), mSharedWorker(aSharedWorker),
  mWorkerPrivate(nullptr), mSerial(aSerial), mStarted(false)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aSharedWorker);
  SetIsDOMBinding();
}

MessagePort::MessagePort(WorkerPrivate* aWorkerPrivate, uint64_t aSerial)
: mWorkerPrivate(aWorkerPrivate), mSerial(aSerial), mStarted(false)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  SetIsDOMBinding();
}

MessagePort::~MessagePort()
{
  Close();
}

void
MessagePort::PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                            const Optional<Sequence<JS::Value>>& aTransferable,
                            ErrorResult& aRv)
{
  AssertCorrectThread();

  if (IsClosed()) {
    aRv = NS_ERROR_DOM_INVALID_STATE_ERR;
    return;
  }

  if (mSharedWorker) {
    mSharedWorker->PostMessage(aCx, aMessage, aTransferable, aRv);
  }
  else {
    mWorkerPrivate->PostMessageToParentMessagePort(aCx, Serial(), aMessage,
                                                   aTransferable, aRv);
  }
}

void
MessagePort::Start()
{
  AssertCorrectThread();

  if (IsClosed()) {
    NS_WARNING("Called start() after calling close()!");
    return;
  }

  if (mStarted) {
    return;
  }

  mStarted = true;

  if (!mQueuedEvents.IsEmpty()) {
    WorkerPrivate* workerPrivate;
    WorkerRunnable::TargetAndBusyBehavior behavior;

    if (mWorkerPrivate) {
      workerPrivate = mWorkerPrivate;
      behavior = WorkerRunnable::WorkerThreadModifyBusyCount;
    }
    else {
      workerPrivate = mSharedWorker->GetWorkerPrivate();
      MOZ_ASSERT(workerPrivate);

      behavior = WorkerRunnable::ParentThreadUnchangedBusyCount;
    }

    nsRefPtr<DelayedEventRunnable> runnable =
      new DelayedEventRunnable(workerPrivate, behavior, this, mQueuedEvents);
    runnable->Dispatch(nullptr);
  }
}

void
MessagePort::Close()
{
  AssertCorrectThread();

  if (!IsClosed()) {
    CloseInternal();
  }
}

void
MessagePort::QueueEvent(nsIDOMEvent* aEvent)
{
  AssertCorrectThread();
  MOZ_ASSERT(aEvent);
  MOZ_ASSERT(!IsClosed());
  MOZ_ASSERT(!mStarted);

  mQueuedEvents.AppendElement(aEvent);
}

EventHandlerNonNull*
MessagePort::GetOnmessage()
{
  AssertCorrectThread();

  return NS_IsMainThread() ? GetEventHandler(nsGkAtoms::onmessage, EmptyString())
                           : GetEventHandler(nullptr, NS_LITERAL_STRING("message"));
}

void
MessagePort::SetOnmessage(EventHandlerNonNull* aCallback)
{
  AssertCorrectThread();

  if (NS_IsMainThread()) {
    SetEventHandler(nsGkAtoms::onmessage, EmptyString(), aCallback);
  }
  else {
    SetEventHandler(nullptr, NS_LITERAL_STRING("message"), aCallback);
  }

  Start();
}

already_AddRefed<MessagePortBase>
MessagePort::Clone()
{
  NS_WARNING("Haven't implemented structured clone for these ports yet!");
  return nullptr;
}

void
MessagePort::CloseInternal()
{
  AssertCorrectThread();
  MOZ_ASSERT(!IsClosed());
  MOZ_ASSERT_IF(mStarted, mQueuedEvents.IsEmpty());

  NS_WARN_IF_FALSE(mStarted, "Called close() before start()!");

  if (!mStarted) {
    mQueuedEvents.Clear();
  }

  mSharedWorker = nullptr;
  mWorkerPrivate = nullptr;
}

#ifdef DEBUG
void
MessagePort::AssertCorrectThread() const
{
  if (IsClosed()) {
    return; // Can't assert anything if we nulled out our pointers.
  }

  MOZ_ASSERT((mSharedWorker || mWorkerPrivate) &&
             !(mSharedWorker && mWorkerPrivate));

  if (mSharedWorker) {
    AssertIsOnMainThread();
  }
  else {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }
}
#endif

NS_IMPL_ADDREF_INHERITED(mozilla::dom::workers::MessagePort, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(mozilla::dom::workers::MessagePort, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MessagePort)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(MessagePort)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MessagePort,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSharedWorker)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mQueuedEvents)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MessagePort,
                                                DOMEventTargetHelper)
  tmp->Close();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
MessagePort::WrapObject(JSContext* aCx)
{
  AssertCorrectThread();

  return MessagePortBinding::Wrap(aCx, this);
}

nsresult
MessagePort::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  AssertCorrectThread();

  nsIDOMEvent*& event = aVisitor.mDOMEvent;

  if (event) {
    bool preventDispatch = false;

    if (IsClosed()) {
      preventDispatch = true;
    } else if (NS_IsMainThread() && mSharedWorker->IsSuspended()) {
      mSharedWorker->QueueEvent(event);
      preventDispatch = true;
    } else if (!mStarted) {
      QueueEvent(event);
      preventDispatch = true;
    }

    if (preventDispatch) {
      aVisitor.mCanHandle = false;
      aVisitor.mParentTarget = nullptr;
      return NS_OK;
    }
  }

  return DOMEventTargetHelper::PreHandleEvent(aVisitor);
}

bool
DelayedEventRunnable::WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(mMessagePort);
  mMessagePort->AssertCorrectThread();
  MOZ_ASSERT(mEvents.Length());

  AutoNoJSAPI nojsapi;

  bool ignored;
  for (uint32_t i = 0; i < mEvents.Length(); i++) {
    mMessagePort->DispatchEvent(mEvents[i], &ignored);
  }

  return true;
}
