/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 *
 */

[Pref="dom.serviceWorkers.enabled",
 // XXXbz I have no idea where this should be exposed.  The spec makes
 // no sense.  But since it's got a pref, let's say window.
 Exposed=Window]
interface ServiceWorker : EventTarget {
  readonly attribute DOMString scope;
  readonly attribute DOMString url;

  readonly attribute ServiceWorkerState state;
  attribute EventHandler onstatechange;

  // FIXME(catalinb): Bug 1053483 - This should be inherited from MessageUtils
  [Throws]
  void postMessage(any message, optional sequence<Transferable> transferable);
};

ServiceWorker implements AbstractWorker;

enum ServiceWorkerState {
  "installing",
  "installed",
  "activating",
  "activated",
  "redundant"
};
