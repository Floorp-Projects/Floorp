/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1815733 - Annoying "Paste" overlay when trying to paste
 *
 * As per https://bugzilla.mozilla.org/show_bug.cgi?id=1815733#c13, Outlook
 * is calling clipboard.read() again when they shouldn't. This is causing a
 * visible "Paste" prompt for the user, which is stealing focus and can be
 * annoying.
 */

/* globals exportFunction */

Object.defineProperty(navigator.clipboard.wrappedJSObject, "read", {
  value: exportFunction(function () {
    return new Promise((resolve, _) => {
      console.log(
        "clipboard.read() has been overwriten with a no-op. See https://bugzilla.mozilla.org/show_bug.cgi?id=1815733#c13 for details."
      );

      resolve();
    });
  }, navigator.clipboard),
});
