/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 *
 */

// Still unclear what should be subclassed.
// https://github.com/slightlyoff/ServiceWorker/issues/189
[Pref="dom.serviceWorkers.enabled"]
interface ServiceWorker : EventTarget {
  readonly attribute DOMString scope;
  readonly attribute DOMString url;

  readonly attribute ServiceWorkerState state;
  attribute EventHandler onstatechange;
};

ServiceWorker implements AbstractWorker;

enum ServiceWorkerState {
  "installing",
  "installed",
  "activating",
  "activated",
  "redundant"
};
