/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { combineReducers } = require("../shared/vendor/redux");
const createStore = require("../shared/redux/create-store");
const reducers = require("./reducers");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

module.exports = function () {
  let shouldLog = false;
  let history;

  // If testing, store the action history in an array
  // we'll later attach to the store
  if (DevToolsUtils.testing) {
    history = [];
  }

  let store = createStore({ log: shouldLog, history })(combineReducers(reducers), {});

  if (history) {
    store.history = history;
  }

  return store;
};
