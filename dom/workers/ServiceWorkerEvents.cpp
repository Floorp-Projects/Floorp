/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerEvents.h"

#include "nsContentUtils.h"

#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/workers/bindings/ServiceWorker.h"

using namespace mozilla::dom;

BEGIN_WORKERS_NAMESPACE

ExtendableEvent::ExtendableEvent(EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
{
}

void
ExtendableEvent::WaitUntil(Promise& aPromise)
{
  MOZ_ASSERT(!NS_IsMainThread());

  // Only first caller counts.
  if (EventPhase() == AT_TARGET && !mPromise) {
    mPromise = &aPromise;
  }
}

NS_IMPL_ADDREF_INHERITED(ExtendableEvent, Event)
NS_IMPL_RELEASE_INHERITED(ExtendableEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ExtendableEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ExtendableEvent, Event, mPromise)

InstallEvent::InstallEvent(EventTarget* aOwner)
  : ExtendableEvent(aOwner)
  , mActivateImmediately(false)
{
}

NS_IMPL_ADDREF_INHERITED(InstallEvent, ExtendableEvent)
NS_IMPL_RELEASE_INHERITED(InstallEvent, ExtendableEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(InstallEvent)
NS_INTERFACE_MAP_END_INHERITING(ExtendableEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(InstallEvent, ExtendableEvent, mActiveWorker)

END_WORKERS_NAMESPACE
