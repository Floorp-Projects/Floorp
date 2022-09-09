/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  applyMiddleware,
  createStore,
} = require("devtools/client/shared/vendor/redux");

const {
  waitUntilService,
} = require("devtools/client/shared/redux/middleware/wait-service.js");

const {
  MIN_COLUMN_WIDTH,
  DEFAULT_COLUMN_WIDTH,
} = require("devtools/client/netmonitor/src/constants");

// Middleware
const batching = require("devtools/client/netmonitor/src/middleware/batching");
const prefs = require("devtools/client/netmonitor/src/middleware/prefs");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk");
const throttling = require("devtools/client/netmonitor/src/middleware/throttling");
const eventTelemetry = require("devtools/client/netmonitor/src/middleware/event-telemetry");
const requestBlocking = require("devtools/client/netmonitor/src/middleware/request-blocking");

// Reducers
const rootReducer = require("devtools/client/netmonitor/src/reducers/index");
const {
  FilterTypes,
  Filters,
} = require("devtools/client/netmonitor/src/reducers/filters");
const {
  Requests,
} = require("devtools/client/netmonitor/src/reducers/requests");
const { Sort } = require("devtools/client/netmonitor/src/reducers/sort");
const {
  TimingMarkers,
} = require("devtools/client/netmonitor/src/reducers/timing-markers");
const {
  UI,
  Columns,
  ColumnsData,
} = require("devtools/client/netmonitor/src/reducers/ui");
const {
  Messages,
  getMessageDefaultColumnsState,
} = require("devtools/client/netmonitor/src/reducers/messages");
const { Search } = require("devtools/client/netmonitor/src/reducers/search");

/**
 * Configure state and middleware for the Network monitor tool.
 */
function configureStore(connector, telemetry) {
  // Prepare initial state.
  const initialState = {
    filters: new Filters({
      requestFilterTypes: getFilterState(),
    }),
    requests: new Requests(),
    sort: new Sort(),
    timingMarkers: new TimingMarkers(),
    ui: UI({
      columns: getColumnState(),
      columnsData: getColumnsData(),
    }),
    messages: Messages({
      columns: getMessageColumnState(),
    }),
    search: new Search(),
  };

  // Prepare middleware.
  const middleware = applyMiddleware(
    requestBlocking(connector),
    thunk({ connector }),
    prefs,
    batching,
    throttling(connector),
    eventTelemetry(connector, telemetry),
    waitUntilService
  );

  return createStore(rootReducer, initialState, middleware);
}

// Helpers

/**
 * Get column state from preferences.
 */
function getColumnState() {
  const columns = Columns();
  const visibleColumns = getPref("devtools.netmonitor.visibleColumns");

  const state = {};
  for (const col in columns) {
    state[col] = visibleColumns.includes(col);
  }

  return state;
}

/**
 * Get column state of Messages from preferences.
 */
function getMessageColumnState() {
  const columns = getMessageDefaultColumnsState();
  const visibleColumns = getPref("devtools.netmonitor.msg.visibleColumns");

  const state = {};
  for (const col in columns) {
    state[col] = visibleColumns.includes(col);
  }

  return state;
}

/**
 * Get columns data (width, min-width)
 */
function getColumnsData() {
  const columnsData = getPref("devtools.netmonitor.columnsData");
  if (!columnsData.length) {
    return ColumnsData();
  }

  const newMap = new Map();
  columnsData.forEach(col => {
    if (col.name) {
      col.minWidth = col.minWidth ? col.minWidth : MIN_COLUMN_WIDTH;
      col.width = col.width ? col.width : DEFAULT_COLUMN_WIDTH;
      newMap.set(col.name, col);
    }
  });

  return newMap;
}

/**
 * Get filter state from preferences.
 */
function getFilterState() {
  const activeFilters = {};
  const filters = getPref("devtools.netmonitor.filters");
  filters.forEach(filter => {
    activeFilters[filter] = true;
  });
  return new FilterTypes(activeFilters);
}

/**
 * Get json data from preferences
 */

function getPref(pref) {
  try {
    return JSON.parse(Services.prefs.getCharPref(pref));
  } catch (_) {
    return [];
  }
}

exports.configureStore = configureStore;
