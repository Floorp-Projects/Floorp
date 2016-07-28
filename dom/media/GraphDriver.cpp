/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <MediaStreamGraphImpl.h>
#include "mozilla/dom/AudioContext.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/unused.h"
#include "CubebUtils.h"

#ifdef MOZ_WEBRTC
#include "webrtc/MediaEngineWebRTC.h"
#endif

#ifdef XP_MACOSX
#include <sys/sysctl.h>
#endif

extern mozilla::LazyLogModule gMediaStreamGraphLog;
#define STREAM_LOG(type, msg) MOZ_LOG(gMediaStreamGraphLog, type, msg)

// We don't use NSPR log here because we want this interleaved with adb logcat
// on Android/B2G
// #define ENABLE_LIFECYCLE_LOG
#ifdef ENABLE_LIFECYCLE_LOG
#ifdef ANDROID
#include "android/log.h"
#define LIFECYCLE_LOG(...)  __android_log_print(ANDROID_LOG_INFO, "Gecko - MSG" , __VA_ARGS__); printf(__VA_ARGS__);printf("\n");
#else
#define LIFECYCLE_LOG(...) printf(__VA_ARGS__);printf("\n");
#endif
#else
#define LIFECYCLE_LOG(...)
#endif

namespace mozilla {

StaticRefPtr<nsIThreadPool> AsyncCubebTask::sThreadPool;

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
    mGraphImpl(aGraphImpl),
    mWaitState(WAITSTATE_RUNNING),
    mCurrentTimeStamp(TimeStamp::Now()),
    mPreviousDriver(nullptr),
    mNextDriver(nullptr)
{ }

void GraphDriver::SetGraphTime(GraphDriver* aPreviousDriver,
                               GraphTime aLastSwitchNextIterationStart,
                               GraphTime aLastSwitchNextIterationEnd)
{
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  // We set mIterationEnd here, because the first thing a driver do when it
  // does an iteration is to update graph times, so we are in fact setting
  // mIterationStart of the next iteration by setting the end of the previous
  // iteration.
  mIterationStart = aLastSwitchNextIterationStart;
  mIterationEnd = aLastSwitchNextIterationEnd;

  MOZ_ASSERT(!PreviousDriver());
  MOZ_ASSERT(aPreviousDriver);

  STREAM_LOG(LogLevel::Debug, ("Setting previous driver: %p (%s)",
                               aPreviousDriver,
                               aPreviousDriver->AsAudioCallbackDriver()
                                 ? "AudioCallbackDriver"
                                 : "SystemClockDriver"));
  SetPreviousDriver(aPreviousDriver);
}

void GraphDriver::SwitchAtNextIteration(GraphDriver* aNextDriver)
{
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  LIFECYCLE_LOG("Switching to new driver: %p (%s)",
      aNextDriver, aNextDriver->AsAudioCallbackDriver() ?
      "AudioCallbackDriver" : "SystemClockDriver");
  if (mNextDriver &&
      mNextDriver != GraphImpl()->CurrentDriver()) {
    LIFECYCLE_LOG("Discarding previous next driver: %p (%s)",
                  mNextDriver.get(), mNextDriver->AsAudioCallbackDriver() ?
                  "AudioCallbackDriver" : "SystemClockDriver");
  }
  SetNextDriver(aNextDriver);
}

GraphTime
GraphDriver::StateComputedTime() const
{
  return mGraphImpl->mStateComputedTime;
}

void GraphDriver::EnsureNextIteration()
{
  mGraphImpl->EnsureNextIteration();
}

void GraphDriver::Shutdown()
{
  if (AsAudioCallbackDriver()) {
    LIFECYCLE_LOG("Releasing audio driver off main thread (GraphDriver::Shutdown).\n");
    RefPtr<AsyncCubebTask> releaseEvent =
      new AsyncCubebTask(AsAudioCallbackDriver(), AsyncCubebOperation::SHUTDOWN);
    releaseEvent->Dispatch(NS_DISPATCH_SYNC);
  } else {
    Stop();
  }
}

bool GraphDriver::Switching()
{
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  return mNextDriver || mPreviousDriver;
}

GraphDriver* GraphDriver::NextDriver()
{
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  return mNextDriver;
}

GraphDriver* GraphDriver::PreviousDriver()
{
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  return mPreviousDriver;
}

void GraphDriver::SetNextDriver(GraphDriver* aNextDriver)
{
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  mNextDriver = aNextDriver;
}

void GraphDriver::SetPreviousDriver(GraphDriver* aPreviousDriver)
{
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  mPreviousDriver = aPreviousDriver;
}

ThreadedDriver::ThreadedDriver(MediaStreamGraphImpl* aGraphImpl)
  : GraphDriver(aGraphImpl)
{ }

class MediaStreamGraphShutdownThreadRunnable : public Runnable {
public:
  explicit MediaStreamGraphShutdownThreadRunnable(already_AddRefed<nsIThread> aThread)
    : mThread(aThread)
  {
  }
  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mThread);

