/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  applyMiddleware,
  createStore,
} = require("devtools/client/shared/vendor/redux");

const {
  waitUntilService,
} = require("devtools/client/shared/redux/middleware/wait-service.js");

const { MIN_COLUMN_WIDTH, DEFAULT_COLUMN_WIDTH } = require("./constants");

// Middleware
const batching = require("./middleware/batching");
const prefs = require("./middleware/prefs");
const thunk = require("./middleware/thunk");
const recording = require("./middleware/recording");
const throttling = require("./middleware/throttling");
const eventTelemetry = require("./middleware/event-telemetry");

// Reducers
const rootReducer = require("./reducers/index");
const { FilterTypes, Filters } = require("./reducers/filters");
const { Requests } = require("./reducers/requests");
const { Sort } = require("./reducers/sort");
const { TimingMarkers } = require("./reducers/timing-markers");
const { UI, Columns, ColumnsData } = require("./reducers/ui");
const {
  WebSockets,
  getWebSocketsDefaultColumnsState,
} = require("./reducers/web-sockets");
const { Search } = require("./reducers/search");

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
    webSockets: WebSockets({
      columns: getWebSocketsColumnState(),
    }),
    search: new Search(),
  };

  // Prepare middleware.
  const middleware = applyMiddleware(
    thunk,
    prefs,
    batching,
    recording(connector),
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
 * Get column state of WebSockets from preferences.
 */
function getWebSocketsColumnState() {
  const columns = getWebSocketsDefaultColumnsState();
  const visibleColumns = getPref("devtools.netmonitor.ws.visibleColumns");

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

function getPref(pref) {
  try {
    return JSON.parse(Services.prefs.getCharPref(pref));
  } catch (_) {
    return [];
  }
}

exports.configureStore = configureStore;
