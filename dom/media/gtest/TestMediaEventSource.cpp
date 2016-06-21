/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

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
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

  MediaEventProducer<int> source;
  int i = 0;

  auto func = [&] (int j) { i += j; };
  MediaEventListener listener = source.Connect(queue, func);

  // Call Notify 3 times. The listener should be also called 3 times.
  source.Notify(3);
  source.Notify(5);
  source.Notify(7);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  // Verify the event data is passed correctly to the listener.
  EXPECT_EQ(i, 15); // 3 + 5 + 7
  listener.Disconnect();
}

TEST(MediaEventSource, MultiListener)
{
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

  MediaEventProducer<int> source;
  int i = 0;
  int j = 0;

  auto func1 = [&] (int k) { i = k * 2; };
  auto func2 = [&] (int k) { j = k * 3; };
  MediaEventListener listener1 = source.Connect(queue, func1);
  MediaEventListener listener2 = source.Connect(queue, func2);

  // Both listeners should receive the event.
  source.Notify(11);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  // Verify the event data is passed correctly to the listener.
  EXPECT_EQ(i, 22); // 11 * 2
  EXPECT_EQ(j, 33); // 11 * 3

  listener1.Disconnect();
  listener2.Disconnect();
}

/*
 * Test if disconnecting a listener prevents events from coming.
 */
TEST(MediaEventSource, DisconnectAfterNotification)
{
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

  MediaEventProducer<int> source;
  int i = 0;

  MediaEventListener listener;
  auto func = [&] (int j) { i += j; listener.Disconnect(); };
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
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

  MediaEventProducer<int> source;
  int i = 0;
  int j = 0;

  auto func1 = [&] (int k) { i = k * 2; };
  auto func2 = [&] (int k) { j = k * 3; };
  MediaEventListener listener1 = source.Connect(queue, func1);
  MediaEventListener listener2 = source.Connect(queue, func2);

  // Disconnect listener2 before notification. Only listener1 should receive
  // the event.
  listener2.Disconnect();
  source.Notify(11);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  EXPECT_EQ(i, 22); // 11 * 2
  EXPECT_EQ(j, 0); // event not received

  listener1.Disconnect();
}

/*
 * Test we don't hit the assertion when calling Connect() and Disconnect()
 * repeatedly.
 */
TEST(MediaEventSource, DisconnectAndConnect)
{
  RefPtr<TaskQueue> queue;
  MediaEventProducerExc<int> source;
  MediaEventListener listener = source.Connect(queue, [](){});
  listener.Disconnect();
  listener = source.Connect(queue, [](){});
  listener.Disconnect();
}

/*
 * Test void event type.
 */
TEST(MediaEventSource, VoidEventType)
{
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

  MediaEventProducer<void> source;
  int i = 0;

  // Test function object.
  auto func = [&] () { ++i; };
  MediaEventListener listener1 = source.Connect(queue, func);

  // Test member function.
  struct Foo {
    Foo() : j(1) {}
    void OnNotify() {
      j *= 2;
    }
    int j;
  } foo;
  MediaEventListener listener2 = source.Connect(queue, &foo, &Foo::OnNotify);

  // Call Notify 2 times. The listener should be also called 2 times.
  source.Notify();
  source.Notify();

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  // Verify the event data is passed correctly to the listener.
  EXPECT_EQ(i, 2); // ++i called twice
  EXPECT_EQ(foo.j, 4); // |j *= 2| called twice
  listener1.Disconnect();
  listener2.Disconnect();
}

/*
 * Test listeners can take various event types (T, T&&, const T& and void).
 */
TEST(MediaEventSource, ListenerType1)
{
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

  MediaEventProducer<int> source;
  int i = 0;

  // Test various argument types.
  auto func1 = [&] (int&& j) { i += j; };
  auto func2 = [&] (const int& j) { i += j; };
  auto func3 = [&] () { i += 1; };
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
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

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
  SomeEvent(const SomeEvent& aOther) : mCount(aOther.mCount) {
    ++mCount;
  }
  int& mCount;
};

/*
 * Test we don't have unnecessary copies of the event data.
 */
TEST(MediaEventSource, CopyEvent1)
{
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

  MediaEventProducer<SomeEvent> source;
  int i = 0;

  auto func = [] (SomeEvent&& aEvent) {};
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
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

  MediaEventProducer<SomeEvent> source;
  int i = 0;

  auto func = [] () {};
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
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

  MediaEventProducerExc<UniquePtr<int>> source;

  auto func = [] (UniquePtr<int>&& aEvent) {
    EXPECT_EQ(*aEvent, 20);
  };
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
