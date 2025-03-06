/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1472075 - Build UA override for Bank of America for OSX & Linux
 *
 * Formula1 TV is doing some kind of check for Android devices which is
 * causing it to treat Firefox on Linux as an Android device, and blocking it.
 * Overriding navigator.platform to Win64 on Linux works around this issue.
 */

/* globals exportFunction */

console.info(
  "navigator.platform has been overridden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1950282 for details."
);

Object.defineProperty(window.navigator.wrappedJSObject, "platform", {
  get: exportFunction(function () {
    return "Win64";
  }, window),

  set: exportFunction(function () {}, window),
});
