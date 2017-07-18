/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(CubebUtils_h_)
#define CubebUtils_h_

#include "cubeb/cubeb.h"
#include "mozilla/dom/AudioDeviceInfo.h"
#include "mozilla/dom/AudioChannelBinding.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace CubebUtils {

typedef cubeb_devid AudioDeviceID;

// Initialize Audio Library. Some Audio backends require initializing the
// library before using it.
void InitLibrary();

// Shutdown Audio Library. Some Audio backends require shutting down the
// library after using it.
void ShutdownLibrary();

// Returns the maximum number of channels supported by the audio hardware.
uint32_t MaxNumberOfChannels();

// Get the sample rate the hardware/mixer runs at. Thread safe.
uint32_t PreferredSampleRate();

// Get the bit mask of the connected audio device's preferred layout.
uint32_t PreferredChannelMap(uint32_t aChannels);

enum Side {
  Input,
  Output
};

void PrefChanged(const char* aPref, void* aClosure);
double GetVolumeScale();
bool GetFirstStream();
cubeb* GetCubebContext();
cubeb* GetCubebContextUnlocked();
void ReportCubebStreamInitFailure(bool aIsFirstStream);
void ReportCubebBackendUsed();
uint32_t GetCubebPlaybackLatencyInMilliseconds();
Maybe<uint32_t> GetCubebMSGLatencyInFrames();
bool CubebLatencyPrefSet();
cubeb_channel_layout ConvertChannelMapToCubebLayout(uint32_t aChannelMap);
#if defined(__ANDROID__) && defined(MOZ_B2G)
cubeb_stream_type ConvertChannelToCubebType(dom::AudioChannel aChannel);
#endif
void GetCurrentBackend(nsAString& aBackend);
void GetPreferredChannelLayout(nsAString& aLayout);
void GetDeviceCollection(nsTArray<RefPtr<AudioDeviceInfo>>& aDeviceInfos,
                         Side aSide);
} // namespace CubebUtils
} // namespace mozilla

#endif // CubebUtils_h_
