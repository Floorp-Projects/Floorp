/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const fluxify = require('./fluxify/dispatcher');
const thunkMiddleware = require('./fluxify/thunkMiddleware');
const logMiddleware = require('./fluxify/logMiddleware');
const waitUntilService = require('./fluxify/waitUntilService')
const { compose } = require('devtools/toolkit/DevToolsUtils');

/**
 * This creates a dispatcher with all the standard middleware in place
 * that all code requires. It can also be optionally configured in
 * various ways, such as logging and recording.
 *
 * @param {object} opts - boolean configuration flags
 *        - log: log all dispatched actions to console
 */
module.exports = (opts={}) => {
  const middleware = [
    thunkMiddleware,
    waitUntilService.service
  ];

  if (opts.log) {
    middleware.push(logMiddleware);
  }

  return fluxify.applyMiddleware(...middleware)(fluxify.createDispatcher);
}
