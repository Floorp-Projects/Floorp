/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/ServiceWorker/#cachestorage-interface
 */

interface Principal;

[Exposed=(Window,Worker),
 Func="nsGlobalWindowInner::CachesEnabled"]
interface CacheStorage {
  [Throws, ChromeOnly]
  constructor(CacheStorageNamespace namespace, Principal principal);

  [NewObject]
  Promise<Response> match(RequestInfo request, optional MultiCacheQueryOptions options = {});
  [NewObject]
  Promise<boolean> has(DOMString cacheName);
  [NewObject]
  Promise<Cache> open(DOMString cacheName);
  [NewObject]
  Promise<boolean> delete(DOMString cacheName);
  [NewObject]
  Promise<sequence<DOMString>> keys();
};

dictionary MultiCacheQueryOptions : CacheQueryOptions {
  DOMString cacheName;
};

// chrome-only, gecko specific extension
enum CacheStorageNamespace {
  "content", "chrome"
};
