/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <MediaStreamGraphImpl.h>
#include "CubebUtils.h"

#ifdef XP_MACOSX
#include <sys/sysctl.h>
#endif

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
  : mIterationStart(0),
    mIterationEnd(0),
    mStateComputedTime(0),
    mNextStateComputedTime(0),
    mGraphImpl(aGraphImpl),
    mWaitState(WAITSTATE_RUNNING),
    mNeedAnotherIteration(false),
    mCurrentTimeStamp(TimeStamp::Now()),
    mPreviousDriver(nullptr),
    mNextDriver(nullptr)
{ }

void GraphDriver::SetGraphTime(GraphDriver* aPreviousDriver,
                               GraphTime aLastSwitchNextIterationStart,
                               GraphTime aLastSwitchNextIterationEnd,
                               GraphTime aLastSwitchStateComputedTime,
                               GraphTime aLastSwitchNextStateComputedTime)
{
  // We set mIterationEnd here, because the first thing a driver do when it
  // does an iteration is to update graph times, so we are in fact setting
  // mIterationStart of the next iteration by setting the end of the previous
  // iteration.
  mIterationStart = aLastSwitchNextIterationStart;
  mIterationEnd = aLastSwitchNextIterationEnd;
  mStateComputedTime = aLastSwitchStateComputedTime;
  mNextStateComputedTime = aLastSwitchNextStateComputedTime;

  STREAM_LOG(PR_LOG_DEBUG, ("Setting previous driver: %p (%s)", aPreviousDriver, aPreviousDriver->AsAudioCallbackDriver() ? "AudioCallbackDriver" : "SystemClockDriver"));
  MOZ_ASSERT(!mPreviousDriver);
  mPreviousDriver = aPreviousDriver;
}

void GraphDriver::SwitchAtNextIteration(GraphDriver* aNextDriver)
{

  STREAM_LOG(PR_LOG_DEBUG, ("Switching to new driver: %p (%s)", aNextDriver, aNextDriver->AsAudioCallbackDriver() ? "AudioCallbackDriver" : "SystemClockDriver"));
  // Sometimes we switch twice to a new driver per iteration, this is probably a
  // bug.
  MOZ_ASSERT(!mNextDriver || !mNextDriver->AsAudioCallbackDriver());
  mNextDriver = aNextDriver;
}

void GraphDriver::EnsureImmediateWakeUpLocked()
{
  mGraphImpl->GetMonitor().AssertCurrentThreadOwns();
  mWaitState = WAITSTATE_WAKING_UP;
  mGraphImpl->GetMonitor().Notify();
}

void GraphDriver::UpdateStateComputedTime(GraphTime aStateComputedTime)
{
  MOZ_ASSERT(aStateComputedTime > mIterationEnd);
  // The next state computed time can be the same as the previous, here: it
  // means the driver would be have been blocking indefinitly, but the graph has
  // been woken up right after having been to sleep.
  if (aStateComputedTime < mStateComputedTime) {
    printf("State time can't go backward %ld < %ld.\n", static_cast<long>(aStateComputedTime), static_cast<long>(mStateComputedTime));
  }

  mStateComputedTime = aStateComputedTime;
}

void GraphDriver::EnsureNextIteration()
{
  MonitorAutoLock lock(mGraphImpl->GetMonitor());
  EnsureNextIterationLocked();
}

void GraphDriver::EnsureNextIterationLocked()
{
  mGraphImpl->GetMonitor().AssertCurrentThreadOwns();

  if (IsWaitingIndefinitly()) {
    WakeUp();
  }

  if (mNeedAnotherIteration) {
    return;
  }
  mNeedAnotherIteration = true;
}

ThreadedDriver::ThreadedDriver(MediaStreamGraphImpl* aGraphImpl)
  : GraphDriver(aGraphImpl)
{ }

ThreadedDriver::~ThreadedDriver()
{
  if (mThread) {
    mThread->Shutdown();
  }
}

