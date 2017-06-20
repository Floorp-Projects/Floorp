/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"
#include "FetchConsumer.h"

#include "nsProxyRelease.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "Workers.h"

namespace mozilla {
namespace dom {

using namespace workers;

namespace {

template <class Derived>
class FetchBodyWorkerHolder final : public workers::WorkerHolder
{
  RefPtr<FetchBodyConsumer<Derived>> mConsumer;
  bool mWasNotified;

public:
  explicit FetchBodyWorkerHolder(FetchBodyConsumer<Derived>* aConsumer)
    : mConsumer(aConsumer)
    , mWasNotified(false)
  {
    MOZ_ASSERT(aConsumer);
  }

  ~FetchBodyWorkerHolder() = default;

  bool Notify(workers::Status aStatus) override
  {
    MOZ_ASSERT(aStatus > workers::Running);
    if (!mWasNotified) {
      mWasNotified = true;
      // This will probably cause the releasing of the consumer.
      // The WorkerHolder will be released as well.
      mConsumer->Body()->ContinueConsumeBody(mConsumer, NS_BINDING_ABORTED, 0,
                                             nullptr);
    }

    return true;
  }
};

} // anonymous

template <class Derived>
/* static */ already_AddRefed<FetchBodyConsumer<Derived>>
FetchBodyConsumer<Derived>::Create(FetchBody<Derived>* aBody)
{
  MOZ_ASSERT(aBody);

  RefPtr<FetchBodyConsumer<Derived>> consumer =
    new FetchBodyConsumer<Derived>(aBody);

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    if (!consumer->RegisterWorkerHolder(workerPrivate)) {
      return nullptr;
    }
  }

  return consumer.forget();
}

template <class Derived>
void
FetchBodyConsumer<Derived>::ReleaseObject()
{
  AssertIsOnTargetThread();

  mWorkerHolder = nullptr;
  mBody = nullptr;
}

template <class Derived>
FetchBodyConsumer<Derived>::FetchBodyConsumer(FetchBody<Derived>* aBody)
  : mTargetThread(NS_GetCurrentThread())
  , mBody(aBody)
{}

template <class Derived>
FetchBodyConsumer<Derived>::~FetchBodyConsumer()
{
  NS_ProxyRelease(mTargetThread, mBody.forget());
}

template <class Derived>
void
FetchBodyConsumer<Derived>::AssertIsOnTargetThread() const
{
  MOZ_ASSERT(NS_GetCurrentThread() == mTargetThread);
}

template <class Derived>
bool
FetchBodyConsumer<Derived>::RegisterWorkerHolder(WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(!mWorkerHolder);
  mWorkerHolder.reset(new FetchBodyWorkerHolder<Derived>(this));

  if (!mWorkerHolder->HoldWorker(aWorkerPrivate, Closing)) {
    NS_WARNING("Failed to add workerHolder");
    mWorkerHolder = nullptr;
    return false;
  }

  return true;
}

} // namespace dom
} // namespace mozilla
