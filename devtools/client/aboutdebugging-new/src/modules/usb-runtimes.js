/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "adb", "devtools/shared/adb/adb", true);

/**
 * Used to represent a regular runtime returned by ADB.
 */
class UsbRuntime {
  constructor(adbRuntime) {
    this.id = adbRuntime.id;
    this.deviceId = adbRuntime.deviceId;
    this.deviceName = adbRuntime.deviceName;
    this.shortName = adbRuntime.shortName;
    this.socketPath = adbRuntime.socketPath;
    this.isUnknown = false;
    this.isUnplugged = false;
  }
}

/**
 * Used when a device was detected, meaning USB debugging is enabled on the device, but no
 * runtime/browser is available for connection.
 */
class UnknownUsbRuntime {
  constructor(adbDevice) {
    this.id = adbDevice.id + "|unknown";
    this.deviceId = adbDevice.id;
    this.deviceName = adbDevice.name;
    this.shortName = "Unknown runtime";
    this.socketPath = null;
    this.isUnknown = true;
    this.isUnplugged = false;
  }
}

/**
 * This module provides a collection of helper methods to detect USB runtimes whom Firefox
 * is running on.
 */
function addUSBRuntimesObserver(listener) {
  adb.registerListener(listener);
}
exports.addUSBRuntimesObserver = addUSBRuntimesObserver;

async function getUSBRuntimes() {
  // Get the available runtimes
  const runtimes = adb.getRuntimes().map(r => new UsbRuntime(r));

  // Get devices found by ADB, but without any available runtime.
  const runtimeDevices = runtimes.map(r => r.deviceId);
  const unknownRuntimes = adb.getDevices()
    .filter(d => !runtimeDevices.includes(d.id))
    .map(d => new UnknownUsbRuntime(d));

  return runtimes.concat(unknownRuntimes);
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
