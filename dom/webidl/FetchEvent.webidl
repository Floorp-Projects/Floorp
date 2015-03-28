/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 */

[Constructor(DOMString type, optional FetchEventInit eventInitDict),
 Func="mozilla::dom::workers::ServiceWorkerVisible",
 Exposed=(ServiceWorker)]
interface FetchEvent : Event {
  readonly attribute Request request;

  // https://github.com/slightlyoff/ServiceWorker/issues/631
  readonly attribute Client? client; // The window issuing the request.
  readonly attribute boolean isReload;

  [Throws]
  void respondWith((Response or Promise<Response>) r);
};

dictionary FetchEventInit : EventInit {
  Request request;
  Client client;
  boolean isReload;
};
