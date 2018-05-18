/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CubebUtils.h"

#include "MediaInfo.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "nsAutoRef.h"
#include "nsDebug.h"
#include "nsIStringBundle.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "prdtoa.h"
#include <algorithm>
#include <stdint.h>
#ifdef MOZ_WIDGET_ANDROID
#include "GeneratedJNIWrappers.h"
#endif

#define AUDIOIPC_POOL_SIZE_DEFAULT 1
#define AUDIOIPC_STACK_SIZE_DEFAULT (64*1024)

#define PREF_VOLUME_SCALE "media.volume_scale"
#define PREF_CUBEB_BACKEND "media.cubeb.backend"
#define PREF_CUBEB_LATENCY_PLAYBACK "media.cubeb_latency_playback_ms"
#define PREF_CUBEB_LATENCY_MSG "media.cubeb_latency_msg_frames"
// Allows to get something non-default for the preferred sample-rate, to allow
// troubleshooting in the field and testing.
#define PREF_CUBEB_FORCE_SAMPLE_RATE "media.cubeb.force_sample_rate"
#define PREF_CUBEB_LOGGING_LEVEL "media.cubeb.logging_level"
// Hidden pref used by tests to force failure to obtain cubeb context
#define PREF_CUBEB_FORCE_NULL_CONTEXT "media.cubeb.force_null_context"
// Hidden pref to disable BMO 1427011 experiment; can be removed once proven.
#define PREF_CUBEB_DISABLE_DEVICE_SWITCHING "media.cubeb.disable_device_switching"
#define PREF_CUBEB_SANDBOX "media.cubeb.sandbox"
#define PREF_AUDIOIPC_POOL_SIZE "media.audioipc.pool_size"
#define PREF_AUDIOIPC_STACK_SIZE "media.audioipc.stack_size"

#if (defined(XP_LINUX) && !defined(MOZ_WIDGET_ANDROID)) || defined(XP_MACOSX)
#define MOZ_CUBEB_REMOTING
#endif

extern "C" {

struct AudioIpcInitParams {
  int mServerConnection;
  size_t mPoolSize;
  size_t mStackSize;
};

// These functions are provided by audioipc-server crate
extern void* audioipc_server_start();
extern mozilla::ipc::FileDescriptor::PlatformHandleType audioipc_server_new_client(void*);
extern void audioipc_server_stop(void*);
// These functions are provided by audioipc-client crate
extern int audioipc_client_init(cubeb**, const char*, const AudioIpcInitParams*);
}

