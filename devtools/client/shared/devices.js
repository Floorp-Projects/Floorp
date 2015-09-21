/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cc } = require("chrome");
const { getJSON } = require("devtools/client/shared/getjson");
const { Services } = require("resource://gre/modules/Services.jsm");
const promise = require("promise");

const DEVICES_URL = "devtools.devices.url";
const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/device.properties");

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
 *   require("devtools/client/shared/devices").AddDevice(myPhone, "phones");
 */

// Local devices catalog that addons can add to.
var localDevices = {};

// Add a device to the local catalog.
function AddDevice(device, type = "phones") {
  let list = localDevices[type];
  if (!list) {
    list = localDevices[type] = [];
  }
  list.push(device);
}
exports.AddDevice = AddDevice;

// Get the complete devices catalog.
function GetDevices(bypassCache = false) {
  let deferred = promise.defer();

  // Fetch common devices from Mozilla's CDN.
  getJSON(DEVICES_URL, bypassCache).then(devices => {
    for (let type in localDevices) {
      if (!devices[type]) {
        devices.TYPES.push(type);
        devices[type] = [];
      }
      devices[type] = localDevices[type].concat(devices[type]);
    }
    deferred.resolve(devices);
  });

  return deferred.promise;
}
exports.GetDevices = GetDevices;

// Get the localized string for a device type.
function GetDeviceString(deviceType) {
  return Strings.GetStringFromName("device." + deviceType);
}
exports.GetDeviceString = GetDeviceString;
