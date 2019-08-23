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
    this.isFenix = adbRuntime.isFenix;
    this.isUnavailable = false;
    this.isUnplugged = false;
    this.versionName = adbRuntime.versionName;
  }
}

/**
 * Used when a device was detected, meaning USB debugging is enabled on the device, but no
 * runtime/browser is available for connection.
 */
class UnavailableUsbRuntime {
  constructor(adbDevice) {
    this.id = adbDevice.id + "|unavailable";
    this.deviceId = adbDevice.id;
    this.deviceName = adbDevice.name;
    this.shortName = "Unavailable runtime";
    this.socketPath = null;
    this.isFenix = false;
    this.isUnavailable = true;
    this.isUnplugged = false;
    this.versionName = null;
  }
}

/**
 * Used to represent USB devices that were previously connected but are now missing
 * (presumably after being unplugged/disconnected from the computer).
 */
class UnpluggedUsbRuntime {
  constructor(deviceId, deviceName) {
    this.id = deviceId + "|unplugged";
    this.deviceId = deviceId;
    this.deviceName = deviceName;
    this.shortName = "Unplugged runtime";
    this.socketPath = null;
    this.isFenix = false;
    this.isUnavailable = true;
    this.isUnplugged = true;
    this.versionName = null;
  }
}

/**
 * Map used to keep track of discovered usb devices. Will be used to create the unplugged
 * usb runtimes.
 */
const devices = new Map();

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
  const unavailableRuntimes = adb
    .getDevices()
    .filter(d => !runtimeDevices.includes(d.id))
    .map(d => new UnavailableUsbRuntime(d));

  // Add all devices to the map detected devices.
  const allRuntimes = runtimes.concat(unavailableRuntimes);
  for (const runtime of allRuntimes) {
    devices.set(runtime.deviceId, runtime.deviceName);
  }

  // Get devices previously found by ADB but no longer available.
  const currentDevices = allRuntimes.map(r => r.deviceId);
  const detectedDevices = [...devices.keys()];
  const unpluggedDevices = detectedDevices.filter(
    id => !currentDevices.includes(id)
  );
  const unpluggedRuntimes = unpluggedDevices.map(deviceId => {
    const deviceName = devices.get(deviceId);
    return new UnpluggedUsbRuntime(deviceId, deviceName);
  });

  return allRuntimes.concat(unpluggedRuntimes);
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
