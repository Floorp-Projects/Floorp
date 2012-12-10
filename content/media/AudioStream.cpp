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
#include "nsAlgorithm.h"
#include "VideoUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
extern "C" {
#include "sydneyaudio/sydney_audio.h"
}
#include "mozilla/Preferences.h"

#if defined(MOZ_CUBEB)
#include "nsAutoRef.h"
#include "cubeb/cubeb.h"

template <>
class nsAutoRefTraits<cubeb_stream> : public nsPointerRefTraits<cubeb_stream>
{
public:
  static void Release(cubeb_stream* aStream) { cubeb_stream_destroy(aStream); }
};

#endif

namespace mozilla {

#if defined(XP_MACOSX)
#define SA_PER_STREAM_VOLUME 1
#endif

#ifdef PR_LOGGING
PRLogModuleInfo* gAudioStreamLog = nullptr;
#endif

static const uint32_t FAKE_BUFFER_SIZE = 176400;

// Number of milliseconds per second.
static const int64_t MS_PER_S = 1000;

class NativeAudioStream : public AudioStream
{
 public:
  ~NativeAudioStream();
  NativeAudioStream();

  nsresult Init(int32_t aNumChannels, int32_t aRate,
                const dom::AudioChannelType aAudioChannelType);
  void Shutdown();
  nsresult Write(const AudioDataValue* aBuf, uint32_t aFrames);
  uint32_t Available();
  void SetVolume(double aVolume);
  void Drain();
  void Pause();
  void Resume();
  int64_t GetPosition();
  int64_t GetPositionInFrames();
  int64_t GetPositionInFramesInternal();
  bool IsPaused();
  int32_t GetMinWriteSize();

 private:
  int32_t WriteToBackend(const float* aBuffer, uint32_t aFrames);
  int32_t WriteToBackend(const short* aBuffer, uint32_t aFrames);

  double mVolume;
  void* mAudioHandle;

  // True if this audio stream is paused.
  bool mPaused;

