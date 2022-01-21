/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713694 - Shim AddThis Angular module
 *
 * Sites using Angular with AddThis can break entirely if the module is
 * blocked. This shim mitigates that breakage by loading an empty module.
 */

if (!window.addthisModule) {
  window.addthisModule = window?.angular?.module("addthis", ["ng"]);
}
