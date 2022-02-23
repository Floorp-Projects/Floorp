/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ./shared-head.js */

// Common utility functions for working with Redux stores.  The file is meant
// to be safe to load in both mochitest and xpcshell environments.

/**
 * Wait until the store has reached a state that matches the predicate.
 * @param Store store
 *        The Redux store being used.
 * @param function predicate
 *        A function that returns true when the store has reached the expected
 *        state.
 * @return Promise
 *         Resolved once the store reaches the expected state.
 */
function waitUntilState(store, predicate) {
  return new Promise(resolve => {
    const unsubscribe = store.subscribe(check);

    info(`Waiting for state predicate "${predicate}"`);
    function check() {
      if (predicate(store.getState())) {
        info(`Found state predicate "${predicate}"`);
        unsubscribe();
        resolve();
      }
    }

    // Fire the check immediately in case the action has already occurred
    check();
  });
}

/**
 * Wait for a specific action type to be dispatched.
 *
 * If the action is async and defines a `status` property, this helper will wait
 * for the status to reach either "error" or "done".
 *
 * @param {Object} store
 *        Redux store where the action should be dispatched.
 * @param {String} actionType
 *        The actionType to wait for.
 * @param {Number} repeat
 *        Optional, number of time the action is expected to be dispatched.
 *        Defaults to 1
 * @return {Promise}
 */
function waitForDispatch(store, actionType, repeat = 1) {
  let count = 0;
  return new Promise(resolve => {
    store.dispatch({
      type: "@@service/waitUntil",
      predicate: action => {
        const isDone =
          !action.status ||
          action.status === "done" ||
          action.status === "error";

        if (action.type === actionType && isDone && ++count == repeat) {
          return true;
        }

        return false;
      },
      run: (dispatch, getState, action) => {
        resolve(action);
      },
    });
  });
}
