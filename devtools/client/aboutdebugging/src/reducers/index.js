/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { combineReducers } = require("devtools/client/shared/vendor/redux");
const {
  debugTargetsReducer,
} = require("devtools/client/aboutdebugging/src/reducers/debug-targets-state");
const {
  runtimesReducer,
} = require("devtools/client/aboutdebugging/src/reducers/runtimes-state");
const {
  uiReducer,
} = require("devtools/client/aboutdebugging/src/reducers/ui-state");

module.exports = combineReducers({
  debugTargets: debugTargetsReducer,
  runtimes: runtimesReducer,
  ui: uiReducer,
});
