/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createStore, applyMiddleware } = require("devtools/client/shared/vendor/redux");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk");
const batching = require("./middleware/batching");
const rootReducer = require("./reducers/index");

function configureStore() {
  return createStore(
    rootReducer,
    applyMiddleware(
      thunk,
      batching
    )
  );
}

exports.configureStore = configureStore;
