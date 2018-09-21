/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { check } = require("devtools/shared/adb/adb-running-checker");
const { ADBScanner } = require("devtools/shared/adb/adb-scanner");
const { GetAvailableAddons } = require("devtools/client/webide/modules/addons");

/**
 * This module provides a collection of helper methods to detect USB runtimes whom Firefox
 * is running on.
 */
function addUSBRuntimesObserver(listener) {
  ADBScanner.on("runtime-list-updated", listener);
}
exports.addUSBRuntimesObserver = addUSBRuntimesObserver;

function disableUSBRuntimes() {
  ADBScanner.disable();
}
exports.disableUSBRuntimes = disableUSBRuntimes;

async function enableUSBRuntimes() {
  const { adb } = GetAvailableAddons();
  if (adb.status === "uninstalled" || !(await check())) {
    console.error("ADB extension is not installed");
    return;
  }

  ADBScanner.enable();
}
exports.enableUSBRuntimes = enableUSBRuntimes;

function getUSBRuntimes() {
  return ADBScanner.listRuntimes();
}
exports.getUSBRuntimes = getUSBRuntimes;

function removeUSBRuntimesObserver(listener) {
  ADBScanner.off("runtime-list-updated", listener);
}
exports.removeUSBRuntimesObserver = removeUSBRuntimesObserver;