  // True if this stream has encountered an error.
  bool mInError;

};

#define PREF_VOLUME_SCALE "media.volume_scale"
#define PREF_USE_CUBEB "media.use_cubeb"
#define PREF_CUBEB_LATENCY "media.cubeb_latency_ms"

static Mutex* gAudioPrefsLock = nullptr;
static double gVolumeScale;
static bool gUseCubeb;
static uint32_t gCubebLatency;

static int PrefChanged(const char* aPref, void* aClosure)
{
  if (strcmp(aPref, PREF_VOLUME_SCALE) == 0) {
    nsAdoptingString value = Preferences::GetString(aPref);
    MutexAutoLock lock(*gAudioPrefsLock);
    if (value.IsEmpty()) {
      gVolumeScale = 1.0;
    } else {
      NS_ConvertUTF16toUTF8 utf8(value);
      gVolumeScale = NS_MAX<double>(0, PR_strtod(utf8.get(), nullptr));
    }
  } else if (strcmp(aPref, PREF_USE_CUBEB) == 0) {
#ifdef MOZ_WIDGET_GONK
    bool value = Preferences::GetBool(aPref, false);
#else
    bool value = Preferences::GetBool(aPref, true);
#endif
    MutexAutoLock lock(*gAudioPrefsLock);
    gUseCubeb = value;
  } else if (strcmp(aPref, PREF_CUBEB_LATENCY) == 0) {
    // Arbitrary default stream latency of 100ms.  The higher this
    // value, the longer stream volume changes will take to become
    // audible.
    uint32_t value = Preferences::GetUint(aPref, 100);
    MutexAutoLock lock(*gAudioPrefsLock);
    gCubebLatency = NS_MIN<uint32_t>(NS_MAX<uint32_t>(value, 20), 1000);
  }
  return 0;
}

static double GetVolumeScale()
{
  MutexAutoLock lock(*gAudioPrefsLock);
  return gVolumeScale;
}

#if defined(MOZ_CUBEB)
static bool GetUseCubeb()
{
  MutexAutoLock lock(*gAudioPrefsLock);
  return gUseCubeb;
}

static cubeb* gCubebContext;

static cubeb* GetCubebContext()
{
  MutexAutoLock lock(*gAudioPrefsLock);
  if (gCubebContext ||
      cubeb_init(&gCubebContext, "AudioStream") == CUBEB_OK) {
    return gCubebContext;
  }
  NS_WARNING("cubeb_init failed");
  return nullptr;
}

static uint32_t GetCubebLatency()
{
  MutexAutoLock lock(*gAudioPrefsLock);
  return gCubebLatency;
}
#endif

static sa_stream_type_t ConvertChannelToSAType(dom::AudioChannelType aType)
{
  switch(aType) {
    case dom::AUDIO_CHANNEL_NORMAL:
      return SA_STREAM_TYPE_SYSTEM;
    case dom::AUDIO_CHANNEL_CONTENT:
      return SA_STREAM_TYPE_MUSIC;
    case dom::AUDIO_CHANNEL_NOTIFICATION:
      return SA_STREAM_TYPE_NOTIFICATION;
    case dom::AUDIO_CHANNEL_ALARM:
      return SA_STREAM_TYPE_ALARM;
    case dom::AUDIO_CHANNEL_TELEPHONY:
      return SA_STREAM_TYPE_VOICE_CALL;
    case dom::AUDIO_CHANNEL_RINGER:
      return SA_STREAM_TYPE_RING;
    case dom::AUDIO_CHANNEL_PUBLICNOTIFICATION:
      return SA_STREAM_TYPE_ENFORCED_AUDIBLE;
    default:
      NS_ERROR("The value of AudioChannelType is invalid");
      return SA_STREAM_TYPE_MAX;
  }
}

AudioStream::AudioStream()
: mInRate(0),
  mOutRate(0),
  mChannels(0),
  mAudioClock(this)
{}

void AudioStream::InitLibrary()
{
#ifdef PR_LOGGING
  gAudioStreamLog = PR_NewLogModule("AudioStream");
#endif
  gAudioPrefsLock = new Mutex("AudioStream::gAudioPrefsLock");
  PrefChanged(PREF_VOLUME_SCALE, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_VOLUME_SCALE);
#if defined(MOZ_CUBEB)
  PrefChanged(PREF_USE_CUBEB, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_USE_CUBEB);
  PrefChanged(PREF_CUBEB_LATENCY, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_CUBEB_LATENCY);
#endif
}

void AudioStream::ShutdownLibrary()
{
  Preferences::UnregisterCallback(PrefChanged, PREF_VOLUME_SCALE);
#if defined(MOZ_CUBEB)
  Preferences::UnregisterCallback(PrefChanged, PREF_USE_CUBEB);
#endif
  delete gAudioPrefsLock;
  gAudioPrefsLock = nullptr;

#if defined(MOZ_CUBEB)
  if (gCubebContext) {
    cubeb_destroy(gCubebContext);
    gCubebContext = nullptr;
  }
#endif
}

AudioStream::~AudioStream()
{
}

void AudioStream::EnsureTimeStretcherInitialized()
{
  if (!mTimeStretcher) {
    mTimeStretcher = new soundtouch::SoundTouch();
    mTimeStretcher->setSampleRate(mInRate);
    mTimeStretcher->setChannels(mChannels);
    mTimeStretcher->setPitch(1.0);
  }
}

nsresult AudioStream::SetPlaybackRate(double aPlaybackRate)
{
  NS_ASSERTION(aPlaybackRate > 0.0,
               "Can't handle negative or null playbackrate in the AudioStream.");
  // Avoid instantiating the resampler if we are not changing the playback rate.
  if (aPlaybackRate == mAudioClock.GetPlaybackRate()) {
    return NS_OK;
  }
  mAudioClock.SetPlaybackRate(aPlaybackRate);
  mOutRate = mInRate / aPlaybackRate;

  EnsureTimeStretcherInitialized();

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

  EnsureTimeStretcherInitialized();

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

NativeAudioStream::NativeAudioStream() :
  mVolume(1.0),
  mAudioHandle(0),
  mPaused(false),
  mInError(false)
{
}

NativeAudioStream::~NativeAudioStream()
{
  Shutdown();
}

nsresult NativeAudioStream::Init(int32_t aNumChannels, int32_t aRate,
                                   const dom::AudioChannelType aAudioChannelType)
{
  mInRate = mOutRate = aRate;
  mChannels = aNumChannels;

  if (sa_stream_create_pcm(reinterpret_cast<sa_stream_t**>(&mAudioHandle),
                           NULL,
                           SA_MODE_WRONLY,
                           SA_PCM_FORMAT_S16_NE,
                           aRate,
                           aNumChannels) != SA_SUCCESS) {
    mAudioHandle = nullptr;
    mInError = true;
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("NativeAudioStream: sa_stream_create_pcm error"));
    return NS_ERROR_FAILURE;
  }

  int saError = sa_stream_set_stream_type(static_cast<sa_stream_t*>(mAudioHandle),
                       ConvertChannelToSAType(aAudioChannelType));
  if (saError != SA_SUCCESS && saError != SA_ERROR_NOT_SUPPORTED) {
    mAudioHandle = nullptr;
    mInError = true;
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("NativeAudioStream: sa_stream_set_stream_type error"));
    return NS_ERROR_FAILURE;
  }

