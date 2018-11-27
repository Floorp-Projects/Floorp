/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADB_ADDON_INSTALL_START,
  ADB_ADDON_INSTALL_SUCCESS,
  ADB_ADDON_INSTALL_FAILURE,
  ADB_ADDON_UNINSTALL_START,
  ADB_ADDON_UNINSTALL_SUCCESS,
  ADB_ADDON_UNINSTALL_FAILURE,
  ADB_ADDON_STATUS_UPDATED,
  DEBUG_TARGET_COLLAPSIBILITY_UPDATED,
  NETWORK_LOCATIONS_UPDATED,
  PAGE_SELECTED,
  PAGE_TYPES,
  USB_RUNTIMES_SCAN_START,
  USB_RUNTIMES_SCAN_SUCCESS,
} = require("../constants");

const NetworkLocationsModule = require("../modules/network-locations");
const { adbAddon } = require("devtools/shared/adb/adb-addon");
const { refreshUSBRuntimes } = require("../modules/usb-runtimes");

const Actions = require("./index");

function selectPage(page, runtimeId) {
  return async (dispatch, getState) => {
    const isSamePage = (oldPage, newPage) => {
      if (newPage === PAGE_TYPES.RUNTIME && oldPage === PAGE_TYPES.RUNTIME) {
        return runtimeId === getState().runtimes.selectedRuntimeId;
      }
      return newPage === oldPage;
    };

    const currentPage = getState().ui.selectedPage;
    // Nothing to dispatch if the page is the same as the current page, or
    // if we are not providing any page.
    // Note: maybe we should have a PAGE_SELECTED_FAILURE action for proper logging
    if (!page || isSamePage(currentPage, page)) {
      return;
    }

    // Stop watching current runtime, if currently on a RUNTIME page.
    if (currentPage === PAGE_TYPES.RUNTIME) {
      const currentRuntimeId = getState().runtimes.selectedRuntimeId;
      await dispatch(Actions.unwatchRuntime(currentRuntimeId));
    }

    // Start watching current runtime, if moving to a RUNTIME page.
    if (page === PAGE_TYPES.RUNTIME) {
      await dispatch(Actions.watchRuntime(runtimeId));
    }

    dispatch({ type: PAGE_SELECTED, page, runtimeId });
  };
}

function updateDebugTargetCollapsibility(key, isCollapsed) {
  return { type: DEBUG_TARGET_COLLAPSIBILITY_UPDATED, key, isCollapsed };
}

function addNetworkLocation(location) {
  return (dispatch, getState) => {
    NetworkLocationsModule.addNetworkLocation(location);
  };
}

function removeNetworkLocation(location) {
  return (dispatch, getState) => {
    NetworkLocationsModule.removeNetworkLocation(location);
  };
}

function updateAdbAddonStatus(adbAddonStatus) {
  return { type: ADB_ADDON_STATUS_UPDATED, adbAddonStatus };
}

function updateNetworkLocations(locations) {
  return { type: NETWORK_LOCATIONS_UPDATED, locations };
}

function installAdbAddon() {
  return async (dispatch, getState) => {
    dispatch({ type: ADB_ADDON_INSTALL_START });

    try {
      // "aboutdebugging" will be forwarded to telemetry as the installation source
      // for the addon.
      await adbAddon.install("about:debugging");
      dispatch({ type: ADB_ADDON_INSTALL_SUCCESS });
    } catch (e) {
      dispatch({ type: ADB_ADDON_INSTALL_FAILURE, error: e });
    }
  };
}

function uninstallAdbAddon() {
  return async (dispatch, getState) => {
    dispatch({ type: ADB_ADDON_UNINSTALL_START });

    try {
      await adbAddon.uninstall();
      dispatch({ type: ADB_ADDON_UNINSTALL_SUCCESS });
    } catch (e) {
      dispatch({ type: ADB_ADDON_UNINSTALL_FAILURE, error: e });
    }
  };
}

function scanUSBRuntimes() {
  return async (dispatch, getState) => {
    // do not re-scan if we are already doing it
    if (getState().ui.isScanningUsb) {
      return;
    }

    dispatch({ type: USB_RUNTIMES_SCAN_START });
    await refreshUSBRuntimes();
    dispatch({ type: USB_RUNTIMES_SCAN_SUCCESS });
  };
}

module.exports = {
  addNetworkLocation,
  installAdbAddon,
  removeNetworkLocation,
  scanUSBRuntimes,
  selectPage,
  uninstallAdbAddon,
  updateAdbAddonStatus,
  updateDebugTargetCollapsibility,
  updateNetworkLocations,
};
