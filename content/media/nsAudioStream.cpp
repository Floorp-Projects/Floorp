/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PAudioChild.h"
#include "mozilla/dom/AudioChild.h"
#include "nsXULAppAPI.h"
using namespace mozilla::dom;

#include <stdio.h>
#include <math.h>
#include "prlog.h"
#include "prmem.h"
#include "prdtoa.h"
#include "nsAutoPtr.h"
#include "nsAudioStream.h"
#include "nsAlgorithm.h"
#include "VideoUtils.h"
#include "mozilla/Mutex.h"
extern "C" {
#include "sydneyaudio/sydney_audio.h"
}
#include "nsThreadUtils.h"
#include "mozilla/Preferences.h"

#if defined(MOZ_CUBEB)
#include "nsAutoRef.h"
#include "cubeb/cubeb.h"
#endif

using namespace mozilla;

#if defined(XP_MACOSX)
#define SA_PER_STREAM_VOLUME 1
#endif

// Android's audio backend is not available in content processes, so audio must
// be remoted to the parent chrome process.
#if defined(ANDROID)
#define REMOTE_AUDIO 1
#endif

#ifdef PR_LOGGING
PRLogModuleInfo* gAudioStreamLog = nullptr;
#endif

static const PRUint32 FAKE_BUFFER_SIZE = 176400;

// Number of milliseconds per second.
static const PRInt64 MS_PER_S = 1000;

class nsNativeAudioStream : public nsAudioStream
{
 public:
  NS_DECL_ISUPPORTS

  ~nsNativeAudioStream();
  nsNativeAudioStream();

  nsresult Init(PRInt32 aNumChannels, PRInt32 aRate, SampleFormat aFormat);
  void Shutdown();
  nsresult Write(const void* aBuf, PRUint32 aFrames);
  PRUint32 Available();
  void SetVolume(double aVolume);
  void Drain();
  void Pause();
  void Resume();
  PRInt64 GetPosition();
  PRInt64 GetPositionInFrames();
  bool IsPaused();
  PRInt32 GetMinWriteSize();

 private:

  double mVolume;
  void* mAudioHandle;

  // True if this audio stream is paused.
  bool mPaused;

  // True if this stream has encountered an error.
  bool mInError;

};

#if defined(REMOTE_AUDIO)
class nsRemotedAudioStream : public nsAudioStream
{
 public:
  NS_DECL_ISUPPORTS

  nsRemotedAudioStream();
  ~nsRemotedAudioStream();

  nsresult Init(PRInt32 aNumChannels, PRInt32 aRate, SampleFormat aFormat);
  void Shutdown();
  nsresult Write(const void* aBuf, PRUint32 aFrames);
  PRUint32 Available();
  void SetVolume(double aVolume);
  void Drain();
  void Pause();
  void Resume();
  PRInt64 GetPosition();
  PRInt64 GetPositionInFrames();
  bool IsPaused();
  PRInt32 GetMinWriteSize();

private:
  nsRefPtr<AudioChild> mAudioChild;

  PRInt32 mBytesPerFrame;

  // True if this audio stream is paused.
  bool mPaused;

  friend class AudioInitEvent;
};

class AudioInitEvent : public nsRunnable
{
 public:
  AudioInitEvent(nsRemotedAudioStream* owner)
  {
    mOwner = owner;
  }

  NS_IMETHOD Run()
  {
    ContentChild * cpc = ContentChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");
    mOwner->mAudioChild =  static_cast<AudioChild*>(cpc->SendPAudioConstructor(mOwner->mChannels,
                                                                               mOwner->mRate,
                                                                               mOwner->mFormat));
    return NS_OK;
  }

  nsRefPtr<nsRemotedAudioStream> mOwner;
};

class AudioWriteEvent : public nsRunnable
{
 public:
  AudioWriteEvent(AudioChild* aChild,
                  const void* aBuf,
                  PRUint32 aNumberOfFrames,
                  PRUint32 aBytesPerFrame)
  {
    mAudioChild = aChild;
    mBytesPerFrame = aBytesPerFrame;
    mBuffer.Assign((const char*)aBuf, aNumberOfFrames * aBytesPerFrame);
  }

