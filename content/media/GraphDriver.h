/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GRAPHDRIVER_H_
#define GRAPHDRIVER_H_

namespace mozilla {

/**
 * We make the initial graph time nonzero so that zero times can have
 * special meaning if necessary.
 */
static const int32_t INITIAL_CURRENT_TIME = 1;

class MediaStreamGraphImpl;
class MessageBlock;

/**
 * Microseconds relative to the start of the graph timeline.
 */
typedef int64_t GraphTime;
const GraphTime GRAPH_TIME_MAX = MEDIA_TIME_MAX;

/**
 * A driver is responsible for the scheduling of the processing, the thread
 * management, and give the different clocks to a MediaStreamGraph. This is an
 * abstract base class. A MediaStreamGraph can be driven by an
 * OfflineClockDriver, if the graph is offline, or a SystemClockDriver, if the
 * graph is real time.
 * A MediaStreamGraph holds an owning reference to its driver.
 */
class GraphDriver
{
public:
  GraphDriver(MediaStreamGraphImpl* aGraphImpl);
  virtual ~GraphDriver()
  { }

  /**
   * Runs main control loop on the graph thread. Normally a single invocation
   * of this runs for the entire lifetime of the graph thread.
   */
  virtual void RunThread() = 0;
  /* When the graph wakes up to do an iteration, this returns the range of time
   * that will be processed. */
  virtual void GetIntervalForIteration(GraphTime& aFrom,
                                       GraphTime& aTo) = 0;
  /* Returns the current time for this graph. This is the end of the current
   * iteration. */
  virtual GraphTime GetCurrentTime() = 0;
  /* For real-time graphs, this waits until it's time to process more data. For
   * offline graphs, this is a no-op. */
  virtual void WaitForNextIteration() = 0;
  /* Wakes up the graph if it is waiting. */
  virtual void WakeUp() = 0;
  /* Start the graph, creating the thread. */
  virtual void Start() = 0;
  /* Stop the graph, shutting down the thread. */
  virtual void Stop() = 0;
  /* Dispatch an event to the graph's thread. */
  virtual void Dispatch(nsIRunnable* aEvent) = 0;

  /**
   * If we are running a real time graph, get the current time stamp to schedule
   * video frames. This has to be reimplemented by real time drivers.
   */
  virtual TimeStamp GetCurrentTimeStamp() {
    MOZ_ASSERT(false, "This clock does not support getting the current time stamp.");
    return TimeStamp();
  }

  bool IsWaiting() {
    return mWaitState == WAITSTATE_WAITING_INDEFINITELY ||
           mWaitState == WAITSTATE_WAITING_FOR_NEXT_ITERATION;
  }

  bool IsWaitingIndefinitly() {
    return mWaitState == WAITSTATE_WAITING_INDEFINITELY;
  }

  GraphTime IterationStart() {
    return mIterationStart;
  }

  GraphTime IterationEnd() {
    return mIterationEnd;
  }

  GraphTime StateComputedTime() {
    return mStateComputedTime;
  }

  /**
   * Whenever the graph has computed the time until it has all state
   * (mStateComputedState), it calls this to indicate the new time until which
   * we have computed state.
   */
  void UpdateStateComputedTime(GraphTime aStateComputedTime) {
    MOZ_ASSERT(aStateComputedTime > mIterationEnd);

    mStateComputedTime = aStateComputedTime;
  }

  Monitor& GetThreadMonitor() {
    return mMonitor;
  }

  /**
   * Call this to indicate that another iteration of the control loop is
   * required immediately. The monitor must already be held.
   */
  void EnsureImmediateWakeUpLocked() {
    mMonitor.AssertCurrentThreadOwns();
    mWaitState = WAITSTATE_WAKING_UP;
    mMonitor.Notify();
  }

  /**
   * Call this to indicate that another iteration of the control loop is
   * required on its regular schedule. The monitor must not be held.
   * This function has to be idempotent.
   */
  void EnsureNextIteration() {
    MonitorAutoLock lock(mMonitor);
    EnsureNextIterationLocked();
  }

