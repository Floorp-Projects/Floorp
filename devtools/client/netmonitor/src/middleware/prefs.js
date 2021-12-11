/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  ENABLE_REQUEST_FILTER_TYPE_ONLY,
  RESET_COLUMNS,
  TOGGLE_COLUMN,
  TOGGLE_REQUEST_FILTER_TYPE,
  ENABLE_PERSISTENT_LOGS,
  DISABLE_BROWSER_CACHE,
  SET_COLUMNS_WIDTH,
  WS_TOGGLE_COLUMN,
  WS_RESET_COLUMNS,
} = require("devtools/client/netmonitor/src/constants");

/**
 * Update the relevant prefs when:
 *   - a column has been toggled
 *   - a filter type has been set
 */
function prefsMiddleware(store) {
  return next => action => {
    const res = next(action);
    switch (action.type) {
      case ENABLE_REQUEST_FILTER_TYPE_ONLY:
      case TOGGLE_REQUEST_FILTER_TYPE:
        const filters = Object.entries(
          store.getState().filters.requestFilterTypes
        )
          .filter(([type, check]) => check)
          .map(([type, check]) => type);
        Services.prefs.setCharPref(
          "devtools.netmonitor.filters",
          JSON.stringify(filters)
        );
        break;
      case ENABLE_PERSISTENT_LOGS:
        const enabled = store.getState().ui.persistentLogsEnabled;
        Services.prefs.setBoolPref("devtools.netmonitor.persistlog", enabled);
        break;
      case DISABLE_BROWSER_CACHE:
        Services.prefs.setBoolPref(
          "devtools.cache.disabled",
          store.getState().ui.browserCacheDisabled
        );
        break;
      case TOGGLE_COLUMN:
        persistVisibleColumns(store.getState());
        break;
      case RESET_COLUMNS:
        persistVisibleColumns(store.getState());
        persistColumnsData(store.getState());
        break;
      case SET_COLUMNS_WIDTH:
        persistColumnsData(store.getState());
        break;
      case WS_TOGGLE_COLUMN:
      case WS_RESET_COLUMNS:
        persistVisibleWebSocketsColumns(store.getState());
        break;
    }
    return res;
  };
}

/**
 * Store list of visible columns into preferences.
 */
function persistVisibleColumns(state) {
  const visibleColumns = [];
  const { columns } = state.ui;
  for (const column in columns) {
    if (columns[column]) {
      visibleColumns.push(column);
    }
  }

  Services.prefs.setCharPref(
    "devtools.netmonitor.visibleColumns",
    JSON.stringify(visibleColumns)
  );
}

/**
 * Store list of visible columns into preferences.
 */
function persistVisibleWebSocketsColumns(state) {
  const visibleColumns = [];
  const { columns } = state.messages;
  for (const column in columns) {
    if (columns[column]) {
      visibleColumns.push(column);
    }
  }

  Services.prefs.setCharPref(
    "devtools.netmonitor.msg.visibleColumns",
    JSON.stringify(visibleColumns)
  );
}

/**
 * Store columns data (width, min-width, etc.) into preferences.
 */
function persistColumnsData(state) {
  const columnsData = [...state.ui.columnsData.values()];
  Services.prefs.setCharPref(
    "devtools.netmonitor.columnsData",
    JSON.stringify(columnsData)
  );
}

module.exports = prefsMiddleware;
