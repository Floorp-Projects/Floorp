/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 * https://w3c.github.io/push-api/
 * https://notifications.spec.whatwg.org/
 */

[Func="ServiceWorkerVisible",
 Exposed=(Window,Worker)]
interface ServiceWorkerRegistration : EventTarget {
  readonly attribute ServiceWorker? installing;
  readonly attribute ServiceWorker? waiting;
  readonly attribute ServiceWorker? active;

  [Pref="dom.serviceWorkers.navigationPreload.enabled", SameObject]
  readonly attribute NavigationPreloadManager navigationPreload;

  readonly attribute USVString scope;
  [Throws]
  readonly attribute ServiceWorkerUpdateViaCache updateViaCache;

  [Throws, NewObject]
  Promise<void> update();

  [Throws, NewObject]
  Promise<boolean> unregister();

  // event
  attribute EventHandler onupdatefound;
};

enum ServiceWorkerUpdateViaCache {
  "imports",
  "all",
  "none"
};

// https://w3c.github.io/push-api/
partial interface ServiceWorkerRegistration {
  [Throws, Exposed=(Window,Worker), Pref="dom.push.enabled"]
  readonly attribute PushManager pushManager;
};

// https://notifications.spec.whatwg.org/
partial interface ServiceWorkerRegistration {
  [NewObject, Pref="dom.webnotifications.serviceworker.enabled"]
  Promise<void> showNotification(DOMString title, optional NotificationOptions options = {});
  [NewObject, Pref="dom.webnotifications.serviceworker.enabled"]
  Promise<sequence<Notification>> getNotifications(optional GetNotificationOptions filter = {});
};
