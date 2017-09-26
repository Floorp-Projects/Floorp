/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Task } = require("devtools/shared/task");
const { getJSON } = require("devtools/client/shared/getjson");
const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/device.properties");

loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");

const DEVICES_URL = "devtools.devices.url";
const LOCAL_DEVICES = "devtools.devices.local";

/* This is a catalog of common web-enabled devices and their properties,
 * intended for (mobile) device emulation.
 *
 * The properties of a device are:
 * - name: brand and model(s).
 * - width: viewport width.
 * - height: viewport height.
 * - pixelRatio: ratio from viewport to physical screen pixels.
 * - userAgent: UA string of the device's browser.
 * - touch: whether it has a touch screen.
 * - os: default OS, such as "ios", "fxos", "android".
 *
 * The device types are:
 *   ["phones", "tablets", "laptops", "televisions", "consoles", "watches"].
 *
 * To propose new devices for the shared catalog, check out the repo at
 * https://github.com/mozilla/simulated-devices and file a pull request.
 *
 * You can easily add more devices to this catalog from your own code (e.g. an
 * addon) like so:
 *
 *   var myPhone = { name: "My Phone", ... };
 *   require("devtools/client/shared/devices").addDevice(myPhone, "phones");
 */

// Local devices catalog that addons can add to.
let localDevices;
let localDevicesLoaded = false;

// Load local devices from storage.
let loadLocalDevices = Task.async(function* () {
  if (localDevicesLoaded) {
    return;
  }
  let devicesJSON = yield asyncStorage.getItem(LOCAL_DEVICES);
  if (!devicesJSON) {
    devicesJSON = "{}";
  }
  localDevices = JSON.parse(devicesJSON);
  localDevicesLoaded = true;
});

// Add a device to the local catalog.
let addDevice = Task.async(function* (device, type = "phones") {
  yield loadLocalDevices();
  let list = localDevices[type];
  if (!list) {
    list = localDevices[type] = [];
  }
  list.push(Object.assign({}, device));
  yield asyncStorage.setItem(LOCAL_DEVICES, JSON.stringify(localDevices));
});
exports.addDevice = addDevice;

// Remove a device from the local catalog.
// returns `true` if the device is removed, `false` otherwise.
let removeDevice = Task.async(function* (device, type = "phones") {
  yield loadLocalDevices();
  let list = localDevices[type];
  if (!list) {
    return false;
  }

  let index = list.findIndex(item => device);

  if (index === -1) {
    return false;
  }

  list.splice(index, 1);
  yield asyncStorage.setItem(LOCAL_DEVICES, JSON.stringify(localDevices));

  return true;
});
exports.removeDevice = removeDevice;

// Get the complete devices catalog.
let getDevices = Task.async(function* () {
  // Fetch common devices from Mozilla's CDN.
  let devices = yield getJSON(DEVICES_URL);
  yield loadLocalDevices();
  for (let type in localDevices) {
    if (!devices[type]) {
      devices.TYPES.push(type);
      devices[type] = [];
    }
    devices[type] = localDevices[type].concat(devices[type]);
  }
  return devices;
});
exports.getDevices = getDevices;

// Get the localized string for a device type.
function getDeviceString(deviceType) {
  return L10N.getStr("device." + deviceType);
}
exports.getDeviceString = getDeviceString;
