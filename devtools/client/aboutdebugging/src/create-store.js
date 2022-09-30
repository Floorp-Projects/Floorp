/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  applyMiddleware,
  createStore,
} = require("resource://devtools/client/shared/vendor/redux.js");
const {
  thunk,
} = require("resource://devtools/client/shared/redux/middleware/thunk.js");
const {
  waitUntilService,
} = require("resource://devtools/client/shared/redux/middleware/wait-service.js");

const rootReducer = require("resource://devtools/client/aboutdebugging/src/reducers/index.js");
const {
  DebugTargetsState,
} = require("resource://devtools/client/aboutdebugging/src/reducers/debug-targets-state.js");
const {
  RuntimesState,
} = require("resource://devtools/client/aboutdebugging/src/reducers/runtimes-state.js");
const {
  UiState,
} = require("resource://devtools/client/aboutdebugging/src/reducers/ui-state.js");
const debugTargetListenerMiddleware = require("resource://devtools/client/aboutdebugging/src/middleware/debug-target-listener.js");
const errorLoggingMiddleware = require("resource://devtools/client/aboutdebugging/src/middleware/error-logging.js");
const eventRecordingMiddleware = require("resource://devtools/client/aboutdebugging/src/middleware/event-recording.js");
const extensionComponentDataMiddleware = require("resource://devtools/client/aboutdebugging/src/middleware/extension-component-data.js");
const processComponentDataMiddleware = require("resource://devtools/client/aboutdebugging/src/middleware/process-component-data.js");
const tabComponentDataMiddleware = require("resource://devtools/client/aboutdebugging/src/middleware/tab-component-data.js");
const workerComponentDataMiddleware = require("resource://devtools/client/aboutdebugging/src/middleware/worker-component-data.js");
const {
  getDebugTargetCollapsibilities,
} = require("resource://devtools/client/aboutdebugging/src/modules/debug-target-collapsibilities.js");
const {
  getNetworkLocations,
} = require("resource://devtools/client/aboutdebugging/src/modules/network-locations.js");

const {
  PREFERENCES,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");

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