    mThread->Shutdown();
    mThread = nullptr;
    return NS_OK;
  }
private:
  nsCOMPtr<nsIThread> mThread;
};

ThreadedDriver::~ThreadedDriver()
{
  if (mThread) {
    if (NS_IsMainThread()) {
      mThread->Shutdown();
    } else {
      nsCOMPtr<nsIRunnable> event =
        new MediaStreamGraphShutdownThreadRunnable(mThread.forget());
      NS_DispatchToMainThread(event);
    }
  }
}
class MediaStreamGraphInitThreadRunnable : public Runnable {
public:
  explicit MediaStreamGraphInitThreadRunnable(ThreadedDriver* aDriver)
    : mDriver(aDriver)
  {
  }
  NS_IMETHOD Run()
  {
    char aLocal;
    STREAM_LOG(LogLevel::Debug, ("Starting system thread"));
    profiler_register_thread("MediaStreamGraph", &aLocal);
    LIFECYCLE_LOG("Starting a new system driver for graph %p\n",
                  mDriver->mGraphImpl);

    GraphDriver* previousDriver = nullptr;
    {
      MonitorAutoLock mon(mDriver->mGraphImpl->GetMonitor());
      previousDriver = mDriver->PreviousDriver();
    }
    if (previousDriver) {
      LIFECYCLE_LOG("%p releasing an AudioCallbackDriver(%p), for graph %p\n",
                    mDriver,
                    previousDriver,
                    mDriver->GraphImpl());
      MOZ_ASSERT(!mDriver->AsAudioCallbackDriver());
      RefPtr<AsyncCubebTask> releaseEvent =
        new AsyncCubebTask(previousDriver->AsAudioCallbackDriver(), AsyncCubebOperation::SHUTDOWN);
      releaseEvent->Dispatch();

      MonitorAutoLock mon(mDriver->mGraphImpl->GetMonitor());
      mDriver->SetPreviousDriver(nullptr);
    } else {
      MonitorAutoLock mon(mDriver->mGraphImpl->GetMonitor());
      MOZ_ASSERT(mDriver->mGraphImpl->MessagesQueued() ||
                 mDriver->mGraphImpl->mForceShutDown, "Don't start a graph without messages queued.");
      mDriver->mGraphImpl->SwapMessageQueues();
    }

    mDriver->RunThread();
    return NS_OK;
  }
private:
  RefPtr<ThreadedDriver> mDriver;
};

void
ThreadedDriver::Start()
{
  LIFECYCLE_LOG("Starting thread for a SystemClockDriver  %p\n", mGraphImpl);
  Unused << NS_WARN_IF(mThread);
  if (!mThread) { // Ensure we haven't already started it
    nsCOMPtr<nsIRunnable> event = new MediaStreamGraphInitThreadRunnable(this);
    // Note: mThread may be null during event->Run() if we pass to NewNamedThread!  See AudioInitTask
    nsresult rv = NS_NewNamedThread("MediaStreamGrph", getter_AddRefs(mThread));
    if (NS_SUCCEEDED(rv)) {
      mThread->Dispatch(event, NS_DISPATCH_NORMAL);
    }
  }
}

void
ThreadedDriver::Resume()
{
  Start();
}

void
ThreadedDriver::Revive()
{
  // Note: only called on MainThread, without monitor
  // We know were weren't in a running state
  STREAM_LOG(LogLevel::Debug, ("AudioCallbackDriver reviving."));
  // If we were switching, switch now. Otherwise, tell thread to run the main
  // loop again.
  MonitorAutoLock mon(mGraphImpl->GetMonitor());
  if (NextDriver()) {
    NextDriver()->SetGraphTime(this, mIterationStart, mIterationEnd);
    mGraphImpl->SetCurrentDriver(NextDriver());
    NextDriver()->Start();
  } else {
    nsCOMPtr<nsIRunnable> event = new MediaStreamGraphInitThreadRunnable(this);
    mThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }
}

