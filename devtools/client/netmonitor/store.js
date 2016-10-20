/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const createStore = require("devtools/client/shared/redux/create-store");
const reducers = require("devtools/client/netmonitor/reducers/index");

function configureStore() {
  const initialState = {
      // All initial application states will be defined as store from here
  };

  return createStore()(
    reducers,
    initialState
  );
}

exports.configureStore = configureStore;
