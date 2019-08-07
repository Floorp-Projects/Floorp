/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { combineReducers } = require("devtools/client/shared/vendor/redux");
const batchingReducer = require("./batching");
const { requestsReducer } = require("./requests");
const { search } = require("./search");
const { sortReducer } = require("./sort");
const { filters } = require("./filters");
const { timingMarkers } = require("./timing-markers");
const { ui } = require("./ui");
const { webSockets } = require("./web-sockets");
const networkThrottling = require("devtools/client/shared/components/throttling/reducer");

module.exports = batchingReducer(
  combineReducers({
    requests: requestsReducer,
    search,
    sort: sortReducer,
    webSockets,
    filters,
    timingMarkers,
    ui,
    networkThrottling,
  })
);
