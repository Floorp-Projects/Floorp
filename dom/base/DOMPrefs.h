/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMPrefs_h
#define mozilla_dom_DOMPrefs_h

namespace mozilla {
namespace dom {

class DOMPrefs final
{
public:
  // Returns true if the browser.dom.window.dump.enabled pref is set.
  static bool DumpEnabled();

  // Returns true if the canvas.imagebitmap_extensions.enabled pref is set.
  static bool ImageBitmapExtensionsEnabled();
  static bool ImageBitmapExtensionsEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.caches.enabled pref is set.
  static bool DOMCachesEnabled();
  static bool DOMCachesEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.caches.testing.enabled pref is set.
  static bool DOMCachesTestingEnabled();

  // Returns true if the dom.performance.enable_user_timing_logging pref is set.
  static bool PerformanceLoggingEnabled();

  // Returns true if the dom.webnotifications.enabled pref is set.
  // Note that you should use NotificationEnabledInServiceWorkers if you need to
  // enable Notification API for ServiceWorkers
  static bool NotificationEnabled();

  // Returns true if the dom.webnotifications.serviceworker.enabled pref is set.
  static bool NotificationEnabledInServiceWorkers();
  static bool NotificationEnabledInServiceWorkers(JSContext* aCx,
                                                  JSObject* aObj);

  // Returns true if the dom.webnotifications.requireinteraction.enabled pref is
  // set.
  static bool NotificationRIEnabled();
  static bool NotificationRIEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.serviceWorkers.enabled pref is set.
  static bool ServiceWorkersEnabled();
  static bool ServiceWorkersEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.serviceWorkers.testing.enabled pref is set.
  static bool ServiceWorkersTestingEnabled();

  // Returns true if the dom.storageManager.enabled pref is set.
  static bool StorageManagerEnabled();
  static bool StorageManagerEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.promise_rejection_events.enabled pref is set.
  static bool PromiseRejectionEventsEnabled();
  static bool PromiseRejectionEventsEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.push.enabled pref is set.
  static bool PushEnabled();
  static bool PushEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.streams.enabled pref is set.
  static bool StreamsEnabled();
  static bool StreamsEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.requestcontext.enabled pref is set.
  static bool RequestContextEnabled();
  static bool RequestContextEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the gfx.offscreencanvas.enabled pref is set.
  static bool OffscreenCanvasEnabled();
  static bool OffscreenCanvasEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.webkitBlink.dirPicker.enabled pref is set.
  static bool WebkitBlinkDirectoryPickerEnabled();
  static bool WebkitBlinkDirectoryPickerEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.netinfo.enabled pref is set.
  static bool NetworkInformationEnabled();
  static bool NetworkInformationEnabled(JSContext* aCx, JSObject* aObj);

  // Returns true if the dom.fetchObserver.enabled pref is set.
  static bool FetchObserverEnabled();
  static bool FetchObserverEnabled(JSContext* aCx, JSObject* aObj);
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_DOMPrefs_h
