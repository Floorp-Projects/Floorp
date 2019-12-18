/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AudioContext.h"
#include "mozilla/dom/AudioDeviceInfo.h"
#include "mozilla/dom/BaseAudioContextBinding.h"
#include "mozilla/dom/WorkletThread.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Unused.h"
#include "mozilla/MathAlgorithms.h"
#include "CubebDeviceEnumerator.h"
#include "Tracing.h"

#ifdef MOZ_WEBRTC
#  include "webrtc/MediaEngineWebRTC.h"
#endif

#ifdef XP_MACOSX
#  include <sys/sysctl.h>
#endif

extern mozilla::LazyLogModule gMediaTrackGraphLog;
#ifdef LOG
#  undef LOG
#endif  // LOG
#define LOG(type, msg) MOZ_LOG(gMediaTrackGraphLog, type, msg)

namespace mozilla {

GraphDriver::GraphDriver(GraphInterface* aGraphInterface,
                         GraphDriver* aPreviousDriver, uint32_t aSampleRate)
    : mGraphInterface(aGraphInterface),
      mSampleRate(aSampleRate),
      mPreviousDriver(aPreviousDriver) {}

void GraphDriver::SetState(GraphTime aIterationStart, GraphTime aIterationEnd,
                           GraphTime aStateComputedTime) {
  MOZ_ASSERT(InIteration() || !ThreadRunning());

  mIterationStart = aIterationStart;
  mIterationEnd = aIterationEnd;
  mStateComputedTime = aStateComputedTime;
}

#ifdef DEBUG
bool GraphDriver::InIteration() { return Graph()->InDriverIteration(this); }
#endif

GraphDriver* GraphDriver::PreviousDriver() {
  MOZ_ASSERT(InIteration() || !ThreadRunning());
  return mPreviousDriver;
}

void GraphDriver::SetPreviousDriver(GraphDriver* aPreviousDriver) {
  MOZ_ASSERT(InIteration() || !ThreadRunning());
  mPreviousDriver = aPreviousDriver;
}

ThreadedDriver::ThreadedDriver(GraphInterface* aGraphInterface,
                               GraphDriver* aPreviousDriver,
                               uint32_t aSampleRate)
    : GraphDriver(aGraphInterface, aPreviousDriver, aSampleRate),
      mThreadRunning(false) {}

class MediaTrackGraphShutdownThreadRunnable : public Runnable {
 public:
  explicit MediaTrackGraphShutdownThreadRunnable(
      already_AddRefed<nsIThread> aThread)
      : Runnable("MediaTrackGraphShutdownThreadRunnable"), mThread(aThread) {}
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mThread);

    mThread->Shutdown();
    mThread = nullptr;
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIThread> mThread;
};

ThreadedDriver::~ThreadedDriver() {
  if (mThread) {
    nsCOMPtr<nsIRunnable> event =
        new MediaTrackGraphShutdownThreadRunnable(mThread.forget());
    SystemGroup::Dispatch(TaskCategory::Other, event.forget());
  }
}

class MediaTrackGraphInitThreadRunnable : public Runnable {
 public:
  explicit MediaTrackGraphInitThreadRunnable(ThreadedDriver* aDriver)
      : Runnable("MediaTrackGraphInitThreadRunnable"), mDriver(aDriver) {}
  NS_IMETHOD Run() override {
    MOZ_ASSERT(!mDriver->ThreadRunning());
    LOG(LogLevel::Debug, ("Starting a new system driver for graph %p",
                          mDriver->mGraphInterface.get()));

    if (GraphDriver* previousDriver = mDriver->PreviousDriver()) {
      LOG(LogLevel::Debug,
          ("%p releasing an AudioCallbackDriver(%p), for graph %p",
           mDriver.get(), previousDriver, mDriver->Graph()));
      MOZ_ASSERT(!mDriver->AsAudioCallbackDriver());
      RefPtr<AsyncCubebTask> releaseEvent =
          new AsyncCubebTask(previousDriver->AsAudioCallbackDriver(),
                             AsyncCubebOperation::SHUTDOWN);
      releaseEvent->Dispatch();
      mDriver->SetPreviousDriver(nullptr);
    }

    mDriver->RunThread();
    return NS_OK;
  }

 private:
  RefPtr<ThreadedDriver> mDriver;
};

void ThreadedDriver::Start() {
  MOZ_ASSERT(!ThreadRunning());
  LOG(LogLevel::Debug,
      ("Starting thread for a SystemClockDriver  %p", mGraphInterface.get()));
  Unused << NS_WARN_IF(mThread);
  MOZ_ASSERT(!mThread);  // Ensure we haven't already started it

  nsCOMPtr<nsIRunnable> event = new MediaTrackGraphInitThreadRunnable(this);
  // Note: mThread may be null during event->Run() if we pass to NewNamedThread!
  // See AudioInitTask
  nsresult rv = NS_NewNamedThread("MediaTrackGrph", getter_AddRefs(mThread));
  if (NS_SUCCEEDED(rv)) {
    mThread->EventTarget()->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
  }
}

