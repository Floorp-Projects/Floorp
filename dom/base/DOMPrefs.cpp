/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMPrefs.h"
#include "mozilla/Atomics.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"

namespace mozilla {
namespace dom {

void
DOMPrefs::Initialize()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Let's cache all the values on the main-thread.
#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  DOMPrefs::DumpEnabled();
#endif

#define DOM_WEBIDL_PREF(name)
#define DOM_UINT32_PREF(name, pref, defaultValue) DOMPrefs::name();

#include "DOMPrefsInternal.h"

#undef DOM_WEBIDL_PREF
#undef DOM_UINT32_PREF
}

/* static */ bool
DOMPrefs::DumpEnabled()
{
#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  return StaticPrefs::browser_dom_window_dump_enabled();
#else
  return true;
#endif
}

#define DOM_WEBIDL_PREF(name)                    \
  /* static */ bool                              \
  DOMPrefs::name(JSContext* aCx, JSObject* aObj) \
  {                                              \
    return StaticPrefs::name();                  \
  }

#define DOM_UINT32_PREF(name, pref, defaultValue)                             \
  /* static */ uint32_t                                                       \
  DOMPrefs::name()                                                            \
  {                                                                           \
      static bool initialized = false;                                        \
      static Atomic<uint32_t> cachedValue;                                    \
      if (!initialized) {                                                     \
        initialized = true;                                                   \
        Preferences::AddAtomicUintVarCache(&cachedValue, pref, defaultValue); \
    }                                                                         \
    return cachedValue;                                                       \
  }

#include "DOMPrefsInternal.h"

#undef DOM_WEBIDL_PREF
#undef DOM_UINT32_PREF

} // dom namespace
} // mozilla namespace
