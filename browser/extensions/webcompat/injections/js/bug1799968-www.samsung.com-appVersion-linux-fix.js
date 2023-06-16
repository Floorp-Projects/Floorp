/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1799968 - Build site patch for www.samsung.com
 * WebCompat issue #108993 - https://webcompat.com/issues/108993
 *
 * Samsung's Watch pages try to detect the OS via navigator.appVersion,
 * but fail with Linux because they expect it to contain the literal
 * string "linux", and their JS breaks.
 *
 * As such this site patch sets appVersion to "5.0 (Linux)", and is
 * only meant to be applied on Linux.
 */

/* globals exportFunction */

console.info(
  "navigator.appVersion has been shimmed for compatibility reasons. See https://webcompat.com/issues/108993 for details."
);

Object.defineProperty(navigator.wrappedJSObject, "appVersion", {
  get: exportFunction(function () {
    return "5.0 (Linux)";
  }, window),

  set: exportFunction(function () {}, window),
});
