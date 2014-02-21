/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PREFS_H
#define GFX_PREFS_H

#include <stdint.h>
#include "mozilla/Assertions.h"
#include "mozilla/TypedEnum.h"

// First time gfxPrefs::One() needs to be called on the main thread,
// before any of the methods accessing the values are used, but after
// the Preferences system has been initialized.

// The static methods to access the preference value are safe to call
// from any thread after that first call.

// To register a preference, you need to add a line in this file using
// the DECL_GFX_PREFS macro.
//
// Update argument controls whether we read the preference value and save it
// or connect with a callback.  See UpdatePolicy enum below.
// Pref is the string with the preference name.
// Name argument is the name of the static function to create.
// Type is the type of the preference - bool, int32_t, uint32_t.
// Default is the default value for the preference.
//
// For example this line in the .h:
//   DECL_GFX_PREFS(Once,"layers.dump",LayersDump,bool,false);
// means that you can call
//   bool var = gfxPrefs::LayersDump();
// from any thread, but that you will only get the preference value of
// "layers.dump" as it was set at the start of the session. If the value
// was not set, the default would be false.
//
// In another example, this line in the .h:
//   DECL_GFX_PREFS(Live,"gl.msaa-level",MSAALevel,uint32_t,2);
// means that every time you call
//   uint32_t var = gfxPrefs::MSAALevel();
// from any thread, you will get the most up to date preference value of
// "gl.msaa-level".  If the value is not set, the default would be 2.

// Note that changing a preference from Live to Once is now as simple
// as changing the Update argument.  If your code worked before, it will
// keep working, and behave as if the user never changes the preference.
// Things are a bit more complicated and perhaps even dangerous when
// going from Once to Live, or indeed setting a preference to be Live
// in the first place, so be careful.  You need to be ready for the
// values changing mid execution, and if you're using those preferences
// in any setup and initialization, you may need to do extra work.

#define DECL_GFX_PREFS(Update, Pref, Name, Type, Default)                     \
public:                                                                       \
static Type Name() { MOZ_ASSERT(Exists()); return One().mPref##Name.mValue; } \
private:                                                                      \
static const char* Get##Name##PrefName() { return Pref; }                     \
PrefTemplate<UpdatePolicy::Update, Type, Default, Get##Name##PrefName> mPref##Name

class gfxPrefs;
class gfxPrefs MOZ_FINAL
{
private:
  // Enums for the update policy.
  MOZ_BEGIN_NESTED_ENUM_CLASS(UpdatePolicy)
    Skip, // Set the value to default, skip any Preferences calls
    Once, // Evaluate the preference once, unchanged during the session
    Live  // Evaluate the preference and set callback so it stays current/live
  MOZ_END_NESTED_ENUM_CLASS(UpdatePolicy)

  // Since we cannot use const char*, use a function that returns it.
    template <MOZ_ENUM_CLASS_ENUM_TYPE(UpdatePolicy) Update, class T, T Default, const char* Pref(void)>
  class PrefTemplate
  {
  public:
    PrefTemplate()
    : mValue(Default)
    {
      Register(Update, Pref());
    }
    void Register(UpdatePolicy aUpdate, const char* aPreference)
    {
      switch(aUpdate) {
        case UpdatePolicy::Skip:
          break;
        case UpdatePolicy::Once:
          mValue = PrefGet(aPreference, mValue);
          break;
        case UpdatePolicy::Live:
          PrefAddVarCache(&mValue,aPreference, mValue);
          break;
        default:
          MOZ_CRASH();
          break;
      }
    }
    T mValue;
  };

public:
  // This is where DECL_GFX_PREFS for each of the preferences should go.
  // We will keep these in an alphabetical order to make it easier to see if
  // a method accessing a pref already exists. Just add yours in the list.

public:
  // Manage the singleton:
  static gfxPrefs& One()
  { 
    if (!sInstance) {
      sInstance = new gfxPrefs;
    }
    return *sInstance;
  }
  static void Destroy();
  static bool Exists();

private:
  static gfxPrefs* sInstance;

private:
  // Creating these to avoid having to include Preferences.h in the .h
  static void PrefAddVarCache(bool*, const char*, bool);
  static void PrefAddVarCache(int32_t*, const char*, int32_t);
  static void PrefAddVarCache(uint32_t*, const char*, uint32_t);
  static bool PrefGet(const char*, bool);
  static int32_t PrefGet(const char*, int32_t);
  static uint32_t PrefGet(const char*, uint32_t);

  gfxPrefs();
  ~gfxPrefs();
  gfxPrefs(const gfxPrefs&) MOZ_DELETE;
  gfxPrefs& operator=(const gfxPrefs&) MOZ_DELETE;
};

#undef DECL_GFX_PREFS /* Don't need it outside of this file */

#endif /* GFX_PREFS_H */