  NS_IMETHOD Run()
  {
    if (!mAudioChild->IsIPCOpen())
      return NS_OK;

    mAudioChild->SendWrite(mBuffer, mBuffer.Length() / mBytesPerFrame);
    return NS_OK;
  }

  nsRefPtr<AudioChild> mAudioChild;
  nsCString mBuffer;
  PRUint32 mBytesPerFrame;
};

class AudioSetVolumeEvent : public nsRunnable
{
 public:
  AudioSetVolumeEvent(AudioChild* aChild, double aVolume)
  {
    mAudioChild = aChild;
    mVolume = aVolume;
  }

  NS_IMETHOD Run()
  {
    if (!mAudioChild->IsIPCOpen())
      return NS_OK;

    mAudioChild->SendSetVolume(mVolume);
    return NS_OK;
  }

  nsRefPtr<AudioChild> mAudioChild;
  double mVolume;
};


class AudioMinWriteSizeEvent : public nsRunnable
{
 public:
  AudioMinWriteSizeEvent(AudioChild* aChild)
  {
    mAudioChild = aChild;
  }

  NS_IMETHOD Run()
  {
    if (!mAudioChild->IsIPCOpen())
      return NS_OK;

    mAudioChild->SendMinWriteSize();
    return NS_OK;
  }

  nsRefPtr<AudioChild> mAudioChild;
};

class AudioDrainEvent : public nsRunnable
{
 public:
  AudioDrainEvent(AudioChild* aChild)
  {
    mAudioChild = aChild;
  }

  NS_IMETHOD Run()
  {
    if (!mAudioChild->IsIPCOpen())
      return NS_OK;

    mAudioChild->SendDrain();
    return NS_OK;
  }

  nsRefPtr<AudioChild> mAudioChild;
};


class AudioPauseEvent : public nsRunnable
{
 public:
  AudioPauseEvent(AudioChild* aChild, bool pause)
  {
    mAudioChild = aChild;
    mPause = pause;
  }

  NS_IMETHOD Run()
  {
    if (!mAudioChild->IsIPCOpen())
      return NS_OK;

    if (mPause)
      mAudioChild->SendPause();
    else
      mAudioChild->SendResume();

    return NS_OK;
  }

  nsRefPtr<AudioChild> mAudioChild;
  bool mPause;
};


class AudioShutdownEvent : public nsRunnable
{
 public:
  AudioShutdownEvent(AudioChild* aChild)
  {
    mAudioChild = aChild;
  }

  NS_IMETHOD Run()
  {
    if (mAudioChild->IsIPCOpen())
      mAudioChild->SendShutdown();
    return NS_OK;
  }

  nsRefPtr<AudioChild> mAudioChild;
};
#endif

#define PREF_VOLUME_SCALE "media.volume_scale"
#define PREF_USE_CUBEB "media.use_cubeb"
#define PREF_CUBEB_LATENCY "media.cubeb_latency_ms"

static mozilla::Mutex* gAudioPrefsLock = nullptr;
static double gVolumeScale;
static bool gUseCubeb;
static PRUint32 gCubebLatency;

static int PrefChanged(const char* aPref, void* aClosure)
{
  if (strcmp(aPref, PREF_VOLUME_SCALE) == 0) {
    nsAdoptingString value = Preferences::GetString(aPref);
    mozilla::MutexAutoLock lock(*gAudioPrefsLock);
    if (value.IsEmpty()) {
      gVolumeScale = 1.0;
    } else {
      NS_ConvertUTF16toUTF8 utf8(value);
      gVolumeScale = NS_MAX<double>(0, PR_strtod(utf8.get(), nullptr));
    }
  } else if (strcmp(aPref, PREF_USE_CUBEB) == 0) {
    bool value = Preferences::GetBool(aPref, true);
    mozilla::MutexAutoLock lock(*gAudioPrefsLock);
    gUseCubeb = value;
  } else if (strcmp(aPref, PREF_CUBEB_LATENCY) == 0) {
    // Arbitrary default stream latency of 100ms.  The higher this
    // value, the longer stream volume changes will take to become
    // audible.
    PRUint32 value = Preferences::GetUint(aPref, 100);
    mozilla::MutexAutoLock lock(*gAudioPrefsLock);
    gCubebLatency = NS_MIN<PRUint32>(NS_MAX<PRUint32>(value, 20), 1000);
  }
  return 0;
}