  /**
   * Same thing, but not locked.
   */
  void EnsureNextIterationLocked() {
    mMonitor.AssertCurrentThreadOwns();

    if (mNeedAnotherIteration) {
      return;
    }
    mNeedAnotherIteration = true;
    if (IsWaitingIndefinitly()) {
      WakeUp();
    }
  }

protected:
  // Time of the start of this graph iteration.
  GraphTime mIterationStart;
  // Time of the end of this graph iteration.
  GraphTime mIterationEnd;
  // Time, in the future, for which blocking has been computed.
  GraphTime mStateComputedTime;
  // The MediaStreamGraphImpl that owns this driver. This has a lifetime longer
  // than the driver, and will never be null.
  MediaStreamGraphImpl* mGraphImpl;

  /**
   * This enum specifies the wait state of the graph thread.
   */
  enum WaitState {
    // RunThread() is running normally
    WAITSTATE_RUNNING,
    // RunThread() is paused waiting for its next iteration, which will
    // happen soon
    WAITSTATE_WAITING_FOR_NEXT_ITERATION,
    // RunThread() is paused indefinitely waiting for something to change
    WAITSTATE_WAITING_INDEFINITELY,
    // Something has signaled RunThread() to wake up immediately,
    // but it hasn't done so yet
    WAITSTATE_WAKING_UP
  };
  WaitState mWaitState;

  bool mNeedAnotherIteration;
  // Monitor to synchronize the graph thread and the main thread.
  Monitor mMonitor;
};

/**
 * A driver holder allows a MediaStreamGraph to seamlessly switch between
 * different Drivers, remembering the current time of the graph at time of
 * switch.
 */
class DriverHolder
{
public:
  DriverHolder(MediaStreamGraphImpl* aGraphImpl);
  GraphTime GetCurrentTime();

  void Switch(GraphDriver* aDriver);

  GraphDriver* GetDriver() {
    MOZ_ASSERT(mDriver);
    return mDriver.get();
  }

protected:
  // The current driver
  nsAutoPtr<GraphDriver> mDriver;
  // The lifetime of this pointer is equal to the lifetime of the graph, so it
  // will never be null.
  MediaStreamGraphImpl* mGraphImpl;
  // Time we last switched driver, to properly offset from the new clock, that
  // starts at zero.
  GraphTime mLastSwitchOffset;
};

/**
 * This class is a driver that manages its own thread.
 */
class ThreadedDriver : public GraphDriver
{
public:
  ThreadedDriver(MediaStreamGraphImpl* aGraphImpl);
  virtual ~ThreadedDriver();
  virtual void Start() MOZ_OVERRIDE;
  virtual void Stop() MOZ_OVERRIDE;
  virtual void Dispatch(nsIRunnable* aEvent) MOZ_OVERRIDE;
  void RunThread();
private:
  nsCOMPtr<nsIThread> mThread;
};

/**
 * A SystemClockDriver drives a MediaStreamGraph using a system clock, and waits
 * using a monitor, between each iteration.
 */
class SystemClockDriver : public ThreadedDriver
{
public:
  SystemClockDriver(MediaStreamGraphImpl* aGraphImpl);
  virtual ~SystemClockDriver();
  virtual void GetIntervalForIteration(GraphTime& aFrom,
                                       GraphTime& aTo) MOZ_OVERRIDE;
  virtual GraphTime GetCurrentTime() MOZ_OVERRIDE;
  virtual void WaitForNextIteration() MOZ_OVERRIDE;
  virtual void WakeUp() MOZ_OVERRIDE;

  virtual TimeStamp GetCurrentTimeStamp() MOZ_OVERRIDE;

private:
  TimeStamp mInitialTimeStamp;
  TimeStamp mLastTimeStamp;
  TimeStamp mCurrentTimeStamp;
};

/**
 * An OfflineClockDriver runs the graph as fast as possible, without waiting
 * between iteration.
 */
class OfflineClockDriver : public ThreadedDriver
{
public:
  OfflineClockDriver(MediaStreamGraphImpl* aGraphImpl, GraphTime aSlice);
  virtual ~OfflineClockDriver();
  virtual void GetIntervalForIteration(GraphTime& aFrom,
                                       GraphTime& aTo) MOZ_OVERRIDE;
  virtual GraphTime GetCurrentTime() MOZ_OVERRIDE;
  virtual void WaitForNextIteration() MOZ_OVERRIDE;
  virtual void WakeUp() MOZ_OVERRIDE;

private:
  // Time, in GraphTime, for each iteration
  GraphTime mSlice;
};

}

#endif // GRAPHDRIVER_H_