void
ThreadedDriver::RemoveCallback()
{
}

void
ThreadedDriver::Stop()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  // mGraph's thread is not running so it's OK to do whatever here
  STREAM_LOG(LogLevel::Debug, ("Stopping threads for MediaStreamGraph %p", this));

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }
}

SystemClockDriver::SystemClockDriver(MediaStreamGraphImpl* aGraphImpl)
  : ThreadedDriver(aGraphImpl),
    mInitialTimeStamp(TimeStamp::Now()),
    mLastTimeStamp(TimeStamp::Now()),
    mIsFallback(false)
{}

SystemClockDriver::~SystemClockDriver()
{ }

void
SystemClockDriver::MarkAsFallback()
{
  mIsFallback = true;
}

bool
SystemClockDriver::IsFallback()
{
  return mIsFallback;
}

void
ThreadedDriver::RunThread()
{
  AutoProfilerUnregisterThread autoUnregister;

  bool stillProcessing = true;
  while (stillProcessing) {
    mIterationStart = IterationEnd();
    mIterationEnd += GetIntervalForIteration();

    GraphTime stateComputedTime = StateComputedTime();
    if (stateComputedTime < mIterationEnd) {
      STREAM_LOG(LogLevel::Warning, ("Media graph global underrun detected"));
      mIterationEnd = stateComputedTime;
    }

    if (mIterationStart >= mIterationEnd) {
      NS_ASSERTION(mIterationStart == mIterationEnd ,
                   "Time can't go backwards!");
      // This could happen due to low clock resolution, maybe?
      STREAM_LOG(LogLevel::Debug, ("Time did not advance"));
    }

    GraphTime nextStateComputedTime =
      mGraphImpl->RoundUpToNextAudioBlock(
        mIterationEnd + mGraphImpl->MillisecondsToMediaTime(AUDIO_TARGET_MS));
    if (nextStateComputedTime < stateComputedTime) {
      // A previous driver may have been processing further ahead of
      // iterationEnd.
      STREAM_LOG(LogLevel::Warning,
                 ("Prevent state from going backwards. interval[%ld; %ld] state[%ld; %ld]",
                  (long)mIterationStart, (long)mIterationEnd,
                  (long)stateComputedTime, (long)nextStateComputedTime));
      nextStateComputedTime = stateComputedTime;
    }
    STREAM_LOG(LogLevel::Verbose,
               ("interval[%ld; %ld] state[%ld; %ld]",
               (long)mIterationStart, (long)mIterationEnd,
               (long)stateComputedTime, (long)nextStateComputedTime));

    stillProcessing = mGraphImpl->OneIteration(nextStateComputedTime);

    MonitorAutoLock lock(GraphImpl()->GetMonitor());
    if (NextDriver() && stillProcessing) {
      STREAM_LOG(LogLevel::Debug, ("Switching to AudioCallbackDriver"));
      RemoveCallback();
      NextDriver()->SetGraphTime(this, mIterationStart, mIterationEnd);
      mGraphImpl->SetCurrentDriver(NextDriver());
      NextDriver()->Start();
      return;
    }
  }
}