static double GetVolumeScale()
{
  mozilla::MutexAutoLock lock(*gAudioPrefsLock);
  return gVolumeScale;
}

#if defined(MOZ_CUBEB)
static bool GetUseCubeb()
{
  mozilla::MutexAutoLock lock(*gAudioPrefsLock);
  return gUseCubeb;
}

static cubeb* gCubebContext;

static cubeb* GetCubebContext()
{
  mozilla::MutexAutoLock lock(*gAudioPrefsLock);
  if (gCubebContext ||
      cubeb_init(&gCubebContext, "nsAudioStream") == CUBEB_OK) {
    return gCubebContext;
  }
  NS_WARNING("cubeb_init failed");
  return nullptr;
}

static PRUint32 GetCubebLatency()
{
  mozilla::MutexAutoLock lock(*gAudioPrefsLock);
  return gCubebLatency;
}
#endif

void nsAudioStream::InitLibrary()
{
#ifdef PR_LOGGING
  gAudioStreamLog = PR_NewLogModule("nsAudioStream");
#endif
  gAudioPrefsLock = new mozilla::Mutex("nsAudioStream::gAudioPrefsLock");
  PrefChanged(PREF_VOLUME_SCALE, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_VOLUME_SCALE);
#if defined(MOZ_CUBEB)
  PrefChanged(PREF_USE_CUBEB, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_USE_CUBEB);
  PrefChanged(PREF_CUBEB_LATENCY, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_CUBEB_LATENCY);
#endif
}

void nsAudioStream::ShutdownLibrary()
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

nsIThread *
nsAudioStream::GetThread()
{
  if (!mAudioPlaybackThread) {
    NS_NewNamedThread("Audio Stream",
                      getter_AddRefs(mAudioPlaybackThread),
                      nullptr,
                      MEDIA_THREAD_STACK_SIZE);
  }
  return mAudioPlaybackThread;
}

class AsyncShutdownPlaybackThread : public nsRunnable
{
public:
  AsyncShutdownPlaybackThread(nsIThread* aThread) : mThread(aThread) {}
  NS_IMETHODIMP Run() { return mThread->Shutdown(); }
private:
  nsCOMPtr<nsIThread> mThread;
};

nsAudioStream::~nsAudioStream()
{
  if (mAudioPlaybackThread) {
    nsCOMPtr<nsIRunnable> event = new AsyncShutdownPlaybackThread(mAudioPlaybackThread);
    NS_DispatchToMainThread(event);
  }
}

nsNativeAudioStream::nsNativeAudioStream() :
  mVolume(1.0),
  mAudioHandle(0),
  mPaused(false),
  mInError(false)
{
}

nsNativeAudioStream::~nsNativeAudioStream()
{
  Shutdown();
}

NS_IMPL_THREADSAFE_ISUPPORTS0(nsNativeAudioStream)

nsresult nsNativeAudioStream::Init(PRInt32 aNumChannels, PRInt32 aRate, SampleFormat aFormat)
{
  mRate = aRate;
  mChannels = aNumChannels;
  mFormat = aFormat;

  if (sa_stream_create_pcm(reinterpret_cast<sa_stream_t**>(&mAudioHandle),
                           NULL,
                           SA_MODE_WRONLY,
                           SA_PCM_FORMAT_S16_NE,
                           aRate,
                           aNumChannels) != SA_SUCCESS) {
    mAudioHandle = nullptr;
    mInError = true;
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsNativeAudioStream: sa_stream_create_pcm error"));
    return NS_ERROR_FAILURE;
  }

  if (sa_stream_open(static_cast<sa_stream_t*>(mAudioHandle)) != SA_SUCCESS) {
    sa_stream_destroy(static_cast<sa_stream_t*>(mAudioHandle));
    mAudioHandle = nullptr;
    mInError = true;
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsNativeAudioStream: sa_stream_open error"));
    return NS_ERROR_FAILURE;
  }
  mInError = false;

  return NS_OK;
}

