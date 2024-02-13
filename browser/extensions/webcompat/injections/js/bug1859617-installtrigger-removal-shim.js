/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1859617 - Generic window.InstallTrigger removal shim
 *
 * This interventions shims window.InstallTrigger to undefine it.
 */

/* globals exportFunction */

if (typeof window.InstallTrigger !== "undefined") {
  console.info(
    "window.InstallTrigger has been undefined for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1859617 for details."
  );

  Object.defineProperty(window.wrappedJSObject, "InstallTrigger", {
    get: exportFunction(function () {
      return undefined;
    }, window),
    set: exportFunction(function (_) {}, window),
  });
}
