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
#include <algorithm>
#include "mozilla/Telemetry.h"
#include "CubebUtils.h"
#include "nsPrintfCString.h"
#include "gfxPrefs.h"
#include "AudioConverter.h"

namespace mozilla {

#undef LOG
#undef LOGW

LazyLogModule gAudioStreamLog("AudioStream");
// For simple logs
#define LOG(x, ...) MOZ_LOG(gAudioStreamLog, mozilla::LogLevel::Debug, ("%p " x, this, ##__VA_ARGS__))
#define LOGW(x, ...) MOZ_LOG(gAudioStreamLog, mozilla::LogLevel::Warning, ("%p " x, this, ##__VA_ARGS__))

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
  FrameHistory()
    : mBaseOffset(0), mBasePosition(0) {}

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
          (c.servicedFrames == c.totalFrames ||
           aServiced == 0)) {
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
      // we are able to compress chunks so that |mChunks| won't grow unlimitedly.
      // Note that we lose precision in converting integers into floats and
      // inaccuracy will accumulate over time. However, for a 24hr long,
      // sample rate = 44.1k file, the error will be less than 1 microsecond
      // after playing 24 hours. So we are fine with that.
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
  : mMonitor("AudioStream")
  , mChannels(0)
  , mOutChannels(0)
  , mTimeStretcher(nullptr)
  , mDumpFile(nullptr)
  , mState(INITIALIZED)
  , mDataSource(aSource)
{
}

AudioStream::~AudioStream()
{
  LOG("deleted, state %d", mState);
  MOZ_ASSERT(mState == SHUTDOWN && !mCubebStream,
             "Should've called Shutdown() before deleting an AudioStream");
  if (mDumpFile) {
    fclose(mDumpFile);
  }
  if (mTimeStretcher) {
    soundtouch::destroySoundTouchObj(mTimeStretcher);
  }
}

size_t
AudioStream::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = aMallocSizeOf(this);

  // Possibly add in the future:
  // - mTimeStretcher
  // - mCubebStream

  return amount;
}

nsresult AudioStream::EnsureTimeStretcherInitializedUnlocked()
{
  mMonitor.AssertCurrentThreadOwns();
  if (!mTimeStretcher) {
    mTimeStretcher = soundtouch::createSoundTouchObj();
    mTimeStretcher->setSampleRate(mAudioClock.GetInputRate());
    mTimeStretcher->setChannels(mOutChannels);
    mTimeStretcher->setPitch(1.0);
  }
  return NS_OK;
}

