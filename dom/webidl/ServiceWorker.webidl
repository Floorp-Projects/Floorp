/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html#service-worker-obj
 *
 */

// Still unclear what should be subclassed.
// https://github.com/slightlyoff/ServiceWorker/issues/189
[Pref="dom.serviceWorkers.enabled",
 // FIXME(nsm): Bug 1113522. Should also be exposed on Workers too.
 Exposed=Window]
interface ServiceWorker : EventTarget {
  readonly attribute USVString scriptURL;
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
