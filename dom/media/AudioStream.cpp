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
#include "mozilla/dom/AudioDeviceInfo.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"
#include <algorithm>
#include "mozilla/Telemetry.h"
#include "CubebUtils.h"
#include "nsNativeCharsetUtils.h"
#include "nsPrintfCString.h"
#include "AudioConverter.h"
#include "UnderrunHandler.h"
#if defined(XP_WIN)
#  include "nsXULAppAPI.h"
#endif
#include "Tracing.h"
#include "webaudio/blink/DenormalDisabler.h"
#include "AudioThreadRegistry.h"
#include "mozilla/StaticPrefs_media.h"

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
  static T FramesToUs(uint32_t frames, uint32_t rate) {
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
        return static_cast<int64_t>(mBasePosition);
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

AudioStream::AudioStream(DataSource& aSource, uint32_t aInRate,
                         uint32_t aOutputChannels,
                         AudioConfig::ChannelLayout::ChannelMap aChannelMap)
    : mTimeStretcher(nullptr),
      mAudioClock(aInRate),
      mChannelMap(aChannelMap),
      mMonitor("AudioStream"),
      mOutChannels(aOutputChannels),
      mState(INITIALIZED),
      mDataSource(aSource),
      mAudioThreadId(ProfilerThreadId{}),
      mSandboxed(CubebUtils::SandboxEnabled()),
      mPlaybackComplete(false),
      mPlaybackRate(1.0f),
      mPreservesPitch(true),
      mCallbacksStarted(false) {}

AudioStream::~AudioStream() {
  LOG("deleted, state %d", mState.load());
  MOZ_ASSERT(mState == SHUTDOWN && !mCubebStream,
             "Should've called Shutdown() before deleting an AudioStream");
}

size_t AudioStream::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = aMallocSizeOf(this);

  // Possibly add in the future:
  // - mTimeStretcher
  // - mCubebStream

  return amount;
}

nsresult AudioStream::EnsureTimeStretcherInitialized() {
  AssertIsOnAudioThread();
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
    mTimeStretcher->setSetting(
        SETTING_SEQUENCE_MS,
        StaticPrefs::media_audio_playbackrate_soundtouch_sequence_ms());
    mTimeStretcher->setSetting(
        SETTING_SEEKWINDOW_MS,
        StaticPrefs::media_audio_playbackrate_soundtouch_seekwindow_ms());
    mTimeStretcher->setSetting(
        SETTING_OVERLAP_MS,
        StaticPrefs::media_audio_playbackrate_soundtouch_overlap_ms());
  }
  return NS_OK;
}

nsresult AudioStream::SetPlaybackRate(double aPlaybackRate) {
  TRACE("AudioStream::SetPlaybackRate");
  NS_ASSERTION(
      aPlaybackRate > 0.0,
      "Can't handle negative or null playbackrate in the AudioStream.");
  if (aPlaybackRate == mPlaybackRate) {
    return NS_OK;
  }

  mPlaybackRate = static_cast<float>(aPlaybackRate);

  return NS_OK;
}

nsresult AudioStream::SetPreservesPitch(bool aPreservesPitch) {
  TRACE("AudioStream::SetPreservesPitch");
  if (aPreservesPitch == mPreservesPitch) {
    return NS_OK;
  }

  mPreservesPitch = aPreservesPitch;

  return NS_OK;
}

template <typename Function, typename... Args>
int AudioStream::InvokeCubeb(Function aFunction, Args&&... aArgs) {
  mMonitor.AssertCurrentThreadOwns();
  MonitorAutoUnlock mon(mMonitor);
  return aFunction(mCubebStream.get(), std::forward<Args>(aArgs)...);
}

nsresult AudioStream::Init(AudioDeviceInfo* aSinkInfo)
    MOZ_NO_THREAD_SAFETY_ANALYSIS {
  auto startTime = TimeStamp::Now();
  TRACE("AudioStream::Init");

  LOG("%s channels: %d, rate: %d", __FUNCTION__, mOutChannels,
      mAudioClock.GetInputRate());

  mSinkInfo = aSinkInfo;

  cubeb_stream_params params;
  params.rate = mAudioClock.GetInputRate();
  params.channels = mOutChannels;
  params.layout = static_cast<uint32_t>(mChannelMap);
  params.format = CubebUtils::ToCubebFormat<AUDIO_OUTPUT_FORMAT>::value;
  params.prefs = CubebUtils::GetDefaultStreamPrefs(CUBEB_DEVICE_TYPE_OUTPUT);

  // This is noop if MOZ_DUMP_AUDIO is not set.
  mDumpFile.Open("AudioStream", mOutChannels, mAudioClock.GetInputRate());

  cubeb* cubebContext = CubebUtils::GetCubebContext();
  if (!cubebContext) {
    LOGE("Can't get cubeb context!");
    CubebUtils::ReportCubebStreamInitFailure(true);
    return NS_ERROR_DOM_MEDIA_CUBEB_INITIALIZATION_ERR;
  }

  return OpenCubeb(cubebContext, params, startTime,
                   CubebUtils::GetFirstStream());
}

