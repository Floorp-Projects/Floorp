/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { applyMiddleware, combineReducers, createStore } = require("redux");

const { logger } = require("redux-logger");
const { promise } = require("./utils/redux/middleware/promise");
const { thunk } = require("./utils/redux/middleware/thunk");
const reducers = require("./reducers");

function configureStore(options, client) {
  return createStore(
    combineReducers(reducers),
    applyMiddleware(thunk(options.makeThunkArgs), promise, logger)
  );
}

module.exports = {
  configureStore,
};
