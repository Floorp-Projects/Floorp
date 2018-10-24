/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const { applyMiddleware, createStore } = require("devtools/client/shared/vendor/redux");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk.js");

const rootReducer = require("./reducers/index");
const { DebugTargetsState } = require("./reducers/debug-targets-state");
const { RuntimesState } = require("./reducers/runtimes-state");
const { UiState } = require("./reducers/ui-state");
const debugTargetListenerMiddleware = require("./middleware/debug-target-listener");
const errorLoggingMiddleware = require("./middleware/error-logging");
const extensionComponentDataMiddleware = require("./middleware/extension-component-data");
const tabComponentDataMiddleware = require("./middleware/tab-component-data");
const workerComponentDataMiddleware = require("./middleware/worker-component-data");
const { getDebugTargetCollapsibilities } = require("./modules/debug-target-collapsibilities");
const { getNetworkLocations } = require("./modules/network-locations");

const { PREFERENCES } = require("./constants");

function configureStore() {
  const initialState = {
    debugTargets: new DebugTargetsState(),
    runtimes: new RuntimesState(),
    ui: getUiState(),
  };

  const middleware = applyMiddleware(thunk,
                                     debugTargetListenerMiddleware,
                                     errorLoggingMiddleware,
                                     extensionComponentDataMiddleware,
                                     tabComponentDataMiddleware,
                                     workerComponentDataMiddleware);

  return createStore(rootReducer, initialState, middleware);
}

function getUiState() {
  const collapsibilities = getDebugTargetCollapsibilities();
  const locations = getNetworkLocations();
  const networkEnabled = Services.prefs.getBoolPref(PREFERENCES.NETWORK_ENABLED, false);
  const wifiEnabled = Services.prefs.getBoolPref(PREFERENCES.WIFI_ENABLED, false);
  return new UiState(locations, collapsibilities, networkEnabled, wifiEnabled);
}

exports.configureStore = configureStore;
