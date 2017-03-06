/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createStore, applyMiddleware } = require("devtools/client/shared/vendor/redux");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk");
const batching = require("./middleware/batching");
const prefs = require("./middleware/prefs");
const rootReducer = require("./reducers/index");
const { Prefs } = require("./utils/prefs");
const { FilterTypes, Filters } = require("./reducers/filters");
const { Requests } = require("./reducers/requests");
const { Sort } = require("./reducers/sort");
const { TimingMarkers } = require("./reducers/timing-markers");
const { UI } = require("./reducers/ui");

function configureStore() {
  let activeFilters = {};
  Prefs.filters.forEach((filter) => {
    activeFilters[filter] = true;
  });
  const initialState = {
    filters: new Filters({
      requestFilterTypes: new FilterTypes(activeFilters)
    }),
    requests: new Requests(),
    sort: new Sort(),
    timingMarkers: new TimingMarkers(),
    ui: new UI()
  };

  return createStore(
    rootReducer,
    initialState,
    applyMiddleware(
      thunk,
      prefs,
      batching
    )
  );
}

exports.configureStore = configureStore;
