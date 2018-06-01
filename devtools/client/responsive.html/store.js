/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { combineReducers } = require("devtools/client/shared/vendor/redux");
const createStore = require("devtools/client/shared/redux/create-store");
const reducers = require("./reducers");
const flags = require("devtools/shared/flags");

module.exports = function() {
  let shouldLog = false;
  let history;

  // If testing, store the action history in an array
  // we'll later attach to the store
  if (flags.testing) {
    history = [];
    shouldLog = true;
  }

  const store = createStore({
    log: shouldLog,
    history
  })(combineReducers(reducers), {});

  if (history) {
    store.history = history;
  }

  return store;
};
