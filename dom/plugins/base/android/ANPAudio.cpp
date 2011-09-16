#include "base/basictypes.h"
#include "AndroidBridge.h"

#include <android/log.h>
#include <stdlib.h>
#include <time.h>

#include "assert.h"
#include "ANPBase.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

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

static struct AudioTrack at;

static jclass
init_jni_bindings(JNIEnv *jenv) {
  jclass jc = jenv->FindClass("android/media/AudioTrack");

  at.constructor = jenv->GetMethodID(jc, "<init>", "(IIIIII)V");
  at.flush       = jenv->GetMethodID(jc, "flush", "()V");
  at.pause       = jenv->GetMethodID(jc, "pause", "()V");
  at.play        = jenv->GetMethodID(jc, "play",  "()V");
  at.setvol      = jenv->GetMethodID(jc, "setStereoVolume",  "(FF)I");
  at.stop        = jenv->GetMethodID(jc, "stop",  "()V");
  at.write       = jenv->GetMethodID(jc, "write", "([BII)I");
  at.getpos      = jenv->GetMethodID(jc, "getPlaybackHeadPosition", "()I");

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

  void* user;
  ANPAudioCallbackProc proc;
  ANPSampleFormat format;
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
  JNIEnv* jenv = GetJNIForThread();

  if (jenv->PushLocalFrame(128)) {
    return NS_ERROR_FAILURE;
  }

  jbyteArray bytearray = jenv->NewByteArray(mTrack->bufferSize);
  if (!bytearray) {
    LOG("AudioRunnable:: Run.  Could not create bytearray");
    return NS_ERROR_FAILURE;
  }

  jbyte *byte = jenv->GetByteArrayElements(bytearray, NULL);
  if (!byte) {
    LOG("AudioRunnable:: Run.  Could not create bytearray");
    return NS_ERROR_FAILURE;
  }

  ANPAudioBuffer buffer;
  buffer.channelCount = mTrack->channels;
  buffer.format = mTrack->format;
  buffer.bufferData = (void*) byte;

  while (mTrack->keepGoing)
  {
    // reset the buffer size
    buffer.size = mTrack->bufferSize;

    // Get data from the plugin
    mTrack->proc(kMoreData_ANPAudioEvent, mTrack->user, &buffer);

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

  jenv->DeleteGlobalRef(mTrack->output_unit);
  jenv->DeleteGlobalRef(mTrack->at_class);

  free(mTrack);

  jenv->ReleaseByteArrayElements(bytearray, byte, 0);
  jenv->PopLocalFrame(NULL);

  return NS_OK;
}

ANPAudioTrack*
anp_audio_newTrack(uint32_t sampleRate,    // sampling rate in Hz
                   ANPSampleFormat format,
                   int channelCount,       // MONO=1, STEREO=2
                   ANPAudioCallbackProc proc,
                   void* user)
{
  ANPAudioTrack *s = (ANPAudioTrack*) malloc(sizeof(ANPAudioTrack));
  if (s == NULL) {
    return NULL;
  }

  JNIEnv *jenv = GetJNIForThread();
  if (!jenv)
    return NULL;

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

  jobject obj = jenv->NewObject(s->at_class,
                                at.constructor,
                                STREAM_MUSIC,
                                s->rate,
                                jChannels,
                                jformat,
                                s->bufferSize,
                                MODE_STREAM);

  jthrowable exception = jenv->ExceptionOccurred();
  if (exception) {
    LOG("%s fAILED  ", __PRETTY_FUNCTION__);
    jenv->ExceptionDescribe();
    jenv->ExceptionClear();
    jenv->DeleteGlobalRef(s->at_class);
    free(s);
    return NULL;
  }

  s->output_unit = jenv->NewGlobalRef(obj);
  return s;
}

void
anp_audio_deleteTrack(ANPAudioTrack* s)
{
  if (s == NULL) {
    return;
  }

  s->keepGoing = false;

  // deallocation happens in the AudioThread.  There is a
  // potential leak if anp_audio_start is never called, but
  // we do not see that from flash.
}

void
anp_audio_start(ANPAudioTrack* s)
{
  if (s == NULL || s->output_unit == NULL) {
    return;
  }
  
  if (s->keepGoing) {
    // we are already playing.  Ignore.
    LOG("anp_audio_start called twice!");
    return;
  }

  JNIEnv *jenv = GetJNIForThread();
  jenv->CallVoidMethod(s->output_unit, at.play);

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
  if (s == NULL || s->output_unit == NULL) {
    return;
  }

  JNIEnv *jenv = GetJNIForThread();
  jenv->CallVoidMethod(s->output_unit, at.pause);
}

void
anp_audio_stop(ANPAudioTrack* s)
{
  if (s == NULL || s->output_unit == NULL) {
    return;
  }

  s->isStopped = true;
  JNIEnv *jenv = GetJNIForThread();
  jenv->CallVoidMethod(s->output_unit, at.stop);
}

bool
anp_audio_isStopped(ANPAudioTrack* s)
{
  return s->isStopped;
}

void InitAudioTrackInterface(ANPAudioTrackInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, newTrack);
  ASSIGN(i, deleteTrack);
  ASSIGN(i, start);
  ASSIGN(i, pause);
  ASSIGN(i, stop);
  ASSIGN(i, isStopped);
}
