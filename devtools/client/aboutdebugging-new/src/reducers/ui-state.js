/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DEBUG_TARGET_COLLAPSIBILITY_UPDATED,
  NETWORK_LOCATIONS_UPDATED,
  PAGE_SELECTED,
} = require("../constants");

function UiState(locations = [], debugTargetCollapsibilities = {}) {
  return {
    debugTargetCollapsibilities,
    networkLocations: locations,
    selectedPage: null,
  };
}

function uiReducer(state = UiState(), action) {
  switch (action.type) {
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
      const { page } = action;
      return Object.assign({}, state, { selectedPage: page });
    }

    default:
      return state;
  }
}

module.exports = {
  UiState,
  uiReducer,
};
