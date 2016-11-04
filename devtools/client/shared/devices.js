/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getJSON } = require("devtools/client/shared/getjson");

const DEVICES_URL = "devtools.devices.url";
const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/device.properties");

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
 * - firefoxOS: whether Firefox OS is supported.
 *
 * The device types are:
 *   ["phones", "tablets", "laptops", "televisions", "consoles", "watches"].
 *
 * You can easily add more devices to this catalog from your own code (e.g. an
 * addon) like so:
 *
 *   var myPhone = { name: "My Phone", ... };
 *   require("devtools/client/shared/devices").addDevice(myPhone, "phones");
 */

// Local devices catalog that addons can add to.
let localDevices = {};

// Add a device to the local catalog.
function addDevice(device, type = "phones") {
  let list = localDevices[type];
  if (!list) {
    list = localDevices[type] = [];
  }
  list.push(device);
}
exports.addDevice = addDevice;

// Remove a device from the local catalog.
// returns `true` if the device is removed, `false` otherwise.
function removeDevice(device, type = "phones") {
  let list = localDevices[type];
  if (!list) {
    return false;
  }

  let index = list.findIndex(item => device);

  if (index === -1) {
    return false;
  }

  list.splice(index, 1);

  return true;
}
exports.removeDevice = removeDevice;

// Get the complete devices catalog.
function getDevices() {
  // Fetch common devices from Mozilla's CDN.
  return getJSON(DEVICES_URL).then(devices => {
    for (let type in localDevices) {
      if (!devices[type]) {
        devices.TYPES.push(type);
        devices[type] = [];
      }
      devices[type] = localDevices[type].concat(devices[type]);
    }
    return devices;
  });
}
exports.getDevices = getDevices;

// Get the localized string for a device type.
function getDeviceString(deviceType) {
  return L10N.getStr("device." + deviceType);
}
exports.getDeviceString = getDeviceString;
