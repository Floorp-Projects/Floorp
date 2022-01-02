/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(CubebUtils_h_)
#  define CubebUtils_h_

#  include "cubeb/cubeb.h"
#  include "nsString.h"
#  include "mozilla/RefPtr.h"

class AudioDeviceInfo;

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(cubeb_stream_prefs)

namespace mozilla {

class AudioThreadRegistry;

namespace CubebUtils {

typedef cubeb_devid AudioDeviceID;

// Initialize Audio Library. Some Audio backends require initializing the
// library before using it.
void InitLibrary();

// Shutdown Audio Library. Some Audio backends require shutting down the
// library after using it.
void ShutdownLibrary();

bool SandboxEnabled();

// Returns the global instance of AudioThreadRegistry. Initialized and
// destroying in Init/ShutdownLibrary(), and safe from all threads.
AudioThreadRegistry* GetAudioThreadRegistry();

// Returns the maximum number of channels supported by the audio hardware.
uint32_t MaxNumberOfChannels();

// Get the sample rate the hardware/mixer runs at. Thread safe.
uint32_t PreferredSampleRate();

enum Side { Input, Output };

double GetVolumeScale();
bool GetFirstStream();
cubeb* GetCubebContext();
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
bool EstimatedRoundTripLatencyDefaultDevices(double* aMean, double* aStdDev);

#  ifdef MOZ_WIDGET_ANDROID
uint32_t AndroidGetAudioOutputSampleRate();
uint32_t AndroidGetAudioOutputFramesPerBuffer();
#  endif

#  ifdef ENABLE_SET_CUBEB_BACKEND
void ForceSetCubebContext(cubeb* aCubebContext);
#  endif
}  // namespace CubebUtils
}  // namespace mozilla

#endif  // CubebUtils_h_
