/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { entries, compose } = require("devtools/toolkit/DevToolsUtils");

/**
 * A store creator that creates a dispatch function that runs the
 * provided middlewares before actually dispatching. This allows
 * simple middleware to augment the kinds of actions that can
 * be dispatched. This would be used like this:
 * `createDispatcher = applyMiddleware([fooMiddleware, ...])(createDispatcher)`
 *
 * Middlewares are simple functions that are provided `dispatch` and
 * `getState` functions. They create functions that accept actions and
 * can re-dispatch them in any way they want. A common scenario is
 * asynchronously dispatching multiple actions. Here is a full
 * middleware:
 *
 * function thunkMiddleware({ dispatch, getState }) {
 *  return next => action => {
 *    return typeof action === 'function' ?
 *      action(dispatch, getState) :
 *      next(action);
 *  }
 * }
 *
 * `next` is essentially a `dispatch` function, but it calls the next
 * middelware in the chain (or the real `dispatch` function). Using
 * this middleware, you can return a function that gives you a
 * dispatch function:
 *
 * function actionCreator(timeout) {
 *   return (dispatch, getState) => {
 *     dispatch({ type: TIMEOUT, status: "start" });
 *     setTimeout(() => dispatch({ type: TIMEOUT, status: "end" }),
 *                timeout);
 *   }
 * }
 *
 */
function applyMiddleware(...middlewares) {
  return next => stores => {
    const dispatcher = next(stores);
    let dispatch = dispatcher.dispatch;

    const api = {
      getState: dispatcher.getState,
      dispatch: action => dispatch(action)
    };
    const chain = middlewares.map(middleware => middleware(api));
    dispatch = compose(...chain)(dispatcher.dispatch);

    return Object.assign({}, dispatcher, { dispatch: dispatch });
  }
}

/*
 * The heart of the system. This creates a dispatcher which is the
 * interface between views and stores. Views can use a dispatcher
 * instance to dispatch actions, which know nothing about the stores.
 * Actions are broadcasted to all registered stores, and stores can
 * handle the action and update their state. The dispatcher gives
 * stores an `emitChange` function, which signifies that a piece of
 * state has changed. The dispatcher will notify all views that are
 * listening to that piece of state, registered with `onChange`.
 *
 * Views generally are stateless, pure components (eventually React
 * components). They simply take state and render it.
 *
 * Stores make up the entire app state, and are all grouped together
 * into a single app state atom, returned by the dispatcher's
 * `getState` function. The shape of the app state is determined by
 * the `stores` object passed in to the dispatcher, so if
 * `{ foo: fooStore }` was passed to `createDispatcher` the app state
 * would be `{ foo: fooState }`
 *
 * Actions are just JavaScript object with a `type` property and any
 * other fields pertinent to the action. Action creators should
 * generally be used to create actions, which are just functions that
 * return the action object. Additionally, the `bindActionCreators`
 * module provides a function for automatically binding action
 * creators to a dispatch function, so calling them automatically
 * dispatches. For example:
 *
 * ```js
 * // Manually dispatch
 * dispatcher.dispatch({ type: constants.ADD_ITEM, item: item });
 * // Using an action creator
 * dispatcher.dispatch(addItem(item));
 *
 * // Using an action creator bound to dispatch
 * actions = bindActionCreators({ addItem: addItem });
 * actions.addItem(item);
 * ```
 *
 * Our system expects stores to exist as an `update` function. You
 * should define an update function in a module, and optionally
 * any action creators that are useful to go along with it. Here is
 * an example store file:
 *
 * ```js
 * const initialState = { items: [] };
 * function update(state = initialState, action, emitChange) {
 *   if (action.type === constants.ADD_ITEM) {
 *     state.items.push(action.item);
 *     emitChange("items", state.items);
 *   }
 *
 *   return state;
 * }
 *
 * function addItem(item) {
 *   return {
 *     type: constants.ADD_ITEM,
 *     item: item
 *   };
 * }
 *
 * module.exports = {
 *   update: update,
 *   actions: { addItem }
 * }
 * ```
 *
 * Lastly, "constants" are simple strings that specify action names.
 * Usually the entire set of available action types are specified in
 * a `constants.js` file, so they are available globally. Use
 * variables that are the same name as the string, for example
 * `const ADD_ITEM = "ADD_ITEM"`.
 *
 * This entire system was inspired by Redux, which hopefully we will
 * eventually use once things get cleaned up enough. You should read
 * its docs, and keep in mind that it calls stores "reducers" and the
 * dispatcher instance is called a single store.
 * http://rackt.github.io/redux/
 */
function createDispatcher(stores) {
  const state = {};
  const listeners = {};
  let enqueuedChanges = [];
  let isDispatching = false;

  // Validate the stores to make sure they have the right shape,
  // and accumulate the initial state
  entries(stores).forEach(([name, store]) => {
    if (!store || typeof store.update !== "function") {
      throw new Error("Error creating dispatcher: store \"" + name +
                      "\" does not have an `update` function");
    }

    state[name] = store.update(undefined, {});
  });

  function getState() {
    return state;
  }

  function emitChange(storeName, dataName, payload) {
    enqueuedChanges.push([storeName, dataName, payload]);
  }

  function onChange(paths, view) {
    entries(paths).forEach(([storeName, data]) => {
      if (!stores[storeName]) {
        throw new Error("Error adding onChange handler to store: store " +
                        "\"" + storeName + "\" does not exist");
      }

      if (!listeners[storeName]) {
        listeners[storeName] = [];
      }

      if (typeof data == 'function') {
        listeners[storeName].push(data.bind(view));
      }
      else {
        entries(data).forEach(([watchedName, handler]) => {
          listeners[storeName].push((payload, dataName, storeName) => {
            if (dataName === watchedName) {
              handler.call(view, payload, dataName, storeName);
            }
          });
        });
      }
    });
  }

  /**
   * Flush any enqueued state changes from the dispatch cycle. Listeners
   * are not immediately notified of changes, only after dispatching
   * is completed, to ensure that all state is consistent (in the case
   * of multiple stores changes at once).
   */
  function flushChanges() {
    enqueuedChanges.forEach(([storeName, dataName, payload]) => {
      if (listeners[storeName]) {
        listeners[storeName].forEach(listener => {
          listener(payload, dataName, storeName);
        });
      }
    });

    enqueuedChanges = [];
  }

  function dispatch(action) {
    if (isDispatching) {
      throw new Error('Cannot dispatch in the middle of a dispatch');
    }
    if (!action.type) {
      throw new Error(
        'action type is null, ' +
        'did you make a typo when publishing this action? ' +
        JSON.stringify(action, null, 2)
      );
    }

    isDispatching = true;
    try {
      entries(stores).forEach(([name, store]) => {
        state[name] = store.update(
          state[name],
          action,
          emitChange.bind(null, name)
        );
      });
    }
    finally {
      isDispatching = false;
    }

    flushChanges();
  }

  return {
    getState,
    dispatch,
    onChange
  };
}

module.exports = {
  createDispatcher: createDispatcher,
  applyMiddleware: applyMiddleware
};
