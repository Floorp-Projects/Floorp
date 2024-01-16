/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GraphDriver.h"

#include "AudioNodeEngine.h"
#include "cubeb/cubeb.h"
#include "mozilla/dom/AudioContext.h"
#include "mozilla/dom/AudioDeviceInfo.h"
#include "mozilla/dom/BaseAudioContextBinding.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Unused.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/StaticPrefs_media.h"
#include "CubebDeviceEnumerator.h"
#include "MediaTrackGraphImpl.h"
#include "CallbackThreadRegistry.h"
#include "Tracing.h"

#ifdef MOZ_WEBRTC
#  include "webrtc/MediaEngineWebRTC.h"
#endif

#ifdef XP_MACOSX
#  include <sys/sysctl.h>
#  include "nsCocoaFeatures.h"
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

void GraphDriver::SetStreamName(const nsACString& aStreamName) {
  MOZ_ASSERT(InIteration() || (!ThreadRunning() && NS_IsMainThread()));
  mStreamName = aStreamName;
  LOG(LogLevel::Debug, ("%p: GraphDriver::SetStreamName driver=%p %s", Graph(),
                        this, mStreamName.get()));
}

void GraphDriver::SetState(const nsACString& aStreamName,
                           GraphTime aIterationEnd,
                           GraphTime aStateComputedTime) {
  MOZ_ASSERT(InIteration() || !ThreadRunning());

  mStreamName = aStreamName;
  mIterationEnd = aIterationEnd;
  mStateComputedTime = aStateComputedTime;
}

#ifdef DEBUG
bool GraphDriver::InIteration() const {
  return OnThread() || Graph()->InDriverIteration(this);
}
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
    TRACE("MediaTrackGraphShutdownThreadRunnable");
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mThread);

    mThread->AsyncShutdown();
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
    SchedulerGroup::Dispatch(event.forget());
  }
}

class MediaTrackGraphInitThreadRunnable : public Runnable {
 public:
  explicit MediaTrackGraphInitThreadRunnable(ThreadedDriver* aDriver)
      : Runnable("MediaTrackGraphInitThreadRunnable"), mDriver(aDriver) {}
  NS_IMETHOD Run() override {
    TRACE("MediaTrackGraphInitThreadRunnable");
    MOZ_ASSERT(!mDriver->ThreadRunning());
    LOG(LogLevel::Debug, ("Starting a new system driver for graph %p",
                          mDriver->mGraphInterface.get()));

    if (GraphDriver* previousDriver = mDriver->PreviousDriver()) {
      LOG(LogLevel::Debug,
          ("%p releasing an AudioCallbackDriver(%p), for graph %p",
           mDriver.get(), previousDriver, mDriver->Graph()));
      MOZ_ASSERT(!mDriver->AsAudioCallbackDriver());
      AudioCallbackDriver* audioCallbackDriver =
          previousDriver->AsAudioCallbackDriver();
      audioCallbackDriver->mCubebOperationThread->Dispatch(
          NS_NewRunnableFunction(
              "ThreadedDriver previousDriver::Stop()",
              [audioCallbackDriver = RefPtr{audioCallbackDriver}] {
                audioCallbackDriver->Stop();
              }));
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
    mThread->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
  }
}

void ThreadedDriver::Shutdown() {
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  // mGraph's thread is not running so it's OK to do whatever here
  LOG(LogLevel::Debug, ("Stopping threads for MediaTrackGraph %p", this));

  if (mThread) {
    LOG(LogLevel::Debug,
        ("%p: Stopping ThreadedDriver's %p thread", Graph(), this));
    mThread->AsyncShutdown();
    mThread = nullptr;
  }
}

SystemClockDriver::SystemClockDriver(GraphInterface* aGraphInterface,
                                     GraphDriver* aPreviousDriver,
                                     uint32_t aSampleRate)
    : ThreadedDriver(aGraphInterface, aPreviousDriver, aSampleRate),
      mInitialTimeStamp(TimeStamp::Now()),
      mCurrentTimeStamp(TimeStamp::Now()),
      mLastTimeStamp(TimeStamp::Now()) {}

SystemClockDriver::~SystemClockDriver() = default;

void ThreadedDriver::RunThread() {
  mThreadRunning = true;
  while (true) {
    auto iterationStart = mIterationEnd;
    mIterationEnd += GetIntervalForIteration();

    if (mStateComputedTime < mIterationEnd) {
      LOG(LogLevel::Warning, ("%p: Global underrun detected", Graph()));
      mIterationEnd = mStateComputedTime;
    }

    if (iterationStart >= mIterationEnd) {
      NS_ASSERTION(iterationStart == mIterationEnd, "Time can't go backwards!");
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
           Graph(), (long)iterationStart, (long)mIterationEnd,
           (long)mStateComputedTime, (long)nextStateComputedTime));
      nextStateComputedTime = mStateComputedTime;
    }
    LOG(LogLevel::Verbose,
        ("%p: interval[%ld; %ld] state[%ld; %ld]", Graph(),
         (long)iterationStart, (long)mIterationEnd, (long)mStateComputedTime,
         (long)nextStateComputedTime));