  if (sa_stream_open(static_cast<sa_stream_t*>(mAudioHandle)) != SA_SUCCESS) {
    sa_stream_destroy(static_cast<sa_stream_t*>(mAudioHandle));
    mAudioHandle = nullptr;
    mInError = true;
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("NativeAudioStream: sa_stream_open error"));
    return NS_ERROR_FAILURE;
  }
  mInError = false;

  mAudioClock.Init();

  return NS_OK;
}

void NativeAudioStream::Shutdown()
{
  if (!mAudioHandle)
    return;

  sa_stream_destroy(static_cast<sa_stream_t*>(mAudioHandle));
  mAudioHandle = nullptr;
  mInError = true;
}

int32_t NativeAudioStream::WriteToBackend(const AudioDataValue* aBuffer, uint32_t aSamples)
{
  double scaledVolume = GetVolumeScale() * mVolume;

  nsAutoArrayPtr<short> outputBuffer(new short[aSamples]);
  ConvertAudioSamplesWithScale(aBuffer, outputBuffer.get(), aSamples, scaledVolume);

  if (sa_stream_write(static_cast<sa_stream_t*>(mAudioHandle),
                      outputBuffer,
                      aSamples * sizeof(short)) != SA_SUCCESS) {
    return -1;
  }
  mAudioClock.UpdateWritePosition(aSamples / mChannels);
  return aSamples;
}

nsresult NativeAudioStream::Write(const AudioDataValue* aBuf, uint32_t aFrames)
{
  NS_ASSERTION(!mPaused, "Don't write audio when paused, you'll block");

  if (mInError)
    return NS_ERROR_FAILURE;

  uint32_t samples = aFrames * mChannels;
  int32_t written = -1;

  if (mInRate != mOutRate) {
    EnsureTimeStretcherInitialized();
    mTimeStretcher->putSamples(aBuf, aFrames);
    uint32_t numFrames = mTimeStretcher->numSamples();
    uint32_t arraySize = numFrames * mChannels * sizeof(AudioDataValue);
    nsAutoArrayPtr<AudioDataValue> data(new AudioDataValue[arraySize]);
    uint32_t framesAvailable = mTimeStretcher->receiveSamples(data, numFrames);
    NS_ASSERTION(mTimeStretcher->numSamples() == 0,
                 "We did not get all the data from the SoundTouch pipeline.");
    // It is possible to have nothing to write: the data are in the processing
    // pipeline, and will be written to the backend next time.
    if (framesAvailable) {
      written = WriteToBackend(data, framesAvailable * mChannels);
    } else {
      written = 0;
    }
  } else {
    written = WriteToBackend(aBuf, samples);
  }

  if (written == -1) {
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("NativeAudioStream: sa_stream_write error"));
    mInError = true;
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

uint32_t NativeAudioStream::Available()
{
  // If the audio backend failed to open, lie and say we'll accept some
  // data.
  if (mInError)
    return FAKE_BUFFER_SIZE;

  size_t s = 0;
  if (sa_stream_get_write_size(static_cast<sa_stream_t*>(mAudioHandle), &s) != SA_SUCCESS)
    return 0;

  return s / mChannels / sizeof(short);
}

void NativeAudioStream::SetVolume(double aVolume)
{
  NS_ASSERTION(aVolume >= 0.0 && aVolume <= 1.0, "Invalid volume");
#if defined(SA_PER_STREAM_VOLUME)
  if (sa_stream_set_volume_abs(static_cast<sa_stream_t*>(mAudioHandle), aVolume) != SA_SUCCESS) {
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("NativeAudioStream: sa_stream_set_volume_abs error"));
    mInError = true;
  }
#else
  mVolume = aVolume;
#endif
}

