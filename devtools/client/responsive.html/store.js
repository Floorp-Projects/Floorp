/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { combineReducers } = require("devtools/client/shared/vendor/redux");
const createStore = require("devtools/client/shared/redux/create-store");
const reducers = require("./reducers");
const flags = require("devtools/shared/flags");
const telemetryMiddleware = require("./middleware/telemetry");

module.exports = function(options = {}) {
  let shouldLog = false;
  let history;
  const { telemetry } = options;

  // If testing, store the action history in an array
  // we'll later attach to the store
  if (flags.testing) {
    history = [];
    shouldLog = true;
  }

  const store = createStore({
    log: shouldLog,
    history,
    middleware: [telemetryMiddleware(telemetry)],
  })(combineReducers(reducers), {});

  if (history) {
    store.history = history;
  }

  return store;
};
