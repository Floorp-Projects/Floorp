/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(CubebUtils_h_)
#define CubebUtils_h_

#include "cubeb/cubeb.h"
#include "nsAutoRef.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/dom/AudioChannelBinding.h"

template <>
class nsAutoRefTraits<cubeb_stream> : public nsPointerRefTraits<cubeb_stream>
{
public:
  static void Release(cubeb_stream* aStream) { cubeb_stream_destroy(aStream); }
};

namespace mozilla {

class CubebUtils {
public:
  // Initialize Audio Library. Some Audio backends require initializing the
  // library before using it.
  static void InitLibrary();

  // Shutdown Audio Library. Some Audio backends require shutting down the
  // library after using it.
  static void ShutdownLibrary();

  // Returns the maximum number of channels supported by the audio hardware.
  static int MaxNumberOfChannels();

  // Queries the samplerate the hardware/mixer runs at, and stores it.
  // Can be called on any thread. When this returns, it is safe to call
  // PreferredSampleRate without locking.
  static void InitPreferredSampleRate();
  // Get the aformentionned sample rate. Does not lock.
  static int PreferredSampleRate();

  static void PrefChanged(const char* aPref, void* aClosure);
  static double GetVolumeScale();
  static bool GetFirstStream();
  static cubeb* GetCubebContext();
  static cubeb* GetCubebContextUnlocked();
  static uint32_t GetCubebLatency();
  static bool CubebLatencyPrefSet();
#if defined(__ANDROID__) && defined(MOZ_B2G)
  static cubeb_stream_type ConvertChannelToCubebType(dom::AudioChannel aChannel);
#endif

private:
  // This mutex protects the static members below.
  static StaticMutex sMutex;
  static cubeb* sCubebContext;

  // Prefered samplerate, in Hz (characteristic of the
  // hardware/mixer/platform/API used).
  static uint32_t sPreferredSampleRate;

  static double sVolumeScale;
  static uint32_t sCubebLatency;
  static bool sCubebLatencyPrefSet;
};
}



#endif // CubebUtils_h_
