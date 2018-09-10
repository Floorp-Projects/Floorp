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

#ifdef JS_BUILD_BINAST
DOM_PREF(BinASTEncodingEnabled, "dom.script_loader.binast_encoding.enabled")
#endif

DOM_WEBIDL_PREF2(canvas_imagebitmap_extensions_enabled)
DOM_WEBIDL_PREF2(dom_caches_enabled)
DOM_WEBIDL_PREF2(dom_webnotifications_serviceworker_enabled)
DOM_WEBIDL_PREF2(dom_webnotifications_requireinteraction_enabled)
DOM_WEBIDL_PREF2(dom_serviceWorkers_enabled)
DOM_WEBIDL_PREF2(dom_storageManager_enabled)
DOM_WEBIDL_PREF2(dom_promise_rejection_events_enabled)
DOM_WEBIDL_PREF2(dom_push_enabled)
DOM_WEBIDL_PREF2(dom_streams_enabled)
DOM_WEBIDL_PREF2(gfx_offscreencanvas_enabled)
DOM_WEBIDL_PREF2(dom_webkitBlink_dirPicker_enabled)
DOM_WEBIDL_PREF2(dom_netinfo_enabled)
DOM_WEBIDL_PREF2(dom_fetchObserver_enabled)
DOM_WEBIDL_PREF2(dom_enable_performance_observer)
DOM_WEBIDL_PREF(SchedulerTimingEnabled)

DOM_UINT32_PREF(WorkerCancelingTimeoutMillis,
                "dom.worker.canceling.timeoutMilliseconds",
                30000 /* 30 seconds */)
