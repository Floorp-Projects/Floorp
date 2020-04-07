/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "mozilla/Logging.h"
#include "prdtoa.h"
#include "AudioStream.h"
#include "VideoUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"
#include <algorithm>
#include "mozilla/Telemetry.h"
#include "CubebUtils.h"
#include "nsPrintfCString.h"
#include "AudioConverter.h"
#include "UnderrunHandler.h"
#if defined(XP_WIN)
#  include "nsXULAppAPI.h"
#endif
#include "Tracing.h"

// Use abort() instead of exception in SoundTouch.
#define ST_NO_EXCEPTION_HANDLING 1
#include "soundtouch/SoundTouchFactory.h"

namespace mozilla {

#undef LOG
#undef LOGW
#undef LOGE

LazyLogModule gAudioStreamLog("AudioStream");
// For simple logs
#define LOG(x, ...)                                  \
  MOZ_LOG(gAudioStreamLog, mozilla::LogLevel::Debug, \
          ("%p " x, this, ##__VA_ARGS__))
#define LOGW(x, ...)                                   \
  MOZ_LOG(gAudioStreamLog, mozilla::LogLevel::Warning, \
          ("%p " x, this, ##__VA_ARGS__))
#define LOGE(x, ...)                                                          \
  NS_DebugBreak(NS_DEBUG_WARNING,                                             \
                nsPrintfCString("%p " x, this, ##__VA_ARGS__).get(), nullptr, \
                __FILE__, __LINE__)

/**
 * Keep a list of frames sent to the audio engine in each DataCallback along
 * with the playback rate at the moment. Since the playback rate and number of
 * underrun frames can vary in each callback. We need to keep the whole history
 * in order to calculate the playback position of the audio engine correctly.
 */
class FrameHistory {
  struct Chunk {
    uint32_t servicedFrames;
    uint32_t totalFrames;
    uint32_t rate;
  };

  template <typename T>
  static T FramesToUs(uint32_t frames, int rate) {
    return static_cast<T>(frames) * USECS_PER_S / rate;
  }

 public:
  FrameHistory() : mBaseOffset(0), mBasePosition(0) {}

  void Append(uint32_t aServiced, uint32_t aUnderrun, uint32_t aRate) {
    /* In most case where playback rate stays the same and we don't underrun
     * frames, we are able to merge chunks to avoid lose of precision to add up
     * in compressing chunks into |mBaseOffset| and |mBasePosition|.
     */
    if (!mChunks.IsEmpty()) {
      Chunk& c = mChunks.LastElement();
      // 2 chunks (c1 and c2) can be merged when rate is the same and
      // adjacent frames are zero. That is, underrun frames in c1 are zero
      // or serviced frames in c2 are zero.
      if (c.rate == aRate &&
          (c.servicedFrames == c.totalFrames || aServiced == 0)) {
        c.servicedFrames += aServiced;
        c.totalFrames += aServiced + aUnderrun;
        return;
      }
    }
    Chunk* p = mChunks.AppendElement();
    p->servicedFrames = aServiced;
    p->totalFrames = aServiced + aUnderrun;
    p->rate = aRate;
  }

  /**
   * @param frames The playback position in frames of the audio engine.
   * @return The playback position in microseconds of the audio engine,
   *         adjusted by playback rate changes and underrun frames.
   */
  int64_t GetPosition(int64_t frames) {
    // playback position should not go backward.
    MOZ_ASSERT(frames >= mBaseOffset);
    while (true) {
      if (mChunks.IsEmpty()) {
        return mBasePosition;
      }
      const Chunk& c = mChunks[0];
      if (frames <= mBaseOffset + c.totalFrames) {
        uint32_t delta = frames - mBaseOffset;
        delta = std::min(delta, c.servicedFrames);
        return static_cast<int64_t>(mBasePosition) +
               FramesToUs<int64_t>(delta, c.rate);
      }
      // Since the playback position of the audio engine will not go backward,
      // we are able to compress chunks so that |mChunks| won't grow
      // unlimitedly. Note that we lose precision in converting integers into
      // floats and inaccuracy will accumulate over time. However, for a 24hr
      // long, sample rate = 44.1k file, the error will be less than 1
      // microsecond after playing 24 hours. So we are fine with that.
      mBaseOffset += c.totalFrames;
      mBasePosition += FramesToUs<double>(c.servicedFrames, c.rate);
      mChunks.RemoveElementAt(0);
    }
  }

 private:
  AutoTArray<Chunk, 7> mChunks;
  int64_t mBaseOffset;
  double mBasePosition;
};

AudioStream::AudioStream(DataSource& aSource)
    : mMonitor("AudioStream"),
      mChannels(0),
      mOutChannels(0),
      mTimeStretcher(nullptr),
      mState(INITIALIZED),
      mDataSource(aSource),
      mPrefillQuirk(false) {
#if defined(XP_WIN)
  if (XRE_IsContentProcess()) {
    audio::AudioNotificationReceiver::Register(this);
  }
#endif
}

AudioStream::~AudioStream() {
  LOG("deleted, state %d", mState);
  MOZ_ASSERT(mState == SHUTDOWN && !mCubebStream,
             "Should've called Shutdown() before deleting an AudioStream");
  if (mTimeStretcher) {
    soundtouch::destroySoundTouchObj(mTimeStretcher);
  }
#if defined(XP_WIN)
  if (XRE_IsContentProcess()) {
    audio::AudioNotificationReceiver::Unregister(this);
  }
#endif
}

size_t AudioStream::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = aMallocSizeOf(this);

  // Possibly add in the future:
  // - mTimeStretcher
  // - mCubebStream

  return amount;
}

nsresult AudioStream::EnsureTimeStretcherInitializedUnlocked() {
  mMonitor.AssertCurrentThreadOwns();
  if (!mTimeStretcher) {
    mTimeStretcher = soundtouch::createSoundTouchObj();
    mTimeStretcher->setSampleRate(mAudioClock.GetInputRate());
    mTimeStretcher->setChannels(mOutChannels);
    mTimeStretcher->setPitch(1.0);

    // SoundTouch v2.1.2 uses automatic time-stretch settings with the following
    // values:
    // Tempo 0.5: 90ms sequence, 20ms seekwindow, 8ms overlap
    // Tempo 2.0: 40ms sequence, 15ms seekwindow, 8ms overlap
    // We are going to use a smaller 10ms sequence size to improve speech
    // clarity, giving more resolution at high tempo and less reverb at low
    // tempo. Maintain 15ms seekwindow and 8ms overlap for smoothness.
    mTimeStretcher->setSetting(SETTING_SEQUENCE_MS, 10);
    mTimeStretcher->setSetting(SETTING_SEEKWINDOW_MS, 15);
    mTimeStretcher->setSetting(SETTING_OVERLAP_MS, 8);
  }
  return NS_OK;
}

nsresult AudioStream::SetPlaybackRate(double aPlaybackRate) {
  TRACE();
  // MUST lock since the rate transposer is used from the cubeb callback,
  // and rate changes can cause the buffer to be reallocated
  MonitorAutoLock mon(mMonitor);

  NS_ASSERTION(
      aPlaybackRate > 0.0,
      "Can't handle negative or null playbackrate in the AudioStream.");
  // Avoid instantiating the resampler if we are not changing the playback rate.
  // GetPreservesPitch/SetPreservesPitch don't need locking before calling
  if (aPlaybackRate == mAudioClock.GetPlaybackRate()) {
    return NS_OK;
  }

  if (EnsureTimeStretcherInitializedUnlocked() != NS_OK) {
    return NS_ERROR_FAILURE;
  }

  mAudioClock.SetPlaybackRate(aPlaybackRate);

  if (mAudioClock.GetPreservesPitch()) {
    mTimeStretcher->setTempo(aPlaybackRate);
    mTimeStretcher->setRate(1.0f);
  } else {
    mTimeStretcher->setTempo(1.0f);
    mTimeStretcher->setRate(aPlaybackRate);
  }
  return NS_OK;
}

nsresult AudioStream::SetPreservesPitch(bool aPreservesPitch) {
  TRACE();
  // MUST lock since the rate transposer is used from the cubeb callback,
  // and rate changes can cause the buffer to be reallocated
  MonitorAutoLock mon(mMonitor);

  // Avoid instantiating the timestretcher instance if not needed.
  if (aPreservesPitch == mAudioClock.GetPreservesPitch()) {
    return NS_OK;
  }

  if (EnsureTimeStretcherInitializedUnlocked() != NS_OK) {
    return NS_ERROR_FAILURE;
  }

  if (aPreservesPitch == true) {
    mTimeStretcher->setTempo(mAudioClock.GetPlaybackRate());
    mTimeStretcher->setRate(1.0f);
  } else {
    mTimeStretcher->setTempo(1.0f);
    mTimeStretcher->setRate(mAudioClock.GetPlaybackRate());
  }

  mAudioClock.SetPreservesPitch(aPreservesPitch);

  return NS_OK;
}

template <AudioSampleFormat N>
struct ToCubebFormat {
  static const cubeb_sample_format value = CUBEB_SAMPLE_FLOAT32NE;
};

template <>
struct ToCubebFormat<AUDIO_FORMAT_S16> {
  static const cubeb_sample_format value = CUBEB_SAMPLE_S16NE;
};

template <typename Function, typename... Args>
int AudioStream::InvokeCubeb(Function aFunction, Args&&... aArgs) {
  MonitorAutoUnlock mon(mMonitor);
  return aFunction(mCubebStream.get(), std::forward<Args>(aArgs)...);
}

nsresult AudioStream::Init(uint32_t aNumChannels,
                           AudioConfig::ChannelLayout::ChannelMap aChannelMap,
                           uint32_t aRate, AudioDeviceInfo* aSinkInfo) {
  StartAudioCallbackTracing();

  auto startTime = TimeStamp::Now();
  TRACE();

  LOG("%s channels: %d, rate: %d", __FUNCTION__, aNumChannels, aRate);
  mChannels = aNumChannels;
  mOutChannels = aNumChannels;

  mSinkInfo = aSinkInfo;

  cubeb_stream_params params;
  params.rate = aRate;
  params.channels = mOutChannels;
  params.layout = static_cast<uint32_t>(aChannelMap);
  params.format = ToCubebFormat<AUDIO_OUTPUT_FORMAT>::value;
  params.prefs = CubebUtils::GetDefaultStreamPrefs();

  // This is noop if MOZ_DUMP_AUDIO is not set.
  mDumpFile.Open("AudioStream", mOutChannels, aRate);

  mAudioClock.Init(aRate);

  cubeb* cubebContext = CubebUtils::GetCubebContext();
  if (!cubebContext) {
    LOGE("Can't get cubeb context!");
    CubebUtils::ReportCubebStreamInitFailure(true);
    return NS_ERROR_DOM_MEDIA_CUBEB_INITIALIZATION_ERR;
  }

  // cubeb's winmm backend prefills buffers on init rather than stream start.
  // See https://github.com/kinetiknz/cubeb/issues/150
  mPrefillQuirk = !strcmp(cubeb_get_backend_id(cubebContext), "winmm");

  return OpenCubeb(cubebContext, params, startTime,
                   CubebUtils::GetFirstStream());
}

nsresult AudioStream::OpenCubeb(cubeb* aContext, cubeb_stream_params& aParams,
                                TimeStamp aStartTime, bool aIsFirst) {
  TRACE();
  MOZ_ASSERT(aContext);

  cubeb_stream* stream = nullptr;
  /* Convert from milliseconds to frames. */
  uint32_t latency_frames =
      CubebUtils::GetCubebPlaybackLatencyInMilliseconds() * aParams.rate / 1000;
  cubeb_devid deviceID = nullptr;
  if (mSinkInfo && mSinkInfo->DeviceID()) {
    deviceID = mSinkInfo->DeviceID();
  }
  if (cubeb_stream_init(aContext, &stream, "AudioStream", nullptr, nullptr,
                        deviceID, &aParams, latency_frames, DataCallback_S,
                        StateCallback_S, this) == CUBEB_OK) {
    mCubebStream.reset(stream);
    CubebUtils::ReportCubebBackendUsed();
  } else {
    LOGE("OpenCubeb() failed to init cubeb");
    CubebUtils::ReportCubebStreamInitFailure(aIsFirst);
    return NS_ERROR_FAILURE;
  }

  TimeDuration timeDelta = TimeStamp::Now() - aStartTime;
  LOG("creation time %sfirst: %u ms", aIsFirst ? "" : "not ",
      (uint32_t)timeDelta.ToMilliseconds());

  return NS_OK;
}

void AudioStream::SetVolume(double aVolume) {
  TRACE();
  MOZ_ASSERT(aVolume >= 0.0 && aVolume <= 1.0, "Invalid volume");

  {
    MonitorAutoLock mon(mMonitor);
    MOZ_ASSERT(mState != SHUTDOWN, "Don't set volume after shutdown.");
    if (mState == ERRORED) {
      return;
    }
  }

  if (cubeb_stream_set_volume(mCubebStream.get(),
                              aVolume * CubebUtils::GetVolumeScale()) !=
      CUBEB_OK) {
    LOGE("Could not change volume on cubeb stream.");
  }
}

nsresult AudioStream::Start() {
  TRACE();
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mState == INITIALIZED);
  mState = STARTED;
  auto r = InvokeCubeb(cubeb_stream_start);
  if (r != CUBEB_OK) {
    mState = ERRORED;
  }
  LOG("started, state %s", mState == STARTED
                               ? "STARTED"
                               : mState == DRAINED ? "DRAINED" : "ERRORED");
  if (mState == STARTED || mState == DRAINED) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

void AudioStream::Pause() {
  TRACE();
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mState != INITIALIZED, "Must be Start()ed.");
  MOZ_ASSERT(mState != STOPPED, "Already Pause()ed.");
  MOZ_ASSERT(mState != SHUTDOWN, "Already Shutdown()ed.");

  // Do nothing if we are already drained or errored.
  if (mState == DRAINED || mState == ERRORED) {
    return;
  }

  if (InvokeCubeb(cubeb_stream_stop) != CUBEB_OK) {
    mState = ERRORED;
  } else if (mState != DRAINED && mState != ERRORED) {
    // Don't transition to other states if we are already
    // drained or errored.
    mState = STOPPED;
  }
}

void AudioStream::Resume() {
  TRACE();
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mState != INITIALIZED, "Must be Start()ed.");
  MOZ_ASSERT(mState != STARTED, "Already Start()ed.");
  MOZ_ASSERT(mState != SHUTDOWN, "Already Shutdown()ed.");

  // Do nothing if we are already drained or errored.
  if (mState == DRAINED || mState == ERRORED) {
    return;
  }

  if (InvokeCubeb(cubeb_stream_start) != CUBEB_OK) {
    mState = ERRORED;
  } else if (mState != DRAINED && mState != ERRORED) {
    // Don't transition to other states if we are already
    // drained or errored.
    mState = STARTED;
  }
}

void AudioStream::Shutdown() {
  TRACE();
  MonitorAutoLock mon(mMonitor);
  LOG("Shutdown, state %d", mState);

  if (mCubebStream) {
    MonitorAutoUnlock mon(mMonitor);
    // Force stop to put the cubeb stream in a stable state before deletion.
    cubeb_stream_stop(mCubebStream.get());
    // Must not try to shut down cubeb from within the lock!  wasapi may still
    // call our callback after Pause()/stop()!?! Bug 996162
    mCubebStream.reset();

    StopAudioCallbackTracing();
  }

  mState = SHUTDOWN;
}

#if defined(XP_WIN)
void AudioStream::ResetDefaultDevice() {
  TRACE();
  MonitorAutoLock mon(mMonitor);
  if (mState != STARTED && mState != STOPPED) {
    return;
  }

  MOZ_ASSERT(mCubebStream);
  auto r = InvokeCubeb(cubeb_stream_reset_default_device);
  if (!(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED)) {
    mState = ERRORED;
  }
}
#endif

int64_t AudioStream::GetPosition() {
  TRACE();
  MonitorAutoLock mon(mMonitor);
  int64_t frames = GetPositionInFramesUnlocked();
  return frames >= 0 ? mAudioClock.GetPosition(frames) : -1;
}

int64_t AudioStream::GetPositionInFrames() {
  TRACE();
  MonitorAutoLock mon(mMonitor);
  int64_t frames = GetPositionInFramesUnlocked();
  return frames >= 0 ? mAudioClock.GetPositionInFrames(frames) : -1;
}

int64_t AudioStream::GetPositionInFramesUnlocked() {
  mMonitor.AssertCurrentThreadOwns();

  if (mState == ERRORED) {
    return -1;
  }

  uint64_t position = 0;
  if (InvokeCubeb(cubeb_stream_get_position, &position) != CUBEB_OK) {
    return -1;
  }
  return std::min<uint64_t>(position, INT64_MAX);
}

bool AudioStream::IsValidAudioFormat(Chunk* aChunk) {
  if (aChunk->Rate() != mAudioClock.GetInputRate()) {
    LOGW("mismatched sample %u, mInRate=%u", aChunk->Rate(),
         mAudioClock.GetInputRate());
    return false;
  }

  if (aChunk->Channels() > 8) {
    return false;
  }

  return true;
}

void AudioStream::GetUnprocessed(AudioBufferWriter& aWriter) {
  TRACE();
  mMonitor.AssertCurrentThreadOwns();

  // Flush the timestretcher pipeline, if we were playing using a playback rate
  // other than 1.0.
  if (mTimeStretcher && mTimeStretcher->numSamples()) {
    auto timeStretcher = mTimeStretcher;
    aWriter.Write(
        [timeStretcher](AudioDataValue* aPtr, uint32_t aFrames) {
          return timeStretcher->receiveSamples(aPtr, aFrames);
        },
        aWriter.Available());

    // TODO: There might be still unprocessed samples in the stretcher.
    // We should either remove or flush them so they won't be in the output
    // next time we switch a playback rate other than 1.0.
    NS_WARNING_ASSERTION(mTimeStretcher->numUnprocessedSamples() == 0,
                         "no samples");
  }

  while (aWriter.Available() > 0) {
    UniquePtr<Chunk> c = mDataSource.PopFrames(aWriter.Available());
    if (c->Frames() == 0) {
      break;
    }
    MOZ_ASSERT(c->Frames() <= aWriter.Available());
    if (IsValidAudioFormat(c.get())) {
      aWriter.Write(c->Data(), c->Frames());
    } else {
      // Write silence if invalid format.
      aWriter.WriteZeros(c->Frames());
    }
  }
}

void AudioStream::GetTimeStretched(AudioBufferWriter& aWriter) {
  TRACE();
  mMonitor.AssertCurrentThreadOwns();

  // We need to call the non-locking version, because we already have the lock.
  if (EnsureTimeStretcherInitializedUnlocked() != NS_OK) {
    return;
  }

  uint32_t toPopFrames =
      ceil(aWriter.Available() * mAudioClock.GetPlaybackRate());

  while (mTimeStretcher->numSamples() < aWriter.Available()) {
    UniquePtr<Chunk> c = mDataSource.PopFrames(toPopFrames);
    if (c->Frames() == 0) {
      break;
    }
    MOZ_ASSERT(c->Frames() <= toPopFrames);
    if (IsValidAudioFormat(c.get())) {
      mTimeStretcher->putSamples(c->Data(), c->Frames());
    } else {
      // Write silence if invalid format.
      AutoTArray<AudioDataValue, 1000> buf;
      auto size = CheckedUint32(mOutChannels) * c->Frames();
      if (!size.isValid()) {
        // The overflow should not happen in normal case.
        LOGW("Invalid member data: %d channels, %d frames", mOutChannels,
             c->Frames());
        return;
      }
      buf.SetLength(size.value());
      size = size * sizeof(AudioDataValue);
      if (!size.isValid()) {
        LOGW("The required memory size is too large.");
        return;
      }
      memset(buf.Elements(), 0, size.value());
      mTimeStretcher->putSamples(buf.Elements(), c->Frames());
    }
  }

  auto timeStretcher = mTimeStretcher;
  aWriter.Write(
      [timeStretcher](AudioDataValue* aPtr, uint32_t aFrames) {
        return timeStretcher->receiveSamples(aPtr, aFrames);
      },
      aWriter.Available());
}

long AudioStream::DataCallback(void* aBuffer, long aFrames) {
  TRACE_AUDIO_CALLBACK_BUDGET(aFrames, mAudioClock.GetInputRate());
  TRACE();
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mState != SHUTDOWN, "No data callback after shutdown");

  if (SoftRealTimeLimitReached()) {
    DemoteThreadFromRealTime();
  }

  auto writer = AudioBufferWriter(
      MakeSpan<AudioDataValue>(reinterpret_cast<AudioDataValue*>(aBuffer),
                               mOutChannels * aFrames),
      mOutChannels, aFrames);

  if (mPrefillQuirk) {
    // Don't consume audio data until Start() is called.
    // Expected only with cubeb winmm backend.
    if (mState == INITIALIZED) {
      NS_WARNING("data callback fires before cubeb_stream_start() is called");
      mAudioClock.UpdateFrameHistory(0, aFrames);
      return writer.WriteZeros(aFrames);
    }
  } else {
    MOZ_ASSERT(mState != INITIALIZED);
  }

  // NOTE: wasapi (others?) can call us back *after* stop()/Shutdown() (mState
  // == SHUTDOWN) Bug 996162

  if (mAudioClock.GetInputRate() == mAudioClock.GetOutputRate()) {
    GetUnprocessed(writer);
  } else {
    GetTimeStretched(writer);
  }

  // Always send audible frames first, and silent frames later.
  // Otherwise it will break the assumption of FrameHistory.
  if (!mDataSource.Ended()) {
    mAudioClock.UpdateFrameHistory(aFrames - writer.Available(),
                                   writer.Available());
    if (writer.Available() > 0) {
      LOGW("lost %d frames", writer.Available());
      writer.WriteZeros(writer.Available());
    }
  } else {
    // No more new data in the data source. Don't send silent frames so the
    // cubeb stream can start draining.
    mAudioClock.UpdateFrameHistory(aFrames - writer.Available(), 0);
  }

  mDumpFile.Write(static_cast<const AudioDataValue*>(aBuffer),
                  aFrames * mOutChannels);

  return aFrames - writer.Available();
}

void AudioStream::StateCallback(cubeb_state aState) {
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mState != SHUTDOWN, "No state callback after shutdown");
  LOG("StateCallback, mState=%d cubeb_state=%d", mState, aState);
  if (aState == CUBEB_STATE_DRAINED) {
    mState = DRAINED;
    mDataSource.Drained();
  } else if (aState == CUBEB_STATE_ERROR) {
    LOGE("StateCallback() state %d cubeb error", mState);
    mState = ERRORED;
    mDataSource.Errored();
  }
}

AudioClock::AudioClock()
    : mOutRate(0),
      mInRate(0),
      mPreservesPitch(true),
      mFrameHistory(new FrameHistory()) {}

void AudioClock::Init(uint32_t aRate) {
  mOutRate = aRate;
  mInRate = aRate;
}

void AudioClock::UpdateFrameHistory(uint32_t aServiced, uint32_t aUnderrun) {
  mFrameHistory->Append(aServiced, aUnderrun, mOutRate);
}

int64_t AudioClock::GetPositionInFrames(int64_t aFrames) const {
  CheckedInt64 v = UsecsToFrames(GetPosition(aFrames), mInRate);
  return v.isValid() ? v.value() : -1;
}

int64_t AudioClock::GetPosition(int64_t frames) const {
  return mFrameHistory->GetPosition(frames);
}

void AudioClock::SetPlaybackRate(double aPlaybackRate) {
  mOutRate = static_cast<uint32_t>(mInRate / aPlaybackRate);
}

double AudioClock::GetPlaybackRate() const {
  return static_cast<double>(mInRate) / mOutRate;
}

void AudioClock::SetPreservesPitch(bool aPreservesPitch) {
  mPreservesPitch = aPreservesPitch;
}

bool AudioClock::GetPreservesPitch() const { return mPreservesPitch; }

}  // namespace mozilla
