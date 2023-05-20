/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1801277 - Shim Microsoft virtual assistant.
 *
 * The microsoft virtual assistant will break when accessing the indexedDB that
 * will throw a security error because the virtual assistant is under a
 * third-party tracking domain 'liveperson.net'. The shim replaces the indexedDB
 * with a fake interface that won't throw an error.
 */

/* globals cloneInto */

(function () {
  const win = window.wrappedJSObject;

  try {
    // We only replace the indexedDB when liveperson.net is loaded in a
    // third-party context. Note that this is not strictly correct because
    // this is a cross-origin check but not a third-party check.
    if (win.parent == win || win.location.origin == win.top.location.origin) {
      return;
    }
  } catch (e) {
    // If we get a security error when accessing the top-level origin, this
    // shows that the window is in a cross-origin context. In this case, we can
    // proceed to apply the shim.
    if (e.name != "SecurityError") {
      throw e;
    }
  }

  const emptyMsg = cloneInto({ message: "" }, window);

  const idb = {
    open: () => win.Promise.reject(emptyMsg),
  };

  Object.defineProperty(win, "indexedDB", {
    value: cloneInto(idb, window, { cloneFunctions: true }),
  });
})();
