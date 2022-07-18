/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1767407 - Shim Firebase
 *
 * Sites relying on firebase-messaging.js will break in Private
 * browsing mode because it assumes that they require service
 * workers and indexedDB, when they generally do not.
 */

/* globals cloneInto */

(function() {
  const win = window.wrappedJSObject;
  const emptyObj = new win.Object();
  const emptyArr = new win.Array();
  const emptyMsg = cloneInto({ message: "" }, window);
  const noOpFn = cloneInto(function() {}, window, { cloneFunctions: true });

  if (!win.indexedDB) {
    const idb = {
      open: () => win.Promise.reject(emptyMsg),
    };

    Object.defineProperty(win, "indexedDB", {
      value: cloneInto(idb, window, { cloneFunctions: true }),
    });
  }

  // bug 1778993
  for (const name of [
    "IDBCursor",
    "IDBDatabase",
    "IDBIndex",
    "IDBOpenDBRequest",
    "IDBRequest",
    "IDBTransaction",
  ]) {
    if (!win[name]) {
      Object.defineProperty(win, name, { value: emptyObj });
    }
  }

  if (!win.serviceWorker) {
    const sw = {
      addEventListener() {},
      getRegistrations: () => win.Promise.resolve(emptyArr),
      register: () => win.Promise.reject(emptyMsg),
    };

    Object.defineProperty(navigator.wrappedJSObject, "serviceWorker", {
      value: cloneInto(sw, window, { cloneFunctions: true }),
    });

    // bug 1779536
    Object.defineProperty(navigator.wrappedJSObject.serviceWorker, "ready", {
      value: new win.Promise(noOpFn),
    });
  }

  // bug 1750699
  if (!win.PushManager) {
    Object.defineProperty(win, "PushManager", { value: emptyObj });
  }

  // bug 1750699
  if (!win.PushSubscription) {
    const ps = {
      prototype: {
        getKey() {},
      },
    };

    Object.defineProperty(win, "PushSubscription", {
      value: cloneInto(ps, window, { cloneFunctions: true }),
    });
  }

  // bug 1750699
  if (!win.ServiceWorkerRegistration) {
    const swr = {
      prototype: {
        showNotification() {},
      },
    };

    Object.defineProperty(win, "ServiceWorkerRegistration", {
      value: cloneInto(swr, window, { cloneFunctions: true }),
    });
  }
})();
