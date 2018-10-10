/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "adbAddon", "devtools/shared/adb/adb-addon", true);
loader.lazyRequireGetter(this, "ADB_ADDON_STATES", "devtools/shared/adb/adb-addon", true);
loader.lazyGetter(this, "adbScanner", () => {
  const { ADBScanner } = require("devtools/shared/adb/adb-scanner");
  return new ADBScanner();
});

function _onAdbAddonUpdate() {
  // We are only listening to adbAddon updates if usb-runtimes are enabled.
  if (adbAddon.status === ADB_ADDON_STATES.INSTALLED) {
    // If the status switched to installed, the scanner should be enabled.
    adbScanner.enable();
  } else {
    // Otherwise disable the scanner. disable() can be called several times without side
    // effect.
    adbScanner.disable();
  }
}

/**
 * This module provides a collection of helper methods to detect USB runtimes whom Firefox
 * is running on.
 */
function addUSBRuntimesObserver(listener) {
  adbScanner.on("runtime-list-updated", listener);
}
exports.addUSBRuntimesObserver = addUSBRuntimesObserver;

function disableUSBRuntimes() {
  if (adbAddon.status === ADB_ADDON_STATES.INSTALLED) {
    adbScanner.disable();
  }
  adbAddon.off("update", _onAdbAddonUpdate);
}
exports.disableUSBRuntimes = disableUSBRuntimes;

async function enableUSBRuntimes() {
  if (adbAddon.status === ADB_ADDON_STATES.INSTALLED) {
    adbScanner.enable();
  }
  adbAddon.on("update", _onAdbAddonUpdate);
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
