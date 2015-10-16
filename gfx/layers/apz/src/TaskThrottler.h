/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TaskThrottler_h
#define mozilla_dom_TaskThrottler_h

#include <stdint.h>                     // for uint32_t
#include "base/task.h"                  // for CancelableTask
#include "mozilla/Monitor.h"            // for Monitor
#include "mozilla/mozalloc.h"           // for operator delete
#include "mozilla/RollingMean.h"        // for RollingMean
#include "mozilla/TimeStamp.h"          // for TimeDuration, TimeStamp
#include "mozilla/UniquePtr.h"          // for UniquePtr
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsISupportsImpl.h"            // for NS_INLINE_DECL_THREADSAFE_REFCOUNTING
#include "nsTArray.h"                   // for nsTArray

namespace tracked_objects {
class Location;
} // namespace tracked_objects

namespace mozilla {
namespace layers {

/** The TaskThrottler prevents update event overruns. It is used in cases where
 * you're sending an async message and waiting for a reply. You need to call
 * PostTask to queue a task and TaskComplete when you get a response.
 *
 * The call to TaskComplete will run the most recent task posted since the last
 * request was sent, if any. This means that at any time there can be at most 1
 * outstanding request being processed and at most 1 queued behind it.
 *
 * However, to guard against task runs that error out and fail to call TaskComplete,
 * the TaskThrottler also has a max-wait timeout. If the caller requests a new
 * task be posted, and it has been greater than the max-wait timeout since the
 * last one was sent, then we send the new one regardless of whether or not the
 * last one was marked as completed.
 *
 * This is used in the context of repainting a scrollable region. While another
 * process is painting you might get several updates from the UI thread but when
 * the paint is complete you want to send the most recent.
 */

class TaskThrottler {
public:
  TaskThrottler(const TimeStamp& aTimeStamp, const TimeDuration& aMaxWait);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TaskThrottler)

  /** Post a task to be run as soon as there are no outstanding tasks, or
   * post it immediately if it has been more than the max-wait time since
   * the last task was posted.
   *
   * @param aLocation Use the macro FROM_HERE
   * @param aTask     Ownership of this object is transferred to TaskThrottler
   *                  which will delete it when it is either run or becomes
   *                  obsolete or the TaskThrottler is destructed.
   */
  void PostTask(const tracked_objects::Location& aLocation,
                UniquePtr<CancelableTask> aTask, const TimeStamp& aTimeStamp);
  /**
   * Mark the task as complete and process the next queued task.
   */
  void TaskComplete(const TimeStamp& aTimeStamp);

  /**
   * Calculate the average time between processing the posted task and getting
   * the TaskComplete() call back.
   */
  TimeDuration AverageDuration();

  /**
   * Cancel the queued task if there is one.
   */
  void CancelPendingTask();

  /**
   * Return the time elapsed since the last request was processed
   */
  TimeDuration TimeSinceLastRequest(const TimeStamp& aTimeStamp);

  /**
   * Clear average history.
   */
  void ClearHistory();

  /**
   * @param aMaxDurations The maximum number of durations to measure.
   */

  void SetMaxDurations(uint32_t aMaxDurations);

private:
  mutable Monitor mMonitor;
  bool mOutstanding;
  UniquePtr<CancelableTask> mQueuedTask;
  TimeStamp mStartTime;
  TimeDuration mMaxWait;
  RollingMean<TimeDuration, TimeDuration> mMean;
  CancelableTask* mTimeoutTask;  // not owned because it's posted to a MessageLoop
                                 // which deletes it

  ~TaskThrottler();
  void RunQueuedTask(const TimeStamp& aTimeStamp,
                     const MonitorAutoLock& aProofOfLock);
  void CancelPendingTask(const MonitorAutoLock& aProofOfLock);
  TimeDuration TimeSinceLastRequest(const TimeStamp& aTimeStamp,
                                    const MonitorAutoLock& aProofOfLock);
  void OnTimeout();
  void CancelTimeoutTask(const MonitorAutoLock& aProofOfLock);
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_dom_TaskThrottler_h
