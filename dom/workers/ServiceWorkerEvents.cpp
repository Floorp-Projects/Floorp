/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerEvents.h"

#include "nsContentUtils.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/workers/bindings/ServiceWorker.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"

using namespace mozilla::dom;

BEGIN_WORKERS_NAMESPACE

bool
ServiceWorkerEventsVisible(JSContext* aCx, JSObject* aObj)
{
  ServiceWorkerGlobalScope* scope = nullptr;
  nsresult rv = UnwrapObject<prototypes::id::ServiceWorkerGlobalScope_workers,
                             mozilla::dom::ServiceWorkerGlobalScopeBinding_workers::NativeType>(aObj, scope);
  return NS_SUCCEEDED(rv) && scope;
}

InstallPhaseEvent::InstallPhaseEvent(EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
{
}

void
InstallPhaseEvent::WaitUntil(Promise& aPromise)
{
  MOZ_ASSERT(!NS_IsMainThread());

  // Only first caller counts.
  if (EventPhase() == AT_TARGET && !mPromise) {
    mPromise = &aPromise;
  }
}

NS_IMPL_ADDREF_INHERITED(InstallPhaseEvent, Event)
NS_IMPL_RELEASE_INHERITED(InstallPhaseEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(InstallPhaseEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_CYCLE_COLLECTION_INHERITED(InstallPhaseEvent, Event, mPromise)

InstallEvent::InstallEvent(EventTarget* aOwner)
  : InstallPhaseEvent(aOwner)
{
}

NS_IMPL_ADDREF_INHERITED(InstallEvent, InstallPhaseEvent)
NS_IMPL_RELEASE_INHERITED(InstallEvent, InstallPhaseEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(InstallEvent)
NS_INTERFACE_MAP_END_INHERITING(InstallPhaseEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(InstallEvent, InstallPhaseEvent, mActiveWorker)

END_WORKERS_NAMESPACE
