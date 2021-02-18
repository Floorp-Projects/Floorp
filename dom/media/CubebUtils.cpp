/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CubebUtils.h"

#include "audio_thread_priority.h"
#include "MediaInfo.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/AudioDeviceInfo.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Components.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UnderrunHandler.h"
#include "nsAutoRef.h"
#include "nsDebug.h"
#include "nsIStringBundle.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "prdtoa.h"
#include <algorithm>
#include <stdint.h>
#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/GeckoAppShellWrappers.h"
#endif
#ifdef XP_WIN
#  include "mozilla/mscom/EnsureMTA.h"
#endif
#include "audioipc_server_ffi_generated.h"
#include "audioipc_client_ffi_generated.h"
#include <cmath>
#include <thread>
#include "AudioThreadRegistry.h"
#include "mozilla/StaticPrefs_media.h"

#define AUDIOIPC_POOL_SIZE_DEFAULT 1
#define AUDIOIPC_STACK_SIZE_DEFAULT (64 * 4096)

#define PREF_VOLUME_SCALE "media.volume_scale"
#define PREF_CUBEB_BACKEND "media.cubeb.backend"
#define PREF_CUBEB_OUTPUT_DEVICE "media.cubeb.output_device"
#define PREF_CUBEB_LATENCY_PLAYBACK "media.cubeb_latency_playback_ms"
#define PREF_CUBEB_LATENCY_MTG "media.cubeb_latency_mtg_frames"
// Allows to get something non-default for the preferred sample-rate, to allow
// troubleshooting in the field and testing.
#define PREF_CUBEB_FORCE_SAMPLE_RATE "media.cubeb.force_sample_rate"
#define PREF_CUBEB_LOGGING_LEVEL "media.cubeb.logging_level"
// Hidden pref used by tests to force failure to obtain cubeb context
#define PREF_CUBEB_FORCE_NULL_CONTEXT "media.cubeb.force_null_context"
#define PREF_CUBEB_OUTPUT_VOICE_ROUTING "media.cubeb.output_voice_routing"
#define PREF_CUBEB_SANDBOX "media.cubeb.sandbox"
#define PREF_AUDIOIPC_POOL_SIZE "media.audioipc.pool_size"
#define PREF_AUDIOIPC_STACK_SIZE "media.audioipc.stack_size"

#if (defined(XP_LINUX) && !defined(MOZ_WIDGET_ANDROID)) || \
    defined(XP_MACOSX) || (defined(XP_WIN) && !defined(_ARM64_))
#  define MOZ_CUBEB_REMOTING
#endif

