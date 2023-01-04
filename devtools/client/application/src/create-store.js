/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  thunk,
} = require("resource://devtools/client/shared/redux/middleware/thunk.js");
const eventTelemetryMiddleware = require("resource://devtools/client/application/src/middleware/event-telemetry.js");

const {
  applyMiddleware,
  createStore,
} = require("resource://devtools/client/shared/vendor/redux.js");

// Reducers

const rootReducer = require("resource://devtools/client/application/src/reducers/index.js");
const {
  ManifestState,
} = require("resource://devtools/client/application/src/reducers/manifest-state.js");
const {
  WorkersState,
} = require("resource://devtools/client/application/src/reducers/workers-state.js");
const {
  PageState,
} = require("resource://devtools/client/application/src/reducers/page-state.js");
const {
  UiState,
} = require("resource://devtools/client/application/src/reducers/ui-state.js");

function configureStore(telemetry) {
  // Prepare initial state.
  const initialState = {
    manifest: new ManifestState(),
    page: new PageState(),
    ui: new UiState(),
    workers: new WorkersState(),
  };

  const middleware = applyMiddleware(
    thunk(),
    eventTelemetryMiddleware(telemetry)
  );

  return createStore(rootReducer, initialState, middleware);
}

exports.configureStore = configureStore;