namespace mozilla {

namespace {

#ifdef MOZ_CUBEB_REMOTING
////////////////////////////////////////////////////////////////////////////////
// Cubeb Sound Server Thread
void* sServerHandle = nullptr;

// Initialized during early startup, protected by sMutex.
StaticAutoPtr<ipc::FileDescriptor> sIPCConnection;

static bool
StartSoundServer()
{
  sServerHandle = audioipc_server_start();
  return sServerHandle != nullptr;
}

static void
ShutdownSoundServer()
{
  if (!sServerHandle)
    return;

  audioipc_server_stop(sServerHandle);
  sServerHandle = nullptr;
}
#endif // MOZ_CUBEB_REMOTING

////////////////////////////////////////////////////////////////////////////////

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
double sVolumeScale = 1.0;
uint32_t sCubebPlaybackLatencyInMilliseconds = 100;
uint32_t sCubebMSGLatencyInFrames = 512;
// If sCubebForcedSampleRate is zero, PreferredSampleRate will return the
// preferred sample-rate for the audio backend in use. Otherwise, it will be
// used as the preferred sample-rate.
uint32_t sCubebForcedSampleRate = 0;
bool sCubebPlaybackLatencyPrefSet = false;
bool sCubebMSGLatencyPrefSet = false;
bool sAudioStreamInitEverSucceeded = false;
bool sCubebForceNullContext = false;
bool sCubebDisableDeviceSwitching = true;
#ifdef MOZ_CUBEB_REMOTING
bool sCubebSandbox = false;
size_t sAudioIPCPoolSize;
size_t sAudioIPCStackSize;
#endif
StaticAutoPtr<char> sBrandName;
StaticAutoPtr<char> sCubebBackendName;

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

static const uint32_t CUBEB_NORMAL_LATENCY_MS = 100;
// Consevative default that can work on all platforms.
static const uint32_t CUBEB_NORMAL_LATENCY_FRAMES = 1024;

namespace CubebUtils {
cubeb* GetCubebContextUnlocked();

void PrefChanged(const char* aPref, void* aClosure)
{
  if (strcmp(aPref, PREF_VOLUME_SCALE) == 0) {
    nsAutoCString value;
    Preferences::GetCString(aPref, value);
    StaticMutexAutoLock lock(sMutex);
    if (value.IsEmpty()) {
      sVolumeScale = 1.0;
    } else {
      sVolumeScale = std::max<double>(0, PR_strtod(value.get(), nullptr));
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
  } else if (strcmp(aPref, PREF_CUBEB_FORCE_SAMPLE_RATE) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sCubebForcedSampleRate = Preferences::GetUint(aPref);
  } else if (strcmp(aPref, PREF_CUBEB_LOGGING_LEVEL) == 0) {
    nsAutoCString value;
    Preferences::GetCString(aPref, value);
    LogModule* cubebLog = LogModule::Get("cubeb");
    if (value.EqualsLiteral("verbose")) {
      cubeb_set_log_callback(CUBEB_LOG_VERBOSE, CubebLogCallback);
      cubebLog->SetLevel(LogLevel::Verbose);
    } else if (value.EqualsLiteral("normal")) {
      cubeb_set_log_callback(CUBEB_LOG_NORMAL, CubebLogCallback);
      cubebLog->SetLevel(LogLevel::Error);
    } else if (value.IsEmpty()) {
      cubeb_set_log_callback(CUBEB_LOG_DISABLED, nullptr);
      cubebLog->SetLevel(LogLevel::Disabled);
    }
  } else if (strcmp(aPref, PREF_CUBEB_BACKEND) == 0) {
    nsAutoCString value;
    Preferences::GetCString(aPref, value);
    if (value.IsEmpty()) {
      sCubebBackendName = nullptr;
    } else {
      sCubebBackendName = new char[value.Length() + 1];
      PodCopy(sCubebBackendName.get(), value.get(), value.Length());
      sCubebBackendName[value.Length()] = 0;
    }
  } else if (strcmp(aPref, PREF_CUBEB_FORCE_NULL_CONTEXT) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sCubebForceNullContext = Preferences::GetBool(aPref, false);
    MOZ_LOG(gCubebLog, LogLevel::Verbose,
            ("%s: %s", PREF_CUBEB_FORCE_NULL_CONTEXT, sCubebForceNullContext ? "true" : "false"));
  } else if (strcmp(aPref, PREF_CUBEB_DISABLE_DEVICE_SWITCHING) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sCubebDisableDeviceSwitching = Preferences::GetBool(aPref, true);
    MOZ_LOG(gCubebLog, LogLevel::Verbose,
            ("%s: %s", PREF_CUBEB_DISABLE_DEVICE_SWITCHING, sCubebDisableDeviceSwitching ? "true" : "false"));
 }
#ifdef MOZ_CUBEB_REMOTING
  else if (strcmp(aPref, PREF_CUBEB_SANDBOX) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sCubebSandbox = Preferences::GetBool(aPref);
    MOZ_LOG(gCubebLog, LogLevel::Verbose, ("%s: %s", PREF_CUBEB_SANDBOX, sCubebSandbox ? "true" : "false"));

    if (sCubebSandbox && !sServerHandle && XRE_IsParentProcess()) {
      MOZ_LOG(gCubebLog, LogLevel::Debug, ("Starting cubeb server..."));
      StartSoundServer();
    }
  }
  else if (strcmp(aPref, PREF_AUDIOIPC_POOL_SIZE) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sAudioIPCPoolSize =  Preferences::GetUint(PREF_AUDIOIPC_POOL_SIZE,
                                             AUDIOIPC_POOL_SIZE_DEFAULT);
  }
  else if (strcmp(aPref, PREF_AUDIOIPC_STACK_SIZE) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sAudioIPCStackSize = Preferences::GetUint(PREF_AUDIOIPC_STACK_SIZE,
                                             AUDIOIPC_STACK_SIZE_DEFAULT);
  }
#endif
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
#ifdef MOZ_WIDGET_ANDROID
  sPreferredSampleRate = AndroidGetAudioOutputSampleRate();
#else
  cubeb* context = GetCubebContextUnlocked();
  if (!context) {
    return false;
  }
  if (cubeb_get_preferred_sample_rate(context,
                                      &sPreferredSampleRate) != CUBEB_OK) {

    return false;
  }
#endif
  MOZ_ASSERT(sPreferredSampleRate);
  return true;
}

uint32_t PreferredSampleRate()
{
  if (sCubebForcedSampleRate) {
    return sCubebForcedSampleRate;
  }
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
  nsAutoString brandName;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
    mozilla::services::GetStringBundleService();
  if (stringBundleService) {
    nsCOMPtr<nsIStringBundle> brandBundle;
    nsresult rv = stringBundleService->CreateBundle(kBrandBundleURL,
                                           getter_AddRefs(brandBundle));
    if (NS_SUCCEEDED(rv)) {
      rv = brandBundle->GetStringFromName("brandShortName", brandName);
      NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv), "Could not get the program name for a cubeb stream.");
    }
  }
  NS_LossyConvertUTF16toASCII ascii(brandName);
  sBrandName = new char[ascii.Length() + 1];
  PodCopy(sBrandName.get(), ascii.get(), ascii.Length());
  sBrandName[ascii.Length()] = 0;
}

#ifdef MOZ_CUBEB_REMOTING
void InitAudioIPCConnection()
{
  MOZ_ASSERT(NS_IsMainThread());
  auto contentChild = dom::ContentChild::GetSingleton();
  auto promise = contentChild->SendCreateAudioIPCConnection();
  promise->Then(AbstractThread::MainThread(),
                __func__,
                [](ipc::FileDescriptor aFD) {
                  StaticMutexAutoLock lock(sMutex);
                  MOZ_ASSERT(!sIPCConnection);
                  sIPCConnection = new ipc::FileDescriptor(aFD);
                },
                [](mozilla::ipc::ResponseRejectReason aReason) {
                  MOZ_LOG(gCubebLog, LogLevel::Error, ("SendCreateAudioIPCConnection failed: %d",
                                                       int(aReason)));
                });
}
#endif

ipc::FileDescriptor CreateAudioIPCConnection()
{
#ifdef MOZ_CUBEB_REMOTING
  MOZ_ASSERT(sServerHandle);
  int rawFD = audioipc_server_new_client(sServerHandle);
  ipc::FileDescriptor fd(rawFD);
  close(rawFD);
  return fd;
#else
  return ipc::FileDescriptor();
#endif
}

cubeb* GetCubebContextUnlocked()
{
  sMutex.AssertCurrentThreadOwns();
  if (sCubebForceNullContext) {
    // Pref set such that we should return a null context
    MOZ_LOG(gCubebLog, LogLevel::Debug,
            ("%s: returning null context due to %s!", __func__, PREF_CUBEB_FORCE_NULL_CONTEXT));
    return nullptr;
  }
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

#ifdef MOZ_CUBEB_REMOTING
  MOZ_LOG(gCubebLog, LogLevel::Info, ("%s: %s", PREF_CUBEB_SANDBOX, sCubebSandbox ? "true" : "false"));

  int rv = CUBEB_OK;
  if (sCubebSandbox) {
    if (XRE_IsParentProcess()) {
      // TODO: Don't use audio IPC when within the same process.
      MOZ_ASSERT(!sIPCConnection);
      sIPCConnection = new ipc::FileDescriptor(CreateAudioIPCConnection());
    } else {
      MOZ_DIAGNOSTIC_ASSERT(sIPCConnection);
    }

    AudioIpcInitParams initParams;
    initParams.mPoolSize = sAudioIPCPoolSize;
    initParams.mStackSize = sAudioIPCStackSize;
    initParams.mServerConnection = sIPCConnection->ClonePlatformHandle().release();

    MOZ_LOG(gCubebLog, LogLevel::Debug, ("%s: %d", PREF_AUDIOIPC_POOL_SIZE, (int) initParams.mPoolSize));
    MOZ_LOG(gCubebLog, LogLevel::Debug, ("%s: %d", PREF_AUDIOIPC_STACK_SIZE, (int) initParams.mStackSize));

    rv = audioipc_client_init(&sCubebContext, sBrandName, &initParams);
  } else {
    rv = cubeb_init(&sCubebContext, sBrandName, sCubebBackendName.get());
  }
  sIPCConnection = nullptr;
#else // !MOZ_CUBEB_REMOTING
  int rv = cubeb_init(&sCubebContext, sBrandName, sCubebBackendName.get());
#endif // MOZ_CUBEB_REMOTING
  NS_WARNING_ASSERTION(rv == CUBEB_OK, "Could not get a cubeb context.");
  sCubebState = (rv == CUBEB_OK) ? CubebState::Initialized : CubebState::Uninitialized;

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

uint32_t GetCubebMSGLatencyInFrames(cubeb_stream_params * params)
{
  StaticMutexAutoLock lock(sMutex);
  if (sCubebMSGLatencyPrefSet) {
    MOZ_ASSERT(sCubebMSGLatencyInFrames > 0);
    return sCubebMSGLatencyInFrames;
  }

#ifdef MOZ_WIDGET_ANDROID
  return AndroidGetAudioOutputFramesPerBuffer();
#else
  cubeb* context = GetCubebContextUnlocked();
  if (!context) {
    return sCubebMSGLatencyInFrames; // default 512
  }
  uint32_t latency_frames = 0;
  if (cubeb_get_min_latency(context, params, &latency_frames) != CUBEB_OK) {
    NS_WARNING("Could not get minimal latency from cubeb.");
    return sCubebMSGLatencyInFrames; // default 512
  }
  return latency_frames;
#endif
}

void InitLibrary()
{
  Preferences::RegisterCallbackAndCall(PrefChanged, PREF_VOLUME_SCALE);
  Preferences::RegisterCallbackAndCall(PrefChanged, PREF_CUBEB_LATENCY_PLAYBACK);
  Preferences::RegisterCallbackAndCall(PrefChanged, PREF_CUBEB_LATENCY_MSG);
  Preferences::RegisterCallback(PrefChanged, PREF_CUBEB_FORCE_SAMPLE_RATE);
  Preferences::RegisterCallbackAndCall(PrefChanged, PREF_CUBEB_BACKEND);
  Preferences::RegisterCallbackAndCall(PrefChanged, PREF_CUBEB_FORCE_NULL_CONTEXT);
  Preferences::RegisterCallbackAndCall(PrefChanged, PREF_CUBEB_SANDBOX);
  Preferences::RegisterCallbackAndCall(PrefChanged, PREF_AUDIOIPC_POOL_SIZE);
  Preferences::RegisterCallbackAndCall(PrefChanged, PREF_AUDIOIPC_STACK_SIZE);
  if (MOZ_LOG_TEST(gCubebLog, LogLevel::Verbose)) {
    cubeb_set_log_callback(CUBEB_LOG_VERBOSE, CubebLogCallback);
  } else if (MOZ_LOG_TEST(gCubebLog, LogLevel::Error)) {
    cubeb_set_log_callback(CUBEB_LOG_NORMAL, CubebLogCallback);
  }
  // We don't want to call the callback on startup, because the pref is the
  // empty string by default ("", which means "logging disabled"). Because the
  // logging can be enabled via environment variables (MOZ_LOG="module:5"),
  // calling this callback on init would immediately re-disable the logging.
  Preferences::RegisterCallback(PrefChanged, PREF_CUBEB_LOGGING_LEVEL);
#ifndef MOZ_WIDGET_ANDROID
  AbstractThread::MainThread()->Dispatch(
    NS_NewRunnableFunction("CubebUtils::InitLibrary", &InitBrandName));
#endif
#ifdef MOZ_CUBEB_REMOTING
  if (sCubebSandbox && XRE_IsContentProcess()) {
    InitAudioIPCConnection();
  }
#endif
}

void ShutdownLibrary()
{
  Preferences::UnregisterCallback(PrefChanged, PREF_VOLUME_SCALE);
  Preferences::UnregisterCallback(PrefChanged, PREF_AUDIOIPC_STACK_SIZE);
  Preferences::UnregisterCallback(PrefChanged, PREF_AUDIOIPC_POOL_SIZE);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_SANDBOX);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_BACKEND);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_LATENCY_PLAYBACK);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_FORCE_SAMPLE_RATE);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_LATENCY_MSG);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_LOGGING_LEVEL);
  Preferences::UnregisterCallback(PrefChanged, PREF_CUBEB_FORCE_NULL_CONTEXT);

  StaticMutexAutoLock lock(sMutex);
  if (sCubebContext) {
    cubeb_destroy(sCubebContext);
    sCubebContext = nullptr;
  }
  sBrandName = nullptr;
  sCubebBackendName = nullptr;
  // This will ensure we don't try to re-create a context.
  sCubebState = CubebState::Shutdown;

