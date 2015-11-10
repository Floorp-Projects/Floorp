/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/Feature.h"

#include "mozilla/dom/cache/ActorChild.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::dom::workers::Terminating;
using mozilla::dom::workers::Status;
using mozilla::dom::workers::WorkerPrivate;

// static
already_AddRefed<Feature>
Feature::Create(WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);

  RefPtr<Feature> feature = new Feature(aWorkerPrivate);

  if (!aWorkerPrivate->AddFeature(aWorkerPrivate->GetJSContext(), feature)) {
    return nullptr;
  }

  return feature.forget();
}

void
Feature::AddActor(ActorChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(Feature);
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActorList.Contains(aActor));

  mActorList.AppendElement(aActor);

  // Allow an actor to be added after we've entered the Notifying case.  We
  // can't stop the actor creation from racing with out destruction of the
  // other actors and we need to wait for this extra one to close as well.
  // Signal it should destroy itself right away.
  if (mNotified) {
    aActor->StartDestroy();
  }
}

void
Feature::RemoveActor(ActorChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(Feature);
  MOZ_ASSERT(aActor);

  DebugOnly<bool> removed = mActorList.RemoveElement(aActor);

  MOZ_ASSERT(removed);
  MOZ_ASSERT(!mActorList.Contains(aActor));
}

bool
Feature::Notified() const
{
  return mNotified;
}

bool
Feature::Notify(JSContext* aCx, Status aStatus)
{
  NS_ASSERT_OWNINGTHREAD(Feature);

  // When the service worker thread is stopped we will get Terminating,
  // but nothing higher than that.  We must shut things down at Terminating.
  if (aStatus < Terminating || mNotified) {
    return true;
  }

  mNotified = true;

  // Start the asynchronous destruction of our actors.  These will call back
  // into RemoveActor() once the actor is destroyed.
  for (uint32_t i = 0; i < mActorList.Length(); ++i) {
    mActorList[i]->StartDestroy();
  }

  return true;
}

Feature::Feature(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate)
  , mNotified(false)
{
  MOZ_ASSERT(mWorkerPrivate);
}

Feature::~Feature()
{
  NS_ASSERT_OWNINGTHREAD(Feature);
  MOZ_ASSERT(mActorList.IsEmpty());

  mWorkerPrivate->RemoveFeature(mWorkerPrivate->GetJSContext(), this);
}

} // namespace cache
} // namespace dom
} // namespace mozilla
