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
  ADB_READY_UPDATED,
  DEBUG_TARGET_COLLAPSIBILITY_UPDATED,
  HIDE_PROFILER_DIALOG,
  NETWORK_LOCATIONS_UPDATE_FAILURE,
  NETWORK_LOCATIONS_UPDATE_START,
  NETWORK_LOCATIONS_UPDATE_SUCCESS,
  PAGE_TYPES,
  SELECT_PAGE_FAILURE,
  SELECT_PAGE_START,
  SELECT_PAGE_SUCCESS,
  SELECTED_RUNTIME_ID_UPDATED,
  SHOW_PROFILER_DIALOG,
  USB_RUNTIMES_SCAN_START,
  USB_RUNTIMES_SCAN_SUCCESS,
} = require("../constants");

const NetworkLocationsModule = require("../modules/network-locations");
const { adbAddon } = require("devtools/shared/adb/adb-addon");
const { refreshUSBRuntimes } = require("../modules/usb-runtimes");

const Actions = require("./index");

function selectPage(page, runtimeId) {
  return async (dispatch, getState) => {
    dispatch({ type: SELECT_PAGE_START });

    try {
      const isSamePage = (oldPage, newPage) => {
        if (newPage === PAGE_TYPES.RUNTIME && oldPage === PAGE_TYPES.RUNTIME) {
          return runtimeId === getState().runtimes.selectedRuntimeId;
        }
        return newPage === oldPage;
      };

      if (!page) {
        throw new Error("No page provided.");
      }

      const currentPage = getState().ui.selectedPage;
      // Nothing to dispatch if the page is the same as the current page
      if (isSamePage(currentPage, page)) {
        return;
      }

      // Stop showing the profiler dialog if we are navigating to another page.
      if (getState().ui.showProfilerDialog) {
        await dispatch({ type: HIDE_PROFILER_DIALOG });
      }

      // Stop watching current runtime, if currently on a RUNTIME page.
      if (currentPage === PAGE_TYPES.RUNTIME) {
        const currentRuntimeId = getState().runtimes.selectedRuntimeId;
        await dispatch(Actions.unwatchRuntime(currentRuntimeId));
      }

      // Always update the selected runtime id.
      // If we are navigating to a non-runtime page, the Runtime page components are no
      // longer rendered so it is safe to nullify the runtimeId.
      // If we are navigating to a runtime page, the runtime corresponding to runtimeId
      // is already connected, so components can safely get runtimeDetails on this new
      // runtime.
      dispatch({ type: SELECTED_RUNTIME_ID_UPDATED, runtimeId });

      // Start watching current runtime, if moving to a RUNTIME page.
      if (page === PAGE_TYPES.RUNTIME) {
        await dispatch(Actions.watchRuntime(runtimeId));
      }

      dispatch({ type: SELECT_PAGE_SUCCESS, page });
    } catch (e) {
      dispatch({ type: SELECT_PAGE_FAILURE, error: e });
    }
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

function showProfilerDialog() {
  return { type: SHOW_PROFILER_DIALOG };
}

function hideProfilerDialog() {
  return { type: HIDE_PROFILER_DIALOG };
}

function updateAdbAddonStatus(adbAddonStatus) {
  return { type: ADB_ADDON_STATUS_UPDATED, adbAddonStatus };
}

function updateAdbReady(isAdbReady) {
  return { type: ADB_READY_UPDATED, isAdbReady };
}

function updateNetworkLocations(locations) {
  return async (dispatch, getState) => {
    dispatch({ type: NETWORK_LOCATIONS_UPDATE_START });
    try {
      await dispatch(Actions.updateNetworkRuntimes(locations));
      dispatch({ type: NETWORK_LOCATIONS_UPDATE_SUCCESS, locations });
    } catch (e) {
      dispatch({ type: NETWORK_LOCATIONS_UPDATE_FAILURE, error: e });
    }
  };
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
  hideProfilerDialog,
  installAdbAddon,
  removeNetworkLocation,
  scanUSBRuntimes,
  selectPage,
  showProfilerDialog,
  uninstallAdbAddon,
  updateAdbAddonStatus,
  updateAdbReady,
  updateDebugTargetCollapsibility,
  updateNetworkLocations,
};
