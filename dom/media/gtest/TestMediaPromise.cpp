/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsISupportsImpl.h"
#include "MediaTaskQueue.h"
#include "MediaPromise.h"
#include "SharedThreadPool.h"
#include "VideoUtils.h"

using namespace mozilla;

typedef MediaPromise<int, double, false> TestPromise;
typedef TestPromise::ResolveOrRejectValue RRValue;

class MOZ_STACK_CLASS AutoTaskQueue
{
public:
  AutoTaskQueue()
    : mTaskQueue(new MediaTaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK)))
  {}

  ~AutoTaskQueue()
  {
    mTaskQueue->AwaitShutdownAndIdle();
  }

  MediaTaskQueue* TaskQueue() { return mTaskQueue; }
private:
  nsRefPtr<MediaTaskQueue> mTaskQueue;
};

class DelayedResolveOrReject : public nsRunnable
{
public:
  DelayedResolveOrReject(MediaTaskQueue* aTaskQueue,
                         TestPromise::Private* aPromise,
                         TestPromise::ResolveOrRejectValue aValue,
                         int aIterations)
  : mTaskQueue(aTaskQueue)
  , mPromise(aPromise)
  , mValue(aValue)
  , mIterations(aIterations)
  {}

  NS_IMETHODIMP Run()
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    if (!mPromise) {
      // Canceled.
      return NS_OK;
    }

    if (--mIterations == 0) {
      mPromise->ResolveOrReject(mValue, __func__);
    } else {
      nsCOMPtr<nsIRunnable> r = this;
      mTaskQueue->Dispatch(r.forget());
    }

    return NS_OK;
  }

  void Cancel() {
    mPromise = nullptr;
  }

protected:
  ~DelayedResolveOrReject() {}

private:
  nsRefPtr<MediaTaskQueue> mTaskQueue;
  nsRefPtr<TestPromise::Private> mPromise;
  TestPromise::ResolveOrRejectValue mValue;
  int mIterations;
};

template<typename FunctionType>
void
RunOnTaskQueue(MediaTaskQueue* aQueue, FunctionType aFun)
{
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(aFun);
  aQueue->Dispatch(r.forget());
}

// std::function can't come soon enough. :-(
#define DO_FAIL []()->void { EXPECT_TRUE(false); }

TEST(MediaPromise, BasicResolve)
{
  AutoTaskQueue atq;
  nsRefPtr<MediaTaskQueue> queue = atq.TaskQueue();
  RunOnTaskQueue(queue, [queue] () -> void {
    TestPromise::CreateAndResolve(42, __func__)->Then(queue, __func__,
      [queue] (int aResolveValue) -> void { EXPECT_EQ(aResolveValue, 42); queue->BeginShutdown(); },
      DO_FAIL);
  });
}

TEST(MediaPromise, BasicReject)
{
  AutoTaskQueue atq;
  nsRefPtr<MediaTaskQueue> queue = atq.TaskQueue();
  RunOnTaskQueue(queue, [queue] () -> void {
    TestPromise::CreateAndReject(42.0, __func__)->Then(queue, __func__,
      DO_FAIL,
      [queue] (int aRejectValue) -> void { EXPECT_EQ(aRejectValue, 42.0); queue->BeginShutdown(); });
  });
}

TEST(MediaPromise, AsyncResolve)
{
  AutoTaskQueue atq;
  nsRefPtr<MediaTaskQueue> queue = atq.TaskQueue();
  RunOnTaskQueue(queue, [queue] () -> void {
    nsRefPtr<TestPromise::Private> p = new TestPromise::Private(__func__);

    // Kick off three racing tasks, and make sure we get the one that finishes earliest.
    nsRefPtr<DelayedResolveOrReject> a = new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(32), 10);
    nsRefPtr<DelayedResolveOrReject> b = new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(42), 5);
    nsRefPtr<DelayedResolveOrReject> c = new DelayedResolveOrReject(queue, p, RRValue::MakeReject(32.0), 7);

    nsCOMPtr<nsIRunnable> ref = a.get();
    queue->Dispatch(ref.forget());
    ref = b.get();
    queue->Dispatch(ref.forget());
    ref = c.get();
    queue->Dispatch(ref.forget());

    p->Then(queue, __func__, [queue, a, b, c] (int aResolveValue) -> void {
      EXPECT_EQ(aResolveValue, 42);
      a->Cancel();
      b->Cancel();
      c->Cancel();
      queue->BeginShutdown();
    }, DO_FAIL);
  });
}

