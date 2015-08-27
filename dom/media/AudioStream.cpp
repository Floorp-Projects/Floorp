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
#include "mozilla/Snprintf.h"
#include <algorithm>
#include "mozilla/Telemetry.h"
#include "CubebUtils.h"
#include "nsPrintfCString.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

PRLogModuleInfo* gAudioStreamLog = nullptr;
// For simple logs
#define LOG(x) MOZ_LOG(gAudioStreamLog, mozilla::LogLevel::Debug, x)

/**
 * When MOZ_DUMP_AUDIO is set in the environment (to anything),
 * we'll drop a series of files in the current working directory named
 * dumped-audio-<nnn>.wav, one per AudioStream created, containing
 * the audio for the stream including any skips due to underruns.
 */
static int gDumpedAudioCount = 0;

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
    int rate;
  };

  template <typename T>
  static T FramesToUs(uint32_t frames, int rate) {
    return static_cast<T>(frames) * USECS_PER_S / rate;
  }
public:
  FrameHistory()
    : mBaseOffset(0), mBasePosition(0) {}

  void Append(uint32_t aServiced, uint32_t aUnderrun, int aRate) {
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
  nsAutoTArray<Chunk, 7> mChunks;
  int64_t mBaseOffset;
  double mBasePosition;
};

AudioStream::AudioStream()
  : mMonitor("AudioStream")
  , mInRate(0)
  , mOutRate(0)
  , mChannels(0)
  , mOutChannels(0)
  , mWritten(0)
  , mAudioClock(this)
  , mTimeStretcher(nullptr)
  , mDumpFile(nullptr)
  , mBytesPerFrame(0)
  , mState(INITIALIZED)
  , mLastGoodPosition(0)
{
}

AudioStream::~AudioStream()
{
  LOG(("AudioStream: delete %p, state %d", this, mState));
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

  amount += mBuffer.SizeOfExcludingThis(aMallocSizeOf);

  return amount;
}

