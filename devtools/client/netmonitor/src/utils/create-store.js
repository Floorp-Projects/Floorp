/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { applyMiddleware, createStore } = require("devtools/client/shared/vendor/redux");
const batching = require("../middleware/batching");
const prefs = require("../middleware/prefs");
const thunk = require("../middleware/thunk");
const rootReducer = require("../reducers/index");
const { FilterTypes, Filters } = require("../reducers/filters");
const { Requests } = require("../reducers/requests");
const { Sort } = require("../reducers/sort");
const { TimingMarkers } = require("../reducers/timing-markers");
const { UI, Columns } = require("../reducers/ui");

function configureStore() {
  const getPref = (pref) => {
    try {
      return JSON.parse(Services.prefs.getCharPref(pref));
    } catch (_) {
      return [];
    }
  };

  let activeFilters = {};
  let filters = getPref("devtools.netmonitor.filters");
  filters.forEach((filter) => {
    activeFilters[filter] = true;
  });

  let columns = new Columns();
  let hiddenColumns = getPref("devtools.netmonitor.hiddenColumns");

  for (let [col] of columns) {
    columns = columns.withMutations((state) => {
      state.set(col, !hiddenColumns.includes(col));
    });
  }

  const initialState = {
    filters: new Filters({
      requestFilterTypes: new FilterTypes(activeFilters)
    }),
    requests: new Requests(),
    sort: new Sort(),
    timingMarkers: new TimingMarkers(),
    ui: new UI({
      columns,
    }),
  };

  return createStore(rootReducer, initialState, applyMiddleware(thunk, prefs, batching));
}

exports.configureStore = configureStore;