class MediaStreamGraphShutdownThreadRunnable : public nsRunnable {
public:
  explicit MediaStreamGraphShutdownThreadRunnable(GraphDriver* aDriver)
    : mDriver(aDriver)
  {
  }
  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    // We can't release an audio driver on the main thread, because it can be
    // blocking.
    if (mDriver->AsAudioCallbackDriver()) {
      STREAM_LOG(PR_LOG_DEBUG, ("Releasing audio driver off main thread.\n"));
      nsRefPtr<AsyncCubebTask> releaseEvent =
        new AsyncCubebTask(mDriver->AsAudioCallbackDriver(), AsyncCubebTask::SHUTDOWN);
      mDriver = nullptr;
      releaseEvent->Dispatch();
    } else {
      mDriver = nullptr;
    }
    return NS_OK;
  }
private:
  nsRefPtr<GraphDriver> mDriver;
};

class MediaStreamGraphInitThreadRunnable : public nsRunnable {
public:
  explicit MediaStreamGraphInitThreadRunnable(ThreadedDriver* aDriver)
    : mDriver(aDriver)
  {
  }
  NS_IMETHOD Run()
  {
    char aLocal;
    STREAM_LOG(PR_LOG_DEBUG, ("Starting system thread"));
    profiler_register_thread("MediaStreamGraph", &aLocal);
    if (mDriver->mPreviousDriver) {
      MOZ_ASSERT(!mDriver->AsAudioCallbackDriver());
      // Stop and release the previous driver off-main-thread.
      nsRefPtr<AsyncCubebTask> releaseEvent =
        new AsyncCubebTask(mDriver->mPreviousDriver->AsAudioCallbackDriver(), AsyncCubebTask::SHUTDOWN);
      mDriver->mPreviousDriver = nullptr;
      releaseEvent->Dispatch();
    } else {
      MonitorAutoLock mon(mDriver->mGraphImpl->GetMonitor());
      MOZ_ASSERT(mDriver->mGraphImpl->MessagesQueued(), "Don't start a graph without messages queued.");
      mDriver->mGraphImpl->SwapMessageQueues();
    }
    mDriver->RunThread();
    return NS_OK;
  }
private:
  ThreadedDriver* mDriver;
};

void
ThreadedDriver::Start()
{
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphInitThreadRunnable(this);
  NS_NewNamedThread("MediaStreamGrph", getter_AddRefs(mThread), event);
}

void
ThreadedDriver::Resume()
{
  Start();
}

void
ThreadedDriver::Revive()
{
  STREAM_LOG(PR_LOG_DEBUG, ("AudioCallbackDriver reviving."));
  // If we were switching, switch now. Otherwise, tell thread to run the main
  // loop again.
  if (mNextDriver) {
    mNextDriver->SetGraphTime(this, mIterationStart, mIterationEnd,
                               mStateComputedTime, mNextStateComputedTime);
    mGraphImpl->SetCurrentDriver(mNextDriver);
    mNextDriver->Start();
  } else {
    nsCOMPtr<nsIRunnable> event = new MediaStreamGraphInitThreadRunnable(this);
    mThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }
}

void
ThreadedDriver::Stop()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  // mGraph's thread is not running so it's OK to do whatever here
  STREAM_LOG(PR_LOG_DEBUG, ("Stopping threads for MediaStreamGraph %p", this));

  if (mThread) {
    mThread->Shutdown();
  }
}

SystemClockDriver::SystemClockDriver(MediaStreamGraphImpl* aGraphImpl)
  : ThreadedDriver(aGraphImpl),
    mInitialTimeStamp(TimeStamp::Now()),
    mLastTimeStamp(TimeStamp::Now())
{}

SystemClockDriver::~SystemClockDriver()
{ }

