/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPrefs.h"

#include "MainThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/GPUChild.h"
#include "mozilla/gfx/GPUProcessManager.h"

using namespace mozilla;

nsTArray<gfxPrefs::Pref*>* gfxPrefs::sGfxPrefList = nullptr;
gfxPrefs* gfxPrefs::sInstance = nullptr;
bool gfxPrefs::sInstanceHasBeenDestroyed = false;

void
gfxPrefs::DestroySingleton()
{
  if (sInstance) {
    delete sInstance;
    sInstance = nullptr;
    sInstanceHasBeenDestroyed = true;
  }
  MOZ_ASSERT(!SingletonExists());
}

bool
gfxPrefs::SingletonExists()
{
  return sInstance != nullptr;
}

gfxPrefs::gfxPrefs()
{
  // UI, content, and plugin processes use XPCOM and should have prefs
  // ready by the time we initialize gfxPrefs.
  MOZ_ASSERT_IF(XRE_IsContentProcess() ||
                XRE_IsParentProcess() ||
                XRE_GetProcessType() == GeckoProcessType_Plugin,
                Preferences::IsServiceAvailable());

  gfxPrefs::AssertMainThread();
}

void
gfxPrefs::Init()
{
  // Set up Moz2D prefs.
  mPrefGfxLoggingLevel.SetChangeCallback([]() -> void {
    mozilla::gfx::LoggingPrefs::sGfxLogLevel = GetSingleton().mPrefGfxLoggingLevel.GetLiveValue();
  });
}

gfxPrefs::~gfxPrefs()
{
  gfxPrefs::AssertMainThread();
  mPrefGfxLoggingLevel.SetChangeCallback(nullptr);
  delete sGfxPrefList;
  sGfxPrefList = nullptr;
}

void gfxPrefs::AssertMainThread()
{
  MOZ_ASSERT(NS_IsMainThread(), "this code must be run on the main thread");
}

void
gfxPrefs::Pref::OnChange()
{
  if (auto gpm = gfx::GPUProcessManager::Get()) {
    if (gfx::GPUChild* gpu = gpm->GetGPUChild()) {
      GfxPrefValue value;
      GetCachedValue(&value);
      Unused << gpu->SendUpdatePref(gfx::GfxPrefSetting(mIndex, value));
    }
  }
  FireChangeCallback();
}

void
gfxPrefs::Pref::FireChangeCallback()
{
  if (mChangeCallback) {
    mChangeCallback();
  }
}

void
gfxPrefs::Pref::SetChangeCallback(ChangeCallback aCallback)
{
  mChangeCallback = aCallback;

  if (!IsParentProcess() && IsPrefsServiceAvailable()) {
    // If we're in the parent process, we watch prefs by default so we can
    // send changes over to the GPU process. Otherwise, we need to add or
    // remove a watch for the pref now.
    if (aCallback) {
      WatchChanges(Name(), this);
    } else {
      UnwatchChanges(Name(), this);
    }
  }

  // Fire the callback once to make initialization easier for the caller.
  FireChangeCallback();
}

// On lightweight processes such as for GMP and GPU, XPCOM is not initialized,
// and therefore we don't have access to Preferences. When XPCOM is not
// available we rely on manual synchronization of gfxPrefs values over IPC.
/* static */ bool
gfxPrefs::IsPrefsServiceAvailable()
{
  return Preferences::IsServiceAvailable();
}

/* static */ bool
gfxPrefs::IsParentProcess()
{
  return XRE_IsParentProcess();
}

void gfxPrefs::PrefAddVarCache(bool* aVariable,
                               const char* aPref,
                               bool aDefault)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  Preferences::AddBoolVarCache(aVariable, aPref, aDefault);
}

void gfxPrefs::PrefAddVarCache(int32_t* aVariable,
                               const char* aPref,
                               int32_t aDefault)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  Preferences::AddIntVarCache(aVariable, aPref, aDefault);
}

void gfxPrefs::PrefAddVarCache(uint32_t* aVariable,
                               const char* aPref,
                               uint32_t aDefault)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  Preferences::AddUintVarCache(aVariable, aPref, aDefault);
}

void gfxPrefs::PrefAddVarCache(float* aVariable,
                               const char* aPref,
                               float aDefault)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  Preferences::AddFloatVarCache(aVariable, aPref, aDefault);
}

bool gfxPrefs::PrefGet(const char* aPref, bool aDefault)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  return Preferences::GetBool(aPref, aDefault);
}

int32_t gfxPrefs::PrefGet(const char* aPref, int32_t aDefault)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  return Preferences::GetInt(aPref, aDefault);
}

uint32_t gfxPrefs::PrefGet(const char* aPref, uint32_t aDefault)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  return Preferences::GetUint(aPref, aDefault);
}

float gfxPrefs::PrefGet(const char* aPref, float aDefault)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  return Preferences::GetFloat(aPref, aDefault);
}

void gfxPrefs::PrefSet(const char* aPref, bool aValue)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  Preferences::SetBool(aPref, aValue);
}

void gfxPrefs::PrefSet(const char* aPref, int32_t aValue)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  Preferences::SetInt(aPref, aValue);
}

void gfxPrefs::PrefSet(const char* aPref, uint32_t aValue)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  Preferences::SetUint(aPref, aValue);
}

void gfxPrefs::PrefSet(const char* aPref, float aValue)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  Preferences::SetFloat(aPref, aValue);
}

static void
OnGfxPrefChanged(const char* aPrefname, void* aClosure)
{
  reinterpret_cast<gfxPrefs::Pref*>(aClosure)->OnChange();
}

void gfxPrefs::WatchChanges(const char* aPrefname, Pref* aPref)
{
  MOZ_ASSERT(IsPrefsServiceAvailable());
  Preferences::RegisterCallback(OnGfxPrefChanged, aPrefname, aPref, Preferences::ExactMatch);
}

void gfxPrefs::UnwatchChanges(const char* aPrefname, Pref* aPref)
{
  // The Preferences service can go offline before gfxPrefs is destroyed.
  if (IsPrefsServiceAvailable()) {
    Preferences::UnregisterCallback(OnGfxPrefChanged, aPrefname, aPref, Preferences::ExactMatch);
  }
}

void gfxPrefs::CopyPrefValue(const bool* aValue, GfxPrefValue* aOutValue)
{
  *aOutValue = *aValue;
}

void gfxPrefs::CopyPrefValue(const int32_t* aValue, GfxPrefValue* aOutValue)
{
  *aOutValue = *aValue;
}

void gfxPrefs::CopyPrefValue(const uint32_t* aValue, GfxPrefValue* aOutValue)
{
  *aOutValue = *aValue;
}

void gfxPrefs::CopyPrefValue(const float* aValue, GfxPrefValue* aOutValue)
{
  *aOutValue = *aValue;
}

void gfxPrefs::CopyPrefValue(const GfxPrefValue* aValue, bool* aOutValue)
{
  *aOutValue = aValue->get_bool();
}

void gfxPrefs::CopyPrefValue(const GfxPrefValue* aValue, int32_t* aOutValue)
{
  *aOutValue = aValue->get_int32_t();
}

void gfxPrefs::CopyPrefValue(const GfxPrefValue* aValue, uint32_t* aOutValue)
{
  *aOutValue = aValue->get_uint32_t();
}

void gfxPrefs::CopyPrefValue(const GfxPrefValue* aValue, float* aOutValue)
{
  *aOutValue = aValue->get_float();
}
