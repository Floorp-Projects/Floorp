/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <algorithm>
#include "nsIStringBundle.h"
#include "nsDebug.h"
#include "nsString.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Logging.h"
#include "nsThreadUtils.h"
#include "CubebUtils.h"
#include "nsAutoRef.h"
#include "prdtoa.h"

#define PREF_VOLUME_SCALE "media.volume_scale"
#define PREF_CUBEB_LATENCY_PLAYBACK "media.cubeb_latency_playback_ms"
#define PREF_CUBEB_LATENCY_MSG "media.cubeb_latency_msg_frames"

namespace mozilla {

namespace {

LazyLogModule gCubebLog("cubeb");

void CubebLogCallback(const char* aFmt, ...)
{
  char buffer[256];

  va_list arglist;
  va_start(arglist, aFmt);
  VsprintfLiteral (buffer, aFmt, arglist);
  MOZ_LOG(gCubebLog, LogLevel::Error, ("%s", buffer));
  va_end(arglist);
}

// This mutex protects the variables below.
StaticMutex sMutex;
enum class CubebState {
  Uninitialized = 0,
  Initialized,
  Shutdown
} sCubebState = CubebState::Uninitialized;
cubeb* sCubebContext;
double sVolumeScale;
uint32_t sCubebPlaybackLatencyInMilliseconds;
uint32_t sCubebMSGLatencyInFrames;
bool sCubebPlaybackLatencyPrefSet;
bool sCubebMSGLatencyPrefSet;
bool sAudioStreamInitEverSucceeded = false;
StaticAutoPtr<char> sBrandName;

const char kBrandBundleURL[]      = "chrome://branding/locale/brand.properties";

const char* AUDIOSTREAM_BACKEND_ID_STR[] = {
  "jack",
  "pulse",
  "alsa",
  "audiounit",
  "audioqueue",
  "wasapi",
  "winmm",
  "directsound",
  "sndio",
  "opensl",
  "audiotrack",
  "kai"
};
/* Index for failures to create an audio stream the first time. */
const int CUBEB_BACKEND_INIT_FAILURE_FIRST =
  ArrayLength(AUDIOSTREAM_BACKEND_ID_STR);
/* Index for failures to create an audio stream after the first time */
const int CUBEB_BACKEND_INIT_FAILURE_OTHER = CUBEB_BACKEND_INIT_FAILURE_FIRST + 1;
/* Index for an unknown backend. */
const int CUBEB_BACKEND_UNKNOWN = CUBEB_BACKEND_INIT_FAILURE_FIRST + 2;


// Prefered samplerate, in Hz (characteristic of the hardware, mixer, platform,
// and API used).
//
// sMutex protects *initialization* of this, which must be performed from each
// thread before fetching, after which it is safe to fetch without holding the
// mutex because it is only written once per process execution (by the first
// initialization to complete).  Since the init must have been called on a
// given thread before fetching the value, it's guaranteed (via the mutex) that
// sufficient memory barriers have occurred to ensure the correct value is
// visible on the querying thread/CPU.
uint32_t sPreferredSampleRate;

} // namespace

extern LazyLogModule gAudioStreamLog;

static const uint32_t CUBEB_NORMAL_LATENCY_MS = 100;
// Consevative default that can work on all platforms.
static const uint32_t CUBEB_NORMAL_LATENCY_FRAMES = 1024;

namespace CubebUtils {

void PrefChanged(const char* aPref, void* aClosure)
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
  } else if (strcmp(aPref, PREF_CUBEB_LATENCY_PLAYBACK) == 0) {
    // Arbitrary default stream latency of 100ms.  The higher this
    // value, the longer stream volume changes will take to become
    // audible.
    sCubebPlaybackLatencyPrefSet = Preferences::HasUserValue(aPref);
    uint32_t value = Preferences::GetUint(aPref, CUBEB_NORMAL_LATENCY_MS);
    StaticMutexAutoLock lock(sMutex);
    sCubebPlaybackLatencyInMilliseconds = std::min<uint32_t>(std::max<uint32_t>(value, 1), 1000);
  } else if (strcmp(aPref, PREF_CUBEB_LATENCY_MSG) == 0) {
    sCubebMSGLatencyPrefSet = Preferences::HasUserValue(aPref);
    uint32_t value = Preferences::GetUint(aPref, CUBEB_NORMAL_LATENCY_FRAMES);
    StaticMutexAutoLock lock(sMutex);
    // 128 is the block size for the Web Audio API, which limits how low the
    // latency can be here.
    // We don't want to limit the upper limit too much, so that people can
    // experiment.
    sCubebMSGLatencyInFrames = std::min<uint32_t>(std::max<uint32_t>(value, 128), 1e6);
  }
}

