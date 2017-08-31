/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const I = require("devtools/client/shared/vendor/immutable");
const Services = require("Services");
const {
  CLEAR_REQUESTS,
  OPEN_NETWORK_DETAILS,
  DISABLE_BROWSER_CACHE,
  OPEN_STATISTICS,
  REMOVE_SELECTED_CUSTOM_REQUEST,
  RESET_COLUMNS,
  RESPONSE_HEADERS,
  SELECT_DETAILS_PANEL_TAB,
  SEND_CUSTOM_REQUEST,
  SELECT_REQUEST,
  TOGGLE_COLUMN,
  WATERFALL_RESIZE,
  PANELS,
} = require("../constants");

const cols = {
  status: true,
  method: true,
  file: true,
  protocol: false,
  scheme: false,
  domain: true,
  remoteip: false,
  cause: true,
  type: true,
  cookies: false,
  setCookies: false,
  transferred: true,
  contentSize: true,
  startTime: false,
  endTime: false,
  responseTime: false,
  duration: false,
  latency: false,
  waterfall: true,
};
const Columns = I.Record(
  Object.assign(
    cols,
    RESPONSE_HEADERS.reduce((acc, header) => Object.assign(acc, { [header]: false }), {})
  )
);

const UI = I.Record({
  columns: new Columns(),
  detailsPanelSelectedTab: PANELS.HEADERS,
  networkDetailsOpen: false,
  browserCacheDisabled: Services.prefs.getBoolPref("devtools.cache.disabled"),
  statisticsOpen: false,
  waterfallWidth: null,
});

function resetColumns(state) {
  return state.set("columns", new Columns());
}

function resizeWaterfall(state, action) {
  return state.set("waterfallWidth", action.width);
}

function openNetworkDetails(state, action) {
  return state.set("networkDetailsOpen", action.open);
}

function disableBrowserCache(state, action) {
  return state.set("browserCacheDisabled", action.disabled);
}

function openStatistics(state, action) {
  return state.set("statisticsOpen", action.open);
}

function setDetailsPanelTab(state, action) {
  return state.set("detailsPanelSelectedTab", action.id);
}

function toggleColumn(state, action) {
  let { column } = action;

  if (!state.has(column)) {
    return state;
  }

  let newState = state.withMutations(columns => {
    columns.set(column, !state.get(column));
  });
  return newState;
}

function ui(state = new UI(), action) {
  switch (action.type) {
    case CLEAR_REQUESTS:
      return openNetworkDetails(state, { open: false });
    case OPEN_NETWORK_DETAILS:
      return openNetworkDetails(state, action);
    case DISABLE_BROWSER_CACHE:
      return disableBrowserCache(state, action);
    case OPEN_STATISTICS:
      return openStatistics(state, action);
    case RESET_COLUMNS:
      return resetColumns(state);
    case REMOVE_SELECTED_CUSTOM_REQUEST:
    case SEND_CUSTOM_REQUEST:
      return openNetworkDetails(state, { open: false });
    case SELECT_DETAILS_PANEL_TAB:
      return setDetailsPanelTab(state, action);
    case SELECT_REQUEST:
      return openNetworkDetails(state, { open: true });
    case TOGGLE_COLUMN:
      return state.set("columns", toggleColumn(state.columns, action));
    case WATERFALL_RESIZE:
      return resizeWaterfall(state, action);
    default:
      return state;
  }
}

module.exports = {
  Columns,
  UI,
  ui
};
