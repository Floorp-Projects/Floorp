/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <MediaStreamGraphImpl.h>

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaStreamGraphLog;
#define STREAM_LOG(type, msg) PR_LOG(gMediaStreamGraphLog, type, msg)
#else
#define STREAM_LOG(type, msg)
#endif

namespace mozilla {

struct AutoProfilerUnregisterThread
{
  // The empty ctor is used to silence a pre-4.8.0 GCC unused variable warning.
  AutoProfilerUnregisterThread()
  {
  }

  ~AutoProfilerUnregisterThread()
  {
    profiler_unregister_thread();
  }
};

GraphDriver::GraphDriver(MediaStreamGraphImpl* aGraphImpl)
    : mIterationStart(INITIAL_CURRENT_TIME),
      mIterationEnd(INITIAL_CURRENT_TIME),
      mStateComputedTime(INITIAL_CURRENT_TIME),
      mGraphImpl(aGraphImpl),
      mWaitState(WAITSTATE_RUNNING),
      mNeedAnotherIteration(false),
      mMonitor("MediaStreamGraphMonitor")
  { }

DriverHolder::DriverHolder(MediaStreamGraphImpl* aGraphImpl)
  : mGraphImpl(aGraphImpl),
    mLastSwitchOffset(0) // init at INITIAL_CURRENT_TIME ?
{ }

GraphTime
DriverHolder::GetCurrentTime()
{
  MOZ_ASSERT(mDriver, "Can't get current time without a clock.");
  return mLastSwitchOffset + mDriver->GetCurrentTime();
}

void
DriverHolder::Switch(GraphDriver* aDriver)
{
  if (mDriver) {
    mLastSwitchOffset = mDriver->GetCurrentTime();
  }
  mDriver = aDriver;
}

SystemClockDriver::SystemClockDriver(MediaStreamGraphImpl* aGraphImpl)
  : GraphDriver(aGraphImpl),
    mInitialTimeStamp(TimeStamp::Now()),
    mLastTimeStamp(TimeStamp::Now()),
    mCurrentTimeStamp(TimeStamp::Now())
{}

SystemClockDriver::~SystemClockDriver()
{ }

class MediaStreamGraphInitThreadRunnable : public nsRunnable {
public:
  explicit MediaStreamGraphInitThreadRunnable(GraphDriver* aDriver)
    : mDriver(aDriver)
  {
  }
  NS_IMETHOD Run()
  {
    char aLocal;
    profiler_register_thread("MediaStreamGraph", &aLocal);
    mDriver->RunThread();
    return NS_OK;
  }
private:
  GraphDriver* mDriver;
};

void
SystemClockDriver::Start()
{
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphInitThreadRunnable(this);
  NS_NewNamedThread("MediaStreamGrph", getter_AddRefs(mThread), event);
}

void
SystemClockDriver::Dispatch(nsIRunnable* aEvent)
{
  mThread->Dispatch(aEvent, NS_DISPATCH_NORMAL);
}

void
SystemClockDriver::Stop()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  // mGraph's thread is not running so it's OK to do whatever here
  STREAM_LOG(PR_LOG_DEBUG, ("Stopping threads for MediaStreamGraph %p", this));

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }
}

void
SystemClockDriver::RunThread()
{
  AutoProfilerUnregisterThread autoUnregister;
  nsTArray<MessageBlock> messageQueue;
  {
    MonitorAutoLock lock(mMonitor);
    messageQueue.SwapElements(mGraphImpl->MessageQueue());
  }
  NS_ASSERTION(!messageQueue.IsEmpty(),
               "Shouldn't have started a graph with empty message queue!");

  while(mGraphImpl->OneIteration(messageQueue));
}

void
SystemClockDriver::GetIntervalForIteration(GraphTime& aFrom, GraphTime& aTo)
{
  TimeStamp now = TimeStamp::Now();
  aFrom = mIterationStart = IterationEnd();
  aTo = mIterationEnd = mGraphImpl->SecondsToMediaTime((now - mCurrentTimeStamp).ToSeconds()) + IterationEnd();

  mCurrentTimeStamp = now;

  PR_LOG(gMediaStreamGraphLog, PR_LOG_DEBUG+1, ("Updating current time to %f (real %f, mStateComputedTime %f)",
             mGraphImpl->MediaTimeToSeconds(aTo),
             (now - mInitialTimeStamp).ToSeconds(),
             mGraphImpl->MediaTimeToSeconds(StateComputedTime())));

  if (mStateComputedTime < aTo) {
    STREAM_LOG(PR_LOG_WARNING, ("Media graph global underrun detected"));
    aTo = mIterationEnd = mStateComputedTime;
  }

  if (aFrom >= aTo) {
    NS_ASSERTION(aFrom == aTo , "Time can't go backwards!");
    // This could happen due to low clock resolution, maybe?
    STREAM_LOG(PR_LOG_DEBUG, ("Time did not advance"));
  }
}

GraphTime
SystemClockDriver::GetCurrentTime()
{
  return IterationEnd();
}