void ThreadedDriver::Shutdown() {
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  // mGraph's thread is not running so it's OK to do whatever here
  LOG(LogLevel::Debug, ("Stopping threads for MediaTrackGraph %p", this));

  if (mThread) {
    LOG(LogLevel::Debug,
        ("%p: Stopping ThreadedDriver's %p thread", Graph(), this));
    mThread->Shutdown();
    mThread = nullptr;
  }
}

SystemClockDriver::SystemClockDriver(GraphInterface* aGraphInterface,
                                     GraphDriver* aPreviousDriver,
                                     uint32_t aSampleRate,
                                     FallbackMode aFallback)
    : ThreadedDriver(aGraphInterface, aPreviousDriver, aSampleRate),
      mInitialTimeStamp(TimeStamp::Now()),
      mCurrentTimeStamp(TimeStamp::Now()),
      mLastTimeStamp(TimeStamp::Now()),
      mIsFallback(aFallback == FallbackMode::Fallback) {}

SystemClockDriver::~SystemClockDriver() {}

bool SystemClockDriver::IsFallback() { return mIsFallback; }

void ThreadedDriver::RunThread() {
  mThreadRunning = true;
  while (true) {
    mIterationStart = mIterationEnd;
    mIterationEnd += GetIntervalForIteration();

    if (mStateComputedTime < mIterationEnd) {
      LOG(LogLevel::Warning, ("%p: Global underrun detected", Graph()));
      mIterationEnd = mStateComputedTime;
    }

    if (mIterationStart >= mIterationEnd) {
      NS_ASSERTION(mIterationStart == mIterationEnd,
                   "Time can't go backwards!");
      // This could happen due to low clock resolution, maybe?
      LOG(LogLevel::Debug, ("%p: Time did not advance", Graph()));
    }

    GraphTime nextStateComputedTime =
        MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(
            mIterationEnd + MillisecondsToMediaTime(AUDIO_TARGET_MS));
    if (nextStateComputedTime < mStateComputedTime) {
      // A previous driver may have been processing further ahead of
      // iterationEnd.
      LOG(LogLevel::Warning,
          ("%p: Prevent state from going backwards. interval[%ld; %ld] "
           "state[%ld; "
           "%ld]",
           Graph(), (long)mIterationStart, (long)mIterationEnd,
           (long)mStateComputedTime, (long)nextStateComputedTime));
      nextStateComputedTime = mStateComputedTime;
    }
    LOG(LogLevel::Verbose,
        ("%p: interval[%ld; %ld] state[%ld; %ld]", Graph(),
         (long)mIterationStart, (long)mIterationEnd, (long)mStateComputedTime,
         (long)nextStateComputedTime));

    mStateComputedTime = nextStateComputedTime;
    IterationResult result =
        Graph()->OneIteration(mStateComputedTime, mIterationEnd, nullptr);

    if (result.IsStop()) {
      // Signal that we're done stopping.
      dom::WorkletThread::DeleteCycleCollectedJSContext();
      result.Stopped();
      break;
    }
    WaitForNextIteration();
    if (GraphDriver* nextDriver = result.NextDriver()) {
      LOG(LogLevel::Debug, ("%p: Switching to AudioCallbackDriver", Graph()));
      result.Switched();
      nextDriver->SetState(mIterationStart, mIterationEnd, mStateComputedTime);
      nextDriver->Start();
      break;
    }
    MOZ_ASSERT(result.IsStillProcessing());
  }
  mThreadRunning = false;
}

MediaTime SystemClockDriver::GetIntervalForIteration() {
  TimeStamp now = TimeStamp::Now();
  MediaTime interval =
      SecondsToMediaTime((now - mCurrentTimeStamp).ToSeconds());
  mCurrentTimeStamp = now;

  MOZ_LOG(gMediaTrackGraphLog, LogLevel::Verbose,
          ("%p: Updating current time to %f (real %f, StateComputedTime() %f)",
           Graph(), MediaTimeToSeconds(mIterationEnd + interval),
           (now - mInitialTimeStamp).ToSeconds(),
           MediaTimeToSeconds(mStateComputedTime)));

  return interval;
}

void ThreadedDriver::EnsureNextIteration() {
  mWaitHelper.EnsureNextIteration();
}

void ThreadedDriver::WaitForNextIteration() {
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(OnThread());
  mWaitHelper.WaitForNextIterationAtLeast(WaitInterval());
}