    mStateComputedTime = nextStateComputedTime;
    IterationResult result =
        Graph()->OneIteration(mStateComputedTime, mIterationEnd, nullptr);

    if (result.IsStop()) {
      // Signal that we're done stopping.
      result.Stopped();
      break;
    }
    WaitForNextIteration();
    if (GraphDriver* nextDriver = result.NextDriver()) {
      LOG(LogLevel::Debug, ("%p: Switching to AudioCallbackDriver", Graph()));
      result.Switched();
      nextDriver->SetState(mStreamName, mIterationEnd, mStateComputedTime);
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

OfflineClockDriver::~OfflineClockDriver() = default;

void OfflineClockDriver::RunThread() {
  nsCOMPtr<nsIThreadInternal> threadInternal = do_QueryInterface(mThread);
  nsCOMPtr<nsIThreadObserver> observer = do_QueryInterface(Graph());
  threadInternal->SetObserver(observer);

  ThreadedDriver::RunThread();
}

MediaTime OfflineClockDriver::GetIntervalForIteration() {
  return MillisecondsToMediaTime(mSlice);
}

/* Helper to proxy the GraphInterface methods used by a running
 * mFallbackDriver. */
class AudioCallbackDriver::FallbackWrapper : public GraphInterface {
 public:
  FallbackWrapper(RefPtr<GraphInterface> aGraph,
                  RefPtr<AudioCallbackDriver> aOwner, uint32_t aSampleRate,
                  const nsACString& aStreamName, GraphTime aIterationEnd,
                  GraphTime aStateComputedTime)
      : mGraph(std::move(aGraph)),
        mOwner(std::move(aOwner)),
        mFallbackDriver(
            MakeRefPtr<SystemClockDriver>(this, nullptr, aSampleRate)) {
    mFallbackDriver->SetState(aStreamName, aIterationEnd, aStateComputedTime);
  }

  NS_DECL_THREADSAFE_ISUPPORTS

  /* Proxied SystemClockDriver methods */
  void Start() { mFallbackDriver->Start(); }
  MOZ_CAN_RUN_SCRIPT void Shutdown() {
    RefPtr<SystemClockDriver> driver = mFallbackDriver;
    driver->Shutdown();
  }
  void SetStreamName(const nsACString& aStreamName) {
    mFallbackDriver->SetStreamName(aStreamName);
  }
  void EnsureNextIteration() { mFallbackDriver->EnsureNextIteration(); }
#ifdef DEBUG
  bool InIteration() { return mFallbackDriver->InIteration(); }
#endif
  bool OnThread() { return mFallbackDriver->OnThread(); }

  /* GraphInterface methods */
  void NotifyInputStopped() override {
    MOZ_CRASH("Unexpected NotifyInputStopped from fallback SystemClockDriver");
  }
  void NotifyInputData(const AudioDataValue* aBuffer, size_t aFrames,
                       TrackRate aRate, uint32_t aChannels,
                       uint32_t aAlreadyBuffered) override {
    MOZ_CRASH("Unexpected NotifyInputData from fallback SystemClockDriver");
  }
  void DeviceChanged() override {
    MOZ_CRASH("Unexpected DeviceChanged from fallback SystemClockDriver");
  }
#ifdef DEBUG
  bool InDriverIteration(const GraphDriver* aDriver) const override {
    return mGraph->InDriverIteration(mOwner) && mOwner->OnFallback();
  }
#endif
  IterationResult OneIteration(GraphTime aStateComputedEnd,
                               GraphTime aIterationEnd,
                               MixerCallbackReceiver* aMixerReceiver) override {
    MOZ_ASSERT(!aMixerReceiver);

#ifdef DEBUG
    AutoInCallback aic(mOwner);
#endif

    IterationResult result =
        mGraph->OneIteration(aStateComputedEnd, aIterationEnd, aMixerReceiver);

    AudioStreamState audioState = mOwner->mAudioStreamState;

    MOZ_ASSERT(audioState != AudioStreamState::Stopping,
               "The audio driver can only enter stopping if it iterated the "
               "graph, which it can only do if there's no fallback driver");
    if (audioState != AudioStreamState::Running && result.IsStillProcessing()) {
      mOwner->MaybeStartAudioStream();
      return result;
    }

    MOZ_ASSERT(result.IsStillProcessing() || result.IsStop() ||
               result.IsSwitchDriver());

    // Proxy the release of the fallback driver to a background thread, so it
    // doesn't perform unexpected suicide.
    IterationResult stopFallback =
        IterationResult::CreateStop(NS_NewRunnableFunction(
            "AudioCallbackDriver::FallbackDriverStopped",
            [self = RefPtr<FallbackWrapper>(this), this, aIterationEnd,
             aStateComputedEnd, result = std::move(result)]() mutable {
              FallbackDriverState fallbackState =
                  result.IsStillProcessing() ? FallbackDriverState::None
                                             : FallbackDriverState::Stopped;
              mOwner->FallbackDriverStopped(aIterationEnd, aStateComputedEnd,
                                            fallbackState);

              if (fallbackState == FallbackDriverState::Stopped) {
#ifdef DEBUG
                // The AudioCallbackDriver may not iterate the graph, but we'll
                // call into it so we need to be regarded as "in iteration".
                AutoInCallback aic(mOwner);
#endif
                if (GraphDriver* nextDriver = result.NextDriver()) {
                  LOG(LogLevel::Debug,
                      ("%p: Switching from fallback to other driver.",
                       mOwner.get()));
                  result.Switched();
                  nextDriver->SetState(mOwner->mStreamName, aIterationEnd,
                                       aStateComputedEnd);
                  nextDriver->Start();
                } else if (result.IsStop()) {
                  LOG(LogLevel::Debug,
                      ("%p: Stopping fallback driver.", mOwner.get()));
                  result.Stopped();
                }
              }
              mOwner = nullptr;
              NS_DispatchBackgroundTask(NS_NewRunnableFunction(
                  "AudioCallbackDriver::FallbackDriverStopped::Release",
                  [fallback = std::move(self->mFallbackDriver)] {}));
            }));

    return stopFallback;
  }

 private:
  virtual ~FallbackWrapper() = default;

  const RefPtr<GraphInterface> mGraph;
  // Valid until mFallbackDriver has finished its last iteration.
  RefPtr<AudioCallbackDriver> mOwner;
  RefPtr<SystemClockDriver> mFallbackDriver;
};

NS_IMPL_ISUPPORTS0(AudioCallbackDriver::FallbackWrapper)

AudioCallbackDriver::AudioCallbackDriver(
    GraphInterface* aGraphInterface, GraphDriver* aPreviousDriver,
    uint32_t aSampleRate, uint32_t aOutputChannelCount,
    uint32_t aInputChannelCount, CubebUtils::AudioDeviceID aOutputDeviceID,
    CubebUtils::AudioDeviceID aInputDeviceID, AudioInputType aAudioInputType)
    : GraphDriver(aGraphInterface, aPreviousDriver, aSampleRate),
      mOutputChannelCount(aOutputChannelCount),
      mInputChannelCount(aInputChannelCount),
      mOutputDeviceID(aOutputDeviceID),
      mInputDeviceID(aInputDeviceID),
      mIterationDurationMS(MEDIA_GRAPH_TARGET_PERIOD_MS),
      mCubebOperationThread(CUBEB_TASK_THREAD),
      mAudioThreadId(ProfilerThreadId{}),
      mAudioThreadIdInCb(std::thread::id()),
      mFallback("AudioCallbackDriver::mFallback"),
      mSandboxed(CubebUtils::SandboxEnabled()) {
  LOG(LogLevel::Debug, ("%p: AudioCallbackDriver %p ctor - input: device %p, "
                        "channel %d, output: device %p, channel %d",
                        Graph(), this, mInputDeviceID, mInputChannelCount,
                        mOutputDeviceID, mOutputChannelCount));

  NS_WARNING_ASSERTION(mOutputChannelCount != 0,
                       "Invalid output channel count");
  MOZ_ASSERT(mOutputChannelCount <= 8);

  const uint32_t kIdleThreadTimeoutMs = 2000;
  mCubebOperationThread->SetIdleThreadTimeout(
      PR_MillisecondsToInterval(kIdleThreadTimeoutMs));

  bool allowVoice = true;
#ifdef MOZ_WIDGET_COCOA
  // Using the VoiceProcessingIO audio unit on MacOS 12 causes crashes in
  // platform code.
  allowVoice = nsCocoaFeatures::macOSVersionMajor() != 12;
#endif

  if (aAudioInputType == AudioInputType::Voice && allowVoice) {
    LOG(LogLevel::Debug, ("VOICE."));
    mInputDevicePreference = CUBEB_DEVICE_PREF_VOICE;
    CubebUtils::SetInCommunication(true);
  } else {
    mInputDevicePreference = CUBEB_DEVICE_PREF_ALL;
  }
}

AudioCallbackDriver::~AudioCallbackDriver() {
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

void AudioCallbackDriver::Init(const nsCString& aStreamName) {
  LOG(LogLevel::Debug,
      ("%p: AudioCallbackDriver::Init driver=%p", Graph(), this));
  TRACE("AudioCallbackDriver::Init");
  MOZ_ASSERT(OnCubebOperationThread());
  MOZ_ASSERT(mAudioStreamState == AudioStreamState::Pending);
  FallbackDriverState fallbackState = mFallbackDriverState;
  if (fallbackState == FallbackDriverState::Stopped) {
    // The graph has already stopped us.
    return;
  }
  bool fromFallback = fallbackState == FallbackDriverState::Running;
  cubeb* cubebContext = CubebUtils::GetCubebContext();
  if (!cubebContext) {
    NS_WARNING("Could not get cubeb context.");
    LOG(LogLevel::Warning, ("%s: Could not get cubeb context", __func__));
    mAudioStreamState = AudioStreamState::None;
    if (!fromFallback) {
      CubebUtils::ReportCubebStreamInitFailure(true);
      FallbackToSystemClockDriver();
    }
    return;
  }

  cubeb_stream_params output;
  cubeb_stream_params input;
  bool firstStream = CubebUtils::GetFirstStream();

  MOZ_ASSERT(!NS_IsMainThread(),
             "This is blocking and should never run on the main thread.");

  output.rate = mSampleRate;
  output.format = CUBEB_SAMPLE_FLOAT32NE;

  if (!mOutputChannelCount) {
    LOG(LogLevel::Warning, ("Output number of channels is 0."));
    mAudioStreamState = AudioStreamState::None;
    if (!fromFallback) {
      CubebUtils::ReportCubebStreamInitFailure(firstStream);
      FallbackToSystemClockDriver();
    }
    return;
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

  mBuffer = AudioCallbackBufferWrapper<AudioDataValue>(mOutputChannelCount);
  mScratchBuffer =
      SpillBuffer<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 2>(mOutputChannelCount);

  output.channels = mOutputChannelCount;
  AudioConfig::ChannelLayout::ChannelMap channelMap =
      AudioConfig::ChannelLayout(mOutputChannelCount).Map();

  output.layout = static_cast<uint32_t>(channelMap);
  output.prefs = CubebUtils::GetDefaultStreamPrefs(CUBEB_DEVICE_TYPE_OUTPUT);
  if (mInputDevicePreference == CUBEB_DEVICE_PREF_VOICE &&
      CubebUtils::RouteOutputAsVoice()) {
    output.prefs |= static_cast<cubeb_stream_prefs>(CUBEB_STREAM_PREF_VOICE);
  }

  uint32_t latencyFrames = CubebUtils::GetCubebMTGLatencyInFrames(&output);

  LOG(LogLevel::Debug, ("Minimum latency in frames: %d", latencyFrames));

  // Macbook and MacBook air don't have enough CPU to run very low latency
  // MediaTrackGraphs, cap the minimal latency to 512 frames int this case.
  if (IsMacbookOrMacbookAir()) {
    latencyFrames = std::max((uint32_t)512, latencyFrames);
    LOG(LogLevel::Debug,
        ("Macbook or macbook air, new latency: %d", latencyFrames));
  }

  // Buffer sizes lower than 10ms are nowadays common. It's not very useful
  // when doing voice, because all the WebRTC code that does audio input
  // processing deals in 10ms chunks of audio. Take the first power of two
  // above 10ms at the current rate in this case. It's probably 512, for common
  // rates.
  if (mInputDevicePreference == CUBEB_DEVICE_PREF_VOICE) {
    if (latencyFrames < mSampleRate / 100) {
      latencyFrames = mozilla::RoundUpPow2(mSampleRate / 100);
      LOG(LogLevel::Debug,
          ("AudioProcessing enabled, new latency %d", latencyFrames));
    }
  }

  // It's not useful for the graph to run with a block size lower than the Web
  // Audio API block size, but increasingly devices report that they can do
  // audio latencies lower than that.
  if (latencyFrames < WEBAUDIO_BLOCK_SIZE) {
    LOG(LogLevel::Debug,
        ("Latency clamped to %d from %d", WEBAUDIO_BLOCK_SIZE, latencyFrames));
    latencyFrames = WEBAUDIO_BLOCK_SIZE;
  }
  LOG(LogLevel::Debug, ("Effective latency in frames: %d", latencyFrames));

  input = output;
  input.channels = mInputChannelCount;
  input.layout = CUBEB_LAYOUT_UNDEFINED;
  input.prefs = CubebUtils::GetDefaultStreamPrefs(CUBEB_DEVICE_TYPE_INPUT);
  if (mInputDevicePreference == CUBEB_DEVICE_PREF_VOICE) {
    input.prefs |= static_cast<cubeb_stream_prefs>(CUBEB_STREAM_PREF_VOICE);
  }

  cubeb_stream* stream = nullptr;
  const char* streamName =
      aStreamName.IsEmpty() ? "AudioCallbackDriver" : aStreamName.get();
  bool inputWanted = mInputChannelCount > 0;
  CubebUtils::AudioDeviceID outputId = mOutputDeviceID;
  CubebUtils::AudioDeviceID inputId = mInputDeviceID;

  if (CubebUtils::CubebStreamInit(
          cubebContext, &stream, streamName, inputId,
          inputWanted ? &input : nullptr,
          forcedOutputDeviceId ? forcedOutputDeviceId : outputId, &output,
          latencyFrames, DataCallback_s, StateCallback_s, this) == CUBEB_OK) {
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
    mAudioStreamState = AudioStreamState::None;
    // Only report failures when we're not coming from a driver that was
    // created itself as a fallback driver because of a previous audio driver
    // failure.
    if (!fromFallback) {
      CubebUtils::ReportCubebStreamInitFailure(firstStream);
      FallbackToSystemClockDriver();
    }
    return;
  }

#ifdef XP_MACOSX
  PanOutputIfNeeded(inputWanted);
#endif

  cubeb_stream_register_device_changed_callback(
      mAudioStream, AudioCallbackDriver::DeviceChangedCallback_s);

  // No-op if MOZ_DUMP_AUDIO is not defined as an environment variable. This
  // is intended for diagnosing issues, and only works if the content sandbox is
  // disabled.
  mInputStreamFile.Open("GraphDriverInput", input.channels, input.rate);
  mOutputStreamFile.Open("GraphDriverOutput", output.channels, output.rate);

  if (NS_WARN_IF(!StartStream())) {
    LOG(LogLevel::Warning,
        ("%p: AudioCallbackDriver couldn't start a cubeb stream.", Graph()));
    return;
  }

  LOG(LogLevel::Debug, ("%p: AudioCallbackDriver started.", Graph()));
}

void AudioCallbackDriver::SetCubebStreamName(const nsCString& aStreamName) {
  MOZ_ASSERT(OnCubebOperationThread());
  MOZ_ASSERT(mAudioStream);
  cubeb_stream_set_name(mAudioStream, aStreamName.get());
}

void AudioCallbackDriver::Start() {
  MOZ_ASSERT(!IsStarted());
  MOZ_ASSERT(mAudioStreamState == AudioStreamState::None);
  MOZ_ASSERT_IF(PreviousDriver(), PreviousDriver()->InIteration());
  mAudioStreamState = AudioStreamState::Pending;

  if (mFallbackDriverState == FallbackDriverState::None) {
    // Starting an audio driver could take a while. We start a system driver in
    // the meantime so that the graph is kept running.
    FallbackToSystemClockDriver();
  }

  if (mPreviousDriver) {
    if (AudioCallbackDriver* previousAudioCallback =
            mPreviousDriver->AsAudioCallbackDriver()) {
      LOG(LogLevel::Debug, ("Releasing audio driver off main thread."));
      mCubebOperationThread->Dispatch(NS_NewRunnableFunction(
          "AudioCallbackDriver previousDriver::Stop()",
          [previousDriver = RefPtr{previousAudioCallback}] {
            previousDriver->Stop();
          }));
    } else {
      LOG(LogLevel::Debug,
          ("Dropping driver reference for SystemClockDriver."));
      MOZ_ASSERT(mPreviousDriver->AsSystemClockDriver());
    }
    mPreviousDriver = nullptr;
  }

  LOG(LogLevel::Debug, ("Starting new audio driver off main thread, "
                        "to ensure it runs after previous shutdown."));
  mCubebOperationThread->Dispatch(
      NS_NewRunnableFunction("AudioCallbackDriver Init()",
                             [self = RefPtr{this}, streamName = mStreamName] {
                               self->Init(streamName);
                             }));
}

bool AudioCallbackDriver::StartStream() {
  TRACE("AudioCallbackDriver::StartStream");
  MOZ_ASSERT(!IsStarted() && OnCubebOperationThread());
  // Set STARTING before cubeb_stream_start, since starting the cubeb stream
  // can result in a callback (that may read mAudioStreamState) before
  // mAudioStreamState would otherwise be set.
  mAudioStreamState = AudioStreamState::Starting;
  if (cubeb_stream_start(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not start cubeb stream for MTG.");
    return false;
  }

  return true;
}

void AudioCallbackDriver::Stop() {
  LOG(LogLevel::Debug,
      ("%p: AudioCallbackDriver::Stop driver=%p", Graph(), this));
  TRACE("AudioCallbackDriver::Stop");
  MOZ_ASSERT(OnCubebOperationThread());
  cubeb_stream_register_device_changed_callback(mAudioStream, nullptr);
  if (cubeb_stream_stop(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not stop cubeb stream for MTG.");
  } else {
    mAudioStreamState = AudioStreamState::None;
  }
}

void AudioCallbackDriver::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<FallbackWrapper> fallback;
  {
    auto fallbackLock = mFallback.Lock();
    fallback = fallbackLock.ref();
    fallbackLock.ref() = nullptr;
  }
  if (fallback) {
    LOG(LogLevel::Debug,
        ("%p: Releasing fallback driver %p.", Graph(), fallback.get()));
    fallback->Shutdown();
  }

  LOG(LogLevel::Debug,
      ("%p: Releasing audio driver off main thread (GraphDriver::Shutdown).",
       Graph()));

  nsLiteralCString reason("AudioCallbackDriver::Shutdown");
  NS_DispatchAndSpinEventLoopUntilComplete(
      reason, mCubebOperationThread,
      NS_NewRunnableFunction(reason.get(),
                             [self = RefPtr{this}] { self->Stop(); }));
}

void AudioCallbackDriver::SetStreamName(const nsACString& aStreamName) {
  MOZ_ASSERT(InIteration() || !ThreadRunning());
  if (aStreamName == mStreamName) {
    return;
  }
  // Record the stream name, which will be passed onto the next driver, if
  // any, either from this driver or the fallback driver.
  GraphDriver::SetStreamName(aStreamName);
  {
    auto fallbackLock = mFallback.Lock();
    FallbackWrapper* fallback = fallbackLock.ref().get();
    if (fallback) {
      MOZ_ASSERT(fallback->InIteration());
      fallback->SetStreamName(aStreamName);
    }
  }
  AudioStreamState streamState = mAudioStreamState;
  if (streamState != AudioStreamState::None &&
      streamState != AudioStreamState::Stopping) {
    mCubebOperationThread->Dispatch(
        NS_NewRunnableFunction("AudioCallbackDriver SetStreamName()",
                               [self = RefPtr{this}, streamName = mStreamName] {
                                 self->SetCubebStreamName(streamName);
                               }));
  }
}

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
  MOZ_ASSERT(mDriver->mAudioThreadIdInCb == std::thread::id());
  mDriver->mAudioThreadIdInCb = std::this_thread::get_id();
}

AudioCallbackDriver::AutoInCallback::~AutoInCallback() {
  MOZ_ASSERT(mDriver->mAudioThreadIdInCb == std::this_thread::get_id());
  mDriver->mAudioThreadIdInCb = std::thread::id();
}

bool AudioCallbackDriver::CheckThreadIdChanged() {
  ProfilerThreadId id = profiler_current_thread_id();
  if (id != mAudioThreadId) {
    mAudioThreadId = id;
    return true;
  }
  return false;
}

long AudioCallbackDriver::DataCallback(const AudioDataValue* aInputBuffer,
                                       AudioDataValue* aOutputBuffer,
                                       long aFrames) {
  if (!mSandboxed && CheckThreadIdChanged()) {
    CallbackThreadRegistry::Get()->Register(mAudioThreadId,
                                            "NativeAudioCallback");
  }

  if (mAudioStreamState.compareExchange(AudioStreamState::Starting,
                                        AudioStreamState::Running)) {
    LOG(LogLevel::Verbose, ("%p: AudioCallbackDriver %p First audio callback "
                            "close the Fallback driver",
                            Graph(), this));
  }

  FallbackDriverState fallbackState = mFallbackDriverState;
  if (MOZ_UNLIKELY(fallbackState == FallbackDriverState::Running)) {
    // Wait for the fallback driver to stop. Wake it up so it can stop if it's
    // sleeping.
    LOG(LogLevel::Verbose,
        ("%p: AudioCallbackDriver %p Waiting for the Fallback driver to stop",
         Graph(), this));
    EnsureNextIteration();
    PodZero(aOutputBuffer, aFrames * mOutputChannelCount);
    return aFrames;
  }

  if (MOZ_UNLIKELY(fallbackState == FallbackDriverState::Stopped)) {
    // We're supposed to stop.
    PodZero(aOutputBuffer, aFrames * mOutputChannelCount);
    if (!mSandboxed) {
      CallbackThreadRegistry::Get()->Unregister(mAudioThreadId);
    }
    return aFrames - 1;
  }

  MOZ_ASSERT(mAudioStreamState == AudioStreamState::Running);
  TRACE_AUDIO_CALLBACK_BUDGET("AudioCallbackDriver real-time budget", aFrames,
                              mSampleRate);
  TRACE("AudioCallbackDriver::DataCallback");

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
  uint32_t alreadyBuffered = mScratchBuffer.Empty(mBuffer);

  // State computed time is decided by the audio callback's buffer length. We
  // compute the iteration start and end from there, trying to keep the amount
  // of buffering in the graph constant.
  GraphTime nextStateComputedTime =
      MediaTrackGraphImpl::RoundUpToEndOfAudioBlock(mStateComputedTime +
                                                    mBuffer.Available());

  auto iterationStart = mIterationEnd;
  // inGraph is the number of audio frames there is between the state time and
  // the current time, i.e. the maximum theoretical length of the interval we
  // could use as [iterationStart; mIterationEnd].
  GraphTime inGraph = mStateComputedTime - iterationStart;
  // We want the interval [iterationStart; mIterationEnd] to be before the
  // interval [mStateComputedTime; nextStateComputedTime]. We also want
  // the distance between these intervals to be roughly equivalent each time, to
  // ensure there is no clock drift between current time and state time. Since
  // we can't act on the state time because we have to fill the audio buffer, we
  // reclock the current time against the state time, here.
  mIterationEnd = iterationStart + 0.8 * inGraph;

  LOG(LogLevel::Verbose,
      ("%p: interval[%ld; %ld] state[%ld; %ld] (frames: %ld) (durationMS: %u) "
       "(duration ticks: %ld)",
       Graph(), (long)iterationStart, (long)mIterationEnd,
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
                             mSampleRate, mInputChannelCount, alreadyBuffered);
  }

  IterationResult result =
      Graph()->OneIteration(nextStateComputedTime, mIterationEnd, this);

  mStateComputedTime = nextStateComputedTime;

  MOZ_ASSERT(mBuffer.Available() == 0,
             "The graph should have filled the buffer");

  mBuffer.BufferFilled();

#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  // Prevent returning NaN to the OS mixer, and propagating NaN into the reverse
  // stream of the AEC.
  NaNToZeroInPlace(aOutputBuffer, aFrames * mOutputChannelCount);
#endif

#ifdef XP_MACOSX
  // This only happens when the output is on a macbookpro's external speaker,
  // that are stereo, but let's just be safe.
  if (mNeedsPanning && mOutputChannelCount == 2) {
    // hard pan to the right
    for (uint32_t i = 0; i < aFrames * 2; i += 2) {
      aOutputBuffer[i + 1] += aOutputBuffer[i];
      aOutputBuffer[i] = 0.0;
    }
  }
#endif

  // No-op if MOZ_DUMP_AUDIO is not defined as an environment variable
  if (aInputBuffer) {
    mInputStreamFile.Write(static_cast<const AudioDataValue*>(aInputBuffer),
                           aFrames * mInputChannelCount);
  }
  mOutputStreamFile.Write(static_cast<const AudioDataValue*>(aOutputBuffer),
                          aFrames * mOutputChannelCount);

  if (result.IsStop()) {
    if (mInputDeviceID) {
      mGraphInterface->NotifyInputStopped();
    }
    // Signal that we have stopped.
    result.Stopped();
    // Update the flag before handing over the graph and going to drain.
    mAudioStreamState = AudioStreamState::Stopping;
    if (!mSandboxed) {
      CallbackThreadRegistry::Get()->Unregister(mAudioThreadId);
    }
    return aFrames - 1;
  }

  if (GraphDriver* nextDriver = result.NextDriver()) {
    LOG(LogLevel::Debug,
        ("%p: Switching to %s driver.", Graph(),
         nextDriver->AsAudioCallbackDriver() ? "audio" : "system"));
    if (mInputDeviceID) {
      mGraphInterface->NotifyInputStopped();
    }
    result.Switched();
    mAudioStreamState = AudioStreamState::Stopping;
    nextDriver->SetState(mStreamName, mIterationEnd, mStateComputedTime);
    nextDriver->Start();
    if (!mSandboxed) {
      CallbackThreadRegistry::Get()->Unregister(mAudioThreadId);
    }
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
      ("AudioCallbackDriver(%p) State: %s", this, StateToString(aState)));

  if (aState == CUBEB_STATE_STARTED || aState == CUBEB_STATE_STOPPED) {
    // Nothing to do for STARTED.
    //
    // For STOPPED, don't reset mAudioStreamState until after
    // cubeb_stream_stop() returns, as wasapi_stream_stop() dispatches
    // CUBEB_STATE_STOPPED before ensuring that data callbacks have finished.
    // https://searchfox.org/mozilla-central/rev/f9beb753a84aa297713d1565dcd0c5e3c66e4174/media/libcubeb/src/cubeb_wasapi.cpp#3009,3012
    return;
  }

  AudioStreamState streamState = mAudioStreamState;
  if (streamState < AudioStreamState::Starting) {
    // mAudioStream has already entered STOPPED, DRAINED, or ERROR.
    // Don't reset a Pending state indicating that a task to destroy
    // mAudioStream and init a new cubeb_stream has already been triggered.
    return;
  }

  // Reset for DRAINED or ERROR.
  streamState = mAudioStreamState.exchange(AudioStreamState::None);

  if (aState == CUBEB_STATE_ERROR) {
    // About to hand over control of the graph.  Do not start a new driver if
    // StateCallback() receives an error for this stream while the main thread
    // or another driver has control of the graph.
    if (streamState == AudioStreamState::Running) {
      if (mFallbackDriverState == FallbackDriverState::None) {
        // Only switch to fallback if it's not already running. It could be
        // running with the callback driver having started but not seen a single
        // callback yet. I.e., handover from fallback to callback is not done.
        if (mInputDeviceID) {
#ifdef DEBUG
          // No audio callback after an error. We're calling into the graph here
          // so we need to be regarded as "in iteration".
          AutoInCallback aic(this);
#endif
          mGraphInterface->NotifyInputStopped();
        }
        FallbackToSystemClockDriver();
      }
    }
  }
}

void AudioCallbackDriver::MixerCallback(AudioChunk* aMixedBuffer,
                                        uint32_t aSampleRate) {
  MOZ_ASSERT(InIteration());
  uint32_t toWrite = mBuffer.Available();

  TrackTime frameCount = aMixedBuffer->mDuration;
  if (!mBuffer.Available() && frameCount > 0) {
    NS_WARNING("DataCallback buffer full, expect frame drops.");
  }

  MOZ_ASSERT(mBuffer.Available() <= frameCount);

  mBuffer.WriteFrames(*aMixedBuffer, mBuffer.Available());
  MOZ_ASSERT(mBuffer.Available() == 0,
             "Missing frames to fill audio callback's buffer.");
  if (toWrite == frameCount) {
    return;
  }

  aMixedBuffer->SliceTo(toWrite, frameCount);
  DebugOnly<uint32_t> written = mScratchBuffer.Fill(*aMixedBuffer);
  NS_WARNING_ASSERTION(written == frameCount - toWrite, "Dropping frames.");
};

void AudioCallbackDriver::PanOutputIfNeeded(bool aMicrophoneActive) {
#ifdef XP_MACOSX
  TRACE("AudioCallbackDriver::PanOutputIfNeeded");
  cubeb_device* out = nullptr;
  int rv;
  char name[128];
  size_t length = sizeof(name);

  rv = sysctlbyname("hw.model", name, &length, NULL, 0);
  if (rv) {
    return;
  }

  int major, minor;
  for (uint32_t i = 0; i < length; i++) {
    // skip the model name
    if (isalpha(name[i])) {
      continue;
    }
    sscanf(name + i, "%d,%d", &major, &minor);
    break;
  }

  enum MacbookModel { MacBook, MacBookPro, MacBookAir, NotAMacbook };

  MacbookModel model;

  if (!strncmp(name, "MacBookPro", length)) {
    model = MacBookPro;
  } else if (strncmp(name, "MacBookAir", length)) {
    model = MacBookAir;
  } else if (strncmp(name, "MacBook", length)) {
    model = MacBook;
  } else {
    model = NotAMacbook;
  }
  // For macbook pro before 2016 model (change of chassis), hard pan the audio
  // to the right if the speakers are in use to avoid feedback.
  if (model == MacBookPro && major <= 12) {
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

void AudioCallbackDriver::EnsureNextIteration() {
  if (mFallbackDriverState == FallbackDriverState::Running) {
    auto fallback = mFallback.Lock();
    if (fallback.ref()) {
      fallback.ref()->EnsureNextIteration();
    }
  }
}

TimeDuration AudioCallbackDriver::AudioOutputLatency() {
  TRACE("AudioCallbackDriver::AudioOutputLatency");
  uint32_t latencyFrames;
  int rv = cubeb_stream_get_latency(mAudioStream, &latencyFrames);
  if (rv || mSampleRate == 0) {
    return TimeDuration::FromSeconds(0.0);
  }

  return TimeDuration::FromSeconds(static_cast<double>(latencyFrames) /
                                   mSampleRate);
}

bool AudioCallbackDriver::OnFallback() const {
  MOZ_ASSERT(InIteration());
  return mFallbackDriverState == FallbackDriverState::Running;
}

void AudioCallbackDriver::FallbackToSystemClockDriver() {
  MOZ_ASSERT(!ThreadRunning());
  MOZ_ASSERT(mAudioStreamState == AudioStreamState::None ||
             mAudioStreamState == AudioStreamState::Pending);
  MOZ_ASSERT(mFallbackDriverState == FallbackDriverState::None);
  LOG(LogLevel::Debug,
      ("%p: AudioCallbackDriver %p Falling back to SystemClockDriver.", Graph(),
       this));
  mFallbackDriverState = FallbackDriverState::Running;
  mNextReInitBackoffStep =
      TimeDuration::FromMilliseconds(AUDIO_INITIAL_FALLBACK_BACKOFF_STEP_MS);
  mNextReInitAttempt = TimeStamp::Now() + mNextReInitBackoffStep;
  auto fallback =
      MakeRefPtr<FallbackWrapper>(Graph(), this, mSampleRate, mStreamName,
                                  mIterationEnd, mStateComputedTime);
  {
    auto driver = mFallback.Lock();
    driver.ref() = fallback;
  }
  fallback->Start();
}

void AudioCallbackDriver::FallbackDriverStopped(GraphTime aIterationEnd,
                                                GraphTime aStateComputedTime,
                                                FallbackDriverState aState) {
  mIterationEnd = aIterationEnd;
  mStateComputedTime = aStateComputedTime;
  mNextReInitAttempt = TimeStamp();
  mNextReInitBackoffStep = TimeDuration();
  {
    auto fallback = mFallback.Lock();
    MOZ_ASSERT(fallback.ref()->OnThread());
    fallback.ref() = nullptr;
  }

  MOZ_ASSERT(aState == FallbackDriverState::None ||
             aState == FallbackDriverState::Stopped);
  MOZ_ASSERT_IF(aState == FallbackDriverState::None,
                mAudioStreamState == AudioStreamState::Running);
  mFallbackDriverState = aState;
}

void AudioCallbackDriver::MaybeStartAudioStream() {
  AudioStreamState streamState = mAudioStreamState;
  if (streamState != AudioStreamState::None) {
    LOG(LogLevel::Verbose,
        ("%p: AudioCallbackDriver %p Cannot re-init.", Graph(), this));
    return;
  }

  TimeStamp now = TimeStamp::Now();
  if (now < mNextReInitAttempt) {
    LOG(LogLevel::Verbose,
        ("%p: AudioCallbackDriver %p Not time to re-init yet. %.3fs left.",
         Graph(), this, (mNextReInitAttempt - now).ToSeconds()));
    return;
  }

  LOG(LogLevel::Debug, ("%p: AudioCallbackDriver %p Attempting to re-init "
                        "audio stream from fallback driver.",
                        Graph(), this));
  mNextReInitBackoffStep =
      std::min(mNextReInitBackoffStep * 2,
               TimeDuration::FromMilliseconds(
                   StaticPrefs::media_audio_device_retry_ms()));
  mNextReInitAttempt = now + mNextReInitBackoffStep;
  Start();
}

}  // namespace mozilla

// avoid redefined macro in unified build
#undef LOG
