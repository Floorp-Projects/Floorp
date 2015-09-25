/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { createStore, applyMiddleware } = require("devtools/client/shared/vendor/redux");
const { thunk } = require("./middleware/thunk");
const { waitUntilService } = require("./middleware/wait-service");
const { log } = require("./middleware/log");

/**
 * This creates a dispatcher with all the standard middleware in place
 * that all code requires. It can also be optionally configured in
 * various ways, such as logging and recording.
 *
 * @param {object} opts - boolean configuration flags
 *        - log: log all dispatched actions to console
 *        - middleware: array of middleware to be included in the redux store
 */
module.exports = (opts={}) => {
  const middleware = [
    thunk,
    waitUntilService
  ];

  if (opts.log) {
    middleware.push(log);
  }

  if (opts.middleware) {
    opts.middleware.forEach(fn => middleware.push(fn));
  }

  return applyMiddleware(...middleware)(createStore);
}