#ifdef MOZ_CUBEB_REMOTING
  sIPCConnection = nullptr;
  ShutdownSoundServer();
#endif
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

uint16_t ConvertCubebType(cubeb_device_type aType)
{
  uint16_t map[] = {
    nsIAudioDeviceInfo::TYPE_UNKNOWN, // CUBEB_DEVICE_TYPE_UNKNOWN
    nsIAudioDeviceInfo::TYPE_INPUT,   // CUBEB_DEVICE_TYPE_INPUT,
    nsIAudioDeviceInfo::TYPE_OUTPUT   // CUBEB_DEVICE_TYPE_OUTPUT
  };
  return map[aType];
}

uint16_t ConvertCubebState(cubeb_device_state aState)
{
  uint16_t map[] = {
    nsIAudioDeviceInfo::STATE_DISABLED,   // CUBEB_DEVICE_STATE_DISABLED
    nsIAudioDeviceInfo::STATE_UNPLUGGED,  // CUBEB_DEVICE_STATE_UNPLUGGED
    nsIAudioDeviceInfo::STATE_ENABLED     // CUBEB_DEVICE_STATE_ENABLED
  };
  return map[aState];
}

uint16_t ConvertCubebPreferred(cubeb_device_pref aPreferred)
{
  if (aPreferred == CUBEB_DEVICE_PREF_NONE) {
    return nsIAudioDeviceInfo::PREF_NONE;
  } else if (aPreferred == CUBEB_DEVICE_PREF_ALL) {
    return nsIAudioDeviceInfo::PREF_ALL;
  }

  uint16_t preferred = 0;
  if (aPreferred & CUBEB_DEVICE_PREF_MULTIMEDIA) {
    preferred |= nsIAudioDeviceInfo::PREF_MULTIMEDIA;
  }
  if (aPreferred & CUBEB_DEVICE_PREF_VOICE) {
    preferred |= nsIAudioDeviceInfo::PREF_VOICE;
  }
  if (aPreferred & CUBEB_DEVICE_PREF_NOTIFICATION) {
    preferred |= nsIAudioDeviceInfo::PREF_NOTIFICATION;
  }
  return preferred;
}

