/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "adb", "devtools/shared/adb/adb", true);

/**
 * This module provides a collection of helper methods to detect USB runtimes whom Firefox
 * is running on.
 */
function addUSBRuntimesObserver(listener) {
  adb.registerListener(listener);
}
exports.addUSBRuntimesObserver = addUSBRuntimesObserver;

function getUSBRuntimes() {
  return adb.getRuntimes();
}
exports.getUSBRuntimes = getUSBRuntimes;

function removeUSBRuntimesObserver(listener) {
  adb.unregisterListener(listener);
}
exports.removeUSBRuntimesObserver = removeUSBRuntimesObserver;

function refreshUSBRuntimes() {
  return adb.updateRuntimes();
}
exports.refreshUSBRuntimes = refreshUSBRuntimes;

require("./test-helper").enableMocks(module, "modules/usb-runtimes");
