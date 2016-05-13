/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaPrefs.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "MainThreadUtils.h"

namespace mozilla {

StaticAutoPtr<MediaPrefs> MediaPrefs::sInstance;

MediaPrefs&
MediaPrefs::GetSingleton()
{
  if (!sInstance) {
    sInstance = new MediaPrefs;
    ClearOnShutdown(&sInstance);
  }
  MOZ_ASSERT(SingletonExists());
  return *sInstance;
}

bool
MediaPrefs::SingletonExists()
{
  return sInstance != nullptr;
}

MediaPrefs::MediaPrefs()
{
  MediaPrefs::AssertMainThread();
}

void MediaPrefs::AssertMainThread()
{
  MOZ_ASSERT(NS_IsMainThread(), "this code must be run on the main thread");
}

void MediaPrefs::PrefAddVarCache(bool* aVariable,
                                 const char* aPref,
                                 bool aDefault)
{
  Preferences::AddBoolVarCache(aVariable, aPref, aDefault);
}

void MediaPrefs::PrefAddVarCache(int32_t* aVariable,
                                 const char* aPref,
                                 int32_t aDefault)
{
  Preferences::AddIntVarCache(aVariable, aPref, aDefault);
}

void MediaPrefs::PrefAddVarCache(uint32_t* aVariable,
                                 const char* aPref,
                                 uint32_t aDefault)
{
  Preferences::AddUintVarCache(aVariable, aPref, aDefault);
}

void MediaPrefs::PrefAddVarCache(float* aVariable,
                                 const char* aPref,
                                 float aDefault)
{
  Preferences::AddFloatVarCache(aVariable, aPref, aDefault);
}

} // namespace mozilla