uint16_t ConvertCubebFormat(cubeb_device_fmt aFormat)
{
  uint16_t format = 0;
  if (aFormat & CUBEB_DEVICE_FMT_S16LE) {
    format |= nsIAudioDeviceInfo::FMT_S16LE;
  }
  if (aFormat & CUBEB_DEVICE_FMT_S16BE) {
    format |= nsIAudioDeviceInfo::FMT_S16BE;
  }
  if (aFormat & CUBEB_DEVICE_FMT_F32LE) {
    format |= nsIAudioDeviceInfo::FMT_F32LE;
  }
  if (aFormat & CUBEB_DEVICE_FMT_F32BE) {
    format |= nsIAudioDeviceInfo::FMT_F32BE;
  }
  return format;
}

void GetDeviceCollection(nsTArray<RefPtr<AudioDeviceInfo>>& aDeviceInfos,
                         Side aSide)
{
  cubeb* context = GetCubebContext();
  if (context) {
    cubeb_device_collection collection = { nullptr, 0 };
    if (cubeb_enumerate_devices(context,
                                aSide == Input ? CUBEB_DEVICE_TYPE_INPUT :
                                                 CUBEB_DEVICE_TYPE_OUTPUT,
                                &collection) == CUBEB_OK) {
      for (unsigned int i = 0; i < collection.count; ++i) {
        auto device = collection.device[i];
        RefPtr<AudioDeviceInfo> info =
          new AudioDeviceInfo(NS_ConvertUTF8toUTF16(device.friendly_name),
                              NS_ConvertUTF8toUTF16(device.group_id),
                              NS_ConvertUTF8toUTF16(device.vendor_name),
                              ConvertCubebType(device.type),
                              ConvertCubebState(device.state),
                              ConvertCubebPreferred(device.preferred),
                              ConvertCubebFormat(device.format),
                              ConvertCubebFormat(device.default_format),
                              device.max_channels,
                              device.default_rate,
                              device.max_rate,
                              device.min_rate,
                              device.latency_hi,
                              device.latency_lo);
        aDeviceInfos.AppendElement(info);
      }
    }
    cubeb_device_collection_destroy(context, &collection);
  }
}

cubeb_stream_prefs GetDefaultStreamPrefs()
{
#ifdef XP_WIN
  // Investigation for bug 1427011 - if we're in E10S mode, rely on the
  // AudioNotification IPC to detect device changes.
  if (sCubebDisableDeviceSwitching &&
      (XRE_IsE10sParentProcess() || XRE_IsContentProcess())) {
    return CUBEB_STREAM_PREF_DISABLE_DEVICE_SWITCHING;
  }
#endif
  return CUBEB_STREAM_PREF_NONE;
}

#ifdef MOZ_WIDGET_ANDROID
uint32_t AndroidGetAudioOutputSampleRate()
{
  int32_t sample_rate = java::GeckoAppShell::GetAudioOutputSampleRate();
  MOZ_ASSERT(sample_rate > 0);
  return sample_rate;
}
uint32_t AndroidGetAudioOutputFramesPerBuffer()
{
  int32_t frames = java::GeckoAppShell::GetAudioOutputFramesPerBuffer();
  MOZ_ASSERT(frames > 0);
  return frames;
}
#endif

} // namespace CubebUtils
} // namespace mozilla
