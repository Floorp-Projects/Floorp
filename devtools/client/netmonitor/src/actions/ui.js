/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const Services = require("Services");
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
  SET_COLUMNS_WIDTH,
} = require("../constants");

const { getDisplayedRequests } = require("../selectors/index");

const DEVTOOLS_DISABLE_CACHE_PREF = "devtools.cache.disabled";

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
function enablePersistentLogs(enabled, skipTelemetry = false) {
  return {
    type: ENABLE_PERSISTENT_LOGS,
    enabled,
    skipTelemetry,
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
  } else if (Services.prefs.getBoolPref(DEVTOOLS_DISABLE_CACHE_PREF)) {
    // Opening the Statistics panel reconfigures the page and enables
    // the browser cache (using ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED).
    // So, make sure to disable the cache again when the user returns back
    // from the Statistics panel (if DEVTOOLS_DISABLE_CACHE_PREF == true).
    // See also bug 1430359.
    connector.triggerActivity(ACTIVITY_TYPE.DISABLE_CACHE);
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
    width,
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
 * Set width of multiple columns
 *
 * @param {array} widths - array of pairs {name, width}
 */
function setColumnsWidth(widths) {
  return {
    type: SET_COLUMNS_WIDTH,
    widths,
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
  setColumnsWidth,
  toggleNetworkDetails,
  togglePersistentLogs,
  toggleBrowserCache,
  toggleStatistics,
};