namespace mozilla {

namespace {

using Telemetry::LABELS_MEDIA_AUDIO_BACKEND;
using Telemetry::LABELS_MEDIA_AUDIO_INIT_FAILURE;

LazyLogModule gCubebLog("cubeb");

void CubebLogCallback(const char* aFmt, ...) {
  char buffer[1024];

  va_list arglist;
  va_start(arglist, aFmt);
  VsprintfLiteral(buffer, aFmt, arglist);
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
uint32_t sCubebMTGLatencyInFrames = 512;
// If sCubebForcedSampleRate is zero, PreferredSampleRate will return the
// preferred sample-rate for the audio backend in use. Otherwise, it will be
// used as the preferred sample-rate.
uint32_t sCubebForcedSampleRate = 0;
bool sCubebPlaybackLatencyPrefSet = false;
bool sCubebMTGLatencyPrefSet = false;
bool sAudioStreamInitEverSucceeded = false;
bool sCubebForceNullContext = false;
bool sRouteOutputAsVoice = false;
#ifdef MOZ_CUBEB_REMOTING
bool sCubebSandbox = false;
size_t sAudioIPCPoolSize;
size_t sAudioIPCStackSize;
#endif
StaticAutoPtr<char> sBrandName;
StaticAutoPtr<char> sCubebBackendName;
StaticAutoPtr<char> sCubebOutputDeviceName;
StaticAutoPtr<AudioThreadRegistry> sAudioThreadRegistry;
#ifdef MOZ_WIDGET_ANDROID
// Counts the number of time a request for switching to global "communication
// mode" has been received. If this is > 0, global communication mode is to be
// enabled. If it is 0, the global communication mode is to be disabled.
// This allows to correctly track the global behaviour to adopt accross
// asynchronous GraphDriver changes, on Android.
int sInCommunicationCount = 0;
#endif

const char kBrandBundleURL[] = "chrome://branding/locale/brand.properties";

std::unordered_map<std::string, LABELS_MEDIA_AUDIO_BACKEND>
    kTelemetryBackendLabel = {
        {"audiounit", LABELS_MEDIA_AUDIO_BACKEND::audiounit},
        {"audiounit-rust", LABELS_MEDIA_AUDIO_BACKEND::audiounit_rust},
        {"aaudio", LABELS_MEDIA_AUDIO_BACKEND::aaudio},
        {"opensl", LABELS_MEDIA_AUDIO_BACKEND::opensl},
        {"wasapi", LABELS_MEDIA_AUDIO_BACKEND::wasapi},
        {"winmm", LABELS_MEDIA_AUDIO_BACKEND::winmm},
        {"alsa", LABELS_MEDIA_AUDIO_BACKEND::alsa},
        {"jack", LABELS_MEDIA_AUDIO_BACKEND::jack},
        {"oss", LABELS_MEDIA_AUDIO_BACKEND::oss},
        {"pulse", LABELS_MEDIA_AUDIO_BACKEND::pulse},
        {"pulse-rust", LABELS_MEDIA_AUDIO_BACKEND::pulse_rust},
        {"sndio", LABELS_MEDIA_AUDIO_BACKEND::sndio},
        {"sun", LABELS_MEDIA_AUDIO_BACKEND::sunaudio},
};

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

#ifdef MOZ_CUBEB_REMOTING
// AudioIPC server handle
void* sServerHandle = nullptr;

// Initialized during early startup, protected by sMutex.
StaticAutoPtr<ipc::FileDescriptor> sIPCConnection;

static bool StartAudioIPCServer() {
  sServerHandle =
      audioipc::audioipc_server_start(sBrandName, sCubebBackendName);
  return sServerHandle != nullptr;
}

static void ShutdownAudioIPCServer() {
  if (!sServerHandle) {
    return;
  }

  audioipc::audioipc_server_stop(sServerHandle);
  sServerHandle = nullptr;
}
#endif  // MOZ_CUBEB_REMOTING
}  // namespace

static const uint32_t CUBEB_NORMAL_LATENCY_MS = 100;
// Consevative default that can work on all platforms.
static const uint32_t CUBEB_NORMAL_LATENCY_FRAMES = 1024;

namespace CubebUtils {
cubeb* GetCubebContextUnlocked();

void GetPrefAndSetString(const char* aPref, StaticAutoPtr<char>& aStorage) {
  nsAutoCString value;
  Preferences::GetCString(aPref, value);
  if (value.IsEmpty()) {
    aStorage = nullptr;
  } else {
    aStorage = new char[value.Length() + 1];
    PodCopy(aStorage.get(), value.get(), value.Length());
    aStorage[value.Length()] = 0;
  }
}

void PrefChanged(const char* aPref, void* aClosure) {
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
    StaticMutexAutoLock lock(sMutex);
    // Arbitrary default stream latency of 100ms.  The higher this
    // value, the longer stream volume changes will take to become
    // audible.
    sCubebPlaybackLatencyPrefSet = Preferences::HasUserValue(aPref);
    uint32_t value = Preferences::GetUint(aPref, CUBEB_NORMAL_LATENCY_MS);
    sCubebPlaybackLatencyInMilliseconds =
        std::min<uint32_t>(std::max<uint32_t>(value, 1), 1000);
  } else if (strcmp(aPref, PREF_CUBEB_LATENCY_MTG) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sCubebMTGLatencyPrefSet = Preferences::HasUserValue(aPref);
    uint32_t value = Preferences::GetUint(aPref, CUBEB_NORMAL_LATENCY_FRAMES);
    // 128 is the block size for the Web Audio API, which limits how low the
    // latency can be here.
    // We don't want to limit the upper limit too much, so that people can
    // experiment.
    sCubebMTGLatencyInFrames =
        std::min<uint32_t>(std::max<uint32_t>(value, 128), 1e6);
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
    StaticMutexAutoLock lock(sMutex);
    GetPrefAndSetString(aPref, sCubebBackendName);
  } else if (strcmp(aPref, PREF_CUBEB_OUTPUT_DEVICE) == 0) {
    StaticMutexAutoLock lock(sMutex);
    GetPrefAndSetString(aPref, sCubebOutputDeviceName);
  } else if (strcmp(aPref, PREF_CUBEB_FORCE_NULL_CONTEXT) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sCubebForceNullContext = Preferences::GetBool(aPref, false);
    MOZ_LOG(gCubebLog, LogLevel::Verbose,
            ("%s: %s", PREF_CUBEB_FORCE_NULL_CONTEXT,
             sCubebForceNullContext ? "true" : "false"));
  }
#ifdef MOZ_CUBEB_REMOTING
  else if (strcmp(aPref, PREF_CUBEB_SANDBOX) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sCubebSandbox = Preferences::GetBool(aPref);
    MOZ_LOG(gCubebLog, LogLevel::Verbose,
            ("%s: %s", PREF_CUBEB_SANDBOX, sCubebSandbox ? "true" : "false"));
  } else if (strcmp(aPref, PREF_AUDIOIPC_POOL_SIZE) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sAudioIPCPoolSize = Preferences::GetUint(PREF_AUDIOIPC_POOL_SIZE,
                                             AUDIOIPC_POOL_SIZE_DEFAULT);
  } else if (strcmp(aPref, PREF_AUDIOIPC_STACK_SIZE) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sAudioIPCStackSize = Preferences::GetUint(PREF_AUDIOIPC_STACK_SIZE,
                                              AUDIOIPC_STACK_SIZE_DEFAULT);
  }
#endif
  else if (strcmp(aPref, PREF_CUBEB_OUTPUT_VOICE_ROUTING) == 0) {
    StaticMutexAutoLock lock(sMutex);
    sRouteOutputAsVoice = Preferences::GetBool(aPref);
    MOZ_LOG(gCubebLog, LogLevel::Verbose,
            ("%s: %s", PREF_CUBEB_OUTPUT_VOICE_ROUTING,
             sRouteOutputAsVoice ? "true" : "false"));
  }
}

bool GetFirstStream() {
  static bool sFirstStream = true;

  StaticMutexAutoLock lock(sMutex);
  bool result = sFirstStream;
  sFirstStream = false;
  return result;
}

double GetVolumeScale() {
  StaticMutexAutoLock lock(sMutex);
  return sVolumeScale;
}

cubeb* GetCubebContext() {
  StaticMutexAutoLock lock(sMutex);
  return GetCubebContextUnlocked();
}

// This is only exported when running tests.
void ForceSetCubebContext(cubeb* aCubebContext) {
  StaticMutexAutoLock lock(sMutex);
  sCubebContext = aCubebContext;
  sCubebState = CubebState::Initialized;
}

void SetInCommunication(bool aInCommunication) {
#ifdef MOZ_WIDGET_ANDROID
  StaticMutexAutoLock lock(sMutex);
  if (aInCommunication) {
    sInCommunicationCount++;
  } else {
    MOZ_ASSERT(sInCommunicationCount > 0);
    sInCommunicationCount--;
  }

  if (sInCommunicationCount == 1) {
    java::GeckoAppShell::SetCommunicationAudioModeOn(true);
  } else if (sInCommunicationCount == 0) {
    java::GeckoAppShell::SetCommunicationAudioModeOn(false);
  }
#endif
}

bool InitPreferredSampleRate() {
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
  if (cubeb_get_preferred_sample_rate(context, &sPreferredSampleRate) !=
      CUBEB_OK) {
    return false;
  }
#endif
  MOZ_ASSERT(sPreferredSampleRate);
  return true;
}

uint32_t PreferredSampleRate() {
  if (sCubebForcedSampleRate) {
    return sCubebForcedSampleRate;
  }
  if (StaticPrefs::privacy_resistFingerprinting()) {
    return 44100;
  }
  if (!InitPreferredSampleRate()) {
    return 44100;
  }
  MOZ_ASSERT(sPreferredSampleRate);
  return sPreferredSampleRate;
}

void InitBrandName() {
  if (sBrandName) {
    return;
  }
  nsAutoString brandName;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
      mozilla::components::StringBundle::Service();
  if (stringBundleService) {
    nsCOMPtr<nsIStringBundle> brandBundle;
    nsresult rv = stringBundleService->CreateBundle(
        kBrandBundleURL, getter_AddRefs(brandBundle));
    if (NS_SUCCEEDED(rv)) {
      rv = brandBundle->GetStringFromName("brandShortName", brandName);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Could not get the program name for a cubeb stream.");
    }
  }
  NS_LossyConvertUTF16toASCII ascii(brandName);
  sBrandName = new char[ascii.Length() + 1];
  PodCopy(sBrandName.get(), ascii.get(), ascii.Length());
  sBrandName[ascii.Length()] = 0;
}

#ifdef MOZ_CUBEB_REMOTING
void InitAudioIPCConnection() {
  MOZ_ASSERT(NS_IsMainThread());
  auto contentChild = dom::ContentChild::GetSingleton();
  auto promise = contentChild->SendCreateAudioIPCConnection();
  promise->Then(
      AbstractThread::MainThread(), __func__,
      [](dom::FileDescOrError&& aFD) {
        StaticMutexAutoLock lock(sMutex);
        MOZ_ASSERT(!sIPCConnection);
        if (aFD.type() == dom::FileDescOrError::Type::TFileDescriptor) {
          sIPCConnection = new ipc::FileDescriptor(std::move(aFD));
        } else {
          MOZ_LOG(gCubebLog, LogLevel::Error,
                  ("SendCreateAudioIPCConnection failed: invalid FD"));
        }
      },
      [](mozilla::ipc::ResponseRejectReason&& aReason) {
        MOZ_LOG(gCubebLog, LogLevel::Error,
                ("SendCreateAudioIPCConnection rejected: %d", int(aReason)));
      });
}
#endif

ipc::FileDescriptor CreateAudioIPCConnection() {
#ifdef MOZ_CUBEB_REMOTING
  MOZ_ASSERT(sCubebSandbox && XRE_IsParentProcess());
  if (!sServerHandle) {
    MOZ_LOG(gCubebLog, LogLevel::Debug, ("Starting cubeb server..."));
    if (!StartAudioIPCServer()) {
      MOZ_LOG(gCubebLog, LogLevel::Error, ("audioipc_server_start failed"));
      return ipc::FileDescriptor();
    }
  }
  MOZ_ASSERT(sServerHandle);
  ipc::FileDescriptor::PlatformHandleType rawFD =
      audioipc::audioipc_server_new_client(sServerHandle);
  ipc::FileDescriptor fd(rawFD);
  if (!fd.IsValid()) {
    MOZ_LOG(gCubebLog, LogLevel::Error, ("audioipc_server_new_client failed"));
    return ipc::FileDescriptor();
  }
  // Close rawFD since FileDescriptor's ctor cloned it.
  // TODO: Find cleaner cross-platform way to close rawFD.
#  ifdef XP_WIN
  CloseHandle(rawFD);
#  else
  close(rawFD);
#  endif
  return fd;
#else
  return ipc::FileDescriptor();
#endif
}

cubeb* GetCubebContextUnlocked() {
  sMutex.AssertCurrentThreadOwns();
  if (sCubebForceNullContext) {
    // Pref set such that we should return a null context
    MOZ_LOG(gCubebLog, LogLevel::Debug,
            ("%s: returning null context due to %s!", __func__,
             PREF_CUBEB_FORCE_NULL_CONTEXT));
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
        sBrandName,
        "Did not initialize sbrandName, and not on the main thread?");
  }

