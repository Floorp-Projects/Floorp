/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ACTIVITY_TYPE,
  OPEN_NETWORK_DETAILS,
  RESIZE_NETWORK_DETAILS,
  ENABLE_PERSISTENT_LOGS,
  DISABLE_BROWSER_CACHE,
  OPEN_STATISTICS,
  RESET_COLUMNS,
  SELECT_DETAILS_PANEL_TAB,
  TOGGLE_COLUMN,
  WATERFALL_RESIZE,
} = require("../constants");

const { getDisplayedRequests } = require("../selectors/index");

/**
 * Change network details panel.
 *
 * @param {boolean} open - expected network details panel open state
 */
function openNetworkDetails(open) {
  return (dispatch, getState) => {
    const visibleRequestItems = getDisplayedRequests(getState());
    const defaultSelectedId = visibleRequestItems.length
      ? visibleRequestItems[0].id
      : null;

    return dispatch({
      type: OPEN_NETWORK_DETAILS,
      open,
      defaultSelectedId,
    });
  };
}

/**
 * Change network details panel size.
 *
 * @param {integer} width
 * @param {integer} height
 */
function resizeNetworkDetails(width, height) {
  return {
    type: RESIZE_NETWORK_DETAILS,
    width,
    height,
  };
}

/**
 * Change persistent logs state.
 *
 * @param {boolean} enabled - expected persistent logs enabled state
 */
function enablePersistentLogs(enabled) {
  return {
    type: ENABLE_PERSISTENT_LOGS,
    enabled,
  };
}

/**
 * Change browser cache state.
 *
 * @param {boolean} disabled - expected browser cache in disable state
 */
function disableBrowserCache(disabled) {
  return {
    type: DISABLE_BROWSER_CACHE,
    disabled,
  };
}

/**
 * Change performance statistics panel open state.
 *
 * @param {Object} connector - connector object to the backend
 * @param {boolean} visible - expected performance statistics panel open state
 */
function openStatistics(connector, open) {
  if (open) {
    connector.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);
  }
  return {
    type: OPEN_STATISTICS,
    open,
  };
}

/**
 * Resets all columns to their default state.
 *
 */
function resetColumns() {
  return {
    type: RESET_COLUMNS,
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
 * Toggles a column
 *
 * @param {string} column - The column that is going to be toggled
 */
function toggleColumn(column) {
  return {
    type: TOGGLE_COLUMN,
    column,
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
 * Toggle persistent logs status.
 */
function togglePersistentLogs() {
  return (dispatch, getState) =>
    dispatch(enablePersistentLogs(!getState().ui.persistentLogsEnabled));
}

/**
 * Toggle browser cache status.
 */
function toggleBrowserCache() {
  return (dispatch, getState) =>
    dispatch(disableBrowserCache(!getState().ui.browserCacheDisabled));
}

/**
 * Toggle performance statistics panel.
 */
function toggleStatistics(connector) {
  return (dispatch, getState) =>
    dispatch(openStatistics(connector, !getState().ui.statisticsOpen));
}

module.exports = {
  openNetworkDetails,
  resizeNetworkDetails,
  enablePersistentLogs,
  disableBrowserCache,
  openStatistics,
  resetColumns,
  resizeWaterfall,
  selectDetailsPanelTab,
  toggleColumn,
  toggleNetworkDetails,
  togglePersistentLogs,
  toggleBrowserCache,
  toggleStatistics,
};
