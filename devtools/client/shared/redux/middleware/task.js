/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

loader.lazyRequireGetter(
  this,
  ["executeSoon", "isAsyncFunction", "reportException"],
  "devtools/shared/DevToolsUtils",
  true
);

const ERROR_TYPE = (exports.ERROR_TYPE = "@@redux/middleware/task#error");

/**
 * A middleware that allows async thunks (async functions) to be dispatched.
 * The middleware is called "task" for historical reasons. TODO: rename?
 */

function task({ dispatch, getState }) {
  return next => action => {
    if (isAsyncFunction(action)) {
      return action({ dispatch, getState }).catch(
        handleError.bind(null, dispatch)
      );
    }
    return next(action);
  };
}

function handleError(dispatch, error) {
  executeSoon(() => {
    reportException(ERROR_TYPE, error);
    dispatch({ type: ERROR_TYPE, error });
  });
}

exports.task = task;
