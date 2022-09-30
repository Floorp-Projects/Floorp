/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  thunk,
} = require("resource://devtools/client/shared/redux/middleware/thunk.js");
const configureStore = require("redux-mock-store").default;

/**
 * Prepare the store for use in testing.
 */
function setupStore(preloadedState = {}) {
  const middleware = [thunk()];
  const mockStore = configureStore(middleware);
  return mockStore(preloadedState);
}

/**
 * This gives an opportunity to Promises to resolve in tests
 * (since they are microtasks)
 */
async function flushPromises() {
  await new Promise(r => setTimeout(r, 0));
}

module.exports = {
  flushPromises,
  setupStore,
};