nsresult AudioStream::OpenCubeb(cubeb* aContext, cubeb_stream_params& aParams,
                                TimeStamp aStartTime, bool aIsFirst) {
  TRACE("AudioStream::OpenCubeb");
  MOZ_ASSERT(aContext);

  cubeb_stream* stream = nullptr;
  /* Convert from milliseconds to frames. */
  uint32_t latency_frames =
      CubebUtils::GetCubebPlaybackLatencyInMilliseconds() * aParams.rate / 1000;
  cubeb_devid deviceID = nullptr;
  if (mSinkInfo && mSinkInfo->DeviceID()) {
    deviceID = mSinkInfo->DeviceID();
  }
  if (CubebUtils::CubebStreamInit(aContext, &stream, "AudioStream", nullptr,
                                  nullptr, deviceID, &aParams, latency_frames,
                                  DataCallback_S, StateCallback_S,
                                  this) == CUBEB_OK) {
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
  TRACE("AudioStream::SetVolume");
  MOZ_ASSERT(aVolume >= 0.0 && aVolume <= 1.0, "Invalid volume");

  MOZ_ASSERT(mState != SHUTDOWN, "Don't set volume after shutdown.");
  if (mState == ERRORED) {
    return;
  }

  MonitorAutoLock mon(mMonitor);
  if (InvokeCubeb(cubeb_stream_set_volume,
                  aVolume * CubebUtils::GetVolumeScale()) != CUBEB_OK) {
    LOGE("Could not change volume on cubeb stream.");
  }
}

void AudioStream::SetStreamName(const nsAString& aStreamName) {
  TRACE("AudioStream::SetStreamName");

  nsAutoCString aRawStreamName;
  nsresult rv = NS_CopyUnicodeToNative(aStreamName, aRawStreamName);

  if (NS_FAILED(rv) || aStreamName.IsEmpty()) {
    return;
  }

  MonitorAutoLock mon(mMonitor);
  if (InvokeCubeb(cubeb_stream_set_name, aRawStreamName.get()) != CUBEB_OK) {
    LOGE("Could not set cubeb stream name.");
  }
}

nsresult AudioStream::Start(
    MozPromiseHolder<MediaSink::EndedPromise>& aEndedPromise) {
  TRACE("AudioStream::Start");
  MOZ_ASSERT(mState == INITIALIZED);
  mState = STARTED;
  RefPtr<MediaSink::EndedPromise> promise;
  {
    MonitorAutoLock mon(mMonitor);
    // As cubeb might call audio stream's state callback very soon after we
    // start cubeb, we have to create the promise beforehand in order to handle
    // the case where we immediately get `drained`.
    mEndedPromise = std::move(aEndedPromise);
    mPlaybackComplete = false;

    if (InvokeCubeb(cubeb_stream_start) != CUBEB_OK) {
      mState = ERRORED;
    }
  }

  LOG("started, state %s", mState == STARTED   ? "STARTED"
                           : mState == DRAINED ? "DRAINED"
                                               : "ERRORED");
  if (mState == STARTED || mState == DRAINED) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

void AudioStream::Pause() {
  TRACE("AudioStream::Pause");
  MOZ_ASSERT(mState != INITIALIZED, "Must be Start()ed.");
  MOZ_ASSERT(mState != STOPPED, "Already Pause()ed.");
  MOZ_ASSERT(mState != SHUTDOWN, "Already Shutdown()ed.");

  // Do nothing if we are already drained or errored.
  if (mState == DRAINED || mState == ERRORED) {
    return;
  }

  MonitorAutoLock mon(mMonitor);
  if (InvokeCubeb(cubeb_stream_stop) != CUBEB_OK) {
    mState = ERRORED;
  } else if (mState != DRAINED && mState != ERRORED) {
    // Don't transition to other states if we are already
    // drained or errored.
    mState = STOPPED;
  }
}

void AudioStream::Resume() {
  TRACE("AudioStream::Resume");
  MOZ_ASSERT(mState != INITIALIZED, "Must be Start()ed.");
  MOZ_ASSERT(mState != STARTED, "Already Start()ed.");
  MOZ_ASSERT(mState != SHUTDOWN, "Already Shutdown()ed.");

  // Do nothing if we are already drained or errored.
  if (mState == DRAINED || mState == ERRORED) {
    return;
  }

  MonitorAutoLock mon(mMonitor);
  if (InvokeCubeb(cubeb_stream_start) != CUBEB_OK) {
    mState = ERRORED;
  } else if (mState != DRAINED && mState != ERRORED) {
    // Don't transition to other states if we are already
    // drained or errored.
    mState = STARTED;
  }
}

Maybe<MozPromiseHolder<MediaSink::EndedPromise>> AudioStream::Shutdown(
    ShutdownCause aCause) {
  TRACE("AudioStream::Shutdown");
  LOG("Shutdown, state %d", mState.load());

  MonitorAutoLock mon(mMonitor);
  if (mCubebStream) {
    // Force stop to put the cubeb stream in a stable state before deletion.
    InvokeCubeb(cubeb_stream_stop);
    // Must not try to shut down cubeb from within the lock!  wasapi may still
    // call our callback after Pause()/stop()!?! Bug 996162
    cubeb_stream* cubeb = mCubebStream.release();
    MonitorAutoUnlock unlock(mMonitor);
    cubeb_stream_destroy(cubeb);
  }

  // After `cubeb_stream_stop` has been called, there is no audio thread
  // anymore. We can delete the time stretcher.
  if (mTimeStretcher) {
    soundtouch::destroySoundTouchObj(mTimeStretcher);
    mTimeStretcher = nullptr;
  }

  mState = SHUTDOWN;
  // When shutting down, if this AudioStream is shutting down because the
  // HTMLMediaElement is now muted, hand back the ended promise, so that it can
  // properly be resolved if the end of the media is reached while muted (i.e.
  // without having an AudioStream)
  if (aCause != ShutdownCause::Muting) {
    mEndedPromise.ResolveIfExists(true, __func__);
    return Nothing();
  }
  return Some(std::move(mEndedPromise));
}

int64_t AudioStream::GetPosition() {
  TRACE("AudioStream::GetPosition");
#ifndef XP_MACOSX
  MonitorAutoLock mon(mMonitor);
#endif
  int64_t frames = GetPositionInFramesUnlocked();
  return frames >= 0 ? mAudioClock.GetPosition(frames) : -1;
}

int64_t AudioStream::GetPositionInFrames() {
  TRACE("AudioStream::GetPositionInFrames");
#ifndef XP_MACOSX
  MonitorAutoLock mon(mMonitor);
#endif
  int64_t frames = GetPositionInFramesUnlocked();

  return frames >= 0 ? mAudioClock.GetPositionInFrames(frames) : -1;
}

int64_t AudioStream::GetPositionInFramesUnlocked() {
  TRACE("AudioStream::GetPositionInFramesUnlocked");
#ifndef XP_MACOSX
  mMonitor.AssertCurrentThreadOwns();
#endif

  if (mState == ERRORED) {
    return -1;
  }

  uint64_t position = 0;
  int rv;

#ifndef XP_MACOSX
  rv = InvokeCubeb(cubeb_stream_get_position, &position);
#else
  rv = cubeb_stream_get_position(mCubebStream.get(), &position);
#endif

  if (rv != CUBEB_OK) {
    return -1;
  }
  return static_cast<int64_t>(std::min<uint64_t>(position, INT64_MAX));
}

bool AudioStream::IsValidAudioFormat(Chunk* aChunk) {
  if (aChunk->Rate() != mAudioClock.GetInputRate()) {
    LOGW("mismatched sample %u, mInRate=%u", aChunk->Rate(),
         mAudioClock.GetInputRate());
    return false;
  }

  return aChunk->Channels() <= 8;
}

void AudioStream::GetUnprocessed(AudioBufferWriter& aWriter) {
  TRACE("AudioStream::GetUnprocessed");
  AssertIsOnAudioThread();
  // Flush the timestretcher pipeline, if we were playing using a playback rate
  // other than 1.0.
  if (mTimeStretcher && mTimeStretcher->numSamples()) {
    auto* timeStretcher = mTimeStretcher;
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
  } else if (mTimeStretcher) {
    // Don't need it anymore: playbackRate is 1.0, and the time stretcher has
    // been flushed.
    soundtouch::destroySoundTouchObj(mTimeStretcher);
    mTimeStretcher = nullptr;
  }

  while (aWriter.Available() > 0) {
    uint32_t count = mDataSource.PopFrames(aWriter.Ptr(), aWriter.Available(),
                                           mAudioThreadChanged);
    if (count == 0) {
      break;
    }
    aWriter.Advance(count);
  }
}

void AudioStream::GetTimeStretched(AudioBufferWriter& aWriter) {
  TRACE("AudioStream::GetTimeStretched");
  AssertIsOnAudioThread();
  if (EnsureTimeStretcherInitialized() != NS_OK) {
    return;
  }

  uint32_t toPopFrames =
      ceil(aWriter.Available() * mAudioClock.GetPlaybackRate());

  while (mTimeStretcher->numSamples() < aWriter.Available()) {
    // pop into a temp buffer, and put into the stretcher.
    AutoTArray<AudioDataValue, 1000> buf;
    auto size = CheckedUint32(mOutChannels) * toPopFrames;
    if (!size.isValid()) {
      // The overflow should not happen in normal case.
      LOGW("Invalid member data: %d channels, %d frames", mOutChannels,
           toPopFrames);
      return;
    }
    buf.SetLength(size.value());
    // ensure no variable channel count or something like that
    uint32_t count =
        mDataSource.PopFrames(buf.Elements(), toPopFrames, mAudioThreadChanged);
    if (count == 0) {
      break;
    }
    mTimeStretcher->putSamples(buf.Elements(), count);
  }

  auto* timeStretcher = mTimeStretcher;
  aWriter.Write(
      [timeStretcher](AudioDataValue* aPtr, uint32_t aFrames) {
        return timeStretcher->receiveSamples(aPtr, aFrames);
      },
      aWriter.Available());
}

bool AudioStream::CheckThreadIdChanged() {
  ProfilerThreadId id = profiler_current_thread_id();
  if (id != mAudioThreadId) {
    mAudioThreadId = id;
    mAudioThreadChanged = true;
    return true;
  }
  mAudioThreadChanged = false;
  return false;
}

void AudioStream::AssertIsOnAudioThread() const {
  // This can be called right after CheckThreadIdChanged, because the audio
  // thread can change when not sandboxed.
  MOZ_ASSERT(mAudioThreadId.load() == profiler_current_thread_id());
}

void AudioStream::UpdatePlaybackRateIfNeeded() {
  AssertIsOnAudioThread();
  if (mAudioClock.GetPreservesPitch() == mPreservesPitch &&
      mAudioClock.GetPlaybackRate() == mPlaybackRate) {
    return;
  }

  EnsureTimeStretcherInitialized();

  mAudioClock.SetPlaybackRate(mPlaybackRate);
  mAudioClock.SetPreservesPitch(mPreservesPitch);

  if (mPreservesPitch) {
    mTimeStretcher->setTempo(mPlaybackRate);
    mTimeStretcher->setRate(1.0f);
  } else {
    mTimeStretcher->setTempo(1.0f);
    mTimeStretcher->setRate(mPlaybackRate);
  }
}

long AudioStream::DataCallback(void* aBuffer, long aFrames) {
  if (CheckThreadIdChanged() && !mSandboxed) {
    CubebUtils::GetAudioThreadRegistry()->Register(mAudioThreadId);
  }
  WebCore::DenormalDisabler disabler;
  if (!mCallbacksStarted) {
    mCallbacksStarted = true;
  }

  TRACE_AUDIO_CALLBACK_BUDGET(aFrames, mAudioClock.GetInputRate());
  TRACE("AudioStream::DataCallback");
  MOZ_ASSERT(mState != SHUTDOWN, "No data callback after shutdown");

  if (SoftRealTimeLimitReached()) {
    DemoteThreadFromRealTime();
  }

  UpdatePlaybackRateIfNeeded();

  auto writer = AudioBufferWriter(
      Span<AudioDataValue>(reinterpret_cast<AudioDataValue*>(aBuffer),
                           mOutChannels * aFrames),
      mOutChannels, aFrames);

  if (mAudioClock.GetInputRate() == mAudioClock.GetOutputRate()) {
    GetUnprocessed(writer);
  } else {
    GetTimeStretched(writer);
  }

  // Always send audible frames first, and silent frames later.
  // Otherwise it will break the assumption of FrameHistory.
  if (!mDataSource.Ended()) {
#ifndef XP_MACOSX
    MonitorAutoLock mon(mMonitor);
#endif
    mAudioClock.UpdateFrameHistory(aFrames - writer.Available(),
                                   writer.Available(), mAudioThreadChanged);
    if (writer.Available() > 0) {
      TRACE_COMMENT("AudioStream::DataCallback", "Underrun: %d frames missing",
                    writer.Available());
      LOGW("lost %d frames", writer.Available());
      writer.WriteZeros(writer.Available());
    }
  } else {
    // No more new data in the data source, and the drain has completed. We
    // don't need the time stretcher anymore at this point.
    if (mTimeStretcher && writer.Available()) {
      soundtouch::destroySoundTouchObj(mTimeStretcher);
      mTimeStretcher = nullptr;
    }
#ifndef XP_MACOSX
    MonitorAutoLock mon(mMonitor);
#endif
    mAudioClock.UpdateFrameHistory(aFrames - writer.Available(), 0,
                                   mAudioThreadChanged);
  }

  mDumpFile.Write(static_cast<const AudioDataValue*>(aBuffer),
                  aFrames * mOutChannels);

  if (!mSandboxed && writer.Available() != 0) {
    CubebUtils::GetAudioThreadRegistry()->Unregister(mAudioThreadId);
  }
  return aFrames - writer.Available();
}

void AudioStream::StateCallback(cubeb_state aState) {
  MOZ_ASSERT(mState != SHUTDOWN, "No state callback after shutdown");
  LOG("StateCallback, mState=%d cubeb_state=%d", mState.load(), aState);

  MonitorAutoLock mon(mMonitor);
  if (aState == CUBEB_STATE_DRAINED) {
    LOG("Drained");
    mState = DRAINED;
    mPlaybackComplete = true;
    mEndedPromise.ResolveIfExists(true, __func__);
  } else if (aState == CUBEB_STATE_ERROR) {
    LOGE("StateCallback() state %d cubeb error", mState.load());
    mState = ERRORED;
    mPlaybackComplete = true;
    mEndedPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
  }
}

bool AudioStream::IsPlaybackCompleted() const { return mPlaybackComplete; }

AudioClock::AudioClock(uint32_t aInRate)
    : mOutRate(aInRate),
      mInRate(aInRate),
      mPreservesPitch(true),
      mFrameHistory(new FrameHistory()) {}

// Audio thread only
void AudioClock::UpdateFrameHistory(uint32_t aServiced, uint32_t aUnderrun,
                                    bool aAudioThreadChanged) {
#ifdef XP_MACOSX
  if (aAudioThreadChanged) {
    mCallbackInfoQueue.ResetThreadIds();
  }
  // Flush the local items, if any, and then attempt to enqueue the current
  // item. This is only a fallback mechanism, under non-critical load this is
  // just going to enqueue an item in the queue.
  while (!mAudioThreadCallbackInfo.IsEmpty()) {
    CallbackInfo& info = mAudioThreadCallbackInfo[0];
    // If still full, keep it audio-thread side for now.
    if (mCallbackInfoQueue.Enqueue(info) != 1) {
      break;
    }
    mAudioThreadCallbackInfo.RemoveElementAt(0);
  }
  CallbackInfo info(aServiced, aUnderrun, mOutRate);
  if (mCallbackInfoQueue.Enqueue(info) != 1) {
    NS_WARNING(
        "mCallbackInfoQueue full, storing the values in the audio thread.");
    mAudioThreadCallbackInfo.AppendElement(info);
  }
#else
  MutexAutoLock lock(mMutex);
  mFrameHistory->Append(aServiced, aUnderrun, mOutRate);
#endif
}

int64_t AudioClock::GetPositionInFrames(int64_t aFrames) {
  CheckedInt64 v = UsecsToFrames(GetPosition(aFrames), mInRate);
  return v.isValid() ? v.value() : -1;
}

int64_t AudioClock::GetPosition(int64_t frames) {
#ifdef XP_MACOSX
  // Dequeue all history info, and apply them before returning the position
  // based on frame history.
  CallbackInfo info;
  while (mCallbackInfoQueue.Dequeue(&info, 1)) {
    mFrameHistory->Append(info.mServiced, info.mUnderrun, info.mOutputRate);
  }
#else
  MutexAutoLock lock(mMutex);
#endif
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
