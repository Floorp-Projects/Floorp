/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_DEVICE,
  ADD_DEVICE_TYPE,
  LOAD_DEVICE_LIST_START,
  LOAD_DEVICE_LIST_ERROR,
  LOAD_DEVICE_LIST_END,
  UPDATE_DEVICE_DISPLAYED,
  UPDATE_DEVICE_MODAL_OPEN,
} = require("./index");

const { GetDevices } = require("devtools/client/shared/devices");

const Services = require("Services");
const DISPLAYED_DEVICES_PREF = "devtools.responsive.html.displayedDeviceList";

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

module.exports = {

  // This function is only exported for testing purposes
  _loadPreferredDevices: loadPreferredDevices,

  updatePreferredDevices: updatePreferredDevices,

  addDevice(device, deviceType) {
    return {
      type: ADD_DEVICE,
      device,
      deviceType,
    };
  },

  addDeviceType(deviceType) {
    return {
      type: ADD_DEVICE_TYPE,
      deviceType,
    };
  },

  updateDeviceDisplayed(device, deviceType, displayed) {
    return {
      type: UPDATE_DEVICE_DISPLAYED,
      device,
      deviceType,
      displayed,
    };
  },

  loadDevices() {
    return function* (dispatch, getState) {
      yield dispatch({ type: LOAD_DEVICE_LIST_START });
      let preferredDevices = loadPreferredDevices();
      let devices;

      try {
        devices = yield GetDevices();
      } catch (e) {
        console.error("Could not load device list: " + e);
        dispatch({ type: LOAD_DEVICE_LIST_ERROR });
        return;
      }

      for (let type of devices.TYPES) {
        dispatch(module.exports.addDeviceType(type));
        for (let device of devices[type]) {
          if (device.os == "fxos") {
            continue;
          }

          let newDevice = Object.assign({}, device, {
            displayed: preferredDevices.added.has(device.name) ||
              (device.featured && !(preferredDevices.removed.has(device.name))),
          });

          dispatch(module.exports.addDevice(newDevice, type));
        }
      }
      dispatch({ type: LOAD_DEVICE_LIST_END });
    };
  },

  updateDeviceModalOpen(isOpen) {
    return {
      type: UPDATE_DEVICE_MODAL_OPEN,
      isOpen,
    };
  },

};