TimeDuration SystemClockDriver::WaitInterval() {
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(OnThread());
  TimeStamp now = TimeStamp::Now();
  int64_t timeoutMS = MEDIA_GRAPH_TARGET_PERIOD_MS -
                      int64_t((now - mCurrentTimeStamp).ToMilliseconds());
  // Make sure timeoutMS doesn't overflow 32 bits by waking up at
  // least once a minute, if we need to wake up at all
  timeoutMS = std::max<int64_t>(0, std::min<int64_t>(timeoutMS, 60 * 1000));
  LOG(LogLevel::Verbose,
      ("%p: Waiting for next iteration; at %f, timeout=%f", Graph(),
       (now - mInitialTimeStamp).ToSeconds(), timeoutMS / 1000.0));

  return TimeDuration::FromMilliseconds(timeoutMS);
}

OfflineClockDriver::OfflineClockDriver(GraphInterface* aGraphInterface,
                                       uint32_t aSampleRate, GraphTime aSlice)
    : ThreadedDriver(aGraphInterface, nullptr, aSampleRate), mSlice(aSlice) {}

OfflineClockDriver::~OfflineClockDriver() {}

MediaTime OfflineClockDriver::GetIntervalForIteration() {
  return MillisecondsToMediaTime(mSlice);
}

AsyncCubebTask::AsyncCubebTask(AudioCallbackDriver* aDriver,
                               AsyncCubebOperation aOperation)
    : Runnable("AsyncCubebTask"),
      mDriver(aDriver),
      mOperation(aOperation),
      mShutdownGrip(aDriver->Graph()) {
  NS_WARNING_ASSERTION(
      mDriver->mAudioStream || aOperation == AsyncCubebOperation::INIT,
      "No audio stream!");
}

AsyncCubebTask::~AsyncCubebTask() {}

