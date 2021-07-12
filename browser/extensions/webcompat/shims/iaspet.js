/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713701 - Shim Integral Ad Science iaspet.js
 *
 * Some sites use iaspet to place content, often together with Google Publisher
 * Tags. This shim prevents breakage when the script is blocked.
 */

if (!window.__iasPET?.VERSION) {
  window.__iasPET = {
    VERSION: "1.16.18",
    queue: [],
    sessionId: "",
    setTargetingForAppNexus() {},
    setTargetingForGPT() {},
    start() {},
  };
}
