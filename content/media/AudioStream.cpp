/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>
#include <math.h>
#include "prlog.h"
#include "prdtoa.h"
#include "AudioStream.h"
#include "VideoUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include <algorithm>
#include "mozilla/Preferences.h"
#include "soundtouch/SoundTouch.h"
#include "Latency.h"

namespace mozilla {

#ifdef PR_LOGGING
PRLogModuleInfo* gAudioStreamLog = nullptr;
#endif

/**
 * When MOZ_DUMP_AUDIO is set in the environment (to anything),
 * we'll drop a series of files in the current working directory named
 * dumped-audio-<nnn>.wav, one per AudioStream created, containing
 * the audio for the stream including any skips due to underruns.
 */
static int gDumpedAudioCount = 0;

#define PREF_VOLUME_SCALE "media.volume_scale"
#define PREF_CUBEB_LATENCY "media.cubeb_latency_ms"

static const uint32_t CUBEB_NORMAL_LATENCY_MS = 100;

StaticMutex AudioStream::sMutex;
cubeb* AudioStream::sCubebContext;
uint32_t AudioStream::sPreferredSampleRate;
double AudioStream::sVolumeScale;
uint32_t AudioStream::sCubebLatency;
bool AudioStream::sCubebLatencyPrefSet;

/*static*/ int AudioStream::PrefChanged(const char* aPref, void* aClosure)
{
  if (strcmp(aPref, PREF_VOLUME_SCALE) == 0) {
    nsAdoptingString value = Preferences::GetString(aPref);
    StaticMutexAutoLock lock(sMutex);
    if (value.IsEmpty()) {
      sVolumeScale = 1.0;
    } else {
      NS_ConvertUTF16toUTF8 utf8(value);
      sVolumeScale = std::max<double>(0, PR_strtod(utf8.get(), nullptr));
    }
  } else if (strcmp(aPref, PREF_CUBEB_LATENCY) == 0) {
    // Arbitrary default stream latency of 100ms.  The higher this
    // value, the longer stream volume changes will take to become
    // audible.
    sCubebLatencyPrefSet = Preferences::HasUserValue(aPref);
    uint32_t value = Preferences::GetUint(aPref, CUBEB_NORMAL_LATENCY_MS);
    StaticMutexAutoLock lock(sMutex);
    sCubebLatency = std::min<uint32_t>(std::max<uint32_t>(value, 1), 1000);
  }
  return 0;
}

/*static*/ double AudioStream::GetVolumeScale()
{
  StaticMutexAutoLock lock(sMutex);
  return sVolumeScale;
}

/*static*/ cubeb* AudioStream::GetCubebContext()
{
  StaticMutexAutoLock lock(sMutex);
  return GetCubebContextUnlocked();
}

/*static*/ cubeb* AudioStream::GetCubebContextUnlocked()
{
  sMutex.AssertCurrentThreadOwns();
  if (sCubebContext ||
      cubeb_init(&sCubebContext, "AudioStream") == CUBEB_OK) {
    return sCubebContext;
  }
  NS_WARNING("cubeb_init failed");
  return nullptr;
}

/*static*/ uint32_t AudioStream::GetCubebLatency()
{
  StaticMutexAutoLock lock(sMutex);
  return sCubebLatency;
}

/*static*/ bool AudioStream::CubebLatencyPrefSet()
{
  StaticMutexAutoLock lock(sMutex);
  return sCubebLatencyPrefSet;
}

#if defined(__ANDROID__) && defined(MOZ_B2G)
static cubeb_stream_type ConvertChannelToCubebType(dom::AudioChannelType aType)
{
  switch(aType) {
    case dom::AUDIO_CHANNEL_NORMAL:
      return CUBEB_STREAM_TYPE_SYSTEM;
    case dom::AUDIO_CHANNEL_CONTENT:
      return CUBEB_STREAM_TYPE_MUSIC;
    case dom::AUDIO_CHANNEL_NOTIFICATION:
      return CUBEB_STREAM_TYPE_NOTIFICATION;
    case dom::AUDIO_CHANNEL_ALARM:
      return CUBEB_STREAM_TYPE_ALARM;
    case dom::AUDIO_CHANNEL_TELEPHONY:
      return CUBEB_STREAM_TYPE_VOICE_CALL;
    case dom::AUDIO_CHANNEL_RINGER:
      return CUBEB_STREAM_TYPE_RING;
    // Currently Android openSLES library doesn't support FORCE_AUDIBLE yet.
    case dom::AUDIO_CHANNEL_PUBLICNOTIFICATION:
    default:
      NS_ERROR("The value of AudioChannelType is invalid");
      return CUBEB_STREAM_TYPE_MAX;
  }
}
#endif

AudioStream::AudioStream()
  : mMonitor("AudioStream")
  , mInRate(0)
  , mOutRate(0)
  , mChannels(0)
  , mWritten(0)
  , mAudioClock(MOZ_THIS_IN_INITIALIZER_LIST())
  , mLatencyRequest(HighLatency)
  , mReadPoint(0)
  , mLostFrames(0)
  , mDumpFile(nullptr)
  , mVolume(1.0)
  , mBytesPerFrame(0)
  , mState(INITIALIZED)
{
  // keep a ref in case we shut down later than nsLayoutStatics
  mLatencyLog = AsyncLatencyLogger::Get(true);
}

AudioStream::~AudioStream()
{
  Shutdown();
  if (mDumpFile) {
    fclose(mDumpFile);
  }
}

/*static*/ void AudioStream::InitLibrary()
{
#ifdef PR_LOGGING
  gAudioStreamLog = PR_NewLogModule("AudioStream");
#endif
  PrefChanged(PREF_VOLUME_SCALE, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_VOLUME_SCALE);
  PrefChanged(PREF_CUBEB_LATENCY, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_CUBEB_LATENCY);

  InitPreferredSampleRate();
}

/*static*/ void AudioStream::ShutdownLibrary()
{
  Preferences::UnregisterCallback(PrefChanged, PREF_VOLUME_SCALE);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_LATENCY);

