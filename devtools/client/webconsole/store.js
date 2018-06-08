/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// State
const { FilterState } = require("devtools/client/webconsole/reducers/filters");
const { PrefState } = require("devtools/client/webconsole/reducers/prefs");
const { UiState } = require("devtools/client/webconsole/reducers/ui");

// Redux
const {
  applyMiddleware,
  compose,
  createStore
} = require("devtools/client/shared/vendor/redux");

// Prefs
const { PREFS } = require("devtools/client/webconsole/constants");
const { getPrefsService } = require("devtools/client/webconsole/utils/prefs");

// Reducers
const { reducers } = require("./reducers/index");

// Middleware
const historyPersistence = require("./middleware/history-persistence");
const thunk = require("./middleware/thunk");

// Enhancers
const enableBatching = require("./enhancers/batching");
const enableActorReleaser = require("./enhancers/actor-releaser");
const ensureCSSErrorReportingEnabled = require("./enhancers/css-error-reporting");
const enableNetProvider = require("./enhancers/net-provider");
const enableMessagesCacheClearing = require("./enhancers/message-cache-clearing");

/**
 * Create and configure store for the Console panel. This is the place
 * where various enhancers and middleware can be registered.
 */
function configureStore(hud, options = {}) {
  const prefsService = getPrefsService(hud);
  const {
    getBoolPref,
    getIntPref,
  } = prefsService;

  const logLimit = options.logLimit
    || Math.max(getIntPref("devtools.hud.loglimit"), 1);
  const sidebarToggle = getBoolPref(PREFS.FEATURES.SIDEBAR_TOGGLE);
  const jstermCodeMirror = getBoolPref(PREFS.FEATURES.JSTERM_CODE_MIRROR);
  const historyCount = getIntPref(PREFS.UI.INPUT_HISTORY_COUNT);

  const initialState = {
    prefs: PrefState({
      logLimit,
      sidebarToggle,
      jstermCodeMirror,
      historyCount,
    }),
    filters: FilterState({
      error: getBoolPref(PREFS.FILTER.ERROR),
      warn: getBoolPref(PREFS.FILTER.WARN),
      info: getBoolPref(PREFS.FILTER.INFO),
      debug: getBoolPref(PREFS.FILTER.DEBUG),
      log: getBoolPref(PREFS.FILTER.LOG),
      css: getBoolPref(PREFS.FILTER.CSS),
      net: getBoolPref(PREFS.FILTER.NET),
      netxhr: getBoolPref(PREFS.FILTER.NETXHR),
    }),
    ui: UiState({
      filterBarVisible: getBoolPref(PREFS.UI.FILTER_BAR),
      networkMessageActiveTabId: "headers",
      persistLogs: getBoolPref(PREFS.UI.PERSIST),
    })
  };

  // Prepare middleware.
  const middleware = applyMiddleware(
    thunk.bind(null, {prefsService}),
    historyPersistence,
  );

  return createStore(
    createRootReducer(),
    initialState,
    compose(
      middleware,
      enableActorReleaser(hud),
      enableBatching(),
      enableNetProvider(hud),
      enableMessagesCacheClearing(hud),
      ensureCSSErrorReportingEnabled(hud),
    )
  );
}

function createRootReducer() {
  return function rootReducer(state, action) {
    // We want to compute the new state for all properties except
    // "messages" and "history". These two reducers are handled
    // separately since they are receiving additional arguments.
    const newState = [...Object.entries(reducers)].reduce((res, [key, reducer]) => {
      if (key !== "messages" && key !== "history") {
        res[key] = reducer(state[key], action);
      }
      return res;
    }, {});

    // Pass prefs state as additional argument to the history reducer.
    newState.history = reducers.history(state.history, action, newState.prefs);

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

// Provide the store factory for test code so that each test is working with
// its own instance.
module.exports.configureStore = configureStore;

