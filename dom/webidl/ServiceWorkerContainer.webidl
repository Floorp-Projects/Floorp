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

  [Throws]
  readonly attribute Promise<any> ready;

  [Throws]
  Promise<any> getAll();

  [Throws]
  Promise<ServiceWorker> register(DOMString url, optional RegistrationOptionList options);

  [Throws]
  Promise<any> unregister(DOMString? scope);

  attribute EventHandler onupdatefound;
  attribute EventHandler oncontrollerchange;
  attribute EventHandler onreloadpage;
  attribute EventHandler onerror;
};

// Testing only.
partial interface ServiceWorkerContainer {
  [Throws,Pref="dom.serviceWorkers.testing.enabled"]
  Promise<any> clearAllServiceWorkerData();

  [Throws,Pref="dom.serviceWorkers.testing.enabled"]
  DOMString getScopeForUrl(DOMString url);

  [Throws,Pref="dom.serviceWorkers.testing.enabled"]
  DOMString getControllingWorkerScriptURLForPath(DOMString path);
};

dictionary RegistrationOptionList {
  DOMString scope = "/*";
};
