/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  CLEAR_REQUESTS,
  OPEN_NETWORK_DETAILS,
  OPEN_ACTION_BAR,
  RESIZE_NETWORK_DETAILS,
  ENABLE_PERSISTENT_LOGS,
  DISABLE_BROWSER_CACHE,
  OPEN_STATISTICS,
  REMOVE_SELECTED_CUSTOM_REQUEST,
  RESET_COLUMNS,
  RESPONSE_HEADERS,
  SELECT_DETAILS_PANEL_TAB,
  SELECT_ACTION_BAR_TAB,
  SEND_CUSTOM_REQUEST,
  SELECT_REQUEST,
  TOGGLE_COLUMN,
  WATERFALL_RESIZE,
  PANELS,
  MIN_COLUMN_WIDTH,
  SET_COLUMNS_WIDTH,
} = require("devtools/client/netmonitor/src/constants");

const cols = {
  status: true,
  method: true,
  domain: true,
  file: true,
  url: false,
  protocol: false,
  scheme: false,
  remoteip: false,
  initiator: true,
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

function Columns() {
  return Object.assign(
    cols,
    RESPONSE_HEADERS.reduce(
      (acc, header) => Object.assign(acc, { [header]: false }),
      {}
    )
  );
}

function ColumnsData() {
  const defaultColumnsData = JSON.parse(
    Services.prefs
      .getDefaultBranch(null)
      .getCharPref("devtools.netmonitor.columnsData")
  );
  return new Map(defaultColumnsData.map(i => [i.name, i]));
}

function UI(initialState = {}) {
  return {
    columns: Columns(),
    columnsData: ColumnsData(),
    detailsPanelSelectedTab: PANELS.HEADERS,
    networkDetailsOpen: false,
    networkDetailsWidth: null,
    networkDetailsHeight: null,
    persistentLogsEnabled: Services.prefs.getBoolPref(
      "devtools.netmonitor.persistlog"
    ),
    browserCacheDisabled: Services.prefs.getBoolPref("devtools.cache.disabled"),
    statisticsOpen: false,
    waterfallWidth: null,
    networkActionOpen: false,
    selectedActionBarTabId: null,
    ...initialState,
  };
}

function resetColumns(state) {
  return {
    ...state,
    columns: Columns(),
    columnsData: ColumnsData(),
  };
}

function resizeWaterfall(state, action) {
  return {
    ...state,
    waterfallWidth: action.width,
  };
}

function openNetworkDetails(state, action) {
  return {
    ...state,
    networkDetailsOpen: action.open,
  };
}

function openNetworkAction(state, action) {
  return {
    ...state,
    networkActionOpen: action.open,
  };
}

function resizeNetworkDetails(state, action) {
  return {
    ...state,
    networkDetailsWidth: action.width,
    networkDetailsHeight: action.height,
  };
}

function enablePersistentLogs(state, action) {
  return {
    ...state,
    persistentLogsEnabled: action.enabled,
  };
}

function disableBrowserCache(state, action) {
  return {
    ...state,
    browserCacheDisabled: action.disabled,
  };
}

function openStatistics(state, action) {
  return {
    ...state,
    statisticsOpen: action.open,
  };
}

function setDetailsPanelTab(state, action) {
  return {
    ...state,
    detailsPanelSelectedTab: action.id,
  };
}

function setActionBarTab(state, action) {
  return {
    ...state,
    selectedActionBarTabId: action.id,
  };
}

function toggleColumn(state, action) {
  const { column } = action;

  if (!state.columns.hasOwnProperty(column)) {
    return state;
  }

  return {
    ...state,
    columns: {
      ...state.columns,
      [column]: !state.columns[column],
    },
  };
}

function setColumnsWidth(state, action) {
  const { widths } = action;
  const columnsData = new Map(state.columnsData);

  widths.forEach(col => {
    let data = columnsData.get(col.name);
    if (!data) {
      data = {
        name: col.name,
        minWidth: MIN_COLUMN_WIDTH,
      };
    }
    columnsData.set(col.name, {
      ...data,
      width: col.width,
    });
  });

  return {
    ...state,
    columnsData: columnsData,
  };
}

function ui(state = UI(), action) {
  switch (action.type) {
    case CLEAR_REQUESTS:
      return openNetworkDetails(state, { open: false });
    case OPEN_NETWORK_DETAILS:
      return openNetworkDetails(state, action);
    case RESIZE_NETWORK_DETAILS:
      return resizeNetworkDetails(state, action);
    case ENABLE_PERSISTENT_LOGS:
      return enablePersistentLogs(state, action);
    case DISABLE_BROWSER_CACHE:
      return disableBrowserCache(state, action);
    case OPEN_STATISTICS:
      return openStatistics(state, action);
    case RESET_COLUMNS:
      return resetColumns(state);
    case REMOVE_SELECTED_CUSTOM_REQUEST:
      return openNetworkDetails(state, { open: true });
    case SEND_CUSTOM_REQUEST:
      return openNetworkDetails(state, { open: false });
    case SELECT_DETAILS_PANEL_TAB:
      return setDetailsPanelTab(state, action);
    case SELECT_ACTION_BAR_TAB:
      return setActionBarTab(state, action);
    case SELECT_REQUEST:
      return openNetworkDetails(state, { open: true });
    case TOGGLE_COLUMN:
      return toggleColumn(state, action);
    case WATERFALL_RESIZE:
      return resizeWaterfall(state, action);
    case SET_COLUMNS_WIDTH:
      return setColumnsWidth(state, action);
    case OPEN_ACTION_BAR:
      return openNetworkAction(state, action);
    default:
      return state;
  }
}

module.exports = {
  Columns,
  ColumnsData,
  UI,
  ui,
};
