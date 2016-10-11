/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  OPEN_SIDEBAR,
  WATERFALL_RESIZE,
} = require("../constants");

/**
 * Change sidebar open state.
 *
 * @param {boolean} open - open state
 */
function openSidebar(open) {
  return {
    type: OPEN_SIDEBAR,
    open,
  };
}

/**
 * Toggle sidebar open state.
 */
function toggleSidebar() {
  return (dispatch, getState) => dispatch(openSidebar(!getState().ui.sidebarOpen));
}

/**
 * Waterfall width has changed (likely on window resize). Update the UI.
 */
function resizeWaterfall(width) {
  return {
    type: WATERFALL_RESIZE,
    width
  };
}

module.exports = {
  openSidebar,
  toggleSidebar,
  resizeWaterfall,
};
