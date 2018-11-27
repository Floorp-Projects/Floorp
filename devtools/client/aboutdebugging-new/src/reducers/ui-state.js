/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADB_ADDON_STATUS_UPDATED,
  DEBUG_TARGET_COLLAPSIBILITY_UPDATED,
  NETWORK_LOCATIONS_UPDATED,
  PAGE_SELECTED,
  USB_RUNTIMES_SCAN_START,
  USB_RUNTIMES_SCAN_SUCCESS,
} = require("../constants");

function UiState(locations = [], debugTargetCollapsibilities = {},
                 networkEnabled = false, wifiEnabled = false, showSystemAddons = false) {
  return {
    adbAddonStatus: null,
    debugTargetCollapsibilities,
    isScanningUsb: false,
    networkEnabled,
    networkLocations: locations,
    selectedPage: null,
    selectedRuntime: null,
    showSystemAddons,
    wifiEnabled,
  };
}

function uiReducer(state = UiState(), action) {
  switch (action.type) {
    case ADB_ADDON_STATUS_UPDATED: {
      const { adbAddonStatus } = action;
      return Object.assign({}, state, { adbAddonStatus });
    }

    case DEBUG_TARGET_COLLAPSIBILITY_UPDATED: {
      const { isCollapsed, key } = action;
      const debugTargetCollapsibilities = new Map(state.debugTargetCollapsibilities);
      debugTargetCollapsibilities.set(key, isCollapsed);
      return Object.assign({}, state, { debugTargetCollapsibilities });
    }

    case NETWORK_LOCATIONS_UPDATED: {
      const { locations } = action;
      return Object.assign({}, state, { networkLocations: locations });
    }

    case PAGE_SELECTED: {
      const { page, runtimeId } = action;
      return Object.assign({}, state,
        { selectedPage: page, selectedRuntime: runtimeId });
    }

    case USB_RUNTIMES_SCAN_START: {
      return Object.assign({}, state, { isScanningUsb: true });
    }

    case USB_RUNTIMES_SCAN_SUCCESS: {
      return Object.assign({}, state, { isScanningUsb: false });
    }

    default:
      return state;
  }
}

module.exports = {
  UiState,
  uiReducer,
};
