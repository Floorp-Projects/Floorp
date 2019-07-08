/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

loader.lazyRequireGetter(this, "Task", "devtools/shared/task", true);
loader.lazyRequireGetter(
  this,
  "executeSoon",
  "devtools/shared/DevToolsUtils",
  true
);
loader.lazyRequireGetter(
  this,
  "isGenerator",
  "devtools/shared/DevToolsUtils",
  true
);
loader.lazyRequireGetter(
  this,
  "isAsyncFunction",
  "devtools/shared/DevToolsUtils",
  true
);
loader.lazyRequireGetter(
  this,
  "reportException",
  "devtools/shared/DevToolsUtils",
  true
);

const ERROR_TYPE = (exports.ERROR_TYPE = "@@redux/middleware/task#error");

/**
 * A middleware that allows generator thunks (functions) and promise
 * to be dispatched. If it's a generator, it is called with `dispatch`
 * and `getState`, allowing the action to create multiple actions (most likely
 * asynchronously) and yield on each. If called with a promise, calls `dispatch`
 * on the results.
 */

function task({ dispatch, getState }) {
  return next => action => {
    if (isGenerator(action)) {
      return Task.spawn(action.bind(null, dispatch, getState)).catch(
        handleError.bind(null, dispatch)
      );
    }
    if (isAsyncFunction(action)) {
      return action(dispatch, getState).catch(handleError.bind(null, dispatch));
    }

    /*
    if (isPromise(action)) {
      return action.then(dispatch, handleError.bind(null, dispatch));
    }
    */

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
