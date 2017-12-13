/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMPreferences.h"
#include "mozilla/Atomics.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace dom {

void
DOMPreferences::Initialize()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Let's cache all the values on the main-thread.
#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  DOMPreferences::DumpEnabled();
#endif

#define PREF(name, pref) DOMPreferences::name();
#include "DOMPreferencesInternal.h"
#undef PREF
}

#define PREF(name, pref)                               \
  /* static */ bool                                    \
  DOMPreferences::name()                               \
  {                                                    \
    static bool initialized = false;                   \
    static Atomic<bool> cachedValue;                   \
    if (!initialized) {                                \
      initialized = true;                              \
      Preferences::AddAtomicBoolVarCache(&cachedValue, \
                                         pref, false); \
    }                                                  \
    return cachedValue;                                \
  }

#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
PREF(DumpEnabled, "browser.dom.window.dump.enabled")
#else
/* static */ bool
DOMPreferences::DumpEnabled()
{
  return true;
}
#endif

#include "DOMPreferencesInternal.h"
#undef PREF

#define PREF_WEBIDL(name)                              \
  /* static */ bool                                    \
  DOMPreferences::name(JSContext* aCx, JSObject* aObj) \
  {                                                    \
    return DOMPreferences::name();                     \
  }

PREF_WEBIDL(ImageBitmapExtensionsEnabled)
PREF_WEBIDL(DOMCachesEnabled)
PREF_WEBIDL(NotificationEnabledInServiceWorkers)
PREF_WEBIDL(NotificationRIEnabled)
PREF_WEBIDL(ServiceWorkersEnabled)
PREF_WEBIDL(StorageManagerEnabled)
PREF_WEBIDL(PromiseRejectionEventsEnabled)
PREF_WEBIDL(PushEnabled)
PREF_WEBIDL(StreamsEnabled)
PREF_WEBIDL(RequestContextEnabled)
PREF_WEBIDL(OffscreenCanvasEnabled)
PREF_WEBIDL(WebkitBlinkDirectoryPickerEnabled)
PREF_WEBIDL(NetworkInformationEnabled)
PREF_WEBIDL(FetchObserverEnabled)

#undef PREF_WEBIDL

} // dom namespace
} // mozilla namespace
