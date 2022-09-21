/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 */

[Func="ServiceWorkerVisible",
 Exposed=(ServiceWorker)]
interface FetchEvent : ExtendableEvent {
  constructor(DOMString type, FetchEventInit eventInitDict);

  [SameObject, BinaryName="request_"] readonly attribute Request request;
  [Pref="dom.serviceWorkers.navigationPreload.enabled"]
  readonly attribute Promise<any> preloadResponse;
  readonly attribute DOMString clientId;
  readonly attribute DOMString resultingClientId;
  readonly attribute Promise<undefined> handled;

  [Throws]
  undefined respondWith(Promise<Response> r);
};

dictionary FetchEventInit : EventInit {
  required Request request;
  DOMString clientId = "";
  DOMString resultingClientId = "";
};
