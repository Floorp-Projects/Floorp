/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const createStore = require("resource://devtools/client/shared/redux/create-store.js");
const {
  combineReducers,
} = require("resource://devtools/client/shared/vendor/redux.js");
// Reducers which need to be available immediately when the Inspector loads.
const reducers = {
  // Provide a dummy default reducer.
  // Redux throws an error when calling combineReducers() with an empty object.
  default: (state = {}) => state,
};

function createReducer(laterReducers = {}) {
  return combineReducers({
    ...reducers,
    ...laterReducers,
  });
}

module.exports = inspector => {
  const store = createStore(createReducer(), {
    // Enable log middleware in tests
    shouldLog: true,
    // Pass the client inspector instance so thunks (dispatched functions)
    // can access it from their arguments
    thunkOptions: { inspector },
  });

  // Map of registered reducers loaded on-demand.
  store.laterReducers = {};

  /**
   * Augment the current Redux store with a slice reducer.
   * Call this method to add reducers on-demand after the initial store creation.
   *
   * @param {String} key
   *        Slice name.
   * @param {Function} reducer
   *        Slice reducer function.
   */
  store.injectReducer = (key, reducer) => {
    if (store.laterReducers[key]) {
      console.log(`Already loaded reducer: ${key}`);
      return;
    }
    store.laterReducers[key] = reducer;
    store.replaceReducer(createReducer(store.laterReducers));
  };

  return store;
};
