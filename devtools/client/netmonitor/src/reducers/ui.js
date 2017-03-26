/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const I = require("devtools/client/shared/vendor/immutable");
const {
  CLEAR_REQUESTS,
  OPEN_NETWORK_DETAILS,
  OPEN_STATISTICS,
  REMOVE_SELECTED_CUSTOM_REQUEST,
  SELECT_DETAILS_PANEL_TAB,
  SEND_CUSTOM_REQUEST,
  SELECT_REQUEST,
  WATERFALL_RESIZE,
} = require("../constants");

const UI = I.Record({
  detailsPanelSelectedTab: "headers",
  networkDetailsOpen: false,
  statisticsOpen: false,
  waterfallWidth: null,
});

// Safe bounds for waterfall width (px)
const REQUESTS_WATERFALL_SAFE_BOUNDS = 90;

function resizeWaterfall(state, action) {
  return state.set("waterfallWidth", action.width - REQUESTS_WATERFALL_SAFE_BOUNDS);
}

function openNetworkDetails(state, action) {
  return state.set("networkDetailsOpen", action.open);
}

function openStatistics(state, action) {
  return state.set("statisticsOpen", action.open);
}

function setDetailsPanelTab(state, action) {
  return state.set("detailsPanelSelectedTab", action.id);
}

function ui(state = new UI(), action) {
  switch (action.type) {
    case CLEAR_REQUESTS:
      return openNetworkDetails(state, { open: false });
    case OPEN_NETWORK_DETAILS:
      return openNetworkDetails(state, action);
    case OPEN_STATISTICS:
      return openStatistics(state, action);
    case REMOVE_SELECTED_CUSTOM_REQUEST:
    case SEND_CUSTOM_REQUEST:
      return openNetworkDetails(state, { open: false });
    case SELECT_DETAILS_PANEL_TAB:
      return setDetailsPanelTab(state, action);
    case SELECT_REQUEST:
      return openNetworkDetails(state, { open: true });
    case WATERFALL_RESIZE:
      return resizeWaterfall(state, action);
    default:
      return state;
  }
}

module.exports = {
  UI,
  ui
};
