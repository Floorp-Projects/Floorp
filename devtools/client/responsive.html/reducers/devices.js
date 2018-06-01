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
  REMOVE_DEVICE,
  UPDATE_DEVICE_DISPLAYED,
  UPDATE_DEVICE_MODAL,
} = require("../actions/index");

const Types = require("../types");

const INITIAL_DEVICES = {
  types: [],
  isModalOpen: false,
  modalOpenedFromViewport: null,
  listState: Types.loadableState.INITIALIZED,
};

const reducers = {

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
    const newDevices = devices[deviceType].map(d => {
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
      listState: Types.loadableState.LOADING,
    });
  },

  [LOAD_DEVICE_LIST_ERROR](devices, action) {
    return Object.assign({}, devices, {
      listState: Types.loadableState.ERROR,
    });
  },

  [LOAD_DEVICE_LIST_END](devices, action) {
    return Object.assign({}, devices, {
      listState: Types.loadableState.LOADED,
    });
  },

  [REMOVE_DEVICE](devices, { device, deviceType }) {
    const index = devices[deviceType].indexOf(device);
    if (index < 0) {
      return devices;
    }

    const list = [...devices[deviceType]];
    list.splice(index, 1);
    return Object.assign({}, devices, {
      [deviceType]: list
    });
  },

  [UPDATE_DEVICE_MODAL](devices, { isOpen, modalOpenedFromViewport }) {
    return Object.assign({}, devices, {
      isModalOpen: isOpen,
      modalOpenedFromViewport,
    });
  },

};

module.exports = function(devices = INITIAL_DEVICES, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return devices;
  }
  return reducer(devices, action);
};