void NativeAudioStream::Drain()
{
  NS_ASSERTION(!mPaused, "Don't drain audio when paused, it won't finish!");

  // Write all the frames still in the time stretcher pipeline.
  if (mTimeStretcher) {
    uint32_t numFrames = mTimeStretcher->numSamples();
    uint32_t arraySize = numFrames * mChannels * sizeof(AudioDataValue);
    nsAutoArrayPtr<AudioDataValue> data(new AudioDataValue[arraySize]);
    uint32_t framesAvailable = mTimeStretcher->receiveSamples(data, numFrames);
    int32_t written = 0;
    if (framesAvailable) {
      written = WriteToBackend(data, framesAvailable * mChannels);
    }

    if (written == -1) {
      PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("NativeAudioStream: sa_stream_write error"));
      mInError = true;
    }

    NS_ASSERTION(mTimeStretcher->numSamples() == 0,
                 "We did not get all the data from the SoundTouch pipeline.");
  }

  if (mInError)
    return;

  int r = sa_stream_drain(static_cast<sa_stream_t*>(mAudioHandle));
  if (r != SA_SUCCESS && r != SA_ERROR_INVALID) {
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("NativeAudioStream: sa_stream_drain error"));
    mInError = true;
  }
}

void NativeAudioStream::Pause()
{
  if (mInError)
    return;
  mPaused = true;
  sa_stream_pause(static_cast<sa_stream_t*>(mAudioHandle));
}

void NativeAudioStream::Resume()
{
  if (mInError)
    return;
  mPaused = false;
  sa_stream_resume(static_cast<sa_stream_t*>(mAudioHandle));
}

int64_t NativeAudioStream::GetPosition()
{
  return mAudioClock.GetPosition();
}

int64_t NativeAudioStream::GetPositionInFrames()
{
  return mAudioClock.GetPositionInFrames();
}

int64_t NativeAudioStream::GetPositionInFramesInternal()
{
  if (mInError) {
    return -1;
  }

  sa_position_t positionType = SA_POSITION_WRITE_SOFTWARE;
#if defined(XP_WIN)
  positionType = SA_POSITION_WRITE_HARDWARE;
#endif
  int64_t position = 0;
  if (sa_stream_get_position(static_cast<sa_stream_t*>(mAudioHandle),
                             positionType, &position) == SA_SUCCESS) {
    return position / mChannels / sizeof(short);
  }

  return -1;
}

bool NativeAudioStream::IsPaused()
{
  return mPaused;
}

int32_t NativeAudioStream::GetMinWriteSize()
{
  size_t size;
  int r = sa_stream_get_min_write(static_cast<sa_stream_t*>(mAudioHandle),
                                  &size);
  if (r == SA_ERROR_NOT_SUPPORTED)
    return 1;
  else if (r != SA_SUCCESS || size > INT32_MAX)
    return -1;

  return static_cast<int32_t>(size / mChannels / sizeof(short));
}

#if defined(MOZ_CUBEB)
class nsCircularByteBuffer
{
public:
  nsCircularByteBuffer()
    : mBuffer(nullptr), mCapacity(0), mStart(0), mCount(0)
  {}

  // Set the capacity of the buffer in bytes.  Must be called before any
  // call to append or pop elements.
  void SetCapacity(uint32_t aCapacity) {
    NS_ABORT_IF_FALSE(!mBuffer, "Buffer allocated.");
    mCapacity = aCapacity;
    mBuffer = new uint8_t[mCapacity];
  }

  uint32_t Length() {
    return mCount;
  }

  uint32_t Capacity() {
    return mCapacity;
  }

  uint32_t Available() {
    return Capacity() - Length();
  }

  // Append aLength bytes from aSrc to the buffer.  Caller must check that
  // sufficient space is available.
  void AppendElements(const uint8_t* aSrc, uint32_t aLength) {
    NS_ABORT_IF_FALSE(mBuffer && mCapacity, "Buffer not initialized.");
    NS_ABORT_IF_FALSE(aLength <= Available(), "Buffer full.");

    uint32_t end = (mStart + mCount) % mCapacity;

    uint32_t toCopy = NS_MIN(mCapacity - end, aLength);
    memcpy(&mBuffer[end], aSrc, toCopy);
    memcpy(&mBuffer[0], aSrc + toCopy, aLength - toCopy);
    mCount += aLength;
  }