bool GetFirstStream()
{
  static bool sFirstStream = true;

  StaticMutexAutoLock lock(sMutex);
  bool result = sFirstStream;
  sFirstStream = false;
  return result;
}

double GetVolumeScale()
{
  StaticMutexAutoLock lock(sMutex);
  return sVolumeScale;
}

cubeb* GetCubebContext()
{
  StaticMutexAutoLock lock(sMutex);
  return GetCubebContextUnlocked();
}

bool InitPreferredSampleRate()
{
  StaticMutexAutoLock lock(sMutex);
  if (sPreferredSampleRate != 0) {
    return true;
  }
  cubeb* context = GetCubebContextUnlocked();
  if (!context) {
    return false;
  }
  if (cubeb_get_preferred_sample_rate(context,
                                      &sPreferredSampleRate) != CUBEB_OK) {

    return false;
  }
  MOZ_ASSERT(sPreferredSampleRate);
  return true;
}

uint32_t PreferredSampleRate()
{
  if (!InitPreferredSampleRate()) {
    return 44100;
  }
  MOZ_ASSERT(sPreferredSampleRate);
  return sPreferredSampleRate;
}

void InitBrandName()
{
  if (sBrandName) {
    return;
  }
  nsXPIDLString brandName;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
    mozilla::services::GetStringBundleService();
  if (stringBundleService) {
    nsCOMPtr<nsIStringBundle> brandBundle;
    nsresult rv = stringBundleService->CreateBundle(kBrandBundleURL,
                                           getter_AddRefs(brandBundle));
    if (NS_SUCCEEDED(rv)) {
      rv = brandBundle->GetStringFromName(u"brandShortName",
                                          getter_Copies(brandName));
      NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv), "Could not get the program name for a cubeb stream.");
    }
  }
  /* cubeb expects a c-string. */
  const char* ascii = NS_LossyConvertUTF16toASCII(brandName).get();
  sBrandName = new char[brandName.Length() + 1];
  PodCopy(sBrandName.get(), ascii, brandName.Length());
  sBrandName[brandName.Length()] = 0;
}

cubeb* GetCubebContextUnlocked()
{
  sMutex.AssertCurrentThreadOwns();
  if (sCubebState != CubebState::Uninitialized) {
    // If we have already passed the initialization point (below), just return
    // the current context, which may be null (e.g., after error or shutdown.)
    return sCubebContext;
  }

  if (!sBrandName && NS_IsMainThread()) {
    InitBrandName();
  } else {
    NS_WARNING_ASSERTION(
      sBrandName, "Did not initialize sbrandName, and not on the main thread?");
  }

  int rv = cubeb_init(&sCubebContext, sBrandName);
  NS_WARNING_ASSERTION(rv == CUBEB_OK, "Could not get a cubeb context.");
  sCubebState = (rv == CUBEB_OK) ? CubebState::Initialized : CubebState::Uninitialized;

  if (MOZ_LOG_TEST(gCubebLog, LogLevel::Verbose)) {
    cubeb_set_log_callback(CUBEB_LOG_VERBOSE, CubebLogCallback);
  } else if (MOZ_LOG_TEST(gCubebLog, LogLevel::Error)) {
    cubeb_set_log_callback(CUBEB_LOG_NORMAL, CubebLogCallback);
  }

  return sCubebContext;
}

void ReportCubebBackendUsed()
{
  StaticMutexAutoLock lock(sMutex);

  sAudioStreamInitEverSucceeded = true;

  bool foundBackend = false;
  for (uint32_t i = 0; i < ArrayLength(AUDIOSTREAM_BACKEND_ID_STR); i++) {
    if (!strcmp(cubeb_get_backend_id(sCubebContext), AUDIOSTREAM_BACKEND_ID_STR[i])) {
      Telemetry::Accumulate(Telemetry::AUDIOSTREAM_BACKEND_USED, i);
      foundBackend = true;
    }
  }
  if (!foundBackend) {
    Telemetry::Accumulate(Telemetry::AUDIOSTREAM_BACKEND_USED,
                          CUBEB_BACKEND_UNKNOWN);
  }
}

