/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <algorithm>
#include "mozilla/Preferences.h"
#include "mozilla/StaticMutex.h"
#include "CubebUtils.h"
#include "nsAutoRef.h"
#include "prdtoa.h"

#define PREF_VOLUME_SCALE "media.volume_scale"
#define PREF_CUBEB_LATENCY "media.cubeb_latency_ms"

namespace mozilla {

namespace {

// This mutex protects the variables below.
StaticMutex sMutex;
cubeb* sCubebContext;
double sVolumeScale;
uint32_t sCubebLatency;
bool sCubebLatencyPrefSet;

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

extern PRLogModuleInfo* gAudioStreamLog;

static const uint32_t CUBEB_NORMAL_LATENCY_MS = 100;

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

void InitPreferredSampleRate()
{
  StaticMutexAutoLock lock(sMutex);
  if (sPreferredSampleRate == 0 &&
      cubeb_get_preferred_sample_rate(GetCubebContextUnlocked(),
                                      &sPreferredSampleRate) != CUBEB_OK) {
    // Query failed, use a sensible default.
    sPreferredSampleRate = 44100;
  }
}

cubeb* GetCubebContextUnlocked()
{
  sMutex.AssertCurrentThreadOwns();
  if (sCubebContext ||
      cubeb_init(&sCubebContext, "CubebUtils") == CUBEB_OK) {
    return sCubebContext;
  }
  NS_WARNING("cubeb_init failed");
  return nullptr;
}

uint32_t GetCubebLatency()
{
  StaticMutexAutoLock lock(sMutex);
  return sCubebLatency;
}

bool CubebLatencyPrefSet()
{
  StaticMutexAutoLock lock(sMutex);
  return sCubebLatencyPrefSet;
}

void InitLibrary()
{
  gAudioStreamLog = PR_NewLogModule("AudioStream");
  PrefChanged(PREF_VOLUME_SCALE, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_VOLUME_SCALE);
  PrefChanged(PREF_CUBEB_LATENCY, nullptr);
  Preferences::RegisterCallback(PrefChanged, PREF_CUBEB_LATENCY);
}

void ShutdownLibrary()
{
  Preferences::UnregisterCallback(PrefChanged, PREF_VOLUME_SCALE);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_LATENCY);

  StaticMutexAutoLock lock(sMutex);
  if (sCubebContext) {
    cubeb_destroy(sCubebContext);
    sCubebContext = nullptr;
  }
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

uint32_t PreferredSampleRate()
{
  MOZ_ASSERT(sPreferredSampleRate,
             "sPreferredSampleRate has not been initialized!");
  return sPreferredSampleRate;
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

} // namespace CubebUtils
} // namespace mozilla
