/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_DEVICE,
  ADD_DEVICE_TYPE,
  UPDATE_DEVICE_DISPLAYED,
  UPDATE_DEVICE_MODAL_OPEN,
} = require("../actions/index");

const INITIAL_DEVICES = {
  types: [],
  isModalOpen: false,
};

let reducers = {

  [ADD_DEVICE](devices, { device, deviceType }) {
    return Object.assign({}, devices, {
      [deviceType]: [...devices[deviceType], device],
    });
  },

  [ADD_DEVICE_TYPE](devices, { deviceType }) {
    return Object.assign({}, devices, {
      types: [...devices.types, deviceType],
      [deviceType]: [],
    });
  },

  [UPDATE_DEVICE_DISPLAYED](devices, { device, deviceType, displayed }) {
    let newDevices = devices[deviceType].map(d => {
      if (d == device) {
        d.displayed = displayed;
      }

      return d;
    });

    return Object.assign({}, devices, {
      [deviceType]: newDevices,
    });
  },

  [UPDATE_DEVICE_MODAL_OPEN](devices, { isOpen }) {
    return Object.assign({}, devices, {
      isModalOpen: isOpen,
    });
  },

};

module.exports = function(devices = INITIAL_DEVICES, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return devices;
  }
  return reducer(devices, action);
};
