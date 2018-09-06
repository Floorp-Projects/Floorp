/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getCurrentClient(runtimesState) {
  const thisFirefoxRuntime = runtimesState.thisFirefoxRuntimes[0];
  return thisFirefoxRuntime.client;
}
exports.getCurrentClient = getCurrentClient;

function findRuntimeById(id, runtimesState) {
  const allRuntimes = [
    ...runtimesState.networkRuntimes,
    ...runtimesState.thisFirefoxRuntimes,
  ];
  return allRuntimes.find(r => r.id === id);
}
exports.findRuntimeById = findRuntimeById;