  StaticMutexAutoLock lock(sMutex);
  if (sCubebContext) {
    cubeb_destroy(sCubebContext);
    sCubebContext = nullptr;
  }
}

nsresult AudioStream::EnsureTimeStretcherInitialized()
{
  MonitorAutoLock mon(mMonitor);
  return EnsureTimeStretcherInitializedUnlocked();
}

nsresult AudioStream::EnsureTimeStretcherInitializedUnlocked()
{
  mMonitor.AssertCurrentThreadOwns();
  if (!mTimeStretcher) {
    // SoundTouch does not support a number of channels > 2
    if (mChannels > 2) {
      return NS_ERROR_FAILURE;
    }
    mTimeStretcher = new soundtouch::SoundTouch();
    mTimeStretcher->setSampleRate(mInRate);
    mTimeStretcher->setChannels(mChannels);
    mTimeStretcher->setPitch(1.0);
  }
  return NS_OK;
}

nsresult AudioStream::SetPlaybackRate(double aPlaybackRate)
{
  NS_ASSERTION(aPlaybackRate > 0.0,
               "Can't handle negative or null playbackrate in the AudioStream.");
  // Avoid instantiating the resampler if we are not changing the playback rate.
  if (aPlaybackRate == mAudioClock.GetPlaybackRate()) {
    return NS_OK;
  }

  if (EnsureTimeStretcherInitialized() != NS_OK) {
    return NS_ERROR_FAILURE;
  }

  mAudioClock.SetPlaybackRate(aPlaybackRate);
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
  // Avoid instantiating the timestretcher instance if not needed.
  if (aPreservesPitch == mAudioClock.GetPreservesPitch()) {
    return NS_OK;
  }

  if (EnsureTimeStretcherInitialized() != NS_OK) {
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
  return mWritten;
}

/*static*/ int AudioStream::MaxNumberOfChannels()
{
  cubeb* cubebContext = GetCubebContext();
  uint32_t maxNumberOfChannels;
  if (cubebContext &&
      cubeb_get_max_channel_count(cubebContext,
                                  &maxNumberOfChannels) == CUBEB_OK) {
    return static_cast<int>(maxNumberOfChannels);
  }

  return 0;
}

/*static */ void AudioStream::InitPreferredSampleRate()
{
  // Get the preferred samplerate for this platform, or fallback to something
  // sensible if we fail. We cache the value, because this might be accessed
  // often, and the complexity of the function call below depends on the
  // backend used.
  if (cubeb_get_preferred_sample_rate(GetCubebContext(),
                                      &sPreferredSampleRate) != CUBEB_OK) {
    sPreferredSampleRate = 44100;
  }
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
  sprintf(buf, "dumped-audio-%d.wav", gDumpedAudioCount);
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

  uint32_t samples = aStream->GetChannels()*aFrames;
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
                  const dom::AudioChannelType aAudioChannelType,
                  LatencyRequest aLatencyRequest)
{
  cubeb* cubebContext = GetCubebContext();

  if (!cubebContext || aNumChannels < 0 || aRate < 0) {
    return NS_ERROR_FAILURE;
  }

  PR_LOG(gAudioStreamLog, PR_LOG_DEBUG,
    ("%s  channels: %d, rate: %d", __FUNCTION__, aNumChannels, aRate));
  mInRate = mOutRate = aRate;
  mChannels = aNumChannels;
  mLatencyRequest = aLatencyRequest;

  mDumpFile = OpenDumpFile(this);

  cubeb_stream_params params;
  params.rate = aRate;
  params.channels = aNumChannels;
#if defined(__ANDROID__)
#if defined(MOZ_B2G)
  params.stream_type = ConvertChannelToCubebType(aAudioChannelType);
#else
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
  mBytesPerFrame = sizeof(AudioDataValue) * aNumChannels;

  mAudioClock.Init();

  // If the latency pref is set, use it. Otherwise, if this stream is intended
  // for low latency playback, try to get the lowest latency possible.
  // Otherwise, for normal streams, use 100ms.
  uint32_t latency;
  if (aLatencyRequest == LowLatency && !CubebLatencyPrefSet()) {
    if (cubeb_get_min_latency(cubebContext, params, &latency) != CUBEB_OK) {
      latency = GetCubebLatency();
    }
  } else {
    latency = GetCubebLatency();
  }

  {
    cubeb_stream* stream;
    if (cubeb_stream_init(cubebContext, &stream, "AudioStream", params,
                          latency, DataCallback_S, StateCallback_S, this) == CUBEB_OK) {
      mCubebStream.own(stream);
    }
  }

  if (!mCubebStream) {
    return NS_ERROR_FAILURE;
  }

  // Size mBuffer for one second of audio.  This value is arbitrary, and was
  // selected based on the observed behaviour of the existing AudioStream
  // implementations.
  uint32_t bufferLimit = FramesToBytes(aRate);
  NS_ABORT_IF_FALSE(bufferLimit % mBytesPerFrame == 0, "Must buffer complete frames");
  mBuffer.SetCapacity(bufferLimit);

  // Start the stream right away when low latency has been requested. This means
  // that the DataCallback will feed silence to cubeb, until the first frames
  // are writtent to this AudioStream.
  if (mLatencyRequest == LowLatency) {
    Start();
  }

  return NS_OK;
}

void
AudioStream::Shutdown()
{
  if (mState == STARTED) {
    Pause();
  }
  if (mCubebStream) {
    mCubebStream.reset();
  }
}

// aTime is the time in ms the samples were inserted into MediaStreamGraph
nsresult
AudioStream::Write(const AudioDataValue* aBuf, uint32_t aFrames, TimeStamp *aTime)
{
  MonitorAutoLock mon(mMonitor);
  if (!mCubebStream || mState == ERRORED) {
    return NS_ERROR_FAILURE;
  }
  NS_ASSERTION(mState == INITIALIZED || mState == STARTED,
    "Stream write in unexpected state.");

  const uint8_t* src = reinterpret_cast<const uint8_t*>(aBuf);
  uint32_t bytesToCopy = FramesToBytes(aFrames);

  // XXX this will need to change if we want to enable this on-the-fly!
  if (PR_LOG_TEST(GetLatencyLog(), PR_LOG_DEBUG)) {
    // Record the position and time this data was inserted
    int64_t timeMs;
    if (aTime && !aTime->IsNull()) {
      if (mStartTime.IsNull()) {
        AsyncLatencyLogger::Get(true)->GetStartTime(mStartTime);
      }
      timeMs = (*aTime - mStartTime).ToMilliseconds();
    } else {
      timeMs = 0;
    }
    struct Inserts insert = { timeMs, aFrames};
    mInserts.AppendElement(insert);
  }

  while (bytesToCopy > 0) {
    uint32_t available = std::min(bytesToCopy, mBuffer.Available());
    NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0,
        "Must copy complete frames.");

    mBuffer.AppendElements(src, available);
    src += available;
    bytesToCopy -= available;

    if (bytesToCopy > 0) {
      // If we are not playing, but our buffer is full, start playing to make
      // room for soon-to-be-decoded data.
      if (mState != STARTED) {
        StartUnlocked();
        if (mState != STARTED) {
          return NS_ERROR_FAILURE;
        }
      }
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
  NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Buffer invariant violated.");
  return BytesToFrames(mBuffer.Available());
}

void
AudioStream::SetVolume(double aVolume)
{
  MonitorAutoLock mon(mMonitor);
  NS_ABORT_IF_FALSE(aVolume >= 0.0 && aVolume <= 1.0, "Invalid volume");
  mVolume = aVolume;
}

void
AudioStream::Drain()
{
  MonitorAutoLock mon(mMonitor);
  if (mState != STARTED) {
    NS_ASSERTION(mBuffer.Available() == 0, "Draining with unplayed audio");
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
  if (!mCubebStream || mState != INITIALIZED) {
    return;
  }
  if (mState != STARTED) {
    int r;
    {
      MonitorAutoUnlock mon(mMonitor);
      r = cubeb_stream_start(mCubebStream);
    }
    if (mState != ERRORED) {
      mState = r == CUBEB_OK ? STARTED : ERRORED;
    }
  }
}

void
AudioStream::Pause()
{
  MonitorAutoLock mon(mMonitor);
  if (!mCubebStream || mState != STARTED) {
    return;
  }

  int r;
  {
    MonitorAutoUnlock mon(mMonitor);
    r = cubeb_stream_stop(mCubebStream);
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
    r = cubeb_stream_start(mCubebStream);
  }
  if (mState != ERRORED && r == CUBEB_OK) {
    mState = STARTED;
  }
}

int64_t
AudioStream::GetPosition()
{
  return mAudioClock.GetPosition();
}

// This function is miscompiled by PGO with MSVC 2010.  See bug 768333.
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
int64_t
AudioStream::GetPositionInFrames()
{
  return mAudioClock.GetPositionInFrames();
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

int64_t
AudioStream::GetPositionInFramesInternal()
{
  MonitorAutoLock mon(mMonitor);
  return GetPositionInFramesUnlocked();
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
    if (cubeb_stream_get_position(mCubebStream, &position) != CUBEB_OK) {
      return -1;
    }
  }

  // Adjust the reported position by the number of silent frames written
  // during stream underruns.
  uint64_t adjustedPosition = 0;
  if (position >= mLostFrames) {
    adjustedPosition = position - mLostFrames;
  }
  return std::min<uint64_t>(adjustedPosition, INT64_MAX);
}

int64_t
AudioStream::GetLatencyInFrames()
{
  uint32_t latency;
  if (cubeb_stream_get_latency(mCubebStream, &latency)) {
    NS_WARNING("Could not get cubeb latency.");
    return 0;
  }
  return static_cast<int64_t>(latency);
}

bool
AudioStream::IsPaused()
{
  MonitorAutoLock mon(mMonitor);
  return mState == STOPPED;
}

void
AudioStream::GetBufferInsertTime(int64_t &aTimeMs)
{
  if (mInserts.Length() > 0) {
    // Find the right block, but don't leave the array empty
    while (mInserts.Length() > 1 && mReadPoint >= mInserts[0].mFrames) {
      mReadPoint -= mInserts[0].mFrames;
      mInserts.RemoveElementAt(0);
    }
    // offset for amount already read
    // XXX Note: could misreport if we couldn't find a block in the right timeframe
    aTimeMs = mInserts[0].mTimeMs + ((mReadPoint * 1000) / mOutRate);
  } else {
    aTimeMs = INT64_MAX;
  }
}

long
AudioStream::GetUnprocessed(void* aBuffer, long aFrames, int64_t &aTimeMs)
{
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

  // First time block now has our first returned sample
  mReadPoint += BytesToFrames(available);
  GetBufferInsertTime(aTimeMs);

  return BytesToFrames(available) + flushedFrames;
}

// Get unprocessed samples, and pad the beginning of the buffer with silence if
// there is not enough data.
long
AudioStream::GetUnprocessedWithSilencePadding(void* aBuffer, long aFrames, int64_t& aTimeMs)
{
  uint32_t toPopBytes = FramesToBytes(aFrames);
  uint32_t available = std::min(toPopBytes, mBuffer.Length());
  uint32_t silenceOffset = toPopBytes - available;

  uint8_t* wpos = reinterpret_cast<uint8_t*>(aBuffer);

  memset(wpos, 0, silenceOffset);
  wpos += silenceOffset;

  void* input[2];
  uint32_t input_size[2];
  mBuffer.PopElements(available, &input[0], &input_size[0], &input[1], &input_size[1]);
  memcpy(wpos, input[0], input_size[0]);
  wpos += input_size[0];
  memcpy(wpos, input[1], input_size[1]);

  GetBufferInsertTime(aTimeMs);

  return aFrames;
}

long
AudioStream::GetTimeStretched(void* aBuffer, long aFrames, int64_t &aTimeMs)
{
  long processedFrames = 0;

  // We need to call the non-locking version, because we already have the lock.
  if (EnsureTimeStretcherInitializedUnlocked() != NS_OK) {
    return 0;
  }

  uint8_t* wpos = reinterpret_cast<uint8_t*>(aBuffer);
  double playbackRate = static_cast<double>(mInRate) / mOutRate;
  uint32_t toPopBytes = FramesToBytes(ceil(aFrames / playbackRate));
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
      mReadPoint += BytesToFrames(available);
      for(uint32_t i = 0; i < 2; i++) {
        mTimeStretcher->putSamples(reinterpret_cast<AudioDataValue*>(input[i]), BytesToFrames(input_size[i]));
      }
    }
    uint32_t receivedFrames = mTimeStretcher->receiveSamples(reinterpret_cast<AudioDataValue*>(wpos), aFrames - processedFrames);
    wpos += FramesToBytes(receivedFrames);
    processedFrames += receivedFrames;
  } while (processedFrames < aFrames && !lowOnBufferedData);

  GetBufferInsertTime(aTimeMs);

  return processedFrames;
}

long
AudioStream::DataCallback(void* aBuffer, long aFrames)
{
  MonitorAutoLock mon(mMonitor);
  uint32_t available = std::min(static_cast<uint32_t>(FramesToBytes(aFrames)), mBuffer.Length());
  NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0, "Must copy complete frames");
  AudioDataValue* output = reinterpret_cast<AudioDataValue*>(aBuffer);
  uint32_t underrunFrames = 0;
  uint32_t servicedFrames = 0;
  int64_t insertTime;

  if (available) {
    // When we are playing a low latency stream, and it is the first time we are
    // getting data from the buffer, we prefer to add the silence for an
    // underrun at the beginning of the buffer, so the first buffer is not cut
    // in half by the silence inserted to compensate for the underrun.
    if (mInRate == mOutRate) {
      if (mLatencyRequest == LowLatency && !mWritten) {
        servicedFrames = GetUnprocessedWithSilencePadding(output, aFrames, insertTime);
      } else {
        servicedFrames = GetUnprocessed(output, aFrames, insertTime);
      }
    } else {
      servicedFrames = GetTimeStretched(output, aFrames, insertTime);
    }
    float scaled_volume = float(GetVolumeScale() * mVolume);

    ScaleAudioSamples(output, aFrames * mChannels, scaled_volume);

    NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Must copy complete frames");

    // Notify any blocked Write() call that more space is available in mBuffer.
    mon.NotifyAll();
  } else {
    GetBufferInsertTime(insertTime);
  }

  underrunFrames = aFrames - servicedFrames;

  if (mState != DRAINING) {
    uint8_t* rpos = static_cast<uint8_t*>(aBuffer) + FramesToBytes(aFrames - underrunFrames);
    memset(rpos, 0, FramesToBytes(underrunFrames));
    if (underrunFrames) {
      PR_LOG(gAudioStreamLog, PR_LOG_WARNING,
             ("AudioStream %p lost %d frames", this, underrunFrames));
    }
    mLostFrames += underrunFrames;
    servicedFrames += underrunFrames;
  }

  WriteDumpFile(mDumpFile, this, aFrames, aBuffer);
  // Don't log if we're not interested or if the stream is inactive
  if (PR_LOG_TEST(GetLatencyLog(), PR_LOG_DEBUG) &&
      insertTime != INT64_MAX && servicedFrames > underrunFrames) {
    uint32_t latency = UINT32_MAX;
    if (cubeb_stream_get_latency(mCubebStream, &latency)) {
      NS_WARNING("Could not get latency from cubeb.");
    }
    TimeStamp now = TimeStamp::Now();

    mLatencyLog->Log(AsyncLatencyLogger::AudioStream, reinterpret_cast<uint64_t>(this),
                     insertTime, now);
    mLatencyLog->Log(AsyncLatencyLogger::Cubeb, reinterpret_cast<uint64_t>(mCubebStream.get()),
                     (latency * 1000) / mOutRate, now);
  }

  mAudioClock.UpdateWritePosition(servicedFrames);
  return servicedFrames;
}