  // Remove aSize bytes from the buffer.  Caller must check returned size in
  // aSize{1,2} before using the pointer returned in aData{1,2}.  Caller
  // must not specify an aSize larger than Length().
  void PopElements(uint32_t aSize, void** aData1, uint32_t* aSize1,
                   void** aData2, uint32_t* aSize2) {
    NS_ABORT_IF_FALSE(mBuffer && mCapacity, "Buffer not initialized.");
    NS_ABORT_IF_FALSE(aSize <= Length(), "Request too large.");

    *aData1 = &mBuffer[mStart];
    *aSize1 = NS_MIN(mCapacity - mStart, aSize);
    *aData2 = &mBuffer[0];
    *aSize2 = aSize - *aSize1;
    mCount -= *aSize1 + *aSize2;
    mStart += *aSize1 + *aSize2;
    mStart %= mCapacity;
  }

private:
  nsAutoArrayPtr<uint8_t> mBuffer;
  uint32_t mCapacity;
  uint32_t mStart;
  uint32_t mCount;
};

class BufferedAudioStream : public AudioStream
{
 public:
  BufferedAudioStream();
  ~BufferedAudioStream();

  nsresult Init(int32_t aNumChannels, int32_t aRate,
                const dom::AudioChannelType aAudioChannelType);
  void Shutdown();
  nsresult Write(const AudioDataValue* aBuf, uint32_t aFrames);
  uint32_t Available();
  void SetVolume(double aVolume);
  void Drain();
  void Pause();
  void Resume();
  int64_t GetPosition();
  int64_t GetPositionInFrames();
  int64_t GetPositionInFramesInternal();
  bool IsPaused();
  int32_t GetMinWriteSize();

private:
  static long DataCallback_S(cubeb_stream*, void* aThis, void* aBuffer, long aFrames)
  {
    return static_cast<BufferedAudioStream*>(aThis)->DataCallback(aBuffer, aFrames);
  }

  static void StateCallback_S(cubeb_stream*, void* aThis, cubeb_state aState)
  {
    static_cast<BufferedAudioStream*>(aThis)->StateCallback(aState);
  }

  long DataCallback(void* aBuffer, long aFrames);
  void StateCallback(cubeb_state aState);

  long GetUnprocessed(void* aBuffer, long aFrames);

  long GetTimeStretched(void* aBuffer, long aFrames);


  // Shared implementation of underflow adjusted position calculation.
  // Caller must own the monitor.
  int64_t GetPositionInFramesUnlocked();

  // The monitor is held to protect all access to member variables.  Write()
  // waits while mBuffer is full; DataCallback() notifies as it consumes
  // data from mBuffer.  Drain() waits while mState is DRAINING;
  // StateCallback() notifies when mState is DRAINED.
  Monitor mMonitor;

  // Sum of silent frames written when DataCallback requests more frames
  // than are available in mBuffer.
  uint64_t mLostFrames;

  // Temporary audio buffer.  Filled by Write() and consumed by
  // DataCallback().  Once mBuffer is full, Write() blocks until sufficient
  // space becomes available in mBuffer.  mBuffer is sized in bytes, not
  // frames.
  nsCircularByteBuffer mBuffer;

  // Software volume level.  Applied during the servicing of DataCallback().
  double mVolume;

  // Owning reference to a cubeb_stream.  cubeb_stream_destroy is called by
  // nsAutoRef's destructor.
  nsAutoRef<cubeb_stream> mCubebStream;

  uint32_t mBytesPerFrame;

  uint32_t BytesToFrames(uint32_t aBytes) {
    NS_ASSERTION(aBytes % mBytesPerFrame == 0,
                 "Byte count not aligned on frames size.");
    return aBytes / mBytesPerFrame;
  }

  uint32_t FramesToBytes(uint32_t aFrames) {
    return aFrames * mBytesPerFrame;
  }


