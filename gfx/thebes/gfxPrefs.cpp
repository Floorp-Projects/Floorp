/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPrefs.h"

#include "mozilla/Preferences.h"
#include "MainThreadUtils.h"
#include "mozilla/gfx/Preferences.h"

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

void gfxPrefs::PrefAddVarCache(bool* aVariable,
                               const char* aPref,
                               bool aDefault)
{
  Preferences::AddBoolVarCache(aVariable, aPref, aDefault);
}

void gfxPrefs::PrefAddVarCache(int32_t* aVariable,
                               const char* aPref,
                               int32_t aDefault)
{
  Preferences::AddIntVarCache(aVariable, aPref, aDefault);
}

void gfxPrefs::PrefAddVarCache(uint32_t* aVariable,
                               const char* aPref,
                               uint32_t aDefault)
{
  Preferences::AddUintVarCache(aVariable, aPref, aDefault);
}

void gfxPrefs::PrefAddVarCache(float* aVariable,
                               const char* aPref,
                               float aDefault)
{
  Preferences::AddFloatVarCache(aVariable, aPref, aDefault);
}

bool gfxPrefs::PrefGet(const char* aPref, bool aDefault)
{
  return Preferences::GetBool(aPref, aDefault);
}

int32_t gfxPrefs::PrefGet(const char* aPref, int32_t aDefault)
{
  return Preferences::GetInt(aPref, aDefault);
}

uint32_t gfxPrefs::PrefGet(const char* aPref, uint32_t aDefault)
{
  return Preferences::GetUint(aPref, aDefault);
}

float gfxPrefs::PrefGet(const char* aPref, float aDefault)
{
  return Preferences::GetFloat(aPref, aDefault);
}

void gfxPrefs::PrefSet(const char* aPref, bool aValue)
{
  Preferences::SetBool(aPref, aValue);
}

void gfxPrefs::PrefSet(const char* aPref, int32_t aValue)
{
  Preferences::SetInt(aPref, aValue);
}

void gfxPrefs::PrefSet(const char* aPref, uint32_t aValue)
{
  Preferences::SetUint(aPref, aValue);
}

void gfxPrefs::PrefSet(const char* aPref, float aValue)
{
  Preferences::SetFloat(aPref, aValue);
}

