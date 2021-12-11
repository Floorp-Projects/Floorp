/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { combineReducers } = require("devtools/client/shared/vendor/redux");
const batchingReducer = require("devtools/client/netmonitor/src/reducers/batching");
const requestBlockingReducer = require("devtools/client/netmonitor/src/reducers/request-blocking");
const {
  requestsReducer,
} = require("devtools/client/netmonitor/src/reducers/requests");
const { search } = require("devtools/client/netmonitor/src/reducers/search");
const { sortReducer } = require("devtools/client/netmonitor/src/reducers/sort");
const { filters } = require("devtools/client/netmonitor/src/reducers/filters");
const {
  timingMarkers,
} = require("devtools/client/netmonitor/src/reducers/timing-markers");
const { ui } = require("devtools/client/netmonitor/src/reducers/ui");
const {
  messages,
} = require("devtools/client/netmonitor/src/reducers/messages");
const networkThrottling = require("devtools/client/shared/components/throttling/reducer");

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