  enum StreamState {
    INITIALIZED, // Initialized, playback has not begun.
    STARTED,     // Started by a call to Write() (iff INITIALIZED) or Resume().
    STOPPED,     // Stopped by a call to Pause().
    DRAINING,    // Drain requested.  DataCallback will indicate end of stream
                 // once the remaining contents of mBuffer are requested by
                 // cubeb, after which StateCallback will indicate drain
                 // completion.
    DRAINED,     // StateCallback has indicated that the drain is complete.
    ERRORED      // Stream disabled due to an internal error.
  };

  StreamState mState;
};
#endif

AudioStream* AudioStream::AllocateStream()
{
#if defined(MOZ_CUBEB)
  if (GetUseCubeb()) {
    return new BufferedAudioStream();
  }
#endif
  return new NativeAudioStream();
}

#if defined(MOZ_CUBEB)
BufferedAudioStream::BufferedAudioStream()
  : mMonitor("BufferedAudioStream"), mLostFrames(0), mVolume(1.0),
    mBytesPerFrame(0), mState(INITIALIZED)
{
}

BufferedAudioStream::~BufferedAudioStream()
{
  Shutdown();
}

nsresult
BufferedAudioStream::Init(int32_t aNumChannels, int32_t aRate,
                            const dom::AudioChannelType aAudioChannelType)
{
  cubeb* cubebContext = GetCubebContext();

  if (!cubebContext || aNumChannels < 0 || aRate < 0) {
    return NS_ERROR_FAILURE;
  }

  mInRate = mOutRate = aRate;
  mChannels = aNumChannels;

  cubeb_stream_params params;
  params.rate = aRate;
  params.channels = aNumChannels;
  if (AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_S16) {
    params.format = CUBEB_SAMPLE_S16NE;
  } else {
    params.format = CUBEB_SAMPLE_FLOAT32NE;
  }
  mBytesPerFrame = sizeof(AudioDataValue) * aNumChannels;

  mAudioClock.Init();

  {
    cubeb_stream* stream;
    if (cubeb_stream_init(cubebContext, &stream, "BufferedAudioStream", params,
                          GetCubebLatency(), DataCallback_S, StateCallback_S, this) == CUBEB_OK) {
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

  return NS_OK;
}

void
BufferedAudioStream::Shutdown()
{
  if (mState == STARTED) {
    Pause();
  }
  if (mCubebStream) {
    mCubebStream.reset();
  }
}

nsresult
BufferedAudioStream::Write(const AudioDataValue* aBuf, uint32_t aFrames)
{
  MonitorAutoLock mon(mMonitor);
  if (!mCubebStream || mState == ERRORED) {
    return NS_ERROR_FAILURE;
  }
  NS_ASSERTION(mState == INITIALIZED || mState == STARTED,
    "Stream write in unexpected state.");

  const uint8_t* src = reinterpret_cast<const uint8_t*>(aBuf);
  uint32_t bytesToCopy = FramesToBytes(aFrames);

  while (bytesToCopy > 0) {
    uint32_t available = NS_MIN(bytesToCopy, mBuffer.Available());
    NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0,
        "Must copy complete frames.");

    mBuffer.AppendElements(src, available);
    src += available;
    bytesToCopy -= available;

    if (mState != STARTED) {
      int r;
      {
        MonitorAutoUnlock mon(mMonitor);
        r = cubeb_stream_start(mCubebStream);
      }
      mState = r == CUBEB_OK ? STARTED : ERRORED;
    }

    if (mState != STARTED) {
      return NS_ERROR_FAILURE;
    }

    if (bytesToCopy > 0) {
      mon.Wait();
    }
  }

  return NS_OK;
}

uint32_t
BufferedAudioStream::Available()
{
  MonitorAutoLock mon(mMonitor);
  NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Buffer invariant violated.");
  return BytesToFrames(mBuffer.Available());
}

int32_t
BufferedAudioStream::GetMinWriteSize()
{
  return 1;
}

void
BufferedAudioStream::SetVolume(double aVolume)
{
  MonitorAutoLock mon(mMonitor);
  NS_ABORT_IF_FALSE(aVolume >= 0.0 && aVolume <= 1.0, "Invalid volume");
  mVolume = aVolume;
}

void
BufferedAudioStream::Drain()
{
  MonitorAutoLock mon(mMonitor);
  if (mState != STARTED) {
    return;
  }
  mState = DRAINING;
  while (mState == DRAINING) {
    mon.Wait();
  }
}

void
BufferedAudioStream::Pause()
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
BufferedAudioStream::Resume()
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
BufferedAudioStream::GetPosition()
{
  return mAudioClock.GetPosition();
}

// This function is miscompiled by PGO with MSVC 2010.  See bug 768333.
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
int64_t
BufferedAudioStream::GetPositionInFrames()
{
  return mAudioClock.GetPositionInFrames();
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

int64_t
BufferedAudioStream::GetPositionInFramesInternal()
{
  MonitorAutoLock mon(mMonitor);
  return GetPositionInFramesUnlocked();
}

int64_t
BufferedAudioStream::GetPositionInFramesUnlocked()
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
  return NS_MIN<uint64_t>(adjustedPosition, INT64_MAX);
}

bool
BufferedAudioStream::IsPaused()
{
  MonitorAutoLock mon(mMonitor);
  return mState == STOPPED;
}

long
BufferedAudioStream::GetUnprocessed(void* aBuffer, long aFrames)
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
  uint32_t available = NS_MIN(toPopBytes, mBuffer.Length());

  void* input[2];
  uint32_t input_size[2];
  mBuffer.PopElements(available, &input[0], &input_size[0], &input[1], &input_size[1]);
  memcpy(wpos, input[0], input_size[0]);
  wpos += input_size[0];
  memcpy(wpos, input[1], input_size[1]);
  return BytesToFrames(available) + flushedFrames;
}

