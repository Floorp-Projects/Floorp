/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { combineReducers } = require("../shared/vendor/redux");
const createStore = require("../shared/redux/create-store");
const reducers = require("./reducers");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

module.exports = function () {
  let shouldLog = DevToolsUtils.testing;
  return createStore({ log: shouldLog })(combineReducers(reducers), {});
};
