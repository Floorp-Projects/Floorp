/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { applyMiddleware, createStore } = require("devtools/client/shared/vendor/redux");

// Middleware
const batching = require("./middleware/batching");
const prefs = require("./middleware/prefs");
const thunk = require("./middleware/thunk");
const recording = require("./middleware/recording");
const throttling = require("./middleware/throttling");

// Reducers
const rootReducer = require("./reducers/index");
const { FilterTypes, Filters } = require("./reducers/filters");
const { Requests } = require("./reducers/requests");
const { Sort } = require("./reducers/sort");
const { TimingMarkers } = require("./reducers/timing-markers");
const { UI, Columns } = require("./reducers/ui");

/**
 * Configure state and middleware for the Network monitor tool.
 */
function configureStore(connector) {
  // Prepare initial state.
  const initialState = {
    filters: new Filters({
      requestFilterTypes: getFilterState()
    }),
    requests: new Requests(),
    sort: new Sort(),
    timingMarkers: new TimingMarkers(),
    ui: UI({
      columns: getColumnState()
    }),
  };

  // Prepare middleware.
  let middleware = applyMiddleware(
    thunk,
    prefs,
    batching,
    recording(connector),
    throttling(connector),
  );

  return createStore(rootReducer, initialState, middleware);
}

// Helpers

/**
 * Get column state from preferences.
 */
function getColumnState() {
  let columns = Columns();
  let visibleColumns = getPref("devtools.netmonitor.visibleColumns");

  const state = {};
  for (let col in columns) {
    state[col] = visibleColumns.includes(col);
  }

  return state;
}

/**
 * Get filter state from preferences.
 */
function getFilterState() {
  let activeFilters = {};
  let filters = getPref("devtools.netmonitor.filters");
  filters.forEach((filter) => {
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
