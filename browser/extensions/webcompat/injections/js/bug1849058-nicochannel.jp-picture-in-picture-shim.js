/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1849058 - Shim PictureInPictureWindow for nicochannel.jp
 * WebCompat issue #124463 - https://webcompat.com/issues/124463
 *
 * The page is showing unsupported message based on typeof
 * window.PictureInPictureWindow, which is undefined in Firefox.
 * Shimming it to `class {}` makes the pages work.
 */

/* globals exportFunction */

console.info(
  "PictureInPictureWindow was shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1849058 for details."
);

Object.defineProperty(window.wrappedJSObject, "PictureInPictureWindow", {
  value: exportFunction(function () {
    return class {};
  }, window),
});
