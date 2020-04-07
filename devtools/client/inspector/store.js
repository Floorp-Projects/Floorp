/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const createStore = require("devtools/client/shared/redux/create-store");
const { combineReducers } = require("devtools/client/shared/vendor/redux");
// Reducers which need to be available immediately when the Inspector loads.
const reducers = {};

function createReducer(laterReducers = {}) {
  return combineReducers({
    ...reducers,
    ...laterReducers,
  });
}

module.exports = () => {
  const store = createStore(createReducer(), {
    // Enable log middleware in tests
    shouldLog: true,
    thunkOptions: {},
  });

  // Dictionary to keep track of the registered reducers loaded on-demand later
  store.laterReducers = {};

  // Create an inject reducer function attached to the store instance.
  // This function adds the on-demand reducer, and creates a new combined reducer.
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