void nsNativeAudioStream::Shutdown()
{
  if (!mAudioHandle)
    return;

  sa_stream_destroy(static_cast<sa_stream_t*>(mAudioHandle));
  mAudioHandle = nullptr;
  mInError = true;
}

nsresult nsNativeAudioStream::Write(const void* aBuf, PRUint32 aFrames)
{
  NS_ASSERTION(!mPaused, "Don't write audio when paused, you'll block");

  if (mInError)
    return NS_ERROR_FAILURE;

  PRUint32 samples = aFrames * mChannels;
  nsAutoArrayPtr<short> s_data(new short[samples]);

  if (s_data) {
    double scaled_volume = GetVolumeScale() * mVolume;
    switch (mFormat) {
      case FORMAT_U8: {
        const PRUint8* buf = static_cast<const PRUint8*>(aBuf);
        PRInt32 volume = PRInt32((1 << 16) * scaled_volume);
        for (PRUint32 i = 0; i < samples; ++i) {
          s_data[i] = short(((PRInt32(buf[i]) - 128) * volume) >> 8);
        }
        break;
      }
      case FORMAT_S16_LE: {
        const short* buf = static_cast<const short*>(aBuf);
        PRInt32 volume = PRInt32((1 << 16) * scaled_volume);
        for (PRUint32 i = 0; i < samples; ++i) {
          short s = buf[i];
#if defined(IS_BIG_ENDIAN)
          s = ((s & 0x00ff) << 8) | ((s & 0xff00) >> 8);
#endif
          s_data[i] = short((PRInt32(s) * volume) >> 16);
        }
        break;
      }
      case FORMAT_FLOAT32: {
        const float* buf = static_cast<const float*>(aBuf);
        for (PRUint32 i = 0; i <  samples; ++i) {
          float scaled_value = floorf(0.5 + 32768 * buf[i] * scaled_volume);
          if (buf[i] < 0.0) {
            s_data[i] = (scaled_value < -32768.0) ?
              -32768 :
              short(scaled_value);
          } else {
            s_data[i] = (scaled_value > 32767.0) ?
              32767 :
              short(scaled_value);
          }
        }
        break;
      }
    }

    if (sa_stream_write(static_cast<sa_stream_t*>(mAudioHandle),
                        s_data.get(),
                        samples * sizeof(short)) != SA_SUCCESS)
    {
      PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsNativeAudioStream: sa_stream_write error"));
      mInError = true;
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

PRUint32 nsNativeAudioStream::Available()
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

void nsNativeAudioStream::SetVolume(double aVolume)
{
  NS_ASSERTION(aVolume >= 0.0 && aVolume <= 1.0, "Invalid volume");
#if defined(SA_PER_STREAM_VOLUME)
  if (sa_stream_set_volume_abs(static_cast<sa_stream_t*>(mAudioHandle), aVolume) != SA_SUCCESS) {
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsNativeAudioStream: sa_stream_set_volume_abs error"));
    mInError = true;
  }
#else
  mVolume = aVolume;
#endif
}

void nsNativeAudioStream::Drain()
{
  NS_ASSERTION(!mPaused, "Don't drain audio when paused, it won't finish!");

  if (mInError)
    return;

  int r = sa_stream_drain(static_cast<sa_stream_t*>(mAudioHandle));
  if (r != SA_SUCCESS && r != SA_ERROR_INVALID) {
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsNativeAudioStream: sa_stream_drain error"));
    mInError = true;
  }
}

void nsNativeAudioStream::Pause()
{
  if (mInError)
    return;
  mPaused = true;
  sa_stream_pause(static_cast<sa_stream_t*>(mAudioHandle));
}

void nsNativeAudioStream::Resume()
{
  if (mInError)
    return;
  mPaused = false;
  sa_stream_resume(static_cast<sa_stream_t*>(mAudioHandle));
}

PRInt64 nsNativeAudioStream::GetPosition()
{
  PRInt64 position = GetPositionInFrames();
  if (position >= 0) {
    return ((USECS_PER_S * position) / mRate);
  }
  return -1;
}

PRInt64 nsNativeAudioStream::GetPositionInFrames()
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

bool nsNativeAudioStream::IsPaused()
{
  return mPaused;
}

PRInt32 nsNativeAudioStream::GetMinWriteSize()
{
  size_t size;
  int r = sa_stream_get_min_write(static_cast<sa_stream_t*>(mAudioHandle),
                                  &size);
  if (r == SA_ERROR_NOT_SUPPORTED)
    return 1;
  else if (r != SA_SUCCESS || size > PR_INT32_MAX)
    return -1;

  return static_cast<PRInt32>(size / mChannels / sizeof(short));
}

#if defined(REMOTE_AUDIO)
nsRemotedAudioStream::nsRemotedAudioStream()
 : mAudioChild(nullptr),
   mBytesPerFrame(0),
   mPaused(false)
{}

nsRemotedAudioStream::~nsRemotedAudioStream()
{
  Shutdown();
}

NS_IMPL_THREADSAFE_ISUPPORTS0(nsRemotedAudioStream)

nsresult
nsRemotedAudioStream::Init(PRInt32 aNumChannels,
                           PRInt32 aRate,
                           SampleFormat aFormat)
{
  mRate = aRate;
  mChannels = aNumChannels;
  mFormat = aFormat;

  switch (mFormat) {
    case FORMAT_U8: {
      mBytesPerFrame = sizeof(PRUint8) * mChannels;
      break;
    }
    case FORMAT_S16_LE: {
      mBytesPerFrame = sizeof(short) * mChannels;
      break;
    }
    case FORMAT_FLOAT32: {
      mBytesPerFrame = sizeof(float) * mChannels;
    }
  }

  nsCOMPtr<nsIRunnable> event = new AudioInitEvent(this);
  NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
  return NS_OK;
}

void
nsRemotedAudioStream::Shutdown()
{
  if (!mAudioChild)
    return;
  nsCOMPtr<nsIRunnable> event = new AudioShutdownEvent(mAudioChild);
  NS_DispatchToMainThread(event);
  mAudioChild = nullptr;
}

nsresult
nsRemotedAudioStream::Write(const void* aBuf, PRUint32 aFrames)
{
  if (!mAudioChild)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIRunnable> event = new AudioWriteEvent(mAudioChild,
                                                    aBuf,
                                                    aFrames,
                                                    mBytesPerFrame);
  NS_DispatchToMainThread(event);
  mAudioChild->WaitForWrite();
  return NS_OK;
}

PRUint32
nsRemotedAudioStream::Available()
{
  return FAKE_BUFFER_SIZE;
}

PRInt32 nsRemotedAudioStream::GetMinWriteSize()
{
  if (!mAudioChild)
    return -1;
  nsCOMPtr<nsIRunnable> event = new AudioMinWriteSizeEvent(mAudioChild);
  NS_DispatchToMainThread(event);
  return mAudioChild->WaitForMinWriteSize();
}

void
nsRemotedAudioStream::SetVolume(double aVolume)
{
  if (!mAudioChild)
    return;
  nsCOMPtr<nsIRunnable> event = new AudioSetVolumeEvent(mAudioChild, aVolume);
  NS_DispatchToMainThread(event);
}

void
nsRemotedAudioStream::Drain()
{
  if (!mAudioChild)
    return;
  nsCOMPtr<nsIRunnable> event = new AudioDrainEvent(mAudioChild);
  NS_DispatchToMainThread(event);
  mAudioChild->WaitForDrain();
}

void
nsRemotedAudioStream::Pause()
{
  mPaused = true;
  if (!mAudioChild)
    return;
  nsCOMPtr<nsIRunnable> event = new AudioPauseEvent(mAudioChild, true);
  NS_DispatchToMainThread(event);
}

void
nsRemotedAudioStream::Resume()
{
  mPaused = false;
  if (!mAudioChild)
    return;
  nsCOMPtr<nsIRunnable> event = new AudioPauseEvent(mAudioChild, false);
  NS_DispatchToMainThread(event);
}

PRInt64 nsRemotedAudioStream::GetPosition()
{
  PRInt64 position = GetPositionInFrames();
  if (position >= 0) {
    return ((USECS_PER_S * position) / mRate);
  }
  return 0;
}

PRInt64
nsRemotedAudioStream::GetPositionInFrames()
{
  if(!mAudioChild)
    return 0;

  PRInt64 position = mAudioChild->GetLastKnownPosition();
  if (position == -1)
    return 0;

  PRInt64 time = mAudioChild->GetLastKnownPositionTimestamp();
  PRInt64 dt = PR_IntervalToMilliseconds(PR_IntervalNow() - time);

  return position + (mRate * dt / MS_PER_S);
}

bool
nsRemotedAudioStream::IsPaused()
{
  return mPaused;
}
#endif

#if defined(MOZ_CUBEB)
template <>
class nsAutoRefTraits<cubeb_stream> : public nsPointerRefTraits<cubeb_stream>
{
public:
  static void Release(cubeb_stream* aStream) { cubeb_stream_destroy(aStream); }
};

class nsCircularByteBuffer
{
public:
  nsCircularByteBuffer()
    : mBuffer(nullptr), mCapacity(0), mStart(0), mCount(0)
  {}

  // Set the capacity of the buffer in bytes.  Must be called before any
  // call to append or pop elements.
  void SetCapacity(PRUint32 aCapacity) {
    NS_ABORT_IF_FALSE(!mBuffer, "Buffer allocated.");
    mCapacity = aCapacity;
    mBuffer = new PRUint8[mCapacity];
  }

  PRUint32 Length() {
    return mCount;
  }

  PRUint32 Capacity() {
    return mCapacity;
  }

  PRUint32 Available() {
    return Capacity() - Length();
  }

  // Append aLength bytes from aSrc to the buffer.  Caller must check that
  // sufficient space is available.
  void AppendElements(const PRUint8* aSrc, PRUint32 aLength) {
    NS_ABORT_IF_FALSE(mBuffer && mCapacity, "Buffer not initialized.");
    NS_ABORT_IF_FALSE(aLength <= Available(), "Buffer full.");

    PRUint32 end = (mStart + mCount) % mCapacity;

    PRUint32 toCopy = NS_MIN(mCapacity - end, aLength);
    memcpy(&mBuffer[end], aSrc, toCopy);
    memcpy(&mBuffer[0], aSrc + toCopy, aLength - toCopy);
    mCount += aLength;
  }

  // Remove aSize bytes from the buffer.  Caller must check returned size in
  // aSize{1,2} before using the pointer returned in aData{1,2}.  Caller
  // must not specify an aSize larger than Length().
  void PopElements(PRUint32 aSize, void** aData1, PRUint32* aSize1,
                   void** aData2, PRUint32* aSize2) {
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
  nsAutoArrayPtr<PRUint8> mBuffer;
  PRUint32 mCapacity;
  PRUint32 mStart;
  PRUint32 mCount;
};

class nsBufferedAudioStream : public nsAudioStream
{
 public:
  NS_DECL_ISUPPORTS

  nsBufferedAudioStream();
  ~nsBufferedAudioStream();

  nsresult Init(PRInt32 aNumChannels, PRInt32 aRate, SampleFormat aFormat);
  void Shutdown();
  nsresult Write(const void* aBuf, PRUint32 aFrames);
  PRUint32 Available();
  void SetVolume(double aVolume);
  void Drain();
  void Pause();
  void Resume();
  PRInt64 GetPosition();
  PRInt64 GetPositionInFrames();
  bool IsPaused();
  PRInt32 GetMinWriteSize();

private:
  static long DataCallback_S(cubeb_stream*, void* aThis, void* aBuffer, long aFrames)
  {
    return static_cast<nsBufferedAudioStream*>(aThis)->DataCallback(aBuffer, aFrames);
  }

  static void StateCallback_S(cubeb_stream*, void* aThis, cubeb_state aState)
  {
    static_cast<nsBufferedAudioStream*>(aThis)->StateCallback(aState);
  }

  long DataCallback(void* aBuffer, long aFrames);
  void StateCallback(cubeb_state aState);

  // Shared implementation of underflow adjusted position calculation.
  // Caller must own the monitor.
  PRInt64 GetPositionInFramesUnlocked();

  // The monitor is held to protect all access to member variables.  Write()
  // waits while mBuffer is full; DataCallback() notifies as it consumes
  // data from mBuffer.  Drain() waits while mState is DRAINING;
  // StateCallback() notifies when mState is DRAINED.
  Monitor mMonitor;

  // Sum of silent frames written when DataCallback requests more frames
  // than are available in mBuffer.
  PRUint64 mLostFrames;

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

  PRUint32 mBytesPerFrame;

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

nsAudioStream* nsAudioStream::AllocateStream()
{
#if defined(REMOTE_AUDIO)
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    return new nsRemotedAudioStream();
  }
#endif
#if defined(MOZ_CUBEB)
  if (GetUseCubeb()) {
    return new nsBufferedAudioStream();
  }
#endif
  return new nsNativeAudioStream();
}

#if defined(MOZ_CUBEB)
nsBufferedAudioStream::nsBufferedAudioStream()
  : mMonitor("nsBufferedAudioStream"), mLostFrames(0), mVolume(1.0),
    mBytesPerFrame(0), mState(INITIALIZED)
{
}

nsBufferedAudioStream::~nsBufferedAudioStream()
{
  Shutdown();
}

NS_IMPL_THREADSAFE_ISUPPORTS0(nsBufferedAudioStream)

nsresult
nsBufferedAudioStream::Init(PRInt32 aNumChannels, PRInt32 aRate, SampleFormat aFormat)
{
  cubeb* cubebContext = GetCubebContext();

  if (!cubebContext || aNumChannels < 0 || aRate < 0) {
    return NS_ERROR_FAILURE;
  }

  mRate = aRate;
  mChannels = aNumChannels;
  mFormat = aFormat;

  cubeb_stream_params params;
  params.rate = aRate;
  params.channels = aNumChannels;
  switch (aFormat) {
  case FORMAT_S16_LE:
    params.format = CUBEB_SAMPLE_S16LE;
    mBytesPerFrame = sizeof(short) * aNumChannels;
    break;
  case FORMAT_FLOAT32:
    params.format = CUBEB_SAMPLE_FLOAT32NE;
    mBytesPerFrame = sizeof(float) * aNumChannels;
    break;
  default:
    return NS_ERROR_FAILURE;
  }

  {
    cubeb_stream* stream;
    if (cubeb_stream_init(cubebContext, &stream, "nsBufferedAudioStream", params,
                          GetCubebLatency(), DataCallback_S, StateCallback_S, this) == CUBEB_OK) {
      mCubebStream.own(stream);
    }
  }

  if (!mCubebStream) {
    return NS_ERROR_FAILURE;
  }

  // Size mBuffer for one second of audio.  This value is arbitrary, and was
  // selected based on the observed behaviour of the existing nsAudioStream
  // implementations.
  PRUint32 bufferLimit = aRate * mBytesPerFrame;
  NS_ABORT_IF_FALSE(bufferLimit % mBytesPerFrame == 0, "Must buffer complete frames");
  mBuffer.SetCapacity(bufferLimit);

  return NS_OK;
}

void
nsBufferedAudioStream::Shutdown()
{
  if (mState == STARTED) {
    Pause();
  }
  if (mCubebStream) {
    mCubebStream.reset();
  }
}

nsresult
nsBufferedAudioStream::Write(const void* aBuf, PRUint32 aFrames)
{
  MonitorAutoLock mon(mMonitor);
  if (!mCubebStream || mState == ERRORED) {
    return NS_ERROR_FAILURE;
  }
  NS_ASSERTION(mState == INITIALIZED || mState == STARTED, "Stream write in unexpected state.");

  const PRUint8* src = static_cast<const PRUint8*>(aBuf);
  PRUint32 bytesToCopy = aFrames * mBytesPerFrame;

  while (bytesToCopy > 0) {
    PRUint32 available = NS_MIN(bytesToCopy, mBuffer.Available());
    NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0, "Must copy complete frames.");

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

PRUint32
nsBufferedAudioStream::Available()
{
  MonitorAutoLock mon(mMonitor);
  NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Buffer invariant violated.");
  return mBuffer.Available() / mBytesPerFrame;
}

PRInt32
nsBufferedAudioStream::GetMinWriteSize()
{
  return 1;
}

void
nsBufferedAudioStream::SetVolume(double aVolume)
{
  MonitorAutoLock mon(mMonitor);
  NS_ABORT_IF_FALSE(aVolume >= 0.0 && aVolume <= 1.0, "Invalid volume");
  mVolume = aVolume;
}

void
nsBufferedAudioStream::Drain()
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
nsBufferedAudioStream::Pause()
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
nsBufferedAudioStream::Resume()
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

PRInt64
nsBufferedAudioStream::GetPosition()
{
  MonitorAutoLock mon(mMonitor);
  PRInt64 frames = GetPositionInFramesUnlocked();
  if (frames >= 0) {
    return USECS_PER_S * frames / mRate;
  }
  return -1;
}

// This function is miscompiled by PGO with MSVC 2010.  See bug 768333.
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
PRInt64
nsBufferedAudioStream::GetPositionInFrames()
{
  MonitorAutoLock mon(mMonitor);
  return GetPositionInFramesUnlocked();
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

PRInt64
nsBufferedAudioStream::GetPositionInFramesUnlocked()
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
  PRUint64 adjustedPosition = 0;
  if (position >= mLostFrames) {
    adjustedPosition = position - mLostFrames;
  }
  return NS_MIN<PRUint64>(adjustedPosition, PR_INT64_MAX);
}

bool
nsBufferedAudioStream::IsPaused()
{
  MonitorAutoLock mon(mMonitor);
  return mState == STOPPED;
}

long
nsBufferedAudioStream::DataCallback(void* aBuffer, long aFrames)
{
  MonitorAutoLock mon(mMonitor);
  PRUint32 bytesWanted = aFrames * mBytesPerFrame;

  // Adjust bytesWanted to fit what is available in mBuffer.
  PRUint32 available = NS_MIN(bytesWanted, mBuffer.Length());
  NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0, "Must copy complete frames");

  if (available > 0) {
    // Copy each sample from mBuffer to aBuffer, adjusting the volume during the copy.
    double scaled_volume = GetVolumeScale() * mVolume;

    // Fetch input pointers from the ring buffer.
    void* input[2];
    PRUint32 input_size[2];
    mBuffer.PopElements(available, &input[0], &input_size[0], &input[1], &input_size[1]);

    PRUint8* output = reinterpret_cast<PRUint8*>(aBuffer);
    for (int i = 0; i < 2; ++i) {
      // Fast path for unity volume case.
      if (scaled_volume == 1.0) {
        memcpy(output, input[i], input_size[i]);
        output += input_size[i];
      } else {
        // Adjust volume as each sample is copied out.
        switch (mFormat) {
        case FORMAT_S16_LE: {
          PRInt32 volume = PRInt32(1 << 16) * scaled_volume;

          const short* src = static_cast<const short*>(input[i]);
          short* dst = reinterpret_cast<short*>(output);
          for (PRUint32 j = 0; j < input_size[i] / (mBytesPerFrame / mChannels); ++j) {
            dst[j] = short((PRInt32(src[j]) * volume) >> 16);
          }
          output += input_size[i];
          break;
        }
        case FORMAT_FLOAT32: {
          const float* src = static_cast<const float*>(input[i]);
          float* dst = reinterpret_cast<float*>(output);
          for (PRUint32 j = 0; j < input_size[i] / (mBytesPerFrame / mChannels); ++j) {
            dst[j] = src[j] * scaled_volume;
          }
          output += input_size[i];
          break;
        }
        default:
          return -1;
        }
      }
    }

    NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Must copy complete frames");

    // Notify any blocked Write() call that more space is available in mBuffer.
    mon.NotifyAll();

    // Calculate remaining bytes requested by caller.  If the stream is not
    // draining an underrun has occurred, so fill the remaining buffer with
    // silence.
    bytesWanted -= available;
  }

  if (mState != DRAINING) {
    memset(static_cast<PRUint8*>(aBuffer) + available, 0, bytesWanted);
    mLostFrames += bytesWanted / mBytesPerFrame;
    bytesWanted = 0;
  }

  return aFrames - (bytesWanted / mBytesPerFrame);
}

void
nsBufferedAudioStream::StateCallback(cubeb_state aState)
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

