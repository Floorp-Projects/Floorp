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

#define PREF(name, pref)                                             \
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

#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
PREF(DumpEnabled, "browser.dom.window.dump.enabled")
#else
/* static */ bool
DOMPrefs::DumpEnabled()
{
  return true;
}
#endif

PREF(ImageBitmapExtensionsEnabled, "canvas.imagebitmap_extensions.enabled")
PREF(DOMCachesEnabled, "dom.caches.enabled")
PREF(DOMCachesTestingEnabled, "dom.caches.testing.enabled")
PREF(PerformanceLoggingEnabled, "dom.performance.enable_user_timing_logging")
PREF(NotificationEnabled, "dom.webnotifications.enabled")
PREF(NotificationEnabledInServiceWorkers, "dom.webnotifications.serviceworker.enabled")
PREF(NotificationRIEnabled, "dom.webnotifications.requireinteraction.enabled")
PREF(ServiceWorkersEnabled, "dom.serviceWorkers.enabled")
PREF(ServiceWorkersTestingEnabled, "dom.serviceWorkers.testing.enabled")
PREF(StorageManagerEnabled, "dom.storageManager.enabled")
PREF(PromiseRejectionEventsEnabled, "dom.promise_rejection_events.enabled")
PREF(PushEnabled, "dom.push.enabled")
PREF(StreamsEnabled, "dom.streams.enabled")

#undef PREF

#define PREF_WEBIDL(name)                        \
  /* static */ bool                              \
  DOMPrefs::name(JSContext* aCx, JSObject* aObj) \
  {                                              \
    return DOMPrefs::name();                     \
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

#undef PREF_WEBIDL

} // dom namespace
} // mozilla namespace
