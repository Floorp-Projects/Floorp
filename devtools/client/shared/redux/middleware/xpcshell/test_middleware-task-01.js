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
} = require("resource://devtools/client/shared/redux/middleware/task.js");

/**
 * Tests that task middleware allows dispatching generators, promises and objects
 * that return actions;
 */
add_task(async function () {
  const store = applyMiddleware(task)(createStore)(reducer);

  store.dispatch(fetch1("generator"));
  await waitUntilState(store, () => store.getState().length === 1);
  equal(
    store.getState()[0].data,
    "generator",
    "task middleware async dispatches an action via generator"
  );

  store.dispatch(fetch2("sync"));
  await waitUntilState(store, () => store.getState().length === 2);
  equal(
    store.getState()[1].data,
    "sync",
    "task middleware sync dispatches an action via sync"
  );
});

function fetch1(data) {
  return async function ({ dispatch, getState }) {
    equal(
      getState().length,
      0,
      "`getState` is accessible in a generator action"
    );
    let moreData = await new Promise(resolve => resolve(data));
    // Ensure it handles more than one yield
    moreData = await new Promise(resolve => resolve(data));
    dispatch({ type: "fetch1", data: moreData });
  };
}

function fetch2(data) {
  return {
    type: "fetch2",
    data,
  };
}

function reducer(state = [], action) {
  info("Action called: " + action.type);
  if (["fetch1", "fetch2"].includes(action.type)) {
    state.push(action);
  }
  return [...state];
}
