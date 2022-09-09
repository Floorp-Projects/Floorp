/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  applyMiddleware,
  createStore,
} = require("devtools/client/shared/vendor/redux");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk.js");
const {
  waitUntilService,
} = require("devtools/client/shared/redux/middleware/wait-service.js");

const rootReducer = require("devtools/client/aboutdebugging/src/reducers/index");
const {
  DebugTargetsState,
} = require("devtools/client/aboutdebugging/src/reducers/debug-targets-state");
const {
  RuntimesState,
} = require("devtools/client/aboutdebugging/src/reducers/runtimes-state");
const {
  UiState,
} = require("devtools/client/aboutdebugging/src/reducers/ui-state");
const debugTargetListenerMiddleware = require("devtools/client/aboutdebugging/src/middleware/debug-target-listener");
const errorLoggingMiddleware = require("devtools/client/aboutdebugging/src/middleware/error-logging");
const eventRecordingMiddleware = require("devtools/client/aboutdebugging/src/middleware/event-recording");
const extensionComponentDataMiddleware = require("devtools/client/aboutdebugging/src/middleware/extension-component-data");
const processComponentDataMiddleware = require("devtools/client/aboutdebugging/src/middleware/process-component-data");
const tabComponentDataMiddleware = require("devtools/client/aboutdebugging/src/middleware/tab-component-data");
const workerComponentDataMiddleware = require("devtools/client/aboutdebugging/src/middleware/worker-component-data");
const {
  getDebugTargetCollapsibilities,
} = require("devtools/client/aboutdebugging/src/modules/debug-target-collapsibilities");
const {
  getNetworkLocations,
} = require("devtools/client/aboutdebugging/src/modules/network-locations");

const { PREFERENCES } = require("devtools/client/aboutdebugging/src/constants");

function configureStore() {
  const initialState = {
    debugTargets: new DebugTargetsState(),
    runtimes: new RuntimesState(),
    ui: getUiState(),
  };

  const middleware = applyMiddleware(
    thunk(),
    debugTargetListenerMiddleware,
    errorLoggingMiddleware,
    eventRecordingMiddleware,
    extensionComponentDataMiddleware,
    processComponentDataMiddleware,
    tabComponentDataMiddleware,
    workerComponentDataMiddleware,
    waitUntilService
  );

  return createStore(rootReducer, initialState, middleware);
}

function getUiState() {
  const collapsibilities = getDebugTargetCollapsibilities();
  const locations = getNetworkLocations();
  const showHiddenAddons = Services.prefs.getBoolPref(
    PREFERENCES.SHOW_HIDDEN_ADDONS,
    false
  );
  return new UiState(locations, collapsibilities, showHiddenAddons);
}

exports.configureStore = configureStore;
