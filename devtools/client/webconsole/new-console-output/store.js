/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {FilterState} = require("devtools/client/webconsole/new-console-output/reducers/filters");
const {PrefState} = require("devtools/client/webconsole/new-console-output/reducers/prefs");
const {UiState} = require("devtools/client/webconsole/new-console-output/reducers/ui");
const {
  applyMiddleware,
  compose,
  createStore
} = require("devtools/client/shared/vendor/redux");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk");
const {
  debounceActions,
  BATCH_ACTIONS
} = require("devtools/client/shared/redux/middleware/debounce");
const {
  MESSAGE_ADD,
  MESSAGES_CLEAR,
  REMOVED_MESSAGES_CLEAR,
  PREFS,
} = require("devtools/client/webconsole/new-console-output/constants");
const { reducers } = require("./reducers/index");
const Services = require("Services");

function configureStore(hud, options = {}) {
  const logLimit = options.logLimit
    || Math.max(Services.prefs.getIntPref("devtools.hud.loglimit"), 1);

  const initialState = {
    prefs: new PrefState({ logLimit }),
    filters: new FilterState({
      error: Services.prefs.getBoolPref(PREFS.FILTER.ERROR),
      warn: Services.prefs.getBoolPref(PREFS.FILTER.WARN),
      info: Services.prefs.getBoolPref(PREFS.FILTER.INFO),
      debug: Services.prefs.getBoolPref(PREFS.FILTER.DEBUG),
      log: Services.prefs.getBoolPref(PREFS.FILTER.LOG),
      css: Services.prefs.getBoolPref(PREFS.FILTER.CSS),
      net: Services.prefs.getBoolPref(PREFS.FILTER.NET),
      netxhr: Services.prefs.getBoolPref(PREFS.FILTER.NETXHR),
    }),
    ui: new UiState({
      filterBarVisible: Services.prefs.getBoolPref(PREFS.UI.FILTER_BAR),
    })
  };

  let args = [thunk];
  if (!options.noDebounce) {
    args.push(debounceActions(16, 500));
  }

  let middleware = applyMiddleware(...args);
  return createStore(
    createRootReducer(),
    initialState,
    compose(middleware, enableActorReleaser(hud), enableBatching())
  );
}

function createRootReducer() {
  return function rootReducer(state, action) {
    // We want to compute the new state for all properties except "messages".
    const newState = [...Object.entries(reducers)].reduce((res, [key, reducer]) => {
      if (key !== "messages") {
        res[key] = reducer(state[key], action);
      }
      return res;
    }, {});

    return Object.assign(newState, {
      // specifically pass the updated filters and prefs state as additional arguments.
      messages: reducers.messages(
        state.messages,
        action,
        newState.filters,
        newState.prefs,
      ),
    });
  };
}

/**
 * A enhancer for the store to handle batched actions.
 */
function enableBatching() {
  return next => (reducer, initialState, enhancer) => {
    function batchingReducer(state, action) {
      switch (action.type) {
        case BATCH_ACTIONS:
          return action.actions.reduce(batchingReducer, state);
        default:
          return reducer(state, action);
      }
    }

    if (typeof initialState === "function" && typeof enhancer === "undefined") {
      enhancer = initialState;
      initialState = undefined;
    }

    return next(batchingReducer, initialState, enhancer);
  };
}

/**
 * This enhancer is responsible for releasing actors on the backend.
 * When messages with arguments are removed from the store we should also
 * clean up the backend.
 */
function enableActorReleaser(hud) {
  return next => (reducer, initialState, enhancer) => {
    function releaseActorsEnhancer(state, action) {
      state = reducer(state, action);

      let type = action.type;
      let proxy = hud ? hud.proxy : null;
      if (proxy && (type == MESSAGE_ADD || type == MESSAGES_CLEAR)) {
        releaseActors(state.messages.removedMessages, proxy);

        // Reset `removedMessages` in message reducer.
        state = reducer(state, {
          type: REMOVED_MESSAGES_CLEAR
        });
      }

      return state;
    }

    return next(releaseActorsEnhancer, initialState, enhancer);
  };
}

/**
 * Helper function for releasing backend actors.
 */
function releaseActors(removedMessages, proxy) {
  if (!proxy) {
    return;
  }

  removedMessages.forEach(msg => {
    for (let i = 0; i < msg.parameters.length; i++) {
      let param = msg.parameters[i];
      if (param && param.actor) {
        proxy.releaseActor(param.actor);
      }
    }
  });
}

// Provide the store factory for test code so that each test is working with
// its own instance.
module.exports.configureStore = configureStore;

