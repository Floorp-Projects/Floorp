/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PREFS_H
#define GFX_PREFS_H

#include <cmath>  // for M_PI
#include <stdint.h>
#include <string>
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/gfx/LoggingConstants.h"
#include "nsTArray.h"
#include "nsString.h"

// First time gfxPrefs::GetSingleton() needs to be called on the main thread,
// before any of the methods accessing the values are used, but after
// the Preferences system has been initialized.

// The static methods to access the preference value are safe to call
// from any thread after that first call.

// To register a preference, you need to add a line in this file using
// the DECL_GFX_PREF macro.
//
// Update argument controls whether we read the preference value and save it
// or connect with a callback.  See UpdatePolicy enum below.
// Pref is the string with the preference name.
// Name argument is the name of the static function to create.
// Type is the type of the preference - bool, int32_t, uint32_t.
// Default is the default value for the preference.
//
// For example this line in the .h:
//   DECL_GFX_PREF(Once,"layers.dump",LayersDump,bool,false);
// means that you can call
//   bool var = gfxPrefs::LayersDump();
// from any thread, but that you will only get the preference value of
// "layers.dump" as it was set at the start of the session (subject to
// note 2 below). If the value was not set, the default would be false.
//
// In another example, this line in the .h:
//   DECL_GFX_PREF(Live,"gl.msaa-level",MSAALevel,uint32_t,2);
// means that every time you call
//   uint32_t var = gfxPrefs::MSAALevel();
// from any thread, you will get the most up to date preference value of
// "gl.msaa-level".  If the value is not set, the default would be 2.

// Note 1: Changing a preference from Live to Once is now as simple
// as changing the Update argument.  If your code worked before, it will
// keep working, and behave as if the user never changes the preference.
// Things are a bit more complicated and perhaps even dangerous when
// going from Once to Live, or indeed setting a preference to be Live
// in the first place, so be careful.  You need to be ready for the
// values changing mid execution, and if you're using those preferences
// in any setup and initialization, you may need to do extra work.

// Note 2: Prefs can be set by using the corresponding Set method. For
// example, if the accessor is Foo() then calling SetFoo(...) will update
// the preference and also change the return value of subsequent Foo() calls.
// This is true even for 'Once' prefs which otherwise do not change if the
// pref is updated after initialization. Changing gfxPrefs values in content
// processes will not affect the result in other processes. Changing gfxPrefs
// values in the GPU process is not supported at all.

#define DECL_GFX_PREF(Update, Prefname, Name, Type, Default)              \
 public:                                                                  \
  static Type Name() {                                                    \
    MOZ_ASSERT(SingletonExists());                                        \
    return GetSingleton().mPref##Name.mValue;                             \
  }                                                                       \
  static void Set##Name(Type aVal) {                                      \
    MOZ_ASSERT(SingletonExists());                                        \
    GetSingleton().mPref##Name.Set(UpdatePolicy::Update,                  \
                                   Get##Name##PrefName(), aVal);          \
  }                                                                       \
  static const char* Get##Name##PrefName() { return Prefname; }           \
  static Type Get##Name##PrefDefault() { return Default; }                \
  static void Set##Name##ChangeCallback(Pref::ChangeCallback aCallback) { \
    MOZ_ASSERT(SingletonExists());                                        \
    GetSingleton().mPref##Name.SetChangeCallback(aCallback);              \
  }                                                                       \
                                                                          \
 private:                                                                 \
  PrefTemplate<UpdatePolicy::Update, Type, Get##Name##PrefDefault,        \
               Get##Name##PrefName>                                       \
      mPref##Name

// This declares an "override" pref, which is exposed as a "bool" pref by the
// API, but is internally stored as a tri-state int pref with three possible
// values:
// - A value of 0 means that it has been force-disabled, and is exposed as a
//   false-valued bool.
// - A value of 1 means that it has been force-enabled, and is exposed as a
//   true-valued bool.
// - A value of 2 (the default) means that it returns the provided BaseValue
//   as a boolean. The BaseValue may be a constant expression or a function.
// If the prefs defined with this macro are listed in prefs files (e.g. all.js),
// then they must be listed with an int value (default to 2, but you can use 0
// or 1 if you want to force it on or off).
#define DECL_OVERRIDE_PREF(Update, Prefname, Name, BaseValue)             \
 public:                                                                  \
  static bool Name() {                                                    \
    MOZ_ASSERT(SingletonExists());                                        \
    int32_t val = GetSingleton().mPref##Name.mValue;                      \
    return val == 2 ? !!(BaseValue) : !!val;                              \
  }                                                                       \
  static void Set##Name(bool aVal) {                                      \
    MOZ_ASSERT(SingletonExists());                                        \
    GetSingleton().mPref##Name.Set(UpdatePolicy::Update,                  \
                                   Get##Name##PrefName(), aVal ? 1 : 0);  \
  }                                                                       \
  static const char* Get##Name##PrefName() { return Prefname; }           \
  static int32_t Get##Name##PrefDefault() { return 2; }                   \
  static void Set##Name##ChangeCallback(Pref::ChangeCallback aCallback) { \
    MOZ_ASSERT(SingletonExists());                                        \
    GetSingleton().mPref##Name.SetChangeCallback(aCallback);              \
  }                                                                       \
                                                                          \
 private:                                                                 \
  PrefTemplate<UpdatePolicy::Update, int32_t, Get##Name##PrefDefault,     \
               Get##Name##PrefName>                                       \
      mPref##Name

