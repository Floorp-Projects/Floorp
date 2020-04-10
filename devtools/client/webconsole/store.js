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
  createStore,
} = require("devtools/client/shared/vendor/redux");

// Prefs
const { PREFS } = require("devtools/client/webconsole/constants");
const { getPrefsService } = require("devtools/client/webconsole/utils/prefs");

// Reducers
const { reducers } = require("devtools/client/webconsole/reducers/index");

// Middlewares
const { ignore } = require("devtools/client/shared/redux/middleware/ignore");
const eventTelemetry = require("devtools/client/webconsole/middleware/event-telemetry");
const historyPersistence = require("devtools/client/webconsole/middleware/history-persistence");
const {
  thunkWithOptions,
} = require("devtools/client/shared/redux/middleware/thunk-with-options");

// Enhancers
const enableBatching = require("devtools/client/webconsole/enhancers/batching");
const enableActorReleaser = require("devtools/client/webconsole/enhancers/actor-releaser");
const ensureCSSErrorReportingEnabled = require("devtools/client/webconsole/enhancers/css-error-reporting");
const enableNetProvider = require("devtools/client/webconsole/enhancers/net-provider");
const enableMessagesCacheClearing = require("devtools/client/webconsole/enhancers/message-cache-clearing");

/**
 * Create and configure store for the Console panel. This is the place
 * where various enhancers and middleware can be registered.
 */
function configureStore(webConsoleUI, options = {}) {
  const prefsService = getPrefsService(webConsoleUI);
  const { getBoolPref, getIntPref } = prefsService;

  const logLimit =
    options.logLimit || Math.max(getIntPref("devtools.hud.loglimit"), 1);
  const sidebarToggle = getBoolPref(PREFS.FEATURES.SIDEBAR_TOGGLE);
  const autocomplete = getBoolPref(PREFS.FEATURES.AUTOCOMPLETE);
  const eagerEvaluation = getBoolPref(PREFS.FEATURES.EAGER_EVALUATION);
  const groupWarnings = getBoolPref(PREFS.FEATURES.GROUP_WARNINGS);
  const historyCount = getIntPref(PREFS.UI.INPUT_HISTORY_COUNT);

  const initialState = {
    prefs: PrefState({
      logLimit,
      sidebarToggle,
      autocomplete,
      eagerEvaluation,
      historyCount,
      groupWarnings,
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
      networkMessageActiveTabId: "headers",
      persistLogs: getBoolPref(PREFS.UI.PERSIST),
      showContentMessages:
        webConsoleUI.isBrowserConsole || webConsoleUI.isBrowserToolboxConsole
          ? getBoolPref(PREFS.UI.CONTENT_MESSAGES)
          : true,
      editor: getBoolPref(PREFS.UI.EDITOR),
      editorWidth: getIntPref(PREFS.UI.EDITOR_WIDTH),
      showEditorOnboarding: getBoolPref(PREFS.UI.EDITOR_ONBOARDING),
      timestampsVisible: getBoolPref(PREFS.UI.MESSAGE_TIMESTAMP),
      showEvaluationContextSelector: getBoolPref(
        webConsoleUI.isBrowserToolboxConsole
          ? PREFS.UI.CONTEXT_SELECTOR_BROWSER_TOOLBOX
          : PREFS.UI.CONTEXT_SELECTOR_CONTENT_TOOLBOX
      ),
    }),
  };

  const { toolbox } = options.thunkArgs;
  const sessionId = (toolbox && toolbox.sessionId) || -1;
  const middleware = applyMiddleware(
    ignore,
    thunkWithOptions.bind(null, {
      prefsService,
      ...options.thunkArgs,
    }),
    historyPersistence,
    eventTelemetry.bind(null, options.telemetry, sessionId)
  );

  return createStore(
    createRootReducer(),
    initialState,
    compose(
      middleware,
      enableActorReleaser(webConsoleUI),
      enableBatching(),
      enableNetProvider(webConsoleUI),
      enableMessagesCacheClearing(webConsoleUI),
      ensureCSSErrorReportingEnabled(webConsoleUI)
    )
  );
}

function createRootReducer() {
  return function rootReducer(state, action) {
    // We want to compute the new state for all properties except
    // "messages" and "history". These two reducers are handled
    // separately since they are receiving additional arguments.
    const newState = Object.entries(reducers).reduce((res, [key, reducer]) => {
      if (key !== "messages" && key !== "history") {
        res[key] = reducer(state[key], action);
      }
      return res;
    }, {});

    // Pass prefs state as additional argument to the history reducer.
    newState.history = reducers.history(state.history, action, newState.prefs);

    // Specifically pass the updated filters, prefs and ui states as additional arguments.
    newState.messages = reducers.messages(
      state.messages,
      action,
      newState.filters,
      newState.prefs,
      newState.ui
    );

    return newState;
  };
}

// Provide the store factory for test code so that each test is working with
// its own instance.
module.exports.configureStore = configureStore;
