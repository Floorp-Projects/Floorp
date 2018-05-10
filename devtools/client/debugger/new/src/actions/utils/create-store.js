"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _redux = require("devtools/client/shared/vendor/redux");

var _waitService = require("./middleware/wait-service");

var _log = require("./middleware/log");

var _history = require("./middleware/history");

var _promise = require("./middleware/promise");

var _thunk = require("./middleware/thunk");

var _timing = require("./middleware/timing");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global window */

/**
 * Redux store utils
 * @module utils/create-store
 */

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
 * @memberof utils/create-store
 * @static
 */
const configureStore = (opts = {}) => {
  const middleware = [(0, _thunk.thunk)(opts.makeThunkArgs), _promise.promise, // Order is important: services must go last as they always
  // operate on "already transformed" actions. Actions going through
  // them shouldn't have any special fields like promises, they
  // should just be normal JSON objects.
  _waitService.waitUntilService];

  if (opts.history) {
    middleware.push((0, _history.history)(opts.history));
  }

  if (opts.middleware) {
    opts.middleware.forEach(fn => middleware.push(fn));
  }

  if (opts.log) {
    middleware.push(_log.log);
  }

  if (opts.timing) {
    middleware.push(_timing.timing);
  } // Hook in the redux devtools browser extension if it exists


  const devtoolsExt = typeof window === "object" && window.devToolsExtension ? window.devToolsExtension() : f => f;
  return (0, _redux.applyMiddleware)(...middleware)(devtoolsExt(_redux.createStore));
};

exports.default = configureStore;