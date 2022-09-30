/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  combineReducers,
} = require("resource://devtools/client/shared/vendor/redux.js");
const {
  debugTargetsReducer,
} = require("resource://devtools/client/aboutdebugging/src/reducers/debug-targets-state.js");
const {
  runtimesReducer,
} = require("resource://devtools/client/aboutdebugging/src/reducers/runtimes-state.js");
const {
  uiReducer,
} = require("resource://devtools/client/aboutdebugging/src/reducers/ui-state.js");

module.exports = combineReducers({
  debugTargets: debugTargetsReducer,
  runtimes: runtimesReducer,
  ui: uiReducer,
});