namespace mozilla {
namespace gfx {
class GfxPrefValue;  // defined in PGPU.ipdl
}  // namespace gfx
}  // namespace mozilla

class gfxPrefs;
class gfxPrefs final {
  typedef mozilla::gfx::GfxPrefValue GfxPrefValue;

  typedef mozilla::Atomic<bool, mozilla::Relaxed> AtomicBool;
  typedef mozilla::Atomic<int32_t, mozilla::Relaxed> AtomicInt32;
  typedef mozilla::Atomic<uint32_t, mozilla::Relaxed> AtomicUint32;

 private:
  // Enums for the update policy.
  enum class UpdatePolicy {
    Skip,  // Set the value to default, skip any Preferences calls
    Once,  // Evaluate the preference once, unchanged during the session
    Live   // Evaluate the preference and set callback so it stays current/live
  };

 public:
  class Pref {
   public:
    Pref() : mChangeCallback(nullptr) {
      mIndex = sGfxPrefList->Length();
      sGfxPrefList->AppendElement(this);
    }

    size_t Index() const { return mIndex; }
    void OnChange();

    typedef void (*ChangeCallback)(const GfxPrefValue&);
    void SetChangeCallback(ChangeCallback aCallback);

    virtual const char* Name() const = 0;

    // Returns true if the value is default, false if changed.
    virtual bool HasDefaultValue() const = 0;

    // Returns the pref value as a discriminated union.
    virtual void GetLiveValue(GfxPrefValue* aOutValue) const = 0;

    // Returns the pref value as a discriminated union.
    virtual void GetCachedValue(GfxPrefValue* aOutValue) const = 0;

    // Change the cached value. GfxPrefValue must be a compatible type.
    virtual void SetCachedValue(const GfxPrefValue& aOutValue) = 0;

   protected:
    void FireChangeCallback();

   private:
    size_t mIndex;
    ChangeCallback mChangeCallback;
  };

  static const nsTArray<Pref*>& all() { return *sGfxPrefList; }

 private:
  // We split out a base class to reduce the number of virtual function
  // instantiations that we do, which saves code size.
  template <class T>
  class TypedPref : public Pref {
   public:
    explicit TypedPref(T aValue) : mValue(aValue) {}

    void GetCachedValue(GfxPrefValue* aOutValue) const override {
      CopyPrefValue(&mValue, aOutValue);
    }
    void SetCachedValue(const GfxPrefValue& aOutValue) override {
      T newValue;
      CopyPrefValue(&aOutValue, &newValue);

      if (mValue != newValue) {
        mValue = newValue;
        FireChangeCallback();
      }
    }

   protected:
    T GetLiveValueByName(const char* aPrefName) const {
      if (IsPrefsServiceAvailable()) {
        return PrefGet(aPrefName, mValue);
      }
      return mValue;
    }

   public:
    T mValue;
  };

  // Since we cannot use const char*, use a function that returns it.
  template <UpdatePolicy Update, class T, T Default(void),
            const char* Prefname(void)>
  class PrefTemplate final : public TypedPref<T> {
    typedef TypedPref<T> BaseClass;

   public:
    PrefTemplate() : BaseClass(Default()) {
      // If not using the Preferences service, values are synced over IPC, so
      // there's no need to register us as a Preferences observer.
      if (IsPrefsServiceAvailable()) {
        Register(Update, Prefname());
      }
      // By default we only watch changes in the parent process, to communicate
      // changes to the GPU process.
      if (IsParentProcess() && Update == UpdatePolicy::Live) {
        WatchChanges(Prefname(), this);
      }
    }
    ~PrefTemplate() {
      if (IsParentProcess() && Update == UpdatePolicy::Live) {
        UnwatchChanges(Prefname(), this);
      }
    }
    void Register(UpdatePolicy aUpdate, const char* aPreference) {
      AssertMainThread();
      switch (aUpdate) {
        case UpdatePolicy::Skip:
          break;
        case UpdatePolicy::Once:
          this->mValue = PrefGet(aPreference, this->mValue);
          break;
        case UpdatePolicy::Live: {
          nsCString pref;
          pref.AssignLiteral(aPreference, strlen(aPreference));
          PrefAddVarCache(&this->mValue, pref, this->mValue);
        } break;
        default:
          MOZ_CRASH("Incomplete switch");
      }
    }
    void Set(UpdatePolicy aUpdate, const char* aPref, T aValue) {
      AssertMainThread();
      PrefSet(aPref, aValue);
      switch (aUpdate) {
        case UpdatePolicy::Skip:
        case UpdatePolicy::Live:
          break;
        case UpdatePolicy::Once:
          this->mValue = PrefGet(aPref, this->mValue);
          break;
        default:
          MOZ_CRASH("Incomplete switch");
      }
    }
    const char* Name() const override { return Prefname(); }
    void GetLiveValue(GfxPrefValue* aOutValue) const override {
      T value = GetLiveValue();
      CopyPrefValue(&value, aOutValue);
    }
    // When using the Preferences service, the change callback can be triggered
    // *before* our cached value is updated, so we expose a method to grab the
    // true live value.
    T GetLiveValue() const { return BaseClass::GetLiveValueByName(Prefname()); }
    bool HasDefaultValue() const override { return this->mValue == Default(); }
  };

