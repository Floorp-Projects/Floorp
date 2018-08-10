/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PAGE_SELECTED,
  PAGES
} = require("../constants");

function UiState(locations = []) {
  return {
    networkLocations: locations,
    selectedPage: PAGES.THIS_FIREFOX,
  };
}

function uiReducer(state = UiState(), action) {
  switch (action.type) {
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
