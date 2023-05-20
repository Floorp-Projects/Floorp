/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * A middleware that allows thunks (functions) to be dispatched
 * If it's a thunk, it is called with an argument that will be an object
 * containing `dispatch` and `getState` properties, plus any additional properties
 * defined in the `options` parameters.
 * This allows the action to create multiple actions (most likely asynchronously).
 */
function thunk(options = {}) {
  return function ({ dispatch, getState }) {
    return next => action => {
      return typeof action === "function"
        ? action({ dispatch, getState, ...options })
        : next(action);
    };
  };
}

exports.thunk = thunk;
