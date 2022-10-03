/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  combineReducers,
  createStore,
  applyMiddleware,
} = require("resource://devtools/client/shared/vendor/redux.js");
const {
  thunk,
} = require("resource://devtools/client/shared/redux/middleware/thunk.js");
const {
  waitUntilService,
} = require("resource://devtools/client/shared/redux/middleware/wait-service.js");
const {
  task,
} = require("resource://devtools/client/shared/redux/middleware/task.js");
const {
  promise,
} = require("resource://devtools/client/shared/redux/middleware/promise.js");
const flags = require("resource://devtools/shared/flags.js");

loader.lazyRequireGetter(
  this,
  "log",
  "resource://devtools/client/shared/redux/middleware/log.js",
  true
);

/**
 * This creates a dispatcher with all the standard middleware in place
 * that all code requires. It can also be optionally configured in
 * various ways, such as logging and recording.
 *
 * @param {object} opts:
 *        - enableTaskMiddleware: if true, include the task middleware
 *        - log: log all dispatched actions to console
 *        - middleware: array of middleware to be included in the redux store
 *        - thunkOptions: object that will be spread within a {dispatch, getState} object,
 *                        that will be passed in each thunk action.
 */
const createStoreWithMiddleware = (opts = {}) => {
  const middleware = [];
  if (opts.enableTaskMiddleware) {
    middleware.push(task);
  }
  middleware.push(
    thunk(opts.thunkOptions),
    promise,

    // Order is important: services must go last as they always
    // operate on "already transformed" actions. Actions going through
    // them shouldn't have any special fields like promises, they
    // should just be normal JSON objects.
    waitUntilService
  );

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
  {
    shouldLog = false,
    initialState = undefined,
    thunkOptions,
    enableTaskMiddleware = false,
  } = {}
) => {
  const reducer =
    typeof reducers === "function" ? reducers : combineReducers(reducers);

  const store = createStoreWithMiddleware({
    enableTaskMiddleware,
    log: flags.testing && shouldLog,
    thunkOptions,
  })(reducer, initialState);

  return store;
};
