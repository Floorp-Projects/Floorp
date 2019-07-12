/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheWorkerRef.h"

#include "mozilla/dom/cache/ActorChild.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla {
namespace dom {
namespace cache {

// static
already_AddRefed<CacheWorkerRef> CacheWorkerRef::Create(
    WorkerPrivate* aWorkerPrivate, Behavior aBehavior) {
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);

  RefPtr<CacheWorkerRef> workerRef = new CacheWorkerRef(aBehavior);
  if (aBehavior == eStrongWorkerRef) {
    workerRef->mStrongWorkerRef =
        StrongWorkerRef::Create(aWorkerPrivate, "CacheWorkerRef-Strong",
                                [workerRef] { workerRef->Notify(); });
  } else {
    MOZ_ASSERT(aBehavior == eIPCWorkerRef);
    workerRef->mIPCWorkerRef =
        IPCWorkerRef::Create(aWorkerPrivate, "CacheWorkerRef-IPC",
                             [workerRef] { workerRef->Notify(); });
  }

  if (NS_WARN_IF(!workerRef->mIPCWorkerRef && !workerRef->mStrongWorkerRef)) {
    return nullptr;
  }

  return workerRef.forget();
}

// static
already_AddRefed<CacheWorkerRef> CacheWorkerRef::PreferBehavior(
    CacheWorkerRef* aCurrentRef, Behavior aBehavior) {
  if (!aCurrentRef) {
    return nullptr;
  }

  RefPtr<CacheWorkerRef> orig = aCurrentRef;
  if (orig->mBehavior == aBehavior) {
    return orig.forget();
  }

  WorkerPrivate* workerPrivate = nullptr;
  if (orig->mBehavior == eStrongWorkerRef) {
    workerPrivate = orig->mStrongWorkerRef->Private();
  } else {
    MOZ_ASSERT(orig->mBehavior == eIPCWorkerRef);
    workerPrivate = orig->mIPCWorkerRef->Private();
  }

  MOZ_ASSERT(workerPrivate);

  RefPtr<CacheWorkerRef> replace = Create(workerPrivate, aBehavior);
  if (!replace) {
    return orig.forget();
  }

  return replace.forget();
}

void CacheWorkerRef::AddActor(ActorChild* aActor) {
  NS_ASSERT_OWNINGTHREAD(CacheWorkerRef);
  MOZ_DIAGNOSTIC_ASSERT(aActor);
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

void CacheWorkerRef::RemoveActor(ActorChild* aActor) {
  NS_ASSERT_OWNINGTHREAD(CacheWorkerRef);
  MOZ_DIAGNOSTIC_ASSERT(aActor);

#if defined(RELEASE_OR_BETA)
  mActorList.RemoveElement(aActor);
#else
  MOZ_DIAGNOSTIC_ASSERT(mActorList.RemoveElement(aActor));
#endif

  MOZ_ASSERT(!mActorList.Contains(aActor));

  if (mActorList.IsEmpty()) {
    mStrongWorkerRef = nullptr;
    mIPCWorkerRef = nullptr;
  }
}

bool CacheWorkerRef::Notified() const { return mNotified; }

void CacheWorkerRef::Notify() {
  NS_ASSERT_OWNINGTHREAD(CacheWorkerRef);

  mNotified = true;

  // Start the asynchronous destruction of our actors.  These will call back
  // into RemoveActor() once the actor is destroyed.
  for (uint32_t i = 0; i < mActorList.Length(); ++i) {
    MOZ_DIAGNOSTIC_ASSERT(mActorList[i]);
    mActorList[i]->StartDestroy();
  }
}

CacheWorkerRef::CacheWorkerRef(Behavior aBehavior)
    : mBehavior(aBehavior), mNotified(false) {}

CacheWorkerRef::~CacheWorkerRef() {
  NS_ASSERT_OWNINGTHREAD(CacheWorkerRef);
  MOZ_DIAGNOSTIC_ASSERT(mActorList.IsEmpty());
}

}  // namespace cache
}  // namespace dom
}  // namespace mozilla
