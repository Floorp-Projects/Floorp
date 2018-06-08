/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A middleware that allows thunks (functions) to be dispatched.
 */
function thunk(options = {}, { dispatch, getState }) {
  return next => action => {
    return (typeof action === "function")
      ? action(dispatch, getState, options)
      : next(action);
  };
}

module.exports = thunk;
