/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createStore } = require("devtools/client/shared/vendor/redux");

// Reducers
const rootReducer = require("./reducers/index");
const { WorkersState } = require("./reducers/workers-state");

function configureStore() {
  // Prepare initial state.
  const initialState = {
    workers: new WorkersState(),
  };

  return createStore(rootReducer, initialState);
}

exports.configureStore = configureStore;
