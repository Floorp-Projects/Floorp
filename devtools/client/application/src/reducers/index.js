/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { combineReducers } = require("devtools/client/shared/vendor/redux");
const { workersReducer } = require("./workers-state");
const { pageReducer } = require("./page-state");
const { uiReducer } = require("./ui-state");
const { manifestReducer } = require("./manifest-state");

module.exports = combineReducers({
  manifest: manifestReducer,
  page: pageReducer,
  workers: workersReducer,
  ui: uiReducer,
});
