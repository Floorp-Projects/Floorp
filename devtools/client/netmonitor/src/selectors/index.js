/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const requests = require("devtools/client/netmonitor/src/selectors/requests");
const search = require("devtools/client/netmonitor/src/selectors/search");
const timingMarkers = require("devtools/client/netmonitor/src/selectors/timing-markers");
const ui = require("devtools/client/netmonitor/src/selectors/ui");
const webSockets = require("devtools/client/netmonitor/src/selectors/web-sockets");

Object.assign(exports, search, requests, timingMarkers, ui, webSockets);
