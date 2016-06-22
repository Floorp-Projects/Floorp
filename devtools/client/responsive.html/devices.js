/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Task } = require("devtools/shared/task");
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
  let preferredDevices = loadPreferredDevices();
  let devices = yield GetDevices();

  for (let type of devices.TYPES) {
    dispatch(addDeviceType(type));
    for (let device of devices[type]) {
      if (device.os == "fxos") {
        continue;
      }

      let newDevice = Object.assign({}, device, {
        displayed: preferredDevices.added.has(device.name) ||
          (device.featured && !(preferredDevices.removed.has(device.name))),
      });

      dispatch(addDevice(newDevice, type));
    }
  }
});

/**
 * Returns an object containing the user preference of displayed devices.
 *
 * @return {Object} containing two Sets:
 * - added: Names of the devices that were explicitly enabled by the user
 * - removed: Names of the devices that were explicitly removed by the user
 */
function loadPreferredDevices() {
  let preferredDevices = {
    "added": new Set(),
    "removed": new Set(),
  };

  if (Services.prefs.prefHasUserValue(DISPLAYED_DEVICES_PREF)) {
    try {
      let savedData = Services.prefs.getCharPref(DISPLAYED_DEVICES_PREF);
      savedData = JSON.parse(savedData);
      if (savedData.added && savedData.removed) {
        preferredDevices.added = new Set(savedData.added);
        preferredDevices.removed = new Set(savedData.removed);
      }
    } catch (e) {
      console.error(e);
    }
  }

  return preferredDevices;
}

/**
 * Update the displayed device list preference with the given device list.
 *
 * @param {Object} containing two Sets:
 * - added: Names of the devices that were explicitly enabled by the user
 * - removed: Names of the devices that were explicitly removed by the user
 */
function updatePreferredDevices(devices) {
  let devicesToSave = {
    added: Array.from(devices.added),
    removed: Array.from(devices.removed),
  };
  devicesToSave = JSON.stringify(devicesToSave);
  Services.prefs.setCharPref(DISPLAYED_DEVICES_PREF, devicesToSave);
}

exports.initDevices = initDevices;
exports.loadPreferredDevices = loadPreferredDevices;
exports.updatePreferredDevices = updatePreferredDevices;
