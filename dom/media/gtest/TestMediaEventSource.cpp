/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/SharedThreadPool.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/UniquePtr.h"
#include "MediaEventSource.h"
#include "VideoUtils.h"

using namespace mozilla;

/*
 * Test if listeners receive the event data correctly.
 */
TEST(MediaEventSource, SingleListener)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource SingleListener");

  MediaEventProducer<int> source;
  int i = 0;

  auto func = [&](int j) { i += j; };
  MediaEventListener listener = source.Connect(queue, func);

  // Call Notify 3 times. The listener should be also called 3 times.
  source.Notify(3);
  source.Notify(5);
  source.Notify(7);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  // Verify the event data is passed correctly to the listener.
  EXPECT_EQ(i, 15);  // 3 + 5 + 7
  listener.Disconnect();
}

TEST(MediaEventSource, MultiListener)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource MultiListener");

  MediaEventProducer<int> source;
  int i = 0;
  int j = 0;

  auto func1 = [&](int k) { i = k * 2; };
  auto func2 = [&](int k) { j = k * 3; };
  MediaEventListener listener1 = source.Connect(queue, func1);
  MediaEventListener listener2 = source.Connect(queue, func2);

  // Both listeners should receive the event.
  source.Notify(11);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  // Verify the event data is passed correctly to the listener.
  EXPECT_EQ(i, 22);  // 11 * 2
  EXPECT_EQ(j, 33);  // 11 * 3

  listener1.Disconnect();
  listener2.Disconnect();
}

/*
 * Test if disconnecting a listener prevents events from coming.
 */
TEST(MediaEventSource, DisconnectAfterNotification)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource DisconnectAfterNotification");

  MediaEventProducer<int> source;
  int i = 0;

  MediaEventListener listener;
  auto func = [&](int j) {
    i += j;
    listener.Disconnect();
  };
  listener = source.Connect(queue, func);

  // Call Notify() twice. Since we disconnect the listener when receiving
  // the 1st event, the 2nd event should not reach the listener.
  source.Notify(11);
  source.Notify(11);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  // Check only the 1st event is received.
  EXPECT_EQ(i, 11);
}

TEST(MediaEventSource, DisconnectBeforeNotification)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource DisconnectBeforeNotification");

  MediaEventProducer<int> source;
  int i = 0;
  int j = 0;

  auto func1 = [&](int k) { i = k * 2; };
  auto func2 = [&](int k) { j = k * 3; };
  MediaEventListener listener1 = source.Connect(queue, func1);
  MediaEventListener listener2 = source.Connect(queue, func2);

  // Disconnect listener2 before notification. Only listener1 should receive
  // the event.
  listener2.Disconnect();
  source.Notify(11);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  EXPECT_EQ(i, 22);  // 11 * 2
  EXPECT_EQ(j, 0);   // event not received

  listener1.Disconnect();
}

/*
 * Test we don't hit the assertion when calling Connect() and Disconnect()
 * repeatedly.
 */
TEST(MediaEventSource, DisconnectAndConnect)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource DisconnectAndConnect");

  MediaEventProducerExc<int> source;
  MediaEventListener listener = source.Connect(queue, []() {});
  listener.Disconnect();
  listener = source.Connect(queue, []() {});
  listener.Disconnect();
}

/*
 * Test void event type.
 */
TEST(MediaEventSource, VoidEventType)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource VoidEventType");

  MediaEventProducer<void> source;
  int i = 0;

  // Test function object.
  auto func = [&]() { ++i; };
  MediaEventListener listener1 = source.Connect(queue, func);

  // Test member function.
  struct Foo {
    Foo() : j(1) {}
    void OnNotify() { j *= 2; }
    int j;
  } foo;
  MediaEventListener listener2 = source.Connect(queue, &foo, &Foo::OnNotify);

  // Call Notify 2 times. The listener should be also called 2 times.
  source.Notify();
  source.Notify();

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  // Verify the event data is passed correctly to the listener.
  EXPECT_EQ(i, 2);      // ++i called twice
  EXPECT_EQ(foo.j, 4);  // |j *= 2| called twice
  listener1.Disconnect();
  listener2.Disconnect();
}

/*
 * Test listeners can take various event types (T, T&&, const T& and void).
 */
