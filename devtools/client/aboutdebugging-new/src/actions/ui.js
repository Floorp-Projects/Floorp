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

const Actions = require("./index");

function selectPage(page) {
  return async (dispatch, getState) => {
    const currentPage = getState().ui.selectedPage;
    if (page === currentPage) {
      // Nothing to dispatch if the page is the same as the current page.
      return;
    }

    if (page === PAGES.THIS_FIREFOX) {
      await dispatch(Actions.connectRuntime());
    } else {
      await dispatch(Actions.disconnectRuntime());
    }

    dispatch({ type: PAGE_SELECTED, page });
  };
}

function updateDebugTargetCollapsibility(key, isCollapsed) {
  return { type: DEBUG_TARGET_COLLAPSIBILITY_UPDATED, key, isCollapsed };
}

function updateNetworkLocations(locations) {
  return { type: NETWORK_LOCATIONS_UPDATED, locations };
}

module.exports = {
  selectPage,
  updateDebugTargetCollapsibility,
  updateNetworkLocations,
};
