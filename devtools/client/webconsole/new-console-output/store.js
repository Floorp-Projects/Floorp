/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { combineReducers, createStore } = require("devtools/client/shared/vendor/redux");
const { reducers } = require("./reducers/index");

function storeFactory(initialState = {}) {
  return createStore(combineReducers(reducers), initialState);
}

// Provide the single store instance for app code.
module.exports.store = storeFactory();
// Provide the store factory for test code so that each test is working with
// its own instance.
module.exports.storeFactory = storeFactory;

