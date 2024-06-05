/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(CubebUtils_h_)
#  define CubebUtils_h_

#  include "cubeb/cubeb.h"

#  include "AudioSampleFormat.h"
#  include "nsString.h"
#  include "nsISupportsImpl.h"

class AudioDeviceInfo;

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(cubeb_stream_prefs)
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(cubeb_input_processing_params)

namespace mozilla {

class CallbackThreadRegistry;
class SharedThreadPool;

namespace CubebUtils {

typedef cubeb_devid AudioDeviceID;

template <AudioSampleFormat N>
struct ToCubebFormat {
  static const cubeb_sample_format value = CUBEB_SAMPLE_FLOAT32NE;
};

template <>
struct ToCubebFormat<AUDIO_FORMAT_S16> {
  static const cubeb_sample_format value = CUBEB_SAMPLE_S16NE;
};

nsCString ProcessingParamsToString(cubeb_input_processing_params aParams);

class CubebHandle {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CubebHandle)
  explicit CubebHandle(cubeb* aCubeb) : mCubeb(aCubeb) {
    MOZ_RELEASE_ASSERT(mCubeb);
  };
  CubebHandle(const CubebHandle&) = delete;
  cubeb* Context() const { return mCubeb.get(); }

 private:
  struct CubebDeletePolicy {
    void operator()(cubeb* aCubeb) { cubeb_destroy(aCubeb); }
  };
  const UniquePtr<cubeb, CubebDeletePolicy> mCubeb;
  ~CubebHandle() = default;
};

// Initialize Audio Library. Some Audio backends require initializing the
// library before using it.
void InitLibrary();

// Shutdown Audio Library. Some Audio backends require shutting down the
// library after using it.
void ShutdownLibrary();

bool SandboxEnabled();

// A thread pool containing only one thread to execute the cubeb operations. We
// should always use this thread to init, destroy, start, or stop cubeb streams,
// to avoid data racing or deadlock issues across platforms.
already_AddRefed<SharedThreadPool> GetCubebOperationThread();

// Returns the maximum number of channels supported by the audio hardware.
uint32_t MaxNumberOfChannels();

// Get the sample rate the hardware/mixer runs at. Thread safe.
uint32_t PreferredSampleRate(bool aShouldResistFingerprinting);

// Initialize a cubeb stream. A pass through wrapper for cubeb_stream_init,
// that can simulate streams that are very slow to start, by setting the pref
// media.cubeb.slow_stream_init_ms.
int CubebStreamInit(cubeb* context, cubeb_stream** stream,
                    char const* stream_name, cubeb_devid input_device,
                    cubeb_stream_params* input_stream_params,
                    cubeb_devid output_device,
                    cubeb_stream_params* output_stream_params,
                    uint32_t latency_frames, cubeb_data_callback data_callback,
                    cubeb_state_callback state_callback, void* user_ptr);

enum Side { Input = 1 << 0, Output = 1 << 1 };

double GetVolumeScale();
bool GetFirstStream();
RefPtr<CubebHandle> GetCubeb();
void ReportCubebStreamInitFailure(bool aIsFirstStream);
void ReportCubebBackendUsed();
uint32_t GetCubebPlaybackLatencyInMilliseconds();
uint32_t GetCubebMTGLatencyInFrames(cubeb_stream_params* params);
bool CubebLatencyPrefSet();
void GetCurrentBackend(nsAString& aBackend);
cubeb_stream_prefs GetDefaultStreamPrefs(cubeb_device_type aType);
char* GetForcedOutputDevice();
// No-op on all platforms but Android, where it tells the device's AudioManager
// to switch to "communication mode", which might change audio routing,
// bluetooth communication type, etc.
void SetInCommunication(bool aInCommunication);
// Returns true if the output streams should be routed like a stream containing
// voice data, and not generic audio. This can influence audio processing and
// device selection.
bool RouteOutputAsVoice();
// Returns, in seconds, the roundtrip latency Gecko thinks there is between the
// default input and output devices. This is for diagnosing purposes, the
// latency figures are best used directly from the cubeb streams themselves, as
// the devices being used matter. This is blocking.
bool EstimatedLatencyDefaultDevices(
    double* aMean, double* aStdDev,
    Side aSide = static_cast<Side>(Side::Input | Side::Output));

#  ifdef MOZ_WIDGET_ANDROID
int32_t AndroidGetAudioOutputSampleRate();
int32_t AndroidGetAudioOutputFramesPerBuffer();
#  endif

#  ifdef ENABLE_SET_CUBEB_BACKEND
void ForceSetCubebContext(cubeb* aCubebContext);
#  endif
}  // namespace CubebUtils
}  // namespace mozilla

#endif  // CubebUtils_h_
