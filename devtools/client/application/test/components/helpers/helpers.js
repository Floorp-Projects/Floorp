/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  reducers,
} = require("devtools/client/application/test/components/helpers/helper-reducer");

const {
  createStore,
  combineReducers,
} = require("devtools/client/shared/vendor/redux");

/**
 * Prepare the store for use in testing.
 */
function setupStore({ preloadedState } = {}) {
  const store = createStore(combineReducers(reducers), preloadedState);
  return store;
}

module.exports = {
  setupStore,
};
