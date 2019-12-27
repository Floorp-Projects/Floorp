/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { thunk } = require("devtools/client/shared/redux/middleware/thunk.js");
const {
  applyMiddleware,
  createStore,
} = require("devtools/client/shared/vendor/redux");

// Reducers

const rootReducer = require("devtools/client/application/src/reducers/index");
const {
  ManifestState,
} = require("devtools/client/application/src/reducers/manifest-state");
const {
  WorkersState,
} = require("devtools/client/application/src/reducers/workers-state");
const {
  PageState,
} = require("devtools/client/application/src/reducers/page-state");
const {
  UiState,
} = require("devtools/client/application/src/reducers/ui-state");

function configureStore() {
  // Prepare initial state.
  const initialState = {
    manifest: new ManifestState(),
    page: new PageState(),
    ui: new UiState(),
    workers: new WorkersState(),
  };

  const middleware = applyMiddleware(thunk);

  return createStore(rootReducer, initialState, middleware);
}

exports.configureStore = configureStore;