  int rv = CUBEB_ERROR;
#ifdef MOZ_CUBEB_REMOTING
  MOZ_LOG(gCubebLog, LogLevel::Info,
          ("%s: %s", PREF_CUBEB_SANDBOX, sCubebSandbox ? "true" : "false"));

  if (sCubebSandbox) {
    if (XRE_IsParentProcess() && !sIPCConnection) {
      // TODO: Don't use audio IPC when within the same process.
      auto fd = CreateAudioIPCConnection();
      if (fd.IsValid()) {
        sIPCConnection = new ipc::FileDescriptor(fd);
      }
    }
    if (NS_WARN_IF(!sIPCConnection)) {
      // Either the IPC connection failed to init or we're still waiting for
      // InitAudioIPCConnection to complete (bug 1454782).
      return nullptr;
    }

    audioipc::AudioIpcInitParams initParams;
    initParams.mPoolSize = sAudioIPCPoolSize;
    initParams.mStackSize = sAudioIPCStackSize;
    initParams.mServerConnection =
        sIPCConnection->ClonePlatformHandle().release();
    initParams.mThreadCreateCallback = [](const char* aName) {
      PROFILER_REGISTER_THREAD(aName);
    };
    initParams.mThreadDestroyCallback = []() { PROFILER_UNREGISTER_THREAD(); };

    MOZ_LOG(gCubebLog, LogLevel::Debug,
            ("%s: %d", PREF_AUDIOIPC_POOL_SIZE, (int)initParams.mPoolSize));
    MOZ_LOG(gCubebLog, LogLevel::Debug,
            ("%s: %d", PREF_AUDIOIPC_STACK_SIZE, (int)initParams.mStackSize));

    rv =
        audioipc::audioipc_client_init(&sCubebContext, sBrandName, &initParams);
  } else {
#endif  // MOZ_CUBEB_REMOTING
#ifdef XP_WIN
    mozilla::mscom::EnsureMTA([&]() -> void {
#endif
      rv = cubeb_init(&sCubebContext, sBrandName, sCubebBackendName);
#ifdef XP_WIN
    });
#endif
#ifdef MOZ_CUBEB_REMOTING
  }
  sIPCConnection = nullptr;
#endif  // MOZ_CUBEB_REMOTING
  NS_WARNING_ASSERTION(rv == CUBEB_OK, "Could not get a cubeb context.");
  sCubebState =
      (rv == CUBEB_OK) ? CubebState::Initialized : CubebState::Uninitialized;

