/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is the list of the preferences that are exposed to workers.
// The format is as follows:
//
//   WORKER_SIMPLE_PREF("foo.bar", FooBar, FOO_BAR, UpdaterFunction)
//
//   * First argument is the name of the pref.
//   * The name of the getter function.  This defines a FindName()
//     function that returns the value of the pref on WorkerPrivate.
//   * The macro version of the name.  This defines a WORKERPREF_FOO_BAR
//     macro in Workers.h.
//   * The name of the function that updates the new value of a pref.
//
//   WORKER_PREF("foo.bar", UpdaterFunction)
//
//   * First argument is the name of the pref.
//   * The name of the function that updates the new value of a pref.

#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
WORKER_SIMPLE_PREF("browser.dom.window.dump.enabled", DumpEnabled, DUMP)
#endif
WORKER_SIMPLE_PREF("canvas.imagebitmap_extensions.enabled", ImageBitmapExtensionsEnabled, IMAGEBITMAP_EXTENSIONS_ENABLED)
WORKER_SIMPLE_PREF("dom.caches.enabled", DOMCachesEnabled, DOM_CACHES)
WORKER_SIMPLE_PREF("dom.caches.testing.enabled", DOMCachesTestingEnabled, DOM_CACHES_TESTING)
WORKER_SIMPLE_PREF("dom.performance.enable_user_timing_logging", PerformanceLoggingEnabled, PERFORMANCE_LOGGING_ENABLED)
WORKER_SIMPLE_PREF("dom.webnotifications.enabled", DOMWorkerNotificationEnabled, DOM_WORKERNOTIFICATION)
WORKER_SIMPLE_PREF("dom.webnotifications.serviceworker.enabled", DOMServiceWorkerNotificationEnabled, DOM_SERVICEWORKERNOTIFICATION)
WORKER_SIMPLE_PREF("dom.serviceWorkers.enabled", ServiceWorkersEnabled, SERVICEWORKERS_ENABLED)
WORKER_SIMPLE_PREF("dom.serviceWorkers.testing.enabled", ServiceWorkersTestingEnabled, SERVICEWORKERS_TESTING_ENABLED)
WORKER_SIMPLE_PREF("dom.serviceWorkers.openWindow.enabled", OpenWindowEnabled, OPEN_WINDOW_ENABLED)
WORKER_SIMPLE_PREF("dom.push.enabled", PushEnabled, PUSH_ENABLED)
WORKER_SIMPLE_PREF("dom.requestcontext.enabled", RequestContextEnabled, REQUESTCONTEXT_ENABLED)
WORKER_SIMPLE_PREF("gfx.offscreencanvas.enabled", OffscreenCanvasEnabled, OFFSCREENCANVAS_ENABLED)
WORKER_SIMPLE_PREF("dom.webkitBlink.dirPicker.enabled", WebkitBlinkDirectoryPickerEnabled, DOM_WEBKITBLINK_DIRPICKER_WEBKITBLINK)
WORKER_PREF("dom.workers.latestJSVersion", JSVersionChanged)
WORKER_PREF("intl.accept_languages", PrefLanguagesChanged)
WORKER_PREF("general.appname.override", AppNameOverrideChanged)
WORKER_PREF("general.appversion.override", AppVersionOverrideChanged)
WORKER_PREF("general.platform.override", PlatformOverrideChanged)
#ifdef JS_GC_ZEAL
WORKER_PREF("dom.workers.options.gcZeal", LoadGCZealOptions)
#endif