TimeStamp
SystemClockDriver::GetCurrentTimeStamp()
{
  return mCurrentTimeStamp;
}

void
SystemClockDriver::DoIteration(nsTArray<MessageBlock>& aMessageQueue)
{
  mGraphImpl->DoIteration(aMessageQueue);
}

void
SystemClockDriver::WaitForNextIteration()
{
  PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT;
  TimeStamp now = TimeStamp::Now();
  if (mNeedAnotherIteration) {
    int64_t timeoutMS = MEDIA_GRAPH_TARGET_PERIOD_MS -
      int64_t((now - mCurrentTimeStamp).ToMilliseconds());
    // Make sure timeoutMS doesn't overflow 32 bits by waking up at
    // least once a minute, if we need to wake up at all
    timeoutMS = std::max<int64_t>(0, std::min<int64_t>(timeoutMS, 60*1000));
    timeout = PR_MillisecondsToInterval(uint32_t(timeoutMS));
    STREAM_LOG(PR_LOG_DEBUG+1, ("Waiting for next iteration; at %f, timeout=%f", (now - mInitialTimeStamp).ToSeconds(), timeoutMS/1000.0));
    mWaitState = WAITSTATE_WAITING_FOR_NEXT_ITERATION;
  } else {
    mWaitState = WAITSTATE_WAITING_INDEFINITELY;
    mGraphImpl->PausedIndefinitly();
  }
  if (timeout > 0) {
    mMonitor.Wait(timeout);
    STREAM_LOG(PR_LOG_DEBUG+1, ("Resuming after timeout; at %f, elapsed=%f",
          (TimeStamp::Now() - mInitialTimeStamp).ToSeconds(),
          (TimeStamp::Now() - now).ToSeconds()));
  }

  mGraphImpl->ResumedFromPaused();
  mWaitState = WAITSTATE_RUNNING;
  mNeedAnotherIteration = false;
}

void
SystemClockDriver::WakeUp()
{
  mWaitState = WAITSTATE_WAKING_UP;
  mMonitor.Notify();
}

OfflineClockDriver::OfflineClockDriver(MediaStreamGraphImpl* aGraphImpl, GraphTime aSlice)
  : GraphDriver(aGraphImpl),
    mSlice(aSlice)
{

}

OfflineClockDriver::~OfflineClockDriver()
{ }

void
OfflineClockDriver::Start()
{
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphInitThreadRunnable(this);
  NS_NewNamedThread("MediaStreamGrph", getter_AddRefs(mThread), event);
}

void
OfflineClockDriver::Stop()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  // mGraph's thread is not running so it's OK to do whatever here
  STREAM_LOG(PR_LOG_DEBUG, ("Stopping threads for MediaStreamGraph %p", this));

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }
}

void
OfflineClockDriver::Dispatch(nsIRunnable* aEvent)
{
  mThread->Dispatch(aEvent, NS_DISPATCH_NORMAL);
}

void
OfflineClockDriver::RunThread()
{
  AutoProfilerUnregisterThread autoUnregister;
  nsTArray<MessageBlock> messageQueue;
  {
    MonitorAutoLock lock(mMonitor);
    messageQueue.SwapElements(mGraphImpl->MessageQueue());
  }
  NS_ASSERTION(!messageQueue.IsEmpty(),
               "Shouldn't have started a graph with empty message queue!");

  while(mGraphImpl->OneIteration(messageQueue));
}

void
OfflineClockDriver::GetIntervalForIteration(GraphTime& aFrom, GraphTime& aTo)
{
  aFrom = mIterationStart = IterationEnd();
  aTo = mIterationEnd = IterationEnd() + mGraphImpl->MillisecondsToMediaTime(mSlice);
  PR_LOG(gMediaStreamGraphLog, PR_LOG_DEBUG+1, ("Updating offline current time to %f (%f)",
             mGraphImpl->MediaTimeToSeconds(aTo),
             mGraphImpl->MediaTimeToSeconds(StateComputedTime())));

  if (mStateComputedTime < aTo) {
    STREAM_LOG(PR_LOG_WARNING, ("Media graph global underrun detected"));
    aTo = mIterationEnd = mStateComputedTime;
  }

  if (aFrom >= aTo) {
    NS_ASSERTION(aFrom == aTo , "Time can't go backwards!");
    // This could happen due to low clock resolution, maybe?
    STREAM_LOG(PR_LOG_DEBUG, ("Time did not advance"));
  }
}

GraphTime
OfflineClockDriver::GetCurrentTime()
{
  return mIterationEnd;
}

void
OfflineClockDriver::DoIteration(nsTArray<MessageBlock>& aMessageQueue)
{
  mGraphImpl->DoIteration(aMessageQueue);
}

void
OfflineClockDriver::WaitForNextIteration()
{
  // No op: we want to go as fast as possible when we are offline
}

void
OfflineClockDriver::WakeUp()
{
  MOZ_ASSERT(false, "An offline graph should not have to wake up.");
}



} // namepace mozilla