NS_IMETHODIMP
AsyncCubebTask::Run() {
  MOZ_ASSERT(mDriver);

  switch (mOperation) {
    case AsyncCubebOperation::INIT: {
      LOG(LogLevel::Debug, ("%p: AsyncCubebOperation::INIT driver=%p",
                            mDriver->Graph(), mDriver.get()));
      if (!mDriver->Init()) {
        LOG(LogLevel::Warning,
            ("AsyncCubebOperation::INIT failed for driver=%p", mDriver.get()));
        return NS_ERROR_FAILURE;
      }
      mDriver->CompleteAudioContextOperations(mOperation);
      break;
    }
    case AsyncCubebOperation::SHUTDOWN: {
      LOG(LogLevel::Debug, ("%p: AsyncCubebOperation::SHUTDOWN driver=%p",
                            mDriver->Graph(), mDriver.get()));
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

TrackAndPromiseForOperation::TrackAndPromiseForOperation(
    MediaTrack* aTrack, dom::AudioContextOperation aOperation,
    AbstractThread* aMainThread,
    MozPromiseHolder<MediaTrackGraph::AudioContextOperationPromise>&& aHolder)
    : mTrack(aTrack),
      mOperation(aOperation),
      mMainThread(aMainThread),
      mHolder(std::move(aHolder)) {}

TrackAndPromiseForOperation::TrackAndPromiseForOperation(
    TrackAndPromiseForOperation&& aOther) noexcept
    : mTrack(std::move(aOther.mTrack)),
      mOperation(aOther.mOperation),
      mMainThread(std::move(aOther.mMainThread)),
      mHolder(std::move(aOther.mHolder)) {}

AudioCallbackDriver::AudioCallbackDriver(
    GraphInterface* aGraphInterface, GraphDriver* aPreviousDriver,
    uint32_t aSampleRate, uint32_t aOutputChannelCount,
    uint32_t aInputChannelCount, CubebUtils::AudioDeviceID aOutputDeviceID,
    CubebUtils::AudioDeviceID aInputDeviceID, AudioInputType aAudioInputType)
    : GraphDriver(aGraphInterface, aPreviousDriver, aSampleRate),
      mOutputChannels(aOutputChannelCount),
      mInputChannelCount(aInputChannelCount),
      mOutputDeviceID(aOutputDeviceID),
      mInputDeviceID(aInputDeviceID),
      mIterationDurationMS(MEDIA_GRAPH_TARGET_PERIOD_MS),
      mStarted(false),
      mInitShutdownThread(
          SharedThreadPool::Get(NS_LITERAL_CSTRING("CubebOperation"), 1)),
      mPromisesForOperation("AudioCallbackDriver::mPromisesForOperation"),
      mAudioThreadId(std::thread::id()),
      mAudioThreadRunning(false),
      mShouldFallbackIfError(false),
      mFromFallback(false) {
  LOG(LogLevel::Debug, ("%p: AudioCallbackDriver ctor", Graph()));

  const uint32_t kIdleThreadTimeoutMs = 2000;
  mInitShutdownThread->SetIdleThreadTimeout(
      PR_MillisecondsToInterval(kIdleThreadTimeoutMs));

#if defined(XP_WIN)
  if (XRE_IsContentProcess()) {
    audio::AudioNotificationReceiver::Register(this);
  }
#endif
  if (aAudioInputType == AudioInputType::Voice) {
    LOG(LogLevel::Debug, ("VOICE."));
    mInputDevicePreference = CUBEB_DEVICE_PREF_VOICE;
    CubebUtils::SetInCommunication(true);
  } else {
    mInputDevicePreference = CUBEB_DEVICE_PREF_ALL;
  }

  mMixer.AddCallback(WrapNotNull(this));
}

AudioCallbackDriver::~AudioCallbackDriver() {
#ifdef DEBUG
  {
    auto promises = mPromisesForOperation.Lock();
    MOZ_ASSERT(promises->IsEmpty());
  }
#endif
#if defined(XP_WIN)
  if (XRE_IsContentProcess()) {
    audio::AudioNotificationReceiver::Unregister(this);
  }
#endif
  if (mInputDevicePreference == CUBEB_DEVICE_PREF_VOICE) {
    CubebUtils::SetInCommunication(false);
  }
}

bool IsMacbookOrMacbookAir() {
#ifdef XP_MACOSX
  size_t len = 0;
  sysctlbyname("hw.model", NULL, &len, NULL, 0);
  if (len) {
    UniquePtr<char[]> model(new char[len]);
    // This string can be
    // MacBook%d,%d for a normal MacBook
    // MacBookPro%d,%d for a MacBook Pro
    // MacBookAir%d,%d for a Macbook Air
    sysctlbyname("hw.model", model.get(), &len, NULL, 0);
    char* substring = strstr(model.get(), "MacBook");
    if (substring) {
      const size_t offset = strlen("MacBook");
      if (!strncmp(model.get() + offset, "Air", 3) ||
          isdigit(model[offset + 1])) {
        return true;
      }
    }
  }
#endif
  return false;
}

bool AudioCallbackDriver::Init() {
  cubeb* cubebContext = CubebUtils::GetCubebContext();
  if (!cubebContext) {
    NS_WARNING("Could not get cubeb context.");
    LOG(LogLevel::Warning, ("%s: Could not get cubeb context", __func__));
    if (!mFromFallback) {
      CubebUtils::ReportCubebStreamInitFailure(true);
    }
    FallbackToSystemClockDriver();
    return true;
  }

  cubeb_stream_params output;
  cubeb_stream_params input;
  bool firstStream = CubebUtils::GetFirstStream();

  MOZ_ASSERT(!NS_IsMainThread(),
             "This is blocking and should never run on the main thread.");

  output.rate = mSampleRate;

  if (AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_S16) {
    output.format = CUBEB_SAMPLE_S16NE;
  } else {
    output.format = CUBEB_SAMPLE_FLOAT32NE;
  }

  if (!mOutputChannels) {
    LOG(LogLevel::Warning, ("Output number of channels is 0."));
    FallbackToSystemClockDriver();
    return true;
  }

  CubebUtils::AudioDeviceID forcedOutputDeviceId = nullptr;

  char* forcedOutputDeviceName = CubebUtils::GetForcedOutputDevice();
  if (forcedOutputDeviceName) {
    RefPtr<CubebDeviceEnumerator> enumerator = Enumerator::GetInstance();
    RefPtr<AudioDeviceInfo> device = enumerator->DeviceInfoFromName(
        NS_ConvertUTF8toUTF16(forcedOutputDeviceName), EnumeratorSide::OUTPUT);
    if (device && device->DeviceID()) {
      forcedOutputDeviceId = device->DeviceID();
    }
  }

  mBuffer = AudioCallbackBufferWrapper<AudioDataValue>(mOutputChannels);
  mScratchBuffer =
      SpillBuffer<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 2>(mOutputChannels);

  output.channels = mOutputChannels;
  AudioConfig::ChannelLayout::ChannelMap channelMap =
      AudioConfig::ChannelLayout(mOutputChannels).Map();

  output.layout = static_cast<uint32_t>(channelMap);
  output.prefs = CubebUtils::GetDefaultStreamPrefs();
#if !defined(XP_WIN)
  if (mInputDevicePreference == CUBEB_DEVICE_PREF_VOICE) {
    output.prefs |= static_cast<cubeb_stream_prefs>(CUBEB_STREAM_PREF_VOICE);
  }
#endif

  uint32_t latencyFrames = CubebUtils::GetCubebMTGLatencyInFrames(&output);

  // Macbook and MacBook air don't have enough CPU to run very low latency
  // MediaTrackGraphs, cap the minimal latency to 512 frames int this case.
  if (IsMacbookOrMacbookAir()) {
    latencyFrames = std::max((uint32_t)512, latencyFrames);
  }

  // On OSX, having a latency that is lower than 10ms is very common. It's
  // not very useful when doing voice, because all the WebRTC code deal in 10ms
  // chunks of audio.  Take the first power of two above 10ms at the current
  // rate in this case. It's probably 512, for common rates.
#if defined(XP_MACOSX)
  if (mInputDevicePreference == CUBEB_DEVICE_PREF_VOICE) {
    if (latencyFrames < mSampleRate / 100) {
      latencyFrames = mozilla::RoundUpPow2(mSampleRate / 100);
    }
  }
#endif
  LOG(LogLevel::Debug, ("Effective latency in frames: %d", latencyFrames));

  input = output;
  input.channels = mInputChannelCount;
  input.layout = CUBEB_LAYOUT_UNDEFINED;

  cubeb_stream* stream = nullptr;
  bool inputWanted = mInputChannelCount > 0;
  CubebUtils::AudioDeviceID outputId = mOutputDeviceID;
  CubebUtils::AudioDeviceID inputId = mInputDeviceID;

  // XXX Only pass input input if we have an input listener.  Always
  // set up output because it's easier, and it will just get silence.
  if (cubeb_stream_init(cubebContext, &stream, "AudioCallbackDriver", inputId,
                        inputWanted ? &input : nullptr,
                        forcedOutputDeviceId ? forcedOutputDeviceId : outputId,
                        &output, latencyFrames, DataCallback_s, StateCallback_s,
                        this) == CUBEB_OK) {
    mAudioStream.own(stream);
    DebugOnly<int> rv =
        cubeb_stream_set_volume(mAudioStream, CubebUtils::GetVolumeScale());
    NS_WARNING_ASSERTION(
        rv == CUBEB_OK,
        "Could not set the audio stream volume in GraphDriver.cpp");
    CubebUtils::ReportCubebBackendUsed();
  } else {
    NS_WARNING(
        "Could not create a cubeb stream for MediaTrackGraph, falling "
        "back to a SystemClockDriver");
    // Only report failures when we're not coming from a driver that was
    // created itself as a fallback driver because of a previous audio driver
    // failure.
    if (!mFromFallback) {
      CubebUtils::ReportCubebStreamInitFailure(firstStream);
    }
    FallbackToSystemClockDriver();
    return true;
  }

#ifdef XP_MACOSX
  PanOutputIfNeeded(inputWanted);
#endif

  cubeb_stream_register_device_changed_callback(
      mAudioStream, AudioCallbackDriver::DeviceChangedCallback_s);

  if (!StartStream()) {
    LOG(LogLevel::Warning,
        ("%p: AudioCallbackDriver couldn't start a cubeb stream.", Graph()));
    return false;
  }

  LOG(LogLevel::Debug, ("%p: AudioCallbackDriver started.", Graph()));
  return true;
}

void AudioCallbackDriver::Start() {
  MOZ_ASSERT(!IsStarted());
  MOZ_ASSERT(NS_IsMainThread() || OnCubebOperationThread() ||
             (PreviousDriver() && PreviousDriver()->InIteration()));
  if (mPreviousDriver) {
    if (mPreviousDriver->AsAudioCallbackDriver()) {
      LOG(LogLevel::Debug, ("Releasing audio driver off main thread."));
      RefPtr<AsyncCubebTask> releaseEvent =
          new AsyncCubebTask(mPreviousDriver->AsAudioCallbackDriver(),
                             AsyncCubebOperation::SHUTDOWN);
      releaseEvent->Dispatch();
    } else {
      LOG(LogLevel::Debug,
          ("Dropping driver reference for SystemClockDriver."));
      MOZ_ASSERT(mPreviousDriver->AsSystemClockDriver());
      mFromFallback = mPreviousDriver->AsSystemClockDriver()->IsFallback();
    }
    mPreviousDriver = nullptr;
  }

  LOG(LogLevel::Debug, ("Starting new audio driver off main thread, "
                        "to ensure it runs after previous shutdown."));
  RefPtr<AsyncCubebTask> initEvent =
      new AsyncCubebTask(AsAudioCallbackDriver(), AsyncCubebOperation::INIT);
  initEvent->Dispatch();
}

bool AudioCallbackDriver::StartStream() {
  MOZ_ASSERT(!IsStarted() && OnCubebOperationThread());
  mShouldFallbackIfError = true;
  // Set mStarted before cubeb_stream_start, since starting the cubeb stream can
  // result in a callback (that may read mStarted) before mStarted would
  // otherwise be set to true.
  mStarted = true;
  if (cubeb_stream_start(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not start cubeb stream for MTG.");
    mStarted = false;
    return false;
  }

  return true;
}

void AudioCallbackDriver::Stop() {
  MOZ_ASSERT(OnCubebOperationThread());
  if (cubeb_stream_stop(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not stop cubeb stream for MTG.");
  }
  mStarted = false;
}

void AudioCallbackDriver::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Debug,
      ("%p: Releasing audio driver off main thread (GraphDriver::Shutdown).",
       Graph()));
  RefPtr<AsyncCubebTask> releaseEvent =
      new AsyncCubebTask(this, AsyncCubebOperation::SHUTDOWN);
  releaseEvent->Dispatch(NS_DISPATCH_SYNC);
}

#if defined(XP_WIN)
void AudioCallbackDriver::ResetDefaultDevice() {
  if (cubeb_stream_reset_default_device(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not reset cubeb stream to default output device.");
  }
}
#endif

/* static */
long AudioCallbackDriver::DataCallback_s(cubeb_stream* aStream, void* aUser,
                                         const void* aInputBuffer,
                                         void* aOutputBuffer, long aFrames) {
  AudioCallbackDriver* driver = reinterpret_cast<AudioCallbackDriver*>(aUser);
  return driver->DataCallback(static_cast<const AudioDataValue*>(aInputBuffer),
                              static_cast<AudioDataValue*>(aOutputBuffer),
                              aFrames);
}

/* static */
void AudioCallbackDriver::StateCallback_s(cubeb_stream* aStream, void* aUser,
                                          cubeb_state aState) {
  AudioCallbackDriver* driver = reinterpret_cast<AudioCallbackDriver*>(aUser);
  driver->StateCallback(aState);
}

/* static */
void AudioCallbackDriver::DeviceChangedCallback_s(void* aUser) {
  AudioCallbackDriver* driver = reinterpret_cast<AudioCallbackDriver*>(aUser);
  driver->DeviceChangedCallback();
}

AudioCallbackDriver::AutoInCallback::AutoInCallback(
    AudioCallbackDriver* aDriver)
    : mDriver(aDriver) {
  mDriver->mAudioThreadId = std::this_thread::get_id();
}

AudioCallbackDriver::AutoInCallback::~AutoInCallback() {
  mDriver->mAudioThreadId = std::thread::id();
}

long AudioCallbackDriver::DataCallback(const AudioDataValue* aInputBuffer,
                                       AudioDataValue* aOutputBuffer,
                                       long aFrames) {
  TRACE_AUDIO_CALLBACK_BUDGET(aFrames, mSampleRate);
  TRACE_AUDIO_CALLBACK();

#ifdef DEBUG
  AutoInCallback aic(this);
#endif

  uint32_t durationMS = aFrames * 1000 / mSampleRate;

  // For now, simply average the duration with the previous
  // duration so there is some damping against sudden changes.
  if (!mIterationDurationMS) {
    mIterationDurationMS = durationMS;
  } else {
    mIterationDurationMS = (mIterationDurationMS * 3) + durationMS;
    mIterationDurationMS /= 4;
  }

  mBuffer.SetBuffer(aOutputBuffer, aFrames);
  // fill part or all with leftover data from last iteration (since we
  // align to Audio blocks)
  mScratchBuffer.Empty(mBuffer);

  // State computed time is decided by the audio callback's buffer length. We
  // compute the iteration start and end from there, trying to keep the amount
  // of buffering in the graph constant.
  GraphTime nextStateComputedTime =
      MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(mStateComputedTime +
                                                    mBuffer.Available());

  mIterationStart = mIterationEnd;
  // inGraph is the number of audio frames there is between the state time and
  // the current time, i.e. the maximum theoretical length of the interval we
  // could use as [mIterationStart; mIterationEnd].
  GraphTime inGraph = mStateComputedTime - mIterationStart;
  // We want the interval [mIterationStart; mIterationEnd] to be before the
  // interval [mStateComputedTime; nextStateComputedTime]. We also want
  // the distance between these intervals to be roughly equivalent each time, to
  // ensure there is no clock drift between current time and state time. Since
  // we can't act on the state time because we have to fill the audio buffer, we
  // reclock the current time against the state time, here.
  mIterationEnd = mIterationStart + 0.8 * inGraph;

  LOG(LogLevel::Verbose,
      ("%p: interval[%ld; %ld] state[%ld; %ld] (frames: %ld) (durationMS: %u) "
       "(duration ticks: %ld)",
       Graph(), (long)mIterationStart, (long)mIterationEnd,
       (long)mStateComputedTime, (long)nextStateComputedTime, (long)aFrames,
       (uint32_t)durationMS,
       (long)(nextStateComputedTime - mStateComputedTime)));

  if (mStateComputedTime < mIterationEnd) {
    LOG(LogLevel::Error, ("%p: Media graph global underrun detected", Graph()));
    MOZ_ASSERT_UNREACHABLE("We should not underrun in full duplex");
    mIterationEnd = mStateComputedTime;
  }

  // Process mic data if any/needed
  if (aInputBuffer && mInputChannelCount > 0) {
    Graph()->NotifyInputData(aInputBuffer, static_cast<size_t>(aFrames),
                             mSampleRate, mInputChannelCount);
  }

  bool iterate = mBuffer.Available();
  IterationResult result =
      iterate
          ? Graph()->OneIteration(nextStateComputedTime, mIterationEnd, &mMixer)
          : IterationResult::CreateStillProcessing();
  if (iterate) {
    // We totally filled the buffer (and mScratchBuffer isn't empty).
    // We don't need to run an iteration and if we do so we may overflow.
    mStateComputedTime = nextStateComputedTime;
  } else {
    LOG(LogLevel::Verbose,
        ("%p: DataCallback buffer filled entirely from scratch "
         "buffer, skipping iteration.",
         Graph()));
    result = IterationResult::CreateStillProcessing();
  }

  mBuffer.BufferFilled();

  // Callback any observers for the AEC speaker data.  Note that one
  // (maybe) of these will be full-duplex, the others will get their input
  // data off separate cubeb callbacks.  Take care with how stuff is
  // removed/added to this list and TSAN issues, but input and output will
  // use separate callback methods.
  Graph()->NotifyOutputData(aOutputBuffer, static_cast<size_t>(aFrames),
                            mSampleRate, mOutputChannels);

#ifdef XP_MACOSX
  // This only happens when the output is on a macbookpro's external speaker,
  // that are stereo, but let's just be safe.
  if (mNeedsPanning && mOutputChannels == 2) {
    // hard pan to the right
    for (uint32_t i = 0; i < aFrames * 2; i += 2) {
      aOutputBuffer[i + 1] += aOutputBuffer[i];
      aOutputBuffer[i] = 0.0;
    }
  }
#endif

  if (result.IsStop()) {
    // About to hand over control of the graph.  Do not start a new driver if
    // StateCallback() receives an error for this stream while the main thread
    // or another driver has control of the graph.
    mShouldFallbackIfError = false;
    // Signal that we have stopped.
    result.Stopped();
    // Update the flag before handing over the graph and going to drain.
    mAudioThreadRunning = false;
    return aFrames - 1;
  }

  if (GraphDriver* nextDriver = result.NextDriver()) {
    LOG(LogLevel::Debug, ("%p: Switching to system driver.", Graph()));
    result.Switched();
    mShouldFallbackIfError = false;
    mAudioThreadRunning = false;
    nextDriver->SetState(mIterationStart, mIterationEnd, mStateComputedTime);
    nextDriver->Start();
    // Returning less than aFrames starts the draining and eventually stops the
    // audio thread. This function will never get called again.
    return aFrames - 1;
  }

  MOZ_ASSERT(result.IsStillProcessing());
  return aFrames;
}

static const char* StateToString(cubeb_state aState) {
  switch (aState) {
    case CUBEB_STATE_STARTED:
      return "STARTED";
    case CUBEB_STATE_STOPPED:
      return "STOPPED";
    case CUBEB_STATE_DRAINED:
      return "DRAINED";
    case CUBEB_STATE_ERROR:
      return "ERROR";
    default:
      MOZ_CRASH("Unexpected state!");
  }
}

void AudioCallbackDriver::StateCallback(cubeb_state aState) {
  MOZ_ASSERT(!InIteration());
  LOG(LogLevel::Debug,
      ("AudioCallbackDriver State: %s", StateToString(aState)));

  // Clear the flag for the not running
  // states: stopped, drained, error.
  mAudioThreadRunning = (aState == CUBEB_STATE_STARTED);

  if (aState == CUBEB_STATE_ERROR && mShouldFallbackIfError) {
    MOZ_ASSERT(!ThreadRunning());
    mShouldFallbackIfError = false;
    FallbackToSystemClockDriver();
  } else if (aState == CUBEB_STATE_STOPPED) {
    MOZ_ASSERT(!ThreadRunning());
  }
}

void AudioCallbackDriver::MixerCallback(AudioDataValue* aMixedBuffer,
                                        AudioSampleFormat aFormat,
                                        uint32_t aChannels, uint32_t aFrames,
                                        uint32_t aSampleRate) {
  MOZ_ASSERT(InIteration());
  uint32_t toWrite = mBuffer.Available();

  if (!mBuffer.Available()) {
    NS_WARNING("DataCallback buffer full, expect frame drops.");
  }

  MOZ_ASSERT(mBuffer.Available() <= aFrames);

  mBuffer.WriteFrames(aMixedBuffer, mBuffer.Available());
  MOZ_ASSERT(mBuffer.Available() == 0,
             "Missing frames to fill audio callback's buffer.");

  DebugOnly<uint32_t> written = mScratchBuffer.Fill(
      aMixedBuffer + toWrite * aChannels, aFrames - toWrite);
  NS_WARNING_ASSERTION(written == aFrames - toWrite, "Dropping frames.");
};

void AudioCallbackDriver::PanOutputIfNeeded(bool aMicrophoneActive) {
#ifdef XP_MACOSX
  cubeb_device* out = nullptr;
  int rv;
  char name[128];
  size_t length = sizeof(name);

  rv = sysctlbyname("hw.model", name, &length, NULL, 0);
  if (rv) {
    return;
  }

  if (!strncmp(name, "MacBookPro", 10)) {
    if (cubeb_stream_get_current_device(mAudioStream, &out) == CUBEB_OK) {
      MOZ_ASSERT(out);
      // Check if we are currently outputing sound on external speakers.
      if (out->output_name && !strcmp(out->output_name, "ispk")) {
        // Pan everything to the right speaker.
        LOG(LogLevel::Debug, ("Using the built-in speakers, with%s audio input",
                              aMicrophoneActive ? "" : "out"));
        mNeedsPanning = aMicrophoneActive;
      } else {
        LOG(LogLevel::Debug, ("Using an external output device"));
        mNeedsPanning = false;
      }
      cubeb_stream_device_destroy(mAudioStream, out);
    }
  }
#endif
}

void AudioCallbackDriver::DeviceChangedCallback() {
  MOZ_ASSERT(!InIteration());
  // Tell the audio engine the device has changed, it might want to reset some
  // state.
  Graph()->DeviceChanged();
#ifdef XP_MACOSX
  RefPtr<AudioCallbackDriver> self(this);
  bool hasInput = mInputChannelCount;
  NS_DispatchBackgroundTask(NS_NewRunnableFunction(
      "PanOutputIfNeeded", [self{std::move(self)}, hasInput]() {
        self->PanOutputIfNeeded(hasInput);
      }));
#endif
}

uint32_t AudioCallbackDriver::IterationDuration() {
  MOZ_ASSERT(InIteration());
  // The real fix would be to have an API in cubeb to give us the number. Short
  // of that, we approximate it here. bug 1019507
  return mIterationDurationMS;
}

bool AudioCallbackDriver::IsStarted() { return mStarted; }

void AudioCallbackDriver::EnqueueTrackAndPromiseForOperation(
    MediaTrack* aTrack, dom::AudioContextOperation aOperation,
    AbstractThread* aMainThread,
    MozPromiseHolder<MediaTrackGraph::AudioContextOperationPromise>&& aHolder) {
  MOZ_ASSERT(InIteration() || !ThreadRunning());
  auto promises = mPromisesForOperation.Lock();
  promises->AppendElement(TrackAndPromiseForOperation(
      aTrack, aOperation, aMainThread, std::move(aHolder)));
}

void AudioCallbackDriver::CompleteAudioContextOperations(
    AsyncCubebOperation aOperation) {
  MOZ_ASSERT(OnCubebOperationThread());
  auto promises = mPromisesForOperation.Lock();
  for (uint32_t i = 0; i < promises->Length(); i++) {
    TrackAndPromiseForOperation& s = promises.ref()[i];
    if ((aOperation == AsyncCubebOperation::INIT &&
         s.mOperation == dom::AudioContextOperation::Resume) ||
        (aOperation == AsyncCubebOperation::SHUTDOWN &&
         s.mOperation != dom::AudioContextOperation::Resume)) {
      AudioContextState state;
      switch (s.mOperation) {
        case dom::AudioContextOperation::Resume:
          state = dom::AudioContextState::Running;
          break;
        case dom::AudioContextOperation::Suspend:
          state = dom::AudioContextState::Suspended;
          break;
        case dom::AudioContextOperation::Close:
          state = dom::AudioContextState::Closed;
          break;
        default:
          MOZ_CRASH("Unexpected operation");
      }
      s.mMainThread->Dispatch(NS_NewRunnableFunction(
          "AudioContextOperation::Resolve",
          [holder = std::move(s.mHolder), state]() mutable {
            holder.Resolve(state, __func__);
          }));
      promises->RemoveElementAt(i);
      i--;
    }
  }
}

TimeDuration AudioCallbackDriver::AudioOutputLatency() {
  uint32_t latencyFrames;
  int rv = cubeb_stream_get_latency(mAudioStream, &latencyFrames);
  if (rv || mSampleRate == 0) {
    return TimeDuration::FromSeconds(0.0);
  }

  return TimeDuration::FromSeconds(static_cast<double>(latencyFrames) /
                                   mSampleRate);
}

void AudioCallbackDriver::FallbackToSystemClockDriver() {
  MOZ_ASSERT(!ThreadRunning());
  MOZ_CRASH("Not implemented");
}

}  // namespace mozilla

// avoid redefined macro in unified build
#undef LOG
