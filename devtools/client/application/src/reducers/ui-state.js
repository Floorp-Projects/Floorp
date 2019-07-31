/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DEFAULT_PAGE, UPDATE_SELECTED_PAGE } = require("../constants");

function UiState() {
  return {
    selectedPage: DEFAULT_PAGE,
  };
}

function uiReducer(state = UiState(), action) {
  switch (action.type) {
    case UPDATE_SELECTED_PAGE:
      return Object.assign({}, state, { selectedPage: action.selectedPage });
    default:
      return state;
  }
}

module.exports = {
  UiState,
  uiReducer,
};
