/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { combineReducers } = require("devtools/client/shared/vendor/redux");

/**
 * Function that takes a hash of reducers, like `combineReducers`,
 * and an `emit` function and returns a function to be used as a reducer
 * for a Redux store. This allows all reducers defined here to receive
 * a third argument, the `emit` function, for event-based subscriptions
 * from within reducers.
 *
 * @param {Object} reducers
 * @param {Function} emit
 * @return {Function}
 */
function combineEmittingReducers (reducers, emit) {
  // Wrap each reducer with a wrapper function that calls
  // the reducer with a third argument, an `emit` function.
  // Use this rather than a new custom top level reducer that would ultimately
  // have to replicate redux's `combineReducers` so we only pass in correct state,
  // the error checking, and other edge cases.
  function wrapReduce (newReducers, key) {
    newReducers[key] = (state, action) => reducers[key](state, action, emit);
    return newReducers;
  }

  return combineReducers(Object.keys(reducers).reduce(wrapReduce, Object.create(null)));
}

exports.combineEmittingReducers = combineEmittingReducers;
