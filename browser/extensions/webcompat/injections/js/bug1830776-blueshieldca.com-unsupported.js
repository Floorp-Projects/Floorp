/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1830776 - blueshieldca.com
 * WebCompat issue #112630 - https://webcompat.com/issues/112630
 *
 * The site is showing unsupported message in Firefox.
 * They're also checking for "browserCollapsed" item in sessionStorage
 * before showing the message, to only show it once. Adding this
 * item to sessionStorage will make sure the message is not shown
 * on the initial load.
 */

console.info(
  "browserCollapsed in sessionStorage has been shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1830776 for details."
);

if (!sessionStorage.getItem("browserCollapsed")) {
  sessionStorage.setItem("browserCollapsed", "true");
}
