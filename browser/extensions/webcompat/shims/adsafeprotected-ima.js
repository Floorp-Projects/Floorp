/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 *
 * Sites relying on Ad Safe Protected's adapter for Google IMA may
 * have broken videos when the script is blocked. This shim stubs
 * out the API to help mitigate major breakage.
 */

if (!window.googleImaVansAdapter) {
  window.googleImaVansAdapter = {
    init() {},
    dispose() {},
  };
}
