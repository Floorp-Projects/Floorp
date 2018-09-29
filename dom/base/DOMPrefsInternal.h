/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is the list of the preferences that are exposed to workers and
// main-thread in DOM.
// The format is as follows:
//
//   DOM_WEBIDL_PREF(foo_bar)
//
//   * This defines DOMPrefs::foo_bar(JSContext* aCx, JSObject* aObj) which
//     returns the value of StaticPrefs::foo_bar().
//     This is allows the use of DOMPrefs in WebIDL files.

DOM_WEBIDL_PREF(canvas_imagebitmap_extensions_enabled)
DOM_WEBIDL_PREF(dom_caches_enabled)
DOM_WEBIDL_PREF(dom_webnotifications_serviceworker_enabled)
DOM_WEBIDL_PREF(dom_webnotifications_requireinteraction_enabled)
DOM_WEBIDL_PREF(dom_serviceWorkers_enabled)
DOM_WEBIDL_PREF(dom_storageManager_enabled)
DOM_WEBIDL_PREF(dom_promise_rejection_events_enabled)
DOM_WEBIDL_PREF(dom_push_enabled)
DOM_WEBIDL_PREF(gfx_offscreencanvas_enabled)
DOM_WEBIDL_PREF(dom_webkitBlink_dirPicker_enabled)
DOM_WEBIDL_PREF(dom_netinfo_enabled)
DOM_WEBIDL_PREF(dom_fetchObserver_enabled)
DOM_WEBIDL_PREF(dom_enable_performance_observer)
DOM_WEBIDL_PREF(dom_performance_enable_scheduler_timing)
DOM_WEBIDL_PREF(javascript_options_streams)
