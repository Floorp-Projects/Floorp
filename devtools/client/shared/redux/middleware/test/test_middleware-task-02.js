/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Tests that task middleware allows dispatching generators that dispatch
 * additional sync and async actions.
 */

const { createStore, applyMiddleware } = require("devtools/client/shared/vendor/redux");
const { task } = require("devtools/client/shared/redux/middleware/task");

function run_test() {
  run_next_test();
}

add_task(function* () {
  let store = applyMiddleware(task)(createStore)(reducer);

  store.dispatch(comboAction());
  yield waitUntilState(store, () => store.getState().length === 3);

  equal(store.getState()[0].type, "fetchAsync-start",
        "Async dispatched actions in a generator task are fired");
  equal(store.getState()[1].type, "fetchAsync-end",
        "Async dispatched actions in a generator task are fired");
  equal(store.getState()[2].type, "fetchSync",
        "Return values of yielded sync dispatched actions are correct");
  equal(store.getState()[3].type, "fetch-done",
        "Return values of yielded async dispatched actions are correct");
  equal(store.getState()[3].data.sync.data, "sync",
        "Return values of dispatched sync values are correct");
  equal(store.getState()[3].data.async, "async",
        "Return values of dispatched async values are correct");
});

function comboAction() {
  return function* (dispatch, getState) {
    let data = {};
    data.async = yield dispatch(fetchAsync("async"));
    data.sync = yield dispatch(fetchSync("sync"));
    dispatch({ type: "fetch-done", data });
  };
}

function fetchSync(data) {
  return { type: "fetchSync", data };
}

function fetchAsync(data) {
  return function* (dispatch) {
    dispatch({ type: "fetchAsync-start" });
    let val = yield new Promise(resolve => resolve(data));
    dispatch({ type: "fetchAsync-end" });
    return val;
  };
}

function reducer(state = [], action) {
  do_print("Action called: " + action.type);
  if (/fetch/.test(action.type)) {
    state.push(action);
  }
  return [...state];
}
