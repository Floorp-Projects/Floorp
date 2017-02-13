/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ACTIVITY_TYPE,
  OPEN_NETWORK_DETAILS,
  OPEN_STATISTICS,
  SELECT_DETAILS_PANEL_TAB,
  WATERFALL_RESIZE,
} = require("../constants");

/**
 * Change network details panel.
 *
 * @param {boolean} open - expected network details panel open state
 */
function openNetworkDetails(open) {
  return {
    type: OPEN_NETWORK_DETAILS,
    open,
  };
}

/**
 * Change performance statistics panel open state.
 *
 * @param {boolean} visible - expected performance statistics panel open state
 */
function openStatistics(open) {
  if (open) {
    window.NetMonitorController.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);
  }
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
 * Change the selected tab for network details panel.
 *
 * @param {string} id - tab id to be selected
 */
function selectDetailsPanelTab(id) {
  return {
    type: SELECT_DETAILS_PANEL_TAB,
    id,
  };
}

/**
 * Toggle network details panel.
 */
function toggleNetworkDetails() {
  return (dispatch, getState) =>
    dispatch(openNetworkDetails(!getState().ui.networkDetailsOpen));
}

/**
 * Toggle performance statistics panel.
 */
function toggleStatistics() {
  return (dispatch, getState) =>
    dispatch(openStatistics(!getState().ui.statisticsOpen));
}

module.exports = {
  openNetworkDetails,
  openStatistics,
  resizeWaterfall,
  selectDetailsPanelTab,
  toggleNetworkDetails,
  toggleStatistics,
};