void
ThreadedDriver::RunThread()
{
  AutoProfilerUnregisterThread autoUnregister;

  bool stillProcessing = true;
  while (stillProcessing) {
    GraphTime prevCurrentTime, nextCurrentTime;
    GetIntervalForIteration(prevCurrentTime, nextCurrentTime);

    mStateComputedTime = mNextStateComputedTime;
    mNextStateComputedTime =
      mGraphImpl->RoundUpToNextAudioBlock(
        nextCurrentTime + mGraphImpl->MillisecondsToMediaTime(AUDIO_TARGET_MS));
    STREAM_LOG(PR_LOG_DEBUG,
               ("interval[%ld; %ld] state[%ld; %ld]",
               (long)mIterationStart, (long)mIterationEnd,
               (long)mStateComputedTime, (long)mNextStateComputedTime));

    stillProcessing = mGraphImpl->OneIteration(prevCurrentTime,
                                               nextCurrentTime,
                                               StateComputedTime(),
                                               mNextStateComputedTime);

    if (mNextDriver && stillProcessing) {
      STREAM_LOG(PR_LOG_DEBUG, ("Switching to AudioCallbackDriver"));
      mNextDriver->SetGraphTime(this, mIterationStart, mIterationEnd,
                                 mStateComputedTime, mNextStateComputedTime);
      mGraphImpl->SetCurrentDriver(mNextDriver);
      mNextDriver->Start();
      return;
    }
  }
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
OfflineClockDriver::GetCurrentTimeStamp()
{
  MOZ_CRASH("This driver does not support getting the current timestamp.");
  return TimeStamp();
}

void
SystemClockDriver::WaitForNextIteration()
{
  mGraphImpl->GetMonitor().AssertCurrentThreadOwns();

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
  }
  if (timeout > 0) {
    mGraphImpl->GetMonitor().Wait(timeout);
    STREAM_LOG(PR_LOG_DEBUG+1, ("Resuming after timeout; at %f, elapsed=%f",
          (TimeStamp::Now() - mInitialTimeStamp).ToSeconds(),
          (TimeStamp::Now() - now).ToSeconds()));
  }

  mWaitState = WAITSTATE_RUNNING;
  mNeedAnotherIteration = false;
}

void
SystemClockDriver::WakeUp()
{
  mGraphImpl->GetMonitor().AssertCurrentThreadOwns();
  mWaitState = WAITSTATE_WAKING_UP;
  mGraphImpl->GetMonitor().Notify();
}

OfflineClockDriver::OfflineClockDriver(MediaStreamGraphImpl* aGraphImpl, GraphTime aSlice)
  : ThreadedDriver(aGraphImpl),
    mSlice(aSlice)
{

}

class MediaStreamGraphShutdownThreadRunnable2 : public nsRunnable {
public:
  explicit MediaStreamGraphShutdownThreadRunnable2(nsIThread* aThread)
    : mThread(aThread)
  {
  }
  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mThread->Shutdown();
    return NS_OK;
  }
private:
  nsRefPtr<nsIThread> mThread;
};

OfflineClockDriver::~OfflineClockDriver()
{
  // transfer the ownership of mThread to the event
  nsCOMPtr<nsIRunnable> event = new MediaStreamGraphShutdownThreadRunnable2(mThread);
  mThread = nullptr;
  NS_DispatchToMainThread(event);
}

