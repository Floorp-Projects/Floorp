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

#define DOM_PREF(name, pref) DOMPreferences::name();
#include "DOMPreferencesInternal.h"
#undef DOM_PREF
}

#define DOM_PREF(name, pref)                           \
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
DOM_PREF(DumpEnabled, "browser.dom.window.dump.enabled")
#else
/* static */ bool
DOMPreferences::DumpEnabled()
{
  return true;
}
#endif

#include "DOMPreferencesInternal.h"
#undef DOM_PREF

#define DOM_PREF_WEBIDL(name)                          \
  /* static */ bool                                    \
  DOMPreferences::name(JSContext* aCx, JSObject* aObj) \
  {                                                    \
    return DOMPreferences::name();                     \
  }

DOM_PREF_WEBIDL(ImageBitmapExtensionsEnabled)
DOM_PREF_WEBIDL(DOMCachesEnabled)
DOM_PREF_WEBIDL(NotificationEnabledInServiceWorkers)
DOM_PREF_WEBIDL(NotificationRIEnabled)
DOM_PREF_WEBIDL(ServiceWorkersEnabled)
DOM_PREF_WEBIDL(OpenWindowEnabled)
DOM_PREF_WEBIDL(StorageManagerEnabled)
DOM_PREF_WEBIDL(PromiseRejectionEventsEnabled)
DOM_PREF_WEBIDL(PushEnabled)
DOM_PREF_WEBIDL(StreamsEnabled)
DOM_PREF_WEBIDL(RequestContextEnabled)
DOM_PREF_WEBIDL(OffscreenCanvasEnabled)
DOM_PREF_WEBIDL(WebkitBlinkDirectoryPickerEnabled)
DOM_PREF_WEBIDL(NetworkInformationEnabled)
DOM_PREF_WEBIDL(FetchObserverEnabled)

#undef DOM_PREF_WEBIDL

} // dom namespace
} // mozilla namespace
