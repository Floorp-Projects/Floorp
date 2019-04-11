/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const asyncStorage = require("devtools/shared/async-storage");

const {
  ADD_DEVICE,
  ADD_DEVICE_TYPE,
  EDIT_DEVICE,
  LOAD_DEVICE_LIST_START,
  LOAD_DEVICE_LIST_ERROR,
  LOAD_DEVICE_LIST_END,
  REMOVE_DEVICE,
  UPDATE_DEVICE_DISPLAYED,
  UPDATE_DEVICE_MODAL,
} = require("./index");
const { post } = require("../utils/message");

const { addDevice, editDevice, getDevices, removeDevice } = require("devtools/client/shared/devices");
const { changeUserAgent, toggleTouchSimulation } = require("./ui");
const { changeDevice, changePixelRatio } = require("./viewports");

const DISPLAYED_DEVICES_PREF = "devtools.responsive.html.displayedDeviceList";

/**
 * Returns an object containing the user preference of displayed devices.
 *
 * @return {Object} containing two Sets:
 * - added: Names of the devices that were explicitly enabled by the user
 * - removed: Names of the devices that were explicitly removed by the user
 */
function loadPreferredDevices() {
  const preferredDevices = {
    "added": new Set(),
    "removed": new Set(),
  };

  if (Services.prefs.prefHasUserValue(DISPLAYED_DEVICES_PREF)) {
    try {
      let savedData = Services.prefs.getStringPref(DISPLAYED_DEVICES_PREF);
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
  Services.prefs.setStringPref(DISPLAYED_DEVICES_PREF, devicesToSave);
}

module.exports = {

  // This function is only exported for testing purposes
  _loadPreferredDevices: loadPreferredDevices,

  updatePreferredDevices: updatePreferredDevices,

  addCustomDevice(device) {
    return async function(dispatch) {
      // Add custom device to device storage
      await addDevice(device, "custom");
      dispatch({
        type: ADD_DEVICE,
        device,
        deviceType: "custom",
      });
    };
  },

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

  editCustomDevice(viewport, oldDevice, newDevice) {
    return async function(dispatch) {
      // Edit custom device in storage
      await editDevice(oldDevice, newDevice, "custom");
      // Notify the window that the device should be updated in the device selector.
      post(window, {
        type: "change-device",
        device: newDevice,
      });

      // Update UI if the device is selected.
      if (viewport) {
        dispatch(changeUserAgent(newDevice.userAgent));
        dispatch(toggleTouchSimulation(newDevice.touch));
      }

      dispatch({
        type: EDIT_DEVICE,
        deviceType: "custom",
        viewport,
        oldDevice,
        newDevice,
      });
    };
  },

  removeCustomDevice(device) {
    return async function(dispatch) {
      // Remove custom device from device storage
      await removeDevice(device, "custom");
      dispatch({
        type: REMOVE_DEVICE,
        device,
        deviceType: "custom",
      });
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
    return async function(dispatch) {
      dispatch({ type: LOAD_DEVICE_LIST_START });
      const preferredDevices = loadPreferredDevices();
      let devices;

      try {
        devices = await getDevices();
      } catch (e) {
        console.error("Could not load device list: " + e);
        dispatch({ type: LOAD_DEVICE_LIST_ERROR });
        return;
      }

      for (const type of devices.TYPES) {
        dispatch(module.exports.addDeviceType(type));
        for (const device of devices[type]) {
          if (device.os == "fxos") {
            continue;
          }

          const newDevice = Object.assign({}, device, {
            displayed: preferredDevices.added.has(device.name) ||
              (device.featured && !(preferredDevices.removed.has(device.name))),
          });

          dispatch(module.exports.addDevice(newDevice, type));
        }
      }

      // Add an empty "custom" type if it doesn't exist in device storage
      if (!devices.TYPES.find(type => type == "custom")) {
        dispatch(module.exports.addDeviceType("custom"));
      }

      dispatch({ type: LOAD_DEVICE_LIST_END });
    };
  },

  restoreDeviceState() {
    return async function(dispatch, getState) {
      const deviceState = await asyncStorage.getItem("devtools.responsive.deviceState");
      if (!deviceState) {
        return;
      }

      const { id, device: deviceName, deviceType } = deviceState;
      const devices = getState().devices;

      if (!devices.types.includes(deviceType)) {
        // Can't find matching device type.
        return;
      }

      const device = devices[deviceType].find(d => d.name === deviceName);
      if (!device) {
        // Can't find device with the same device name.
        return;
      }

      post(window, {
        type: "change-device",
        device,
      });

      dispatch(changeDevice(id, device.name, deviceType));
      dispatch(changePixelRatio(id, device.pixelRatio));
      dispatch(changeUserAgent(device.userAgent));
      dispatch(toggleTouchSimulation(device.touch));
    };
  },

  updateDeviceModal(isOpen, modalOpenedFromViewport = null) {
    return {
      type: UPDATE_DEVICE_MODAL,
      isOpen,
      modalOpenedFromViewport,
    };
  },

};
