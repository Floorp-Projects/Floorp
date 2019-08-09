/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  combineReducers,
  createStore,
  applyMiddleware,
} = require("devtools/client/shared/vendor/redux");
const { thunk } = require("./middleware/thunk");
const { waitUntilService } = require("./middleware/wait-service");
const { task } = require("./middleware/task");
const { promise } = require("./middleware/promise");
const flags = require("devtools/shared/flags");

loader.lazyRequireGetter(
  this,
  "history",
  "devtools/client/shared/redux/middleware/history",
  true
);
loader.lazyRequireGetter(
  this,
  "log",
  "devtools/client/shared/redux/middleware/log",
  true
);

/**
 * This creates a dispatcher with all the standard middleware in place
 * that all code requires. It can also be optionally configured in
 * various ways, such as logging and recording.
 *
 * @param {object} opts:
 *        - log: log all dispatched actions to console
 *        - history: an array to store every action in. Should only be
 *                   used in tests.
 *        - middleware: array of middleware to be included in the redux store
 */
const createStoreWithMiddleware = (opts = {}) => {
  const middleware = [
    task,
    thunk,
    promise,

    // Order is important: services must go last as they always
    // operate on "already transformed" actions. Actions going through
    // them shouldn't have any special fields like promises, they
    // should just be normal JSON objects.
    waitUntilService,
  ];

  if (opts.history) {
    middleware.push(history(opts.history));
  }

  if (opts.middleware) {
    opts.middleware.forEach(fn => middleware.push(fn));
  }

  if (opts.log) {
    middleware.push(log);
  }

  return applyMiddleware(...middleware)(createStore);
};

module.exports = (
  reducers,
  { shouldLog = false, initialState = undefined } = {}
) => {
  const reducer =
    typeof reducers === "function" ? reducers : combineReducers(reducers);

  let historyEntries;

  // If testing, store the action history in an array
  // we'll later attach to the store
  if (flags.testing) {
    historyEntries = [];
  }

  const store = createStoreWithMiddleware({
    log: flags.testing && shouldLog,
    history: historyEntries,
  })(reducer, initialState);

  if (history) {
    store.history = historyEntries;
  }

  return store;
};
