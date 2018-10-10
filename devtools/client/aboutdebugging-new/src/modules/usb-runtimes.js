/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyGetter(this, "adbScanner", () => {
  const { AddonAwareADBScanner } = require("devtools/shared/adb/addon-aware-adb-scanner");
  return new AddonAwareADBScanner();
});

/**
 * This module provides a collection of helper methods to detect USB runtimes whom Firefox
 * is running on.
 */
function addUSBRuntimesObserver(listener) {
  adbScanner.on("runtime-list-updated", listener);
}
exports.addUSBRuntimesObserver = addUSBRuntimesObserver;

function disableUSBRuntimes() {
  adbScanner.disable();
}
exports.disableUSBRuntimes = disableUSBRuntimes;

async function enableUSBRuntimes() {
  adbScanner.enable();
}
exports.enableUSBRuntimes = enableUSBRuntimes;

function getUSBRuntimes() {
  return adbScanner.listRuntimes();
}
exports.getUSBRuntimes = getUSBRuntimes;

function removeUSBRuntimesObserver(listener) {
  adbScanner.off("runtime-list-updated", listener);
}
exports.removeUSBRuntimesObserver = removeUSBRuntimesObserver;
