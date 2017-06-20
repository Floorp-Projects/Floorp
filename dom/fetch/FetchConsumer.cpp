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
  RefPtr<FetchBodyWrapper<Derived>> mWrapper;
  bool mWasNotified;

public:
  explicit FetchBodyWorkerHolder(FetchBodyWrapper<Derived>* aWrapper)
    : mWrapper(aWrapper)
    , mWasNotified(false)
  {
    MOZ_ASSERT(aWrapper);
  }

  ~FetchBodyWorkerHolder() = default;

  bool Notify(workers::Status aStatus) override
  {
    MOZ_ASSERT(aStatus > workers::Running);
    if (!mWasNotified) {
      mWasNotified = true;
      // This will probably cause the releasing of the wrapper.
      // The WorkerHolder will be released as well.
      mWrapper->Body()->ContinueConsumeBody(mWrapper, NS_BINDING_ABORTED, 0,
                                            nullptr);
    }

    return true;
  }
};

} // anonymous

template <class Derived>
/* static */ already_AddRefed<FetchBodyWrapper<Derived>>
FetchBodyWrapper<Derived>::Create(FetchBody<Derived>* aBody)
{
  MOZ_ASSERT(aBody);

  RefPtr<FetchBodyWrapper<Derived>> wrapper =
    new FetchBodyWrapper<Derived>(aBody);

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    if (!wrapper->RegisterWorkerHolder(workerPrivate)) {
      return nullptr;
    }
  }

  return wrapper.forget();
}

template <class Derived>
void
FetchBodyWrapper<Derived>::ReleaseObject()
{
  AssertIsOnTargetThread();

  mWorkerHolder = nullptr;
  mBody = nullptr;
}

template <class Derived>
FetchBodyWrapper<Derived>::FetchBodyWrapper(FetchBody<Derived>* aBody)
  : mTargetThread(NS_GetCurrentThread())
  , mBody(aBody)
{}

template <class Derived>
FetchBodyWrapper<Derived>::~FetchBodyWrapper()
{
  NS_ProxyRelease(mTargetThread, mBody.forget());
}

template <class Derived>
void
FetchBodyWrapper<Derived>::AssertIsOnTargetThread() const
{
  MOZ_ASSERT(NS_GetCurrentThread() == mTargetThread);
}

template <class Derived>
bool
FetchBodyWrapper<Derived>::RegisterWorkerHolder(WorkerPrivate* aWorkerPrivate)
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
