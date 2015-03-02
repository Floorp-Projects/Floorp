/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 *
 */

// https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html#cache-storage

[Exposed=(Window,Worker),
 Func="mozilla::dom::cache::CacheStorage::PrefEnabled"]
interface CacheStorage {
[Throws]
Promise<Response> match(RequestInfo request, optional CacheQueryOptions options);
[Throws]
Promise<boolean> has(DOMString cacheName);
[Throws]
Promise<Cache> open(DOMString cacheName);
[Throws]
Promise<boolean> delete(DOMString cacheName);
[Throws]
Promise<sequence<DOMString>> keys();
};
