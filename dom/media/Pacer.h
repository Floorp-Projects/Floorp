/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEventSource.h"
#include "MediaTimer.h"
#include "mozilla/TaskQueue.h"
#include "nsDeque.h"

#ifndef DOM_MEDIA_PACER_H_
#  define DOM_MEDIA_PACER_H_

namespace mozilla {

/**
 * Pacer<T> takes a queue of Ts tied to timestamps, and emits PacedItemEvents
 * for every T at its corresponding timestamp.
 *
 * The queue is ordered. Enqueing an item at time t will drop all items at times
 * later than T. This is because of how video sources work (some send out frames
 * in the future, some don't), and to allow swapping one source for another.
 *
 * It supports a duplication interval. If there is no new item enqueued within
 * the duplication interval since the last enqueued item, the last enqueud item
 * is emitted again.
 */
template <typename T>
class Pacer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Pacer)

  Pacer(RefPtr<TaskQueue> aTaskQueue, TimeDuration aDuplicationInterval)
      : mTaskQueue(std::move(aTaskQueue)),
        mDuplicationInterval(aDuplicationInterval),
        mTimer(MakeAndAddRef<MediaTimer>()) {}

  /**
   * Enqueues an item and schedules a timer to pass it on to PacedItemEvent() at
   * t=aTime. Already queued items with t>=aTime will be dropped.
   */
  void Enqueue(T aItem, TimeStamp aTime) {
    MOZ_ALWAYS_SUCCEEDS(mTaskQueue->Dispatch(NS_NewRunnableFunction(
        __func__,
        [this, self = RefPtr<Pacer>(this), aItem = std::move(aItem), aTime] {
          MOZ_DIAGNOSTIC_ASSERT(!mIsShutdown);
          while (const auto* item = mQueue.Peek()) {
            if (item->mTime < aTime) {
              break;
            }
            RefPtr<QueueItem> dropping = mQueue.Pop();
          }
          mQueue.Push(MakeAndAddRef<QueueItem>(std::move(aItem), aTime));
          EnsureTimerScheduled(aTime);
        })));
  }

  RefPtr<GenericPromise> Shutdown() {
    return InvokeAsync(
        mTaskQueue, __func__, [this, self = RefPtr<Pacer>(this)] {
          mIsShutdown = true;
          mTimer->Cancel();
          mQueue.Erase();
          mCurrentTimerTarget = Nothing();
          return GenericPromise::CreateAndResolve(true, "Pacer::Shutdown");
        });
  }

  MediaEventSourceExc<T, TimeStamp>& PacedItemEvent() {
    return mPacedItemEvent;
  }

 protected:
  ~Pacer() = default;

  void EnsureTimerScheduled(TimeStamp aTime) {
    if (mCurrentTimerTarget && *mCurrentTimerTarget <= aTime) {
      return;
    }

    if (mCurrentTimerTarget) {
      mTimer->Cancel();
      mCurrentTimerTarget = Nothing();
    }

    mTimer->WaitUntil(aTime, __func__)
        ->Then(
            mTaskQueue, __func__,
            [this, self = RefPtr<Pacer>(this)] { OnTimerTick(); },
            [] {
              // Timer was rejected. This is fine.
            });
    mCurrentTimerTarget = Some(aTime);
  }

  void OnTimerTick() {
    MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

    mCurrentTimerTarget = Nothing();

    while (RefPtr<QueueItem> item = mQueue.PopFront()) {
      auto now = TimeStamp::Now();

      if (item->mTime <= now) {
        // It's time to process this item.
        if (const auto& next = mQueue.PeekFront();
            !next || next->mTime > (item->mTime + mDuplicationInterval)) {
          // No future frame within the duplication interval exists. Schedule
          // a copy.
          mQueue.PushFront(MakeAndAddRef<QueueItem>(
              item->mItem, item->mTime + mDuplicationInterval));
        }
        mPacedItemEvent.Notify(std::move(item->mItem), item->mTime);
        continue;
      }

      // This item is in the future. Put it back.
      mQueue.PushFront(item.forget());
      break;
    }

    if (const auto& next = mQueue.PeekFront(); next) {
      // The queue is not empty. Schedule the timer.
      EnsureTimerScheduled(next->mTime);
    }
  }

 public:
  const RefPtr<TaskQueue> mTaskQueue;
  const TimeDuration mDuplicationInterval;

 protected:
  struct QueueItem {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(QueueItem)

    QueueItem(T aItem, TimeStamp aTime)
        : mItem(std::forward<T>(aItem)), mTime(aTime) {}

    T mItem;
    TimeStamp mTime;

   private:
    ~QueueItem() = default;
  };

  // Accessed on mTaskQueue.
  nsRefPtrDeque<QueueItem> mQueue;

  // Accessed on mTaskQueue.
  RefPtr<MediaTimer> mTimer;

  // Accessed on mTaskQueue.
  Maybe<TimeStamp> mCurrentTimerTarget;

  // Accessed on mTaskQueue.
  bool mIsShutdown = false;

  MediaEventProducerExc<T, TimeStamp> mPacedItemEvent;
};

}  // namespace mozilla

#endif
