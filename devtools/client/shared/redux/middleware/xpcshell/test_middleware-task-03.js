/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createStore,
  applyMiddleware,
} = require("resource://devtools/client/shared/vendor/redux.js");
const {
  task,
  ERROR_TYPE,
} = require("resource://devtools/client/shared/redux/middleware/task.js");

/**
 * Tests that the middleware handles errors thrown in tasks, and rejected promises.
 */

add_task(async function () {
  const store = applyMiddleware(task)(createStore)(reducer);

  store.dispatch(asyncError());
  await waitUntilState(store, () => store.getState().length === 1);
  equal(
    store.getState()[0].type,
    ERROR_TYPE,
    "generator errors dispatch ERROR_TYPE actions"
  );
  equal(
    store.getState()[0].error,
    "task-middleware-error-generator",
    "generator errors dispatch ERROR_TYPE actions with error"
  );
});

function asyncError() {
  return async () => {
    const error = "task-middleware-error-generator";
    throw error;
  };
}

function reducer(state = [], action) {
  info("Action called: " + action.type);
  if (action.type === ERROR_TYPE) {
    state.push(action);
  }
  return [...state];
}