void ReportCubebStreamInitFailure(bool aIsFirst)
{
  StaticMutexAutoLock lock(sMutex);
  if (!aIsFirst && !sAudioStreamInitEverSucceeded) {
    // This machine has no audio hardware, or it's in really bad shape, don't
    // send this info, since we want CUBEB_BACKEND_INIT_FAILURE_OTHER to detect
    // failures to open multiple streams in a process over time.
    return;
  }
  Telemetry::Accumulate(Telemetry::AUDIOSTREAM_BACKEND_USED,
                        aIsFirst ? CUBEB_BACKEND_INIT_FAILURE_FIRST
                                 : CUBEB_BACKEND_INIT_FAILURE_OTHER);
}

uint32_t GetCubebPlaybackLatencyInMilliseconds()
{
  StaticMutexAutoLock lock(sMutex);
  return sCubebPlaybackLatencyInMilliseconds;
}

bool CubebPlaybackLatencyPrefSet()
{
  StaticMutexAutoLock lock(sMutex);
  return sCubebPlaybackLatencyPrefSet;
}

bool CubebMSGLatencyPrefSet()
{
  StaticMutexAutoLock lock(sMutex);
  return sCubebMSGLatencyPrefSet;
}

Maybe<uint32_t> GetCubebMSGLatencyInFrames()
{
  StaticMutexAutoLock lock(sMutex);
  if (!sCubebMSGLatencyPrefSet) {
    return Maybe<uint32_t>();
  }
  MOZ_ASSERT(sCubebMSGLatencyInFrames > 0);
  return Some(sCubebMSGLatencyInFrames);
}

void InitLibrary()
{
  PrefChanged(PREF_VOLUME_SCALE, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_VOLUME_SCALE);
  PrefChanged(PREF_CUBEB_LATENCY_PLAYBACK, nullptr);
  PrefChanged(PREF_CUBEB_LATENCY_MSG, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_CUBEB_LATENCY_PLAYBACK);
  Preferences::RegisterCallback(PrefChanged, PREF_CUBEB_LATENCY_MSG);
#ifndef MOZ_WIDGET_ANDROID
  NS_DispatchToMainThread(NS_NewRunnableFunction(&InitBrandName));
#endif
}

void ShutdownLibrary()
{
  Preferences::UnregisterCallback(PrefChanged, PREF_VOLUME_SCALE);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_LATENCY_PLAYBACK);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_LATENCY_MSG);

  StaticMutexAutoLock lock(sMutex);
  if (sCubebContext) {
    cubeb_destroy(sCubebContext);
    sCubebContext = nullptr;
  }
  sBrandName = nullptr;
  // This will ensure we don't try to re-create a context.
  sCubebState = CubebState::Shutdown;
}

uint32_t MaxNumberOfChannels()
{
  cubeb* cubebContext = GetCubebContext();
  uint32_t maxNumberOfChannels;
  if (cubebContext &&
      cubeb_get_max_channel_count(cubebContext,
                                  &maxNumberOfChannels) == CUBEB_OK) {
    return maxNumberOfChannels;
  }

  return 0;
}

#if defined(__ANDROID__) && defined(MOZ_B2G)
cubeb_stream_type ConvertChannelToCubebType(dom::AudioChannel aChannel)
{
  switch(aChannel) {
    case dom::AudioChannel::Normal:
      /* FALLTHROUGH */
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
    case dom::AudioChannel::System:
      return CUBEB_STREAM_TYPE_SYSTEM;
    case dom::AudioChannel::Publicnotification:
      return CUBEB_STREAM_TYPE_SYSTEM_ENFORCED;
    default:
      NS_ERROR("The value of AudioChannel is invalid");
      return CUBEB_STREAM_TYPE_MAX;
  }
}
#endif

void GetCurrentBackend(nsAString& aBackend)
{
  cubeb* cubebContext = GetCubebContext();
  if (cubebContext) {
    const char* backend = cubeb_get_backend_id(cubebContext);
    if (backend) {
      aBackend.AssignASCII(backend);
      return;
    }
  }
  aBackend.AssignLiteral("unknown");
}

} // namespace CubebUtils
} // namespace mozilla
