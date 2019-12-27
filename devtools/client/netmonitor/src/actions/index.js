/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const batching = require("devtools/client/netmonitor/src/actions/batching");
const filters = require("devtools/client/netmonitor/src/actions/filters");
const requests = require("devtools/client/netmonitor/src/actions/requests");
const selection = require("devtools/client/netmonitor/src/actions/selection");
const sort = require("devtools/client/netmonitor/src/actions/sort");
const timingMarkers = require("devtools/client/netmonitor/src/actions/timing-markers");
const ui = require("devtools/client/netmonitor/src/actions/ui");
const webSockets = require("devtools/client/netmonitor/src/actions/web-sockets");
const search = require("devtools/client/netmonitor/src/actions/search");
const requestBlocking = require("devtools/client/netmonitor/src/actions/request-blocking");

Object.assign(
  exports,
  batching,
  filters,
  requests,
  search,
  selection,
  sort,
  timingMarkers,
  ui,
  webSockets,
  requestBlocking
);
