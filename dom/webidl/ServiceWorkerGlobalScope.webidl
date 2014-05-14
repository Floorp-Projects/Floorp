/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 *
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

// The Pref controls exposure in general, the Func restricts it to inside the
// ServiceWorkerGlobalScope (itself).
[Global, Func="mozilla::dom::workers::ServiceWorkerGlobalScope::Visible",
 Pref="dom.serviceWorkers.enabled"]
interface ServiceWorkerGlobalScope : WorkerGlobalScope {
  // FIXME(nsm): Bug 982725
  // readonly attribute CacheList caches;

  // FIXME(nsm): Bug 982726
  // A container for a list of window objects, identifiable by ID, that
  // correspond to windows (or workers) that are "controlled" by this SW
  // readonly attribute ServiceWorkerClients clients;

  [Unforgeable] readonly attribute DOMString scope;

  // FIXME(nsm): Bug 995484
  // ResponsePromise<any> fetch((Request or [EnsureUTF16] DOMString) request);

  void update();
  void unregister();

  attribute EventHandler oninstall;
  attribute EventHandler onactivate;
  attribute EventHandler onfetch;
  attribute EventHandler onbeforeevicted;
  attribute EventHandler onevicted;

  // The event.source of these MessageEvents are instances of Client
  attribute EventHandler onmessage;

  // close() method inherited from WorkerGlobalScope is not exposed.
  // FIXME(nsm): For now, overridden so it can be a no-op.
  void close();
};


