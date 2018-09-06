/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getCurrentClient(runtimesState) {
  const selectedRuntimeId = runtimesState.selectedRuntimeId;
  const selectedRuntime = findRuntimeById(selectedRuntimeId, runtimesState);
  return selectedRuntime.client;
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