void
AudioStream::StateCallback(cubeb_state aState)
{
  MonitorAutoLock mon(mMonitor);
  if (aState == CUBEB_STATE_DRAINED) {
    mState = DRAINED;
  } else if (aState == CUBEB_STATE_ERROR) {
    mState = ERRORED;
  }
  mon.NotifyAll();
}

AudioClock::AudioClock(AudioStream* aStream)
 :mAudioStream(aStream),
  mOldOutRate(0),
  mBasePosition(0),
  mBaseOffset(0),
  mOldBaseOffset(0),
  mOldBasePosition(0),
  mPlaybackRateChangeOffset(0),
  mPreviousPosition(0),
  mWritten(0),
  mOutRate(0),
  mInRate(0),
  mPreservesPitch(true),
  mCompensatingLatency(false)
{}

void AudioClock::Init()
{
  mOutRate = mAudioStream->GetRate();
  mInRate = mAudioStream->GetRate();
  mOldOutRate = mOutRate;
}

void AudioClock::UpdateWritePosition(uint32_t aCount)
{
  mWritten += aCount;
}

uint64_t AudioClock::GetPosition()
{
  int64_t position = mAudioStream->GetPositionInFramesInternal();
  int64_t diffOffset;
  NS_ASSERTION(position < 0 || (mInRate != 0 && mOutRate != 0), "AudioClock not initialized.");
  if (position >= 0) {
    if (position < mPlaybackRateChangeOffset) {
      // See if we are still playing frames pushed with the old playback rate in
      // the backend. If we are, use the old output rate to compute the
      // position.
      mCompensatingLatency = true;
      diffOffset = position - mOldBaseOffset;
      position = static_cast<uint64_t>(mOldBasePosition +
        static_cast<float>(USECS_PER_S * diffOffset) / mOldOutRate);
      mPreviousPosition = position;
      return position;
    }

    if (mCompensatingLatency) {
      diffOffset = position - mPlaybackRateChangeOffset;
      mCompensatingLatency = false;
      mBasePosition = mPreviousPosition;
    } else {
      diffOffset = position - mPlaybackRateChangeOffset;
    }
    position =  static_cast<uint64_t>(mBasePosition +
      (static_cast<float>(USECS_PER_S * diffOffset) / mOutRate));
    return position;
  }
  return UINT64_MAX;
}

