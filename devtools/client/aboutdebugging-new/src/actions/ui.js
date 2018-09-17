/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DEBUG_TARGET_COLLAPSIBILITY_UPDATED,
  NETWORK_LOCATIONS_UPDATED,
  PAGE_SELECTED,
  PAGES,
} = require("../constants");

const NetworkLocationsModule = require("../modules/network-locations");

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

function updateNetworkLocations(locations) {
  return { type: NETWORK_LOCATIONS_UPDATED, locations };
}

module.exports = {
  addNetworkLocation,
  removeNetworkLocation,
  selectPage,
  updateDebugTargetCollapsibility,
  updateNetworkLocations,
};
