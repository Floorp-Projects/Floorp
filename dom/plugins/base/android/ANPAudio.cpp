/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "AndroidBridge.h"

#include <android/log.h>
#include <stdlib.h>
#include <time.h>

#include "assert.h"
#include "ANPBase.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "mozilla/Mutex.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPluginsAudio" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_audio_##name

/* android.media.AudioTrack */
struct AudioTrack {
  jclass    at_class;
  jmethodID constructor;
  jmethodID flush;
  jmethodID pause;
  jmethodID play;
  jmethodID setvol;
  jmethodID stop;
  jmethodID write;
  jmethodID getpos;
  jmethodID getstate;
  jmethodID release;
};

enum AudioTrackMode {
  MODE_STATIC = 0,
  MODE_STREAM = 1
};

/* android.media.AudioManager */
enum AudioManagerStream {
  STREAM_VOICE_CALL = 0,
  STREAM_SYSTEM = 1,
  STREAM_RING = 2,
  STREAM_MUSIC = 3,
  STREAM_ALARM = 4,
  STREAM_NOTIFICATION = 5,
  STREAM_DTMF = 8
};

/* android.media.AudioFormat */
enum AudioFormatChannel {
  CHANNEL_OUT_MONO = 4,
  CHANNEL_OUT_STEREO = 12
};

enum AudioFormatEncoding {
  ENCODING_PCM_16BIT = 2,
  ENCODING_PCM_8BIT = 3
};

enum AudioFormatState {
  STATE_UNINITIALIZED = 0,
  STATE_INITIALIZED = 1,
  STATE_NO_STATIC_DATA = 2
};

static struct AudioTrack at;

static jclass
init_jni_bindings(JNIEnv *jenv) {
  jclass jc =
    (jclass)jenv->NewGlobalRef(jenv->FindClass("android/media/AudioTrack"));

  at.constructor = jenv->GetMethodID(jc, "<init>", "(IIIIII)V");
  at.flush       = jenv->GetMethodID(jc, "flush", "()V");
  at.pause       = jenv->GetMethodID(jc, "pause", "()V");
  at.play        = jenv->GetMethodID(jc, "play",  "()V");
  at.setvol      = jenv->GetMethodID(jc, "setStereoVolume",  "(FF)I");
  at.stop        = jenv->GetMethodID(jc, "stop",  "()V");
  at.write       = jenv->GetMethodID(jc, "write", "([BII)I");
  at.getpos      = jenv->GetMethodID(jc, "getPlaybackHeadPosition", "()I");
  at.getstate    = jenv->GetMethodID(jc, "getState", "()I");
  at.release     = jenv->GetMethodID(jc, "release", "()V");

  return jc;
}

struct ANPAudioTrack {
  jobject output_unit;
  jclass at_class;

  unsigned int rate;
  unsigned int channels;
  unsigned int bufferSize;
  unsigned int isStopped;
  unsigned int keepGoing;

  mozilla::Mutex lock;

  void* user;
  ANPAudioCallbackProc proc;
  ANPSampleFormat format;

  ANPAudioTrack() : lock("ANPAudioTrack") { }
};

class AudioRunnable : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  AudioRunnable(ANPAudioTrack* aAudioTrack) {
    mTrack = aAudioTrack;
  }

  ANPAudioTrack* mTrack;
};

NS_IMETHODIMP
AudioRunnable::Run()
{
  PR_SetCurrentThreadName("Android Audio");

  JNIEnv* jenv = GetJNIForThread();

  mozilla::AutoLocalJNIFrame autoFrame(jenv, 2);

  jbyteArray bytearray = jenv->NewByteArray(mTrack->bufferSize);
  if (!bytearray) {
    LOG("AudioRunnable:: Run.  Could not create bytearray");
    return NS_ERROR_FAILURE;
  }

  jbyte *byte = jenv->GetByteArrayElements(bytearray, nullptr);
  if (!byte) {
    LOG("AudioRunnable:: Run.  Could not create bytearray");
    return NS_ERROR_FAILURE;
  }

  ANPAudioBuffer buffer;
  buffer.channelCount = mTrack->channels;
  buffer.format = mTrack->format;
  buffer.bufferData = (void*) byte;

  while (true)
  {
    // reset the buffer size
    buffer.size = mTrack->bufferSize;
    
    {
      mozilla::MutexAutoLock lock(mTrack->lock);

      if (!mTrack->keepGoing)
        break;

      // Get data from the plugin
      mTrack->proc(kMoreData_ANPAudioEvent, mTrack->user, &buffer);
    }

    if (buffer.size == 0) {
      LOG("%p - kMoreData_ANPAudioEvent", mTrack);
      continue;
    }

    size_t wroteSoFar = 0;
    jint retval;
    do {
      retval = jenv->CallIntMethod(mTrack->output_unit,
                                   at.write,
                                   bytearray,
                                   wroteSoFar,
                                   buffer.size - wroteSoFar);
      if (retval < 0) {
        LOG("%p - Write failed %d", mTrack, retval);
        break;
      }

      wroteSoFar += retval;

    } while(wroteSoFar < buffer.size);
  }

  jenv->CallVoidMethod(mTrack->output_unit, at.release);

  jenv->DeleteGlobalRef(mTrack->output_unit);
  jenv->DeleteGlobalRef(mTrack->at_class);

  delete mTrack;
  
  jenv->ReleaseByteArrayElements(bytearray, byte, 0);

  return NS_OK;
}

