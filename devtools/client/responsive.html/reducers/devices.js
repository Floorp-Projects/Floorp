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
} = require("../actions/index");

const Types = require("../types");

const INITIAL_DEVICES = {
  types: [],
  isModalOpen: false,
  listState: Types.deviceListState.INITIALIZED,
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

  [LOAD_DEVICE_LIST_START](devices, action) {
    return Object.assign({}, devices, {
      listState: Types.deviceListState.LOADING,
    });
  },

  [LOAD_DEVICE_LIST_ERROR](devices, action) {
    return Object.assign({}, devices, {
      listState: Types.deviceListState.ERROR,
    });
  },

  [LOAD_DEVICE_LIST_END](devices, action) {
    return Object.assign({}, devices, {
      listState: Types.deviceListState.LOADED,
    });
  },

  [UPDATE_DEVICE_MODAL_OPEN](devices, { isOpen }) {
    return Object.assign({}, devices, {
      isModalOpen: isOpen,
    });
  },

};

module.exports = function (devices = INITIAL_DEVICES, action) {
  let reducer = reducers[action.type];
  if (!reducer) {
    return devices;
  }
  return reducer(devices, action);
};
