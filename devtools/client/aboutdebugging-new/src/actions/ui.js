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
  PAGES,
} = require("../constants");

const NetworkLocationsModule = require("../modules/network-locations");
const { adbAddon } = require("devtools/shared/adb/adb-addon");

const Actions = require("./index");

// XXX: Isolating the code here, because it feels wrong to rely solely on the page "not"
// being CONNECT to decide what to do. Should we have a page "type" on top of page "id"?
function _isRuntimePage(page) {
  return page && page !== PAGES.CONNECT;
}

function selectPage(page, runtimeId) {
  return async (dispatch, getState) => {
    const currentPage = getState().ui.selectedPage;
    // Nothing to dispatch if the page is the same as the current page.
    if (page === currentPage) {
      return;
    }

    // Stop watching current runtime, if currently on a DEVICE or THIS_FIREFOX page.
    if (_isRuntimePage(currentPage)) {
      const currentRuntimeId = getState().runtimes.selectedRuntimeId;
      await dispatch(Actions.unwatchRuntime(currentRuntimeId));
    }

    // Start watching current runtime, if moving to a DEVICE or THIS_FIREFOX page.
    if (_isRuntimePage(page)) {
      await dispatch(Actions.watchRuntime(runtimeId));
    }

    dispatch({ type: PAGE_SELECTED, page });
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
      await adbAddon.install();
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

module.exports = {
  addNetworkLocation,
  installAdbAddon,
  removeNetworkLocation,
  selectPage,
  uninstallAdbAddon,
  updateAdbAddonStatus,
  updateDebugTargetCollapsibility,
  updateNetworkLocations,
};
