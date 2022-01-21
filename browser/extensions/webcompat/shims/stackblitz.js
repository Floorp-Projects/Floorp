/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1668408 - Disable service workers on StackBlitz
 *
 * StackBlitz does not function correctly with Total Cookie
 * Protection because it does not request storage access
 * permissions before trying to register service workers.
 * Until this is addressed, service workers are disabled
 * for StackBlitz to mitigate the breakage.
 */

console.info(
  "Service Workers are disabled by Firefox on this page to prevent breakage. See https://bugzilla.mozilla.org/show_bug.cgi?id=1668408 for details."
);

delete window.navigator.wrappedJSObject.__proto__.serviceWorker;
