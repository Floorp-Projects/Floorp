/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

import {
  actionCreators as ac,
  actionTypes as at,
  actionUtils as au,
} from "common/Actions.jsm";
import { applyMiddleware, combineReducers, createStore } from "redux";

export const MERGE_STORE_ACTION = "NEW_TAB_INITIAL_STATE";
export const OUTGOING_MESSAGE_NAME = "ActivityStream:ContentToMain";
export const INCOMING_MESSAGE_NAME = "ActivityStream:MainToContent";
export const EARLY_QUEUED_ACTIONS = [at.SAVE_SESSION_PERF_DATA];

/**
 * A higher-order function which returns a reducer that, on MERGE_STORE action,
 * will return the action.data object merged into the previous state.
 *
 * For all other actions, it merely calls mainReducer.
 *
 * Because we want this to merge the entire state object, it's written as a
 * higher order function which takes the main reducer (itself often a call to
 * combineReducers) as a parameter.
 *
 * @param  {function} mainReducer reducer to call if action != MERGE_STORE_ACTION
 * @return {function}             a reducer that, on MERGE_STORE_ACTION action,
 *                                will return the action.data object merged
 *                                into the previous state, and the result
 *                                of calling mainReducer otherwise.
 */
function mergeStateReducer(mainReducer) {
  return (prevState, action) => {
    if (action.type === MERGE_STORE_ACTION) {
      return { ...prevState, ...action.data };
    }

    return mainReducer(prevState, action);
  };
}

/**
 * messageMiddleware - Middleware that looks for SentToMain type actions, and sends them if necessary
 */
const messageMiddleware = store => next => action => {
  const skipLocal = action.meta && action.meta.skipLocal;
  if (au.isSendToMain(action)) {
    RPMSendAsyncMessage(OUTGOING_MESSAGE_NAME, action);
  }
  if (!skipLocal) {
    next(action);
  }
};

export const rehydrationMiddleware = ({ getState }) => {
  // NB: The parameter here is MiddlewareAPI which looks like a Store and shares
  // the same getState, so attached properties are accessible from the store.
  getState.didRehydrate = false;
  getState.didRequestInitialState = false;
  return next => action => {
    if (getState.didRehydrate || window.__FROM_STARTUP_CACHE__) {
      return next(action);
    }

    const isMergeStoreAction = action.type === MERGE_STORE_ACTION;
    const isRehydrationRequest = action.type === at.NEW_TAB_STATE_REQUEST;

    if (isRehydrationRequest) {
      getState.didRequestInitialState = true;
      return next(action);
    }

    if (isMergeStoreAction) {
      getState.didRehydrate = true;
      return next(action);
    }

    // If init happened after our request was made, we need to re-request
    if (getState.didRequestInitialState && action.type === at.INIT) {
      return next(ac.AlsoToMain({ type: at.NEW_TAB_STATE_REQUEST }));
    }

    if (
      au.isBroadcastToContent(action) ||
      au.isSendToOneContent(action) ||
      au.isSendToPreloaded(action)
    ) {
      // Note that actions received before didRehydrate will not be dispatched
      // because this could negatively affect preloading and the the state
      // will be replaced by rehydration anyway.
      return null;
    }

    return next(action);
  };
};

/**
 * This middleware queues up all the EARLY_QUEUED_ACTIONS until it receives
 * the first action from main. This is useful for those actions for main which
 * require higher reliability, i.e. the action will not be lost in the case
 * that it gets sent before the main is ready to receive it. Conversely, any
 * actions allowed early are accepted to be ignorable or re-sendable.
 */
export const queueEarlyMessageMiddleware = ({ getState }) => {
  // NB: The parameter here is MiddlewareAPI which looks like a Store and shares
  // the same getState, so attached properties are accessible from the store.
  getState.earlyActionQueue = [];
  getState.receivedFromMain = false;
  return next => action => {
    if (getState.receivedFromMain) {
      next(action);
    } else if (au.isFromMain(action)) {
      next(action);
      getState.receivedFromMain = true;
      // Sending out all the early actions as main is ready now
      getState.earlyActionQueue.forEach(next);
      getState.earlyActionQueue.length = 0;
    } else if (EARLY_QUEUED_ACTIONS.includes(action.type)) {
      getState.earlyActionQueue.push(action);
    } else {
      // Let any other type of action go through
      next(action);
    }
  };
};

/**
 * initStore - Create a store and listen for incoming actions
 *
 * @param  {object} reducers An object containing Redux reducers
 * @param  {object} intialState (optional) The initial state of the store, if desired
 * @return {object}          A redux store
 */
export function initStore(reducers, initialState) {
  const store = createStore(
    mergeStateReducer(combineReducers(reducers)),
    initialState,
    global.RPMAddMessageListener &&
      applyMiddleware(
        rehydrationMiddleware,
        queueEarlyMessageMiddleware,
        messageMiddleware
      )
  );

  if (global.RPMAddMessageListener) {
    global.RPMAddMessageListener(INCOMING_MESSAGE_NAME, msg => {
      try {
        store.dispatch(msg.data);
      } catch (ex) {
        console.error("Content msg:", msg, "Dispatch error: ", ex); // eslint-disable-line no-console
        dump(
          `Content msg: ${JSON.stringify(msg)}\nDispatch error: ${ex}\n${
            ex.stack
          }`
        );
      }
    });
  }

  return store;
}
