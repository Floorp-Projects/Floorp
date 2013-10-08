/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePort.h"

#include "mozilla/dom/WorkerMessagePortBinding.h"
#include "nsDOMEvent.h"
#include "nsEventDispatcher.h"

#include "SharedWorker.h"

using mozilla::dom::Optional;
using mozilla::dom::Sequence;

USING_WORKERS_NAMESPACE

namespace {

class DelayedEventRunnable MOZ_FINAL : public nsIRunnable
{
  nsRefPtr<MessagePort> mMessagePort;
  nsCOMPtr<nsIDOMEvent> mEvent;

public:
  DelayedEventRunnable(MessagePort* aMessagePort,
                       already_AddRefed<nsIDOMEvent> aEvent)
  : mMessagePort(aMessagePort), mEvent(aEvent)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aMessagePort);
    MOZ_ASSERT(aEvent.get());
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
};

} // anonymous namespace

MessagePort::MessagePort(nsPIDOMWindow* aWindow, SharedWorker* aSharedWorker,
                         uint64_t aSerial)
: nsDOMEventTargetHelper(aWindow), mSharedWorker(aSharedWorker),
  mSerial(aSerial), mStarted(false)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aSharedWorker);
}

MessagePort::~MessagePort()
{
  AssertIsOnMainThread();

  Close();
}

// static
bool
MessagePort::PrefEnabled()
{
  AssertIsOnMainThread();

  // Currently tied to the SharedWorker preference.
  return SharedWorker::PrefEnabled();
}

void
MessagePort::PostMessageMoz(JSContext* aCx, JS::HandleValue aMessage,
                            const Optional<Sequence<JS::Value>>& aTransferable,
                            ErrorResult& aRv)
{
  AssertIsOnMainThread();

  if (IsClosed()) {
    aRv = NS_ERROR_DOM_INVALID_STATE_ERR;
    return;
  }

  mSharedWorker->PostMessage(aCx, aMessage, aTransferable, aRv);
}

void
MessagePort::Start()
{
  AssertIsOnMainThread();

  if (IsClosed()) {
    NS_WARNING("Called start() after calling close()!");
    return;
  }

  if (mStarted) {
    return;
  }

  mStarted = true;

  if (!mQueuedEvents.IsEmpty()) {
    for (uint32_t index = 0; index < mQueuedEvents.Length(); index++) {
      nsCOMPtr<nsIRunnable> runnable =
        new DelayedEventRunnable(this, mQueuedEvents[index].forget());
      if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
        NS_WARNING("Failed to dispatch queued event!");
      }
    }
    mQueuedEvents.Clear();
  }
}

void
MessagePort::QueueEvent(nsIDOMEvent* aEvent)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aEvent);
  MOZ_ASSERT(!IsClosed());
  MOZ_ASSERT(!mStarted);

  mQueuedEvents.AppendElement(aEvent);
}

void
MessagePort::CloseInternal()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsClosed());
  MOZ_ASSERT_IF(mStarted, mQueuedEvents.IsEmpty());

  NS_WARN_IF_FALSE(mStarted, "Called close() before start()!");

  if (!mStarted) {
    mQueuedEvents.Clear();
  }

  mSharedWorker = nullptr;
}

NS_IMPL_ADDREF_INHERITED(MessagePort, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MessagePort, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MessagePort)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(MessagePort)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MessagePort,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSharedWorker)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mQueuedEvents)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MessagePort,
                                                nsDOMEventTargetHelper)
  tmp->Close();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
MessagePort::WrapObject(JSContext* aCx, JS::HandleObject aScope)
{
  AssertIsOnMainThread();

  return WorkerMessagePortBinding::Wrap(aCx, aScope, this);
}

nsresult
MessagePort::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  AssertIsOnMainThread();

  nsIDOMEvent*& event = aVisitor.mDOMEvent;

  if (event) {
    bool preventDispatch = false;

    if (IsClosed()) {
      preventDispatch = true;
    } else if (mSharedWorker->IsSuspended()) {
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

  return nsDOMEventTargetHelper::PreHandleEvent(aVisitor);
}

NS_IMPL_ISUPPORTS1(DelayedEventRunnable, nsIRunnable)

NS_IMETHODIMP
DelayedEventRunnable::Run()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mMessagePort);
  MOZ_ASSERT(mEvent);

  bool ignored;
  nsresult rv = mMessagePort->DispatchEvent(mEvent, &ignored);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