TEST(MediaEventSource, ListenerType1)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource ListenerType1");

  MediaEventProducer<int> source;
  int i = 0;

  // Test various argument types.
  auto func1 = [&](int&& j) { i += j; };
  auto func2 = [&](const int& j) { i += j; };
  auto func3 = [&]() { i += 1; };
  MediaEventListener listener1 = source.Connect(queue, func1);
  MediaEventListener listener2 = source.Connect(queue, func2);
  MediaEventListener listener3 = source.Connect(queue, func3);

  source.Notify(1);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  EXPECT_EQ(i, 3);

  listener1.Disconnect();
  listener2.Disconnect();
  listener3.Disconnect();
}

TEST(MediaEventSource, ListenerType2)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource ListenerType2");

  MediaEventProducer<int> source;

  struct Foo {
    Foo() : mInt(0) {}
    void OnNotify1(int&& i) { mInt += i; }
    void OnNotify2(const int& i) { mInt += i; }
    void OnNotify3() { mInt += 1; }
    void OnNotify4(int i) const { mInt += i; }
    void OnNotify5(int i) volatile { mInt += i; }
    mutable int mInt;
  } foo;

  // Test member functions which might be CV qualified.
  MediaEventListener listener1 = source.Connect(queue, &foo, &Foo::OnNotify1);
  MediaEventListener listener2 = source.Connect(queue, &foo, &Foo::OnNotify2);
  MediaEventListener listener3 = source.Connect(queue, &foo, &Foo::OnNotify3);
  MediaEventListener listener4 = source.Connect(queue, &foo, &Foo::OnNotify4);
  MediaEventListener listener5 = source.Connect(queue, &foo, &Foo::OnNotify5);

  source.Notify(1);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  EXPECT_EQ(foo.mInt, 5);

  listener1.Disconnect();
  listener2.Disconnect();
  listener3.Disconnect();
  listener4.Disconnect();
  listener5.Disconnect();
}

struct SomeEvent {
  explicit SomeEvent(int& aCount) : mCount(aCount) {}
  // Increment mCount when copy constructor is called to know how many times
  // the event data is copied.
  SomeEvent(const SomeEvent& aOther) : mCount(aOther.mCount) { ++mCount; }
  SomeEvent(SomeEvent&& aOther) : mCount(aOther.mCount) {}
  int& mCount;
};

/*
 * Test we don't have unnecessary copies of the event data.
 */
TEST(MediaEventSource, CopyEvent1)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource CopyEvent1");

  MediaEventProducer<SomeEvent> source;
  int i = 0;

  auto func = [](SomeEvent&& aEvent) {};
  struct Foo {
    void OnNotify(SomeEvent&& aEvent) {}
  } foo;

  MediaEventListener listener1 = source.Connect(queue, func);
  MediaEventListener listener2 = source.Connect(queue, &foo, &Foo::OnNotify);

  // We expect i to be 2 since SomeEvent should be copied only once when
  // passing to each listener.
  source.Notify(SomeEvent(i));

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  EXPECT_EQ(i, 2);
  listener1.Disconnect();
  listener2.Disconnect();
}

TEST(MediaEventSource, CopyEvent2)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource CopyEvent2");

  MediaEventProducer<SomeEvent> source;
  int i = 0;

  auto func = []() {};
  struct Foo {
    void OnNotify() {}
  } foo;

  MediaEventListener listener1 = source.Connect(queue, func);
  MediaEventListener listener2 = source.Connect(queue, &foo, &Foo::OnNotify);

  // SomeEvent won't be copied at all since the listeners take no arguments.
  source.Notify(SomeEvent(i));

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  EXPECT_EQ(i, 0);
  listener1.Disconnect();
  listener2.Disconnect();
}

/*
 * Test move-only types.
 */
TEST(MediaEventSource, MoveOnly)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource MoveOnly");

  MediaEventProducerExc<UniquePtr<int>> source;

  auto func = [](UniquePtr<int>&& aEvent) { EXPECT_EQ(*aEvent, 20); };
  MediaEventListener listener = source.Connect(queue, func);

  // It is OK to pass an rvalue which is move-only.
  source.Notify(UniquePtr<int>(new int(20)));
  // It is an error to pass an lvalue which is move-only.
  // UniquePtr<int> event(new int(30));
  // source.Notify(event);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  listener.Disconnect();
}

struct RefCounter {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCounter)
  explicit RefCounter(int aVal) : mVal(aVal) {}
  int mVal;

 private:
  ~RefCounter() = default;
};

/*
 * Test we should copy instead of move in NonExclusive mode
 * for each listener must get a copy.
 */
