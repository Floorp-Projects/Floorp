/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { createStore, applyMiddleware } = require("devtools/client/shared/vendor/redux");
const { thunk } = require("./middleware/thunk");
const { waitUntilService } = require("./middleware/wait-service");
const { task } = require("./middleware/task");
const { log } = require("./middleware/log");
const { promise } = require("./middleware/promise");
const { history } = require("./middleware/history");

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
module.exports = (opts={}) => {
  const middleware = [
    task,
    thunk,
    waitUntilService,
    promise,
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
}
