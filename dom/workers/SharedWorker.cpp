/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorker.h"

#include "nsPIDOMWindow.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/SharedWorkerBinding.h"
#include "nsContentUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIDOMEvent.h"

#include "MessagePort.h"
#include "RuntimeService.h"
#include "WorkerPrivate.h"

using mozilla::dom::Optional;
using mozilla::dom::Sequence;
using namespace mozilla;

USING_WORKERS_NAMESPACE

SharedWorker::SharedWorker(nsPIDOMWindow* aWindow,
                           WorkerPrivate* aWorkerPrivate)
: nsDOMEventTargetHelper(aWindow), mWorkerPrivate(aWorkerPrivate),
  mSuspended(false)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWorkerPrivate);

  mSerial = aWorkerPrivate->NextMessagePortSerial();

  mMessagePort = new MessagePort(aWindow, this, mSerial);
}

SharedWorker::~SharedWorker()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!mWorkerPrivate);
}

// static
already_AddRefed<SharedWorker>
SharedWorker::Constructor(const GlobalObject& aGlobal, JSContext* aCx,
                          const nsAString& aScriptURL,
                          const mozilla::dom::Optional<nsAString>& aName,
                          ErrorResult& aRv)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetOrCreateService();
  if (!rts) {
    aRv = NS_ERROR_NOT_AVAILABLE;
    return nullptr;
  }

  nsCString name;
  if (aName.WasPassed()) {
    name = NS_ConvertUTF16toUTF8(aName.Value());
  }

  nsRefPtr<SharedWorker> sharedWorker;
  nsresult rv = rts->CreateSharedWorker(aGlobal, aScriptURL, name,
                                        getter_AddRefs(sharedWorker));
  if (NS_FAILED(rv)) {
    aRv = rv;
    return nullptr;
  }

  return sharedWorker.forget();
}

already_AddRefed<MessagePort>
SharedWorker::Port()
{
  AssertIsOnMainThread();

  nsRefPtr<MessagePort> messagePort = mMessagePort;
  return messagePort.forget();
}

void
SharedWorker::Suspend()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsSuspended());

  mSuspended = true;
}

void
SharedWorker::Resume()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(IsSuspended());

  mSuspended = false;

  if (!mSuspendedEvents.IsEmpty()) {
    nsTArray<nsCOMPtr<nsIDOMEvent>> events;
    mSuspendedEvents.SwapElements(events);

    for (uint32_t index = 0; index < events.Length(); index++) {
      nsCOMPtr<nsIDOMEvent>& event = events[index];
      MOZ_ASSERT(event);

      nsCOMPtr<nsIDOMEventTarget> target;
      if (NS_SUCCEEDED(event->GetTarget(getter_AddRefs(target)))) {
        bool ignored;
        if (NS_FAILED(target->DispatchEvent(event, &ignored))) {
          NS_WARNING("Failed to dispatch event!");
        }
      } else {
        NS_WARNING("Failed to get target!");
      }
    }
  }
}

void
SharedWorker::QueueEvent(nsIDOMEvent* aEvent)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aEvent);
  MOZ_ASSERT(IsSuspended());

  mSuspendedEvents.AppendElement(aEvent);
}

void
SharedWorker::Close()
{
  AssertIsOnMainThread();

  if (mMessagePort) {
    mMessagePort->Close();
  }

  if (mWorkerPrivate) {
    AutoSafeJSContext cx;
    NoteDeadWorker(cx);
  }
}

void
SharedWorker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                          const Optional<Sequence<JS::Value>>& aTransferable,
                          ErrorResult& aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mWorkerPrivate);
  MOZ_ASSERT(mMessagePort);

  mWorkerPrivate->PostMessageToMessagePort(aCx, mMessagePort->Serial(),
                                           aMessage, aTransferable, aRv);
}

void
SharedWorker::NoteDeadWorker(JSContext* aCx)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mWorkerPrivate);

  mWorkerPrivate->UnregisterSharedWorker(aCx, this);
  mWorkerPrivate = nullptr;
}

NS_IMPL_ADDREF_INHERITED(SharedWorker, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SharedWorker, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SharedWorker)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(SharedWorker)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SharedWorker,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSuspendedEvents)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SharedWorker,
                                                nsDOMEventTargetHelper)
  tmp->Close();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSuspendedEvents)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
SharedWorker::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  AssertIsOnMainThread();

  return SharedWorkerBinding::Wrap(aCx, aScope, this);
}

nsresult
SharedWorker::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  AssertIsOnMainThread();

  nsIDOMEvent*& event = aVisitor.mDOMEvent;

  if (IsSuspended() && event) {
    QueueEvent(event);

    aVisitor.mCanHandle = false;
    aVisitor.mParentTarget = nullptr;
    return NS_OK;
  }

  return nsDOMEventTargetHelper::PreHandleEvent(aVisitor);
}
