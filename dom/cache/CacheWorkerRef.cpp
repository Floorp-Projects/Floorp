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

namespace {
// XXX Move this to mfbt, or do we already have something like this? Or remove
// the need for that by changing StrongWorkerRef/IPCWorkerRef?

template <class T>
class FakeCopyable {
 public:
  explicit FakeCopyable(T&& aTarget) : mTarget(std::forward<T>(aTarget)) {}

  FakeCopyable(FakeCopyable&&) = default;

  FakeCopyable(const FakeCopyable& aOther)
      : mTarget(std::move(const_cast<FakeCopyable&>(aOther).mTarget)) {
    MOZ_CRASH("Do not copy.");
  }

  template <typename... Args>
  auto operator()(Args&&... aArgs) {
    return mTarget(std::forward<Args>(aArgs)...);
  }

 private:
  T mTarget;
};

}  // namespace

// static
SafeRefPtr<CacheWorkerRef> CacheWorkerRef::Create(WorkerPrivate* aWorkerPrivate,
                                                  Behavior aBehavior) {
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);

  // XXX This looks as if this could be simplified now by moving into the ctor
  // of CacheWorkerRef, since we can now use SafeRefPtrFromThis in the ctor
  auto workerRef =
      MakeSafeRefPtr<CacheWorkerRef>(aBehavior, ConstructorGuard{});
  auto notify =
      FakeCopyable([workerRef = workerRef.clonePtr()] { workerRef->Notify(); });
  if (aBehavior == eStrongWorkerRef) {
    workerRef->mStrongWorkerRef = StrongWorkerRef::Create(
        aWorkerPrivate, "CacheWorkerRef-Strong", std::move(notify));
  } else {
    MOZ_ASSERT(aBehavior == eIPCWorkerRef);
    workerRef->mIPCWorkerRef = IPCWorkerRef::Create(
        aWorkerPrivate, "CacheWorkerRef-IPC", std::move(notify));
  }

  if (NS_WARN_IF(!workerRef->mIPCWorkerRef && !workerRef->mStrongWorkerRef)) {
    return nullptr;
  }

  return workerRef;
}

// static
SafeRefPtr<CacheWorkerRef> CacheWorkerRef::PreferBehavior(
    SafeRefPtr<CacheWorkerRef> aCurrentRef, Behavior aBehavior) {
  if (!aCurrentRef) {
    return nullptr;
  }

  SafeRefPtr<CacheWorkerRef> orig = std::move(aCurrentRef);
  if (orig->mBehavior == aBehavior) {
    return orig;
  }

  WorkerPrivate* workerPrivate = nullptr;
  if (orig->mBehavior == eStrongWorkerRef) {
    workerPrivate = orig->mStrongWorkerRef->Private();
  } else {
    MOZ_ASSERT(orig->mBehavior == eIPCWorkerRef);
    workerPrivate = orig->mIPCWorkerRef->Private();
  }

  MOZ_ASSERT(workerPrivate);

  SafeRefPtr<CacheWorkerRef> replace = Create(workerPrivate, aBehavior);
  return static_cast<bool>(replace) ? std::move(replace) : std::move(orig);
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

CacheWorkerRef::CacheWorkerRef(Behavior aBehavior, ConstructorGuard)
    : mBehavior(aBehavior), mNotified(false) {}

CacheWorkerRef::~CacheWorkerRef() {
  NS_ASSERT_OWNINGTHREAD(CacheWorkerRef);
  MOZ_DIAGNOSTIC_ASSERT(mActorList.IsEmpty());
}

}  // namespace cache
}  // namespace dom
}  // namespace mozilla
