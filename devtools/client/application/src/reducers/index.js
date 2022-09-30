/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  combineReducers,
} = require("resource://devtools/client/shared/vendor/redux.js");
const {
  workersReducer,
} = require("resource://devtools/client/application/src/reducers/workers-state.js");
const {
  pageReducer,
} = require("resource://devtools/client/application/src/reducers/page-state.js");
const {
  uiReducer,
} = require("resource://devtools/client/application/src/reducers/ui-state.js");
const {
  manifestReducer,
} = require("resource://devtools/client/application/src/reducers/manifest-state.js");

module.exports = combineReducers({
  manifest: manifestReducer,
  page: pageReducer,
  workers: workersReducer,
  ui: uiReducer,
});
