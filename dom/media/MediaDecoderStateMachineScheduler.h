/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaDecoderStateMachineScheduler_h__
#define MediaDecoderStateMachineScheduler_h__

#include "nsCOMPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/DebugOnly.h"

class nsITimer;
class nsIEventTarget;

namespace mozilla {

class ReentrantMonitor;

class MediaDecoderStateMachineScheduler {
  enum State {
    SCHEDULER_STATE_NONE,
    SCHEDULER_STATE_FROZEN,
    SCHEDULER_STATE_FROZEN_WITH_PENDING_TASK,
    SCHEDULER_STATE_SHUTDOWN
  };
public:
  MediaDecoderStateMachineScheduler(ReentrantMonitor& aMonitor,
                                    nsresult (*aTimeoutCallback)(void*),
                                    void* aClosure, bool aRealTime);
  ~MediaDecoderStateMachineScheduler();
  nsresult Init();
  nsresult Schedule(int64_t aUsecs = 0);
  void ScheduleAndShutdown();
  nsresult TimeoutExpired(int aTimerId);
  void FreezeScheduling();
  void ThawScheduling();

  bool OnStateMachineThread() const;
  bool IsScheduled() const;

  bool IsRealTime() const {
    return mRealTime;
  }

  nsIEventTarget* GetStateMachineThread() const {
    return mEventTarget;
  }

  bool IsFrozen() const {
    return mState == SCHEDULER_STATE_FROZEN ||
           mState == SCHEDULER_STATE_FROZEN_WITH_PENDING_TASK;
  }

private:
  void ResetTimer();

  // Callback function provided by MediaDecoderStateMachine to run
  // state machine cycles.
  nsresult (*const mTimeoutCallback)(void*);
  // Since StateMachineScheduler will never outlive the state machine,
  // it is safe to keep a raw pointer only to avoid reference cycles.
  void* const mClosure;
  // True is we are decoding a realtime stream, like a camera stream
  const bool mRealTime;
  // Monitor of the decoder
  ReentrantMonitor& mMonitor;
  // State machine thread
  const nsCOMPtr<nsIEventTarget> mEventTarget;
  // Timer to schedule callbacks to run the state machine cycles.
  nsCOMPtr<nsITimer> mTimer;
  // Timestamp at which the next state machine cycle will run.
  TimeStamp mTimeout;
  // The id of timer tasks, timer callback will only run if id matches.
  int mTimerId;
  // No more state machine cycles in shutdown state.
  State mState;

  // Used to check if state machine cycles are running in sequence.
  DebugOnly<bool> mInRunningStateMachine;
};

} // namespace mozilla

#endif // MediaDecoderStateMachineScheduler_h__
