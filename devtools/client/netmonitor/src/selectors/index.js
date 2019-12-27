/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const requests = require("./requests");
const search = require("./search");
const timingMarkers = require("./timing-markers");
const ui = require("./ui");
const webSockets = require("./web-sockets");

Object.assign(exports, search, requests, timingMarkers, ui, webSockets);
