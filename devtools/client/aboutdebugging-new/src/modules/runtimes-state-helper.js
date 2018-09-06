/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getCurrentClient(state) {
  const thisFirefoxRuntime = state.runtimes.thisFirefoxRuntimes[0];
  return thisFirefoxRuntime.client;
}
exports.getCurrentClient = getCurrentClient;
