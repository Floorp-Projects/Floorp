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
#include "mozilla/Telemetry.h"
#include "soundtouch/SoundTouch.h"
#include "Latency.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

#ifdef PR_LOGGING
PRLogModuleInfo* gAudioStreamLog = nullptr;
// For simple logs
#define LOG(x) PR_LOG(gAudioStreamLog, PR_LOG_DEBUG, x)
#else
#define LOG(x)
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

/*static*/ void AudioStream::PrefChanged(const char* aPref, void* aClosure)
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
}

/*static*/ bool AudioStream::GetFirstStream()
{
  static bool sFirstStream = true;

  StaticMutexAutoLock lock(sMutex);
  bool result = sFirstStream;
  sFirstStream = false;
  return result;
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

/*static*/ void AudioStream::InitPreferredSampleRate()
{
  StaticMutexAutoLock lock(sMutex);
  if (sPreferredSampleRate == 0 &&
      cubeb_get_preferred_sample_rate(GetCubebContextUnlocked(),
                                      &sPreferredSampleRate) != CUBEB_OK) {
    sPreferredSampleRate = 44100;
  }
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
static cubeb_stream_type ConvertChannelToCubebType(dom::AudioChannel aChannel)
{
  switch(aChannel) {
    case dom::AudioChannel::Normal:
      return CUBEB_STREAM_TYPE_SYSTEM;
    case dom::AudioChannel::Content:
      return CUBEB_STREAM_TYPE_MUSIC;
    case dom::AudioChannel::Notification:
      return CUBEB_STREAM_TYPE_NOTIFICATION;
    case dom::AudioChannel::Alarm:
      return CUBEB_STREAM_TYPE_ALARM;
    case dom::AudioChannel::Telephony:
      return CUBEB_STREAM_TYPE_VOICE_CALL;
    case dom::AudioChannel::Ringer:
      return CUBEB_STREAM_TYPE_RING;
    case dom::AudioChannel::Publicnotification:
      return CUBEB_STREAM_TYPE_SYSTEM_ENFORCED;
    default:
      NS_ERROR("The value of AudioChannel is invalid");
      return CUBEB_STREAM_TYPE_MAX;
  }
}
#endif

AudioStream::AudioStream()
  : mMonitor("AudioStream")
  , mInRate(0)
  , mOutRate(0)
  , mChannels(0)
  , mOutChannels(0)
  , mWritten(0)
  , mAudioClock(MOZ_THIS_IN_INITIALIZER_LIST())
  , mLatencyRequest(HighLatency)
  , mReadPoint(0)
  , mDumpFile(nullptr)
  , mVolume(1.0)
  , mBytesPerFrame(0)
  , mState(INITIALIZED)
  , mNeedsStart(false)
{
  // keep a ref in case we shut down later than nsLayoutStatics
  mLatencyLog = AsyncLatencyLogger::Get(true);
}

AudioStream::~AudioStream()
{
  LOG(("AudioStream: delete %p, state %d", this, mState));
  MOZ_ASSERT(mState == SHUTDOWN && !mCubebStream,
             "Should've called Shutdown() before deleting an AudioStream");
  if (mDumpFile) {
    fclose(mDumpFile);
  }
}

size_t
AudioStream::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = aMallocSizeOf(this);

  // Possibly add in the future:
  // - mTimeStretcher
  // - mLatencyLog
  // - mCubebStream

  amount += mInserts.SizeOfExcludingThis(aMallocSizeOf);
  amount += mBuffer.SizeOfExcludingThis(aMallocSizeOf);

  return amount;
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

nsresult AudioStream::EnsureTimeStretcherInitializedUnlocked()
{
  mMonitor.AssertCurrentThreadOwns();
  if (!mTimeStretcher) {
    mTimeStretcher = new soundtouch::SoundTouch();
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

/*static*/ int AudioStream::PreferredSampleRate()
{
  MOZ_ASSERT(sPreferredSampleRate,
             "sPreferredSampleRate has not been initialized!");
  return sPreferredSampleRate;
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

// NOTE: this must not block a LowLatency stream for any significant amount
// of time, or it will block the entirety of MSG
nsresult
AudioStream::Init(int32_t aNumChannels, int32_t aRate,
                  const dom::AudioChannel aAudioChannel,
                  LatencyRequest aLatencyRequest)
{
  mStartTime = TimeStamp::Now();
  mIsFirst = GetFirstStream();

  if (!GetCubebContext() || aNumChannels < 0 || aRate < 0) {
    return NS_ERROR_FAILURE;
  }

  PR_LOG(gAudioStreamLog, PR_LOG_DEBUG,
    ("%s  channels: %d, rate: %d for %p", __FUNCTION__, aNumChannels, aRate, this));
  mInRate = mOutRate = aRate;
  mChannels = aNumChannels;
  mOutChannels = (aNumChannels > 2) ? 2 : aNumChannels;
  mLatencyRequest = aLatencyRequest;

  mDumpFile = OpenDumpFile(this);

  cubeb_stream_params params;
  params.rate = aRate;
  params.channels = mOutChannels;
#if defined(__ANDROID__)
#if defined(MOZ_B2G)
  params.stream_type = ConvertChannelToCubebType(aAudioChannel);
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
  mBytesPerFrame = sizeof(AudioDataValue) * mOutChannels;

  mAudioClock.Init();

  // Size mBuffer for one second of audio.  This value is arbitrary, and was
  // selected based on the observed behaviour of the existing AudioStream
  // implementations.
  uint32_t bufferLimit = FramesToBytes(aRate);
  NS_ABORT_IF_FALSE(bufferLimit % mBytesPerFrame == 0, "Must buffer complete frames");
  mBuffer.SetCapacity(bufferLimit);

  if (aLatencyRequest == LowLatency) {
    // Don't block this thread to initialize a cubeb stream.
    // When this is done, it will start callbacks from Cubeb.  Those will
    // cause us to move from INITIALIZED to RUNNING.  Until then, we
    // can't access any cubeb functions.
    // Use a RefPtr to avoid leaks if Dispatch fails
    RefPtr<AudioInitTask> init = new AudioInitTask(this, aLatencyRequest, params);
    init->Dispatch();
    return NS_OK;
  }
  // High latency - open synchronously
  nsresult rv = OpenCubeb(params, aLatencyRequest);
  // See if we need to start() the stream, since we must do that from this
  // thread for now (cubeb API issue)
  {
    MonitorAutoLock mon(mMonitor);
    CheckForStart();
  }
  return rv;
}

// This code used to live inside AudioStream::Init(), but on Mac (others?)
// it has been known to take 300-800 (or even 8500) ms to execute(!)
nsresult
AudioStream::OpenCubeb(cubeb_stream_params &aParams,
                       LatencyRequest aLatencyRequest)
{
  cubeb* cubebContext = GetCubebContext();
  if (!cubebContext) {
    MonitorAutoLock mon(mMonitor);
    mState = AudioStream::ERRORED;
    return NS_ERROR_FAILURE;
  }

  // If the latency pref is set, use it. Otherwise, if this stream is intended
  // for low latency playback, try to get the lowest latency possible.
  // Otherwise, for normal streams, use 100ms.
  uint32_t latency;
  if (aLatencyRequest == LowLatency && !CubebLatencyPrefSet()) {
    if (cubeb_get_min_latency(cubebContext, aParams, &latency) != CUBEB_OK) {
      latency = GetCubebLatency();
    }
  } else {
    latency = GetCubebLatency();
  }

  {
    cubeb_stream* stream;
    if (cubeb_stream_init(cubebContext, &stream, "AudioStream", aParams,
                          latency, DataCallback_S, StateCallback_S, this) == CUBEB_OK) {
      MonitorAutoLock mon(mMonitor);
      mCubebStream.own(stream);
      // Make sure we weren't shut down while in flight!
      if (mState == SHUTDOWN) {
        mCubebStream.reset();
        LOG(("AudioStream::OpenCubeb() %p Shutdown while opening cubeb", this));
        return NS_ERROR_FAILURE;
      }

      // We can't cubeb_stream_start() the thread from a transient thread due to
      // cubeb API requirements (init can be called from another thread, but
      // not start/stop/destroy/etc)
    } else {
      MonitorAutoLock mon(mMonitor);
      mState = ERRORED;
      LOG(("AudioStream::OpenCubeb() %p failed to init cubeb", this));
      return NS_ERROR_FAILURE;
    }
  }

  if (!mStartTime.IsNull()) {
		TimeDuration timeDelta = TimeStamp::Now() - mStartTime;
    LOG(("AudioStream creation time %sfirst: %u ms", mIsFirst ? "" : "not ",
         (uint32_t) timeDelta.ToMilliseconds()));
		Telemetry::Accumulate(mIsFirst ? Telemetry::AUDIOSTREAM_FIRST_OPEN_MS :
                          Telemetry::AUDIOSTREAM_LATER_OPEN_MS, timeDelta.ToMilliseconds());
  }

  return NS_OK;
}

void
AudioStream::CheckForStart()
{
  mMonitor.AssertCurrentThreadOwns();
  if (mState == INITIALIZED) {
    // Start the stream right away when low latency has been requested. This means
    // that the DataCallback will feed silence to cubeb, until the first frames
    // are written to this AudioStream.  Also start if a start has been queued.
    if (mLatencyRequest == LowLatency || mNeedsStart) {
      StartUnlocked(); // mState = STARTED or ERRORED
      mNeedsStart = false;
      PR_LOG(gAudioStreamLog, PR_LOG_WARNING,
             ("Started waiting %s-latency stream",
              mLatencyRequest == LowLatency ? "low" : "high"));
    } else {
      // high latency, not full - OR Pause() was called before we got here
      PR_LOG(gAudioStreamLog, PR_LOG_DEBUG,
             ("Not starting waiting %s-latency stream",
              mLatencyRequest == LowLatency ? "low" : "high"));
    }
  }
}

NS_IMETHODIMP
AudioInitTask::Run()
{
  MOZ_ASSERT(mThread);
  if (NS_IsMainThread()) {
    mThread->Shutdown(); // can't Shutdown from the thread itself, darn
    // Don't null out mThread!
    // See bug 999104.  We must hold a ref to the thread across Dispatch()
    // since the internal mThread ref could be released while processing
    // the Dispatch(), and Dispatch/PutEvent itself doesn't hold a ref; it
    // assumes the caller does.
    return NS_OK;
  }

  nsresult rv = mAudioStream->OpenCubeb(mParams, mLatencyRequest);

  // and now kill this thread
  NS_DispatchToMainThread(this);
  return rv;
}

// aTime is the time in ms the samples were inserted into MediaStreamGraph
nsresult
AudioStream::Write(const AudioDataValue* aBuf, uint32_t aFrames, TimeStamp *aTime)
{
  MonitorAutoLock mon(mMonitor);
  if (mState == ERRORED) {
    return NS_ERROR_FAILURE;
  }
  NS_ASSERTION(mState == INITIALIZED || mState == STARTED || mState == RUNNING,
    "Stream write in unexpected state.");

  // See if we need to start() the stream, since we must do that from this thread
  CheckForStart();

  // Downmix to Stereo.
  if (mChannels > 2 && mChannels <= 8) {
    DownmixAudioToStereo(const_cast<AudioDataValue*> (aBuf), mChannels, aFrames);
  }
  else if (mChannels > 8) {
    return NS_ERROR_FAILURE;
  }

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
      // Careful - the CubebInit thread may not have gotten to STARTED yet
      if ((mState == INITIALIZED || mState == STARTED) && mLatencyRequest == LowLatency) {
        // don't ever block MediaStreamGraph low-latency streams
        uint32_t remains = 0; // we presume the buffer is full
        if (mBuffer.Length() > bytesToCopy) {
          remains = mBuffer.Length() - bytesToCopy; // Free up just enough space
        }
        // account for dropping samples
        PR_LOG(gAudioStreamLog, PR_LOG_WARNING, ("Stream %p dropping %u bytes (%u frames)in Write()",
            this, mBuffer.Length() - remains, BytesToFrames(mBuffer.Length() - remains)));
        mReadPoint += BytesToFrames(mBuffer.Length() - remains);
        mBuffer.ContractTo(remains);
      } else { // RUNNING or high latency
        // If we are not playing, but our buffer is full, start playing to make
        // room for soon-to-be-decoded data.
        if (mState != STARTED && mState != RUNNING) {
          PR_LOG(gAudioStreamLog, PR_LOG_WARNING, ("Starting stream %p in Write (%u waiting)",
                                                 this, bytesToCopy));
          StartUnlocked();
          if (mState == ERRORED) {
            return NS_ERROR_FAILURE;
          }
        }
        PR_LOG(gAudioStreamLog, PR_LOG_WARNING, ("Stream %p waiting in Write() (%u waiting)",
                                                 this, bytesToCopy));
        mon.Wait();
      }
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
    mNeedsStart = true;
    return;
  }
  if (mState == INITIALIZED) {
    int r;
    {
      MonitorAutoUnlock mon(mMonitor);
      r = cubeb_stream_start(mCubebStream);
    }
    mState = r == CUBEB_OK ? STARTED : ERRORED;
    LOG(("AudioStream: started %p, state %s", this, mState == STARTED ? "STARTED" : "ERRORED"));
  }
}

void
AudioStream::Pause()
{
  MonitorAutoLock mon(mMonitor);
  if (!mCubebStream || (mState != STARTED && mState != RUNNING)) {
    mNeedsStart = false;
    mState = STOPPED; // which also tells async OpenCubeb not to start, just init
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

void
AudioStream::Shutdown()
{
  MonitorAutoLock mon(mMonitor);
  LOG(("AudioStream: Shutdown %p, state %d", this, mState));

  if (mCubebStream) {
    MonitorAutoUnlock mon(mMonitor);
    // Force stop to put the cubeb stream in a stable state before deletion.
    cubeb_stream_stop(mCubebStream);
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

// This function is miscompiled by PGO with MSVC 2010.  See bug 768333.
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
int64_t
AudioStream::GetPositionInFrames()
{
  MonitorAutoLock mon(mMonitor);
  return mAudioClock.GetPositionInFrames();
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

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

  return std::min<uint64_t>(position, INT64_MAX);
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
  mMonitor.AssertCurrentThreadOwns();
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
  mMonitor.AssertCurrentThreadOwns();
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
  MOZ_ASSERT(mState != SHUTDOWN, "No data callback after shutdown");
  uint32_t available = std::min(static_cast<uint32_t>(FramesToBytes(aFrames)), mBuffer.Length());
  NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0, "Must copy complete frames");
  AudioDataValue* output = reinterpret_cast<AudioDataValue*>(aBuffer);
  uint32_t underrunFrames = 0;
  uint32_t servicedFrames = 0;
  int64_t insertTime;

  // NOTE: wasapi (others?) can call us back *after* stop()/Shutdown() (mState == SHUTDOWN)
  // Bug 996162

  // callback tells us cubeb succeeded initializing
  if (mState == STARTED) {
    // For low-latency streams, we want to minimize any built-up data when
    // we start getting callbacks.
    // Simple version - contract on first callback only.
    if (mLatencyRequest == LowLatency) {
#ifdef PR_LOGGING
      uint32_t old_len = mBuffer.Length();
#endif
      available = mBuffer.ContractTo(FramesToBytes(aFrames));
#ifdef PR_LOGGING
      TimeStamp now = TimeStamp::Now();
      if (!mStartTime.IsNull()) {
        int64_t timeMs = (now - mStartTime).ToMilliseconds();
        PR_LOG(gAudioStreamLog, PR_LOG_WARNING,
               ("Stream took %lldms to start after first Write() @ %u", timeMs, mOutRate));
      } else {
        PR_LOG(gAudioStreamLog, PR_LOG_WARNING,
          ("Stream started before Write() @ %u", mOutRate));
      }

      if (old_len != available) {
        // Note that we may have dropped samples in Write() as well!
        PR_LOG(gAudioStreamLog, PR_LOG_WARNING,
               ("AudioStream %p dropped %u + %u initial frames @ %u", this,
                 mReadPoint, BytesToFrames(old_len - available), mOutRate));
        mReadPoint += BytesToFrames(old_len - available);
      }
#endif
    }
    mState = RUNNING;
  }

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

    ScaleAudioSamples(output, aFrames * mOutChannels, scaled_volume);

    NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Must copy complete frames");

    // Notify any blocked Write() call that more space is available in mBuffer.
    mon.NotifyAll();
  } else {
    GetBufferInsertTime(insertTime);
  }

  underrunFrames = aFrames - servicedFrames;

  // Always send audible frames first, and silent frames later.
  // Otherwise it will break the assumption of FrameHistory.
  if (mState != DRAINING) {
    mAudioClock.UpdateFrameHistory(servicedFrames, underrunFrames);
    uint8_t* rpos = static_cast<uint8_t*>(aBuffer) + FramesToBytes(aFrames - underrunFrames);
    memset(rpos, 0, FramesToBytes(underrunFrames));
    if (underrunFrames) {
      PR_LOG(gAudioStreamLog, PR_LOG_WARNING,
             ("AudioStream %p lost %d frames", this, underrunFrames));
    }
    servicedFrames += underrunFrames;
  } else {
    mAudioClock.UpdateFrameHistory(servicedFrames, 0);
  }

  WriteDumpFile(mDumpFile, this, aFrames, aBuffer);
  // Don't log if we're not interested or if the stream is inactive
  if (PR_LOG_TEST(GetLatencyLog(), PR_LOG_DEBUG) &&
      mState != SHUTDOWN &&
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
