/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * A middleware that allows thunks (functions) to be dispatched.
 * If it's a thunk, it is called with `dispatch` and `getState`,
 * allowing the action to create multiple actions (most likely
 * asynchronously).
 */
function thunk({ dispatch, getState }) {
  return next => action => {
    return (typeof action === "function")
      ? action(dispatch, getState)
      : next(action);
  };
}
exports.thunk = thunk;
