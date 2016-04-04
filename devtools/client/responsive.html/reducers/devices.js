/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_DEVICE,
  ADD_DEVICE_TYPE,
} = require("../actions/index");

const INITIAL_DEVICES = {
  types: [],
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

};

module.exports = function(devices = INITIAL_DEVICES, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return devices;
  }
  return reducer(devices, action);
};