long
BufferedAudioStream::GetTimeStretched(void* aBuffer, long aFrames)
{
  long processedFrames = 0;

  EnsureTimeStretcherInitialized();

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
      available = NS_MIN(mBuffer.Length(), toPopBytes);
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
BufferedAudioStream::DataCallback(void* aBuffer, long aFrames)
{
  MonitorAutoLock mon(mMonitor);
  uint32_t available = NS_MIN(static_cast<uint32_t>(FramesToBytes(aFrames)), mBuffer.Length());
  NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0, "Must copy complete frames");
  uint32_t underrunFrames = 0;
  uint32_t servicedFrames = 0;

  if (available) {
    AudioDataValue* output = reinterpret_cast<AudioDataValue*>(aBuffer);
    if (mInRate == mOutRate) {
      servicedFrames = GetUnprocessed(output, aFrames);
    } else {
      servicedFrames = GetTimeStretched(output, aFrames);
    }
    float scaled_volume = float(GetVolumeScale() * mVolume);

    ScaleAudioSamples(output, aFrames * mChannels, scaled_volume);

    NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Must copy complete frames");

    // Notify any blocked Write() call that more space is available in mBuffer.
    mon.NotifyAll();
  }

  underrunFrames = aFrames - servicedFrames;

  if (mState != DRAINING) {
    uint8_t* rpos = static_cast<uint8_t*>(aBuffer) + FramesToBytes(aFrames - underrunFrames);
    memset(rpos, 0, FramesToBytes(underrunFrames));
    mLostFrames += underrunFrames;
    servicedFrames += underrunFrames;
  }

  mAudioClock.UpdateWritePosition(servicedFrames);
  return servicedFrames;
}

void
BufferedAudioStream::StateCallback(cubeb_state aState)
{
  MonitorAutoLock mon(mMonitor);
  if (aState == CUBEB_STATE_DRAINED) {
    mState = DRAINED;
  } else if (aState == CUBEB_STATE_ERROR) {
    mState = ERRORED;
  }
  mon.NotifyAll();
}

#endif

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
  mPlaybackRate(1.0),
  mCompensatingLatency(false)
{}

void AudioClock::Init()
{
  mOutRate = mAudioStream->GetRate();
  mInRate = mAudioStream->GetRate();
  mPlaybackRate = 1.0;
  mOldOutRate = mOutRate;
}

void AudioClock::UpdateWritePosition(uint32_t aCount)
{
  mWritten += aCount;
}

uint64_t AudioClock::GetPosition()
{
  NS_ASSERTION(mInRate != 0 && mOutRate != 0, "AudioClock not initialized.");
  int64_t position = mAudioStream->GetPositionInFramesInternal();
  int64_t diffOffset;
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
  return -1;
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
  return mPlaybackRate;
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