TEST(MediaPromise, CompletionPromises)
{
  bool invokedPass = false;
  AutoTaskQueue atq;
  nsRefPtr<MediaTaskQueue> queue = atq.TaskQueue();
  RunOnTaskQueue(queue, [queue, &invokedPass] () -> void {
    TestPromise::CreateAndResolve(40, __func__)
    ->Then(queue, __func__,
      [] (int aVal) -> nsRefPtr<TestPromise> { return TestPromise::CreateAndResolve(aVal + 10, __func__); },
      DO_FAIL)
    ->CompletionPromise()
    ->Then(queue, __func__, [&invokedPass] () -> void { invokedPass = true; }, DO_FAIL)
    ->CompletionPromise()
    ->Then(queue, __func__,
      [queue] (int aVal) -> nsRefPtr<TestPromise> {
        nsRefPtr<TestPromise::Private> p = new TestPromise::Private(__func__);
        nsCOMPtr<nsIRunnable> resolver = new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(aVal - 8), 10);
        queue->Dispatch(resolver.forget());
        return nsRefPtr<TestPromise>(p);
      },
      DO_FAIL)
    ->CompletionPromise()
    ->Then(queue, __func__,
      [queue] (int aVal) -> nsRefPtr<TestPromise> { return TestPromise::CreateAndReject(double(aVal - 42) + 42.0, __func__); },
      DO_FAIL)
    ->CompletionPromise()
    ->Then(queue, __func__,
      DO_FAIL,
      [queue, &invokedPass] (double aVal) -> void { EXPECT_EQ(aVal, 42.0); EXPECT_TRUE(invokedPass); queue->BeginShutdown(); });
  });
}

TEST(MediaPromise, PromiseAllResolve)
{
  AutoTaskQueue atq;
  nsRefPtr<MediaTaskQueue> queue = atq.TaskQueue();
  RunOnTaskQueue(queue, [queue] () -> void {

    nsTArray<nsRefPtr<TestPromise>> promises;
    promises.AppendElement(TestPromise::CreateAndResolve(22, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(32, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(42, __func__));

    TestPromise::All(queue, promises)->Then(queue, __func__,
      [queue] (const nsTArray<int>& aResolveValues) -> void {
        EXPECT_EQ(aResolveValues.Length(), 3UL);
        EXPECT_EQ(aResolveValues[0], 22);
        EXPECT_EQ(aResolveValues[1], 32);
        EXPECT_EQ(aResolveValues[2], 42);
        queue->BeginShutdown();
      },
      DO_FAIL
    );
  });
}

TEST(MediaPromise, PromiseAllReject)
{
  AutoTaskQueue atq;
  nsRefPtr<MediaTaskQueue> queue = atq.TaskQueue();
  RunOnTaskQueue(queue, [queue] () -> void {

    nsTArray<nsRefPtr<TestPromise>> promises;
    promises.AppendElement(TestPromise::CreateAndResolve(22, __func__));
    promises.AppendElement(TestPromise::CreateAndReject(32.0, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(42, __func__));

    TestPromise::All(queue, promises)->Then(queue, __func__,
      DO_FAIL,
      [queue] (float aRejectValue) -> void {
        EXPECT_EQ(aRejectValue, 32.0);
        queue->BeginShutdown();
      }
    );
  });
}

#undef DO_FAIL
