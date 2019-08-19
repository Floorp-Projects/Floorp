/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ./shared-head.js */
// Currently this file expects "defer" to be imported into scope.

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
  const deferred = defer();
  const unsubscribe = store.subscribe(check);

  info(`Waiting for state predicate "${predicate}"`);
  function check() {
    if (predicate(store.getState())) {
      info(`Found state predicate "${predicate}"`);
      unsubscribe();
      deferred.resolve();
    }
  }

  // Fire the check immediately in case the action has already occurred
  check();

  return deferred.promise;
}

/**
 * Wait until a particular action has been emitted by the store.
 * @param {Store} store
 *        The Redux store being used.
 * @param {String} actionType
 *        The expected action to wait for.
 * @param {Number} count
 *         Number of times to expect the action to occur. Default is once.
 * @return Promise
 *         Resolved once the expected action is emitted by the store.
 */
function waitUntilAction(store, actionType, count = 1) {
  const deferred = defer();
  const unsubscribe = store.subscribe(check);
  const history = store.history;
  let index = history.length;

  info(`Waiting for action "${actionType}"`);
  function check() {
    const action = history[index++];
    if (action && action.type === actionType) {
      info(`Found action "${actionType}"`);
      count--;
      if (count === 0) {
        unsubscribe();
        deferred.resolve(store.getState());
      }
    }
  }

  return deferred.promise;
}
