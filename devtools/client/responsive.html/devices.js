/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Task } = require("resource://gre/modules/Task.jsm");
const { GetDevices } = require("devtools/client/shared/devices");
const { addDevice, addDeviceType } = require("./actions/devices");

const DISPLAYED_DEVICES_PREF = "devtools.responsive.html.displayedDeviceList";

/**
 * Get the device catalog and load the devices onto the store.
 *
 * @param  {Function} dispatch
 *         Action dispatch function
 */
let initDevices = Task.async(function* (dispatch) {
  let deviceList = loadDeviceList();
  let devices = yield GetDevices();

  for (let type of devices.TYPES) {
    dispatch(addDeviceType(type));
    for (let device of devices[type]) {
      if (device.os == "fxos") {
        continue;
      }

      let newDevice = Object.assign({}, device, {
        displayed: deviceList.has(device.name) ?
                   true :
                   !!device.featured,
      });

      if (newDevice.displayed) {
        deviceList.add(newDevice.name);
      }

      dispatch(addDevice(newDevice, type));
    }
  }

  updateDeviceList(deviceList);
});

/**
 * Returns a set containing the user preference of displayed devices.
 *
 * @return {Set} containing the device names that are to be displayed in the
 *         device catalog.
 */
function loadDeviceList() {
  let deviceList = new Set();

  if (Services.prefs.prefHasUserValue(DISPLAYED_DEVICES_PREF)) {
    try {
      let savedList = Services.prefs.getCharPref(DISPLAYED_DEVICES_PREF);
      deviceList = new Set(JSON.parse(savedList));
    } catch (e) {
      console.error(e);
    }
  }

  return deviceList;
}

/**
 * Update the displayed device list preference with the given device list.
 *
 * @param  {Set} devices
 *         Set of device names that are displayed in the device catalog.
 */
function updateDeviceList(devices) {
  let listToSave = JSON.stringify(Array.from(devices));
  Services.prefs.setCharPref(DISPLAYED_DEVICES_PREF, listToSave);
}

exports.initDevices = initDevices;
exports.loadDeviceList = loadDeviceList;
exports.updateDeviceList = updateDeviceList;
