/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_PREFS_H
#define MEDIA_PREFS_H

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#include "mozilla/Atomics.h"

// First time MediaPrefs::GetSingleton() needs to be called on the main thread,
// before any of the methods accessing the values are used, but after
// the Preferences system has been initialized.

// The static methods to access the preference value are safe to call
// from any thread after that first call.

// To register a preference, you need to add a line in this file using
// the DECL_MEDIA_PREF macro.
//
// For example this line in the .h:
//   DECL_MEDIA_PREF("media.resampling.enabled",AudioSinkResampling,bool,false);
// means that you can call
//   const bool& var = MediaPrefs::AudioSinkResampling();
// from any thread, you will get the most up to date preference value of
// "media.resampling.enabled".  If the value is not set, the default would be
// false.

#define DECL_MEDIA_PREF(Pref, Name, Type, Default)                            \
public:                                                                       \
static const Type& Name() { MOZ_ASSERT(SingletonExists()); return GetSingleton().mPref##Name.mValue; } \
private:                                                                      \
static const char* Get##Name##PrefName() { return Pref; }                     \
static StripAtomic<Type> Get##Name##PrefDefault() { return Default; }         \
PrefTemplate<Type, Get##Name##PrefDefault, Get##Name##PrefName> mPref##Name

// Custom Definitions.
#define GMP_DEFAULT_ASYNC_SHUTDOWN_TIMEOUT 3000
#define SUSPEND_BACKGROUND_VIDEO_DELAY_MS 10000
#define TEST_PREFERENCE_FAKE_RECOGNITION_SERVICE "media.webspeech.test.fake_recognition_service"

namespace mozilla {

template<class T> class StaticAutoPtr;

class MediaPrefs final
{
  typedef Atomic<uint32_t, Relaxed> AtomicUint32;

  template <typename T>
  struct StripAtomicImpl {
    typedef T Type;
  };

  template <typename T, MemoryOrdering Order>
  struct StripAtomicImpl<Atomic<T, Order>> {
    typedef T Type;
  };

  template <typename T>
  using StripAtomic = typename StripAtomicImpl<T>::Type;

private:
  // Since we cannot use const char*, use a function that returns it.
  template <class T, StripAtomic<T> Default(), const char* Pref()>
  class PrefTemplate
  {
  public:
    PrefTemplate()
    : mValue(Default())
    {
      Register(Pref());
    }
    T mValue;
  private:
    void Register(const char* aPreference)
    {
      AssertMainThread();
      PrefAddVarCache(&mValue, aPreference, mValue);
    }
  };

  // This is where DECL_MEDIA_PREF for each of the preferences should go.

  // AudioSink
  DECL_MEDIA_PREF("accessibility.monoaudio.enable",           MonoAudio, bool, false);
  DECL_MEDIA_PREF("media.resampling.enabled",                 AudioSinkResampling, bool, false);
  DECL_MEDIA_PREF("media.resampling.rate",                    AudioSinkResampleRate, uint32_t, 48000);
#if defined(XP_WIN) || defined(XP_DARWIN) || defined(MOZ_PULSEAUDIO)
  // libcubeb backend implement .get_preferred_channel_layout
  DECL_MEDIA_PREF("media.forcestereo.enabled",                AudioSinkForceStereo, bool, false);
#else
  DECL_MEDIA_PREF("media.forcestereo.enabled",                AudioSinkForceStereo, bool, true);
#endif
  // VideoSink
  DECL_MEDIA_PREF("media.ruin-av-sync.enabled",               RuinAvSync, bool, false);

  // Encrypted Media Extensions
  DECL_MEDIA_PREF("media.clearkey.persistent-license.enabled", ClearKeyPersistentLicenseEnabled, bool, false);

  // PlatformDecoderModule
  DECL_MEDIA_PREF("media.apple.forcevda",                     AppleForceVDA, bool, false);
  DECL_MEDIA_PREF("media.gmp.insecure.allow",                 GMPAllowInsecure, bool, false);
  DECL_MEDIA_PREF("media.eme.enabled",                        EMEEnabled, bool, false);
  DECL_MEDIA_PREF("media.use-blank-decoder",                  PDMUseBlankDecoder, bool, false);
  DECL_MEDIA_PREF("media.gpu-process-decoder",                PDMUseGPUDecoder, bool, false);
#ifdef MOZ_GONK_MEDIACODEC
  DECL_MEDIA_PREF("media.gonk.enabled",                       PDMGonkDecoderEnabled, bool, true);
#endif
#ifdef MOZ_WIDGET_ANDROID
  DECL_MEDIA_PREF("media.android-media-codec.enabled",        PDMAndroidMediaCodecEnabled, bool, false);
  DECL_MEDIA_PREF("media.android-media-codec.preferred",      PDMAndroidMediaCodecPreferred, bool, false);
  DECL_MEDIA_PREF("media.navigator.hardware.vp8_encode.acceleration_remote_enabled", RemoteMediaCodecVP8EncoderEnabled, bool, false);
#endif
#ifdef MOZ_FFMPEG
  DECL_MEDIA_PREF("media.ffmpeg.enabled",                     PDMFFmpegEnabled, bool, true);
  DECL_MEDIA_PREF("media.libavcodec.allow-obsolete",          LibavcodecAllowObsolete, bool, false);
#endif
#if defined(MOZ_FFMPEG) || defined(MOZ_FFVPX)
  DECL_MEDIA_PREF("media.ffmpeg.low-latency.enabled",         PDMFFmpegLowLatencyEnabled, bool, false);
#endif
#ifdef MOZ_FFVPX
  DECL_MEDIA_PREF("media.ffvpx.enabled",                      PDMFFVPXEnabled, bool, true);
#endif
#ifdef XP_WIN
  DECL_MEDIA_PREF("media.wmf.enabled",                        PDMWMFEnabled, bool, true);
  DECL_MEDIA_PREF("media.wmf.skip-blacklist",                 PDMWMFSkipBlacklist, bool, false);
  DECL_MEDIA_PREF("media.decoder-doctor.wmf-disabled-is-failure", DecoderDoctorWMFDisabledIsFailure, bool, false);
  DECL_MEDIA_PREF("media.wmf.vp9.enabled",                    PDMWMFVP9DecoderEnabled, bool, true);
  DECL_MEDIA_PREF("media.wmf.decoder.thread-count",           PDMWMFThreadCount, int32_t, -1);
  DECL_MEDIA_PREF("media.wmf.allow-unsupported-resolutions",  PDMWMFAllowUnsupportedResolutions, bool, false);
#endif
  DECL_MEDIA_PREF("media.decoder.fuzzing.enabled",            PDMFuzzingEnabled, bool, false);
  DECL_MEDIA_PREF("media.decoder.fuzzing.video-output-minimum-interval-ms", PDMFuzzingInterval, uint32_t, 0);
  DECL_MEDIA_PREF("media.decoder.fuzzing.dont-delay-inputexhausted", PDMFuzzingDelayInputExhausted, bool, true);
  DECL_MEDIA_PREF("media.decoder.recycle.enabled",            MediaDecoderCheckRecycling, bool, false);
  DECL_MEDIA_PREF("media.gmp.decoder.enabled",                PDMGMPEnabled, bool, true);
  DECL_MEDIA_PREF("media.gmp.decoder.aac",                    GMPAACPreferred, uint32_t, 0);
  DECL_MEDIA_PREF("media.gmp.decoder.h264",                   GMPH264Preferred, uint32_t, 0);
  DECL_MEDIA_PREF("media.eme.audio.blank",                    EMEBlankAudio, bool, false);
  DECL_MEDIA_PREF("media.eme.video.blank",                    EMEBlankVideo, bool, false);
  DECL_MEDIA_PREF("media.eme.chromium-api.enabled",           EMEChromiumAPIEnabled, bool, false);
  DECL_MEDIA_PREF("media.eme.chromium-api.video-shmems",
                  EMEChromiumAPIVideoShmemCount,
                  uint32_t,
                  3);

  // MediaDecoderStateMachine
  DECL_MEDIA_PREF("media.suspend-bkgnd-video.enabled",        MDSMSuspendBackgroundVideoEnabled, bool, false);
  DECL_MEDIA_PREF("media.suspend-bkgnd-video.delay-ms",       MDSMSuspendBackgroundVideoDelay, AtomicUint32, SUSPEND_BACKGROUND_VIDEO_DELAY_MS);
  DECL_MEDIA_PREF("media.dormant-on-pause-timeout-ms",        DormantOnPauseTimeout, int32_t, 5000);

  // WebSpeech
  DECL_MEDIA_PREF("media.webspeech.synth.force_global_queue", WebSpeechForceGlobal, bool, false);
  DECL_MEDIA_PREF("media.webspeech.test.enable",              WebSpeechTestEnabled, bool, false);
  DECL_MEDIA_PREF("media.webspeech.test.fake_fsm_events",     WebSpeechFakeFSMEvents, bool, false);
  DECL_MEDIA_PREF(TEST_PREFERENCE_FAKE_RECOGNITION_SERVICE,   WebSpeechFakeRecognitionService, bool, false);
  DECL_MEDIA_PREF("media.webspeech.recognition.enable",       WebSpeechRecognitionEnabled, bool, false);
  DECL_MEDIA_PREF("media.webspeech.recognition.force_enable", WebSpeechRecognitionForceEnabled, bool, false);

  DECL_MEDIA_PREF("media.num-decode-threads",                 MediaThreadPoolDefaultCount, uint32_t, 4);
  DECL_MEDIA_PREF("media.decoder.limit",                      MediaDecoderLimit, int32_t, MediaDecoderLimitDefault());

  // Ogg
  DECL_MEDIA_PREF("media.ogg.enabled",                        OggEnabled, bool, true);
  // Flac
  DECL_MEDIA_PREF("media.ogg.flac.enabled",                   FlacInOgg, bool, false);
  DECL_MEDIA_PREF("media.flac.enabled",                       FlacEnabled, bool, true);

#if !defined(RELEASE_OR_BETA)
  DECL_MEDIA_PREF("media.rust.test_mode",                     RustTestMode, bool, false);
#endif

#if defined(MOZ_WIDGET_GTK)
  DECL_MEDIA_PREF("media.rust.mp4parser",                     EnableRustMP4Parser, bool, true);
#else
  DECL_MEDIA_PREF("media.rust.mp4parser",                     EnableRustMP4Parser, bool, false);
#endif

  // Error/warning handling, Decoder Doctor
  DECL_MEDIA_PREF("media.playback.warnings-as-errors",        MediaWarningsAsErrors, bool, false);
  DECL_MEDIA_PREF("media.playback.warnings-as-errors.stagefright-vs-rust",
                                                              MediaWarningsAsErrorsStageFrightVsRust, bool, false);

public:
  // Manage the singleton:
  static MediaPrefs& GetSingleton();
  static bool SingletonExists();

private:
  template<class T> friend class StaticAutoPtr;
  static StaticAutoPtr<MediaPrefs> sInstance;

  // Default value functions
  static int32_t MediaDecoderLimitDefault()
  {
#ifdef MOZ_WIDGET_ANDROID
    if (AndroidBridge::Bridge() &&
        AndroidBridge::Bridge()->GetAPIVersion() < 18) {
      // Older Android versions have broken support for multiple simultaneous
      // decoders, see bug 1278574.
      return 1;
    }
#endif
    // Otherwise, set no decoder limit.
    return -1;
  }

  // Creating these to avoid having to include Preferences.h in the .h
  static void PrefAddVarCache(bool*, const char*, bool);
  static void PrefAddVarCache(int32_t*, const char*, int32_t);
  static void PrefAddVarCache(uint32_t*, const char*, uint32_t);
  static void PrefAddVarCache(float*, const char*, float);
  static void PrefAddVarCache(AtomicUint32*, const char*, uint32_t);

  static void AssertMainThread();

  MediaPrefs();
  MediaPrefs(const MediaPrefs&) = delete;
  MediaPrefs& operator=(const MediaPrefs&) = delete;
};

#undef DECL_MEDIA_PREF /* Don't need it outside of this file */

} // namespace mozilla

#endif /* MEDIA_PREFS_H */