uint64_t AudioClock::GetPositionInFrames()
{
  return (GetPosition() * mOutRate) / USECS_PER_S;
}

void AudioClock::SetPlaybackRate(double aPlaybackRate)
{
  int64_t position = mAudioStream->GetPositionInFramesInternal();
  if (position > mPlaybackRateChangeOffset) {
    mOldBasePosition = mBasePosition;
    mBasePosition = GetPosition();
    mOldBaseOffset = mPlaybackRateChangeOffset;
    mBaseOffset = position;
    mPlaybackRateChangeOffset = mWritten;
    mOldOutRate = mOutRate;
    mOutRate = static_cast<int>(mInRate / aPlaybackRate);
  } else {
    // The playbackRate has been changed before the end of the latency
    // compensation phase. We don't update the mOld* variable. That way, the
    // last playbackRate set is taken into account.
    mBasePosition = GetPosition();
    mBaseOffset = position;
    mPlaybackRateChangeOffset = mWritten;
    mOutRate = static_cast<int>(mInRate / aPlaybackRate);
  }
}

double AudioClock::GetPlaybackRate()
{
  return static_cast<double>(mInRate) / mOutRate;
}

void AudioClock::SetPreservesPitch(bool aPreservesPitch)
{
  mPreservesPitch = aPreservesPitch;
}

bool AudioClock::GetPreservesPitch()
{
  return mPreservesPitch;
}
} // namespace mozilla