MediaTime
SystemClockDriver::GetIntervalForIteration()
{
  TimeStamp now = TimeStamp::Now();
  MediaTime interval =
    mGraphImpl->SecondsToMediaTime((now - mCurrentTimeStamp).ToSeconds());
  mCurrentTimeStamp = now;

  MOZ_LOG(gMediaStreamGraphLog, LogLevel::Verbose,
          ("Updating current time to %f (real %f, StateComputedTime() %f)",
           mGraphImpl->MediaTimeToSeconds(IterationEnd() + interval),
           (now - mInitialTimeStamp).ToSeconds(),
           mGraphImpl->MediaTimeToSeconds(StateComputedTime())));

  return interval;
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
  if (mGraphImpl->mNeedAnotherIteration) {
    int64_t timeoutMS = MEDIA_GRAPH_TARGET_PERIOD_MS -
      int64_t((now - mCurrentTimeStamp).ToMilliseconds());
    // Make sure timeoutMS doesn't overflow 32 bits by waking up at
    // least once a minute, if we need to wake up at all
    timeoutMS = std::max<int64_t>(0, std::min<int64_t>(timeoutMS, 60*1000));
    timeout = PR_MillisecondsToInterval(uint32_t(timeoutMS));
    STREAM_LOG(LogLevel::Verbose, ("Waiting for next iteration; at %f, timeout=%f", (now - mInitialTimeStamp).ToSeconds(), timeoutMS/1000.0));
    if (mWaitState == WAITSTATE_WAITING_INDEFINITELY) {
      mGraphImpl->mGraphDriverAsleep = false; // atomic
    }
    mWaitState = WAITSTATE_WAITING_FOR_NEXT_ITERATION;
  } else {
    mGraphImpl->mGraphDriverAsleep = true; // atomic
    mWaitState = WAITSTATE_WAITING_INDEFINITELY;
  }
  if (timeout > 0) {
    mGraphImpl->GetMonitor().Wait(timeout);
    STREAM_LOG(LogLevel::Verbose, ("Resuming after timeout; at %f, elapsed=%f",
          (TimeStamp::Now() - mInitialTimeStamp).ToSeconds(),
          (TimeStamp::Now() - now).ToSeconds()));
  }

  if (mWaitState == WAITSTATE_WAITING_INDEFINITELY) {
    mGraphImpl->mGraphDriverAsleep = false; // atomic
  }
  mWaitState = WAITSTATE_RUNNING;
  mGraphImpl->mNeedAnotherIteration = false;
}

void SystemClockDriver::WakeUp()
{
  mGraphImpl->GetMonitor().AssertCurrentThreadOwns();
  mWaitState = WAITSTATE_WAKING_UP;
  mGraphImpl->mGraphDriverAsleep = false; // atomic
  mGraphImpl->GetMonitor().Notify();
}

OfflineClockDriver::OfflineClockDriver(MediaStreamGraphImpl* aGraphImpl, GraphTime aSlice)
  : ThreadedDriver(aGraphImpl),
    mSlice(aSlice)
{

}

OfflineClockDriver::~OfflineClockDriver()
{
}

