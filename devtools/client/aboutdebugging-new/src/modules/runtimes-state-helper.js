/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getCurrentRuntime(runtimesState) {
  const selectedRuntimeId = runtimesState.selectedRuntimeId;
  return findRuntimeById(selectedRuntimeId, runtimesState);
}
exports.getCurrentRuntime = getCurrentRuntime;

function getCurrentClient(runtimesState) {
  return getCurrentRuntime(runtimesState).client;
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