nsresult AudioStream::EnsureTimeStretcherInitializedUnlocked()
{
  mMonitor.AssertCurrentThreadOwns();
  if (!mTimeStretcher) {
    mTimeStretcher = soundtouch::createSoundTouchObj();
    mTimeStretcher->setSampleRate(mInRate);
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

  mAudioClock.SetPlaybackRateUnlocked(aPlaybackRate);
  mOutRate = mInRate / aPlaybackRate;

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

int64_t AudioStream::GetWritten()
{
  MonitorAutoLock mon(mMonitor);
  return mWritten;
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
OpenDumpFile(AudioStream* aStream)
{
  if (!getenv("MOZ_DUMP_AUDIO"))
    return nullptr;
  char buf[100];
  snprintf_literal(buf, "dumped-audio-%d.wav", gDumpedAudioCount);
  FILE* f = fopen(buf, "wb");
  if (!f)
    return nullptr;
  ++gDumpedAudioCount;

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
  SetUint16LE(header + CHANNEL_OFFSET, aStream->GetChannels());
  SetUint32LE(header + SAMPLE_RATE_OFFSET, aStream->GetRate());
  SetUint16LE(header + BLOCK_ALIGN_OFFSET, aStream->GetChannels()*2);
  fwrite(header, sizeof(header), 1, f);

  return f;
}

static void
WriteDumpFile(FILE* aDumpFile, AudioStream* aStream, uint32_t aFrames,
              void* aBuffer)
{
  if (!aDumpFile)
    return;

  uint32_t samples = aStream->GetOutChannels()*aFrames;
  if (AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_S16) {
    fwrite(aBuffer, 2, samples, aDumpFile);
    return;
  }

  NS_ASSERTION(AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_FLOAT32, "bad format");
  nsAutoTArray<uint8_t, 1024*2> buf;
  buf.SetLength(samples*2);
  float* input = static_cast<float*>(aBuffer);
  uint8_t* output = buf.Elements();
  for (uint32_t i = 0; i < samples; ++i) {
    SetUint16LE(output + i*2, int16_t(input[i]*32767.0f));
  }
  fwrite(output, 2, samples, aDumpFile);
  fflush(aDumpFile);
}

nsresult
AudioStream::Init(int32_t aNumChannels, int32_t aRate,
                  const dom::AudioChannel aAudioChannel)
{
  mStartTime = TimeStamp::Now();
  mIsFirst = CubebUtils::GetFirstStream();

  if (!CubebUtils::GetCubebContext() || aNumChannels < 0 || aRate < 0) {
    return NS_ERROR_FAILURE;
  }

  MOZ_LOG(gAudioStreamLog, LogLevel::Debug,
    ("%s  channels: %d, rate: %d for %p", __FUNCTION__, aNumChannels, aRate, this));
  mInRate = mOutRate = aRate;
  mChannels = aNumChannels;
  mOutChannels = (aNumChannels > 2) ? 2 : aNumChannels;

  mDumpFile = OpenDumpFile(this);

  cubeb_stream_params params;
  params.rate = aRate;
  params.channels = mOutChannels;
#if defined(__ANDROID__)
#if defined(MOZ_B2G)
  mAudioChannel = aAudioChannel;
  params.stream_type = CubebUtils::ConvertChannelToCubebType(aAudioChannel);
#else
  mAudioChannel = dom::AudioChannel::Content;
  params.stream_type = CUBEB_STREAM_TYPE_MUSIC;
#endif

  if (params.stream_type == CUBEB_STREAM_TYPE_MAX) {
    return NS_ERROR_INVALID_ARG;
  }
#endif
  if (AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_S16) {
    params.format = CUBEB_SAMPLE_S16NE;
  } else {
    params.format = CUBEB_SAMPLE_FLOAT32NE;
  }
  mBytesPerFrame = sizeof(AudioDataValue) * mOutChannels;

  mAudioClock.Init();

  // Size mBuffer for one second of audio.  This value is arbitrary, and was
  // selected based on the observed behaviour of the existing AudioStream
  // implementations.
  uint32_t bufferLimit = FramesToBytes(aRate);
  MOZ_ASSERT(bufferLimit % mBytesPerFrame == 0, "Must buffer complete frames");
  mBuffer.SetCapacity(bufferLimit);

  return OpenCubeb(params);
}

// This code used to live inside AudioStream::Init(), but on Mac (others?)
// it has been known to take 300-800 (or even 8500) ms to execute(!)
nsresult
AudioStream::OpenCubeb(cubeb_stream_params &aParams)
{
  cubeb* cubebContext = CubebUtils::GetCubebContext();
  if (!cubebContext) {
    NS_WARNING("Can't get cubeb context!");
    MonitorAutoLock mon(mMonitor);
    mState = AudioStream::ERRORED;
    return NS_ERROR_FAILURE;
  }

  // If the latency pref is set, use it. Otherwise, if this stream is intended
  // for low latency playback, try to get the lowest latency possible.
  // Otherwise, for normal streams, use 100ms.
  uint32_t latency = CubebUtils::GetCubebLatency();

  {
    cubeb_stream* stream;
    if (cubeb_stream_init(cubebContext, &stream, "AudioStream", aParams,
                          latency, DataCallback_S, StateCallback_S, this) == CUBEB_OK) {
      MonitorAutoLock mon(mMonitor);
      MOZ_ASSERT(mState != SHUTDOWN);
      mCubebStream.reset(stream);
    } else {
      MonitorAutoLock mon(mMonitor);
      mState = ERRORED;
      NS_WARNING(nsPrintfCString("AudioStream::OpenCubeb() %p failed to init cubeb", this).get());
      return NS_ERROR_FAILURE;
    }
  }

  mState = INITIALIZED;

  if (!mStartTime.IsNull()) {
    TimeDuration timeDelta = TimeStamp::Now() - mStartTime;
    LOG(("AudioStream creation time %sfirst: %u ms", mIsFirst ? "" : "not ",
          (uint32_t) timeDelta.ToMilliseconds()));
    Telemetry::Accumulate(mIsFirst ? Telemetry::AUDIOSTREAM_FIRST_OPEN_MS :
        Telemetry::AUDIOSTREAM_LATER_OPEN_MS, timeDelta.ToMilliseconds());
  }

  return NS_OK;
}

// aTime is the time in ms the samples were inserted into MediaStreamGraph
nsresult
AudioStream::Write(const AudioDataValue* aBuf, uint32_t aFrames)
{
  MonitorAutoLock mon(mMonitor);

  if (mState == ERRORED) {
    return NS_ERROR_FAILURE;
  }
  NS_ASSERTION(mState == INITIALIZED || mState == STARTED || mState == RUNNING,
    "Stream write in unexpected state.");

  // Downmix to Stereo.
  if (mChannels > 2 && mChannels <= 8) {
    DownmixAudioToStereo(const_cast<AudioDataValue*> (aBuf), mChannels, aFrames);
  }
  else if (mChannels > 8) {
    return NS_ERROR_FAILURE;
  }

  const uint8_t* src = reinterpret_cast<const uint8_t*>(aBuf);
  uint32_t bytesToCopy = FramesToBytes(aFrames);

  while (bytesToCopy > 0) {
    uint32_t available = std::min(bytesToCopy, mBuffer.Available());
    MOZ_ASSERT(available % mBytesPerFrame == 0,
               "Must copy complete frames.");

    mBuffer.AppendElements(src, available);
    src += available;
    bytesToCopy -= available;

    if (bytesToCopy > 0) {
     // If we are not playing, but our buffer is full, start playing to make
     // room for soon-to-be-decoded data.
     if (mState != STARTED && mState != RUNNING) {
       MOZ_LOG(gAudioStreamLog, LogLevel::Warning, ("Starting stream %p in Write (%u waiting)",
                                              this, bytesToCopy));
       StartUnlocked();
       if (mState == ERRORED) {
         return NS_ERROR_FAILURE;
       }
     }
     MOZ_LOG(gAudioStreamLog, LogLevel::Warning, ("Stream %p waiting in Write() (%u waiting)",
                                              this, bytesToCopy));
     mon.Wait();
    }
  }

  mWritten += aFrames;
  return NS_OK;
}

uint32_t
AudioStream::Available()
{
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mBuffer.Length() % mBytesPerFrame == 0, "Buffer invariant violated.");
  return BytesToFrames(mBuffer.Available());
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
AudioStream::Cancel()
{
  MonitorAutoLock mon(mMonitor);
  mState = ERRORED;
  mon.NotifyAll();
}

void
AudioStream::Drain()
{
  MonitorAutoLock mon(mMonitor);
  LOG(("AudioStream::Drain() for %p, state %d, avail %u", this, mState, mBuffer.Available()));
  if (mState != STARTED && mState != RUNNING) {
    NS_ASSERTION(mState == ERRORED || mBuffer.Available() == 0, "Draining without full buffer of unplayed audio");
    return;
  }
  mState = DRAINING;
  while (mState == DRAINING) {
    mon.Wait();
  }
}

void
AudioStream::Start()
{
  MonitorAutoLock mon(mMonitor);
  StartUnlocked();
}

void
AudioStream::StartUnlocked()
{
  mMonitor.AssertCurrentThreadOwns();
  if (!mCubebStream) {
    return;
  }

  if (mState == INITIALIZED) {
    int r;
    {
      MonitorAutoUnlock mon(mMonitor);
      r = cubeb_stream_start(mCubebStream.get());
    }
    mState = r == CUBEB_OK ? STARTED : ERRORED;
    LOG(("AudioStream: started %p, state %s", this, mState == STARTED ? "STARTED" : "ERRORED"));
  }
}

void
AudioStream::Pause()
{
  MonitorAutoLock mon(mMonitor);

  if (mState == ERRORED) {
    return;
  }

  if (!mCubebStream || (mState != STARTED && mState != RUNNING)) {
    mState = STOPPED; // which also tells async OpenCubeb not to start, just init
    return;
  }

  int r;
  {
    MonitorAutoUnlock mon(mMonitor);
    r = cubeb_stream_stop(mCubebStream.get());
  }
  if (mState != ERRORED && r == CUBEB_OK) {
    mState = STOPPED;
  }
}

void
AudioStream::Resume()
{
  MonitorAutoLock mon(mMonitor);
  if (!mCubebStream || mState != STOPPED) {
    return;
  }

  int r;
  {
    MonitorAutoUnlock mon(mMonitor);
    r = cubeb_stream_start(mCubebStream.get());
  }
  if (mState != ERRORED && r == CUBEB_OK) {
    mState = STARTED;
  }
}

void
AudioStream::Shutdown()
{
  MonitorAutoLock mon(mMonitor);
  LOG(("AudioStream: Shutdown %p, state %d", this, mState));

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

int64_t
AudioStream::GetPosition()
{
  MonitorAutoLock mon(mMonitor);
  return mAudioClock.GetPositionUnlocked();
}

int64_t
AudioStream::GetPositionInFrames()
{
  MonitorAutoLock mon(mMonitor);
  return mAudioClock.GetPositionInFrames();
}

int64_t
AudioStream::GetPositionInFramesUnlocked()
{
  mMonitor.AssertCurrentThreadOwns();

  if (!mCubebStream || mState == ERRORED) {
    return -1;
  }

  uint64_t position = 0;
  {
    MonitorAutoUnlock mon(mMonitor);
    if (cubeb_stream_get_position(mCubebStream.get(), &position) != CUBEB_OK) {
      return -1;
    }
  }

  MOZ_ASSERT(position >= mLastGoodPosition, "cubeb position shouldn't go backward");
  // This error handling/recovery keeps us in good shape in release build.
  if (position >= mLastGoodPosition) {
    mLastGoodPosition = position;
  }
  return std::min<uint64_t>(mLastGoodPosition, INT64_MAX);
}

bool
AudioStream::IsPaused()
{
  MonitorAutoLock mon(mMonitor);
  return mState == STOPPED;
}

long
AudioStream::GetUnprocessed(void* aBuffer, long aFrames)
{
  mMonitor.AssertCurrentThreadOwns();
  uint8_t* wpos = reinterpret_cast<uint8_t*>(aBuffer);

  // Flush the timestretcher pipeline, if we were playing using a playback rate
  // other than 1.0.
  uint32_t flushedFrames = 0;
  if (mTimeStretcher && mTimeStretcher->numSamples()) {
    flushedFrames = mTimeStretcher->receiveSamples(reinterpret_cast<AudioDataValue*>(wpos), aFrames);
    wpos += FramesToBytes(flushedFrames);
  }
  uint32_t toPopBytes = FramesToBytes(aFrames - flushedFrames);
  uint32_t available = std::min(toPopBytes, mBuffer.Length());

  void* input[2];
  uint32_t input_size[2];
  mBuffer.PopElements(available, &input[0], &input_size[0], &input[1], &input_size[1]);
  memcpy(wpos, input[0], input_size[0]);
  wpos += input_size[0];
  memcpy(wpos, input[1], input_size[1]);

  return BytesToFrames(available) + flushedFrames;
}

long
AudioStream::GetTimeStretched(void* aBuffer, long aFrames)
{
  mMonitor.AssertCurrentThreadOwns();
  long processedFrames = 0;

  // We need to call the non-locking version, because we already have the lock.
  if (EnsureTimeStretcherInitializedUnlocked() != NS_OK) {
    return 0;
  }

  uint8_t* wpos = reinterpret_cast<uint8_t*>(aBuffer);
  double playbackRate = static_cast<double>(mInRate) / mOutRate;
  uint32_t toPopBytes = FramesToBytes(ceil(aFrames * playbackRate));
  uint32_t available = 0;
  bool lowOnBufferedData = false;
  do {
    // Check if we already have enough data in the time stretcher pipeline.
    if (mTimeStretcher->numSamples() <= static_cast<uint32_t>(aFrames)) {
      void* input[2];
      uint32_t input_size[2];
      available = std::min(mBuffer.Length(), toPopBytes);
      if (available != toPopBytes) {
        lowOnBufferedData = true;
      }
      mBuffer.PopElements(available, &input[0], &input_size[0],
                                     &input[1], &input_size[1]);
      for(uint32_t i = 0; i < 2; i++) {
        mTimeStretcher->putSamples(reinterpret_cast<AudioDataValue*>(input[i]), BytesToFrames(input_size[i]));
      }
    }
    uint32_t receivedFrames = mTimeStretcher->receiveSamples(reinterpret_cast<AudioDataValue*>(wpos), aFrames - processedFrames);
    wpos += FramesToBytes(receivedFrames);
    processedFrames += receivedFrames;
  } while (processedFrames < aFrames && !lowOnBufferedData);

  return processedFrames;
}

long
AudioStream::DataCallback(void* aBuffer, long aFrames)
{
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mState != SHUTDOWN, "No data callback after shutdown");
  uint32_t available = std::min(static_cast<uint32_t>(FramesToBytes(aFrames)), mBuffer.Length());
  MOZ_ASSERT(available % mBytesPerFrame == 0, "Must copy complete frames");
  AudioDataValue* output = reinterpret_cast<AudioDataValue*>(aBuffer);
  uint32_t underrunFrames = 0;
  uint32_t servicedFrames = 0;

  // NOTE: wasapi (others?) can call us back *after* stop()/Shutdown() (mState == SHUTDOWN)
  // Bug 996162

  // callback tells us cubeb succeeded initializing
  if (mState == STARTED) {
    mState = RUNNING;
  }

  if (available) {
    if (mInRate == mOutRate) {
      servicedFrames = GetUnprocessed(output, aFrames);
    } else {
      servicedFrames = GetTimeStretched(output, aFrames);
    }

    MOZ_ASSERT(mBuffer.Length() % mBytesPerFrame == 0, "Must copy complete frames");

    // Notify any blocked Write() call that more space is available in mBuffer.
    mon.NotifyAll();
  }

  underrunFrames = aFrames - servicedFrames;

  // Always send audible frames first, and silent frames later.
  // Otherwise it will break the assumption of FrameHistory.
  if (mState != DRAINING) {
    mAudioClock.UpdateFrameHistory(servicedFrames, underrunFrames);
    uint8_t* rpos = static_cast<uint8_t*>(aBuffer) + FramesToBytes(aFrames - underrunFrames);
    memset(rpos, 0, FramesToBytes(underrunFrames));
    if (underrunFrames) {
      MOZ_LOG(gAudioStreamLog, LogLevel::Warning,
             ("AudioStream %p lost %d frames", this, underrunFrames));
    }
    servicedFrames += underrunFrames;
  } else {
    mAudioClock.UpdateFrameHistory(servicedFrames, 0);
  }

  WriteDumpFile(mDumpFile, this, aFrames, aBuffer);

  return servicedFrames;
}

void
AudioStream::StateCallback(cubeb_state aState)
{
  MonitorAutoLock mon(mMonitor);
  MOZ_ASSERT(mState != SHUTDOWN, "No state callback after shutdown");
  LOG(("AudioStream: StateCallback %p, mState=%d cubeb_state=%d", this, mState, aState));
  if (aState == CUBEB_STATE_DRAINED) {
    mState = DRAINED;
  } else if (aState == CUBEB_STATE_ERROR) {
    LOG(("AudioStream::StateCallback() state %d cubeb error", mState));
    mState = ERRORED;
  }
  mon.NotifyAll();
}

AudioClock::AudioClock(AudioStream* aStream)
 :mAudioStream(aStream),
  mOutRate(0),
  mInRate(0),
  mPreservesPitch(true),
  mFrameHistory(new FrameHistory())
{}

void AudioClock::Init()
{
  mOutRate = mAudioStream->GetRate();
  mInRate = mAudioStream->GetRate();
}

void AudioClock::UpdateFrameHistory(uint32_t aServiced, uint32_t aUnderrun)
{
  mFrameHistory->Append(aServiced, aUnderrun, mOutRate);
}

int64_t AudioClock::GetPositionUnlocked() const
{
  // GetPositionInFramesUnlocked() asserts it owns the monitor
  int64_t frames = mAudioStream->GetPositionInFramesUnlocked();
  NS_ASSERTION(frames < 0 || (mInRate != 0 && mOutRate != 0), "AudioClock not initialized.");
  return frames >= 0 ? mFrameHistory->GetPosition(frames) : -1;
}

int64_t AudioClock::GetPositionInFrames() const
{
  return (GetPositionUnlocked() * mInRate) / USECS_PER_S;
}

void AudioClock::SetPlaybackRateUnlocked(double aPlaybackRate)
{
  mOutRate = static_cast<int>(mInRate / aPlaybackRate);
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
