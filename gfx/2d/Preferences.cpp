/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Preferences.h"

#include "mozilla/Assertions.h"
#include "mozilla/Vector.h"

namespace mozilla {
namespace gfx {

static PreferenceAccess* sAccess = nullptr;

struct Int32Pref
{
  const char* name;
  int32_t* varPtr;
};

static Vector<Int32Pref>& Int32Prefs()
{
  static Vector<Int32Pref>* sInt32Prefs = new Vector<Int32Pref>();
  return *sInt32Prefs;
}

/* static */
int32_t
PreferenceAccess::RegisterLivePref(const char* aName, int32_t* aVar,
                                   int32_t aDefault)
{
  Int32Prefs().append(Int32Pref{ aName, aVar });
  return aDefault;
}

/* static */
void
PreferenceAccess::SetAccess(PreferenceAccess* aAccess)
{
  sAccess = aAccess;
  if (!sAccess) {
    return;
  }

#if defined(DEBUG)
  static uint32_t sProvideAccessCount;
  MOZ_ASSERT(!sProvideAccessCount++,
             "ProvideAccess must only be called with non-nullptr once.");
#endif

  for (Int32Pref pref : Int32Prefs()) {
    sAccess->LivePref(pref.name, pref.varPtr, *pref.varPtr);
  }
  Int32Prefs().clearAndFree();
}

} // namespace gfx
} // namespace mozilla