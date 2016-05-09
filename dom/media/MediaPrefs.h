/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_PREFS_H
#define MEDIA_PREFS_H

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
static Type Get##Name##PrefDefault() { return Default; }                      \
PrefTemplate<Type, Get##Name##PrefDefault, Get##Name##PrefName> mPref##Name

namespace mozilla {

template<class T> class StaticAutoPtr;

class MediaPrefs final
{

private:
  // Since we cannot use const char*, use a function that returns it.
  template <class T, T Default(), const char* Pref()>
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
  // We will keep these in an alphabetical order to make it easier to see if
  // a method accessing a pref already exists. Just add yours in the list.
  DECL_MEDIA_PREF("accessibility.monoaudio.enable", MonoAudio, bool, false);
  DECL_MEDIA_PREF("media.resampling.enabled",       AudioSinkResampling, bool, false);
  DECL_MEDIA_PREF("media.resampling.rate",          AudioSinkResampleRate, uint32_t, 48000);
  DECL_MEDIA_PREF("media.forcestereo.enabled",      AudioSinkForceStereo, bool, true);

  // WARNING:
  // Please make sure that you've added your new preference to the list above in alphabetical order.
  // Please do not just append it to the end of the list.

public:
  // Manage the singleton:
  static MediaPrefs& GetSingleton();
  static bool SingletonExists();

private:
  template<class T> friend class StaticAutoPtr;
  static StaticAutoPtr<MediaPrefs> sInstance;

  // Creating these to avoid having to include Preferences.h in the .h
  static void PrefAddVarCache(bool*, const char*, bool);
  static void PrefAddVarCache(int32_t*, const char*, int32_t);
  static void PrefAddVarCache(uint32_t*, const char*, uint32_t);
  static void PrefAddVarCache(float*, const char*, float);

  static void AssertMainThread();

  MediaPrefs();
  MediaPrefs(const MediaPrefs&) = delete;
  MediaPrefs& operator=(const MediaPrefs&) = delete;
};

#undef DECL_MEDIA_PREF /* Don't need it outside of this file */

} // namespace mozilla

#endif /* MEDIA_PREFS_H */
