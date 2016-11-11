/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createStore, applyMiddleware } = require("devtools/client/shared/vendor/redux");
const { task, ERROR_TYPE } = require("devtools/client/shared/redux/middleware/task");

/**
 * Tests that the middleware handles errors thrown in tasks, and rejected promises.
 */

function run_test() {
  run_next_test();
}

add_task(function* () {
  let store = applyMiddleware(task)(createStore)(reducer);

  store.dispatch(generatorError());
  yield waitUntilState(store, () => store.getState().length === 1);
  equal(store.getState()[0].type, ERROR_TYPE,
        "generator errors dispatch ERROR_TYPE actions");
  equal(store.getState()[0].error, "task-middleware-error-generator",
        "generator errors dispatch ERROR_TYPE actions with error");
});

function generatorError() {
  return function* (dispatch, getState) {
    let error = "task-middleware-error-generator";
    throw error;
  };
}

function reducer(state = [], action) {
  do_print("Action called: " + action.type);
  if (action.type === ERROR_TYPE) {
    state.push(action);
  }
  return [...state];
}
