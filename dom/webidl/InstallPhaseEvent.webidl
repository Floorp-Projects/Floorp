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
[Constructor(DOMString type, optional EventInit eventInitDict),
 Func="mozilla::dom::workers::ServiceWorkerEventsVisible",
 Exposed=(ServiceWorker,Window)]
interface InstallPhaseEvent : Event {
  // https://github.com/slightlyoff/ServiceWorker/issues/261
  void waitUntil(Promise<any> p);
};
