/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  OPEN_SIDEBAR,
  OPEN_STATISTICS,
  SELECT_DETAILS_PANEL_TAB,
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
 * Change performance statistics view open state.
 *
 * @param {boolean} visible - expected sidebar open state
 */
function openStatistics(open) {
  return {
    type: OPEN_STATISTICS,
    open,
  };
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

/**
 * Change the selected tab for details panel.
 *
 * @param {number} index - tab index to be selected
 */
function selectDetailsPanelTab(index) {
  return {
    type: SELECT_DETAILS_PANEL_TAB,
    index,
  };
}

/**
 * Toggle sidebar open state.
 */
function toggleSidebar() {
  return (dispatch, getState) => dispatch(openSidebar(!getState().ui.sidebarOpen));
}

/**
 * Toggle to show/hide performance statistics view.
 */
function toggleStatistics() {
  return (dispatch, getState) => dispatch(openStatistics(!getState().ui.statisticsOpen));
}

module.exports = {
  openSidebar,
  openStatistics,
  resizeWaterfall,
  selectDetailsPanelTab,
  toggleSidebar,
  toggleStatistics,
};
