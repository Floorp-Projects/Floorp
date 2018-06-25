/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorker.h"

#include "nsPIDOMWindow.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/SharedWorkerBinding.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsIClassInfoImpl.h"

#include "RuntimeService.h"
#include "WorkerPrivate.h"

#ifdef XP_WIN
#undef PostMessage
#endif

using mozilla::dom::Optional;
using mozilla::dom::Sequence;
using mozilla::dom::MessagePort;
using namespace mozilla;
using namespace mozilla::dom;

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
                          const StringOrWorkerOptions& aOptions,
                          ErrorResult& aRv)
{
  AssertIsOnMainThread();

  workerinternals::RuntimeService* rts =
    workerinternals::RuntimeService::GetOrCreateService();
  if (!rts) {
    aRv = NS_ERROR_NOT_AVAILABLE;
    return nullptr;
  }

  nsAutoString name;
  if (aOptions.IsString()) {
    name = aOptions.GetAsString();
  } else {
    MOZ_ASSERT(aOptions.IsWorkerOptions());
    name = aOptions.GetAsWorkerOptions().mName;
  }

  RefPtr<SharedWorker> sharedWorker;
  nsresult rv = rts->CreateSharedWorker(aGlobal, aScriptURL, name,
                                        getter_AddRefs(sharedWorker));
  if (NS_FAILED(rv)) {
    aRv = rv;
    return nullptr;
  }

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
    nsTArray<RefPtr<Event>> events;
    mFrozenEvents.SwapElements(events);

    for (uint32_t index = 0; index < events.Length(); index++) {
      RefPtr<Event>& event = events[index];
      MOZ_ASSERT(event);

      RefPtr<EventTarget> target = event->GetTarget();
      ErrorResult rv;
      target->DispatchEvent(*event, rv);
      if (rv.Failed()) {
        NS_WARNING("Failed to dispatch event!");
      }
    }
  }
}

void
SharedWorker::QueueEvent(Event* aEvent)
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

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SharedWorker)
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

  return SharedWorker_Binding::Wrap(aCx, this, aGivenProto);
}

void
SharedWorker::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  AssertIsOnMainThread();

  if (IsFrozen()) {
    RefPtr<Event> event = aVisitor.mDOMEvent;
    if (!event) {
      event = EventDispatcher::CreateEvent(aVisitor.mEvent->mOriginalTarget,
                                           aVisitor.mPresContext,
                                           aVisitor.mEvent, EmptyString());
    }

    QueueEvent(event);

    aVisitor.mCanHandle = false;
    aVisitor.SetParentTarget(nullptr, false);
    return;
  }

  DOMEventTargetHelper::GetEventTargetParent(aVisitor);
}
