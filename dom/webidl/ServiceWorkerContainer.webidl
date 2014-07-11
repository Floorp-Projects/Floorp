/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 *
 */

[Pref="dom.serviceWorkers.enabled"]
interface ServiceWorkerContainer {
  // FIXME(nsm):
  // https://github.com/slightlyoff/ServiceWorker/issues/198
  // and discussion at https://etherpad.mozilla.org/serviceworker07apr
  [Unforgeable] readonly attribute ServiceWorker? installing;
  [Unforgeable] readonly attribute ServiceWorker? waiting;
  [Unforgeable] readonly attribute ServiceWorker? active;
  [Unforgeable] readonly attribute ServiceWorker? controller;

  // Promise<ServiceWorker>
  readonly attribute Promise ready;

  // Promise<sequence<ServiceWorker>?>
  [Throws]
  Promise getAll();

  // Promise<ServiceWorker>
  [Throws]
  Promise register(DOMString url, optional RegistrationOptionList options);

  // Promise<any>
  [Throws]
  Promise unregister(DOMString? scope);

  attribute EventHandler onupdatefound;
  attribute EventHandler oncontrollerchange;
  attribute EventHandler onreloadpage;
  attribute EventHandler onerror;
};

// Testing only.
[ChromeOnly, Pref="dom.serviceWorkers.testing.enabled"]
partial interface ServiceWorkerContainer {
  [Throws]
  Promise clearAllServiceWorkerData();

  [Throws]
  DOMString getScopeForUrl(DOMString url);

  [Throws]
  DOMString getControllingWorkerScriptURLForPath(DOMString path);
};

dictionary RegistrationOptionList {
  DOMString scope = "/*";
};
