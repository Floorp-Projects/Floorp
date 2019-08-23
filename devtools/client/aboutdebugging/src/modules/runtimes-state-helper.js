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
  const runtimeDetails = getCurrentRuntimeDetails(runtimesState);
  return runtimeDetails ? runtimeDetails.clientWrapper : null;
}
exports.getCurrentClient = getCurrentClient;

function findRuntimeById(id, runtimesState) {
  return getAllRuntimes(runtimesState).find(r => r.id === id);
}
exports.findRuntimeById = findRuntimeById;

function getAllRuntimes(runtimesState) {
  return [
    ...runtimesState.networkRuntimes,
    ...runtimesState.thisFirefoxRuntimes,
    ...runtimesState.usbRuntimes,
  ];
}
exports.getAllRuntimes = getAllRuntimes;

function getCurrentRuntimeDetails(runtimesState) {
  const runtime = getCurrentRuntime(runtimesState);
  return runtime ? runtime.runtimeDetails : null;
}
exports.getCurrentRuntimeDetails = getCurrentRuntimeDetails;
