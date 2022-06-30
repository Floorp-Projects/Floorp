/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is:
 * https://html.spec.whatwg.org/multipage/webappapis.html#windoworworkerglobalscope-mixin
 * https://fetch.spec.whatwg.org/#fetch-method
 * https://w3c.github.io/webappsec-secure-contexts/#monkey-patching-global-object
 * https://w3c.github.io/ServiceWorker/#self-caches
 */

// https://html.spec.whatwg.org/multipage/webappapis.html#windoworworkerglobalscope-mixin
[Exposed=(Window,Worker)]
interface mixin WindowOrWorkerGlobalScope {
  [Replaceable] readonly attribute USVString origin;
  readonly attribute boolean crossOriginIsolated;

  [Throws, NeedsCallerType]
  void reportError(any e);

  // base64 utility methods
  [Throws]
  DOMString btoa(DOMString btoa);
  [Throws]
  DOMString atob(DOMString atob);

  // timers
  // NOTE: We're using overloads where the spec uses a union.  Should
  // be black-box the same.
  [Throws]
  long setTimeout(Function handler, optional long timeout = 0, any... arguments);
  [Throws]
  long setTimeout(DOMString handler, optional long timeout = 0, any... unused);
  void clearTimeout(optional long handle = 0);
  [Throws]
  long setInterval(Function handler, optional long timeout = 0, any... arguments);
  [Throws]
  long setInterval(DOMString handler, optional long timeout = 0, any... unused);
  void clearInterval(optional long handle = 0);

  // microtask queuing
  void queueMicrotask(VoidFunction callback);

  // ImageBitmap
  [Throws]
  Promise<ImageBitmap> createImageBitmap(ImageBitmapSource aImage,
                                         optional ImageBitmapOptions aOptions = {});
  [Throws]
  Promise<ImageBitmap> createImageBitmap(ImageBitmapSource aImage,
                                         long aSx, long aSy, long aSw, long aSh,
                                         optional ImageBitmapOptions aOptions = {});

  // structured cloning
  [Throws]
  any structuredClone(any value, optional StructuredSerializeOptions options = {});
};

// https://fetch.spec.whatwg.org/#fetch-method
partial interface mixin WindowOrWorkerGlobalScope {
  [NewObject, NeedsCallerType]
  Promise<Response> fetch(RequestInfo input, optional RequestInit init = {});
};

// https://w3c.github.io/webappsec-secure-contexts/#monkey-patching-global-object
partial interface mixin WindowOrWorkerGlobalScope {
  readonly attribute boolean isSecureContext;
};

// http://w3c.github.io/IndexedDB/#factory-interface
partial interface mixin WindowOrWorkerGlobalScope {
   // readonly attribute IDBFactory indexedDB;
   [Throws]
   readonly attribute IDBFactory? indexedDB;
};

// https://w3c.github.io/ServiceWorker/#self-caches
partial interface mixin WindowOrWorkerGlobalScope {
  [Throws, Func="nsGlobalWindowInner::CachesEnabled", SameObject]
  readonly attribute CacheStorage caches;
};

// https://wicg.github.io/scheduling-apis/#ref-for-windoworworkerglobalscope-scheduler
partial interface mixin WindowOrWorkerGlobalScope {
  [Replaceable, Pref="dom.enable_web_task_scheduling", SameObject]
  readonly attribute Scheduler scheduler;
};
