/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPrefs.h"

#include "mozilla/Preferences.h"
#include "MainThreadUtils.h"
#include "mozilla/gfx/Preferences.h"
#include "nsXULAppAPI.h"

using namespace mozilla;

gfxPrefs* gfxPrefs::sInstance = nullptr;
bool gfxPrefs::sInstanceHasBeenDestroyed = false;

class PreferenceAccessImpl : public mozilla::gfx::PreferenceAccess
{
public:
  virtual ~PreferenceAccessImpl();
  virtual void LivePref(const char* aName, int32_t* aVar, int32_t aDefault) override;
};

PreferenceAccessImpl::~PreferenceAccessImpl()
{
}

void PreferenceAccessImpl::LivePref(const char* aName,
                                    int32_t* aVar,
                                    int32_t aDefault)
{
  Preferences::AddIntVarCache(aVar, aName, aDefault);
}

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
  mMoz2DPrefAccess = new PreferenceAccessImpl;
  mozilla::gfx::PreferenceAccess::SetAccess(mMoz2DPrefAccess);
}

gfxPrefs::~gfxPrefs()
{
  gfxPrefs::AssertMainThread();

  // gfxPrefs is a singleton, we can reset this to null once
  // it goes away.
  mozilla::gfx::PreferenceAccess::SetAccess(nullptr);
  delete mMoz2DPrefAccess;
  mMoz2DPrefAccess = nullptr;
}

void gfxPrefs::AssertMainThread()
{
  MOZ_ASSERT(NS_IsMainThread(), "this code must be run on the main thread");
}

// On lightweight processes such as for GMP and GPU, XPCOM is not initialized,
// and therefore we don't have access to Preferences. When XPCOM is not
// available we rely on manual synchronization of gfxPrefs values over IPC.
/* static */ bool
gfxPrefs::IsPrefsServiceAvailable()
{
  return Preferences::IsServiceAvailable();
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

