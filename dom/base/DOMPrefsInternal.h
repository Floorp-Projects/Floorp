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
DOM_PREF(IndexedDBStorageOptionsEnabled, "dom.indexedDB.storageOption.enabled")
#ifdef JS_BUILD_BINAST
DOM_PREF(BinASTEncodingEnabled, "dom.script_loader.binast_encoding.enabled")
#endif

DOM_WEBIDL_PREF2(canvas_imagebitmap_extensions_enabled)
DOM_WEBIDL_PREF2(dom_caches_enabled)
DOM_WEBIDL_PREF2(dom_webnotifications_serviceworker_enabled)
DOM_WEBIDL_PREF2(dom_webnotifications_requireinteraction_enabled)
DOM_WEBIDL_PREF2(dom_serviceWorkers_enabled)
DOM_WEBIDL_PREF2(dom_storageManager_enabled)
DOM_WEBIDL_PREF(PromiseRejectionEventsEnabled)
DOM_WEBIDL_PREF(PushEnabled)
DOM_WEBIDL_PREF(StreamsEnabled)
DOM_WEBIDL_PREF(OffscreenCanvasEnabled)
DOM_WEBIDL_PREF(WebkitBlinkDirectoryPickerEnabled)
DOM_WEBIDL_PREF(NetworkInformationEnabled)
DOM_WEBIDL_PREF(FetchObserverEnabled)
DOM_WEBIDL_PREF(PerformanceObserverEnabled)
DOM_WEBIDL_PREF(SchedulerTimingEnabled)

DOM_UINT32_PREF(WorkerCancelingTimeoutMillis,
                "dom.worker.canceling.timeoutMilliseconds",
                30000 /* 30 seconds */)
