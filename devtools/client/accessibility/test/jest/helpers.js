/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { reducers } = require("devtools/client/accessibility/reducers/index");

const {
  createStore,
  combineReducers,
} = require("devtools/client/shared/vendor/redux");

/**
 * Prepare the store for use in testing.
 */
function setupStore({
  preloadedState,
} = {}) {
  const store = createStore(combineReducers(reducers), preloadedState);
  return store;
}

/**
 * Build a mock accessible object.
 * @param {Object} form
 *        Data similar to what accessible actor passes to accessible front.
 */
function mockAccessible(form) {
  return {
    on: jest.fn(),
    off: jest.fn(),
    audit: jest.fn().mockReturnValue(Promise.resolve()),
    ...form,
  };
}

module.exports = {
  mockAccessible,
  setupStore,
};
