/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 */

// While not explicitly restricted to ServiceWorkerGlobalScope, it probably
// should be. https://github.com/slightlyoff/ServiceWorker/issues/254
[Constructor(DOMString type, optional InstallEventInit eventInitDict),
 Func="mozilla::dom::workers::ServiceWorkerEventsVisible"]
interface InstallEvent : InstallPhaseEvent {
  // The currently active worker for this scope when this worker is asked to
  // install itself.
  // This may be null when a ServiceWorker is being installed for a previously
  // uncontrolled scope.
  // https://github.com/slightlyoff/ServiceWorker/issues/260
  readonly attribute ServiceWorker? activeWorker;
  void replace();
};

// Should be in the spec soon to satisfy conventions about events.
// https://github.com/slightlyoff/ServiceWorker/issues/216.
dictionary InstallEventInit : EventInit {
  ServiceWorker? activeWorker = null;
};