TEST(MediaEventSource, NoMove)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource NoMove");

  MediaEventProducer<RefPtr<RefCounter>> source;

  auto func1 = [](RefPtr<RefCounter>&& aEvent) { EXPECT_EQ(aEvent->mVal, 20); };
  auto func2 = [](RefPtr<RefCounter>&& aEvent) { EXPECT_EQ(aEvent->mVal, 20); };
  MediaEventListener listener1 = source.Connect(queue, func1);
  MediaEventListener listener2 = source.Connect(queue, func2);

  // We should copy this rvalue instead of move it in NonExclusive mode.
  RefPtr<RefCounter> val = new RefCounter(20);
  source.Notify(std::move(val));

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  listener1.Disconnect();
  listener2.Disconnect();
}

/*
 * Rvalue lambda should be moved instead of copied.
 */
TEST(MediaEventSource, MoveLambda)
{
  RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource MoveLambda");

  MediaEventProducer<void> source;

  int counter = 0;
  SomeEvent someEvent(counter);

  auto func = [someEvent]() {};
  // someEvent is copied when captured by the lambda.
  EXPECT_EQ(someEvent.mCount, 1);

  // someEvent should be copied for we pass |func| as an lvalue.
  MediaEventListener listener1 = source.Connect(queue, func);
  EXPECT_EQ(someEvent.mCount, 2);

  // someEvent should be moved for we pass |func| as an rvalue.
  MediaEventListener listener2 = source.Connect(queue, std::move(func));
  EXPECT_EQ(someEvent.mCount, 2);

  listener1.Disconnect();
  listener2.Disconnect();
}

template <typename Bool>
struct DestroyChecker {
  explicit DestroyChecker(Bool* aIsDestroyed) : mIsDestroyed(aIsDestroyed) {
    EXPECT_FALSE(*mIsDestroyed);
  }
  ~DestroyChecker() {
    EXPECT_FALSE(*mIsDestroyed);
    *mIsDestroyed = true;
  }

 private:
  Bool* const mIsDestroyed;
};

class ClassForDestroyCheck final : private DestroyChecker<bool> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ClassForDestroyCheck);

  explicit ClassForDestroyCheck(bool* aIsDestroyed)
      : DestroyChecker(aIsDestroyed) {}

  int32_t RefCountNums() const { return mRefCnt; }

 protected:
  ~ClassForDestroyCheck() = default;
};

TEST(MediaEventSource, ResetFuncReferenceAfterDisconnect)
{
  const RefPtr<TaskQueue> queue =
      new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                    "TestMediaEventSource ResetFuncReferenceAfterDisconnect");
  MediaEventProducer<void> source;

  // Using a class that supports refcounting to check the object destruction.
  bool isDestroyed = false;
  auto object = MakeRefPtr<ClassForDestroyCheck>(&isDestroyed);
  EXPECT_FALSE(isDestroyed);
  EXPECT_EQ(object->RefCountNums(), 1);

  // Function holds a strong reference to object.
  MediaEventListener listener = source.Connect(queue, [ptr = object] {});
  EXPECT_FALSE(isDestroyed);
  EXPECT_EQ(object->RefCountNums(), 2);

  // This should destroy the function and release the object reference from the
  // function on the task queue,
  listener.Disconnect();
  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  EXPECT_FALSE(isDestroyed);
  EXPECT_EQ(object->RefCountNums(), 1);

  // No one is holding reference to object, it should be destroyed
  // immediately.
  object = nullptr;
  EXPECT_TRUE(isDestroyed);
}

class TestTaskQueue : public TaskQueue, private DestroyChecker<Atomic<bool>> {
 public:
  TestTaskQueue(already_AddRefed<nsIEventTarget> aTarget,
                Atomic<bool>* aIsDestroyed)
      : TaskQueue(std::move(aTarget),
                  "TestMediaEventSource ResetTargetAfterDisconnect"),
        DestroyChecker(aIsDestroyed) {}
};

TEST(MediaEventSource, ResetTargetAfterDisconnect)
{
  Atomic<bool> isDestroyed(false);
  RefPtr<TaskQueue> queue = new TestTaskQueue(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR), &isDestroyed);
  MediaEventProducer<void> source;
  MediaEventListener listener = source.Connect(queue, [] {});

  // MediaEventListener::Disconnect eventually gives up its target
  listener.Disconnect();
  queue->AwaitIdle();
  queue = nullptr;
  EXPECT_TRUE(isDestroyed);
}