MediaTime
OfflineClockDriver::GetIntervalForIteration()
{
  return mGraphImpl->MillisecondsToMediaTime(mSlice);
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

AsyncCubebTask::AsyncCubebTask(AudioCallbackDriver* aDriver, AsyncCubebOperation aOperation)
  : mDriver(aDriver),
    mOperation(aOperation),
    mShutdownGrip(aDriver->GraphImpl())
{
  NS_WARN_IF_FALSE(mDriver->mAudioStream || aOperation == INIT, "No audio stream !");
}

AsyncCubebTask::~AsyncCubebTask()
{
}

/* static */
nsresult
AsyncCubebTask::EnsureThread()
{
  if (!sThreadPool) {
    nsCOMPtr<nsIThreadPool> threadPool =
      SharedThreadPool::Get(NS_LITERAL_CSTRING("CubebOperation"), 1);
    sThreadPool = threadPool;
    // Need to null this out before xpcom-shutdown-threads Observers run
    // since we don't know the order that the shutdown-threads observers
    // will run.  ClearOnShutdown guarantees it runs first.
    if (!NS_IsMainThread()) {
      NS_DispatchToMainThread(NS_NewRunnableFunction([]() -> void {
            ClearOnShutdown(&sThreadPool, ShutdownPhase::ShutdownThreads);
      }));
    } else {
      ClearOnShutdown(&sThreadPool, ShutdownPhase::ShutdownThreads);
    }

    const uint32_t kIdleThreadTimeoutMs = 2000;

    nsresult rv = sThreadPool->SetIdleThreadTimeout(PR_MillisecondsToInterval(kIdleThreadTimeoutMs));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
AsyncCubebTask::Run()
{
  MOZ_ASSERT(mDriver);

  switch(mOperation) {
    case AsyncCubebOperation::INIT: {
      LIFECYCLE_LOG("AsyncCubebOperation::INIT driver=%p\n", mDriver.get());
      mDriver->Init();
      mDriver->CompleteAudioContextOperations(mOperation);
      break;
    }
    case AsyncCubebOperation::SHUTDOWN: {
      LIFECYCLE_LOG("AsyncCubebOperation::SHUTDOWN driver=%p\n", mDriver.get());
      mDriver->Stop();

      mDriver->CompleteAudioContextOperations(mOperation);

      mDriver = nullptr;
      mShutdownGrip = nullptr;
      break;
    }
    default:
      MOZ_CRASH("Operation not implemented.");
  }

  // The thread will kill itself after a bit
  return NS_OK;
}

StreamAndPromiseForOperation::StreamAndPromiseForOperation(MediaStream* aStream,
                                          void* aPromise,
                                          dom::AudioContextOperation aOperation)
  : mStream(aStream)
  , mPromise(aPromise)
  , mOperation(aOperation)
{
  // MOZ_ASSERT(aPromise);
}

AudioCallbackDriver::AudioCallbackDriver(MediaStreamGraphImpl* aGraphImpl)
  : GraphDriver(aGraphImpl)
  , mSampleRate(0)
  , mInputChannels(1)
  , mIterationDurationMS(MEDIA_GRAPH_TARGET_PERIOD_MS)
  , mStarted(false)
  , mAudioInput(nullptr)
  , mAudioChannel(aGraphImpl->AudioChannel())
  , mAddedMixer(false)
  , mInCallback(false)
  , mMicrophoneActive(false)
  , mFromFallback(false)
{
  STREAM_LOG(LogLevel::Debug, ("AudioCallbackDriver ctor for graph %p", aGraphImpl));
}

AudioCallbackDriver::~AudioCallbackDriver()
{
  MOZ_ASSERT(mPromisesForOperation.IsEmpty());
}

void
AudioCallbackDriver::Init()
{
  cubeb_stream_params output;
  cubeb_stream_params input;
  uint32_t latency_frames;
  bool firstStream = CubebUtils::GetFirstStream();

  MOZ_ASSERT(!NS_IsMainThread(),
      "This is blocking and should never run on the main thread.");

  mSampleRate = output.rate = CubebUtils::PreferredSampleRate();

#if defined(__ANDROID__)
#if defined(MOZ_B2G)
  output.stream_type = CubebUtils::ConvertChannelToCubebType(mAudioChannel);
#else
  output.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif
  if (output.stream_type == CUBEB_STREAM_TYPE_MAX) {
    NS_WARNING("Bad stream type");
    return;
  }
#else
  (void)mAudioChannel;
#endif

  output.channels = mGraphImpl->AudioChannelCount();
  if (AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_S16) {
    output.format = CUBEB_SAMPLE_S16NE;
  } else {
    output.format = CUBEB_SAMPLE_FLOAT32NE;
  }

  if (cubeb_get_min_latency(CubebUtils::GetCubebContext(), output, &latency_frames) != CUBEB_OK) {
    NS_WARNING("Could not get minimal latency from cubeb.");
    return;
  }

  input = output;
  input.channels = mInputChannels; // change to support optional stereo capture

  cubeb_stream* stream = nullptr;
  CubebUtils::AudioDeviceID input_id = nullptr, output_id = nullptr;
  // We have to translate the deviceID values to cubeb devid's since those can be
  // freed whenever enumerate is called.
  {
#ifdef MOZ_WEBRTC
    StaticMutexAutoLock lock(AudioInputCubeb::Mutex());
#endif
    if ((!mGraphImpl->mInputWanted
#ifdef MOZ_WEBRTC
         || AudioInputCubeb::GetDeviceID(mGraphImpl->mInputDeviceID, input_id)
#endif
         ) &&
        (mGraphImpl->mOutputDeviceID == -1 // pass nullptr for ID for default output
#ifdef MOZ_WEBRTC
         // XXX we should figure out how we would use a deviceID for output without webrtc.
         // Currently we don't set this though, so it's ok
         || AudioInputCubeb::GetDeviceID(mGraphImpl->mOutputDeviceID, output_id)
#endif
         ) &&
        // XXX Only pass input input if we have an input listener.  Always
        // set up output because it's easier, and it will just get silence.
        // XXX Add support for adding/removing an input listener later.
        cubeb_stream_init(CubebUtils::GetCubebContext(), &stream,
                          "AudioCallbackDriver",
                          input_id,
                          mGraphImpl->mInputWanted ? &input : nullptr,
                          output_id,
                          mGraphImpl->mOutputWanted ? &output : nullptr, latency_frames,
                          DataCallback_s, StateCallback_s, this) == CUBEB_OK) {
      mAudioStream.own(stream);
      DebugOnly<int> rv = cubeb_stream_set_volume(mAudioStream, CubebUtils::GetVolumeScale());
      NS_WARN_IF_FALSE(rv == CUBEB_OK,
          "Could not set the audio stream volume in GraphDriver.cpp");
      CubebUtils::ReportCubebBackendUsed();
    } else {
#ifdef MOZ_WEBRTC
      StaticMutexAutoUnlock unlock(AudioInputCubeb::Mutex());
#endif
      NS_WARNING("Could not create a cubeb stream for MediaStreamGraph, falling back to a SystemClockDriver");
      // Only report failures when we're not coming from a driver that was
      // created itself as a fallback driver because of a previous audio driver
      // failure.
      if (!mFromFallback) {
        CubebUtils::ReportCubebStreamInitFailure(firstStream);
      }
      // Fall back to a driver using a normal thread.
      MonitorAutoLock lock(GraphImpl()->GetMonitor());
      SystemClockDriver* nextDriver = new SystemClockDriver(GraphImpl());
      SetNextDriver(nextDriver);
      nextDriver->MarkAsFallback();
      nextDriver->SetGraphTime(this, mIterationStart, mIterationEnd);
      mGraphImpl->SetCurrentDriver(nextDriver);
      nextDriver->Start();
      return;
    }
  }
  bool aec;
  Unused << mGraphImpl->AudioTrackPresent(aec);
  SetMicrophoneActive(aec);

  cubeb_stream_register_device_changed_callback(mAudioStream,
                                                AudioCallbackDriver::DeviceChangedCallback_s);

  StartStream();

  STREAM_LOG(LogLevel::Debug, ("AudioCallbackDriver started."));
}


void
AudioCallbackDriver::Destroy()
{
  STREAM_LOG(LogLevel::Debug, ("AudioCallbackDriver destroyed."));
  mAudioInput = nullptr;
  mAudioStream.reset();
}

void
AudioCallbackDriver::Resume()
{
  STREAM_LOG(LogLevel::Debug, ("Resuming audio threads for MediaStreamGraph %p", mGraphImpl));
  if (cubeb_stream_start(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not start cubeb stream for MSG.");
  }
}

void
AudioCallbackDriver::Start()
{
  if (mPreviousDriver) {
    if (mPreviousDriver->AsAudioCallbackDriver()) {
      LIFECYCLE_LOG("Releasing audio driver off main thread.");
      RefPtr<AsyncCubebTask> releaseEvent =
        new AsyncCubebTask(mPreviousDriver->AsAudioCallbackDriver(),
                           AsyncCubebOperation::SHUTDOWN);
      releaseEvent->Dispatch();
      mPreviousDriver = nullptr;
    } else {
      LIFECYCLE_LOG("Dropping driver reference for SystemClockDriver.");
      MOZ_ASSERT(mPreviousDriver->AsSystemClockDriver());
      mFromFallback = mPreviousDriver->AsSystemClockDriver()->IsFallback();
      mPreviousDriver = nullptr;
    }
  }

  LIFECYCLE_LOG("Starting new audio driver off main thread, "
                "to ensure it runs after previous shutdown.");
  RefPtr<AsyncCubebTask> initEvent =
    new AsyncCubebTask(AsAudioCallbackDriver(), AsyncCubebOperation::INIT);
  initEvent->Dispatch();
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
  // Note: only called on MainThread, without monitor
  // We know were weren't in a running state
  STREAM_LOG(LogLevel::Debug, ("AudioCallbackDriver reviving."));
  // If we were switching, switch now. Otherwise, start the audio thread again.
  MonitorAutoLock mon(mGraphImpl->GetMonitor());
  if (NextDriver()) {
    RemoveCallback();
    NextDriver()->SetGraphTime(this, mIterationStart, mIterationEnd);
    mGraphImpl->SetCurrentDriver(NextDriver());
    NextDriver()->Start();
  } else {
    STREAM_LOG(LogLevel::Debug, ("Starting audio threads for MediaStreamGraph %p from a new thread.", mGraphImpl));
    RefPtr<AsyncCubebTask> initEvent =
      new AsyncCubebTask(this, AsyncCubebOperation::INIT);
    initEvent->Dispatch();
  }
}

void
AudioCallbackDriver::RemoveCallback()
{
  if (mAddedMixer) {
    mGraphImpl->mMixer.RemoveCallback(this);
    mAddedMixer = false;
  }
}

void
AudioCallbackDriver::WaitForNextIteration()
{
}

void
AudioCallbackDriver::WakeUp()
{
  mGraphImpl->GetMonitor().AssertCurrentThreadOwns();
  mGraphImpl->GetMonitor().Notify();
}

/* static */ long
AudioCallbackDriver::DataCallback_s(cubeb_stream* aStream,
                                    void* aUser,
                                    const void* aInputBuffer,
                                    void* aOutputBuffer,
                                    long aFrames)
{
  AudioCallbackDriver* driver = reinterpret_cast<AudioCallbackDriver*>(aUser);
  return driver->DataCallback(static_cast<const AudioDataValue*>(aInputBuffer),
                              static_cast<AudioDataValue*>(aOutputBuffer), aFrames);
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
  return mInCallback;
}

AudioCallbackDriver::AutoInCallback::AutoInCallback(AudioCallbackDriver* aDriver)
  : mDriver(aDriver)
{
  mDriver->mInCallback = true;
}

AudioCallbackDriver::AutoInCallback::~AutoInCallback() {
  mDriver->mInCallback = false;
}

long
AudioCallbackDriver::DataCallback(const AudioDataValue* aInputBuffer,
                                  AudioDataValue* aOutputBuffer, long aFrames)
{
  bool stillProcessing;

  // Don't add the callback until we're inited and ready
  if (!mAddedMixer) {
    mGraphImpl->mMixer.AddCallback(this);
    mAddedMixer = true;
  }

#ifdef DEBUG
  // DebugOnly<> doesn't work here... it forces an initialization that will cause
  // mInCallback to be set back to false before we exit the statement.  Do it by
  // hand instead.
  AutoInCallback aic(this);
#endif

  GraphTime stateComputedTime = StateComputedTime();
  if (stateComputedTime == 0) {
    MonitorAutoLock mon(mGraphImpl->GetMonitor());
    // Because this function is called during cubeb_stream_init (to prefill the
    // audio buffers), it can be that we don't have a message here (because this
    // driver is the first one for this graph), and the graph would exit. Simply
    // return here until we have messages.
    if (!mGraphImpl->MessagesQueued()) {
      PodZero(aOutputBuffer, aFrames * mGraphImpl->AudioChannelCount());
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
    mIterationDurationMS = (mIterationDurationMS*3) + durationMS;
    mIterationDurationMS /= 4;
  }

  // Process mic data if any/needed
  if (aInputBuffer) {
    if (mAudioInput) { // for this specific input-only or full-duplex stream
      mAudioInput->NotifyInputData(mGraphImpl, aInputBuffer,
                                   static_cast<size_t>(aFrames),
                                   mSampleRate, mInputChannels);
    }
  }

  mBuffer.SetBuffer(aOutputBuffer, aFrames);
  // fill part or all with leftover data from last iteration (since we
  // align to Audio blocks)
  mScratchBuffer.Empty(mBuffer);
  // if we totally filled the buffer (and mScratchBuffer isn't empty),
  // we don't need to run an iteration and if we do so we may overflow.
  if (mBuffer.Available()) {

    // State computed time is decided by the audio callback's buffer length. We
    // compute the iteration start and end from there, trying to keep the amount
    // of buffering in the graph constant.
    GraphTime nextStateComputedTime =
      mGraphImpl->RoundUpToNextAudioBlock(stateComputedTime + mBuffer.Available());

    mIterationStart = mIterationEnd;
    // inGraph is the number of audio frames there is between the state time and
    // the current time, i.e. the maximum theoretical length of the interval we
    // could use as [mIterationStart; mIterationEnd].
    GraphTime inGraph = stateComputedTime - mIterationStart;
    // We want the interval [mIterationStart; mIterationEnd] to be before the
    // interval [stateComputedTime; nextStateComputedTime]. We also want
    // the distance between these intervals to be roughly equivalent each time, to
    // ensure there is no clock drift between current time and state time. Since
    // we can't act on the state time because we have to fill the audio buffer, we
    // reclock the current time against the state time, here.
    mIterationEnd = mIterationStart + 0.8 * inGraph;

    STREAM_LOG(LogLevel::Verbose, ("interval[%ld; %ld] state[%ld; %ld] (frames: %ld) (durationMS: %u) (duration ticks: %ld)\n",
                                   (long)mIterationStart, (long)mIterationEnd,
                                   (long)stateComputedTime, (long)nextStateComputedTime,
                                   (long)aFrames, (uint32_t)durationMS,
                                   (long)(nextStateComputedTime - stateComputedTime)));

    mCurrentTimeStamp = TimeStamp::Now();

    if (stateComputedTime < mIterationEnd) {
      STREAM_LOG(LogLevel::Warning, ("Media graph global underrun detected"));
      mIterationEnd = stateComputedTime;
    }

    stillProcessing = mGraphImpl->OneIteration(nextStateComputedTime);
  } else {
    STREAM_LOG(LogLevel::Verbose, ("DataCallback buffer filled entirely from scratch buffer, skipping iteration."));
    stillProcessing = true;
  }

  mBuffer.BufferFilled();

  // Callback any observers for the AEC speaker data.  Note that one
  // (maybe) of these will be full-duplex, the others will get their input
  // data off separate cubeb callbacks.  Take care with how stuff is
  // removed/added to this list and TSAN issues, but input and output will
  // use separate callback methods.
  mGraphImpl->NotifyOutputData(aOutputBuffer, static_cast<size_t>(aFrames),
                               mSampleRate, ChannelCount);

  bool switching = false;
  {
    MonitorAutoLock mon(mGraphImpl->GetMonitor());
    switching = !!NextDriver();
  }

  if (switching && stillProcessing) {
    // If the audio stream has not been started by the previous driver or
    // the graph itself, keep it alive.
    MonitorAutoLock mon(mGraphImpl->GetMonitor());
    if (!IsStarted()) {
      return aFrames;
    }
    STREAM_LOG(LogLevel::Debug, ("Switching to system driver."));
    RemoveCallback();
    NextDriver()->SetGraphTime(this, mIterationStart, mIterationEnd);
    mGraphImpl->SetCurrentDriver(NextDriver());
    NextDriver()->Start();
    // Returning less than aFrames starts the draining and eventually stops the
    // audio thread. This function will never get called again.
    return aFrames - 1;
  }

  if (!stillProcessing) {
    LIFECYCLE_LOG("Stopping audio thread for MediaStreamGraph %p", this);
    return aFrames - 1;
  }
  return aFrames;
}

void
AudioCallbackDriver::StateCallback(cubeb_state aState)
{
  STREAM_LOG(LogLevel::Debug, ("AudioCallbackDriver State: %d", aState));
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
    NS_WARNING("DataCallback buffer full, expect frame drops.");
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
  // Tell the audio engine the device has changed, it might want to reset some
  // state.
  MonitorAutoLock mon(mGraphImpl->GetMonitor());
  if (mAudioInput) {
    mAudioInput->DeviceChanged();
  }
#ifdef XP_MACOSX
  PanOutputIfNeeded(mMicrophoneActive);
#endif
}

void
AudioCallbackDriver::SetMicrophoneActive(bool aActive)
{
  MonitorAutoLock mon(mGraphImpl->GetMonitor());

  mMicrophoneActive = aActive;

#ifdef XP_MACOSX
  PanOutputIfNeeded(mMicrophoneActive);
#endif
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

void
AudioCallbackDriver::EnqueueStreamAndPromiseForOperation(MediaStream* aStream,
                                          void* aPromise,
                                          dom::AudioContextOperation aOperation)
{
  MonitorAutoLock mon(mGraphImpl->GetMonitor());
  mPromisesForOperation.AppendElement(StreamAndPromiseForOperation(aStream,
                                                                   aPromise,
                                                                   aOperation));
}

void AudioCallbackDriver::CompleteAudioContextOperations(AsyncCubebOperation aOperation)
{
  AutoTArray<StreamAndPromiseForOperation, 1> array;

  // We can't lock for the whole function because AudioContextOperationCompleted
  // will grab the monitor
  {
    MonitorAutoLock mon(GraphImpl()->GetMonitor());
    array.SwapElements(mPromisesForOperation);
  }

  for (uint32_t i = 0; i < array.Length(); i++) {
    StreamAndPromiseForOperation& s = array[i];
    if ((aOperation == AsyncCubebOperation::INIT &&
         s.mOperation == dom::AudioContextOperation::Resume) ||
        (aOperation == AsyncCubebOperation::SHUTDOWN &&
         s.mOperation != dom::AudioContextOperation::Resume)) {

      GraphImpl()->AudioContextOperationCompleted(s.mStream,
                                                  s.mPromise,
                                                  s.mOperation);
      array.RemoveElementAt(i);
      i--;
    }
  }

  if (!array.IsEmpty()) {
    MonitorAutoLock mon(GraphImpl()->GetMonitor());
    mPromisesForOperation.AppendElements(array);
  }
}


} // namespace mozilla
