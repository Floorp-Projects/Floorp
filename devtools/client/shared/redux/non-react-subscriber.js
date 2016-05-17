/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This file defines functions to add the ability for redux reducers
 * to broadcast specific state changes to a non-React UI. You should
 * *never* use this for new code that uses React, as it violates the
 * core principals of a functional UI. This should only be used when
 * migrating old code to redux, because it allows you to use redux
 * with event-listening UI elements. The typical way to set all of
 * this up is this:
 *
 *  const emitter = makeEmitter();
 *  let store = createStore(combineEmittingReducers(
 *    reducers,
 *    emitter.emit
 *  ));
 *  store = enhanceStoreWithEmitter(store, emitter);
 *
 * Now reducers will receive a 3rd argument, `emit`, for emitting
 * events, and the store has an `on` function for listening to them.
 * For example, a reducer can now do this:
 *
 * function update(state = initialState, action, emitChange) {
 *   if (action.type === constants.ADD_BREAKPOINT) {
 *     const id = action.breakpoint.id;
 *     emitChange('add-breakpoint', action.breakpoint);
 *     return state.merge({ [id]: action.breakpoint });
 *   }
 *   return state;
 * }
 *
 * `emitChange` is *not* synchronous, the state changes will be
 * broadcasted *after* all reducers are run and the state has been
 * updated.
 *
 * Now, a non-React widget can do this:
 *
 * store.on('add-breakpoint', breakpoint => { ... });
 */

const { combineReducers } = require("devtools/client/shared/vendor/redux");

/**
 * Make an emitter that is meant to be used in redux reducers. This
 * does not run listeners immediately when an event is emitted; it
 * waits until all reducers have run and the store has updated the
 * state, and then fires any enqueued events. Events *are* fired
 * synchronously, but just later in the process.
 *
 * This is important because you never want the UI to be updating in
 * the middle of a reducing process. Reducers will fire these events
 * in the middle of generating new state, but the new state is *not*
 * available from the store yet. So if the UI executes in the middle
 * of the reducing process and calls `getState()` to get something
 * from the state, it will get stale state.
 *
 * We want the reducing and the UI updating phases to execute
 * atomically and independent from each other.
 *
 * @param {Function} stillAliveFunc
 *        A function that indicates the app is still active. If this
 *        returns false, changes will stop being broadcasted.
 */
function makeStateBroadcaster(stillAliveFunc) {
  const listeners = {};
  let enqueuedChanges = [];

  return {
    onChange: (name, cb) => {
      if (!listeners[name]) {
        listeners[name] = [];
      }
      listeners[name].push(cb);
    },

    offChange: (name, cb) => {
      listeners[name] = listeners[name].filter(listener => listener !== cb);
    },

    emitChange: (name, payload) => {
      enqueuedChanges.push([name, payload]);
    },

    subscribeToStore: store => {
      store.subscribe(() => {
        if (stillAliveFunc()) {
          enqueuedChanges.forEach(([name, payload]) => {
            if (listeners[name]) {
              listeners[name].forEach(listener => {
                listener(payload);
              });
            }
          });
          enqueuedChanges = [];
        }
      });
    }
  };
}

/**
 * Make a store fire any enqueued events whenever the state changes,
 * and add an `on` function to allow users to listen for specific
 * events.
 *
 * @param {Object} store
 * @param {Object} broadcaster
 * @return {Object}
 */
function enhanceStoreWithBroadcaster(store, broadcaster) {
  broadcaster.subscribeToStore(store);
  store.onChange = broadcaster.onChange;
  store.offChange = broadcaster.offChange;
  return store;
}

/**
 * Function that takes a hash of reducers, like `combineReducers`, and
 * an `emitChange` function and returns a function to be used as a
 * reducer for a Redux store. This allows all reducers defined here to
 * receive a third argument, the `emitChange` function, for
 * event-based subscriptions from within reducers.
 *
 * @param {Object} reducers
 * @param {Function} emitChange
 * @return {Function}
 */
function combineBroadcastingReducers(reducers, emitChange) {
  // Wrap each reducer with a wrapper function that calls
  // the reducer with a third argument, an `emitChange` function.
  // Use this rather than a new custom top level reducer that would ultimately
  // have to replicate redux's `combineReducers` so we only pass in correct
  // state, the error checking, and other edge cases.
  function wrapReduce(newReducers, key) {
    newReducers[key] = (state, action) => {
      return reducers[key](state, action, emitChange);
    };
    return newReducers;
  }

  return combineReducers(
    Object.keys(reducers).reduce(wrapReduce, Object.create(null))
  );
}

module.exports = {
  makeStateBroadcaster,
  enhanceStoreWithBroadcaster,
  combineBroadcastingReducers
};
