/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A stub function for preparing the network if needed
 *
 */
function startNetworkAndTest() {
  return Promise.resolve();
}

/**
 * A stub function to shutdown the network if needed
 */
function networkTestFinished() {
  return Promise.resolve().then(() => finish());
}