void
OfflineClockDriver::GetIntervalForIteration(GraphTime& aFrom, GraphTime& aTo)
{
  aFrom = mIterationStart = IterationEnd();
  aTo = mIterationEnd = IterationEnd() + mGraphImpl->MillisecondsToMediaTime(mSlice);

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
OfflineClockDriver::WaitForNextIteration()
{
  // No op: we want to go as fast as possible when we are offline
}

void
OfflineClockDriver::WakeUp()
{
  MOZ_ASSERT(false, "An offline graph should not have to wake up.");
}


NS_IMETHODIMP
AsyncCubebTask::Run()
{
  MOZ_ASSERT(mThread);
  if (NS_IsMainThread()) {
    mThread->Shutdown(); // can't shutdown from the thread itself, darn
    // don't null out mThread!
    // See bug 999104.  we must hold a ref to the thread across Dispatch()
    // since the internal mthread ref could be released while processing
    // the Dispatch(), and Dispatch/PutEvent itself doesn't hold a ref; it
    // assumes the caller does.
    return NS_OK;
  }

  MOZ_ASSERT(mDriver);

  switch(mOperation) {
    case AsyncCubebOperation::INIT:
      mDriver->Init();
      break;
    case AsyncCubebOperation::SHUTDOWN:
      mDriver->Stop();
      mDriver = nullptr;
      break;
    case AsyncCubebOperation::SLEEP: {
      {
        MonitorAutoLock mon(mDriver->mGraphImpl->GetMonitor());
        // We might just have been awoken
        if (mDriver->mNeedAnotherIteration) {
          mDriver->mPauseRequested = false;
          mDriver->mWaitState = AudioCallbackDriver::WAITSTATE_RUNNING;
          break;
        }
        mDriver->Stop();
        mDriver->mWaitState = AudioCallbackDriver::WAITSTATE_WAITING_INDEFINITELY;
        mDriver->mPauseRequested = false;
        mDriver->mGraphImpl->GetMonitor().Wait(PR_INTERVAL_NO_TIMEOUT);
      }
      STREAM_LOG(PR_LOG_DEBUG, ("Restarting audio stream from sleep."));
      mDriver->StartStream();
      break;
    }
    default:
      MOZ_CRASH("Operation not implemented.");
  }

  // and now kill this thread
  NS_DispatchToMainThread(this);

  return NS_OK;
}

AudioCallbackDriver::AudioCallbackDriver(MediaStreamGraphImpl* aGraphImpl, dom::AudioChannel aChannel)
  : GraphDriver(aGraphImpl)
  , mStarted(false)
  , mAudioChannel(aChannel)
  , mInCallback(false)
  , mPauseRequested(false)
{
  STREAM_LOG(PR_LOG_DEBUG, ("AudioCallbackDriver ctor for graph %p", aGraphImpl));
}

AudioCallbackDriver::~AudioCallbackDriver()
{}

void
AudioCallbackDriver::Init()
{
  cubeb_stream_params params;
  uint32_t latency;

  MOZ_ASSERT(!NS_IsMainThread(),
      "This is blocking and should never run on the main thread.");

  mSampleRate = params.rate = CubebUtils::PreferredSampleRate();

#if defined(__ANDROID__)
#if defined(MOZ_B2G)
  params.stream_type = CubebUtils::ConvertChannelToCubebType(mAudioChannel);
#else
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif
  if (params.stream_type == CUBEB_STREAM_TYPE_MAX) {
    NS_WARNING("Bad stream type");
    return;
  }
#else
  (void)mAudioChannel;
#endif

  params.channels = mGraphImpl->AudioChannelCount();
  if (AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_S16) {
    params.format = CUBEB_SAMPLE_S16NE;
  } else {
    params.format = CUBEB_SAMPLE_FLOAT32NE;
  }

  if (cubeb_get_min_latency(CubebUtils::GetCubebContext(), params, &latency) != CUBEB_OK) {
    NS_WARNING("Could not get minimal latency from cubeb.");
    return;
  }

  cubeb_stream* stream;
  if (cubeb_stream_init(CubebUtils::GetCubebContext(), &stream,
                        "AudioCallbackDriver", params, latency,
                        DataCallback_s, StateCallback_s, this) == CUBEB_OK) {
    mAudioStream.own(stream);
  } else {
    NS_WARNING("Could not create a cubeb stream for MediaStreamGraph.");
    return;
  }

  cubeb_stream_register_device_changed_callback(mAudioStream,
                                                AudioCallbackDriver::DeviceChangedCallback_s);

  StartStream();

  STREAM_LOG(PR_LOG_DEBUG, ("AudioCallbackDriver started."));
}


void
AudioCallbackDriver::Destroy()
{
  STREAM_LOG(PR_LOG_DEBUG, ("AudioCallbackDriver destroyed."));
  mAudioStream.reset();
}

void
AudioCallbackDriver::Resume()
{
  STREAM_LOG(PR_LOG_DEBUG, ("Resuming audio threads for MediaStreamGraph %p", mGraphImpl));
  if (cubeb_stream_start(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not start cubeb stream for MSG.");
  }
}

void
AudioCallbackDriver::Start()
{
  // If this is running on the main thread, we can't open the stream directly,
  // because it is a blocking operation.
  if (NS_IsMainThread()) {
    STREAM_LOG(PR_LOG_DEBUG, ("Starting audio threads for MediaStreamGraph %p from a new thread.", mGraphImpl));
    nsRefPtr<AsyncCubebTask> initEvent =
      new AsyncCubebTask(this, AsyncCubebTask::INIT);
    initEvent->Dispatch();
  } else {
    STREAM_LOG(PR_LOG_DEBUG, ("Starting audio threads for MediaStreamGraph %p from the previous driver's thread", mGraphImpl));
    Init();

    if (mPreviousDriver) {
      nsCOMPtr<nsIRunnable> event =
        new MediaStreamGraphShutdownThreadRunnable(mPreviousDriver);
      mPreviousDriver = nullptr;
      NS_DispatchToMainThread(event);
    }
  }
}

void
AudioCallbackDriver::StartStream()
{
  if (cubeb_stream_start(mAudioStream) != CUBEB_OK) {
    MOZ_CRASH("Could not start cubeb stream for MSG.");
  }

  {
    MonitorAutoLock mon(mGraphImpl->GetMonitor());
    mStarted = true;
    mWaitState = WAITSTATE_RUNNING;
  }
}

void
AudioCallbackDriver::Stop()
{
  if (cubeb_stream_stop(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not stop cubeb stream for MSG.");
  }
}

void
AudioCallbackDriver::Revive()
{
  STREAM_LOG(PR_LOG_DEBUG, ("AudioCallbackDriver reviving."));
  // If we were switching, switch now. Otherwise, start the audio thread again.
  if (mNextDriver) {
    mNextDriver->SetGraphTime(this, mIterationStart, mIterationEnd,
                               mStateComputedTime, mNextStateComputedTime);
    mGraphImpl->SetCurrentDriver(mNextDriver);
    mNextDriver->Start();
  } else {
    Init();
    Start();
  }
}

void
AudioCallbackDriver::GetIntervalForIteration(GraphTime& aFrom,
                                             GraphTime& aTo)
{
}

GraphTime
AudioCallbackDriver::GetCurrentTime()
{
  uint64_t position = 0;

  if (cubeb_stream_get_position(mAudioStream, &position) != CUBEB_OK) {
    NS_WARNING("Could not get current time from cubeb.");
  }

  return mSampleRate * position;
}

void AudioCallbackDriver::WaitForNextIteration()
{
#if 0
  mGraphImpl->GetMonitor().AssertCurrentThreadOwns();

  // We can't block on the monitor in the audio callback, so we kick off a new
  // thread that will pause the audio stream, and restart it when unblocked.
  // We don't want to sleep when we haven't started the driver yet.
  if (!mNeedAnotherIteration && mAudioStream && mGraphImpl->Running()) {
    STREAM_LOG(PR_LOG_DEBUG+1, ("AudioCallbackDriver going to sleep"));
    mPauseRequested = true;
    nsRefPtr<AsyncCubebTask> sleepEvent =
      new AsyncCubebTask(this, AsyncCubebTask::SLEEP);
    sleepEvent->Dispatch();
  }
#endif
}

void
AudioCallbackDriver::WakeUp()
{
  mGraphImpl->GetMonitor().AssertCurrentThreadOwns();
  mGraphImpl->GetMonitor().Notify();
}

/* static */ long
AudioCallbackDriver::DataCallback_s(cubeb_stream* aStream,
                                    void* aUser, void* aBuffer,
                                    long aFrames)
{
  AudioCallbackDriver* driver = reinterpret_cast<AudioCallbackDriver*>(aUser);
  return driver->DataCallback(static_cast<AudioDataValue*>(aBuffer), aFrames);
}

/* static */ void
AudioCallbackDriver::StateCallback_s(cubeb_stream* aStream, void * aUser,
                                     cubeb_state aState)
{
  AudioCallbackDriver* driver = reinterpret_cast<AudioCallbackDriver*>(aUser);
  driver->StateCallback(aState);
}

/* static */ void
AudioCallbackDriver::DeviceChangedCallback_s(void* aUser)
{
  AudioCallbackDriver* driver = reinterpret_cast<AudioCallbackDriver*>(aUser);
  driver->DeviceChangedCallback();
}

bool AudioCallbackDriver::InCallback() {
  MonitorAutoLock mon(mGraphImpl->GetMonitor());
  return mInCallback;
}

AudioCallbackDriver::AutoInCallback::AutoInCallback(AudioCallbackDriver* aDriver)
  : mDriver(aDriver)
{
  MonitorAutoLock mon(mDriver->mGraphImpl->GetMonitor());
  mDriver->mInCallback = true;
}

AudioCallbackDriver::AutoInCallback::~AutoInCallback() {
  MonitorAutoLock mon(mDriver->mGraphImpl->GetMonitor());
  mDriver->mInCallback = false;
}

long
AudioCallbackDriver::DataCallback(AudioDataValue* aBuffer, long aFrames)
{
  bool stillProcessing;

  if (mPauseRequested) {
    PodZero(aBuffer, aFrames * mGraphImpl->AudioChannelCount());
    return aFrames;
  }

  DebugOnly<AutoInCallback> aic(this);

  if (mStateComputedTime == 0) {
    MonitorAutoLock mon(mGraphImpl->GetMonitor());
    // Because this function is called during cubeb_stream_init (to prefill the
    // audio buffers), it can be that we don't have a message here (because this
    // driver is the first one for this graph), and the graph would exit. Simply
    // return here until we have messages.
    if (!mGraphImpl->MessagesQueued()) {
      PodZero(aBuffer, aFrames * mGraphImpl->AudioChannelCount());
      return aFrames;
    }
    mGraphImpl->SwapMessageQueues();
  }

  uint32_t durationMS = aFrames * 1000 / mSampleRate;

  // For now, simply average the duration with the previous
  // duration so there is some damping against sudden changes.
  if (!mIterationDurationMS) {
    mIterationDurationMS = durationMS;
  } else {
    mIterationDurationMS += durationMS;
    mIterationDurationMS /= 2;
  }

  mBuffer.SetBuffer(aBuffer, aFrames);

  mScratchBuffer.Empty(mBuffer);

  mStateComputedTime = mNextStateComputedTime;

  // State computed time is decided by the audio callback's buffer length. We
  // compute the iteration start and end from there, trying to keep the amount
  // of buffering in the graph constant.
  mNextStateComputedTime =
    mGraphImpl->RoundUpToNextAudioBlock(mStateComputedTime + mBuffer.Available());

  mIterationStart = mIterationEnd;
  // inGraph is the number of audio frames there is between the state time and
  // the current time, i.e. the maximum theoretical length of the interval we
  // could use as [mIterationStart; mIterationEnd].
  GraphTime inGraph = mStateComputedTime - mIterationStart;
  // We want the interval [mIterationStart; mIterationEnd] to be before the
  // interval [mStateComputedTime; mNextStateComputedTime]. We also want
  // the distance between these intervals to be roughly equivalent each time, to
  // ensure there is no clock drift between current time and state time. Since
  // we can't act on the state time because we have to fill the audio buffer, we
  // reclock the current time against the state time, here.
  mIterationEnd = mIterationStart + 0.8 * inGraph;

  STREAM_LOG(PR_LOG_DEBUG, ("interval[%ld; %ld] state[%ld; %ld] (frames: %ld) (durationMS: %u) (duration ticks: %ld)\n",
             (long)mIterationStart, (long)mIterationEnd,
             (long)mStateComputedTime, (long)mNextStateComputedTime,
             (long)aFrames, (uint32_t)durationMS,
             (long)(mNextStateComputedTime - mStateComputedTime)));

  mCurrentTimeStamp = TimeStamp::Now();

  if (mStateComputedTime < mIterationEnd) {
    STREAM_LOG(PR_LOG_WARNING, ("Media graph global underrun detected"));
    mIterationEnd = mStateComputedTime;
  }

  stillProcessing = mGraphImpl->OneIteration(mIterationStart,
                                             mIterationEnd,
                                             mStateComputedTime,
                                             mNextStateComputedTime);

  mBuffer.BufferFilled();

  if (mNextDriver && stillProcessing) {
    {
      // If the audio stream has not been started by the previous driver or
      // the graph itself, keep it alive.
      MonitorAutoLock mon(mGraphImpl->GetMonitor());
      if (!IsStarted()) {
        return aFrames;
      }
    }
    STREAM_LOG(PR_LOG_DEBUG, ("Switching to system driver."));
    mNextDriver->SetGraphTime(this, mIterationStart, mIterationEnd,
                               mStateComputedTime, mNextStateComputedTime);
    mGraphImpl->SetCurrentDriver(mNextDriver);
    mNextDriver->Start();
    // Returning less than aFrames starts the draining and eventually stops the
    // audio thread. This function will never get called again.
    return aFrames - 1;
  }

  if (!stillProcessing) {
    STREAM_LOG(PR_LOG_DEBUG, ("Stopping audio thread for MediaStreamGraph %p", this));
    return aFrames - 1;
  }
  return aFrames;
}

void
AudioCallbackDriver::StateCallback(cubeb_state aState)
{
  STREAM_LOG(PR_LOG_DEBUG, ("AudioCallbackDriver State: %d", aState));
}

void
AudioCallbackDriver::MixerCallback(AudioDataValue* aMixedBuffer,
                                   AudioSampleFormat aFormat,
                                   uint32_t aChannels,
                                   uint32_t aFrames,
                                   uint32_t aSampleRate)
{
  uint32_t toWrite = mBuffer.Available();

  if (!mBuffer.Available()) {
    NS_WARNING("MediaStreamGraph SpillBuffer full, expect frame drop.");
  }

  MOZ_ASSERT(mBuffer.Available() <= aFrames);

  mBuffer.WriteFrames(aMixedBuffer, mBuffer.Available());
  MOZ_ASSERT(mBuffer.Available() == 0, "Missing frames to fill audio callback's buffer.");

  DebugOnly<uint32_t> written = mScratchBuffer.Fill(aMixedBuffer + toWrite * aChannels, aFrames - toWrite);
  NS_WARN_IF_FALSE(written == aFrames - toWrite, "Dropping frames.");
};

void AudioCallbackDriver::PanOutputIfNeeded(bool aMicrophoneActive)
{
#ifdef XP_MACOSX
  cubeb_device* out;
  int rv;
  char name[128];
  size_t length = sizeof(name);

  rv = sysctlbyname("hw.model", name, &length, NULL, 0);
  if (rv) {
    return;
  }

  if (!strncmp(name, "MacBookPro", 10)) {
    if (cubeb_stream_get_current_device(mAudioStream, &out) == CUBEB_OK) {
      // Check if we are currently outputing sound on external speakers.
      if (!strcmp(out->output_name, "ispk")) {
        // Pan everything to the right speaker.
        if (aMicrophoneActive) {
          if (cubeb_stream_set_panning(mAudioStream, 1.0) != CUBEB_OK) {
            NS_WARNING("Could not pan audio output to the right.");
          }
        } else {
          if (cubeb_stream_set_panning(mAudioStream, 0.0) != CUBEB_OK) {
            NS_WARNING("Could not pan audio output to the center.");
          }
        }
      } else {
        if (cubeb_stream_set_panning(mAudioStream, 0.0) != CUBEB_OK) {
          NS_WARNING("Could not pan audio output to the center.");
        }
      }
      cubeb_stream_device_destroy(mAudioStream, out);
    }
  }
#endif
}

void
AudioCallbackDriver::DeviceChangedCallback() {
  MonitorAutoLock mon(mGraphImpl->GetMonitor());
  PanOutputIfNeeded(mMicrophoneActive);
}

void
AudioCallbackDriver::SetMicrophoneActive(bool aActive)
{
  MonitorAutoLock mon(mGraphImpl->GetMonitor());

  mMicrophoneActive = aActive;

  PanOutputIfNeeded(mMicrophoneActive);
}

uint32_t
AudioCallbackDriver::IterationDuration()
{
  // The real fix would be to have an API in cubeb to give us the number. Short
  // of that, we approximate it here. bug 1019507
  return mIterationDurationMS;
}

bool
AudioCallbackDriver::IsStarted() {
  mGraphImpl->GetMonitor().AssertCurrentThreadOwns();
  return mStarted;
}


} // namepace mozilla
