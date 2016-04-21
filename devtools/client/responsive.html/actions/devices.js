/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_DEVICE,
  ADD_DEVICE_TYPE,
  UPDATE_DEVICE_DISPLAYED,
  UPDATE_DEVICE_MODAL_OPEN,
} = require("./index");

module.exports = {

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

  updateDeviceModalOpen(isOpen) {
    return {
      type: UPDATE_DEVICE_MODAL_OPEN,
      isOpen,
    };
  },

};
