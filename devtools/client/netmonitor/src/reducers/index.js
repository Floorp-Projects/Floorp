/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  combineReducers,
} = require("resource://devtools/client/shared/vendor/redux.js");
const batchingReducer = require("resource://devtools/client/netmonitor/src/reducers/batching.js");
const requestBlockingReducer = require("resource://devtools/client/netmonitor/src/reducers/request-blocking.js");
const {
  requestsReducer,
} = require("resource://devtools/client/netmonitor/src/reducers/requests.js");
const {
  search,
} = require("resource://devtools/client/netmonitor/src/reducers/search.js");
const {
  sortReducer,
} = require("resource://devtools/client/netmonitor/src/reducers/sort.js");
const {
  filters,
} = require("resource://devtools/client/netmonitor/src/reducers/filters.js");
const {
  timingMarkers,
} = require("resource://devtools/client/netmonitor/src/reducers/timing-markers.js");
const {
  ui,
} = require("resource://devtools/client/netmonitor/src/reducers/ui.js");
const {
  messages,
} = require("resource://devtools/client/netmonitor/src/reducers/messages.js");
const networkThrottling = require("resource://devtools/client/shared/components/throttling/reducer.js");

module.exports = batchingReducer(
  combineReducers({
    filters,
    messages,
    networkThrottling,
    requestBlocking: requestBlockingReducer,
    requests: requestsReducer,
    search,
    sort: sortReducer,
    timingMarkers,
    ui,
  })
);
