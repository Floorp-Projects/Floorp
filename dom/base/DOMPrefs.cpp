/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMPrefs.h"
#include "mozilla/Atomics.h"
#include "mozilla/Preferences.h"

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

#define DOM_PREF(name, pref) DOMPrefs::name();
#define DOM_WEBIDL_PREF(name)
#define DOM_UINT32_PREF(name, pref, defaultValue) DOMPrefs::name();

#include "DOMPrefsInternal.h"

#undef DOM_PREF
#undef DOM_WEBIDL_PREF
#undef DOM_UINT32_PREF
}

#define DOM_PREF(name, pref)                                         \
  /* static */ bool                                                  \
  DOMPrefs::name()                                                   \
  {                                                                  \
    static bool initialized = false;                                 \
    static Atomic<bool> cachedValue;                                 \
    if (!initialized) {                                              \
      initialized = true;                                            \
      Preferences::AddAtomicBoolVarCache(&cachedValue, pref, false); \
    }                                                                \
    return cachedValue;                                              \
  }

#define DOM_WEBIDL_PREF(name)                    \
  /* static */ bool                              \
  DOMPrefs::name(JSContext* aCx, JSObject* aObj) \
  {                                              \
    return DOMPrefs::name();                     \
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

#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
DOM_PREF(DumpEnabled, "browser.dom.window.dump.enabled")
#else
/* static */ bool
DOMPrefs::DumpEnabled()
{
  return true;
}
#endif

#include "DOMPrefsInternal.h"

#undef DOM_PREF
#undef DOM_WEBIDL_PREF
#undef DOM_UINT32_PREF

} // dom namespace
} // mozilla namespace