nsresult AudioStream::SetPlaybackRate(double aPlaybackRate)
{
  // MUST lock since the rate transposer is used from the cubeb callback,
  // and rate changes can cause the buffer to be reallocated
  MonitorAutoLock mon(mMonitor);

  NS_ASSERTION(aPlaybackRate > 0.0,
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

nsresult AudioStream::SetPreservesPitch(bool aPreservesPitch)
{
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

static void SetUint16LE(uint8_t* aDest, uint16_t aValue)
{
  aDest[0] = aValue & 0xFF;
  aDest[1] = aValue >> 8;
}

static void SetUint32LE(uint8_t* aDest, uint32_t aValue)
{
  SetUint16LE(aDest, aValue & 0xFFFF);
  SetUint16LE(aDest + 2, aValue >> 16);
}

static FILE*
OpenDumpFile(uint32_t aChannels, uint32_t aRate)
{
  /**
   * When MOZ_DUMP_AUDIO is set in the environment (to anything),
   * we'll drop a series of files in the current working directory named
   * dumped-audio-<nnn>.wav, one per AudioStream created, containing
   * the audio for the stream including any skips due to underruns.
   */
  static Atomic<int> gDumpedAudioCount(0);

  if (!getenv("MOZ_DUMP_AUDIO"))
    return nullptr;
  char buf[100];
  SprintfLiteral(buf, "dumped-audio-%d.wav", ++gDumpedAudioCount);
  FILE* f = fopen(buf, "wb");
  if (!f)
    return nullptr;

  uint8_t header[] = {
    // RIFF header
    0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45,
    // fmt chunk. We always write 16-bit samples.
    0x66, 0x6d, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x10, 0x00,
    // data chunk
    0x64, 0x61, 0x74, 0x61, 0xFE, 0xFF, 0xFF, 0x7F
  };
  static const int CHANNEL_OFFSET = 22;
  static const int SAMPLE_RATE_OFFSET = 24;
  static const int BLOCK_ALIGN_OFFSET = 32;
  SetUint16LE(header + CHANNEL_OFFSET, aChannels);
  SetUint32LE(header + SAMPLE_RATE_OFFSET, aRate);
  SetUint16LE(header + BLOCK_ALIGN_OFFSET, aChannels * 2);
  fwrite(header, sizeof(header), 1, f);

  return f;
}

template <typename T>
typename EnableIf<IsSame<T, int16_t>::value, void>::Type
WriteDumpFileHelper(T* aInput, size_t aSamples, FILE* aFile) {
  fwrite(aInput, sizeof(T), aSamples, aFile);
}

template <typename T>
typename EnableIf<IsSame<T, float>::value, void>::Type
WriteDumpFileHelper(T* aInput, size_t aSamples, FILE* aFile) {
  AutoTArray<uint8_t, 1024*2> buf;
  buf.SetLength(aSamples*2);
  uint8_t* output = buf.Elements();
  for (uint32_t i = 0; i < aSamples; ++i) {
    SetUint16LE(output + i*2, int16_t(aInput[i]*32767.0f));
  }
  fwrite(output, 2, aSamples, aFile);
  fflush(aFile);
}

static void
WriteDumpFile(FILE* aDumpFile, AudioStream* aStream, uint32_t aFrames,
              void* aBuffer)
{
  if (!aDumpFile)
    return;

  uint32_t samples = aStream->GetOutChannels()*aFrames;

  using SampleT = AudioSampleTraits<AUDIO_OUTPUT_FORMAT>::Type;
  WriteDumpFileHelper(reinterpret_cast<SampleT*>(aBuffer), samples, aDumpFile);
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
int AudioStream::InvokeCubeb(Function aFunction, Args&&... aArgs)
{
  MonitorAutoUnlock mon(mMonitor);
  return aFunction(mCubebStream.get(), Forward<Args>(aArgs)...);
}

nsresult
AudioStream::Init(uint32_t aNumChannels, uint32_t aChannelMap, uint32_t aRate,
                  const dom::AudioChannel aAudioChannel)
{
  auto startTime = TimeStamp::Now();

  LOG("%s channels: %d, rate: %d", __FUNCTION__, aNumChannels, aRate);
  mChannels = aNumChannels;
  mOutChannels = aNumChannels;

  mDumpFile = OpenDumpFile(aNumChannels, aRate);

  cubeb_stream_params params;
  params.rate = aRate;
  params.channels = mOutChannels;
  params.layout = CubebUtils::ConvertChannelMapToCubebLayout(aChannelMap);
#if defined(__ANDROID__)
#if defined(MOZ_B2G)
  params.stream_type = CubebUtils::ConvertChannelToCubebType(aAudioChannel);
#else
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  if (params.stream_type == CUBEB_STREAM_TYPE_MAX) {
    return NS_ERROR_INVALID_ARG;
  }
#endif

  params.format = ToCubebFormat<AUDIO_OUTPUT_FORMAT>::value;
  mAudioClock.Init(aRate);

  cubeb* cubebContext = CubebUtils::GetCubebContext();
  if (!cubebContext) {
    NS_WARNING("Can't get cubeb context!");
    CubebUtils::ReportCubebStreamInitFailure(true);
    return NS_ERROR_DOM_MEDIA_CUBEB_INITIALIZATION_ERR;
  }

  return OpenCubeb(cubebContext, params, startTime, CubebUtils::GetFirstStream());
}

nsresult
AudioStream::OpenCubeb(cubeb* aContext, cubeb_stream_params& aParams,
                       TimeStamp aStartTime, bool aIsFirst)
{
  MOZ_ASSERT(aContext);

  cubeb_stream* stream = nullptr;
  /* Convert from milliseconds to frames. */
  uint32_t latency_frames =
    CubebUtils::GetCubebPlaybackLatencyInMilliseconds() * aParams.rate / 1000;
  if (cubeb_stream_init(aContext, &stream, "AudioStream",
                        nullptr, nullptr, nullptr, &aParams,
                        latency_frames,
                        DataCallback_S, StateCallback_S, this) == CUBEB_OK) {
    mCubebStream.reset(stream);
    CubebUtils::ReportCubebBackendUsed();
  } else {
    NS_WARNING(nsPrintfCString("AudioStream::OpenCubeb() %p failed to init cubeb", this).get());
    CubebUtils::ReportCubebStreamInitFailure(aIsFirst);
    return NS_ERROR_FAILURE;
  }

  TimeDuration timeDelta = TimeStamp::Now() - aStartTime;
  LOG("creation time %sfirst: %u ms", aIsFirst ? "" : "not ",
      (uint32_t) timeDelta.ToMilliseconds());
  Telemetry::Accumulate(aIsFirst ? Telemetry::AUDIOSTREAM_FIRST_OPEN_MS :
      Telemetry::AUDIOSTREAM_LATER_OPEN_MS, timeDelta.ToMilliseconds());

  return NS_OK;
}

void
AudioStream::SetVolume(double aVolume)
{
  MOZ_ASSERT(aVolume >= 0.0 && aVolume <= 1.0, "Invalid volume");

  if (cubeb_stream_set_volume(mCubebStream.get(), aVolume * CubebUtils::GetVolumeScale()) != CUBEB_OK) {
    NS_WARNING("Could not change volume on cubeb stream.");
  }
}

void
AudioStream::Start()
{
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mState == INITIALIZED);
  mState = STARTED;
  auto r = InvokeCubeb(cubeb_stream_start);
  if (r != CUBEB_OK) {
    mState = ERRORED;
  }
  LOG("started, state %s", mState == STARTED ? "STARTED" : mState == DRAINED ? "DRAINED" : "ERRORED");
}

void
AudioStream::Pause()
{
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

void
AudioStream::Resume()
{
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

void
AudioStream::Shutdown()
{
  MonitorAutoLock mon(mMonitor);
  LOG("Shutdown, state %d", mState);

  if (mCubebStream) {
    MonitorAutoUnlock mon(mMonitor);
    // Force stop to put the cubeb stream in a stable state before deletion.
    cubeb_stream_stop(mCubebStream.get());
    // Must not try to shut down cubeb from within the lock!  wasapi may still
    // call our callback after Pause()/stop()!?! Bug 996162
    mCubebStream.reset();
  }

  mState = SHUTDOWN;
}

void
AudioStream::ResetDefaultDevice()
{
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

int64_t
AudioStream::GetPosition()
{
  MonitorAutoLock mon(mMonitor);
  int64_t frames = GetPositionInFramesUnlocked();
  return frames >= 0 ? mAudioClock.GetPosition(frames) : -1;
}

int64_t
AudioStream::GetPositionInFrames()
{
  MonitorAutoLock mon(mMonitor);
  int64_t frames = GetPositionInFramesUnlocked();
  return frames >= 0 ? mAudioClock.GetPositionInFrames(frames) : -1;
}

int64_t
AudioStream::GetPositionInFramesUnlocked()
{
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

bool
AudioStream::IsValidAudioFormat(Chunk* aChunk)
{
  if (aChunk->Rate() != mAudioClock.GetInputRate()) {
    LOGW("mismatched sample %u, mInRate=%u", aChunk->Rate(), mAudioClock.GetInputRate());
    return false;
  }

  if (aChunk->Channels() > 8) {
    return false;
  }

  return true;
}

void
AudioStream::GetUnprocessed(AudioBufferWriter& aWriter)
{
  mMonitor.AssertCurrentThreadOwns();

  // Flush the timestretcher pipeline, if we were playing using a playback rate
  // other than 1.0.
  if (mTimeStretcher && mTimeStretcher->numSamples()) {
    auto timeStretcher = mTimeStretcher;
    aWriter.Write([timeStretcher] (AudioDataValue* aPtr, uint32_t aFrames) {
      return timeStretcher->receiveSamples(aPtr, aFrames);
    }, aWriter.Available());

    // TODO: There might be still unprocessed samples in the stretcher.
    // We should either remove or flush them so they won't be in the output
    // next time we switch a playback rate other than 1.0.
    NS_WARNING_ASSERTION(
      mTimeStretcher->numUnprocessedSamples() == 0, "no samples");
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

void
AudioStream::GetTimeStretched(AudioBufferWriter& aWriter)
{
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
        LOGW("Invalid member data: %d channels, %d frames", mOutChannels, c->Frames());
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
  aWriter.Write([timeStretcher] (AudioDataValue* aPtr, uint32_t aFrames) {
    return timeStretcher->receiveSamples(aPtr, aFrames);
  }, aWriter.Available());
}

long
AudioStream::DataCallback(void* aBuffer, long aFrames)
{
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mState != SHUTDOWN, "No data callback after shutdown");

  auto writer = AudioBufferWriter(
    reinterpret_cast<AudioDataValue*>(aBuffer), mOutChannels, aFrames);

  if (!strcmp(cubeb_get_backend_id(CubebUtils::GetCubebContext()), "winmm")) {
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

  // NOTE: wasapi (others?) can call us back *after* stop()/Shutdown() (mState == SHUTDOWN)
  // Bug 996162

  if (mAudioClock.GetInputRate() == mAudioClock.GetOutputRate()) {
    GetUnprocessed(writer);
  } else {
    GetTimeStretched(writer);
  }

  // Always send audible frames first, and silent frames later.
  // Otherwise it will break the assumption of FrameHistory.
  if (!mDataSource.Ended()) {
    mAudioClock.UpdateFrameHistory(aFrames - writer.Available(), writer.Available());
    if (writer.Available() > 0) {
      LOGW("lost %d frames", writer.Available());
      writer.WriteZeros(writer.Available());
    }
  } else {
    // No more new data in the data source. Don't send silent frames so the
    // cubeb stream can start draining.
    mAudioClock.UpdateFrameHistory(aFrames - writer.Available(), 0);
  }

  WriteDumpFile(mDumpFile, this, aFrames, aBuffer);

  return aFrames - writer.Available();
}

void
AudioStream::StateCallback(cubeb_state aState)
{
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mState != SHUTDOWN, "No state callback after shutdown");
  LOG("StateCallback, mState=%d cubeb_state=%d", mState, aState);
  if (aState == CUBEB_STATE_DRAINED) {
    mState = DRAINED;
    mDataSource.Drained();
  } else if (aState == CUBEB_STATE_ERROR) {
    LOG("StateCallback() state %d cubeb error", mState);
    mState = ERRORED;
  }
}

AudioClock::AudioClock()
: mOutRate(0),
  mInRate(0),
  mPreservesPitch(true),
  mFrameHistory(new FrameHistory())
{}

void AudioClock::Init(uint32_t aRate)
{
  mOutRate = aRate;
  mInRate = aRate;
}

void AudioClock::UpdateFrameHistory(uint32_t aServiced, uint32_t aUnderrun)
{
  mFrameHistory->Append(aServiced, aUnderrun, mOutRate);
}

int64_t AudioClock::GetPositionInFrames(int64_t aFrames) const
{
  CheckedInt64 v = UsecsToFrames(GetPosition(aFrames), mInRate);
  return v.isValid() ? v.value() : -1;
}

int64_t AudioClock::GetPosition(int64_t frames) const
{
  return mFrameHistory->GetPosition(frames);
}

void AudioClock::SetPlaybackRate(double aPlaybackRate)
{
  mOutRate = static_cast<uint32_t>(mInRate / aPlaybackRate);
}

double AudioClock::GetPlaybackRate() const
{
  return static_cast<double>(mInRate) / mOutRate;
}

void AudioClock::SetPreservesPitch(bool aPreservesPitch)
{
  mPreservesPitch = aPreservesPitch;
}

bool AudioClock::GetPreservesPitch() const
{
  return mPreservesPitch;
}

} // namespace mozilla
