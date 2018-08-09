/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { applyMiddleware, createStore } = require("devtools/client/shared/vendor/redux");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk.js");

const rootReducer = require("./reducers/index");
const { RuntimeState } = require("./reducers/runtime-state");
const { UiState } = require("./reducers/ui-state");
const debugTargetListenerMiddleware = require("./middleware/debug-target-listener");

exports.configureStore = function() {
  const initialState = {
    runtime: new RuntimeState(),
    ui: new UiState()
  };

  const middleware = applyMiddleware(thunk, debugTargetListenerMiddleware);

  return createStore(rootReducer, initialState, middleware);
};