  return sCubebContext;
}

void ReportCubebBackendUsed() {
  StaticMutexAutoLock lock(sMutex);

  sAudioStreamInitEverSucceeded = true;

  LABELS_MEDIA_AUDIO_BACKEND label = LABELS_MEDIA_AUDIO_BACKEND::unknown;
  auto backend =
      kTelemetryBackendLabel.find(cubeb_get_backend_id(sCubebContext));
  if (backend != kTelemetryBackendLabel.end()) {
    label = backend->second;
  }
  AccumulateCategorical(label);
}

void ReportCubebStreamInitFailure(bool aIsFirst) {
  StaticMutexAutoLock lock(sMutex);
  if (!aIsFirst && !sAudioStreamInitEverSucceeded) {
    // This machine has no audio hardware, or it's in really bad shape, don't
    // send this info, since we want CUBEB_BACKEND_INIT_FAILURE_OTHER to detect
    // failures to open multiple streams in a process over time.
    return;
  }
  AccumulateCategorical(aIsFirst ? LABELS_MEDIA_AUDIO_INIT_FAILURE::first
                                 : LABELS_MEDIA_AUDIO_INIT_FAILURE::other);
}

uint32_t GetCubebPlaybackLatencyInMilliseconds() {
  StaticMutexAutoLock lock(sMutex);
  return sCubebPlaybackLatencyInMilliseconds;
}

bool CubebPlaybackLatencyPrefSet() {
  StaticMutexAutoLock lock(sMutex);
  return sCubebPlaybackLatencyPrefSet;
}

bool CubebMTGLatencyPrefSet() {
  StaticMutexAutoLock lock(sMutex);
  return sCubebMTGLatencyPrefSet;
}

uint32_t GetCubebMTGLatencyInFrames(cubeb_stream_params* params) {
  StaticMutexAutoLock lock(sMutex);
  if (sCubebMTGLatencyPrefSet) {
    MOZ_ASSERT(sCubebMTGLatencyInFrames > 0);
    return sCubebMTGLatencyInFrames;
  }

#ifdef MOZ_WIDGET_ANDROID
  return AndroidGetAudioOutputFramesPerBuffer();
#else
  cubeb* context = GetCubebContextUnlocked();
  if (!context) {
    return sCubebMTGLatencyInFrames;  // default 512
  }
  uint32_t latency_frames = 0;
  if (cubeb_get_min_latency(context, params, &latency_frames) != CUBEB_OK) {
    NS_WARNING("Could not get minimal latency from cubeb.");
    return sCubebMTGLatencyInFrames;  // default 512
  }
  return latency_frames;
#endif
}

static const char* gInitCallbackPrefs[] = {
    PREF_VOLUME_SCALE,           PREF_CUBEB_OUTPUT_DEVICE,
    PREF_CUBEB_LATENCY_PLAYBACK, PREF_CUBEB_LATENCY_MTG,
    PREF_CUBEB_BACKEND,          PREF_CUBEB_FORCE_NULL_CONTEXT,
    PREF_CUBEB_SANDBOX,          PREF_AUDIOIPC_POOL_SIZE,
    PREF_AUDIOIPC_STACK_SIZE,    nullptr,
};
static const char* gCallbackPrefs[] = {
    PREF_CUBEB_FORCE_SAMPLE_RATE,
    // We don't want to call the callback on startup, because the pref is the
    // empty string by default ("", which means "logging disabled"). Because the
    // logging can be enabled via environment variables (MOZ_LOG="module:5"),
    // calling this callback on init would immediately re-disable the logging.
    PREF_CUBEB_LOGGING_LEVEL,
    nullptr,
};

void InitLibrary() {
  Preferences::RegisterCallbacksAndCall(PrefChanged, gInitCallbackPrefs);
  Preferences::RegisterCallbacks(PrefChanged, gCallbackPrefs);

  if (MOZ_LOG_TEST(gCubebLog, LogLevel::Verbose)) {
    cubeb_set_log_callback(CUBEB_LOG_VERBOSE, CubebLogCallback);
  } else if (MOZ_LOG_TEST(gCubebLog, LogLevel::Error)) {
    cubeb_set_log_callback(CUBEB_LOG_NORMAL, CubebLogCallback);
  }

#ifndef MOZ_WIDGET_ANDROID
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("CubebUtils::InitLibrary", &InitBrandName));
#endif
#ifdef MOZ_CUBEB_REMOTING
  if (sCubebSandbox && XRE_IsContentProcess()) {
#  ifdef XP_LINUX
    if (atp_set_real_time_limit(0, 48000)) {
      NS_WARNING("could not set real-time limit in CubebUtils::InitLibrary");
    }
    InstallSoftRealTimeLimitHandler();
#  endif
    InitAudioIPCConnection();
  }
#endif

