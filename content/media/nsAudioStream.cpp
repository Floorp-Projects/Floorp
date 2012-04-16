/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Double <chris.double@double.co.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "mozilla/TimeStamp.h"
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

using mozilla::TimeStamp;

#ifdef PR_LOGGING
PRLogModuleInfo* gAudioStreamLog = nsnull;
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

static mozilla::Mutex* gAudioPrefsLock = nsnull;
static double gVolumeScale = 1.0;
static bool gUseCubeb = false;

static int PrefChanged(const char* aPref, void* aClosure)
{
  if (strcmp(aPref, PREF_VOLUME_SCALE) == 0) {
    nsAdoptingString value = Preferences::GetString(aPref);
    mozilla::MutexAutoLock lock(*gAudioPrefsLock);
    if (value.IsEmpty()) {
      gVolumeScale = 1.0;
    } else {
      NS_ConvertUTF16toUTF8 utf8(value);
      gVolumeScale = NS_MAX<double>(0, PR_strtod(utf8.get(), nsnull));
    }
  } else if (strcmp(aPref, PREF_USE_CUBEB) == 0) {
    bool value = Preferences::GetBool(aPref, true);
    mozilla::MutexAutoLock lock(*gAudioPrefsLock);
    gUseCubeb = value;
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
  return nsnull;
}
#endif

void nsAudioStream::InitLibrary()
{
#ifdef PR_LOGGING
  gAudioStreamLog = PR_NewLogModule("nsAudioStream");
#endif
  gAudioPrefsLock = new mozilla::Mutex("nsAudioStream::gAudioPrefsLock");
  PrefChanged(PREF_VOLUME_SCALE, nsnull);
  Preferences::RegisterCallback(PrefChanged, PREF_VOLUME_SCALE);
#if defined(MOZ_CUBEB)
  PrefChanged(PREF_USE_CUBEB, nsnull);
  Preferences::RegisterCallback(PrefChanged, PREF_USE_CUBEB);
#endif
}

void nsAudioStream::ShutdownLibrary()
{
  Preferences::UnregisterCallback(PrefChanged, PREF_VOLUME_SCALE);
#if defined(MOZ_CUBEB)
  Preferences::UnregisterCallback(PrefChanged, PREF_USE_CUBEB);
#endif
  delete gAudioPrefsLock;
  gAudioPrefsLock = nsnull;

#if defined(MOZ_CUBEB)
  if (gCubebContext) {
    cubeb_destroy(gCubebContext);
    gCubebContext = nsnull;
  }
#endif
}

nsIThread *
nsAudioStream::GetThread()
{
  if (!mAudioPlaybackThread) {
    NS_NewThread(getter_AddRefs(mAudioPlaybackThread),
                 nsnull,
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
    mAudioHandle = nsnull;
    mInError = true;
    PR_LOG(gAudioStreamLog, PR_LOG_ERROR, ("nsNativeAudioStream: sa_stream_create_pcm error"));
    return NS_ERROR_FAILURE;
  }

  if (sa_stream_open(static_cast<sa_stream_t*>(mAudioHandle)) != SA_SUCCESS) {
    sa_stream_destroy(static_cast<sa_stream_t*>(mAudioHandle));
    mAudioHandle = nsnull;
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
  mAudioHandle = nsnull;
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
 : mAudioChild(nsnull),
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
  mAudioChild = nsnull;
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

  static int StateCallback_S(cubeb_stream*, void* aThis, cubeb_state aState)
  {
    return static_cast<nsBufferedAudioStream*>(aThis)->StateCallback(aState);
  }

  long DataCallback(void* aBuffer, long aFrames);
  int StateCallback(cubeb_state aState);

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
  // DataCallback().  Once mBufferLimit is reached, Write() blocks until
  // sufficient space becomes available in mBuffer.  The buffer and buffer
  // limit deal in bytes, not frames.
  nsTArray<PRUint8> mBuffer;
  PRUint32 mBufferLimit;

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

  // Arbitrary default stream latency.  The higher this value, the longer stream
  // volume changes will take to become audible.
  static const unsigned int DEFAULT_LATENCY_MS = 100;
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
    params.format = CUBEB_SAMPLE_FLOAT32LE;
    mBytesPerFrame = sizeof(float) * aNumChannels;
    break;
  default:
    return NS_ERROR_FAILURE;
  }

  {
    cubeb_stream* stream;
    if (cubeb_stream_init(cubebContext, &stream, "nsBufferedAudioStream", params,
                          DEFAULT_LATENCY_MS, DataCallback_S, StateCallback_S, this) == CUBEB_OK) {
      mCubebStream.own(stream);
    }
  }

  if (!mCubebStream) {
    return NS_ERROR_FAILURE;
  }

  // Limit mBuffer to one second of audio.  This value is arbitrary, and was
  // selected based on the observed behaviour of the existing nsAudioStream
  // implementations.
  mBufferLimit = aRate * mBytesPerFrame;
  NS_ABORT_IF_FALSE(mBufferLimit % mBytesPerFrame == 0, "Must buffer complete frames");

  // Pre-allocate the buffer.  nsTArray::RemoveElementsAt shrinks the buffer
  // only if its length reaches zero, so allocator thrashing should be
  // minimal.
  mBuffer.SetCapacity(mBufferLimit);

  return NS_OK;
}

void
nsBufferedAudioStream::Shutdown()
{
  if (mCubebStream) {
    cubeb_stream_stop(mCubebStream);
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
    NS_ABORT_IF_FALSE(mBuffer.Length() <= mBufferLimit, "Buffer invariant violated.");

    PRUint32 available = NS_MIN(bytesToCopy, mBufferLimit - mBuffer.Length());
    NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0, "Must copy complete frames.");

    mBuffer.AppendElements(src, available);
    src += available;
    bytesToCopy -= available;

    if (mState != STARTED && cubeb_stream_start(mCubebStream) == CUBEB_OK) {
      mState = STARTED;
    }

    if (mState == STARTED && bytesToCopy > 0) {
      mon.Wait();
    }

    if (mState != STARTED) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

PRUint32
nsBufferedAudioStream::Available()
{
  MonitorAutoLock mon(mMonitor);
  NS_ABORT_IF_FALSE(mBuffer.Length() <= mBufferLimit, "Buffer invariant violated.");
  NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Buffer invariant violated.");
  return (mBufferLimit - mBuffer.Length()) / mBytesPerFrame;
}

PRInt32 nsBufferedAudioStream::GetMinWriteSize()
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

  if (cubeb_stream_stop(mCubebStream) == CUBEB_OK) {
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

  if (cubeb_stream_start(mCubebStream) == CUBEB_OK) {
    mState = STARTED;
  }
}

PRInt64 nsBufferedAudioStream::GetPosition()
{
  MonitorAutoLock mon(mMonitor);
  PRInt64 frames = GetPositionInFramesUnlocked();
  if (frames >= 0) {
    return USECS_PER_S * frames / mRate;
  }
  return -1;
}

PRInt64
nsBufferedAudioStream::GetPositionInFrames()
{
  MonitorAutoLock mon(mMonitor);
  return GetPositionInFramesUnlocked();
}

PRInt64
nsBufferedAudioStream::GetPositionInFramesUnlocked()
{
  mMonitor.AssertCurrentThreadOwns();

  if (!mCubebStream || mState == ERRORED) {
    return -1;
  }

  uint64_t position = 0;
  if (cubeb_stream_get_position(mCubebStream, &position) != CUBEB_OK) {
    return -1;
  }

  // Adjust the reported position by the number of silent frames written
  // during stream underruns.
  PRInt64 adjustedPosition = 0;
  if (position >= mLostFrames) {
    adjustedPosition = position - mLostFrames;
  }
  return adjustedPosition;
}

bool
nsBufferedAudioStream::IsPaused()
{
  MonitorAutoLock mon(mMonitor);
  return mState == STOPPED;
}

template<typename T>
void
SampleCopy(void* aDst, const PRUint8* aSrc, PRUint32 aSamples, double aVolume)
{
  const T* src = reinterpret_cast<const T*>(aSrc);
  double scaled_volume = GetVolumeScale() * aVolume;
  T* dst = static_cast<T*>(aDst);
  for (PRUint32 i = 0; i < aSamples; ++i) {
    dst[i] = T(src[i] * scaled_volume);
  }
}

long
nsBufferedAudioStream::DataCallback(void* aBuffer, long aFrames)
{
  MonitorAutoLock mon(mMonitor);
  PRUint32 bytesWanted = aFrames * mBytesPerFrame;

  // Adjust bytesWanted to fit what is available in mBuffer.
  PRUint32 available = NS_MIN(bytesWanted, mBuffer.Length());
  NS_ABORT_IF_FALSE(available % mBytesPerFrame == 0, "Must copy complete frames");

  // Copy each sample from mBuffer to aBuffer, adjusting the volume during the copy.
  PRUint32 samplesToCopy = available / mBytesPerFrame * mChannels;
  switch (mFormat) {
  case FORMAT_S16_LE:
    SampleCopy<PRInt16>(aBuffer, mBuffer.Elements(), samplesToCopy, mVolume);
    break;
  case FORMAT_FLOAT32:
    SampleCopy<float>(aBuffer, mBuffer.Elements(), samplesToCopy, mVolume);
    break;
  default:
    return -1;
  }

  // Remove copied data from the temporary audio buffer.
  mBuffer.RemoveElementsAt(0, available);
  NS_ABORT_IF_FALSE(mBuffer.Length() % mBytesPerFrame == 0, "Must copy complete frames");

  // Notify any blocked Write() call that more space is available in mBuffer.
  mon.NotifyAll();

  // Calculate remaining bytes requested by caller.  If the stream is not
  // draining an underrun has occurred, so fill the remaining buffer with
  // silence.
  bytesWanted -= available;
  if (mState != DRAINING) {
    memset(static_cast<PRUint8*>(aBuffer) + available, 0, bytesWanted);
    mLostFrames += bytesWanted / mBytesPerFrame;
    bytesWanted = 0;
  }

  return aFrames - (bytesWanted / mBytesPerFrame);
}

int
nsBufferedAudioStream::StateCallback(cubeb_state aState)
{
  if (aState == CUBEB_STATE_DRAINED) {
    MonitorAutoLock mon(mMonitor);
    mState = DRAINED;
    mon.NotifyAll();
  } else if (aState == CUBEB_STATE_ERROR) {
    MonitorAutoLock mon(mMonitor);
    mState = ERRORED;
    mon.NotifyAll();
  }
  return CUBEB_OK;
}
#endif

