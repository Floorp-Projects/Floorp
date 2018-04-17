/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is the list of the preferences that are exposed to workers and
// main-thread in DOM.
// The format is as follows:
//
//   DOM_PREF(FooBar, "foo.bar")
//
//   * First argument is the name of the getter function.  This defines a
//     DOMPrefs::FooBar()
//   * The second argument is the name of the pref.
//
//   DOM_WEBIDL_PREF(FooBar)
//
//   * This defines DOMPrefs::FooBar(JSContext* aCx, JSObject* aObj);
//     This is allows the use of DOMPrefs in WebIDL files.

DOM_PREF(ImageBitmapExtensionsEnabled, "canvas.imagebitmap_extensions.enabled")
DOM_PREF(DOMCachesEnabled, "dom.caches.enabled")
DOM_PREF(DOMCachesTestingEnabled, "dom.caches.testing.enabled")
DOM_PREF(PerformanceLoggingEnabled, "dom.performance.enable_user_timing_logging")
DOM_PREF(SchedulerLoggingEnabled, "dom.performance.enable_scheduler_timing")
DOM_PREF(NotificationEnabled, "dom.webnotifications.enabled")
DOM_PREF(NotificationEnabledInServiceWorkers, "dom.webnotifications.serviceworker.enabled")
DOM_PREF(NotificationRIEnabled, "dom.webnotifications.requireinteraction.enabled")
DOM_PREF(ServiceWorkersEnabled, "dom.serviceWorkers.enabled")
DOM_PREF(ServiceWorkersTestingEnabled, "dom.serviceWorkers.testing.enabled")
DOM_PREF(StorageManagerEnabled, "dom.storageManager.enabled")
DOM_PREF(PromiseRejectionEventsEnabled, "dom.promise_rejection_events.enabled")
DOM_PREF(PushEnabled, "dom.push.enabled")
DOM_PREF(StreamsEnabled, "dom.streams.enabled")
DOM_PREF(OffscreenCanvasEnabled, "gfx.offscreencanvas.enabled")
DOM_PREF(WebkitBlinkDirectoryPickerEnabled, "dom.webkitBlink.dirPicker.enabled")
DOM_PREF(NetworkInformationEnabled, "dom.netinfo.enabled")
DOM_PREF(FetchObserverEnabled, "dom.fetchObserver.enabled")
DOM_PREF(ResistFingerprintingEnabled, "privacy.resistFingerprinting")
DOM_PREF(EnableAutoDeclineCanvasPrompts, "privacy.resistFingerprinting.autoDeclineNoUserInputCanvasPrompts")
DOM_PREF(DevToolsEnabled, "devtools.enabled")
DOM_PREF(PerformanceObserverEnabled, "dom.enable_performance_observer")

DOM_WEBIDL_PREF(ImageBitmapExtensionsEnabled)
DOM_WEBIDL_PREF(DOMCachesEnabled)
DOM_WEBIDL_PREF(NotificationEnabledInServiceWorkers)
DOM_WEBIDL_PREF(NotificationRIEnabled)
DOM_WEBIDL_PREF(ServiceWorkersEnabled)
DOM_WEBIDL_PREF(StorageManagerEnabled)
DOM_WEBIDL_PREF(PromiseRejectionEventsEnabled)
DOM_WEBIDL_PREF(PushEnabled)
DOM_WEBIDL_PREF(StreamsEnabled)
DOM_WEBIDL_PREF(OffscreenCanvasEnabled)
DOM_WEBIDL_PREF(WebkitBlinkDirectoryPickerEnabled)
DOM_WEBIDL_PREF(NetworkInformationEnabled)
DOM_WEBIDL_PREF(FetchObserverEnabled)
DOM_WEBIDL_PREF(PerformanceObserverEnabled)

DOM_UINT32_PREF(WorkerCancelingTimeoutMillis,
                "dom.worker.canceling.timeoutMilliseconds",
                30000 /* 30 seconds */)