  sAudioThreadRegistry = new AudioThreadRegistry;
}

void ShutdownLibrary() {
  Preferences::UnregisterCallbacks(PrefChanged, gInitCallbackPrefs);
  Preferences::UnregisterCallbacks(PrefChanged, gCallbackPrefs);

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
  ShutdownAudioIPCServer();
#endif
  sAudioThreadRegistry = nullptr;
}

bool SandboxEnabled() {
#ifdef MOZ_CUBEB_REMOTING
  StaticMutexAutoLock lock(sMutex);
  return !!sCubebSandbox;
#else
  return false;
#endif
}

AudioThreadRegistry* GetAudioThreadRegistry() { return sAudioThreadRegistry; }

uint32_t MaxNumberOfChannels() {
  cubeb* cubebContext = GetCubebContext();
  uint32_t maxNumberOfChannels;
  if (cubebContext && cubeb_get_max_channel_count(
                          cubebContext, &maxNumberOfChannels) == CUBEB_OK) {
    return maxNumberOfChannels;
  }

  return 0;
}

void GetCurrentBackend(nsAString& aBackend) {
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

char* GetForcedOutputDevice() {
  StaticMutexAutoLock lock(sMutex);
  return sCubebOutputDeviceName;
}

cubeb_stream_prefs GetDefaultStreamPrefs(cubeb_device_type aType) {
  cubeb_stream_prefs prefs = CUBEB_STREAM_PREF_NONE;
#ifdef XP_WIN
  if (StaticPrefs::media_cubeb_wasapi_raw() & static_cast<uint32_t>(aType)) {
    prefs |= CUBEB_STREAM_PREF_RAW;
  }
#endif
  return prefs;
}

bool RouteOutputAsVoice() { return sRouteOutputAsVoice; }

long datacb(cubeb_stream*, void*, const void*, void* out_buffer, long nframes) {
  PodZero(static_cast<float*>(out_buffer), nframes * 2);
  return nframes;
}

void statecb(cubeb_stream*, void*, cubeb_state) {}

bool EstimatedRoundTripLatencyDefaultDevices(double* aMean, double* aStdDev) {
  nsTArray<double> roundtripLatencies;
  // Create a cubeb stream with the correct latency and default input/output
  // devices (mono/stereo channels). Wait for two seconds, get the latency a few
  // times.
  int rv;
  uint32_t rate;
  uint32_t latencyFrames;
  rv = cubeb_get_preferred_sample_rate(GetCubebContext(), &rate);
  if (rv != CUBEB_OK) {
    MOZ_LOG(gCubebLog, LogLevel::Error, ("Could not get preferred rate"));
    return false;
  }

  cubeb_stream_params output_params;
  output_params.format = CUBEB_SAMPLE_FLOAT32NE;
  output_params.rate = rate;
  output_params.channels = 2;
  output_params.layout = CUBEB_LAYOUT_UNDEFINED;
  output_params.prefs = GetDefaultStreamPrefs(CUBEB_DEVICE_TYPE_OUTPUT);

  latencyFrames = GetCubebMTGLatencyInFrames(&output_params);

  cubeb_stream_params input_params;
  input_params.format = CUBEB_SAMPLE_FLOAT32NE;
  input_params.rate = rate;
  input_params.channels = 1;
  input_params.layout = CUBEB_LAYOUT_UNDEFINED;
  input_params.prefs = GetDefaultStreamPrefs(CUBEB_DEVICE_TYPE_INPUT);

  cubeb_stream* stm;
  rv = cubeb_stream_init(GetCubebContext(), &stm,
                         "about:support latency estimation", NULL,
                         &input_params, NULL, &output_params, latencyFrames,
                         datacb, statecb, NULL);
  if (rv != CUBEB_OK) {
    MOZ_LOG(gCubebLog, LogLevel::Error, ("Could not get init stream"));
    return false;
  }

  rv = cubeb_stream_start(stm);
  if (rv != CUBEB_OK) {
    MOZ_LOG(gCubebLog, LogLevel::Error, ("Could not start stream"));
    return false;
  }
  // +-2s
  for (uint32_t i = 0; i < 40; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    uint32_t inputLatency, outputLatency, rvIn, rvOut;
    rvOut = cubeb_stream_get_latency(stm, &outputLatency);
    if (rvOut) {
      MOZ_LOG(gCubebLog, LogLevel::Error, ("Could not get output latency"));
    }
    rvIn = cubeb_stream_get_input_latency(stm, &inputLatency);
    if (rvIn) {
      MOZ_LOG(gCubebLog, LogLevel::Error, ("Could not get input latency"));
    }
    if (rvIn != CUBEB_OK || rvOut != CUBEB_OK) {
      continue;
    }

    double roundTrip = static_cast<double>(outputLatency + inputLatency) / rate;
    roundtripLatencies.AppendElement(roundTrip);
  }
  rv = cubeb_stream_stop(stm);
  if (rv != CUBEB_OK) {
    MOZ_LOG(gCubebLog, LogLevel::Error, ("Could not stop the stream"));
  }

  *aMean = 0.0;
  *aStdDev = 0.0;
  double variance = 0.0;
  for (uint32_t i = 0; i < roundtripLatencies.Length(); i++) {
    *aMean += roundtripLatencies[i];
  }

  *aMean /= roundtripLatencies.Length();

  for (uint32_t i = 0; i < roundtripLatencies.Length(); i++) {
    variance += pow(roundtripLatencies[i] - *aMean, 2.);
  }
  variance /= roundtripLatencies.Length();

  *aStdDev = sqrt(variance);

  MOZ_LOG(gCubebLog, LogLevel::Debug,
          ("Default device roundtrip latency in seconds %lf (stddev: %lf)",
           *aMean, *aStdDev));

  cubeb_stream_destroy(stm);

  return true;
}

#ifdef MOZ_WIDGET_ANDROID
uint32_t AndroidGetAudioOutputSampleRate() {
  int32_t sample_rate = java::GeckoAppShell::GetAudioOutputSampleRate();
  MOZ_ASSERT(sample_rate > 0);
  return sample_rate;
}
uint32_t AndroidGetAudioOutputFramesPerBuffer() {
  int32_t frames = java::GeckoAppShell::GetAudioOutputFramesPerBuffer();
  MOZ_ASSERT(frames > 0);
  return frames;
}
#endif

}  // namespace CubebUtils
}  // namespace mozilla
