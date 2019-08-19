/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const reducers = require("devtools/client/application/src/reducers/index");
const { createStore } = require("devtools/client/shared/vendor/redux");

/**
 * Prepare the store for use in testing.
 */
function setupStore({ preloadedState } = {}) {
  const store = createStore(reducers, preloadedState);
  return store;
}

/**
 * This gives an opportunity to Promises to resolve in tests, even if they are
 * resolve immediately (since they are microtasks)
 */
async function flushPromises() {
  return new Promise(resolve => resolve());
}

module.exports = {
  flushPromises,
  setupStore,
};