ANPAudioTrack*
anp_audio_newTrack(uint32_t sampleRate,    // sampling rate in Hz
                   ANPSampleFormat format,
                   int channelCount,       // MONO=1, STEREO=2
                   ANPAudioCallbackProc proc,
                   void* user)
{
  ANPAudioTrack *s = new ANPAudioTrack();
  if (s == nullptr) {
    return nullptr;
  }

  JNIEnv *jenv = GetJNIForThread();

  s->at_class = init_jni_bindings(jenv);
  s->rate = sampleRate;
  s->channels = channelCount;
  s->bufferSize = s->rate * s->channels;
  s->isStopped = true;
  s->keepGoing = false;
  s->user = user;
  s->proc = proc;
  s->format = format;

  int jformat;
  switch (format) {
  case kPCM16Bit_ANPSampleFormat:
    jformat = ENCODING_PCM_16BIT;
    break;
  case kPCM8Bit_ANPSampleFormat:
    jformat = ENCODING_PCM_8BIT;
    break;
  default:
    LOG("Unknown audio format.  defaulting to 16bit.");
    jformat = ENCODING_PCM_16BIT;
    break;
  }

  int jChannels;
  switch (channelCount) {
  case 1:
    jChannels = CHANNEL_OUT_MONO;
    break;
  case 2:
    jChannels = CHANNEL_OUT_STEREO;
    break;
  default:
    LOG("Unknown channel count.  defaulting to mono.");
    jChannels = CHANNEL_OUT_MONO;
    break;
  }

  mozilla::AutoLocalJNIFrame autoFrame(jenv);

  jobject obj = jenv->NewObject(s->at_class,
                                at.constructor,
                                STREAM_MUSIC,
                                s->rate,
                                jChannels,
                                jformat,
                                s->bufferSize,
                                MODE_STREAM);

  if (autoFrame.CheckForException() || obj == nullptr) {
    jenv->DeleteGlobalRef(s->at_class);
    free(s);
    return nullptr;
  }

  jint state = jenv->CallIntMethod(obj, at.getstate);

  if (autoFrame.CheckForException() || state == STATE_UNINITIALIZED) {
    jenv->DeleteGlobalRef(s->at_class);
    free(s);
    return nullptr;
  }

  s->output_unit = jenv->NewGlobalRef(obj);
  return s;
}

void
anp_audio_deleteTrack(ANPAudioTrack* s)
{
  if (s == nullptr) {
    return;
  }

  mozilla::MutexAutoLock lock(s->lock);
  s->keepGoing = false;

  // deallocation happens in the AudioThread.  There is a
  // potential leak if anp_audio_start is never called, but
  // we do not see that from flash.
}

void
anp_audio_start(ANPAudioTrack* s)
{
  if (s == nullptr || s->output_unit == nullptr) {
    return;
  }

  if (s->keepGoing) {
    // we are already playing.  Ignore.
    return;
  }

  JNIEnv *jenv = GetJNIForThread();

  mozilla::AutoLocalJNIFrame autoFrame(jenv, 0);
  jenv->CallVoidMethod(s->output_unit, at.play);

  if (autoFrame.CheckForException()) {
    jenv->DeleteGlobalRef(s->at_class);
    free(s);
    return;
  }

  s->isStopped = false;
  s->keepGoing = true;

  // AudioRunnable now owns the ANPAudioTrack
  nsRefPtr<AudioRunnable> runnable = new AudioRunnable(s);

  nsCOMPtr<nsIThread> thread;
  NS_NewThread(getter_AddRefs(thread), runnable);
}

void
anp_audio_pause(ANPAudioTrack* s)
{
  if (s == nullptr || s->output_unit == nullptr) {
    return;
  }

  JNIEnv *jenv = GetJNIForThread();

  mozilla::AutoLocalJNIFrame autoFrame(jenv, 0);
  jenv->CallVoidMethod(s->output_unit, at.pause);
}

void
anp_audio_stop(ANPAudioTrack* s)
{
  if (s == nullptr || s->output_unit == nullptr) {
    return;
  }

  s->isStopped = true;
  JNIEnv *jenv = GetJNIForThread();

  mozilla::AutoLocalJNIFrame autoFrame(jenv, 0);
  jenv->CallVoidMethod(s->output_unit, at.stop);
}

bool
anp_audio_isStopped(ANPAudioTrack* s)
{
  return s->isStopped;
}

uint32_t
anp_audio_trackLatency(ANPAudioTrack* s) {
  // Hardcode an estimate of the system's audio latency. Flash hardcodes
  // similar latency estimates for pre-Honeycomb devices that do not support
  // ANPAudioTrackInterfaceV1's trackLatency(). The Android stock browser
  // calls android::AudioTrack::latency(), an internal Android API that is
  // not available in the public NDK:
  // https://github.com/android/platform_external_webkit/commit/49bf866973cb3b2a6c74c0eab864e9562e4cbab1
  return 100; // milliseconds
}

void InitAudioTrackInterfaceV0(ANPAudioTrackInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, newTrack);
  ASSIGN(i, deleteTrack);
  ASSIGN(i, start);
  ASSIGN(i, pause);
  ASSIGN(i, stop);
  ASSIGN(i, isStopped);
}

void InitAudioTrackInterfaceV1(ANPAudioTrackInterfaceV1 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, newTrack);
  ASSIGN(i, deleteTrack);
  ASSIGN(i, start);
  ASSIGN(i, pause);
  ASSIGN(i, stop);
  ASSIGN(i, isStopped);
  ASSIGN(i, trackLatency);
}