  // clang-format off

  // This is where DECL_GFX_PREF for each of the preferences should go.  We
  // will keep these in an alphabetical order to make it easier to see if a
  // method accessing a pref already exists. Just add yours in the list.

  // Note that        "gfx.logging.level" is defined in Logging.h.
  DECL_GFX_PREF(Live, "gfx.logging.level",                     GfxLoggingLevel, int32_t, mozilla::gfx::LOG_DEFAULT);
  DECL_GFX_PREF(Once, "layers.windowrecording.path",           LayersWindowRecordingPath, std::string, std::string());

  DECL_GFX_PREF(Live, "layout.frame_rate",                     LayoutFrameRate, int32_t, -1);

  // WARNING:
  // Please make sure that you've added your new preference to the list above
  // in alphabetical order.
  // Please do not just append it to the end of the list.

  // clang-format on

 public:
  // Manage the singleton:
  static gfxPrefs& GetSingleton() {
    return sInstance ? *sInstance : CreateAndInitializeSingleton();
  }
  static void DestroySingleton();
  static bool SingletonExists();

 private:
  static gfxPrefs& CreateAndInitializeSingleton();

  static gfxPrefs* sInstance;
  static bool sInstanceHasBeenDestroyed;
  static nsTArray<Pref*>* sGfxPrefList;

 private:
  // The constructor cannot access GetSingleton(), since sInstance (necessarily)
  // has not been assigned yet. Follow-up initialization that needs
  // GetSingleton() must be added to Init().
  void Init();

  static bool IsPrefsServiceAvailable();
  static bool IsParentProcess();
  // Creating these to avoid having to include Preferences.h in the .h
  static void PrefAddVarCache(bool*, const nsACString&, bool);
  static void PrefAddVarCache(int32_t*, const nsACString&, int32_t);
  static void PrefAddVarCache(uint32_t*, const nsACString&, uint32_t);
  static void PrefAddVarCache(float*, const nsACString&, float);
  static void PrefAddVarCache(std::string*, const nsCString&, std::string);
  static void PrefAddVarCache(AtomicBool*, const nsACString&, bool);
  static void PrefAddVarCache(AtomicInt32*, const nsACString&, int32_t);
  static void PrefAddVarCache(AtomicUint32*, const nsACString&, uint32_t);
  static bool PrefGet(const char*, bool);
  static int32_t PrefGet(const char*, int32_t);
  static uint32_t PrefGet(const char*, uint32_t);
  static float PrefGet(const char*, float);
  static std::string PrefGet(const char*, std::string);
  static void PrefSet(const char* aPref, bool aValue);
  static void PrefSet(const char* aPref, int32_t aValue);
  static void PrefSet(const char* aPref, uint32_t aValue);
  static void PrefSet(const char* aPref, float aValue);
  static void PrefSet(const char* aPref, std::string aValue);
  static void WatchChanges(const char* aPrefname, Pref* aPref);
  static void UnwatchChanges(const char* aPrefname, Pref* aPref);
  // Creating these to avoid having to include PGPU.h in the .h
  static void CopyPrefValue(const bool* aValue, GfxPrefValue* aOutValue);
  static void CopyPrefValue(const int32_t* aValue, GfxPrefValue* aOutValue);
  static void CopyPrefValue(const uint32_t* aValue, GfxPrefValue* aOutValue);
  static void CopyPrefValue(const float* aValue, GfxPrefValue* aOutValue);
  static void CopyPrefValue(const std::string* aValue, GfxPrefValue* aOutValue);
  static void CopyPrefValue(const GfxPrefValue* aValue, bool* aOutValue);
  static void CopyPrefValue(const GfxPrefValue* aValue, int32_t* aOutValue);
  static void CopyPrefValue(const GfxPrefValue* aValue, uint32_t* aOutValue);
  static void CopyPrefValue(const GfxPrefValue* aValue, float* aOutValue);
  static void CopyPrefValue(const GfxPrefValue* aValue, std::string* aOutValue);

  static void AssertMainThread();

  // Some wrapper functions for the DECL_OVERRIDE_PREF prefs' base values, so
  // that we don't to include all sorts of header files into this gfxPrefs.h
  // file.
  static bool OverrideBase_WebRender();

  gfxPrefs();
  ~gfxPrefs();
  gfxPrefs(const gfxPrefs&) = delete;
  gfxPrefs& operator=(const gfxPrefs&) = delete;
};

#undef DECL_GFX_PREF /* Don't need it outside of this file */

#endif /* GFX_PREFS_H */
