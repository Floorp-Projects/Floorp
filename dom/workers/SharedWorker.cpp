/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorker.h"

#include "nsPIDOMWindow.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/SharedWorkerBinding.h"
#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIDOMEvent.h"

#include "RuntimeService.h"
#include "WorkerPrivate.h"

using mozilla::dom::Optional;
using mozilla::dom::Sequence;
using mozilla::dom::MessagePort;
using namespace mozilla;

USING_WORKERS_NAMESPACE

SharedWorker::SharedWorker(nsPIDOMWindowInner* aWindow,
                           WorkerPrivate* aWorkerPrivate,
                           MessagePort* aMessagePort)
  : DOMEventTargetHelper(aWindow)
  , mWorkerPrivate(aWorkerPrivate)
  , mMessagePort(aMessagePort)
  , mFrozen(false)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWorkerPrivate);
  MOZ_ASSERT(aMessagePort);
}

SharedWorker::~SharedWorker()
{
  AssertIsOnMainThread();
}

// static
already_AddRefed<SharedWorker>
SharedWorker::Constructor(const GlobalObject& aGlobal,
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

  RefPtr<SharedWorker> sharedWorker;
  nsresult rv = rts->CreateSharedWorker(aGlobal, aScriptURL, name,
                                        getter_AddRefs(sharedWorker));
  if (NS_FAILED(rv)) {
    aRv = rv;
    return nullptr;
  }

  Telemetry::Accumulate(Telemetry::SHARED_WORKER_COUNT, 1);

  return sharedWorker.forget();
}

MessagePort*
SharedWorker::Port()
{
  AssertIsOnMainThread();
  return mMessagePort;
}

void
SharedWorker::Freeze()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsFrozen());

  mFrozen = true;
}

void
SharedWorker::Thaw()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(IsFrozen());

  mFrozen = false;

  if (!mFrozenEvents.IsEmpty()) {
    nsTArray<nsCOMPtr<nsIDOMEvent>> events;
    mFrozenEvents.SwapElements(events);

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
  MOZ_ASSERT(IsFrozen());

  mFrozenEvents.AppendElement(aEvent);
}

void
SharedWorker::Close()
{
  AssertIsOnMainThread();

  if (mMessagePort) {
    mMessagePort->Close();
  }
}

void
SharedWorker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                          const Sequence<JSObject*>& aTransferable,
                          ErrorResult& aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mWorkerPrivate);
  MOZ_ASSERT(mMessagePort);

  mMessagePort->PostMessage(aCx, aMessage, aTransferable, aRv);
}

NS_IMPL_ADDREF_INHERITED(SharedWorker, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SharedWorker, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SharedWorker)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(SharedWorker)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SharedWorker,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrozenEvents)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SharedWorker,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFrozenEvents)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
SharedWorker::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  AssertIsOnMainThread();

  return SharedWorkerBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
SharedWorker::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  AssertIsOnMainThread();

  if (IsFrozen()) {
    nsCOMPtr<nsIDOMEvent> event = aVisitor.mDOMEvent;
    if (!event) {
      event = EventDispatcher::CreateEvent(aVisitor.mEvent->mOriginalTarget,
                                           aVisitor.mPresContext,
                                           aVisitor.mEvent, EmptyString());
    }

    QueueEvent(event);

    aVisitor.mCanHandle = false;
    aVisitor.mParentTarget = nullptr;
    return NS_OK;
  }

  return DOMEventTargetHelper::GetEventTargetParent(aVisitor);
}
